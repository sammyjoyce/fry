#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "core/config.h"
#include "core/error.h"
#include "core/keychain.h"
#include "../core/oauth.h"
#include "../core/session.h"
#include "../utils/logging.h"
#include "../utils/memory.h"
#include "commands.h"

// Session command stubs
app_error cmd_sessions_list(int argc, char **argv, app_config_t *config) {
  (void)config;

  bool show_all = false;
  bool verbose = false;

  // Parse options
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--all") == 0 || strcmp(argv[i], "-a") == 0) {
      show_all = true;
    } else if (strcmp(argv[i], "--verbose") == 0 ||
               strcmp(argv[i], "-v") == 0) {
      verbose = true;
    }
  }

  // Initialize session manager
  app_session_manager_t *manager;
  app_error err = app_session_manager_create(&manager);
  if (err != APP_SUCCESS) {
    return err;
  }

  // List sessions
  app_session_info_t **sessions = NULL;
  size_t count;
  err = app_session_list(manager, &sessions, &count);
  if (err != APP_SUCCESS) {
    app_session_manager_destroy(manager);
    return err;
  }

  if (count == 0) {
    printf("No active sessions found.\n");
    if (!show_all) {
      printf("Use --all to show saved sessions as well.\n");
    }
  } else {
    printf("Active Claude Code sessions:\n\n");

    for (size_t i = 0; i < count; i++) {
      app_session_info_t *info = sessions[i];

      printf("  %s\n", info->id);

      if (verbose) {
        printf("    PID: %d\n", info->pid);
        printf("    Status: %s\n", info->is_active ? "Active" : "Inactive");
        printf("    Started: %s", ctime(&info->created_at));
        if (info->project_path) {
          printf("    Project: %s\n", info->project_path);
        }
        if (info->account_name) {
          printf("    Account: %s\n", info->account_name);
        }
        printf("\n");
      }
    }

    if (!verbose) {
      printf("\nUse --verbose for more details.\n");
    }
  }

  // Clean up
  if (sessions) {
    for (size_t i = 0; i < count; i++) {
      app_session_info_destroy(sessions[i]);
    }
    app_free(sessions);
  }
  app_session_manager_destroy(manager);

  return APP_SUCCESS;
}

app_error cmd_sessions_start(int argc, char **argv, app_config_t *config) {
  const char *project_path = NULL;
  const char *account_name = NULL;
  const char *model = NULL;
  bool detached = false;

  // Parse arguments
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--account") == 0 && i + 1 < argc) {
      account_name = argv[++i];
    } else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
      model = argv[++i];
    } else if (strcmp(argv[i], "--detached") == 0 ||
               strcmp(argv[i], "-d") == 0) {
      detached = true;
    } else if (argv[i][0] != '-') {
      project_path = argv[i];
    }
  }

  // Use default account if not specified
  if (!account_name) {
    account_name = app_config_get_default_account(config);
    if (!account_name) {
      fprintf(stderr, "No account specified and no default account set.\n");
      fprintf(stderr, "Use 'fry auth login' to authenticate first.\n");
      return APP_ERROR_NOT_AUTHENTICATED;
    }
  }

  // Use current directory if no project path specified
  char cwd[PATH_MAX];
  if (!project_path) {
    if (getcwd(cwd, sizeof(cwd))) {
      project_path = cwd;
    } else {
      project_path = ".";
    }
  }

  // Initialize session manager
  app_session_manager_t *manager;
  app_error err = app_session_manager_create(&manager);
  if (err != APP_SUCCESS) {
    return err;
  }

  // Create session options
  app_session_options_t options = {
      .project_path = project_path,
      .account_name = account_name,
      .model = model,
      .env_vars = NULL,
      .env_count = 0,
      .detached = detached,
  };

  // Start session
  app_session_t *session;
  err = app_session_start(manager, &session, &options);
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Failed to start session: %s\n", app_strerror(err));
    app_session_manager_destroy(manager);
    return err;
  }

  printf("Started Claude Code session: %s\n", session->id);
  printf("Project: %s\n", project_path);
  printf("Account: %s\n", account_name);

  if (detached) {
    printf("\nSession running in background (PID: %d)\n", session->pid);
    printf("Use 'fry sessions attach %s' to connect.\n", session->id);
  } else {
    printf("\nAttaching to session...\n");

    // Attach to session
    err = app_session_attach(session);
    if (err != APP_SUCCESS) {
      fprintf(stderr, "Failed to attach to session: %s\n", app_strerror(err));
    }
  }

  app_session_destroy(session);
  app_session_manager_destroy(manager);

  return err;
}

