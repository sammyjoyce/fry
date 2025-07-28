# Session Multiplexer Technical Design

## 1. Overview

### Problem Statement
Developers working with Claude Code need to manage multiple accounts and maintain separate session contexts for different projects, clients, or use cases. Currently, switching between accounts requires manual re-authentication and loses session state. Additionally, working with multiple Claude Code instances simultaneously requires multiple terminal windows.

### Solution Summary
Fry provides a CLI tool that manages multiple Claude Code OAuth accounts with persistent session storage and a Terminal User Interface (TUI) for managing multiple Claude Code sessions simultaneously in a single terminal window. The solution allows seamless switching between different Claude Code instances while maintaining session state and configuration, all within a tiled NCurses-based interface.

### Key Benefits
- **Multi-account support**: Manage multiple Claude Code accounts from a single CLI
- **Context preservation**: Maintain separate Claude Code sessions per account
- **Fast switching**: Change active accounts in milliseconds
- **Secure storage**: OAuth tokens stored in system keychain or encrypted files
- **Persistent sessions**: Continue Claude Code sessions across terminal restarts
- **Tiled interface**: Manage multiple Claude Code sessions in a single terminal
- **Keyboard-driven**: Efficient navigation and control without mouse

### Assumptions and Constraints
- Users have Claude Code CLI installed at `/Users/sam/.local/bin/claude`
- Users have access to Anthropic's OAuth API for Claude Code authentication
- Target platforms: macOS, Linux (Windows is not currently supported)
- C23 standard with POSIX extensions
- Zig build system (0.13.0+)
- Terminal-based interface with NCurses TUI support

## 2. Architecture Design

### High-level Architecture
```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│   CLI Layer     │────▶│   Core Layer     │────▶│  Storage Layer  │
│  (commands.c)   │     │  (session.c)     │     │  (config.c)     │
└─────────────────┘     └──────────────────┘     └─────────────────┘
         │                       │                         │
         ▼                       ▼                         ▼
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│   TUI Layer     │     │   OAuth Layer    │     │ Keychain Layer  │
│   (tui.c)       │     │   (oauth.c)      │     │ (platform-spec) │
└─────────────────┘     └──────────────────┘     └─────────────────┘
         │                       │
         ▼                       ▼
┌─────────────────┐     ┌──────────────────┐
│  PTY Layer      │     │  HTTP Client     │
│  (pty.c)        │     │   (http.c)       │
└─────────────────┘     └──────────────────┘
```

### Component Breakdown

#### CLI Layer (`src/cli/`)
- **commands.c**: Command routing and execution
- **cmd_auth.c**: Authentication commands (login, logout)
- **cmd_accounts.c**: Account management (add, list, remove, rename, use)
- **cmd_session.c**: Session commands (new, list, resume, close)
- **cmd_token.c**: Token management (inspect, refresh)
- **cmd_tui.c**: TUI launch and control commands

#### Core Layer (`src/core/`)
- **session.c**: Session state management and Claude Code process lifecycle
- **oauth.c**: OAuth 2.0 flow implementation with PKCE
- **config.c**: Configuration and account storage
- **keychain.c**: Secure token storage abstraction

#### TUI Layer (`src/tui/`)
- **tui.c**: Main TUI loop and coordination
- **tui_pane.c**: Pane management and rendering
- **tui_layout.c**: Layout calculation and arrangement
- **tui_input.c**: Input handling and command processing
- **tui_render.c**: Screen rendering and optimization

#### PTY Layer (`src/pty/`)
- **pty.c**: Pseudo-terminal creation and management
- **pty_io.c**: I/O multiplexing between PTYs and TUI panes

#### Storage Layer
- **File-based**: JSON configuration files in `~/.config/fry/`
- **Keychain**: Platform-specific secure storage for tokens
- **Session cache**: Claude Code session state in `~/.cache/fry/sessions/`
- **Workspace cache**: TUI layout state in `~/.cache/fry/workspaces/`

### Data Flow
1. User executes `fry tui`. The **CLI Layer** parses the command.
2. The **CLI Layer** calls the **Core Layer** to initialize the application state, loading accounts and session history from the **Storage Layer**.
3. The **TUI Layer** is initialized, creating a layout and panes based on the user's workspace configuration.
4. For each pane, the **TUI Layer** requests the **Core Layer** to launch a Claude Code session.
5. The **Core Layer** retrieves the appropriate OAuth token from the **Keychain Layer** and spawns a Claude Code process via the **PTY Layer**.
6. The **TUI Layer** multiplexes I/O between the user input and the various PTYs, rendering output to the screen.
7. As the user interacts with the application, changes to accounts, sessions, and workspaces are persisted by the **Storage Layer**.

