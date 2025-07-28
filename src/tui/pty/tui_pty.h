#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#include <core/error.h>
#include <core/types.h>

// PTY structure
typedef struct {
  int master_fd;     // Master file descriptor
  int slave_fd;      // Slave file descriptor
  char *slave_name;  // Slave device name
  pid_t child_pid;   // Child process PID
  bool is_open;      // PTY is open
} app_pty_t;

// PTY configuration
typedef struct {
  int rows;                 // Terminal rows
  int cols;                 // Terminal columns
  const char *term_type;    // Terminal type (e.g., "xterm-256color")
  const char *working_dir;  // Working directory for child process
  char **env_vars;          // Environment variables (NULL-terminated)
} app_pty_config_t;

// Process launch configuration
typedef struct {
  const char *command;      // Command to execute
  char **args;              // Command arguments (NULL-terminated)
  const char *working_dir;  // Working directory
  char **env_vars;          // Environment variables (NULL-terminated)
} app_process_config_t;

// PTY lifecycle
APP_NODISCARD app_error app_pty_create(app_pty_t **pty);

APP_NODISCARD app_error app_pty_destroy(app_pty_t *pty);

// PTY operations
APP_NODISCARD app_error app_pty_open(app_pty_t *pty,
                                     const app_pty_config_t *config);

APP_NODISCARD app_error app_pty_close(app_pty_t *pty);

// Process management
APP_NODISCARD app_error app_pty_spawn(app_pty_t *pty,
                                      const app_process_config_t *config);

APP_NODISCARD app_error app_pty_wait(app_pty_t *pty, int *exit_status,
                                     bool non_blocking);

APP_NODISCARD app_error app_pty_kill(app_pty_t *pty, int signal);

APP_NODISCARD app_error app_pty_is_alive(app_pty_t *pty, bool *alive);

// I/O operations
APP_NODISCARD app_error app_pty_read(app_pty_t *pty, char *buffer, size_t size,
                                     size_t *bytes_read, int timeout_ms);

APP_NODISCARD app_error app_pty_write(app_pty_t *pty, const char *data,
                                      size_t size, size_t *bytes_written);

APP_NODISCARD app_error app_pty_set_nonblocking(app_pty_t *pty,
                                                bool non_blocking);

// Terminal operations
APP_NODISCARD app_error app_pty_resize(app_pty_t *pty, int rows, int cols);

APP_NODISCARD app_error app_pty_get_size(app_pty_t *pty, int *rows, int *cols);

// Utility functions
APP_NODISCARD app_error app_pty_make_raw(int fd);

APP_NODISCARD app_error app_pty_restore_mode(int fd);

// POSIX PTY support
APP_NODISCARD app_error app_pty_create_posix(app_pty_t *pty,
                                             const app_pty_config_t *config);