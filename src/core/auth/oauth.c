#include "oauth.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "keychain.h"
#include "utils/http.h"
#include "utils/json.h"
#include "utils/logging.h"
#include "utils/memory.h"
#include "config.h"

#define OAUTH_ACCOUNTS_DIR ".fry/accounts"
#define OAUTH_KEY_FILE ".fry/.key"
#define OAUTH_IV_SIZE 16
#define OAUTH_KEY_SIZE 32
#define OAUTH_TAG_SIZE 16
#define OAUTH_CLIENT_ID "9d1c250a-e61b-44d9-88ed-5944d1962f5e"
#define OAUTH_REDIRECT_URI "https://console.anthropic.com/oauth/code/callback"
#define OAUTH_SCOPES "org:create_api_key user:profile user:inference"

static char *g_accounts_dir = NULL;
static unsigned char g_encryption_key[OAUTH_KEY_SIZE];
static bool g_initialized = false;

static app_error ensure_directory_exists(const char *path) {
  struct stat st = {0};
  if (stat(path, &st) == -1) {
    if (mkdir(path, 0700) != 0) {
      LOG_ERROR("Failed to create directory %s: %s", path, strerror(errno));
      return APP_ERROR_IO;
    }
  }
  return APP_SUCCESS;
}

static app_error load_or_generate_key(void) {
  char *home = getenv("HOME");
  if (!home) {
    LOG_ERROR("HOME environment variable not set");
    return APP_ERROR_ENV;
  }

  char key_path[PATH_MAX];
  snprintf(key_path, sizeof(key_path), "%s/%s", home, OAUTH_KEY_FILE);

  // Try to load existing key
  int fd = open(key_path, O_RDONLY);
  if (fd >= 0) {
    ssize_t bytes_read = read(fd, g_encryption_key, OAUTH_KEY_SIZE);
    close(fd);
    if (bytes_read == OAUTH_KEY_SIZE) {
      return APP_SUCCESS;
    }
  }

  // Generate new key
  if (RAND_bytes(g_encryption_key, OAUTH_KEY_SIZE) != 1) {
    LOG_ERROR("Failed to generate encryption key");
    return APP_ERROR_CRYPTO;
  }

  // Save key
  fd = open(key_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0) {
    LOG_ERROR("Failed to create key file: %s", strerror(errno));
    return APP_ERROR_IO;
  }

  ssize_t bytes_written = write(fd, g_encryption_key, OAUTH_KEY_SIZE);
  close(fd);

  if (bytes_written != OAUTH_KEY_SIZE) {
    LOG_ERROR("Failed to write encryption key");
    return APP_ERROR_IO;
  }

  return APP_SUCCESS;
}

app_error app_oauth_init(void) {
  if (g_initialized) {
    return APP_SUCCESS;
  }

  char *home = getenv("HOME");
  if (!home) {
    LOG_ERROR("HOME environment variable not set");
    return APP_ERROR_ENV;
  }

  // Setup accounts directory
  char fry_dir[PATH_MAX];
  snprintf(fry_dir, sizeof(fry_dir), "%s/.fry", home);
  app_error err = ensure_directory_exists(fry_dir);
  if (err != APP_SUCCESS) {
    return err;
  }

  g_accounts_dir = app_malloc(PATH_MAX);
  snprintf(g_accounts_dir, PATH_MAX, "%s/%s", home, OAUTH_ACCOUNTS_DIR);
  err = ensure_directory_exists(g_accounts_dir);
  if (err != APP_SUCCESS) {
    app_free(g_accounts_dir);
    g_accounts_dir = NULL;
    return err;
  }

  // Load or generate encryption key
  err = load_or_generate_key();
  if (err != APP_SUCCESS) {
    app_free(g_accounts_dir);
    g_accounts_dir = NULL;
    return err;
  }

  g_initialized = true;
  return APP_SUCCESS;
}

void app_oauth_cleanup(void) {
  if (g_accounts_dir) {
    app_free(g_accounts_dir);
    g_accounts_dir = NULL;
  }
  g_initialized = false;
}

const char *app_oauth_get_accounts_dir(void) {
  return g_accounts_dir;
}

app_error app_oauth_account_create(app_oauth_account_t **account,
                                   const char *name) {
  if (!account || !name) {
    return APP_ERROR_INVALID_PARAM;
  }

  *account = app_malloc(sizeof(app_oauth_account_t));
  (*account)->name = strdup(name);
  (*account)->is_default = false;
  (*account)->is_disabled = false;
  (*account)->disabled_until = 0;

  // Generate file path
  char file_name[256];
  snprintf(file_name, sizeof(file_name), "%s.enc", name);

  char file_path[PATH_MAX];
  snprintf(file_path, sizeof(file_path), "%s/%s", g_accounts_dir, file_name);
  (*account)->file_path = strdup(file_path);

  // Initialize empty token
  memset(&(*account)->token, 0, sizeof(app_oauth_token_t));

  return APP_SUCCESS;
}

void app_oauth_account_destroy(app_oauth_account_t *account) {
  if (!account)
    return;

  app_free(account->name);
  app_free(account->file_path);

  // Free token fields
  app_free(account->token.access_token);
  app_free(account->token.refresh_token);
  app_free(account->token.subscription_type);

  if (account->token.scopes) {
    for (size_t i = 0; i < account->token.scopes_count; i++) {
      app_free(account->token.scopes[i]);
    }
    app_free(account->token.scopes);
  }

  app_free(account);
}

