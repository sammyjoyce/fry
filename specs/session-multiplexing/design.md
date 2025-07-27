# Session Multiplexing Technical Design

## 1. Overview

### Problem Statement
Developers working with Claude Code need to manage multiple accounts and maintain separate session contexts for different projects, clients, or use cases. Currently, switching between accounts requires manual re-authentication and loses session state.

### Solution Summary
Fry provides a CLI tool that manages multiple Claude Code OAuth accounts with persistent session storage, allowing seamless switching between different Claude Code instances while maintaining session state and configuration.

### Key Benefits
- **Multi-account support**: Manage multiple Claude Code accounts from a single CLI
- **Context preservation**: Maintain separate Claude Code sessions per account
- **Fast switching**: Change active accounts in milliseconds
- **Secure storage**: OAuth tokens stored in system keychain or encrypted files
- **Persistent sessions**: Continue Claude Code sessions across terminal restarts

### Assumptions and Constraints
- Users have Claude Code CLI installed at `/Users/sam/.local/bin/claude`
- Users have access to Anthropic's OAuth API for Claude Code authentication
- Target platforms: macOS, Linux, Windows (with platform-specific keychain support)
- C23 standard with POSIX extensions
- Zig build system (0.13.0+)
- Terminal-based interface with optional TUI support

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
                                 │
                                 ▼
                        ┌──────────────────┐
                        │  HTTP Client     │
                        │   (http.c)       │
                        └──────────────────┘
```

### Component Breakdown

#### CLI Layer (`src/cli/`)
- **commands.c**: Command routing and execution
- **cmd_auth.c**: Authentication commands (login, logout)
- **cmd_accounts.c**: Account management (add, list, remove, use)
- **cmd_session.c**: Session commands (new, list, resume)
- **cmd_token.c**: Token management (inspect, refresh)

#### Core Layer (`src/core/`)
- **session.c**: Claude Code session state management and process handling
- **oauth.c**: OAuth 2.0 flow implementation with PKCE
- **config.c**: Configuration and account storage
- **keychain.c**: Secure token storage abstraction

#### Storage Layer
- **File-based**: JSON configuration files in `~/.config/fry/`
- **Keychain**: Platform-specific secure storage for tokens
- **Session cache**: Claude Code session state in `~/.cache/fry/sessions/`

### Data Flow
1. User executes command → CLI layer parses arguments
2. CLI layer calls appropriate core function
3. Core layer manages state and launches Claude Code with appropriate auth
4. Storage layer persists changes
5. Response flows back through layers to user

### System Boundaries and Interfaces

#### External Interfaces
- **Anthropic OAuth API**: OAuth 2.0 endpoints for Claude Code authentication
- **Claude Code CLI**: Process management for `/Users/sam/.local/bin/claude`
- **System Keychain**: macOS Keychain, Linux Secret Service, Windows Credential Store
- **File System**: Configuration and cache storage

#### Internal Interfaces
- **CLI ↔ Core**: Function calls with `app_error` return codes
- **Core ↔ Storage**: JSON serialization/deserialization
- **Core ↔ HTTP**: Request/response structures
- **Core ↔ Keychain**: Token storage/retrieval APIs

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

### Data Models and Schemas

#### Account Schema (`~/.config/fry/accounts.json`)
```json
{
  "version": "1.0",
  "accounts": [
    {
      "id": "acc_1234567890",
      "name": "Personal",
      "email": "user@example.com",
      "auth_mode": "max",  // "max" for Claude Pro/Max, "console" for API key
      "created_at": "2024-01-15T10:30:00Z",
      "last_used": "2024-01-20T15:45:00Z",
      "is_active": true,
      "token_type": "oauth",  // "oauth" or "api"
      "metadata": {
        "color": "blue",
        "description": "Personal projects"
      }
    }
  ],
  "default_account": "acc_1234567890"
}
```

#### Session Schema (`~/.cache/fry/sessions/{session_id}.json`)
```json
{
  "session_id": "sess_abc123",
  "account_id": "acc_1234567890",
  "process_info": {
    "pid": 12345,
    "start_time": "2024-01-20T10:00:00Z",
    "claude_version": "1.0.0",
    "args": ["--model", "claude-3-opus-20240229"]
  },
  "created_at": "2024-01-20T10:00:00Z",
  "last_active": "2024-01-20T15:45:00Z",
  "working_directory": "/Users/sam/projects/myapp",
  "environment": {
    "ANTHROPIC_API_KEY": "***",
    "CLAUDE_SESSION_ID": "sess_abc123"
  },
  "metadata": {
    "title": "MyApp Development",
    "tags": ["development", "typescript", "react"]
  }
}
```

### Algorithm Descriptions

#### Token Refresh Algorithm
```
1. Check token expiration (expires_at - current_time)
2. If expires in < 5 minutes:
   a. Acquire refresh lock (prevent concurrent refreshes)
   b. Call OAuth refresh endpoint with refresh_token
   c. Update keychain with new tokens
   d. Update in-memory token cache
   e. Release refresh lock
