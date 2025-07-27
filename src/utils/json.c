#include "json.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/error.h"
#include "logging.h"
#include "memory.h"

typedef struct {
  const char *data;
  size_t pos;
  size_t len;
} json_parser_t;

static void skip_whitespace(json_parser_t *parser) {
  while (parser->pos < parser->len &&
         isspace((unsigned char)parser->data[parser->pos])) {
    parser->pos++;
  }
}

static bool peek_char(json_parser_t *parser, char ch) {
  skip_whitespace(parser);
  return parser->pos < parser->len && parser->data[parser->pos] == ch;
}

static bool consume_char(json_parser_t *parser, char ch) {
  if (peek_char(parser, ch)) {
    parser->pos++;
    return true;
  }
  return false;
}

static app_error parse_string(json_parser_t *parser, char **out) {
  if (!consume_char(parser, '"')) {
    return APP_ERROR_PARSE;
  }

  size_t start = parser->pos;
  size_t len = 0;

  while (parser->pos < parser->len && parser->data[parser->pos] != '"') {
    if (parser->data[parser->pos] == '\\') {
      parser->pos++;
      if (parser->pos >= parser->len) {
        return APP_ERROR_PARSE;
      }
    }
    parser->pos++;
    len++;
  }

  if (!consume_char(parser, '"')) {
    return APP_ERROR_PARSE;
  }

  *out = app_malloc(len + 1);
  size_t out_pos = 0;

  for (size_t i = start; i < start + len; i++) {
    if (parser->data[i] == '\\' && i + 1 < start + len) {
      i++;
      switch (parser->data[i]) {
      case '"':
        (*out)[out_pos++] = '"';
        break;
      case '\\':
        (*out)[out_pos++] = '\\';
        break;
      case '/':
        (*out)[out_pos++] = '/';
        break;
      case 'b':
        (*out)[out_pos++] = '\b';
        break;
      case 'f':
        (*out)[out_pos++] = '\f';
        break;
      case 'n':
        (*out)[out_pos++] = '\n';
        break;
      case 'r':
        (*out)[out_pos++] = '\r';
        break;
      case 't':
        (*out)[out_pos++] = '\t';
        break;
      default:
        (*out)[out_pos++] = parser->data[i];
        break;
      }
    } else {
      (*out)[out_pos++] = parser->data[i];
    }
  }

  (*out)[out_pos] = '\0';
  return APP_SUCCESS;
}

static app_error parse_number(json_parser_t *parser, double *out) {
  skip_whitespace(parser);

  size_t start = parser->pos;
  bool has_digit = false;

  if (parser->pos < parser->len && parser->data[parser->pos] == '-') {
    parser->pos++;
  }

  while (parser->pos < parser->len &&
         isdigit((unsigned char)parser->data[parser->pos])) {
    has_digit = true;
    parser->pos++;
  }

  if (parser->pos < parser->len && parser->data[parser->pos] == '.') {
    parser->pos++;
    while (parser->pos < parser->len &&
           isdigit((unsigned char)parser->data[parser->pos])) {
      has_digit = true;
      parser->pos++;
    }
  }

  if (!has_digit) {
    return APP_ERROR_PARSE;
  }

  if (parser->pos < parser->len &&
      (parser->data[parser->pos] == 'e' || parser->data[parser->pos] == 'E')) {
    parser->pos++;
    if (parser->pos < parser->len && (parser->data[parser->pos] == '+' ||
                                      parser->data[parser->pos] == '-')) {
      parser->pos++;
    }
    while (parser->pos < parser->len &&
           isdigit((unsigned char)parser->data[parser->pos])) {
      parser->pos++;
    }
  }

  char *num_str = app_malloc(parser->pos - start + 1);
  memcpy(num_str, parser->data + start, parser->pos - start);
  num_str[parser->pos - start] = '\0';

  char *endptr;
  *out = strtod(num_str, &endptr);
  app_free(num_str);

  if (*endptr != '\0') {
    return APP_ERROR_PARSE;
  }

  return APP_SUCCESS;
}

