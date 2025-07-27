#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENABLE_TUI_TESTS 1

#include "../src/tui/tui_ncurses.h"
#include "../src/tui/tui_pane.h"
#include "../src/utils/memory.h"

// Test pane creation and destruction
static void test_pane_lifecycle(void) {
  printf("Testing pane lifecycle...\n");

  // Initialize mock NCurses
  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  // Create a pane
  app_pane_rect_t rect = {.x = 10, .y = 5, .width = 40, .height = 10};
  app_tui_pane_t *pane = NULL;
  err = app_tui_pane_create(&pane, &rect, "Test Pane");
  assert(err == APP_SUCCESS);
  assert(pane != NULL);
  assert(pane->rect.x == 10);
  assert(pane->rect.y == 5);
  assert(pane->rect.width == 40);
  assert(pane->rect.height == 10);
  assert(strcmp(pane->title, "Test Pane") == 0);
  assert(pane->window != NULL);
  assert(pane->content_window != NULL);
  assert(pane->is_focused == false);

  // Verify content window dimensions
  mock_window_t *content_win = (mock_window_t *)pane->content_window;
  assert(content_win->width == 38);  // 40 - 2 for borders
  assert(content_win->height == 8);  // 10 - 2 for borders
  assert(content_win->x == 11);      // 10 + 1 for left border
  assert(content_win->y == 6);       // 5 + 1 for top border

  // Destroy pane
  app_tui_pane_destroy(pane);

  // Cleanup
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Pane lifecycle works correctly\n");
}

