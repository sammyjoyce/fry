#include "tui_mux.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifdef __APPLE__
#include <ncurses.h>
#else
#include <ncurses/ncurses.h>
#endif

#include <core/session.h>
#include <utils/logging.h>
#include <utils/memory.h>
#include "tui/components/tui_command.h"
#include "tui/components/tui_input.h"
#include "tui/backend/tui_ncurses.h"
#include "tui/tui_workspace.h"

// Forward declaration for terminal size function
static app_error app_get_terminal_size(int *width, int *height);

// Create multiplexer
app_error app_tui_mux_create(app_tui_mux_t **mux,
                             const app_tui_mux_config_t *config,
                             app_session_manager_t *session_manager) {
  if (!mux || !config || !session_manager) {
    return APP_ERROR_INVALID_ARG;
  }

  app_tui_mux_t *m = app_calloc(1, sizeof(app_tui_mux_t));
  if (!m) {
    return APP_ERROR_MEMORY;
  }

  // Copy configuration
  m->config = *config;
  m->session_manager = session_manager;

  // Initialize state
  m->state = MUX_STATE_NORMAL;
  m->input_mode = INPUT_MODE_NORMAL;
  m->running = false;

  // Create components
  app_error err =
      app_tui_pane_manager_create(&m->pane_manager, config->max_panes);
  if (err != APP_SUCCESS) {
    app_free(m);
    return err;
  }

  // Get terminal size without relying on initialized TUI
  int width, height;
  err = app_get_terminal_size(&width, &height);
  if (err != APP_SUCCESS) {
    // Fallback to default size
    width = 80;
    height = 24;
    LOG_WARNING("Failed to get terminal size, using default %dx%d", width, height);
  }

  err = app_tui_layout_create(&m->layout_manager, width, height);
  if (err != APP_SUCCESS) {
    (void)app_tui_pane_manager_destroy(m->pane_manager);
    app_free(m);
    return err;
  }

  // Initialize command buffer
  m->command_buffer_size = 256;
  m->command_buffer = app_calloc(m->command_buffer_size, 1);
  if (!m->command_buffer) {
    (void)app_tui_layout_destroy(m->layout_manager);
    (void)app_tui_pane_manager_destroy(m->pane_manager);
    app_free(m);
    return APP_ERROR_MEMORY;
  }

  // Create input system
  err = app_tui_input_create(&m->input_system);
  if (err != APP_SUCCESS) {
    LOG_WARNING("Failed to create input system: %s", app_error_string(err));
    // Continue without advanced input system
    m->input_system = NULL;
  }

  // Initialize command mode
  err = app_tui_command_init(m);
  if (err != APP_SUCCESS) {
    LOG_WARNING("Failed to initialize command mode: %s", app_error_string(err));
    // Continue without command mode
  }

  *mux = m;
  return APP_SUCCESS;
}

// Destroy multiplexer
app_error app_tui_mux_destroy(app_tui_mux_t *mux) {
  if (!mux) {
    return APP_SUCCESS;
  }

  // Destroy components
  (void)app_tui_pane_manager_destroy(mux->pane_manager);
  (void)app_tui_layout_destroy(mux->layout_manager);

  // Destroy input system
  if (mux->input_system) {
    app_tui_input_destroy(mux->input_system);
  }

  // Cleanup command mode
  (void)app_tui_command_cleanup(mux);

  // Free command buffer
  app_free(mux->command_buffer);

  // Free workspace info
  app_free(mux->workspace_id);
  app_free(mux->workspace_name);

  app_free(mux);

  return APP_SUCCESS;
}

