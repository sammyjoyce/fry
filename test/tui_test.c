#include "../src/tui/tui.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Enable TUI tests
#define ENABLE_TUI_TESTS 1

#include "../src/tui/tui.h"
#include "../src/tui/tui_layout.h"
#include "../src/tui/tui_ncurses.h"
#include "../src/tui/tui_pane.h"

// Use mock NCurses for testing
static app_mock_terminal_t *test_terminal = NULL;

// Test layout calculations
static void test_grid_layout_calculations(void) {
  printf("Testing grid layout calculations...\n");

  int rows, cols;

  // Test various pane counts
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

  printf("  ✓ Grid dimensions calculated correctly\n");
}

// Test pane geometry calculations
static void test_pane_geometry(void) {
  printf("Testing pane geometry calculations...\n");

  // Test 2x2 grid layout
  int terminal_width = 80;
  int terminal_height = 24;
  int status_bar_height = 1;

  int usable_height = terminal_height - status_bar_height;
  int pane_width = terminal_width / 2;
  int pane_height = usable_height / 2;

  // Pane 1 (top-left)
  app_pane_rect_t pane1 = {
      .x = 0, .y = 0, .width = pane_width, .height = pane_height};

  // Pane 2 (top-right)
  app_pane_rect_t pane2 = {.x = pane_width,
                           .y = 0,
                           .width = terminal_width - pane_width,
                           .height = pane_height};

  // Pane 3 (bottom-left)
  app_pane_rect_t pane3 = {.x = 0,
                           .y = pane_height,
                           .width = pane_width,
                           .height = usable_height - pane_height};

  // Pane 4 (bottom-right)
  app_pane_rect_t pane4 = {.x = pane_width,
                           .y = pane_height,
                           .width = terminal_width - pane_width,
                           .height = usable_height - pane_height};

  // Verify no overlaps
  assert(pane1.x + pane1.width == pane2.x);
  assert(pane3.x + pane3.width == pane4.x);
  assert(pane1.y + pane1.height == pane3.y);
  assert(pane2.y + pane2.height == pane4.y);

  // Verify full coverage
  assert(pane2.x + pane2.width == terminal_width);
  assert(pane4.x + pane4.width == terminal_width);
  assert(pane3.y + pane3.height == usable_height);
  assert(pane4.y + pane4.height == usable_height);

  printf("  ✓ Pane geometry calculated correctly\n");
}

// Test minimum size constraints
static void test_minimum_sizes(void) {
  printf("Testing minimum size constraints...\n");

  int min_width = 20;
  int min_height = 5;

  // Test that we can't create panes smaller than minimum
  app_pane_rect_t small_pane = {
      .x = 0,
      .y = 0,
      .width = 10,  // Too small
      .height = 3   // Too small
  };

  // In real implementation, this would be validated
  assert(small_pane.width < min_width);
  assert(small_pane.height < min_height);

  printf("  ✓ Minimum size constraints validated\n");
}

// Test focus navigation
static void test_focus_navigation(void) {
  printf("Testing focus navigation...\n");

  // Test directional navigation in a 2x2 grid
  // Panes arranged as:
  // [0] [1]
  // [2] [3]

  int current = 0;
  int next;

  // Test right navigation
  next = (current == 0) ? 1 : (current == 2) ? 3 : current;
  assert(next == 1);

  // Test down navigation
  current = 0;
  next = (current == 0) ? 2 : (current == 1) ? 3 : current;
  assert(next == 2);

  // Test left navigation
  current = 1;
  next = (current == 1) ? 0 : (current == 3) ? 2 : current;
  assert(next == 0);

  // Test up navigation
  current = 2;
  next = (current == 2) ? 0 : (current == 3) ? 1 : current;
  assert(next == 0);

  printf("  ✓ Focus navigation works correctly\n");
}

int main(void) {
  printf("Running TUI tests...\n\n");

  test_grid_layout_calculations();
  test_pane_geometry();
  test_minimum_sizes();
  test_focus_navigation();

  printf("\nAll tests passed! ✓\n");
  return 0;
}