#include "../src/tui/tui_command.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/tui/tui_mux.h"
#include "../src/utils/memory.h"

// Mock NCurses for testing
#define ENABLE_TUI_TESTS 1

// Test command mode initialization
static void test_command_init(void) {
  printf("Testing command mode initialization...\n");

  app_tui_mux_config_t config = {.enable_mouse = false,
                                 .enable_colors = true,
                                 .default_layout = TUI_LAYOUT_GRID,
                                 .max_panes = 9,
                                 .refresh_rate_ms = 50};

  app_tui_mux_t mux = {0};
  mux.config = config;
  mux.command_buffer_size = 256;
  mux.command_buffer = app_calloc(mux.command_buffer_size, 1);

  app_error err = app_tui_command_init(&mux);
  assert(err == APP_SUCCESS);
  assert(mux.history != NULL);
  assert(mux.history->capacity == 100);

  err = app_tui_command_cleanup(&mux);
  assert(err == APP_SUCCESS);

  app_free(mux.command_buffer);
  printf("✓ Command mode initialization\n");
}

// Test command parsing
static void test_command_parsing(void) {
  printf("Testing command parsing...\n");

  app_tui_mux_config_t config = {.enable_mouse = false,
                                 .enable_colors = true,
                                 .default_layout = TUI_LAYOUT_GRID,
                                 .max_panes = 9,
                                 .refresh_rate_ms = 50};

  app_tui_mux_t mux = {0};
  mux.config = config;
  mux.command_buffer_size = 256;
  mux.command_buffer = app_calloc(mux.command_buffer_size, 1);
  mux.running = true;

  app_error err = app_tui_command_init(&mux);
  assert(err == APP_SUCCESS);

  // Test quit command
  err = app_tui_command_parse_and_execute(&mux, "quit");
  assert(err == APP_SUCCESS);
  assert(mux.running == false);

  // Test help command
  err = app_tui_command_parse_and_execute(&mux, "help");
  assert(err == APP_SUCCESS);

  // Test invalid command
  err = app_tui_command_parse_and_execute(&mux, "invalid_command");
  assert(err == APP_ERROR_NOT_FOUND);

  // Test empty command
  err = app_tui_command_parse_and_execute(&mux, "");
  assert(err == APP_SUCCESS);

  // Test command with arguments
  err = app_tui_command_parse_and_execute(&mux, "set mouse on");
  assert(err == APP_SUCCESS);
  assert(mux.config.enable_mouse == true);

  err = app_tui_command_cleanup(&mux);
  assert(err == APP_SUCCESS);

  app_free(mux.command_buffer);
  printf("✓ Command parsing\n");
}

// Test command completion
static void test_command_completion(void) {
  printf("Testing command completion...\n");

  app_tui_mux_t mux = {0};
  app_command_completion_t completion = {0};

  // Test completion for "q"
  app_error err = app_tui_command_complete(&mux, "q", &completion);
  assert(err == APP_SUCCESS);
  assert(completion.count == 1);
  assert(strcmp(completion.suggestions[0], "quit") == 0);
  app_tui_command_free_completion(&completion);

  // Test completion for "s"
  err = app_tui_command_complete(&mux, "s", &completion);
  assert(err == APP_SUCCESS);
  assert(completion.count >= 3);  // split, sp, save, set, s
  app_tui_command_free_completion(&completion);

  // Test completion for "hel"
  err = app_tui_command_complete(&mux, "hel", &completion);
  assert(err == APP_SUCCESS);
  assert(completion.count == 1);
  assert(strcmp(completion.suggestions[0], "help") == 0);
  app_tui_command_free_completion(&completion);

  // Test completion for non-matching prefix
  err = app_tui_command_complete(&mux, "xyz", &completion);
  assert(err == APP_SUCCESS);
  assert(completion.count == 0);
  app_tui_command_free_completion(&completion);

  printf("✓ Command completion\n");
}

