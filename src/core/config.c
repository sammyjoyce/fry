/*
 * Configuration management implementation.
 */

#include "config.h"

#include <limits.h>
#ifndef _WIN32
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <io.h>
#define R_OK 4
#define access _access
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../utils/json.h"
#include "../utils/logging.h"
#include "../utils/memory.h"

#define MAX_COMMAND_ARGS 100

struct app_config {
  char *program_name;
  char *command;
  char *noun;
  char *verb;
  char *command_args[MAX_COMMAND_ARGS];
  int command_arg_count;
  char *config_file;
  char *default_account;
  bool quiet;
  bool debug;
  bool verbose;
  bool json_output;
  bool plain_output;
  bool no_color;
};

app_error app_config_create(app_config_t **config) {
  CHECK_NULL(config, APP_ERROR_INVALID_ARG);

  *config = calloc(1, sizeof(app_config_t));
  if (*config == NULL) {
    return APP_ERROR_MEMORY;
  }

  // Set defaults
  (*config)->quiet = false;
  (*config)->debug = false;
  (*config)->verbose = false;
  (*config)->json_output = false;
  (*config)->plain_output = false;
  (*config)->no_color = false;
  (*config)->command_arg_count = 0;

  return APP_SUCCESS;
}

void app_config_destroy(app_config_t *config) {
  if (config == NULL) {
    return;
  }

  // Free allocated strings
  if (config->program_name) {
    free(config->program_name);
  }
  if (config->command) {
    free(config->command);
  }
  if (config->noun) {
    free(config->noun);
  }
  if (config->verb) {
    free(config->verb);
  }
  if (config->config_file) {
    free(config->config_file);
  }
  if (config->default_account) {
    free(config->default_account);
  }

  // Free command arguments
  for (int i = 0; i < config->command_arg_count; i++) {
    if (config->command_args[i]) {
      free(config->command_args[i]);
    }
  }

  free(config);
}

// Find default config file path
static char *find_config_file(void) {
  static char config_path[PATH_MAX];

  // Check environment variable first
  const char *config_env = getenv("APP_CONFIG_PATH");
  if (config_env && access(config_env, R_OK) == 0) {
    return strdup(config_env);
  }

  // Try user config directory
#ifdef _WIN32
  const char *home = getenv("USERPROFILE");
  if (home) {
    snprintf(config_path, PATH_MAX, "%s\\AppData\\Local\\%s\\config.json", home,
             APP_NAME);
    if (access(config_path, R_OK) == 0) {
      return strdup(config_path);
    }
  }
#else
  const char *home = getenv("HOME");
  if (home) {
    // Check .fry directory first
    snprintf(config_path, PATH_MAX, "%s/.fry/config.json", home);
    if (access(config_path, R_OK) == 0) {
      return strdup(config_path);
    }

    // Then check .config directory
    snprintf(config_path, PATH_MAX, "%s/.config/%s/config.json", home,
             APP_NAME);
    if (access(config_path, R_OK) == 0) {
      return strdup(config_path);
    }
  }
#endif

  // Try system config directory
#ifdef _WIN32
  snprintf(config_path, PATH_MAX, "C:\\ProgramData\\%s\\config.json", APP_NAME);
#else
  snprintf(config_path, PATH_MAX, "/etc/%s/config.json", APP_NAME);
#endif
  if (access(config_path, R_OK) == 0) {
    return strdup(config_path);
  }

  return NULL;
}

