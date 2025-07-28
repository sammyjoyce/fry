#include "tui_workspace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "../utils/json.h"
#include "../utils/logging.h"
#include "../utils/memory.h"
#include "tui_layout.h"
#include "tui_mux.h"
#include "tui_pane.h"

// Get default workspace path
app_error app_tui_workspace_get_default_path(char *path, size_t size) {
  if (!path || size == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  const char *home = getenv("HOME");
  if (!home) {
    return APP_ERROR_ENV;
  }

  snprintf(path, size, "%s/.cache/fry/workspaces/default.json", home);
  return APP_SUCCESS;
}

// Save workspace to file
app_error app_tui_workspace_save(app_tui_mux_t *mux, const char *path) {
  if (!mux || !path) {
    return APP_ERROR_INVALID_ARG;
  }

  LOG_INFO("Saving workspace to: %s", path);

  // Create workspace structure
  app_workspace_t workspace = {0};

  // Set metadata
  workspace.metadata.workspace_id =
      mux->workspace_id ? app_strdup(mux->workspace_id) : app_strdup("default");
  workspace.metadata.name = mux->workspace_name
                                ? app_strdup(mux->workspace_name)
                                : app_strdup("Default Workspace");

  // Set timestamps
  char timestamp[64];
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm);
  workspace.metadata.last_active = app_strdup(timestamp);
  workspace.metadata.created_at =
      app_strdup(timestamp);  // TODO: Track actual creation time

  // Set layout
  app_tui_layout_mode_t current_mode;
  app_tui_layout_get_mode(mux->layout_manager, &current_mode);
  switch (current_mode) {
  case TUI_LAYOUT_GRID:
    workspace.layout.mode = app_strdup("grid");
    break;
  case TUI_LAYOUT_VSPLIT:
    workspace.layout.mode = app_strdup("vsplit");
    break;
  case TUI_LAYOUT_HSPLIT:
    workspace.layout.mode = app_strdup("hsplit");
    break;
  case TUI_LAYOUT_CUSTOM:
    workspace.layout.mode = app_strdup("custom");
    break;
  case TUI_LAYOUT_FOCUS:
    workspace.layout.mode = app_strdup("focus");
    break;
  }

  // Save pane states
  size_t pane_count = app_tui_pane_manager_get_count(mux->pane_manager);

  // Get grid dimensions
  int rows, cols;
  app_tui_layout_calculate_grid_dimensions((int)pane_count, &rows, &cols);
  workspace.layout.rows = rows;
  workspace.layout.cols = cols;
  workspace.layout.pane_count = pane_count;
  workspace.layout.panes =
      app_calloc(pane_count, sizeof(app_workspace_pane_t *));
  if (!workspace.layout.panes) {
    app_tui_workspace_free(&workspace);
    return APP_ERROR_MEMORY;
  }

  for (size_t i = 0; i < pane_count; i++) {
    app_tui_pane_t *pane =
        app_tui_pane_manager_get_by_index(mux->pane_manager, i);
    if (!pane)
      continue;

    app_workspace_pane_t *ws_pane = app_calloc(1, sizeof(app_workspace_pane_t));
    if (!ws_pane) {
      app_tui_workspace_free(&workspace);
      return APP_ERROR_MEMORY;
    }

    // Generate pane ID
    char pane_id[32];
    snprintf(pane_id, sizeof(pane_id), "pane_%03zu", i + 1);
    ws_pane->pane_id = app_strdup(pane_id);

    // Get position in grid
    ws_pane->position_row = i / cols;
    ws_pane->position_col = i % cols;

    // Get account and session info
    ws_pane->account_id =
        pane->account_id ? app_strdup(pane->account_id) : app_strdup("");
    ws_pane->session_id =
        pane->session_id ? app_strdup(pane->session_id) : app_strdup("");

    ws_pane->size_weight = 1.0;  // TODO: Track actual size weights
    ws_pane->working_directory =
        app_strdup(getenv("PWD") ? getenv("PWD") : "/");

    workspace.layout.panes[i] = ws_pane;
  }

  // Convert to JSON
  char *json = NULL;
  app_error err = app_tui_workspace_to_json(&workspace, &json);
  if (err != APP_SUCCESS) {
    app_tui_workspace_free(&workspace);
    return err;
  }

  // Ensure directory exists
  char *dir = app_strdup(path);
  char *last_slash = strrchr(dir, '/');
  if (last_slash) {
    *last_slash = '\0';
    mkdir(dir, 0755);  // Ignore errors, will fail on write if needed
  }
  app_free(dir);

  // Write to file
  FILE *file = fopen(path, "w");
  if (!file) {
    app_free(json);
    app_tui_workspace_free(&workspace);
    return APP_ERROR_IO;
  }

  fprintf(file, "%s\n", json);
  fclose(file);

  app_free(json);
  app_tui_workspace_free(&workspace);

  LOG_INFO("Workspace saved successfully");
  return APP_SUCCESS;
}

