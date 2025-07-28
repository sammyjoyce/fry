/*
 * Terminal emulator implementation
 * Implements Task 9.2: Add ANSI escape sequence processing
 */

#include "tui_term_emulator.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "../utils/logging.h"
#include "../utils/memory.h"

// Default colors
#define DEFAULT_FG TERM_COLOR_WHITE
#define DEFAULT_BG TERM_COLOR_BLACK

// Helper to allocate 2D array
static tui_term_cell_t **alloc_screen(int width, int height) {
  tui_term_cell_t **screen = app_calloc(height, sizeof(tui_term_cell_t *));
  if (!screen)
    return NULL;

  for (int y = 0; y < height; y++) {
    screen[y] = app_calloc(width, sizeof(tui_term_cell_t));
    if (!screen[y]) {
      // Cleanup on failure
      for (int i = 0; i < y; i++) {
        app_free(screen[i]);
      }
      app_free(screen);
      return NULL;
    }
  }

  return screen;
}

// Helper to free 2D array
static void free_screen(tui_term_cell_t **screen, int height) {
  if (!screen)
    return;

  for (int y = 0; y < height; y++) {
    if (screen[y]) {
      app_free(screen[y]);
    }
  }
  app_free(screen);
}

// Initialize cell with default values
static void init_cell(tui_term_cell_t *cell) {
  cell->ch = ' ';
  cell->fg_color = DEFAULT_FG;
  cell->bg_color = DEFAULT_BG;
  cell->attrs = TERM_ATTR_NONE;
}

// Create terminal emulator
tui_term_emulator_t *tui_term_emulator_create(int width, int height,
                                              int scrollback_size) {
  if (width <= 0 || height <= 0) {
    LOG_ERROR("Invalid terminal dimensions: %dx%d", width, height);
    return NULL;
  }

  tui_term_emulator_t *term = app_calloc(1, sizeof(tui_term_emulator_t));
  if (!term)
    return NULL;

  term->width = width;
  term->height = height;
  term->scrollback_size = scrollback_size;

  // Allocate screen buffers
  term->screen = alloc_screen(width, height);
  if (!term->screen) {
    app_free(term);
    return NULL;
  }

  // Initialize screen with spaces
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      init_cell(&term->screen[y][x]);
    }
  }

  // Allocate scrollback buffer if requested
  if (scrollback_size > 0) {
    term->scrollback = alloc_screen(width, scrollback_size);
    if (!term->scrollback) {
      free_screen(term->screen, height);
      app_free(term);
      return NULL;
    }

    // Initialize scrollback
    for (int y = 0; y < scrollback_size; y++) {
      for (int x = 0; x < width; x++) {
        init_cell(&term->scrollback[y][x]);
      }
    }
  }

  // Allocate tab stops
  term->tab_stops = app_calloc(width, sizeof(bool));
  if (!term->tab_stops) {
    free_screen(term->scrollback, scrollback_size);
    free_screen(term->screen, height);
    app_free(term);
    return NULL;
  }

  // Set default tab stops every 8 columns
  for (int i = 0; i < width; i += 8) {
    term->tab_stops[i] = true;
  }

  // Initialize state
  term->cursor_visible = true;
  term->auto_wrap = true;
  term->current_fg = DEFAULT_FG;
  term->current_bg = DEFAULT_BG;
  term->scroll_bottom = height - 1;
  term->parser_state = STATE_NORMAL;

  LOG_DEBUG("Created terminal emulator %dx%d with %d lines scrollback", width,
            height, scrollback_size);

  return term;
}

// Destroy terminal emulator
void tui_term_emulator_destroy(tui_term_emulator_t *term) {
  if (!term)
    return;

  free_screen(term->screen, term->height);
  free_screen(term->alt_screen, term->height);
  free_screen(term->scrollback, term->scrollback_size);
  app_free(term->tab_stops);
  app_free(term);
}

