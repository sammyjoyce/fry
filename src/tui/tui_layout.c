#include "tui_layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utils/logging.h>
#include <utils/memory.h>

// Create layout manager
app_error app_tui_layout_create(app_tui_layout_manager_t **manager,
                                int terminal_width, int terminal_height) {
  if (!manager || terminal_width < 20 || terminal_height < 5) {
    return APP_ERROR_INVALID_ARG;
  }

  app_tui_layout_manager_t *mgr =
      app_calloc(1, sizeof(app_tui_layout_manager_t));
  if (!mgr) {
    return APP_ERROR_MEMORY;
  }

  mgr->mode = TUI_LAYOUT_GRID;
  mgr->root = NULL;
  mgr->terminal_width = terminal_width;
  mgr->terminal_height = terminal_height;
  mgr->min_pane_width = 20;
  mgr->min_pane_height = 5;
  mgr->status_bar_height = 1;
  mgr->focused_node = NULL;
  mgr->prev_mode = TUI_LAYOUT_GRID;

  *manager = mgr;
  return APP_SUCCESS;
}

// Destroy layout manager
app_error app_tui_layout_destroy(app_tui_layout_manager_t *manager) {
  if (!manager) {
    return APP_SUCCESS;
  }

  // TODO: Free layout tree nodes

  app_free(manager);
  return APP_SUCCESS;
}

// Calculate grid dimensions for N panes
app_error app_tui_layout_calculate_grid_dimensions(int pane_count, int *rows,
                                                   int *cols) {
  if (!rows || !cols || pane_count < 1) {
    return APP_ERROR_INVALID_ARG;
  }

  // Calculate optimal grid dimensions
  // Try to keep aspect ratio close to terminal aspect ratio
  if (pane_count == 1) {
    *rows = 1;
    *cols = 1;
  } else if (pane_count == 2) {
    *rows = 1;
    *cols = 2;
  } else if (pane_count <= 4) {
    *rows = 2;
    *cols = 2;
  } else if (pane_count <= 6) {
    *rows = 2;
    *cols = 3;
  } else if (pane_count <= 9) {
    *rows = 3;
    *cols = 3;
  } else {
    // More than 9 panes not supported in basic grid
    return APP_ERROR_OUT_OF_RANGE;
  }

  return APP_SUCCESS;
}

