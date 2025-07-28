#include "keychain.h"

#include <stdio.h>
#include <string.h>

#include "../utils/json.h"
#include "../utils/logging.h"
#include "../utils/memory.h"

// Platform-specific function declarations
#ifdef __APPLE__
// Forward declarations for macOS implementation
app_error keychain_store_token(const char *account_id,
                               const app_oauth_token_t *token);
app_error keychain_retrieve_token(const char *account_id,
                                  app_oauth_token_t **token);
app_error keychain_delete_token(const char *account_id);
app_error keychain_list_accounts(char ***account_ids, size_t *count);
app_error keychain_has_token(const char *account_id, bool *exists);
#endif

#ifdef __linux__
// Forward declarations for Linux implementation
app_error keychain_linux_store_token(const char *account_id,
                                     const app_oauth_token_t *token);
app_error keychain_linux_retrieve_token(const char *account_id,
                                        app_oauth_token_t **token);
app_error keychain_linux_delete_token(const char *account_id);
app_error keychain_linux_list_accounts(char ***account_ids, size_t *count);
app_error keychain_linux_has_token(const char *account_id, bool *exists);
#endif

/**
 * @brief Internal keychain structure
 *
 * This structure is opaque to users and contains platform-specific
 * implementation details.
 */
struct app_keychain {
  bool is_mock;
#ifdef __APPLE__
  bool use_macos_keychain;
#endif
#ifdef __linux__
  bool use_linux_keychain;
#endif
  json_object_t *mock_storage;  // For mock implementation
};

/**
 * @brief Mock keychain implementation for testing
 */
static app_error mock_store_token(app_keychain_t *keychain,
                                  const char *account_id,
                                  const app_oauth_token_t *token) {
  if (!keychain || !account_id || !token) {
    return APP_ERROR_INVALID_ARG;
  }

  // Create JSON representation of token
  json_value_t *token_obj = json_value_object();
  if (!token_obj) {
    return APP_ERROR_MEMORY;
  }

  json_object_t *obj = token_obj->object_val;

  app_error err;
  err = json_object_set(obj, "access_token",
                        json_value_string(token->access_token));
  if (err != APP_SUCCESS) {
    json_value_destroy(token_obj);
    return err;
  }

  err = json_object_set(obj, "refresh_token",
                        json_value_string(token->refresh_token));
  if (err != APP_SUCCESS) {
    json_value_destroy(token_obj);
    return err;
  }

  err = json_object_set(obj, "expires_at",
                        json_value_number((double)token->expires_at));
  if (err != APP_SUCCESS) {
    json_value_destroy(token_obj);
    return err;
  }

  // Store scopes as an array
  if (token->scopes && token->scopes_count > 0) {
    json_value_t *scopes_arr = json_value_array();
    for (size_t i = 0; i < token->scopes_count; i++) {
      err = json_array_append(scopes_arr->array_val,
                              json_value_string(token->scopes[i]));
      if (err != APP_SUCCESS) {
        json_value_destroy(scopes_arr);
        json_value_destroy(token_obj);
        return err;
      }
    }
    err = json_object_set(obj, "scopes", scopes_arr);
    if (err != APP_SUCCESS) {
      json_value_destroy(token_obj);
      return err;
    }
  }

  if (token->subscription_type) {
    err = json_object_set(obj, "subscription_type",
                          json_value_string(token->subscription_type));
    if (err != APP_SUCCESS) {
      json_value_destroy(token_obj);
      return err;
    }
  }

  // Store in mock storage
  err = json_object_set(keychain->mock_storage, account_id, token_obj);
  if (err != APP_SUCCESS) {
    json_value_destroy(token_obj);
    return err;
  }

  LOG_DEBUG("Mock keychain: stored token for account %s", account_id);
  return APP_SUCCESS;
}