3. If refresh fails:
   a. Mark account as needs_reauth
   b. Return AUTH_REQUIRED error
   c. Prompt user to re-authenticate
```

#### Session Resume Algorithm
```
1. Load session file from cache
2. Validate session integrity:
   a. Check account exists
   b. Verify token validity
   c. Check if Claude process is still running
3. Restore session context:
   a. Set environment variables for auth
   b. Change to saved working directory
   c. Launch Claude Code with saved arguments
4. Update last_active timestamp
5. Return success or appropriate error
```

### State Machines

#### OAuth Flow State Machine
```
States: INIT → AUTH_PENDING → CODE_RECEIVED → TOKEN_OBTAINED → COMPLETE

Transitions:
- INIT → AUTH_PENDING: User initiates login
- AUTH_PENDING → CODE_RECEIVED: Callback received with code
- CODE_RECEIVED → TOKEN_OBTAINED: Token exchange successful
- TOKEN_OBTAINED → COMPLETE: Tokens stored successfully
- Any State → INIT: Error or timeout occurs
```

#### Session State Machine
```
States: INACTIVE → ACTIVE → SUSPENDED → TERMINATED

Transitions:
- INACTIVE → ACTIVE: Session created or resumed
- ACTIVE → SUSPENDED: User switches accounts
- SUSPENDED → ACTIVE: User resumes session
- ACTIVE → TERMINATED: User ends session
- SUSPENDED → TERMINATED: Session expires or deleted
```

## 4. Technology Stack

### Programming Languages
- **C23**: Core implementation with GNU extensions
- **Zig**: Build system and test runner
- **Shell**: Helper scripts for demos and setup

### Frameworks and Libraries
- **cJSON**: JSON parsing and generation (embedded)
- **libcurl**: HTTP client implementation
- **ncurses**: Terminal UI (optional, compile-time flag)

### Databases and Storage
- **File System**: JSON files for configuration and cache
- **System Keychain**: 
  - macOS: Security Framework
  - Linux: libsecret (Secret Service API)
  - Windows: Windows Credential Manager

### Third-party Services
- **Anthropic OAuth**: Authentication provider for Claude Code
  - Authorization endpoints: `claude.ai/oauth/authorize` (Pro/Max) or `console.anthropic.com/oauth/authorize` (API key)
  - Token endpoint: `console.anthropic.com/v1/oauth/token`
  - Client ID: `9d1c250a-e61b-44d9-88ed-5944d1962f5e`
  - Scopes: `org:create_api_key user:profile user:inference`
- **Claude Code CLI**: The official Anthropic CLI tool at `/Users/sam/.local/bin/claude`

### Development Tools
- **clang-format**: Code formatting (Google style)
- **clang-tidy**: Static analysis
- **pre-commit**: Git hooks for quality checks
- **GitHub Actions**: CI/CD pipeline

## 5. Security Considerations

### Authentication and Authorization
- OAuth 2.0 with PKCE for secure authentication
- No client secrets stored (public client flow)
- Tokens never logged or displayed in plain text
- Automatic token refresh before expiration

### Data Encryption
- Tokens stored in system keychain when available (encrypted by OS)
- Fallback to JSON file storage in user's home directory with OS file permissions
- No additional encryption layer for fallback storage (similar to opencode's approach)
- TLS 1.2+ for all API communications
- OAuth callback handled via manual code paste (no local server needed)

### Security Vulnerabilities and Mitigations
1. **Token Leakage**
   - Mitigation: Use system keychain, never log tokens
   - Mitigation: Implement secure token wipe on logout

2. **OAuth Redirect Attacks**
   - Mitigation: Validate state parameter
   - Mitigation: Strict redirect URI validation

3. **Session Hijacking**
   - Mitigation: Session tokens bound to account
   - Mitigation: Implement session timeouts

4. **Command Injection**
   - Mitigation: No shell execution, direct syscalls only
   - Mitigation: Input validation on all user data

### Compliance Requirements
- GDPR: User data deletion on account removal
- SOC2: Audit logging for security events
- OAuth 2.0 Security Best Practices (RFC 8252)

## 6. Performance Considerations

### Expected Load and Scalability
- Single-user CLI tool (no concurrent user concerns)
- Expected session count: 10-100 per user
- Message history: 1000-10000 messages per session
- Account count: 1-10 per installation

### Performance Metrics and SLAs
- Startup time: < 500ms
- Account switch: < 100ms
- Token refresh: < 2s (network dependent)
- Claude Code launch: < 1s (process spawn)
- Session list/search: < 200ms for 100 sessions

### Optimization Strategies
1. **Lazy Loading**: Load session history on-demand
2. **Caching**: In-memory cache for active session
3. **Indexing**: Session metadata index for fast search
4. **Compression**: Gzip session history files
5. **Pagination**: Limit message history in memory

### Caching Strategies
- **Token Cache**: In-memory with 5-minute TTL
- **Session Metadata**: Indexed cache file updated on changes
- **Account List**: Cached for entire process lifetime
- **HTTP Responses**: No caching (real-time requirement)

## 7. Testing Strategy

### Unit Testing Approach
- Test framework: Zig's built-in testing
- Mock HTTP client for API tests
- Mock keychain for storage tests
- Test coverage target: 80%

Example test structure:
```zig
test "oauth_refresh_token_success" {
    var mock_client = MockHttpClient.init();
    defer mock_client.deinit();
    
    var oauth = try OAuthClient.init(&mock_client);
    defer oauth.deinit();
    
    const token = try oauth.refreshToken("old_refresh_token");
    try testing.expect(token.access_token != null);
}
```

### Integration Testing
- Real OAuth flow with test account
- Keychain integration per platform
- End-to-end CLI command tests
- Session persistence across restarts

### Performance Testing
- Benchmark account switching time
- Measure startup time with N accounts
- Load test with large session histories
- Memory usage profiling

### Security Testing
- Token storage encryption verification
- OAuth state parameter validation
- Input fuzzing for command injection
- Keychain access permission tests

## 8. Deployment Strategy

### Environment Setup
```bash
# Development
zig build -Doptimize=Debug -Denable-tui=true

