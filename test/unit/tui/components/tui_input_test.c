/*
 * TUI Input System Tests
 */

#include "../src/tui/tui_input.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../src/core/error.h"
#include "../src/tui/tui_input.h"
#include "../src/utils/memory.h"

// Test context for keybindings
typedef struct {
  int action_called;
  app_key_event_t last_event;
} test_context_t;

// Test keybinding action
static app_error test_action(void *context, const app_key_event_t *event) {
  test_context_t *ctx = (test_context_t *)context;
  ctx->action_called++;
  ctx->last_event = *event;
  return APP_SUCCESS;
}

// Test escape sequence parsing
static void test_escape_sequences(void) {
  printf("Testing escape sequence parsing...\n");

  app_key_event_t event;
  size_t consumed;

  // Test arrow keys
  const char *up_arrow = "\033[A";
  app_error err =
      app_tui_input_parse_escape_sequence(up_arrow, 3, &event, &consumed);
  printf("  Up arrow parse result: %s, code=%d, consumed=%zu\n",
         app_error_string(err), event.code, consumed);
  assert(err == APP_SUCCESS);
  assert(event.code == APP_KEY_UP);
  assert(event.is_special == true);
  assert(consumed == 3);

  const char *down_arrow = "\033[B";
  assert(app_tui_input_parse_escape_sequence(down_arrow, 3, &event,
                                             &consumed) == APP_SUCCESS);
  assert(event.code == APP_KEY_DOWN);

  // Test function keys
  const char *f1 = "\033OP";
  assert(app_tui_input_parse_escape_sequence(f1, 3, &event, &consumed) ==
         APP_SUCCESS);
  assert(event.code == APP_KEY_F1);

  // Test home/end
  const char *home = "\033[H";
  assert(app_tui_input_parse_escape_sequence(home, 3, &event, &consumed) ==
         APP_SUCCESS);
  assert(event.code == APP_KEY_HOME);

  // Test delete key
  const char *del = "\033[3~";
  assert(app_tui_input_parse_escape_sequence(del, 4, &event, &consumed) ==
         APP_SUCCESS);
  assert(event.code == APP_KEY_DELETE);

  // Test Alt+key
  const char *alt_a = "\033a";
  assert(app_tui_input_parse_escape_sequence(alt_a, 2, &event, &consumed) ==
         APP_SUCCESS);
  assert(event.code == 'a');
  assert(event.modifiers == KEY_MOD_ALT);

  // Test incomplete sequence
  const char *incomplete = "\033[";
  assert(app_tui_input_parse_escape_sequence(
             incomplete, 2, &event, &consumed) == APP_ERROR_INCOMPLETE);

  printf("  ✓ Escape sequence parsing tests passed\n");
}

// Test key name formatting
static void test_key_names(void) {
  printf("Testing key name formatting...\n");

  assert(strcmp(app_tui_input_key_name('a', KEY_MOD_NONE), "a") == 0);
  assert(strcmp(app_tui_input_key_name('A', KEY_MOD_SHIFT), "Shift-A") == 0);
  assert(strcmp(app_tui_input_key_name('C', KEY_MOD_CTRL), "Ctrl-C") == 0);
  assert(strcmp(app_tui_input_key_name('X', KEY_MOD_ALT), "Alt-X") == 0);
  assert(strcmp(app_tui_input_key_name(APP_KEY_UP, KEY_MOD_NONE), "Up") == 0);
  assert(strcmp(app_tui_input_key_name(APP_KEY_F1, KEY_MOD_NONE), "F1") == 0);
  assert(strcmp(app_tui_input_key_name(APP_KEY_ESCAPE, KEY_MOD_NONE), "Esc") == 0);
  assert(strcmp(app_tui_input_key_name('Q', KEY_MOD_CTRL | KEY_MOD_ALT),
                "Ctrl-Alt-Q") == 0);

  printf("  ✓ Key name formatting tests passed\n");
}

