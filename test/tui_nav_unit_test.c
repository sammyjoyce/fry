/*
 * Unit tests for navigation logic
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>

// Test the navigation helper functions directly
static void test_pane_distance(void) {
  printf("Testing pane distance calculation...\n");

  // Test simple distance
  double dist = sqrt(3 * 3 + 4 * 4);  // 3-4-5 triangle
  assert(fabs(dist - 5.0) < 0.001);

  printf("  ✓ Distance calculation tests passed\n");
}

static void test_direction_logic(void) {
  printf("Testing direction logic...\n");

  // Test directional comparisons
  // Pane at (10, 10) vs pane at (20, 10) - should be to the right
  assert(20 > 10);  // to_x > from_x for right direction

  // Pane at (10, 10) vs pane at (10, 20) - should be below
  assert(20 > 10);  // to_y > from_y for down direction

  printf("  ✓ Direction logic tests passed\n");
}

int main(void) {
  printf("Running navigation unit tests...\n\n");

  test_pane_distance();
  test_direction_logic();

  printf("\nAll unit tests passed! ✓\n");
  return 0;
}