#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/config.h"
#include "../core/error.h"
#include "../core/types.h"
#include "../utils/logging.h"
#include "../utils/memory.h"
#include "commands.h"

#ifdef ENABLE_TUI
#include "../core/session.h"
#include "../tui/tui_mux.h"

// Launch multiplexer
app_error cmd_mux_up(int argc, char **argv, app_config_t *config) {
  (void)config;  // TODO: Use config for defaults

  LOG_INFO("Launching multiplexer...");

  // Parse options
  bool auto_rotate = false;
  bool skip_disabled = false;
  const char *layout_str = "grid";
  char **accounts = NULL;
  int account_count = 0;

  // Simple option parsing
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--auto-rotate") == 0) {
      auto_rotate = true;
    } else if (strcmp(argv[i], "--skip-disabled") == 0) {
      skip_disabled = true;
    } else if (strcmp(argv[i], "--layout") == 0 && i + 1 < argc) {
      layout_str = argv[++i];
    } else if (argv[i][0] != '-') {
      // Account name
      if (account_count == 0) {
        accounts = &argv[i];
      }
      account_count++;
    }
  }

  // Determine layout mode
  app_tui_layout_mode_t layout_mode = TUI_LAYOUT_GRID;
  if (strcmp(layout_str, "vsplit") == 0) {
    layout_mode = TUI_LAYOUT_VSPLIT;
  } else if (strcmp(layout_str, "hsplit") == 0) {
    layout_mode = TUI_LAYOUT_HSPLIT;
  }

  // Create multiplexer configuration
  app_tui_mux_config_t mux_config = {.enable_mouse = true,
                                     .enable_colors = true,
                                     .default_layout = layout_mode,
                                     .max_panes = 9,
                                     .refresh_rate_ms = 50,
                                     .min_pane_width = 20,
                                     .min_pane_height = 5};

  // Create session manager
  app_session_manager_t *session_manager = NULL;
  app_error err = app_session_manager_create(&session_manager);
  if (err != APP_SUCCESS) {
    LOG_ERROR("Failed to create session manager: %s", app_error_string(err));
    return err;
  }

  // Create and run multiplexer
  app_tui_mux_t *mux = NULL;
  err = app_tui_mux_create(&mux, &mux_config, session_manager);
  if (err != APP_SUCCESS) {
    LOG_ERROR("Failed to create multiplexer: %s", app_error_string(err));
    app_session_manager_destroy(session_manager);
    return err;
  }

  // Initialize TUI
  err = app_tui_mux_init(mux);
  if (err != APP_SUCCESS) {
    LOG_ERROR("Failed to initialize multiplexer: %s", app_error_string(err));
    app_tui_mux_destroy(mux);
    app_session_manager_destroy(session_manager);
    return err;
  }

  // Add initial panes
  if (account_count > 0) {
    // Add specified accounts
    for (int i = 0; i < account_count && i < 9; i++) {
      app_tui_pane_t *pane = NULL;
      err = app_tui_mux_add_pane(mux, accounts[i], &pane);
      if (err != APP_SUCCESS) {
        LOG_WARNING("Failed to add pane for account '%s': %s", accounts[i],
                    app_error_string(err));
      }
    }
  } else {
    // Add default account
    const char *default_account = app_config_get_default_account(config);
    if (!default_account) {
      LOG_INFO("No default account configured, using first available");
    }

    app_tui_pane_t *pane = NULL;
    err = app_tui_mux_add_pane(mux, default_account, &pane);
    if (err != APP_SUCCESS) {
      LOG_ERROR("Failed to add default pane: %s", app_error_string(err));
      app_tui_mux_shutdown(mux);
      app_tui_mux_destroy(mux);
      app_session_manager_destroy(session_manager);
      return err;
    }
  }

  // Run the multiplexer with enhanced event loop
  LOG_INFO("Starting multiplexer main loop...");
  err = app_tui_mux_run_enhanced(mux);

  // Cleanup
  LOG_INFO("Shutting down multiplexer...");
  app_tui_mux_shutdown(mux);
  app_tui_mux_destroy(mux);
  app_session_manager_destroy(session_manager);

  return err;
}

