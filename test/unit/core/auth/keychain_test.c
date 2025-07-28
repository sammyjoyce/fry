#include "../src/core/keychain.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../src/core/oauth.h"
#include "../src/utils/memory.h"

static void test_mock_keychain_basic() {
  printf("Testing mock keychain basic operations...\n");

  app_keychain_t *keychain = NULL;
  app_error err = app_keychain_create_mock(&keychain);
  assert(err == APP_SUCCESS);
  assert(keychain != NULL);

  // Test capabilities
  app_keychain_capabilities_t caps;
  err = app_keychain_get_capabilities(keychain, &caps);
  assert(err == APP_SUCCESS);
  assert(caps.can_store_tokens == true);
  assert(caps.requires_unlock == false);
  assert(strcmp(caps.backend_name, "Mock Keychain (In-Memory)") == 0);

  // Create a test token
  app_oauth_token_t token = {.access_token = "test_access_token",
                             .refresh_token = "test_refresh_token",
                             .expires_at = time(NULL) + 3600,
                             .scope = "read write"};

  // Store token
  err = app_keychain_store_token(keychain, "test_account", &token);
  assert(err == APP_SUCCESS);

  // Check if token exists
  bool exists = false;
  err = app_keychain_has_token(keychain, "test_account", &exists);
  assert(err == APP_SUCCESS);
  assert(exists == true);

  // Retrieve token
  app_oauth_token_t *retrieved = NULL;
  err = app_keychain_retrieve_token(keychain, "test_account", &retrieved);
  assert(err == APP_SUCCESS);
  assert(retrieved != NULL);
  assert(strcmp(retrieved->access_token, token.access_token) == 0);
  assert(strcmp(retrieved->refresh_token, token.refresh_token) == 0);
  assert(retrieved->expires_at == token.expires_at);
  assert(strcmp(retrieved->scope, token.scope) == 0);

  // Free retrieved token
  app_oauth_token_free(retrieved);

  // Delete token
  err = app_keychain_delete_token(keychain, "test_account");
  assert(err == APP_SUCCESS);

  // Verify deletion
  exists = true;
  err = app_keychain_has_token(keychain, "test_account", &exists);
  assert(err == APP_SUCCESS);
  assert(exists == false);

  // Try to retrieve deleted token
  err = app_keychain_retrieve_token(keychain, "test_account", &retrieved);
  assert(err == APP_ERROR_KEYCHAIN_NOT_FOUND);

  // Cleanup
  err = app_keychain_cleanup(keychain);
  assert(err == APP_SUCCESS);

  printf("✓ Mock keychain basic operations passed\n");
}

static void test_mock_keychain_multiple_accounts() {
  printf("Testing mock keychain with multiple accounts...\n");

  app_keychain_t *keychain = NULL;
  app_error err = app_keychain_create_mock(&keychain);
  assert(err == APP_SUCCESS);

  // Store tokens for multiple accounts
  for (int i = 0; i < 5; i++) {
    char account_id[32];
    snprintf(account_id, sizeof(account_id), "account_%d", i);

    app_oauth_token_t token = {.access_token = "access_token",
                               .refresh_token = "refresh_token",
                               .expires_at = time(NULL) + 3600,
                               .scope = "read"};

    err = app_keychain_store_token(keychain, account_id, &token);
    assert(err == APP_SUCCESS);
  }

  // List all accounts
  char **account_ids = NULL;
  size_t count = 0;
  err = app_keychain_list_accounts(keychain, &account_ids, &count);
  assert(err == APP_SUCCESS);
  assert(count == 5);
  assert(account_ids != NULL);

  // Verify all accounts are present
  for (size_t i = 0; i < count; i++) {
    assert(account_ids[i] != NULL);
    printf("  Found account: %s\n", account_ids[i]);
  }

  // Free account list
  for (size_t i = 0; i < count; i++) {
    app_free(account_ids[i]);
  }
  app_free(account_ids);

  // Cleanup
  err = app_keychain_cleanup(keychain);
  assert(err == APP_SUCCESS);

  printf("✓ Mock keychain multiple accounts passed\n");
}

static void test_keychain_error_handling() {
  printf("Testing keychain error handling...\n");

  app_keychain_t *keychain = NULL;
  app_error err;

  // Test NULL parameters
  err = app_keychain_init(NULL);
  assert(err == APP_ERROR_INVALID_ARG);

  err = app_keychain_create_mock(&keychain);
  assert(err == APP_SUCCESS);

  err = app_keychain_store_token(NULL, "account", NULL);
  assert(err == APP_ERROR_INVALID_ARG);

  err = app_keychain_store_token(keychain, NULL, NULL);
  assert(err == APP_ERROR_INVALID_ARG);

  err = app_keychain_retrieve_token(keychain, NULL, NULL);
  assert(err == APP_ERROR_INVALID_ARG);

  err = app_keychain_delete_token(keychain, NULL);
  assert(err == APP_ERROR_INVALID_ARG);

  // Test non-existent token
  app_oauth_token_t *token = NULL;
  err = app_keychain_retrieve_token(keychain, "nonexistent", &token);
  assert(err == APP_ERROR_KEYCHAIN_NOT_FOUND);
  assert(token == NULL);

  err = app_keychain_delete_token(keychain, "nonexistent");
  assert(err == APP_ERROR_KEYCHAIN_NOT_FOUND);

  // Cleanup
  err = app_keychain_cleanup(keychain);
  assert(err == APP_SUCCESS);

  printf("✓ Keychain error handling passed\n");
}

static void test_keychain_overwrite() {
  printf("Testing keychain token overwrite...\n");

  app_keychain_t *keychain = NULL;
  app_error err = app_keychain_create_mock(&keychain);
  assert(err == APP_SUCCESS);

  // Store initial token
  app_oauth_token_t token1 = {.access_token = "first_token",
                              .refresh_token = "first_refresh",
                              .expires_at = 1000,
                              .scope = "read"};

  err = app_keychain_store_token(keychain, "account", &token1);
  assert(err == APP_SUCCESS);

  // Overwrite with new token
  app_oauth_token_t token2 = {.access_token = "second_token",
                              .refresh_token = "second_refresh",
                              .expires_at = 2000,
                              .scope = "read write"};

  err = app_keychain_store_token(keychain, "account", &token2);
  assert(err == APP_SUCCESS);

  // Retrieve and verify it's the second token
  app_oauth_token_t *retrieved = NULL;
  err = app_keychain_retrieve_token(keychain, "account", &retrieved);
  assert(err == APP_SUCCESS);
  assert(strcmp(retrieved->access_token, "second_token") == 0);
  assert(retrieved->expires_at == 2000);

  app_oauth_token_free(retrieved);
  app_keychain_cleanup(keychain);

  printf("✓ Keychain token overwrite passed\n");
}

int main() {
  printf("\n=== Keychain Tests ===\n\n");

  test_mock_keychain_basic();
  test_mock_keychain_multiple_accounts();
  test_keychain_error_handling();
  test_keychain_overwrite();

  printf("\n✓ All keychain tests passed!\n\n");
  return 0;
}