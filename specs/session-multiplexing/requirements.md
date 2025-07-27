# Session Multiplexing Requirements

## Introduction

The fry CLI tool enables users to manage multiple Claude Code OAuth accounts and seamlessly switch between different Claude Code sessions. Claude Code is the official CLI tool located at `/Users/sam/.local/bin/claude` that provides an interactive coding assistant. This feature allows developers to maintain separate Claude Code sessions for different projects, clients, or use cases without the overhead of managing multiple installations or constantly re-authenticating.

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

### 6. Configuration Management
**User Story:** As a user, I want to configure fry's behavior, so that I can customize it to my workflow.

**Acceptance Criteria:**
6.1. The system shall support a configuration file for default settings
6.2. The system shall allow users to set a default account
6.3. The system shall support environment variable overrides for configuration
6.4. When configuration is invalid, the system shall provide clear error messages
6.5. The system shall allow users to reset configuration to defaults

### 7. Error Handling and Recovery
**User Story:** As a user, I want clear error messages and recovery options, so that I can resolve issues quickly.

**Acceptance Criteria:**
7.1. The system shall provide descriptive error messages for all failure scenarios
7.2. When network requests fail, the system shall implement exponential backoff retry logic
7.3. If Claude Code cannot be launched, then the system shall inform the user and suggest troubleshooting steps
7.4. The system shall gracefully handle interrupted sessions without data loss
7.5. The system shall provide debug logging options for troubleshooting

### 8. Security Requirements
**User Story:** As a security-conscious user, I want my credentials and data protected, so that my accounts remain secure.

**Acceptance Criteria:**
8.1. The system shall never log or display tokens in plain text
8.2. The system shall use the system keychain where available for token storage
8.3. When using file-based storage, the system shall rely on OS file permissions for security
8.4. The system shall implement secure random generation for PKCE verifiers and OAuth state parameters
8.5. The system shall use SHA256 for PKCE code challenge generation
8.6. The system shall validate OAuth state parameters to prevent CSRF attacks

### 9. Performance Requirements
**User Story:** As a user, I want fast and responsive CLI operations, so that my workflow isn't interrupted.

**Acceptance Criteria:**
9.1. The system shall complete account switching in under 100ms
9.2. The system shall start up and be ready for input in under 500ms
9.3. While launching Claude Code, the system shall provide visual feedback
9.4. The system shall implement timeouts to prevent indefinite hangs
9.5. The system shall cache account metadata to reduce startup time