app_error cmd_sessions_attach(int argc, char **argv, app_config_t *config) {
  (void)config;

  if (argc < 1) {
    fprintf(stderr, "Usage: fry sessions attach <session-id>\n");
    return APP_ERROR_MISSING_ARG;
  }

  const char *session_id = argv[0];

  // Initialize session manager
  app_session_manager_t *manager;
  app_error err = app_session_manager_create(&manager);
  if (err != APP_SUCCESS) {
    return err;
  }

  // Find session
  app_session_t *session;
  err = app_session_find(manager, &session, session_id);
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Session '%s' not found\n", session_id);
    app_session_manager_destroy(manager);
    return err;
  }

  // Check if session is active
  if (!app_session_is_active(session)) {
    fprintf(stderr, "Session '%s' is not active\n", session_id);
    app_session_destroy(session);
    app_session_manager_destroy(manager);
    return APP_ERROR_SESSION_INACTIVE;
  }

  printf("Attaching to session '%s'...\n", session_id);

  // Attach to session
  err = app_session_attach(session);
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Failed to attach to session: %s\n", app_strerror(err));
  }

  app_session_destroy(session);
  app_session_manager_destroy(manager);

  return err;
}

app_error cmd_sessions_save(int argc, char **argv, app_config_t *config) {
  (void)config;

  if (argc < 1) {
    fprintf(stderr, "Usage: fry sessions save <session-id> [name]\n");
    return APP_ERROR_MISSING_ARG;
  }

  const char *session_id = argv[0];
  const char *save_name = argc > 1 ? argv[1] : NULL;

  // Initialize session manager
  app_session_manager_t *manager;
  app_error err = app_session_manager_create(&manager);
  if (err != APP_SUCCESS) {
    return err;
  }

  // Find session
  app_session_t *session;
  err = app_session_find(manager, &session, session_id);
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Session '%s' not found\n", session_id);
    app_session_manager_destroy(manager);
    return err;
  }

  // Save session
  err = app_session_save(session, save_name);
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Failed to save session: %s\n", app_strerror(err));
    app_session_destroy(session);
    app_session_manager_destroy(manager);
    return err;
  }

  printf("Session saved successfully");
  if (save_name) {
    printf(" as '%s'", save_name);
  }
  printf(".\n");

  app_session_destroy(session);
  app_session_manager_destroy(manager);
  return err;
}

app_error cmd_sessions_restore(int argc, char **argv, app_config_t *config) {
  (void)config;

  if (argc < 1) {
    fprintf(stderr, "Usage: fry sessions restore <save-name>\n");
    return APP_ERROR_MISSING_ARG;
  }

  const char *save_name = argv[0];
  bool detached = false;

  // Parse options
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--detached") == 0 || strcmp(argv[i], "-d") == 0) {
      detached = true;
    }
  }

  // Initialize session manager
  app_session_manager_t *manager;
  app_error err = app_session_manager_create(&manager);
  if (err != APP_SUCCESS) {
    return err;
  }

  // Restore session
  app_session_t *session;
  err = app_session_restore(manager, &session, save_name);
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Failed to restore session '%s': %s\n", save_name,
            app_strerror(err));
    app_session_manager_destroy(manager);
    return err;
  }

  printf("Restored session: %s\n", session->id);

  if (detached) {
    printf("Session running in background (PID: %d)\n", session->pid);
    printf("Use 'fry sessions attach %s' to connect.\n", session->id);
  } else {
    printf("\nAttaching to session...\n");

    // Attach to session
    err = app_session_attach(session);
    if (err != APP_SUCCESS) {
      fprintf(stderr, "Failed to attach to session: %s\n", app_strerror(err));
    }
  }

  app_session_destroy(session);
  app_session_manager_destroy(manager);
  return err;
}

