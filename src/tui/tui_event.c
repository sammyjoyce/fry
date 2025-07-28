#include "tui_event.h"

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#include "../utils/logging.h"
#include "../utils/memory.h"
#include "tui.h"
#include "tui_main.h"
#include "tui_mux.h"
#include "tui_ncurses.h"
#include "tui_pane.h"
#include "tui_pty.h"
#include "tui_render.h"

// Global flag for resize handling
static volatile sig_atomic_t g_resize_pending = 0;

// Signal handler for SIGWINCH
void tui_event_handle_sigwinch(int sig) {
  (void)sig;
  g_resize_pending = 1;
}

// Calculate time difference in microseconds
static int64_t timeval_diff_us(const struct timeval *a,
                               const struct timeval *b) {
  return (a->tv_sec - b->tv_sec) * 1000000LL + (a->tv_usec - b->tv_usec);
}

// Add microseconds to timeval
static void timeval_add_us(struct timeval *tv, int64_t us) {
  tv->tv_usec += us;
  while (tv->tv_usec >= 1000000) {
    tv->tv_sec++;
    tv->tv_usec -= 1000000;
  }
}

// Get current time
static void get_current_time(struct timeval *tv) {
  gettimeofday(tv, NULL);
}

// Default event configuration
static tui_event_config_t default_config = {.refresh_rate_fps = 60,
                                            .enable_mouse = false,
                                            .input_timeout_ms = 10,
                                            .max_events_per_loop = 10};

tui_event_loop_t *tui_event_loop_create(tui_t *tui, tui_render_t *renderer) {
  return tui_event_loop_create_with_config(tui, renderer, &default_config);
}

tui_event_loop_t *tui_event_loop_create_with_config(
    tui_t *tui, tui_render_t *renderer, const tui_event_config_t *config) {
  if (!tui || !renderer || !config) {
    LOG_ERROR("Invalid parameters for event loop creation");
    return NULL;
  }

  // Validate configuration
  if (config->refresh_rate_fps < 1 || config->refresh_rate_fps > 120) {
    LOG_ERROR("Invalid refresh rate: %d (must be 1-120)",
              config->refresh_rate_fps);
    return NULL;
  }

  tui_event_loop_t *loop = app_calloc(1, sizeof(tui_event_loop_t));
  if (!loop) {
    LOG_ERROR("Failed to allocate event loop");
    return NULL;
  }

  // Copy configuration
  loop->config = *config;
  loop->tui = tui;
  loop->renderer = renderer;

  // Calculate frame interval
  int64_t frame_interval_us = 1000000 / config->refresh_rate_fps;
  loop->frame_interval.tv_sec = frame_interval_us / 1000000;
  loop->frame_interval.tv_usec = frame_interval_us % 1000000;

  // Initialize timing
  get_current_time(&loop->last_frame_time);

  // Initialize file descriptor sets
  FD_ZERO(&loop->read_fds);
  FD_ZERO(&loop->write_fds);
  loop->max_fd = 0;

  // Add stdin for keyboard input
  tui_event_loop_add_fd(loop, STDIN_FILENO);

  // Install signal handler for resize
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = tui_event_handle_sigwinch;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  if (sigaction(SIGWINCH, &sa, NULL) < 0) {
    LOG_WARN("Failed to install SIGWINCH handler: %s", strerror(errno));
  }

  // Enable mouse if requested
  if (config->enable_mouse) {
    // This would be done through NCurses
    LOG_DEBUG("Mouse support requested but not yet implemented");
  }

  LOG_INFO("Event loop created with %d FPS target", config->refresh_rate_fps);
  return loop;
}

void tui_event_loop_destroy(tui_event_loop_t *loop) {
  if (!loop)
    return;

  LOG_INFO("Event loop destroyed - processed %llu events, rendered %llu frames",
           (unsigned long long)loop->events_processed,
           (unsigned long long)loop->frames_rendered);

  app_free(loop);
}