// Test command history
static void test_command_history(void) {
  printf("Testing command history...\n");

  app_tui_mux_config_t config = {.enable_mouse = false,
                                 .enable_colors = true,
                                 .default_layout = TUI_LAYOUT_GRID,
                                 .max_panes = 9,
                                 .refresh_rate_ms = 50};

  app_tui_mux_t mux = {0};
  mux.config = config;
  mux.command_buffer_size = 256;
  mux.command_buffer = app_calloc(mux.command_buffer_size, 1);

  app_error err = app_tui_command_init(&mux);
  assert(err == APP_SUCCESS);

  // Add commands to history
  err = app_tui_command_history_add(&mux, "help");
  assert(err == APP_SUCCESS);
  assert(mux.history->count == 1);

  err = app_tui_command_history_add(&mux, "quit");
  assert(err == APP_SUCCESS);
  assert(mux.history->count == 2);

  // Test duplicate command (should not be added)
  err = app_tui_command_history_add(&mux, "quit");
  assert(err == APP_SUCCESS);
  assert(mux.history->count == 2);

  // Test empty command (should not be added)
  err = app_tui_command_history_add(&mux, "");
  assert(err == APP_SUCCESS);
  assert(mux.history->count == 2);

  // Navigate history
  mux.history->current = mux.history->count;
  err = app_tui_command_history_prev(&mux);
  assert(err == APP_SUCCESS);
  assert(strcmp(mux.command_buffer, "quit") == 0);

  err = app_tui_command_history_prev(&mux);
  assert(err == APP_SUCCESS);
  assert(strcmp(mux.command_buffer, "help") == 0);

  err = app_tui_command_history_next(&mux);
  assert(err == APP_SUCCESS);
  assert(strcmp(mux.command_buffer, "quit") == 0);

  err = app_tui_command_cleanup(&mux);
  assert(err == APP_SUCCESS);

  app_free(mux.command_buffer);
  printf("✓ Command history\n");
}

// Test command input handling
static void test_command_input(void) {
  printf("Testing command input handling...\n");

  app_tui_mux_config_t config = {.enable_mouse = false,
                                 .enable_colors = true,
                                 .default_layout = TUI_LAYOUT_GRID,
                                 .max_panes = 9,
                                 .refresh_rate_ms = 50};

  app_tui_mux_t mux = {0};
  mux.config = config;
  mux.command_buffer_size = 256;
  mux.command_buffer = app_calloc(mux.command_buffer_size, 1);
  mux.input_mode = INPUT_MODE_COMMAND;

  app_error err = app_tui_command_init(&mux);
  assert(err == APP_SUCCESS);

  // Test character insertion
  err = app_tui_command_insert_char(&mux, 'h');
  assert(err == APP_SUCCESS);
  assert(strcmp(mux.command_buffer, "h") == 0);
  assert(mux.command_cursor == 1);

  err = app_tui_command_insert_char(&mux, 'e');
  assert(err == APP_SUCCESS);
  assert(strcmp(mux.command_buffer, "he") == 0);
  assert(mux.command_cursor == 2);

  err = app_tui_command_insert_char(&mux, 'l');
  assert(err == APP_SUCCESS);
  assert(strcmp(mux.command_buffer, "hel") == 0);
  assert(mux.command_cursor == 3);

  err = app_tui_command_insert_char(&mux, 'p');
  assert(err == APP_SUCCESS);
  assert(strcmp(mux.command_buffer, "help") == 0);
  assert(mux.command_cursor == 4);

  // Test cursor movement
  err = app_tui_command_move_cursor(&mux, -2);
  assert(err == APP_SUCCESS);
  assert(mux.command_cursor == 2);

  // Test character deletion
  err = app_tui_command_delete_char(&mux);
  assert(err == APP_SUCCESS);
  assert(strcmp(mux.command_buffer, "hlp") == 0);
  assert(mux.command_cursor == 1);

  // Test cursor at beginning
  mux.command_cursor = 0;
  err = app_tui_command_delete_char(&mux);
  assert(err == APP_SUCCESS);  // Should do nothing
  assert(strcmp(mux.command_buffer, "hlp") == 0);

  err = app_tui_command_cleanup(&mux);
  assert(err == APP_SUCCESS);

  app_free(mux.command_buffer);
  printf("✓ Command input handling\n");
}

int main(void) {
  printf("Running TUI command mode tests...\n\n");

  test_command_init();
  test_command_parsing();
  test_command_completion();
  test_command_history();
  test_command_input();

  printf("\nAll tests passed! ✓\n");
  return 0;
}