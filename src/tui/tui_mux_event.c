/*
 * Enhanced event loop for TUI multiplexer
 * Implements Task 6: Create main event loop with input and resize handling
 */

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#include "../utils/logging.h"
#include "../utils/memory.h"
#include "tui_input.h"
#include "tui_mux.h"
#include "tui_ncurses.h"
#include "tui_pane.h"
#include "tui_term_emulator.h"

// Global flag for resize handling
static volatile sig_atomic_t g_resize_pending = 0;

// Signal handler for SIGWINCH
static void handle_sigwinch(int sig) {
  (void)sig;
  g_resize_pending = 1;
}

// Install signal handler for terminal resize
app_error app_tui_mux_install_resize_handler(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handle_sigwinch;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  if (sigaction(SIGWINCH, &sa, NULL) < 0) {
    LOG_WARNING("Failed to install SIGWINCH handler: %s", strerror(errno));
    return APP_ERROR_SYSTEM;
  }

  return APP_SUCCESS;
}

// Enhanced event loop with better timing and PTY monitoring
app_error app_tui_mux_run_enhanced(app_tui_mux_t *mux) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  // Install resize handler
  app_tui_mux_install_resize_handler();

  mux->running = true;

  // Initial render
  (void)app_tui_mux_render(mux);

  // Calculate frame interval
  struct timeval frame_interval;
  int64_t frame_interval_us = 1000000 / (1000 / mux->config.refresh_rate_ms);
  frame_interval.tv_sec = frame_interval_us / 1000000;
  frame_interval.tv_usec = frame_interval_us % 1000000;

  struct timeval last_frame_time;
  gettimeofday(&last_frame_time, NULL);

  // Main event loop
  while (mux->running && mux->state != MUX_STATE_EXITING) {
    fd_set read_fds;
    FD_ZERO(&read_fds);

    // Add stdin for keyboard input
    FD_SET(STDIN_FILENO, &read_fds);
    int max_fd = STDIN_FILENO;

    // Add PTY file descriptors
    for (size_t i = 0; i < mux->pane_manager->pane_count; i++) {
      app_tui_pane_t *pane = mux->pane_manager->panes[i];
      if (pane && pane->pty_master >= 0) {
        FD_SET(pane->pty_master, &read_fds);
        if (pane->pty_master > max_fd) {
          max_fd = pane->pty_master;
        }
      }
    }

    // Calculate timeout for select
    struct timeval now;
    gettimeofday(&now, NULL);

    int64_t elapsed_us = (now.tv_sec - last_frame_time.tv_sec) * 1000000LL +
                         (now.tv_usec - last_frame_time.tv_usec);
    int64_t target_us =
        frame_interval.tv_sec * 1000000LL + frame_interval.tv_usec;
    int64_t remaining_us = target_us - elapsed_us;

    struct timeval timeout;
    if (remaining_us > 0) {
      timeout.tv_sec = remaining_us / 1000000;
      timeout.tv_usec = remaining_us % 1000000;
    } else {
      timeout.tv_sec = 0;
      timeout.tv_usec = 0;
    }

    // Wait for events
    int ready = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

    if (ready < 0) {
      if (errno != EINTR) {
        LOG_ERROR("select() failed: %s", strerror(errno));
        return APP_ERROR_SYSTEM;
      }
    }

    // Check for resize
    if (g_resize_pending) {
      g_resize_pending = 0;

      // Update NCurses
      if (g_ncurses && g_ncurses->endwin && g_ncurses->refresh) {
        g_ncurses->endwin();
        g_ncurses->refresh();
      }

      // Get new size
      int width = tui_get_max_x();
      int height = tui_get_max_y();

      LOG_INFO("Terminal resized to %dx%d", width, height);

      // Update layout
      if (mux->layout_manager) {
        // Reapply layout with new dimensions
        app_error err =
            app_tui_layout_apply(mux->layout_manager, mux->pane_manager->panes,
                                 mux->pane_manager->pane_count);
        if (err != APP_SUCCESS) {
          LOG_WARNING("Failed to reapply layout after resize");
        }
      }

      mux->pane_manager->needs_refresh = true;
    }

    // Handle keyboard input
    if (ready > 0 && FD_ISSET(STDIN_FILENO, &read_fds)) {
      // Use new input system if available
      if (mux->input_system) {
        app_key_event_t event;
        app_error err = app_tui_input_read_event(mux->input_system, &event, 0);
        if (err == APP_SUCCESS) {
          // Try keybinding first
          err = app_tui_input_process_keybinding(mux->input_system, &event);
          if (err == APP_ERROR_NOT_FOUND) {
            // No keybinding, handle as raw input
            (void)app_tui_mux_handle_input(mux, event.code);
          }
        }
      } else {
        // Fallback to old input handling
        int ch = tui_get_char();
        if (ch != ERR) {
          (void)app_tui_mux_handle_input(mux, ch);
        }
      }
    }

    // Handle PTY output
    if (ready > 0) {
      for (size_t i = 0; i < mux->pane_manager->pane_count; i++) {
        app_tui_pane_t *pane = mux->pane_manager->panes[i];
        if (pane && pane->pty_master >= 0 &&
            FD_ISSET(pane->pty_master, &read_fds)) {
          // Read from PTY
          char buffer[4096];
          ssize_t n = read(pane->pty_master, buffer, sizeof(buffer));

          if (n > 0) {
            // Process data through terminal emulator
            if (pane->term_emulator) {
              tui_term_emulator_process(pane->term_emulator, buffer, n);
            } else {
              // Fallback to simple buffer (shouldn't happen)
              buffer[n] = '\0';

              // Add to pane's buffer
              if (pane->buffer_lines < pane->buffer_size) {
                if (!pane->buffer) {
                  pane->buffer = app_calloc(pane->buffer_size, sizeof(char *));
                }
                if (pane->buffer) {
                  pane->buffer[pane->buffer_lines] = app_strdup(buffer);
                  pane->buffer_lines++;
                }
              } else {
                // Buffer full, shift lines up
                if (pane->buffer && pane->buffer[0]) {
                  app_free(pane->buffer[0]);
                }
                for (size_t i = 1; i < pane->buffer_size; i++) {
                  pane->buffer[i - 1] = pane->buffer[i];
                }
                pane->buffer[pane->buffer_size - 1] = app_strdup(buffer);
              }
            }

            // Mark for refresh
            mux->pane_manager->needs_refresh = true;
          } else if (n == 0 ||
                     (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            // PTY closed or error
            LOG_WARNING("PTY for pane %d closed or error: %s", pane->number,
                        strerror(errno));
            pane->state = PANE_STATE_DISCONNECTED;
            mux->pane_manager->needs_refresh = true;
          }
        }
      }
    }

    // Check if it's time to render
    gettimeofday(&now, NULL);
    elapsed_us = (now.tv_sec - last_frame_time.tv_sec) * 1000000LL +
                 (now.tv_usec - last_frame_time.tv_usec);

    if (mux->pane_manager->needs_refresh && elapsed_us >= target_us) {
      (void)app_tui_mux_render(mux);
      mux->pane_manager->needs_refresh = false;
      last_frame_time = now;
    }
  }

  return APP_SUCCESS;
}