### System Boundaries and Interfaces

#### External Interfaces
- **Anthropic OAuth API**: OAuth 2.0 endpoints for Claude Code authentication
- **Claude Code CLI**: Process management for `/Users/sam/.local/bin/claude`
- **System Keychain**: macOS Keychain, Linux Secret Service
- **File System**: Configuration and cache storage
- **Terminal**: User interface via NCurses

#### Internal Interfaces
- **CLI ↔ Core**: Function calls with `app_error` return codes
- **Core ↔ Storage**: JSON serialization/deserialization
- **Core ↔ HTTP**: Request/response structures
- **Core ↔ Keychain**: Token storage/retrieval APIs
- **Core ↔ TUI**: Session state and account information
- **TUI ↔ PTY**: Pane I/O and process management

## 3. Detailed Design

### Component Specifications

#### Session Manager (`src/core/session.h`)
```c
typedef struct {
    char* session_id;
    char* account_id;
    pid_t claude_pid;  // Process ID of Claude Code instance
    time_t created_at;
    time_t last_active;
    json_t* metadata;
} app_session_t;

typedef struct {
    app_session_t* active_session;
    app_account_t* active_account;
    char* claude_binary_path;  // Path to claude executable
    char** env_vars;  // Environment variables for Claude Code
} app_session_manager_t;

// Core session operations
app_error app_session_create(app_session_manager_t* mgr, const char* account_id, app_session_t** session);
app_error app_session_resume(app_session_manager_t* mgr, const char* session_id);
app_error app_session_list(app_session_manager_t* mgr, app_session_t*** sessions, size_t* count);
app_error app_session_launch_claude(app_session_manager_t* mgr, const char* args[], pid_t* pid);
app_error app_session_terminate(app_session_manager_t* mgr, const char* session_id);
```

#### OAuth Manager (`src/core/oauth.h`)
```c
typedef struct {
    char* access_token;
    char* refresh_token;
    time_t expires_at;
    char* scope;
} app_oauth_token_t;

typedef struct {
    char* client_id;  // Fixed: 9d1c250a-e61b-44d9-88ed-5944d1962f5e
    char* auth_endpoint;  // claude.ai/oauth/authorize or console.anthropic.com/oauth/authorize
    char* token_endpoint;  // console.anthropic.com/v1/oauth/token
    char* redirect_uri;  // console.anthropic.com/oauth/code/callback
    app_http_client_t* http_client;
    char* code_verifier;  // PKCE verifier
    char* code_challenge;  // PKCE challenge
} app_oauth_client_t;

// OAuth flow operations with PKCE support
app_error app_oauth_generate_pkce(char** verifier, char** challenge);
app_error app_oauth_start_flow(app_oauth_client_t* client, const char* mode, char** auth_url, char** state);
app_error app_oauth_complete_flow(app_oauth_client_t* client, const char* code, const char* state, app_oauth_token_t** token);
app_error app_oauth_refresh_token(app_oauth_client_t* client, const char* refresh_token, app_oauth_token_t** new_token);
app_error app_oauth_create_api_key(app_oauth_client_t* client, const char* access_token, char** api_key);
```

#### Keychain Interface (`src/core/keychain.h`)
```c
typedef struct {
    void* platform_handle;
    bool use_fallback;  // True if using file storage instead of keychain
} app_keychain_t;

// Token storage structure (matches opencode's approach)
typedef struct {
    char* type;  // "oauth" or "api"
    char* refresh;  // Refresh token (OAuth only)
    char* access;  // Access token or API key
    time_t expires;  // Expiration timestamp (OAuth only)
} app_stored_token_t;

// Platform-agnostic keychain operations
app_error app_keychain_init(app_keychain_t** keychain);
app_error app_keychain_store_token(app_keychain_t* keychain, const char* account_id, const app_stored_token_t* token);
app_error app_keychain_retrieve_token(app_keychain_t* keychain, const char* account_id, app_stored_token_t** token);
app_error app_keychain_delete_token(app_keychain_t* keychain, const char* account_id);
app_error app_keychain_cleanup(app_keychain_t* keychain);
```

