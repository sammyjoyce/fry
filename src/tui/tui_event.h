#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>

// Forward declarations
typedef struct tui_t tui_t;
typedef struct tui_render_t tui_render_t;

// Event types
typedef enum {
  TUI_EVENT_NONE = 0,
  TUI_EVENT_KEY,
  TUI_EVENT_RESIZE,
  TUI_EVENT_TIMER,
  TUI_EVENT_PTY_DATA,
  TUI_EVENT_QUIT
} tui_event_type_t;

// Key event data
typedef struct {
  int key;          // NCurses key code
  bool is_special;  // True for special keys (arrows, function keys, etc.)
} tui_key_event_t;

// Resize event data
typedef struct {
  int width;
  int height;
} tui_resize_event_t;

// PTY data event
typedef struct {
  int pty_fd;         // Which PTY has data
  size_t pane_index;  // Associated pane index
} tui_pty_event_t;

// Event structure
typedef struct {
  tui_event_type_t type;
  union {
    tui_key_event_t key;
    tui_resize_event_t resize;
    tui_pty_event_t pty;
  } data;
} tui_event_t;

// Event loop configuration
typedef struct {
  int refresh_rate_fps;     // Target frame rate (1-120)
  bool enable_mouse;        // Enable mouse support
  int input_timeout_ms;     // Timeout for input polling
  int max_events_per_loop;  // Maximum events to process per iteration
} tui_event_config_t;

// Event loop state
typedef struct tui_event_loop_t {
  tui_event_config_t config;
  tui_t *tui;
  tui_render_t *renderer;

  // Timing
  struct timeval last_frame_time;
  struct timeval frame_interval;
  uint64_t frame_count;

  // State
  bool running;
  bool needs_render;
  bool resize_pending;

  // File descriptors for select/poll
  int max_fd;
  fd_set read_fds;
  fd_set write_fds;

  // Statistics
  uint64_t events_processed;
  uint64_t frames_rendered;
  double avg_frame_time_ms;
} tui_event_loop_t;

// Initialize event loop with default configuration
tui_event_loop_t *tui_event_loop_create(tui_t *tui, tui_render_t *renderer);

// Initialize with custom configuration
tui_event_loop_t *tui_event_loop_create_with_config(
    tui_t *tui, tui_render_t *renderer, const tui_event_config_t *config);

// Destroy event loop
void tui_event_loop_destroy(tui_event_loop_t *loop);

// Run the event loop (blocking)
int tui_event_loop_run(tui_event_loop_t *loop);

// Stop the event loop
void tui_event_loop_stop(tui_event_loop_t *loop);

// Process a single iteration (non-blocking)
int tui_event_loop_iterate(tui_event_loop_t *loop);

// Add a file descriptor to monitor for reading
void tui_event_loop_add_fd(tui_event_loop_t *loop, int fd);

// Remove a file descriptor from monitoring
void tui_event_loop_remove_fd(tui_event_loop_t *loop, int fd);

// Force a render on next iteration
void tui_event_loop_request_render(tui_event_loop_t *loop);

// Get event loop statistics
void tui_event_loop_get_stats(const tui_event_loop_t *loop,
                              uint64_t *events_out, uint64_t *frames_out,
                              double *avg_frame_time_out);

// Signal handler for terminal resize
void tui_event_handle_sigwinch(int sig);