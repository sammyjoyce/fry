# Session Multiplexing Implementation Plan

## Executive Summary

This implementation plan outlines the development of session multiplexing capabilities for the fry CLI tool. Fry will manage multiple OAuth accounts for Claude Code (the official Anthropic CLI at `/Users/sam/.local/bin/claude`), allowing users to switch between different authenticated sessions. The implementation is divided into 5 phases over approximately 8-10 weeks, with clear milestones and deliverables for each phase.

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

### Phase 4: Enhanced Features (Week 7-8)
**Goal**: Add token management, session search, and TUI enhancements

**Milestones**:
- Token inspect/refresh commands
- Session search and filtering
- TUI session browser

**Dependencies**: Phase 3 completion

**Risk Factors**:
- TUI complexity may impact timeline
- Search performance with large histories

### Phase 5: Testing & Polish (Week 9-10)
**Goal**: Comprehensive testing, documentation, and release preparation

**Milestones**:
- 80% test coverage achieved
- Security audit complete
- Documentation and examples ready

**Dependencies**: Phases 1-4 complete

**Risk Factors**:
- Security issues discovered late
- Platform-specific bugs

## Task Breakdown

### Phase 1 Tasks

```
Task ID: [FRY-001]
Title: Create keychain abstraction interface
Description: Design and implement platform-agnostic keychain interface
Acceptance Criteria:
- [ ] keychain.h interface defined
- [ ] Error codes for keychain operations added
- [ ] Mock implementation for testing
Dependencies: None
Estimated Effort: 4 hours
Priority: High
Assigned Skills: C programming, API design
```

```
Task ID: [FRY-002]
Title: Implement macOS keychain backend
Description: Implement keychain operations using macOS Security Framework
Acceptance Criteria:
- [ ] Store/retrieve/delete operations working
- [ ] Proper error handling for all edge cases
- [ ] Memory management verified (no leaks)
Dependencies: [FRY-001]
Estimated Effort: 8 hours
Priority: High
Assigned Skills: macOS development, C programming
```

```
Task ID: [FRY-003]
Title: Implement Linux keychain backend
Description: Implement keychain operations using libsecret
Acceptance Criteria:
- [ ] Integration with Secret Service API
- [ ] Fallback for systems without libsecret
- [ ] Cross-distro testing (Ubuntu, Fedora, Arch)
Dependencies: [FRY-001]
Estimated Effort: 8 hours
Priority: High
Assigned Skills: Linux development, DBus knowledge
```

```
Task ID: [FRY-004]
Title: Implement Windows keychain backend
Description: Implement keychain operations using Windows Credential Manager
Acceptance Criteria:
- [ ] Credential storage/retrieval working
- [ ] Unicode handling correct
- [ ] Windows 10/11 compatibility verified
Dependencies: [FRY-001]
Estimated Effort: 8 hours
Priority: Medium
Assigned Skills: Windows API, C programming
```

```
Task ID: [FRY-005]
Title: Enhance OAuth implementation with token storage
Description: Integrate keychain storage into existing OAuth flow
Acceptance Criteria:
- [ ] Tokens stored in keychain after auth (or fallback JSON)
- [ ] Automatic token refresh using console.anthropic.com/v1/oauth/token
- [ ] Token expiration handling with 5-minute buffer
- [ ] Support for API key creation in console mode
Dependencies: [FRY-001]
Estimated Effort: 6 hours
Priority: High
Assigned Skills: OAuth 2.0, C programming
```

```
Task ID: [FRY-006]
Title: Add OAuth PKCE support
Description: Implement Proof Key for Code Exchange for enhanced security
Acceptance Criteria:
- [ ] Code verifier generation using secure random (43-128 chars)
- [ ] Code challenge calculation using SHA256 and base64url encoding
- [ ] Support for both claude.ai and console.anthropic.com endpoints
- [ ] Manual code paste flow implemented (no local server)
Dependencies: None
Estimated Effort: 4 hours
Priority: High
Assigned Skills: OAuth 2.0, Cryptography
```

```
Task ID: [FRY-007]
Title: Create OAuth flow integration tests
Description: End-to-end tests for complete OAuth flow
Acceptance Criteria:
- [ ] Mock OAuth endpoints for claude.ai and console.anthropic.com
- [ ] PKCE flow validation tests
- [ ] Token refresh scenarios with expired tokens
- [ ] API key creation flow for console mode
Dependencies: [FRY-005], [FRY-006]
Estimated Effort: 6 hours
Priority: High
Assigned Skills: Testing, OAuth 2.0
```