// Initialize TUI
app_error app_tui_mux_init(app_tui_mux_t *mux) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  // Initialize NCurses
  app_error err = tui_init();
  if (err != APP_SUCCESS) {
    return err;
  }

  // Enable mouse if configured
  if (mux->config.enable_mouse) {
    g_ncurses->mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
  }

  // Set up colors if available
  if (mux->config.enable_colors && has_colors()) {
    g_ncurses->start_color();
    g_ncurses->use_default_colors();

    // Define color pairs for pane states
    g_ncurses->init_pair(1, COLOR_GREEN, -1);   // Active
    g_ncurses->init_pair(2, COLOR_YELLOW, -1);  // Busy
    g_ncurses->init_pair(3, COLOR_RED, -1);     // Error
    g_ncurses->init_pair(4, COLOR_WHITE, -1);   // Inactive
  }

  // Create status bar window
  int max_y = tui_get_max_y();
  int max_x = tui_get_max_x();

  mux->status_bar = g_ncurses->newwin(1, max_x, max_y - 1, 0);
  if (!mux->status_bar) {
    tui_cleanup();
    return APP_ERROR_INTERNAL;
  }

  // Enable raw mode in input system
  if (mux->input_system) {
    err = app_tui_input_enable_raw_mode(mux->input_system);
    if (err != APP_SUCCESS) {
      LOG_WARNING("Failed to enable raw mode: %s", app_error_string(err));
    }

    // Enable mouse if configured
    if (mux->config.enable_mouse) {
      err = app_tui_input_enable_mouse(mux->input_system);
      if (err != APP_SUCCESS) {
        LOG_WARNING("Failed to enable mouse: %s", app_error_string(err));
      }
    }

    // Register navigation keybindings
    err = app_tui_mux_register_nav_keys(mux);
    if (err != APP_SUCCESS) {
      LOG_WARNING("Failed to register navigation keys: %s",
                  app_error_string(err));
    }
  }

  LOG_INFO("Multiplexer TUI initialized");

  return APP_SUCCESS;
}

// Shutdown TUI
app_error app_tui_mux_shutdown(app_tui_mux_t *mux) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  mux->running = false;

  // Destroy windows
  if (mux->status_bar) {
    g_ncurses->delwin(mux->status_bar);
    mux->status_bar = NULL;
  }

  if (mux->command_line) {
    g_ncurses->delwin(mux->command_line);
    mux->command_line = NULL;
  }

  // Cleanup NCurses
  tui_cleanup();

  LOG_INFO("Multiplexer TUI shutdown");

  return APP_SUCCESS;
}

// Main run loop
app_error app_tui_mux_run(app_tui_mux_t *mux) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  mux->running = true;

  // Initial render
  (void)app_tui_mux_render(mux);

  // Main event loop
  while (mux->running && mux->state != MUX_STATE_EXITING) {
    // Handle input
    int ch = tui_get_char();
    if (ch != ERR) {
      (void)app_tui_mux_handle_input(mux, ch);
    }

    // TODO: Poll PTYs for output

    // Render if needed
    if (mux->pane_manager->needs_refresh) {
      (void)app_tui_mux_render(mux);
      mux->pane_manager->needs_refresh = false;
    }

    // Small delay to prevent CPU spinning
    g_ncurses->napms(mux->config.refresh_rate_ms);
  }

  return APP_SUCCESS;
}

// Add a new pane
app_error app_tui_mux_add_pane(app_tui_mux_t *mux, const char *account_id,
                               app_tui_pane_t **pane) {
  if (!mux || !pane) {
    return APP_ERROR_INVALID_ARG;
  }

  // Create pane with temporary geometry (will be updated by layout)
  app_pane_rect_t temp_geometry = {0, 0, 80, 24};

  app_error err =
      app_tui_pane_create(mux->pane_manager, account_id, &temp_geometry, pane);

  if (err != APP_SUCCESS) {
    return err;
  }

  // Recalculate layout
  err = app_tui_layout_apply(mux->layout_manager, mux->pane_manager->panes,
                             mux->pane_manager->pane_count);

  if (err != APP_SUCCESS) {
    LOG_WARNING("Failed to apply layout after adding pane: %s",
                app_error_string(err));
  }

  // Launch Claude Code in the new pane
  if (*pane) {
    // For now, pass NULL for session - OAuth token will come from environment
    err = app_tui_pane_launch_claude(*pane, account_id, NULL);
    if (err != APP_SUCCESS) {
      LOG_ERROR("Failed to launch Claude Code in pane: %s",
                app_error_string(err));
      // Don't fail the whole operation, just mark pane as error
      (*pane)->state = PANE_STATE_ERROR;
    }

    // Add PTY to event loop monitoring
    if ((*pane)->pty_master >= 0 && mux->running) {
      // This will be picked up by the enhanced event loop
      LOG_DEBUG("Added PTY fd %d to monitoring for pane %d",
                (*pane)->pty_master, (*pane)->number);
    }
  }

  return APP_SUCCESS;
}

