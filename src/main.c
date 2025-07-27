/*
 * Generic CLI Application Template
 *
 * A modern C23 CLI application starter using Zig build system and Aro compiler.
 * This template provides a solid foundation for building command-line tools
 * with proper error handling, configuration management, and testing support.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <signal.h>
#include <unistd.h>
#endif

#include "cli/args.h"
#include "cli/commands.h"
#include "cli/help.h"
#include "core/config.h"
#include "core/error.h"
#include "core/oauth.h"
#include "core/types.h"
#include "io/input.h"
#include "io/output.h"
#ifdef ENABLE_TUI
#include "tui/tui.h"
#endif
#include "utils/colors.h"
#include "utils/http.h"
#include "utils/logging.h"
#include "utils/memory.h"

#if __STDC_VERSION__ >= 201112L
static _Thread_local int app_thread_id = 0;
#endif

static app_error initialize_app(int argc, char *argv[], app_config_t **config) {
  // Show help if no arguments
  if (argc == 1) {
    app_print_concise_help(argv[0]);
    exit(0);
  }

  // Create configuration
  app_error err = app_config_create(config);
  if (err != APP_SUCCESS) {
    return err;
  }

  // Load configuration from various sources
  (void)app_config_load_default(*config);  // Load from default locations
  (void)app_config_load_env(*config);

  // Parse command line arguments (may exit for --help or --version)
  err = app_parse_args(argc, argv, *config);
  if (err != APP_SUCCESS) {
    app_config_destroy(*config);
    return err;
  }

  // Set up debug logging if requested
  if (app_config_is_debug(*config)) {
    app_log_set_level(LOG_LEVEL_DEBUG);
    LOG_DEBUG("Debug mode enabled");
  }

  return APP_SUCCESS;
}

static app_error handle_command(app_config_t *config) {
  const char *noun = app_config_get_noun(config);
  const char *verb = app_config_get_verb(config);

  if (!noun) {
    app_print_concise_help(app_config_get_program_name(config));
    return APP_ERROR_INVALID_ARG;
  }

  if (!verb) {
    fprintf(stderr, "Error: Missing verb for noun '%s'\n", noun);
    fprintf(stderr, "Run '%s %s --help' for available commands\n",
            app_config_get_program_name(config), noun);
    return APP_ERROR_INVALID_ARG;
  }

  // Get command arguments
  int cmd_argc = 0;
  char **cmd_argv = app_config_get_command_args(config, &cmd_argc);

  // Dispatch to command handler
  return app_commands_dispatch(noun, verb, cmd_argc, cmd_argv, config);
}

int main(int argc, char *argv[]) {
#if __STDC_VERSION__ >= 201112L
  (void)app_thread_id;  // Suppress unused variable warning
#endif

  // Start performance timing
  struct timespec start_time, end_time;
  clock_gettime(CLOCK_MONOTONIC, &start_time);

  // Initialize logging
  app_log_init();

  // Initialize HTTP client
  app_error err = app_http_init();
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Failed to initialize HTTP client: %s\n",
            app_strerror(err));
    return err;
  }

  // Initialize OAuth system
  err = app_oauth_init();
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Failed to initialize OAuth system: %s\n",
            app_strerror(err));
    app_http_cleanup();
    return err;
  }

  // Initialize command system
  err = app_commands_init();
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Failed to initialize command system: %s\n",
            app_strerror(err));
    app_oauth_cleanup();
    app_http_cleanup();
    return err;
  }

  // Initialize command system
  err = app_commands_init();
  if (err != APP_SUCCESS) {
    fprintf(stderr, "Failed to initialize command system: %s\n",
            app_strerror(err));
    app_oauth_cleanup();
    return err;
  }

  // Initialize configuration
  app_config_t *config = NULL;
  err = initialize_app(argc, argv, &config);
  if (err != APP_SUCCESS) {
    app_commands_cleanup();
    app_oauth_cleanup();
    return err;
  }

  // Handle the command
  err = handle_command(config);

  // Calculate total processing time
  clock_gettime(CLOCK_MONOTONIC, &end_time);
  int64_t elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 +
                       (end_time.tv_nsec - start_time.tv_nsec) / 1000000;

  const char *noun = app_config_get_noun(config);
  const char *verb = app_config_get_verb(config);
  if (noun && verb) {
    LOG_INFO("Command '%s %s' completed in %ld ms with status %d", noun, verb,
             (long)elapsed_ms, err);
  }

  // Cleanup
  app_config_destroy(config);
  app_commands_cleanup();
  app_oauth_cleanup();
  app_http_cleanup();

  return err;
}
