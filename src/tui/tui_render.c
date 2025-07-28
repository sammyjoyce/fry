#include "tui_render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <core/error.h>
#include <utils/logging.h>
#include <utils/memory.h>
#include "tui/tui.h"
#include "tui/backend/tui_ncurses.h"

// Helper to get current time in milliseconds
static double get_time_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

// Create renderer
app_error app_tui_renderer_create(app_tui_renderer_t **renderer,
                                  bool enable_optimizations) {
  if (!renderer) {
    return APP_ERROR_INVALID_ARG;
  }

  app_tui_renderer_t *r = app_calloc(1, sizeof(app_tui_renderer_t));
  if (!r) {
    return APP_ERROR_MEMORY;
  }

  // Configure renderer
  r->enable_dirty_tracking = enable_optimizations;
  r->enable_double_buffer = false;  // Not implemented yet
  r->target_fps = 60;               // Default 60 FPS
  r->needs_full_redraw = true;      // First frame needs full redraw
  r->last_render_time = 0;

  // Initialize dirty region tracking
  if (r->enable_dirty_tracking) {
    r->dirty_region_capacity = 32;
    r->dirty_regions =
        app_calloc(r->dirty_region_capacity, sizeof(app_dirty_region_t));
    if (!r->dirty_regions) {
      app_free(r);
      return APP_ERROR_MEMORY;
    }
  }

  // Initialize statistics
  r->stats.start_time = time(NULL);

  *renderer = r;
  LOG_DEBUG("Created renderer with%s optimizations",
            enable_optimizations ? "" : "out");

  return APP_SUCCESS;
}

// Destroy renderer
app_error app_tui_renderer_destroy(app_tui_renderer_t *renderer) {
  if (!renderer) {
    return APP_SUCCESS;
  }

  if (renderer->dirty_regions) {
    app_free(renderer->dirty_regions);
  }

  if (renderer->back_buffer) {
    app_free(renderer->back_buffer);
  }

  app_free(renderer);
  return APP_SUCCESS;
}

// Set target FPS
app_error app_tui_renderer_set_target_fps(app_tui_renderer_t *renderer,
                                          int fps) {
  if (!renderer || fps < 1 || fps > 120) {
    return APP_ERROR_INVALID_ARG;
  }

  renderer->target_fps = fps;
  LOG_DEBUG("Set target FPS to %d", fps);

  return APP_SUCCESS;
}

// Mark a region as dirty
app_error app_tui_renderer_mark_dirty(app_tui_renderer_t *renderer, int x,
                                      int y, int width, int height) {
  if (!renderer) {
    return APP_ERROR_INVALID_ARG;
  }

  if (!renderer->enable_dirty_tracking) {
    renderer->needs_full_redraw = true;
    return APP_SUCCESS;
  }

  // Check if we need to expand the dirty regions array
  if (renderer->dirty_region_count >= renderer->dirty_region_capacity) {
    // Just mark for full redraw if we have too many dirty regions
    renderer->needs_full_redraw = true;
    return APP_SUCCESS;
  }

  // Add the dirty region
  app_dirty_region_t *region =
      &renderer->dirty_regions[renderer->dirty_region_count++];
  region->x = x;
  region->y = y;
  region->width = width;
  region->height = height;
  region->is_dirty = true;

  LOG_DEBUG("Marked region dirty: %dx%d at (%d,%d)", width, height, x, y);

  return APP_SUCCESS;
}

// Mark a pane as dirty
app_error app_tui_renderer_mark_pane_dirty(app_tui_renderer_t *renderer,
                                           app_tui_pane_t *pane) {
  if (!renderer || !pane) {
    return APP_ERROR_INVALID_ARG;
  }

  return app_tui_renderer_mark_dirty(renderer, pane->geometry.x,
                                     pane->geometry.y, pane->geometry.width,
                                     pane->geometry.height);
}

// Mark everything as dirty
app_error app_tui_renderer_mark_all_dirty(app_tui_renderer_t *renderer) {
  if (!renderer) {
    return APP_ERROR_INVALID_ARG;
  }

  renderer->needs_full_redraw = true;
  renderer->dirty_region_count = 0;

  return APP_SUCCESS;
}

// Clear dirty regions
app_error app_tui_renderer_clear_dirty(app_tui_renderer_t *renderer) {
  if (!renderer) {
    return APP_ERROR_INVALID_ARG;
  }

  renderer->needs_full_redraw = false;
  renderer->dirty_region_count = 0;

  return APP_SUCCESS;
}

// Begin a new frame
app_error app_tui_renderer_begin_frame(app_tui_renderer_t *renderer) {
  if (!renderer) {
    return APP_ERROR_INVALID_ARG;
  }

  // Record frame start time
  renderer->last_render_time = time(NULL);

  return APP_SUCCESS;
}

