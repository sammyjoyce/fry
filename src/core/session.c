#include "session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../utils/logging.h"
#include "../utils/memory.h"

// Forward declarations
static app_error app_session_destroy_internal(app_session_manager_t *manager,
                                              app_session_t *session);

// Create session manager
app_error app_session_manager_create(app_session_manager_t **manager) {
  if (!manager) {
    return APP_ERROR_INVALID_ARG;
  }

  app_session_manager_t *mgr = app_calloc(1, sizeof(app_session_manager_t));
  if (!mgr) {
    return APP_ERROR_MEMORY;
  }

  // Initialize with defaults
  mgr->capacity = 10;
  mgr->sessions = app_calloc(mgr->capacity, sizeof(app_session_t *));
  if (!mgr->sessions) {
    app_free(mgr);
    return APP_ERROR_MEMORY;
  }

  mgr->session_count = 0;

  // Set default Claude binary path
  const char *home = getenv("HOME");
  if (home) {
    char claude_path[1024];
    snprintf(claude_path, sizeof(claude_path), "%s/.local/bin/claude", home);
    mgr->claude_binary_path = app_strdup(claude_path);
  } else {
    mgr->claude_binary_path = app_strdup("claude");  // Fallback to PATH
  }

  // Set default sessions directory
  if (home) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/.cache/fry/sessions", home);
    mgr->sessions_dir = app_strdup(path);
  }

  *manager = mgr;
  return APP_SUCCESS;
}

// Destroy session manager
void app_session_manager_destroy(app_session_manager_t *manager) {
  if (!manager) {
    return;
  }

  // Destroy all sessions
  for (size_t i = 0; i < manager->session_count; i++) {
    if (manager->sessions[i]) {
      app_session_destroy_internal(manager, manager->sessions[i]);
    }
  }

  app_free(manager->sessions);
  app_free(manager->claude_binary_path);
  app_free(manager->sessions_dir);
  app_free(manager);
}

// Create a new session
app_error app_session_create(app_session_manager_t *manager,
                             const char *account_id, app_session_t **session) {
  if (!manager || !session) {
    return APP_ERROR_INVALID_ARG;
  }

  if (manager->session_count >= manager->capacity) {
    // Expand capacity
    size_t new_capacity = manager->capacity * 2;
    app_session_t **new_sessions =
        app_realloc(manager->sessions, new_capacity * sizeof(app_session_t *));
    if (!new_sessions) {
      return APP_ERROR_MEMORY;
    }
    manager->sessions = new_sessions;
    manager->capacity = new_capacity;
  }

  app_session_t *s = app_calloc(1, sizeof(app_session_t));
  if (!s) {
    return APP_ERROR_MEMORY;
  }

  // Generate session ID
  char id_buf[64];
  snprintf(id_buf, sizeof(id_buf), "sess_%zu_%d", manager->session_count,
           getpid());
  s->id = app_strdup(id_buf);

  // Copy account ID if provided
  if (account_id) {
    s->account_id = app_strdup(account_id);
  }

  // Initialize state
  s->state = SESSION_STATE_CREATED;
  s->pid = -1;
  s->created_at = time(NULL);
  s->last_active = s->created_at;

  // Add to manager
  manager->sessions[manager->session_count++] = s;

  *session = s;
  LOG_DEBUG("Created session %s for account %s", s->id,
            account_id ? account_id : "(none)");

  return APP_SUCCESS;
}

// Destroy a session
static app_error app_session_destroy_internal(app_session_manager_t *manager,
                                              app_session_t *session) {
  if (!manager || !session) {
    return APP_ERROR_INVALID_ARG;
  }

  // Stop session if running
  if (session->state == SESSION_STATE_RUNNING) {
    app_session_stop(manager, session);
  }

  // Free session data
  app_free(session->id);
  app_free(session->account_id);
  app_free(session->working_directory);
  app_free(session->name);

  // Free environment variables
  if (session->environment) {
    for (char **env = session->environment; *env; env++) {
      app_free(*env);
    }
    app_free(session->environment);
  }

  // Free command arguments
  if (session->command_args) {
    for (char **arg = session->command_args; *arg; arg++) {
      app_free(*arg);
    }
    app_free(session->command_args);
  }

  // Remove from manager
  for (size_t i = 0; i < manager->session_count; i++) {
    if (manager->sessions[i] == session) {
      // Shift remaining sessions
      for (size_t j = i; j < manager->session_count - 1; j++) {
        manager->sessions[j] = manager->sessions[j + 1];
      }
      manager->session_count--;
      break;
    }
  }

  app_free(session);

  return APP_SUCCESS;
}

// Start a session (old signature)
static app_error app_session_start_old(app_session_manager_t *manager,
                                       app_session_t *session, pid_t *pid) {
  if (!manager || !session) {
    return APP_ERROR_INVALID_ARG;
  }

  if (session->state == SESSION_STATE_RUNNING) {
    if (pid) {
      *pid = session->pid;
    }
    return APP_SUCCESS;
  }

  // TODO: Actually launch Claude Code process
  LOG_WARNING("Session start not yet implemented");

  session->state = SESSION_STATE_RUNNING;
  session->last_active = time(NULL);

  return APP_ERROR_NOT_IMPLEMENTED;
}

// Stop a session
app_error app_session_stop(app_session_manager_t *manager,
                           app_session_t *session) {
  if (!manager || !session) {
    return APP_ERROR_INVALID_ARG;
  }

  if (session->state != SESSION_STATE_RUNNING) {
    return APP_SUCCESS;
  }

  // TODO: Actually stop Claude Code process
  LOG_WARNING("Session stop not yet implemented");

  session->state = SESSION_STATE_TERMINATED;
  session->last_active = time(NULL);

  return APP_SUCCESS;
}