app_error app_oauth_token_encrypt(char **encrypted_data, size_t *data_len,
                                  const app_oauth_token_t *token) {
  if (!encrypted_data || !data_len || !token) {
    return APP_ERROR_INVALID_PARAM;
  }

  // Serialize token to JSON (matching opencode format)
  char json_buffer[4096];
  int len = snprintf(json_buffer, sizeof(json_buffer),
                     "{"
                     "\"type\":\"oauth\","
                     "\"access\":\"%s\","
                     "\"refresh\":\"%s\","
                     "\"expires\":%lld",
                     token->access_token ? token->access_token : "",
                     token->refresh_token ? token->refresh_token : "",
                     (long long)token->expires_at);
  // Close JSON object
  len += snprintf(json_buffer + len, sizeof(json_buffer) - len, "}");

  // Encrypt using AES-256-GCM
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    return APP_ERROR_CRYPTO;
  }

  unsigned char iv[OAUTH_IV_SIZE];
  if (RAND_bytes(iv, OAUTH_IV_SIZE) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return APP_ERROR_CRYPTO;
  }

  if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, g_encryption_key, iv) !=
      1) {
    EVP_CIPHER_CTX_free(ctx);
    return APP_ERROR_CRYPTO;
  }

  // Allocate output buffer: IV + ciphertext + tag
  size_t plaintext_len = strlen(json_buffer);
  *data_len = OAUTH_IV_SIZE + plaintext_len + OAUTH_TAG_SIZE;
  *encrypted_data = app_malloc(*data_len);

  // Copy IV to output
  memcpy(*encrypted_data, iv, OAUTH_IV_SIZE);

  // Encrypt
  int out_len;
  if (EVP_EncryptUpdate(ctx, (unsigned char *)(*encrypted_data + OAUTH_IV_SIZE),
                        &out_len, (unsigned char *)json_buffer,
                        plaintext_len) != 1) {
    app_free(*encrypted_data);
    EVP_CIPHER_CTX_free(ctx);
    return APP_ERROR_CRYPTO;
  }

  int final_len;
  if (EVP_EncryptFinal_ex(
          ctx, (unsigned char *)(*encrypted_data + OAUTH_IV_SIZE + out_len),
          &final_len) != 1) {
    app_free(*encrypted_data);
    EVP_CIPHER_CTX_free(ctx);
    return APP_ERROR_CRYPTO;
  }

  // Get tag
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, OAUTH_TAG_SIZE,
                          *encrypted_data + OAUTH_IV_SIZE + plaintext_len) !=
      1) {
    app_free(*encrypted_data);
    EVP_CIPHER_CTX_free(ctx);
    return APP_ERROR_CRYPTO;
  }

  EVP_CIPHER_CTX_free(ctx);
  return APP_SUCCESS;
}

