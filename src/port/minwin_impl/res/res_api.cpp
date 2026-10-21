// Win32-named entry points on the PE resource reader:
//   kernel32  LoadLibraryA FreeLibrary GetModuleHandleA FindResourceA LoadResource LockResource FreeResource
//   SizeofResource user32    LoadStringA LoadCursorA LoadIconA LoadAcceleratorsA version   GetFileVersionInfoSizeA
//   GetFileVersionInfoA VerQueryValueA ; kernel32 VerLanguageNameA
// HMODULE is a Module*, HRSRC an entry record, HGLOBAL the resource's data pointer. HCURSOR / HICON are CursorObject*,
// HACCEL a vector<Accelerator>*; modules.h gives the window system the typed view.
#include "port/minwin/kernel32.h"
#include "port/minwin/user32.h"
#include "port/minwin/version.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <map>
#include <set>

#include "../config/config.h"
#include "modules.h"
#include "path.h"

using namespace mw::res;

namespace
{

struct ResourceRecord {
  Module* module;
  Entry entry;
};

std::map<const uint8_t*, ResourceRecord*>& Records()
{
  static std::map<const uint8_t*, ResourceRecord*> records;
  return records;
}

#if defined(_WIN32)
extern "C" const char __ImageBase;
#endif

// HMODULE is a Module*, except for the instance handle the C runtime hands WinMain on Windows (the real image base of
// the process): that one stands for the main module as well.
Module* ModuleOf(HMODULE h)
{
  if (!h) {
    return MainModule();
  }
#if defined(_WIN32)
  if (reinterpret_cast<const char*>(h) == &__ImageBase) {
    return MainModule();
  }
#endif
  return reinterpret_cast<Module*>(h);
}

std::map<std::pair<const Module*, std::string>, CursorObject*>& CursorCache()
{
  static std::map<std::pair<const Module*, std::string>, CursorObject*> cache;
  return cache;
}

std::string CacheKey(const ResId& id)
{
  return id.isString ? "s:" + id.name : "i:" + std::to_string(id.id);
}

HICON LoadImageObject(HINSTANCE hInstance, LPCSTR name, bool cursor)
{
  ResId id = ResId::FromPointer(name);
  if (cursor && !hInstance && !id.isString) {
    auto key = std::make_pair(static_cast<const Module*>(nullptr), CacheKey(id));
    auto it = CursorCache().find(key);
    if (it != CursorCache().end()) {
      return reinterpret_cast<HICON>(it->second);
    }
    auto* system = new CursorObject;
    system->systemId = id.id;
    CursorCache()[key] = system;
    return reinterpret_cast<HICON>(system);
  }
  Module* module = ModuleOf(hInstance);
  if (!module) {
    return nullptr;
  }
  auto key = std::make_pair(static_cast<const Module*>(module), std::string(cursor ? "c:" : "i:") + CacheKey(id));
  auto it = CursorCache().find(key);
  if (it != CursorCache().end()) {
    return reinterpret_cast<HICON>(it->second);
  }
  auto* object = new CursorObject;
  bool ok = cursor ? LoadCursorImage(*module, id, &object->image) : LoadIconImage(*module, id, &object->image);
  if (!ok) {
    delete object;
    return nullptr;
  }
  CursorCache()[key] = object;
  return reinterpret_cast<HICON>(object);
}

std::map<std::pair<const Module*, std::string>, std::vector<Accelerator>*>& AccelCache()
{
  static std::map<std::pair<const Module*, std::string>, std::vector<Accelerator>*> cache;
  return cache;
}

struct VersionFile {
  std::string path;
};

bool ReadVersionBlob(const char* filename, std::vector<uint8_t>* out)
{
  std::string path = ResolvePath(filename);
  Module* m = Module::Open(path);
  if (!m && !strpbrk(filename, "/\\")) {
    std::string install = mw::config::Global().InstallDir();
    if (!install.empty()) {
      m = Module::Open(ResolvePath(JoinPath(install, filename)));
    }
  }
  if (!m) {
    return false;
  }
  Entry e;
  bool ok = false;
  std::vector<ResId> names = m->ListNames(ResId::Int(kVersion));
  if (!names.empty() && m->Find(ResId::Int(kVersion), names.front(), &e)) {
    out->assign(e.data, e.data + e.size);
    ok = true;
  }
  delete m;
  return ok;
}

// Text values are stored as UTF-16 in the block; VerQueryValueA hands out ANSI, converted in place the first time.
std::set<const uint8_t*>& ConvertedValues()
{
  static std::set<const uint8_t*> converted;
  return converted;
}

} // namespace

