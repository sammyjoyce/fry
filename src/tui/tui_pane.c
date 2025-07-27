#include "tui_pane.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../utils/logging.h"
#include "../utils/memory.h"

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

  // Resize NCurses windows if they exist
  if (pane->window) {
    // TODO: Implement NCurses window resizing
  }

  return APP_SUCCESS;
}

// Refresh pane display
app_error app_tui_pane_refresh(app_tui_pane_t *pane) {
  if (!pane) {
    return APP_ERROR_INVALID_ARG;
  }

  // TODO: Implement NCurses refresh

  return APP_SUCCESS;
}