static app_error parse_value(json_parser_t *parser, json_value_t **out);

static app_error parse_array(json_parser_t *parser, json_array_t **out) {
  if (!consume_char(parser, '[')) {
    return APP_ERROR_PARSE;
  }

  *out = app_malloc(sizeof(json_array_t));
  (*out)->items = NULL;
  (*out)->count = 0;
  (*out)->capacity = 0;

  skip_whitespace(parser);
  if (peek_char(parser, ']')) {
    parser->pos++;
    return APP_SUCCESS;
  }

  while (true) {
    json_value_t *value;
    app_error err = parse_value(parser, &value);
    if (err != APP_SUCCESS) {
      // TODO: Clean up array
      return err;
    }

    if ((*out)->count >= (*out)->capacity) {
      size_t new_capacity = (*out)->capacity == 0 ? 4 : (*out)->capacity * 2;
      (*out)->items =
          app_realloc((*out)->items, sizeof(json_value_t *) * new_capacity);
      (*out)->capacity = new_capacity;
    }

    (*out)->items[(*out)->count++] = value;

    skip_whitespace(parser);
    if (consume_char(parser, ']')) {
      break;
    }

    if (!consume_char(parser, ',')) {
      return APP_ERROR_PARSE;
    }
  }

  return APP_SUCCESS;
}

static app_error parse_object(json_parser_t *parser, json_object_t **out) {
  if (!consume_char(parser, '{')) {
    return APP_ERROR_PARSE;
  }

  *out = app_malloc(sizeof(json_object_t));
  (*out)->keys = NULL;
  (*out)->values = NULL;
  (*out)->count = 0;
  (*out)->capacity = 0;

  skip_whitespace(parser);
  if (peek_char(parser, '}')) {
    parser->pos++;
    return APP_SUCCESS;
  }

  while (true) {
    char *key;
    app_error err = parse_string(parser, &key);
    if (err != APP_SUCCESS) {
      // TODO: Clean up object
      return err;
    }

    if (!consume_char(parser, ':')) {
      app_free(key);
      return APP_ERROR_PARSE;
    }

    json_value_t *value;
    err = parse_value(parser, &value);
    if (err != APP_SUCCESS) {
      app_free(key);
      return err;
    }

    if ((*out)->count >= (*out)->capacity) {
      size_t new_capacity = (*out)->capacity == 0 ? 4 : (*out)->capacity * 2;
      (*out)->keys = app_realloc((*out)->keys, sizeof(char *) * new_capacity);
      (*out)->values =
          app_realloc((*out)->values, sizeof(json_value_t *) * new_capacity);
      (*out)->capacity = new_capacity;
    }

    (*out)->keys[(*out)->count] = key;
    (*out)->values[(*out)->count] = value;
    (*out)->count++;

    skip_whitespace(parser);
    if (consume_char(parser, '}')) {
      break;
    }

    if (!consume_char(parser, ',')) {
      return APP_ERROR_PARSE;
    }
  }

  return APP_SUCCESS;
}

