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

  // Create pane manager
  app_tui_pane_manager_t *manager = NULL;
  err = app_tui_pane_manager_create(&manager, 9);
  assert(err == APP_SUCCESS);
  assert(manager != NULL);

  // Create a pane
  app_pane_rect_t rect = {.x = 10, .y = 5, .width = 40, .height = 10};
  app_tui_pane_t *pane = NULL;
  err = app_tui_pane_create(manager, "test-account", &rect, &pane);
  assert(err == APP_SUCCESS);
  assert(pane != NULL);
  assert(pane->geometry.x == 10);
  assert(pane->geometry.y == 5);
  assert(pane->geometry.width == 40);
  assert(pane->geometry.height == 10);
  assert(pane->window != NULL);
  assert(pane->border_window != NULL);
  // First pane is automatically focused
  assert(pane->has_focus == true);
  assert(manager->focused_pane == pane);

  // Verify window dimensions
  if (pane->window) {
    // The window is a tui_window_t, not a raw WINDOW
    assert(pane->window->width == 38);  // 40 - 2 for borders
    assert(pane->window->height == 8);  // 10 - 2 for borders
    assert(pane->window->x == 11);      // 10 + 1 for left border
    assert(pane->window->y == 6);       // 5 + 1 for top border

    // Check the underlying mock window
    if (pane->window->win) {
      mock_window_t *content_win = (mock_window_t *)pane->window->win;
      assert(content_win->width == 38);
      assert(content_win->height == 8);
      assert(content_win->x == 11);
      assert(content_win->y == 6);
    }
  } else {
    printf("  Debug: Content window is NULL!\n");
  }

  // Destroy pane
  err = app_tui_pane_destroy(manager, pane);
  assert(err == APP_SUCCESS);

  // Destroy manager
  err = app_tui_pane_manager_destroy(manager);
  assert(err == APP_SUCCESS);

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

  // Create pane manager
  app_tui_pane_manager_t *manager = NULL;
  err = app_tui_pane_manager_create(&manager, 9);
  assert(err == APP_SUCCESS);

  // Create a pane
  app_pane_rect_t rect = {.x = 0, .y = 0, .width = 20, .height = 10};
  app_tui_pane_t *pane = NULL;
  err = app_tui_pane_create(manager, "test-account", &rect, &pane);
  assert(err == APP_SUCCESS);

  // Set title
  err = app_tui_pane_set_title(pane, "Test");
  assert(err == APP_SUCCESS);

  // Refresh the pane (which renders it)
  err = app_tui_pane_refresh(pane);
  assert(err == APP_SUCCESS);

  // Verify border rendering
  mock_window_t *win = (mock_window_t *)pane->border_window->win;

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

  // Focus the pane and re-render
  err = app_tui_pane_focus(manager, pane);
  assert(err == APP_SUCCESS);
  err = app_tui_pane_refresh(pane);
  assert(err == APP_SUCCESS);

  // Verify title is rendered
  assert(app_mock_ncurses_verify_text(terminal, win, 0, 1, " Test "));

  app_tui_pane_destroy(manager, pane);
  app_tui_pane_manager_destroy(manager);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Pane rendering works correctly\n");
}