static app_error mock_retrieve_token(app_keychain_t *keychain,
                                     const char *account_id,
                                     app_oauth_token_t **token) {
  if (!keychain || !account_id || !token) {
    return APP_ERROR_INVALID_ARG;
  }

  json_value_t *token_val = json_object_get(keychain->mock_storage, account_id);
  if (!token_val || token_val->type != JSON_TYPE_OBJECT) {
    return APP_ERROR_KEYCHAIN_NOT_FOUND;
  }

  json_object_t *token_obj = token_val->object_val;

  // Allocate and populate token
  app_oauth_token_t *new_token = app_calloc(1, sizeof(app_oauth_token_t));
  if (!new_token) {
    return APP_ERROR_MEMORY;
  }

  json_value_t *value;
  value = json_object_get(token_obj, "access_token");
  if (value && value->type == JSON_TYPE_STRING) {
    new_token->access_token = app_strdup(value->string_val);
  }

  value = json_object_get(token_obj, "refresh_token");
  if (value && value->type == JSON_TYPE_STRING) {
    new_token->refresh_token = app_strdup(value->string_val);
  }

  value = json_object_get(token_obj, "expires_at");
  if (value && value->type == JSON_TYPE_NUMBER) {
    new_token->expires_at = (time_t)value->number_val;
  }

  value = json_object_get(token_obj, "scopes");
  if (value && value->type == JSON_TYPE_ARRAY) {
    json_array_t *scopes_arr = value->array_val;
    new_token->scopes_count = scopes_arr->count;
    if (new_token->scopes_count > 0) {
      new_token->scopes = app_calloc(new_token->scopes_count, sizeof(char *));
      for (size_t i = 0; i < new_token->scopes_count; i++) {
        json_value_t *scope_val = json_array_get(scopes_arr, i);
        if (scope_val && scope_val->type == JSON_TYPE_STRING) {
          new_token->scopes[i] = app_strdup(scope_val->string_val);
        }
      }
    }
  }

  value = json_object_get(token_obj, "subscription_type");
  if (value && value->type == JSON_TYPE_STRING) {
    new_token->subscription_type = app_strdup(value->string_val);
  }

  *token = new_token;
  LOG_DEBUG("Mock keychain: retrieved token for account %s", account_id);
  return APP_SUCCESS;
}

static app_error mock_delete_token(app_keychain_t *keychain,
                                   const char *account_id) {
  if (!keychain || !account_id) {
    return APP_ERROR_INVALID_ARG;
  }

  // Find and remove the token
  json_value_t *existing = json_object_get(keychain->mock_storage, account_id);
  if (!existing) {
    return APP_ERROR_KEYCHAIN_NOT_FOUND;
  }

  // Remove from storage
  // Note: The custom JSON API doesn't have a delete function, so we'll set to
  // NULL
  app_error err =
      json_object_set(keychain->mock_storage, account_id, json_value_null());
  if (err != APP_SUCCESS) {
    return err;
  }

  LOG_DEBUG("Mock keychain: deleted token for account %s", account_id);
  return APP_SUCCESS;
}

static app_error mock_list_accounts(app_keychain_t *keychain,
                                    char ***account_ids, size_t *count) {
  if (!keychain || !account_ids || !count) {
    return APP_ERROR_INVALID_ARG;
  }

  // Count non-null entries
  size_t num_accounts = 0;
  for (size_t i = 0; i < keychain->mock_storage->count; i++) {
    json_value_t *val = keychain->mock_storage->values[i];
    if (val && val->type == JSON_TYPE_OBJECT) {
      num_accounts++;
    }
  }

  *count = num_accounts;
  if (num_accounts == 0) {
    *account_ids = NULL;
    return APP_SUCCESS;
  }

  *account_ids = app_calloc(num_accounts, sizeof(char *));
  if (!*account_ids) {
    return APP_ERROR_MEMORY;
  }

  size_t index = 0;
  for (size_t i = 0; i < keychain->mock_storage->count && index < num_accounts;
       i++) {
    json_value_t *val = keychain->mock_storage->values[i];
    if (val && val->type == JSON_TYPE_OBJECT) {
      (*account_ids)[index] = app_strdup(keychain->mock_storage->keys[i]);
      if (!(*account_ids)[index]) {
        // Clean up on failure
        for (size_t j = 0; j < index; j++) {
          app_free((*account_ids)[j]);
        }
        app_free(*account_ids);
        return APP_ERROR_MEMORY;
      }
      index++;
    }
  }

  return APP_SUCCESS;
}

static app_error mock_has_token(app_keychain_t *keychain,
                                const char *account_id, bool *exists) {
  if (!keychain || !account_id || !exists) {
    return APP_ERROR_INVALID_ARG;
  }

  json_value_t *val = json_object_get(keychain->mock_storage, account_id);
  *exists = (val != NULL && val->type == JSON_TYPE_OBJECT);
  return APP_SUCCESS;
}

