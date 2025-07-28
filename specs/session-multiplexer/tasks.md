# Session Multiplexer Implementation Tasks

## Overview
This document tracks the implementation status of the Session Multiplexer feature. Each task shows its completion status and what has been implemented.

## Implementation Status

### Phase 1: Foundation & OAuth Enhancement

- [ ] 1. Create keychain abstraction interface
  - [ ] 1.1. Define keychain.h interface
  - [ ] 1.2. Add error codes for keychain operations
  - [ ] 1.3. Create mock implementation for testing
  - References: Requirement 17.1, 17.2, 17.3, 17.4, 17.5, 17.6

- [ ] 2. Implement platform-specific keychain backends
  - [ ] 2.1. Implement macOS keychain backend
    - [ ] 2.1.1. Store/retrieve/delete operations working
    - [ ] 2.1.2. Proper error handling for all edge cases
    - [ ] 2.1.3. Memory management verified (no leaks)
    - References: Requirement 17.2
  - [ ] 2.2. Implement Linux keychain backend
    - [ ] 2.2.1. Integration with Secret Service API
    - [ ] 2.2.2. Fallback for systems without libsecret
    - [ ] 2.2.3. Cross-distro testing (Ubuntu, Fedora, Arch)
    - References: Requirement 17.2
  - [ ] 2.3. Windows Support (Not Currently Planned)
    - [ ] 2.3.1. N/A - Windows is not currently supported
    - References: N/A

- [ ] 3. Enhance OAuth implementation with token storage
  - [ ] 3.1. Integrate keychain storage into existing OAuth flow
    - [ ] 3.1.1. Tokens stored in keychain after auth (or fallback JSON)
    - [ ] 3.1.2. Automatic token refresh using console.anthropic.com/v1/oauth/token
    - [ ] 3.1.3. Token expiration handling with 5-minute buffer
    - [ ] 3.1.4. Support for API key creation in console mode
    - References: Requirement 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7
  - [ ] 3.2. Add OAuth PKCE support
    - [ ] 3.2.1. Code verifier generation using secure random (43-128 chars)
    - [ ] 3.2.2. Code challenge calculation using SHA256 and base64url encoding
    - [ ] 3.2.3. Support for both claude.ai and console.anthropic.com endpoints
    - [ ] 3.2.4. Manual code paste flow implemented (no local server)
    - References: Requirement 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7, 3.8, 17.4, 17.5
  - [ ] 3.3. Create OAuth flow integration tests
    - [ ] 3.3.1. Mock OAuth endpoints for claude.ai and console.anthropic.com
    - [ ] 3.3.2. PKCE flow validation tests
    - [ ] 3.3.3. Token refresh scenarios with expired tokens
    - [ ] 3.3.4. API key creation flow for console mode
    - References: Requirement 16.5
  - [ ] 3.4. Implement Anthropic OAuth configuration
    - [ ] 3.4.1. Client ID hardcoded: 9d1c250a-e61b-44d9-88ed-5944d1962f5e
    - [ ] 3.4.2. Redirect URI: console.anthropic.com/oauth/code/callback
    - [ ] 3.4.3. Scopes: org:create_api_key user:profile user:inference
    - [ ] 3.4.4. Mode selection UI for Pro/Max vs API key
    - References: Requirement 3.8

### Phase 2: Session Management Core

- [ ] 4. Create session manager interface
  - [ ] 4.1. Define session.h interface with all required functions
  - [ ] 4.2. Create data structures for session tracking
  - [ ] 4.3. Define error codes for session operations
  - References: Requirement 4.1, 4.2, 4.3, 4.4, 4.5

- [ ] 5. Implement Claude Code process launching
  - [ ] 5.1. Process creation with OAuth tokens in environment
  - [ ] 5.2. Working directory handling
  - [ ] 5.3. Environment variable setup
  - [ ] 5.4. Process monitoring
  - References: Requirement 4.1, 4.2, 4.3, 4.4, 4.5

- [ ] 6. Implement session persistence
  - [ ] 6.1. Session state serialized to JSON
  - [ ] 6.2. Session state restored on startup
  - [ ] 6.3. Multiple concurrent sessions supported
  - References: Requirement 4.1, 4.2, 4.3, 4.4, 4.5

- [ ] 7. Add session management commands
  - [ ] 7.1. Session listing command with details
  - [ ] 7.2. Session resuming by ID
  - [ ] 7.3. Session termination command
  - References: Requirement 4.1, 4.2, 4.3, 4.4, 4.5

### Phase 3: Multi-Account Support

- [ ] 8. Implement account switching mechanism
  - [ ] 8.1. Account switching < 100ms
  - [ ] 8.2. Session state preservation
  - [ ] 8.3. Active account indication
  - References: Requirement 2.1, 2.2, 2.3, 2.4, 2.5

