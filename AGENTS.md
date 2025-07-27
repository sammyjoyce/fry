# Agent Instructions for fry

## Build & Test Commands
- **Build**: `zig build` (debug) or `zig build -Doptimize=ReleaseSafe` (release)
- **Run tests**: `zig build test` or `just test`
- **Run single test**: Tests are in `test/main.zig` - modify and run `zig build test`
- **Format check**: `zig build fmt-check` or `clang-format --dry-run src/**/*.{c,h}`
- **Lint**: Use pre-commit hooks: `pre-commit run --all-files`

## Code Style Guidelines
- **C Standard**: C23 with `-D_GNU_SOURCE` for POSIX extensions
- **Formatting**: Google style via .clang-format (2-space indent, 80 col limit)
- **Headers**: Use `#pragma once`, group includes: system → project → local
- **Error Handling**: Return `app_error` enum values, never raw integers
- **Memory**: Use `app_malloc/app_free` wrappers from `utils/memory.h`
- **Logging**: Use `LOG_*` macros from `utils/logging.h` (DEBUG/INFO/WARN/ERROR)
- **Naming**: snake_case for functions/variables, UPPER_CASE for macros/enums
- **Types**: Suffix custom types with `_t` (e.g., `app_config_t`)
- **Functions**: Prefix with module name (e.g., `app_config_create`)
- **Attributes**: Use `APP_NODISCARD` for functions that return errors
- **TUI**: Conditionally compiled with `-DENABLE_TUI=1` flag