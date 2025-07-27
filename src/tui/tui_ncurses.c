#include "tui_ncurses.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../utils/logging.h"
#include "../utils/memory.h"

// Global NCurses vtable
app_ncurses_vtable_t *g_ncurses = NULL;

// Real NCurses vtable
static app_ncurses_vtable_t real_ncurses = {
    // Initialization
    .initscr = initscr,
    .endwin = endwin,
    .cbreak = cbreak,
    .nocbreak = nocbreak,
    .echo = echo,
    .noecho = noecho,
    .keypad = keypad,
    .curs_set = curs_set,

    // Window operations
    .newwin = newwin,
    .delwin = delwin,
    .wrefresh = wrefresh,
    .wclear = wclear,
    .box = box,
    .wborder = wborder,

    // Output operations
    .mvwprintw = mvwprintw,
    .mvwaddch = mvwaddch,
    .wmove = wmove,
    .waddnstr = waddnstr,
    .wattr_on = wattr_on,
    .wattr_off = wattr_off,

    // Input operations
    .getch = getch,
    .wgetch = wgetch,
    .wgetnstr = wgetnstr,

    // Screen info
    .getmaxx = getmaxx,
    .getmaxy = getmaxy,

    // Color support
    .has_colors = has_colors,
    .start_color = start_color,
    .use_default_colors = use_default_colors,
    .init_pair = init_pair,

    // Mouse support
    .mousemask = mousemask,
    .getmouse = getmouse,

    // Misc
    .beep = beep,
    .flash = flash,
    .napms = napms,
    .clear = clear,
    .refresh = refresh,
    .touchwin = touchwin,
    .wtouchln = wtouchln,

    // Attributes
    .attron = attron,
    .attroff = attroff,
    .mvprintw = mvprintw,
    .mvaddch = mvaddch};

// Initialize NCurses abstraction
app_error app_ncurses_init(bool use_real_ncurses) {
  if (use_real_ncurses) {
    g_ncurses = &real_ncurses;
    LOG_DEBUG("Using real NCurses implementation");
  } else {
#ifdef ENABLE_TUI_TESTS
    // Mock implementation will be set by test code
    LOG_DEBUG("Using mock NCurses implementation");
#else
    LOG_ERROR("Mock NCurses not available (ENABLE_TUI_TESTS not defined)");
    return APP_ERROR_NOT_IMPLEMENTED;
#endif
  }

  return APP_SUCCESS;
}

// Cleanup NCurses abstraction
void app_ncurses_cleanup(void) {
  g_ncurses = NULL;
}

// Detect terminal capabilities
app_error app_ncurses_detect_capabilities(app_terminal_caps_t *caps) {
  if (!caps) {
    return APP_ERROR_INVALID_ARG;
  }

  memset(caps, 0, sizeof(*caps));

  // Get terminal type
  const char *term = getenv("TERM");
  caps->term_type = term ? term : "unknown";

  // Check if NCurses is initialized
  if (!g_ncurses || !stdscr) {
    // Try to get basic info without initializing NCurses
    caps->terminal_width = 80;   // Default
    caps->terminal_height = 24;  // Default

    // Check COLUMNS and LINES environment variables
    const char *cols_env = getenv("COLUMNS");
    const char *lines_env = getenv("LINES");
    if (cols_env) {
      caps->terminal_width = atoi(cols_env);
    }
    if (lines_env) {
      caps->terminal_height = atoi(lines_env);
    }

    return APP_SUCCESS;
  }

  // Get terminal dimensions
  caps->terminal_width = g_ncurses->getmaxx(stdscr);
  caps->terminal_height = g_ncurses->getmaxy(stdscr);

  // Check color support
  caps->has_colors = g_ncurses->has_colors();
  if (caps->has_colors) {
    caps->max_colors = COLORS;
    caps->max_pairs = COLOR_PAIRS;
    caps->can_change_colors = can_change_color();
  }

  // Check mouse support
  mmask_t old_mask;
  mmask_t test_mask = ALL_MOUSE_EVENTS;
  mmask_t result = g_ncurses->mousemask(test_mask, &old_mask);
  caps->has_mouse = (result != 0);
  // Restore old mouse mask
  g_ncurses->mousemask(old_mask, NULL);

  LOG_DEBUG("Terminal capabilities: %dx%d, colors=%d, mouse=%d, term=%s",
            caps->terminal_width, caps->terminal_height, caps->has_colors,
            caps->has_mouse, caps->term_type);

  return APP_SUCCESS;
}

