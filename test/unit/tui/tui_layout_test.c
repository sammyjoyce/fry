#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENABLE_TUI_TESTS 1

#include "../src/tui/tui_layout.h"
#include "../src/tui/tui_ncurses.h"
#include "../src/tui/tui_pane.h"
#include "../src/utils/memory.h"

// Test layout creation and destruction
static void test_layout_lifecycle(void) {
  printf("Testing layout lifecycle...\n");

  // Initialize mock NCurses
  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  // Create a layout
  app_tui_layout_t *layout = NULL;
  err = app_tui_layout_create(&layout, APP_TUI_LAYOUT_GRID);
  assert(err == APP_SUCCESS);
  assert(layout != NULL);
  assert(layout->type == APP_TUI_LAYOUT_GRID);
  assert(layout->panes != NULL);
  assert(layout->pane_count == 0);
  assert(layout->capacity > 0);
  assert(layout->focused_pane == -1);

  // Destroy layout
  app_tui_layout_destroy(layout);

  // Cleanup
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Layout lifecycle works correctly\n");
}

// Test adding panes to layout
static void test_layout_add_panes(void) {
  printf("Testing adding panes to layout...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  app_tui_layout_t *layout = NULL;
  err = app_tui_layout_create(&layout, APP_TUI_LAYOUT_GRID);
  assert(err == APP_SUCCESS);

  // Add first pane
  int pane1_id = -1;
  err = app_tui_layout_add_pane(layout, "Pane 1", &pane1_id);
  assert(err == APP_SUCCESS);
  assert(pane1_id == 0);
  assert(layout->pane_count == 1);
  assert(layout->panes[0] != NULL);
  assert(strcmp(layout->panes[0]->title, "Pane 1") == 0);

  // Add second pane
  int pane2_id = -1;
  err = app_tui_layout_add_pane(layout, "Pane 2", &pane2_id);
  assert(err == APP_SUCCESS);
  assert(pane2_id == 1);
  assert(layout->pane_count == 2);

  // Add more panes to test capacity growth
  for (int i = 3; i <= 10; i++) {
    char title[32];
    snprintf(title, sizeof(title), "Pane %d", i);
    int pane_id = -1;
    err = app_tui_layout_add_pane(layout, title, &pane_id);
    assert(err == APP_SUCCESS);
    assert(pane_id == i - 1);
  }

  assert(layout->pane_count == 10);

  app_tui_layout_destroy(layout);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Adding panes to layout works correctly\n");
}

// Test removing panes from layout
static void test_layout_remove_panes(void) {
  printf("Testing removing panes from layout...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  app_tui_layout_t *layout = NULL;
  err = app_tui_layout_create(&layout, APP_TUI_LAYOUT_GRID);
  assert(err == APP_SUCCESS);

  // Add several panes
  int pane_ids[5];
  for (int i = 0; i < 5; i++) {
    char title[32];
    snprintf(title, sizeof(title), "Pane %d", i);
    err = app_tui_layout_add_pane(layout, title, &pane_ids[i]);
    assert(err == APP_SUCCESS);
  }

  assert(layout->pane_count == 5);

  // Remove middle pane
  err = app_tui_layout_remove_pane(layout, pane_ids[2]);
  assert(err == APP_SUCCESS);
  assert(layout->pane_count == 4);
  assert(layout->panes[2] == NULL);

  // Remove first pane
  err = app_tui_layout_remove_pane(layout, pane_ids[0]);
  assert(err == APP_SUCCESS);
  assert(layout->pane_count == 3);
  assert(layout->panes[0] == NULL);

  // Remove last pane
  err = app_tui_layout_remove_pane(layout, pane_ids[4]);
  assert(err == APP_SUCCESS);
  assert(layout->pane_count == 2);
  assert(layout->panes[4] == NULL);

  // Try to remove non-existent pane
  err = app_tui_layout_remove_pane(layout, 99);
  assert(err == APP_ERROR_NOT_FOUND);

  app_tui_layout_destroy(layout);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Removing panes from layout works correctly\n");
}

// Test grid layout calculations
static void test_grid_layout_calculations(void) {
  printf("Testing grid layout calculations...\n");

  // Test various pane counts
  int rows, cols;

  app_tui_layout_calculate_grid_dimensions(1, &rows, &cols);
  assert(rows == 1 && cols == 1);

  app_tui_layout_calculate_grid_dimensions(2, &rows, &cols);
  assert(rows == 1 && cols == 2);

  app_tui_layout_calculate_grid_dimensions(3, &rows, &cols);
  assert(rows == 2 && cols == 2);

  app_tui_layout_calculate_grid_dimensions(4, &rows, &cols);
  assert(rows == 2 && cols == 2);

  app_tui_layout_calculate_grid_dimensions(5, &rows, &cols);
  assert(rows == 2 && cols == 3);

  app_tui_layout_calculate_grid_dimensions(6, &rows, &cols);
  assert(rows == 2 && cols == 3);

  app_tui_layout_calculate_grid_dimensions(7, &rows, &cols);
  assert(rows == 3 && cols == 3);

  app_tui_layout_calculate_grid_dimensions(8, &rows, &cols);
  assert(rows == 3 && cols == 3);

  app_tui_layout_calculate_grid_dimensions(9, &rows, &cols);
  assert(rows == 3 && cols == 3);

  app_tui_layout_calculate_grid_dimensions(10, &rows, &cols);
  assert(rows == 3 && cols == 4);

  printf("  ✓ Grid layout calculations work correctly\n");
}

// Test layout arrangement
static void test_layout_arrange(void) {
  printf("Testing layout arrangement...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  app_tui_layout_t *layout = NULL;
  err = app_tui_layout_create(&layout, APP_TUI_LAYOUT_GRID);
  assert(err == APP_SUCCESS);

  // Add 4 panes for a 2x2 grid
  for (int i = 0; i < 4; i++) {
    char title[32];
    snprintf(title, sizeof(title), "Pane %d", i);
    int pane_id;
    err = app_tui_layout_add_pane(layout, title, &pane_id);
    assert(err == APP_SUCCESS);
  }

  // Arrange the layout
  err = app_tui_layout_arrange(layout);
  assert(err == APP_SUCCESS);

  // Verify pane positions for 2x2 grid on 80x24 terminal
  // Status bar takes 1 line, so usable height is 23
  int expected_width = 40;   // 80 / 2
  int expected_height = 11;  // 23 / 2 (rounded down)

  // Top-left pane
  assert(layout->panes[0]->rect.x == 0);
  assert(layout->panes[0]->rect.y == 0);
  assert(layout->panes[0]->rect.width == expected_width);
  assert(layout->panes[0]->rect.height == expected_height);

  // Top-right pane
  assert(layout->panes[1]->rect.x == expected_width);
  assert(layout->panes[1]->rect.y == 0);
  assert(layout->panes[1]->rect.width == expected_width);
  assert(layout->panes[1]->rect.height == expected_height);

  // Bottom-left pane
  assert(layout->panes[2]->rect.x == 0);
  assert(layout->panes[2]->rect.y == expected_height);
  assert(layout->panes[2]->rect.width == expected_width);
  assert(layout->panes[2]->rect.height == 12);  // Gets the extra line

  // Bottom-right pane
  assert(layout->panes[3]->rect.x == expected_width);
  assert(layout->panes[3]->rect.y == expected_height);
  assert(layout->panes[3]->rect.width == expected_width);
  assert(layout->panes[3]->rect.height == 12);  // Gets the extra line

  app_tui_layout_destroy(layout);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Layout arrangement works correctly\n");
}

// Test focus navigation
static void test_layout_focus_navigation(void) {
  printf("Testing layout focus navigation...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  app_tui_layout_t *layout = NULL;
  err = app_tui_layout_create(&layout, APP_TUI_LAYOUT_GRID);
  assert(err == APP_SUCCESS);

  // Add 4 panes for a 2x2 grid
  for (int i = 0; i < 4; i++) {
    char title[32];
    snprintf(title, sizeof(title), "Pane %d", i);
    int pane_id;
    err = app_tui_layout_add_pane(layout, title, &pane_id);
    assert(err == APP_SUCCESS);
  }

  err = app_tui_layout_arrange(layout);
  assert(err == APP_SUCCESS);

  // Initially no pane is focused
  assert(layout->focused_pane == -1);

  // Focus first pane
  err = app_tui_layout_focus_pane(layout, 0);
  assert(err == APP_SUCCESS);
  assert(layout->focused_pane == 0);
  assert(layout->panes[0]->is_focused == true);

  // Navigate right (0 -> 1)
  err = app_tui_layout_focus_next(layout, APP_TUI_FOCUS_RIGHT);
  assert(err == APP_SUCCESS);
  assert(layout->focused_pane == 1);
  assert(layout->panes[0]->is_focused == false);
  assert(layout->panes[1]->is_focused == true);

  // Navigate down (1 -> 3)
  err = app_tui_layout_focus_next(layout, APP_TUI_FOCUS_DOWN);
  assert(err == APP_SUCCESS);
  assert(layout->focused_pane == 3);

  // Navigate left (3 -> 2)
  err = app_tui_layout_focus_next(layout, APP_TUI_FOCUS_LEFT);
  assert(err == APP_SUCCESS);
  assert(layout->focused_pane == 2);

  // Navigate up (2 -> 0)
  err = app_tui_layout_focus_next(layout, APP_TUI_FOCUS_UP);
  assert(err == APP_SUCCESS);
  assert(layout->focused_pane == 0);

  // Test wrapping
  // Navigate left from leftmost pane (should wrap to rightmost)
  err = app_tui_layout_focus_next(layout, APP_TUI_FOCUS_LEFT);
  assert(err == APP_SUCCESS);
  assert(layout->focused_pane == 1);

  app_tui_layout_destroy(layout);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Layout focus navigation works correctly\n");
}

// Test layout with different terminal sizes
static void test_layout_terminal_sizes(void) {
  printf("Testing layout with different terminal sizes...\n");

  // Test small terminal
  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 40, 12);
  assert(err == APP_SUCCESS);

  app_tui_layout_t *layout = NULL;
  err = app_tui_layout_create(&layout, APP_TUI_LAYOUT_GRID);
  assert(err == APP_SUCCESS);

  // Add 2 panes
  for (int i = 0; i < 2; i++) {
    char title[32];
    snprintf(title, sizeof(title), "Pane %d", i);
    int pane_id;
    err = app_tui_layout_add_pane(layout, title, &pane_id);
    assert(err == APP_SUCCESS);
  }

  err = app_tui_layout_arrange(layout);
  assert(err == APP_SUCCESS);

  // Verify panes fit in small terminal
  assert(layout->panes[0]->rect.width == 20);
  assert(layout->panes[1]->rect.width == 20);

  app_tui_layout_destroy(layout);
  app_mock_ncurses_cleanup(terminal);

  // Test large terminal
  err = app_mock_ncurses_init(&terminal, 160, 48);
  assert(err == APP_SUCCESS);

  err = app_tui_layout_create(&layout, APP_TUI_LAYOUT_GRID);
  assert(err == APP_SUCCESS);

  // Add 6 panes
  for (int i = 0; i < 6; i++) {
    char title[32];
    snprintf(title, sizeof(title), "Pane %d", i);
    int pane_id;
    err = app_tui_layout_add_pane(layout, title, &pane_id);
    assert(err == APP_SUCCESS);
  }

  err = app_tui_layout_arrange(layout);
  assert(err == APP_SUCCESS);

  // Verify panes use available space
  // 6 panes = 2x3 grid
  assert(layout->panes[0]->rect.width >= 53);  // ~160/3

  app_tui_layout_destroy(layout);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Layout works with different terminal sizes\n");
}

// Test layout type switching
static void test_layout_type_switching(void) {
  printf("Testing layout type switching...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  app_tui_layout_t *layout = NULL;
  err = app_tui_layout_create(&layout, APP_TUI_LAYOUT_GRID);
  assert(err == APP_SUCCESS);

  // Add panes
  for (int i = 0; i < 3; i++) {
    char title[32];
    snprintf(title, sizeof(title), "Pane %d", i);
    int pane_id;
    err = app_tui_layout_add_pane(layout, title, &pane_id);
    assert(err == APP_SUCCESS);
  }

  // Arrange as grid
  err = app_tui_layout_arrange(layout);
  assert(err == APP_SUCCESS);

  // Switch to vertical split
  err = app_tui_layout_set_type(layout, APP_TUI_LAYOUT_VSPLIT);
  assert(err == APP_SUCCESS);
  assert(layout->type == APP_TUI_LAYOUT_VSPLIT);

  err = app_tui_layout_arrange(layout);
  assert(err == APP_SUCCESS);

  // Verify vertical arrangement
  assert(layout->panes[0]->rect.x == 0);
  assert(layout->panes[1]->rect.x > 0);
  assert(layout->panes[2]->rect.x > layout->panes[1]->rect.x);

  // Switch to horizontal split
  err = app_tui_layout_set_type(layout, APP_TUI_LAYOUT_HSPLIT);
  assert(err == APP_SUCCESS);
  assert(layout->type == APP_TUI_LAYOUT_HSPLIT);

  err = app_tui_layout_arrange(layout);
  assert(err == APP_SUCCESS);

  // Verify horizontal arrangement
  assert(layout->panes[0]->rect.y == 0);
  assert(layout->panes[1]->rect.y > 0);
  assert(layout->panes[2]->rect.y > layout->panes[1]->rect.y);

  app_tui_layout_destroy(layout);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Layout type switching works correctly\n");
}

int main(void) {
  printf("Running TUI layout tests...\n\n");

  test_layout_lifecycle();
  test_layout_add_panes();
  test_layout_remove_panes();
  test_grid_layout_calculations();
  test_layout_arrange();
  test_layout_focus_navigation();
  test_layout_terminal_sizes();
  test_layout_type_switching();

  printf("\nAll layout tests passed! ✓\n");
  return 0;
}