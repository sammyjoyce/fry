#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../../core/config.h"
#include "core/error.h"
#include "core/auth/keychain.h"
#include "core/auth/oauth.h"
#include "utils/logging.h"
#include "utils/memory.h"
#include "cli/commands.h"

app_error cmd_auth_login(int argc, char **argv, app_config_t *config) {
  (void)argc;
  (void)argv;
  (void)config;

  printf("Select login method:\n");
  printf("  1. Claude Pro/Max (OAuth)\n");
  printf("  2. Create API Key (OAuth)\n");
  printf("  3. Enter API Key manually\n");
  printf("\nChoice (1-3): ");

  char choice[10];
  if (!fgets(choice, sizeof(choice), stdin)) {
    return APP_ERROR_IO;
  }

  int method = atoi(choice);

  if (method == 1 || method == 2) {
    // Generate PKCE challenge
    app_oauth_pkce_t *pkce;
    app_error err = app_oauth_pkce_generate(&pkce);
    if (err != APP_SUCCESS) {
      fprintf(stderr, "Failed to generate PKCE challenge\n");
      return err;
    }

    // Generate authorization URL
    char *url;
    const char *mode = (method == 1) ? "max" : "console";
    err = app_oauth_authorize_url(&url, pkce, mode);
    if (err != APP_SUCCESS) {
      app_oauth_pkce_destroy(pkce);
      return err;
    }

    printf("\nPlease visit this URL to authorize:\n%s\n\n", url);

// Try to open browser
#ifdef __APPLE__
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "open '%s'", url);
    system(cmd);
#elif defined(__linux__)
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "xdg-open '%s'", url);
    system(cmd);