```
Task ID: [FRY-007A]
Title: Implement Anthropic OAuth configuration
Description: Configure OAuth endpoints and parameters for Anthropic
Acceptance Criteria:
- [ ] Client ID hardcoded: 9d1c250a-e61b-44d9-88ed-5944d1962f5e
- [ ] Redirect URI: console.anthropic.com/oauth/code/callback
- [ ] Scopes: org:create_api_key user:profile user:inference
- [ ] Mode selection UI for Pro/Max vs API key
Dependencies: [FRY-006]
Estimated Effort: 3 hours
Priority: High
Assigned Skills: OAuth 2.0, UI design
```

### Phase 2 Tasks

```
Task ID: [FRY-008]
Title: Design session manager architecture
Description: Create session.h interface and data structures
Acceptance Criteria:
- [ ] Session state structure defined
- [ ] Manager interface complete
- [ ] Error handling strategy documented
Dependencies: None
Estimated Effort: 4 hours
Priority: High
Assigned Skills: System design, C programming
```

```
Task ID: [FRY-009]
Title: Implement session storage layer
Description: JSON-based session persistence in cache directory
Acceptance Criteria:
- [ ] Session save/load working
- [ ] Atomic file operations used
- [ ] Cache directory management implemented
Dependencies: [FRY-008]
Estimated Effort: 6 hours
Priority: High
Assigned Skills: File I/O, JSON handling
```

```
Task ID: [FRY-010]
Title: Create Claude Code process manager
Description: Process management for Claude Code CLI
Acceptance Criteria:
- [ ] Process launching with auth tokens
- [ ] Process monitoring and status checking
- [ ] Clean termination handling
Dependencies: [FRY-008]
Estimated Effort: 8 hours
Priority: High
Assigned Skills: Process management, POSIX
```

```
Task ID: [FRY-011]
Title: Implement session lifecycle management
Description: Create, resume, suspend, and terminate sessions
Acceptance Criteria:
- [ ] State transitions correct
- [ ] Persistence at each transition
- [ ] Concurrent session support
Dependencies: [FRY-009], [FRY-010]
Estimated Effort: 8 hours
Priority: High
Assigned Skills: State machines, C programming
```

```
Task ID: [FRY-012]
Title: Add session context management
Description: Maintain Claude Code session state and working directory
Acceptance Criteria:
- [ ] Working directory saved and restored
- [ ] Environment variables preserved
- [ ] Session metadata tracked
Dependencies: [FRY-011]
Estimated Effort: 6 hours
Priority: Medium
Assigned Skills: File systems, Process management
```

```
Task ID: [FRY-013]
Title: Implement session commands
Description: CLI commands for session management
Acceptance Criteria:
- [ ] session new/list/resume commands
- [ ] Proper error messages
- [ ] Help text comprehensive
Dependencies: [FRY-011]
Estimated Effort: 4 hours
Priority: High
Assigned Skills: CLI design, C programming
```

### Phase 3 Tasks

```
Task ID: [FRY-014]
Title: Enhance account manager for multi-account
Description: Refactor account storage for multiple accounts
Acceptance Criteria:
- [ ] Multiple accounts stored correctly
- [ ] Account metadata preserved
- [ ] Migration from single account
Dependencies: None
Estimated Effort: 6 hours
Priority: High
Assigned Skills: Data modeling, C programming
```

```
Task ID: [FRY-015]
Title: Implement account switching logic
Description: Fast account switching with session preservation
Acceptance Criteria:
- [ ] Switch completes < 100ms
- [ ] Session state preserved
- [ ] Active account tracking correct
Dependencies: [FRY-014]
Estimated Effort: 6 hours
Priority: High
Assigned Skills: Performance optimization, C programming
```

```
Task ID: [FRY-016]
Title: Add account-session association
Description: Link sessions to specific accounts
Acceptance Criteria:
- [ ] Sessions filtered by account
- [ ] Cross-account isolation enforced
- [ ] Session transfer between accounts
Dependencies: [FRY-014], [FRY-011]
Estimated Effort: 4 hours
Priority: High
Assigned Skills: Data relationships, C programming
```

```
Task ID: [FRY-017]
Title: Implement session caching per account
Description: Separate session caches for each account
Acceptance Criteria:
- [ ] Cache directories organized by account
- [ ] Cache size limits per account
- [ ] Cache cleanup implemented
Dependencies: [FRY-016]
Estimated Effort: 4 hours
Priority: Medium
Assigned Skills: File systems, Caching strategies
```

