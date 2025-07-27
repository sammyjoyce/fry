#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "../core/error.h"
#include "../core/types.h"

// HTTP request/response structures
typedef struct {
  char *url;
  char *method;  // GET, POST, PUT, DELETE
  char *body;
  size_t body_size;
  char **headers;
  size_t header_count;
} app_http_request_t;

typedef struct {
  int status_code;
  char *body;
  size_t body_size;
  char **headers;
  size_t header_count;
} app_http_response_t;

// HTTP client functions
APP_NODISCARD app_error app_http_init(void);
void app_http_cleanup(void);

APP_NODISCARD app_error app_http_request(app_http_response_t **response,
                                         const app_http_request_t *request);

void app_http_response_destroy(app_http_response_t *response);

// Convenience functions
APP_NODISCARD app_error app_http_post_json(app_http_response_t **response,
                                           const char *url,
                                           const char *json_body);

APP_NODISCARD app_error app_http_get(app_http_response_t **response,
                                     const char *url);

// Header helpers
APP_NODISCARD app_error app_http_add_header(app_http_request_t *request,
                                            const char *header);
const char *app_http_get_header(const app_http_response_t *response,
                                const char *name);

// URL encoding
char *app_http_url_encode(const char *str);