app_error cmd_sessions_kill(int argc, char **argv, app_config_t *config) {
  (void)config;

  if (argc < 1) {
    fprintf(stderr, "Usage: fry sessions kill <session-id> [session-id...]\n");
    return APP_ERROR_MISSING_ARG;
  }

  // Initialize session manager
  app_session_manager_t *manager;
  app_error err = app_session_manager_create(&manager);
  if (err != APP_SUCCESS) {
    return err;
  }

  int killed = 0;
  int failed = 0;

  // Kill each specified session
  for (int i = 0; i < argc; i++) {
    const char *session_id = argv[i];

    // Find session
    app_session_t *session;
    err = app_session_find(manager, &session, session_id);
    if (err != APP_SUCCESS) {
      fprintf(stderr, "Session '%s' not found\n", session_id);
      failed++;
      continue;
    }

    // Stop session
    err = app_session_stop(manager, session);
    if (err != APP_SUCCESS) {
      fprintf(stderr, "Failed to kill session '%s': %s\n", session_id,
              app_strerror(err));
      failed++;
    } else {
      printf("Killed session '%s'\n", session_id);
      killed++;
    }

    app_session_destroy(session);
  }

  app_session_manager_destroy(manager);

  if (killed > 0) {
    printf("\nKilled %d session%s.\n", killed, killed == 1 ? "" : "s");
  }

  if (failed > 0) {
    fprintf(stderr, "Failed to kill %d session%s.\n", failed,
            failed == 1 ? "" : "s");
    return APP_ERROR_PARTIAL;
  }

  return APP_SUCCESS;
}

// Multiplexer command stubs are now in cmd_mux.c

// Config command stubs
app_error cmd_config_show(int argc, char **argv, app_config_t *config) {
  (void)argc;
  (void)argv;

  printf("Configuration:\n");
  printf("  Default account: %s\n",
         app_config_get_default_account(config) ?: "(none)");
  printf("  Debug mode: %s\n", app_config_is_debug(config) ? "yes" : "no");
  printf("  Quiet mode: %s\n", app_config_is_quiet(config) ? "yes" : "no");
  printf("  JSON output: %s\n",
         app_config_is_json_output(config) ? "yes" : "no");
  printf("  No color: %s\n", app_config_is_no_color(config) ? "yes" : "no");

  const char *config_file = app_config_get_config_file(config);
  if (config_file) {
    printf("  Config file: %s\n", config_file);
  }

  return APP_SUCCESS;
}

app_error cmd_config_set(int argc, char **argv, app_config_t *config) {
  if (argc < 2) {
    fprintf(stderr, "Usage: fry config set KEY VALUE\n");
    return APP_ERROR_MISSING_ARG;
  }

  const char *key = argv[0];
  const char *value = argv[1];

  if (strcmp(key, "default_account") == 0) {
    app_config_set_default_account(config, value);
    app_error err = app_config_save(config);
    if (err != APP_SUCCESS) {
      fprintf(stderr, "Failed to save config: %s\n", app_strerror(err));
      return err;
    }
    printf("Set default_account to '%s'\n", value);
  } else {
    fprintf(stderr, "Unknown configuration key: %s\n", key);
    return APP_ERROR_INVALID_ARG;
  }

  return APP_SUCCESS;
}

app_error cmd_config_edit(int argc, char **argv, app_config_t *config) {
  (void)argc;
  (void)argv;
  (void)config;

  const char *editor = getenv("EDITOR");
  if (!editor) {
    editor = "vi";
  }

  char *home = getenv("HOME");
  if (!home) {
    return APP_ERROR_ENV;
  }

  char config_path[PATH_MAX];
  snprintf(config_path, sizeof(config_path), "%s/.fry/config.json", home);

  // Ensure config exists
  app_error err = app_config_save(config);
  if (err != APP_SUCCESS) {
    return err;
  }

  // Open in editor
  char command[PATH_MAX + 256];
  snprintf(command, sizeof(command), "%s '%s'", editor, config_path);
  system(command);

  // Reload config
  return app_config_load_file(config, config_path);
}

