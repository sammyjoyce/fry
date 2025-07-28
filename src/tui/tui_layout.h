#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <core/error.h>
#include <core/types.h>
#include "panes/tui_pane.h"

// Layout modes
typedef enum {
  TUI_LAYOUT_GRID,    // Automatic grid layout
  TUI_LAYOUT_VSPLIT,  // Vertical split
  TUI_LAYOUT_HSPLIT,  // Horizontal split
  TUI_LAYOUT_FOCUS,   // Single pane maximized
  TUI_LAYOUT_CUSTOM   // User-defined layout
} app_tui_layout_mode_t;

// Split direction
typedef enum { SPLIT_HORIZONTAL, SPLIT_VERTICAL } app_split_direction_t;

// Layout node for tree-based layouts
typedef struct app_layout_node {
  bool is_leaf;  // True if this is a pane, false if split

  union {
    // For leaf nodes (panes)
    struct {
      app_tui_pane_t *pane;  // Associated pane
      float size_weight;     // Relative size weight
    } leaf;

    // For split nodes
    struct {
      struct app_layout_node *first;    // First child
      struct app_layout_node *second;   // Second child
      app_split_direction_t direction;  // Split direction
      float split_ratio;                // Split position (0.0-1.0)
    } split;
  } data;

  struct app_layout_node *parent;  // Parent node
  app_pane_rect_t geometry;        // Calculated geometry
} app_layout_node_t;

// Layout manager
typedef struct {
  app_tui_layout_mode_t mode;  // Current layout mode
  app_layout_node_t *root;     // Root of layout tree

  // Grid layout parameters
  int grid_rows;  // Number of rows in grid
  int grid_cols;  // Number of columns in grid

  // Terminal dimensions
  int terminal_width;   // Terminal width
  int terminal_height;  // Terminal height

  // Layout constraints
  int min_pane_width;     // Minimum pane width
  int min_pane_height;    // Minimum pane height
  int status_bar_height;  // Height reserved for status bar

  // Focus mode state
  app_layout_node_t *focused_node;  // Node that was maximized
  app_tui_layout_mode_t prev_mode;  // Mode before entering focus
} app_tui_layout_manager_t;

// Layout manager lifecycle
APP_NODISCARD app_error
app_tui_layout_create(app_tui_layout_manager_t **manager, int terminal_width,
                      int terminal_height);

APP_NODISCARD app_error
app_tui_layout_destroy(app_tui_layout_manager_t *manager);

// Layout mode management
APP_NODISCARD app_error app_tui_layout_set_mode(
    app_tui_layout_manager_t *manager, app_tui_layout_mode_t mode);

APP_NODISCARD app_error app_tui_layout_get_mode(
    app_tui_layout_manager_t *manager, app_tui_layout_mode_t *mode);

// Grid layout operations
APP_NODISCARD app_error
app_tui_layout_arrange_grid(app_tui_layout_manager_t *manager,
                            app_tui_pane_t **panes, size_t pane_count);

APP_NODISCARD app_error app_tui_layout_calculate_grid_dimensions(int pane_count,
                                                                 int *rows,
                                                                 int *cols);

// Split layout operations
APP_NODISCARD app_error app_tui_layout_split_pane(
    app_tui_layout_manager_t *manager, app_tui_pane_t *pane,
    app_split_direction_t direction, app_tui_pane_t *new_pane);

APP_NODISCARD app_error app_tui_layout_remove_pane(
    app_tui_layout_manager_t *manager, app_tui_pane_t *pane);

// Focus mode operations
APP_NODISCARD app_error app_tui_layout_focus_pane(
    app_tui_layout_manager_t *manager, app_tui_pane_t *pane);

APP_NODISCARD app_error
app_tui_layout_unfocus(app_tui_layout_manager_t *manager);

// Resize operations
APP_NODISCARD app_error app_tui_layout_resize_terminal(
    app_tui_layout_manager_t *manager, int new_width, int new_height);

APP_NODISCARD app_error app_tui_layout_resize_pane(
    app_tui_layout_manager_t *manager, app_tui_pane_t *pane, int delta_width,
    int delta_height);

// Layout calculation
APP_NODISCARD app_error
app_tui_layout_calculate(app_tui_layout_manager_t *manager,
                         app_tui_pane_t **panes, size_t pane_count);

APP_NODISCARD app_error app_tui_layout_apply(app_tui_layout_manager_t *manager,
                                             app_tui_pane_t **panes,
                                             size_t pane_count);

// Utility functions
APP_NODISCARD app_error app_tui_layout_find_node(app_layout_node_t *root,
                                                 app_tui_pane_t *pane,
                                                 app_layout_node_t **node);

APP_NODISCARD app_error app_tui_layout_get_neighbor(
    app_tui_layout_manager_t *manager, app_tui_pane_t *pane, int dx, int dy,
    app_tui_pane_t **neighbor);

// Layout persistence
APP_NODISCARD app_error
app_tui_layout_to_json(app_tui_layout_manager_t *manager, char **json_str);

APP_NODISCARD app_error app_tui_layout_from_json(
    app_tui_layout_manager_t *manager, const char *json_str);