namespace
{
#if defined(_WIN32)
extern "C" __declspec(dllimport) unsigned long long __stdcall GetTickCount64(void);
#endif

void Trace(const char* format, ...)
{
  static const char* path = getenv("MW_RES_TRACE");
  if (!path) {
    return;
  }
  FILE* log = fopen(path, "a");
  if (!log) {
    return;
  }
#if defined(_WIN32)
  fprintf(log, "[%llu] ", GetTickCount64());
#endif
  va_list args;
  va_start(args, format);
  vfprintf(log, format, args);
  va_end(args);
  fclose(log);
}
} // namespace

HMODULE REB_STDCALL reb::compat::LoadLibraryA(LPCSTR lpLibFileName)
{
  HMODULE m = reinterpret_cast<HMODULE>(LoadModule(lpLibFileName ? lpLibFileName : ""));
  Trace("LoadLibraryA(%s) -> %p\n", lpLibFileName ? lpLibFileName : "(null)", static_cast<void*>(m));
  return m;
}

BOOL REB_STDCALL reb::compat::FreeLibrary(HMODULE hLibModule)
{
  return FreeModule(reinterpret_cast<Module*>(hLibModule)) ? TRUE : FALSE;
}

HMODULE REB_STDCALL reb::compat::GetModuleHandleA(LPCSTR lpModuleName)
{
  if (!lpModuleName) {
    return reinterpret_cast<HMODULE>(MainModule());
  }
  return reinterpret_cast<HMODULE>(FindLoadedModule(lpModuleName));
}

HRSRC REB_STDCALL reb::compat::FindResourceA(HMODULE hModule, LPCSTR lpName, LPCSTR lpType)
{
  Module* module = ModuleOf(hModule);
  if (!module) {
    Trace("FindResourceA(null module %p)\n", static_cast<void*>(hModule));
    return nullptr;
  }
  Entry entry;
  Trace("FindResourceA(%p, %p, %p)\n", static_cast<void*>(hModule), static_cast<const void*>(lpName),
        static_cast<const void*>(lpType));
  if (!module->Find(ResId::FromPointer(lpType), ResId::FromPointer(lpName), &entry)) {
    Trace("FindResourceA(%p, %p, %p) not found\n", static_cast<void*>(hModule), static_cast<const void*>(lpName),
          static_cast<const void*>(lpType));
    return nullptr;
  }
  auto& records = Records();
  auto it = records.find(entry.data);
  if (it != records.end() && it->second->module == module && it->second->entry.size == entry.size) {
    return reinterpret_cast<HRSRC>(it->second);
  }
  auto* r = new ResourceRecord{module, entry};
  records[entry.data] = r;
  return reinterpret_cast<HRSRC>(r);
}

HGLOBAL REB_STDCALL reb::compat::LoadResource(HMODULE, HRSRC hResInfo)
{
  if (!hResInfo) {
    return nullptr;
  }
  return const_cast<uint8_t*>(reinterpret_cast<ResourceRecord*>(hResInfo)->entry.data);
}

LPVOID REB_STDCALL reb::compat::LockResource(HGLOBAL hResData)
{
  return hResData;
}

BOOL REB_STDCALL reb::compat::FreeResource(HGLOBAL)
{
  return FALSE;
}

DWORD REB_STDCALL reb::compat::SizeofResource(HMODULE, HRSRC hResInfo)
{
  return hResInfo ? reinterpret_cast<ResourceRecord*>(hResInfo)->entry.size : 0;
}

int REB_STDCALL reb::compat::LoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int cchBufferMax)
{
  Module* module = ModuleOf(hInstance);
  std::string text;
  if (!lpBuffer || cchBufferMax <= 0) {
    return 0;
  }
  if (!module || !LoadStringText(*module, uID, &text)) {
    lpBuffer[0] = 0;
    return 0;
  }
  size_t n = text.size();
  if (n > static_cast<size_t>(cchBufferMax) - 1) {
    n = cchBufferMax - 1;
  }
  memcpy(lpBuffer, text.data(), n);
  lpBuffer[n] = 0;
  return static_cast<int>(n);
}

