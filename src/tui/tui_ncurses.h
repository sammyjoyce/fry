#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef _WIN32
#include <curses.h>
#else
#include <ncurses.h>
#endif

#include "../core/error.h"
#include "../core/types.h"

// NCurses abstraction layer for testability
// This allows us to mock NCurses functions in tests

// Function pointers for NCurses operations
typedef struct {
  // Initialization
  WINDOW *(*initscr)(void);
  int (*endwin)(void);
  int (*cbreak)(void);
  int (*nocbreak)(void);
  int (*echo)(void);
  int (*noecho)(void);
  int (*keypad)(WINDOW *, bool);
  int (*curs_set)(int);

  // Window operations
  WINDOW *(*newwin)(int, int, int, int);
  int (*delwin)(WINDOW *);
  int (*wrefresh)(WINDOW *);
  int (*wclear)(WINDOW *);
  int (*box)(WINDOW *, chtype, chtype);
  int (*wborder)(WINDOW *, chtype, chtype, chtype, chtype, chtype, chtype,
                 chtype, chtype);

  // Output operations
  int (*mvwprintw)(WINDOW *, int, int, const char *, ...);
  int (*mvwaddch)(WINDOW *, int, int, chtype);
  int (*wmove)(WINDOW *, int, int);
  int (*waddnstr)(WINDOW *, const char *, int);
  int (*wattr_on)(WINDOW *, attr_t, void *);
  int (*wattr_off)(WINDOW *, attr_t, void *);

  // Input operations
  int (*getch)(void);
  int (*wgetch)(WINDOW *);
  int (*wgetnstr)(WINDOW *, char *, int);

  // Screen info
  int (*getmaxx)(const WINDOW *);
  int (*getmaxy)(const WINDOW *);

  // Color support
  bool (*has_colors)(void);
  int (*start_color)(void);
  int (*use_default_colors)(void);
  int (*init_pair)(short, short, short);

  // Mouse support
  mmask_t (*mousemask)(mmask_t, mmask_t *);
  int (*getmouse)(MEVENT *);

  // Misc
  int (*beep)(void);
  int (*flash)(void);
  int (*napms)(int);
  int (*clear)(void);
  int (*refresh)(void);
  int (*touchwin)(WINDOW *);
  int (*wtouchln)(WINDOW *, int, int, int);

  // Attributes
  int (*attron)(int);
  int (*attroff)(int);
  int (*mvprintw)(int, int, const char *, ...);
  int (*mvaddch)(int, int, chtype);
} app_ncurses_vtable_t;

// Global NCurses vtable
extern app_ncurses_vtable_t *g_ncurses;

// Helper macros for common operations
#define NC_WATTRON(win, attr) g_ncurses->wattr_on(win, (attr_t)(attr), NULL)
#define NC_WATTROFF(win, attr) g_ncurses->wattr_off(win, (attr_t)(attr), NULL)
#define NC_BOX(win, v, h) g_ncurses->wborder(win, v, v, h, h, 0, 0, 0, 0)
#define NC_MVWADDNSTR(win, y, x, str, n) \
  (g_ncurses->wmove(win, y, x) == ERR ? ERR : g_ncurses->waddnstr(win, str, n))
#define NC_TOUCHWIN(win) g_ncurses->wtouchln(win, 0, g_ncurses->getmaxy(win), 1)

// Initialize NCurses abstraction
APP_NODISCARD app_error app_ncurses_init(bool use_real_ncurses);

// Cleanup NCurses abstraction
void app_ncurses_cleanup(void);

// Terminal capability detection
typedef struct {
  bool has_colors;
  bool can_change_colors;
  bool has_mouse;
  int max_colors;
  int max_pairs;
  int terminal_width;
  int terminal_height;
  const char *term_type;
} app_terminal_caps_t;

// Detect terminal capabilities
APP_NODISCARD app_error
app_ncurses_detect_capabilities(app_terminal_caps_t *caps);

// Fallback mode support
typedef enum {
  FALLBACK_MODE_NONE,        // Full NCurses support
  FALLBACK_MODE_MONOCHROME,  // No color support
  FALLBACK_MODE_ASCII,       // ASCII-only borders
  FALLBACK_MODE_MINIMAL      // Minimal TUI
} app_fallback_mode_t;

// Determine appropriate fallback mode
APP_NODISCARD app_error app_ncurses_get_fallback_mode(
    const app_terminal_caps_t *caps, app_fallback_mode_t *mode);

// Mock NCurses for testing
#ifdef ENABLE_TUI_TESTS

// Mock window structure
typedef struct mock_window {
  int height;
  int width;
  int y;
  int x;
  char **content;  // 2D array of characters
  int *attrs;      // Attributes for each position
  bool is_deleted;
} mock_window_t;

// Mock terminal state
typedef struct {
  int width;
  int height;
  bool colors_enabled;
  bool echo_enabled;
  bool cbreak_enabled;
  int cursor_visibility;
  mock_window_t *stdscr;
  mock_window_t **windows;
  size_t window_count;
  size_t window_capacity;

  // Input queue for testing
  int *input_queue;
  size_t input_count;
  size_t input_capacity;
  size_t input_pos;

  // Color pairs
  struct {
    short fg;
    short bg;
  } color_pairs[256];

  // Mouse state
  mmask_t mouse_mask;
  MEVENT last_mouse_event;
} app_mock_terminal_t;

// Initialize mock NCurses
APP_NODISCARD app_error app_mock_ncurses_init(app_mock_terminal_t **terminal,
                                              int width, int height);

// Cleanup mock NCurses
void app_mock_ncurses_cleanup(app_mock_terminal_t *terminal);

// Queue input for mock getch
APP_NODISCARD app_error
app_mock_ncurses_queue_input(app_mock_terminal_t *terminal, int key);

// Queue mouse event
APP_NODISCARD app_error app_mock_ncurses_queue_mouse(
    app_mock_terminal_t *terminal, int x, int y, mmask_t button_state);

// Get mock window content
APP_NODISCARD app_error app_mock_ncurses_get_content(
    app_mock_terminal_t *terminal, mock_window_t *window, int y, int x,
    char *ch, int *attr);

// Verify mock window content
APP_NODISCARD bool app_mock_ncurses_verify_text(app_mock_terminal_t *terminal,
                                                mock_window_t *window, int y,
                                                int x, const char *expected);

#endif  // ENABLE_TUI_TESTS