#endif

    printf("Enter the authorization code: ");
    char code[256];
    if (!fgets(code, sizeof(code), stdin)) {
      app_free(url);
      app_oauth_pkce_destroy(pkce);
      return APP_ERROR_IO;
    }

    // Remove newline
    code[strcspn(code, "\n")] = '\0';

    // Exchange code for tokens
    app_oauth_token_t *token;
    err = app_oauth_exchange_code(&token, code, pkce->verifier);
    app_free(url);
    app_oauth_pkce_destroy(pkce);

    if (err != APP_SUCCESS) {
      fprintf(stderr, "Failed to exchange authorization code\n");
      return err;
    }

    // Create account name
    printf("Enter account name: ");
    char account_name[256];
    if (!fgets(account_name, sizeof(account_name), stdin)) {
      app_free(token->access_token);
      app_free(token->refresh_token);
      app_free(token);
      return APP_ERROR_IO;
    }
    account_name[strcspn(account_name, "\n")] = '\0';

    // Create and save account
    app_oauth_account_t *account;
    err = app_oauth_account_create(&account, account_name);
    if (err != APP_SUCCESS) {
      app_free(token->access_token);
      app_free(token->refresh_token);
      app_free(token);
      return err;
    }

    // Copy token to account
    account->token = *token;
    app_free(token);

    // Save encrypted account
    err = app_oauth_account_save(account);
    if (err != APP_SUCCESS) {
      app_oauth_account_destroy(account);
      return err;
    }

    // Also store in keychain for secure access
    app_keychain_t *keychain;
    err = app_keychain_init(&keychain);
    if (err == APP_SUCCESS) {
      err = app_keychain_store_token(keychain, account_name, &account->token);
      if (err != APP_SUCCESS) {
        LOG_WARNING("Failed to store token in keychain: %s", app_strerror(err));
      }
      app_keychain_cleanup(keychain);
    }

    // Set as default account if it's the first one
    app_oauth_account_t **accounts;
    size_t count;
    err = app_oauth_accounts_list(&accounts, &count);
    if (err == APP_SUCCESS && count == 1) {
      app_config_set_default_account(config, account_name);
      app_config_save(config);
    }
    if (accounts) {
      for (size_t i = 0; i < count; i++) {
        app_oauth_account_destroy(accounts[i]);
      }
      app_free(accounts);
    }

    printf("Login successful! Account '%s' saved.\n", account_name);
    app_oauth_account_destroy(account);
  } else if (method == 3) {
    printf("Enter API key (sk-ant-...): ");
    char api_key[256];
    if (!fgets(api_key, sizeof(api_key), stdin)) {
      return APP_ERROR_IO;
    }
    api_key[strcspn(api_key, "\n")] = '\0';

    // Validate API key format
    if (strncmp(api_key, "sk-ant-", 7) != 0) {
      fprintf(stderr, "Invalid API key format. Must start with 'sk-ant-'\n");
      app_secure_zero(api_key, sizeof(api_key));
      return APP_ERROR_INVALID_ARG;
    }

    printf("Enter account name: ");
    char account_name[256];
    if (!fgets(account_name, sizeof(account_name), stdin)) {
      return APP_ERROR_IO;
    }
    account_name[strcspn(account_name, "\n")] = '\0';

    // Create account with API key as access token
    app_oauth_account_t *account;
    app_error err = app_oauth_account_create(&account, account_name);
    if (err != APP_SUCCESS) {
      return err;
    }

    // Set up token (API keys don't expire or have refresh tokens)
    account->token.access_token = app_secure_strdup(api_key);
    account->token.refresh_token =
        app_secure_strdup("");      // Empty refresh token
    account->token.expires_at = 0;  // Never expires
    account->token.scopes_count = 0;
    account->token.scopes = NULL;

    // Save account
    err = app_oauth_account_save(account);
    if (err != APP_SUCCESS) {
      app_oauth_account_destroy(account);
      return err;
    }

    // Store in keychain
    app_keychain_t *keychain;
    err = app_keychain_init(&keychain);
    if (err == APP_SUCCESS) {
      err = app_keychain_store_token(keychain, account_name, &account->token);
      if (err != APP_SUCCESS) {
        LOG_WARNING("Failed to store API key in keychain: %s",
                    app_strerror(err));
      }
      app_keychain_cleanup(keychain);
    }

    printf("API key saved for account '%s'.\n", account_name);
    app_oauth_account_destroy(account);

    // Clear the API key from memory
    app_secure_zero(api_key, sizeof(api_key));
  } else {
    fprintf(stderr, "Invalid choice\n");
    return APP_ERROR_INVALID_ARG;
  }

  return APP_SUCCESS;
}

app_error cmd_auth_logout(int argc, char **argv, app_config_t *config) {
  const char *account_name = NULL;

  // Parse arguments
  if (argc > 0) {
    account_name = argv[0];
  } else {
    // Use default account
    account_name = app_config_get_default_account(config);
    if (!account_name) {
      fprintf(stderr, "No default account set. Specify account name.\n");
      return APP_ERROR_MISSING_ARG;
    }
  }

  // Load account
  char file_path[PATH_MAX];
  const char *accounts_dir = app_oauth_get_accounts_dir();
  if (!accounts_dir) {
    fprintf(stderr, "OAuth not initialized\n");
    return APP_ERROR_NOT_INITIALIZED;
  }

  snprintf(file_path, sizeof(file_path), "%s/%s.enc", accounts_dir,
           account_name);

  app_oauth_account_t *account;
  app_error err = app_oauth_account_load(&account, file_path);
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Account '%s' not found\n", account_name);
    return err;
  }

  // Remove from keychain
  app_keychain_t *keychain;
  err = app_keychain_init(&keychain);
  if (err == APP_SUCCESS) {
    err = app_keychain_delete_token(keychain, account_name);
    if (err != APP_SUCCESS && err != APP_ERROR_NOT_FOUND) {
      LOG_WARNING("Failed to remove token from keychain: %s",
                  app_strerror(err));
    }
    app_keychain_cleanup(keychain);
  }

  // Delete account file
  if (unlink(file_path) != 0) {
    fprintf(stderr, "Failed to delete account file: %s\n", strerror(errno));
    app_oauth_account_destroy(account);
    return APP_ERROR_IO;
  }

  // If this was the default account, clear it
  const char *default_account = app_config_get_default_account(config);
  if (default_account && strcmp(default_account, account_name) == 0) {
    app_config_set_default_account(config, NULL);
    app_config_save(config);
  }

  printf("Logged out of account '%s'\n", account_name);
  app_oauth_account_destroy(account);

  return APP_SUCCESS;
}