/**
 * @brief Create a mock keychain for testing
 */
app_error app_keychain_create_mock(app_keychain_t **keychain) {
  if (!keychain) {
    return APP_ERROR_INVALID_ARG;
  }

  app_keychain_t *mock = app_calloc(1, sizeof(app_keychain_t));
  if (!mock) {
    return APP_ERROR_MEMORY;
  }

  mock->is_mock = true;
  json_value_t *storage_val = json_value_object();
  if (!storage_val) {
    app_free(mock);
    return APP_ERROR_MEMORY;
  }
  mock->mock_storage = storage_val->object_val;

  *keychain = mock;
  LOG_DEBUG("Created mock keychain for testing");
  return APP_SUCCESS;
}

/**
 * @brief Get keychain capabilities
 */
app_error app_keychain_get_capabilities(const app_keychain_t *keychain,
                                        app_keychain_capabilities_t *caps) {
  if (!keychain || !caps) {
    return APP_ERROR_INVALID_ARG;
  }

#ifdef __APPLE__
  if (keychain->use_macos_keychain) {
    caps->can_store_tokens = true;
    caps->requires_unlock = true;      // May require unlock
    caps->supports_encryption = true;  // AES-256 by OS
    caps->supports_sync = false;       // We use local keychain only
    caps->max_token_size = 0;          // Effectively unlimited
    caps->backend_name = "macOS Keychain";
    return APP_SUCCESS;
  }
#endif

#ifdef __linux__
  if (keychain->use_linux_keychain) {
    caps->can_store_tokens = true;
    caps->requires_unlock = true;      // May require unlock
    caps->supports_encryption = true;  // Encrypted by keyring daemon
    caps->supports_sync = false;       // We use local keyring only
    caps->max_token_size = 0;          // Effectively unlimited
    caps->backend_name = "Linux Secret Service (libsecret)";
    return APP_SUCCESS;
  }
#endif

  if (keychain->is_mock) {
    caps->can_store_tokens = true;
    caps->requires_unlock = false;
    caps->supports_encryption = false;
    caps->supports_sync = false;
    caps->max_token_size = 0;  // Unlimited
    caps->backend_name = "Mock Keychain (In-Memory)";
    return APP_SUCCESS;
  }

  return APP_ERROR_KEYCHAIN_UNSUPPORTED;
}
/**
 * @brief Store token - dispatches to platform or mock implementation
 */
app_error app_keychain_store_token(app_keychain_t *keychain,
                                   const char *account_id,
                                   const app_oauth_token_t *token) {
  if (!keychain) {
    return APP_ERROR_INVALID_ARG;
  }

#ifdef __APPLE__
  if (keychain->use_macos_keychain) {
    app_error err = keychain_store_token(account_id, token);

    // If we get permission errors on macOS, fall back to mock
    if (err == APP_ERROR_KEYCHAIN_ACCESS_DENIED) {
      LOG_WARNING(
          "macOS Keychain write access denied (likely due to unsigned "
          "binary).");
      LOG_WARNING(
          "Falling back to in-memory storage. To use Keychain, sign the "
          "binary.");

      // Convert to mock keychain
      keychain->use_macos_keychain = false;
      keychain->is_mock = true;

      // Create mock storage
      json_value_t *storage_val = json_value_object();
      if (!storage_val) {
        return APP_ERROR_MEMORY;
      }
      keychain->mock_storage = storage_val->object_val;

      // Retry with mock
      return mock_store_token(keychain, account_id, token);
    }

    return err;
  }
#endif

  if (keychain->is_mock) {
    return mock_store_token(keychain, account_id, token);
  }

  return APP_ERROR_KEYCHAIN_UNSUPPORTED;
}

/**
 * @brief Retrieve token - dispatches to platform or mock implementation
 */
app_error app_keychain_retrieve_token(app_keychain_t *keychain,
                                      const char *account_id,
                                      app_oauth_token_t **token) {
  if (!keychain) {
    return APP_ERROR_INVALID_ARG;
  }

#ifdef __APPLE__
  if (keychain->use_macos_keychain) {
    return keychain_retrieve_token(account_id, token);
  }
#endif

#ifdef __linux__
  if (keychain->use_linux_keychain) {
    return keychain_linux_retrieve_token(account_id, token);
  }
#endif

  if (keychain->is_mock) {
    return mock_retrieve_token(keychain, account_id, token);
  }

  return APP_ERROR_KEYCHAIN_UNSUPPORTED;
}