# Release
zig build -Doptimize=ReleaseSafe

# Platform-specific
zig build -Dtarget=x86_64-macos
zig build -Dtarget=x86_64-linux-gnu
zig build -Dtarget=x86_64-windows-gnu
```

### CI/CD Pipeline
1. **Build Stage**
   - Multi-platform compilation
   - Static analysis (clang-tidy)
   - Format checking

2. **Test Stage**
   - Unit tests
   - Integration tests
   - Security scanning

3. **Package Stage**
   - Platform-specific packages
   - Code signing (macOS/Windows)
   - Checksum generation

4. **Release Stage**
   - GitHub releases
   - Homebrew formula update
   - Package manager submissions

### Rollback Procedures
1. **Version Pinning**: Allow users to pin specific versions
2. **Config Migration**: Backward-compatible config changes
3. **Data Backup**: Automatic backup before upgrades
4. **Rollback Command**: `fry --rollback-version`

### Monitoring and Alerting
- Error reporting opt-in (anonymized)
- Local error logs in `~/.local/share/fry/logs/`
- Performance metrics collection (opt-in)
- Crash reports with stack traces

## 9. Risks and Mitigations

### Technical Risks

1. **API Changes**
   - Risk: Anthropic changes OAuth/API endpoints
   - Mitigation: Version detection and compatibility layer
   - Mitigation: Graceful degradation with clear errors

2. **Platform Keychain Issues**
   - Risk: Keychain APIs unavailable or fail
   - Mitigation: Fallback to encrypted file storage
   - Mitigation: Clear error messages with manual fixes

3. **Token Expiration Edge Cases**
   - Risk: Race conditions during refresh
   - Mitigation: Mutex locks on token operations
   - Mitigation: Exponential backoff on failures

### Business Risks

1. **Adoption Barriers**
   - Risk: Complex setup deters users
   - Mitigation: Streamlined onboarding flow
   - Mitigation: Comprehensive documentation

2. **Security Concerns**
   - Risk: Users distrust token storage
   - Mitigation: Transparent security documentation
   - Mitigation: Security audit and certification

3. **Support Burden**
   - Risk: High volume of support requests
   - Mitigation: Detailed error messages
   - Mitigation: Self-diagnostic commands

### Mitigation Strategies

1. **Defensive Programming**
   - Validate all inputs
   - Handle all error cases explicitly
   - Fail fast with clear messages

2. **Progressive Enhancement**
   - Core features work without TUI
   - Graceful degradation on older systems
   - Optional features behind flags

3. **Community Engagement**
   - Open source for transparency
   - Active issue tracking
   - Regular security updates