static app_error parse_value(json_parser_t *parser, json_value_t **out) {
  skip_whitespace(parser);

  *out = app_malloc(sizeof(json_value_t));

  if (parser->pos >= parser->len) {
    app_free(*out);
    return APP_ERROR_PARSE;
  }

  // null
  if (parser->pos + 4 <= parser->len &&
      strncmp(parser->data + parser->pos, "null", 4) == 0) {
    parser->pos += 4;
    (*out)->type = JSON_TYPE_NULL;
    return APP_SUCCESS;
  }

  // true
  if (parser->pos + 4 <= parser->len &&
      strncmp(parser->data + parser->pos, "true", 4) == 0) {
    parser->pos += 4;
    (*out)->type = JSON_TYPE_BOOL;
    (*out)->bool_val = true;
    return APP_SUCCESS;
  }

  // false
  if (parser->pos + 5 <= parser->len &&
      strncmp(parser->data + parser->pos, "false", 5) == 0) {
    parser->pos += 5;
    (*out)->type = JSON_TYPE_BOOL;
    (*out)->bool_val = false;
    return APP_SUCCESS;
  }

  // string
  if (peek_char(parser, '"')) {
    (*out)->type = JSON_TYPE_STRING;
    return parse_string(parser, &(*out)->string_val);
  }

  // array
  if (peek_char(parser, '[')) {
    (*out)->type = JSON_TYPE_ARRAY;
    return parse_array(parser, &(*out)->array_val);
  }

  // object
  if (peek_char(parser, '{')) {
    (*out)->type = JSON_TYPE_OBJECT;
    return parse_object(parser, &(*out)->object_val);
  }

  // number
  if (isdigit((unsigned char)parser->data[parser->pos]) ||
      parser->data[parser->pos] == '-') {
    (*out)->type = JSON_TYPE_NUMBER;
    return parse_number(parser, &(*out)->number_val);
  }

  app_free(*out);
  return APP_ERROR_PARSE;
}

app_error json_parse(json_value_t **value, const char *json) {
  if (!value || !json) {
    return APP_ERROR_INVALID_PARAM;
  }

  json_parser_t parser = {.data = json, .pos = 0, .len = strlen(json)};

  return parse_value(&parser, value);
}

void json_value_destroy(json_value_t *value) {
  if (!value)
    return;

  switch (value->type) {
  case JSON_TYPE_STRING:
    app_free(value->string_val);
    break;

  case JSON_TYPE_ARRAY:
    if (value->array_val) {
      for (size_t i = 0; i < value->array_val->count; i++) {
        json_value_destroy(value->array_val->items[i]);
      }
      app_free(value->array_val->items);
      app_free(value->array_val);
    }
    break;

  case JSON_TYPE_OBJECT:
    if (value->object_val) {
      for (size_t i = 0; i < value->object_val->count; i++) {
        app_free(value->object_val->keys[i]);
        json_value_destroy(value->object_val->values[i]);
      }
      app_free(value->object_val->keys);
      app_free(value->object_val->values);
      app_free(value->object_val);
    }
    break;

  default:
    break;
  }

  app_free(value);
}

json_value_t *json_object_get(const json_object_t *obj, const char *key) {
  if (!obj || !key)
    return NULL;

  for (size_t i = 0; i < obj->count; i++) {
    if (strcmp(obj->keys[i], key) == 0) {
      return obj->values[i];
    }
  }

  return NULL;
}

json_value_t *json_value_null(void) {
  json_value_t *val = app_malloc(sizeof(json_value_t));
  val->type = JSON_TYPE_NULL;
  return val;
}

json_value_t *json_value_bool(bool b) {
  json_value_t *val = app_malloc(sizeof(json_value_t));
  val->type = JSON_TYPE_BOOL;
  val->bool_val = b;
  return val;
}

json_value_t *json_value_number(double n) {
  json_value_t *val = app_malloc(sizeof(json_value_t));
  val->type = JSON_TYPE_NUMBER;
  val->number_val = n;
  return val;
}

json_value_t *json_value_string(const char *s) {
  json_value_t *val = app_malloc(sizeof(json_value_t));
  val->type = JSON_TYPE_STRING;
  val->string_val = strdup(s);
  return val;
}

json_value_t *json_value_array(void) {
  json_value_t *val = app_malloc(sizeof(json_value_t));
  val->type = JSON_TYPE_ARRAY;
  val->array_val = app_malloc(sizeof(json_array_t));
  val->array_val->items = NULL;
  val->array_val->count = 0;
  val->array_val->capacity = 0;
  return val;
}