- [ ] 9. Add account management commands
  - [ ] 9.1. Account listing with status
  - [ ] 9.2. Account addition with OAuth flow
  - [ ] 9.3. Account removal with cleanup
  - [ ] 9.4. Improved UX for `accounts add`: clear message & interactive prompt when `--name` (or other required flags) are missing
  - References: Requirement 1.1, 1.2, 1.3, 1.4, 1.5, 1.6

- [ ] 10. Implement token management commands
  - [ ] 10.1. Token inspection command
  - [ ] 10.2. Manual token refresh
  - [ ] 10.3. Token expiration warnings
  - References: Requirement 5.1, 5.2, 5.3, 5.4, 5.5

### Phase 4: NCurses Foundation & Basic Panes

- [ ] 11. Create NCurses abstraction layer
  - [ ] 11.1. Initialize/shutdown NCurses properly
  - [ ] 11.2. Handle terminal capabilities detection
  - [ ] 11.3. Provide mock implementation for tests
  - [ ] 11.4. Support both color and monochrome
  - References: Requirement 13.1, 13.2, 13.3, 13.4, 13.5, 13.6

- [ ] 12. Implement basic pane structure
  - [ ] 12.1. Pane creation and destruction
  - [ ] 12.2. Window association per pane
  - [ ] 12.3. Border rendering with state colors
  - [ ] 12.4. Content area calculation
  - References: Requirement 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 10.1, 10.2, 10.3, 10.4, 10.5, 10.6

- [ ] 13. Create grid layout manager
  - [ ] 13.1. Calculate optimal grid dimensions
  - [ ] 13.2. Position panes in grid
  - [ ] 13.3. Handle various pane counts (1-9)
  - [ ] 13.4. Maintain aspect ratios
  - References: Requirement 6.1, 6.2, 6.3, 6.4, 6.5, 6.6

- [ ] 14. Implement render engine
  - [ ] 14.1. Full screen refresh
  - [ ] 14.2. Partial update optimization
  - [ ] 14.3. Double buffering if needed
  - [ ] 14.4. Flicker-free rendering
  - References: Requirement 13.1, 13.2, 13.3, 13.4, 13.5, 13.6

- [ ] 15. Add basic event loop
  - [ ] 15.1. Non-blocking input handling
  - [ ] 15.2. Terminal resize detection
  - [ ] 15.3. Refresh rate control
  - [ ] 15.4. Clean shutdown handling
  - References: Requirement 13.1, 13.2, 13.3, 13.4, 13.5, 13.6

### Phase 5: PTY Integration & Process Management

- [ ] 16. Implement PTY management
  - [ ] 16.1. PTY creation cross-platform
  - [ ] 16.2. Proper terminal settings
  - [ ] 16.3. Signal forwarding
  - [ ] 16.4. Clean PTY cleanup
  - References: Requirement 8.1

- [ ] 17. Integrate Claude Code launching
  - [ ] 17.1. Process creation with auth
  - [ ] 17.2. Environment variable setup
  - [ ] 17.3. Working directory handling
  - [ ] 17.4. Process monitoring
  - References: Requirement 8.1

- [ ] 18. Implement I/O multiplexing
  - [ ] 18.1. Non-blocking I/O
  - [ ] 18.2. Buffer management
  - [ ] 18.3. Scrollback support
  - [ ] 18.4. ANSI escape sequence handling
  - References: Requirement 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 13.1, 13.2, 13.3, 13.4, 13.5, 13.6

### Phase 6: Input Handling & Navigation

- [ ] 19. Implement keyboard navigation
  - [ ] 19.1. Vim-style hjkl navigation
  - [ ] 19.2. Tab cycling between panes
  - [ ] 19.3. Numbered pane access
  - [ ] 19.4. Focus highlighting
  - References: Requirement 7.1, 7.2, 7.3, 7.4

- [ ] 20. Add command mode
  - [ ] 20.1. ':' activation
  - [ ] 20.2. Command parsing
  - [ ] 20.3. Tab completion
  - [ ] 20.4. Command history
  - References: Requirement 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 11.1, 11.2, 11.3, 11.4, 11.5, 11.6

- [ ] 21. Implement mouse support
  - [ ] 21.1. Pane focusing by click
  - [ ] 21.2. Border dragging for resize
  - [ ] 21.3. Text selection
  - [ ] 21.4. Context menu
  - References: Requirement 12.1, 12.2, 12.3, 12.4, 12.5

### Phase 7: Layout Management, UI Polish & Testing

