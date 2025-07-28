/*
 * Test for terminal emulator (Task 9.2)
 */

#include "../src/tui/tui_term_emulator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/utils/logging.h"
#include "../src/utils/memory.h"

// Test basic terminal operations
static void test_basic_operations(void) {
  printf("Testing basic terminal operations...\n");

  tui_term_emulator_t *term = tui_term_emulator_create(80, 24, 100);
  if (!term) {
    printf("  ✗ Failed to create terminal emulator\n");
    return;
  }
  printf("  ✓ Terminal emulator created (80x24)\n");

  // Test writing simple text
  const char *text = "Hello, Terminal!";
  tui_term_emulator_process(term, text, strlen(text));

  // Check if text was written
  bool text_found = true;
  for (int i = 0; i < strlen(text); i++) {
    const tui_term_cell_t *cell = tui_term_emulator_get_cell(term, i, 0);
    if (!cell || cell->ch != text[i]) {
      text_found = false;
      break;
    }
  }

  if (text_found) {
    printf("  ✓ Simple text writing works\n");
  } else {
    printf("  ✗ Simple text writing failed\n");
  }

  // Test newline
  tui_term_emulator_process(term, "\n", 1);
  tui_term_emulator_process(term, "Line 2", 6);

  const tui_term_cell_t *cell = tui_term_emulator_get_cell(term, 0, 1);
  if (cell && cell->ch == 'L') {
    printf("  ✓ Newline handling works\n");
  } else {
    printf("  ✗ Newline handling failed\n");
  }

  // Test carriage return
  tui_term_emulator_process(term, "\rXYZ", 4);
  cell = tui_term_emulator_get_cell(term, 0, 1);
  if (cell && cell->ch == 'X') {
    printf("  ✓ Carriage return works\n");
  } else {
    printf("  ✗ Carriage return failed\n");
  }

  tui_term_emulator_destroy(term);
}

// Test ANSI escape sequences
static void test_ansi_sequences(void) {
  printf("\nTesting ANSI escape sequences...\n");

  tui_term_emulator_t *term = tui_term_emulator_create(80, 24, 100);
  if (!term) {
    printf("  ✗ Failed to create terminal emulator\n");
    return;
  }

  // Test cursor movement
  tui_term_emulator_process(term, "ABC", 3);
  tui_term_emulator_process(term, "\x1B[2D", 4);  // Move cursor back 2
  tui_term_emulator_process(term, "X", 1);

  const tui_term_cell_t *cell = tui_term_emulator_get_cell(term, 1, 0);
  if (cell && cell->ch == 'X') {
    printf("  ✓ Cursor back (ESC[D) works\n");
  } else {
    printf("  ✗ Cursor back failed\n");
  }

  // Test absolute cursor positioning
  tui_term_emulator_clear(term);
  tui_term_emulator_process(term, "\x1B[5;10H", 8);  // Move to row 5, col 10
  tui_term_emulator_process(term, "X", 1);

  cell = tui_term_emulator_get_cell(term, 9, 4);  // 0-based
  if (cell && cell->ch == 'X') {
    printf("  ✓ Cursor positioning (ESC[H) works\n");
  } else {
    printf("  ✗ Cursor positioning failed\n");
  }

  // Test clear screen
  tui_term_emulator_process(term, "\x1B[2J", 4);  // Clear entire screen
  bool screen_clear = true;
  for (int y = 0; y < 24 && screen_clear; y++) {
    for (int x = 0; x < 80 && screen_clear; x++) {
      cell = tui_term_emulator_get_cell(term, x, y);
      if (cell && cell->ch != ' ') {
        screen_clear = false;
      }
    }
  }

  if (screen_clear) {
    printf("  ✓ Clear screen (ESC[2J) works\n");
  } else {
    printf("  ✗ Clear screen failed\n");
  }

  // Test colors
  tui_term_emulator_clear(term);
  tui_term_emulator_process(term, "\x1B[31mRed\x1B[0m", 11);  // Red text

  cell = tui_term_emulator_get_cell(term, 0, 0);
  if (cell && cell->fg_color == TERM_COLOR_RED) {
    printf("  ✓ Foreground color (ESC[31m) works\n");
  } else {
    printf("  ✗ Foreground color failed\n");
  }

  // Test attributes
  tui_term_emulator_clear(term);
  tui_term_emulator_process(term, "\x1B[1mBold\x1B[0m", 12);  // Bold text

  cell = tui_term_emulator_get_cell(term, 0, 0);
  if (cell && (cell->attrs & TERM_ATTR_BOLD)) {
    printf("  ✓ Bold attribute (ESC[1m) works\n");
  } else {
    printf("  ✗ Bold attribute failed\n");
  }

  tui_term_emulator_destroy(term);
}

