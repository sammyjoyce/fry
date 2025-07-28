/*
 * TUI Multiplexer Navigation Commands
 * Implements Task 11: Pane navigation with vim-style controls
 */

#include <math.h>
#include <stdlib.h>
#include <time.h>

#include <core/error.h>
#include <utils/logging.h>
#include "tui/components/tui_input.h"
#include "tui/mux/tui_mux.h"
#include "tui/panes/tui_pane.h"

// Helper to calculate distance between pane centers
static double pane_distance(const app_tui_pane_t *p1,
                            const app_tui_pane_t *p2) {
  if (!p1 || !p2) {
    return INFINITY;
  }

  // Calculate center points
  double x1 = p1->geometry.x + p1->geometry.width / 2.0;
  double y1 = p1->geometry.y + p1->geometry.height / 2.0;
  double x2 = p2->geometry.x + p2->geometry.width / 2.0;
  double y2 = p2->geometry.y + p2->geometry.height / 2.0;

  // Euclidean distance
  double dx = x2 - x1;
  double dy = y2 - y1;
  return sqrt(dx * dx + dy * dy);
}

// Helper to check if pane is in the specified direction
static bool is_pane_in_direction(const app_tui_pane_t *from,
                                 const app_tui_pane_t *to, int dx, int dy) {
  if (!from || !to) {
    return false;
  }

  // Calculate center points
  double from_x = from->geometry.x + from->geometry.width / 2.0;
  double from_y = from->geometry.y + from->geometry.height / 2.0;
  double to_x = to->geometry.x + to->geometry.width / 2.0;
  double to_y = to->geometry.y + to->geometry.height / 2.0;

  // Check direction
  if (dx < 0) {  // Left
    return to_x < from_x;
  } else if (dx > 0) {  // Right
    return to_x > from_x;
  } else if (dy < 0) {  // Up
    return to_y < from_y;
  } else if (dy > 0) {  // Down
    return to_y > from_y;
  }

  return false;
}

// Focus pane in specified direction (vim-style hjkl navigation)
app_error app_tui_mux_focus_direction(app_tui_mux_t *mux, int dx, int dy) {
  if (!mux || !mux->pane_manager) {
    return APP_ERROR_INVALID_ARG;
  }

  app_tui_pane_manager_t *pm = mux->pane_manager;
  if (pm->pane_count == 0) {
    return APP_SUCCESS;  // No panes
  }

  // Get current focused pane
  app_tui_pane_t *current = NULL;
  for (size_t i = 0; i < pm->pane_count; i++) {
    if (pm->panes[i] && pm->panes[i]->has_focus) {
      current = pm->panes[i];
      break;
    }
  }

  if (!current) {
    // No focused pane, focus the first one
    return app_tui_pane_focus(pm, pm->panes[0]);
  }

  // Find the closest pane in the specified direction
  app_tui_pane_t *best = NULL;
  double best_distance = INFINITY;

  for (size_t i = 0; i < pm->pane_count; i++) {
    app_tui_pane_t *pane = pm->panes[i];
    if (!pane || pane == current) {
      continue;
    }

    // Check if pane is in the right direction
    if (is_pane_in_direction(current, pane, dx, dy)) {
      double distance = pane_distance(current, pane);
      if (distance < best_distance) {
        best = pane;
        best_distance = distance;
      }
    }
  }

  // If no pane found in direction, optionally wrap around
  if (!best && mux->config.wrap_navigation) {
    // Find the furthest pane in the opposite direction
    for (size_t i = 0; i < pm->pane_count; i++) {
      app_tui_pane_t *pane = pm->panes[i];
      if (!pane || pane == current) {
        continue;
      }

      // Check opposite direction
      if (is_pane_in_direction(current, pane, -dx, -dy)) {
        double distance = pane_distance(current, pane);
        if (!best || distance > best_distance) {
          best = pane;
          best_distance = distance;
        }
      }
    }
  }

  if (best) {
    LOG_DEBUG("Navigating from pane %d to pane %d", current->number,
              best->number);
    return app_tui_pane_focus(pm, best);
  }

  return APP_SUCCESS;  // No pane to navigate to
}

