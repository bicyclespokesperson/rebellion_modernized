// advapi32 subset on the config file: RegOpenKeyExA / RegQueryValueExA / RegSetValueExA / RegCloseKey / GetUserNameA.
// The only key that exists is HKLM\<kRegistrySubKey>; its values live in the config file's [registry] section.
#define MINWIN_IMPLEMENTATION 1
#include "port/minwin/advapi32.h"

#include <cstdlib>
#include <cstring>

#include "config.h"

namespace
{

constexpr LSTATUS kErrorFileNotFound = 2;
constexpr LSTATUS kErrorMoreData = 234;
constexpr LSTATUS kErrorInvalidParameter = 87;
constexpr DWORD kRegSz = 1;

struct HKEY__game_key
{
  int unused;
};
HKEY__game_key g_gameKey;

bool IsGameKey(HKEY key)
{
  return reinterpret_cast<void*>(key) == &g_gameKey;
}

bool IsGameSubKey(LPCSTR subKey)
{
  std::string s = subKey ? subKey : "";
  while (!s.empty() && (s.front() == '\\' || s.front() == '/'))
    s.erase(s.begin());
  while (!s.empty() && (s.back() == '\\' || s.back() == '/'))
    s.pop_back();
  return mw::config::EqualsNoCase(s, mw::config::kRegistrySubKey);
}

} // namespace

extern "C" {

LSTATUS REB_STDCALL RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD, REGSAM, PHKEY phkResult)
{
  if (!phkResult)
    return kErrorInvalidParameter;
  *phkResult = nullptr;
  if (hKey != HKEY_LOCAL_MACHINE || !IsGameSubKey(lpSubKey))
    return kErrorFileNotFound;
  *phkResult = reinterpret_cast<HKEY>(&g_gameKey);
  return ERROR_SUCCESS;
}

LSTATUS REB_STDCALL RegCloseKey(HKEY hKey)
{
  return IsGameKey(hKey) ? ERROR_SUCCESS : ERROR_INVALID_HANDLE;
}

LSTATUS REB_STDCALL
RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
  if (!IsGameKey(hKey))
    return ERROR_INVALID_HANDLE;
  mw::config::RegistryValue v;
  if (!mw::config::Global().GetRegistryValue(lpValueName ? lpValueName : "", &v))
    return kErrorFileNotFound;
  if (lpType)
    *lpType = static_cast<DWORD>(v.type);
  uint8_t dword[4] = {static_cast<uint8_t>(v.dword), static_cast<uint8_t>(v.dword >> 8),
                      static_cast<uint8_t>(v.dword >> 16), static_cast<uint8_t>(v.dword >> 24)};
  const uint8_t* bytes =
    v.type == mw::config::ValueType::Dword ? dword : reinterpret_cast<const uint8_t*>(v.text.c_str());
  DWORD size = v.type == mw::config::ValueType::Dword ? 4 : static_cast<DWORD>(v.text.size() + 1);
  if (!lpcbData)
    return lpData ? kErrorInvalidParameter : ERROR_SUCCESS;
  if (!lpData)
  {
    *lpcbData = size;
    return ERROR_SUCCESS;
  }
  if (*lpcbData < size)
  {
    *lpcbData = size;
    return kErrorMoreData;
  }
  memcpy(lpData, bytes, size);
  *lpcbData = size;
  return ERROR_SUCCESS;
}

LSTATUS REB_STDCALL RegSetValueExA(HKEY hKey, LPCSTR lpValueName, DWORD, DWORD dwType, const BYTE* lpData, DWORD cbData)
{
  if (!IsGameKey(hKey))
    return ERROR_INVALID_HANDLE;
  mw::config::RegistryValue v;
  if (dwType == REG_DWORD)
  {
    if (cbData < 4 || !lpData)
      return kErrorInvalidParameter;
    v.type = mw::config::ValueType::Dword;
    v.dword = lpData[0] | (lpData[1] << 8) | (lpData[2] << 16) | (static_cast<uint32_t>(lpData[3]) << 24);
  }
  else if (dwType == kRegSz)
  {
    v.type = mw::config::ValueType::String;
    size_t n = 0;
    while (lpData && n < cbData && lpData[n])
      ++n;
    v.text.assign(reinterpret_cast<const char*>(lpData), n);
  }
  else
  {
    return kErrorInvalidParameter;
  }
  mw::config::Global().SetRegistryValue(lpValueName ? lpValueName : "", v);
  mw::config::Global().Save(mw::config::GlobalPath());
  return ERROR_SUCCESS;
}

BOOL REB_STDCALL GetUserNameA(LPSTR lpBuffer, LPDWORD pcbBuffer)
{
  const char* name = getenv("USER");
  if (!name)
    name = getenv("USERNAME");
  if (!name || !*name)
    name = "player";
  auto need = static_cast<DWORD>(strlen(name) + 1);
  if (!pcbBuffer || !lpBuffer || *pcbBuffer < need)
  {
    if (pcbBuffer)
      *pcbBuffer = need;
    return FALSE;
  }
  memcpy(lpBuffer, name, need);
  *pcbBuffer = need;
  return TRUE;
}

} // extern "C"
