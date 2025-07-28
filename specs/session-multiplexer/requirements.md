# Session Multiplexer Requirements

## Introduction

The fry CLI tool enables users to manage multiple Claude Code OAuth accounts and seamlessly switch between different Claude Code sessions through a Terminal User Interface (TUI). Claude Code is the official CLI tool located at `/Users/sam/.local/bin/claude` that provides an interactive coding assistant. This feature allows developers to maintain separate Claude Code sessions for different projects, clients, or use cases without the overhead of managing multiple installations or constantly re-authenticating, all within a tiled terminal interface.

## Requirements

### 1. Account Management
**User Story:** As a developer, I want to manage multiple Claude Code accounts, so that I can easily switch between different contexts without re-authentication.

**Acceptance Criteria:**
1.1. The system shall store multiple OAuth accounts with unique identifiers
1.2. When adding a new account, the system shall initiate the OAuth flow and securely store the resulting tokens
1.3. The system shall allow users to list all configured accounts with their status (active/inactive)
1.4. When removing an account, the system shall delete all associated tokens and session data
1.5. The system shall allow users to rename accounts for better organization
1.6. The system shall support exporting account configurations for backup purposes

### 2. Session Switching
**User Story:** As a developer, I want to quickly switch between Claude Code sessions, so that I can maintain separate coding assistant contexts for different projects.

**Acceptance Criteria:**
2.1. The system shall allow users to switch the active account with a single command
2.2. When switching accounts, the system shall preserve the Claude Code session state of the previous account
2.3. The system shall indicate which account is currently active in all interactions
2.4. If no account is active, then the system shall prompt the user to select one
2.5. The system shall maintain Claude Code session history separately for each account

### 3. OAuth Authentication
**User Story:** As a user, I want secure OAuth authentication with Anthropic, so that I can safely connect my Claude Code account without exposing credentials.

**Acceptance Criteria:**
3.1. The system shall implement the OAuth 2.0 authorization code flow with PKCE (Proof Key for Code Exchange)
3.2. When initiating authentication, the system shall open the user's default browser to Anthropic's OAuth endpoint (claude.ai or console.anthropic.com)
3.3. The system shall prompt the user to paste the authorization code from the browser redirect
3.4. The system shall support both "Claude Pro/Max" mode and "Create API Key" mode for authentication
3.5. The system shall securely store refresh tokens in the system keychain or fallback to file storage
3.6. If token refresh fails, then the system shall prompt for re-authentication
3.7. The system shall validate token expiration before launching Claude Code sessions
3.8. The system shall use the fixed client ID: 9d1c250a-e61b-44d9-88ed-5944d1962f5e

### 4. Session Management
**User Story:** As a developer, I want persistent Claude Code sessions, so that I can continue my coding work across multiple terminal sessions.

**Acceptance Criteria:**
4.1. The system shall maintain Claude Code session state for each account
4.2. When starting a new terminal session, the system shall restore the previous Claude Code context
4.3. The system shall allow users to start new Claude Code sessions while preserving old ones
4.4. The system shall provide commands to list and resume previous Claude Code sessions
4.5. While running Claude Code, the system shall display the current context (account, session ID)

### 5. Token Management
**User Story:** As a power user, I want to inspect and manage OAuth tokens, so that I can troubleshoot authentication issues and maintain security.

**Acceptance Criteria:**
5.1. The system shall provide a command to inspect token details (expiration, scopes, etc.)
5.2. The system shall allow manual token refresh via command
5.3. The system shall automatically refresh tokens when they are close to expiration
5.4. If a token is invalid, then the system shall clear it and prompt for re-authentication
5.5. The system shall log token operations for debugging purposes

### 6. TUI Layout Management
**User Story:** As a developer, I want to view multiple Claude Code sessions in a tiled layout, so that I can work with different contexts simultaneously without switching terminals.

**Acceptance Criteria:**
6.1. The system shall support multiple layout modes: grid, vertical split, horizontal split, and focus mode
6.2. When launching the multiplexer, the system shall automatically arrange sessions based on the number of active accounts
6.3. The system shall allow dynamic resizing of panes while maintaining readable content
6.4. If terminal is resized, then the system shall reflow the layout proportionally
6.5. The system shall support a maximum of 9 simultaneous panes (3x3 grid)
6.6. The system shall display a status bar showing active account for each pane

### 7. Session Navigation
**User Story:** As a developer, I want to navigate between Claude Code sessions using keyboard shortcuts, so that I can quickly switch context without using the mouse.

**Acceptance Criteria:**
7.1. The system shall support vim-style navigation keys (hjkl) for moving between panes
7.2. The system shall highlight the currently active pane with a distinct border
7.3. When pressing Tab, the system shall cycle through panes in order
7.4. The system shall support numbered pane access (Alt+1 through Alt+9)
7.5. The system shall display pane numbers in the top-left corner of each pane
7.6. The system shall support a command mode for advanced navigation

### 8. Session Control
**User Story:** As a developer, I want to control individual Claude Code sessions from the multiplexer, so that I can manage sessions without leaving the TUI.

