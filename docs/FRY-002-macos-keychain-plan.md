# FRY-002: macOS Keychain Backend Implementation Plan

## Overview
Implement secure token storage using macOS Keychain Services API to store OAuth tokens with proper access control and encryption.

## Technical Background

### macOS Keychain Services
- **Framework**: Security.framework
- **API Level**: Core Foundation C API
- **Storage Type**: Generic Password items (kSecClassGenericPassword)
- **Encryption**: Automatic AES-256 encryption by the OS
- **Access Control**: Per-app access with user authorization

### Key Concepts
1. **Keychain Items**: Stored as dictionary attributes
2. **Service Name**: Identifies our app (e.g., "com.anthropic.fry")
3. **Account Name**: Maps to our account_id
4. **Password Data**: Stores serialized token JSON
5. **Access Groups**: For sharing between apps (not needed for fry)

## Implementation Design

### File Structure
```
src/core/
├── keychain.h          (existing)
├── keychain.c          (existing - update)
└── keychain_macos.c    (new)
```

### Data Storage Strategy
Store OAuth tokens as JSON in keychain "password" field:
```json
{
  "access_token": "...",
  "refresh_token": "...",
  "expires_at": 1234567890,
  "scopes": ["read", "write"],
  "subscription_type": "pro"
}
```

### Security Considerations
1. **Service Identifier**: Use reverse DNS notation
2. **Item Attributes**: Mark as user-facing for Keychain Access.app visibility
3. **Access Control**: Default to "when unlocked" protection
4. **Error Handling**: Handle user denial gracefully

## Detailed Implementation Steps

### Step 1: Create keychain_macos.c Structure
```c
#ifdef __APPLE__
#include <Security/Security.h>
#include <CoreFoundation/CoreFoundation.h>
#include "keychain_internal.h"

// Constants
static const char* KEYCHAIN_SERVICE = "com.anthropic.fry";
static const char* KEYCHAIN_LABEL = "Fry CLI OAuth Token";

// Helper functions
static CFStringRef create_cf_string(const char* str);
static char* cf_string_to_c_string(CFStringRef cf_str);
static app_error translate_osstatus(OSStatus status);
```

### Step 2: Implement Helper Functions

#### 2.1 String Conversion Helpers
```c
// Convert C string to CFString
static CFStringRef create_cf_string(const char* str) {
    return CFStringCreateWithCString(NULL, str, kCFStringEncodingUTF8);
}

// Convert CFString to C string (caller must free)
static char* cf_string_to_c_string(CFStringRef cf_str) {
    CFIndex length = CFStringGetLength(cf_str);
    CFIndex max_size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    char* buffer = malloc(max_size);
    if (CFStringGetCString(cf_str, buffer, max_size, kCFStringEncodingUTF8)) {
        return buffer;
    }
    free(buffer);
    return NULL;
}
```

#### 2.2 Error Translation
```c
static app_error translate_osstatus(OSStatus status) {
    switch (status) {
        case errSecSuccess:
            return APP_SUCCESS;
        case errSecItemNotFound:
            return APP_ERROR_KEYCHAIN_NOT_FOUND;
        case errSecAuthFailed:
        case errSecUserCanceled:
            return APP_ERROR_KEYCHAIN_ACCESS_DENIED;
        case errSecInteractionNotAllowed:
            return APP_ERROR_KEYCHAIN_LOCKED;
        case errSecDuplicateItem:
            return APP_SUCCESS; // We'll update existing
        default:
            return APP_ERROR_KEYCHAIN_PLATFORM;
    }
}
```

### Step 3: Implement Core Functions

#### 3.1 Store Token
```c
app_error keychain_macos_store_token(const char* account_id, 
                                    const app_oauth_token_t* token) {
    // 1. Serialize token to JSON
    char* json_data = serialize_token_to_json(token);
    
    // 2. Create keychain query dictionary
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    // Set item class and attributes
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, CFSTR(KEYCHAIN_SERVICE));
    CFDictionarySetValue(query, kSecAttrAccount, create_cf_string(account_id));
    
    // 3. Try to update existing item first
    CFMutableDictionaryRef update = CFDictionaryCreateMutable(...);
    CFDictionarySetValue(update, kSecValueData, CFDataCreate(...));
    
    OSStatus status = SecItemUpdate(query, update);
    
    // 4. If not found, add new item
    if (status == errSecItemNotFound) {
        CFDictionarySetValue(query, kSecValueData, ...);
        CFDictionarySetValue(query, kSecAttrLabel, CFSTR(KEYCHAIN_LABEL));
        status = SecItemAdd(query, NULL);
    }
    
    // 5. Cleanup and return
    CFRelease(query);
    CFRelease(update);
    free(json_data);
    
    return translate_osstatus(status);
}
```

