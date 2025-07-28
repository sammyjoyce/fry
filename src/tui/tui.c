/*
 * Terminal UI (TUI) implementation using ncurses.
 */

#include "tui.h"

#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include <utils/logging.h>
#include <utils/memory.h>
#include "tui/backend/tui_ncurses.h"

static bool tui_initialized = false;
static app_terminal_caps_t terminal_caps;
static app_fallback_mode_t fallback_mode = FALLBACK_MODE_NONE;

app_error tui_init(void) {
  if (tui_initialized) {
    return APP_SUCCESS;
  }

  // Set locale for proper Unicode support
  setlocale(LC_ALL, "");

  // Initialize NCurses abstraction
  app_error err = app_ncurses_init(true);  // Use real NCurses
  if (err != APP_SUCCESS) {
    LOG_ERROR("Failed to initialize NCurses abstraction");
    return err;
  }

  // Initialize ncurses
  if (g_ncurses->initscr() == NULL) {
    LOG_ERROR("Failed to initialize ncurses");
    app_ncurses_cleanup();
    return APP_ERROR_INTERNAL;
  }

  // Detect terminal capabilities
  err = app_ncurses_detect_capabilities(&terminal_caps);
  if (err != APP_SUCCESS) {
    LOG_WARNING("Failed to detect terminal capabilities, using defaults");
  }

  // Determine fallback mode
  err = app_ncurses_get_fallback_mode(&terminal_caps, &fallback_mode);
  if (err != APP_SUCCESS) {
    LOG_WARNING("Failed to determine fallback mode, using full features");
    fallback_mode = FALLBACK_MODE_NONE;
  }

  // Configure ncurses
  g_ncurses->cbreak();              // Disable line buffering
  g_ncurses->noecho();              // Don't echo input
  g_ncurses->keypad(stdscr, TRUE);  // Enable special keys
  g_ncurses->curs_set(0);           // Hide cursor by default

  // Initialize colors if supported
  if (terminal_caps.has_colors && fallback_mode != FALLBACK_MODE_MONOCHROME) {
    g_ncurses->start_color();
    g_ncurses->use_default_colors();  // Use terminal's default colors
    app_error color_err = tui_init_colors();
    if (color_err != APP_SUCCESS) {
      LOG_WARNING("Failed to initialize colors: %s",
                  app_error_string(color_err));
    }
  }

  // Clear and refresh
  g_ncurses->clear();
  g_ncurses->refresh();

  tui_initialized = true;
  LOG_DEBUG("TUI initialized successfully (mode=%d, colors=%d, size=%dx%d)",
            fallback_mode, terminal_caps.has_colors,
            terminal_caps.terminal_width, terminal_caps.terminal_height);
  return APP_SUCCESS;
}

void tui_cleanup(void) {
  if (!tui_initialized) {
    return;
  }

  // Reset terminal
  g_ncurses->clear();
  g_ncurses->refresh();
  g_ncurses->endwin();

  // Cleanup NCurses abstraction
  app_ncurses_cleanup();

  tui_initialized = false;
  LOG_DEBUG("TUI cleaned up");
}

bool tui_is_initialized(void) {
  return tui_initialized;
}

app_error tui_init_colors(void) {
  if (!g_ncurses->has_colors()) {
    LOG_WARNING("Terminal does not support colors");
    return APP_SUCCESS;  // Not an error, just no colors
  }

  // Define color pairs
  g_ncurses->init_pair(TUI_COLOR_DEFAULT, -1, -1);
  g_ncurses->init_pair(TUI_COLOR_HIGHLIGHT, COLOR_BLACK, COLOR_WHITE);
  g_ncurses->init_pair(TUI_COLOR_ERROR, COLOR_RED, -1);
  g_ncurses->init_pair(TUI_COLOR_SUCCESS, COLOR_GREEN, -1);
  g_ncurses->init_pair(TUI_COLOR_WARNING, COLOR_YELLOW, -1);
  g_ncurses->init_pair(TUI_COLOR_INFO, COLOR_CYAN, -1);
  g_ncurses->init_pair(TUI_COLOR_MENU_SELECTED, COLOR_BLACK, COLOR_CYAN);
  g_ncurses->init_pair(TUI_COLOR_MENU_NORMAL, -1, -1);
  g_ncurses->init_pair(TUI_COLOR_BORDER, COLOR_BLUE, -1);
  g_ncurses->init_pair(TUI_COLOR_TITLE, COLOR_MAGENTA, -1);

  return APP_SUCCESS;
}