**Acceptance Criteria:**
8.1. The system shall allow starting new Claude Code sessions in empty panes
8.2. The system shall support closing individual panes with confirmation
8.3. When a Claude Code process exits, the system shall mark the pane as inactive
8.4. The system shall allow restarting failed sessions in-place
8.5. The system shall support detaching from the multiplexer while keeping sessions running
8.6. The system shall allow broadcasting input to all panes simultaneously

### 9. Account Integration
**User Story:** As a developer, I want each pane to use a different account, so that I can leverage multiple Claude Code accounts for increased capacity.

**Acceptance Criteria:**
9.1. The system shall assign accounts to panes based on availability
9.2. When an account is rate-limited, the system shall mark its pane as throttled
9.3. The system shall support automatic account rotation when limits are reached
9.4. The system shall display account name and status in each pane's status bar
9.5. If all accounts are exhausted, then the system shall queue requests
9.6. The system shall support manual account switching per pane

### 10. Visual Feedback
**User Story:** As a user, I want clear visual indicators of session state, so that I can quickly understand the status of each Claude Code instance.

**Acceptance Criteria:**
10.1. The system shall use color-coded borders: green (active), yellow (busy), red (error), gray (inactive)
10.2. The system shall display a spinner when Claude Code is processing
10.3. The system shall show token usage and rate limit warnings
10.4. When approaching rate limits, the system shall display a countdown timer
10.5. The system shall support both color and monochrome terminals
10.6. The system shall display connection status for each session

### 11. Command Interface
**User Story:** As a power user, I want a command interface within the TUI, so that I can perform advanced operations without leaving the multiplexer.

**Acceptance Criteria:**
11.1. The system shall provide a command mode activated by ':'
11.2. The system shall support commands like :split, :vsplit, :close, :restart
11.3. The system shall provide tab completion for commands
11.4. The system shall maintain command history across sessions
11.5. When entering command mode, the system shall display available commands
11.6. The system shall support custom key bindings via configuration

### 12. Mouse Support
**User Story:** As a user, I want optional mouse support, so that I can interact with the TUI using familiar point-and-click actions.

**Acceptance Criteria:**
12.1. The system shall detect mouse support in the terminal
12.2. When mouse is enabled, clicking a pane shall focus it
12.3. The system shall support dragging pane borders to resize
12.4. The system shall allow text selection within panes
12.5. Right-click shall open a context menu with pane actions
12.6. The system shall allow disabling mouse support via configuration

### 13. Performance Requirements
**User Story:** As a user, I want a responsive TUI that doesn't lag, so that my workflow remains smooth even with multiple active sessions.

**Acceptance Criteria:**
13.1. The system shall update the display at minimum 30 FPS
13.2. The system shall handle input with less than 50ms latency
13.3. When switching panes, the transition shall be immediate (<10ms)
13.4. The system shall use minimal CPU when sessions are idle
13.5. The system shall efficiently handle terminal resizing
13.6. The system shall support smooth scrolling in panes

### 14. Session Persistence
**User Story:** As a developer, I want my multiplexer layout to persist, so that I can resume my exact workspace after interruptions.

**Acceptance Criteria:**
14.1. The system shall save workspace layout configuration to disk
14.2. When launching the multiplexer, the system shall restore the last workspace
14.3. The system shall preserve pane-account associations between sessions
14.4. The system shall support multiple named workspaces
14.5. The system shall allow importing/exporting workspace configurations
14.6. If workspace file is corrupted, the system shall provide recovery options

### 15. Configuration Management
**User Story:** As a user, I want to configure fry's behavior, so that I can customize it to my workflow.

**Acceptance Criteria:**
15.1. The system shall support a configuration file for default settings
15.2. The system shall allow users to set a default account
15.3. The system shall support environment variable overrides for configuration
15.4. When configuration is invalid, the system shall provide clear error messages
15.5. The system shall allow users to reset configuration to defaults

### 16. Error Handling and Recovery
**User Story:** As a user, I want clear error messages and recovery options, so that I can resolve issues quickly.

**Acceptance Criteria:**
16.1. The system shall provide descriptive error messages for all failure scenarios
16.2. When network requests fail, the system shall implement exponential backoff retry logic
16.3. If Claude Code cannot be launched, then the system shall inform the user and suggest troubleshooting steps
16.4. The system shall gracefully handle interrupted sessions without data loss
16.5. The system shall provide debug logging options for troubleshooting

### 17. Security Requirements
**User Story:** As a security-conscious user, I want my credentials and data protected, so that my accounts remain secure.

**Acceptance Criteria:**
17.1. The system shall never log or display tokens in plain text
17.2. The system shall use the system keychain where available for token storage
17.3. When using file-based storage, the system shall rely on OS file permissions for security
17.4. The system shall implement secure random generation for PKCE verifiers and OAuth state parameters
17.5. The system shall use SHA256 for PKCE code challenge generation
17.6. The system shall validate OAuth state parameters to prevent CSRF attacks
