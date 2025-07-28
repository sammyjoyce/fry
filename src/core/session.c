#include "session.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
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

  // Auto-load saved sessions
  if (home) {
    char sessions_dir[PATH_MAX];
    snprintf(sessions_dir, sizeof(sessions_dir), "%s/.fry/sessions", home);

    DIR *dir = opendir(sessions_dir);
    if (dir) {
      struct dirent *entry;
      while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (entry->d_name[0] == '.')
          continue;

        // Check if it's a .json file
        size_t len = strlen(entry->d_name);
        if (len > 5 && strcmp(entry->d_name + len - 5, ".json") == 0) {
          // Extract session name (remove .json)
          char session_name[256];
          strncpy(session_name, entry->d_name, len - 5);
          session_name[len - 5] = '\0';

          // Try to restore the session
          app_session_t *restored_session;
          if (app_session_restore(mgr, &restored_session, session_name) ==
              APP_SUCCESS) {
            LOG_DEBUG("Auto-loaded saved session: %s", session_name);
          }
        }
      }
      closedir(dir);
    }
  }

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
    (void)app_session_stop(manager, session);
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

// Stop a session
app_error app_session_stop(app_session_manager_t *manager,
                           app_session_t *session) {
  if (!manager || !session) {
    return APP_ERROR_INVALID_ARG;
  }

  if (session->state != SESSION_STATE_RUNNING) {
    return APP_SUCCESS;
  }

  // Send SIGTERM to the process
  if (session->pid > 0) {
    LOG_DEBUG("Sending SIGTERM to process %d", session->pid);
    if (kill(session->pid, SIGTERM) != 0) {
      if (errno == ESRCH) {
        // Process doesn't exist
        LOG_DEBUG("Process %d already terminated", session->pid);
        session->state = SESSION_STATE_TERMINATED;
        return APP_SUCCESS;
      } else {
        LOG_ERROR("Failed to send SIGTERM to process %d: %s", session->pid,
                  strerror(errno));
        return APP_ERROR_SYSTEM;
      }
    }

    // Give process time to terminate gracefully
    int status;
    pid_t result = waitpid(session->pid, &status, WNOHANG);
    if (result == 0) {
      // Process still running, wait a bit
      sleep(2);
      result = waitpid(session->pid, &status, WNOHANG);

      if (result == 0) {
        // Still running, send SIGKILL
        LOG_DEBUG("Process %d didn't terminate, sending SIGKILL", session->pid);
        kill(session->pid, SIGKILL);
        waitpid(session->pid, &status, 0);
      }
    }

    if (WIFEXITED(status)) {
      LOG_DEBUG("Process %d exited with status %d", session->pid,
                WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      LOG_DEBUG("Process %d terminated by signal %d", session->pid,
                WTERMSIG(status));
    }
  }

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
  } else {
    // Use current directory if not specified
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
      (*session)->working_directory = app_strdup(cwd);
    }
  }

  // Build command arguments
  size_t arg_count = 0;
  size_t arg_capacity = 10;
  char **args = app_calloc(arg_capacity, sizeof(char *));
  if (!args) {
    app_session_destroy(*session);
    *session = NULL;
    return APP_ERROR_MEMORY;
  }

  // Base command
  args[arg_count++] = app_strdup(manager->claude_binary_path);

  // Add account if specified
  if (options->account_name) {
    args[arg_count++] = app_strdup("--account");
    args[arg_count++] = app_strdup(options->account_name);
  }

  // Add model if specified
  if (options->model) {
    args[arg_count++] = app_strdup("--model");
    args[arg_count++] = app_strdup(options->model);
  }

  // Add project path
  if (options->project_path) {
    args[arg_count++] = app_strdup("--project");
    args[arg_count++] = app_strdup(options->project_path);
  }

  // Null terminate
  args[arg_count] = NULL;
  (*session)->command_args = args;

  // Fork and exec
  pid_t pid = fork();
  if (pid < 0) {
    // Fork failed
    LOG_ERROR("Failed to fork process: %s", strerror(errno));
    app_session_destroy(*session);
    *session = NULL;
    return APP_ERROR_SYSTEM;
  } else if (pid == 0) {
    // Child process

    // Change to working directory
    if ((*session)->working_directory) {
      if (chdir((*session)->working_directory) != 0) {
        LOG_ERROR("Failed to change directory to %s: %s",
                  (*session)->working_directory, strerror(errno));
        exit(1);
      }
    }

    // Set environment variables if provided
    if (options->env_vars && options->env_count > 0) {
      for (size_t i = 0; i < options->env_count; i++) {
        if (putenv(options->env_vars[i]) != 0) {
          LOG_ERROR("Failed to set environment variable: %s",
                    options->env_vars[i]);
        }
      }
    }

    // Execute Claude
    execvp(manager->claude_binary_path, args);

    // If we get here, exec failed
    LOG_ERROR("Failed to execute %s: %s", manager->claude_binary_path,
              strerror(errno));
    exit(1);
  } else {
    // Parent process
    (*session)->pid = pid;
    (*session)->state = SESSION_STATE_RUNNING;

    LOG_INFO("Started session %s with PID %d", (*session)->id, pid);

    // Auto-save session for persistence
    app_error save_err = app_session_save(*session, NULL);
    if (save_err != APP_SUCCESS) {
      LOG_WARNING("Failed to auto-save session: %s", app_strerror(save_err));
    }

    // If not detached, wait for process
    if (!options->detached) {
      // For now, we'll just return success
      // In a real implementation, we'd need to handle process monitoring
      LOG_DEBUG("Session started in foreground mode");
    }
  }

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

  // Check if process is still running
  if (session->pid > 0) {
    if (kill(session->pid, 0) != 0) {
      // Process no longer exists
      session->state = SESSION_STATE_TERMINATED;
      session->pid = -1;
      return APP_ERROR_SESSION_INACTIVE;
    }
  } else {
    return APP_ERROR_SESSION_INACTIVE;
  }

  // TODO: Implement actual terminal attachment
  // This would require:
  // 1. Finding the PTY associated with the process
  // 2. Connecting current terminal to that PTY
  // 3. Handling signal forwarding
  // 4. Managing terminal state

  LOG_WARNING("Session attach not fully implemented - process %d is running",
              session->pid);
  LOG_INFO(
      "In a full implementation, this would attach to the Claude process "
      "terminal");

  // For now, just update last active time
  session->last_active = time(NULL);

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

  // Send SIGKILL immediately
  if (session->pid > 0) {
    LOG_DEBUG("Sending SIGKILL to process %d", session->pid);
    if (kill(session->pid, SIGKILL) != 0) {
      if (errno == ESRCH) {
        // Process doesn't exist
        LOG_DEBUG("Process %d already terminated", session->pid);
      } else {
        LOG_ERROR("Failed to send SIGKILL to process %d: %s", session->pid,
                  strerror(errno));
        return APP_ERROR_SYSTEM;
      }
    } else {
      // Wait for process to die
      int status;
      waitpid(session->pid, &status, 0);
      LOG_DEBUG("Process %d killed", session->pid);
    }
  }

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

  // Build session data as JSON
  char *json = app_malloc(4096);
  if (!json) {
    return APP_ERROR_MEMORY;
  }

  int len =
      snprintf(json, 4096,
               "{\n"
               "  \"id\": \"%s\",\n"
               "  \"name\": \"%s\",\n"
               "  \"account_id\": \"%s\",\n"
               "  \"working_directory\": \"%s\",\n"
               "  \"state\": %d,\n"
               "  \"pid\": %d,\n"
               "  \"created_at\": %ld,\n"
               "  \"last_active\": %ld\n"
               "}\n",
               session->id ? session->id : "",
               name ? name : (session->name ? session->name : ""),
               session->account_id ? session->account_id : "",
               session->working_directory ? session->working_directory : "",
               session->state, session->pid, (long)session->created_at,
               (long)session->last_active);

  if (len >= 4096) {
    app_free(json);
    return APP_ERROR_BUFFER_TOO_SMALL;
  }

  // Get home directory
  const char *home = getenv("HOME");
  if (!home) {
    app_free(json);
    return APP_ERROR_SYSTEM;
  }

  // Create sessions directory if it doesn't exist
  char sessions_dir[PATH_MAX];
  snprintf(sessions_dir, sizeof(sessions_dir), "%s/.fry/sessions", home);

  // Create directory with mkdir -p equivalent
  char mkdir_cmd[PATH_MAX + 20];
  snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", sessions_dir);
  if (system(mkdir_cmd) != 0) {
    app_free(json);
    return APP_ERROR_SYSTEM;
  }

  // Build file path
  char filepath[PATH_MAX];
  const char *save_name = name ? name : session->id;
  snprintf(filepath, sizeof(filepath), "%s/%s.json", sessions_dir, save_name);

  // Write to file
  FILE *file = fopen(filepath, "w");
  if (!file) {
    LOG_ERROR("Failed to open session file %s: %s", filepath, strerror(errno));
    app_free(json);
    return APP_ERROR_FILE_OPEN;
  }

  if (fwrite(json, 1, len, file) != (size_t)len) {
    LOG_ERROR("Failed to write session data: %s", strerror(errno));
    fclose(file);
    app_free(json);
    return APP_ERROR_FILE_WRITE;
  }

  fclose(file);
  app_free(json);

  // Update session name if provided
  if (name && (!session->name || strcmp(session->name, name) != 0)) {
    app_free(session->name);
    session->name = app_strdup(name);
  }

  LOG_INFO("Saved session %s to %s", session->id, filepath);

  return APP_SUCCESS;
}