// Handle keyboard input
app_error app_tui_mux_handle_input(app_tui_mux_t *mux, int key) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  // Handle based on current mode
  switch (mux->input_mode) {
  case INPUT_MODE_NORMAL:
    // Check for mode switches
    if (key == ':') {
      return app_tui_mux_enter_command_mode(mux);
    }

    // Navigation keys
    switch (key) {
    case 'h':
    case KEY_LEFT:
      return app_tui_mux_focus_direction(mux, -1, 0);

    case 'l':
    case KEY_RIGHT:
      return app_tui_mux_focus_direction(mux, 1, 0);

    case 'k':
    case KEY_UP:
      return app_tui_mux_focus_direction(mux, 0, -1);

    case 'j':
    case KEY_DOWN:
      return app_tui_mux_focus_direction(mux, 0, 1);

    case '\t':  // Tab
      return app_tui_pane_focus_next(mux->pane_manager);

    case 'q':
      mux->state = MUX_STATE_EXITING;
      return APP_SUCCESS;

    case ':':
      return app_tui_command_enter(mux);

    default:
      // Alt+number for direct pane access
      if (key >= '1' && key <= '9') {
        return app_tui_mux_focus_number(mux, key - '0');
      }
      break;
    }
    break;

  case INPUT_MODE_COMMAND:
    return app_tui_command_handle_key(mux, key);

  default:
    break;
  }

  return APP_SUCCESS;
}

// Enter command mode
app_error app_tui_mux_enter_command_mode(app_tui_mux_t *mux) {
  return app_tui_command_enter(mux);
}

// Exit command mode
app_error app_tui_mux_exit_command_mode(app_tui_mux_t *mux) {
  return app_tui_command_exit(mux);
}

// Execute command
app_error app_tui_mux_execute_command(app_tui_mux_t *mux, const char *command) {
  return app_tui_command_parse_and_execute(mux, command);
}

// Render the multiplexer
app_error app_tui_mux_render(app_tui_mux_t *mux) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  // Clear screen
  g_ncurses->clear();

  // Render panes
  for (size_t i = 0; i < mux->pane_manager->pane_count; i++) {
    app_tui_pane_t *pane = mux->pane_manager->panes[i];

    // Draw pane border
    int color_pair = 4;  // Default inactive
    switch (pane->state) {
    case PANE_STATE_ACTIVE:
      color_pair = 1;
      break;
    case PANE_STATE_CONNECTING:
      color_pair = 2;
      break;
    case PANE_STATE_ERROR:
      color_pair = 3;
      break;
    default:
      break;
    }

    if (pane->has_focus) {
      g_ncurses->attron(A_BOLD);
    }

    g_ncurses->attron(COLOR_PAIR(color_pair));

    // Draw simple border
    WINDOW *win = app_ncurses_stdscr();
    for (int y = pane->geometry.y; y < pane->geometry.y + pane->geometry.height;
         y++) {
      g_ncurses->wmove(win, y, pane->geometry.x);
      g_ncurses->waddch(win, '|');
      g_ncurses->wmove(win, y, pane->geometry.x + pane->geometry.width - 1);
      g_ncurses->waddch(win, '|');
    }
    for (int x = pane->geometry.x; x < pane->geometry.x + pane->geometry.width;
         x++) {
      g_ncurses->wmove(win, pane->geometry.y, x);
      g_ncurses->waddch(win, '-');
      g_ncurses->wmove(win, pane->geometry.y + pane->geometry.height - 1, x);
      g_ncurses->waddch(win, '-');
    }

    // Draw corners
    g_ncurses->wmove(win, pane->geometry.y, pane->geometry.x);
    g_ncurses->waddch(win, '+');
    g_ncurses->wmove(win, pane->geometry.y,
                     pane->geometry.x + pane->geometry.width - 1);
    g_ncurses->waddch(win, '+');
    g_ncurses->wmove(win, pane->geometry.y + pane->geometry.height - 1,
                     pane->geometry.x);
    g_ncurses->waddch(win, '+');
    g_ncurses->wmove(win, pane->geometry.y + pane->geometry.height - 1,
                     pane->geometry.x + pane->geometry.width - 1);
    g_ncurses->waddch(win, '+');
    // Draw pane number
    g_ncurses->mvprintw(pane->geometry.y, pane->geometry.x + 2, " %d ",
                        pane->number);

    // Draw title if set
    if (pane->title) {
      g_ncurses->mvprintw(pane->geometry.y, pane->geometry.x + 6, " %s ",
                          pane->title);
    }

    g_ncurses->attroff(COLOR_PAIR(color_pair));
    if (pane->has_focus) {
      g_ncurses->attroff(A_BOLD);
    }
  }

  // Render status bar
  (void)app_tui_mux_render_status_bar(mux);

  // Render command line if in command mode
  if (mux->input_mode == INPUT_MODE_COMMAND) {
    (void)app_tui_mux_render_command_line(mux);
  }

  // Refresh screen
  g_ncurses->refresh();

  return APP_SUCCESS;
}

