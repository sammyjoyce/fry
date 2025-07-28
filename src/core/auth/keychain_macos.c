#ifdef __APPLE__

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <stdio.h>
#include <string.h>

#include "utils/json.h"
#include "utils/logging.h"
#include "utils/memory.h"
#include "oauth.h"

// Constants
static const char *KEYCHAIN_SERVICE = "com.anthropic.fry";
static const char *KEYCHAIN_LABEL = "Fry CLI OAuth Token";

/**
 * @brief Convert C string to CFString
 *
 * Caller must CFRelease the returned string
 */
static CFStringRef create_cf_string(const char *str) {
  if (!str)
    return NULL;
  return CFStringCreateWithCString(kCFAllocatorDefault, str,
                                   kCFStringEncodingUTF8);
}

/**
 * @brief Convert CFString to C string
 *
 * Caller must free the returned string with app_free
 */
static char *cf_string_to_c_string(CFStringRef cf_str) {
  if (!cf_str)
    return NULL;

  CFIndex length = CFStringGetLength(cf_str);
  CFIndex max_size =
      CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
  char *buffer = app_malloc(max_size);
  if (!buffer)
    return NULL;

  if (CFStringGetCString(cf_str, buffer, max_size, kCFStringEncodingUTF8)) {
    return buffer;
  }

  app_free(buffer);
  return NULL;
}

/**
 * @brief Translate macOS OSStatus to app_error
 */
static app_error translate_osstatus(OSStatus status) {
  switch (status) {
  case errSecSuccess:
    return APP_SUCCESS;
  case errSecItemNotFound:
    return APP_ERROR_KEYCHAIN_NOT_FOUND;
  case errSecAuthFailed:
  case errSecUserCanceled:
    return APP_ERROR_KEYCHAIN_ACCESS_DENIED;
  case errSecInteractionNotAllowed:
    return APP_ERROR_KEYCHAIN_LOCKED;
  case errSecDuplicateItem:
    return APP_SUCCESS;  // We'll handle this in store_token
  case errSecWrPerm:
  case errSecReadOnly:
    LOG_ERROR(
        "macOS Keychain write permission denied (error %d). Try running with "
        "code signing.",
        (int)status);
    return APP_ERROR_KEYCHAIN_ACCESS_DENIED;
  default:
    LOG_ERROR("macOS Keychain error: %d", (int)status);
    return APP_ERROR_KEYCHAIN_PLATFORM;
  }
}

/**
 * @brief Serialize OAuth token to JSON string
 */
static char *serialize_token_to_json(const app_oauth_token_t *token) {
  json_value_t *root = json_value_object();
  if (!root)
    return NULL;

  json_object_t *obj = root->object_val;

  // Add token fields
  json_object_set(obj, "access_token", json_value_string(token->access_token));
  json_object_set(obj, "refresh_token",
                  json_value_string(token->refresh_token));
  json_object_set(obj, "expires_at",
                  json_value_number((double)token->expires_at));

  // Add scopes array
  if (token->scopes && token->scopes_count > 0) {
    json_value_t *scopes_arr = json_value_array();
    for (size_t i = 0; i < token->scopes_count; i++) {
      json_array_append(scopes_arr->array_val,
                        json_value_string(token->scopes[i]));
    }
    json_object_set(obj, "scopes", scopes_arr);
  }

  // Add subscription type if present
  if (token->subscription_type) {
    json_object_set(obj, "subscription_type",
                    json_value_string(token->subscription_type));
  }

  // Serialize to string
  char *json_str = NULL;
  app_error err = json_stringify(&json_str, root);
  json_value_destroy(root);

  if (err != APP_SUCCESS) {
    return NULL;
  }

  return json_str;
}

/**
 * @brief Parse OAuth token from JSON string
 */
