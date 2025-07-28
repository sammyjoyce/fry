#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ENABLE_TUI_TESTS 1

#include "../src/tui/tui_ncurses.h"
#include "../src/tui/tui_pane.h"
#include "../src/tui/tui_render.h"
#include "../src/utils/memory.h"

// Test renderer creation and destruction
static void test_renderer_lifecycle(void) {
  printf("Testing renderer lifecycle...\n");

  app_tui_renderer_t *renderer = NULL;

  // Create without optimizations
  app_error err = app_tui_renderer_create(&renderer, false);
  assert(err == APP_SUCCESS);
  assert(renderer != NULL);
  assert(renderer->enable_dirty_tracking == false);
  assert(renderer->target_fps == 60);
  assert(renderer->needs_full_redraw == true);

  err = app_tui_renderer_destroy(renderer);
  assert(err == APP_SUCCESS);

  // Create with optimizations
  err = app_tui_renderer_create(&renderer, true);
  assert(err == APP_SUCCESS);
  assert(renderer != NULL);
  assert(renderer->enable_dirty_tracking == true);
  assert(renderer->dirty_regions != NULL);

  err = app_tui_renderer_destroy(renderer);
  assert(err == APP_SUCCESS);

  printf("  ✓ Renderer lifecycle works correctly\n");
}

// Test dirty region tracking
static void test_dirty_tracking(void) {
  printf("Testing dirty region tracking...\n");

  app_tui_renderer_t *renderer = NULL;
  app_error err = app_tui_renderer_create(&renderer, true);
  assert(err == APP_SUCCESS);

  // Initially should need full redraw
  assert(renderer->needs_full_redraw == true);

  // Clear dirty state
  err = app_tui_renderer_clear_dirty(renderer);
  assert(err == APP_SUCCESS);
  assert(renderer->needs_full_redraw == false);
  assert(renderer->dirty_region_count == 0);

  // Mark a region dirty
  err = app_tui_renderer_mark_dirty(renderer, 10, 5, 20, 10);
  assert(err == APP_SUCCESS);
  assert(renderer->dirty_region_count == 1);
  assert(renderer->dirty_regions[0].x == 10);
  assert(renderer->dirty_regions[0].y == 5);
  assert(renderer->dirty_regions[0].width == 20);
  assert(renderer->dirty_regions[0].height == 10);

  // Mark another region dirty
  err = app_tui_renderer_mark_dirty(renderer, 40, 15, 30, 20);
  assert(err == APP_SUCCESS);
  assert(renderer->dirty_region_count == 2);

  // Mark all dirty
  err = app_tui_renderer_mark_all_dirty(renderer);
  assert(err == APP_SUCCESS);
  assert(renderer->needs_full_redraw == true);
  assert(renderer->dirty_region_count == 0);

  app_tui_renderer_destroy(renderer);

  printf("  ✓ Dirty region tracking works correctly\n");
}

// Test frame rate control
static void test_frame_rate_control(void) {
  printf("Testing frame rate control...\n");

  app_tui_renderer_t *renderer = NULL;
  app_error err = app_tui_renderer_create(&renderer, false);
  assert(err == APP_SUCCESS);

  // Set different frame rates
  err = app_tui_renderer_set_target_fps(renderer, 30);
  assert(err == APP_SUCCESS);
  assert(renderer->target_fps == 30);

  err = app_tui_renderer_set_target_fps(renderer, 120);
  assert(err == APP_SUCCESS);
  assert(renderer->target_fps == 120);

  // Test invalid frame rates
  err = app_tui_renderer_set_target_fps(renderer, 0);
  assert(err == APP_ERROR_INVALID_ARG);

  err = app_tui_renderer_set_target_fps(renderer, 200);
  assert(err == APP_ERROR_INVALID_ARG);

  // Test should_render
  assert(app_tui_renderer_should_render(renderer) == true);  // First frame

  app_tui_renderer_destroy(renderer);

  printf("  ✓ Frame rate control works correctly\n");
}

// Test statistics
static void test_statistics(void) {
  printf("Testing render statistics...\n");

  app_tui_renderer_t *renderer = NULL;
  app_error err = app_tui_renderer_create(&renderer, true);
  assert(err == APP_SUCCESS);

  // Get initial stats
  app_render_stats_t stats;
  err = app_tui_renderer_get_stats(renderer, &stats);
  assert(err == APP_SUCCESS);
  assert(stats.frame_count == 0);
  assert(stats.full_redraws == 0);
  assert(stats.partial_updates == 0);
  assert(stats.skipped_frames == 0);

  // Simulate some frames
  err = app_tui_renderer_begin_frame(renderer);
  assert(err == APP_SUCCESS);
  err = app_tui_renderer_end_frame(renderer);
  assert(err == APP_SUCCESS);

  err = app_tui_renderer_get_stats(renderer, &stats);
  assert(err == APP_SUCCESS);
  assert(stats.frame_count == 1);
  assert(stats.full_redraws == 1);  // First frame is full redraw

  // Reset stats
  err = app_tui_renderer_reset_stats(renderer);
  assert(err == APP_SUCCESS);

  err = app_tui_renderer_get_stats(renderer, &stats);
  assert(err == APP_SUCCESS);
  assert(stats.frame_count == 0);

  app_tui_renderer_destroy(renderer);

  printf("  ✓ Render statistics work correctly\n");
}

// Test rendering with mock NCurses
static void test_rendering_with_mock(void) {
  printf("Testing rendering with mock NCurses...\n");

  // Initialize mock NCurses
  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  // Create renderer
  app_tui_renderer_t *renderer = NULL;
  err = app_tui_renderer_create(&renderer, true);
  assert(err == APP_SUCCESS);

  // Create pane manager and a test pane
  app_tui_pane_manager_t *manager = NULL;
  err = app_tui_pane_manager_create(&manager, 9);
  assert(err == APP_SUCCESS);

  app_pane_rect_t rect = {.x = 10, .y = 5, .width = 40, .height = 10};
  app_tui_pane_t *pane = NULL;
  err = app_tui_pane_create(manager, "test-account", &rect, &pane);
  assert(err == APP_SUCCESS);

  // Mark pane dirty
  err = app_tui_renderer_mark_pane_dirty(renderer, pane);
  assert(err == APP_SUCCESS);

  // Render panes
  app_tui_pane_t *panes[] = {pane};
  err = app_tui_renderer_render_panes(renderer, panes, 1, RENDER_FLAG_NONE);
  assert(err == APP_SUCCESS);

  // Test status bar rendering
  err = app_tui_renderer_render_status_bar(renderer, 23, "Left", "Center",
                                           "Right");
  assert(err == APP_SUCCESS);

  // Test command line rendering
  err = app_tui_renderer_render_command_line(renderer, 22, ":> ",
                                             "test command", 5);
  assert(err == APP_SUCCESS);

  // Cleanup
  app_tui_pane_destroy(manager, pane);
  app_tui_pane_manager_destroy(manager);
  app_tui_renderer_destroy(renderer);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Rendering with mock NCurses works correctly\n");
}

// Main test runner
int main(void) {
  printf("Running TUI render engine tests...\n\n");

  test_renderer_lifecycle();
  test_dirty_tracking();
  test_frame_rate_control();
  test_statistics();
  test_rendering_with_mock();

  printf("\nAll render engine tests passed! ✓\n");
  return 0;
}