void tui_event_loop_add_fd(tui_event_loop_t *loop, int fd) {
  if (!loop || fd < 0)
    return;

  FD_SET(fd, &loop->read_fds);
  if (fd > loop->max_fd) {
    loop->max_fd = fd;
  }

  LOG_DEBUG("Added fd %d to event loop", fd);
}

void tui_event_loop_remove_fd(tui_event_loop_t *loop, int fd) {
  if (!loop || fd < 0)
    return;

  FD_CLR(fd, &loop->read_fds);

  // Recalculate max_fd if necessary
  if (fd == loop->max_fd) {
    loop->max_fd = 0;
    for (int i = 0; i < FD_SETSIZE; i++) {
      if (FD_ISSET(i, &loop->read_fds) && i > loop->max_fd) {
        loop->max_fd = i;
      }
    }
  }

  LOG_DEBUG("Removed fd %d from event loop", fd);
}

void tui_event_loop_request_render(tui_event_loop_t *loop) {
  if (loop) {
    loop->needs_render = true;
  }
}

// Process keyboard input
static bool process_keyboard_input(tui_event_loop_t *loop, tui_event_t *event) {
  tui_ncurses_t *ncurses = tui_get_ncurses(loop->tui);
  if (!ncurses)
    return false;

  // Set non-blocking mode for input
  tui_ncurses_set_timeout(ncurses, 0);

  int ch = tui_ncurses_get_char(ncurses);
  if (ch == -1) {
    return false;  // No input available
  }

  event->type = TUI_EVENT_KEY;
  event->data.key.key = ch;
  event->data.key.is_special = (ch >= 256);  // NCurses special keys

  // Check for quit key (Ctrl+Q)
  if (ch == 17) {  // Ctrl+Q
    event->type = TUI_EVENT_QUIT;
  }

  return true;
}

// Process resize event
static bool process_resize(tui_event_loop_t *loop, tui_event_t *event) {
  if (!g_resize_pending)
    return false;

  g_resize_pending = 0;

  // Get new terminal size
  tui_ncurses_t *ncurses = tui_get_ncurses(loop->tui);
  if (!ncurses)
    return false;

  int width, height;
  tui_ncurses_get_size(ncurses, &width, &height);

  event->type = TUI_EVENT_RESIZE;
  event->data.resize.width = width;
  event->data.resize.height = height;

  // Update NCurses
  tui_ncurses_handle_resize(ncurses);

  // Mark for render
  loop->needs_render = true;
  loop->resize_pending = false;

  LOG_INFO("Terminal resized to %dx%d", width, height);
  return true;
}

// Process PTY data
static bool process_pty_data(tui_event_loop_t *loop, tui_event_t *event,
                             fd_set *readfds) {
  tui_mux_t *mux = tui_get_mux(loop->tui);
  if (!mux)
    return false;

  // Check each PTY for data
  size_t pane_count = tui_mux_get_pane_count(mux);
  for (size_t i = 0; i < pane_count; i++) {
    app_tui_pane_t *pane = tui_mux_get_pane(mux, i);
    if (!pane)
      continue;

    // Check if pane has a PTY
    if (pane->pty_master >= 0 && FD_ISSET(pane->pty_master, readfds)) {
      event->type = TUI_EVENT_PTY_DATA;
      event->data.pty.pty_fd = pane->pty_master;
      event->data.pty.pane_index = i;
      return true;
    }
  }

  return false;
}
}

return false;
}

// Process a single event
static bool process_event(tui_event_loop_t *loop, tui_event_t *event) {
  // Check for resize first (highest priority)
  if (process_resize(loop, event)) {
    return true;
  }

  // Set up select() for non-blocking I/O
  fd_set readfds = loop->read_fds;
  struct timeval timeout = {0, 0};  // Non-blocking

  int ready = select(loop->max_fd + 1, &readfds, NULL, NULL, &timeout);
  if (ready < 0) {
    if (errno != EINTR) {
      LOG_ERROR("select() failed: %s", strerror(errno));
    }
    return false;
  }

  if (ready == 0) {
    return false;  // No events
  }

  // Check for keyboard input
  if (FD_ISSET(STDIN_FILENO, &readfds)) {
    if (process_keyboard_input(loop, event)) {
      return true;
    }
  }

  // Check for PTY data
  if (process_pty_data(loop, event, &readfds)) {
    return true;
  }

  return false;
}