app_error app_oauth_token_decrypt(app_oauth_token_t **token,
                                  const char *encrypted_data, size_t data_len) {
  if (!token || !encrypted_data || data_len < OAUTH_IV_SIZE + OAUTH_TAG_SIZE) {
    return APP_ERROR_INVALID_PARAM;
  }

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    return APP_ERROR_CRYPTO;
  }

  // Extract IV
  unsigned char iv[OAUTH_IV_SIZE];
  memcpy(iv, encrypted_data, OAUTH_IV_SIZE);

  if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, g_encryption_key, iv) !=
      1) {
    EVP_CIPHER_CTX_free(ctx);
    return APP_ERROR_CRYPTO;
  }

  // Set tag
  size_t ciphertext_len = data_len - OAUTH_IV_SIZE - OAUTH_TAG_SIZE;
  if (EVP_CIPHER_CTX_ctrl(
          ctx, EVP_CTRL_GCM_SET_TAG, OAUTH_TAG_SIZE,
          (void *)(encrypted_data + OAUTH_IV_SIZE + ciphertext_len)) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return APP_ERROR_CRYPTO;
  }

  // Decrypt
  char *plaintext = app_malloc(ciphertext_len + 1);
  int out_len;
  if (EVP_DecryptUpdate(ctx, (unsigned char *)plaintext, &out_len,
                        (unsigned char *)(encrypted_data + OAUTH_IV_SIZE),
                        ciphertext_len) != 1) {
    app_free(plaintext);
    EVP_CIPHER_CTX_free(ctx);
    return APP_ERROR_CRYPTO;
  }

  int final_len;
  if (EVP_DecryptFinal_ex(ctx, (unsigned char *)(plaintext + out_len),
                          &final_len) != 1) {
    app_free(plaintext);
    EVP_CIPHER_CTX_free(ctx);
    return APP_ERROR_CRYPTO;
  }

  plaintext[out_len + final_len] = '\0';
  EVP_CIPHER_CTX_free(ctx);

  // Parse JSON
  json_value_t *root;
  app_error parse_err = json_parse(&root, plaintext);
  app_free(plaintext);

  if (parse_err != APP_SUCCESS) {
    return parse_err;
  }

  *token = app_malloc(sizeof(app_oauth_token_t));
  memset(*token, 0, sizeof(app_oauth_token_t));

  // Extract token fields - support both formats
  if (root->type == JSON_TYPE_OBJECT) {
    // Check for opencode format first
    json_value_t *type = json_object_get(root->object_val, "type");
    if (type && type->type == JSON_TYPE_STRING &&
        strcmp(type->string_val, "oauth") == 0) {
      // OpenCode format
      json_value_t *access = json_object_get(root->object_val, "access");
      if (access && access->type == JSON_TYPE_STRING) {
        (*token)->access_token = app_secure_strdup(access->string_val);
      }

      json_value_t *refresh = json_object_get(root->object_val, "refresh");
      if (refresh && refresh->type == JSON_TYPE_STRING) {
        (*token)->refresh_token = app_secure_strdup(refresh->string_val);
      }

      json_value_t *expires = json_object_get(root->object_val, "expires");
      if (expires && expires->type == JSON_TYPE_NUMBER) {
        (*token)->expires_at = (int64_t)expires->number_val;
      }

      // Set default scopes for opencode format
      (*token)->scopes_count = 3;
      (*token)->scopes = app_malloc(sizeof(char *) * 3);
      (*token)->scopes[0] = strdup("org:create_api_key");
      (*token)->scopes[1] = strdup("user:profile");
      (*token)->scopes[2] = strdup("user:inference");
    } else {
      // Legacy Claude format
      json_value_t *oauth = json_object_get(root->object_val, "claudeAiOauth");
      if (oauth && oauth->type == JSON_TYPE_OBJECT) {
        json_value_t *access =
            json_object_get(oauth->object_val, "accessToken");
        if (access && access->type == JSON_TYPE_STRING) {
          (*token)->access_token = app_secure_strdup(access->string_val);
        }

        json_value_t *refresh =
            json_object_get(oauth->object_val, "refreshToken");
        if (refresh && refresh->type == JSON_TYPE_STRING) {
          (*token)->refresh_token = app_secure_strdup(refresh->string_val);
        }

        json_value_t *expires = json_object_get(oauth->object_val, "expiresAt");
        if (expires && expires->type == JSON_TYPE_NUMBER) {
          (*token)->expires_at = (int64_t)expires->number_val;
        }

        json_value_t *sub_type =
            json_object_get(oauth->object_val, "subscriptionType");
        if (sub_type && sub_type->type == JSON_TYPE_STRING) {
          (*token)->subscription_type = strdup(sub_type->string_val);
        }

        json_value_t *scopes = json_object_get(oauth->object_val, "scopes");
        if (scopes && scopes->type == JSON_TYPE_ARRAY) {
          (*token)->scopes_count = scopes->array_val->count;
          (*token)->scopes =
              app_malloc(sizeof(char *) * (*token)->scopes_count);
          for (size_t i = 0; i < (*token)->scopes_count; i++) {
            json_value_t *scope = scopes->array_val->items[i];
            if (scope && scope->type == JSON_TYPE_STRING) {
              (*token)->scopes[i] = strdup(scope->string_val);
            }
          }
        }
      }
    }
  }

  json_value_destroy(root);
  return APP_SUCCESS;
}

app_error app_oauth_account_save(const app_oauth_account_t *account) {
  if (!account || !account->file_path) {
    return APP_ERROR_INVALID_PARAM;
  }

  char *encrypted_data;
  size_t data_len;
  app_error err =
      app_oauth_token_encrypt(&encrypted_data, &data_len, &account->token);
  if (err != APP_SUCCESS) {
    return err;
  }

  // Write to file
  int fd = open(account->file_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0) {
    LOG_ERROR("Failed to open account file: %s", strerror(errno));
    app_free(encrypted_data);
    return APP_ERROR_IO;
  }

  ssize_t written = write(fd, encrypted_data, data_len);
  close(fd);
  app_free(encrypted_data);

  if (written != (ssize_t)data_len) {
    LOG_ERROR("Failed to write account data");
    return APP_ERROR_IO;
  }

  return APP_SUCCESS;
}

app_error app_oauth_account_load(app_oauth_account_t **account,
                                 const char *file_path) {
  if (!account || !file_path) {
    return APP_ERROR_INVALID_PARAM;
  }

  // Read encrypted data
  int fd = open(file_path, O_RDONLY);
  if (fd < 0) {
    LOG_ERROR("Failed to open account file: %s", strerror(errno));
    return APP_ERROR_IO;
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    close(fd);
    return APP_ERROR_IO;
  }

  char *encrypted_data = app_malloc(st.st_size);
  ssize_t bytes_read = read(fd, encrypted_data, st.st_size);
  close(fd);

  if (bytes_read != st.st_size) {
    app_free(encrypted_data);
    return APP_ERROR_IO;
  }

  // Decrypt token
  app_oauth_token_t *token;
  app_error err = app_oauth_token_decrypt(&token, encrypted_data, st.st_size);
  app_free(encrypted_data);

  if (err != APP_SUCCESS) {
    return err;
  }

  // Extract account name from file path
  const char *base_name = strrchr(file_path, '/');
  if (!base_name) {
    base_name = file_path;
  } else {
    base_name++;
  }

  char name[256];
  strncpy(name, base_name, sizeof(name) - 1);
  name[sizeof(name) - 1] = '\0';

  // Remove .enc extension
  char *ext = strstr(name, ".enc");
  if (ext) {
    *ext = '\0';
  }

  // Create account
  *account = app_malloc(sizeof(app_oauth_account_t));
  (*account)->name = strdup(name);
  (*account)->file_path = strdup(file_path);
  (*account)->token = *token;
  (*account)->is_default = false;
  (*account)->is_disabled = false;
  (*account)->disabled_until = 0;

  app_free(token);
  return APP_SUCCESS;
}

