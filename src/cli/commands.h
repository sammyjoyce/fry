#pragma once

#include "../core/error.h"
#include "../core/types.h"

// Forward declarations
typedef struct app_config app_config_t;

// Command handler function type
typedef app_error (*command_handler_t)(int argc, char **argv,
                                       app_config_t *config);

// Command structure
typedef struct {
  const char *noun;
  const char *verb;
  const char *description;
  command_handler_t handler;
} app_command_t;

// Command registration and dispatch
APP_NODISCARD app_error app_commands_init(void);
void app_commands_cleanup(void);
APP_NODISCARD app_error app_commands_register(const app_command_t *command);
APP_NODISCARD app_error app_commands_dispatch(const char *noun,
                                              const char *verb, int argc,
                                              char **argv,
                                              app_config_t *config);
void app_commands_list(void);

// Account commands
APP_NODISCARD app_error cmd_accounts_list(int argc, char **argv,
                                          app_config_t *config);
APP_NODISCARD app_error cmd_accounts_add(int argc, char **argv,
                                         app_config_t *config);
APP_NODISCARD app_error cmd_accounts_remove(int argc, char **argv,
                                            app_config_t *config);
APP_NODISCARD app_error cmd_accounts_rename(int argc, char **argv,
                                            app_config_t *config);
APP_NODISCARD app_error cmd_accounts_use(int argc, char **argv,
                                         app_config_t *config);
APP_NODISCARD app_error cmd_accounts_export(int argc, char **argv,
                                            app_config_t *config);
APP_NODISCARD app_error cmd_accounts_disable(int argc, char **argv,
                                             app_config_t *config);
APP_NODISCARD app_error cmd_accounts_enable(int argc, char **argv,
                                            app_config_t *config);
APP_NODISCARD app_error cmd_accounts_status(int argc, char **argv,
                                            app_config_t *config);

// Session commands
APP_NODISCARD app_error cmd_sessions_list(int argc, char **argv,
                                          app_config_t *config);
APP_NODISCARD app_error cmd_sessions_start(int argc, char **argv,
                                           app_config_t *config);
APP_NODISCARD app_error cmd_sessions_attach(int argc, char **argv,
                                            app_config_t *config);
APP_NODISCARD app_error cmd_sessions_save(int argc, char **argv,
                                          app_config_t *config);
APP_NODISCARD app_error cmd_sessions_restore(int argc, char **argv,
                                             app_config_t *config);
APP_NODISCARD app_error cmd_sessions_kill(int argc, char **argv,
                                          app_config_t *config);

// Multiplexer commands
APP_NODISCARD app_error cmd_mux_up(int argc, char **argv, app_config_t *config);
APP_NODISCARD app_error cmd_mux_ls(int argc, char **argv, app_config_t *config);
APP_NODISCARD app_error cmd_mux_attach(int argc, char **argv,
                                       app_config_t *config);
APP_NODISCARD app_error cmd_mux_send(int argc, char **argv,
                                     app_config_t *config);
APP_NODISCARD app_error cmd_mux_down(int argc, char **argv,
                                     app_config_t *config);

// Config commands
APP_NODISCARD app_error cmd_config_show(int argc, char **argv,
                                        app_config_t *config);
APP_NODISCARD app_error cmd_config_set(int argc, char **argv,
                                       app_config_t *config);
APP_NODISCARD app_error cmd_config_edit(int argc, char **argv,
                                        app_config_t *config);
APP_NODISCARD app_error cmd_config_reset(int argc, char **argv,
                                         app_config_t *config);

// Token commands
APP_NODISCARD app_error cmd_tokens_inspect(int argc, char **argv,
                                           app_config_t *config);
APP_NODISCARD app_error cmd_tokens_refresh(int argc, char **argv,
                                           app_config_t *config);
APP_NODISCARD app_error cmd_tokens_expire(int argc, char **argv,
                                          app_config_t *config);

// Auth commands
APP_NODISCARD app_error cmd_auth_login(int argc, char **argv,
                                       app_config_t *config);
APP_NODISCARD app_error cmd_auth_logout(int argc, char **argv,
                                        app_config_t *config);
APP_NODISCARD app_error cmd_auth_status(int argc, char **argv,
                                        app_config_t *config);