```
Task ID: [FRY-018]
Title: Add account status indicators
Description: Show account status in CLI and TUI
Acceptance Criteria:
- [ ] Active account clearly marked
- [ ] Token expiration warnings
- [ ] Last activity timestamps shown
Dependencies: [FRY-015]
Estimated Effort: 3 hours
Priority: Medium
Assigned Skills: UI/UX, C programming
```

### Phase 4 Tasks

```
Task ID: [FRY-019]
Title: Implement token inspect command
Description: Show token details and expiration
Acceptance Criteria:
- [ ] Token details displayed safely
- [ ] Expiration time human-readable
- [ ] Scopes and permissions shown
Dependencies: [FRY-005]
Estimated Effort: 3 hours
Priority: Medium
Assigned Skills: CLI design, Security awareness
```

```
Task ID: [FRY-020]
Title: Implement token refresh command
Description: Manual token refresh capability
Acceptance Criteria:
- [ ] Force refresh working
- [ ] Success/failure feedback clear
- [ ] Automatic retry on failure
Dependencies: [FRY-005]
Estimated Effort: 3 hours
Priority: Medium
Assigned Skills: OAuth 2.0, Error handling
```

```
Task ID: [FRY-021]
Title: Add session search functionality
Description: Search sessions by content and metadata
Acceptance Criteria:
- [ ] Full-text search working
- [ ] Metadata filtering implemented
- [ ] Search performance < 200ms
Dependencies: [FRY-012]
Estimated Effort: 8 hours
Priority: Medium
Assigned Skills: Search algorithms, C programming
```

```
Task ID: [FRY-022]
Title: Create TUI session browser
Description: Interactive session selection interface
Acceptance Criteria:
- [ ] Session list navigation smooth
- [ ] Preview pane shows context
- [ ] Keyboard shortcuts intuitive
Dependencies: [FRY-021]
Estimated Effort: 12 hours
Priority: Low
Assigned Skills: TUI development, ncurses
```

```
Task ID: [FRY-023]
Title: Add session export/import
Description: Backup and restore sessions
Acceptance Criteria:
- [ ] Export format documented
- [ ] Import validates data integrity
- [ ] Encryption option available
Dependencies: [FRY-012]
Estimated Effort: 6 hours
Priority: Low
Assigned Skills: Data serialization, Security
```

### Phase 5 Tasks

```
Task ID: [FRY-024]
Title: Create comprehensive test suite
Description: Unit and integration tests for all components
Acceptance Criteria:
- [ ] 80% code coverage achieved
- [ ] All error paths tested
- [ ] Platform-specific tests passing
Dependencies: All previous phases
Estimated Effort: 16 hours
Priority: High
Assigned Skills: Testing, C programming
```

```
Task ID: [FRY-025]
Title: Perform security audit
Description: Review all security-sensitive code
Acceptance Criteria:
- [ ] Token handling reviewed
- [ ] Input validation verified
- [ ] Encryption correctness confirmed
Dependencies: All previous phases
Estimated Effort: 8 hours
Priority: High
Assigned Skills: Security, Code review
```

```
Task ID: [FRY-026]
Title: Write user documentation
Description: Complete documentation for all features
Acceptance Criteria:
- [ ] README updated
- [ ] Man page created
- [ ] Example workflows documented
Dependencies: All features complete
Estimated Effort: 8 hours
Priority: High
Assigned Skills: Technical writing
```

```
Task ID: [FRY-027]
Title: Create demo scripts
Description: Automated demos for key features
Acceptance Criteria:
- [ ] Multi-account demo
- [ ] Session switching demo
- [ ] Error recovery demo
Dependencies: All features complete
Estimated Effort: 4 hours
Priority: Medium
Assigned Skills: Scripting, Demo creation
```

```
Task ID: [FRY-028]
Title: Performance optimization pass
Description: Profile and optimize critical paths
Acceptance Criteria:
- [ ] Startup time < 500ms verified
- [ ] Memory usage profiled
- [ ] Bottlenecks eliminated
Dependencies: All features complete
Estimated Effort: 8 hours
Priority: Medium
Assigned Skills: Performance analysis, C optimization
```

## Development Sequence

### Critical Path
1. [FRY-001] → [FRY-002/003/004] → [FRY-005] → [FRY-008] → [FRY-010] → [FRY-011]

### Parallel Development Opportunities
- Phase 1: Keychain backends (macOS/Linux/Windows) can be developed in parallel
- Phase 2: Session storage [FRY-009] and Claude client [FRY-010] can be parallel
- Phase 4: Token commands and session search can be developed independently

