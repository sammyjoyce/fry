/*
 * PTY process spawning implementation
 * Implements Task 8: Integrate Claude Code process launching in PTYs
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <core/session.h>
#include <utils/logging.h>
#include <utils/memory.h>
#include "tui_pty.h"

// Default environment variables for Claude Code
static const char *DEFAULT_ENV_VARS[] = {
    "TERM=xterm-256color", "COLORTERM=truecolor", "LANG=en_US.UTF-8",
    "LC_ALL=en_US.UTF-8", NULL};

// Build environment for child process
static char **build_environment(const app_process_config_t *config,
                                const char *oauth_token,
                                const char *account_id) {
  // Count environment variables
  size_t count = 0;

  // Count default vars
  for (const char **p = DEFAULT_ENV_VARS; *p; p++) {
    count++;
  }

  // Count user-provided vars
  if (config->env_vars) {
    for (char **p = config->env_vars; *p; p++) {
      count++;
    }
  }

  // Add space for OAuth token, account ID, PATH, HOME, USER
  count += 5;

  // Allocate environment array
  char **env = app_calloc(count + 1, sizeof(char *));
  if (!env) {
    return NULL;
  }

  size_t i = 0;

  // Copy default environment
  for (const char **p = DEFAULT_ENV_VARS; *p; p++) {
    env[i++] = app_strdup(*p);
  }

  // Add OAuth token if provided
  if (oauth_token) {
    char *token_env =
        app_malloc(strlen("CLAUDE_OAUTH_TOKEN=") + strlen(oauth_token) + 1);
    if (token_env) {
      sprintf(token_env, "CLAUDE_OAUTH_TOKEN=%s", oauth_token);
      env[i++] = token_env;
    }
  }

  // Add account ID if provided
  if (account_id) {
    char *account_env =
        app_malloc(strlen("CLAUDE_ACCOUNT_ID=") + strlen(account_id) + 1);
    if (account_env) {
      sprintf(account_env, "CLAUDE_ACCOUNT_ID=%s", account_id);
      env[i++] = account_env;
    }
  }

  // Copy PATH from parent
  const char *path = getenv("PATH");
  if (path) {
    char *path_env = app_malloc(strlen("PATH=") + strlen(path) + 1);
    if (path_env) {
      sprintf(path_env, "PATH=%s", path);
      env[i++] = path_env;
    }
  }

  // Copy HOME from parent
  const char *home = getenv("HOME");
  if (home) {
    char *home_env = app_malloc(strlen("HOME=") + strlen(home) + 1);
    if (home_env) {
      sprintf(home_env, "HOME=%s", home);
      env[i++] = home_env;
    }
  }

  // Copy USER from parent
  const char *user = getenv("USER");
  if (user) {
    char *user_env = app_malloc(strlen("USER=") + strlen(user) + 1);
    if (user_env) {
      sprintf(user_env, "USER=%s", user);
      env[i++] = user_env;
    }
  }

  // Copy user-provided environment
  if (config->env_vars) {
    for (char **p = config->env_vars; *p && i < count; p++) {
      env[i++] = app_strdup(*p);
    }
  }

  env[i] = NULL;
  return env;
}

// Free environment array
static void free_environment(char **env) {
  if (!env)
    return;

  for (char **p = env; *p; p++) {
    app_free(*p);
  }
  app_free(env);
}

// Spawn process in PTY
app_error app_pty_spawn(app_pty_t *pty, const app_process_config_t *config) {
  if (!pty || !config || !config->command) {
    return APP_ERROR_INVALID_ARG;
  }

  if (!pty->is_open) {
    LOG_ERROR("PTY must be opened before spawning process");
    return APP_ERROR_INVALID_ARG;
  }

  if (pty->child_pid > 0) {
    LOG_ERROR("Process already running in PTY");
    return APP_ERROR_INVALID_ARG;
  }

  // Get OAuth token and account ID from session if available
  // TODO: Get these from the session manager
  const char *oauth_token = getenv("CLAUDE_OAUTH_TOKEN");
  const char *account_id = config->env_vars ? NULL : "default";

  // Build environment
  char **env = build_environment(config, oauth_token, account_id);
  if (!env) {
    LOG_ERROR("Failed to build environment");
    return APP_ERROR_MEMORY;
  }

  // Fork process
  pid_t pid = fork();
  if (pid < 0) {
    LOG_ERROR("Failed to fork: %s", strerror(errno));
    free_environment(env);
    return APP_ERROR_SYSTEM;
  }

  if (pid == 0) {
    // Child process

    // Create new session and process group
    if (setsid() < 0) {
      LOG_ERROR("Failed to create new session: %s", strerror(errno));
      _exit(1);
    }

    // Make the slave PTY the controlling terminal
    if (ioctl(pty->slave_fd, TIOCSCTTY, 0) < 0) {
      LOG_ERROR("Failed to set controlling terminal: %s", strerror(errno));
      _exit(1);
    }

    // Redirect stdin/stdout/stderr to PTY slave
    if (dup2(pty->slave_fd, STDIN_FILENO) < 0 ||
        dup2(pty->slave_fd, STDOUT_FILENO) < 0 ||
        dup2(pty->slave_fd, STDERR_FILENO) < 0) {
      LOG_ERROR("Failed to redirect stdio: %s", strerror(errno));
      _exit(1);
    }

    // Close all file descriptors except stdio
    int max_fd = sysconf(_SC_OPEN_MAX);
    for (int fd = 3; fd < max_fd; fd++) {
      close(fd);
    }

    // Change working directory if specified
    if (config->working_dir) {
      if (chdir(config->working_dir) < 0) {
        LOG_ERROR("Failed to change directory to %s: %s", config->working_dir,
                  strerror(errno));
        _exit(1);
      }
    }

    // Build argument array
    char **args;
    if (config->args) {
      // Count arguments
      int argc = 1;  // For command
      for (char **p = config->args; *p; p++) {
        argc++;
      }

      args = app_calloc(argc + 1, sizeof(char *));
      if (!args) {
        _exit(1);
      }

      args[0] = (char *)config->command;
      int i = 1;
      for (char **p = config->args; *p; p++) {
        args[i++] = *p;
      }
      args[i] = NULL;
    } else {
      // No arguments, just command
      args = app_calloc(2, sizeof(char *));
      if (!args) {
        _exit(1);
      }
      args[0] = (char *)config->command;
      args[1] = NULL;
    }

    // Execute the command
    execve(config->command, args, env);

    // If we get here, exec failed
    LOG_ERROR("Failed to execute %s: %s", config->command, strerror(errno));
    _exit(127);
  }

  // Parent process
  pty->child_pid = pid;

  // Close slave FD in parent
  if (pty->slave_fd >= 0) {
    close(pty->slave_fd);
    pty->slave_fd = -1;
  }

  free_environment(env);

  LOG_INFO("Spawned process %d in PTY: %s", pid, config->command);
  return APP_SUCCESS;
}

// Wait for process to exit
app_error app_pty_wait(app_pty_t *pty, int *exit_status, bool non_blocking) {
  if (!pty) {
    return APP_ERROR_INVALID_ARG;
  }

  if (pty->child_pid <= 0) {
    LOG_ERROR("No child process to wait for");
    return APP_ERROR_INVALID_ARG;
  }

  int status;
  int options = non_blocking ? WNOHANG : 0;

  pid_t result = waitpid(pty->child_pid, &status, options);

  if (result < 0) {
    LOG_ERROR("waitpid failed: %s", strerror(errno));
    return APP_ERROR_SYSTEM;
  }

  if (result == 0) {
    // Child still running (non-blocking mode)
    return APP_ERROR_RESOURCE;  // Process still running
  }

  // Child has exited
  pty->child_pid = -1;

  if (exit_status) {
    if (WIFEXITED(status)) {
      *exit_status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      *exit_status = -WTERMSIG(status);
    } else {
      *exit_status = -1;
    }
  }

  LOG_INFO("Child process exited with status %d",
           WIFEXITED(status) ? WEXITSTATUS(status) : -WTERMSIG(status));

  return APP_SUCCESS;
}

// Send signal to process
app_error app_pty_kill(app_pty_t *pty, int signal) {
  if (!pty) {
    return APP_ERROR_INVALID_ARG;
  }

  if (pty->child_pid <= 0) {
    LOG_ERROR("No child process to signal");
    return APP_ERROR_INVALID_ARG;
  }

  if (kill(pty->child_pid, signal) < 0) {
    if (errno == ESRCH) {
      // Process doesn't exist
      pty->child_pid = -1;
      return APP_ERROR_NOT_FOUND;
    }
    LOG_ERROR("Failed to send signal %d to process %d: %s", signal,
              pty->child_pid, strerror(errno));
    return APP_ERROR_SYSTEM;
  }

  LOG_DEBUG("Sent signal %d to process %d", signal, pty->child_pid);
  return APP_SUCCESS;
}

// Check if process is alive
app_error app_pty_is_alive(app_pty_t *pty, bool *alive) {
  if (!pty || !alive) {
    return APP_ERROR_INVALID_ARG;
  }

  *alive = false;

  if (pty->child_pid <= 0) {
    return APP_SUCCESS;
  }

  // Check if process exists
  if (kill(pty->child_pid, 0) == 0) {
    *alive = true;
  } else if (errno == ESRCH) {
    // Process doesn't exist
    pty->child_pid = -1;
  }

  return APP_SUCCESS;
}

// Read from PTY with timeout
app_error app_pty_read(app_pty_t *pty, char *buffer, size_t size,
                       size_t *bytes_read, int timeout_ms) {
  if (!pty || !buffer || size == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  if (!pty->is_open) {
    return APP_ERROR_INVALID_ARG;
  }

  if (bytes_read) {
    *bytes_read = 0;
  }

  // Use select for timeout
  if (timeout_ms >= 0) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(pty->master_fd, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    int result = select(pty->master_fd + 1, &read_fds, NULL, NULL,
                        timeout_ms >= 0 ? &timeout : NULL);

    if (result < 0) {
      LOG_ERROR("select failed: %s", strerror(errno));
      return APP_ERROR_SYSTEM;
    }

    if (result == 0) {
      // Timeout
      return APP_ERROR_RESOURCE;
    }
  }

  // Read data
  ssize_t n = read(pty->master_fd, buffer, size);

  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return APP_ERROR_RESOURCE;  // No data available yet
    }
    LOG_ERROR("Failed to read from PTY: %s", strerror(errno));
    return APP_ERROR_IO;
  }

  if (n == 0) {
    // EOF - child process has closed the PTY
    return APP_ERROR_IO;
  }

  if (bytes_read) {
    *bytes_read = n;
  }

  return APP_SUCCESS;
}

// Write to PTY
app_error app_pty_write(app_pty_t *pty, const char *data, size_t size,
                        size_t *bytes_written) {
  if (!pty || !data || size == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  if (!pty->is_open) {
    return APP_ERROR_INVALID_ARG;
  }

  if (bytes_written) {
    *bytes_written = 0;
  }

  ssize_t n = write(pty->master_fd, data, size);

  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return APP_ERROR_RESOURCE;  // Write would block
    }
    LOG_ERROR("Failed to write to PTY: %s", strerror(errno));
    return APP_ERROR_IO;
  }

  if (bytes_written) {
    *bytes_written = n;
  }

  return APP_SUCCESS;
}