#### TUI Main Loop (`src/tui/tui.h`)
```c
typedef struct {
    char* workspace_id;
    app_layout_t* layout;
    app_pane_t** panes;
    size_t pane_count;
    command_history_t* cmd_history;
    keybinding_map_t* keybindings;
    bool mouse_enabled;
    color_scheme_t* colors;
} app_mux_state_t;

// TUI operations
app_error app_tui_init(app_mux_state_t** state);
app_error app_tui_run(app_mux_state_t* state);
app_error app_tui_shutdown(app_mux_state_t* state);
app_error app_tui_handle_input(app_mux_state_t* state, int ch);
app_error app_tui_render(app_mux_state_t* state);
```

#### Pane Structure (`src/tui/tui_pane.h`)
```c
typedef struct {
    pane_id_t id;
    WINDOW* window;  // NCurses window
    WINDOW* border_window;  // Border decoration
    session_id_t session_id;
    account_id_t account_id;
    pane_state_t state;  // ACTIVE, BUSY, ERROR, INACTIVE
    rect_t geometry;  // Position and size
    char* title;
    time_t last_activity;
    bool has_focus;
} app_pane_t;

// Pane operations
app_error app_pane_create(app_mux_state_t* mux, account_id_t account, app_pane_t** pane);
app_error app_pane_focus(app_mux_state_t* mux, pane_id_t id);
app_error app_pane_close(app_mux_state_t* mux, pane_id_t id);
app_error app_pane_render(app_pane_t* pane);
app_error app_pane_update_content(app_pane_t* pane, const char* content, size_t len);
```

#### Layout Configuration (`src/tui/tui_layout.h`)
```c
typedef struct {
    layout_mode_t mode;  // GRID, VSPLIT, HSPLIT, FOCUS
    int rows;
    int cols;
    pane_id_t* pane_order;  // Array of pane IDs
    size_t pane_count;
    pane_id_t focused_pane;
} app_layout_t;

// Layout operations
app_error app_layout_arrange(app_mux_state_t* mux, layout_mode_t mode);
app_error app_layout_split(app_mux_state_t* mux, direction_t dir);
app_error app_layout_resize_pane(app_mux_state_t* mux, pane_id_t id, int delta);
app_error app_layout_calculate_dimensions(app_mux_state_t* mux, rect_t* dimensions);
```

#### PTY Manager (`src/pty/pty.h`)
```c
typedef struct {
    int master_fd;  // Master side of PTY
    int slave_fd;   // Slave side of PTY
    pid_t child_pid;  // PID of child process
    char* pts_name;   // Name of PTY device
} app_pty_t;

// PTY operations
app_error app_pty_create(app_pty_t** pty);
app_error app_pty_spawn_process(app_pty_t* pty, const char* program, char* const argv[], char* const envp[]);
app_error app_pty_read(app_pty_t* pty, char* buffer, size_t size, ssize_t* bytes_read);
app_error app_pty_write(app_pty_t* pty, const char* data, size_t len);
app_error app_pty_close(app_pty_t* pty);
```

### API Contracts

#### Claude Code Process Management
```c
// Claude Code launch configuration
typedef struct {
    char* account_id;
    char* session_id;
    char** extra_args;  // Additional CLI arguments
    char** env_overrides;  // Environment variable overrides
} app_claude_launch_config_t;

// Process monitoring structure
typedef struct {
    pid_t pid;
    int exit_status;
    bool is_running;
    time_t start_time;
} app_claude_process_info_t;

// Claude Code process management functions
app_error app_claude_launch(app_session_manager_t* mgr, const app_claude_launch_config_t* config, app_claude_process_info_t** info);
app_error app_claude_check_status(app_session_manager_t* mgr, pid_t pid, app_claude_process_info_t** info);
app_error app_claude_terminate(app_session_manager_t* mgr, pid_t pid);
```

#### TUI Input Handling
```c
typedef enum {
    INPUT_MODE_NORMAL,    // Pane navigation and basic commands
    INPUT_MODE_COMMAND,   // Command mode (:)
    INPUT_MODE_INSERT,    // Direct input to active pane
} input_mode_t;

// Input handler structure
typedef struct {
    input_mode_t mode;
    char* command_buffer;  // For command mode
    size_t buffer_len;
    keybinding_map_t* bindings;
} app_input_handler_t;

// Input processing functions
app_error app_input_process_key(app_mux_state_t* mux, int ch);
app_error app_input_process_mouse(app_mux_state_t* mux, MEVENT* event);
app_error app_input_set_mode(app_mux_state_t* mux, input_mode_t mode);
app_error app_input_execute_command(app_mux_state_t* mux, const char* command);
```

