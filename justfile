# Justfile for C23 CLI Template

# Build the project
build:
    zig build

default: build

# Build with release optimization
release:
    zig build -Doptimize=ReleaseSafe

# Run the application (with optional arguments)
run *args:
    zig build run -- {{ args }}

# Run tests
test:
    zig build test

# Run tests with different optimization levels
test-debug:
    zig build test -Doptimize=Debug

test-release:
    zig build test -Doptimize=ReleaseSafe

test-fast:
    zig build test -Doptimize=ReleaseFast

# Build and run TUI tests
test-tui:
    cd test && make -f Makefile test

# Run specific TUI test suite
test-tui-ncurses:
    cd test && make -f Makefile ncurses

test-tui-pane:
    cd test && make -f Makefile pane

test-tui-layout:
    cd test && make -f Makefile layout

test-tui-pty:
    cd test && make -f Makefile pty

test-tui-mux:
    cd test && make -f Makefile mux

# Build TUI tests without running
build-tui-tests:
    cd test && make -f Makefile all

# Clean TUI test artifacts
clean-tui-tests:
    cd test && make -f Makefile clean

# Check code formatting
fmt:
    zig build fmt

fmt-check:
    zig build fmt-check

# Run all checks
check:
    zig build check

# Clean build artifacts
clean:
    zig build clean

# Install dependencies (if any)
install:
    # No additional installation needed for this template
    echo "No additional installation needed"

# Generate documentation
docs:
    # Generate documentation using your preferred tool
    echo "Documentation generation not configured"

# Display help
help:
    echo "Available commands:"
    echo "  build       - Build the project"
    echo "  release     - Build with release optimization"
    echo "  run         - Run the application"
    echo "  test        - Run tests"
    echo "  test-debug  - Run tests with debug optimization"
    echo "  test-release - Run tests with release optimization"
    echo "  test-fast   - Run tests with fast optimization"
    echo "  fmt         - Format code"
    echo "  fmt-check   - Check code formatting"
    echo "  check       - Run all checks"
    echo "  clean       - Clean build artifacts"
    echo "  install     - Install dependencies"
    echo "  docs        - Generate documentation"
    echo "  help        - Display this help message"
