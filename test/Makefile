# Makefile for TUI tests

CC = clang
CFLAGS = -Wall -Wextra -std=c23 -D_GNU_SOURCE -DENABLE_TUI_TESTS=1 -I../src
LDFLAGS = -lncurses

# Source files
TUI_SRCS = ../src/tui/tui_pane.c \
           ../src/tui/tui_layout.c \
           ../src/tui/tui_pty.c \
           ../src/tui/tui_mux.c \
           ../src/tui/tui_ncurses.c \
           ../src/tui/tui.c \
           ../src/tui/tui_render.c \
           ../src/tui/tui_input.c

UTIL_SRCS = ../src/utils/memory.c \
            ../src/utils/logging.c \
            ../src/core/error.c

# Test executables
TEST_EXES = test_tui_ncurses \
            test_tui_pane \
            test_tui_layout \
            test_tui_pty \
            test_tui_mux \
            test_tui \
            run_tui_tests

.PHONY: all clean test

all: $(TEST_EXES)

# NCurses test
test_tui_ncurses: tui_ncurses_test.c $(TUI_SRCS) $(UTIL_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Pane test
test_tui_pane: tui_pane_test.c $(TUI_SRCS) $(UTIL_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Layout test
test_tui_layout: tui_layout_test.c $(TUI_SRCS) $(UTIL_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# PTY test
test_tui_pty: tui_pty_test.c $(TUI_SRCS) $(UTIL_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Multiplexer test
test_tui_mux: tui_mux_test.c $(TUI_SRCS) $(UTIL_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Integration test
test_tui: tui_test.c $(TUI_SRCS) $(UTIL_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Test runner
run_tui_tests: run_tui_tests.c
	$(CC) $(CFLAGS) -o $@ $^

# Run all tests
test: all
	./run_tui_tests

# Run specific test
test-%: test_tui_%
	./$<

clean:
	rm -f $(TEST_EXES)

# Individual test targets
ncurses: test_tui_ncurses
	./$<

pane: test_tui_pane
	./$<

layout: test_tui_layout
	./$<

pty: test_tui_pty
	./$<

mux: test_tui_mux
	./$<

integration: test_tui
	./$<