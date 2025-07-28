/*
 * Help text display implementation.
 */

#include "help.h"

#include <stdio.h>
#include <string.h>

#include "core/types.h"
#include "utils/colors.h"
#include "utils/logging.h"

void app_print_concise_help(const char *program_name) {
  if (program_name == nullptr || strlen(program_name) == 0) {
    LOG_ERROR("Invalid program name");
    program_name = APP_NAME;
  }

  printf("%s - Claude Code OAuth account manager [version %s]\n\n", APP_NAME,
         APP_VERSION);

  printf("Usage: %s [options] <noun> <verb> [arguments]\n\n", program_name);
  printf("       %s [options]                 (launches multiplexer by default)\n\n", program_name);

  printf("Common commands:\n");
  printf("  accounts list    List all stored accounts\n");
  printf("  accounts add     Add new account\n");
  printf("  sessions start   Start new Claude session\n");
  printf("  mux up           Launch multiplexer (default when run without args)\n");
  printf("\n");

  printf("Options:\n");
  printf("  -h, --help      Show this help message\n");
  printf("  --version       Show version information\n");
  printf("  -d, --debug     Enable debug output\n");
  printf("  -q, --quiet     Suppress non-essential output\n\n");

  printf("Examples:\n");
  printf("  $ %s                        (launches multiplexer)\n", program_name);
  printf("  $ %s accounts list\n", program_name);
  printf("  $ %s sessions start\n", program_name);
  printf("\n");

  printf("For more options, use %s --help\n", program_name);
}

void app_print_verbose_usage(const char *program_name) {
  if (program_name == nullptr || strlen(program_name) == 0) {
    LOG_ERROR("Invalid program name");
    program_name = APP_NAME;
  }

  const char *bold = app_use_colors(nullptr) ? APP_COLOR_BOLD : "";
  const char *reset = app_use_colors(nullptr) ? APP_COLOR_RESET : "";

  printf("%s%s - A modern CLI application%s\n", bold, APP_NAME, reset);
  printf("Version %s\n\n", APP_VERSION);

  printf("%sUSAGE%s\n", bold, reset);
  printf("  %s [options] <command> [arguments]\n\n", program_name);

  printf("%sDESCRIPTION%s\n", bold, reset);
  printf(
      "  A modern C23 CLI application template with comprehensive tooling.\n");
  printf(
      "  This template provides a solid foundation for building "
      "command-line\n");
  printf("  tools with proper error handling, configuration, and testing.\n\n");

  printf("%sCOMMANDS%s\n", bold, reset);
  printf("  %sAccount Management:%s\n", bold, reset);
  printf("    accounts list      List all stored accounts\n");
  printf(
      "    accounts add       Add new account (--name NAME [--file FILE])\n");
  printf("    accounts use       Set default account\n");
  printf("    accounts rm        Remove account\n");
  printf("    accounts disable   Temporarily disable account\n");
  printf("    accounts enable    Re-enable account\n\n");

  printf("  %sSession Management:%s\n", bold, reset);
  printf("    sessions list      Show running and saved sessions\n");
  printf("    sessions start     Start new session\n");
  printf("    sessions attach    Attach to running session\n");
  printf("    sessions save      Save session to disk\n");
  printf("    sessions restore   Restore saved session\n\n");

  printf("  %sMultiplexer:%s\n", bold, reset);
  printf("    mux up             Launch multiplexer\n");
  printf("    mux ls             List active multiplexers\n");
  printf("    mux attach         Attach to multiplexer\n");
  printf("    mux down           Close multiplexer\n\n");

  printf("  %sConfiguration:%s\n", bold, reset);
  printf("    config show        Display configuration\n");
  printf("    config set         Update a setting\n");
  printf("    config edit        Open config in editor\n");
  printf("    config reset       Reset to defaults\n\n");

  printf("  %sToken Management:%s\n", bold, reset);
  printf("    tokens inspect     Decode and display token info\n");
  printf("    tokens refresh     Force token refresh\n");
  printf("    tokens expire      Mark token as expired\n\n");

  printf("%sOPTIONS%s\n", bold, reset);
  printf("  -h, --help         Show this help message and exit\n");
  printf("  --version          Show version information and exit\n");
  printf("  -d, --debug        Enable debug output (DEBUG level logs)\n");
  printf("  -q, --quiet        Suppress non-essential output\n");
  printf("  -v, --verbose      Enable verbose output\n");
  printf("  --json             Output in JSON format\n");
  printf("  --plain            Output in plain text format\n");
  printf("  --no-color         Disable colored output\n");
  printf("  -c, --config PATH  Specify configuration file path\n\n");

  printf("%sENVIRONMENT%s\n", bold, reset);
  printf(
      "  APP_LOG_LEVEL      Set logging level: ERROR, WARNING, INFO, DEBUG\n");
  printf("                     Default: ERROR\n");
  printf("  NO_COLOR           Disable colored output when set\n\n");

  printf("%sCONFIGURATION%s\n", bold, reset);
  printf("  Configuration can be loaded from:\n");
  printf("  - ~/.config/%s/config.json (user-specific)\n", APP_NAME);
  printf("  - /etc/%s/config.json (system-wide)\n", APP_NAME);
  printf("  - Custom path via --config option\n\n");

  printf("  Configuration precedence (highest to lowest):\n");
  printf("  1. Command-line arguments\n");
  printf("  2. Environment variables\n");
  printf("  3. Configuration file\n");
  printf("  4. Default values\n\n");

  printf("%sEXAMPLES%s\n", bold, reset);
  printf("  Basic greeting:\n");
  printf("    $ %s hello\n", program_name);
  printf("    Hello, World!\n\n");

  printf("  Personalized greeting:\n");
  printf("    $ %s hello Alice\n", program_name);
  printf("    Hello, Alice!\n\n");

  printf("  Echo multiple words:\n");
  printf("    $ %s echo Hello from the CLI\n", program_name);
  printf("    Hello from the CLI\n\n");

  printf("  Show application info:\n");
  printf("    $ %s info\n", program_name);
  printf("    Application: %s\n", APP_NAME);
  printf("    Version: %s\n", APP_VERSION);
  printf("    Build: <timestamp>\n\n");

  printf("  Enable debug logging:\n");
  printf("    $ %s -d info\n", program_name);
  printf("    [DEBUG] Debug mode enabled\n");
  printf("    Application: %s\n", APP_NAME);
  printf("    ...\n\n");

  printf("%sEXIT CODES%s\n", bold, reset);
  printf("  0    Success\n");
  printf("  1    General error\n");
  printf("  2    Invalid command or argument\n");
  printf("  3    Configuration error\n");
  printf("  10   Memory allocation error\n");
  printf("  11   I/O error\n");
  printf("  12   Permission denied\n\n");

  printf("%sAUTHOR%s\n", bold, reset);
  printf("  Written by Sam\n\n");

  printf("%sREPORTING BUGS%s\n", bold, reset);
  printf(
      "  Report bugs to: "
      "https://github.com/sam/yourproject/issues\n\n");

  printf("%sSEE ALSO%s\n", bold, reset);
  printf("  Project homepage: https://github.com/sam/yourproject\n");
  printf("  Documentation: https://github.com/sam/yourproject#readme\n");
}