// Destroy session info
void app_session_info_destroy(app_session_info_t *info) {
  if (!info) {
    return;
  }

  app_free(info->id);
  app_free(info->project_path);
  app_free(info->account_name);
  app_free(info);
}

// Start a new session with options
app_error app_session_start(app_session_manager_t *manager,
                            app_session_t **session,
                            const app_session_options_t *options) {
  if (!manager || !session || !options) {
    return APP_ERROR_INVALID_ARG;
  }

  // Create session
  app_error err = app_session_create(manager, options->account_name, session);
  if (err != APP_SUCCESS) {
    return err;
  }

  // Set working directory
  if (options->project_path) {
    (*session)->working_directory = app_strdup(options->project_path);
  }

  // TODO: Actually start Claude Code process
  (*session)->pid = getpid();  // Placeholder
  (*session)->state = SESSION_STATE_RUNNING;

  LOG_INFO("Started session %s", (*session)->id);

  return APP_SUCCESS;
}

// Attach to a session
app_error app_session_attach(app_session_t *session) {
  if (!session) {
    return APP_ERROR_INVALID_ARG;
  }

  if (session->state != SESSION_STATE_RUNNING) {
    return APP_ERROR_SESSION_INACTIVE;
  }

  // TODO: Actually attach to Claude Code process
  LOG_WARNING("Session attach not yet implemented");

  return APP_SUCCESS;
}

// Kill a session
app_error app_session_kill(app_session_t *session) {
  if (!session) {
    return APP_ERROR_INVALID_ARG;
  }

  if (session->state != SESSION_STATE_RUNNING) {
    return APP_SUCCESS;
  }

  // TODO: Actually kill Claude Code process
  LOG_WARNING("Session kill not yet implemented");

  session->state = SESSION_STATE_TERMINATED;

  return APP_SUCCESS;
}

// Check if session is active
bool app_session_is_active(app_session_t *session) {
  if (!session) {
    return false;
  }

  return session->state == SESSION_STATE_RUNNING;
}

// Find session by ID
app_error app_session_find(app_session_manager_t *manager,
                           app_session_t **session, const char *id) {
  if (!manager || !session || !id) {
    return APP_ERROR_INVALID_ARG;
  }

  *session = NULL;

  // Search through all sessions
  for (size_t i = 0; i < manager->session_count; i++) {
    if (manager->sessions[i] && manager->sessions[i]->id &&
        strcmp(manager->sessions[i]->id, id) == 0) {
      *session = manager->sessions[i];
      return APP_SUCCESS;
    }
  }

  return APP_ERROR_NOT_FOUND;
}

// List sessions
app_error app_session_list(app_session_manager_t *manager,
                           app_session_info_t ***sessions, size_t *count) {
  if (!manager || !sessions || !count) {
    return APP_ERROR_INVALID_ARG;
  }

  *count = 0;
  *sessions = NULL;

  if (manager->session_count == 0) {
    return APP_SUCCESS;
  }

  // Allocate info array
  *sessions = app_calloc(manager->session_count, sizeof(app_session_info_t *));
  if (!*sessions) {
    return APP_ERROR_MEMORY;
  }

  // Fill info for each session
  for (size_t i = 0; i < manager->session_count; i++) {
    app_session_t *s = manager->sessions[i];
    if (!s)
      continue;

    app_session_info_t *info = app_calloc(1, sizeof(app_session_info_t));
    if (!info) {
      // Clean up on error
      for (size_t j = 0; j < *count; j++) {
        app_session_info_destroy((*sessions)[j]);
      }
      app_free(*sessions);
      return APP_ERROR_MEMORY;
    }

    info->id = app_strdup(s->id);
    info->pid = s->pid;
    info->is_active = (s->state == SESSION_STATE_RUNNING);
    info->created_at = s->created_at;
    info->project_path =
        s->working_directory ? app_strdup(s->working_directory) : NULL;
    info->account_name = s->account_id ? app_strdup(s->account_id) : NULL;

    (*sessions)[*count] = info;
    (*count)++;
  }

  return APP_SUCCESS;
}

// Save session
app_error app_session_save(app_session_t *session, const char *name) {
  if (!session) {
    return APP_ERROR_INVALID_ARG;
  }

  // TODO: Implement session saving
  LOG_WARNING("Session save not yet implemented for '%s'",
              name ? name : "unnamed");

  return APP_SUCCESS;
}

// Restore session
app_error app_session_restore(app_session_manager_t *manager,
                              app_session_t **session, const char *name) {
  if (!manager || !session || !name) {
    return APP_ERROR_INVALID_ARG;
  }

  // TODO: Implement session restore
  LOG_WARNING("Session restore not yet implemented");

  return APP_ERROR_NOT_IMPLEMENTED;
}

// Destroy session (wrapper for new signature)
void app_session_destroy(app_session_t *session) {
  if (!session) {
    return;
  }

  // Free session data
  app_free(session->id);
  app_free(session->account_id);
  app_free(session->working_directory);
  app_free(session->name);

  // Free environment variables
  if (session->environment) {
    for (char **env = session->environment; *env; env++) {
      app_free(*env);
    }
    app_free(session->environment);
  }

  // Free command args
  if (session->command_args) {
    for (char **arg = session->command_args; *arg; arg++) {
      app_free(*arg);
    }
    app_free(session->command_args);
  }

  app_free(session);
}