// Render status bar
app_error app_tui_mux_render_status_bar(app_tui_mux_t *mux) {
  if (!mux || !mux->status_bar) {
    return APP_ERROR_INVALID_ARG;
  }

  g_ncurses->wclear(mux->status_bar);

  // Show basic info
  g_ncurses->mvwprintw(mux->status_bar, 0, 0, " Panes: %zu/%zu",
                       mux->pane_manager->pane_count,
                       mux->pane_manager->max_panes);

  // Show mode
  const char *mode_str = "NORMAL";
  switch (mux->input_mode) {
  case INPUT_MODE_COMMAND:
    mode_str = "COMMAND";
    break;
  case INPUT_MODE_SEARCH:
    mode_str = "SEARCH";
    break;
  case INPUT_MODE_VISUAL:
    mode_str = "VISUAL";
    break;
  default:
    break;
  }

  int max_x = g_ncurses->getmaxx(mux->status_bar);
  g_ncurses->mvwprintw(mux->status_bar, 0, max_x - 20, " Mode: %s ", mode_str);

  g_ncurses->wrefresh(mux->status_bar);

  return APP_SUCCESS;
}

// Render command line
app_error app_tui_mux_render_command_line(app_tui_mux_t *mux) {
  if (!mux || !mux->command_line) {
    return APP_ERROR_INVALID_ARG;
  }

  g_ncurses->wclear(mux->command_line);
  g_ncurses->mvwprintw(mux->command_line, 0, 0, ":%s", mux->command_buffer);

  // Position cursor
  g_ncurses->wmove(mux->command_line, 0, 1 + (int)mux->command_cursor);
  g_ncurses->wrefresh(mux->command_line);

  return APP_SUCCESS;
}

// Save workspace
app_error app_tui_mux_save_workspace(app_tui_mux_t *mux, const char *path) {
  return app_tui_workspace_save(mux, path);
}

// Load workspace
app_error app_tui_mux_load_workspace(app_tui_mux_t *mux, const char *path) {
  return app_tui_workspace_load(mux, path);
}

// Remove pane from multiplexer
app_error app_tui_mux_remove_pane(app_tui_mux_t *mux, app_tui_pane_t *pane) {
  if (!mux || !mux->pane_manager || !pane) {
    return APP_ERROR_INVALID_ARG;
  }

  // Remove pane from pane manager
  app_error err = app_tui_pane_destroy(mux->pane_manager, pane);
  if (err != APP_SUCCESS) {
    return err;
  }

  // Recalculate layout
  return app_tui_layout_calculate(mux->layout_manager, mux->pane_manager->panes, mux->pane_manager->pane_count);
}

// Get terminal size using ioctl
static app_error app_get_terminal_size(int *width, int *height) {
  if (!width || !height) {
    return APP_ERROR_INVALID_ARG;
  }

  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
    return APP_ERROR_IO;
  }

  *width = ws.ws_col;
  *height = ws.ws_row;
  
  return APP_SUCCESS;
}

// Set layout mode
app_error app_tui_mux_set_layout(app_tui_mux_t *mux, app_tui_layout_mode_t mode) {
  if (!mux || !mux->layout_manager) {
    return APP_ERROR_INVALID_ARG;
  }

  // Update layout mode
  mux->layout_manager->mode = mode;

  // Recalculate layout
  return app_tui_layout_calculate(mux->layout_manager, mux->pane_manager->panes, mux->pane_manager->pane_count);
}