# Multiplexer TUI Implementation Tasks

## Overview
This document contains the implementation tasks for the Multiplexer TUI feature. Each task is designed to be executed by a coding agent in sequence.

## Tasks

- [ ] 1. Set up NCurses development environment and create TUI module structure
  - Create `src/tui/tui.h` with main TUI interface definitions
  - Create `src/tui/tui.c` with NCurses initialization and cleanup functions
  - Add NCurses detection to `build.zig` with conditional compilation
  - Create basic test file `test/tui_test.c` with NCurses mock
  - References: Requirement 1.1, 8.1

- [ ] 2. Implement NCurses abstraction layer for testability
  - [ ] 2.1. Create terminal capability detection functions
    - Implement color support detection
    - Add mouse capability checking
    - Create fallback modes for limited terminals
    - References: Requirement 5.5, 7.1
  - [ ] 2.2. Create NCurses wrapper functions for mocking
    - Wrap window creation/destruction functions
    - Abstract input and output operations
    - Add test mode that doesn't require real terminal
    - References: Requirement 8.1

- [ ] 3. Implement basic pane data structures and management
  - [ ] 3.1. Create pane structure and lifecycle functions
    - Define `app_pane_t` structure in `src/tui/tui_pane.h`
    - Implement pane creation with NCurses window allocation
    - Add pane destruction with proper cleanup
    - References: Requirement 1.1, 1.3
  - [ ] 3.2. Implement pane rendering with borders and content areas
    - Create border drawing functions with state-based colors
    - Calculate content area within borders
    - Add pane title rendering in top border
    - References: Requirement 1.6, 5.1

- [ ] 4. Create grid layout manager for automatic pane arrangement
  - [ ] 4.1. Implement grid dimension calculation
    - Calculate optimal rows/cols for N panes (1-9)
    - Handle terminal size constraints
    - Maintain aspect ratios for readability
    - References: Requirement 1.2, 1.5
  - [ ] 4.2. Create pane positioning algorithm
    - Calculate pane coordinates in grid
    - Implement equal sizing with remainder distribution
    - Add minimum size enforcement
    - References: Requirement 1.3, 1.4

- [ ] 5. Build render engine with optimized screen updates
  - [ ] 5.1. Implement full screen rendering pipeline
    - Create render loop in `src/tui/tui_render.c`
    - Add dirty region tracking for optimization
    - Implement double buffering if needed
    - References: Requirement 8.1, 8.5
  - [ ] 5.2. Add partial update optimization
    - Track changed panes since last render
    - Implement differential rendering
    - Add flicker prevention techniques
    - References: Requirement 8.1

- [ ] 6. Create main event loop with input and resize handling
  - [ ] 6.1. Implement non-blocking event loop
    - Set up main loop with configurable refresh rate
    - Add non-blocking input reading
    - Integrate render engine calls
    - References: Requirement 8.1, 8.2
  - [ ] 6.2. Add terminal resize handling
    - Detect SIGWINCH signals
    - Recalculate layout on resize
    - Preserve pane focus during resize
    - References: Requirement 1.4, 8.5

- [ ] 7. Implement pseudo-terminal (PTY) management system
  - [ ] 7.1. Create cross-platform PTY abstraction
    - Implement PTY creation for POSIX systems
    - Add proper terminal mode configuration
    - Create PTY cleanup functions
    - References: Requirement 3.1, 4.1
  - [ ] 7.2. Add Windows ConPTY support
    - Implement Windows-specific PTY handling
    - Ensure API compatibility with POSIX version
    - Add fallback for older Windows versions
    - References: Requirement 3.1

- [ ] 8. Integrate Claude Code process launching in PTYs
  - [ ] 8.1. Implement process spawning with authentication
    - Launch Claude Code with OAuth tokens in environment
    - Set working directory per pane
    - Configure process environment variables
    - References: Requirement 3.1, 4.1
  - [ ] 8.2. Add process monitoring and lifecycle management
    - Monitor process status with waitpid/equivalent
    - Detect process termination
    - Implement zombie process cleanup
    - References: Requirement 3.3, 3.4

- [ ] 9. Build I/O multiplexing between PTYs and panes
  - [ ] 9.1. Implement non-blocking I/O handling
    - Use select/poll for PTY file descriptors
    - Create read buffers per pane
    - Handle partial reads gracefully
    - References: Requirement 8.2, 8.4
  - [ ] 9.2. Add ANSI escape sequence processing
    - Parse color codes and cursor movements
    - Implement scrollback buffer
    - Handle terminal control sequences
    - References: Requirement 5.1, 8.6

- [ ] 10. Create keyboard input system with configurable bindings
  - [ ] 10.1. Implement raw mode input processing
    - Set terminal to raw mode during TUI
    - Create key sequence parser
    - Handle multi-byte sequences (arrows, function keys)
    - References: Requirement 2.1, 2.4
  - [ ] 10.2. Add keybinding configuration system
    - Load keybindings from configuration
    - Support multi-key sequences (Ctrl-W, etc.)
    - Implement keybinding conflict detection
    - References: Requirement 6.6