// Focus pane by number (Alt+1 through Alt+9)
app_error app_tui_mux_focus_number(app_tui_mux_t *mux, int number) {
  if (!mux || !mux->pane_manager) {
    return APP_ERROR_INVALID_ARG;
  }

  if (number < 1 || number > 9) {
    return APP_ERROR_INVALID_ARG;
  }

  app_tui_pane_manager_t *pm = mux->pane_manager;

  // Find pane with matching number
  for (size_t i = 0; i < pm->pane_count; i++) {
    if (pm->panes[i] && pm->panes[i]->number == number) {
      LOG_DEBUG("Focusing pane %d by number", number);
      return app_tui_pane_focus(pm, pm->panes[i]);
    }
  }

  LOG_DEBUG("No pane with number %d", number);
  return APP_SUCCESS;  // No pane with that number
}

// Cycle through panes (Tab/Shift-Tab)
app_error app_tui_mux_cycle_focus(app_tui_mux_t *mux, bool reverse) {
  if (!mux || !mux->pane_manager) {
    return APP_ERROR_INVALID_ARG;
  }

  app_tui_pane_manager_t *pm = mux->pane_manager;
  if (pm->pane_count == 0) {
    return APP_SUCCESS;  // No panes
  }

  // Find current focused pane index
  int current_index = -1;
  for (size_t i = 0; i < pm->pane_count; i++) {
    if (pm->panes[i] && pm->panes[i]->has_focus) {
      current_index = (int)i;
      break;
    }
  }

  // Calculate next index
  int next_index;
  if (current_index < 0) {
    // No focused pane, start at first
    next_index = 0;
  } else if (reverse) {
    // Previous pane
    next_index = current_index - 1;
    if (next_index < 0) {
      next_index = (int)pm->pane_count - 1;
    }
  } else {
    // Next pane
    next_index = (current_index + 1) % (int)pm->pane_count;
  }

  if (next_index >= 0 && next_index < (int)pm->pane_count &&
      pm->panes[next_index]) {
    LOG_DEBUG("Cycling focus from pane %d to pane %d",
              current_index >= 0 ? pm->panes[current_index]->number : -1,
              pm->panes[next_index]->number);
    return app_tui_pane_focus(pm, pm->panes[next_index]);
  }

  return APP_SUCCESS;
}

// Focus last active pane (useful for toggling between two panes)
app_error app_tui_mux_focus_last(app_tui_mux_t *mux) {
  if (!mux || !mux->pane_manager) {
    return APP_ERROR_INVALID_ARG;
  }

  app_tui_pane_manager_t *pm = mux->pane_manager;

  // Find the pane that was focused before the current one
  app_tui_pane_t *last_focused = NULL;
  time_t last_focus_time = 0;
  app_tui_pane_t *current_focused = NULL;

  for (size_t i = 0; i < pm->pane_count; i++) {
    if (pm->panes[i]) {
      if (pm->panes[i]->has_focus) {
        current_focused = pm->panes[i];
      } else if (pm->panes[i]->last_focus_time > last_focus_time) {
        last_focused = pm->panes[i];
        last_focus_time = pm->panes[i]->last_focus_time;
      }
    }
  }

  if (last_focused) {
    LOG_DEBUG("Focusing last active pane %d", last_focused->number);
    return app_tui_pane_focus(pm, last_focused);
  }

  return APP_SUCCESS;  // No previous pane to focus
}

// Forward declarations for action callbacks
static app_error nav_action_left(void *context, const app_key_event_t *event);
static app_error nav_action_right(void *context, const app_key_event_t *event);
static app_error nav_action_up(void *context, const app_key_event_t *event);
static app_error nav_action_down(void *context, const app_key_event_t *event);
static app_error nav_action_cycle_next(void *context,
                                       const app_key_event_t *event);
static app_error nav_action_cycle_prev(void *context,
                                       const app_key_event_t *event);
static app_error nav_action_number(void *context, const app_key_event_t *event);
static app_error nav_action_window_prefix(void *context,
                                          const app_key_event_t *event);

