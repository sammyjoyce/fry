# Multiplexer TUI Requirements

## Introduction

The fry CLI tool needs a Terminal User Interface (TUI) to enable users to manage multiple Claude Code sessions simultaneously in a single terminal window. This multiplexer feature will provide a tiled interface similar to tmux or screen, allowing developers to view and interact with multiple Claude Code instances side-by-side, switch between them seamlessly, and manage their sessions visually.

## Requirements

### 1. TUI Layout Management
**User Story:** As a developer, I want to view multiple Claude Code sessions in a tiled layout, so that I can work with different contexts simultaneously without switching terminals.

**Acceptance Criteria:**
1.1. The system shall support multiple layout modes: grid, vertical split, horizontal split, and focus mode
1.2. When launching the multiplexer, the system shall automatically arrange sessions based on the number of active accounts
1.3. The system shall allow dynamic resizing of panes while maintaining readable content
1.4. If terminal is resized, then the system shall reflow the layout proportionally
1.5. The system shall support a maximum of 9 simultaneous panes (3x3 grid)
1.6. The system shall display a status bar showing active account for each pane

### 2. Session Navigation
**User Story:** As a developer, I want to navigate between Claude Code sessions using keyboard shortcuts, so that I can quickly switch context without using the mouse.

**Acceptance Criteria:**
2.1. The system shall support vim-style navigation keys (hjkl) for moving between panes
2.2. The system shall highlight the currently active pane with a distinct border
2.3. When pressing Tab, the system shall cycle through panes in order
2.4. The system shall support numbered pane access (Alt+1 through Alt+9)
2.5. The system shall display pane numbers in the top-left corner of each pane
2.6. The system shall support a command mode for advanced navigation

### 3. Session Control
**User Story:** As a developer, I want to control individual Claude Code sessions from the multiplexer, so that I can manage sessions without leaving the TUI.

**Acceptance Criteria:**
3.1. The system shall allow starting new Claude Code sessions in empty panes
3.2. The system shall support closing individual panes with confirmation
3.3. When a Claude Code process exits, the system shall mark the pane as inactive
3.4. The system shall allow restarting failed sessions in-place
3.5. The system shall support detaching from the multiplexer while keeping sessions running
3.6. The system shall allow broadcasting input to all panes simultaneously

### 4. Account Integration
**User Story:** As a developer, I want each pane to use a different account, so that I can leverage multiple Claude Code accounts for increased capacity.

**Acceptance Criteria:**
4.1. The system shall assign accounts to panes based on availability
4.2. When an account is rate-limited, the system shall mark its pane as throttled
4.3. The system shall support automatic account rotation when limits are reached
4.4. The system shall display account name and status in each pane's status bar
4.5. If all accounts are exhausted, then the system shall queue requests
4.6. The system shall support manual account switching per pane

### 5. Visual Feedback
**User Story:** As a user, I want clear visual indicators of session state, so that I can quickly understand the status of each Claude Code instance.

**Acceptance Criteria:**
5.1. The system shall use color-coded borders: green (active), yellow (busy), red (error), gray (inactive)
5.2. The system shall display a spinner when Claude Code is processing
5.3. The system shall show token usage and rate limit warnings
5.4. When approaching rate limits, the system shall display a countdown timer
5.5. The system shall support both color and monochrome terminals
5.6. The system shall display connection status for each session

### 6. Command Interface
**User Story:** As a power user, I want a command interface within the TUI, so that I can perform advanced operations without leaving the multiplexer.

**Acceptance Criteria:**
6.1. The system shall provide a command mode activated by ':'
6.2. The system shall support commands like :split, :vsplit, :close, :restart
6.3. The system shall provide tab completion for commands
6.4. The system shall maintain command history across sessions
6.5. When entering command mode, the system shall display available commands
6.6. The system shall support custom key bindings via configuration

### 7. Mouse Support
**User Story:** As a user, I want optional mouse support, so that I can interact with the TUI using familiar point-and-click actions.

**Acceptance Criteria:**
7.1. The system shall detect mouse support in the terminal
7.2. When mouse is enabled, clicking a pane shall focus it
7.3. The system shall support dragging pane borders to resize
7.4. The system shall allow text selection within panes
7.5. Right-click shall open a context menu with pane actions
7.6. The system shall allow disabling mouse support via configuration

### 8. Performance Requirements
**User Story:** As a user, I want a responsive TUI that doesn't lag, so that my workflow remains smooth even with multiple active sessions.

**Acceptance Criteria:**
8.1. The system shall update the display at minimum 30 FPS
8.2. The system shall handle input with less than 50ms latency
8.3. When switching panes, the transition shall be immediate (<10ms)
8.4. The system shall use minimal CPU when sessions are idle
8.5. The system shall efficiently handle terminal resizing
8.6. The system shall support smooth scrolling in panes

### 9. Session Persistence
**User Story:** As a developer, I want my multiplexer layout to persist, so that I can resume my exact workspace after interruptions.

**Acceptance Criteria:**
9.1. The system shall save layout configuration on exit
9.2. The system shall restore pane arrangement on restart
9.3. The system shall preserve account assignments per pane
9.4. When restoring, the system shall reconnect to running Claude Code processes
9.5. The system shall support named workspace configurations
9.6. The system shall allow exporting/importing workspace layouts

### 10. Error Handling
**User Story:** As a user, I want graceful error handling in the TUI, so that issues don't crash the entire multiplexer.

**Acceptance Criteria:**
10.1. If a pane crashes, the system shall contain the failure to that pane
10.2. The system shall display error messages in a non-intrusive notification area
10.3. When terminal capabilities are insufficient, the system shall fall back gracefully
10.4. The system shall log errors to a debug file for troubleshooting
10.5. The system shall provide recovery suggestions for common errors
10.6. The system shall handle loss of network connectivity gracefully