// Handle a single event
static void handle_event(tui_event_loop_t *loop, const tui_event_t *event) {
  switch (event->type) {
  case TUI_EVENT_KEY:
    // Forward to TUI for handling
    tui_handle_input(loop->tui, event->data.key.key);
    loop->needs_render = true;
    break;

  case TUI_EVENT_RESIZE:
    // Update layout
    tui_handle_resize(loop->tui, event->data.resize.width,
                      event->data.resize.height);
    loop->needs_render = true;
    break;

  case TUI_EVENT_PTY_DATA:
    // Read PTY data
    tui_mux_t *mux = tui_get_mux(loop->tui);
    if (mux) {
      app_tui_pane_t *pane = tui_mux_get_pane(mux, event->data.pty.pane_index);
      if (pane && pane->pty_master >= 0) {
        char buffer[4096];
        ssize_t n = read(pane->pty_master, buffer, sizeof(buffer));
        if (n > 0) {
          // TODO: Write to pane's terminal emulator
          // For now, just mark as needing render
          loop->needs_render = true;
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
          LOG_ERROR("Failed to read from PTY: %s", strerror(errno));
        }
      }
    }
    break;

  case TUI_EVENT_QUIT:
    loop->running = false;
    break;

  default:
    break;
  }
}

int tui_event_loop_iterate(tui_event_loop_t *loop) {
  if (!loop)
    return -1;

  // Process events
  int events_this_iteration = 0;
  tui_event_t event;

  while (events_this_iteration < loop->config.max_events_per_loop &&
         process_event(loop, &event)) {
    handle_event(loop, &event);
    events_this_iteration++;
    loop->events_processed++;
  }

  // Check if it's time to render
  struct timeval now;
  get_current_time(&now);

  int64_t elapsed_us = timeval_diff_us(&now, &loop->last_frame_time);
  int64_t target_us =
      loop->frame_interval.tv_sec * 1000000LL + loop->frame_interval.tv_usec;

  if (loop->needs_render && elapsed_us >= target_us) {
    // Render
    tui_render_frame(loop->renderer);
    loop->needs_render = false;
    loop->frames_rendered++;
    loop->frame_count++;

    // Update timing
    loop->last_frame_time = now;

    // Update average frame time
    double frame_time_ms = elapsed_us / 1000.0;
    if (loop->frame_count == 1) {
      loop->avg_frame_time_ms = frame_time_ms;
    } else {
      // Exponential moving average
      loop->avg_frame_time_ms =
          0.9 * loop->avg_frame_time_ms + 0.1 * frame_time_ms;
    }
  }

  return events_this_iteration;
}

int tui_event_loop_run(tui_event_loop_t *loop) {
  if (!loop)
    return -1;

  LOG_INFO("Starting event loop");
  loop->running = true;

  // Initial render
  loop->needs_render = true;

  while (loop->running) {
    tui_event_loop_iterate(loop);

    // Sleep to avoid busy waiting
    usleep(1000);  // 1ms
  }

  LOG_INFO("Event loop stopped");
  return 0;
}

void tui_event_loop_stop(tui_event_loop_t *loop) {
  if (loop) {
    loop->running = false;
  }
}

void tui_event_loop_get_stats(const tui_event_loop_t *loop,
                              uint64_t *events_out, uint64_t *frames_out,
                              double *avg_frame_time_out) {
  if (!loop)
    return;

  if (events_out)
    *events_out = loop->events_processed;
  if (frames_out)
    *frames_out = loop->frames_rendered;
  if (avg_frame_time_out)
    *avg_frame_time_out = loop->avg_frame_time_ms;
}