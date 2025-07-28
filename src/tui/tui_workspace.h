#pragma once

#include "../core/error.h"

// Forward declarations
typedef struct app_tui_mux app_tui_mux_t;

// Workspace metadata
typedef struct {
  char *workspace_id;
  char *name;
  char *created_at;
  char *last_active;
} app_workspace_metadata_t;

// Pane state for persistence
typedef struct {
  char *pane_id;
  int position_row;
  int position_col;
  char *account_id;
  char *session_id;
  double size_weight;
  char *working_directory;
} app_workspace_pane_t;

// Layout state for persistence
typedef struct {
  char *mode;  // "grid", "vsplit", "hsplit", "focus"
  int rows;
  int cols;
  app_workspace_pane_t **panes;
  size_t pane_count;
} app_workspace_layout_t;

// Complete workspace state
typedef struct {
  app_workspace_metadata_t metadata;
  app_workspace_layout_t layout;
  // Future: keybindings, settings, etc.
} app_workspace_t;

// Workspace operations
APP_NODISCARD app_error app_tui_workspace_save(app_tui_mux_t *mux,
                                               const char *path);
APP_NODISCARD app_error app_tui_workspace_load(app_tui_mux_t *mux,
                                               const char *path);

// Workspace management
APP_NODISCARD app_error app_tui_workspace_list(char ***workspaces,
                                               size_t *count);
APP_NODISCARD app_error app_tui_workspace_delete(const char *workspace_id);
APP_NODISCARD app_error app_tui_workspace_get_default_path(char *path,
                                                           size_t size);

// Workspace serialization (internal)
APP_NODISCARD app_error app_tui_workspace_to_json(app_workspace_t *workspace,
                                                  char **json);
APP_NODISCARD app_error
app_tui_workspace_from_json(const char *json, app_workspace_t **workspace);
APP_NODISCARD app_error app_tui_workspace_free(app_workspace_t *workspace);