// Load workspace from file
app_error app_tui_workspace_load(app_tui_mux_t *mux, const char *path) {
  if (!mux || !path) {
    return APP_ERROR_INVALID_ARG;
  }

  LOG_INFO("Loading workspace from: %s", path);

  // Read file
  FILE *file = fopen(path, "r");
  if (!file) {
    LOG_ERROR("Failed to open workspace file: %s", path);
    return APP_ERROR_IO;
  }

  // Get file size
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (size <= 0 || size > 1024 * 1024) {  // Max 1MB workspace file
    fclose(file);
    return APP_ERROR_INVALID_DATA;
  }

  // Read content
  char *json = app_malloc(size + 1);
  if (!json) {
    fclose(file);
    return APP_ERROR_MEMORY;
  }

  size_t read = fread(json, 1, size, file);
  fclose(file);

  if (read != (size_t)size) {
    app_free(json);
    return APP_ERROR_IO;
  }
  json[size] = '\0';

  // Parse JSON
  app_workspace_t *workspace = NULL;
  app_error err = app_tui_workspace_from_json(json, &workspace);
  app_free(json);

  if (err != APP_SUCCESS) {
    return err;
  }

  // Clear existing panes
  size_t current_count = app_tui_pane_manager_get_count(mux->pane_manager);
  for (size_t i = current_count; i > 0; i--) {
    app_tui_pane_t *pane =
        app_tui_pane_manager_get_by_index(mux->pane_manager, i - 1);
    if (pane) {
      (void)app_tui_mux_remove_pane(mux, pane);
    }
  }

  // Update workspace info
  app_free(mux->workspace_id);
  app_free(mux->workspace_name);
  mux->workspace_id = workspace->metadata.workspace_id
                          ? app_strdup(workspace->metadata.workspace_id)
                          : NULL;
  mux->workspace_name =
      workspace->metadata.name ? app_strdup(workspace->metadata.name) : NULL;

  // Set layout mode
  app_tui_layout_mode_t mode = TUI_LAYOUT_GRID;
  if (workspace->layout.mode) {
    if (strcmp(workspace->layout.mode, "vsplit") == 0) {
      mode = TUI_LAYOUT_VSPLIT;
    } else if (strcmp(workspace->layout.mode, "hsplit") == 0) {
      mode = TUI_LAYOUT_HSPLIT;
    } else if (strcmp(workspace->layout.mode, "focus") == 0) {
      mode = TUI_LAYOUT_FOCUS;
    }
  }
  (void)app_tui_mux_set_layout(mux, mode);

  // Restore panes
  for (size_t i = 0; i < workspace->layout.pane_count; i++) {
    app_workspace_pane_t *ws_pane = workspace->layout.panes[i];
    if (!ws_pane)
      continue;

    app_tui_pane_t *pane = NULL;
    err = app_tui_mux_add_pane(mux, ws_pane->account_id, &pane);
    if (err != APP_SUCCESS) {
      LOG_WARNING("Failed to restore pane %s: %s", ws_pane->pane_id,
                  app_error_string(err));
    }

    // TODO: Restore working directory
    // TODO: Attempt to reconnect to session if still running
  }

  app_tui_workspace_free(workspace);

  LOG_INFO("Workspace loaded successfully");
  return APP_SUCCESS;
}