### Integration Points
1. **After Phase 1**: OAuth + Keychain integration testing
2. **After Phase 2**: Session + Claude API integration testing  
3. **After Phase 3**: Multi-account + Session integration testing
4. **After Phase 4**: Full feature integration testing

## Code Organization

### Project Structure
```
src/
├── core/
│   ├── keychain.h          # New: Keychain abstraction
│   ├── keychain.c          # New: Common keychain code
│   ├── keychain_macos.c    # New: macOS implementation
│   ├── keychain_linux.c    # New: Linux implementation
│   ├── keychain_windows.c  # New: Windows implementation
│   ├── session.h           # New: Session management
│   ├── session.c           # New: Session implementation
│   ├── claude_process.h    # New: Claude Code process manager
│   ├── claude_process.c    # New: Process management implementation
│   └── oauth.c             # Enhanced: Token storage integration
├── cli/
│   ├── cmd_session.c       # New: Session commands
│   ├── cmd_token.c         # New: Token commands
│   └── cmd_accounts.c      # Enhanced: Multi-account support
└── tui/
    └── tui_session.c       # New: Session browser
```

### Module Organization
- **keychain**: Platform-specific implementations with common interface
- **session**: Core session logic, independent of storage
- **claude_process**: Claude Code process management and monitoring
- **commands**: Thin wrappers calling core functions

### Naming Conventions
- Types: `app_[module]_[type]_t` (e.g., `app_session_manager_t`)
- Functions: `app_[module]_[action]` (e.g., `app_session_create`)
- Constants: `APP_[MODULE]_[NAME]` (e.g., `APP_SESSION_MAX_HISTORY`)
- Files: `[module]_[platform].c` for platform-specific code

### Code Style Guidelines
- Google C style (enforced by .clang-format)
- All functions return `app_error`
- NULL checks on all pointer parameters
- Resource cleanup in reverse order of acquisition
- Platform-specific code isolated in separate files

### Documentation Standards
- Doxygen comments for all public APIs
- Implementation notes for complex algorithms
- Platform-specific quirks documented inline
- Example usage in header comments

## Development Environment

### Required Tools and Versions
- **Compiler**: clang 15+ or gcc 12+ (C23 support)
- **Build**: Zig 0.13.0+
- **Format**: clang-format 15+
- **Analysis**: clang-tidy 15+
- **Docs**: doxygen 1.9+

### Environment Setup Steps
```bash
# 1. Install dependencies (macOS)
brew install zig clang-format doxygen

# 2. Install platform-specific deps
# macOS: Xcode Command Line Tools
# Linux: libsecret-1-dev, libcurl4-openssl-dev
# Windows: Visual Studio Build Tools

# 3. Clone and setup
git clone https://github.com/user/fry.git
cd fry
pre-commit install

# 4. Build
zig build -Doptimize=Debug -Denable-tui=true

# 5. Run tests
zig build test
```

### Configuration Management
- Development config: `.env.development`
- Test config: `.env.test`
- Platform flags in `build.zig`
- Feature flags via `-D` options

### Development Workflows
1. **Feature Development**
   ```bash
   git checkout -b feature/FRY-XXX-description
   # Make changes
   zig build test
   pre-commit run --all-files
   git commit -m "feat: implement FRY-XXX"
   ```

2. **Testing Workflow**
   ```bash
   # Unit tests
   zig build test
   
   # Integration tests
   ./test-runner.sh integration
   
   # Platform-specific
   zig build test -Dtarget=x86_64-windows-gnu
   ```

## Testing Plan

### Test-Driven Development Approach
1. Write interface header first
2. Create test file with expected behavior
3. Implement until tests pass
4. Refactor with confidence

### Test Coverage Targets
- Core modules: 90% coverage
- CLI commands: 80% coverage
- Platform-specific: 70% coverage
- Overall target: 80% coverage

### Testing Milestones
- Phase 1: Keychain and OAuth tests complete
- Phase 2: Session and API tests complete
- Phase 3: Multi-account tests complete
- Phase 4: Feature tests complete
- Phase 5: Full regression suite passing

### Test Data Requirements
- Mock OAuth responses
- Sample session histories
- Test account configurations
- Error response samples

## Integration Strategy

### Component Integration Order
1. Keychain + OAuth
2. Session + Storage
3. Session + Claude API
4. Account + Session
5. CLI + Core
6. TUI + Core

### Integration Testing Approach
- Bottom-up integration
- Mock external dependencies
- Real integration tests with test accounts
- Platform-specific integration suites

