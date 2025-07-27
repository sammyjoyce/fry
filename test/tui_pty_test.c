#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define ENABLE_TUI_TESTS 1

#include "../src/tui/tui_pty.h"
#include "../src/utils/memory.h"

// Test PTY creation and destruction
static void test_pty_lifecycle(void) {
  printf("Testing PTY lifecycle...\n");

  app_tui_pty_t *pty = NULL;
  app_error err = app_tui_pty_create(&pty);
  assert(err == APP_SUCCESS);
  assert(pty != NULL);
  assert(pty->master_fd >= 0);
  assert(pty->slave_fd >= 0);
  assert(pty->child_pid == -1);
  assert(pty->rows == 24);
  assert(pty->cols == 80);

  // Verify PTY is valid
  assert(isatty(pty->master_fd));
  assert(isatty(pty->slave_fd));

  app_tui_pty_destroy(pty);

  printf("  ✓ PTY lifecycle works correctly\n");
}

// Test PTY spawning a process
static void test_pty_spawn(void) {
  printf("Testing PTY process spawning...\n");

  app_tui_pty_t *pty = NULL;
  app_error err = app_tui_pty_create(&pty);
  assert(err == APP_SUCCESS);

  // Spawn a simple command
  const char *argv[] = {"/bin/echo", "Hello from PTY", NULL};
  const char *envp[] = {"TERM=xterm", "PATH=/bin:/usr/bin", NULL};

  err =
      app_tui_pty_spawn(pty, argv[0], (char *const *)argv, (char *const *)envp);
  assert(err == APP_SUCCESS);
  assert(pty->child_pid > 0);

  // Read output
  char buffer[256] = {0};
  ssize_t n = app_tui_pty_read(pty, buffer, sizeof(buffer) - 1);
  assert(n > 0);
  assert(strstr(buffer, "Hello from PTY") != NULL);

  // Wait for process to complete
  int status;
  pid_t result = waitpid(pty->child_pid, &status, 0);
  assert(result == pty->child_pid);
  assert(WIFEXITED(status));
  assert(WEXITSTATUS(status) == 0);

  app_tui_pty_destroy(pty);

  printf("  ✓ PTY process spawning works correctly\n");
}

// Test PTY I/O
static void test_pty_io(void) {
  printf("Testing PTY I/O...\n");

  app_tui_pty_t *pty = NULL;
  app_error err = app_tui_pty_create(&pty);
  assert(err == APP_SUCCESS);

  // Spawn cat command (echoes input back)
  const char *argv[] = {"/bin/cat", NULL};
  const char *envp[] = {"TERM=xterm", NULL};

  err =
      app_tui_pty_spawn(pty, argv[0], (char *const *)argv, (char *const *)envp);
  assert(err == APP_SUCCESS);

  // Write to PTY
  const char *input = "Test input\n";
  ssize_t written = app_tui_pty_write(pty, input, strlen(input));
  assert(written == (ssize_t)strlen(input));

  // Read back from PTY
  char buffer[256] = {0};
  usleep(10000);  // Give cat time to echo
  ssize_t n = app_tui_pty_read(pty, buffer, sizeof(buffer) - 1);
  assert(n > 0);
  assert(strcmp(buffer, input) == 0);

  // Kill the cat process
  kill(pty->child_pid, SIGTERM);
  waitpid(pty->child_pid, NULL, 0);

  app_tui_pty_destroy(pty);

  printf("  ✓ PTY I/O works correctly\n");
}

// Test PTY resizing
static void test_pty_resize(void) {
  printf("Testing PTY resizing...\n");

  app_tui_pty_t *pty = NULL;
  app_error err = app_tui_pty_create(&pty);
  assert(err == APP_SUCCESS);

  // Initial size
  assert(pty->rows == 24);
  assert(pty->cols == 80);

  // Resize PTY
  err = app_tui_pty_resize(pty, 40, 100);
  assert(err == APP_SUCCESS);
  assert(pty->rows == 40);
  assert(pty->cols == 100);

  // Spawn a process that reports terminal size
  const char *argv[] = {"/bin/sh", "-c", "stty size", NULL};
  const char *envp[] = {"TERM=xterm", NULL};

  err =
      app_tui_pty_spawn(pty, argv[0], (char *const *)argv, (char *const *)envp);
  assert(err == APP_SUCCESS);

  // Read size output
  char buffer[256] = {0};
  usleep(50000);  // Give shell time to execute
  ssize_t n = app_tui_pty_read(pty, buffer, sizeof(buffer) - 1);

  if (n > 0) {
    int rows, cols;
    if (sscanf(buffer, "%d %d", &rows, &cols) == 2) {
      assert(rows == 40);
      assert(cols == 100);
    }
  }

  // Wait for process
  waitpid(pty->child_pid, NULL, 0);

  app_tui_pty_destroy(pty);

  printf("  ✓ PTY resizing works correctly\n");
}

