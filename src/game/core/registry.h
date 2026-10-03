#pragma once
#include <cstdint>

// HKEY value for a root key name ("HKEY_LOCAL_MACHINE", "HKEY_CURRENT_USER", "HKEY_CLASSES_ROOT", "HKEY_USERS");
// 0 for anything else.
uint32_t RegistryRootFromName(const char* rootName);

// Value `valueName` of HKEY_LOCAL_MACHINE\subKey as a zero-initialised heap copy (delete it with operator
// delete), or null if it does not exist or is empty.
uint8_t* ReadRegistryValue(const char* subKey, const char* valueName);

// REG_DWORD value of HKEY_LOCAL_MACHINE\subKey, 0 if missing or of another size.
uint32_t ReadRegistryDword(const char* subKey, const char* valueName);

// Sets a REG_DWORD in an existing key under the named root. Returns 1 on success.
int WriteRegistryDword(const char* rootName, const char* subKey, const char* valueName, uint32_t value);