static app_oauth_token_t *parse_token_from_json(const char *json_str,
                                                size_t length) {
  // Create null-terminated string for parser
  char *json_copy = app_malloc(length + 1);
  if (!json_copy)
    return NULL;
  memcpy(json_copy, json_str, length);
  json_copy[length] = '\0';

  json_value_t *root = NULL;
  app_error err = json_parse(&root, json_copy);
  app_free(json_copy);

  if (err != APP_SUCCESS || !root || root->type != JSON_TYPE_OBJECT) {
    if (root)
      json_value_destroy(root);
    return NULL;
  }

  json_object_t *obj = root->object_val;
  app_oauth_token_t *token = app_calloc(1, sizeof(app_oauth_token_t));
  if (!token) {
    json_value_destroy(root);
    return NULL;
  }

  // Extract token fields
  json_value_t *val;

  val = json_object_get(obj, "access_token");
  if (val && val->type == JSON_TYPE_STRING) {
    token->access_token = app_strdup(val->string_val);
  }

  val = json_object_get(obj, "refresh_token");
  if (val && val->type == JSON_TYPE_STRING) {
    token->refresh_token = app_strdup(val->string_val);
  }

  val = json_object_get(obj, "expires_at");
  if (val && val->type == JSON_TYPE_NUMBER) {
    token->expires_at = (int64_t)val->number_val;
  }

  // Extract scopes array
  val = json_object_get(obj, "scopes");
  if (val && val->type == JSON_TYPE_ARRAY) {
    json_array_t *scopes_arr = val->array_val;
    token->scopes_count = scopes_arr->count;
    if (token->scopes_count > 0) {
      token->scopes = app_calloc(token->scopes_count, sizeof(char *));
      for (size_t i = 0; i < token->scopes_count; i++) {
        json_value_t *scope_val = json_array_get(scopes_arr, i);
        if (scope_val && scope_val->type == JSON_TYPE_STRING) {
          token->scopes[i] = app_strdup(scope_val->string_val);
        }
      }
    }
  }

  val = json_object_get(obj, "subscription_type");
  if (val && val->type == JSON_TYPE_STRING) {
    token->subscription_type = app_strdup(val->string_val);
  }

  json_value_destroy(root);
  return token;
}

/**
 * @brief Store token in macOS keychain
 */
app_error keychain_store_token(const char *account_id,
                              const app_oauth_token_t *token) {
  if (!account_id || !token) {
    return APP_ERROR_INVALID_ARG;
  }

  // Serialize token to JSON
  char *json_data = serialize_token_to_json(token);
  if (!json_data) {
    return APP_ERROR_MEMORY;
  }

  // Create keychain query dictionary
  CFMutableDictionaryRef query = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);

  if (!query) {
    app_free(json_data);
    return APP_ERROR_MEMORY;
  }

  // Set up query parameters
  CFStringRef cf_service = create_cf_string(KEYCHAIN_SERVICE);
  CFStringRef cf_account = create_cf_string(account_id);
  CFDataRef cf_data = CFDataCreate(kCFAllocatorDefault,
                                   (const UInt8 *)json_data, strlen(json_data));

  CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
  CFDictionarySetValue(query, kSecAttrService, cf_service);
  CFDictionarySetValue(query, kSecAttrAccount, cf_account);
  // Try to update existing item first
  CFMutableDictionaryRef update = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);

  CFDictionarySetValue(update, kSecValueData, cf_data);

  OSStatus status = SecItemUpdate(query, update);

  // If not found, add new item
  if (status == errSecItemNotFound) {
    CFStringRef cf_label = create_cf_string(KEYCHAIN_LABEL);
    CFDictionarySetValue(query, kSecValueData, cf_data);
    CFDictionarySetValue(query, kSecAttrLabel, cf_label);
    CFDictionarySetValue(query, kSecAttrDescription, cf_label);
    // Add accessibility attribute - when unlocked
    CFDictionarySetValue(query, kSecAttrAccessible,
                         kSecAttrAccessibleWhenUnlocked);
    // Don't sync to iCloud
    CFDictionarySetValue(query, kSecAttrSynchronizable, kCFBooleanFalse);
    status = SecItemAdd(query, NULL);
    CFRelease(cf_label);
  }

  // Cleanup
  CFRelease(query);
  CFRelease(update);
  CFRelease(cf_service);
  CFRelease(cf_account);
  CFRelease(cf_data);
  app_free(json_data);

  if (status == errSecSuccess) {
    LOG_DEBUG("macOS Keychain: stored token for account %s", account_id);
  }

  return translate_osstatus(status);
}

/**
 * @brief Retrieve token from macOS keychain
 */
app_error keychain_retrieve_token(const char *account_id,
                                 app_oauth_token_t **token) {
  if (!account_id || !token) {
    return APP_ERROR_INVALID_ARG;
  }

  *token = NULL;

  // Create search query
  CFMutableDictionaryRef query = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);

  if (!query) {
    return APP_ERROR_MEMORY;
  }

  CFStringRef cf_service = create_cf_string(KEYCHAIN_SERVICE);
  CFStringRef cf_account = create_cf_string(account_id);

  CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
  CFDictionarySetValue(query, kSecAttrService, cf_service);
  CFDictionarySetValue(query, kSecAttrAccount, cf_account);
  CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
  CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

  // Execute search
  CFTypeRef result = NULL;
  OSStatus status = SecItemCopyMatching(query, &result);

  // Parse result if found
  if (status == errSecSuccess && result) {
    CFDataRef data = (CFDataRef)result;
    const char *json_str = (const char *)CFDataGetBytePtr(data);
    CFIndex length = CFDataGetLength(data);

    *token = parse_token_from_json(json_str, length);
    if (!*token) {
      status = errSecInternalError;
    } else {
      LOG_DEBUG("macOS Keychain: retrieved token for account %s", account_id);
    }

    CFRelease(result);
  }

  // Cleanup
  CFRelease(query);
  CFRelease(cf_service);
  CFRelease(cf_account);

  return translate_osstatus(status);
}