void tui_set_color(WINDOW *win, tui_color_pair_t color) {
  if (g_ncurses->has_colors() && color < TUI_COLOR_MAX) {
    NC_WATTRON(win, COLOR_PAIR(color));
  }
}

tui_window_t *tui_create_window(int height, int width, int y, int x) {
  tui_window_t *window = app_secure_malloc(sizeof(tui_window_t));
  if (!window) {
    return NULL;
  }

  window->height = height;
  window->width = width;
  window->y = y;
  window->x = x;
  window->has_border = false;
  window->title = NULL;

  window->win = g_ncurses->newwin(height, width, y, x);
  if (!window->win) {
    app_secure_free(window, sizeof(tui_window_t));
    return NULL;
  }

  g_ncurses->keypad(window->win, TRUE);  // Enable special keys for this window
  return window;
}

void tui_destroy_window(tui_window_t *window) {
  if (!window) {
    return;
  }

  if (window->win) {
    g_ncurses->delwin(window->win);
  }

  if (window->title) {
    app_secure_free(window->title, strlen(window->title) + 1);
  }

  app_secure_free(window, sizeof(tui_window_t));
}

void tui_draw_border(tui_window_t *window) {
  if (!window || !window->win) {
    return;
  }

  tui_set_color(window->win, TUI_COLOR_BORDER);
  NC_BOX(window->win, 0, 0);
  NC_WATTROFF(window->win, COLOR_PAIR(TUI_COLOR_BORDER));
  window->has_border = true;

  // Draw title if set
  if (window->title) {
    tui_set_window_title(window, window->title);
  }
}

void tui_set_window_title(tui_window_t *window, const char *title) {
  if (!window || !window->win || !title) {
    return;
  }

  // Store title
  if (window->title) {
    app_secure_free(window->title, strlen(window->title) + 1);
  }
  window->title = app_secure_strdup(title);

  // Draw title if window has border
  if (window->has_border) {
    const int max_width = window->width - 4;  // Account for borders and spacing
    const size_t title_len = strlen(title);
    const int display_len =
        (title_len > (size_t)max_width) ? max_width : (int)title_len;

    const int x_pos = (window->width - display_len) / 2;
    tui_set_color(window->win, TUI_COLOR_TITLE);
    g_ncurses->mvwprintw(window->win, 0, x_pos, " %.*s ", max_width, title);
    NC_WATTROFF(window->win, COLOR_PAIR(TUI_COLOR_TITLE));
  }
}

void tui_refresh_window(tui_window_t *window) {
  if (window && window->win) {
    g_ncurses->wrefresh(window->win);
  }
}

void tui_clear_window(tui_window_t *window) {
  if (window && window->win) {
    g_ncurses->wclear(window->win);
    if (window->has_border) {
      tui_draw_border(window);
    }
  }
}

void tui_draw_box(tui_window_t *window, int y, int x, int height, int width) {
  if (!window || !window->win) {
    return;
  }

  WINDOW *win = window->win;

  // Draw corners
  g_ncurses->wmove(win, y, x);
  g_ncurses->waddch(win, ACS_ULCORNER);
  g_ncurses->wmove(win, y, x + width - 1);
  g_ncurses->waddch(win, ACS_URCORNER);
  g_ncurses->wmove(win, y + height - 1, x);
  g_ncurses->waddch(win, ACS_LLCORNER);
  g_ncurses->wmove(win, y + height - 1, x + width - 1);
  g_ncurses->waddch(win, ACS_LRCORNER);

  // Draw horizontal lines
  for (int i = 1; i < width - 1; i++) {
    g_ncurses->wmove(win, y, x + i);
    g_ncurses->waddch(win, ACS_HLINE);
    g_ncurses->wmove(win, y + height - 1, x + i);
    g_ncurses->waddch(win, ACS_HLINE);
  }

  // Draw vertical lines
  for (int i = 1; i < height - 1; i++) {
    g_ncurses->wmove(win, y + i, x);
    g_ncurses->waddch(win, ACS_VLINE);
    g_ncurses->wmove(win, y + i, x + width - 1);
    g_ncurses->waddch(win, ACS_VLINE);
  }
}

void tui_mvprint(tui_window_t *window, int y, int x, const char *text) {
  if (!window || !window->win || !text) {
    return;
  }

  g_ncurses->mvwprintw(window->win, y, x, "%s", text);
}