app_error app_config_load_file(app_config_t *const config, const char *path) {
  CHECK_NULL(config, APP_ERROR_INVALID_ARG);

  // Use provided path or find default
  char *config_path = NULL;
  if (path) {
    config_path = strdup(path);
  } else {
    config_path = find_config_file();
  }

  if (!config_path) {
    LOG_DEBUG("No configuration file found");
    return APP_SUCCESS;  // Not an error if no config file exists
  }

  // Read file
  FILE *f = fopen(config_path, "r");
  if (!f) {
    LOG_WARNING("Failed to open config file: %s", config_path);
    free(config_path);
    return APP_ERROR_IO;
  }

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *content = app_secure_malloc(size + 1);
  if (!content) {
    fclose(f);
    free(config_path);
    return APP_ERROR_MEMORY;
  }

  if (fread(content, 1, size, f) != (size_t)size) {
    app_secure_free(content, size + 1);
    fclose(f);
    free(config_path);
    return APP_ERROR_IO;
  }
  content[size] = '\0';
  fclose(f);

  // Parse JSON
  json_value_t *root;
  app_error err = json_parse(&root, content);
  app_secure_free(content, size + 1);

  if (err != APP_SUCCESS) {
    LOG_ERROR("Failed to parse config file: %s", config_path);
    free(config_path);
    return err;
  }

  // Extract configuration values
  if (root->type == JSON_TYPE_OBJECT) {
    json_value_t *log_level = json_object_get(root->object_val, "log_level");
    if (log_level && log_level->type == JSON_TYPE_STRING) {
      if (strcmp(log_level->string_val, "debug") == 0) {
        config->debug = true;
      }
    }

    json_value_t *default_account =
        json_object_get(root->object_val, "default_account");
    if (default_account && default_account->type == JSON_TYPE_STRING) {
      app_config_set_default_account(config, default_account->string_val);
    }
  }

  json_value_destroy(root);
  LOG_INFO("Loaded configuration from %s", config_path);
  free(config_path);
  return APP_SUCCESS;
}

app_error app_config_load_env(app_config_t *config) {
  CHECK_NULL(config, APP_ERROR_INVALID_ARG);

  // Check NO_COLOR environment variable
  if (getenv("NO_COLOR")) {
    config->no_color = true;
  }

  // Check APP_LOG_LEVEL
  const char *log_level = getenv("APP_LOG_LEVEL");
  if (log_level) {
    if (strcmp(log_level, "DEBUG") == 0) {
      config->debug = true;
    }
  }

  return APP_SUCCESS;
}

app_error app_config_load_args(app_config_t *config, int argc, char *argv[]) {
  // This is handled by args.c
  (void)config;
  (void)argc;
  (void)argv;
  return APP_SUCCESS;
}

// Getters
const char *app_config_get_program_name(const app_config_t *config) {
  return config ? config->program_name : APP_NAME;
}

const char *app_config_get_command(const app_config_t *config) {
  return config ? config->command : NULL;
}

char **app_config_get_command_args(const app_config_t *config, int *count) {
  if (!config || !count) {
    return NULL;
  }
  *count = config->command_arg_count;
  return (char **)config->command_args;
}

const char *app_config_get_config_file(const app_config_t *config) {
  return config ? config->config_file : NULL;
}

bool app_config_is_quiet(const app_config_t *config) {
  return config ? config->quiet : false;
}

bool app_config_is_debug(const app_config_t *config) {
  return config ? config->debug : false;
}

bool app_config_is_json_output(const app_config_t *config) {
  return config ? config->json_output : false;
}

bool app_config_is_plain_output(const app_config_t *config) {
  return config ? config->plain_output : false;
}

bool app_config_is_no_color(const app_config_t *config) {
  return config ? config->no_color : false;
}

bool app_config_is_verbose(const app_config_t *config) {
  return config ? config->verbose : false;
}

// Setters
void app_config_set_debug(app_config_t *config, bool debug) {
  if (config) {
    config->debug = debug;
  }
}

void app_config_set_quiet(app_config_t *config, bool quiet) {
  if (config) {
    config->quiet = quiet;
  }
}

void app_config_set_verbose(app_config_t *config, bool verbose) {
  if (config) {
    config->verbose = verbose;
  }
}

void app_config_set_json_output(app_config_t *config, bool json) {
  if (config) {
    config->json_output = json;
  }
}

void app_config_set_plain_output(app_config_t *config, bool plain) {
  if (config) {
    config->plain_output = plain;
  }
}

void app_config_set_no_color(app_config_t *config, bool no_color) {
  if (config) {
    config->no_color = no_color;
  }
}

void app_config_set_program_name(app_config_t *config, const char *name) {
  if (config && name) {
    if (config->program_name) {
      free(config->program_name);
    }
    config->program_name = strdup(name);
  }
}

