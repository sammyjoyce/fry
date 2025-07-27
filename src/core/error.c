/*
 * Error handling implementation for the application.
 *
 * Provides human-readable error messages for all error codes. We use a simple
 * switch statement rather than a lookup table to ensure compile-time
 * verification that all error codes have corresponding messages, preventing
 * oversight when adding new error codes.
 */

#include "error.h"

const char *app_strerror(app_error error_code) {
  switch (error_code) {
  // Success
  case APP_SUCCESS:
    return "Success";

  // Input/configuration errors: Messages guide users to fix their input.
  // We provide specific details about what went wrong to reduce debugging time.
  case APP_ERROR_INVALID_ARG:
    return "Invalid argument";
  case APP_ERROR_INVALID_COMMAND:
    return "Invalid or unknown command";
  case APP_ERROR_CONFIG:
    return "Configuration file error";
  case APP_ERROR_CONFIG_PARSE:
    return "Configuration file parse error";
  case APP_ERROR_CONFIG_INVALID:
    return "Configuration file has invalid values";
  case APP_ERROR_MISSING_ARG:
    return "Missing required argument";
  case APP_ERROR_UNKNOWN_OPTION:
    return "Unknown option";

  // System errors: Messages indicate serious problems requiring system-level
  // fixes. These are kept concise as they often appear in logs that
  // administrators review.
  case APP_ERROR_MEMORY:
    return "Memory allocation error";
  case APP_ERROR_IO:
    return "I/O error";
  case APP_ERROR_PERMISSION:
    return "Permission denied";
  case APP_ERROR_INTERNAL:
    return "Internal error";
  case APP_ERROR_THREADING:
    return "Thread/mutex error";
  case APP_ERROR_RESOURCE:
    return "Resource exhaustion";
  case APP_ERROR_SIGNAL:
    return "Signal handling error";
  case APP_ERROR_NOT_FOUND:
    return "File or resource not found";

  // Data processing errors: Messages help debug data validation issues.
  case APP_ERROR_INVALID_DATA:
    return "Invalid data format";
  case APP_ERROR_PARSE_ERROR:
    return "Parse error";
  case APP_ERROR_VALIDATION:
    return "Validation failed";
  case APP_ERROR_OVERFLOW:
    return "Numeric overflow";
  case APP_ERROR_UNDERFLOW:
    return "Numeric underflow";
  case APP_ERROR_OUT_OF_RANGE:
    return "Value out of range";

  // OAuth/Authentication errors
  case APP_ERROR_INVALID_TOKEN:
    return "Invalid token format or content";
  case APP_ERROR_TOKEN_EXPIRED:
    return "Token has expired";
  case APP_ERROR_CRYPTO:
    return "Cryptographic operation failed";
  case APP_ERROR_INVALID_PARAM:
    return "Invalid parameter";
  case APP_ERROR_ENV:
    return "Environment variable error";

  // Command errors
  case APP_ERROR_UNKNOWN_COMMAND:
    return "Unknown command";
  case APP_ERROR_NOT_IMPLEMENTED:
    return "Feature not yet implemented";

  // JSON errors
  case APP_ERROR_PARSE:
    return "JSON parse error";

  // Keychain errors
  case APP_ERROR_KEYCHAIN_NOT_FOUND:
    return "Token not found in keychain";
  case APP_ERROR_KEYCHAIN_ACCESS_DENIED:
    return "Access denied to keychain";
  case APP_ERROR_KEYCHAIN_LOCKED:
    return "Keychain is locked";
  case APP_ERROR_KEYCHAIN_PLATFORM:
    return "Platform-specific keychain error";
  case APP_ERROR_KEYCHAIN_UNSUPPORTED:
    return "Keychain operation not supported on this platform";
  case APP_ERROR_KEYCHAIN_CORRUPTED:
    return "Token data in keychain is corrupted";
  case APP_ERROR_KEYCHAIN_STORAGE_FULL:
    return "Keychain storage is full";
  case APP_ERROR_KEYCHAIN:
    return "Keychain error";

  // Additional errors
  case APP_ERROR_NOT_INITIALIZED:
    return "Not initialized";
  case APP_ERROR_NOT_AUTHENTICATED:
    return "Not authenticated";
  case APP_ERROR_CANCELLED:
    return "Operation cancelled";
  case APP_ERROR_INVALID_OPERATION:
    return "Invalid operation";
  case APP_ERROR_SESSION_INACTIVE:
    return "Session is not active";
  case APP_ERROR_PARTIAL:
    return "Operation partially completed";
  case APP_ERROR_RATE_LIMITED:
    return "Rate limited";
  case APP_ERROR_API:
    return "API error";

  default:
    return "Unknown error";
  }
}