bool app_oauth_token_is_expired(const app_oauth_token_t *token) {
  if (!token || token->expires_at == 0) {
    return true;
  }

  // Get current time in milliseconds
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  int64_t now_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

  // Add 5 minute buffer
  return now_ms >= (token->expires_at - 300000);
}

app_error app_oauth_token_validate(const app_oauth_token_t *token) {
  if (!token) {
    return APP_ERROR_INVALID_PARAM;
  }

  if (!token->access_token || strlen(token->access_token) == 0) {
    LOG_ERROR("Missing access token");
    return APP_ERROR_INVALID_TOKEN;
  }

  if (!token->refresh_token || strlen(token->refresh_token) == 0) {
    LOG_ERROR("Missing refresh token");
    return APP_ERROR_INVALID_TOKEN;
  }

  // Validate token format
  if (strncmp(token->access_token, "sk-ant-oat01-", 13) != 0) {
    LOG_ERROR("Invalid access token format");
    return APP_ERROR_INVALID_TOKEN;
  }

  if (strncmp(token->refresh_token, "sk-ant-ort01-", 13) != 0) {
    LOG_ERROR("Invalid refresh token format");
    return APP_ERROR_INVALID_TOKEN;
  }

  // Check required scopes
  bool has_inference = false;
  bool has_profile = false;
  for (size_t i = 0; i < token->scopes_count; i++) {
    if (strcmp(token->scopes[i], "user:inference") == 0) {
      has_inference = true;
    } else if (strcmp(token->scopes[i], "user:profile") == 0) {
      has_profile = true;
    }
  }

  if (!has_inference || !has_profile) {
    LOG_ERROR("Missing required scopes");
    return APP_ERROR_INVALID_TOKEN;
  }

  return APP_SUCCESS;
}

app_error app_oauth_accounts_list(app_oauth_account_t ***accounts,
                                  size_t *count) {
  if (!accounts || !count) {
    return APP_ERROR_INVALID_PARAM;
  }

  DIR *dir = opendir(g_accounts_dir);
  if (!dir) {
    LOG_ERROR("Failed to open accounts directory: %s", strerror(errno));
    return APP_ERROR_IO;
  }

  // Count .enc files
  *count = 0;
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strstr(entry->d_name, ".enc") != NULL) {
      (*count)++;
    }
  }

  if (*count == 0) {
    closedir(dir);
    *accounts = NULL;
    return APP_SUCCESS;
  }

  // Allocate array
  *accounts = app_malloc(sizeof(app_oauth_account_t *) * (*count));

  // Load accounts
  rewinddir(dir);
  size_t index = 0;
  while ((entry = readdir(dir)) != NULL && index < *count) {
    if (strstr(entry->d_name, ".enc") == NULL) {
      continue;
    }

    char file_path[PATH_MAX];
    snprintf(file_path, sizeof(file_path), "%s/%s", g_accounts_dir,
             entry->d_name);

    app_error err = app_oauth_account_load(&(*accounts)[index], file_path);
    if (err == APP_SUCCESS) {
      index++;
    } else {
      LOG_WARNING("Failed to load account %s: %d", entry->d_name, err);
    }
  }

  closedir(dir);
  *count = index;
  return APP_SUCCESS;
}

app_error app_oauth_import_credentials(app_oauth_account_t **account,
                                       const char *json_path,
                                       const char *account_name) {
  if (!account || !json_path || !account_name) {
    return APP_ERROR_INVALID_PARAM;
  }

  // Read JSON file
  FILE *fp = fopen(json_path, "r");
  if (!fp) {
    LOG_ERROR("Failed to open credentials file: %s", strerror(errno));
    return APP_ERROR_IO;
  }

  fseek(fp, 0, SEEK_END);
  long file_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char *json_content = app_malloc(file_size + 1);
  size_t bytes_read = fread(json_content, 1, file_size, fp);
  fclose(fp);

  if (bytes_read != (size_t)file_size) {
    app_free(json_content);
    return APP_ERROR_IO;
  }
  json_content[file_size] = '\0';

  // Parse JSON
  json_value_t *root;
  app_error err = json_parse(&root, json_content);
  app_free(json_content);

  if (err != APP_SUCCESS) {
    return err;
  }

  // Create account
  err = app_oauth_account_create(account, account_name);
  if (err != APP_SUCCESS) {
    json_value_destroy(root);
    return err;
  }

  // Extract token fields
  if (root->type == JSON_TYPE_OBJECT) {
    json_value_t *oauth = json_object_get(root->object_val, "claudeAiOauth");
    if (oauth && oauth->type == JSON_TYPE_OBJECT) {
      json_value_t *access = json_object_get(oauth->object_val, "accessToken");
      if (access && access->type == JSON_TYPE_STRING) {
        (*account)->token.access_token = app_secure_strdup(access->string_val);
      }

      json_value_t *refresh =
          json_object_get(oauth->object_val, "refreshToken");
      if (refresh && refresh->type == JSON_TYPE_STRING) {
        (*account)->token.refresh_token =
            app_secure_strdup(refresh->string_val);
      }

      json_value_t *expires = json_object_get(oauth->object_val, "expiresAt");
      if (expires && expires->type == JSON_TYPE_NUMBER) {
        (*account)->token.expires_at = (int64_t)expires->number_val;
      }

      json_value_t *sub_type =
          json_object_get(oauth->object_val, "subscriptionType");
      if (sub_type && sub_type->type == JSON_TYPE_STRING) {
        (*account)->token.subscription_type = strdup(sub_type->string_val);
      }

      json_value_t *scopes = json_object_get(oauth->object_val, "scopes");
      if (scopes && scopes->type == JSON_TYPE_ARRAY) {
        (*account)->token.scopes_count = scopes->array_val->count;
        (*account)->token.scopes =
            app_malloc(sizeof(char *) * (*account)->token.scopes_count);
        for (size_t i = 0; i < (*account)->token.scopes_count; i++) {
          json_value_t *scope = scopes->array_val->items[i];
          if (scope && scope->type == JSON_TYPE_STRING) {
            (*account)->token.scopes[i] = strdup(scope->string_val);
          }
        }
      }
    }
  }

  json_value_destroy(root);

  // Validate token
  err = app_oauth_token_validate(&(*account)->token);
  if (err != APP_SUCCESS) {
    app_oauth_account_destroy(*account);
    *account = NULL;
    return err;
  }

  // Save encrypted account
  err = app_oauth_account_save(*account);
  if (err != APP_SUCCESS) {
    app_oauth_account_destroy(*account);
    *account = NULL;
    return err;
  }

  return APP_SUCCESS;
}

