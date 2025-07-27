# Agent Instructions for fry

## Important Development Guidelines

- **Before making changes**: Search codebase (don't assume not implemented)
- **After implementing functionality or resolving problems**: Run the tests for that unit of code that was improved. If functionality is missing then it's your job to add it as per the application specifications. Think hard.
- **Single sources of truth**: We want single sources of truth, no migrations/adapters. If tests unrelated to your work fail then it's your job to resolve these tests as part of the increment of change.
- **Learning documentation**: When you learn something new about how to run the compiler or examples make sure you update AGENTS.md using a subagent but keep it brief. For example if you run commands multiple times before learning the correct command then that file should be updated.
- **Bug resolution**: IMPORTANT when you discover a bug resolve it using subagents even if it is unrelated to the current piece of work
- **Keep AGENTS.md up to date**: Keep AGENTS.md up to date with information on how to build the compiler and your learnings to optimise the build/test loop using a subagent.
- **Full implementations only**: DO NOT IMPLEMENT PLACEHOLDER OR SIMPLE IMPLEMENTATIONS. WE WANT FULL IMPLEMENTATIONS. DO IT OR I WILL YELL AT YOU
- **No status reports**: SUPER IMPORTANT DO NOT IGNORE. DO NOT PLACE STATUS REPORT UPDATES INTO AGENTS.md

## Style

We follow TigerStyle principles adapted for C code. Key rules:

### Safety First

- **No recursion**: Use iterative approaches with explicit bounds
- **Limit everything**: All loops must have fixed upper bounds
- **Assert extensively**: Average 2+ assertions per function minimum - use static_assert
- **Pair assertions**: Assert properties in at least two code paths
- **Assert positive AND negative space**: Check what you expect AND what you don't
- **Static memory allocation**: Allocate all memory at startup, no dynamic allocation after init
- **Fail fast**: Detect violations immediately rather than later
- **Handle all errors**: Every error must be explicitly handled

### Code Organization

- **70 lines max per function**: Hard limit, no exceptions
- **Smallest possible scope**: Minimize variables in scope
- **Control flow clarity**: Keep all switch/if in parent functions, pure logic in helpers
- **Explicit over implicit**: Pass all options explicitly, don't rely on defaults
- **Always say why**: Comments explain rationale, not just what

### Performance

- **Design-phase optimization**: Best performance wins come from architecture
- **Back-of-envelope sketches**: Always estimate resource usage (network/disk/memory/CPU)
- **Batch operations**: Amortize costs across multiple operations
- **Hot loop extraction**: Extract performance-critical loops into standalone functions

### Naming & Style

- **Perfect names matter**: Take time to find the right nouns and verbs
- **No abbreviations**: Use full descriptive names (except loop counters)
- **Units last**: `latency_ms_max` not `max_latency_ms`
- **Same-length related names**: `source`/`target` not `src`/`dest`
- **Big-endian naming**: Most significant word first
- **Order matters**: Fields → Types → Methods in structs

### Development Practices

- **Zero technical debt policy**: Do it right the first time
- **Zero dependencies**: No external dependencies beyond build tools
- **Compound assertions**: Split `assert(a && b)` into `assert(a); assert(b)`
- **In-place construction**: Initialize large structs in-place with out pointers
- **Calculate near use**: Don't introduce variables before needed

## Build & Test Commands

- **Build**: `zig build` (debug) or `zig build -Doptimize=ReleaseSafe` (release)
- **Run tests**: `zig build test` or `just test`
- **Run single test**: Tests are in `test/main.zig` - modify and run `zig build test`
- **Format check**: `zig build fmt-check` or `clang-format --dry-run src/**/*.{c,h}`
- **Lint**: Use pre-commit hooks: `pre-commit run --all-files`
- **Sign binary (macOS)**: `zig build sign` for ad-hoc signing or `zig build -Dsign-identity="Developer ID" sign-cert` for certificate signing
    - Required on macOS 15+ for keychain access, otherwise falls back to in-memory storage

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

## Modern C Patterns & Best Practices

### Error Handling Pattern

Use the `goto cleanup` idiom for centralized resource cleanup:

```c
int process_file(const char *filename) {
    FILE *file = NULL;
    char *buffer = NULL;
    int status = -1;

    file = fopen(filename, "r");
    if (!file) goto cleanup;

    buffer = app_malloc(1024);
    if (!buffer) goto cleanup;

    // Process file...
    status = 0;

cleanup:
    if (buffer) app_free(buffer);
    if (file) fclose(file);
    return status;
}
```

### Object-Oriented Patterns

Simulate OOP using structs with function pointers:

```c
typedef struct {
    // Data members
    void *data;
    size_t size;

    // Methods
    void (*destroy)(void *self);
    int (*process)(void *self, const char *input);
} app_handler_t;
```

### Memory Management

- **Arena Allocation**: For performance-critical paths, use arena allocators
- **Ownership**: Be explicit about memory ownership in function documentation
- **Validation**: Always check malloc returns and validate sizes before allocation

### Concurrency (C11)

- Use `_Atomic` types for lock-free counters
- Protect shared data with `mtx_t` mutexes
- Use condition variables (`cnd_t`) for thread coordination
- Follow the pattern: lock → while(condition) wait → work → unlock

### Security Practices

- **Input Validation**: Sanitize all external input, especially sizes and indices
- **Bounds Checking**: Use sized functions (snprintf, strncat, etc.)
- **Integer Overflow**: Use C23's `<stdckdint.h>` for safe arithmetic when available
- **Compiler Hardening**: Default flags include `-fstack-protector-strong -D_FORTIFY_SOURCE=2`

### Type Safety

- Use `static_assert` for compile-time invariants
- Prefer typed enums over raw integers for state/status
- Use opaque pointers to hide implementation details
- Apply `const` correctly for immutable data

### Modern C Features (C11/C23)

- `_Generic` for type-generic macros
- `static_assert` for compile-time checks
- `alignas` for explicit alignment requirements
- Anonymous structs/unions for cleaner APIs
- `typeof` for type inference in macros (C23)

