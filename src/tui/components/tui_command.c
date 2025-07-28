#include "tui_command.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <ncurses.h>
#else
#include <ncurses/ncurses.h>
#endif

#include <core/error.h>
#include <utils/logging.h>
#include <utils/memory.h>
#include "tui/mux/tui_mux.h"
#include "tui/backend/tui_ncurses.h"
#include "tui/tui_render.h"
#include "tui/components/tui_input.h"
#include "tui/tui.h"

// CTRL macro definition
#ifndef CTRL
#define CTRL(c) ((c) & 0x1f)
#endif

// Error code definitions
#ifndef APP_ERROR_TUI
#define APP_ERROR_TUI APP_ERROR_INVALID_OPERATION
#endif

#ifndef APP_ERROR_BUFFER_FULL
#define APP_ERROR_BUFFER_FULL APP_ERROR_OVERFLOW
#endif

// Built-in commands
static const app_command_def_t builtin_commands[] = {
    {"quit", "q", "Exit the multiplexer", app_tui_cmd_quit, 0, 0},
    {"split", "sp", "Split pane horizontally", app_tui_cmd_split, 0, 1},
    {"vsplit", "vsp", "Split pane vertically", app_tui_cmd_vsplit, 0, 1},
    {"close", "c", "Close current pane", app_tui_cmd_close, 0, 0},
    {"focus", "f", "Focus pane by number or direction", app_tui_cmd_focus, 1,
     1},
    {"layout", "l", "Change layout mode", app_tui_cmd_layout, 1, 1},
    {"new", "n", "Create new pane", app_tui_cmd_new, 0, 1},
    {"restart", "r", "Restart current pane", app_tui_cmd_restart, 0, 0},
    {"save", "w", "Save workspace", app_tui_cmd_save, 0, 1},
    {"load", "e", "Load workspace", app_tui_cmd_load, 1, 1},
    {"help", "h", "Show help", app_tui_cmd_help, 0, 1},
    {"set", "s", "Set option", app_tui_cmd_set, 2, 2},
    {"bind", "b", "Bind key", app_tui_cmd_bind, 2, 2},
};

// Initialize command mode
app_error app_tui_command_init(app_tui_mux_t *mux) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  // Initialize command history
  mux->history = app_calloc(1, sizeof(app_command_history_t));
  if (!mux->history) {
    return APP_ERROR_MEMORY;
  }

  mux->history->capacity = 100;
  mux->history->commands = app_calloc(mux->history->capacity, sizeof(char *));
  if (!mux->history->commands) {
    app_free(mux->history);
    mux->history = NULL;
    return APP_ERROR_MEMORY;
  }

  // Load command history from file
  char history_path[256];
  snprintf(history_path, sizeof(history_path), "%s/.cache/fry/command_history",
           getenv("HOME"));
  (void)app_tui_command_history_load(mux, history_path);

  return APP_SUCCESS;
}

// Cleanup command mode
app_error app_tui_command_cleanup(app_tui_mux_t *mux) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  // Save command history
  if (mux->history) {
    char history_path[256];
    snprintf(history_path, sizeof(history_path),
             "%s/.cache/fry/command_history", getenv("HOME"));
    (void)app_tui_command_history_save(mux, history_path);

    // Free history
    for (size_t i = 0; i < mux->history->count; i++) {
      app_free(mux->history->commands[i]);
    }
    app_free(mux->history->commands);
    app_free(mux->history);
    mux->history = NULL;
  }

  return APP_SUCCESS;
}

// Enter command mode
app_error app_tui_command_enter(app_tui_mux_t *mux) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  LOG_DEBUG("Entering command mode");

  // Switch to command mode
  mux->input_mode = INPUT_MODE_COMMAND;

  // Clear command buffer
  memset(mux->command_buffer, 0, mux->command_buffer_size);
  mux->command_cursor = 0;

  // Reset history position
  if (mux->history) {
    mux->history->current = mux->history->count;
  }

  // Create command line window if needed
  if (!mux->command_line) {
    int height = tui_get_max_y();
    tui_window_t *cmd_window = tui_create_window(1, tui_get_max_x(), height - 1, 0);
    if (!cmd_window) {
      return APP_ERROR_TUI;
    }
    mux->command_line = cmd_window->win;
  }

  // Render command line
  return app_tui_mux_render_command_line(mux);
}

