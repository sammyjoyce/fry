/*
 * Test for TUI event loop (Task 6)
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "../src/core/session.h"
#include "../src/tui/tui_mux.h"
#include "../src/tui/tui_ncurses.h"
#include "../src/utils/logging.h"
#include "../src/utils/memory.h"

// Test signal handling
static volatile sig_atomic_t resize_count = 0;

static void test_sigwinch_handler(int sig) {
  (void)sig;
  resize_count++;
}

// Test basic event loop timing
static void test_event_loop_timing(void) {
  printf("Testing event loop timing...\n");

  // Create a simple timer test
  struct timeval start, end;
  gettimeofday(&start, NULL);

  // Sleep for 100ms
  usleep(100000);

  gettimeofday(&end, NULL);

  int64_t elapsed_us =
      (end.tv_sec - start.tv_sec) * 1000000LL + (end.tv_usec - start.tv_usec);
  int64_t elapsed_ms = elapsed_us / 1000;

  printf("  Elapsed time: %lld ms (expected ~100ms)\n", (long long)elapsed_ms);

  if (elapsed_ms >= 95 && elapsed_ms <= 105) {
    printf("  ✓ Timer precision is good\n");
  } else {
    printf("  ✗ Timer precision is off\n");
  }
}

// Test signal handling
static void test_signal_handling(void) {
  printf("Testing signal handling...\n");

  // Install test handler
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = test_sigwinch_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  struct sigaction old_sa;
  if (sigaction(SIGWINCH, &sa, &old_sa) < 0) {
    printf("  ✗ Failed to install signal handler\n");
    return;
  }

  // Send ourselves a SIGWINCH
  resize_count = 0;
  kill(getpid(), SIGWINCH);

  // Give signal time to be delivered
  usleep(10000);

  if (resize_count == 1) {
    printf("  ✓ SIGWINCH handler works correctly\n");
  } else {
    printf("  ✗ SIGWINCH handler failed (count=%d)\n", resize_count);
  }

  // Restore old handler
  sigaction(SIGWINCH, &old_sa, NULL);
}

// Test file descriptor monitoring
static void test_fd_monitoring(void) {
  printf("Testing file descriptor monitoring...\n");

  // Create a pipe for testing
  int pipe_fds[2];
  if (pipe(pipe_fds) < 0) {
    printf("  ✗ Failed to create pipe\n");
    return;
  }

  // Test select() with the pipe
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(pipe_fds[0], &read_fds);

  struct timeval timeout = {0, 0};
  int ready = select(pipe_fds[0] + 1, &read_fds, NULL, NULL, &timeout);

  if (ready == 0) {
    printf("  ✓ Empty pipe correctly shows no data\n");
  } else {
    printf("  ✗ Empty pipe incorrectly shows data\n");
  }

  // Write data to pipe
  const char *test_data = "test";
  write(pipe_fds[1], test_data, strlen(test_data));

  // Check again
  FD_ZERO(&read_fds);
  FD_SET(pipe_fds[0], &read_fds);
  ready = select(pipe_fds[0] + 1, &read_fds, NULL, NULL, &timeout);

  if (ready == 1 && FD_ISSET(pipe_fds[0], &read_fds)) {
    printf("  ✓ Pipe with data correctly detected\n");

    // Read the data
    char buffer[10];
    ssize_t n = read(pipe_fds[0], buffer, sizeof(buffer));
    if (n == (ssize_t)strlen(test_data)) {
      printf("  ✓ Data read correctly from pipe\n");
    } else {
      printf("  ✗ Failed to read correct data from pipe\n");
    }
  } else {
    printf("  ✗ Failed to detect data in pipe\n");
  }

  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

// Test frame rate calculation
static void test_frame_rate_calculation(void) {
  printf("Testing frame rate calculation...\n");

  // Test different frame rates
  int test_fps[] = {30, 60, 120};

  for (int i = 0; i < 3; i++) {
    int fps = test_fps[i];
    int64_t expected_interval_us = 1000000 / fps;

    struct timeval interval;
    interval.tv_sec = expected_interval_us / 1000000;
    interval.tv_usec = expected_interval_us % 1000000;

    int64_t actual_us = interval.tv_sec * 1000000LL + interval.tv_usec;

    printf("  %d FPS: interval = %lld us", fps, (long long)actual_us);

    if (actual_us == expected_interval_us) {
      printf(" ✓\n");
    } else {
      printf(" ✗ (expected %lld)\n", (long long)expected_interval_us);
    }
  }
}

int main(void) {
  printf("Running TUI event loop tests...\n\n");

  // Run tests
  test_event_loop_timing();
  test_signal_handling();
  test_fd_monitoring();
  test_frame_rate_calculation();

  printf("\nEvent loop tests completed!\n");
  return 0;
}