#### 3.2 Retrieve Token
```c
app_error keychain_macos_retrieve_token(const char* account_id,
                                       app_oauth_token_t** token) {
    // 1. Create search query
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(...);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, CFSTR(KEYCHAIN_SERVICE));
    CFDictionarySetValue(query, kSecAttrAccount, create_cf_string(account_id));
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);
    
    // 2. Execute search
    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching(query, &result);
    
    // 3. Parse result if found
    if (status == errSecSuccess && result) {
        CFDataRef data = (CFDataRef)result;
        const char* json_str = (const char*)CFDataGetBytePtr(data);
        CFIndex length = CFDataGetLength(data);
        
        // Parse JSON and create token
        *token = parse_token_from_json(json_str, length);
        
        CFRelease(result);
    }
    
    CFRelease(query);
    return translate_osstatus(status);
}
```

#### 3.3 Delete Token
```c
app_error keychain_macos_delete_token(const char* account_id) {
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(...);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, CFSTR(KEYCHAIN_SERVICE));
    CFDictionarySetValue(query, kSecAttrAccount, create_cf_string(account_id));
    
    OSStatus status = SecItemDelete(query);
    CFRelease(query);
    
    // Not found is success for delete
    if (status == errSecItemNotFound) {
        return APP_SUCCESS;
    }
    
    return translate_osstatus(status);
}
```

#### 3.4 List Accounts
```c
app_error keychain_macos_list_accounts(char*** account_ids, size_t* count) {
    // 1. Create query for all items with our service
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(...);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, CFSTR(KEYCHAIN_SERVICE));
    CFDictionarySetValue(query, kSecReturnAttributes, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
    
    // 2. Execute search
    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching(query, &result);
    
    // 3. Extract account names from results
    if (status == errSecSuccess && result) {
        CFArrayRef items = (CFArrayRef)result;
        CFIndex item_count = CFArrayGetCount(items);
        
        *count = item_count;
        *account_ids = calloc(item_count, sizeof(char*));
        
        for (CFIndex i = 0; i < item_count; i++) {
            CFDictionaryRef item = CFArrayGetValueAtIndex(items, i);
            CFStringRef account = CFDictionaryGetValue(item, kSecAttrAccount);
            (*account_ids)[i] = cf_string_to_c_string(account);
        }
        
        CFRelease(result);
    }
    
    CFRelease(query);
    return translate_osstatus(status);
}
```

### Step 4: Integration with keychain.c

#### 4.1 Update keychain structure
```c
struct app_keychain {
    void* platform_handle;  // Not used for macOS
    bool is_mock;
    json_object_t* mock_storage;  // Only for mock
    #ifdef __APPLE__
    bool use_macos_keychain;
    #endif
};
```

#### 4.2 Update app_keychain_init
```c
app_error app_keychain_init(app_keychain_t** keychain) {
    if (!keychain) {
        return APP_ERROR_INVALID_ARG;
    }

    #ifdef __APPLE__
    // Try to use macOS keychain
    app_keychain_t* kc = app_calloc(1, sizeof(app_keychain_t));
    kc->use_macos_keychain = true;
    kc->is_mock = false;
    
    // Test keychain access
    OSStatus status = SecKeychainCopyDefault(NULL);
    if (status == errSecSuccess) {
        *keychain = kc;
        LOG_INFO("Using macOS Keychain for token storage");
        return APP_SUCCESS;
    }
    
    // Fall back to mock if keychain unavailable
    app_free(kc);
    LOG_WARNING("macOS Keychain unavailable, using mock storage");
    #endif
    
    return app_keychain_create_mock(keychain);
}
```

#### 4.3 Update dispatch functions
```c
app_error app_keychain_store_token(app_keychain_t* keychain,
                                   const char* account_id,
                                   const app_oauth_token_t* token) {
    if (!keychain) {
        return APP_ERROR_INVALID_ARG;
    }

    #ifdef __APPLE__
    if (keychain->use_macos_keychain) {
        return keychain_macos_store_token(account_id, token);
    }
    #endif

    if (keychain->is_mock) {
        return mock_store_token(keychain, account_id, token);
    }

    return APP_ERROR_KEYCHAIN_UNSUPPORTED;
}
```

### Step 5: Build System Updates