// Scroll screen up by one line
static void scroll_up_one(tui_term_emulator_t *term) {
  // Save top line to scrollback if available
  if (term->scrollback && term->scrollback_size > 0) {
    // Copy line to scrollback
    memcpy(term->scrollback[term->scrollback_pos],
           term->screen[term->scroll_top],
           term->width * sizeof(tui_term_cell_t));

    term->scrollback_pos = (term->scrollback_pos + 1) % term->scrollback_size;
    if (term->scrollback_lines < term->scrollback_size) {
      term->scrollback_lines++;
    }
  }

  // Shift lines up within scroll region
  for (int y = term->scroll_top; y < term->scroll_bottom; y++) {
    memcpy(term->screen[y], term->screen[y + 1],
           term->width * sizeof(tui_term_cell_t));
  }

  // Clear bottom line
  for (int x = 0; x < term->width; x++) {
    init_cell(&term->screen[term->scroll_bottom][x]);
  }
}

// Write a character at current cursor position
static void write_char(tui_term_emulator_t *term, uint32_t ch) {
  // Handle special characters
  switch (ch) {
  case '\r':  // Carriage return
    term->cursor_x = 0;
    return;

  case '\n':  // Line feed
    term->cursor_y++;
    if (term->cursor_y > term->scroll_bottom) {
      term->cursor_y = term->scroll_bottom;
      scroll_up_one(term);
    }
    return;

  case '\t':  // Tab
    // Move to next tab stop
    do {
      term->cursor_x++;
    } while (term->cursor_x < term->width && !term->tab_stops[term->cursor_x]);

    if (term->cursor_x >= term->width) {
      if (term->auto_wrap) {
        term->cursor_x = 0;
        term->cursor_y++;
        if (term->cursor_y > term->scroll_bottom) {
          term->cursor_y = term->scroll_bottom;
          scroll_up_one(term);
        }
      } else {
        term->cursor_x = term->width - 1;
      }
    }
    return;

  case '\b':  // Backspace
    if (term->cursor_x > 0) {
      term->cursor_x--;
    }
    return;

  case '\a':  // Bell
    // TODO: Trigger bell
    return;
  }

  // Regular character
  if (term->cursor_x >= term->width) {
    if (term->auto_wrap) {
      term->cursor_x = 0;
      term->cursor_y++;
      if (term->cursor_y > term->scroll_bottom) {
        term->cursor_y = term->scroll_bottom;
        scroll_up_one(term);
      }
    } else {
      term->cursor_x = term->width - 1;
    }
  }

  // Write character at cursor position
  tui_term_cell_t *cell = &term->screen[term->cursor_y][term->cursor_x];
  cell->ch = ch;
  cell->fg_color = term->current_fg;
  cell->bg_color = term->current_bg;
  cell->attrs = term->current_attrs;

  // Advance cursor
  term->cursor_x++;
}

