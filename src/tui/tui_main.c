#include "tui_main.h"

#include <string.h>

#include <core/error.h>
#include <utils/logging.h>
#include <utils/memory.h>
#include "tui/tui_event.h"
#include "tui/tui_layout.h"
#include "tui/mux/tui_mux.h"
#include "tui/backend/tui_ncurses.h"
#include "tui/panes/tui_pane.h"
#include "tui/tui_render.h"

tui_t *tui_create(void) {
  tui_t *tui = app_calloc(1, sizeof(tui_t));
  if (!tui) {
    LOG_ERROR("Failed to allocate TUI structure");
    return NULL;
  }

  // Set default configuration
  tui->enable_mouse = false;
  tui->refresh_rate_fps = 60;

  return tui;
}

void tui_destroy(tui_t *tui) {
  if (!tui)
    return;

  // Stop event loop if running
  if (tui->running) {
    tui_stop(tui);
  }

  // Destroy components in reverse order
  if (tui->event_loop) {
    tui_event_loop_destroy(tui->event_loop);
    tui->event_loop = NULL;
  }

  if (tui->renderer) {
    tui_render_destroy(tui->renderer);
    tui->renderer = NULL;
  }

  if (tui->mux) {
    tui_mux_destroy(tui->mux);
    tui->mux = NULL;
  }

  if (tui->ncurses) {
    tui_ncurses_destroy(tui->ncurses);
    tui->ncurses = NULL;
  }

  app_free(tui);
}

app_error tui_initialize(tui_t *tui) {
  if (!tui)
    return APP_ERROR_INVALID_ARGUMENT;
  if (tui->initialized)
    return APP_ERROR_NONE;

  // Initialize NCurses
  app_error err = app_ncurses_init(true);  // Use real ncurses
  if (err != APP_ERROR_NONE) {
    LOG_ERROR("Failed to initialize NCurses");
    return err;
  }

  // For now, we'll use the global ncurses state
  // TODO: Create proper ncurses abstraction instance
  // Get terminal size
  int width, height;
  tui_ncurses_get_size(tui->ncurses, &width, &height);

  // Create multiplexer
  tui->mux = tui_mux_create(width, height);
  if (!tui->mux) {
    LOG_ERROR("Failed to create multiplexer");
    tui_ncurses_destroy(tui->ncurses);
    tui->ncurses = NULL;
    return APP_ERROR_TUI;
  }

  // Create renderer
  tui->renderer = tui_render_create(tui->ncurses, tui->mux);
  if (!tui->renderer) {
    LOG_ERROR("Failed to create renderer");
    tui_mux_destroy(tui->mux);
    tui->mux = NULL;
    tui_ncurses_destroy(tui->ncurses);
    tui->ncurses = NULL;
    return APP_ERROR_TUI;
  }

  // Configure renderer
  tui_render_config_t render_config = {.target_fps = tui->refresh_rate_fps,
                                       .enable_dirty_tracking = true,
                                       .show_status_bar = true,
                                       .show_command_line = true};
  tui_render_set_config(tui->renderer, &render_config);

  // Create event loop
  tui_event_config_t event_config = {.refresh_rate_fps = tui->refresh_rate_fps,
                                     .enable_mouse = tui->enable_mouse,
                                     .input_timeout_ms = 10,
                                     .max_events_per_loop = 10};

  tui->event_loop =
      tui_event_loop_create_with_config(tui, tui->renderer, &event_config);
  if (!tui->event_loop) {
    LOG_ERROR("Failed to create event loop");
    tui_render_destroy(tui->renderer);
    tui->renderer = NULL;
    tui_mux_destroy(tui->mux);
    tui->mux = NULL;
    tui_ncurses_destroy(tui->ncurses);
    tui->ncurses = NULL;
    return APP_ERROR_TUI;
  }

  tui->initialized = true;
  LOG_INFO("TUI initialized successfully");
  return APP_ERROR_NONE;
}

app_error tui_run(tui_t *tui) {
  if (!tui || !tui->initialized) {
    return APP_ERROR_INVALID_ARGUMENT;
  }

  if (tui->running) {
    return APP_ERROR_INVALID_STATE;
  }

  tui->running = true;

  // Run the event loop
  int result = tui_event_loop_run(tui->event_loop);

  tui->running = false;

  return (result == 0) ? APP_ERROR_NONE : APP_ERROR_TUI;
}

void tui_stop(tui_t *tui) {
  if (!tui || !tui->event_loop)
    return;

  tui_event_loop_stop(tui->event_loop);
  tui->running = false;
}

tui_ncurses_t *tui_get_ncurses(tui_t *tui) {
  return tui ? tui->ncurses : NULL;
}

tui_mux_t *tui_get_mux(tui_t *tui) {
  return tui ? tui->mux : NULL;
}

tui_render_t *tui_get_renderer(tui_t *tui) {
  return tui ? tui->renderer : NULL;
}

tui_event_loop_t *tui_get_event_loop(tui_t *tui) {
  return tui ? tui->event_loop : NULL;
}

void tui_handle_input(tui_t *tui, int key) {
  if (!tui || !tui->mux)
    return;

  // Handle global keys first
  switch (key) {
  case 9:  // Tab - cycle focus
    tui_mux_cycle_focus(tui->mux);
    break;

  case 27:  // Escape
    // Could be used for command mode or cancel
    break;

  case KEY_F(1):  // F1 - Help
    // TODO: Show help
    break;

  case KEY_F(2):  // F2 - New pane
    // TODO: Create new pane
    break;

  case KEY_F(3):  // F3 - Close pane
    // TODO: Close current pane
    break;

  case KEY_F(10):  // F10 - Quit
    tui_stop(tui);
    break;

  default:
    // Forward to focused pane
    {
      app_tui_pane_t *focused = tui_mux_get_focused_pane(tui->mux);
      if (focused) {
        // TODO: Implement pane input handling
        // For now, just log the key
        LOG_DEBUG("Key %d sent to focused pane", key);
      }
    }
    break;
  }
}

void tui_handle_resize(tui_t *tui, int width, int height) {
  if (!tui)
    return;

  LOG_INFO("Handling resize to %dx%d", width, height);

  // Update multiplexer size
  if (tui->mux) {
    tui_mux_resize(tui->mux, width, height);
  }

  // Renderer will handle the rest during next render
}