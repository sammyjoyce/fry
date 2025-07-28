/*
 * TUI Input System Implementation
 */

#include "tui_input.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../utils/logging.h"
#include "../utils/memory.h"
#include "tui_ncurses.h"

// Keybinding structure
struct app_keybinding {
  int key;
  app_key_modifier_t modifiers;
  app_keybind_action_fn action;
  void *context;
  struct app_keybinding *next;
};

// Input system state
struct app_tui_input {
  // Terminal state
  struct termios original_termios;
  bool raw_mode_enabled;
  bool mouse_enabled;

  // Input buffer for multi-byte sequences
  char buffer[32];
  size_t buffer_len;

  // Keybindings
  app_keybinding_t *keybindings;

  // Escape sequence timeout (milliseconds)
  int escape_timeout;
};

// Create input system
app_error app_tui_input_create(app_tui_input_t **input) {
  if (!input) {
    return APP_ERROR_INVALID_ARG;
  }

  app_tui_input_t *inp = app_calloc(1, sizeof(app_tui_input_t));
  if (!inp) {
    return APP_ERROR_MEMORY;
  }

  inp->escape_timeout = 100;  // 100ms timeout for escape sequences
  inp->raw_mode_enabled = false;
  inp->mouse_enabled = false;

  *input = inp;
  return APP_SUCCESS;
}

// Destroy input system
void app_tui_input_destroy(app_tui_input_t *input) {
  if (!input) {
    return;
  }

  // Restore terminal state
  if (input->raw_mode_enabled) {
    app_tui_input_disable_raw_mode(input);
  }

  // Free keybindings
  app_keybinding_t *kb = input->keybindings;
  while (kb) {
    app_keybinding_t *next = kb->next;
    app_free(kb);
    kb = next;
  }

  app_free(input);
}

// Enable raw mode for terminal input
app_error app_tui_input_enable_raw_mode(app_tui_input_t *input) {
  if (!input) {
    return APP_ERROR_INVALID_ARG;
  }

  if (input->raw_mode_enabled) {
    return APP_SUCCESS;  // Already enabled
  }

  // Get current terminal attributes
  if (tcgetattr(STDIN_FILENO, &input->original_termios) < 0) {
    LOG_ERROR("Failed to get terminal attributes: %s", strerror(errno));
    return APP_ERROR_SYSTEM;
  }

  // Create raw mode settings
  struct termios raw = input->original_termios;

  // Input flags: disable break, CR to NL, parity check, strip, flow control
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

  // Output flags: disable post processing
  raw.c_oflag &= ~(OPOST);

  // Control flags: set 8 bit chars
  raw.c_cflag |= (CS8);

  // Local flags: disable echo, canonical mode, extended functions, signals
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  // Control characters: set minimum bytes and timeout
  raw.c_cc[VMIN] = 0;   // Non-blocking read
  raw.c_cc[VTIME] = 0;  // No timeout

  // Apply raw mode
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
    LOG_ERROR("Failed to set raw mode: %s", strerror(errno));
    return APP_ERROR_SYSTEM;
  }

  // Set non-blocking mode on stdin
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  if (flags < 0 || fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) < 0) {
    LOG_WARNING("Failed to set non-blocking mode on stdin");
  }

  input->raw_mode_enabled = true;
  LOG_DEBUG("Raw mode enabled");

  return APP_SUCCESS;
}

// Disable raw mode
app_error app_tui_input_disable_raw_mode(app_tui_input_t *input) {
  if (!input || !input->raw_mode_enabled) {
    return APP_SUCCESS;
  }

  // Restore original terminal attributes
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &input->original_termios) < 0) {
    LOG_ERROR("Failed to restore terminal attributes: %s", strerror(errno));
    return APP_ERROR_SYSTEM;
  }

  input->raw_mode_enabled = false;
  LOG_DEBUG("Raw mode disabled");

  return APP_SUCCESS;
}