// Parse CSI (Control Sequence Introducer) sequences
static void parse_csi(tui_term_emulator_t *term) {
  // Get command character (last character in buffer)
  if (term->escape_len == 0)
    return;

  char cmd = term->escape_buf[term->escape_len - 1];

  // Parse numeric parameters
  term->escape_param_count = 0;
  int param = 0;
  bool in_param = false;

  for (int i = 0; i < term->escape_len - 1; i++) {
    char c = term->escape_buf[i];
    if (c >= '0' && c <= '9') {
      param = param * 10 + (c - '0');
      in_param = true;
    } else if (c == ';' || c == ':') {
      if (term->escape_param_count < 16) {
        term->escape_params[term->escape_param_count++] = in_param ? param : 0;
      }
      param = 0;
      in_param = false;
    }
  }

  // Add last parameter
  if (term->escape_param_count < 16) {
    term->escape_params[term->escape_param_count++] = in_param ? param : 0;
  }

  // Default parameter values
  int p1 = term->escape_param_count > 0 ? term->escape_params[0] : 1;
  int p2 = term->escape_param_count > 1 ? term->escape_params[1] : 1;

  // Handle command
  switch (cmd) {
  case 'A':  // Cursor up
    term->cursor_y -= p1;
    if (term->cursor_y < 0)
      term->cursor_y = 0;
    break;

  case 'B':  // Cursor down
    term->cursor_y += p1;
    if (term->cursor_y >= term->height)
      term->cursor_y = term->height - 1;
    break;

  case 'C':  // Cursor forward
    term->cursor_x += p1;
    if (term->cursor_x >= term->width)
      term->cursor_x = term->width - 1;
    break;

  case 'D':  // Cursor back
    term->cursor_x -= p1;
    if (term->cursor_x < 0)
      term->cursor_x = 0;
    break;

  case 'H':  // Cursor position
  case 'f':
    term->cursor_y = (p1 > 0 ? p1 - 1 : 0);
    term->cursor_x = (p2 > 0 ? p2 - 1 : 0);
    if (term->cursor_y >= term->height)
      term->cursor_y = term->height - 1;
    if (term->cursor_x >= term->width)
      term->cursor_x = term->width - 1;
    break;

  case 'J':  // Erase display
    if (p1 == 0) {
      // Clear from cursor to end
      for (int x = term->cursor_x; x < term->width; x++) {
        init_cell(&term->screen[term->cursor_y][x]);
      }
      for (int y = term->cursor_y + 1; y < term->height; y++) {
        for (int x = 0; x < term->width; x++) {
          init_cell(&term->screen[y][x]);
        }
      }
    } else if (p1 == 1) {
      // Clear from start to cursor
      for (int y = 0; y < term->cursor_y; y++) {
        for (int x = 0; x < term->width; x++) {
          init_cell(&term->screen[y][x]);
        }
      }
      for (int x = 0; x <= term->cursor_x; x++) {
        init_cell(&term->screen[term->cursor_y][x]);
      }
    } else if (p1 == 2) {
      // Clear entire display
      tui_term_emulator_clear(term);
    }
    break;

  case 'K':  // Erase line
    if (p1 == 0) {
      // Clear from cursor to end of line
      for (int x = term->cursor_x; x < term->width; x++) {
        init_cell(&term->screen[term->cursor_y][x]);
      }
    } else if (p1 == 1) {
      // Clear from start to cursor
      for (int x = 0; x <= term->cursor_x; x++) {
        init_cell(&term->screen[term->cursor_y][x]);
      }
    } else if (p1 == 2) {
      // Clear entire line
      for (int x = 0; x < term->width; x++) {
        init_cell(&term->screen[term->cursor_y][x]);
      }
    }
    break;

  case 'm':  // SGR (Select Graphic Rendition)
    if (term->escape_param_count == 0) {
      // Reset attributes
      term->current_fg = DEFAULT_FG;
      term->current_bg = DEFAULT_BG;
      term->current_attrs = TERM_ATTR_NONE;
    } else {
      for (int i = 0; i < term->escape_param_count; i++) {
        int param = term->escape_params[i];

        if (param == 0) {
          // Reset
          term->current_fg = DEFAULT_FG;
          term->current_bg = DEFAULT_BG;
          term->current_attrs = TERM_ATTR_NONE;
        } else if (param == 1) {
          term->current_attrs |= TERM_ATTR_BOLD;
        } else if (param == 2) {
          term->current_attrs |= TERM_ATTR_DIM;
        } else if (param == 3) {
          term->current_attrs |= TERM_ATTR_ITALIC;
        } else if (param == 4) {
          term->current_attrs |= TERM_ATTR_UNDERLINE;
        } else if (param == 5) {
          term->current_attrs |= TERM_ATTR_BLINK;
        } else if (param == 7) {
          term->current_attrs |= TERM_ATTR_REVERSE;
        } else if (param >= 30 && param <= 37) {
          // Foreground color
          term->current_fg = param - 30;
        } else if (param >= 40 && param <= 47) {
          // Background color
          term->current_bg = param - 40;
        } else if (param >= 90 && param <= 97) {
          // Bright foreground
          term->current_fg = param - 90 + 8;
        } else if (param >= 100 && param <= 107) {
          // Bright background
          term->current_bg = param - 100 + 8;
        }
      }
    }
    break;

  case 'r':  // Set scroll region
    if (p1 == 0)
      p1 = 1;
    if (p2 == 0)
      p2 = term->height;
    term->scroll_top = p1 - 1;
    term->scroll_bottom = p2 - 1;
    if (term->scroll_top < 0)
      term->scroll_top = 0;
    if (term->scroll_bottom >= term->height)
      term->scroll_bottom = term->height - 1;
    if (term->scroll_top >= term->scroll_bottom) {
      term->scroll_top = 0;
      term->scroll_bottom = term->height - 1;
    }
    break;

  case 'h':  // Set mode
    // TODO: Handle various modes
    break;

  case 'l':  // Reset mode
    // TODO: Handle various modes
    break;

  default:
    LOG_DEBUG("Unhandled CSI sequence: ESC[%.*s%c", term->escape_len - 1,
              term->escape_buf, cmd);
    break;
  }
}

