/*
 * Error handling definitions for the application.
 *
 * Centralizes all error codes to ensure consistent error reporting across the
 * application. By using numeric codes with human-readable descriptions, we
 * enable both programmatic error handling and meaningful user feedback. The
 * error code ranges are designed to help quickly identify the error category
 * during debugging.
 */

#pragma once

#include "types.h"

// Error codes are grouped by category with reserved ranges to aid debugging.
// Each range represents a different layer of the application, making it easier
// to identify where failures occur without examining stack traces.
typedef enum {
  // Success (0): Indicates successful execution
  APP_SUCCESS = 0,

  // Input/configuration errors (1-9): User-correctable errors that typically
  // occur during startup or argument parsing. These errors indicate the user
  // needs to fix their input or configuration rather than a system failure.
  APP_ERROR_INVALID_ARG = 1,
  APP_ERROR_INVALID_COMMAND = 2,
  APP_ERROR_CONFIG = 3,
  APP_ERROR_CONFIG_PARSE = 4,
  APP_ERROR_CONFIG_INVALID = 5,
  APP_ERROR_MISSING_ARG = 6,
  APP_ERROR_UNKNOWN_OPTION = 7,
  APP_ERROR_USAGE = 8,

  // System errors (10-19): Critical failures that typically cannot be recovered
  // from without administrator intervention. These indicate resource
  // exhaustion,
  // permission issues, or internal bugs that require investigation.
  APP_ERROR_MEMORY = 10,
  APP_ERROR_IO = 11,
  APP_ERROR_PERMISSION = 12,
  APP_ERROR_INTERNAL = 13,
  APP_ERROR_THREADING = 14,
  APP_ERROR_RESOURCE = 15,
  APP_ERROR_SIGNAL = 16,
  APP_ERROR_NOT_FOUND = 17,
  APP_ERROR_SYSTEM = 18,
  APP_ERROR_BUFFER_TOO_SMALL = 19,

  // Data processing errors (20-29): Errors that occur during data validation
  // or processing. These might be recoverable depending on the context.
  APP_ERROR_INVALID_DATA = 20,
  APP_ERROR_PARSE_ERROR = 21,
  APP_ERROR_VALIDATION = 22,
  APP_ERROR_OVERFLOW = 23,
  APP_ERROR_UNDERFLOW = 24,
  APP_ERROR_OUT_OF_RANGE = 25,
  APP_ERROR_FILE_OPEN = 26,
  APP_ERROR_FILE_WRITE = 27,
  APP_ERROR_FILE_READ = 28,

  // Feature-specific errors (30+): Reserved for application-specific features
  // that may be added by users of this template.
  APP_ERROR_FEATURE_BASE = 30,

  // OAuth/Authentication errors (30-39)
  APP_ERROR_INVALID_TOKEN = 30,
  APP_ERROR_TOKEN_EXPIRED = 31,
  APP_ERROR_CRYPTO = 32,
  APP_ERROR_INVALID_PARAM = 33,
  APP_ERROR_ENV = 34,

  // Command errors (40-49)
  APP_ERROR_UNKNOWN_COMMAND = 40,
  APP_ERROR_NOT_IMPLEMENTED = 41,

  // JSON errors (50-59)
  APP_ERROR_PARSE = 50,

  // Keychain errors (60-69)
  APP_ERROR_KEYCHAIN_NOT_FOUND = 60,
  APP_ERROR_KEYCHAIN_ACCESS_DENIED = 61,
  APP_ERROR_KEYCHAIN_LOCKED = 62,
  APP_ERROR_KEYCHAIN_PLATFORM = 63,
  APP_ERROR_KEYCHAIN_UNSUPPORTED = 64,
  APP_ERROR_KEYCHAIN_CORRUPTED = 65,
  APP_ERROR_KEYCHAIN_STORAGE_FULL = 66,
  APP_ERROR_KEYCHAIN = 67,

  // Additional errors (70-79)
  APP_ERROR_NOT_INITIALIZED = 70,
  APP_ERROR_NOT_AUTHENTICATED = 71,
  APP_ERROR_CANCELLED = 72,
  APP_ERROR_INVALID_OPERATION = 73,
  APP_ERROR_SESSION_INACTIVE = 74,
  APP_ERROR_PARTIAL = 75,
  APP_ERROR_RATE_LIMITED = 76,
  APP_ERROR_API = 77,

  // I/O and timing errors (80-89)
  APP_ERROR_TIMEOUT = 80,
  APP_ERROR_EOF = 81,
  APP_ERROR_RETRY = 82,
  APP_ERROR_INCOMPLETE = 83,
} app_error;

// Get human-readable error description for user-facing messages.
// This function ensures users receive meaningful feedback instead of cryptic
// error codes, improving the debugging experience and reducing support burden.
APP_NODISCARD const char *app_strerror(app_error error_code);

// Alias for consistency with OAuth module
#define app_error_string(err) app_strerror(err)