// Restore session
app_error app_session_restore(app_session_manager_t *manager,
                              app_session_t **session, const char *name) {
  if (!manager || !session || !name) {
    return APP_ERROR_INVALID_ARG;
  }

  // Get home directory
  const char *home = getenv("HOME");
  if (!home) {
    return APP_ERROR_SYSTEM;
  }

  // Build file path
  char filepath[PATH_MAX];
  snprintf(filepath, sizeof(filepath), "%s/.fry/sessions/%s.json", home, name);

  // Read file
  FILE *file = fopen(filepath, "r");
  if (!file) {
    LOG_ERROR("Failed to open session file %s: %s", filepath, strerror(errno));
    return APP_ERROR_FILE_OPEN;
  }

  // Read file content
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (file_size <= 0 || file_size > 1024 * 1024) {  // Max 1MB
    fclose(file);
    return APP_ERROR_FILE_READ;
  }

  char *json = app_malloc(file_size + 1);
  if (!json) {
    fclose(file);
    return APP_ERROR_MEMORY;
  }

  if (fread(json, 1, file_size, file) != (size_t)file_size) {
    app_free(json);
    fclose(file);
    return APP_ERROR_FILE_READ;
  }
  json[file_size] = '\0';
  fclose(file);

  // Parse JSON manually (simple parser for our format)
  char id[256] = {0};
  char saved_name[256] = {0};
  char account_id[256] = {0};
  char working_dir[PATH_MAX] = {0};
  int state = SESSION_STATE_CREATED;
  pid_t pid = -1;
  time_t created_at = 0;
  time_t last_active = 0;

  // Extract values using sscanf (simplified parsing)
  char *p;

  // Extract id
  p = strstr(json, "\"id\":");
  if (p) {
    sscanf(p, "\"id\": \"%255[^\"]\"", id);
  }

  // Extract name
  p = strstr(json, "\"name\":");
  if (p) {
    sscanf(p, "\"name\": \"%255[^\"]\"", saved_name);
  }

  // Extract account_id
  p = strstr(json, "\"account_id\":");
  if (p) {
    sscanf(p, "\"account_id\": \"%255[^\"]\"", account_id);
  }

  // Extract working_directory
  p = strstr(json, "\"working_directory\":");
  if (p) {
    sscanf(p, "\"working_directory\": \"%1023[^\"]\"", working_dir);
  }

  // Extract state
  p = strstr(json, "\"state\":");
  if (p) {
    sscanf(p, "\"state\": %d", &state);
  }

  // Extract pid
  p = strstr(json, "\"pid\":");
  if (p) {
    sscanf(p, "\"pid\": %d", &pid);
  }

  // Extract timestamps
  p = strstr(json, "\"created_at\":");
  if (p) {
    long timestamp;
    sscanf(p, "\"created_at\": %ld", &timestamp);
    created_at = (time_t)timestamp;
  }

  p = strstr(json, "\"last_active\":");
  if (p) {
    long timestamp;
    sscanf(p, "\"last_active\": %ld", &timestamp);
    last_active = (time_t)timestamp;
  }

  app_free(json);

  // Create new session
  app_error err =
      app_session_create(manager, account_id[0] ? account_id : NULL, session);
  if (err != APP_SUCCESS) {
    return err;
  }

  // Restore session data
  app_free((*session)->id);
  (*session)->id = app_strdup(id);

  if (saved_name[0]) {
    (*session)->name = app_strdup(saved_name);
  }

  if (working_dir[0]) {
    (*session)->working_directory = app_strdup(working_dir);
  }

  (*session)->state = state;
  (*session)->pid = pid;
  (*session)->created_at = created_at;
  (*session)->last_active = last_active;

  // Check if process is still running (if it was running)
  if (state == SESSION_STATE_RUNNING && pid > 0) {
    if (kill(pid, 0) != 0) {
      // Process no longer exists
      (*session)->state = SESSION_STATE_TERMINATED;
      (*session)->pid = -1;
      LOG_DEBUG(
          "Restored session %s was running but process %d no longer exists", id,
          pid);
    } else {
      LOG_DEBUG("Restored session %s with running process %d", id, pid);
    }
  }

  LOG_INFO("Restored session %s from %s", (*session)->id, filepath);

  return APP_SUCCESS;
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
