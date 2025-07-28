/*
 * TUI Multiplexer Navigation Tests
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/core/error.h"
#include "../src/tui/tui_mux.h"
#include "../src/tui/tui_pane.h"
#include "../src/utils/memory.h"

// Mock NCurses for testing
#define ENABLE_TUI_TESTS 1
#include "../src/tui/tui_ncurses.h"

// Test navigation between panes
static void test_directional_navigation(void) {
  printf("Testing directional navigation...\n");

  // Create multiplexer
  app_tui_mux_config_t config = {
      .enable_mouse = false,
      .enable_colors = false,
      .default_layout = TUI_LAYOUT_GRID,
      .max_panes = 9,
      .refresh_rate_ms = 16,
      .min_pane_width = 20,
      .min_pane_height = 5,
      .wrap_navigation = true,
  };

  app_tui_mux_t *mux;
  assert(app_tui_mux_create(&mux, &config, NULL) == APP_SUCCESS);

  // Initialize TUI (will use mock NCurses if ENABLE_TUI_TESTS is defined)
  assert(app_tui_mux_init(mux) == APP_SUCCESS);

  // Add 4 panes in a 2x2 grid
  app_tui_pane_t *panes[4];
  for (int i = 0; i < 4; i++) {
    char account[32];
    snprintf(account, sizeof(account), "account%d", i + 1);
    assert(app_tui_mux_add_pane(mux, account, &panes[i]) == APP_SUCCESS);
  }

  // Panes should be arranged:
  // [0] [1]
  // [2] [3]

  // Focus pane 0 (top-left)
  assert(app_tui_pane_focus(mux->pane_manager, panes[0]) == APP_SUCCESS);
  assert(panes[0]->has_focus);

  // Navigate right to pane 1
  assert(app_tui_mux_focus_direction(mux, 1, 0) == APP_SUCCESS);
  assert(!panes[0]->has_focus);
  assert(panes[1]->has_focus);

  // Navigate down to pane 3
  assert(app_tui_mux_focus_direction(mux, 0, 1) == APP_SUCCESS);
  assert(!panes[1]->has_focus);
  assert(panes[3]->has_focus);

  // Navigate left to pane 2
  assert(app_tui_mux_focus_direction(mux, -1, 0) == APP_SUCCESS);
  assert(!panes[3]->has_focus);
  assert(panes[2]->has_focus);

  // Navigate up to pane 0
  assert(app_tui_mux_focus_direction(mux, 0, -1) == APP_SUCCESS);
  assert(!panes[2]->has_focus);
  assert(panes[0]->has_focus);

  // Test wrap-around: navigate left from pane 0 should go to pane 1
  assert(app_tui_mux_focus_direction(mux, -1, 0) == APP_SUCCESS);
  assert(!panes[0]->has_focus);
  assert(panes[1]->has_focus);

  assert(app_tui_mux_destroy(mux) == APP_SUCCESS);
  tui_cleanup();

  printf("  ✓ Directional navigation tests passed\n");
}

// Test numbered pane access
static void test_numbered_access(void) {
  printf("Testing numbered pane access...\n");

  app_tui_mux_config_t config = {0};
  config.max_panes = 9;

  app_tui_mux_t *mux;
  assert(app_tui_mux_create(&mux, &config, NULL) == APP_SUCCESS);

  // Initialize TUI
  assert(app_tui_mux_init(mux) == APP_SUCCESS);

  // Add 5 panes
  app_tui_pane_t *panes[5];
  for (int i = 0; i < 5; i++) {
    char account[32];
    snprintf(account, sizeof(account), "account%d", i + 1);
    assert(app_tui_mux_add_pane(mux, account, &panes[i]) == APP_SUCCESS);
  }

  // Focus pane 3 by number
  assert(app_tui_mux_focus_number(mux, 3) == APP_SUCCESS);
  assert(panes[2]->has_focus);  // Pane numbers are 1-based

  // Focus pane 1 by number
  assert(app_tui_mux_focus_number(mux, 1) == APP_SUCCESS);
  assert(!panes[2]->has_focus);
  assert(panes[0]->has_focus);

  // Focus pane 5 by number
  assert(app_tui_mux_focus_number(mux, 5) == APP_SUCCESS);
  assert(!panes[0]->has_focus);
  assert(panes[4]->has_focus);

  // Try invalid number (should do nothing)
  assert(app_tui_mux_focus_number(mux, 9) == APP_SUCCESS);
  assert(panes[4]->has_focus);  // Still focused

  assert(app_tui_mux_destroy(mux) == APP_SUCCESS);
  tui_cleanup();

  printf("  ✓ Numbered access tests passed\n");
}

// Test cycle navigation
static void test_cycle_navigation(void) {
  printf("Testing cycle navigation...\n");

  app_tui_mux_config_t config = {0};
  config.max_panes = 9;

  app_tui_mux_t *mux;
  assert(app_tui_mux_create(&mux, &config, NULL) == APP_SUCCESS);

  // Initialize TUI
  assert(app_tui_mux_init(mux) == APP_SUCCESS);

  // Add 3 panes
  app_tui_pane_t *panes[3];
  for (int i = 0; i < 3; i++) {
    char account[32];
    snprintf(account, sizeof(account), "account%d", i + 1);
    assert(app_tui_mux_add_pane(mux, account, &panes[i]) == APP_SUCCESS);
  }

  // Focus first pane
  assert(app_tui_pane_focus(mux->pane_manager, panes[0]) == APP_SUCCESS);

  // Cycle forward
  assert(app_tui_mux_cycle_focus(mux, false) == APP_SUCCESS);
  assert(!panes[0]->has_focus);
  assert(panes[1]->has_focus);

  assert(app_tui_mux_cycle_focus(mux, false) == APP_SUCCESS);
  assert(!panes[1]->has_focus);
  assert(panes[2]->has_focus);

  // Wrap around
  assert(app_tui_mux_cycle_focus(mux, false) == APP_SUCCESS);
  assert(!panes[2]->has_focus);
  assert(panes[0]->has_focus);

  // Cycle backward
  assert(app_tui_mux_cycle_focus(mux, true) == APP_SUCCESS);
  assert(!panes[0]->has_focus);
  assert(panes[2]->has_focus);

  assert(app_tui_mux_cycle_focus(mux, true) == APP_SUCCESS);
  assert(!panes[2]->has_focus);
  assert(panes[1]->has_focus);

  assert(app_tui_mux_destroy(mux) == APP_SUCCESS);
  tui_cleanup();

  printf("  ✓ Cycle navigation tests passed\n");
}

// Test last active pane
static void test_last_active(void) {
  printf("Testing last active pane focus...\n");

  app_tui_mux_config_t config = {0};
  config.max_panes = 9;

  app_tui_mux_t *mux;
  assert(app_tui_mux_create(&mux, &config, NULL) == APP_SUCCESS);

  // Initialize TUI
  assert(app_tui_mux_init(mux) == APP_SUCCESS);

  // Add 3 panes
  app_tui_pane_t *panes[3];
  for (int i = 0; i < 3; i++) {
    char account[32];
    snprintf(account, sizeof(account), "account%d", i + 1);
    assert(app_tui_mux_add_pane(mux, account, &panes[i]) == APP_SUCCESS);
  }

  // Focus pane 0, then pane 1
  assert(app_tui_pane_focus(mux->pane_manager, panes[0]) == APP_SUCCESS);
  assert(app_tui_pane_focus(mux->pane_manager, panes[1]) == APP_SUCCESS);

  // Focus last should go back to pane 0
  assert(app_tui_mux_focus_last(mux) == APP_SUCCESS);
  assert(panes[0]->has_focus);
  assert(!panes[1]->has_focus);

  // Focus last again should go back to pane 1
  assert(app_tui_mux_focus_last(mux) == APP_SUCCESS);
  assert(!panes[0]->has_focus);
  assert(panes[1]->has_focus);

  assert(app_tui_mux_destroy(mux) == APP_SUCCESS);
  tui_cleanup();

  printf("  ✓ Last active pane tests passed\n");
}

int main(void) {
  printf("Running TUI multiplexer navigation tests...\n\n");

  test_directional_navigation();
  test_numbered_access();
  test_cycle_navigation();
  test_last_active();

  printf("\nAll navigation tests passed! ✓\n");
  return 0;
}