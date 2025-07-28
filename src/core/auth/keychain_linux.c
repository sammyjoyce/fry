/*
 * Linux keychain implementation using libsecret.
 *
 * This module provides secure token storage on Linux systems using the
 * Secret Service API through libsecret. This integrates with system
 * keyrings like GNOME Keyring and KWallet, providing encrypted storage
 * that's unlocked with the user's login credentials.
 */

#include <libsecret/secret.h>
#include <string.h>

#include "utils/logging.h"
#include "utils/memory.h"
#include "keychain.h"
#include "oauth.h"

// Define the schema for our secrets
static const SecretSchema fry_schema = {
    "ai.opencode.fry.Token",
    SECRET_SCHEMA_NONE,
    {
        {"account", SECRET_SCHEMA_ATTRIBUTE_STRING},
        {"service", SECRET_SCHEMA_ATTRIBUTE_STRING},
        {NULL, 0},
    }};

// Convert app_error to appropriate error for consistency
static app_error convert_gerror(GError *error) {
  if (!error) {
    return APP_SUCCESS;
  }

  app_error result = APP_ERROR_KEYCHAIN;

  if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    result = APP_ERROR_CANCELLED;
  } else if (g_error_matches(error, SECRET_ERROR,
                             SECRET_ERROR_NO_SUCH_OBJECT)) {
    result = APP_ERROR_NOT_FOUND;
  }

  LOG_ERROR("libsecret error: %s", error->message);
  g_error_free(error);
  return result;
}

app_error keychain_linux_store_token(const char *account_id,
                                     const app_oauth_token_t *token) {
  if (!account_id || !token) {
    return APP_ERROR_INVALID_ARG;
  }

  // Serialize token to JSON
  char *token_json = NULL;
  int len = asprintf(&token_json,
                     "{"
                     "\"access_token\":\"%s\","
                     "\"refresh_token\":\"%s\","
                     "\"expires_at\":%lld,"
                     "\"scopes\":[",
                     token->access_token ? token->access_token : "",
                     token->refresh_token ? token->refresh_token : "",
                     (long long)token->expires_at);

  if (len < 0) {
    return APP_ERROR_MEMORY;
  }

  // Add scopes
  for (size_t i = 0; i < token->scopes_count; i++) {
    char *new_json = NULL;
    len = asprintf(&new_json, "%s%s\"%s\"", token_json, i > 0 ? "," : "",
                   token->scopes[i]);
    free(token_json);
    if (len < 0) {
      return APP_ERROR_MEMORY;
    }
    token_json = new_json;
  }

  // Close JSON
  char *final_json = NULL;
  len = asprintf(&final_json, "%s]}", token_json);
  free(token_json);
  if (len < 0) {
    return APP_ERROR_MEMORY;
  }

  // Store in keyring
  GError *error = NULL;
  gboolean result = secret_password_store_sync(
      &fry_schema, SECRET_COLLECTION_DEFAULT,
      "Fry CLI Token",  // Display name
      final_json,       // The secret
      NULL,             // Cancellable
      &error, "account", account_id, "service", "fry-cli", NULL);

  // Clean up
  app_secure_zero(final_json, strlen(final_json));
  free(final_json);

  if (!result) {
    return convert_gerror(error);
  }

  return APP_SUCCESS;
}