// Exit command mode
app_error app_tui_command_exit(app_tui_mux_t *mux) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  LOG_DEBUG("Exiting command mode");

  // Switch back to normal mode
  mux->input_mode = INPUT_MODE_NORMAL;

  // Clear command buffer
  memset(mux->command_buffer, 0, mux->command_buffer_size);
  mux->command_cursor = 0;

  // Clear command line
  if (mux->command_line) {
    wclear(mux->command_line);
    wrefresh(mux->command_line);
  }

  // Refresh main display
  return app_tui_mux_render(mux);
}

// Handle key in command mode
app_error app_tui_command_handle_key(app_tui_mux_t *mux, int key) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  switch (key) {
  case 27:  // ESC
  case CTRL('c'):
    return app_tui_command_exit(mux);

  case '\n':  // Enter
  case '\r':
    if (strlen(mux->command_buffer) > 0) {
      // Add to history
      (void)app_tui_command_history_add(mux, mux->command_buffer);

      // Execute command
      app_error err =
          app_tui_command_parse_and_execute(mux, mux->command_buffer);

      // Exit command mode unless there was an error
      if (err == APP_SUCCESS) {
        return app_tui_command_exit(mux);
      }

      // Show error and stay in command mode
      return err;
    }
    return app_tui_command_exit(mux);

  case KEY_BACKSPACE:
  case 127:  // DEL
  case 8:    // Ctrl-H
    return app_tui_command_delete_char(mux);

  case KEY_LEFT:
  case CTRL('b'):
    return app_tui_command_move_cursor(mux, -1);

  case KEY_RIGHT:
  case CTRL('f'):
    return app_tui_command_move_cursor(mux, 1);

  case KEY_HOME:
  case CTRL('a'):
    mux->command_cursor = 0;
    return app_tui_mux_render_command_line(mux);

  case KEY_END:
  case CTRL('e'):
    mux->command_cursor = strlen(mux->command_buffer);
    return app_tui_mux_render_command_line(mux);

  case KEY_UP:
  case CTRL('p'):
    return app_tui_command_history_prev(mux);

  case KEY_DOWN:
  case CTRL('n'):
    return app_tui_command_history_next(mux);

  case '\t':  // Tab - completion
  {
    app_command_completion_t completion = {0};
    app_error err =
        app_tui_command_complete(mux, mux->command_buffer, &completion);
    if (err == APP_SUCCESS && completion.count == 1) {
      // Single match - complete it
      strncpy(mux->command_buffer, completion.suggestions[0],
              mux->command_buffer_size - 1);
      mux->command_cursor = strlen(mux->command_buffer);
    } else if (err == APP_SUCCESS && completion.count > 1) {
      // Multiple matches - show them
      // TODO: Display completion options
    }
    (void)app_tui_command_free_completion(&completion);
    return app_tui_mux_render_command_line(mux);
  }

  case CTRL('u'):  // Clear line
    memset(mux->command_buffer, 0, mux->command_buffer_size);
    mux->command_cursor = 0;
    return app_tui_mux_render_command_line(mux);

  case CTRL('w'):  // Delete word
  {
    size_t pos = mux->command_cursor;
    // Skip trailing spaces
    while (pos > 0 && mux->command_buffer[pos - 1] == ' ')
      pos--;
    // Delete word
    while (pos > 0 && mux->command_buffer[pos - 1] != ' ')
      pos--;

    // Shift remaining text
    size_t len = strlen(mux->command_buffer);
    memmove(&mux->command_buffer[pos],
            &mux->command_buffer[mux->command_cursor],
            len - mux->command_cursor + 1);
    mux->command_cursor = pos;
    return app_tui_mux_render_command_line(mux);
  }

  default:
    // Insert printable character
    if (isprint(key)) {
      return app_tui_command_insert_char(mux, (char)key);
    }
    break;
  }

  return APP_SUCCESS;
}

// Insert character at cursor
app_error app_tui_command_insert_char(app_tui_mux_t *mux, char ch) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  size_t len = strlen(mux->command_buffer);
  if (len >= mux->command_buffer_size - 1) {
    return APP_ERROR_BUFFER_FULL;
  }

  // Shift text to make room
  memmove(&mux->command_buffer[mux->command_cursor + 1],
          &mux->command_buffer[mux->command_cursor],
          len - mux->command_cursor + 1);

  // Insert character
  mux->command_buffer[mux->command_cursor] = ch;
  mux->command_cursor++;

  return app_tui_mux_render_command_line(mux);
}