// Determine appropriate fallback mode
app_error app_ncurses_get_fallback_mode(const app_terminal_caps_t *caps,
                                        app_fallback_mode_t *mode) {
  if (!caps || !mode) {
    return APP_ERROR_INVALID_ARG;
  }

  *mode = FALLBACK_MODE_NONE;

  // Check terminal size
  if (caps->terminal_width < 40 || caps->terminal_height < 10) {
    *mode = FALLBACK_MODE_MINIMAL;
    LOG_INFO("Terminal too small, using minimal mode");
    return APP_SUCCESS;
  }

  // Check for dumb terminal
  if (caps->term_type && strcmp(caps->term_type, "dumb") == 0) {
    *mode = FALLBACK_MODE_ASCII;
    LOG_INFO("Dumb terminal detected, using ASCII mode");
    return APP_SUCCESS;
  }

  // Check color support
  if (!caps->has_colors) {
    *mode = FALLBACK_MODE_MONOCHROME;
    LOG_INFO("No color support, using monochrome mode");
    return APP_SUCCESS;
  }

  // Check for specific terminals that need ASCII mode
  if (caps->term_type) {
    const char *ascii_terms[] = {"vt100", "vt102", "vt220", "ansi", NULL};

    for (const char **term = ascii_terms; *term; term++) {
      if (strstr(caps->term_type, *term)) {
        *mode = FALLBACK_MODE_ASCII;
        LOG_INFO("Terminal %s detected, using ASCII mode", *term);
        return APP_SUCCESS;
      }
    }
  }

  return APP_SUCCESS;
}

#ifdef ENABLE_TUI_TESTS

// Mock implementations for testing

static app_mock_terminal_t *g_mock_terminal = NULL;

// Mock initscr
static WINDOW *mock_initscr(void) {
  if (!g_mock_terminal) {
    return NULL;
  }
  return (WINDOW *)g_mock_terminal->stdscr;
}

// Mock endwin
static int mock_endwin(void) {
  return OK;
}

// Mock newwin
static WINDOW *mock_newwin(int height, int width, int y, int x) {
  if (!g_mock_terminal) {
    return NULL;
  }

  // Expand window array if needed
  if (g_mock_terminal->window_count >= g_mock_terminal->window_capacity) {
    size_t new_capacity = g_mock_terminal->window_capacity * 2;
    mock_window_t **new_windows = app_realloc(
        g_mock_terminal->windows, new_capacity * sizeof(mock_window_t *));
    if (!new_windows) {
      return NULL;
    }
    g_mock_terminal->windows = new_windows;
    g_mock_terminal->window_capacity = new_capacity;
  }

  // Create mock window
  mock_window_t *win = app_calloc(1, sizeof(mock_window_t));
  if (!win) {
    return NULL;
  }

  win->height = height;
  win->width = width;
  win->y = y;
  win->x = x;

  // Allocate content buffer
  win->content = app_calloc(height, sizeof(char *));
  if (!win->content) {
    app_free(win);
    return NULL;
  }

  for (int i = 0; i < height; i++) {
    win->content[i] = app_calloc(width + 1, sizeof(char));
    if (!win->content[i]) {
      // Cleanup on failure
      for (int j = 0; j < i; j++) {
        app_free(win->content[j]);
      }
      app_free(win->content);
      app_free(win);
      return NULL;
    }
    // Initialize with spaces
    memset(win->content[i], ' ', width);
  }

  // Allocate attributes buffer
  win->attrs = app_calloc(height * width, sizeof(int));
  if (!win->attrs) {
    for (int i = 0; i < height; i++) {
      app_free(win->content[i]);
    }
    app_free(win->content);
    app_free(win);
    return NULL;
  }

  g_mock_terminal->windows[g_mock_terminal->window_count++] = win;

  return (WINDOW *)win;
}

