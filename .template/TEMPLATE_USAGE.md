# Template Usage Guide

This document explains how the template variable replacement system works and how to use and customize it.

## Quick Start

### 1. Create Your Repository

**GitHub Web Interface:**
1. Click "Use this template" button
2. Name your repository
3. Click "Create repository from template"

**GitHub CLI:**
```bash
gh repo create my-cli-app --template sammyjoyce/c23-cli-template --clone
cd my-cli-app
```

### 2. Automatic Setup

GitHub Actions will automatically:
- Replace all template variables with your repository info
- Remove template-specific files
- Update the README
- Commit the changes

Check the "Actions" tab to monitor progress.

### 3. Start Development

```bash
# Install Zig (if needed)
curl -sSL https://raw.githubusercontent.com/tristanisham/zvm/master/install.sh | bash
zvm install master

# Build your application
zig build

# Run tests
zig build test

# Run your app
./zig-out/bin/your-app --help
```

## Template Variable System

The template uses a centralized configuration file (`template-vars.json`) to manage all template variables and their replacements.

### Available Variables

| Variable | Description | Example |
|----------|-------------|---------|
| `PROJECT_NAME` | Your project/application name | `my-awesome-cli` |
| `PROJECT_NAME_SNAKE` | Project name in snake_case | `my_awesome_cli` |
| `PROJECT_NAME_KEBAB` | Project name in kebab-case | `my-awesome-cli` |
| `PROJECT_NAME_PASCAL` | Project name in PascalCase | `MyAwesomeCli` |
| `GITHUB_USERNAME` | Your GitHub username/organization | `johndoe` |
| `AUTHOR_NAME` | Your full name | `John Doe` |
| `AUTHOR_EMAIL` | Your email address | `john@example.com` |
| `PROJECT_DESCRIPTION` | Short project description | `A powerful CLI tool` |
| `CURRENT_YEAR` | Current year for copyright | `2025` |
| `LICENSE_TYPE` | License type | `MIT` |

### How It Works

1. **Automatic Detection**: The system automatically detects values from:
   - GitHub repository information
   - Git configuration
   - Environment variables

2. **Smart Transformations**: Variables are automatically transformed to different cases as needed

3. **Validation**: All values are validated against regex patterns to ensure correctness

4. **Fallbacks**: If a value can't be detected, sensible defaults are used

## Manual Setup

If the GitHub Action doesn't run or you're not using GitHub:

### Interactive Mode

```bash
# Run the cleanup script interactively
chmod +x .template/cleanup-template.sh
.template/cleanup-template.sh
```

This will prompt you for all necessary values.

### Dry Run Mode

To see what would be replaced without making changes:

```bash
.template/template-replacer.sh --dry-run
```

### Direct Execution

```bash
# Run the replacer directly (uses automatic detection)
.template/template-replacer.sh
```

## Advanced Usage

### Customizing Template Variables

Edit `.template/template-vars.json` to:

1. **Add new variables**:
```json
{
  "variables": {
    "MY_CUSTOM_VAR": {
      "placeholders": ["PLACEHOLDER_TEXT"],
      "description": "Description of your variable",
      "source": "repository_name",
      "validation": "^[a-zA-Z0-9-]+$"
    }
  }
}
```

2. **Modify existing variables**:
   - Change placeholders
   - Update validation patterns
   - Add transformations

### Variable Sources

- `repository_name`: From Git or GitHub
- `repository_owner`: GitHub username/org
- `git_user_name`: From git config
- `git_user_email`: From git config
- `current_year`: Current year
- `static`: Fixed value in config

### Transformations

Apply automatic case transformations:

```json
{
  "transform": "snake_case"  // or kebab_case, pascal_case, camel_case
}
```

### Special Replacements

For complex replacements in specific files:

```json
{
  "special_replacements": {
    "build.zig.zon": {
      ".cli_starter": ".${PROJECT_NAME_SNAKE}"
    }
  }
}
```

## Project Structure

```
├── src/           # Source code
│   ├── cli/       # Command-line interface
│   ├── core/      # Core functionality
│   ├── io/        # Input/output handling
│   ├── tui/       # Terminal UI (optional)
│   └── utils/     # Utilities
├── test/          # Tests
├── docs/          # Documentation
├── examples/      # Example code
└── .template/     # Template system files
    ├── template-vars.json      # Variable configuration
    ├── template-replacer.sh    # Replacement script
    └── cleanup-template.sh     # Interactive cleanup
```

## Troubleshooting

### Common Issues

1. **jq not installed**: 
   ```bash
   # Ubuntu/Debian
   sudo apt-get install jq
   
   # macOS
   brew install jq
   
   # Fedora
   sudo dnf install jq
   ```

2. **Build fails?**
   - Ensure Zig master is installed: `zvm install master`
   - Check for ncurses: `apt install libncurses-dev` or `brew install ncurses`

3. **Template cleanup didn't run?**
   - Check Actions tab for errors
   - Run manually: `.template/cleanup-template.sh`

4. **Validation failures**: Ensure your values match expected patterns (e.g., no spaces in project names)

### Debug Mode

For detailed output:

```bash
bash -x ./.template/template-replacer.sh
```

## Best Practices

1. **Test changes**: Use `--dry-run` before actual replacement
2. **Document variables**: Update template-vars.json comments
3. **Validate inputs**: Add appropriate regex patterns
4. **Keep it simple**: Don't over-engineer replacements

## Need Help?

- **Template issues**: [Create an issue](https://github.com/sammyjoyce/c23-cli-template/issues)
- **Your project issues**: Use your own repository's issues
- **Documentation**: Check the template repository wiki