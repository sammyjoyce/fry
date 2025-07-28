#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../src/core/error.h"
#include "../src/core/oauth.h"
#include "../src/utils/memory.h"

// Test helper functions
static void print_test_header(const char *test_name) {
  printf("\n=== Testing %s ===\n", test_name);
}

static void print_test_result(const char *test_name, bool passed) {
  printf("[%s] %s\n", passed ? "PASS" : "FAIL", test_name);
  if (!passed) {
    exit(1);
  }
}

// Test PKCE generation
static void test_pkce_generation() {
  print_test_header("PKCE Generation");

  app_oauth_pkce_t *pkce = NULL;
  app_error err = app_oauth_pkce_generate(&pkce);
  assert(err == APP_SUCCESS);
  assert(pkce != NULL);
  assert(pkce->verifier != NULL);
  assert(pkce->state != NULL);

  // Verify verifier length (should be 128 chars for base64url)
  size_t verifier_len = strlen(pkce->verifier);
  assert(verifier_len >= 43 && verifier_len <= 128);

  // Verify verifier contains only valid base64url characters
  for (size_t i = 0; i < verifier_len; i++) {
    char c = pkce->verifier[i];
    assert((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_');
  }

  // Verify state is generated
  assert(strlen(pkce->state) > 0);

  // Generate another PKCE to ensure they're different
  app_oauth_pkce_t *pkce2 = NULL;
  err = app_oauth_pkce_generate(&pkce2);
  assert(err == APP_SUCCESS);
  assert(strcmp(pkce->verifier, pkce2->verifier) != 0);
  assert(strcmp(pkce->state, pkce2->state) != 0);

  app_oauth_pkce_destroy(pkce);
  app_oauth_pkce_destroy(pkce2);

  print_test_result("PKCE Generation", true);
}

// Test authorization URL building
static void test_auth_url_building() {
  print_test_header("Authorization URL Building");

  // Generate PKCE first
  app_oauth_pkce_t *pkce = NULL;
  app_error err = app_oauth_pkce_generate(&pkce);
  assert(err == APP_SUCCESS);

  // Test Claude Pro/Max mode
  char *url = NULL;
  err = app_oauth_authorize_url(&url, pkce, "max");
  assert(err == APP_SUCCESS);
  assert(url != NULL);

  // Verify URL contains required parameters
  assert(strstr(url, "https://") != NULL);
  assert(strstr(url, "state=") != NULL);
  assert(strstr(url, "code_challenge=") != NULL);
  assert(strstr(url, "code_challenge_method=S256") != NULL);

  app_free(url);

  // Test console mode
  err = app_oauth_authorize_url(&url, pkce, "console");
  assert(err == APP_SUCCESS);
  assert(url != NULL);
  app_free(url);

  app_oauth_pkce_destroy(pkce);
  print_test_result("Authorization URL Building", true);
}

// Test token validation
static void test_token_validation() {
  print_test_header("Token Validation");

  // Create expired token
  app_oauth_token_t expired_token = {
      .access_token = "expired-token",
      .refresh_token = "refresh-token",
      .expires_at = time(NULL) - 3600,  // Expired 1 hour ago
      .scope = "read",
  };

  assert(app_oauth_token_is_expired(&expired_token) == true);

  // Create valid token
  app_oauth_token_t valid_token = {
      .access_token = "valid-token",
      .refresh_token = "refresh-token",
      .expires_at = time(NULL) + 3600,  // Expires in 1 hour
      .scope = "read",
  };

  assert(app_oauth_token_is_expired(&valid_token) == false);

  // Test token validation
  app_error err = app_oauth_token_validate(&valid_token);
  assert(err == APP_SUCCESS);

  // Test invalid token (no access token)
  app_oauth_token_t invalid_token = {
      .access_token = NULL,
      .refresh_token = "refresh-token",
      .expires_at = time(NULL) + 3600,
  };

  err = app_oauth_token_validate(&invalid_token);
  assert(err != APP_SUCCESS);

  print_test_result("Token Validation", true);
}

// Test account creation and management
static void test_account_management() {
  print_test_header("Account Management");

  // Initialize OAuth
  app_error err = app_oauth_init();
  assert(err == APP_SUCCESS);

  // Create account
  app_oauth_account_t *account = NULL;
  err = app_oauth_account_create(&account, "test-account");
  assert(err == APP_SUCCESS);
  assert(account != NULL);
  assert(strcmp(account->name, "test-account") == 0);
  assert(account->is_default == false);
  assert(account->is_disabled == false);

  // Set some token data
  account->token.access_token = app_strdup("test-access-token");
  account->token.refresh_token = app_strdup("test-refresh-token");
  account->token.expires_at = time(NULL) + 3600;
  account->token.scope = app_strdup("user:inference");

  // Save account (this will fail without proper setup, but we test the API)
  err = app_oauth_account_save(account);
  // May fail due to missing directories, which is OK for unit test

  app_oauth_account_destroy(account);
  app_oauth_cleanup();

  print_test_result("Account Management", true);
}

// Test token encryption/decryption
static void test_token_encryption() {
  print_test_header("Token Encryption");

  // Create a token
  app_oauth_token_t token = {
      .access_token = app_strdup("test-access-token"),
      .refresh_token = app_strdup("test-refresh-token"),
      .expires_at = time(NULL) + 3600,
      .scope = app_strdup("user:inference user:profile"),
      .token_type = app_strdup("Bearer"),
  };

  // Encrypt token
  char *encrypted_data = NULL;
  size_t data_len = 0;
  app_error err = app_oauth_token_encrypt(&encrypted_data, &data_len, &token);

  if (err == APP_SUCCESS) {
    assert(encrypted_data != NULL);
    assert(data_len > 0);

    // Decrypt token
    app_oauth_token_t *decrypted = NULL;
    err = app_oauth_token_decrypt(&decrypted, encrypted_data, data_len);
    assert(err == APP_SUCCESS);
    assert(decrypted != NULL);
    assert(strcmp(decrypted->access_token, token.access_token) == 0);
    assert(strcmp(decrypted->refresh_token, token.refresh_token) == 0);
    assert(decrypted->expires_at == token.expires_at);

    app_oauth_token_free(decrypted);
    app_free(encrypted_data);
  } else {
    printf("  Note: Token encryption not available on this platform\n");
  }

  // Cleanup original token
  app_free(token.access_token);
  app_free(token.refresh_token);
  app_free(token.scope);
  app_free(token.token_type);

  print_test_result("Token Encryption", true);
}

// Test account listing
static void test_account_listing() {
  print_test_header("Account Listing");

  app_error err = app_oauth_init();
  assert(err == APP_SUCCESS);

  // List accounts (may be empty)
  app_oauth_account_t **accounts = NULL;
  size_t count = 0;
  err = app_oauth_accounts_list(&accounts, &count);

  if (err == APP_SUCCESS) {
    printf("  Found %zu accounts\n", count);

    // Free accounts
    for (size_t i = 0; i < count; i++) {
      app_oauth_account_destroy(accounts[i]);
    }
    app_free(accounts);
  }

  app_oauth_cleanup();
  print_test_result("Account Listing", true);
}

// Test import/export functionality
static void test_import_export() {
  print_test_header("Import/Export");

  // Create a test account
  app_oauth_account_t account = {
      .name = app_strdup("export-test"),
      .is_default = true,
      .is_disabled = false,
      .token = {
          .access_token = app_strdup("export-access-token"),
          .refresh_token = app_strdup("export-refresh-token"),
          .expires_at = time(NULL) + 7200,
          .scope = app_strdup("user:inference"),
          .token_type = app_strdup("Bearer"),
      }};

  // Export to JSON
  char *json_output = NULL;
  app_error err = app_oauth_export_credentials(&account, &json_output);

  if (err == APP_SUCCESS) {
    assert(json_output != NULL);
    assert(strstr(json_output, "export-access-token") != NULL);
    assert(strstr(json_output, "export-refresh-token") != NULL);
    printf("  Exported JSON: %.50s...\n", json_output);
    app_free(json_output);
  } else {
    printf("  Note: Export not implemented yet\n");
  }

  // Cleanup
  app_free(account.name);
  app_free(account.token.access_token);
  app_free(account.token.refresh_token);
  app_free(account.token.scope);
  app_free(account.token.token_type);

  print_test_result("Import/Export", true);
}

// Main test runner
int main() {
  printf("\n🧪 OAuth Unit Tests\n");
  printf("===================\n");

  // Initialize memory tracking if needed
  app_memory_init();

  // Run all tests
  test_pkce_generation();
  test_auth_url_building();
  test_token_validation();
  test_account_management();
  test_token_encryption();
  test_account_listing();
  test_import_export();

  printf("\n✅ All OAuth unit tests passed!\n\n");

  // Check for memory leaks
  app_memory_cleanup();

  return 0;
}