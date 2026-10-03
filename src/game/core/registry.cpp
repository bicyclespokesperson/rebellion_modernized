#include "core/registry.h"

#include <windows.h>

#include <cstring>
#include <new>

namespace {

bool NamesEqual(const char* a, const char* b) {
    while (*a == *b) {
        if (*a == '\0') return true;
        ++a;
        ++b;
    }
    return false;
}

}  // namespace

uint32_t RegistryRootFromName(const char* rootName) {
    if (NamesEqual(rootName, "HKEY_LOCAL_MACHINE")) return 0x80000002;
    if (NamesEqual(rootName, "HKEY_CURRENT_USER")) return 0x80000001;
    if (NamesEqual(rootName, "HKEY_CLASSES_ROOT")) return 0x80000000;
    if (NamesEqual(rootName, "HKEY_USERS")) return 0x80000003;
    return 0;
}

uint8_t* ReadRegistryValue(const char* subKey, const char* valueName) {
    HKEY key = nullptr;
    uint8_t* data = nullptr;
    DWORD type = 0;
    DWORD size = 0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ, &key) == ERROR_SUCCESS) {
        if (RegQueryValueExA(key, valueName, nullptr, &type, nullptr, &size) == ERROR_SUCCESS && size != 0) {
            data = static_cast<uint8_t*>(operator new(size));
            if (data) {
                std::memset(data, 0, size);
                if (RegQueryValueExA(key, valueName, nullptr, &type, data, &size) != ERROR_SUCCESS) {
                    operator delete(data);
                    data = nullptr;
                }
            }
        }
        RegCloseKey(key);
    }
    return data;
}

uint32_t ReadRegistryDword(const char* subKey, const char* valueName) {
    HKEY key = nullptr;
    DWORD value = 0;
    DWORD type = REG_DWORD;
    DWORD size = 0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ, &key) == ERROR_SUCCESS) {
        if (RegQueryValueExA(key, valueName, nullptr, &type, nullptr, &size) == ERROR_SUCCESS && size == 4) {
            value = 0;
            RegQueryValueExA(key, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size);
        }
        RegCloseKey(key);
    }
    return value;
}

int WriteRegistryDword(const char* rootName, const char* subKey, const char* valueName, uint32_t value) {
    int success = 0;
    HKEY root = reinterpret_cast<HKEY>(static_cast<uintptr_t>(RegistryRootFromName(rootName)));
    HKEY key = nullptr;
    if (root && RegOpenKeyExA(root, subKey, 0, KEY_WRITE, &key) == ERROR_SUCCESS) {
        if (RegSetValueExA(key, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), 4) == ERROR_SUCCESS)
            success = 1;
        RegCloseKey(key);
    }
    return success;
}
