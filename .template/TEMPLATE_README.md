# myapp

A modern CLI application

## Features

- Modern C23 standard for better compatibility
- Zig build system for fast, reliable builds
- NCurses TUI support for interactive interfaces
- Secure memory management
- Structured logging with configurable levels
- Configuration system (files, environment, command-line)
- Color output with smart terminal detection
- Comprehensive error handling
- Testing framework
- OpenCLI compliant

## Quick Start

### Prerequisites

- Zig (master branch recommended): Install via [zvm](https://github.com/tristanisham/zvm)
- C compiler (for system libraries)

### Build

```bash
zig build -Doptimize=ReleaseSafe
```

### Run

```bash
./zig-out/bin/myapp --help
```

### Test

```bash
zig build test
```

## Usage

```bash
# Show help
myapp --help

# Show version
myapp --version

# Run with verbose output
myapp --verbose

# Use configuration file
myapp --config myapp.conf
```

## Configuration

myapp supports configuration through multiple sources (in order of precedence):

1. Command-line arguments
2. Environment variables (prefixed with `MYAPP_`)
3. Configuration file (`~/.config/myapp/config.json` or via `--config`)

### Configuration File Example

```json
{
  "log_level": "info",
  "color_output": true,
  "timeout": 30
}
```

## Development

### Project Structure

```
myapp/
├── src/
│   ├── main.c           # Entry point
│   ├── cli/             # CLI argument parsing
│   ├── core/            # Core functionality
│   ├── io/              # Input/output handling
│   ├── tui/             # Terminal UI components
│   └── utils/           # Utility functions
├── test/                # Test files
├── docs/                # Documentation
└── build.zig            # Build configuration
```

### Building for Different Targets

```bash
# Debug build
zig build

# Release build
zig build -Doptimize=ReleaseSafe

# Cross-compile for Linux x86_64
zig build -Dtarget=x86_64-linux

# Cross-compile for macOS ARM64
zig build -Dtarget=aarch64-macos
```

### Running Tests

```bash
# Run all tests
zig build test

# Run tests with verbose output
zig build test --verbose
```

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Built with [Zig](https://ziglang.org/)
- Uses [Aro](https://github.com/Vexu/arocc) C compiler
- Follows [OpenCLI](https://opencli.dev/) standards