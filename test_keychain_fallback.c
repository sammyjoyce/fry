#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "src/core/keychain.h"
#include "src/core/oauth.h"
#include "src/utils/logging.h"
#include "src/utils/memory.h"

int main() {
  printf("\n=== Testing Keychain with Fallback ===\n\n");

  // Initialize logging
  app_log_init();
  app_log_set_level(LOG_LEVEL_DEBUG);

  // Initialize keychain (will try macOS first)
  app_keychain_t *keychain = NULL;
  app_error err = app_keychain_init(&keychain);
  assert(err == APP_SUCCESS);

  // Get initial capabilities
  app_keychain_capabilities_t caps;
  err = app_keychain_get_capabilities(keychain, &caps);
  assert(err == APP_SUCCESS);
  printf("Initial backend: %s\n", caps.backend_name);

  // Create test token
  app_oauth_token_t token = {.access_token = "test_access_token",
                             .refresh_token = "test_refresh_token",
                             .expires_at = time(NULL) + 3600,
                             .scopes = (char *[]){"read", "write"},
                             .scopes_count = 2,
                             .subscription_type = "pro"};

  // Try to store token (may trigger fallback)
  printf("\nAttempting to store token...\n");
  err = app_keychain_store_token(keychain, "test_account", &token);
  assert(err == APP_SUCCESS);

  // Check capabilities again to see if we fell back
  err = app_keychain_get_capabilities(keychain, &caps);
  assert(err == APP_SUCCESS);
  printf("\nBackend after store: %s\n", caps.backend_name);

  // Verify token was stored
  bool exists = false;
  err = app_keychain_has_token(keychain, "test_account", &exists);
  assert(err == APP_SUCCESS);
  assert(exists == true);
  printf("✓ Token stored successfully\n");

  // Retrieve and verify
  app_oauth_token_t *retrieved = NULL;
  err = app_keychain_retrieve_token(keychain, "test_account", &retrieved);
  assert(err == APP_SUCCESS);
  assert(strcmp(retrieved->access_token, token.access_token) == 0);
  printf("✓ Token retrieved successfully\n");

  app_oauth_token_free(retrieved);

  // Cleanup
  err = app_keychain_cleanup(keychain);
  assert(err == APP_SUCCESS);

  printf("\n✅ Test completed successfully!\n\n");
  return 0;
}