#### 5.1 Update build.zig
```zig
// Add platform-specific source
if (target.isDarwin()) {
    exe.addCSourceFile(.{
        .file = b.path("src/core/keychain_macos.c"),
        .flags = flags.items,
    });
    exe.linkFramework("Security");
    exe.linkFramework("CoreFoundation");
}
```

### Step 6: Testing Strategy

#### 6.1 Unit Tests
```c
// test/keychain_macos_test.c
void test_macos_keychain_operations() {
    // Test with real keychain (requires user interaction)
    if (getenv("FRY_TEST_REAL_KEYCHAIN")) {
        test_real_keychain();
    } else {
        printf("Skipping real keychain tests (set FRY_TEST_REAL_KEYCHAIN=1)\n");
    }
}

void test_real_keychain() {
    // Store, retrieve, list, delete operations
    // Handle user authorization prompts
}
```

#### 6.2 Integration Tests
- Test fallback to mock when keychain unavailable
- Test error handling for denied access
- Test with multiple accounts
- Test token update (overwrite)

### Step 7: User Experience Considerations

#### 7.1 First-Time Access
- User will see system prompt: "fry wants to use your confidential information stored in your keychain"
- Add helpful message before operations that trigger this

#### 7.2 Keychain Access App
- Tokens visible in Keychain Access.app under "Passwords"
- Service: "com.anthropic.fry"
- Account: The account ID
- Show as "Fry CLI OAuth Token"

#### 7.3 Error Messages
```c
case APP_ERROR_KEYCHAIN_ACCESS_DENIED:
    fprintf(stderr, "Access to keychain was denied. Please allow fry to access your keychain when prompted.\n");
    break;
case APP_ERROR_KEYCHAIN_LOCKED:
    fprintf(stderr, "Keychain is locked. Please unlock your keychain and try again.\n");
    break;
```

## Implementation Checklist

- [ ] Create keychain_macos.c with Security.framework imports
- [ ] Implement string conversion helpers
- [ ] Implement error translation function
- [ ] Implement token serialization/deserialization
- [ ] Implement store_token with update-or-create logic
- [ ] Implement retrieve_token with proper error handling
- [ ] Implement delete_token
- [ ] Implement list_accounts
- [ ] Update keychain.c to dispatch to macOS implementation
- [ ] Update build.zig to link Security.framework
- [ ] Create comprehensive tests
- [ ] Test on macOS 12, 13, and 14
- [ ] Document user-facing behavior
- [ ] Handle edge cases (locked keychain, user denial)

## Potential Issues & Solutions

### Issue 1: Keychain Access Prompts
**Problem**: Repeated authorization prompts annoy users
**Solution**: Store all tokens under same service name, request access once

### Issue 2: JSON Serialization Size
**Problem**: Large scope arrays might exceed keychain item size limits
**Solution**: Implement compression or store scopes separately if needed

### Issue 3: Keychain Locked at Login
**Problem**: Can't access tokens when keychain is locked
**Solution**: Provide clear error message, suggest unlocking keychain

### Issue 4: Cross-Machine Sync
**Problem**: Users might expect tokens to sync via iCloud Keychain
**Solution**: Use local keychain only, document this limitation

## Success Criteria

1. ✅ Tokens stored securely in macOS Keychain
2. ✅ No passwords/tokens visible in process memory
3. ✅ Graceful fallback to mock storage if needed
4. ✅ Clear error messages for all failure modes
5. ✅ Tokens visible in Keychain Access.app
6. ✅ No memory leaks (test with leaks tool)
7. ✅ Works on macOS 12.0+
8. ✅ Handles all Core Foundation memory management correctly

## Known Limitations

### macOS 15+ Unsigned Binary Restrictions
On macOS 15 and later, unsigned command-line tools face restrictions when accessing the keychain:
- **Issue**: `errSecWrPerm` (-61) errors when trying to store items
- **Cause**: Enhanced security requires proper code signing for keychain write access
- **Solution**: The implementation automatically falls back to encrypted mock storage
- **Fix**: To use real keychain storage, the binary must be properly code signed

This is handled gracefully in the implementation with clear warning messages.

## References

- [Apple Keychain Services Documentation](https://developer.apple.com/documentation/security/keychain_services)
- [SecItem API Reference](https://developer.apple.com/documentation/security/keychain_services/keychain_items)
- [Core Foundation Memory Management](https://developer.apple.com/library/archive/documentation/CoreFoundation/Conceptual/CFMemoryMgmt/CFMemoryMgmt.html)