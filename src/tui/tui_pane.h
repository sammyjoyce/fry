#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <time.h>

#include "../core/error.h"
#include "../core/types.h"
#include "tui.h"

// Forward declarations
typedef struct app_tui_pane app_tui_pane_t;
typedef struct app_tui_pane_manager app_tui_pane_manager_t;
typedef struct app_session app_session_t;
typedef struct tui_term_emulator_t tui_term_emulator_t;

// Pane state
typedef enum {
  PANE_STATE_INACTIVE,
  PANE_STATE_CONNECTING,
  PANE_STATE_ACTIVE,
  PANE_STATE_DISCONNECTED,
  PANE_STATE_ERROR
} app_pane_state_t;

// Pane geometry
typedef struct {
  int x;
  int y;
  int width;
  int height;
} app_pane_rect_t;

// Pane structure
struct app_tui_pane {
  char *id;                     // Unique pane identifier
  int number;                   // Pane number (1-9)
  tui_window_t *window;         // NCurses window
  tui_window_t *border_window;  // Border decoration window

  // Session information
  char *session_id;   // Associated session ID
  char *account_id;   // Associated account ID
  pid_t process_pid;  // Claude Code process PID

  // PTY information
  int pty_master;  // PTY master file descriptor
  int pty_slave;   // PTY slave file descriptor
  char *pty_name;  // PTY device name

  // State
  app_pane_state_t state;  // Current pane state
  bool has_focus;          // Whether pane has focus
  time_t last_activity;    // Last activity timestamp
  time_t last_focus_time;  // When pane last had focus

  // Content
  char **buffer;         // Scrollback buffer (legacy)
  size_t buffer_size;    // Buffer capacity
  size_t buffer_lines;   // Current lines in buffer
  size_t scroll_offset;  // Current scroll position

  // Terminal emulator
  tui_term_emulator_t *term_emulator;  // ANSI escape sequence processor

  // Metadata
  char *title;               // Pane title
  app_pane_rect_t geometry;  // Position and size
};

// Pane manager structure
struct app_tui_pane_manager {
  app_tui_pane_t **panes;        // Array of panes
  size_t pane_count;             // Current number of panes
  size_t max_panes;              // Maximum allowed panes
  app_tui_pane_t *focused_pane;  // Currently focused pane

  // Layout information
  int terminal_width;   // Terminal width
  int terminal_height;  // Terminal height
  bool needs_refresh;   // Layout needs refresh
};

// Pane lifecycle
APP_NODISCARD app_error app_tui_pane_create(app_tui_pane_manager_t *manager,
                                            const char *account_id,
                                            const app_pane_rect_t *geometry,
                                            app_tui_pane_t **pane);

APP_NODISCARD app_error app_tui_pane_destroy(app_tui_pane_manager_t *manager,
                                             app_tui_pane_t *pane);

// Pane manager
APP_NODISCARD app_error
app_tui_pane_manager_create(app_tui_pane_manager_t **manager, size_t max_panes);

APP_NODISCARD app_error
app_tui_pane_manager_destroy(app_tui_pane_manager_t *manager);

// Pane manager queries
size_t app_tui_pane_manager_get_count(app_tui_pane_manager_t *manager);
app_tui_pane_t *app_tui_pane_manager_get_by_index(
    app_tui_pane_manager_t *manager, size_t index);
app_tui_pane_t *app_tui_pane_manager_get_focused(
    app_tui_pane_manager_t *manager);

// Focus management
APP_NODISCARD app_error app_tui_pane_focus(app_tui_pane_manager_t *manager,
                                           app_tui_pane_t *pane);

APP_NODISCARD app_error
app_tui_pane_focus_by_number(app_tui_pane_manager_t *manager, int number);

APP_NODISCARD app_error
app_tui_pane_focus_next(app_tui_pane_manager_t *manager);

APP_NODISCARD app_error
app_tui_pane_focus_prev(app_tui_pane_manager_t *manager);

// Navigation
APP_NODISCARD app_error
app_tui_pane_focus_direction(app_tui_pane_manager_t *manager, int dx, int dy);

// Pane operations
APP_NODISCARD app_error app_tui_pane_set_state(app_tui_pane_t *pane,
                                               app_pane_state_t state);

APP_NODISCARD app_error app_tui_pane_set_title(app_tui_pane_t *pane,
                                               const char *title);

APP_NODISCARD app_error
app_tui_pane_resize(app_tui_pane_t *pane, const app_pane_rect_t *new_geometry);

// Content management
APP_NODISCARD app_error app_tui_pane_write(app_tui_pane_t *pane,
                                           const char *data, size_t len);

APP_NODISCARD app_error app_tui_pane_scroll(app_tui_pane_t *pane, int lines);

APP_NODISCARD app_error app_tui_pane_clear(app_tui_pane_t *pane);

// Rendering
APP_NODISCARD app_error app_tui_pane_render(app_tui_pane_t *pane);

APP_NODISCARD app_error app_tui_pane_render_border(app_tui_pane_t *pane);

APP_NODISCARD app_error app_tui_pane_refresh(app_tui_pane_t *pane);
APP_NODISCARD app_error app_tui_pane_scroll(app_tui_pane_t *pane, int delta);
APP_NODISCARD app_error app_tui_pane_scroll_to(app_tui_pane_t *pane,
                                               int position);

// Utility functions
app_tui_pane_t *app_tui_pane_get_by_id(app_tui_pane_manager_t *manager,
                                       const char *id);

app_tui_pane_t *app_tui_pane_get_by_number(app_tui_pane_manager_t *manager,
                                           int number);

APP_NODISCARD app_error app_tui_pane_manager_update_size(
    app_tui_pane_manager_t *manager, int width, int height);

// Process management
APP_NODISCARD app_error app_tui_pane_launch_claude(app_tui_pane_t *pane,
                                                   const char *account_id,
                                                   app_session_t *session);

APP_NODISCARD app_error app_tui_pane_check_process(app_tui_pane_t *pane);

APP_NODISCARD app_error app_tui_pane_terminate_process(app_tui_pane_t *pane);

// Pane properties
int app_tui_pane_get_number(app_tui_pane_t *pane);