void app_config_set_command(app_config_t *config, const char *command) {
  if (config && command) {
    if (config->command) {
      free(config->command);
    }
    config->command = strdup(command);
  }
}

void app_config_add_command_arg(app_config_t *config, const char *arg) {
  if (config && arg && config->command_arg_count < MAX_COMMAND_ARGS) {
    config->command_args[config->command_arg_count++] = strdup(arg);
  }
}

void app_config_set_config_file(app_config_t *config, const char *path) {
  if (config && path) {
    if (config->config_file) {
      free(config->config_file);
    }
    config->config_file = strdup(path);
  }
}

// Noun/verb getters and setters
const char *app_config_get_noun(const app_config_t *config) {
  return config ? config->noun : NULL;
}

const char *app_config_get_verb(const app_config_t *config) {
  return config ? config->verb : NULL;
}

void app_config_set_noun(app_config_t *config, const char *noun) {
  if (config && noun) {
    if (config->noun) {
      free(config->noun);
    }
    config->noun = strdup(noun);
  }
}

void app_config_set_verb(app_config_t *config, const char *verb) {
  if (config && verb) {
    if (config->verb) {
      free(config->verb);
    }
    config->verb = strdup(verb);
  }
}

// OAuth account support
const char *app_config_get_default_account(const app_config_t *config) {
  return config ? config->default_account : NULL;
}

void app_config_set_default_account(app_config_t *config, const char *account) {
  if (config && account) {
    if (config->default_account) {
      free(config->default_account);
    }
    config->default_account = strdup(account);
  }
}

// Configuration persistence
app_error app_config_save(const app_config_t *config) {
  if (!config) {
    return APP_ERROR_INVALID_ARG;
  }

  // Get config file path
  char *config_path = find_config_file();
  if (!config_path) {
    // Create default path
    char *home = getenv("HOME");
    if (!home) {
      return APP_ERROR_ENV;
    }

    static char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/.fry/config.json", home);
    config_path = path;

    // Ensure directory exists
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s/.fry", home);
    mkdir(dir, 0700);
  }

  // Build JSON object
  json_value_t *root = json_value_object();
  json_object_t *obj = root->object_val;

  // Add basic settings
  json_object_set(obj, "log_level",
                  json_value_string(config->debug ? "debug" : "info"));
  json_object_set(obj, "default_account",
                  json_value_string(
                      config->default_account ? config->default_account : ""));

  // Add OAuth settings
  json_value_t *oauth = json_value_object();
  json_object_set(
      oauth->object_val, "token_endpoint",
      json_value_string("https://console.anthropic.com/v1/oauth/token"));
  json_object_set(
      oauth->object_val, "auth_endpoint",
      json_value_string("https://console.anthropic.com/oauth/authorize"));
  json_object_set(
      oauth->object_val, "redirect_uri",
      json_value_string("https://console.anthropic.com/oauth/code/callback"));
  json_object_set(obj, "oauth", oauth);

  // Add multiplex settings
  json_value_t *multiplex = json_value_object();
  json_object_set(multiplex->object_val, "max_instances", json_value_number(5));
  json_object_set(multiplex->object_val, "default_layout",
                  json_value_string("grid"));
  json_object_set(multiplex->object_val, "auto_rotate_on_quota",
                  json_value_bool(true));
  json_object_set(obj, "multiplex", multiplex);

  // Serialize to JSON
  char *json_str;
  app_error err = json_stringify_pretty(&json_str, root);
  json_value_destroy(root);

  if (err != APP_SUCCESS) {
    return err;
  }

  // Write to file
  FILE *fp = fopen(config_path, "w");
  if (!fp) {
    app_free(json_str);
    return APP_ERROR_IO;
  }

  fprintf(fp, "%s\n", json_str);
  fclose(fp);
  app_free(json_str);

  return APP_SUCCESS;
}

app_error app_config_load_default(app_config_t *config) {
  if (!config) {
    return APP_ERROR_INVALID_ARG;
  }

  char *config_path = find_config_file();
  if (!config_path) {
    // No config file found, use defaults
    return APP_SUCCESS;
  }

  return app_config_load_file(config, config_path);
}
