#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "../core/error.h"

typedef enum {
  JSON_TYPE_NULL,
  JSON_TYPE_BOOL,
  JSON_TYPE_NUMBER,
  JSON_TYPE_STRING,
  JSON_TYPE_ARRAY,
  JSON_TYPE_OBJECT
} json_type_t;

typedef struct json_value json_value_t;
typedef struct json_object json_object_t;
typedef struct json_array json_array_t;

struct json_value {
  json_type_t type;
  union {
    bool bool_val;
    double number_val;
    char *string_val;
    json_array_t *array_val;
    json_object_t *object_val;
  };
};

struct json_array {
  json_value_t **items;
  size_t count;
  size_t capacity;
};

struct json_object {
  char **keys;
  json_value_t **values;
  size_t count;
  size_t capacity;
};

// Parsing
APP_NODISCARD app_error json_parse(json_value_t **value, const char *json);
void json_value_destroy(json_value_t *value);

// Object access
json_value_t *json_object_get(const json_object_t *obj, const char *key);
APP_NODISCARD app_error json_object_set(json_object_t *obj, const char *key,
                                        json_value_t *value);

// Array access
json_value_t *json_array_get(const json_array_t *arr, size_t index);
APP_NODISCARD app_error json_array_append(json_array_t *arr,
                                          json_value_t *value);

// Value creation
json_value_t *json_value_null(void);
json_value_t *json_value_bool(bool val);
json_value_t *json_value_number(double val);
json_value_t *json_value_string(const char *val);
json_value_t *json_value_array(void);
json_value_t *json_value_object(void);

// Serialization
APP_NODISCARD app_error json_stringify(char **output,
                                       const json_value_t *value);
APP_NODISCARD app_error json_stringify_pretty(char **output,
                                              const json_value_t *value);