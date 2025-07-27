#include "http.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "memory.h"

static CURL *g_curl = NULL;
static bool g_initialized = false;

// Write callback for curl
static size_t write_callback(void *contents, size_t size, size_t nmemb,
                             void *userp) {
  size_t realsize = size * nmemb;
  app_http_response_t *response = (app_http_response_t *)userp;

  char *ptr = app_realloc(response->body, response->body_size + realsize + 1);
  if (!ptr) {
    LOG_ERROR("Failed to allocate memory for response body");
    return 0;
  }

  response->body = ptr;
  memcpy(&(response->body[response->body_size]), contents, realsize);
  response->body_size += realsize;
  response->body[response->body_size] = '\0';

  return realsize;
}

// Header callback for curl
static size_t header_callback(char *buffer, size_t size, size_t nitems,
                              void *userdata) {
  size_t realsize = size * nitems;
  app_http_response_t *response = (app_http_response_t *)userdata;

  // Skip empty lines
  if (realsize <= 2) {
    return realsize;
  }

  // Allocate space for new header
  response->headers = app_realloc(
      response->headers, sizeof(char *) * (response->header_count + 1));
  if (!response->headers) {
    return 0;
  }

  // Copy header (remove trailing newline)
  char *header = app_malloc(realsize + 1);
  memcpy(header, buffer, realsize);
  header[realsize] = '\0';

  // Remove trailing \r\n
  char *end = header + strlen(header) - 1;
  while (end > header && (*end == '\r' || *end == '\n')) {
    *end-- = '\0';
  }

  response->headers[response->header_count++] = header;

  return realsize;
}

app_error app_http_init(void) {
  if (g_initialized) {
    return APP_SUCCESS;
  }

  if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
    LOG_ERROR("Failed to initialize curl");
    return APP_ERROR_INTERNAL;
  }

  g_curl = curl_easy_init();
  if (!g_curl) {
    curl_global_cleanup();
    LOG_ERROR("Failed to create curl handle");
    return APP_ERROR_INTERNAL;
  }

  g_initialized = true;
  return APP_SUCCESS;
}

void app_http_cleanup(void) {
  if (g_curl) {
    curl_easy_cleanup(g_curl);
    g_curl = NULL;
  }

  if (g_initialized) {
    curl_global_cleanup();
    g_initialized = false;
  }
}