- [ ] 11. Implement pane navigation commands
  - [ ] 11.1. Add vim-style navigation (hjkl)
    - Implement directional pane movement
    - Add wrap-around at edges option
    - Update focus indicators
    - References: Requirement 2.1, 2.2
  - [ ] 11.2. Create numbered pane access and cycling
    - Implement Alt+1 through Alt+9 shortcuts
    - Add Tab/Shift-Tab cycling
    - Display pane numbers in corners
    - References: Requirement 2.4, 2.5

- [ ] 12. Build command mode interface
  - [ ] 12.1. Create command line at bottom of screen
    - Implement ':' key to enter command mode
    - Add command input buffer
    - Show command as it's typed
    - References: Requirement 6.1, 6.5
  - [ ] 12.2. Implement command parser and execution
    - Parse commands like :split, :vsplit, :close
    - Add command validation
    - Implement command dispatch system
    - References: Requirement 6.2
  - [ ] 12.3. Add tab completion and history
    - Implement tab completion for commands
    - Add command history with up/down navigation
    - Persist history across sessions
    - References: Requirement 6.3, 6.4

- [ ] 13. Add mouse support with terminal detection
  - [ ] 13.1. Implement mouse event capture
    - Enable mouse mode in NCurses
    - Detect mouse support in terminal
    - Create mouse event handlers
    - References: Requirement 7.1, 7.2
  - [ ] 13.2. Add mouse interactions
    - Implement click to focus panes
    - Add drag to resize functionality
    - Create right-click context menu
    - References: Requirement 7.3, 7.5

- [ ] 14. Implement split layout modes
  - [ ] 14.1. Create binary tree layout structure
    - Design tree-based layout representation
    - Implement split operations
    - Add merge operations for closing panes
    - References: Requirement 1.1
  - [ ] 14.2. Add vertical and horizontal split commands
    - Implement :split and :vsplit commands
    - Calculate new pane sizes
    - Maintain minimum size constraints
    - References: Requirement 6.2

- [ ] 15. Build pane resizing functionality
  - [ ] 15.1. Implement keyboard-based resizing
    - Add resize mode with Ctrl-W commands
    - Implement incremental size adjustments
    - Maintain layout constraints
    - References: Requirement 1.3
  - [ ] 15.2. Add mouse drag resizing
    - Detect border drag initiation
    - Update sizes during drag
    - Snap to minimum sizes
    - References: Requirement 7.3

- [ ] 16. Create status bar system
  - [ ] 16.1. Implement per-pane status bars
    - Show account name and status
    - Display session information
    - Add activity indicators
    - References: Requirement 1.6, 4.4
  - [ ] 16.2. Add global status bar
    - Show current time and date
    - Display system information
    - Add mode indicators (command, resize, etc.)
    - References: Requirement 5.3

- [ ] 17. Build notification system
  - [ ] 17.1. Create toast notification rendering
    - Design notification data structure
    - Implement slide-in animation
    - Add auto-dismiss timers
    - References: Requirement 10.2
  - [ ] 17.2. Add notification queue management
    - Implement priority levels
    - Create notification history
    - Add notification persistence
    - References: Requirement 10.2

- [ ] 18. Implement focus mode for single pane maximization
  - Create toggle mechanism for focus mode
  - Save current layout before maximizing
  - Implement smooth transition animation
  - Add visual indicators for focus mode
  - References: Requirement 1.1

- [ ] 19. Build workspace persistence system
  - [ ] 19.1. Implement workspace serialization
    - Create JSON schema for workspace state
    - Serialize layout configuration
    - Save pane-account associations
    - References: Requirement 9.1, 9.2
  - [ ] 19.2. Add workspace restoration
    - Load workspace from JSON
    - Restore layout and pane positions
    - Reconnect to running sessions
    - References: Requirement 9.2, 9.4

- [ ] 20. Implement session reconnection logic
  - [ ] 20.1. Detect existing Claude Code processes
    - Scan for running processes by PID
    - Verify process ownership
    - Check process command line
    - References: Requirement 9.4
  - [ ] 20.2. Reattach to existing PTYs
    - Reconnect to PTY file descriptors
    - Restore terminal state
    - Recover scrollback buffer if possible
    - References: Requirement 9.4

- [ ] 21. Create comprehensive test suite
  - [ ] 21.1. Build unit tests with NCurses mocking
    - Test layout calculations
    - Verify input handling
    - Test pane management
    - References: Requirement 8.1
  - [ ] 21.2. Implement integration tests
    - Test with real PTY operations
    - Verify process management
    - Test workspace persistence
    - References: Requirement 3.1, 9.1

- [ ] 22. Optimize performance and resource usage
  - [ ] 22.1. Profile and optimize rendering
    - Identify rendering bottlenecks
    - Optimize screen update algorithms
    - Reduce unnecessary redraws
    - References: Requirement 8.1, 8.4
  - [ ] 22.2. Optimize memory and CPU usage
    - Implement efficient buffer management
    - Add idle state detection
    - Optimize PTY polling frequency
    - References: Requirement 8.4

- [ ] 23. Write user documentation and examples
  - Create keybinding reference card
  - Write layout mode documentation
  - Add troubleshooting guide
  - Create example workspace configurations
  - References: Requirement 6.5