app_error cmd_auth_status(int argc, char **argv, app_config_t *config) {
  const char *account_name = NULL;

  // Parse arguments
  if (argc > 0) {
    account_name = argv[0];
  } else {
    // Use default account
    account_name = app_config_get_default_account(config);
  }

  if (!account_name) {
    printf("No account specified and no default account set.\n");
    printf("\nAvailable accounts:\n");

    // List all accounts
    app_oauth_account_t **accounts;
    size_t count;
    app_error err = app_oauth_accounts_list(&accounts, &count);
    if (err != APP_SUCCESS) {
      return err;
    }

    if (count == 0) {
      printf("  (none)\n");
    } else {
      for (size_t i = 0; i < count; i++) {
        printf("  - %s\n", accounts[i]->name);
        app_oauth_account_destroy(accounts[i]);
      }
    }

    if (accounts) {
      app_free(accounts);
    }

    return APP_SUCCESS;
  }

  // Load specific account
  char file_path[PATH_MAX];
  const char *accounts_dir = app_oauth_get_accounts_dir();
  if (!accounts_dir) {
    fprintf(stderr, "OAuth not initialized\n");
    return APP_ERROR_NOT_INITIALIZED;
  }

  snprintf(file_path, sizeof(file_path), "%s/%s.enc", accounts_dir,
           account_name);

  app_oauth_account_t *account;
  app_error err = app_oauth_account_load(&account, file_path);
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Account '%s' not found\n", account_name);
    return err;
  }

  // Display account status
  printf("Account: %s\n", account->name);

  // Check if it's the default
  const char *default_account = app_config_get_default_account(config);
  if (default_account && strcmp(default_account, account_name) == 0) {
    printf("Status: Default account\n");
  } else {
    printf("Status: Active\n");
  }

  // Token information
  if (account->token.access_token) {
    // Check token type
    if (strncmp(account->token.access_token, "sk-ant-api", 10) == 0) {
      printf("Auth Type: API Key\n");
      printf("Token: %.*s...%s\n", 15, account->token.access_token,
             account->token.access_token + strlen(account->token.access_token) -
                 4);
    } else if (strncmp(account->token.access_token, "sk-ant-oat", 10) == 0) {
      printf("Auth Type: OAuth\n");
      printf("Access Token: %.*s...%s\n", 15, account->token.access_token,
             account->token.access_token + strlen(account->token.access_token) -
                 4);

      // Check expiration
      if (app_oauth_token_is_expired(&account->token)) {
        printf("Token Status: Expired\n");
      } else if (account->token.expires_at > 0) {
        time_t expires = account->token.expires_at / 1000;  // Convert from ms
        printf("Token Expires: %s", ctime(&expires));
      } else {
        printf("Token Status: Valid\n");
      }
    }

    // Scopes
    if (account->token.scopes_count > 0) {
      printf("Scopes: ");
      for (size_t i = 0; i < account->token.scopes_count; i++) {
        printf("%s%s", account->token.scopes[i],
               i < account->token.scopes_count - 1 ? ", " : "\n");
      }
    }
  } else {
    printf("Status: No valid token\n");
  }

  // Check keychain status
  app_keychain_t *keychain;
  err = app_keychain_init(&keychain);
  if (err == APP_SUCCESS) {
    bool in_keychain = false;
    err = app_keychain_has_token(keychain, account_name, &in_keychain);
    if (err == APP_SUCCESS) {
      printf("Keychain: %s\n", in_keychain ? "Stored" : "Not stored");
    }
    app_keychain_cleanup(keychain);
  }

  app_oauth_account_destroy(account);
  return APP_SUCCESS;
}