app_error app_http_request(app_http_response_t **response,
                           const app_http_request_t *request) {
  if (!response || !request || !request->url) {
    return APP_ERROR_INVALID_PARAM;
  }

  if (!g_initialized) {
    app_error err = app_http_init();
    if (err != APP_SUCCESS) {
      return err;
    }
  }

  // Create response structure
  *response = app_calloc(1, sizeof(app_http_response_t));

  // Reset curl handle
  curl_easy_reset(g_curl);

  // Set URL
  curl_easy_setopt(g_curl, CURLOPT_URL, request->url);

  // Set method
  if (request->method) {
    if (strcmp(request->method, "POST") == 0) {
      curl_easy_setopt(g_curl, CURLOPT_POST, 1L);
    } else if (strcmp(request->method, "PUT") == 0) {
      curl_easy_setopt(g_curl, CURLOPT_CUSTOMREQUEST, "PUT");
    } else if (strcmp(request->method, "DELETE") == 0) {
      curl_easy_setopt(g_curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
  }

  // Set request body
  if (request->body && request->body_size > 0) {
    curl_easy_setopt(g_curl, CURLOPT_POSTFIELDS, request->body);
    curl_easy_setopt(g_curl, CURLOPT_POSTFIELDSIZE, (long)request->body_size);
  }

  // Set headers
  struct curl_slist *headers = NULL;
  for (size_t i = 0; i < request->header_count; i++) {
    headers = curl_slist_append(headers, request->headers[i]);
  }
  if (headers) {
    curl_easy_setopt(g_curl, CURLOPT_HTTPHEADER, headers);
  }

  // Set callbacks
  curl_easy_setopt(g_curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(g_curl, CURLOPT_WRITEDATA, *response);
  curl_easy_setopt(g_curl, CURLOPT_HEADERFUNCTION, header_callback);
  curl_easy_setopt(g_curl, CURLOPT_HEADERDATA, *response);

  // Enable SSL verification
  curl_easy_setopt(g_curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(g_curl, CURLOPT_SSL_VERIFYHOST, 2L);

  // Set timeout
  curl_easy_setopt(g_curl, CURLOPT_TIMEOUT, 30L);

  // Perform request
  CURLcode res = curl_easy_perform(g_curl);

  // Clean up headers
  if (headers) {
    curl_slist_free_all(headers);
  }

  if (res != CURLE_OK) {
    LOG_ERROR("curl_easy_perform() failed: %s", curl_easy_strerror(res));
    app_http_response_destroy(*response);
    *response = NULL;
    return APP_ERROR_IO;
  }

  // Get response code
  long response_code;
  curl_easy_getinfo(g_curl, CURLINFO_RESPONSE_CODE, &response_code);
  (*response)->status_code = (int)response_code;

  return APP_SUCCESS;
}

void app_http_response_destroy(app_http_response_t *response) {
  if (!response)
    return;

  app_free(response->body);

  for (size_t i = 0; i < response->header_count; i++) {
    app_free(response->headers[i]);
  }
  app_free(response->headers);

  app_free(response);
}

app_error app_http_post_json(app_http_response_t **response, const char *url,
                             const char *json_body) {
  if (!response || !url || !json_body) {
    return APP_ERROR_INVALID_PARAM;
  }

  app_http_request_t request = {.url = (char *)url,
                                .method = "POST",
                                .body = (char *)json_body,
                                .body_size = strlen(json_body),
                                .headers = NULL,
                                .header_count = 0};

  // Add Content-Type header
  const char *content_type = "Content-Type: application/json";
  request.headers = app_malloc(sizeof(char *));
  request.headers[0] = (char *)content_type;
  request.header_count = 1;

  app_error err = app_http_request(response, &request);

  app_free(request.headers);

  return err;
}

app_error app_http_get(app_http_response_t **response, const char *url) {
  if (!response || !url) {
    return APP_ERROR_INVALID_PARAM;
  }

  app_http_request_t request = {.url = (char *)url,
                                .method = "GET",
                                .body = NULL,
                                .body_size = 0,
                                .headers = NULL,
                                .header_count = 0};

  return app_http_request(response, &request);
}

app_error app_http_add_header(app_http_request_t *request, const char *header) {
  if (!request || !header) {
    return APP_ERROR_INVALID_PARAM;
  }

  request->headers = app_realloc(request->headers,
                                 sizeof(char *) * (request->header_count + 1));
  if (!request->headers) {
    return APP_ERROR_MEMORY;
  }

  request->headers[request->header_count++] = (char *)header;

  return APP_SUCCESS;
}

const char *app_http_get_header(const app_http_response_t *response,
                                const char *name) {
  if (!response || !name) {
    return NULL;
  }

  size_t name_len = strlen(name);

  for (size_t i = 0; i < response->header_count; i++) {
    if (strncasecmp(response->headers[i], name, name_len) == 0 &&
        response->headers[i][name_len] == ':') {
      // Skip ": " after header name
      const char *value = response->headers[i] + name_len + 1;
      while (*value == ' ')
        value++;
      return value;
    }
  }

  return NULL;
}

char *app_http_url_encode(const char *str) {
  if (!str) {
    return NULL;
  }

  // Count how much space we need
  size_t len = 0;
  for (const char *p = str; *p; p++) {
    if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
        (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' || *p == '.' ||
        *p == '~') {
      len++;
    } else {
      len += 3;  // %XX
    }
  }

  char *encoded = app_malloc(len + 1);
  if (!encoded) {
    return NULL;
  }

  char *out = encoded;
  for (const char *p = str; *p; p++) {
    if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
        (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' || *p == '.' ||
        *p == '~') {
      *out++ = *p;
    } else {
      sprintf(out, "%%%02X", (unsigned char)*p);
      out += 3;
    }
  }
  *out = '\0';

  return encoded;
}