json_value_t *json_value_object(void) {
  json_value_t *val = app_malloc(sizeof(json_value_t));
  val->type = JSON_TYPE_OBJECT;
  val->object_val = app_malloc(sizeof(json_object_t));
  val->object_val->keys = NULL;
  val->object_val->values = NULL;
  val->object_val->count = 0;
  val->object_val->capacity = 0;
  return val;
}

app_error json_object_set(json_object_t *obj, const char *key,
                          json_value_t *value) {
  if (!obj || !key || !value) {
    return APP_ERROR_INVALID_PARAM;
  }

  // Check if key already exists
  for (size_t i = 0; i < obj->count; i++) {
    if (strcmp(obj->keys[i], key) == 0) {
      // Replace existing value
      json_value_destroy(obj->values[i]);
      obj->values[i] = value;
      return APP_SUCCESS;
    }
  }

  // Add new key-value pair
  if (obj->count >= obj->capacity) {
    size_t new_capacity = obj->capacity == 0 ? 4 : obj->capacity * 2;
    obj->keys = app_realloc(obj->keys, sizeof(char *) * new_capacity);
    obj->values =
        app_realloc(obj->values, sizeof(json_value_t *) * new_capacity);
    obj->capacity = new_capacity;
  }

  obj->keys[obj->count] = strdup(key);
  obj->values[obj->count] = value;
  obj->count++;

  return APP_SUCCESS;
}

json_value_t *json_array_get(const json_array_t *arr, size_t index) {
  if (!arr || index >= arr->count) {
    return NULL;
  }
  return arr->items[index];
}

app_error json_array_append(json_array_t *arr, json_value_t *value) {
  if (!arr || !value) {
    return APP_ERROR_INVALID_PARAM;
  }

  if (arr->count >= arr->capacity) {
    size_t new_capacity = arr->capacity == 0 ? 4 : arr->capacity * 2;
    arr->items = app_realloc(arr->items, sizeof(json_value_t *) * new_capacity);
    arr->capacity = new_capacity;
  }

  arr->items[arr->count++] = value;
  return APP_SUCCESS;
}

// JSON stringification
static app_error stringify_value(char **output, size_t *size, size_t *capacity,
                                 const json_value_t *value, int indent_level,
                                 bool pretty);

static app_error append_string(char **output, size_t *size, size_t *capacity,
                               const char *str) {
  size_t len = strlen(str);
  if (*size + len + 1 > *capacity) {
    size_t new_capacity = (*capacity == 0 ? 256 : *capacity * 2);
    while (new_capacity < *size + len + 1) {
      new_capacity *= 2;
    }
    *output = app_realloc(*output, new_capacity);
    *capacity = new_capacity;
  }

  memcpy(*output + *size, str, len);
  *size += len;
  (*output)[*size] = '\0';

  return APP_SUCCESS;
}

static app_error append_indent(char **output, size_t *size, size_t *capacity,
                               int level) {
  for (int i = 0; i < level * 2; i++) {
    app_error err = append_string(output, size, capacity, " ");
    if (err != APP_SUCCESS)
      return err;
  }
  return APP_SUCCESS;
}

static app_error stringify_string(char **output, size_t *size, size_t *capacity,
                                  const char *str) {
  app_error err = append_string(output, size, capacity, "\"");
  if (err != APP_SUCCESS)
    return err;

  for (const char *p = str; *p; p++) {
    switch (*p) {
    case '"':
      err = append_string(output, size, capacity, "\\\"");
      break;
    case '\\':
      err = append_string(output, size, capacity, "\\\\");
      break;
    case '\b':
      err = append_string(output, size, capacity, "\\b");
      break;
    case '\f':
      err = append_string(output, size, capacity, "\\f");
      break;
    case '\n':
      err = append_string(output, size, capacity, "\\n");
      break;
    case '\r':
      err = append_string(output, size, capacity, "\\r");
      break;
    case '\t':
      err = append_string(output, size, capacity, "\\t");
      break;
    default: {
      char ch[2] = {*p, '\0'};
      err = append_string(output, size, capacity, ch);
    }
    }
    if (err != APP_SUCCESS)
      return err;
  }

  return append_string(output, size, capacity, "\"");
}

