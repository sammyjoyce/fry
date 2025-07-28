/*
 * TUI Input System
 * Handles keyboard input processing, key sequence parsing, and keybindings
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <core/error.h>
#include <core/types.h>

// Key modifiers
typedef enum {
  KEY_MOD_NONE = 0,
  KEY_MOD_SHIFT = 1 << 0,
  KEY_MOD_CTRL = 1 << 1,
  KEY_MOD_ALT = 1 << 2,
  KEY_MOD_META = 1 << 3,  // Command key on macOS
} app_key_modifier_t;

// Special key codes (above ASCII range)
typedef enum {
  APP_KEY_UNKNOWN = -1,
  APP_KEY_ESCAPE = 27,
  APP_KEY_ENTER = '\n',
  APP_KEY_TAB = '\t',
  APP_KEY_BACKSPACE = 127,

  // Function keys
  APP_KEY_F1 = 1000,
  APP_KEY_F2,
  APP_KEY_F3,
  APP_KEY_F4,
  APP_KEY_F5,
  APP_KEY_F6,
  APP_KEY_F7,
  APP_KEY_F8,
  APP_KEY_F9,
  APP_KEY_F10,
  APP_KEY_F11,
  APP_KEY_F12,

  // Navigation keys
  APP_KEY_UP = 2000,
  APP_KEY_DOWN,
  APP_KEY_LEFT,
  APP_KEY_RIGHT,
  APP_KEY_HOME,
  APP_KEY_END,
  APP_KEY_PAGE_UP,
  APP_KEY_PAGE_DOWN,

  // Editing keys
  APP_KEY_INSERT = 3000,
  APP_KEY_DELETE,

  // Mouse events (if enabled)
  APP_KEY_MOUSE_CLICK = 4000,
  APP_KEY_MOUSE_RELEASE,
  APP_KEY_MOUSE_DRAG,
  APP_KEY_MOUSE_WHEEL_UP,
  APP_KEY_MOUSE_WHEEL_DOWN,
} app_key_code_t;

// Key event structure
typedef struct {
  int code;                      // Key code or character
  app_key_modifier_t modifiers;  // Active modifiers
  bool is_special;  // True for special keys (arrows, function keys)

  // Mouse-specific fields
  int mouse_x;
  int mouse_y;
  int mouse_button;  // 0=left, 1=middle, 2=right
} app_key_event_t;

// Forward declarations
typedef struct app_tui_input app_tui_input_t;
typedef struct app_keybinding app_keybinding_t;

// Keybinding action callback
typedef app_error (*app_keybind_action_fn)(void *context,
                                           const app_key_event_t *event);

// Input system functions
app_error app_tui_input_create(app_tui_input_t **input);
void app_tui_input_destroy(app_tui_input_t *input);

// Configure terminal for raw input
app_error app_tui_input_enable_raw_mode(app_tui_input_t *input);
app_error app_tui_input_disable_raw_mode(app_tui_input_t *input);

// Read and parse input
app_error app_tui_input_read_event(app_tui_input_t *input,
                                   app_key_event_t *event, int timeout_ms);

// Keybinding management
app_error app_tui_input_bind_key(app_tui_input_t *input, int key,
                                 app_key_modifier_t modifiers,
                                 app_keybind_action_fn action, void *context);
app_error app_tui_input_unbind_key(app_tui_input_t *input, int key,
                                   app_key_modifier_t modifiers);
app_error app_tui_input_process_keybinding(app_tui_input_t *input,
                                           const app_key_event_t *event);

// Key sequence parsing helpers
bool app_tui_input_is_escape_sequence(const char *buffer, size_t len);
app_error app_tui_input_parse_escape_sequence(const char *buffer, size_t len,
                                              app_key_event_t *event,
                                              size_t *consumed);

// Utility functions
const char *app_tui_input_key_name(int key, app_key_modifier_t modifiers);
app_error app_tui_input_parse_key_string(const char *str, int *key,
                                         app_key_modifier_t *modifiers);

// Mouse support
app_error app_tui_input_enable_mouse(app_tui_input_t *input);
app_error app_tui_input_disable_mouse(app_tui_input_t *input);