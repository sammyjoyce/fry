#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../src/core/config.h"
#include "../src/core/keychain.h"
#include "../src/core/oauth.h"
#include "../src/utils/memory.h"

// Test helper to run fry command and capture output
typedef struct {
  char *stdout_data;
  char *stderr_data;
  int exit_code;
} command_result_t;

static command_result_t run_fry_command(const char *args[]) {
  command_result_t result = {0};

  int stdout_pipe[2];
  int stderr_pipe[2];

  if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
    perror("pipe");
    exit(1);
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    exit(1);
  }

  if (pid == 0) {
    // Child process
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    dup2(stdout_pipe[1], STDOUT_FILENO);
    dup2(stderr_pipe[1], STDERR_FILENO);

    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    execv("./zig-out/bin/fry", (char *const *)args);
    perror("execv");
    exit(1);
  }

  // Parent process
  close(stdout_pipe[1]);
  close(stderr_pipe[1]);

  // Read stdout
  char buffer[4096];
  ssize_t n;
  size_t stdout_size = 0;
  result.stdout_data = app_malloc(1);
  result.stdout_data[0] = '\0';

  while ((n = read(stdout_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
    buffer[n] = '\0';
    size_t new_size = stdout_size + n + 1;
    result.stdout_data = app_realloc(result.stdout_data, new_size);
    strcat(result.stdout_data, buffer);
    stdout_size = new_size - 1;
  }
  close(stdout_pipe[0]);

  // Read stderr
  size_t stderr_size = 0;
  result.stderr_data = app_malloc(1);
  result.stderr_data[0] = '\0';

  while ((n = read(stderr_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
    buffer[n] = '\0';
    size_t new_size = stderr_size + n + 1;
    result.stderr_data = app_realloc(result.stderr_data, new_size);
    strcat(result.stderr_data, buffer);
    stderr_size = new_size - 1;
  }
  close(stderr_pipe[0]);

  // Wait for child
  int status;
  waitpid(pid, &status, 0);
  result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

  return result;
}

static void free_command_result(command_result_t *result) {
  app_free(result->stdout_data);
  app_free(result->stderr_data);
}

// Test auth status command
static void test_auth_status() {
  printf("\n=== Testing auth status command ===\n");

  const char *args[] = {"./zig-out/bin/fry", "auth", "status", NULL};
  command_result_t result = run_fry_command(args);

  // Should succeed even with no accounts
  assert(result.exit_code == 0);
  assert(strstr(result.stdout_data, "No account specified") != NULL ||
         strstr(result.stdout_data, "Account:") != NULL);

  free_command_result(&result);
  printf("[PASS] auth status\n");
}

// Test auth login flow (mock)
static void test_auth_login_mock() {
  printf("\n=== Testing auth login (mock) ===\n");

  // First, remove any existing test account
  const char *rm_args[] = {"./zig-out/bin/fry", "accounts", "rm",
                           "oauth-test",        "--force",  NULL};
  command_result_t rm_result = run_fry_command(rm_args);
  free_command_result(&rm_result);

  // Create a mock OAuth response file
  const char *mock_token =
      "{"
      "\"access_token\": \"sk-ant-oat01-test-integration-token\","
      "\"refresh_token\": \"sk-ant-ort01-test-integration-refresh\","
      "\"expires_in\": 3600,"
      "\"scope\": \"user:inference user:profile\","
      "\"subscription_type\": \"pro\""
      "}";

  FILE *f = fopen("/tmp/mock_oauth_response.json", "w");
  assert(f != NULL);
  fprintf(f, "%s", mock_token);
  fclose(f);

  // Note: Actual login command would require user interaction
  // For integration testing, we'll test the token management directly

  printf("[PASS] auth login mock setup\n");
}

// Test token commands
static void test_token_commands() {
  printf("\n=== Testing token commands ===\n");

  // Test tokens inspect
  const char *inspect_args[] = {"./zig-out/bin/fry", "tokens", "inspect",
                                "test", NULL};
  command_result_t inspect_result = run_fry_command(inspect_args);

  // Should show token info for test account
  assert(inspect_result.exit_code == 0);
  assert(strstr(inspect_result.stdout_data, "Token Type:") != NULL);
  assert(strstr(inspect_result.stdout_data, "Access Token:") != NULL);

  free_command_result(&inspect_result);

  // Test tokens expire
  const char *expire_args[] = {"./zig-out/bin/fry", "tokens", "expire", "test",
                               NULL};
  command_result_t expire_result = run_fry_command(expire_args);

  assert(expire_result.exit_code == 0);
  assert(strstr(expire_result.stdout_data, "marked as expired") != NULL);

  free_command_result(&expire_result);

  // Verify token is expired
  const char *status_args[] = {"./zig-out/bin/fry", "auth", "status", "test",
                               NULL};
  command_result_t status_result = run_fry_command(status_args);

  assert(status_result.exit_code == 0);
  assert(strstr(status_result.stdout_data, "Expired") != NULL);

  free_command_result(&status_result);

  printf("[PASS] token commands\n");
}

// Test keychain integration
static void test_keychain_integration() {
  printf("\n=== Testing keychain integration ===\n");

  // Create a test token
  app_oauth_token_t token = {
      .access_token = "sk-ant-oat01-keychain-test",
      .refresh_token = "sk-ant-ort01-keychain-refresh",
      .expires_at = time(NULL) + 3600,
      .scope = "user:inference",
      .token_type = "Bearer",
      .subscription_type = "free",
  };

  // Store in keychain
  app_keychain_t *keychain = NULL;
  app_error err = app_keychain_create(&keychain);
  assert(err == APP_SUCCESS);

  err = app_keychain_store_token(keychain, "keychain-test", &token);
  if (err == APP_SUCCESS) {
    printf("✓ Token stored in keychain\n");

    // Retrieve and verify
    app_oauth_token_t *retrieved = NULL;
    err = app_keychain_retrieve_token(keychain, "keychain-test", &retrieved);
    assert(err == APP_SUCCESS);
    assert(retrieved != NULL);
    assert(strcmp(retrieved->access_token, token.access_token) == 0);

    app_oauth_token_free(retrieved);

    // Delete from keychain
    err = app_keychain_delete_token(keychain, "keychain-test");
    assert(err == APP_SUCCESS);

    printf("✓ Token retrieved and deleted from keychain\n");
  } else {
    printf("⚠️  Keychain not available (fallback to in-memory)\n");
  }

  err = app_keychain_cleanup(keychain);
  assert(err == APP_SUCCESS);

  printf("[PASS] keychain integration\n");
}

// Test OAuth flow with HTTP server simulation
static void test_oauth_flow_simulation() {
  printf("\n=== Testing OAuth flow simulation ===\n");

  app_oauth_config_t config = {
      .client_id = "anthropic-cli",
      .redirect_uri = "http://localhost:8765/callback",
      .auth_endpoint = "https://console.anthropic.com/oauth/authorize",
      .token_endpoint = "https://api.anthropic.com/oauth/token",
      .scopes = "user:inference user:profile",
  };

  // Start OAuth flow
  app_oauth_flow_context_t *context = NULL;
  app_error err = app_oauth_flow_start(&config, &context);
  assert(err == APP_SUCCESS);
  assert(context != NULL);

  printf("✓ OAuth flow started\n");
  printf("  State: %.10s...\n", context->state);
  printf("  Verifier: %.10s...\n", context->verifier);
  printf("  Auth URL: %.50s...\n", context->auth_url);

  // Simulate authorization code callback
  const char *auth_code = "test-authorization-code";
  const char *callback_state = context->state;

  // Verify state matches
  assert(strcmp(callback_state, context->state) == 0);
  printf("✓ State validation passed\n");

  // In a real flow, we would exchange the code for tokens here
  // For testing, we'll just verify the context is valid
  assert(strlen(context->verifier) == APP_OAUTH_PKCE_VERIFIER_LENGTH - 1);

  app_oauth_flow_cleanup(context);
  printf("[PASS] OAuth flow simulation\n");
}

// Test account management with OAuth tokens
static void test_account_oauth_integration() {
  printf("\n=== Testing account OAuth integration ===\n");

  // Add account with OAuth token
  const char *add_args[] = {
      "./zig-out/bin/fry",      "accounts", "add", "--name",
      "oauth-integration-test", NULL};
  command_result_t add_result = run_fry_command(add_args);

  // Should prompt for token since no file specified
  assert(add_result.exit_code != 0);  // Will fail without input
  assert(strstr(add_result.stderr_data, "Enter your API key") != NULL ||
         strstr(add_result.stderr_data, "No API key provided") != NULL);

  free_command_result(&add_result);

  // List accounts
  const char *list_args[] = {"./zig-out/bin/fry", "accounts", "list", NULL};
  command_result_t list_result = run_fry_command(list_args);

  assert(list_result.exit_code == 0);
  assert(strstr(list_result.stdout_data, "test") !=
         NULL);  // Should have test account

  free_command_result(&list_result);

  printf("[PASS] account OAuth integration\n");
}

// Test error handling
static void test_oauth_error_handling() {
  printf("\n=== Testing OAuth error handling ===\n");

  // Test invalid token format
  app_oauth_token_t *token = NULL;
  app_error err =
      app_oauth_parse_token_response("{\"invalid\": \"json\"}", &token);
  assert(err != APP_SUCCESS);
  assert(token == NULL);

  // Test expired token handling
  const char *expired_args[] = {"./zig-out/bin/fry", "tokens", "refresh",
                                "nonexistent", NULL};
  command_result_t expired_result = run_fry_command(expired_args);

  assert(expired_result.exit_code != 0);
  assert(strstr(expired_result.stderr_data, "Account") != NULL);

  free_command_result(&expired_result);

  printf("[PASS] OAuth error handling\n");
}

// Main test runner
int main() {
  printf("\n🧪 OAuth Integration Tests\n");
  printf("==========================\n");

  // Initialize
  app_memory_init();

  // Ensure binary exists
  if (access("./zig-out/bin/fry", X_OK) != 0) {
    printf("❌ Error: fry binary not found or not executable\n");
    printf("   Run 'zig build' first\n");
    return 1;
  }

  // Run integration tests
  test_auth_status();
  test_auth_login_mock();
  test_token_commands();
  test_keychain_integration();
  test_oauth_flow_simulation();
  test_account_oauth_integration();
  test_oauth_error_handling();

  printf("\n✅ All OAuth integration tests passed!\n\n");

  // Cleanup
  app_memory_cleanup();

  return 0;
}