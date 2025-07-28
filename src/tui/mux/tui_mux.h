#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <core/error.h>
#include <core/types.h>
#include "tui/tui.h"
#include "tui/tui_layout.h"
#include "tui/panes/tui_pane.h"

// Forward declarations
typedef struct app_tui_mux app_tui_mux_t;
typedef struct app_session_manager app_session_manager_t;

// Multiplexer states
typedef enum {
  MUX_STATE_NORMAL,   // Normal operation
  MUX_STATE_COMMAND,  // Command mode
  MUX_STATE_RESIZE,   // Resize mode
  MUX_STATE_EXITING   // Shutting down
} app_mux_state_t;

// Input modes
typedef enum {
  INPUT_MODE_NORMAL,   // Normal mode (navigation)
  INPUT_MODE_COMMAND,  // Command mode (typing commands)
  INPUT_MODE_INSERT,   // Insert mode (typing in pane)
  INPUT_MODE_RESIZE,   // Resize mode
  INPUT_MODE_WINDOW,   // Window command mode (after Ctrl-W)
  INPUT_MODE_SEARCH,   // Search mode
  INPUT_MODE_VISUAL,   // Visual selection mode
} app_input_mode_t;

// Command history
typedef struct {
  char **commands;  // Array of commands
  size_t count;     // Number of commands
  size_t capacity;  // Array capacity
  size_t current;   // Current position in history
} app_command_history_t;

// Mux-specific keybinding (different from input system keybinding)
typedef struct {
  int key;              // Key code
  int modifiers;        // Modifier keys (Ctrl, Alt, etc.)
  const char *command;  // Command to execute
} app_mux_keybinding_t;

// Multiplexer configuration
typedef struct {
  bool enable_mouse;                     // Enable mouse support
  bool enable_colors;                    // Enable color support
  app_tui_layout_mode_t default_layout;  // Default layout mode
  int max_panes;                         // Maximum number of panes
  int refresh_rate_ms;                   // Refresh rate in milliseconds

  // Layout constraints
  int min_pane_width;   // Minimum pane width
  int min_pane_height;  // Minimum pane height

  // Navigation options
  bool wrap_navigation;  // Wrap around when navigating past edges

  // Keybindings
  app_mux_keybinding_t *keybindings;  // Array of keybindings
  size_t keybinding_count;            // Number of keybindings
} app_tui_mux_config_t;

// Forward declaration
typedef struct app_tui_input app_tui_input_t;

// Multiplexer structure
struct app_tui_mux {
  // State
  app_mux_state_t state;        // Current state
  app_input_mode_t input_mode;  // Current input mode
  bool running;                 // Main loop running

  // Components
  app_tui_pane_manager_t *pane_manager;      // Pane manager
  app_tui_layout_manager_t *layout_manager;  // Layout manager
  app_session_manager_t *session_manager;    // Session manager
  app_tui_input_t *input_system;             // Input system

  // UI elements
  WINDOW *status_bar;    // Status bar window
  WINDOW *command_line;  // Command line window

  // Command mode
  char *command_buffer;            // Current command being typed
  size_t command_buffer_size;      // Buffer size
  size_t command_cursor;           // Cursor position
  app_command_history_t *history;  // Command history

  // Configuration
  app_tui_mux_config_t config;  // Configuration

  // Workspace
  char *workspace_id;    // Current workspace ID
  char *workspace_name;  // Workspace name
};

// Multiplexer lifecycle
APP_NODISCARD app_error
app_tui_mux_create(app_tui_mux_t **mux, const app_tui_mux_config_t *config,
                   app_session_manager_t *session_manager);

APP_NODISCARD app_error app_tui_mux_destroy(app_tui_mux_t *mux);

// Main operations
APP_NODISCARD app_error app_tui_mux_init(app_tui_mux_t *mux);

APP_NODISCARD app_error app_tui_mux_run(app_tui_mux_t *mux);

// Enhanced event loop with better timing and PTY monitoring
APP_NODISCARD app_error app_tui_mux_run_enhanced(app_tui_mux_t *mux);

APP_NODISCARD app_error app_tui_mux_shutdown(app_tui_mux_t *mux);

// Pane operations
APP_NODISCARD app_error app_tui_mux_add_pane(app_tui_mux_t *mux,
                                             const char *account_id,
                                             app_tui_pane_t **pane);

APP_NODISCARD app_error app_tui_mux_remove_pane(app_tui_mux_t *mux,
                                                app_tui_pane_t *pane);

APP_NODISCARD app_error app_tui_mux_split_pane(app_tui_mux_t *mux,
                                               app_split_direction_t direction);

// Navigation
APP_NODISCARD app_error app_tui_mux_focus_pane(app_tui_mux_t *mux,
                                               app_tui_pane_t *pane);

APP_NODISCARD app_error app_tui_mux_focus_direction(app_tui_mux_t *mux, int dx,
                                                    int dy);

APP_NODISCARD app_error app_tui_mux_focus_number(app_tui_mux_t *mux,
                                                 int number);

APP_NODISCARD app_error app_tui_mux_cycle_focus(app_tui_mux_t *mux,
                                                bool reverse);

APP_NODISCARD app_error app_tui_mux_focus_last(app_tui_mux_t *mux);

APP_NODISCARD app_error app_tui_mux_register_nav_keys(app_tui_mux_t *mux);

// Layout operations
APP_NODISCARD app_error app_tui_mux_set_layout(app_tui_mux_t *mux,
                                               app_tui_layout_mode_t mode);

APP_NODISCARD app_error app_tui_mux_toggle_focus_mode(app_tui_mux_t *mux);

// Command mode
APP_NODISCARD app_error app_tui_mux_enter_command_mode(app_tui_mux_t *mux);

APP_NODISCARD app_error app_tui_mux_exit_command_mode(app_tui_mux_t *mux);

APP_NODISCARD app_error app_tui_mux_execute_command(app_tui_mux_t *mux,
                                                    const char *command);

// Input handling
APP_NODISCARD app_error app_tui_mux_handle_input(app_tui_mux_t *mux, int key);

APP_NODISCARD app_error app_tui_mux_handle_mouse(app_tui_mux_t *mux,
                                                 MEVENT *event);

// Rendering
APP_NODISCARD app_error app_tui_mux_render(app_tui_mux_t *mux);

APP_NODISCARD app_error app_tui_mux_render_status_bar(app_tui_mux_t *mux);

APP_NODISCARD app_error app_tui_mux_render_command_line(app_tui_mux_t *mux);

// Workspace operations
APP_NODISCARD app_error app_tui_mux_save_workspace(app_tui_mux_t *mux,
                                                   const char *path);

APP_NODISCARD app_error app_tui_mux_load_workspace(app_tui_mux_t *mux,
                                                   const char *path);

// Utility functions
APP_NODISCARD app_error app_tui_mux_broadcast_input(app_tui_mux_t *mux,
                                                    const char *input);

APP_NODISCARD app_error app_tui_mux_show_notification(app_tui_mux_t *mux,
                                                      const char *message,
                                                      int duration_ms);