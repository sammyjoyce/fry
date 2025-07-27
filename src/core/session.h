#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <time.h>

#include "error.h"
#include "types.h"

// Forward declarations
typedef struct app_session app_session_t;
typedef struct app_session_manager app_session_manager_t;
typedef struct app_session_info app_session_info_t;
typedef struct app_session_options app_session_options_t;
typedef struct app_session_info app_session_info_t;
typedef struct app_session_options app_session_options_t;

// Session states
typedef enum {
  SESSION_STATE_CREATED,
  SESSION_STATE_RUNNING,
  SESSION_STATE_SUSPENDED,
  SESSION_STATE_TERMINATED
} app_session_state_t;

// Session info structure (for listing)
struct app_session_info {
  char *id;            // Session ID
  pid_t pid;           // Process ID
  bool is_active;      // Is session active
  time_t created_at;   // Creation time
  char *project_path;  // Project path
  char *account_name;  // Account name
};

// Session options for starting
struct app_session_options {
  const char *project_path;  // Project directory
  const char *account_name;  // Account to use
  const char *model;         // Model preference
  char **env_vars;           // Additional environment variables
  size_t env_count;          // Number of env vars
  bool detached;             // Run in background
};

// Session structure
struct app_session {
  char *id;          // Unique session identifier
  char *account_id;  // Associated account ID
  pid_t pid;         // Claude Code process PID (renamed from process_pid)
  app_session_state_t state;  // Current session state

  // Timestamps
  time_t created_at;   // Creation timestamp
  time_t last_active;  // Last activity timestamp

  // Process information
  char *working_directory;  // Working directory
  char **environment;       // Environment variables
  char **command_args;      // Command arguments

  // Metadata
  char *name;      // User-friendly name
  void *metadata;  // Additional metadata (JSON)
};

// Session manager structure
struct app_session_manager {
  app_session_t **sessions;  // Array of sessions
  size_t session_count;      // Number of sessions
  size_t capacity;           // Array capacity

  // Configuration
  char *claude_binary_path;  // Path to Claude Code binary
  char *sessions_dir;        // Directory for session data
};

// Session lifecycle
APP_NODISCARD app_error app_session_create(app_session_manager_t *manager,
                                           const char *account_id,
                                           app_session_t **session);

void app_session_destroy(app_session_t *session);

void app_session_info_destroy(app_session_info_t *info);

// Session manager
APP_NODISCARD app_error
app_session_manager_create(app_session_manager_t **manager);

void app_session_manager_destroy(app_session_manager_t *manager);

// Session operations
APP_NODISCARD app_error app_session_start(app_session_manager_t *manager,
                                          app_session_t **session,
                                          const app_session_options_t *options);

APP_NODISCARD app_error app_session_attach(app_session_t *session);

APP_NODISCARD app_error app_session_kill(app_session_t *session);

APP_NODISCARD app_error app_session_stop(app_session_manager_t *manager,
                                         app_session_t *session);

APP_NODISCARD app_error app_session_suspend(app_session_manager_t *manager,
                                            app_session_t *session);

APP_NODISCARD app_error app_session_resume(app_session_manager_t *manager,
                                           app_session_t *session);

bool app_session_is_active(app_session_t *session);

// Session queries
APP_NODISCARD app_error app_session_find(app_session_manager_t *manager,
                                         app_session_t **session,
                                         const char *id);

APP_NODISCARD app_error app_session_get_by_id(app_session_manager_t *manager,
                                              const char *id,
                                              app_session_t **session);

APP_NODISCARD app_error app_session_get_by_pid(app_session_manager_t *manager,
                                               pid_t pid,
                                               app_session_t **session);

APP_NODISCARD app_error app_session_list(app_session_manager_t *manager,
                                         app_session_info_t ***sessions,
                                         size_t *count);

// Session persistence
APP_NODISCARD app_error app_session_save(app_session_t *session,
                                         const char *name);

APP_NODISCARD app_error app_session_restore(app_session_manager_t *manager,
                                            app_session_t **session,
                                            const char *name);

APP_NODISCARD app_error app_session_load(app_session_manager_t *manager,
                                         const char *path,
                                         app_session_t **session);

// Utility functions
APP_NODISCARD app_error app_session_set_name(app_session_t *session,
                                             const char *name);

APP_NODISCARD app_error
app_session_set_working_directory(app_session_t *session, const char *dir);

APP_NODISCARD app_error app_session_add_environment(app_session_t *session,
                                                    const char *key,
                                                    const char *value);