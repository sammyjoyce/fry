/*
 * Test for PTY process spawning (Task 8)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../src/tui/tui_pty.h"
#include "../src/utils/logging.h"
#include "../src/utils/memory.h"

// Test basic PTY creation and destruction
static void test_pty_lifecycle(void) {
  printf("Testing PTY lifecycle...\n");

  app_pty_t *pty = NULL;
  app_error err = app_pty_create(&pty);

  if (err == APP_SUCCESS && pty) {
    printf("  ✓ PTY created successfully\n");

    // Configure and open PTY
    app_pty_config_t config = {.rows = 24,
                               .cols = 80,
                               .term_type = "xterm",
                               .working_dir = "/tmp",
                               .env_vars = NULL};

    err = app_pty_open(pty, &config);
    if (err == APP_SUCCESS) {
      printf("  ✓ PTY opened successfully (master_fd=%d)\n", pty->master_fd);

      // Close PTY
      err = app_pty_close(pty);
      if (err == APP_SUCCESS) {
        printf("  ✓ PTY closed successfully\n");
      } else {
        printf("  ✗ Failed to close PTY\n");
      }
    } else {
      printf("  ✗ Failed to open PTY\n");
    }

    // Destroy PTY
    err = app_pty_destroy(pty);
    if (err == APP_SUCCESS) {
      printf("  ✓ PTY destroyed successfully\n");
    } else {
      printf("  ✗ Failed to destroy PTY\n");
    }
  } else {
    printf("  ✗ Failed to create PTY\n");
  }
}

// Test spawning a simple process
static void test_process_spawn(void) {
  printf("\nTesting process spawning...\n");

  app_pty_t *pty = NULL;
  app_error err = app_pty_create(&pty);
  if (err != APP_SUCCESS) {
    printf("  ✗ Failed to create PTY\n");
    return;
  }

  // Configure PTY
  app_pty_config_t pty_config = {.rows = 24,
                                 .cols = 80,
                                 .term_type = "xterm",
                                 .working_dir = "/tmp",
                                 .env_vars = NULL};

  err = app_pty_open(pty, &pty_config);
  if (err != APP_SUCCESS) {
    printf("  ✗ Failed to open PTY\n");
    app_pty_destroy(pty);
    return;
  }

  // Spawn a simple command
  app_process_config_t proc_config = {
      .command = "/bin/echo",
      .args = (char *[]){"Hello from PTY!", NULL},
      .working_dir = "/tmp",
      .env_vars = NULL};

  err = app_pty_spawn(pty, &proc_config);
  if (err == APP_SUCCESS) {
    printf("  ✓ Process spawned successfully (PID=%d)\n", pty->child_pid);

    // Read output
    char buffer[256];
    size_t bytes_read = 0;

    // Give process time to run
    usleep(100000);  // 100ms

    err = app_pty_read(pty, buffer, sizeof(buffer) - 1, &bytes_read, 1000);
    if (err == APP_SUCCESS && bytes_read > 0) {
      buffer[bytes_read] = '\0';
      printf("  ✓ Read output: %s", buffer);
    } else {
      printf("  ✗ Failed to read output (err=%d)\n", err);
    }

    // Wait for process to exit
    int exit_status;
    err = app_pty_wait(pty, &exit_status, false);
    if (err == APP_SUCCESS) {
      printf("  ✓ Process exited with status %d\n", exit_status);
    } else {
      printf("  ✗ Failed to wait for process\n");
    }
  } else {
    printf("  ✗ Failed to spawn process\n");
  }

  app_pty_close(pty);
  app_pty_destroy(pty);
}

// Test process termination
static void test_process_termination(void) {
  printf("\nTesting process termination...\n");

  app_pty_t *pty = NULL;
  app_error err = app_pty_create(&pty);
  if (err != APP_SUCCESS) {
    printf("  ✗ Failed to create PTY\n");
    return;
  }

  // Configure PTY
  app_pty_config_t pty_config = {.rows = 24,
                                 .cols = 80,
                                 .term_type = "xterm",
                                 .working_dir = "/tmp",
                                 .env_vars = NULL};

  err = app_pty_open(pty, &pty_config);
  if (err != APP_SUCCESS) {
    printf("  ✗ Failed to open PTY\n");
    app_pty_destroy(pty);
    return;
  }

  // Spawn a long-running process
  app_process_config_t proc_config = {.command = "/bin/sleep",
                                      .args = (char *[]){"10", NULL},
                                      .working_dir = "/tmp",
                                      .env_vars = NULL};

  err = app_pty_spawn(pty, &proc_config);
  if (err == APP_SUCCESS) {
    printf("  ✓ Long-running process spawned (PID=%d)\n", pty->child_pid);

    // Check if alive
    bool alive = false;
    err = app_pty_is_alive(pty, &alive);
    if (err == APP_SUCCESS && alive) {
      printf("  ✓ Process is alive\n");
    } else {
      printf("  ✗ Process check failed\n");
    }

    // Kill process
    err = app_pty_kill(pty, SIGTERM);
    if (err == APP_SUCCESS) {
      printf("  ✓ Sent SIGTERM to process\n");

      // Wait for exit
      int exit_status;
      err = app_pty_wait(pty, &exit_status, false);
      if (err == APP_SUCCESS) {
        printf("  ✓ Process terminated (status=%d)\n", exit_status);
      } else {
        printf("  ✗ Failed to wait for termination\n");
      }
    } else {
      printf("  ✗ Failed to kill process\n");
    }
  } else {
    printf("  ✗ Failed to spawn long-running process\n");
  }

  app_pty_close(pty);
  app_pty_destroy(pty);
}

// Test environment variable passing
static void test_environment_variables(void) {
  printf("\nTesting environment variables...\n");

  app_pty_t *pty = NULL;
  app_error err = app_pty_create(&pty);
  if (err != APP_SUCCESS) {
    printf("  ✗ Failed to create PTY\n");
    return;
  }

  // Configure PTY
  app_pty_config_t pty_config = {.rows = 24,
                                 .cols = 80,
                                 .term_type = "xterm",
                                 .working_dir = "/tmp",
                                 .env_vars = NULL};

  err = app_pty_open(pty, &pty_config);
  if (err != APP_SUCCESS) {
    printf("  ✗ Failed to open PTY\n");
    app_pty_destroy(pty);
    return;
  }

  // Spawn process with custom environment
  char *env_vars[] = {"TEST_VAR=Hello", "ANOTHER_VAR=World", NULL};

  app_process_config_t proc_config = {
      .command = "/bin/sh",
      .args = (char *[]){"-c", "echo $TEST_VAR $ANOTHER_VAR", NULL},
      .working_dir = "/tmp",
      .env_vars = env_vars};

  err = app_pty_spawn(pty, &proc_config);
  if (err == APP_SUCCESS) {
    printf("  ✓ Process spawned with environment\n");

    // Read output
    char buffer[256];
    size_t bytes_read = 0;

    usleep(100000);  // 100ms

    err = app_pty_read(pty, buffer, sizeof(buffer) - 1, &bytes_read, 1000);
    if (err == APP_SUCCESS && bytes_read > 0) {
      buffer[bytes_read] = '\0';
      printf("  ✓ Environment test output: %s", buffer);

      // Check if output contains our variables
      if (strstr(buffer, "Hello World")) {
        printf("  ✓ Environment variables passed correctly\n");
      } else {
        printf("  ✗ Environment variables not found in output\n");
      }
    } else {
      printf("  ✗ Failed to read output\n");
    }

    // Wait for exit
    int exit_status;
    app_pty_wait(pty, &exit_status, false);
  } else {
    printf("  ✗ Failed to spawn process with environment\n");
  }

  app_pty_close(pty);
  app_pty_destroy(pty);
}

int main(void) {
  printf("Running PTY process spawning tests...\n\n");

  // Initialize logging
  app_log_init();

  // Run tests
  test_pty_lifecycle();
  test_process_spawn();
  test_process_termination();
  test_environment_variables();

  printf("\nPTY spawning tests completed!\n");

  return 0;
}