// Process a single byte
static void process_byte(tui_term_emulator_t *term, unsigned char byte) {
  switch (term->parser_state) {
  case STATE_NORMAL:
    if (byte == 0x1B) {  // ESC
      term->parser_state = STATE_ESCAPE;
      term->escape_len = 0;
    } else if (byte < 0x20) {
      // Control character
      write_char(term, byte);
    } else if (byte < 0x80) {
      // ASCII character
      write_char(term, byte);
    } else {
      // Start of UTF-8 sequence
      if ((byte & 0xE0) == 0xC0) {
        // 2-byte sequence
        term->utf8_char = byte & 0x1F;
        term->utf8_remaining = 1;
        term->parser_state = STATE_UTF8;
      } else if ((byte & 0xF0) == 0xE0) {
        // 3-byte sequence
        term->utf8_char = byte & 0x0F;
        term->utf8_remaining = 2;
        term->parser_state = STATE_UTF8;
      } else if ((byte & 0xF8) == 0xF0) {
        // 4-byte sequence
        term->utf8_char = byte & 0x07;
        term->utf8_remaining = 3;
        term->parser_state = STATE_UTF8;
      } else {
        // Invalid UTF-8 start byte
        write_char(term, '?');
      }
    }
    break;

  case STATE_ESCAPE:
    if (byte == '[') {
      term->parser_state = STATE_CSI;
      term->escape_len = 0;
    } else if (byte == ']') {
      term->parser_state = STATE_OSC;
      term->escape_len = 0;
    } else if (byte == 'P') {
      term->parser_state = STATE_DCS;
      term->escape_len = 0;
    } else {
      // Single character escape sequence
      switch (byte) {
      case 'D':  // Index (IND)
        term->cursor_y++;
        if (term->cursor_y > term->scroll_bottom) {
          term->cursor_y = term->scroll_bottom;
          scroll_up_one(term);
        }
        break;

      case 'M':  // Reverse index (RI)
        term->cursor_y--;
        if (term->cursor_y < term->scroll_top) {
          term->cursor_y = term->scroll_top;
          // TODO: Scroll down
        }
        break;

      case 'E':  // Next line (NEL)
        term->cursor_x = 0;
        term->cursor_y++;
        if (term->cursor_y > term->scroll_bottom) {
          term->cursor_y = term->scroll_bottom;
          scroll_up_one(term);
        }
        break;

      case '7':  // Save cursor (DECSC)
        // TODO: Save cursor position and attributes
        break;

      case '8':  // Restore cursor (DECRC)
        // TODO: Restore cursor position and attributes
        break;

      default:
        LOG_DEBUG("Unhandled escape sequence: ESC %c", byte);
        break;
      }
      term->parser_state = STATE_NORMAL;
    }
    break;

  case STATE_CSI:
    if (term->escape_len < sizeof(term->escape_buf) - 1) {
      term->escape_buf[term->escape_len++] = byte;
    }

    // Check if this is the final character
    if ((byte >= 0x40 && byte <= 0x7E) || byte == '~') {
      term->escape_buf[term->escape_len] = '\0';
      parse_csi(term);
      term->parser_state = STATE_NORMAL;
    }
    break;

  case STATE_OSC:
    // Operating System Command - skip until ST or BEL
    if (byte == 0x07 ||
        (term->escape_len > 0 &&
         term->escape_buf[term->escape_len - 1] == 0x1B && byte == '\\')) {
      // End of OSC
      term->parser_state = STATE_NORMAL;
    } else if (term->escape_len < sizeof(term->escape_buf) - 1) {
      term->escape_buf[term->escape_len++] = byte;
    }
    break;

  case STATE_DCS:
    // Device Control String - skip until ST
    if (term->escape_len > 0 &&
        term->escape_buf[term->escape_len - 1] == 0x1B && byte == '\\') {
      // End of DCS
      term->parser_state = STATE_NORMAL;
    } else if (term->escape_len < sizeof(term->escape_buf) - 1) {
      term->escape_buf[term->escape_len++] = byte;
    }
    break;

  case STATE_UTF8:
    if ((byte & 0xC0) == 0x80) {
      // Valid continuation byte
      term->utf8_char = (term->utf8_char << 6) | (byte & 0x3F);
      term->utf8_remaining--;

      if (term->utf8_remaining == 0) {
        // Complete UTF-8 character
        write_char(term, term->utf8_char);
        term->parser_state = STATE_NORMAL;
      }
    } else {
      // Invalid UTF-8 sequence
      write_char(term, '?');
      term->parser_state = STATE_NORMAL;
      // Process this byte again
      process_byte(term, byte);
    }
    break;
  }
}

