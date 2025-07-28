# Session Multiplexer Implementation Plan

## Executive Summary

This implementation plan outlines the development of session multiplexing capabilities with a Terminal User Interface (TUI) for the fry CLI tool. Fry will manage multiple OAuth accounts for Claude Code (the official Anthropic CLI at `/Users/sam/.local/bin/claude`), allowing users to switch between different authenticated sessions, all within a tiled NCurses-based interface. The implementation is divided into 7 phases over approximately 12-14 weeks, with clear milestones and deliverables for each phase.

## Development Phases

### Phase 1: Foundation & OAuth Enhancement (Week 1-2)
**Goal**: Establish secure token storage and complete OAuth implementation

**Milestones**:
- Keychain abstraction layer complete
- OAuth flow tested end-to-end
- Token refresh mechanism working

**Dependencies**: None (can start immediately)

**Risk Factors**:
- Platform-specific keychain APIs may have undocumented quirks
- OAuth endpoint changes from Anthropic

### Phase 2: Session Management Core (Week 3-4)
**Goal**: Implement core session handling and Claude Code process management

**Milestones**:
- Session manager implemented
- Claude Code process launching working
- Session state persistence functional

**Dependencies**: Phase 1 completion (OAuth tokens required)

**Risk Factors**:
- Claude Code CLI changes may require updates
- Process management complexity across platforms

### Phase 3: Multi-Account Support (Week 5-6)
**Goal**: Enable account switching and session persistence

**Milestones**:
- Account switching < 100ms
- Session history preserved across switches
- Concurrent session support

**Dependencies**: Phase 2 completion

**Risk Factors**:
- Data migration for existing users
- Performance degradation with many accounts

### Phase 4: NCurses Foundation & Basic Panes (Week 7-8)
**Goal**: Establish NCurses infrastructure and basic pane rendering

**Milestones**:
- NCurses initialization and cleanup working
- Basic pane creation and rendering
- Simple grid layout functional

**Dependencies**: Session multiplexing feature complete

**Risk Factors**:
- NCurses compatibility across platforms
- Terminal capability detection issues

### Phase 5: PTY Integration & Process Management (Week 9-10)
**Goal**: Integrate pseudo-terminals for Claude Code processes

**Milestones**:
- PTY creation and management working
- Claude Code processes running in panes
- I/O redirection functional

**Dependencies**: Phase 4 completion

**Risk Factors**:
- Platform-specific PTY implementations
- Signal handling complexity

### Phase 6: Input Handling & Navigation (Week 11-12)
**Goal**: Implement keyboard and mouse input processing

**Milestones**:
- Keyboard navigation working
- Command mode implemented
- Mouse support functional

**Dependencies**: Phase 5 completion

**Risk Factors**:
- Complex key sequence handling
- Mouse support varies by terminal

### Phase 7: Layout Management, UI Polish & Testing (Week 13-14)
**Goal**: Advanced layouts, visual enhancements, and comprehensive testing

**Milestones**:
- Multiple layout modes working
- Dynamic resizing functional
- Status bars and notifications
- 80% test coverage achieved
- Documentation complete

**Dependencies**: Phases 1-6 complete

**Risk Factors**:
- Layout calculation complexity
- Performance with many panes
- Security issues discovered late



## Phase Success Criteria

### Phase 1 Success Criteria
- [ ] Keychain abstraction working on all platforms
- [ ] OAuth flow tested end-to-end
- [ ] Token refresh mechanism functional

### Phase 2 Success Criteria
- [ ] Session manager implemented
- [ ] Claude Code process launching working
- [ ] Session state persistence functional

### Phase 3 Success Criteria
- [ ] Account switching < 100ms
- [ ] Session history preserved
- [ ] Concurrent sessions supported

### Phase 4 Success Criteria
- [ ] NCurses initialization working
- [ ] Basic pane rendering functional
- [ ] Grid layout operational

### Phase 5 Success Criteria
- [ ] PTY creation working
- [ ] Claude Code processes in panes
- [ ] I/O redirection functional

### Phase 6 Success Criteria
- [ ] Keyboard navigation working
- [ ] Command mode implemented
- [ ] Mouse support functional

### Phase 7 Success Criteria
- [ ] Advanced layouts working
- [ ] Visual enhancements complete
- [ ] Workspaces persist correctly
- [ ] 80% test coverage achieved
- [ ] Documentation complete

## Resource Allocation Suggestions

### Team Structure (if applicable)
- **Lead Developer**: Overall architecture and cross-component integration
- **Backend Developer**: OAuth, session management, keychain integration
- **Frontend Developer**: TUI implementation, NCurses, input handling
- **QA Engineer**: Testing, cross-platform validation, performance tuning

