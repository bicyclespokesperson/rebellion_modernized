#include "modules.h"

#include <map>

#include "../config/config.h"
#include "path.h"

namespace mw::res
{

namespace
{

std::map<std::string, Module*>& Loaded()
{
  static std::map<std::string, Module*> loaded;
  return loaded;
}

std::string Key(const std::string& p)
{
  std::string k = p;
  for (char& c : k)
  {
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c + 32);
    if (c == '\\')
      c = '/';
  }
  return k;
}

Module* OpenCandidate(const std::string& nativePath)
{
  auto& loaded = Loaded();
  auto it = loaded.find(Key(nativePath));
  if (it != loaded.end())
  {
    ++it->second->refs;
    return it->second;
  }
  Module* m = Module::Open(nativePath);
  if (m)
    loaded[Key(nativePath)] = m;
  return m;
}

std::string g_mainModulePath;
Module* g_mainModule = nullptr;

} // namespace

Module* LoadModule(const std::string& name)
{
  bool hasDir = name.find_first_of("/\\") != std::string::npos || (name.size() > 1 && name[1] == ':');
  if (Module* m = OpenCandidate(ResolvePath(name)))
    return m;
  if (!hasDir)
  {
    std::string install = mw::config::Global().InstallDir();
    if (!install.empty())
    {
      if (Module* m = OpenCandidate(ResolvePath(JoinPath(install, name))))
        return m;
    }
  }
  return nullptr;
}

bool FreeModule(Module* module)
{
  if (!module)
    return false;
  if (module == g_mainModule)
    return true;
  if (--module->refs > 0)
    return true;
  auto& loaded = Loaded();
  for (auto it = loaded.begin(); it != loaded.end(); ++it)
    if (it->second == module)
    {
      loaded.erase(it);
      break;
    }
  delete module;
  return true;
}

void SetMainModule(const std::string& nativePath)
{
  g_mainModulePath = nativePath;
  g_mainModule = nullptr;
}

Module* MainModule()
{
  if (!g_mainModule)
  {
    std::string path = g_mainModulePath;
    if (path.empty())
      path = ResolvePath(JoinPath(mw::config::Global().InstallDir(), "REBEXE.EXE"));
    g_mainModule = OpenCandidate(path);
  }
  return g_mainModule;
}

Module* FindLoadedModule(const std::string& name)
{
  auto& loaded = Loaded();
  std::string k = Key(name);
  for (auto& [path, m] : loaded)
  {
    size_t slash = path.find_last_of('/');
    std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
    if (path == k || base == k)
      return m;
  }
  return nullptr;
}

} // namespace mw::res
