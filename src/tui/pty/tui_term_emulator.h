#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Terminal emulator for processing ANSI escape sequences
// Implements Task 9.2: Add ANSI escape sequence processing

// Forward declarations
typedef struct tui_term_emulator_t tui_term_emulator_t;
typedef struct tui_term_cell_t tui_term_cell_t;

// Terminal colors
typedef enum {
  TERM_COLOR_BLACK = 0,
  TERM_COLOR_RED = 1,
  TERM_COLOR_GREEN = 2,
  TERM_COLOR_YELLOW = 3,
  TERM_COLOR_BLUE = 4,
  TERM_COLOR_MAGENTA = 5,
  TERM_COLOR_CYAN = 6,
  TERM_COLOR_WHITE = 7,
  TERM_COLOR_DEFAULT = 9,

  // Bright colors (8-15)
  TERM_COLOR_BRIGHT_BLACK = 8,
  TERM_COLOR_BRIGHT_RED = 9,
  TERM_COLOR_BRIGHT_GREEN = 10,
  TERM_COLOR_BRIGHT_YELLOW = 11,
  TERM_COLOR_BRIGHT_BLUE = 12,
  TERM_COLOR_BRIGHT_MAGENTA = 13,
  TERM_COLOR_BRIGHT_CYAN = 14,
  TERM_COLOR_BRIGHT_WHITE = 15
} tui_term_color_t;

// Terminal attributes
typedef enum {
  TERM_ATTR_NONE = 0,
  TERM_ATTR_BOLD = 1 << 0,
  TERM_ATTR_DIM = 1 << 1,
  TERM_ATTR_ITALIC = 1 << 2,
  TERM_ATTR_UNDERLINE = 1 << 3,
  TERM_ATTR_BLINK = 1 << 4,
  TERM_ATTR_REVERSE = 1 << 5,
  TERM_ATTR_HIDDEN = 1 << 6,
  TERM_ATTR_STRIKETHROUGH = 1 << 7
} tui_term_attr_t;

// Terminal cell (character with attributes)
struct tui_term_cell_t {
  uint32_t ch;        // Unicode character
  uint16_t fg_color;  // Foreground color (0-255 or special values)
  uint16_t bg_color;  // Background color
  uint16_t attrs;     // Attributes bitmask
};

// Terminal emulator state
struct tui_term_emulator_t {
  // Screen buffer
  tui_term_cell_t **screen;      // 2D array of cells
  tui_term_cell_t **alt_screen;  // Alternate screen buffer
  bool use_alt_screen;           // Using alternate screen

  // Dimensions
  int width;
  int height;

  // Cursor position
  int cursor_x;
  int cursor_y;
  bool cursor_visible;

  // Scrollback buffer
  tui_term_cell_t **scrollback;  // Circular buffer
  int scrollback_size;           // Total size
  int scrollback_pos;            // Current position
  int scrollback_lines;          // Number of lines in buffer

  // Current attributes
  uint16_t current_fg;
  uint16_t current_bg;
  uint16_t current_attrs;

  // Parser state
  enum {
    STATE_NORMAL,
    STATE_ESCAPE,  // Got ESC
    STATE_CSI,     // Got ESC[
    STATE_OSC,     // Got ESC]
    STATE_DCS,     // Got ESC P
    STATE_UTF8     // Processing UTF-8 sequence
  } parser_state;

  char escape_buf[256];   // Buffer for escape sequence
  int escape_len;         // Current length
  int escape_params[16];  // Numeric parameters
  int escape_param_count;

  // UTF-8 decoding state
  uint32_t utf8_char;
  int utf8_remaining;

  // Terminal modes
  bool auto_wrap;    // Auto-wrap at end of line
  bool origin_mode;  // Origin mode (relative to margins)
  bool insert_mode;  // Insert mode

  // Scroll region
  int scroll_top;
  int scroll_bottom;

  // Tab stops
  bool *tab_stops;
};

// Create terminal emulator
tui_term_emulator_t *tui_term_emulator_create(int width, int height,
                                              int scrollback_size);

// Destroy terminal emulator
void tui_term_emulator_destroy(tui_term_emulator_t *term);

// Resize terminal
void tui_term_emulator_resize(tui_term_emulator_t *term, int width, int height);

// Process input data (may contain escape sequences)
void tui_term_emulator_process(tui_term_emulator_t *term, const char *data,
                               size_t len);

// Get cell at position
const tui_term_cell_t *tui_term_emulator_get_cell(
    const tui_term_emulator_t *term, int x, int y);

// Get line from scrollback (negative offset from current)
const tui_term_cell_t *tui_term_emulator_get_scrollback_line(
    const tui_term_emulator_t *term, int offset);

// Clear screen
void tui_term_emulator_clear(tui_term_emulator_t *term);

// Reset terminal state
void tui_term_emulator_reset(tui_term_emulator_t *term);

// Cursor operations
void tui_term_emulator_move_cursor(tui_term_emulator_t *term, int x, int y);
void tui_term_emulator_show_cursor(tui_term_emulator_t *term, bool visible);
void tui_term_emulator_get_cursor(const tui_term_emulator_t *term, int *x,
                                  int *y, bool *visible);

// Scrolling
void tui_term_emulator_scroll_up(tui_term_emulator_t *term, int lines);
void tui_term_emulator_scroll_down(tui_term_emulator_t *term, int lines);

// Convert color to NCurses color pair
int tui_term_color_to_ncurses(uint16_t fg, uint16_t bg);

// Convert attributes to NCurses attributes
int tui_term_attrs_to_ncurses(uint16_t attrs);