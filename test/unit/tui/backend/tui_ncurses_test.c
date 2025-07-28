#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Enable TUI tests
#define ENABLE_TUI_TESTS 1

#include "../src/tui/tui_layout.h"
#include "../src/tui/tui_ncurses.h"
#include "../src/tui/tui_pane.h"

// Test NCurses abstraction initialization
static void test_ncurses_abstraction_init(void) {
  printf("Testing NCurses abstraction initialization...\n");

  // Test real NCurses initialization
  app_error err = app_ncurses_init(true);
  assert(err == APP_SUCCESS);
  assert(g_ncurses != NULL);
  assert(g_ncurses->initscr != NULL);

  app_ncurses_cleanup();
  assert(g_ncurses == NULL);

  printf("  ✓ NCurses abstraction initialized correctly\n");
}

// Test mock NCurses
static void test_mock_ncurses(void) {
  printf("Testing mock NCurses...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);
  assert(terminal != NULL);
  assert(terminal->width == 80);
  assert(terminal->height == 24);
  assert(g_ncurses != NULL);

  // Test mock window creation
  WINDOW *win = g_ncurses->newwin(10, 20, 5, 10);
  assert(win != NULL);

  mock_window_t *mwin = (mock_window_t *)win;
  assert(mwin->width == 20);
  assert(mwin->height == 10);
  assert(mwin->x == 10);
  assert(mwin->y == 5);

  // Test mock output
  int result = g_ncurses->mvwprintw(win, 1, 2, "Hello, World!");
  assert(result == OK);

  // Verify content
  bool verified =
      app_mock_ncurses_verify_text(terminal, mwin, 1, 2, "Hello, World!");
  assert(verified);

  // Test mock input
  err = app_mock_ncurses_queue_input(terminal, 'q');
  assert(err == APP_SUCCESS);

  int ch = g_ncurses->getch();
  assert(ch == 'q');

  // Cleanup
  g_ncurses->delwin(win);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Mock NCurses works correctly\n");
}

// Test terminal capability detection
static void test_terminal_capabilities(void) {
  printf("Testing terminal capability detection...\n");

  app_terminal_caps_t caps;

  // Test without NCurses initialized
  app_error err = app_ncurses_detect_capabilities(&caps);
  assert(err == APP_SUCCESS);
  assert(caps.terminal_width > 0);
  assert(caps.terminal_height > 0);
  assert(caps.term_type != NULL);

  printf("  ✓ Terminal: %s, Size: %dx%d\n", caps.term_type, caps.terminal_width,
         caps.terminal_height);
}

// Test fallback mode detection
static void test_fallback_modes(void) {
  printf("Testing fallback mode detection...\n");

  app_terminal_caps_t caps;
  app_fallback_mode_t mode;

  // Test normal terminal
  caps.terminal_width = 80;
  caps.terminal_height = 24;
  caps.has_colors = true;
  caps.term_type = "xterm-256color";

  app_error err = app_ncurses_get_fallback_mode(&caps, &mode);
  assert(err == APP_SUCCESS);
  assert(mode == FALLBACK_MODE_NONE);

  // Test small terminal
  caps.terminal_width = 30;
  caps.terminal_height = 8;

  err = app_ncurses_get_fallback_mode(&caps, &mode);
  assert(err == APP_SUCCESS);
  assert(mode == FALLBACK_MODE_MINIMAL);

  // Test dumb terminal
  caps.terminal_width = 80;
  caps.terminal_height = 24;
  caps.term_type = "dumb";

  err = app_ncurses_get_fallback_mode(&caps, &mode);
  assert(err == APP_SUCCESS);
  assert(mode == FALLBACK_MODE_ASCII);

  // Test monochrome terminal
  caps.has_colors = false;
  caps.term_type = "xterm";

  err = app_ncurses_get_fallback_mode(&caps, &mode);
  assert(err == APP_SUCCESS);
  assert(mode == FALLBACK_MODE_MONOCHROME);

  printf("  ✓ Fallback modes detected correctly\n");
}

// Test mock window content manipulation
static void test_mock_window_content(void) {
  printf("Testing mock window content manipulation...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  // Create a window
  WINDOW *win = g_ncurses->newwin(10, 30, 0, 0);
  assert(win != NULL);

  mock_window_t *mwin = (mock_window_t *)win;

  // Test multiple prints
  g_ncurses->mvwprintw(win, 0, 0, "Line 1");
  g_ncurses->mvwprintw(win, 1, 0, "Line 2 with more text");
  g_ncurses->mvwprintw(win, 2, 5, "Indented");

  // Verify content
  assert(app_mock_ncurses_verify_text(terminal, mwin, 0, 0, "Line 1"));
  assert(app_mock_ncurses_verify_text(terminal, mwin, 1, 0,
                                      "Line 2 with more text"));
  assert(app_mock_ncurses_verify_text(terminal, mwin, 2, 5, "Indented"));

  // Test overwriting
  g_ncurses->mvwprintw(win, 0, 0, "New Line 1");
  assert(app_mock_ncurses_verify_text(terminal, mwin, 0, 0, "New Line 1"));

  // Test boundary conditions
  g_ncurses->mvwprintw(win, 9, 25, "Edge");  // Should fit
  assert(app_mock_ncurses_verify_text(terminal, mwin, 9, 25, "Edge"));

  // Test truncation
  g_ncurses->mvwprintw(win, 0, 25,
                       "This text is too long and will be truncated");
  // Should only have 5 characters (30 - 25)
  // Debug: print what's actually at positions 25-29
  printf("  Debug: Content at positions 25-29: '");
  for (int i = 25; i < 30; i++) {
    printf("%c", mwin->content[0][i]);
  }
  printf("'\n");
  // The text "This " (with space) fits in positions 25-29
  assert(mwin->content[0][25] == 'T');
  assert(mwin->content[0][26] == 'h');
  assert(mwin->content[0][27] == 'i');
  assert(mwin->content[0][28] == 's');
  assert(mwin->content[0][29] == ' ');  // Space after "This"

  // Cleanup
  g_ncurses->delwin(win);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Mock window content manipulation works correctly\n");
}

// Test input queue
static void test_mock_input_queue(void) {
  printf("Testing mock input queue...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  // Queue multiple inputs
  app_mock_ncurses_queue_input(terminal, 'a');
  app_mock_ncurses_queue_input(terminal, 'b');
  app_mock_ncurses_queue_input(terminal, KEY_UP);
  app_mock_ncurses_queue_input(terminal, '\n');

  // Read inputs in order
  assert(g_ncurses->getch() == 'a');
  assert(g_ncurses->getch() == 'b');
  assert(g_ncurses->getch() == KEY_UP);
  assert(g_ncurses->getch() == '\n');

  // Reading past end should return ERR
  assert(g_ncurses->getch() == ERR);

  // Test queue expansion
  for (int i = 0; i < 150; i++) {
    err = app_mock_ncurses_queue_input(terminal, 'x');
    assert(err == APP_SUCCESS);
  }

  // Verify we can read them all
  for (int i = 0; i < 150; i++) {
    assert(g_ncurses->getch() == 'x');
  }

  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Mock input queue works correctly\n");
}

int main(void) {
  printf("Running NCurses abstraction tests...\n\n");

  test_ncurses_abstraction_init();
  test_mock_ncurses();
  test_terminal_capabilities();
  test_fallback_modes();
  test_mock_window_content();
  test_mock_input_queue();

  printf("\nAll NCurses abstraction tests passed! ✓\n");
  return 0;
}