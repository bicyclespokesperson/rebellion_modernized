#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "app/enhancements.h"
#include "port/minwin_impl/config/config.h"

namespace
{

void Require(bool condition, const char* check)
{
  if (!condition) {
    std::fprintf(stderr, "enhancements test failed: %s\n", check);
    std::abort();
  }
}

void ParsesOffWordsOnly()
{
  for (const char* off : {"0", "false", "OFF", "No"}) {
    Require(!ParseEnhancementsSetting(off), off);
  }
  for (const char* on : {"1", "true", "on", "yes", ""}) {
    Require(ParseEnhancementsSetting(on), on);
  }
}

void DefaultsOnAndPersists()
{
  namespace fs = std::filesystem;
  const fs::path path
      = fs::temp_directory_path()
        / ("rebellion_enhancements_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
           + ".ini");
  {
    std::FILE* file = std::fopen(path.string().c_str(), "w");
    Require(file != nullptr, "temporary config created");
    std::fputs("install_dir=/kept\n", file);
    std::fclose(file);
  }
  mw::config::SetGlobalPath(path.string());
  Require(EnhancementsEnabled(), "missing key means on");

  Require(SetEnhancementsEnabled(false), "saving off");
  mw::config::SetGlobalPath(path.string());
  Require(!EnhancementsEnabled(), "off survives a reload");
  Require(mw::config::Global().Get("install_dir") == "/kept", "other keys kept");

  OverrideEnhancementsForProcess(true);
  Require(EnhancementsEnabled(), "process override wins");
  mw::config::SetGlobalPath(path.string());
  Require(mw::config::Global().Get("enhancements") == "0", "override is not saved");

  Require(SetEnhancementsEnabled(true), "saving on");
  mw::config::SetGlobalPath(path.string());
  Require(mw::config::Global().Get("enhancements") == "1", "on survives a reload");

  std::error_code ignored;
  fs::remove(path, ignored);
}

} // namespace

int main()
{
  ParsesOffWordsOnly();
  DefaultsOnAndPersists();
  return 0;
}