/**
 * @brief Delete token - dispatches to platform or mock implementation
 */
app_error app_keychain_delete_token(app_keychain_t *keychain,
                                    const char *account_id) {
  if (!keychain) {
    return APP_ERROR_INVALID_ARG;
  }

#ifdef __APPLE__
  if (keychain->use_macos_keychain) {
    return keychain_delete_token(account_id);
  }
#endif

#ifdef __linux__
  if (keychain->use_linux_keychain) {
    return keychain_linux_delete_token(account_id);
  }
#endif

  if (keychain->is_mock) {
    return mock_delete_token(keychain, account_id);
  }

  return APP_ERROR_KEYCHAIN_UNSUPPORTED;
}

/**
 * @brief List accounts - dispatches to platform or mock implementation
 */
app_error app_keychain_list_accounts(app_keychain_t *keychain,
                                     char ***account_ids, size_t *count) {
  if (!keychain) {
    return APP_ERROR_INVALID_ARG;
  }

#ifdef __APPLE__
  if (keychain->use_macos_keychain) {
    return keychain_list_accounts(account_ids, count);
  }
#endif

#ifdef __linux__
  if (keychain->use_linux_keychain) {
    return keychain_linux_list_accounts(account_ids, count);
  }
#endif

  if (keychain->is_mock) {
    return mock_list_accounts(keychain, account_ids, count);
  }

  return APP_ERROR_KEYCHAIN_UNSUPPORTED;
}

/**
 * @brief Check if token exists - dispatches to platform or mock implementation
 */
app_error app_keychain_has_token(app_keychain_t *keychain,
                                 const char *account_id, bool *exists) {
  if (!keychain) {
    return APP_ERROR_INVALID_ARG;
  }

#ifdef __APPLE__
  if (keychain->use_macos_keychain) {
    return keychain_has_token(account_id, exists);
  }
#endif

#ifdef __linux__
  if (keychain->use_linux_keychain) {
    return keychain_linux_has_token(account_id, exists);
  }
#endif

  if (keychain->is_mock) {
    return mock_has_token(keychain, account_id, exists);
  }

  return APP_ERROR_KEYCHAIN_UNSUPPORTED;
}

/**
 * @brief Initialize platform-specific keychain
 */
app_error app_keychain_init(app_keychain_t **keychain) {
  if (!keychain) {
    return APP_ERROR_INVALID_ARG;
  }

#ifdef __APPLE__
  // Try to use macOS keychain
  app_keychain_t *kc = app_calloc(1, sizeof(app_keychain_t));
  if (!kc) {
    return APP_ERROR_MEMORY;
  }

  kc->use_macos_keychain = true;
  kc->is_mock = false;

  // On modern macOS, unsigned binaries may have restricted keychain access
  // We'll detect this on first write attempt and fall back gracefully
  *keychain = kc;
  LOG_INFO("Attempting to use macOS Keychain for secure token storage");
  return APP_SUCCESS;
#elif defined(__linux__)
  // Try to use Linux keychain (libsecret)
  app_keychain_t *kc = app_calloc(1, sizeof(app_keychain_t));
  if (!kc) {
    return APP_ERROR_MEMORY;
  }

  kc->use_linux_keychain = true;
  kc->is_mock = false;

  *keychain = kc;
  LOG_INFO("Using libsecret for secure token storage on Linux");
  return APP_SUCCESS;
#endif

  // Fall back to mock keychain on other platforms
  LOG_WARNING("No platform keychain available, using in-memory mock storage");
  return app_keychain_create_mock(keychain);
}

/**
 * @brief Clean up keychain resources
 */
app_error app_keychain_cleanup(app_keychain_t *keychain) {
  if (!keychain) {
    return APP_ERROR_INVALID_ARG;
  }

  if (keychain->is_mock) {
    // The json_value_t that contains mock_storage needs to be freed
    // but we don't have a reference to it. For now, just free the keychain.
    // TODO: Keep a reference to the json_value_t for proper cleanup
    app_free(keychain);
    LOG_DEBUG("Cleaned up mock keychain");
    return APP_SUCCESS;
  }

  // Platform-specific cleanup will be called here
  app_free(keychain);
  return APP_SUCCESS;
}