// Test key string parsing
static void test_key_string_parsing(void) {
  printf("Testing key string parsing...\n");

  int key;
  app_key_modifier_t modifiers;

  // Simple keys
  assert(app_tui_input_parse_key_string("a", &key, &modifiers) == APP_SUCCESS);
  assert(key == 'a' && modifiers == KEY_MOD_NONE);

  assert(app_tui_input_parse_key_string("Ctrl-C", &key, &modifiers) ==
         APP_SUCCESS);
  assert(key == 'C' && modifiers == KEY_MOD_CTRL);

  assert(app_tui_input_parse_key_string("Alt-X", &key, &modifiers) ==
         APP_SUCCESS);
  assert(key == 'X' && modifiers == KEY_MOD_ALT);

  // Special keys
  assert(app_tui_input_parse_key_string("Escape", &key, &modifiers) ==
         APP_SUCCESS);
  assert(key == APP_KEY_ESCAPE);

  assert(app_tui_input_parse_key_string("F1", &key, &modifiers) == APP_SUCCESS);
  assert(key == APP_KEY_F1);

  assert(app_tui_input_parse_key_string("Up", &key, &modifiers) == APP_SUCCESS);
  assert(key == APP_KEY_UP);

  // Multiple modifiers
  assert(app_tui_input_parse_key_string("Ctrl-Alt-Q", &key, &modifiers) ==
         APP_SUCCESS);
  assert(key == 'Q' && modifiers == (KEY_MOD_CTRL | KEY_MOD_ALT));

  printf("  ✓ Key string parsing tests passed\n");
}

// Test keybinding system
static void test_keybindings(void) {
  printf("Testing keybinding system...\n");

  app_tui_input_t *input;
  assert(app_tui_input_create(&input) == APP_SUCCESS);

  test_context_t ctx = {0};

  // Bind some keys
  assert(app_tui_input_bind_key(input, 'q', KEY_MOD_NONE, test_action, &ctx) ==
         APP_SUCCESS);
  assert(app_tui_input_bind_key(input, 'C', KEY_MOD_CTRL, test_action, &ctx) ==
         APP_SUCCESS);
  assert(app_tui_input_bind_key(input, APP_KEY_F1, KEY_MOD_NONE, test_action,
                                &ctx) == APP_SUCCESS);

  // Test processing
  app_key_event_t event = {.code = 'q', .modifiers = KEY_MOD_NONE};
  assert(app_tui_input_process_keybinding(input, &event) == APP_SUCCESS);
  assert(ctx.action_called == 1);
  assert(ctx.last_event.code == 'q');

  event.code = 'C';
  event.modifiers = KEY_MOD_CTRL;
  assert(app_tui_input_process_keybinding(input, &event) == APP_SUCCESS);
  assert(ctx.action_called == 2);

  // Test unbound key
  event.code = 'x';
  event.modifiers = KEY_MOD_NONE;
  assert(app_tui_input_process_keybinding(input, &event) ==
         APP_ERROR_NOT_FOUND);

  // Test unbinding
  assert(app_tui_input_unbind_key(input, 'q', KEY_MOD_NONE) == APP_SUCCESS);
  event.code = 'q';
  assert(app_tui_input_process_keybinding(input, &event) ==
         APP_ERROR_NOT_FOUND);

  app_tui_input_destroy(input);

  printf("  ✓ Keybinding tests passed\n");
}

// Test input buffer handling
static void test_input_buffer(void) {
  printf("Testing input buffer handling...\n");

  // This test would require mocking stdin, which is complex
  // For now, we'll just test the parsing functions

  assert(app_tui_input_is_escape_sequence("\033", 1) == true);
  assert(app_tui_input_is_escape_sequence("\033[", 2) == true);
  assert(app_tui_input_is_escape_sequence("\033O", 2) == true);
  assert(app_tui_input_is_escape_sequence("abc", 3) == false);

  printf("  ✓ Input buffer tests passed\n");
}

int main(void) {
  printf("Running TUI input system tests...\n\n");

  test_escape_sequences();
  test_key_names();
  test_key_string_parsing();
  test_keybindings();
  test_input_buffer();

  printf("\nAll tests passed! ✓\n");
  return 0;
}