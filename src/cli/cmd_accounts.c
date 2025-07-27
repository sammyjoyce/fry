#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../core/config.h"
#include "../core/error.h"
#include "../core/oauth.h"
#include "../utils/colors.h"
#include "../utils/logging.h"
#include "../utils/memory.h"
#include "commands.h"

app_error cmd_accounts_list(int argc, char **argv, app_config_t *config) {
  (void)argc;
  (void)argv;
  (void)config;

  app_oauth_account_t **accounts;
  size_t count;
  app_error err = app_oauth_accounts_list(&accounts, &count);
  if (err != APP_SUCCESS) {
    return err;
  }

  if (count == 0) {
    printf("No accounts configured. Use 'fry accounts add' to add one.\n");
    return APP_SUCCESS;
  }

  printf("Configured accounts:\n\n");
  for (size_t i = 0; i < count; i++) {
    app_oauth_account_t *acc = accounts[i];
    printf("  %s%s%s", acc->is_default ? APP_COLOR_GREEN : APP_COLOR_RESET,
           acc->name, APP_COLOR_RESET);
    if (acc->is_default) {
      printf(" (default)");
    }

    if (acc->is_disabled) {
      printf(" %s[DISABLED", APP_COLOR_RED);
      if (acc->disabled_until > 0) {
        time_t now = time(NULL);
        if (acc->disabled_until > now) {
          int hours = (acc->disabled_until - now) / 3600;
          int mins = ((acc->disabled_until - now) % 3600) / 60;
          printf(" - %dh %dm remaining", hours, mins);
        }
      }
      printf("]%s", APP_COLOR_RESET);
    }

    if (app_oauth_token_is_expired(&acc->token)) {
      printf(" %s[EXPIRED]%s", APP_COLOR_YELLOW, APP_COLOR_RESET);
    }

    printf("\n");
    app_oauth_account_destroy(acc);
  }

  app_free(accounts);
  return APP_SUCCESS;
}

app_error cmd_accounts_add(int argc, char **argv, app_config_t *config) {
  (void)config;
  static struct option long_options[] = {{"name", required_argument, 0, 'n'},
                                         {"file", required_argument, 0, 'f'},
                                         {"stdin", no_argument, 0, 's'},
                                         {0, 0, 0, 0}};

  char *name = NULL;
  char *file = NULL;
  bool use_stdin = false;

  int opt;
  optind = 1;  // Reset getopt
  while ((opt = getopt_long(argc, argv, "n:f:s", long_options, NULL)) != -1) {
    switch (opt) {
    case 'n':
      name = optarg;
      break;
    case 'f':
      file = optarg;
      break;
    case 's':
      use_stdin = true;
      break;
    default:
      fprintf(stderr,
              "Usage: fry accounts add --name NAME [--file FILE | --stdin]\n");
      return APP_ERROR_INVALID_ARG;
    }
  }

  if (!name) {
    fprintf(stderr, "Error: --name is required\n");
    return APP_ERROR_MISSING_ARG;
  }

  if (file) {
    // Import from file
    app_oauth_account_t *account;
    app_error err = app_oauth_import_credentials(&account, file, name);
    if (err != APP_SUCCESS) {
      fprintf(stderr, "Failed to import credentials: %s\n",
              app_error_string(err));
      return err;
    }

    printf("Successfully imported account '%s'\n", name);
    app_oauth_account_destroy(account);
  } else if (use_stdin) {
    // TODO: Read from stdin
    fprintf(stderr, "Reading from stdin not yet implemented\n");
    return APP_ERROR_NOT_IMPLEMENTED;
  } else {
    // Interactive mode
    printf("Adding account '%s' (interactive mode)\n", name);
    printf("Please paste your Claude Code credentials JSON:\n");
    // TODO: Implement interactive credential input
    fprintf(stderr, "Interactive mode not yet implemented\n");
    return APP_ERROR_NOT_IMPLEMENTED;
  }

  return APP_SUCCESS;
}

app_error cmd_accounts_remove(int argc, char **argv, app_config_t *config) {
  (void)config;

  if (argc < 1) {
    fprintf(stderr, "Usage: fry accounts rm ACCOUNT_NAME\n");
    return APP_ERROR_MISSING_ARG;
  }

  const char *name = argv[0];

  // Check if account exists
  char file_path[PATH_MAX];
  snprintf(file_path, sizeof(file_path), "%s/%s.enc",
           app_oauth_get_accounts_dir(), name);

  if (access(file_path, F_OK) != 0) {
    fprintf(stderr, "Account '%s' not found\n", name);
    return APP_ERROR_NOT_FOUND;
  }

  // Prompt for confirmation
  printf("Are you sure you want to remove account '%s'? [y/N] ", name);
  char response[10];
  if (!fgets(response, sizeof(response), stdin)) {
    return APP_ERROR_IO;
  }

  if (response[0] != 'y' && response[0] != 'Y') {
    printf("Cancelled\n");
    return APP_SUCCESS;
  }

  // Delete the file
  if (unlink(file_path) != 0) {
    fprintf(stderr, "Failed to remove account file: %s\n", strerror(errno));
    return APP_ERROR_IO;
  }

  printf("Account '%s' removed\n", name);

  // If this was the default account, clear it
  const char *default_account = app_config_get_default_account(config);
  if (default_account && strcmp(default_account, name) == 0) {
    app_config_set_default_account(config, NULL);
    (void)app_config_save(config);
  }

  return APP_SUCCESS;
}