### Technology Stack
- **Language**: C23 with POSIX extensions
- **Build System**: Zig (0.13.0+)
- **Dependencies**: 
  - NCurses for TUI
  - Platform-specific keychain libraries
  - libsecret (Linux)
  - Security.framework (macOS)
- **Testing**: Custom test framework with mocks

### Development Environment
- **Primary Platforms**: macOS, Linux (Ubuntu)
- **Development Tools**: Zig compiler, Git, VS Code with C extensions
- **Testing Tools**: Valgrind (memory), GDB (debugging)

## Risk Mitigation Strategies

### Technical Risks
1. **Cross-platform compatibility issues**
   - Mitigation: Early and continuous testing on all target platforms
   - Contingency: Fallback implementations for problematic features

2. **Terminal capability variations**
   - Mitigation: Comprehensive terminal capability detection
   - Contingency: Graceful degradation to basic functionality

3. **Performance with many sessions**
   - Mitigation: Efficient I/O multiplexing and rendering
   - Contingency: Session limiting and queuing mechanisms

### Schedule Risks
1. **OAuth implementation delays**
   - Mitigation: Parallel development of independent components
   - Contingency: Mock implementations for testing

2. **TUI complexity exceeding estimates**
   - Mitigation: Incremental implementation with frequent validation
   - Contingency: Phased feature delivery

### Quality Risks
1. **Security vulnerabilities**
   - Mitigation: Security-focused code reviews and static analysis
   - Contingency: External security audit before release

2. **Data loss scenarios**
   - Mitigation: Comprehensive persistence testing
   - Contingency: Backup and recovery mechanisms

## Phase Success Criteria

### Phase 1 Success Criteria
- [ ] Keychain abstraction working on all platforms
- [ ] OAuth flow tested end-to-end
- [ ] Token refresh mechanism functional

### Phase 2 Success Criteria
- [ ] Session manager implemented
- [ ] Claude Code process launching working
- [ ] Session state persistence functional

### Phase 3 Success Criteria
- [ ] Account switching < 100ms
- [ ] Session history preserved
- [ ] Concurrent sessions supported

### Phase 4 Success Criteria
- [ ] NCurses initialization working
- [ ] Basic pane rendering functional
- [ ] Grid layout operational

### Phase 5 Success Criteria
- [ ] PTY creation working
- [ ] Claude Code processes in panes
- [ ] I/O redirection functional

### Phase 6 Success Criteria
- [ ] Keyboard navigation working
- [ ] Command mode implemented
- [ ] Mouse support functional

### Phase 7 Success Criteria
- [ ] Advanced layouts working
- [ ] Visual enhancements complete
- [ ] Workspaces persist correctly
- [ ] 80% test coverage achieved
- [ ] Documentation complete

## Resource Allocation Suggestions

### Team Structure (if applicable)
- **Lead Developer**: Overall architecture and cross-component integration
- **Backend Developer**: OAuth, session management, keychain integration
- **Frontend Developer**: TUI implementation, NCurses, input handling
- **QA Engineer**: Testing, cross-platform validation, performance tuning

### Technology Stack
- **Language**: C23 with POSIX extensions
- **Build System**: Zig (0.13.0+)
- **Dependencies**: 
  - NCurses for TUI
  - Platform-specific keychain libraries
  - libsecret (Linux)
  - Security.framework (macOS)
- **Testing**: Custom test framework with mocks

### Development Environment
- **Primary Platforms**: macOS, Linux (Ubuntu)
- **Development Tools**: Zig compiler, Git, VS Code with C extensions
- **Testing Tools**: Valgrind (memory), GDB (debugging)

## Risk Mitigation Strategies

### Technical Risks
1. **Cross-platform compatibility issues**
   - Mitigation: Early and continuous testing on all target platforms
   - Contingency: Fallback implementations for problematic features

2. **Terminal capability variations**
   - Mitigation: Comprehensive terminal capability detection
   - Contingency: Graceful degradation to basic functionality

3. **Performance with many sessions**
   - Mitigation: Efficient I/O multiplexing and rendering
   - Contingency: Session limiting and queuing mechanisms

### Schedule Risks
1. **OAuth implementation delays**
   - Mitigation: Parallel development of independent components
   - Contingency: Mock implementations for testing

2. **TUI complexity exceeding estimates**
   - Mitigation: Incremental implementation with frequent validation
   - Contingency: Phased feature delivery

### Quality Risks
1. **Security vulnerabilities**
   - Mitigation: Security-focused code reviews and static analysis
   - Contingency: External security audit before release

2. **Data loss scenarios**
   - Mitigation: Comprehensive persistence testing
   - Contingency: Backup and recovery mechanisms