static app_error stringify_value(char **output, size_t *size, size_t *capacity,
                                 const json_value_t *value, int indent_level,
                                 bool pretty) {
  if (!value) {
    return append_string(output, size, capacity, "null");
  }

  app_error err;

  switch (value->type) {
  case JSON_TYPE_NULL:
    return append_string(output, size, capacity, "null");

  case JSON_TYPE_BOOL:
    return append_string(output, size, capacity,
                         value->bool_val ? "true" : "false");

  case JSON_TYPE_NUMBER: {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.17g", value->number_val);
    return append_string(output, size, capacity, buffer);
  }

  case JSON_TYPE_STRING:
    return stringify_string(output, size, capacity, value->string_val);

  case JSON_TYPE_ARRAY: {
    err = append_string(output, size, capacity, "[");
    if (err != APP_SUCCESS)
      return err;

    for (size_t i = 0; i < value->array_val->count; i++) {
      if (i > 0) {
        err = append_string(output, size, capacity, ",");
        if (err != APP_SUCCESS)
          return err;
      }

      if (pretty) {
        err = append_string(output, size, capacity, "\n");
        if (err != APP_SUCCESS)
          return err;
        err = append_indent(output, size, capacity, indent_level + 1);
        if (err != APP_SUCCESS)
          return err;
      }

      err = stringify_value(output, size, capacity, value->array_val->items[i],
                            indent_level + 1, pretty);
      if (err != APP_SUCCESS)
        return err;
    }

    if (pretty && value->array_val->count > 0) {
      err = append_string(output, size, capacity, "\n");
      if (err != APP_SUCCESS)
        return err;
      err = append_indent(output, size, capacity, indent_level);
      if (err != APP_SUCCESS)
        return err;
    }

    return append_string(output, size, capacity, "]");
  }

  case JSON_TYPE_OBJECT: {
    err = append_string(output, size, capacity, "{");
    if (err != APP_SUCCESS)
      return err;

    for (size_t i = 0; i < value->object_val->count; i++) {
      if (i > 0) {
        err = append_string(output, size, capacity, ",");
        if (err != APP_SUCCESS)
          return err;
      }

      if (pretty) {
        err = append_string(output, size, capacity, "\n");
        if (err != APP_SUCCESS)
          return err;
        err = append_indent(output, size, capacity, indent_level + 1);
        if (err != APP_SUCCESS)
          return err;
      }

      err =
          stringify_string(output, size, capacity, value->object_val->keys[i]);
      if (err != APP_SUCCESS)
        return err;

      err = append_string(output, size, capacity, pretty ? ": " : ":");
      if (err != APP_SUCCESS)
        return err;

      err =
          stringify_value(output, size, capacity, value->object_val->values[i],
                          indent_level + 1, pretty);
      if (err != APP_SUCCESS)
        return err;
    }

    if (pretty && value->object_val->count > 0) {
      err = append_string(output, size, capacity, "\n");
      if (err != APP_SUCCESS)
        return err;
      err = append_indent(output, size, capacity, indent_level);
      if (err != APP_SUCCESS)
        return err;
    }

    return append_string(output, size, capacity, "}");
  }
  }

  return APP_ERROR_INTERNAL;
}

app_error json_stringify(char **output, const json_value_t *value) {
  if (!output || !value) {
    return APP_ERROR_INVALID_PARAM;
  }

  *output = NULL;
  size_t size = 0;
  size_t capacity = 0;

  return stringify_value(output, &size, &capacity, value, 0, false);
}

app_error json_stringify_pretty(char **output, const json_value_t *value) {
  if (!output || !value) {
    return APP_ERROR_INVALID_PARAM;
  }

  *output = NULL;
  size_t size = 0;
  size_t capacity = 0;

  return stringify_value(output, &size, &capacity, value, 0, true);
}