// Convert workspace to JSON
app_error app_tui_workspace_to_json(app_workspace_t *workspace, char **json) {
  if (!workspace || !json) {
    return APP_ERROR_INVALID_ARG;
  }

  // Create root object
  json_value_t *root = json_value_object();
  if (!root) {
    return APP_ERROR_MEMORY;
  }

  // Create metadata object
  json_value_t *metadata = json_value_object();
  json_object_set(metadata->object_val, "workspace_id",
                  json_value_string(workspace->metadata.workspace_id
                                        ? workspace->metadata.workspace_id
                                        : ""));
  json_object_set(
      metadata->object_val, "name",
      json_value_string(workspace->metadata.name ? workspace->metadata.name
                                                 : ""));
  json_object_set(metadata->object_val, "created_at",
                  json_value_string(workspace->metadata.created_at
                                        ? workspace->metadata.created_at
                                        : ""));
  if (json_object_set(metadata->object_val, "last_active",
                      json_value_string(workspace->metadata.last_active
                                            ? workspace->metadata.last_active
                                            : "")) != 0) {
    json_value_destroy(metadata);
    json_value_destroy(root);
    return APP_ERROR_PARSE;
  }
  if (json_object_set(root->object_val, "metadata", metadata) != 0) {
    json_value_destroy(metadata);
    json_value_destroy(root);
    return APP_ERROR_PARSE;
  }

  // Create layout object
  json_value_t *layout = json_value_object();
  if (json_object_set(layout->object_val, "mode",
                      json_value_string(workspace->layout.mode ? workspace->layout.mode
                                                               : "grid")) != 0) {
    json_value_destroy(layout);
    json_value_destroy(root);
    return APP_ERROR_PARSE;
  }
  if (json_object_set(layout->object_val, "rows",
                      json_value_number(workspace->layout.rows)) != 0) {
    json_value_destroy(layout);
    json_value_destroy(root);
    return APP_ERROR_PARSE;
  }
  if (json_object_set(layout->object_val, "cols",
                      json_value_number(workspace->layout.cols)) != 0) {
    json_value_destroy(layout);
    json_value_destroy(root);
    return APP_ERROR_PARSE;
  }

  // Create panes array
  json_value_t *panes = json_value_array();
  for (size_t i = 0; i < workspace->layout.pane_count; i++) {
    app_workspace_pane_t *ws_pane = workspace->layout.panes[i];
    if (!ws_pane)
      continue;

    json_value_t *pane = json_value_object();
    if (json_object_set(pane->object_val, "id",
                        json_value_string(ws_pane->pane_id ? ws_pane->pane_id : "")) != 0) {
      json_value_destroy(pane);
      json_value_destroy(panes);
      json_value_destroy(layout);
      json_value_destroy(root);
      return APP_ERROR_PARSE;
    }

    json_value_t *position = json_value_object();
    if (json_object_set(position->object_val, "row",
                        json_value_number(ws_pane->position_row)) != 0) {
      json_value_destroy(position);
      json_value_destroy(pane);
      json_value_destroy(panes);
      json_value_destroy(layout);
      json_value_destroy(root);
      return APP_ERROR_PARSE;
    }
    if (json_object_set(position->object_val, "col",
                        json_value_number(ws_pane->position_col)) != 0) {
      json_value_destroy(position);
      json_value_destroy(pane);
      json_value_destroy(panes);
      json_value_destroy(layout);
      json_value_destroy(root);
      return APP_ERROR_PARSE;
    }
    if (json_object_set(pane->object_val, "position", position) != 0) {
      json_value_destroy(position);
      json_value_destroy(pane);
      json_value_destroy(panes);
      json_value_destroy(layout);
      json_value_destroy(root);
      return APP_ERROR_PARSE;
    }

    if (json_object_set(pane->object_val, "account_id",
                        json_value_string(ws_pane->account_id ? ws_pane->account_id : "")) != 0) {
      json_value_destroy(pane);
      json_value_destroy(panes);
      json_value_destroy(layout);
      json_value_destroy(root);
      return APP_ERROR_PARSE;
    }
    if (json_object_set(pane->object_val, "session_id",
                        json_value_string(ws_pane->session_id ? ws_pane->session_id : "")) != 0) {
      json_value_destroy(pane);
      json_value_destroy(panes);
      json_value_destroy(layout);
      json_value_destroy(root);
      return APP_ERROR_PARSE;
    }
    if (json_object_set(pane->object_val, "size_weight",
                        json_value_number(ws_pane->size_weight)) != 0) {
      json_value_destroy(pane);
      json_value_destroy(panes);
      json_value_destroy(layout);
      json_value_destroy(root);
      return APP_ERROR_PARSE;
    }
    if (json_object_set(pane->object_val, "working_directory",
                        json_value_string(ws_pane->working_directory
                                              ? ws_pane->working_directory
                                              : "")) != 0) {
      json_value_destroy(pane);
      json_value_destroy(panes);
      json_value_destroy(layout);
      json_value_destroy(root);
      return APP_ERROR_PARSE;
    }

    json_array_append(panes->array_val, pane);
  }
  json_object_set(layout->object_val, "panes", panes);
  json_object_set(root->object_val, "layout", layout);

  // Convert to string
  app_error err = json_stringify_pretty(json, root);
  json_value_destroy(root);

  return err;
}

