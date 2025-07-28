#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ENABLE_TUI_TESTS 1

#include "../src/tui/tui_mux.h"
#include "../src/tui/tui_ncurses.h"
#include "../src/utils/memory.h"

// Test multiplexer creation and destruction
static void test_mux_lifecycle(void) {
  printf("Testing multiplexer lifecycle...\n");

  // Initialize mock NCurses
  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  // Create multiplexer
  app_tui_mux_t *mux = NULL;
  err = app_tui_mux_create(&mux, "test-session");
  assert(err == APP_SUCCESS);
  assert(mux != NULL);
  assert(strcmp(mux->session_name, "test-session") == 0);
  assert(mux->layout != NULL);
  assert(mux->mode == APP_TUI_MODE_NORMAL);
  assert(mux->is_running == false);
  assert(mux->status_message == NULL);

  // Destroy multiplexer
  app_tui_mux_destroy(mux);

  // Cleanup
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Multiplexer lifecycle works correctly\n");
}

// Test creating sessions
static void test_mux_create_session(void) {
  printf("Testing session creation...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  app_tui_mux_t *mux = NULL;
  err = app_tui_mux_create(&mux, "test");
  assert(err == APP_SUCCESS);

  // Create a session
  int session_id = -1;
  err = app_tui_mux_create_session(mux, "Session 1", "/bin/sh", &session_id);
  assert(err == APP_SUCCESS);
  assert(session_id == 0);

  // Verify session was created
  app_tui_session_t *session = app_tui_mux_get_session(mux, session_id);
  assert(session != NULL);
  assert(strcmp(session->name, "Session 1") == 0);
  assert(session->pane_id >= 0);
  assert(session->pty != NULL);

  // Create another session
  int session_id2 = -1;
  err = app_tui_mux_create_session(mux, "Session 2", "/bin/bash", &session_id2);
  assert(err == APP_SUCCESS);
  assert(session_id2 == 1);
  assert(session_id2 != session_id);

  app_tui_mux_destroy(mux);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Session creation works correctly\n");
}

// Test closing sessions
static void test_mux_close_session(void) {
  printf("Testing session closing...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  app_tui_mux_t *mux = NULL;
  err = app_tui_mux_create(&mux, "test");
  assert(err == APP_SUCCESS);

  // Create sessions
  int session_ids[3];
  for (int i = 0; i < 3; i++) {
    char name[32];
    snprintf(name, sizeof(name), "Session %d", i);
    err = app_tui_mux_create_session(mux, name, "/bin/sh", &session_ids[i]);
    assert(err == APP_SUCCESS);
  }

  // Close middle session
  err = app_tui_mux_close_session(mux, session_ids[1]);
  assert(err == APP_SUCCESS);

  // Verify session is gone
  app_tui_session_t *session = app_tui_mux_get_session(mux, session_ids[1]);
  assert(session == NULL);

  // Verify other sessions still exist
  session = app_tui_mux_get_session(mux, session_ids[0]);
  assert(session != NULL);
  session = app_tui_mux_get_session(mux, session_ids[2]);
  assert(session != NULL);

  // Try to close non-existent session
  err = app_tui_mux_close_session(mux, 99);
  assert(err == APP_ERROR_NOT_FOUND);

  app_tui_mux_destroy(mux);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Session closing works correctly\n");
}

// Test session switching
static void test_mux_switch_session(void) {
  printf("Testing session switching...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  app_tui_mux_t *mux = NULL;
  err = app_tui_mux_create(&mux, "test");
  assert(err == APP_SUCCESS);

  // Create multiple sessions
  int session_ids[3];
  for (int i = 0; i < 3; i++) {
    char name[32];
    snprintf(name, sizeof(name), "Session %d", i);
    err = app_tui_mux_create_session(mux, name, "/bin/sh", &session_ids[i]);
    assert(err == APP_SUCCESS);
  }

  // Initially first session should be active
  assert(mux->active_session == session_ids[0]);

  // Switch to second session
  err = app_tui_mux_switch_session(mux, session_ids[1]);
  assert(err == APP_SUCCESS);
  assert(mux->active_session == session_ids[1]);

  // Switch to third session
  err = app_tui_mux_switch_session(mux, session_ids[2]);
  assert(err == APP_SUCCESS);
  assert(mux->active_session == session_ids[2]);

  // Try to switch to non-existent session
  err = app_tui_mux_switch_session(mux, 99);
  assert(err == APP_ERROR_NOT_FOUND);
  assert(mux->active_session == session_ids[2]);  // Should remain unchanged

  app_tui_mux_destroy(mux);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Session switching works correctly\n");
}

// Test input handling
static void test_mux_input_handling(void) {
  printf("Testing input handling...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  app_tui_mux_t *mux = NULL;
  err = app_tui_mux_create(&mux, "test");
  assert(err == APP_SUCCESS);

  // Create a session
  int session_id;
  err = app_tui_mux_create_session(mux, "Test", "/bin/cat", &session_id);
  assert(err == APP_SUCCESS);

  // Test normal mode input (should not go to PTY)
  assert(mux->mode == APP_TUI_MODE_NORMAL);
  bool handled = false;
  err = app_tui_mux_handle_input(mux, 'q', &handled);
  assert(err == APP_SUCCESS);
  assert(handled == true);  // 'q' quits in normal mode

  // Test switching to insert mode
  err = app_tui_mux_handle_input(mux, 'i', &handled);
  assert(err == APP_SUCCESS);
  assert(handled == true);
  assert(mux->mode == APP_TUI_MODE_INSERT);

  // Test insert mode input (should go to PTY)
  err = app_tui_mux_handle_input(mux, 'a', &handled);
  assert(err == APP_SUCCESS);
  assert(handled == true);

  // Test escape back to normal mode
  err = app_tui_mux_handle_input(mux, 27, &handled);  // ESC
  assert(err == APP_SUCCESS);
  assert(handled == true);
  assert(mux->mode == APP_TUI_MODE_NORMAL);

  // Test command mode
  err = app_tui_mux_handle_input(mux, ':', &handled);
  assert(err == APP_SUCCESS);
  assert(handled == true);
  assert(mux->mode == APP_TUI_MODE_COMMAND);

  app_tui_mux_destroy(mux);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Input handling works correctly\n");
}

// Test command execution
static void test_mux_commands(void) {
  printf("Testing command execution...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  app_tui_mux_t *mux = NULL;
  err = app_tui_mux_create(&mux, "test");
  assert(err == APP_SUCCESS);

  // Create sessions
  int session_id;
  err = app_tui_mux_create_session(mux, "Test", "/bin/sh", &session_id);
  assert(err == APP_SUCCESS);

  // Test new session command
  err = app_tui_mux_execute_command(mux, "new");
  assert(err == APP_SUCCESS);

  // Test split commands
  err = app_tui_mux_execute_command(mux, "split");
  assert(err == APP_SUCCESS);

  err = app_tui_mux_execute_command(mux, "vsplit");
  assert(err == APP_SUCCESS);

  // Test invalid command
  err = app_tui_mux_execute_command(mux, "invalid");
  assert(err == APP_ERROR_UNKNOWN_COMMAND);

  // Test quit command (should set running to false)
  mux->is_running = true;
  err = app_tui_mux_execute_command(mux, "q");
  assert(err == APP_SUCCESS);
  assert(mux->is_running == false);

  app_tui_mux_destroy(mux);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Command execution works correctly\n");
}

// Test status messages
static void test_mux_status_messages(void) {
  printf("Testing status messages...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  app_tui_mux_t *mux = NULL;
  err = app_tui_mux_create(&mux, "test");
  assert(err == APP_SUCCESS);

  // Set status message
  err = app_tui_mux_set_status(mux, "Test status message");
  assert(err == APP_SUCCESS);
  assert(mux->status_message != NULL);
  assert(strcmp(mux->status_message, "Test status message") == 0);

  // Update status message
  err = app_tui_mux_set_status(mux, "Updated message");
  assert(err == APP_SUCCESS);
  assert(strcmp(mux->status_message, "Updated message") == 0);

  // Clear status message
  err = app_tui_mux_set_status(mux, NULL);
  assert(err == APP_SUCCESS);
  assert(mux->status_message == NULL);

  app_tui_mux_destroy(mux);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Status messages work correctly\n");
}

// Test session listing
static void test_mux_list_sessions(void) {
  printf("Testing session listing...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  app_tui_mux_t *mux = NULL;
  err = app_tui_mux_create(&mux, "test");
  assert(err == APP_SUCCESS);

  // Create sessions
  const char *names[] = {"Editor", "Shell", "Logs"};
  int session_ids[3];

  for (int i = 0; i < 3; i++) {
    err = app_tui_mux_create_session(mux, names[i], "/bin/sh", &session_ids[i]);
    assert(err == APP_SUCCESS);
  }

  // List sessions
  app_tui_session_info_t *infos = NULL;
  size_t count = 0;
  err = app_tui_mux_list_sessions(mux, &infos, &count);
  assert(err == APP_SUCCESS);
  assert(count == 3);
  assert(infos != NULL);

  // Verify session info
  for (size_t i = 0; i < count; i++) {
    bool found = false;
    for (int j = 0; j < 3; j++) {
      if (infos[i].id == session_ids[j]) {
        found = true;
        assert(strcmp(infos[i].name, names[j]) == 0);
        break;
      }
    }
    assert(found);
  }

  // Clean up
  app_free(infos);

  app_tui_mux_destroy(mux);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Session listing works correctly\n");
}

// Test layout integration
static void test_mux_layout_integration(void) {
  printf("Testing layout integration...\n");

  app_mock_terminal_t *terminal = NULL;
  app_error err = app_mock_ncurses_init(&terminal, 80, 24);
  assert(err == APP_SUCCESS);

  app_tui_mux_t *mux = NULL;
  err = app_tui_mux_create(&mux, "test");
  assert(err == APP_SUCCESS);

  // Create sessions which should create panes
  int session_ids[4];
  for (int i = 0; i < 4; i++) {
    char name[32];
    snprintf(name, sizeof(name), "Session %d", i);
    err = app_tui_mux_create_session(mux, name, "/bin/sh", &session_ids[i]);
    assert(err == APP_SUCCESS);
  }

  // Verify layout has panes
  assert(mux->layout->pane_count == 4);

  // Test focus navigation
  err = app_tui_mux_focus_next(mux, APP_TUI_FOCUS_RIGHT);
  assert(err == APP_SUCCESS);

  err = app_tui_mux_focus_next(mux, APP_TUI_FOCUS_DOWN);
  assert(err == APP_SUCCESS);

  app_tui_mux_destroy(mux);
  app_mock_ncurses_cleanup(terminal);

  printf("  ✓ Layout integration works correctly\n");
}

int main(void) {
  printf("Running TUI multiplexer tests...\n\n");

  test_mux_lifecycle();
  test_mux_create_session();
  test_mux_close_session();
  test_mux_switch_session();
  test_mux_input_handling();
  test_mux_commands();
  test_mux_status_messages();
  test_mux_list_sessions();
  test_mux_layout_integration();

  printf("\nAll multiplexer tests passed! ✓\n");
  return 0;
}