// End the current frame
app_error app_tui_renderer_end_frame(app_tui_renderer_t *renderer) {
  if (!renderer) {
    return APP_ERROR_INVALID_ARG;
  }

  // Update statistics
  renderer->stats.frame_count++;

  if (renderer->needs_full_redraw) {
    renderer->stats.full_redraws++;
  } else if (renderer->dirty_region_count > 0) {
    renderer->stats.partial_updates++;
  } else {
    renderer->stats.skipped_frames++;
  }

  // Clear dirty regions for next frame
  app_tui_renderer_clear_dirty(renderer);

  // Refresh the screen
  if (g_ncurses) {
    g_ncurses->refresh();
  }

  return APP_SUCCESS;
}

// Check if a region intersects with a dirty region
static bool is_region_dirty(app_tui_renderer_t *renderer, int x, int y,
                            int width, int height) {
  if (renderer->needs_full_redraw) {
    return true;
  }

  if (!renderer->enable_dirty_tracking) {
    return true;
  }

  // Check intersection with each dirty region
  for (size_t i = 0; i < renderer->dirty_region_count; i++) {
    app_dirty_region_t *dirty = &renderer->dirty_regions[i];
    if (!dirty->is_dirty) {
      continue;
    }

    // Check for intersection
    if (x < dirty->x + dirty->width && x + width > dirty->x &&
        y < dirty->y + dirty->height && y + height > dirty->y) {
      return true;
    }
  }

  return false;
}

// Render all panes
app_error app_tui_renderer_render_panes(app_tui_renderer_t *renderer,
                                        app_tui_pane_t **panes,
                                        size_t pane_count,
                                        app_render_flags_t flags) {
  if (!renderer || !panes) {
    return APP_ERROR_INVALID_ARG;
  }

  double start_time = get_time_ms();

  // Clear screen if requested
  if (flags & RENDER_FLAG_CLEAR_FIRST) {
    if (g_ncurses) {
      g_ncurses->clear();
    }
  }

  // Render each pane
  for (size_t i = 0; i < pane_count; i++) {
    app_tui_pane_t *pane = panes[i];
    if (!pane) {
      continue;
    }

    // Skip if not dirty (unless forced)
    if (!(flags & RENDER_FLAG_FORCE_FULL) &&
        !is_region_dirty(renderer, pane->geometry.x, pane->geometry.y,
                         pane->geometry.width, pane->geometry.height)) {
      continue;
    }

    // Render the pane
    app_error err = app_tui_pane_refresh(pane);
    if (err != APP_SUCCESS) {
      LOG_WARNING("Failed to render pane %zu: %d", i, err);
    }
  }

  // Update render time statistics
  double render_time = get_time_ms() - start_time;
  if (render_time > renderer->stats.max_render_time_ms) {
    renderer->stats.max_render_time_ms = render_time;
  }

  // Update average (simple moving average)
  renderer->stats.avg_render_time_ms =
      (renderer->stats.avg_render_time_ms * (renderer->stats.frame_count - 1) +
       render_time) /
      renderer->stats.frame_count;

  // Refresh if not disabled
  if (!(flags & RENDER_FLAG_NO_REFRESH)) {
    if (g_ncurses) {
      g_ncurses->refresh();
    }
  }

  return APP_SUCCESS;
}

// Render status bar
app_error app_tui_renderer_render_status_bar(app_tui_renderer_t *renderer,
                                             int y, const char *left_text,
                                             const char *center_text,
                                             const char *right_text) {
  if (!renderer) {
    return APP_ERROR_INVALID_ARG;
  }

  WINDOW *stdscr = app_ncurses_stdscr();
  if (!stdscr) {
    return APP_ERROR_NOT_INITIALIZED;
  }

  int width = g_ncurses->getmaxx(stdscr);

  // Skip if not dirty
  if (!is_region_dirty(renderer, 0, y, width, 1)) {
    return APP_SUCCESS;
  }

  // Clear the line and set color
  g_ncurses->wmove(stdscr, y, 0);
  tui_set_color(stdscr, TUI_COLOR_MENU_NORMAL);

  // Draw background (clear line with spaces)
  for (int x = 0; x < width; x++) {
    g_ncurses->waddch(stdscr, ' ');
  }

  // Draw left text
  if (left_text) {
    g_ncurses->mvprintw(y, 1, "%s", left_text);
  }

  // Draw center text
  if (center_text) {
    int center_x = (width - strlen(center_text)) / 2;
    g_ncurses->mvprintw(y, center_x, "%s", center_text);
  }

  // Draw right text
  if (right_text) {
    int right_x = width - strlen(right_text) - 1;
    g_ncurses->mvprintw(y, right_x, "%s", right_text);
  }

  // Reset color
  tui_set_color(stdscr, TUI_COLOR_DEFAULT);

  return APP_SUCCESS;
}

