#include "tui_mux.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../utils/logging.h"
#include "../utils/memory.h"
#include "tui_ncurses.h"

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

  // Get terminal size
  int width = tui_get_max_x();
  int height = tui_get_max_y();

  err = app_tui_layout_create(&m->layout_manager, width, height);
  if (err != APP_SUCCESS) {
    app_tui_pane_manager_destroy(m->pane_manager);
    app_free(m);
    return err;
  }

  // Initialize command buffer
  m->command_buffer_size = 256;
  m->command_buffer = app_calloc(m->command_buffer_size, 1);
  if (!m->command_buffer) {
    app_tui_layout_destroy(m->layout_manager);
    app_tui_pane_manager_destroy(m->pane_manager);
    app_free(m);
    return APP_ERROR_MEMORY;
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
  app_tui_pane_manager_destroy(mux->pane_manager);
  app_tui_layout_destroy(mux->layout_manager);

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
  app_tui_mux_render(mux);

  // Main event loop
  while (mux->running && mux->state != MUX_STATE_EXITING) {
    // Handle input
    int ch = tui_get_char();
    if (ch != ERR) {
      app_tui_mux_handle_input(mux, ch);
    }

    // TODO: Poll PTYs for output

    // Render if needed
    if (mux->pane_manager->needs_refresh) {
      app_tui_mux_render(mux);
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

    default:
      // Alt+number for direct pane access
      if (key >= '1' && key <= '9') {
        return app_tui_mux_focus_number(mux, key - '0');
      }
      break;
    }
    break;

  case INPUT_MODE_COMMAND:
    // TODO: Handle command mode input
    if (key == 27) {  // ESC
      return app_tui_mux_exit_command_mode(mux);
    }
    break;

  default:
    break;
  }

  return APP_SUCCESS;
}

// Enter command mode
app_error app_tui_mux_enter_command_mode(app_tui_mux_t *mux) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  mux->input_mode = INPUT_MODE_COMMAND;
  memset(mux->command_buffer, 0, mux->command_buffer_size);
  mux->command_cursor = 0;

  // Create command line window if needed
  if (!mux->command_line) {
    int max_y = tui_get_max_y();
    int max_x = tui_get_max_x();
    mux->command_line = g_ncurses->newwin(1, max_x, max_y - 1, 0);
  }

  return APP_SUCCESS;
}

// Exit command mode
app_error app_tui_mux_exit_command_mode(app_tui_mux_t *mux) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  mux->input_mode = INPUT_MODE_NORMAL;

  // Clear command line
  if (mux->command_line) {
    g_ncurses->wclear(mux->command_line);
    g_ncurses->wrefresh(mux->command_line);
  }

  return APP_SUCCESS;
}

// Focus pane by number
app_error app_tui_mux_focus_number(app_tui_mux_t *mux, int number) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  return app_tui_pane_focus_by_number(mux->pane_manager, number);
}

// Focus pane by direction
app_error app_tui_mux_focus_direction(app_tui_mux_t *mux, int dx, int dy) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  // TODO: Implement directional navigation based on layout
  // For now, use simple next/prev
  if (dx > 0 || dy > 0) {
    return app_tui_pane_focus_next(mux->pane_manager);
  } else {
    return app_tui_pane_focus_prev(mux->pane_manager);
  }
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
    case PANE_STATE_BUSY:
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
    for (int y = pane->geometry.y; y < pane->geometry.y + pane->geometry.height;
         y++) {
      g_ncurses->mvwaddch(stdscr, y, pane->geometry.x, '|');
      g_ncurses->mvwaddch(stdscr, y, pane->geometry.x + pane->geometry.width - 1, '|');
    }
    for (int x = pane->geometry.x; x < pane->geometry.x + pane->geometry.width;
         x++) {
      g_ncurses->mvwaddch(stdscr, pane->geometry.y, x, '-');
      g_ncurses->mvwaddch(stdscr, pane->geometry.y + pane->geometry.height - 1, x, '-');
    }

    // Draw corners
    g_ncurses->mvwaddch(stdscr, pane->geometry.y, pane->geometry.x, '+');
    g_ncurses->mvwaddch(stdscr, pane->geometry.y,
                       pane->geometry.x + pane->geometry.width - 1, '+');
    g_ncurses->mvwaddch(stdscr, pane->geometry.y + pane->geometry.height - 1,
                       pane->geometry.x, '+');
    g_ncurses->mvwaddch(stdscr, pane->geometry.y + pane->geometry.height - 1,
                       pane->geometry.x + pane->geometry.width - 1, '+');

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
  app_tui_mux_render_status_bar(mux);

  // Render command line if in command mode
  if (mux->input_mode == INPUT_MODE_COMMAND) {
    app_tui_mux_render_command_line(mux);
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
  g_ncurses->wrefresh(mux->command_line);

  return APP_SUCCESS;
}