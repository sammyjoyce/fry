#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// Test runner for TUI tests
// This allows running all TUI tests or individual test suites

typedef struct {
  const char *name;
  const char *executable;
  const char *description;
} test_suite_t;

static const test_suite_t test_suites[] = {
    {"ncurses", "./test_tui_ncurses", "NCurses abstraction tests"},
    {"pane", "./test_tui_pane", "Pane management tests"},
    {"layout", "./test_tui_layout", "Layout management tests"},
    {"pty", "./test_tui_pty", "PTY functionality tests"},
    {"mux", "./test_tui_mux", "Multiplexer tests"},
    {"render", "./test_tui_render", "Render engine tests"},
    {"integration", "./test_tui", "Integration tests"},
};

static const size_t num_test_suites =
    sizeof(test_suites) / sizeof(test_suites[0]);

static int run_test_suite(const test_suite_t *suite) {
  printf("\n🧪 Running %s...\n", suite->description);
  printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    return 1;
  }

  if (pid == 0) {
    // Child process
    execl(suite->executable, suite->executable, NULL);
    perror("execl");
    exit(1);
  }

  // Parent process
  int status;
  waitpid(pid, &status, 0);

  if (WIFEXITED(status)) {
    int exit_code = WEXITSTATUS(status);
    if (exit_code == 0) {
      printf("✅ %s passed\n", suite->description);
      return 0;
    } else {
      printf("❌ %s failed with exit code %d\n", suite->description, exit_code);
      return 1;
    }
  } else if (WIFSIGNALED(status)) {
    printf("❌ %s terminated by signal %d\n", suite->description,
           WTERMSIG(status));
    return 1;
  }

  return 1;
}

static void print_usage(const char *program) {
  printf("Usage: %s [test_name]\n", program);
  printf("\nAvailable test suites:\n");
  for (size_t i = 0; i < num_test_suites; i++) {
    printf("  %-12s - %s\n", test_suites[i].name, test_suites[i].description);
  }
  printf("\nIf no test name is specified, all tests will be run.\n");
}

int main(int argc, char **argv) {
  printf("\n🚀 FRY TUI Test Runner\n");
  printf("════════════════════════════════════════\n");

  if (argc > 1) {
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
      print_usage(argv[0]);
      return 0;
    }

    // Run specific test suite
    for (size_t i = 0; i < num_test_suites; i++) {
      if (strcmp(argv[1], test_suites[i].name) == 0) {
        return run_test_suite(&test_suites[i]);
      }
    }

    printf("❌ Unknown test suite: %s\n", argv[1]);
    print_usage(argv[0]);
    return 1;
  }

  // Run all test suites
  int failed = 0;
  int passed = 0;

  for (size_t i = 0; i < num_test_suites; i++) {
    if (run_test_suite(&test_suites[i]) == 0) {
      passed++;
    } else {
      failed++;
    }
  }

  printf("\n════════════════════════════════════════\n");
  printf("📊 Test Summary:\n");
  printf("   ✅ Passed: %d\n", passed);
  printf("   ❌ Failed: %d\n", failed);
  printf("   📋 Total:  %d\n", passed + failed);
  printf("════════════════════════════════════════\n");

  if (failed > 0) {
    printf("\n❌ Some tests failed!\n");
    return 1;
  } else {
    printf("\n✅ All tests passed!\n");
    return 0;
  }
}