HCURSOR REB_STDCALL reb::compat::LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName)
{
  return LoadImageObject(hInstance, lpCursorName, true);
}

HICON REB_STDCALL reb::compat::LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName)
{
  return LoadImageObject(hInstance, lpIconName, false);
}

HACCEL REB_STDCALL reb::compat::LoadAcceleratorsA(HINSTANCE hInstance, LPCSTR lpTableName)
{
  Module* module = ModuleOf(hInstance);
  if (!module) {
    return nullptr;
  }
  ResId id = ResId::FromPointer(lpTableName);
  auto key = std::make_pair(static_cast<const Module*>(module), CacheKey(id));
  auto it = AccelCache().find(key);
  if (it != AccelCache().end()) {
    return reinterpret_cast<HACCEL>(it->second);
  }
  auto* table = new std::vector<Accelerator>;
  if (!LoadAcceleratorTable(*module, id, table)) {
    delete table;
    return nullptr;
  }
  AccelCache()[key] = table;
  return reinterpret_cast<HACCEL>(table);
}

DWORD REB_STDCALL reb::compat::GetFileVersionInfoSizeA(LPCSTR lptstrFilename, LPDWORD lpdwHandle)
{
  if (lpdwHandle) {
    *lpdwHandle = 0;
  }
  std::vector<uint8_t> blob;
  return ReadVersionBlob(lptstrFilename, &blob) ? static_cast<DWORD>(blob.size()) : 0;
}

BOOL REB_STDCALL reb::compat::GetFileVersionInfoA(LPCSTR lptstrFilename, DWORD, DWORD dwLen, LPVOID lpData)
{
  std::vector<uint8_t> blob;
  if (!lpData || !ReadVersionBlob(lptstrFilename, &blob) || dwLen < blob.size()) {
    return FALSE;
  }
  memcpy(lpData, blob.data(), blob.size());
  return TRUE;
}

BOOL REB_STDCALL reb::compat::VerQueryValueA(LPCVOID pBlock, LPCSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen)
{
  if (!pBlock || !lpSubBlock || !lplpBuffer) {
    return FALSE;
  }
  const auto* data = static_cast<const uint8_t*>(pBlock);
  size_t total = data[0] | (data[1] << 8);
  VersionBlock root;
  if (!ParseVersionInfo(data, total, &root)) {
    return FALSE;
  }
  std::string path = lpSubBlock;
  const VersionBlock* node = QueryVersionBlock(root, path);
  if (!node) {
    return FALSE;
  }
  if (path.empty() || path == "\\") {
    *lplpBuffer = const_cast<uint8_t*>(root.value);
    if (puLen) {
      *puLen = root.valueLength;
    }
    return root.value != nullptr;
  }
  if (!node->value) {
    return FALSE;
  }
  auto* value = const_cast<uint8_t*>(node->value);
  if (node->isText) {
    if (!ConvertedValues().count(value)) {
      uint32_t n = node->valueLength;
      for (uint32_t i = 0; i < n; ++i) {
        auto c = static_cast<uint16_t>(value[i * 2] | (value[i * 2 + 1] << 8));
        value[i] = c < 0x100 ? static_cast<uint8_t>(c) : '?';
      }
      ConvertedValues().insert(value);
    }
  }
  *lplpBuffer = value;
  if (puLen) {
    *puLen = node->valueLength;
  }
  return TRUE;
}

DWORD REB_STDCALL reb::compat::VerLanguageNameA(DWORD wLang, LPSTR szLang, DWORD cchLang)
{
  std::string name = LanguageName(wLang);
  if (!szLang || cchLang == 0) {
    return static_cast<DWORD>(name.size());
  }
  size_t n = name.size() < cchLang - 1 ? name.size() : cchLang - 1;
  memcpy(szLang, name.data(), n);
  szLang[n] = 0;
  return static_cast<DWORD>(n);
}

namespace mw::res
{

const CursorObject* CursorFromHandle(const void* handle)
{
  return static_cast<const CursorObject*>(handle);
}

const std::vector<Accelerator>* AcceleratorsFromHandle(const void* handle)
{
  return static_cast<const std::vector<Accelerator>*>(handle);
}

} // namespace mw::res