app_error cmd_accounts_use(int argc, char **argv, app_config_t *config) {
  if (argc < 1) {
    fprintf(stderr, "Usage: fry accounts use ACCOUNT_NAME\n");
    return APP_ERROR_MISSING_ARG;
  }

  const char *name = argv[0];

  // Verify account exists
  char file_path[PATH_MAX];
  snprintf(file_path, sizeof(file_path), "%s/%s.enc",
           app_oauth_get_accounts_dir(), name);

  if (access(file_path, F_OK) != 0) {
    fprintf(stderr, "Account '%s' not found\n", name);
    return APP_ERROR_NOT_FOUND;
  }

  // Set as default in config
  app_config_set_default_account(config, name);
  app_error err = app_config_save(config);
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Failed to save config: %s\n", app_error_string(err));
    return err;
  }

  printf("Set '%s' as default account\n", name);
  return APP_SUCCESS;
}

app_error cmd_accounts_export(int argc, char **argv, app_config_t *config) {
  (void)config;

  if (argc < 1) {
    fprintf(stderr, "Usage: fry accounts export ACCOUNT_NAME\n");
    return APP_ERROR_MISSING_ARG;
  }

  const char *name = argv[0];

  // Load account
  char file_path[PATH_MAX];
  snprintf(file_path, sizeof(file_path), "%s/%s.enc",
           app_oauth_get_accounts_dir(), name);

  app_oauth_account_t *account;
  app_error err = app_oauth_account_load(&account, file_path);
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Failed to load account '%s': %s\n", name,
            app_error_string(err));
    return err;
  }

  // Export as JSON
  char *json_output;
  err = app_oauth_export_credentials(account, &json_output);
  app_oauth_account_destroy(account);

  if (err != APP_SUCCESS) {
    fprintf(stderr, "Failed to export credentials: %s\n",
            app_error_string(err));
    return err;
  }

  printf("%s\n", json_output);
  app_free(json_output);

  return APP_SUCCESS;
}

app_error cmd_accounts_disable(int argc, char **argv, app_config_t *config) {
  (void)config;

  static struct option long_options[] = {{"until", required_argument, 0, 'u'},
                                         {0, 0, 0, 0}};

  char *until_str = NULL;

  int opt;
  optind = 2;  // Skip noun and verb
  while ((opt = getopt_long(argc, argv, "u:", long_options, NULL)) != -1) {
    switch (opt) {
    case 'u':
      until_str = optarg;
      break;
    default:
      fprintf(stderr, "Usage: fry accounts disable ACCOUNT [--until TIME]\n");
      return APP_ERROR_INVALID_ARG;
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "Error: Account name required\n");
    return APP_ERROR_MISSING_ARG;
  }

  const char *name = argv[optind];
  time_t until = 0;

  if (until_str) {
    // Parse duration (e.g., "5h", "30m", "14:30")
    if (strchr(until_str, ':')) {
      // Time format (HH:MM)
      // TODO: Parse absolute time
    } else if (strchr(until_str, 'h') || strchr(until_str, 'm')) {
      // Duration format
      int hours = 0, minutes = 0;
      sscanf(until_str, "%dh", &hours);
      sscanf(until_str, "%dm", &minutes);
      until = time(NULL) + (hours * 3600) + (minutes * 60);
    }
  }

  app_error err = app_oauth_account_disable(name, until);
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Failed to disable account: %s\n", app_error_string(err));
    return err;
  }

  if (until > 0) {
    printf("Disabled account '%s' until ", name);
    struct tm *tm = localtime(&until);
    printf("%02d:%02d\n", tm->tm_hour, tm->tm_min);
  } else {
    printf("Disabled account '%s' indefinitely\n", name);
  }

  return APP_SUCCESS;
}

app_error cmd_accounts_enable(int argc, char **argv, app_config_t *config) {
  (void)config;

  if (argc < 3) {
    fprintf(stderr, "Usage: fry accounts enable ACCOUNT_NAME\n");
    return APP_ERROR_MISSING_ARG;
  }

  const char *name = argv[2];
  app_error err = app_oauth_account_enable(name);
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Failed to enable account: %s\n", app_error_string(err));
    return err;
  }

  printf("Enabled account '%s'\n", name);
  return APP_SUCCESS;
}

app_error cmd_accounts_rename(int argc, char **argv, app_config_t *config) {
  if (argc < 2) {
    fprintf(stderr, "Usage: fry accounts rename OLD_NAME NEW_NAME\n");
    return APP_ERROR_MISSING_ARG;
  }

  const char *old_name = argv[0];
  const char *new_name = argv[1];

  // Check if old account exists
  char old_path[PATH_MAX];
  snprintf(old_path, sizeof(old_path), "%s/%s.enc",
           app_oauth_get_accounts_dir(), old_name);

  if (access(old_path, F_OK) != 0) {
    fprintf(stderr, "Account '%s' not found\n", old_name);
    return APP_ERROR_NOT_FOUND;
  }

  // Check if new name already exists
  char new_path[PATH_MAX];
  snprintf(new_path, sizeof(new_path), "%s/%s.enc",
           app_oauth_get_accounts_dir(), new_name);

  if (access(new_path, F_OK) == 0) {
    fprintf(stderr, "Account '%s' already exists\n", new_name);
    return APP_ERROR_INVALID_ARG;
  }

  // Rename the file
  if (rename(old_path, new_path) != 0) {
    fprintf(stderr, "Failed to rename account: %s\n", strerror(errno));
    return APP_ERROR_IO;
  }

  printf("Renamed account '%s' to '%s'\n", old_name, new_name);

  // Update default account if needed
  const char *default_account = app_config_get_default_account(config);
  if (default_account && strcmp(default_account, old_name) == 0) {
    app_config_set_default_account(config, new_name);
    (void)app_config_save(config);
  }

  return APP_SUCCESS;
}

app_error cmd_accounts_status(int argc, char **argv, app_config_t *config) {
  (void)argc;
  (void)argv;

  // Similar to list but with more detail
  return cmd_accounts_list(argc, argv, config);
}