// Parse workspace from JSON
app_error app_tui_workspace_from_json(const char *json_str,
                                      app_workspace_t **workspace) {
  if (!json_str || !workspace) {
    return APP_ERROR_INVALID_ARG;
  }

  // Parse JSON
  json_value_t *root = NULL;
  app_error err = json_parse(&root, json_str);
  if (err != APP_SUCCESS || !root) {
    return APP_ERROR_PARSE;
  }

  // Allocate workspace
  app_workspace_t *ws = app_calloc(1, sizeof(app_workspace_t));
  if (!ws) {
    json_value_destroy(root);
    return APP_ERROR_MEMORY;
  }

  // Parse metadata
  json_value_t *metadata = json_object_get(root->object_val, "metadata");
  if (metadata && metadata->type == JSON_TYPE_OBJECT) {
    json_value_t *val;

    val = json_object_get(metadata->object_val, "workspace_id");
    if (val && val->type == JSON_TYPE_STRING) {
      ws->metadata.workspace_id = app_strdup(val->string_val);
    }

    val = json_object_get(metadata->object_val, "name");
    if (val && val->type == JSON_TYPE_STRING) {
      ws->metadata.name = app_strdup(val->string_val);
    }

    val = json_object_get(metadata->object_val, "created_at");
    if (val && val->type == JSON_TYPE_STRING) {
      ws->metadata.created_at = app_strdup(val->string_val);
    }

    val = json_object_get(metadata->object_val, "last_active");
    if (val && val->type == JSON_TYPE_STRING) {
      ws->metadata.last_active = app_strdup(val->string_val);
    }
  }

  // Parse layout
  json_value_t *layout = json_object_get(root->object_val, "layout");
  if (layout && layout->type == JSON_TYPE_OBJECT) {
    json_value_t *val;

    val = json_object_get(layout->object_val, "mode");
    if (val && val->type == JSON_TYPE_STRING) {
      ws->layout.mode = app_strdup(val->string_val);
    }

    val = json_object_get(layout->object_val, "rows");
    if (val && val->type == JSON_TYPE_NUMBER) {
      ws->layout.rows = (int)val->number_val;
    }

    val = json_object_get(layout->object_val, "cols");
    if (val && val->type == JSON_TYPE_NUMBER) {
      ws->layout.cols = (int)val->number_val;
    }

    // Parse panes
    json_value_t *panes = json_object_get(layout->object_val, "panes");
    if (panes && panes->type == JSON_TYPE_ARRAY) {
      ws->layout.pane_count = panes->array_val->count;
      ws->layout.panes =
          app_calloc(ws->layout.pane_count, sizeof(app_workspace_pane_t *));

      if (!ws->layout.panes) {
        app_tui_workspace_free(ws);
        json_value_destroy(root);
        return APP_ERROR_MEMORY;
      }

      for (size_t i = 0; i < ws->layout.pane_count; i++) {
        json_value_t *pane = json_array_get(panes->array_val, i);
        if (!pane || pane->type != JSON_TYPE_OBJECT)
          continue;

        app_workspace_pane_t *ws_pane =
            app_calloc(1, sizeof(app_workspace_pane_t));
        if (!ws_pane) {
          app_tui_workspace_free(ws);
          json_value_destroy(root);
          return APP_ERROR_MEMORY;
        }

        val = json_object_get(pane->object_val, "id");
        if (val && val->type == JSON_TYPE_STRING) {
          ws_pane->pane_id = app_strdup(val->string_val);
        }

        json_value_t *position = json_object_get(pane->object_val, "position");
        if (position && position->type == JSON_TYPE_OBJECT) {
          val = json_object_get(position->object_val, "row");
          if (val && val->type == JSON_TYPE_NUMBER) {
            ws_pane->position_row = (int)val->number_val;
          }

          val = json_object_get(position->object_val, "col");
          if (val && val->type == JSON_TYPE_NUMBER) {
            ws_pane->position_col = (int)val->number_val;
          }
        }

        val = json_object_get(pane->object_val, "account_id");
        if (val && val->type == JSON_TYPE_STRING) {
          ws_pane->account_id = app_strdup(val->string_val);
        }

        val = json_object_get(pane->object_val, "session_id");
        if (val && val->type == JSON_TYPE_STRING) {
          ws_pane->session_id = app_strdup(val->string_val);
        }

        val = json_object_get(pane->object_val, "size_weight");
        if (val && val->type == JSON_TYPE_NUMBER) {
          ws_pane->size_weight = val->number_val;
        }

        val = json_object_get(pane->object_val, "working_directory");
        if (val && val->type == JSON_TYPE_STRING) {
          ws_pane->working_directory = app_strdup(val->string_val);
        }

        ws->layout.panes[i] = ws_pane;
      }
    }
  }

  json_value_destroy(root);
  *workspace = ws;
  return APP_SUCCESS;
}

// Free workspace structure
app_error app_tui_workspace_free(app_workspace_t *workspace) {
  if (!workspace) {
    return APP_SUCCESS;
  }

  // Free metadata
  app_free(workspace->metadata.workspace_id);
  app_free(workspace->metadata.name);
  app_free(workspace->metadata.created_at);
  app_free(workspace->metadata.last_active);

  // Free layout
  app_free(workspace->layout.mode);

  // Free panes
  for (size_t i = 0; i < workspace->layout.pane_count; i++) {
    app_workspace_pane_t *pane = workspace->layout.panes[i];
    if (pane) {
      app_free(pane->pane_id);
      app_free(pane->account_id);
      app_free(pane->session_id);
      app_free(pane->working_directory);
      app_free(pane);
    }
  }
  app_free(workspace->layout.panes);

  return APP_SUCCESS;
}