// Parse escape sequences
static app_error parse_csi_sequence(const char *buffer, size_t len,
                                    app_key_event_t *event) {
  // CSI sequences start with ESC [ and end with a letter
  if (len < 3 || buffer[0] != 27 || buffer[1] != '[') {
    return APP_ERROR_INVALID_ARG;
  }

  // Parse numeric parameters
  int params[8] = {0};
  int param_count = 0;
  size_t i = 2;

  while (i < len && param_count < 8) {
    if (isdigit(buffer[i])) {
      params[param_count] = params[param_count] * 10 + (buffer[i] - '0');
    } else if (buffer[i] == ';') {
      param_count++;
    } else {
      break;
    }
    i++;
  }

  if (i >= len) {
    return APP_ERROR_INCOMPLETE;  // Need more data
  }

  // Handle final character
  char final = buffer[i];
  event->is_special = true;
  event->modifiers = KEY_MOD_NONE;

  // Check for modifier in first parameter (xterm style)
  if (param_count > 0 && params[1] > 1) {
    int mod = params[1] - 1;
    if (mod & 1)
      event->modifiers |= KEY_MOD_SHIFT;
    if (mod & 2)
      event->modifiers |= KEY_MOD_ALT;
    if (mod & 4)
      event->modifiers |= KEY_MOD_CTRL;
    if (mod & 8)
      event->modifiers |= KEY_MOD_META;
  }

  switch (final) {
  case 'A':
    event->code = APP_KEY_UP;
    break;
  case 'B':
    event->code = APP_KEY_DOWN;
    break;
  case 'C':
    event->code = APP_KEY_RIGHT;
    break;
  case 'D':
    event->code = APP_KEY_LEFT;
    break;
  case 'H':
    event->code = APP_KEY_HOME;
    break;
  case 'F':
    event->code = APP_KEY_END;
    break;
  case '~':
    // Extended keys with numeric code
    switch (params[0]) {
    case 1:
      event->code = APP_KEY_HOME;
      break;
    case 2:
      event->code = APP_KEY_INSERT;
      break;
    case 3:
      event->code = APP_KEY_DELETE;
      break;
    case 4:
      event->code = APP_KEY_END;
      break;
    case 5:
      event->code = APP_KEY_PAGE_UP;
      break;
    case 6:
      event->code = APP_KEY_PAGE_DOWN;
      break;
    case 15:
      event->code = APP_KEY_F5;
      break;
    case 17:
      event->code = APP_KEY_F6;
      break;
    case 18:
      event->code = APP_KEY_F7;
      break;
    case 19:
      event->code = APP_KEY_F8;
      break;
    case 20:
      event->code = APP_KEY_F9;
      break;
    case 21:
      event->code = APP_KEY_F10;
      break;
    case 23:
      event->code = APP_KEY_F11;
      break;
    case 24:
      event->code = APP_KEY_F12;
      break;
    default:
      event->code = APP_KEY_UNKNOWN;
      break;
    }
    break;
  case 'M':
    // Mouse event (if enabled)
    if (len >= 6) {
      event->code = APP_KEY_MOUSE_CLICK;
      event->mouse_button = buffer[3] - 32;
      event->mouse_x = buffer[4] - 33;
      event->mouse_y = buffer[5] - 33;
    }
    break;
  default:
    event->code = APP_KEY_UNKNOWN;
    break;
  }

  return APP_SUCCESS;
}

// Parse SS3 sequences (ESC O)
static app_error parse_ss3_sequence(const char *buffer, size_t len,
                                    app_key_event_t *event) {
  if (len < 3 || buffer[0] != 27 || buffer[1] != 'O') {
    return APP_ERROR_INVALID_ARG;
  }

  event->is_special = true;
  event->modifiers = KEY_MOD_NONE;

  switch (buffer[2]) {
  case 'P':
    event->code = APP_KEY_F1;
    break;
  case 'Q':
    event->code = APP_KEY_F2;
    break;
  case 'R':
    event->code = APP_KEY_F3;
    break;
  case 'S':
    event->code = APP_KEY_F4;
    break;
  case 'H':
    event->code = APP_KEY_HOME;
    break;
  case 'F':
    event->code = APP_KEY_END;
    break;
  default:
    event->code = APP_KEY_UNKNOWN;
    break;
  }

  return APP_SUCCESS;
}

// Check if buffer contains start of escape sequence
bool app_tui_input_is_escape_sequence(const char *buffer, size_t len) {
  if (len == 0 || buffer[0] != 27) {
    return false;
  }

  if (len == 1) {
    return true;  // Could be start of sequence
  }

  // Check for known sequence starts
  return buffer[1] == '[' || buffer[1] == 'O' || buffer[1] == 'P';
}

