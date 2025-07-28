#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "error.h"
#include "oauth.h"

/**
 * @file keychain.h
 * @brief Platform-agnostic keychain interface for secure token storage
 *
 * This module provides a unified interface for storing OAuth tokens securely
 * across different platforms (macOS Keychain, Linux Secret Service).
 * Windows is not currently supported.
 *
 * Example usage:
 * @code
 * app_keychain_t* keychain = NULL;
 * app_error err = app_keychain_init(&keychain);
 * if (err == APP_ERROR_NONE) {
 *     app_oauth_token_t* token = ...;
 *     err = app_keychain_store_token(keychain, "account_123", token);
 *     app_keychain_cleanup(keychain);
 * }
 * @endcode
 */

typedef struct app_keychain app_keychain_t;

/**
 * @brief Keychain capabilities for feature detection
 */
typedef struct {
  bool can_store_tokens;     // Platform supports token storage
  bool requires_unlock;      // User must unlock keychain before use
  bool supports_encryption;  // Tokens are encrypted at rest
  bool supports_sync;        // Tokens can sync across devices
  size_t max_token_size;     // Maximum size of token data (0 = unlimited)
  const char *backend_name;  // Human-readable backend name
} app_keychain_capabilities_t;

/**
 * @brief Initialize the keychain subsystem
 *
 * Creates and initializes a keychain instance for the current platform.
 * The caller must call app_keychain_cleanup() when done.
 *
 * @param[out] keychain Pointer to store the initialized keychain instance
 * @return APP_ERROR_NONE on success, error code on failure
 */
APP_NODISCARD app_error app_keychain_init(app_keychain_t **keychain);

/**
 * @brief Get keychain capabilities for the current platform
 *
 * @param[in] keychain The keychain instance
 * @param[out] capabilities Structure to fill with capability information
 * @return APP_ERROR_NONE on success, error code on failure
 */
APP_NODISCARD app_error app_keychain_get_capabilities(
    const app_keychain_t *keychain, app_keychain_capabilities_t *capabilities);

/**
 * @brief Store an OAuth token in the keychain
 *
 * Stores the token securely, associated with the given account ID.
 * If a token already exists for this account, it will be overwritten.
 *
 * @param[in] keychain The keychain instance
 * @param[in] account_id Unique identifier for the account
 * @param[in] token The OAuth token to store
 * @return APP_ERROR_NONE on success, error code on failure
 */
APP_NODISCARD app_error
app_keychain_store_token(app_keychain_t *keychain, const char *account_id,
                         const app_oauth_token_t *token);

/**
 * @brief Retrieve an OAuth token from the keychain
 *
 * Retrieves the token associated with the given account ID.
 * The caller is responsible for freeing the returned token.
 *
 * @param[in] keychain The keychain instance
 * @param[in] account_id Unique identifier for the account
 * @param[out] token Pointer to store the retrieved token
 * @return APP_ERROR_NONE on success, APP_ERROR_KEYCHAIN_NOT_FOUND if not found
 */
APP_NODISCARD app_error app_keychain_retrieve_token(app_keychain_t *keychain,
                                                    const char *account_id,
                                                    app_oauth_token_t **token);

/**
 * @brief Delete an OAuth token from the keychain
 *
 * Removes the token associated with the given account ID.
 *
 * @param[in] keychain The keychain instance
 * @param[in] account_id Unique identifier for the account
 * @return APP_ERROR_NONE on success, error code on failure
 */
APP_NODISCARD app_error app_keychain_delete_token(app_keychain_t *keychain,
                                                  const char *account_id);

/**
 * @brief List all account IDs with stored tokens
 *
 * Returns an array of account IDs that have tokens stored in the keychain.
 * The caller is responsible for freeing the returned array and strings.
 *
 * @param[in] keychain The keychain instance
 * @param[out] account_ids Array of account ID strings
 * @param[out] count Number of account IDs returned
 * @return APP_ERROR_NONE on success, error code on failure
 */
APP_NODISCARD app_error app_keychain_list_accounts(app_keychain_t *keychain,
                                                   char ***account_ids,
                                                   size_t *count);

/**
 * @brief Check if a token exists for an account
 *
 * @param[in] keychain The keychain instance
 * @param[in] account_id Unique identifier for the account
 * @param[out] exists True if token exists, false otherwise
 * @return APP_ERROR_NONE on success, error code on failure
 */
APP_NODISCARD app_error app_keychain_has_token(app_keychain_t *keychain,
                                               const char *account_id,
                                               bool *exists);

/**
 * @brief Clean up and free keychain resources
 *
 * @param[in] keychain The keychain instance to clean up
 * @return APP_ERROR_NONE on success, error code on failure
 */
APP_NODISCARD app_error app_keychain_cleanup(app_keychain_t *keychain);

/**
 * @brief Create a mock keychain for testing
 *
 * Creates an in-memory keychain implementation for unit tests.
 * Behaves identically to platform keychains but stores data in memory.
 *
 * @param[out] keychain Pointer to store the mock keychain instance
 * @return APP_ERROR_NONE on success, error code on failure
 */
APP_NODISCARD app_error app_keychain_create_mock(app_keychain_t **keychain);

/**
 * @brief Free an OAuth token structure
 *
 * Frees all memory associated with an OAuth token, including strings
 * and arrays.
 *
 * @param[in] token The token to free (can be NULL)
 */
void app_oauth_token_free(app_oauth_token_t *token);