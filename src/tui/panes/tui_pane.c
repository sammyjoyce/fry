#include "tui_pane.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <utils/logging.h>
#include <utils/memory.h>
#include "tui.h"
#include "tui_term_emulator.h"

// Create pane manager
app_error app_tui_pane_manager_create(app_tui_pane_manager_t **manager,
                                      size_t max_panes) {
  if (!manager || max_panes == 0 || max_panes > 9) {
    return APP_ERROR_INVALID_ARG;
  }

  app_tui_pane_manager_t *mgr = app_malloc(sizeof(app_tui_pane_manager_t));
  if (!mgr) {
    return APP_ERROR_MEMORY;
  }

  mgr->panes = app_calloc(max_panes, sizeof(app_tui_pane_t *));
  if (!mgr->panes) {
    app_free(mgr);
    return APP_ERROR_MEMORY;
  }

  mgr->pane_count = 0;
  mgr->max_panes = max_panes;
  mgr->focused_pane = NULL;
  mgr->terminal_width = 80;   // Default, will be updated
  mgr->terminal_height = 24;  // Default, will be updated
  mgr->needs_refresh = true;

  *manager = mgr;
  return APP_SUCCESS;
}

// Destroy pane manager
app_error app_tui_pane_manager_destroy(app_tui_pane_manager_t *manager) {
  if (!manager) {
    return APP_SUCCESS;
  }

  // Destroy all panes
  for (size_t i = 0; i < manager->pane_count; i++) {
    if (manager->panes[i]) {
      app_error err = app_tui_pane_destroy(manager, manager->panes[i]);
      if (err != APP_SUCCESS) {
        LOG_WARNING("Failed to destroy pane: %s", app_error_string(err));
      }
    }
  }

  app_free(manager->panes);
  app_free(manager);

  return APP_SUCCESS;
}

// Create a new pane
app_error app_tui_pane_create(app_tui_pane_manager_t *manager,
                              const char *account_id,
                              const app_pane_rect_t *geometry,
                              app_tui_pane_t **pane) {
  if (!manager || !geometry || !pane) {
    return APP_ERROR_INVALID_ARG;
  }

  if (manager->pane_count >= manager->max_panes) {
    return APP_ERROR_RESOURCE;
  }

  app_tui_pane_t *p = app_calloc(1, sizeof(app_tui_pane_t));
  if (!p) {
    return APP_ERROR_MEMORY;
  }

  // Generate unique ID
  char id_buf[32];
  snprintf(id_buf, sizeof(id_buf), "pane_%zu_%d", manager->pane_count,
           getpid());
  p->id = app_strdup(id_buf);

  // Set pane number (1-based for user display)
  p->number = manager->pane_count + 1;

  // Copy account ID if provided
  if (account_id) {
    p->account_id = app_strdup(account_id);
  }

  // Copy geometry
  p->geometry = *geometry;

  // Initialize state
  p->state = PANE_STATE_INACTIVE;
  p->has_focus = false;
  p->last_activity = time(NULL);

  // Initialize PTY to invalid
  p->pty_master = -1;
  p->pty_slave = -1;
  p->process_pid = -1;

  // Initialize buffer
  p->buffer_size = 1000;  // Lines of scrollback
  p->buffer = app_calloc(p->buffer_size, sizeof(char *));
  if (!p->buffer) {
    app_free(p->id);
    app_free(p->account_id);
    app_free(p);
    return APP_ERROR_MEMORY;
  }
  p->buffer_lines = 0;
  p->scroll_offset = 0;

  // Create terminal emulator
  p->term_emulator =
      tui_term_emulator_create(geometry->width - 2,   // Account for borders
                               geometry->height - 2,  // Account for borders
                               p->buffer_size         // Scrollback size
      );
  if (!p->term_emulator) {
    LOG_ERROR("Failed to create terminal emulator for pane");
    app_free(p->buffer);
    app_free(p->id);
    app_free(p->account_id);
    app_free(p);
    return APP_ERROR_MEMORY;
  }

  // Create NCurses windows
  // Border window is the full pane size
  p->border_window = tui_create_window(geometry->height, geometry->width,
                                       geometry->y, geometry->x);
  if (!p->border_window) {
    app_free(p->buffer);
    app_free(p->id);
    app_free(p->account_id);
    app_free(p);
    return APP_ERROR_RESOURCE;
  }

  // Content window is inside the border (1 char smaller on each side)
  if (geometry->width > 2 && geometry->height > 2) {
    p->window = tui_create_window(geometry->height - 2, geometry->width - 2,
                                  geometry->y + 1, geometry->x + 1);
    if (!p->window) {
      tui_destroy_window(p->border_window);
      app_free(p->buffer);
      app_free(p->id);
      app_free(p->account_id);
      app_free(p);
      return APP_ERROR_RESOURCE;
    }
  }

  // Add to manager
  manager->panes[manager->pane_count++] = p;

  // Focus if first pane
  if (manager->pane_count == 1) {
    manager->focused_pane = p;
    p->has_focus = true;
  }

  *pane = p;
  LOG_DEBUG("Created pane %s (#%d) for account %s", p->id, p->number,
            account_id ? account_id : "(none)");

  return APP_SUCCESS;
}

