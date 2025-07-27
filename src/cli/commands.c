#include "commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/error.h"
#include "../utils/logging.h"
#include "../utils/memory.h"

#define MAX_COMMANDS 64

static app_command_t *g_commands[MAX_COMMANDS];
static size_t g_command_count = 0;

app_error app_commands_init(void) {
  g_command_count = 0;
  memset(g_commands, 0, sizeof(g_commands));

  // Register all commands
  static const app_command_t commands[] = {
      // Account commands
      {"accounts", "list", "List all stored accounts", cmd_accounts_list},
      {"accounts", "add", "Add new account", cmd_accounts_add},
      {"accounts", "rm", "Remove account", cmd_accounts_remove},
      {"accounts", "rename", "Rename an account", cmd_accounts_rename},
      {"accounts", "use", "Set default account", cmd_accounts_use},
      {"accounts", "export", "Export account tokens", cmd_accounts_export},
      {"accounts", "disable", "Disable account temporarily",
       cmd_accounts_disable},
      {"accounts", "enable", "Re-enable account", cmd_accounts_enable},
      {"accounts", "status", "Show account quota status", cmd_accounts_status},

      // Session commands
      {"sessions", "list", "Show running and saved sessions",
       cmd_sessions_list},
      {"sessions", "start", "Start new session", cmd_sessions_start},
      {"sessions", "attach", "Attach to running session", cmd_sessions_attach},
      {"sessions", "save", "Save session to disk", cmd_sessions_save},
      {"sessions", "restore", "Restore saved session", cmd_sessions_restore},
      {"sessions", "kill", "Terminate session", cmd_sessions_kill},

      // Multiplexer commands
      {"mux", "up", "Launch multiplexer", cmd_mux_up},
      {"mux", "ls", "List active multiplexers", cmd_mux_ls},
      {"mux", "attach", "Attach to multiplexer", cmd_mux_attach},
      {"mux", "send", "Send command to all panes", cmd_mux_send},
      {"mux", "down", "Close multiplexer", cmd_mux_down},

      // Config commands
      {"config", "show", "Display configuration", cmd_config_show},
      {"config", "set", "Update a setting", cmd_config_set},
      {"config", "edit", "Open config in editor", cmd_config_edit},
      {"config", "reset", "Reset to defaults", cmd_config_reset},

      // Token commands
      {"tokens", "inspect", "Decode and display token info",
       cmd_tokens_inspect},
      {"tokens", "refresh", "Force token refresh", cmd_tokens_refresh},
      {"tokens", "expire", "Mark token as expired", cmd_tokens_expire},

      // Auth commands
      {"auth", "login", "Authenticate with Claude", cmd_auth_login},
      {"auth", "logout", "Remove authentication", cmd_auth_logout},
      {"auth", "status", "Show authentication status", cmd_auth_status},
  };

  for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    app_error err = app_commands_register(&commands[i]);
    if (err != APP_SUCCESS) {
      return err;
    }
  }

  return APP_SUCCESS;
}

void app_commands_cleanup(void) {
  g_command_count = 0;
  memset(g_commands, 0, sizeof(g_commands));
}

app_error app_commands_register(const app_command_t *command) {
  if (!command || g_command_count >= MAX_COMMANDS) {
    return APP_ERROR_INVALID_PARAM;
  }

  g_commands[g_command_count++] = (app_command_t *)command;
  return APP_SUCCESS;
}

app_error app_commands_dispatch(const char *noun, const char *verb, int argc,
                                char **argv, app_config_t *config) {
  if (!noun || !verb) {
    return APP_ERROR_INVALID_PARAM;
  }

  for (size_t i = 0; i < g_command_count; i++) {
    if (strcmp(g_commands[i]->noun, noun) == 0 &&
        strcmp(g_commands[i]->verb, verb) == 0) {
      return g_commands[i]->handler(argc, argv, config);
    }
  }

  LOG_ERROR("Unknown command: %s %s", noun, verb);
  return APP_ERROR_UNKNOWN_COMMAND;
}

void app_commands_list(void) {
  printf("Available commands:\n\n");

  const char *last_noun = NULL;
  for (size_t i = 0; i < g_command_count; i++) {
    if (!last_noun || strcmp(last_noun, g_commands[i]->noun) != 0) {
      if (last_noun)
        printf("\n");
      printf("%s:\n", g_commands[i]->noun);
      last_noun = g_commands[i]->noun;
    }
    printf("  %-12s %s\n", g_commands[i]->verb, g_commands[i]->description);
  }
}