// Arrange panes in grid layout
app_error app_tui_layout_arrange_grid(app_tui_layout_manager_t *manager,
                                      app_tui_pane_t **panes,
                                      size_t pane_count) {
  if (!manager || !panes || pane_count == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  // Calculate grid dimensions
  int rows, cols;
  app_error err =
      app_tui_layout_calculate_grid_dimensions(pane_count, &rows, &cols);
  if (err != APP_SUCCESS) {
    return err;
  }

  manager->grid_rows = rows;
  manager->grid_cols = cols;

  // Calculate usable area (minus status bar)
  int usable_height = manager->terminal_height - manager->status_bar_height;

  // Calculate base pane dimensions
  int pane_width = manager->terminal_width / cols;
  int pane_height = usable_height / rows;

  // Distribute remainder pixels
  int width_remainder = manager->terminal_width % cols;
  int height_remainder = usable_height % rows;

  // Assign geometry to each pane
  size_t pane_idx = 0;
  int y_offset = 0;

  for (int row = 0; row < rows && pane_idx < pane_count; row++) {
    int x_offset = 0;
    int current_height = pane_height;

    // Add remainder height to last row
    if (row == rows - 1) {
      current_height += height_remainder;
    }

    for (int col = 0; col < cols && pane_idx < pane_count; col++) {
      int current_width = pane_width;

      // Add remainder width to last column
      if (col == cols - 1) {
        current_width += width_remainder;
      }

      // Set pane geometry
      app_pane_rect_t geometry = {.x = x_offset,
                                  .y = y_offset,
                                  .width = current_width,
                                  .height = current_height};

      app_error err = app_tui_pane_resize(panes[pane_idx], &geometry);
      if (err != APP_SUCCESS) {
        return err;
      }

      x_offset += current_width;
      pane_idx++;
    }

    y_offset += current_height;
  }

  LOG_DEBUG("Arranged %zu panes in %dx%d grid", pane_count, rows, cols);

  return APP_SUCCESS;
}

// Set layout mode
app_error app_tui_layout_set_mode(app_tui_layout_manager_t *manager,
                                  app_tui_layout_mode_t mode) {
  if (!manager) {
    return APP_ERROR_INVALID_ARG;
  }

  if (mode != manager->mode) {
    manager->prev_mode = manager->mode;
    manager->mode = mode;
    LOG_DEBUG("Layout mode changed from %d to %d", manager->prev_mode, mode);
  }

  return APP_SUCCESS;
}

// Get layout mode
app_error app_tui_layout_get_mode(app_tui_layout_manager_t *manager,
                                  app_tui_layout_mode_t *mode) {
  if (!manager || !mode) {
    return APP_ERROR_INVALID_ARG;
  }

  *mode = manager->mode;
  return APP_SUCCESS;
}

// Resize terminal
app_error app_tui_layout_resize_terminal(app_tui_layout_manager_t *manager,
                                         int new_width, int new_height) {
  if (!manager || new_width < 20 || new_height < 5) {
    return APP_ERROR_INVALID_ARG;
  }

  manager->terminal_width = new_width;
  manager->terminal_height = new_height;

  LOG_DEBUG("Terminal resized to %dx%d", new_width, new_height);

  return APP_SUCCESS;
}

// Calculate layout (main entry point)
app_error app_tui_layout_calculate(app_tui_layout_manager_t *manager,
                                   app_tui_pane_t **panes, size_t pane_count) {
  if (!manager || !panes || pane_count == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  switch (manager->mode) {
  case TUI_LAYOUT_GRID:
    return app_tui_layout_arrange_grid(manager, panes, pane_count);

  case TUI_LAYOUT_VSPLIT:
  case TUI_LAYOUT_HSPLIT:
  case TUI_LAYOUT_CUSTOM:
    // TODO: Implement tree-based layouts
    LOG_WARNING("Layout mode %d not yet implemented, falling back to grid",
                manager->mode);
    return app_tui_layout_arrange_grid(manager, panes, pane_count);

  case TUI_LAYOUT_FOCUS:
    // In focus mode, maximize the focused pane
    if (pane_count > 0) {
      app_pane_rect_t fullscreen = {
          .x = 0,
          .y = 0,
          .width = manager->terminal_width,
          .height = manager->terminal_height - manager->status_bar_height};

      // Find focused pane
      for (size_t i = 0; i < pane_count; i++) {
        if (panes[i]->has_focus) {
          app_error err = app_tui_pane_resize(panes[i], &fullscreen);
          if (err != APP_SUCCESS) {
            return err;
          }
          break;
        }
      }
    }
    return APP_SUCCESS;

  default:
    return APP_ERROR_INVALID_ARG;
  }
}

// Apply calculated layout to panes
app_error app_tui_layout_apply(app_tui_layout_manager_t *manager,
                               app_tui_pane_t **panes, size_t pane_count) {
  if (!manager || !panes || pane_count == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  // Calculate layout first
  app_error err = app_tui_layout_calculate(manager, panes, pane_count);
  if (err != APP_SUCCESS) {
    return err;
  }

  // Apply to NCurses windows
  for (size_t i = 0; i < pane_count; i++) {
    app_error err = app_tui_pane_refresh(panes[i]);
    if (err != APP_SUCCESS) {
      return err;
    }
  }

  return APP_SUCCESS;
}

// Get grid dimensions
void app_tui_layout_get_grid_dimensions(app_tui_layout_manager_t *manager,
                                        int *rows, int *cols) {
  if (!manager || !rows || !cols) {
    if (rows)
      *rows = 1;
    if (cols)
      *cols = 1;
    return;
  }

  *rows = manager->grid_rows;
  *cols = manager->grid_cols;
}