// Process input data
void tui_term_emulator_process(tui_term_emulator_t *term, const char *data,
                               size_t len) {
  if (!term || !data || len == 0)
    return;

  for (size_t i = 0; i < len; i++) {
    process_byte(term, (unsigned char)data[i]);
  }
}

// Get cell at position
const tui_term_cell_t *tui_term_emulator_get_cell(
    const tui_term_emulator_t *term, int x, int y) {
  if (!term || x < 0 || x >= term->width || y < 0 || y >= term->height) {
    return NULL;
  }

  return &term->screen[y][x];
}

// Clear screen
void tui_term_emulator_clear(tui_term_emulator_t *term) {
  if (!term)
    return;

  for (int y = 0; y < term->height; y++) {
    for (int x = 0; x < term->width; x++) {
      init_cell(&term->screen[y][x]);
    }
  }

  term->cursor_x = 0;
  term->cursor_y = 0;
}

// Reset terminal state
void tui_term_emulator_reset(tui_term_emulator_t *term) {
  if (!term)
    return;

  tui_term_emulator_clear(term);

  term->current_fg = DEFAULT_FG;
  term->current_bg = DEFAULT_BG;
  term->current_attrs = TERM_ATTR_NONE;
  term->cursor_visible = true;
  term->auto_wrap = true;
  term->scroll_top = 0;
  term->scroll_bottom = term->height - 1;
  term->parser_state = STATE_NORMAL;

  // Reset tab stops
  for (int i = 0; i < term->width; i++) {
    term->tab_stops[i] = (i % 8 == 0);
  }
}

// Move cursor
void tui_term_emulator_move_cursor(tui_term_emulator_t *term, int x, int y) {
  if (!term)
    return;

  term->cursor_x = x;
  term->cursor_y = y;

  if (term->cursor_x < 0)
    term->cursor_x = 0;
  if (term->cursor_x >= term->width)
    term->cursor_x = term->width - 1;
  if (term->cursor_y < 0)
    term->cursor_y = 0;
  if (term->cursor_y >= term->height)
    term->cursor_y = term->height - 1;
}

// Show/hide cursor
void tui_term_emulator_show_cursor(tui_term_emulator_t *term, bool visible) {
  if (term) {
    term->cursor_visible = visible;
  }
}

// Get cursor position and visibility
void tui_term_emulator_get_cursor(const tui_term_emulator_t *term, int *x,
                                  int *y, bool *visible) {
  if (!term)
    return;

  if (x)
    *x = term->cursor_x;
  if (y)
    *y = term->cursor_y;
  if (visible)
    *visible = term->cursor_visible;
}