// Test pane content management
static void test_pane_content(void) {
  printf("Testing pane content management...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  // Create pane manager
  app_tui_pane_manager_t *manager = NULL;
  err = app_tui_pane_manager_create(&manager, 9);
  assert(err == APP_SUCCESS);

  // Create a pane
  app_pane_rect_t rect = {.x = 0, .y = 0, .width = 30, .height = 10};
  app_tui_pane_t *pane = NULL;
  err = app_tui_pane_create(manager, "test-account", &rect, &pane);
  assert(err == APP_SUCCESS);

  // Write content to the pane
  err = app_tui_pane_write(pane, "Hello, World!\n", 14);
  assert(err == APP_SUCCESS);
  err = app_tui_pane_write(pane, "Line 2\n", 7);
  assert(err == APP_SUCCESS);
  err = app_tui_pane_write(pane, "This is a longer line that might wrap\n", 38);
  assert(err == APP_SUCCESS);

  // Refresh the pane (which renders it)
  err = app_tui_pane_refresh(pane);
  assert(err == APP_SUCCESS);

  // Verify content is rendered
  mock_window_t *content_win = (mock_window_t *)pane->window->win;
  assert(app_mock_ncurses_verify_text(terminal, content_win, 0, 0,
                                      "Hello, World!"));
  assert(app_mock_ncurses_verify_text(terminal, content_win, 1, 0, "Line 2"));

  // Test scrolling
  err = app_tui_pane_scroll(pane, 1);
  assert(err == APP_SUCCESS);
  err = app_tui_pane_refresh(pane);
  assert(err == APP_SUCCESS);

  // After scrolling, Line 2 should be at the top
  assert(app_mock_ncurses_verify_text(terminal, content_win, 0, 0, "Line 2"));

  // Test clearing
  err = app_tui_pane_clear(pane);
  assert(err == APP_SUCCESS);
  err = app_tui_pane_refresh(pane);
  assert(err == APP_SUCCESS);

  // Content should be cleared
  assert(content_win->content[0][0] == ' ');

  app_tui_pane_destroy(manager, pane);
  app_tui_pane_manager_destroy(manager);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Pane content management works correctly\n");
}

// Test pane focus management
static void test_pane_focus(void) {
  printf("Testing pane focus management...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  // Create pane manager
  app_tui_pane_manager_t *manager = NULL;
  err = app_tui_pane_manager_create(&manager, 9);
  assert(err == APP_SUCCESS);

  // Create multiple panes
  app_pane_rect_t rect1 = {.x = 0, .y = 0, .width = 40, .height = 12};
  app_pane_rect_t rect2 = {.x = 40, .y = 0, .width = 40, .height = 12};
  app_pane_rect_t rect3 = {.x = 0, .y = 12, .width = 40, .height = 12};

  app_tui_pane_t *pane1 = NULL, *pane2 = NULL, *pane3 = NULL;
  err = app_tui_pane_create(manager, "account1", &rect1, &pane1);
  assert(err == APP_SUCCESS);
  err = app_tui_pane_create(manager, "account2", &rect2, &pane2);
  assert(err == APP_SUCCESS);
  err = app_tui_pane_create(manager, "account3", &rect3, &pane3);
  assert(err == APP_SUCCESS);

  // Initially no pane should have focus
  assert(manager->focused_pane == NULL);

  // Focus first pane
  err = app_tui_pane_focus(manager, pane1);
  assert(err == APP_SUCCESS);
  assert(manager->focused_pane == pane1);
  assert(pane1->has_focus == true);
  assert(pane2->has_focus == false);
  assert(pane3->has_focus == false);

  // Focus by number
  err = app_tui_pane_focus_by_number(manager, 2);
  assert(err == APP_SUCCESS);
  assert(manager->focused_pane == pane2);
  assert(pane1->has_focus == false);
  assert(pane2->has_focus == true);
  assert(pane3->has_focus == false);

  // Test focus navigation
  err = app_tui_pane_focus_next(manager);
  assert(err == APP_SUCCESS);
  assert(manager->focused_pane == pane3);

  err = app_tui_pane_focus_prev(manager);
  assert(err == APP_SUCCESS);
  assert(manager->focused_pane == pane2);

  // Test directional navigation would go here once implemented
  // For now, just verify we can navigate between panes using next/prev

  // Cleanup
  app_tui_pane_destroy(manager, pane1);
  app_tui_pane_destroy(manager, pane2);
  app_tui_pane_destroy(manager, pane3);
  app_tui_pane_manager_destroy(manager);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Pane focus management works correctly\n");
}

// Main test runner
int main(void) {
  printf("Running TUI pane tests...\n\n");

  test_pane_lifecycle();
  test_pane_rendering();
  test_pane_content();
  test_pane_focus();

  printf("\nAll pane tests passed! ✓\n");
  return 0;
}