app_error cmd_config_reset(int argc, char **argv, app_config_t *config) {
  (void)argc;
  (void)argv;

  // Reset to defaults
  app_config_set_default_account(config, NULL);
  app_config_set_debug(config, false);
  app_config_set_quiet(config, false);
  app_config_set_json_output(config, false);
  app_config_set_no_color(config, false);

  // Save
  app_error err = app_config_save(config);
  if (err != APP_SUCCESS) {
    return err;
  }

  printf("Configuration reset to defaults\n");
  return APP_SUCCESS;
}

// Token command stubs
app_error cmd_tokens_inspect(int argc, char **argv, app_config_t *config) {
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

  // Initialize OAuth
  app_error err = app_oauth_init();
  if (err != APP_SUCCESS) {
    return err;
  }

  // Load account
  char file_path[PATH_MAX];
  const char *accounts_dir = app_oauth_get_accounts_dir();
  if (!accounts_dir) {
    fprintf(stderr, "OAuth not initialized\n");
    app_oauth_cleanup();
    return APP_ERROR_NOT_INITIALIZED;
  }

  snprintf(file_path, sizeof(file_path), "%s/%s.enc", accounts_dir,
           account_name);

  app_oauth_account_t *account;
  err = app_oauth_account_load(&account, file_path);
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Account '%s' not found\n", account_name);
    app_oauth_cleanup();
    return err;
  }

  // Display token information
  printf("Account: %s\n", account->name);
  printf("\n");

  if (account->token.access_token) {
    // Decode token type
    if (strncmp(account->token.access_token, "sk-ant-api", 10) == 0) {
      printf("Token Type: API Key\n");
      printf("Full Token: %s\n", account->token.access_token);
      printf("Expires: Never\n");
    } else if (strncmp(account->token.access_token, "sk-ant-oat", 10) == 0) {
      printf("Token Type: OAuth Access Token\n");
      printf("Access Token: %s\n", account->token.access_token);

      if (account->token.refresh_token) {
        printf("Refresh Token: %s\n", account->token.refresh_token);
      }

      if (account->token.expires_at > 0) {
        time_t expires = account->token.expires_at / 1000;
        printf("Expires: %s", ctime(&expires));

        // Check if expired
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        int64_t now_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

        if (now_ms >= account->token.expires_at) {
          printf("Status: EXPIRED\n");
        } else {
          int64_t remaining_ms = account->token.expires_at - now_ms;
          int64_t remaining_min = remaining_ms / 60000;
          printf("Status: Valid (expires in %lld minutes)\n",
                 (long long)remaining_min);
        }
      } else {
        printf("Expires: Unknown\n");
      }
    } else {
      printf("Token Type: Unknown\n");
      printf("Token: %s\n", account->token.access_token);
    }

    // Display scopes
    if (account->token.scopes_count > 0) {
      printf("\nScopes:\n");
      for (size_t i = 0; i < account->token.scopes_count; i++) {
        printf("  - %s\n", account->token.scopes[i]);
      }
    }

    // Display subscription type if available
    if (account->token.subscription_type) {
      printf("\nSubscription: %s\n", account->token.subscription_type);
    }
  } else {
    printf("No token stored for this account\n");
  }

  app_oauth_account_destroy(account);
  app_oauth_cleanup();
  return APP_SUCCESS;
}