- [ ] 22. Implement advanced layouts
  - [ ] 22.1. Vertical/horizontal splits
  - [ ] 22.2. Focus mode
  - [ ] 22.3. Layout switching
  - [ ] 22.4. Dynamic resizing
  - References: Requirement 6.1, 6.2, 6.3, 6.4, 6.5, 6.6

- [ ] 23. Add visual enhancements
  - [ ] 23.1. Per-pane status bars
  - [ ] 23.2. Notification system
  - [ ] 23.3. Color-coded states
  - [ ] 23.4. Activity indicators
  - References: Requirement 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 10.1, 10.2, 10.3, 10.4, 10.5, 10.6

- [ ] 24. Build workspace persistence system
  - [ ] 24.1. Workspace serialization
  - [ ] 24.2. Layout configuration save/restore
  - [ ] 24.3. Pane-account associations
  - [ ] 24.4. Multiple workspace support
  - References: Requirement 14.1, 14.2, 14.3, 14.4, 14.5, 14.6

- [ ] 25. Create comprehensive test suite
  - [ ] 25.1. Layout calculation tests
  - [ ] 25.2. Input handling tests
  - [ ] 25.3. Integration test harness
  - [ ] 25.4. Cross-platform test coverage
  - References: Requirement 16.1, 16.5

- [ ] 26. Create integration tests
  - [ ] 26.1. Account switching scenarios
  - [ ] 26.2. Session persistence tests
  - [ ] 26.3. TUI interaction tests
  - [ ] 26.4. Performance benchmarks
  - References: Requirement 16.1, 16.5

- [ ] 27. Documentation and examples
  - [ ] 27.1. Installation guide
  - [ ] 27.2. Usage examples
  - [ ] 27.3. Configuration documentation
  - [ ] 27.4. Troubleshooting guide
  - References: Requirement 15.1

- [ ] 28. Account Management Enhancement
  - [ ] 28.1. Implement account storage with unique identifiers
  - [ ] 28.2. Add OAuth flow initiation for new accounts
  - [ ] 28.3. Implement account renaming functionality
  - [ ] 28.4. Add account export functionality
  - [ ] 28.5. Implement account removal with token cleanup
  - References: Requirement 1.1, 1.2, 1.3, 1.4, 1.5, 1.6

- [ ] 29. Session Switching Enhancement
  - [ ] 29.1. Implement session state preservation during account switching
  - [ ] 29.2. Add active account indication
  - [ ] 29.3. Implement account selection prompt when no account is active
  - [ ] 29.4. Maintain separate session history per account
  - References: Requirement 2.1, 2.2, 2.3, 2.4, 2.5

- [ ] 30. OAuth Authentication Enhancement
  - [ ] 30.1. Implement browser opening for OAuth endpoint
  - [ ] 30.2. Add authorization code prompt
  - [ ] 30.3. Implement both Claude Pro/Max and API Key modes
  - [ ] 30.4. Add re-authentication prompt for token refresh failures
  - [ ] 30.5. Implement token expiration validation
  - References: Requirement 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7

- [ ] 31. Session Management Enhancement
  - [ ] 31.1. Implement new session creation while preserving old ones
  - [ ] 31.2. Add commands to list and resume previous sessions
  - References: Requirement 4.1, 4.2, 4.3, 4.4, 4.5

- [ ] 32. Token Management Enhancement
  - [ ] 32.1. Implement manual token refresh command
  - [ ] 32.2. Add automatic token refresh functionality
  - [ ] 32.3. Implement invalid token clearing and re-authentication
  - [ ] 32.4. Add token operation logging
  - References: Requirement 5.1, 5.2, 5.3, 5.4, 5.5

- [ ] 33. TUI Layout Management Enhancement
  - [ ] 33.1. Implement multiple layout modes (grid, splits, focus)
  - [ ] 33.2. Add automatic session arrangement based on active accounts
  - [ ] 33.3. Implement dynamic pane resizing
  - [ ] 33.4. Add proportional layout reflow on terminal resize
  - [ ] 33.5. Implement 9-pane maximum limit
  - References: Requirement 6.1, 6.2, 6.3, 6.4, 6.5, 6.6

- [ ] 34. Session Navigation Enhancement
  - [ ] 34.1. Implement vim-style navigation keys (hjkl)
  - [ ] 34.2. Add distinct border highlighting for active pane
  - [ ] 34.3. Implement Tab cycling through panes
  - [ ] 34.4. Add numbered pane access (Alt+1 through Alt+9)
  - [ ] 34.5. Implement pane number display
  - [ ] 34.6. Add command mode for advanced navigation
  - References: Requirement 7.1, 7.2, 7.3, 7.4, 7.5, 7.6