// Mock delwin
static int mock_delwin(WINDOW *win) {
  if (!win || !g_mock_terminal) {
    return ERR;
  }

  mock_window_t *mwin = (mock_window_t *)win;
  mwin->is_deleted = true;

  // Free content
  if (mwin->content) {
    for (int i = 0; i < mwin->height; i++) {
      app_free(mwin->content[i]);
    }
    app_free(mwin->content);
  }
  app_free(mwin->attrs);

  return OK;
}

// Mock getch
static int mock_getch(void) {
  if (!g_mock_terminal ||
      g_mock_terminal->input_pos >= g_mock_terminal->input_count) {
    return ERR;
  }

  return g_mock_terminal->input_queue[g_mock_terminal->input_pos++];
}

// Mock has_colors
static bool mock_has_colors(void) {
  return g_mock_terminal ? g_mock_terminal->colors_enabled : false;
}

// Mock getmaxx
static int mock_getmaxx(WINDOW *win) {
  if (!win) {
    return g_mock_terminal ? g_mock_terminal->width : 80;
  }
  mock_window_t *mwin = (mock_window_t *)win;
  return mwin->width;
}

// Mock getmaxy
static int mock_getmaxy(WINDOW *win) {
  if (!win) {
    return g_mock_terminal ? g_mock_terminal->height : 24;
  }
  mock_window_t *mwin = (mock_window_t *)win;
  return mwin->height;
}