/**
 * @brief Delete token from macOS keychain
 */
app_error keychain_delete_token(const char *account_id) {
  if (!account_id) {
    return APP_ERROR_INVALID_ARG;
  }

  CFMutableDictionaryRef query = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);

  if (!query) {
    return APP_ERROR_MEMORY;
  }

  CFStringRef cf_service = create_cf_string(KEYCHAIN_SERVICE);
  CFStringRef cf_account = create_cf_string(account_id);

  CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
  CFDictionarySetValue(query, kSecAttrService, cf_service);
  CFDictionarySetValue(query, kSecAttrAccount, cf_account);

  OSStatus status = SecItemDelete(query);

  // Not found is success for delete
  if (status == errSecItemNotFound) {
    status = errSecSuccess;
  }

  if (status == errSecSuccess) {
    LOG_DEBUG("macOS Keychain: deleted token for account %s", account_id);
  }

  // Cleanup
  CFRelease(query);
  CFRelease(cf_service);
  CFRelease(cf_account);

  return translate_osstatus(status);
}

/**
 * @brief List all accounts in macOS keychain
 */
app_error keychain_list_accounts(char ***account_ids, size_t *count) {
  if (!account_ids || !count) {
    return APP_ERROR_INVALID_ARG;
  }

  *account_ids = NULL;
  *count = 0;

  // Create query for all items with our service
  CFMutableDictionaryRef query = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);

  if (!query) {
    return APP_ERROR_MEMORY;
  }

  CFStringRef cf_service = create_cf_string(KEYCHAIN_SERVICE);

  CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
  CFDictionarySetValue(query, kSecAttrService, cf_service);
  CFDictionarySetValue(query, kSecReturnAttributes, kCFBooleanTrue);
  CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);

  // Execute search
  CFTypeRef result = NULL;
  OSStatus status = SecItemCopyMatching(query, &result);

  // Extract account names from results
  if (status == errSecSuccess && result) {
    CFArrayRef items = (CFArrayRef)result;
    CFIndex item_count = CFArrayGetCount(items);

    if (item_count > 0) {
      *count = (size_t)item_count;
      *account_ids = app_calloc(item_count, sizeof(char *));

      if (*account_ids) {
        for (CFIndex i = 0; i < item_count; i++) {
          CFDictionaryRef item = CFArrayGetValueAtIndex(items, i);
          CFStringRef account = CFDictionaryGetValue(item, kSecAttrAccount);
          if (account) {
            (*account_ids)[i] = cf_string_to_c_string(account);
          }
        }
        LOG_DEBUG("macOS Keychain: found %zu accounts", *count);
      } else {
        status = errSecAllocate;
      }
    }

    CFRelease(result);
  } else if (status == errSecItemNotFound) {
    // No items found is not an error
    status = errSecSuccess;
    *count = 0;
  }

  // Cleanup
  CFRelease(query);
  CFRelease(cf_service);

  return translate_osstatus(status);
}

/**
 * @brief Check if token exists in macOS keychain
 */
app_error keychain_has_token(const char *account_id, bool *exists) {
  if (!account_id || !exists) {
    return APP_ERROR_INVALID_ARG;
  }

  *exists = false;

  CFMutableDictionaryRef query = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);

  if (!query) {
    return APP_ERROR_MEMORY;
  }

  CFStringRef cf_service = create_cf_string(KEYCHAIN_SERVICE);
  CFStringRef cf_account = create_cf_string(account_id);

  CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
  CFDictionarySetValue(query, kSecAttrService, cf_service);
  CFDictionarySetValue(query, kSecAttrAccount, cf_account);
  CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

  OSStatus status = SecItemCopyMatching(query, NULL);

  if (status == errSecSuccess) {
    *exists = true;
  } else if (status == errSecItemNotFound) {
    *exists = false;
    status = errSecSuccess;  // Not found is not an error for this operation
  }

  // Cleanup
  CFRelease(query);
  CFRelease(cf_service);
  CFRelease(cf_account);

  return translate_osstatus(status);
}

#endif  // __APPLE__