app_error keychain_linux_retrieve_token(const char *account_id,
                                        app_oauth_token_t **token) {
  if (!account_id || !token) {
    return APP_ERROR_INVALID_ARG;
  }

  GError *error = NULL;
  gchar *secret = secret_password_lookup_sync(&fry_schema,
                                              NULL,  // Cancellable
                                              &error, "account", account_id,
                                              "service", "fry-cli", NULL);

  if (!secret) {
    if (error) {
      return convert_gerror(error);
    }
    return APP_ERROR_NOT_FOUND;
  }

  // Parse JSON (simplified - in production use proper JSON parser)
  *token = app_calloc(1, sizeof(app_oauth_token_t));

  // Extract access_token
  char *access_start = strstr(secret, "\"access_token\":\"");
  if (access_start) {
    access_start += 16;
    char *access_end = strchr(access_start, '"');
    if (access_end && access_end > access_start) {
      size_t len = access_end - access_start;
      (*token)->access_token = app_secure_malloc(len + 1);
      memcpy((*token)->access_token, access_start, len);
      (*token)->access_token[len] = '\0';
    }
  }

  // Extract refresh_token
  char *refresh_start = strstr(secret, "\"refresh_token\":\"");
  if (refresh_start) {
    refresh_start += 17;
    char *refresh_end = strchr(refresh_start, '"');
    if (refresh_end && refresh_end > refresh_start) {
      size_t len = refresh_end - refresh_start;
      (*token)->refresh_token = app_secure_malloc(len + 1);
      memcpy((*token)->refresh_token, refresh_start, len);
      (*token)->refresh_token[len] = '\0';
    }
  }

  // Extract expires_at
  char *expires_start = strstr(secret, "\"expires_at\":");
  if (expires_start) {
    expires_start += 13;
    (*token)->expires_at = strtoll(expires_start, NULL, 10);
  }

  // Clean up secret
  secret_password_free(secret);

  // Validate we got required fields
  if (!(*token)->access_token) {
    app_oauth_token_free(*token);
    *token = NULL;
    return APP_ERROR_INVALID_TOKEN;
  }

  return APP_SUCCESS;
}

app_error keychain_linux_delete_token(const char *account_id) {
  if (!account_id) {
    return APP_ERROR_INVALID_ARG;
  }

  GError *error = NULL;
  gboolean result = secret_password_clear_sync(&fry_schema,
                                               NULL,  // Cancellable
                                               &error, "account", account_id,
                                               "service", "fry-cli", NULL);

  if (!result) {
    if (error &&
        g_error_matches(error, SECRET_ERROR, SECRET_ERROR_NO_SUCH_OBJECT)) {
      g_error_free(error);
      return APP_ERROR_NOT_FOUND;
    }
    return convert_gerror(error);
  }

  return APP_SUCCESS;
}

app_error keychain_linux_list_accounts(char ***account_ids, size_t *count) {
  if (!account_ids || !count) {
    return APP_ERROR_INVALID_ARG;
  }

  *account_ids = NULL;
  *count = 0;

  GError *error = NULL;
  GList *items = secret_password_search_sync(
      &fry_schema, SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK,
      NULL,  // Cancellable
      &error, "service", "fry-cli", NULL);

  if (error) {
    return convert_gerror(error);
  }

  if (!items) {
    return APP_SUCCESS;
  }

  // Count items
  GList *iter;
  for (iter = items; iter != NULL; iter = g_list_next(iter)) {
    (*count)++;
  }

  if (*count == 0) {
    g_list_free_full(items, (GDestroyNotify)secret_item_unref);
    return APP_SUCCESS;
  }

  // Allocate array
  *account_ids = app_calloc(*count, sizeof(char *));

  // Extract account IDs
  size_t i = 0;
  for (iter = items; iter != NULL && i < *count; iter = g_list_next(iter)) {
    SecretItem *item = iter->data;
    GHashTable *attributes = secret_item_get_attributes(item);

    if (attributes) {
      const gchar *account = g_hash_table_lookup(attributes, "account");
      if (account) {
        (*account_ids)[i] = app_strdup(account);
        i++;
      }
      g_hash_table_unref(attributes);
    }
  }

  // Adjust count if some items didn't have account attribute
  *count = i;

  g_list_free_full(items, (GDestroyNotify)secret_item_unref);
  return APP_SUCCESS;
}

app_error keychain_linux_has_token(const char *account_id, bool *exists) {
  if (!account_id || !exists) {
    return APP_ERROR_INVALID_ARG;
  }

  *exists = false;

  GError *error = NULL;
  gchar *secret = secret_password_lookup_sync(&fry_schema,
                                              NULL,  // Cancellable
                                              &error, "account", account_id,
                                              "service", "fry-cli", NULL);

  if (secret) {
    *exists = true;
    secret_password_free(secret);
    return APP_SUCCESS;
  }

  if (error) {
    if (g_error_matches(error, SECRET_ERROR, SECRET_ERROR_NO_SUCH_OBJECT)) {
      g_error_free(error);
      return APP_SUCCESS;  // Not found is not an error for this function
    }
    return convert_gerror(error);
  }

  return APP_SUCCESS;
}