// Mock mvwprintw
static int mock_mvwprintw(WINDOW *win, int y, int x, const char *fmt, ...) {
  if (!win || !fmt) {
    return ERR;
  }

  mock_window_t *mwin = (mock_window_t *)win;
  if (y < 0 || y >= mwin->height || x < 0 || x >= mwin->width) {
    return ERR;
  }

  // Format string
  char buffer[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  // Copy to window content
  size_t len = strlen(buffer);
  size_t max_len = mwin->width - x;
  if (len > max_len) {
    len = max_len;
  }

  memcpy(&mwin->content[y][x], buffer, len);

  return OK;
}

// Mock NCurses vtable
static app_ncurses_vtable_t mock_ncurses = {
    .initscr = mock_initscr,
    .endwin = mock_endwin,
    .cbreak = (int (*)(void))mock_endwin,            // Stub
    .nocbreak = (int (*)(void))mock_endwin,          // Stub
    .echo = (int (*)(void))mock_endwin,              // Stub
    .noecho = (int (*)(void))mock_endwin,            // Stub
    .keypad = (int (*)(WINDOW *, bool))mock_delwin,  // Stub
    .curs_set = (int (*)(int))mock_getch,            // Stub

    .newwin = mock_newwin,
    .delwin = mock_delwin,
    .wrefresh = mock_delwin,                                // Stub
    .wclear = mock_delwin,                                  // Stub
    .box = (int (*)(WINDOW *, chtype, chtype))mock_delwin,  // Stub

    .mvwprintw = mock_mvwprintw,
    .mvwaddch = (int (*)(WINDOW *, int, int, chtype))mock_delwin,  // Stub
    .mvwaddnstr =
        (int (*)(WINDOW *, int, int, const char *, int))mock_delwin,  // Stub
    .wattron = mock_delwin,                                           // Stub
    .wattroff = mock_delwin,                                          // Stub

    .getch = mock_getch,
    .wgetch = (int (*)(WINDOW *))mock_getch,                  // Stub
    .wgetnstr = (int (*)(WINDOW *, char *, int))mock_delwin,  // Stub

    .getmaxx = mock_getmaxx,
    .getmaxy = mock_getmaxy,

    .has_colors = mock_has_colors,
    .start_color = (int (*)(void))mock_endwin,               // Stub
    .use_default_colors = (int (*)(void))mock_endwin,        // Stub
    .init_pair = (int (*)(short, short, short))mock_endwin,  // Stub

    .mousemask = (mmask_t (*)(mmask_t, mmask_t *))mock_endwin,  // Stub
    .getmouse = (int (*)(MEVENT *))mock_endwin,                 // Stub

    .beep = (int (*)(void))mock_endwin,     // Stub
    .flash = (int (*)(void))mock_endwin,    // Stub
    .napms = (int (*)(int))mock_getch,      // Stub
    .clear = (int (*)(void))mock_endwin,    // Stub
    .refresh = (int (*)(void))mock_endwin,  // Stub
    .touchwin = mock_delwin,                // Stub

    .attron = (int (*)(int))mock_getch,                                // Stub
    .attroff = (int (*)(int))mock_getch,                               // Stub
    .mvprintw = (int (*)(int, int, const char *, ...))mock_mvwprintw,  // Stub
    .mvaddch = (int (*)(int, int, chtype))mock_delwin                  // Stub
};

// Initialize mock NCurses
app_error app_mock_ncurses_init(app_mock_terminal_t **terminal, int width,
                                int height) {
  if (!terminal) {
    return APP_ERROR_INVALID_ARG;
  }

  app_mock_terminal_t *term = app_calloc(1, sizeof(app_mock_terminal_t));
  if (!term) {
    return APP_ERROR_MEMORY;
  }

  term->width = width;
  term->height = height;
  term->colors_enabled = true;
  term->echo_enabled = false;
  term->cbreak_enabled = true;
  term->cursor_visibility = 0;

  // Create stdscr
  term->stdscr = app_calloc(1, sizeof(mock_window_t));
  if (!term->stdscr) {
    app_free(term);
    return APP_ERROR_MEMORY;
  }

  term->stdscr->width = width;
  term->stdscr->height = height;
  term->stdscr->x = 0;
  term->stdscr->y = 0;

  // Initialize window array
  term->window_capacity = 10;
  term->windows = app_calloc(term->window_capacity, sizeof(mock_window_t *));
  if (!term->windows) {
    app_free(term->stdscr);
    app_free(term);
    return APP_ERROR_MEMORY;
  }

  // Initialize input queue
  term->input_capacity = 100;
  term->input_queue = app_calloc(term->input_capacity, sizeof(int));
  if (!term->input_queue) {
    app_free(term->windows);
    app_free(term->stdscr);
    app_free(term);
    return APP_ERROR_MEMORY;
  }

  *terminal = term;
  g_mock_terminal = term;
  g_ncurses = &mock_ncurses;

  return APP_SUCCESS;
}

// Cleanup mock NCurses
void app_mock_ncurses_cleanup(app_mock_terminal_t *terminal) {
  if (!terminal) {
    return;
  }

  // Free windows
  if (terminal->windows) {
    for (size_t i = 0; i < terminal->window_count; i++) {
      if (terminal->windows[i] && !terminal->windows[i]->is_deleted) {
        mock_delwin((WINDOW *)terminal->windows[i]);
      }
      app_free(terminal->windows[i]);
    }
    app_free(terminal->windows);
  }

  // Free stdscr
  app_free(terminal->stdscr);

  // Free input queue
  app_free(terminal->input_queue);

  if (g_mock_terminal == terminal) {
    g_mock_terminal = NULL;
    g_ncurses = NULL;
  }

  app_free(terminal);
}

// Queue input for mock getch
app_error app_mock_ncurses_queue_input(app_mock_terminal_t *terminal, int key) {
  if (!terminal) {
    return APP_ERROR_INVALID_ARG;
  }

  if (terminal->input_count >= terminal->input_capacity) {
    // Expand queue
    size_t new_capacity = terminal->input_capacity * 2;
    int *new_queue =
        app_realloc(terminal->input_queue, new_capacity * sizeof(int));
    if (!new_queue) {
      return APP_ERROR_MEMORY;
    }
    terminal->input_queue = new_queue;
    terminal->input_capacity = new_capacity;
  }

  terminal->input_queue[terminal->input_count++] = key;

  return APP_SUCCESS;
}

// Verify mock window content
bool app_mock_ncurses_verify_text(app_mock_terminal_t *terminal,
                                  mock_window_t *window, int y, int x,
                                  const char *expected) {
  if (!terminal || !window || !expected) {
    return false;
  }

  if (y < 0 || y >= window->height || x < 0 || x >= window->width) {
    return false;
  }

  size_t len = strlen(expected);
  if (x + len > (size_t)window->width) {
    return false;
  }

  return memcmp(&window->content[y][x], expected, len) == 0;
}

#endif  // ENABLE_TUI_TESTS