// Delete character at cursor
app_error app_tui_command_delete_char(app_tui_mux_t *mux) {
  if (!mux || mux->command_cursor == 0) {
    return APP_SUCCESS;
  }

  size_t len = strlen(mux->command_buffer);

  // Shift text to remove character
  memmove(&mux->command_buffer[mux->command_cursor - 1],
          &mux->command_buffer[mux->command_cursor],
          len - mux->command_cursor + 1);

  mux->command_cursor--;

  return app_tui_mux_render_command_line(mux);
}

// Move cursor
app_error app_tui_command_move_cursor(app_tui_mux_t *mux, int delta) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  size_t len = strlen(mux->command_buffer);
  int new_pos = (int)mux->command_cursor + delta;

  if (new_pos < 0) {
    mux->command_cursor = 0;
  } else if (new_pos > (int)len) {
    mux->command_cursor = len;
  } else {
    mux->command_cursor = (size_t)new_pos;
  }

  return app_tui_mux_render_command_line(mux);
}

// Parse and execute command
app_error app_tui_command_parse_and_execute(app_tui_mux_t *mux,
                                            const char *command_line) {
  if (!mux || !command_line) {
    return APP_ERROR_INVALID_ARG;
  }

  // Skip leading whitespace
  while (*command_line && isspace(*command_line)) {
    command_line++;
  }

  if (!*command_line) {
    return APP_SUCCESS;  // Empty command
  }

  // Parse command and arguments
  char *cmd_copy = app_strdup(command_line);
  if (!cmd_copy) {
    return APP_ERROR_MEMORY;
  }

  char *argv[32];
  int argc = 0;

  char *token = strtok(cmd_copy, " \t");
  while (token && argc < 32) {
    argv[argc++] = token;
    token = strtok(NULL, " \t");
  }

  if (argc == 0) {
    app_free(cmd_copy);
    return APP_SUCCESS;
  }

  // Find and execute command
  app_error err = APP_ERROR_NOT_FOUND;
  for (size_t i = 0; i < sizeof(builtin_commands) / sizeof(builtin_commands[0]);
       i++) {
    const app_command_def_t *cmd = &builtin_commands[i];
    if (strcmp(argv[0], cmd->name) == 0 ||
        (cmd->alias && strcmp(argv[0], cmd->alias) == 0)) {
      // Check argument count
      if (argc - 1 < cmd->min_args) {
        LOG_ERROR("Command '%s' requires at least %d arguments", cmd->name,
                  cmd->min_args);
        err = APP_ERROR_INVALID_ARG;
        break;
      }
      if (cmd->max_args >= 0 && argc - 1 > cmd->max_args) {
        LOG_ERROR("Command '%s' accepts at most %d arguments", cmd->name,
                  cmd->max_args);
        err = APP_ERROR_INVALID_ARG;
        break;
      }

      // Execute command
      err = cmd->handler(mux, argc - 1, &argv[1]);
      break;
    }
  }

  if (err == APP_ERROR_NOT_FOUND) {
    LOG_ERROR("Unknown command: %s", argv[0]);
  }

  app_free(cmd_copy);
  return err;
}