// Test pane rendering
static void test_pane_rendering(void) {
  printf("Testing pane rendering...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  // Create a pane
  app_pane_rect_t rect = {.x = 0, .y = 0, .width = 20, .height = 10};
  app_tui_pane_t *pane = NULL;
  err = app_tui_pane_create(&pane, &rect, "Test");
  assert(err == APP_SUCCESS);

  // Render the pane
  err = app_tui_pane_render(pane);
  assert(err == APP_SUCCESS);

  // Verify border rendering
  mock_window_t *win = (mock_window_t *)pane->window;

  // Check corners
  assert(win->content[0][0] == '+');   // Top-left
  assert(win->content[0][19] == '+');  // Top-right
  assert(win->content[9][0] == '+');   // Bottom-left
  assert(win->content[9][19] == '+');  // Bottom-right

  // Check horizontal borders
  for (int i = 1; i < 19; i++) {
    assert(win->content[0][i] == '-');  // Top border
    assert(win->content[9][i] == '-');  // Bottom border
  }

  // Check vertical borders
  for (int i = 1; i < 9; i++) {
    assert(win->content[i][0] == '|');   // Left border
    assert(win->content[i][19] == '|');  // Right border
  }

  // Test focused rendering
  pane->is_focused = true;
  err = app_tui_pane_render(pane);
  assert(err == APP_SUCCESS);

  // In focused mode, borders might be highlighted differently
  // This depends on the implementation

  app_tui_pane_destroy(pane);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Pane rendering works correctly\n");
}

// Test pane content management
static void test_pane_content(void) {
  printf("Testing pane content management...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  // Create a pane
  app_pane_rect_t rect = {.x = 0, .y = 0, .width = 30, .height = 10};
  app_tui_pane_t *pane = NULL;
  err = app_tui_pane_create(&pane, &rect, "Content Test");
  assert(err == APP_SUCCESS);

  // Write content
  err = app_tui_pane_write(pane, "Hello, World!\n");
  assert(err == APP_SUCCESS);
  err = app_tui_pane_write(pane, "Line 2\n");
  assert(err == APP_SUCCESS);
  err = app_tui_pane_write(pane, "Line 3 with more text\n");
  assert(err == APP_SUCCESS);

  // Verify content in content window
  mock_window_t *content_win = (mock_window_t *)pane->content_window;
  assert(app_mock_ncurses_verify_text(terminal, content_win, 0, 0,
                                      "Hello, World!"));
  assert(app_mock_ncurses_verify_text(terminal, content_win, 1, 0, "Line 2"));
  assert(app_mock_ncurses_verify_text(terminal, content_win, 2, 0,
                                      "Line 3 with more text"));

  // Test clearing
  err = app_tui_pane_clear(pane);
  assert(err == APP_SUCCESS);

  // Verify content is cleared
  for (int y = 0; y < content_win->height; y++) {
    for (int x = 0; x < content_win->width; x++) {
      assert(content_win->content[y][x] == ' ');
    }
  }

  app_tui_pane_destroy(pane);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Pane content management works correctly\n");
}

// Test pane resizing
static void test_pane_resize(void) {
  printf("Testing pane resizing...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  // Create a pane
  app_pane_rect_t rect = {.x = 0, .y = 0, .width = 20, .height = 10};
  app_tui_pane_t *pane = NULL;
  err = app_tui_pane_create(&pane, &rect, "Resize Test");
  assert(err == APP_SUCCESS);

  // Write some content
  err = app_tui_pane_write(pane, "Original content\n");
  assert(err == APP_SUCCESS);

  // Resize the pane
  app_pane_rect_t new_rect = {.x = 5, .y = 5, .width = 30, .height = 15};
  err = app_tui_pane_resize(pane, &new_rect);
  assert(err == APP_SUCCESS);

  // Verify new dimensions
  assert(pane->rect.x == 5);
  assert(pane->rect.y == 5);
  assert(pane->rect.width == 30);
  assert(pane->rect.height == 15);

  // Verify windows were recreated with new dimensions
  mock_window_t *win = (mock_window_t *)pane->window;
  assert(win->x == 5);
  assert(win->y == 5);
  assert(win->width == 30);
  assert(win->height == 15);

  mock_window_t *content_win = (mock_window_t *)pane->content_window;
  assert(content_win->x == 6);
  assert(content_win->y == 6);
  assert(content_win->width == 28);
  assert(content_win->height == 13);

  app_tui_pane_destroy(pane);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Pane resizing works correctly\n");
}

// Test pane focus management
static void test_pane_focus(void) {
  printf("Testing pane focus management...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  // Create multiple panes
  app_pane_rect_t rect1 = {.x = 0, .y = 0, .width = 40, .height = 12};
  app_pane_rect_t rect2 = {.x = 40, .y = 0, .width = 40, .height = 12};

  app_tui_pane_t *pane1 = NULL;
  app_tui_pane_t *pane2 = NULL;

  err = app_tui_pane_create(&pane1, &rect1, "Pane 1");
  assert(err == APP_SUCCESS);
  err = app_tui_pane_create(&pane2, &rect2, "Pane 2");
  assert(err == APP_SUCCESS);

  // Initially neither is focused
  assert(pane1->is_focused == false);
  assert(pane2->is_focused == false);

  // Focus pane 1
  err = app_tui_pane_set_focus(pane1, true);
  assert(err == APP_SUCCESS);
  assert(pane1->is_focused == true);

  // Focus pane 2 (should unfocus pane 1 in real implementation)
  err = app_tui_pane_set_focus(pane2, true);
  assert(err == APP_SUCCESS);
  assert(pane2->is_focused == true);

  // Unfocus pane 2
  err = app_tui_pane_set_focus(pane2, false);
  assert(err == APP_SUCCESS);
  assert(pane2->is_focused == false);

  app_tui_pane_destroy(pane1);
  app_tui_pane_destroy(pane2);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Pane focus management works correctly\n");
}

// Test pane scrolling
static void test_pane_scrolling(void) {
  printf("Testing pane scrolling...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  // Create a small pane
  app_pane_rect_t rect = {.x = 0, .y = 0, .width = 30, .height = 7};
  app_tui_pane_t *pane = NULL;
  err = app_tui_pane_create(&pane, &rect, "Scroll Test");
  assert(err == APP_SUCCESS);

  // Content window is 5 lines high (7 - 2 for borders)
  // Write more lines than can fit
  for (int i = 1; i <= 10; i++) {
    char line[32];
    snprintf(line, sizeof(line), "Line %d\n", i);
    err = app_tui_pane_write(pane, line);
    assert(err == APP_SUCCESS);
  }

  // Verify scroll position
  assert(pane->scroll_offset > 0);

  // Test scrolling up
  err = app_tui_pane_scroll(pane, -1);
  assert(err == APP_SUCCESS);

  // Test scrolling down
  err = app_tui_pane_scroll(pane, 1);
  assert(err == APP_SUCCESS);

  // Test scrolling to top
  err = app_tui_pane_scroll_to(pane, 0);
  assert(err == APP_SUCCESS);
  assert(pane->scroll_offset == 0);

  // Test scrolling to bottom
  err = app_tui_pane_scroll_to(pane, -1);
  assert(err == APP_SUCCESS);
  assert(pane->scroll_offset > 0);

  app_tui_pane_destroy(pane);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Pane scrolling works correctly\n");
}

// Test minimum size constraints
static void test_pane_minimum_size(void) {
  printf("Testing pane minimum size constraints...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  // Try to create a pane that's too small
  app_pane_rect_t small_rect = {.x = 0, .y = 0, .width = 2, .height = 2};
  app_tui_pane_t *pane = NULL;
  err = app_tui_pane_create(&pane, &small_rect, "Too Small");
  assert(err == APP_ERROR_INVALID_ARG);
  assert(pane == NULL);

  // Create a pane at minimum size
  app_pane_rect_t min_rect = {.x = 0,
                              .y = 0,
                              .width = APP_TUI_MIN_PANE_WIDTH,
                              .height = APP_TUI_MIN_PANE_HEIGHT};
  err = app_tui_pane_create(&pane, &min_rect, "Minimum");
  assert(err == APP_SUCCESS);
  assert(pane != NULL);

  app_tui_pane_destroy(pane);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Pane minimum size constraints work correctly\n");
}

int main(void) {
  printf("Running TUI pane tests...\n\n");

  test_pane_lifecycle();
  test_pane_rendering();
  test_pane_content();
  test_pane_resize();
  test_pane_focus();
  test_pane_scrolling();
  test_pane_minimum_size();

  printf("\nAll pane tests passed! ✓\n");
  return 0;
}