// Test scrolling
static void test_scrolling(void) {
  printf("\nTesting scrolling...\n");

  tui_term_emulator_t *term =
      tui_term_emulator_create(80, 5, 10);  // Small terminal
  if (!term) {
    printf("  ✗ Failed to create terminal emulator\n");
    return;
  }

  // Fill the screen
  for (int i = 0; i < 5; i++) {
    char line[81];
    snprintf(line, sizeof(line), "Line %d\n", i + 1);
    tui_term_emulator_process(term, line, strlen(line));
  }

  // Write one more line to trigger scroll
  tui_term_emulator_process(term, "Line 6\n", 7);

  // Check if Line 2 is now at the top
  const tui_term_cell_t *cell = tui_term_emulator_get_cell(term, 5, 0);
  if (cell && cell->ch == '2') {
    printf("  ✓ Scrolling works\n");
  } else {
    printf("  ✗ Scrolling failed\n");
  }

  tui_term_emulator_destroy(term);
}

// Test UTF-8 handling
static void test_utf8(void) {
  printf("\nTesting UTF-8 handling...\n");

  tui_term_emulator_t *term = tui_term_emulator_create(80, 24, 100);
  if (!term) {
    printf("  ✗ Failed to create terminal emulator\n");
    return;
  }

  // Test 2-byte UTF-8 (é)
  tui_term_emulator_process(term, "\xC3\xA9", 2);
  const tui_term_cell_t *cell = tui_term_emulator_get_cell(term, 0, 0);
  if (cell && cell->ch == 0xE9) {
    printf("  ✓ 2-byte UTF-8 works\n");
  } else {
    printf("  ✗ 2-byte UTF-8 failed (got %u)\n", cell ? cell->ch : 0);
  }

  // Test 3-byte UTF-8 (€)
  tui_term_emulator_clear(term);
  tui_term_emulator_process(term, "\xE2\x82\xAC", 3);
  cell = tui_term_emulator_get_cell(term, 0, 0);
  if (cell && cell->ch == 0x20AC) {
    printf("  ✓ 3-byte UTF-8 works\n");
  } else {
    printf("  ✗ 3-byte UTF-8 failed (got %u)\n", cell ? cell->ch : 0);
  }

  tui_term_emulator_destroy(term);
}

// Test complex sequence
static void test_complex_sequence(void) {
  printf("\nTesting complex sequence...\n");

  tui_term_emulator_t *term = tui_term_emulator_create(80, 24, 100);
  if (!term) {
    printf("  ✗ Failed to create terminal emulator\n");
    return;
  }

  // Simulate a typical prompt with colors
  const char *prompt = "\x1B[32muser@host\x1B[0m:\x1B[34m~/code\x1B[0m$ ";
  tui_term_emulator_process(term, prompt, strlen(prompt));

  // Check if colors were applied correctly
  const tui_term_cell_t *cell = tui_term_emulator_get_cell(term, 0, 0);
  bool colors_ok = cell && cell->fg_color == TERM_COLOR_GREEN;

  cell = tui_term_emulator_get_cell(term, 10, 0);  // After "user@host"
  colors_ok = colors_ok && cell && cell->fg_color == TERM_COLOR_DEFAULT;

  if (colors_ok) {
    printf("  ✓ Complex color sequence works\n");
  } else {
    printf("  ✗ Complex color sequence failed\n");
  }

  tui_term_emulator_destroy(term);
}

int main(void) {
  printf("Running terminal emulator tests...\n\n");

  // Initialize logging
  app_log_init();

  // Run tests
  test_basic_operations();
  test_ansi_sequences();
  test_scrolling();
  test_utf8();
  test_complex_sequence();

  printf("\nTerminal emulator tests completed!\n");

  return 0;
}