// PKCE Implementation

/**
 * @brief Generate a cryptographically secure random string for PKCE
 *
 * Generates a code verifier between 43-128 characters using the
 * unreserved characters defined in RFC 7636.
 */
static app_error generate_code_verifier(char **verifier) {
  if (!verifier) {
    return APP_ERROR_INVALID_ARG;
  }

  // RFC 7636 requires 43-128 characters
  const int verifier_length = 128;

  // Unreserved characters: [A-Z] [a-z] [0-9] - . _ ~
  const char charset[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
  const int charset_size = sizeof(charset) - 1;

  *verifier = app_malloc(verifier_length + 1);
  if (!*verifier) {
    return APP_ERROR_MEMORY;
  }

  // Generate random bytes
  unsigned char random_bytes[verifier_length];
  if (RAND_bytes(random_bytes, verifier_length) != 1) {
    app_free(*verifier);
    *verifier = NULL;
    LOG_ERROR("Failed to generate random bytes for PKCE verifier");
    return APP_ERROR_CRYPTO;
  }

  // Convert to allowed character set
  for (int i = 0; i < verifier_length; i++) {
    (*verifier)[i] = charset[random_bytes[i] % charset_size];
  }
  (*verifier)[verifier_length] = '\0';

  return APP_SUCCESS;
}

/**
 * @brief Calculate the code challenge from the verifier
 *
 * Uses SHA256 hash and base64url encoding as required by RFC 7636.
 */
static app_error calculate_code_challenge(const char *verifier,
                                          char **challenge) {
  if (!verifier || !challenge) {
    return APP_ERROR_INVALID_ARG;
  }

  // Calculate SHA256 hash
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256((const unsigned char *)verifier, strlen(verifier), hash);

  // Base64url encode (no padding)
  // SHA256 produces 32 bytes, which encodes to 43 base64 characters (44 with
  // padding)
  const int encoded_size = 4 * ((SHA256_DIGEST_LENGTH + 2) / 3) + 1;
  *challenge = app_malloc(encoded_size);
  if (!*challenge) {
    return APP_ERROR_MEMORY;
  }

  // Use OpenSSL's base64 encoding
  EVP_ENCODE_CTX *ctx = EVP_ENCODE_CTX_new();
  if (!ctx) {
    app_free(*challenge);
    *challenge = NULL;
    return APP_ERROR_CRYPTO;
  }

  EVP_EncodeInit(ctx);

  int out_len = 0;
  int total_len = 0;
  unsigned char temp_buf[encoded_size];

  EVP_EncodeUpdate(ctx, temp_buf, &out_len, hash, SHA256_DIGEST_LENGTH);
  total_len += out_len;

  EVP_EncodeFinal(ctx, temp_buf + total_len, &out_len);
  total_len += out_len;

  EVP_ENCODE_CTX_free(ctx);

  // Copy to output buffer and convert to base64url
  memcpy(*challenge, temp_buf, total_len);
  (*challenge)[total_len] = '\0';

  // Remove newlines that OpenSSL adds
  char *src = *challenge, *dst = *challenge;
  while (*src) {
    if (*src != '\n' && *src != '\r') {
      *dst++ = *src;
    }
    src++;
  }
  *dst = '\0';

  // Convert base64 to base64url: + to -, / to _, remove padding
  for (char *p = *challenge; *p; p++) {
    if (*p == '+')
      *p = '-';
    else if (*p == '/')
      *p = '_';
    else if (*p == '=') {
      *p = '\0';  // Remove padding
      break;
    }
  }

  return APP_SUCCESS;
}

/**
 * @brief Generate a cryptographically secure state parameter
 */
static app_error generate_state(char **state) {
  if (!state) {
    return APP_ERROR_INVALID_ARG;
  }

  const int state_length = 32;
  unsigned char random_bytes[state_length];

  if (RAND_bytes(random_bytes, state_length) != 1) {
    LOG_ERROR("Failed to generate random bytes for state");
    return APP_ERROR_CRYPTO;
  }

  // Convert to hex string
  *state = app_malloc(state_length * 2 + 1);
  if (!*state) {
    return APP_ERROR_MEMORY;
  }

  for (int i = 0; i < state_length; i++) {
    sprintf((*state) + (i * 2), "%02x", random_bytes[i]);
  }
  (*state)[state_length * 2] = '\0';

  return APP_SUCCESS;
}

app_error app_oauth_pkce_generate(app_oauth_pkce_t **pkce) {
  if (!pkce) {
    return APP_ERROR_INVALID_ARG;
  }

  *pkce = app_calloc(1, sizeof(app_oauth_pkce_t));
  if (!*pkce) {
    return APP_ERROR_MEMORY;
  }

  app_error err;

  // Generate code verifier
  err = generate_code_verifier(&(*pkce)->verifier);
  if (err != APP_SUCCESS) {
    app_free(*pkce);
    *pkce = NULL;
    return err;
  }

  // Calculate code challenge
  err = calculate_code_challenge((*pkce)->verifier, &(*pkce)->challenge);
  if (err != APP_SUCCESS) {
    app_free((*pkce)->verifier);
    app_free(*pkce);
    *pkce = NULL;
    return err;
  }

  // Generate state
  err = generate_state(&(*pkce)->state);
  if (err != APP_SUCCESS) {
    app_free((*pkce)->verifier);
    app_free((*pkce)->challenge);
    app_free(*pkce);
    *pkce = NULL;
    return err;
  }

  LOG_DEBUG(
      "Generated PKCE parameters: verifier=%d chars, challenge=%d chars, "
      "state=%d chars",
      (int)strlen((*pkce)->verifier), (int)strlen((*pkce)->challenge),
      (int)strlen((*pkce)->state));

  return APP_SUCCESS;
}

void app_oauth_pkce_destroy(app_oauth_pkce_t *pkce) {
  if (!pkce) {
    return;
  }

  app_free(pkce->verifier);
  app_free(pkce->challenge);
  app_free(pkce->state);
  app_free(pkce);
}

app_error app_oauth_authorize_url(char **url, const app_oauth_pkce_t *pkce,
                                  const char *mode) {
  if (!url || !pkce) {
    return APP_ERROR_INVALID_ARG;
  }

  // Determine which endpoint to use
  const char *auth_endpoint;
  const char *client_id;
  const char *redirect_uri;
  const char *scopes;

  if (mode && strcmp(mode, "console") == 0) {
    // Console mode for API key creation
    auth_endpoint = "https://console.anthropic.com/oauth/authorize";
    client_id = OAUTH_CLIENT_ID;
    redirect_uri = OAUTH_REDIRECT_URI;
    scopes = OAUTH_SCOPES;
  } else {
    // Default claude.ai mode
    auth_endpoint = "https://claude.ai/oauth/authorize";
    client_id = OAUTH_CLIENT_ID;
    // Use the same redirect URI as console mode (same as SST/OpenCode)
    redirect_uri =
        OAUTH_REDIRECT_URI;  // https://console.anthropic.com/oauth/code/callback
    scopes = "user:inference user:profile";
  }

  // URL encode parameters
  char *encoded_redirect = app_http_url_encode(redirect_uri);
  char *encoded_scopes = app_http_url_encode(scopes);
  char *encoded_challenge = app_http_url_encode(pkce->challenge);
  char *encoded_state = app_http_url_encode(pkce->state);

  if (!encoded_redirect || !encoded_scopes || !encoded_challenge ||
      !encoded_state) {
    app_free(encoded_redirect);
    app_free(encoded_scopes);
    app_free(encoded_challenge);
    app_free(encoded_state);
    return APP_ERROR_MEMORY;
  }

  // Build authorization URL
  int url_size =
      snprintf(NULL, 0,
               "%s?response_type=code&client_id=%s&redirect_uri=%s&scope=%s"
               "&code_challenge=%s&code_challenge_method=S256&state=%s",
               auth_endpoint, client_id, encoded_redirect, encoded_scopes,
               encoded_challenge, encoded_state) +
      1;

  *url = app_malloc(url_size);
  if (!*url) {
    app_free(encoded_redirect);
    app_free(encoded_scopes);
    app_free(encoded_challenge);
    app_free(encoded_state);
    return APP_ERROR_MEMORY;
  }

  snprintf(*url, url_size,
           "%s?response_type=code&client_id=%s&redirect_uri=%s&scope=%s"
           "&code_challenge=%s&code_challenge_method=S256&state=%s",
           auth_endpoint, client_id, encoded_redirect, encoded_scopes,
           encoded_challenge, encoded_state);

  app_free(encoded_redirect);
  app_free(encoded_scopes);
  app_free(encoded_challenge);
  app_free(encoded_state);

  LOG_DEBUG("Generated authorization URL for %s mode",
            mode ? mode : "claude.ai");

  return APP_SUCCESS;
}

app_error app_oauth_exchange_code(app_oauth_token_t **token, const char *code,
                                  const char *verifier) {
  if (!token || !code || !verifier) {
    return APP_ERROR_INVALID_ARG;
  }

  // For now, use the console endpoint (will be made configurable later)
  const char *token_endpoint = "https://console.anthropic.com/oauth/token";

  // Build request body
  char *encoded_code = app_http_url_encode(code);
  char *encoded_verifier = app_http_url_encode(verifier);
  char *encoded_redirect = app_http_url_encode(OAUTH_REDIRECT_URI);

  if (!encoded_code || !encoded_verifier || !encoded_redirect) {
    app_free(encoded_code);
    app_free(encoded_verifier);
    app_free(encoded_redirect);
    return APP_ERROR_MEMORY;
  }

  char *body = NULL;
  int body_size = asprintf(&body,
                           "grant_type=authorization_code&code=%s&client_id=%s"
                           "&redirect_uri=%s&code_verifier=%s",
                           encoded_code, OAUTH_CLIENT_ID, encoded_redirect,
                           encoded_verifier);

  app_free(encoded_code);
  app_free(encoded_verifier);
  app_free(encoded_redirect);

  if (body_size < 0) {
    return APP_ERROR_MEMORY;
  }

  // Make request
  app_http_request_t request = {.method = "POST",
                                .url = (char *)token_endpoint,
                                .body = body,
                                .body_size = strlen(body),
                                .headers = NULL,
                                .header_count = 0};

  // Add Content-Type header
  const char *content_type = "Content-Type: application/x-www-form-urlencoded";
  request.headers = app_malloc(sizeof(char *));
  request.headers[0] = (char *)content_type;
  request.header_count = 1;

  app_http_response_t *response;
  app_error err = app_http_request(&response, &request);
  app_free(request.headers);
  free(body);

  if (err != APP_SUCCESS) {
    LOG_ERROR("Failed to exchange authorization code");
    return err;
  }

  if (response->status_code != 200) {
    LOG_ERROR("Token exchange failed with status %d: %s", response->status_code,
              response->body);
    app_http_response_destroy(response);
    return APP_ERROR_INVALID_TOKEN;
  }

  // Parse response
  json_value_t *root;
  err = json_parse(&root, response->body);
  app_http_response_destroy(response);

  if (err != APP_SUCCESS) {
    LOG_ERROR("Failed to parse token response");
    return err;
  }

  // Extract tokens
  *token = app_calloc(1, sizeof(app_oauth_token_t));
  if (!*token) {
    json_value_destroy(root);
    return APP_ERROR_MEMORY;
  }

  if (root->type == JSON_TYPE_OBJECT) {
    json_value_t *access = json_object_get(root->object_val, "access_token");
    if (access && access->type == JSON_TYPE_STRING) {
      (*token)->access_token = app_secure_strdup(access->string_val);
    }

    json_value_t *refresh = json_object_get(root->object_val, "refresh_token");
    if (refresh && refresh->type == JSON_TYPE_STRING) {
      (*token)->refresh_token = app_secure_strdup(refresh->string_val);
    }

    json_value_t *expires_in = json_object_get(root->object_val, "expires_in");
    if (expires_in && expires_in->type == JSON_TYPE_NUMBER) {
      // Calculate absolute expiry time
      struct timespec ts;
      clock_gettime(CLOCK_REALTIME, &ts);
      int64_t now_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
      (*token)->expires_at = now_ms + ((int64_t)expires_in->number_val * 1000);
    }

    // Extract scopes if provided
    json_value_t *scope = json_object_get(root->object_val, "scope");
    if (scope && scope->type == JSON_TYPE_STRING) {
      // Parse space-separated scopes
      char *scope_copy = app_strdup(scope->string_val);
      char *saveptr;
      char *scope_token = strtok_r(scope_copy, " ", &saveptr);

      // Count scopes
      size_t scope_count = 0;
      char *temp = app_strdup(scope->string_val);
      char *temp_save;
      char *temp_token = strtok_r(temp, " ", &temp_save);
      while (temp_token) {
        scope_count++;
        temp_token = strtok_r(NULL, " ", &temp_save);
      }
      app_free(temp);

      // Allocate and fill scopes array
      if (scope_count > 0) {
        (*token)->scopes = app_calloc(scope_count, sizeof(char *));
        (*token)->scopes_count = scope_count;

        size_t i = 0;
        while (scope_token && i < scope_count) {
          (*token)->scopes[i] = app_strdup(scope_token);
          i++;
          scope_token = strtok_r(NULL, " ", &saveptr);
        }
      }

      app_free(scope_copy);
    }
  }

  json_value_destroy(root);

  // Validate token
  err = app_oauth_token_validate(*token);
  if (err != APP_SUCCESS) {
    app_oauth_token_free(*token);
    *token = NULL;
    return err;
  }

  LOG_INFO("Successfully exchanged authorization code for tokens");

  return APP_SUCCESS;
}
app_error app_oauth_token_refresh(app_oauth_token_t *token,
                                  const app_oauth_config_t *config) {
  (void)config;  // Config not needed for Anthropic OAuth
  if (!token || !token->refresh_token) {
    return APP_ERROR_INVALID_PARAM;
  }

  // Build request body
  char request_body[1024];
  snprintf(request_body, sizeof(request_body),
           "{"
           "\"grant_type\":\"refresh_token\","
           "\"refresh_token\":\"%s\","
           "\"client_id\":\"%s\""
           "}",
           token->refresh_token, OAUTH_CLIENT_ID);

  // Make HTTP request
  app_http_response_t *response;
  app_error err = app_http_post_json(
      &response, "https://console.anthropic.com/v1/oauth/token", request_body);

  if (err != APP_SUCCESS) {
    LOG_ERROR("Failed to make token refresh request");
    return err;
  }

  if (response->status_code != 200) {
    LOG_ERROR("Token refresh failed with status %d: %s", response->status_code,
              response->body);
    app_http_response_destroy(response);
    return APP_ERROR_INVALID_TOKEN;
  }

  // Parse response
  json_value_t *root;
  err = json_parse(&root, response->body);
  app_http_response_destroy(response);

  if (err != APP_SUCCESS) {
    LOG_ERROR("Failed to parse refresh response");
    return err;
  }

  // Update tokens
  if (root->type == JSON_TYPE_OBJECT) {
    json_value_t *access = json_object_get(root->object_val, "access_token");
    if (access && access->type == JSON_TYPE_STRING) {
      app_free(token->access_token);
      token->access_token = app_secure_strdup(access->string_val);
    }

    json_value_t *refresh = json_object_get(root->object_val, "refresh_token");
    if (refresh && refresh->type == JSON_TYPE_STRING) {
      app_free(token->refresh_token);
      token->refresh_token = app_secure_strdup(refresh->string_val);
    }

    json_value_t *expires_in = json_object_get(root->object_val, "expires_in");
    if (expires_in && expires_in->type == JSON_TYPE_NUMBER) {
      // Calculate absolute expiry time
      struct timespec ts;
      clock_gettime(CLOCK_REALTIME, &ts);
      int64_t now_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
      token->expires_at = now_ms + ((int64_t)expires_in->number_val * 1000);
    }
  }

  json_value_destroy(root);

  return APP_SUCCESS;
}

app_error app_oauth_account_set_default(const char *name) {
  if (!name) {
    return APP_ERROR_INVALID_PARAM;
  }

  // TODO: Store default account preference
  LOG_INFO("Setting default account to: %s", name);
  return APP_SUCCESS;
}

app_error app_oauth_account_disable(const char *name, time_t until) {
  if (!name) {
    return APP_ERROR_INVALID_PARAM;
  }

  // TODO: Load account, set disabled flag, save
  LOG_INFO("Disabling account %s until %ld", name, until);
  return APP_SUCCESS;
}

app_error app_oauth_account_enable(const char *name) {
  if (!name) {
    return APP_ERROR_INVALID_PARAM;
  }

  // TODO: Load account, clear disabled flag, save
  LOG_INFO("Enabling account %s", name);
  return APP_SUCCESS;
}

app_error app_oauth_export_credentials(const app_oauth_account_t *account,
                                       char **json_output) {
  if (!account || !json_output) {
    return APP_ERROR_INVALID_PARAM;
  }

  // Create JSON output
  char buffer[4096];
  int len =
      snprintf(buffer, sizeof(buffer),
               "{\n"
               "  \"claudeAiOauth\": {\n"
               "    \"accessToken\": \"%s\",\n"
               "    \"refreshToken\": \"%s\",\n"
               "    \"expiresAt\": %lld,\n"
               "    \"scopes\": [",
               account->token.access_token ? account->token.access_token : "",
               account->token.refresh_token ? account->token.refresh_token : "",
               (long long)account->token.expires_at);

  // Add scopes
  for (size_t i = 0; i < account->token.scopes_count; i++) {
    len += snprintf(buffer + len, sizeof(buffer) - len, "%s\"%s\"",
                    i > 0 ? ", " : "", account->token.scopes[i]);
  }

  len += snprintf(
      buffer + len, sizeof(buffer) - len,
      "],\n"
      "    \"subscriptionType\": \"%s\"\n"
      "  }\n"
      "}\n",
      account->token.subscription_type ? account->token.subscription_type : "");

  *json_output = strdup(buffer);
  return APP_SUCCESS;
}

// Base64 URL encoding (no padding)
static void base64url_encode(const unsigned char *input, size_t input_len,
                             char *output) {
  static const char base64_chars[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

  size_t i, j;
  for (i = 0, j = 0; i < input_len; i += 3) {
    unsigned char byte1 = input[i];
    unsigned char byte2 = (i + 1 < input_len) ? input[i + 1] : 0;
    unsigned char byte3 = (i + 2 < input_len) ? input[i + 2] : 0;

    output[j++] = base64_chars[byte1 >> 2];
    output[j++] = base64_chars[((byte1 & 0x03) << 4) | (byte2 >> 4)];
    if (i + 1 < input_len) {
      output[j++] = base64_chars[((byte2 & 0x0f) << 2) | (byte3 >> 6)];
    }
    if (i + 2 < input_len) {
      output[j++] = base64_chars[byte3 & 0x3f];
    }
  }
  output[j] = '\0';
}

void app_oauth_token_free(app_oauth_token_t *token) {
  if (!token) {
    return;
  }

  // Securely wipe sensitive token data
  if (token->access_token) {
    app_secure_free(token->access_token, strlen(token->access_token));
  }
  if (token->refresh_token) {
    app_secure_free(token->refresh_token, strlen(token->refresh_token));
  }

  app_free(token->subscription_type);

  if (token->scopes) {
    for (size_t i = 0; i < token->scopes_count; i++) {
      app_free(token->scopes[i]);
    }
    app_free(token->scopes);
  }

  app_secure_free(token, sizeof(app_oauth_token_t));
}