// Destroy a pane
app_error app_tui_pane_destroy(app_tui_pane_manager_t *manager,
                               app_tui_pane_t *pane) {
  if (!manager || !pane) {
    return APP_ERROR_INVALID_ARG;
  }

  // Find pane in manager
  size_t index = 0;
  bool found = false;
  for (size_t i = 0; i < manager->pane_count; i++) {
    if (manager->panes[i] == pane) {
      index = i;
      found = true;
      break;
    }
  }

  if (!found) {
    return APP_ERROR_NOT_FOUND;
  }

  // Close PTY if open
  if (pane->pty_master >= 0) {
    close(pane->pty_master);
  }
  if (pane->pty_slave >= 0) {
    close(pane->pty_slave);
  }

  // Kill process if running
  if (pane->process_pid > 0) {
    kill(pane->process_pid, SIGTERM);
  }

  // Free NCurses windows
  if (pane->window) {
    tui_destroy_window(pane->window);
  }
  if (pane->border_window) {
    tui_destroy_window(pane->border_window);
  }

  // Free buffer
  if (pane->buffer) {
    for (size_t i = 0; i < pane->buffer_lines; i++) {
      app_free(pane->buffer[i]);
    }
    app_free(pane->buffer);
  }

  // Free terminal emulator
  if (pane->term_emulator) {
    tui_term_emulator_destroy(pane->term_emulator);
  }

  // Free strings
  app_free(pane->id);
  app_free(pane->session_id);
  app_free(pane->account_id);
  app_free(pane->pty_name);
  app_free(pane->title);

  // Remove from manager
  for (size_t i = index; i < manager->pane_count - 1; i++) {
    manager->panes[i] = manager->panes[i + 1];
    // Update pane numbers
    manager->panes[i]->number = i + 1;
  }
  manager->pane_count--;

  // Update focus if needed
  if (manager->focused_pane == pane) {
    if (manager->pane_count > 0) {
      manager->focused_pane = manager->panes[0];
      manager->focused_pane->has_focus = true;
    } else {
      manager->focused_pane = NULL;
    }
  }

  app_free(pane);
  manager->needs_refresh = true;

  return APP_SUCCESS;
}

// Focus a pane
app_error app_tui_pane_focus(app_tui_pane_manager_t *manager,
                             app_tui_pane_t *pane) {
  if (!manager || !pane) {
    return APP_ERROR_INVALID_ARG;
  }

  // Unfocus current
  if (manager->focused_pane) {
    manager->focused_pane->has_focus = false;
    manager->focused_pane->last_focus_time = time(NULL);
  }

  // Focus new
  manager->focused_pane = pane;
  pane->has_focus = true;
  pane->last_activity = time(NULL);

  manager->needs_refresh = true;

  LOG_DEBUG("Focused pane %s (#%d)", pane->id, pane->number);

  return APP_SUCCESS;
}

// Focus pane by number
app_error app_tui_pane_focus_by_number(app_tui_pane_manager_t *manager,
                                       int number) {
  if (!manager || number < 1 || number > 9) {
    return APP_ERROR_INVALID_ARG;
  }

  for (size_t i = 0; i < manager->pane_count; i++) {
    if (manager->panes[i]->number == number) {
      return app_tui_pane_focus(manager, manager->panes[i]);
    }
  }

  return APP_ERROR_NOT_FOUND;
}