- [ ] 35. Session Control Enhancement
  - [ ] 35.1. Implement individual pane closing with confirmation
  - [ ] 35.2. Add inactive pane marking when Claude Code exits
  - [ ] 35.3. Implement failed session restarting
  - [ ] 35.4. Add session detaching functionality
  - [ ] 35.5. Implement input broadcasting to all panes
  - References: Requirement 8.1, 8.2, 8.3, 8.4, 8.5, 8.6

- [ ] 36. Account Integration Enhancement
  - [ ] 36.1. Implement account assignment to panes based on availability
  - [ ] 36.2. Add throttled pane marking for rate-limited accounts
  - [ ] 36.3. Implement automatic account rotation when limits are reached
  - [ ] 36.4. Add account name and status display in pane status bar
  - [ ] 36.5. Implement request queuing when all accounts are exhausted
  - [ ] 36.6. Add manual account switching per pane
  - References: Requirement 9.1, 9.2, 9.3, 9.4, 9.5, 9.6

- [ ] 37. Visual Feedback Enhancement
  - [ ] 37.1. Implement color-coded borders for pane states
  - [ ] 37.2. Add processing spinner display
  - [ ] 37.3. Implement token usage and rate limit warnings
  - [ ] 37.4. Add countdown timer for approaching rate limits
  - [ ] 37.5. Implement color and monochrome terminal support
  - [ ] 37.6. Add connection status display for each session
  - References: Requirement 10.1, 10.2, 10.3, 10.4, 10.5, 10.6

- [ ] 38. Command Interface Enhancement
  - [ ] 38.1. Implement command mode activation with ':'
  - [ ] 38.2. Add commands like :split, :vsplit, :close, :restart
  - [ ] 38.3. Implement tab completion for commands
  - [ ] 38.4. Add command history maintenance
  - [ ] 38.5. Implement available commands display in command mode
  - [ ] 38.6. Add custom key bindings via configuration
  - References: Requirement 11.1, 11.2, 11.3, 11.4, 11.5, 11.6

- [ ] 39. Mouse Support Enhancement
  - [ ] 39.1. Implement mouse support detection
  - [ ] 39.2. Add pane focusing on mouse click
  - [ ] 39.3. Implement pane border dragging for resizing
  - [ ] 39.4. Add text selection within panes
  - [ ] 39.5. Implement right-click context menu for pane actions
  - [ ] 39.6. Add mouse support disabling via configuration
  - References: Requirement 12.1, 12.2, 12.3, 12.4, 12.5, 12.6

- [ ] 40. Performance Enhancement
  - [ ] 40.1. Implement 30 FPS display update minimum
  - [ ] 40.2. Reduce input handling latency to less than 50ms
  - [ ] 40.3. Ensure immediate pane switching transitions
  - [ ] 40.4. Optimize CPU usage during session idle periods
  - [ ] 40.5. Implement smooth pane scrolling
  - References: Requirement 13.1, 13.2, 13.3, 13.4, 13.5, 13.6

- [ ] 41. Session Persistence Enhancement
  - [ ] 41.1. Implement workspace configuration import/export
  - [ ] 41.2. Add workspace file corruption recovery
  - [ ] 41.3. Implement workspace restoration on launch
  - [ ] 41.4. Add pane-account association preservation
  - [ ] 41.5. Implement multiple named workspaces support
  - References: Requirement 14.1, 14.2, 14.3, 14.4, 14.5, 14.6

- [ ] 42. Configuration Management Enhancement
  - [ ] 42.1. Implement default account setting
  - [ ] 42.2. Add environment variable override support
  - [ ] 42.3. Implement clear error messages for invalid configuration
  - [ ] 42.4. Add configuration reset to defaults functionality
  - References: Requirement 15.1, 15.2, 15.3, 15.4, 15.5, 15.6

- [ ] 43. Error Handling and Recovery Enhancement
  - [ ] 43.1. Implement exponential backoff retry logic for network failures
  - [ ] 43.2. Add user information and troubleshooting steps for Claude Code launch failures
  - [ ] 43.3. Implement graceful handling of interrupted sessions
  - References: Requirement 16.1, 16.2, 16.3, 16.4, 16.5, 16.6

- [ ] 44. Security Enhancement
  - [ ] 44.1. Ensure tokens are never logged or displayed in plain text
  - [ ] 44.2. Implement OS file permissions for file-based storage
  - [ ] 44.3. Add secure random generation for PKCE verifiers and OAuth state parameters
  - [ ] 44.4. Implement SHA256 for PKCE code challenge generation
  - [ ] 44.5. Add OAuth state parameter validation to prevent CSRF attacks
  - References: Requirement 17.1, 17.2, 17.3, 17.4, 17.5, 17.6