// Parse escape sequence from buffer
app_error app_tui_input_parse_escape_sequence(const char *buffer, size_t len,
                                              app_key_event_t *event,
                                              size_t *consumed) {
  if (!buffer || !event || !consumed || len == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  *consumed = 0;

  // Single ESC key
  if (len == 1 && buffer[0] == 27) {
    event->code = APP_KEY_ESCAPE;
    event->modifiers = KEY_MOD_NONE;
    event->is_special = true;
    *consumed = 1;
    return APP_SUCCESS;
  }

  // CSI sequences (ESC [)
  if (len >= 2 && buffer[0] == 27 && buffer[1] == '[') {
    // Find end of sequence
    size_t i = 2;
    while (i < len && !isalpha(buffer[i]) && buffer[i] != '~' &&
           buffer[i] != 'M') {
      i++;
    }

    if (i < len) {
      app_error err = parse_csi_sequence(buffer, i + 1, event);
      if (err == APP_SUCCESS) {
        *consumed = i + 1;
      }
      return err;
    }

    return APP_ERROR_INCOMPLETE;  // Need more data
  }

  // SS3 sequences (ESC O)
  if (len >= 3 && buffer[0] == 27 && buffer[1] == 'O') {
    app_error err = parse_ss3_sequence(buffer, 3, event);
    if (err == APP_SUCCESS) {
      *consumed = 3;
    }
    return err;
  }

  // Alt+key sequences (ESC followed by character)
  if (len >= 2 && buffer[0] == 27) {
    event->code = buffer[1];
    event->modifiers = KEY_MOD_ALT;
    event->is_special = false;
    *consumed = 2;
    return APP_SUCCESS;
  }

  return APP_ERROR_INVALID_ARG;
}

// Read input event with timeout
app_error app_tui_input_read_event(app_tui_input_t *input,
                                   app_key_event_t *event, int timeout_ms) {
  if (!input || !event) {
    return APP_ERROR_INVALID_ARG;
  }

  // Clear event
  memset(event, 0, sizeof(app_key_event_t));

  // Check buffered data first
  if (input->buffer_len > 0) {
    // Try to parse escape sequence
    if (app_tui_input_is_escape_sequence(input->buffer, input->buffer_len)) {
      size_t consumed = 0;
      app_error err = app_tui_input_parse_escape_sequence(
          input->buffer, input->buffer_len, event, &consumed);

      if (err == APP_SUCCESS && consumed > 0) {
        // Remove consumed bytes from buffer
        memmove(input->buffer, input->buffer + consumed,
                input->buffer_len - consumed);
        input->buffer_len -= consumed;
        return APP_SUCCESS;
      }

      if (err == APP_ERROR_INCOMPLETE) {
        // Need more data, fall through to read
      } else {
        // Invalid sequence, treat first byte as regular key
        event->code = input->buffer[0];
        event->modifiers = KEY_MOD_NONE;
        event->is_special = false;
        memmove(input->buffer, input->buffer + 1, input->buffer_len - 1);
        input->buffer_len--;
        return APP_SUCCESS;
      }
    } else {
      // Regular character
      event->code = input->buffer[0];
      event->modifiers = KEY_MOD_NONE;
      event->is_special = false;

      // Check for control characters
      if (event->code < 32) {
        event->modifiers = KEY_MOD_CTRL;
        event->code += 64;  // Convert to letter (Ctrl-A = 1 -> 'A')
      }

      memmove(input->buffer, input->buffer + 1, input->buffer_len - 1);
      input->buffer_len--;
      return APP_SUCCESS;
    }
  }

  // Read new data
  struct pollfd pfd = {.fd = STDIN_FILENO, .events = POLLIN};

  int poll_timeout = timeout_ms;
  if (input->buffer_len > 0 &&
      app_tui_input_is_escape_sequence(input->buffer, input->buffer_len)) {
    // We have partial escape sequence, use shorter timeout
    poll_timeout = input->escape_timeout;
  }

  int ret = poll(&pfd, 1, poll_timeout);
  if (ret < 0) {
    if (errno == EINTR) {
      return APP_ERROR_RETRY;
    }
    return APP_ERROR_SYSTEM;
  }

  if (ret == 0) {
    // Timeout
    if (input->buffer_len > 0) {
      // Treat buffered data as regular keys
      event->code = input->buffer[0];
      event->modifiers = KEY_MOD_NONE;
      event->is_special = false;
      memmove(input->buffer, input->buffer + 1, input->buffer_len - 1);
      input->buffer_len--;
      return APP_SUCCESS;
    }
    return APP_ERROR_TIMEOUT;
  }

  // Read available data
  ssize_t n = read(STDIN_FILENO, input->buffer + input->buffer_len,
                   sizeof(input->buffer) - input->buffer_len);
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return APP_ERROR_TIMEOUT;
    }
    return APP_ERROR_SYSTEM;
  }

  if (n == 0) {
    return APP_ERROR_EOF;
  }

  input->buffer_len += n;

  // Try to parse again
  return app_tui_input_read_event(input, event, 0);
}