// Focus next pane
app_error app_tui_pane_focus_next(app_tui_pane_manager_t *manager) {
  if (!manager || manager->pane_count == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  if (!manager->focused_pane) {
    return app_tui_pane_focus(manager, manager->panes[0]);
  }

  // Find current index
  size_t current = 0;
  for (size_t i = 0; i < manager->pane_count; i++) {
    if (manager->panes[i] == manager->focused_pane) {
      current = i;
      break;
    }
  }

  // Focus next (with wrap)
  size_t next = (current + 1) % manager->pane_count;
  return app_tui_pane_focus(manager, manager->panes[next]);
}

// Focus previous pane
app_error app_tui_pane_focus_prev(app_tui_pane_manager_t *manager) {
  if (!manager || manager->pane_count == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  if (!manager->focused_pane) {
    return app_tui_pane_focus(manager, manager->panes[0]);
  }

  // Find current index
  size_t current = 0;
  for (size_t i = 0; i < manager->pane_count; i++) {
    if (manager->panes[i] == manager->focused_pane) {
      current = i;
      break;
    }
  }

  // Focus previous (with wrap)
  size_t prev = (current == 0) ? manager->pane_count - 1 : current - 1;
  return app_tui_pane_focus(manager, manager->panes[prev]);
}

// Set pane state
app_error app_tui_pane_set_state(app_tui_pane_t *pane, app_pane_state_t state) {
  if (!pane) {
    return APP_ERROR_INVALID_ARG;
  }

  pane->state = state;
  pane->last_activity = time(NULL);

  return APP_SUCCESS;
}

// Set pane title
app_error app_tui_pane_set_title(app_tui_pane_t *pane, const char *title) {
  if (!pane) {
    return APP_ERROR_INVALID_ARG;
  }

  app_free(pane->title);
  pane->title = title ? app_strdup(title) : NULL;

  return APP_SUCCESS;
}

// Write data to pane
app_error app_tui_pane_write(app_tui_pane_t *pane, const char *data,
                             size_t len) {
  if (!pane || !data || len == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  // TODO: Implement proper terminal emulation and ANSI parsing
  // For now, just add to buffer line by line

  const char *start = data;
  const char *end = data + len;

  while (start < end) {
    const char *newline = memchr(start, '\n', end - start);
    if (!newline) {
      newline = end;
    }

    size_t line_len = newline - start;
    if (line_len > 0) {
      // Add line to buffer
      if (pane->buffer_lines >= pane->buffer_size) {
        // Buffer full, remove oldest line
        app_free(pane->buffer[0]);
        memmove(pane->buffer, pane->buffer + 1,
                (pane->buffer_size - 1) * sizeof(char *));
        pane->buffer_lines--;
      }

      char *line = app_malloc(line_len + 1);
      if (line) {
        memcpy(line, start, line_len);
        line[line_len] = '\0';
        pane->buffer[pane->buffer_lines++] = line;
      }
    }

    start = newline + 1;
  }

  pane->last_activity = time(NULL);

  return APP_SUCCESS;
}

// Clear pane content
app_error app_tui_pane_clear(app_tui_pane_t *pane) {
  if (!pane) {
    return APP_ERROR_INVALID_ARG;
  }

  // Free all buffer lines
  for (size_t i = 0; i < pane->buffer_lines; i++) {
    app_free(pane->buffer[i]);
    pane->buffer[i] = NULL;
  }
  pane->buffer_lines = 0;
  pane->scroll_offset = 0;

  return APP_SUCCESS;
}

// Resize pane
app_error app_tui_pane_resize(app_tui_pane_t *pane,
                              const app_pane_rect_t *new_geometry) {
  if (!pane || !new_geometry) {
    return APP_ERROR_INVALID_ARG;
  }

  pane->geometry = *new_geometry;

  // Destroy old windows
  if (pane->window) {
    tui_destroy_window(pane->window);
    pane->window = NULL;
  }
  if (pane->border_window) {
    tui_destroy_window(pane->border_window);
    pane->border_window = NULL;
  }

  // Create new windows with new geometry
  pane->border_window =
      tui_create_window(new_geometry->height, new_geometry->width,
                        new_geometry->y, new_geometry->x);
  if (!pane->border_window) {
    return APP_ERROR_RESOURCE;
  }

  // Create content window if there's room
  if (new_geometry->width > 2 && new_geometry->height > 2) {
    pane->window =
        tui_create_window(new_geometry->height - 2, new_geometry->width - 2,
                          new_geometry->y + 1, new_geometry->x + 1);
    if (!pane->window) {
      tui_destroy_window(pane->border_window);
      pane->border_window = NULL;
      return APP_ERROR_RESOURCE;
    }
  }

  // Adjust scroll offset if needed
  int visible_lines = new_geometry->height - 2;
  if (visible_lines > 0 && pane->buffer_lines > (size_t)visible_lines) {
    // Make sure we're not scrolled past the content
    int max_scroll = (int)pane->buffer_lines - visible_lines;
    if ((int)pane->scroll_offset > max_scroll) {
      pane->scroll_offset = (size_t)max_scroll;
    }
  } else {
    pane->scroll_offset = 0;
  }

  return APP_SUCCESS;
}

// Refresh pane display
app_error app_tui_pane_refresh(app_tui_pane_t *pane) {
  if (!pane) {
    return APP_ERROR_INVALID_ARG;
  }

  if (!pane->border_window) {
    return APP_ERROR_NOT_INITIALIZED;
  }

  // Draw border
  if (pane->has_focus) {
    tui_set_color(pane->border_window->win, TUI_COLOR_FOCUSED_BORDER);
  } else {
    tui_set_color(pane->border_window->win, TUI_COLOR_BORDER);
  }

  tui_draw_box(pane->border_window, 0, 0, pane->geometry.height,
               pane->geometry.width);

  // Draw pane number in top-left corner
  char pane_num[4];
  snprintf(pane_num, sizeof(pane_num), "[%d]", pane->number);
  tui_mvprint(pane->border_window, 0, 1, pane_num);

  // Draw title if set
  if (pane->title) {
    int max_title_len =
        pane->geometry.width - 8;  // Leave room for number and padding
    if (max_title_len > 0) {
      char title_buf[256];
      snprintf(title_buf, sizeof(title_buf), " %.100s ", pane->title);
      if (strlen(title_buf) > (size_t)max_title_len) {
        title_buf[max_title_len - 3] = '.';
        title_buf[max_title_len - 2] = '.';
        title_buf[max_title_len - 1] = '.';
        title_buf[max_title_len] = '\0';
      }
      tui_mvprint(pane->border_window, 0, 5, title_buf);
    }
  }

  // Draw status in bottom border
  const char *status = "";
  switch (pane->state) {
  case PANE_STATE_INACTIVE:
    status = " [INACTIVE] ";
    break;
  case PANE_STATE_CONNECTING:
    status = " [CONNECTING...] ";
    break;
  case PANE_STATE_ACTIVE:
    if (pane->process_pid > 0) {
      status = " [ACTIVE] ";
    }
    break;
  case PANE_STATE_DISCONNECTED:
    status = " [DISCONNECTED] ";
    break;
  case PANE_STATE_ERROR:
    status = " [ERROR] ";
    break;
  }

  if (strlen(status) > 0 && pane->geometry.width > (int)(strlen(status) + 2)) {
    tui_mvprint(pane->border_window, pane->geometry.height - 1,
                pane->geometry.width - strlen(status) - 1, status);
  }

  tui_refresh_window(pane->border_window);

  // Draw content if we have a content window
  if (pane->window) {
    tui_clear_window(pane->window);

    // Calculate visible lines
    int visible_lines = pane->geometry.height - 2;
    int start_line = 0;

    if (pane->buffer_lines > (size_t)visible_lines) {
      // Apply scroll offset
      start_line = pane->buffer_lines - visible_lines - pane->scroll_offset;
      if (start_line < 0)
        start_line = 0;
    }

    // Draw buffer content
    int y = 0;
    for (size_t i = start_line; i < pane->buffer_lines && y < visible_lines;
         i++) {
      if (pane->buffer[i]) {
        tui_mvprint(pane->window, y, 0, pane->buffer[i]);
      }
      y++;
    }

    // Show scroll indicator if needed
    if (pane->buffer_lines > (size_t)visible_lines) {
      char scroll_info[32];
      int visible_end = start_line + visible_lines;
      snprintf(scroll_info, sizeof(scroll_info), "[%d-%d/%zu]", start_line + 1,
               visible_end, pane->buffer_lines);

      // Draw in bottom-right of content area
      int info_x = pane->geometry.width - strlen(scroll_info) - 3;
      if (info_x > 0) {
        tui_mvprint(pane->window, visible_lines - 1, info_x, scroll_info);
      }
    }

    tui_refresh_window(pane->window);
  }

  return APP_SUCCESS;
}

// Scroll pane content
app_error app_tui_pane_scroll(app_tui_pane_t *pane, int delta) {
  if (!pane) {
    return APP_ERROR_INVALID_ARG;
  }

  int visible_lines = pane->geometry.height - 2;
  if (visible_lines <= 0 || pane->buffer_lines <= (size_t)visible_lines) {
    // No scrolling needed
    return APP_SUCCESS;
  }

  int max_scroll = pane->buffer_lines - visible_lines;
  int new_offset = pane->scroll_offset + delta;

  // Clamp to valid range
  if (new_offset < 0) {
    new_offset = 0;
  } else if (new_offset > max_scroll) {
    new_offset = max_scroll;
  }

  if (new_offset != (int)pane->scroll_offset) {
    pane->scroll_offset = new_offset;
    return app_tui_pane_refresh(pane);
  }

  return APP_SUCCESS;
}

// Scroll to specific position
app_error app_tui_pane_scroll_to(app_tui_pane_t *pane, int position) {
  if (!pane) {
    return APP_ERROR_INVALID_ARG;
  }

  int visible_lines = pane->geometry.height - 2;
  if (visible_lines <= 0 || pane->buffer_lines <= (size_t)visible_lines) {
    // No scrolling needed
    pane->scroll_offset = 0;
    return APP_SUCCESS;
  }

  int max_scroll = pane->buffer_lines - visible_lines;

  if (position < 0) {
    // Negative means from end
    position = max_scroll + position + 1;
  }

  // Clamp to valid range
  if (position < 0) {
    position = 0;
  } else if (position > max_scroll) {
    position = max_scroll;
  }

  if (position != (int)pane->scroll_offset) {
    pane->scroll_offset = position;
    return app_tui_pane_refresh(pane);
  }

  return APP_SUCCESS;
}

// Get pane by ID
app_tui_pane_t *app_tui_pane_get_by_id(app_tui_pane_manager_t *manager,
                                       const char *id) {
  if (!manager || !id) {
    return NULL;
  }

  for (size_t i = 0; i < manager->pane_count; i++) {
    if (manager->panes[i] && strcmp(manager->panes[i]->id, id) == 0) {
      return manager->panes[i];
    }
  }

  return NULL;
}

// Get pane by number
app_tui_pane_t *app_tui_pane_get_by_number(app_tui_pane_manager_t *manager,
                                           int number) {
  if (!manager || number < 1 || number > 9) {
    return NULL;
  }

  for (size_t i = 0; i < manager->pane_count; i++) {
    if (manager->panes[i] && manager->panes[i]->number == number) {
      return manager->panes[i];
    }
  }

  return NULL;
}

// Update terminal size
app_error app_tui_pane_manager_update_size(app_tui_pane_manager_t *manager,
                                           int width, int height) {
  if (!manager || width <= 0 || height <= 0) {
    return APP_ERROR_INVALID_ARG;
  }

  manager->terminal_width = width;
  manager->terminal_height = height;
  manager->needs_refresh = true;

  return APP_SUCCESS;
}

// Render pane content using terminal emulator
app_error app_tui_pane_render(app_tui_pane_t *pane) {
  if (!pane || !pane->window) {
    return APP_ERROR_INVALID_ARG;
  }

  // Clear the window
  tui_clear_window(pane->window);

  if (pane->term_emulator) {
    // Render using terminal emulator
    int width = pane->geometry.width - 2;    // Account for borders
    int height = pane->geometry.height - 2;  // Account for borders

    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        const tui_term_cell_t *cell =
            tui_term_emulator_get_cell(pane->term_emulator, x, y);
        if (cell) {
          // Convert attributes to NCurses
          int attrs = 0;
          if (cell->attrs & TERM_ATTR_BOLD)
            attrs |= A_BOLD;
          if (cell->attrs & TERM_ATTR_DIM)
            attrs |= A_DIM;
          if (cell->attrs & TERM_ATTR_UNDERLINE)
            attrs |= A_UNDERLINE;
          if (cell->attrs & TERM_ATTR_REVERSE)
            attrs |= A_REVERSE;

          // Set attributes
          if (attrs) {
            wattron(pane->window->win, attrs);
          }

          // TODO: Handle colors properly
          // For now, just render the character
          if (cell->ch < 128) {
            mvwaddch(pane->window->win, y, x, cell->ch);
          } else {
            // Unicode - render as ?
            mvwaddch(pane->window->win, y, x, '?');
          }

          // Clear attributes
          if (attrs) {
            wattroff(pane->window->win, attrs);
          }
        }
      }
    }

    // Show cursor if visible and pane has focus
    if (pane->has_focus) {
      int cursor_x, cursor_y;
      bool cursor_visible;
      tui_term_emulator_get_cursor(pane->term_emulator, &cursor_x, &cursor_y,
                                   &cursor_visible);

      if (cursor_visible && cursor_x >= 0 && cursor_x < width &&
          cursor_y >= 0 && cursor_y < height) {
        wmove(pane->window->win, cursor_y, cursor_x);
        curs_set(1);
      }
    }
  } else {
    // Fallback to simple buffer rendering
    int y = 0;
    size_t start_line = 0;

    if (pane->buffer_lines > 0) {
      // Calculate starting line based on scroll offset
      if (pane->scroll_offset < pane->buffer_lines) {
        start_line = pane->buffer_lines - pane->scroll_offset - 1;
      }

      // Render lines
      for (size_t i = start_line;
           i < pane->buffer_lines && y < pane->geometry.height - 2; i++) {
        if (pane->buffer[i]) {
          mvwprintw(pane->window->win, y, 0, "%.*s", pane->geometry.width - 2,
                    pane->buffer[i]);
        }
        y++;
      }
    }
  }

  // Refresh the window
  tui_refresh_window(pane->window);

  return APP_SUCCESS;
}

// Render pane border
app_error app_tui_pane_render_border(app_tui_pane_t *pane) {
  if (!pane || !pane->border_window) {
    return APP_ERROR_INVALID_ARG;
  }

  // Set border color based on focus
  if (pane->has_focus) {
    tui_set_color(pane->border_window->win, TUI_COLOR_FOCUSED_BORDER);
  } else {
    tui_set_color(pane->border_window->win, TUI_COLOR_BORDER);
  }

  // Draw border
  tui_draw_border(pane->border_window);

  // Add pane number in top-left corner
  mvwprintw(pane->border_window->win, 0, 2, "[%d]", pane->number);

  // Add account name if available
  if (pane->account_id) {
    mvwprintw(pane->border_window->win, 0, 7, " %s ", pane->account_id);
  }

  // Add state indicator in top-right
  const char *state_str = "";
  switch (pane->state) {
  case PANE_STATE_INACTIVE:
    state_str = "[INACTIVE]";
    break;
  case PANE_STATE_CONNECTING:
    state_str = "[CONNECTING]";
    break;
  case PANE_STATE_ACTIVE:
    state_str = "[ACTIVE]";
    break;
  case PANE_STATE_DISCONNECTED:
    state_str = "[DISCONNECTED]";
    break;
  case PANE_STATE_ERROR:
    state_str = "[ERROR]";
    break;
  }

  int state_len = strlen(state_str);
  if (state_len > 0 && pane->geometry.width > state_len + 2) {
    mvwprintw(pane->border_window->win, 0, pane->geometry.width - state_len - 1,
              "%s", state_str);
  }

  // Refresh border window
  tui_refresh_window(pane->border_window);

  return APP_SUCCESS;
}

// Get pane count
size_t app_tui_pane_manager_get_count(app_tui_pane_manager_t *manager) {
  if (!manager) {
    return 0;
  }
  return manager->pane_count;
}

// Get pane by index
app_tui_pane_t *app_tui_pane_manager_get_by_index(
    app_tui_pane_manager_t *manager, size_t index) {
  if (!manager || index >= manager->pane_count) {
    return NULL;
  }
  return manager->panes[index];
}

// Get focused pane
app_tui_pane_t *app_tui_pane_manager_get_focused(
    app_tui_pane_manager_t *manager) {
  if (!manager) {
    return NULL;
  }
  return manager->focused_pane;
}

// Get pane number
int app_tui_pane_get_number(app_tui_pane_t *pane) {
  if (!pane) {
    return 0;
  }
  return pane->number;
}
