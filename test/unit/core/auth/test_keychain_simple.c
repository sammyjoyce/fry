#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <stdio.h>

int main() {
  printf("Testing basic keychain access...\n");

  // Create a simple query
  CFMutableDictionaryRef query =
      CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks);

  CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
  CFDictionarySetValue(query, kSecAttrService, CFSTR("test.service"));
  CFDictionarySetValue(query, kSecAttrAccount, CFSTR("test.account"));
  CFDictionarySetValue(query, kSecValueData, CFSTR("test.password"));
  CFDictionarySetValue(query, kSecAttrAccessible,
                       kSecAttrAccessibleWhenUnlocked);

  // Try to add item
  OSStatus status = SecItemAdd(query, NULL);
  printf("SecItemAdd status: %d\n", (int)status);

  if (status == errSecDuplicateItem) {
    printf("Item already exists, trying to delete first...\n");
    CFDictionaryRemoveValue(query, kSecValueData);
    status = SecItemDelete(query);
    printf("SecItemDelete status: %d\n", (int)status);
  }

  CFRelease(query);
  return 0;
}