// Keybinding management
app_error app_tui_input_bind_key(app_tui_input_t *input, int key,
                                 app_key_modifier_t modifiers,
                                 app_keybind_action_fn action, void *context) {
  if (!input || !action) {
    return APP_ERROR_INVALID_ARG;
  }

  // Check for existing binding
  app_keybinding_t *kb = input->keybindings;
  while (kb) {
    if (kb->key == key && kb->modifiers == modifiers) {
      // Update existing binding
      kb->action = action;
      kb->context = context;
      return APP_SUCCESS;
    }
    kb = kb->next;
  }

  // Create new binding
  kb = app_calloc(1, sizeof(app_keybinding_t));
  if (!kb) {
    return APP_ERROR_MEMORY;
  }

  kb->key = key;
  kb->modifiers = modifiers;
  kb->action = action;
  kb->context = context;
  kb->next = input->keybindings;
  input->keybindings = kb;

  return APP_SUCCESS;
}

// Remove keybinding
app_error app_tui_input_unbind_key(app_tui_input_t *input, int key,
                                   app_key_modifier_t modifiers) {
  if (!input) {
    return APP_ERROR_INVALID_ARG;
  }

  app_keybinding_t **prev = &input->keybindings;
  app_keybinding_t *kb = input->keybindings;

  while (kb) {
    if (kb->key == key && kb->modifiers == modifiers) {
      *prev = kb->next;
      app_free(kb);
      return APP_SUCCESS;
    }
    prev = &kb->next;
    kb = kb->next;
  }

  return APP_ERROR_NOT_FOUND;
}

// Process keybinding for event
app_error app_tui_input_process_keybinding(app_tui_input_t *input,
                                           const app_key_event_t *event) {
  if (!input || !event) {
    return APP_ERROR_INVALID_ARG;
  }

  app_keybinding_t *kb = input->keybindings;
  while (kb) {
    if (kb->key == event->code && kb->modifiers == event->modifiers) {
      return kb->action(kb->context, event);
    }
    kb = kb->next;
  }

  return APP_ERROR_NOT_FOUND;  // No binding found
}

// Get human-readable key name
const char *app_tui_input_key_name(int key, app_key_modifier_t modifiers) {
  static char buffer[64];
  buffer[0] = '\0';

  // Add modifiers
  if (modifiers & KEY_MOD_CTRL)
    strcat(buffer, "Ctrl-");
  if (modifiers & KEY_MOD_ALT)
    strcat(buffer, "Alt-");
  if (modifiers & KEY_MOD_SHIFT)
    strcat(buffer, "Shift-");
  if (modifiers & KEY_MOD_META)
    strcat(buffer, "Meta-");

  // Add key name
  switch (key) {
  case APP_KEY_ESCAPE:
    strcat(buffer, "Esc");
    break;
  case APP_KEY_ENTER:
    strcat(buffer, "Enter");
    break;
  case APP_KEY_TAB:
    strcat(buffer, "Tab");
    break;
  case APP_KEY_BACKSPACE:
    strcat(buffer, "Backspace");
    break;
  case APP_KEY_UP:
    strcat(buffer, "Up");
    break;
  case APP_KEY_DOWN:
    strcat(buffer, "Down");
    break;
  case APP_KEY_LEFT:
    strcat(buffer, "Left");
    break;
  case APP_KEY_RIGHT:
    strcat(buffer, "Right");
    break;
  case APP_KEY_HOME:
    strcat(buffer, "Home");
    break;
  case APP_KEY_END:
    strcat(buffer, "End");
    break;
  case APP_KEY_PAGE_UP:
    strcat(buffer, "PageUp");
    break;
  case APP_KEY_PAGE_DOWN:
    strcat(buffer, "PageDown");
    break;
  case APP_KEY_INSERT:
    strcat(buffer, "Insert");
    break;
  case APP_KEY_DELETE:
    strcat(buffer, "Delete");
    break;
  default:
    if (key >= APP_KEY_F1 && key <= APP_KEY_F12) {
      sprintf(buffer + strlen(buffer), "F%d", key - APP_KEY_F1 + 1);
    } else if (key >= 32 && key < 127) {
      // Printable character
      char ch[2] = {(char)key, '\0'};
      strcat(buffer, ch);
    } else {
      sprintf(buffer + strlen(buffer), "Key(%d)", key);
    }
    break;
  }

  return buffer;
}

