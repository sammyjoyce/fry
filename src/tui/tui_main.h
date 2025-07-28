#pragma once

#include <stdbool.h>

#include "../core/error.h"

// Forward declarations
typedef struct tui_ncurses_t tui_ncurses_t;
typedef struct tui_mux_t tui_mux_t;
typedef struct tui_render_t tui_render_t;
typedef struct tui_event_loop_t tui_event_loop_t;

// Main TUI structure that ties all components together
typedef struct tui_t {
  tui_ncurses_t *ncurses;        // NCurses abstraction layer
  tui_mux_t *mux;                // Multiplexer with panes
  tui_render_t *renderer;        // Render engine
  tui_event_loop_t *event_loop;  // Event loop

  // State
  bool initialized;
  bool running;

  // Configuration
  bool enable_mouse;
  int refresh_rate_fps;
} tui_t;

// Create and initialize TUI
tui_t *tui_create(void);

// Destroy TUI and all components
void tui_destroy(tui_t *tui);

// Initialize all TUI components
app_error tui_initialize(tui_t *tui);

// Run the TUI (blocking)
app_error tui_run(tui_t *tui);

// Stop the TUI
void tui_stop(tui_t *tui);

// Component accessors
tui_ncurses_t *tui_get_ncurses(tui_t *tui);
tui_mux_t *tui_get_mux(tui_t *tui);
tui_render_t *tui_get_renderer(tui_t *tui);
tui_event_loop_t *tui_get_event_loop(tui_t *tui);

// Event handlers
void tui_handle_input(tui_t *tui, int key);
void tui_handle_resize(tui_t *tui, int width, int height);