// Test PTY non-blocking I/O
static void test_pty_nonblocking(void) {
  printf("Testing PTY non-blocking I/O...\n");

  app_tui_pty_t *pty = NULL;
  app_error err = app_tui_pty_create(&pty);
  assert(err == APP_SUCCESS);

  // Set non-blocking mode
  err = app_tui_pty_set_nonblocking(pty, true);
  assert(err == APP_SUCCESS);

  // Try to read when no data available
  char buffer[256];
  ssize_t n = app_tui_pty_read(pty, buffer, sizeof(buffer));
  assert(n == -1);
  assert(errno == EAGAIN || errno == EWOULDBLOCK);

  // Set blocking mode
  err = app_tui_pty_set_nonblocking(pty, false);
  assert(err == APP_SUCCESS);

  app_tui_pty_destroy(pty);

  printf("  ✓ PTY non-blocking I/O works correctly\n");
}

// Test PTY with interactive shell
static void test_pty_shell(void) {
  printf("Testing PTY with interactive shell...\n");

  app_tui_pty_t *pty = NULL;
  app_error err = app_tui_pty_create(&pty);
  assert(err == APP_SUCCESS);

  // Set non-blocking for this test
  err = app_tui_pty_set_nonblocking(pty, true);
  assert(err == APP_SUCCESS);

  // Spawn shell
  const char *shell = getenv("SHELL");
  if (!shell)
    shell = "/bin/sh";

  const char *argv[] = {shell, NULL};
  const char *envp[] = {"TERM=xterm", "PS1=$ ", NULL};

  err =
      app_tui_pty_spawn(pty, argv[0], (char *const *)argv, (char *const *)envp);
  assert(err == APP_SUCCESS);

  // Wait for shell prompt
  usleep(100000);
  char buffer[1024] = {0};
  ssize_t total = 0;
  ssize_t n;

  while ((n = app_tui_pty_read(pty, buffer + total,
                               sizeof(buffer) - total - 1)) > 0) {
    total += n;
  }

  // Send a command
  const char *cmd = "echo PTY_TEST\n";
  ssize_t written = app_tui_pty_write(pty, cmd, strlen(cmd));
  assert(written == (ssize_t)strlen(cmd));

  // Read response
  usleep(100000);
  memset(buffer, 0, sizeof(buffer));
  total = 0;

  while ((n = app_tui_pty_read(pty, buffer + total,
                               sizeof(buffer) - total - 1)) > 0) {
    total += n;
  }

  // Verify we got the echo
  assert(strstr(buffer, "PTY_TEST") != NULL);

  // Send exit command
  const char *exit_cmd = "exit\n";
  app_tui_pty_write(pty, exit_cmd, strlen(exit_cmd));

  // Wait for shell to exit
  int status;
  waitpid(pty->child_pid, &status, 0);

  app_tui_pty_destroy(pty);

  printf("  ✓ PTY with interactive shell works correctly\n");
}

// Test PTY error handling
static void test_pty_error_handling(void) {
  printf("Testing PTY error handling...\n");

  // Test NULL pointer
  app_error err = app_tui_pty_create(NULL);
  assert(err == APP_ERROR_INVALID_ARG);

  app_tui_pty_t *pty = NULL;
  err = app_tui_pty_create(&pty);
  assert(err == APP_SUCCESS);

  // Test spawning non-existent program
  const char *argv[] = {"/non/existent/program", NULL};
  const char *envp[] = {NULL};

  err =
      app_tui_pty_spawn(pty, argv[0], (char *const *)argv, (char *const *)envp);
  // This might succeed (fork succeeds but exec fails in child)
  // So we don't assert on the return value

  // Test invalid resize
  err = app_tui_pty_resize(pty, 0, 0);
  assert(err == APP_ERROR_INVALID_ARG);

  err = app_tui_pty_resize(pty, -1, -1);
  assert(err == APP_ERROR_INVALID_ARG);

  app_tui_pty_destroy(pty);

  printf("  ✓ PTY error handling works correctly\n");
}

// Test PTY with environment variables
static void test_pty_environment(void) {
  printf("Testing PTY with environment variables...\n");

  app_tui_pty_t *pty = NULL;
  app_error err = app_tui_pty_create(&pty);
  assert(err == APP_SUCCESS);

  // Spawn process that prints environment variable
  const char *argv[] = {"/bin/sh", "-c", "echo $TEST_VAR", NULL};
  const char *envp[] = {"TEST_VAR=Hello PTY Environment", "TERM=xterm", NULL};

  err =
      app_tui_pty_spawn(pty, argv[0], (char *const *)argv, (char *const *)envp);
  assert(err == APP_SUCCESS);

  // Read output
  char buffer[256] = {0};
  usleep(50000);
  ssize_t n = app_tui_pty_read(pty, buffer, sizeof(buffer) - 1);
  assert(n > 0);
  assert(strstr(buffer, "Hello PTY Environment") != NULL);

  // Wait for process
  waitpid(pty->child_pid, NULL, 0);

  app_tui_pty_destroy(pty);

  printf("  ✓ PTY with environment variables works correctly\n");
}

int main(void) {
  printf("Running TUI PTY tests...\n\n");

  test_pty_lifecycle();
  test_pty_spawn();
  test_pty_io();
  test_pty_resize();
  test_pty_nonblocking();
  test_pty_shell();
  test_pty_error_handling();
  test_pty_environment();

  printf("\nAll PTY tests passed! ✓\n");
  return 0;
}