// Register default navigation keybindings
app_error app_tui_mux_register_nav_keys(app_tui_mux_t *mux) {
  if (!mux || !mux->input_system) {
    return APP_ERROR_INVALID_ARG;
  }

  // Vim-style navigation
  app_error err;

  // h - left
  err = app_tui_input_bind_key(mux->input_system, 'h', KEY_MOD_NONE,
                               nav_action_left, mux);
  if (err != APP_SUCCESS)
    return err;

  // j - down
  err = app_tui_input_bind_key(mux->input_system, 'j', KEY_MOD_NONE,
                               nav_action_down, mux);
  if (err != APP_SUCCESS)
    return err;

  // k - up
  err = app_tui_input_bind_key(mux->input_system, 'k', KEY_MOD_NONE,
                               nav_action_up, mux);
  if (err != APP_SUCCESS)
    return err;

  // l - right
  err = app_tui_input_bind_key(mux->input_system, 'l', KEY_MOD_NONE,
                               nav_action_right, mux);
  if (err != APP_SUCCESS)
    return err;

  // Arrow keys
  err = app_tui_input_bind_key(mux->input_system, APP_KEY_LEFT, KEY_MOD_NONE,
                               nav_action_left, mux);
  if (err != APP_SUCCESS)
    return err;

  err = app_tui_input_bind_key(mux->input_system, APP_KEY_DOWN, KEY_MOD_NONE,
                               nav_action_down, mux);
  if (err != APP_SUCCESS)
    return err;

  err = app_tui_input_bind_key(mux->input_system, APP_KEY_UP, KEY_MOD_NONE,
                               nav_action_up, mux);
  if (err != APP_SUCCESS)
    return err;

  err = app_tui_input_bind_key(mux->input_system, APP_KEY_RIGHT, KEY_MOD_NONE,
                               nav_action_right, mux);
  if (err != APP_SUCCESS)
    return err;

  // Tab/Shift-Tab for cycling
  err = app_tui_input_bind_key(mux->input_system, APP_KEY_TAB, KEY_MOD_NONE,
                               nav_action_cycle_next, mux);
  if (err != APP_SUCCESS)
    return err;

  err = app_tui_input_bind_key(mux->input_system, APP_KEY_TAB, KEY_MOD_SHIFT,
                               nav_action_cycle_prev, mux);
  if (err != APP_SUCCESS)
    return err;

  // Alt+1-9 for numbered access
  for (int i = 1; i <= 9; i++) {
    err = app_tui_input_bind_key(mux->input_system, '0' + i, KEY_MOD_ALT,
                                 nav_action_number, mux);
    if (err != APP_SUCCESS)
      return err;
  }

  // Ctrl-W prefix for window commands (vim-style)
  err = app_tui_input_bind_key(mux->input_system, 'W', KEY_MOD_CTRL,
                               nav_action_window_prefix, mux);
  if (err != APP_SUCCESS)
    return err;

  LOG_INFO("Registered navigation keybindings");
  return APP_SUCCESS;
}

// Navigation action callbacks
static app_error nav_action_left(void *context, const app_key_event_t *event) {
  (void)event;
  return app_tui_mux_focus_direction((app_tui_mux_t *)context, -1, 0);
}

static app_error nav_action_right(void *context, const app_key_event_t *event) {
  (void)event;
  return app_tui_mux_focus_direction((app_tui_mux_t *)context, 1, 0);
}

static app_error nav_action_up(void *context, const app_key_event_t *event) {
  (void)event;
  return app_tui_mux_focus_direction((app_tui_mux_t *)context, 0, -1);
}

static app_error nav_action_down(void *context, const app_key_event_t *event) {
  (void)event;
  return app_tui_mux_focus_direction((app_tui_mux_t *)context, 0, 1);
}

static app_error nav_action_cycle_next(void *context,
                                       const app_key_event_t *event) {
  (void)event;
  return app_tui_mux_cycle_focus((app_tui_mux_t *)context, false);
}

static app_error nav_action_cycle_prev(void *context,
                                       const app_key_event_t *event) {
  (void)event;
  return app_tui_mux_cycle_focus((app_tui_mux_t *)context, true);
}

static app_error nav_action_number(void *context,
                                   const app_key_event_t *event) {
  if (event->code >= '1' && event->code <= '9') {
    return app_tui_mux_focus_number((app_tui_mux_t *)context,
                                    event->code - '0');
  }
  return APP_SUCCESS;
}

static app_error nav_action_window_prefix(void *context,
                                          const app_key_event_t *event) {
  (void)event;
  app_tui_mux_t *mux = (app_tui_mux_t *)context;

  // Enter window command mode
  mux->input_mode = INPUT_MODE_WINDOW;
  LOG_DEBUG("Entered window command mode (Ctrl-W)");

  return APP_SUCCESS;
}