// Render command line
app_error app_tui_renderer_render_command_line(app_tui_renderer_t *renderer,
                                               int y, const char *prompt,
                                               const char *command,
                                               size_t cursor_pos) {
  if (!renderer) {
    return APP_ERROR_INVALID_ARG;
  }

  WINDOW *stdscr = app_ncurses_stdscr();
  if (!stdscr) {
    return APP_ERROR_NOT_INITIALIZED;
  }

  int width = g_ncurses->getmaxx(stdscr);

  // Skip if not dirty
  if (!is_region_dirty(renderer, 0, y, width, 1)) {
    return APP_SUCCESS;
  }

  // Clear the line
  g_ncurses->wmove(stdscr, y, 0);
  for (int x = 0; x < width; x++) {
    g_ncurses->waddch(stdscr, ' ');
  }

  // Draw prompt
  int x = 0;
  if (prompt) {
    g_ncurses->mvprintw(y, x, "%s", prompt);
    x += strlen(prompt);
  }

  // Draw command
  if (command) {
    g_ncurses->mvprintw(y, x, "%s", command);
  }

  // Position cursor
  if (command && cursor_pos <= strlen(command)) {
    g_ncurses->move(y, x + cursor_pos);
  }

  return APP_SUCCESS;
}

// Check if we should render a new frame
bool app_tui_renderer_should_render(app_tui_renderer_t *renderer) {
  if (!renderer) {
    return false;
  }

  // Always render if we have dirty regions or need full redraw
  if (renderer->needs_full_redraw || renderer->dirty_region_count > 0) {
    return true;
  }

  // Check frame rate limiting
  if (renderer->target_fps > 0) {
    double frame_time_ms = 1000.0 / renderer->target_fps;
    double current_time = get_time_ms();
    double last_frame_time = renderer->last_render_time * 1000.0;

    if (current_time - last_frame_time < frame_time_ms) {
      return false;
    }
  }

  return true;
}

// Wait for next frame
app_error app_tui_renderer_wait_for_next_frame(app_tui_renderer_t *renderer) {
  if (!renderer || renderer->target_fps <= 0) {
    return APP_SUCCESS;
  }

  double frame_time_ms = 1000.0 / renderer->target_fps;
  double current_time = get_time_ms();
  double last_frame_time = renderer->last_render_time * 1000.0;
  double time_to_wait = frame_time_ms - (current_time - last_frame_time);

  if (time_to_wait > 0) {
    usleep((useconds_t)(time_to_wait * 1000));
  }

  return APP_SUCCESS;
}

// Get statistics
app_error app_tui_renderer_get_stats(app_tui_renderer_t *renderer,
                                     app_render_stats_t *stats) {
  if (!renderer || !stats) {
    return APP_ERROR_INVALID_ARG;
  }

  *stats = renderer->stats;
  return APP_SUCCESS;
}

// Reset statistics
app_error app_tui_renderer_reset_stats(app_tui_renderer_t *renderer) {
  if (!renderer) {
    return APP_ERROR_INVALID_ARG;
  }

  memset(&renderer->stats, 0, sizeof(renderer->stats));
  renderer->stats.start_time = time(NULL);

  return APP_SUCCESS;
}

// Draw dirty regions (for debugging)
app_error app_tui_renderer_draw_dirty_regions(app_tui_renderer_t *renderer) {
  if (!renderer || !renderer->enable_dirty_tracking) {
    return APP_SUCCESS;
  }

  WINDOW *stdscr = app_ncurses_stdscr();
  if (!stdscr) {
    return APP_ERROR_NOT_INITIALIZED;
  }

  // Set debug color
  tui_set_color(stdscr, TUI_COLOR_ERROR);

  // Draw each dirty region
  for (size_t i = 0; i < renderer->dirty_region_count; i++) {
    app_dirty_region_t *region = &renderer->dirty_regions[i];
    if (!region->is_dirty) {
      continue;
    }

    // Draw border around dirty region
    for (int x = region->x; x < region->x + region->width; x++) {
      g_ncurses->wmove(stdscr, region->y, x);
      g_ncurses->waddch(stdscr, '*');
      g_ncurses->wmove(stdscr, region->y + region->height - 1, x);
      g_ncurses->waddch(stdscr, '*');
    }
    for (int y = region->y; y < region->y + region->height; y++) {
      g_ncurses->wmove(stdscr, y, region->x);
      g_ncurses->waddch(stdscr, '*');
      g_ncurses->wmove(stdscr, y, region->x + region->width - 1);
      g_ncurses->waddch(stdscr, '*');
    }
  }

  // Reset color
  tui_set_color(stdscr, TUI_COLOR_DEFAULT);

  return APP_SUCCESS;
}