// Command completion
app_error app_tui_command_complete(app_tui_mux_t *mux, const char *partial,
                                   app_command_completion_t *completion) {
  if (!mux || !partial || !completion) {
    return APP_ERROR_INVALID_ARG;
  }

  completion->count = 0;
  completion->capacity = 16;
  completion->suggestions = app_calloc(completion->capacity, sizeof(char *));
  if (!completion->suggestions) {
    return APP_ERROR_MEMORY;
  }

  size_t partial_len = strlen(partial);

  // Complete command names
  for (size_t i = 0; i < sizeof(builtin_commands) / sizeof(builtin_commands[0]);
       i++) {
    const app_command_def_t *cmd = &builtin_commands[i];

    // Check main name
    if (strncmp(cmd->name, partial, partial_len) == 0) {
      if (completion->count >= completion->capacity) {
        // Resize array
        size_t new_capacity = completion->capacity * 2;
        char **new_suggestions =
            app_realloc(completion->suggestions, new_capacity * sizeof(char *));
        if (!new_suggestions) {
          app_tui_command_free_completion(completion);
          return APP_ERROR_MEMORY;
        }
        completion->suggestions = new_suggestions;
        completion->capacity = new_capacity;
      }

      completion->suggestions[completion->count] = app_strdup(cmd->name);
      if (!completion->suggestions[completion->count]) {
        app_tui_command_free_completion(completion);
        return APP_ERROR_MEMORY;
      }
      completion->count++;
    }

    // Check alias
    if (cmd->alias && strncmp(cmd->alias, partial, partial_len) == 0) {
      if (completion->count >= completion->capacity) {
        // Resize array
        size_t new_capacity = completion->capacity * 2;
        char **new_suggestions =
            app_realloc(completion->suggestions, new_capacity * sizeof(char *));
        if (!new_suggestions) {
          app_tui_command_free_completion(completion);
          return APP_ERROR_MEMORY;
        }
        completion->suggestions = new_suggestions;
        completion->capacity = new_capacity;
      }

      completion->suggestions[completion->count] = app_strdup(cmd->alias);
      if (!completion->suggestions[completion->count]) {
        app_tui_command_free_completion(completion);
        return APP_ERROR_MEMORY;
      }
      completion->count++;
    }
  }

  return APP_SUCCESS;
}

// Free completion results
app_error app_tui_command_free_completion(
    app_command_completion_t *completion) {
  if (!completion) {
    return APP_ERROR_INVALID_ARG;
  }

  for (size_t i = 0; i < completion->count; i++) {
    app_free(completion->suggestions[i]);
  }
  app_free(completion->suggestions);

  completion->suggestions = NULL;
  completion->count = 0;
  completion->capacity = 0;

  return APP_SUCCESS;
}

// Add command to history
app_error app_tui_command_history_add(app_tui_mux_t *mux, const char *command) {
  if (!mux || !mux->history || !command) {
    return APP_ERROR_INVALID_ARG;
  }

  // Don't add empty commands or duplicates of the last command
  if (strlen(command) == 0) {
    return APP_SUCCESS;
  }

  if (mux->history->count > 0 &&
      strcmp(mux->history->commands[mux->history->count - 1], command) == 0) {
    return APP_SUCCESS;
  }

  // Add to history
  if (mux->history->count >= mux->history->capacity) {
    // Remove oldest command
    app_free(mux->history->commands[0]);
    memmove(&mux->history->commands[0], &mux->history->commands[1],
            (mux->history->count - 1) * sizeof(char *));
    mux->history->count--;
  }

  mux->history->commands[mux->history->count] = app_strdup(command);
  if (!mux->history->commands[mux->history->count]) {
    return APP_ERROR_MEMORY;
  }

  mux->history->count++;
  mux->history->current = mux->history->count;

  return APP_SUCCESS;
}

// Navigate history
app_error app_tui_command_history_prev(app_tui_mux_t *mux) {
  if (!mux || !mux->history) {
    return APP_ERROR_INVALID_ARG;
  }

  if (mux->history->current > 0) {
    mux->history->current--;

    if (mux->history->current < mux->history->count) {
      strncpy(mux->command_buffer,
              mux->history->commands[mux->history->current],
              mux->command_buffer_size - 1);
      mux->command_cursor = strlen(mux->command_buffer);
      return app_tui_mux_render_command_line(mux);
    }
  }

  return APP_SUCCESS;
}

app_error app_tui_command_history_next(app_tui_mux_t *mux) {
  if (!mux || !mux->history) {
    return APP_ERROR_INVALID_ARG;
  }

  if (mux->history->current < mux->history->count) {
    mux->history->current++;

    if (mux->history->current < mux->history->count) {
      strncpy(mux->command_buffer,
              mux->history->commands[mux->history->current],
              mux->command_buffer_size - 1);
    } else {
      // Past the end - clear buffer
      memset(mux->command_buffer, 0, mux->command_buffer_size);
    }

    mux->command_cursor = strlen(mux->command_buffer);
    return app_tui_mux_render_command_line(mux);
  }

  return APP_SUCCESS;
}

// Save/load history
app_error app_tui_command_history_save(app_tui_mux_t *mux, const char *path) {
  if (!mux || !mux->history || !path) {
    return APP_ERROR_INVALID_ARG;
  }

  FILE *file = fopen(path, "w");
  if (!file) {
    return APP_ERROR_IO;
  }

  for (size_t i = 0; i < mux->history->count; i++) {
    fprintf(file, "%s\n", mux->history->commands[i]);
  }

  fclose(file);
  return APP_SUCCESS;
}

