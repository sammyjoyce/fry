/*
 * Pane process launching integration
 * Connects panes with PTY process spawning for Claude Code
 */

#include <core/config.h>
#include <core/session.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utils/logging.h>
#include <utils/memory.h>

#include "tui/panes/tui_pane.h"
#include "tui/pty/tui_pty.h"

// Default Claude Code command
#define CLAUDE_CODE_COMMAND "claude"
#define CLAUDE_CODE_FALLBACK "/usr/local/bin/claude"

// Find Claude Code executable
static const char *find_claude_code_command(void) {
  // First try the command in PATH
  if (system("which " CLAUDE_CODE_COMMAND " >/dev/null 2>&1") == 0) {
    return CLAUDE_CODE_COMMAND;
  }

  // Try common installation paths
  const char *paths[] = {CLAUDE_CODE_FALLBACK, "/opt/claude/bin/claude",
                         "/usr/bin/claude", NULL};

  for (const char **p = paths; *p; p++) {
    if (access(*p, X_OK) == 0) {
      return *p;
    }
  }

  LOG_ERROR("Claude Code executable not found in PATH or common locations");
  return NULL;
}

// Launch Claude Code in a pane
app_error app_tui_pane_launch_claude(app_tui_pane_t *pane,
                                     const char *account_id,
                                     app_session_t *session) {
  (void)session;  // Unused for now
  if (!pane) {
    return APP_ERROR_INVALID_ARG;
  }

  // Check if pane already has a process
  if (pane->pty_master >= 0) {
    LOG_ERROR("Pane already has an active PTY");
    return APP_ERROR_INVALID_ARG;
  }

  // Find Claude Code command
  const char *claude_cmd = find_claude_code_command();
  if (!claude_cmd) {
    return APP_ERROR_NOT_FOUND;
  }

  // Create PTY
  app_pty_t *pty = NULL;
  app_error err = app_pty_create(&pty);
  if (err != APP_SUCCESS) {
    LOG_ERROR("Failed to create PTY: %s", app_error_string(err));
    return err;
  }

  // Configure PTY
  app_pty_config_t pty_config = {
      .rows = pane->geometry.height - 2,  // Account for borders
      .cols = pane->geometry.width - 2,
      .term_type = "xterm-256color",
      .working_dir = getenv("HOME"),
      .env_vars = NULL};

  // Open PTY
  err = app_pty_open(pty, &pty_config);
  if (err != APP_SUCCESS) {
    LOG_ERROR("Failed to open PTY: %s", app_error_string(err));
    (void)app_pty_destroy(pty);
    return err;
  }

  // Set non-blocking mode
  err = app_pty_set_nonblocking(pty, true);
  if (err != APP_SUCCESS) {
    LOG_WARNING("Failed to set PTY to non-blocking mode");
  }

  // Build command arguments
  char *args[10];
  int arg_count = 0;

  // Add account selection if specified
  if (account_id && strlen(account_id) > 0) {
    args[arg_count++] = "--account";
    args[arg_count++] = (char *)account_id;
  }

  // Add any additional flags
  args[arg_count++] = "--no-update-check";  // Disable update checks in TUI
  args[arg_count] = NULL;

  // Build environment with OAuth token
  char *env_vars[10];
  int env_count = 0;

  // Get OAuth token from environment or session
  const char *token = getenv("CLAUDE_OAUTH_TOKEN");
  if (token) {
    char *token_env =
        app_malloc(strlen("CLAUDE_OAUTH_TOKEN=") + strlen(token) + 1);
    if (token_env) {
      sprintf(token_env, "CLAUDE_OAUTH_TOKEN=%s", token);
      env_vars[env_count++] = token_env;
    }
  }

  // Add account ID to environment
  if (account_id) {
    char *account_env =
        app_malloc(strlen("CLAUDE_ACCOUNT_ID=") + strlen(account_id) + 1);
    if (account_env) {
      sprintf(account_env, "CLAUDE_ACCOUNT_ID=%s", account_id);
      env_vars[env_count++] = account_env;
    }
  }

  // Add TUI mode indicator
  env_vars[env_count++] = app_strdup("CLAUDE_TUI_MODE=1");
  env_vars[env_count] = NULL;

  // Configure process launch
  app_process_config_t proc_config = {
      .command = claude_cmd,
      .args = arg_count > 0 ? args : NULL,
      .working_dir = pty_config.working_dir,
      .env_vars = env_count > 0 ? env_vars : NULL};

  // Spawn Claude Code process
  err = app_pty_spawn(pty, &proc_config);

  // Free environment strings
  for (int i = 0; i < env_count; i++) {
    if (env_vars[i]) {
      app_free(env_vars[i]);
    }
  }

  if (err != APP_SUCCESS) {
    LOG_ERROR("Failed to spawn Claude Code: %s", app_error_string(err));
    (void)app_pty_close(pty);
    (void)app_pty_destroy(pty);
    return err;
  }

  // Update pane with PTY information
  pane->pty_master = pty->master_fd;
  pane->process_pid = pty->child_pid;
  if (pty->slave_name) {
    pane->pty_name = app_strdup(pty->slave_name);
  }

  // Update pane state
  pane->state = PANE_STATE_ACTIVE;
  pane->account_id = account_id ? app_strdup(account_id) : NULL;

  // Store PTY pointer (we'll need to manage this properly)
  // For now, we'll leak it - in production, store in pane manager

  LOG_INFO("Launched Claude Code in pane %d (PID: %d, account: %s)",
           pane->number, pty->child_pid, account_id ? account_id : "default");

  return APP_SUCCESS;
}

// Monitor pane process status
app_error app_tui_pane_check_process(app_tui_pane_t *pane) {
  if (!pane) {
    return APP_ERROR_INVALID_ARG;
  }

  if (pane->process_pid <= 0) {
    return APP_SUCCESS;  // No process to check
  }

  // Check if process is still alive
  if (kill(pane->process_pid, 0) < 0) {
    if (errno == ESRCH) {
      // Process no longer exists
      LOG_INFO("Process %d in pane %d has exited", pane->process_pid,
               pane->number);

      // Update pane state
      pane->state = PANE_STATE_DISCONNECTED;
      pane->process_pid = -1;

      // Close PTY if still open
      if (pane->pty_master >= 0) {
        close(pane->pty_master);
        pane->pty_master = -1;
      }

      return APP_ERROR_NOT_FOUND;
    }
  }

  return APP_SUCCESS;
}

// Terminate pane process
app_error app_tui_pane_terminate_process(app_tui_pane_t *pane) {
  if (!pane) {
    return APP_ERROR_INVALID_ARG;
  }

  if (pane->process_pid <= 0) {
    return APP_SUCCESS;  // No process to terminate
  }

  LOG_INFO("Terminating process %d in pane %d", pane->process_pid,
           pane->number);

  // First try SIGTERM
  if (kill(pane->process_pid, SIGTERM) == 0) {
    // Give process time to exit gracefully
    usleep(100000);  // 100ms

    // Check if it's still alive
    if (kill(pane->process_pid, 0) == 0) {
      // Still alive, use SIGKILL
      LOG_WARNING("Process %d didn't respond to SIGTERM, using SIGKILL",
                  pane->process_pid);
      kill(pane->process_pid, SIGKILL);
    }
  }

  // Update pane state
  pane->state = PANE_STATE_DISCONNECTED;
  pane->process_pid = -1;

  // Close PTY
  if (pane->pty_master >= 0) {
    close(pane->pty_master);
    pane->pty_master = -1;
  }

  return APP_SUCCESS;
}