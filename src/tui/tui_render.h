#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "../core/error.h"
#include "../core/types.h"
#include "tui_pane.h"

// Forward declarations
typedef struct app_tui_renderer app_tui_renderer_t;

// Render flags
typedef enum {
  RENDER_FLAG_NONE = 0,
  RENDER_FLAG_FORCE_FULL = 1 << 0,    // Force full screen redraw
  RENDER_FLAG_CLEAR_FIRST = 1 << 1,   // Clear screen before render
  RENDER_FLAG_NO_REFRESH = 1 << 2,    // Don't call refresh after render
  RENDER_FLAG_DEBUG_REGIONS = 1 << 3  // Show dirty regions (debug)
} app_render_flags_t;

// Dirty region tracking
typedef struct {
  int x;
  int y;
  int width;
  int height;
  bool is_dirty;
} app_dirty_region_t;

// Render statistics
typedef struct {
  size_t frame_count;         // Total frames rendered
  size_t full_redraws;        // Number of full screen redraws
  size_t partial_updates;     // Number of partial updates
  size_t skipped_frames;      // Frames skipped due to no changes
  double avg_render_time_ms;  // Average render time
  double max_render_time_ms;  // Maximum render time
  time_t start_time;          // When rendering started
} app_render_stats_t;

// Renderer structure
struct app_tui_renderer {
  // Configuration
  bool enable_dirty_tracking;  // Use dirty region optimization
  bool enable_double_buffer;   // Use double buffering
  int target_fps;              // Target frames per second

  // State
  bool needs_full_redraw;   // Full redraw needed
  time_t last_render_time;  // Last render timestamp

  // Dirty tracking
  app_dirty_region_t *dirty_regions;  // Array of dirty regions
  size_t dirty_region_count;          // Number of dirty regions
  size_t dirty_region_capacity;       // Capacity of array

  // Performance
  app_render_stats_t stats;  // Rendering statistics

  // Double buffering (if enabled)
  void *back_buffer;   // Back buffer for double buffering
  size_t buffer_size;  // Size of buffer
};

// Renderer lifecycle
APP_NODISCARD app_error app_tui_renderer_create(app_tui_renderer_t **renderer,
                                                bool enable_optimizations);

APP_NODISCARD app_error app_tui_renderer_destroy(app_tui_renderer_t *renderer);

// Configuration
APP_NODISCARD app_error
app_tui_renderer_set_target_fps(app_tui_renderer_t *renderer, int fps);

APP_NODISCARD app_error app_tui_renderer_enable_double_buffer(
    app_tui_renderer_t *renderer, bool enable);

// Dirty region management
APP_NODISCARD app_error app_tui_renderer_mark_dirty(
    app_tui_renderer_t *renderer, int x, int y, int width, int height);

APP_NODISCARD app_error app_tui_renderer_mark_pane_dirty(
    app_tui_renderer_t *renderer, app_tui_pane_t *pane);

APP_NODISCARD app_error
app_tui_renderer_mark_all_dirty(app_tui_renderer_t *renderer);

APP_NODISCARD app_error
app_tui_renderer_clear_dirty(app_tui_renderer_t *renderer);

// Rendering
APP_NODISCARD app_error
app_tui_renderer_begin_frame(app_tui_renderer_t *renderer);

APP_NODISCARD app_error
app_tui_renderer_end_frame(app_tui_renderer_t *renderer);

APP_NODISCARD app_error app_tui_renderer_render_panes(
    app_tui_renderer_t *renderer, app_tui_pane_t **panes, size_t pane_count,
    app_render_flags_t flags);

APP_NODISCARD app_error app_tui_renderer_render_status_bar(
    app_tui_renderer_t *renderer, int y, const char *left_text,
    const char *center_text, const char *right_text);

APP_NODISCARD app_error app_tui_renderer_render_command_line(
    app_tui_renderer_t *renderer, int y, const char *prompt,
    const char *command, size_t cursor_pos);

// Frame rate control
APP_NODISCARD bool app_tui_renderer_should_render(app_tui_renderer_t *renderer);

APP_NODISCARD app_error
app_tui_renderer_wait_for_next_frame(app_tui_renderer_t *renderer);

// Statistics
APP_NODISCARD app_error app_tui_renderer_get_stats(app_tui_renderer_t *renderer,
                                                   app_render_stats_t *stats);

APP_NODISCARD app_error
app_tui_renderer_reset_stats(app_tui_renderer_t *renderer);

// Debug
APP_NODISCARD app_error
app_tui_renderer_draw_dirty_regions(app_tui_renderer_t *renderer);