#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <core/error.h>

// Forward declarations
typedef struct app_tui_mux app_tui_mux_t;

// Command completion result
typedef struct {
  char **suggestions;  // Array of completion suggestions
  size_t count;        // Number of suggestions
  size_t capacity;     // Array capacity
} app_command_completion_t;

// Command handler function type
typedef app_error (*app_command_handler_t)(app_tui_mux_t *mux, int argc,
                                           char **argv);

// Command definition
typedef struct {
  const char *name;               // Command name
  const char *alias;              // Short alias (optional)
  const char *description;        // Help text
  app_command_handler_t handler;  // Handler function
  int min_args;                   // Minimum arguments
  int max_args;                   // Maximum arguments (-1 for unlimited)
} app_command_def_t;

// Command registry
typedef struct {
  app_command_def_t *commands;  // Array of commands
  size_t count;                 // Number of commands
  size_t capacity;              // Array capacity
} app_command_registry_t;

// Command mode functions
APP_NODISCARD app_error app_tui_command_init(app_tui_mux_t *mux);
APP_NODISCARD app_error app_tui_command_cleanup(app_tui_mux_t *mux);

// Enter/exit command mode
APP_NODISCARD app_error app_tui_command_enter(app_tui_mux_t *mux);
APP_NODISCARD app_error app_tui_command_exit(app_tui_mux_t *mux);

// Command input handling
APP_NODISCARD app_error app_tui_command_handle_key(app_tui_mux_t *mux, int key);
APP_NODISCARD app_error app_tui_command_insert_char(app_tui_mux_t *mux,
                                                    char ch);
APP_NODISCARD app_error app_tui_command_delete_char(app_tui_mux_t *mux);
APP_NODISCARD app_error app_tui_command_move_cursor(app_tui_mux_t *mux,
                                                    int delta);

// Command execution
APP_NODISCARD app_error app_tui_command_execute(app_tui_mux_t *mux,
                                                const char *command);
APP_NODISCARD app_error
app_tui_command_parse_and_execute(app_tui_mux_t *mux, const char *command_line);

// Command completion
APP_NODISCARD app_error
app_tui_command_complete(app_tui_mux_t *mux, const char *partial,
                         app_command_completion_t *completion);
APP_NODISCARD app_error
app_tui_command_free_completion(app_command_completion_t *completion);

// Command history
APP_NODISCARD app_error app_tui_command_history_add(app_tui_mux_t *mux,
                                                    const char *command);
APP_NODISCARD app_error app_tui_command_history_prev(app_tui_mux_t *mux);
APP_NODISCARD app_error app_tui_command_history_next(app_tui_mux_t *mux);
APP_NODISCARD app_error app_tui_command_history_save(app_tui_mux_t *mux,
                                                     const char *path);
APP_NODISCARD app_error app_tui_command_history_load(app_tui_mux_t *mux,
                                                     const char *path);

// Command registry
APP_NODISCARD app_error app_tui_command_register(
    app_command_registry_t *registry, const app_command_def_t *command);
APP_NODISCARD app_error app_tui_command_find(app_command_registry_t *registry,
                                             const char *name,
                                             app_command_def_t **command);

// Built-in command handlers
APP_NODISCARD app_error app_tui_cmd_quit(app_tui_mux_t *mux, int argc,
                                         char **argv);
APP_NODISCARD app_error app_tui_cmd_split(app_tui_mux_t *mux, int argc,
                                          char **argv);
APP_NODISCARD app_error app_tui_cmd_vsplit(app_tui_mux_t *mux, int argc,
                                           char **argv);
APP_NODISCARD app_error app_tui_cmd_close(app_tui_mux_t *mux, int argc,
                                          char **argv);
APP_NODISCARD app_error app_tui_cmd_focus(app_tui_mux_t *mux, int argc,
                                          char **argv);
APP_NODISCARD app_error app_tui_cmd_layout(app_tui_mux_t *mux, int argc,
                                           char **argv);
APP_NODISCARD app_error app_tui_cmd_new(app_tui_mux_t *mux, int argc,
                                        char **argv);
APP_NODISCARD app_error app_tui_cmd_restart(app_tui_mux_t *mux, int argc,
                                            char **argv);
APP_NODISCARD app_error app_tui_cmd_save(app_tui_mux_t *mux, int argc,
                                         char **argv);
APP_NODISCARD app_error app_tui_cmd_load(app_tui_mux_t *mux, int argc,
                                         char **argv);
APP_NODISCARD app_error app_tui_cmd_help(app_tui_mux_t *mux, int argc,
                                         char **argv);
APP_NODISCARD app_error app_tui_cmd_set(app_tui_mux_t *mux, int argc,
                                        char **argv);
APP_NODISCARD app_error app_tui_cmd_bind(app_tui_mux_t *mux, int argc,
                                         char **argv);