### Data Models and Schemas

#### Account Schema (`~/.config/fry/accounts.json`)
```json
{
  "version": "1.0",
  "default_account": "acc_personal",
  "accounts": [
    {
      "id": "acc_personal",
      "name": "Personal",
      "created_at": "2024-01-20T10:00:00Z",
      "last_used": "2024-01-20T15:45:00Z",
      "auth_mode": "oauth",
      "claude_binary": "/Users/sam/.local/bin/claude"
    },
    {
      "id": "acc_work",
      "name": "Work",
      "created_at": "2024-01-21T09:30:00Z",
      "last_used": "2024-01-21T14:20:00Z",
      "auth_mode": "api_key",
      "claude_binary": "/Users/sam/.local/bin/claude"
    }
  ]
}
```

#### Session Schema (`~/.cache/fry/sessions/{session_id}.json`)
```json
{
  "session_id": "sess_abc123",
  "account_id": "acc_1234567890",
  "process_info": {
    "pid": 12345,
    "started_at": "2024-01-20T15:45:00Z",
    "last_activity": "2024-01-20T15:50:00Z"
  },
  "context": {
    "working_directory": "/Users/sam/projects/fry",
    "environment": {
      "CLAUDE_PROJECT": "fry-cli"
    }
  },
  "history": {
    "message_count": 42,
    "last_message": "How do I implement OAuth PKCE?"
  }
}
```

#### Workspace Schema (`~/.cache/fry/workspaces/{workspace_id}.json`)
```json
{
  "workspace_id": "work_abc123",
  "name": "Development",
  "created_at": "2024-01-20T10:00:00Z",
  "last_active": "2024-01-20T15:45:00Z",
  "layout": {
    "mode": "grid",
    "dimensions": {"rows": 2, "cols": 2},
    "panes": [
      {
        "id": "pane_001",
        "position": {"row": 0, "col": 0},
        "account_id": "acc_personal",
        "session_id": "sess_123",
        "geometry": {"x": 0, "y": 0, "width": 80, "height": 25}
      },
      {
        "id": "pane_002",
        "position": {"row": 0, "col": 1},
        "account_id": "acc_work",
        "session_id": "sess_456",
        "geometry": {"x": 80, "y": 0, "width": 80, "height": 25}
      }
    ]
  }
}
```

### Performance Considerations

#### Resource Usage Targets
- Memory footprint: < 50MB for typical usage
- CPU usage: < 5% when idle
- Startup time: < 500ms
- Account switching: < 100ms
- TUI refresh rate: 30 FPS minimum

#### Scalability Limits
- Single-user CLI tool (no concurrent user concerns)
- Expected session count: 10-100 per user
- Message history: 1000-10000 messages per session
- Account count: 1-10 per installation
- Simultaneous panes: 9 maximum (3x3 grid)

#### Optimization Strategies
- Lazy loading of session history
- Efficient I/O multiplexing with select/poll
- Double buffering for TUI rendering
- Memory pooling for frequent allocations
- Connection reuse for HTTP requests

### Security Considerations

#### Data Protection
- OAuth tokens stored in system keychain (fallback to encrypted files)
- Never log or display tokens in plain text
- Secure random generation for PKCE verifiers
- Validate OAuth state parameters to prevent CSRF

#### Access Control
- File permissions for configuration files (user-only read/write)
- Input validation for all user-provided data
- Rate limiting for authentication attempts
- Secure handling of environment variables

### Error Handling

#### Recovery Strategies
- Graceful degradation when features unavailable
- Automatic retry with exponential backoff for network issues
- Session preservation across crashes
- Clear error messages with recovery suggestions

#### Failure Modes
- Network connectivity issues
- Claude Code process crashes
- Terminal capability limitations
- Storage access failures
- Authentication token expiration

### Testing Considerations

#### Test Categories
- Unit tests for core components
- Integration tests for session management
- UI tests for TUI functionality
- Cross-platform compatibility tests
- Performance and stress tests

#### Mock Implementations
- NCurses mock for TUI testing
- HTTP mock for OAuth testing
- PTY mock for process testing
- Keychain mock for token storage testing