// Parse key string (e.g., "Ctrl-C", "Alt-X")
app_error app_tui_input_parse_key_string(const char *str, int *key,
                                         app_key_modifier_t *modifiers) {
  if (!str || !key || !modifiers) {
    return APP_ERROR_INVALID_ARG;
  }

  *key = 0;
  *modifiers = KEY_MOD_NONE;

  char buffer[128];
  strncpy(buffer, str, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';

  // Parse modifiers
  char *token = strtok(buffer, "-");
  char *last_token = NULL;

  while (token) {
    if (last_token) {
      // Previous token was a modifier
      if (strcasecmp(last_token, "ctrl") == 0) {
        *modifiers |= KEY_MOD_CTRL;
      } else if (strcasecmp(last_token, "alt") == 0) {
        *modifiers |= KEY_MOD_ALT;
      } else if (strcasecmp(last_token, "shift") == 0) {
        *modifiers |= KEY_MOD_SHIFT;
      } else if (strcasecmp(last_token, "meta") == 0 ||
                 strcasecmp(last_token, "cmd") == 0) {
        *modifiers |= KEY_MOD_META;
      }
    }
    last_token = token;
    token = strtok(NULL, "-");
  }

  // Parse key
  if (!last_token) {
    return APP_ERROR_INVALID_ARG;
  }

  // Special keys
  if (strcasecmp(last_token, "esc") == 0 ||
      strcasecmp(last_token, "escape") == 0) {
    *key = APP_KEY_ESCAPE;
  } else if (strcasecmp(last_token, "enter") == 0 ||
             strcasecmp(last_token, "return") == 0) {
    *key = APP_KEY_ENTER;
  } else if (strcasecmp(last_token, "tab") == 0) {
    *key = APP_KEY_TAB;
  } else if (strcasecmp(last_token, "backspace") == 0) {
    *key = APP_KEY_BACKSPACE;
  } else if (strcasecmp(last_token, "up") == 0) {
    *key = APP_KEY_UP;
  } else if (strcasecmp(last_token, "down") == 0) {
    *key = APP_KEY_DOWN;
  } else if (strcasecmp(last_token, "left") == 0) {
    *key = APP_KEY_LEFT;
  } else if (strcasecmp(last_token, "right") == 0) {
    *key = APP_KEY_RIGHT;
  } else if (strcasecmp(last_token, "home") == 0) {
    *key = APP_KEY_HOME;
  } else if (strcasecmp(last_token, "end") == 0) {
    *key = APP_KEY_END;
  } else if (strcasecmp(last_token, "pageup") == 0 ||
             strcasecmp(last_token, "pgup") == 0) {
    *key = APP_KEY_PAGE_UP;
  } else if (strcasecmp(last_token, "pagedown") == 0 ||
             strcasecmp(last_token, "pgdn") == 0) {
    *key = APP_KEY_PAGE_DOWN;
  } else if (strcasecmp(last_token, "insert") == 0 ||
             strcasecmp(last_token, "ins") == 0) {
    *key = APP_KEY_INSERT;
  } else if (strcasecmp(last_token, "delete") == 0 ||
             strcasecmp(last_token, "del") == 0) {
    *key = APP_KEY_DELETE;
  } else if (strlen(last_token) == 1) {
    // Single character
    *key = last_token[0];
  } else if (last_token[0] == 'f' || last_token[0] == 'F') {
    // Function key
    int fn = atoi(last_token + 1);
    if (fn >= 1 && fn <= 12) {
      *key = APP_KEY_F1 + fn - 1;
    }
  }

  if (*key == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  return APP_SUCCESS;
}

// Enable mouse support
app_error app_tui_input_enable_mouse(app_tui_input_t *input) {
  if (!input) {
    return APP_ERROR_INVALID_ARG;
  }

  // Enable mouse tracking in terminal
  printf("\033[?1000h");  // Enable mouse reporting
  printf("\033[?1002h");  // Enable mouse motion tracking
  printf("\033[?1015h");  // Enable extended mouse mode
  printf("\033[?1006h");  // Enable SGR mouse mode
  fflush(stdout);

  input->mouse_enabled = true;
  LOG_DEBUG("Mouse support enabled");

  return APP_SUCCESS;
}

// Disable mouse support
app_error app_tui_input_disable_mouse(app_tui_input_t *input) {
  if (!input || !input->mouse_enabled) {
    return APP_SUCCESS;
  }

  // Disable mouse tracking
  printf("\033[?1006l");  // Disable SGR mouse mode
  printf("\033[?1015l");  // Disable extended mouse mode
  printf("\033[?1002l");  // Disable mouse motion tracking
  printf("\033[?1000l");  // Disable mouse reporting
  fflush(stdout);

  input->mouse_enabled = false;
  LOG_DEBUG("Mouse support disabled");

  return APP_SUCCESS;
}