### API Versioning Strategy
- Claude API version in config
- Graceful degradation for older versions
- Version negotiation on startup
- Clear errors for unsupported versions

### Backward Compatibility
- Config format versioning
- Automatic migration on upgrade
- Legacy command aliases
- Deprecation warnings

## Quality Checkpoints

### Code Review Process
1. Self-review against checklist
2. Automated checks (format, lint, test)
3. Peer review for architecture changes
4. Security review for auth/crypto code

### Quality Gates
- [ ] All tests passing
- [ ] Code coverage meets target
- [ ] No compiler warnings
- [ ] clang-tidy clean
- [ ] Documentation complete

### Performance Benchmarks
- Startup: < 500ms
- Account switch: < 100ms  
- Session list (100 items): < 200ms
- Message send: < 100ms (excluding API)

### Security Checkpoints
- [ ] Token storage encrypted
- [ ] No secrets in logs
- [ ] Input validation complete
- [ ] HTTPS enforcement
- [ ] OAuth state validation

## Resource Allocation Suggestions

### Team Structure (if applicable)
- Lead Developer: Architecture and core implementation
- Platform Developers: One per platform (macOS/Linux/Windows)
- QA Engineer: Test suite and automation
- Technical Writer: Documentation

### Skill Requirements
- **Core Team**: C23, OAuth 2.0, HTTP, JSON
- **Platform Specialists**: OS-specific APIs
- **Security**: Cryptography, secure coding
- **UX**: CLI/TUI design

### Time Allocation
- Development: 60%
- Testing: 25%
- Documentation: 10%
- Buffer: 5%

## Risk Mitigation Tasks

```
Task ID: [FRY-MIT-001]
Title: Create fallback token storage
Description: Implement encrypted file storage as keychain fallback
Acceptance Criteria:
- [ ] Encryption using user-derived key
- [ ] Automatic fallback on keychain failure
- [ ] Migration tool for recovery
Dependencies: [FRY-001]
Estimated Effort: 8 hours
Priority: High
Assigned Skills: Cryptography, File systems
```

```
Task ID: [FRY-MIT-002]
Title: Implement Claude Code version detection
Description: Detect and handle Claude Code CLI version changes
Acceptance Criteria:
- [ ] Version detection from claude --version
- [ ] Compatibility checks defined
- [ ] Graceful handling of version mismatches
Dependencies: [FRY-010]
Estimated Effort: 4 hours
Priority: Medium
Assigned Skills: CLI integration, Error handling
```

```
Task ID: [FRY-MIT-003]
Title: Add diagnostic commands
Description: Self-diagnostic tools for troubleshooting
Acceptance Criteria:
- [ ] `fry diagnose` command
- [ ] Connectivity tests
- [ ] Configuration validation
Dependencies: Core features complete
Estimated Effort: 6 hours
Priority: Medium
Assigned Skills: Debugging, CLI design
```

## Success Metrics and Checkpoints

### Phase 1 Success Criteria
- [ ] OAuth flow works end-to-end
- [ ] Tokens stored securely
- [ ] All platforms supported

### Phase 2 Success Criteria
- [ ] Claude Code launches with proper auth
- [ ] Sessions persist across restarts
- [ ] Error handling comprehensive

### Phase 3 Success Criteria
- [ ] Account switching < 100ms
- [ ] No data loss on switch
- [ ] Intuitive multi-account UX

### Phase 4 Success Criteria
- [ ] All commands implemented
- [ ] Search performance meets targets
- [ ] TUI enhances productivity

### Phase 5 Success Criteria
- [ ] Test coverage > 80%
- [ ] Security audit passed
- [ ] Documentation complete
- [ ] Performance targets met

## Gantt Chart (Text Representation)

```
Week:    1  2  3  4  5  6  7  8  9  10
Phase 1: ████████
Phase 2:       ████████
Phase 3:             ████████
Phase 4:                   ████████
Phase 5:                         ████████

Key Milestones:
Week 2: OAuth + Keychain integrated ▼
Week 4: Claude Code launching with auth ▼
Week 6: Multi-account switching ▼
Week 8: All features complete ▼
Week 10: Release ready ▼
```

## Next Steps

1. **Immediate Actions**
   - Set up development environment
   - Create feature branch structure
   - Begin Phase 1 implementation

2. **Week 1 Goals**
   - Complete keychain interface design
   - Start platform implementations
   - Set up CI/CD pipeline

3. **Communication**
   - Daily progress updates
   - Weekly milestone reviews
   - Blocker escalation process