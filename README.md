# fry

> A fast, cross-platform CLI for managing **Claude Code** OAuth accounts and multiplexing multiple Claude sessions in a single NCurses-powered terminal.

---

## ✨ Key Features

• **Multi-account OAuth wallet** – securely store, refresh and switch between unlimited Claude Code accounts.  
• **Session multiplexer** – spin up several Claude instances side-by-side in a tiled NCurses UI.  
• **One-command account switching** – `fry use personal` (no more copying tokens by hand).  
• **Quota management** – automatically disable accounts when rate-limited, re-enable after cooldown.  
• **Session persistence** – automatically save / restore chat history across restarts.  
• **Secure by design** – tokens are AES-256 encrypted on disk.  
• **Portable build** – modern C23 core wrapped by the Zig build system for painless cross-compiles.

---

## 🚀 Quick Start

### 1 · Prerequisites

* [Zig](https://ziglang.org/) (master branch recommended) – easiest via [`zvm`](https://github.com/tristanisham/zvm)
* A C compiler & ncurses development headers
* Valid Claude Code OAuth credentials (see below)

### 2 · Build & run

```bash
# clone & build (ReleaseSafe)
git clone https://github.com/yourname/fry.git && cd fry
zig build -Doptimize=ReleaseSafe

# view help
./zig-out/bin/fry --help
```

Or simply run `just run` if you have [just](https://github.com/casey/just) installed.

### 3 · Add your first account

```bash
# Add account interactively
fry accounts add --name personal

# Or import from Claude Code credentials
fry accounts add --file ~/.claude/.credentials.json --name work

# List all accounts
fry accounts list
```

---

## 🛠️  Command Reference

### CLI Grammar
```
fry <noun> <verb> [options] [args]
```

### Core Commands

#### Accounts
```bash
fry accounts list                    # List all stored accounts
fry accounts add --name personal     # Add new account (interactive)
fry accounts add --file creds.json   # Import from JSON file
fry accounts rename old new          # Rename an account
fry accounts rm work                 # Remove account (with prompt)
fry accounts use personal            # Set default account
fry accounts export work             # Export tokens to stdout

# Quota management
fry accounts disable work --until 5h  # Disable for 5 hours
fry accounts disable work --until 14:30  # Disable until 2:30 PM
fry accounts disable work            # Disable indefinitely
fry accounts enable work             # Re-enable account
fry accounts status                  # Show all accounts with quota status
```

#### Sessions
```bash
fry sessions list                    # Show running + saved sessions
fry sessions start                   # Start new session in current TTY
fry sessions start --name project    # Start named session
fry sessions attach 3fa85f64         # Attach to running session
fry sessions save 3fa85f64           # Persist session to disk
fry sessions restore chat.json       # Load saved session
fry sessions kill 3fa85f64           # Terminate session
```

#### Multiplexer
```bash
fry mux up                           # Launch with all accounts
fry mux up work personal             # Launch specific accounts
fry mux ls                           # List active multiplexers
fry mux attach abc123                # Attach to multiplexer
fry mux send abc123 "clear"          # Broadcast command to all panes
fry mux down abc123                  # Close multiplexer
```

#### Configuration
```bash
fry config show                      # Display merged config
fry config show --json               # Machine-readable output
fry config set default_account work  # Update a setting
fry config edit                      # Open in $EDITOR
fry config reset                     # Reset to defaults
```

#### Token Management
```bash
fry tokens inspect personal          # Decode & display token info
fry tokens refresh personal          # Force refresh using refresh_token
fry tokens expire work               # Mark as expired (debug)
```

### Global Options
```bash
--config PATH     # Use alternate config file
--account NAME    # Override default account
--json            # Machine-readable output
--verbose, -v     # Increase log level (repeatable)
--no-color        # Disable ANSI colors
--help            # Show help for any command
--version         # Print version info
```

### Common Workflows
```bash
# Import account from clipboard
pbpaste | fry accounts import --stdin --name work

# Quick session with specific account
fry sessions start --account personal

# Export session transcript
fry sessions start --name interview | tee transcript.md

# Refresh token in CI/scripts
fry tokens refresh personal --json

# Disable account after hitting rate limit
fry accounts disable work --until 5h

# Auto-rotate through available accounts
fry mux up --auto-rotate --skip-disabled
```

---

## ⚙️  Configuration

`fry` resolves settings in the following order:

1. CLI flags  
2. Environment variables (`FRY_*`)  
3. `~/.fry/config.json` (or path provided via `--config`)

```jsonc
{
  "log_level": "info",
  "default_account": "personal",

  "oauth": {
    "token_endpoint": "https://api.anthropic.com/oauth/token",
    "auth_endpoint": "https://console.anthropic.com/oauth/authorize",
    "redirect_uri": "http://localhost:8080/callback",
    "scopes": ["user:inference", "user:profile"]
  },

  "multiplex": {
    "max_instances": 5,
    "default_layout": "grid",
    "auto_rotate_on_quota": true,
    "tui": {
      "enable_mouse": true,
      "color_scheme": "default"
    }
  },

  "quota": {
    "auto_disable_on_limit": true,
    "default_cooldown": "5h",
    "check_interval": "5m"
  }
}
```

### OAuth Requirements

Fry works with Claude Code's OAuth system, which requires:

| Component        | Details                                              |
|------------------|------------------------------------------------------|
| **OAuth Flow**   | Authorization Code flow with PKCE                    |
| **Token Format** | Anthropic OAuth tokens                               |
| **Required Scopes** | `user:inference`, `user:profile`                  |
| **Token Storage** | JSON structure matching Claude's `.credentials.json` |

#### Token Structure
```json
{
  "claudeAiOauth": {
    "accessToken": "sk-ant-oat01-...",
    "refreshToken": "sk-ant-ort01-...",
    "expiresAt": 1753518682371,
    "scopes": ["user:inference", "user:profile"],
    "subscriptionType": "max"
  }
}
```

| Field            | Format                    | Notes                                                |
|------------------|---------------------------|------------------------------------------------------|
| Access Token     | `sk-ant-oat01-…`          | Bearer token for API calls                           |
| Refresh Token    | `sk-ant-ort01-…`          | For refreshing expired access tokens                 |
| Expires At       | Unix timestamp (ms)       | When the access token expires                        |
| Scopes           | Array of strings          | Must include inference and profile                   |
| Subscription     | String                    | Account tier (max, pro, etc.)                        |

Tokens are AES-encrypted and stored under `~/.fry/accounts/`.

**Note**: Fry can import existing Claude Code credentials directly from `~/.claude/.credentials.json`.

---

## 📂 Project Layout

```
fry/
├── src/
│   ├── cli/            # flag parsing & command dispatch
│   ├── core/           # business logic (oauth.c, multiplex.c, session.c)
│   ├── tui/            # ncurses widgets (accounts.c, multiplexer.c …)
│   └── utils/          # logging, colors, memory helpers
├── docs/               # additional documentation
├── test/               # Zig unit tests + C test harness
└── build.zig           # Zig build script
```

---

## 🧪 Developing

* **Debug build** – `zig build`  
* **Cross-compile** – `zig build -Dtarget=x86_64-linux`  
* **Run tests** – `zig build test`

---

## 🤝 Contributing

Bug reports and pull requests are welcome. Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

---

## 📝 License

MIT © 2025 Your Name

---

### Acknowledgements

* [Zig](https://ziglang.org/) – build system & cross-compilation
* [Aro](https://github.com/Vexu/arocc) – lightweight C23 compiler
* [OpenCLI](https://opencli.dev/) – command spec
* [Claude Code](https://claude.ai) – underlying AI platform