void tui_print_centered(WINDOW *win, int y, const char *text) {
  if (!win || !text) {
    return;
  }

  int max_x = g_ncurses->getmaxx(win);
  int len = strlen(text);
  int x = (max_x - len) / 2;
  if (x < 0)
    x = 0;

  g_ncurses->mvwprintw(win, y, x, "%.*s", max_x, text);
}

void tui_print_wrapped(WINDOW *win, int y, int x, int width, const char *text) {
  if (!win || !text) {
    return;
  }

  int current_y = y;
  int current_x = x;
  const char *word_start = text;
  const char *p = text;

  while (*p) {
    // Find end of current word
    while (*p && *p != ' ' && *p != '\n') {
      p++;
    }

    int word_len = p - word_start;

    // Check if word fits on current line
    if (current_x + word_len > x + width) {
      current_y++;
      current_x = x;
    }

    // Print word
    NC_MVWADDNSTR(win, current_y, current_x, word_start, word_len);
    current_x += word_len;

    // Handle space or newline
    if (*p == ' ') {
      current_x++;
      p++;
    } else if (*p == '\n') {
      current_y++;
      current_x = x;
      p++;
    }

    word_start = p;
  }
}

int tui_get_char(void) {
  return g_ncurses->getch();
}

app_error tui_get_string(WINDOW *win, char *buffer, size_t size,
                         const char *prompt) {
  if (!win || !buffer || size == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  // Show prompt
  if (prompt) {
    wprintw(win, "%s", prompt);
  }

  // Enable cursor and echo temporarily
  curs_set(1);
  echo();

  // Get string
  int result = g_ncurses->wgetnstr(win, buffer, size - 1);

  // Restore settings
  noecho();
  curs_set(0);

  if (result == ERR) {
    return APP_ERROR_IO;
  }

  return APP_SUCCESS;
}

int tui_show_menu(tui_window_t *window, const char *title,
                  tui_menu_item_t *items, int item_count,
                  int default_selection) {
  if (!window || !items || item_count <= 0) {
    return -1;
  }

  int selected = default_selection;
  if (selected < 0 || selected >= item_count) {
    selected = 0;
  }

  // Find first enabled item if default is disabled
  while (selected < item_count && !items[selected].enabled) {
    selected++;
  }
  if (selected >= item_count) {
    return -1;  // No enabled items
  }

  while (1) {
    tui_clear_window(window);

    // Draw title
    if (title) {
      tui_set_color(window->win, TUI_COLOR_TITLE);
      tui_print_centered(window->win, 1, title);
      NC_WATTROFF(window->win, COLOR_PAIR(TUI_COLOR_TITLE));
    }

    // Draw menu items
    int start_y = title ? 3 : 1;
    for (int i = 0; i < item_count; i++) {
      int y = start_y + i * 2;  // Space between items

      if (!items[i].enabled) {
        // Disabled item
        g_ncurses->mvwprintw(window->win, y, 4, "  %s (disabled)",
                             items[i].label);
      } else if (i == selected) {
        // Selected item
        tui_set_color(window->win, TUI_COLOR_MENU_SELECTED);
        g_ncurses->mvwprintw(window->win, y, 2, "> %s", items[i].label);
        NC_WATTROFF(window->win, COLOR_PAIR(TUI_COLOR_MENU_SELECTED));

        // Show description if available
        if (items[i].description) {
          g_ncurses->mvwprintw(window->win, y + 1, 6, "%s",
                               items[i].description);
        }
      } else {
        // Normal item
        g_ncurses->mvwprintw(window->win, y, 4, "%s", items[i].label);
      }
    }

    // Instructions
    int bottom_y = window->height - 2;
    tui_set_color(window->win, TUI_COLOR_INFO);
    g_ncurses->mvwprintw(window->win, bottom_y, 2,
                         "Use ↑/↓ to navigate, Enter to select, q to cancel");
    NC_WATTROFF(window->win, COLOR_PAIR(TUI_COLOR_INFO));

    tui_refresh_window(window);

    // Handle input
    int ch = tui_get_char();
    switch (ch) {
    case KEY_UP:
    case 'k':
      do {
        selected--;
        if (selected < 0)
          selected = item_count - 1;
      } while (!items[selected].enabled);
      break;

    case KEY_DOWN:
    case 'j':
      do {
        selected++;
        if (selected >= item_count)
          selected = 0;
      } while (!items[selected].enabled);
      break;

    case '\n':
    case KEY_ENTER:
      return items[selected].id;

    case 'q':
    case 27:  // ESC
      return -1;
    }
  }
}

void tui_show_message(const char *title, const char *message) {
  if (!tui_initialized) {
    return;
  }

  int max_y = g_ncurses->getmaxy(stdscr);
  int max_x = g_ncurses->getmaxx(stdscr);

  int width = 60;
  int height = 10;
  if (width > max_x - 4)
    width = max_x - 4;
  if (height > max_y - 4)
    height = max_y - 4;

  int y = (max_y - height) / 2;
  int x = (max_x - width) / 2;

  tui_window_t *window = tui_create_window(height, width, y, x);
  if (!window) {
    return;
  }

  tui_draw_border(window);
  if (title) {
    tui_set_window_title(window, title);
  }

  // Print message
  if (message) {
    tui_print_wrapped(window->win, 2, 2, width - 4, message);
  }

  // Instructions
  tui_set_color(window->win, TUI_COLOR_INFO);
  tui_print_centered(window->win, height - 2, "Press any key to continue");
  NC_WATTROFF(window->win, COLOR_PAIR(TUI_COLOR_INFO));

  tui_refresh_window(window);
  (void)tui_get_char();

  tui_destroy_window(window);
  NC_TOUCHWIN(app_ncurses_stdscr());
  g_ncurses->refresh();
}

bool tui_confirm(const char *title, const char *question) {
  if (!tui_initialized) {
    return false;
  }

  int max_y = g_ncurses->getmaxy(stdscr);
  int max_x = g_ncurses->getmaxx(stdscr);

  int width = 50;
  int height = 8;
  if (width > max_x - 4)
    width = max_x - 4;

  int y = (max_y - height) / 2;
  int x = (max_x - width) / 2;

  tui_window_t *window = tui_create_window(height, width, y, x);
  if (!window) {
    return false;
  }

  tui_draw_border(window);
  if (title) {
    tui_set_window_title(window, title);
  }

  // Print question
  if (question) {
    tui_print_wrapped(window->win, 2, 2, width - 4, question);
  }

  // Instructions
  tui_set_color(window->win, TUI_COLOR_INFO);
  tui_print_centered(window->win, height - 2, "y/n");
  NC_WATTROFF(window->win, COLOR_PAIR(TUI_COLOR_INFO));

  tui_refresh_window(window);

  bool result = false;
  while (1) {
    int ch = tui_get_char();
    if (ch == 'y' || ch == 'Y') {
      result = true;
      break;
    } else if (ch == 'n' || ch == 'N' || ch == 27) {  // ESC
      result = false;
      break;
    }
  }

  tui_destroy_window(window);
  NC_TOUCHWIN(app_ncurses_stdscr());
  g_ncurses->refresh();
  return result;
}

app_error tui_input_dialog(const char *title, const char *prompt, char *buffer,
                           size_t size) {
  if (!tui_initialized || !buffer || size == 0) {
    return APP_ERROR_INVALID_ARG;
  }

  int max_y = g_ncurses->getmaxy(stdscr);
  int max_x = g_ncurses->getmaxx(stdscr);

  int width = 60;
  int height = 8;
  if (width > max_x - 4)
    width = max_x - 4;

  int y = (max_y - height) / 2;
  int x = (max_x - width) / 2;

  tui_window_t *window = tui_create_window(height, width, y, x);
  if (!window) {
    return APP_ERROR_INTERNAL;
  }

  tui_draw_border(window);
  if (title) {
    tui_set_window_title(window, title);
  }

  // Print prompt
  if (prompt) {
    g_ncurses->mvwprintw(window->win, 2, 2, "%s", prompt);
  }

  // Input field
  g_ncurses->mvwprintw(window->win, 4, 2, "> ");
  tui_refresh_window(window);

  app_error result = tui_get_string(window->win, buffer, size, NULL);

  tui_destroy_window(window);
  NC_TOUCHWIN(app_ncurses_stdscr());
  g_ncurses->refresh();
  return result;
}

void tui_beep(void) {
  if (g_ncurses && g_ncurses->beep) {
    g_ncurses->beep();
  }
}

void tui_flash(void) {
  if (g_ncurses && g_ncurses->flash) {
    g_ncurses->flash();
  }
}

int tui_get_max_x(void) {
  return g_ncurses->getmaxx(stdscr);
}

int tui_get_max_y(void) {
  return g_ncurses->getmaxy(stdscr);
}
