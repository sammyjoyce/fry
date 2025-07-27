#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "error.h"
#include "types.h"

typedef struct {
  char *access_token;
  char *refresh_token;
  int64_t expires_at;  // Unix timestamp in milliseconds
  char **scopes;
  size_t scopes_count;
  char *subscription_type;
} app_oauth_token_t;

typedef struct {
  char *challenge;
  char *verifier;
  char *state;
} app_oauth_pkce_t;

typedef struct {
  char *name;
  app_oauth_token_t token;
  bool is_default;
  bool is_disabled;
  time_t disabled_until;  // Unix timestamp when to re-enable
  char *file_path;        // Path to encrypted token file
} app_oauth_account_t;

typedef struct {
  char *token_endpoint;
  char *auth_endpoint;
  char *redirect_uri;
  char **scopes;
  size_t scopes_count;
} app_oauth_config_t;

// Account management
APP_NODISCARD app_error app_oauth_account_create(app_oauth_account_t **account,
                                                 const char *name);
APP_NODISCARD app_error app_oauth_account_load(app_oauth_account_t **account,
                                               const char *file_path);
APP_NODISCARD app_error
app_oauth_account_save(const app_oauth_account_t *account);
void app_oauth_account_destroy(app_oauth_account_t *account);

// Token operations
APP_NODISCARD app_error app_oauth_token_refresh(
    app_oauth_token_t *token, const app_oauth_config_t *config);
APP_NODISCARD app_error
app_oauth_token_validate(const app_oauth_token_t *token);
bool app_oauth_token_is_expired(const app_oauth_token_t *token);
APP_NODISCARD app_error app_oauth_token_decrypt(app_oauth_token_t **token,
                                                const char *encrypted_data,
                                                size_t data_len);
APP_NODISCARD app_error app_oauth_token_encrypt(char **encrypted_data,
                                                size_t *data_len,
                                                const app_oauth_token_t *token);
void app_oauth_token_free(app_oauth_token_t *token);

// Account list operations
APP_NODISCARD app_error app_oauth_accounts_list(app_oauth_account_t ***accounts,
                                                size_t *count);
APP_NODISCARD app_error app_oauth_account_set_default(const char *name);
APP_NODISCARD app_error app_oauth_account_disable(const char *name,
                                                  time_t until);
APP_NODISCARD app_error app_oauth_account_enable(const char *name);

// Import/Export
APP_NODISCARD app_error
app_oauth_import_credentials(app_oauth_account_t **account,
                             const char *json_path, const char *account_name);
APP_NODISCARD app_error app_oauth_export_credentials(
    const app_oauth_account_t *account, char **json_output);

// OAuth flow
APP_NODISCARD app_error app_oauth_pkce_generate(app_oauth_pkce_t **pkce);
void app_oauth_pkce_destroy(app_oauth_pkce_t *pkce);
APP_NODISCARD app_error app_oauth_authorize_url(char **url,
                                                const app_oauth_pkce_t *pkce,
                                                const char *mode);
APP_NODISCARD app_error app_oauth_exchange_code(app_oauth_token_t **token,
                                                const char *code,
                                                const char *verifier);

// Utility
const char *app_oauth_get_accounts_dir(void);
APP_NODISCARD app_error app_oauth_init(void);
void app_oauth_cleanup(void);