app_error app_tui_command_history_load(app_tui_mux_t *mux, const char *path) {
  if (!mux || !mux->history || !path) {
    return APP_ERROR_INVALID_ARG;
  }

  FILE *file = fopen(path, "r");
  if (!file) {
    return APP_SUCCESS;  // Not an error if history file doesn't exist
  }

  char line[256];
  while (fgets(line, sizeof(line), file)) {
    // Remove newline
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
    }

    (void)app_tui_command_history_add(mux, line);
  }

  fclose(file);
  return APP_SUCCESS;
}

// Built-in command handlers

app_error app_tui_cmd_quit(app_tui_mux_t *mux, int argc, char **argv) {
  (void)argc;
  (void)argv;

  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  LOG_INFO("Quit command received");
  mux->running = false;
  return APP_SUCCESS;
}

app_error app_tui_cmd_split(app_tui_mux_t *mux, int argc, char **argv) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  const char *account = argc > 0 ? argv[0] : NULL;

  // TODO: Implement horizontal split
  LOG_INFO("Split command: account=%s", account ? account : "default");

  return APP_ERROR_NOT_IMPLEMENTED;
}

app_error app_tui_cmd_vsplit(app_tui_mux_t *mux, int argc, char **argv) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  const char *account = argc > 0 ? argv[0] : NULL;

  // TODO: Implement vertical split
  LOG_INFO("Vsplit command: account=%s", account ? account : "default");

  return APP_ERROR_NOT_IMPLEMENTED;
}

app_error app_tui_cmd_close(app_tui_mux_t *mux, int argc, char **argv) {
  (void)argc;
  (void)argv;

  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  // Get current pane
  app_tui_pane_t *pane = app_tui_pane_manager_get_focused(mux->pane_manager);
  if (!pane) {
    return APP_ERROR_NOT_FOUND;
  }

  LOG_INFO("Closing pane %d", app_tui_pane_get_number(pane));

  // Remove pane
  return app_tui_mux_remove_pane(mux, pane);
}

app_error app_tui_cmd_focus(app_tui_mux_t *mux, int argc, char **argv) {
  if (!mux || argc < 1) {
    return APP_ERROR_INVALID_ARG;
  }

  // Try to parse as number
  char *endptr;
  long num = strtol(argv[0], &endptr, 10);
  if (*endptr == '\0' && num >= 1 && num <= 9) {
    return app_tui_mux_focus_number(mux, (int)num);
  }

  // Try as direction
  if (strcmp(argv[0], "up") == 0 || strcmp(argv[0], "k") == 0) {
    return app_tui_mux_focus_direction(mux, 0, -1);
  } else if (strcmp(argv[0], "down") == 0 || strcmp(argv[0], "j") == 0) {
    return app_tui_mux_focus_direction(mux, 0, 1);
  } else if (strcmp(argv[0], "left") == 0 || strcmp(argv[0], "h") == 0) {
    return app_tui_mux_focus_direction(mux, -1, 0);
  } else if (strcmp(argv[0], "right") == 0 || strcmp(argv[0], "l") == 0) {
    return app_tui_mux_focus_direction(mux, 1, 0);
  }

  LOG_ERROR("Invalid focus target: %s", argv[0]);
  return APP_ERROR_INVALID_ARG;
}

app_error app_tui_cmd_layout(app_tui_mux_t *mux, int argc, char **argv) {
  if (!mux || argc < 1) {
    return APP_ERROR_INVALID_ARG;
  }

  app_tui_layout_mode_t mode;
  if (strcmp(argv[0], "grid") == 0) {
    mode = TUI_LAYOUT_GRID;
  } else if (strcmp(argv[0], "vsplit") == 0) {
    mode = TUI_LAYOUT_VSPLIT;
  } else if (strcmp(argv[0], "hsplit") == 0) {
    mode = TUI_LAYOUT_HSPLIT;
  } else if (strcmp(argv[0], "focus") == 0) {
    mode = TUI_LAYOUT_FOCUS;
  } else {
    LOG_ERROR("Unknown layout mode: %s", argv[0]);
    return APP_ERROR_INVALID_ARG;
  }

  return app_tui_mux_set_layout(mux, mode);
}