app_error cmd_tokens_refresh(int argc, char **argv, app_config_t *config) {
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

  // Initialize OAuth
  app_error err = app_oauth_init();
  if (err != APP_SUCCESS) {
    return err;
  }

  // Load account
  char file_path[PATH_MAX];
  const char *accounts_dir = app_oauth_get_accounts_dir();
  if (!accounts_dir) {
    fprintf(stderr, "OAuth not initialized\n");
    app_oauth_cleanup();
    return APP_ERROR_NOT_INITIALIZED;
  }

  snprintf(file_path, sizeof(file_path), "%s/%s.enc", accounts_dir,
           account_name);

  app_oauth_account_t *account;
  err = app_oauth_account_load(&account, file_path);
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Account '%s' not found\n", account_name);
    app_oauth_cleanup();
    return err;
  }

  // Check if it's an API key (can't refresh)
  if (strncmp(account->token.access_token, "sk-ant-api", 10) == 0) {
    fprintf(stderr, "API keys cannot be refreshed\n");
    app_oauth_account_destroy(account);
    app_oauth_cleanup();
    return APP_ERROR_INVALID_OPERATION;
  }

  // Check if we have a refresh token
  if (!account->token.refresh_token ||
      strlen(account->token.refresh_token) == 0) {
    fprintf(stderr, "No refresh token available for this account\n");
    app_oauth_account_destroy(account);
    app_oauth_cleanup();
    return APP_ERROR_INVALID_TOKEN;
  }

  printf("Refreshing token for account '%s'...\n", account_name);

  // Refresh the token
  err = app_oauth_token_refresh(&account->token, NULL);
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Failed to refresh token: %s\n", app_strerror(err));
    app_oauth_account_destroy(account);
    app_oauth_cleanup();
    return err;
  }

  // Save updated account
  err = app_oauth_account_save(account);
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Failed to save refreshed token: %s\n", app_strerror(err));
    app_oauth_account_destroy(account);
    app_oauth_cleanup();
    return err;
  }

  // Update keychain
  app_keychain_t *keychain;
  err = app_keychain_init(&keychain);
  if (err == APP_SUCCESS) {
    err = app_keychain_store_token(keychain, account_name, &account->token);
    if (err != APP_SUCCESS) {
      LOG_WARNING("Failed to update token in keychain: %s", app_strerror(err));
    }
    app_error cleanup_err = app_keychain_cleanup(keychain);
    if (cleanup_err != APP_SUCCESS) {
      LOG_WARNING("Failed to cleanup keychain: %s", app_strerror(cleanup_err));
    }
  }

  printf("Token refreshed successfully!\n");

  // Display new expiration
  if (account->token.expires_at > 0) {
    time_t expires = account->token.expires_at / 1000;
    printf("New expiration: %s", ctime(&expires));
  }

  app_oauth_account_destroy(account);
  app_oauth_cleanup();
  return APP_SUCCESS;
}

app_error cmd_tokens_expire(int argc, char **argv, app_config_t *config) {
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

  // Initialize OAuth
  app_error err = app_oauth_init();
  if (err != APP_SUCCESS) {
    return err;
  }

  // Load account
  char file_path[PATH_MAX];
  const char *accounts_dir = app_oauth_get_accounts_dir();
  if (!accounts_dir) {
    fprintf(stderr, "OAuth not initialized\n");
    app_oauth_cleanup();
    return APP_ERROR_NOT_INITIALIZED;
  }

  snprintf(file_path, sizeof(file_path), "%s/%s.enc", accounts_dir,
           account_name);

  app_oauth_account_t *account;
  err = app_oauth_account_load(&account, file_path);
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Account '%s' not found\n", account_name);
    app_oauth_cleanup();
    return err;
  }

  // Check if it's an API key (never expires)
  if (strncmp(account->token.access_token, "sk-ant-api", 10) == 0) {
    fprintf(stderr, "API keys never expire\n");
    app_oauth_account_destroy(account);
    app_oauth_cleanup();
    return APP_ERROR_INVALID_OPERATION;
  }

  // Mark token as expired
  account->token.expires_at = 1;  // Set to 1ms past epoch (definitely expired)

  // Save updated account
  err = app_oauth_account_save(account);
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Failed to save account: %s\n", app_strerror(err));
    app_oauth_account_destroy(account);
    app_oauth_cleanup();
    return err;
  }

  printf("Token for account '%s' marked as expired\n", account_name);
  printf("Use 'fry tokens refresh %s' to get a new token\n", account_name);

  app_oauth_account_destroy(account);
  app_oauth_cleanup();
  return APP_SUCCESS;
}