// List multiplexers
app_error cmd_mux_ls(int argc, char **argv, app_config_t *config) {
  (void)argc;
  (void)argv;
  (void)config;

  LOG_INFO("Listing active multiplexers...");

  // TODO: Implement multiplexer tracking
  printf("No active multiplexers\n");
  return APP_SUCCESS;
}

// Attach to multiplexer
app_error cmd_mux_attach(int argc, char **argv, app_config_t *config) {
  (void)config;

  if (argc < 1) {
    fprintf(stderr, "Usage: fry mux attach <mux-id>\n");
    return APP_ERROR_USAGE;
  }

  const char *mux_id = argv[0];
  LOG_INFO("Attaching to multiplexer %s...", mux_id);

  // TODO: Implement multiplexer attachment
  fprintf(stderr, "Multiplexer attachment not yet implemented\n");
  return APP_ERROR_NOT_IMPLEMENTED;
}

// Send command to multiplexer
app_error cmd_mux_send(int argc, char **argv, app_config_t *config) {
  (void)config;

  if (argc < 2) {
    fprintf(stderr, "Usage: fry mux send <mux-id> <command>\n");
    return APP_ERROR_USAGE;
  }

  const char *mux_id = argv[0];
  const char *command = argv[1];

  LOG_INFO("Sending command to multiplexer %s: %s", mux_id, command);

  // TODO: Implement command sending
  fprintf(stderr, "Command sending not yet implemented\n");
  return APP_ERROR_NOT_IMPLEMENTED;
}

// Shutdown multiplexer
app_error cmd_mux_down(int argc, char **argv, app_config_t *config) {
  (void)config;

  const char *mux_id = argc > 0 ? argv[0] : NULL;

  if (mux_id) {
    LOG_INFO("Shutting down multiplexer %s...", mux_id);
  } else {
    LOG_INFO("Shutting down all multiplexers...");
  }

  // TODO: Implement multiplexer tracking and shutdown
  fprintf(stderr, "Multiplexer shutdown not yet implemented\n");
  return APP_ERROR_NOT_IMPLEMENTED;
}

#else  // !ENABLE_TUI

// Stub implementations when TUI is disabled
app_error cmd_mux_up(int argc, char **argv, app_config_t *config) {
  (void)argc;
  (void)argv;
  (void)config;

  fprintf(stderr, "Error: Multiplexer support not compiled in.\n");
  fprintf(stderr, "Rebuild with -Denable-tui=true to enable TUI support.\n");
  return APP_ERROR_NOT_IMPLEMENTED;
}

app_error cmd_mux_ls(int argc, char **argv, app_config_t *config) {
  (void)argc;
  (void)argv;
  (void)config;

  fprintf(stderr, "Error: Multiplexer support not compiled in.\n");
  fprintf(stderr, "Rebuild with -Denable-tui=true to enable TUI support.\n");
  return APP_ERROR_NOT_IMPLEMENTED;
}

app_error cmd_mux_attach(int argc, char **argv, app_config_t *config) {
  (void)argc;
  (void)argv;
  (void)config;

  fprintf(stderr, "Error: Multiplexer support not compiled in.\n");
  fprintf(stderr, "Rebuild with -Denable-tui=true to enable TUI support.\n");
  return APP_ERROR_NOT_IMPLEMENTED;
}

app_error cmd_mux_send(int argc, char **argv, app_config_t *config) {
  (void)argc;
  (void)argv;
  (void)config;

  fprintf(stderr, "Error: Multiplexer support not compiled in.\n");
  fprintf(stderr, "Rebuild with -Denable-tui=true to enable TUI support.\n");
  return APP_ERROR_NOT_IMPLEMENTED;
}

app_error cmd_mux_down(int argc, char **argv, app_config_t *config) {
  (void)argc;
  (void)argv;
  (void)config;

  fprintf(stderr, "Error: Multiplexer support not compiled in.\n");
  fprintf(stderr, "Rebuild with -Denable-tui=true to enable TUI support.\n");
  return APP_ERROR_NOT_IMPLEMENTED;
}

#endif  // ENABLE_TUI