app_error app_tui_cmd_new(app_tui_mux_t *mux, int argc, char **argv) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  const char *account = argc > 0 ? argv[0] : NULL;

  LOG_INFO("Creating new pane with account: %s", account ? account : "default");

  app_tui_pane_t *pane = NULL;
  return app_tui_mux_add_pane(mux, account, &pane);
}

app_error app_tui_cmd_restart(app_tui_mux_t *mux, int argc, char **argv) {
  (void)argc;
  (void)argv;

  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  // Get current pane
  app_tui_pane_t *pane = app_tui_pane_manager_get_focused(mux->pane_manager);
  if (!pane) {
    return APP_ERROR_NOT_FOUND;
  }

  LOG_INFO("Restarting pane %d", app_tui_pane_get_number(pane));

  // TODO: Implement pane restart
  return APP_ERROR_NOT_IMPLEMENTED;
}

app_error app_tui_cmd_save(app_tui_mux_t *mux, int argc, char **argv) {
  if (!mux) {
    return APP_ERROR_INVALID_ARG;
  }

  const char *path = NULL;
  if (argc > 0) {
    path = argv[0];
  } else {
    // Use default path
    static char default_path[256];
    snprintf(default_path, sizeof(default_path), "%s/.cache/fry/workspace.json",
             getenv("HOME"));
    path = default_path;
  }

  LOG_INFO("Saving workspace to: %s", path);
  return app_tui_mux_save_workspace(mux, path);
}

app_error app_tui_cmd_load(app_tui_mux_t *mux, int argc, char **argv) {
  if (!mux || argc < 1) {
    return APP_ERROR_INVALID_ARG;
  }

  LOG_INFO("Loading workspace from: %s", argv[0]);
  return app_tui_mux_load_workspace(mux, argv[0]);
}

app_error app_tui_cmd_help(app_tui_mux_t *mux, int argc, char **argv) {
  (void)mux;

  if (argc > 0) {
    // Show help for specific command
    for (size_t i = 0;
         i < sizeof(builtin_commands) / sizeof(builtin_commands[0]); i++) {
      const app_command_def_t *cmd = &builtin_commands[i];
      if (strcmp(argv[0], cmd->name) == 0 ||
          (cmd->alias && strcmp(argv[0], cmd->alias) == 0)) {
        LOG_INFO("%s (%s) - %s", cmd->name,
                 cmd->alias ? cmd->alias : "no alias", cmd->description);
        return APP_SUCCESS;
      }
    }
    LOG_ERROR("Unknown command: %s", argv[0]);
    return APP_ERROR_NOT_FOUND;
  }

  // Show all commands
  LOG_INFO("Available commands:");
  for (size_t i = 0; i < sizeof(builtin_commands) / sizeof(builtin_commands[0]);
       i++) {
    const app_command_def_t *cmd = &builtin_commands[i];
    LOG_INFO("  %-10s %-5s %s", cmd->name, cmd->alias ? cmd->alias : "",
             cmd->description);
  }

  return APP_SUCCESS;
}

app_error app_tui_cmd_set(app_tui_mux_t *mux, int argc, char **argv) {
  if (!mux || argc < 2) {
    return APP_ERROR_INVALID_ARG;
  }

  const char *option = argv[0];
  const char *value = argv[1];

  LOG_INFO("Setting option %s = %s", option, value);

  // TODO: Implement option setting
  if (strcmp(option, "mouse") == 0) {
    mux->config.enable_mouse = strcmp(value, "on") == 0;
  } else if (strcmp(option, "colors") == 0) {
    mux->config.enable_colors = strcmp(value, "on") == 0;
  } else if (strcmp(option, "wrap") == 0) {
    mux->config.wrap_navigation = strcmp(value, "on") == 0;
  } else {
    LOG_ERROR("Unknown option: %s", option);
    return APP_ERROR_INVALID_ARG;
  }

  return APP_SUCCESS;
}

app_error app_tui_cmd_bind(app_tui_mux_t *mux, int argc, char **argv) {
  if (!mux || argc < 2) {
    return APP_ERROR_INVALID_ARG;
  }

  const char *key = argv[0];
  const char *command = argv[1];

  LOG_INFO("Binding key %s to command %s", key, command);

  // TODO: Implement key binding
  return APP_ERROR_NOT_IMPLEMENTED;
}