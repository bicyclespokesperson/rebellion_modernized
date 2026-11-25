#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace mw::res
{
class Module;
}

namespace reb::native
{

class StartupAssets
{
  public:
  StartupAssets() = default;
  explicit StartupAssets(const std::filesystem::path& path);
  StartupAssets(const StartupAssets&) = delete;
  StartupAssets& operator=(const StartupAssets&) = delete;

  // Loads from the bundled startup-assets.bin when present; otherwise extracts directly from <installDir>/REBEXE.EXE.
  void LoadForInstallation(const std::string& installDir);
  void LoadFromPack(const std::filesystem::path& path);
  void LoadFromExecutable(const std::filesystem::path& executablePath);
  void LoadFromModule(const mw::res::Module& module);

  const std::map<std::string, std::string, std::less<>>& Texts() const
  {
    return texts_;
  }

  private:
  const char* Text(const char* identifier) const;
  void PublishBindings();
  std::map<std::string, std::string, std::less<>> texts_;
};

std::filesystem::path StartupAssetPath();

} // namespace reb::native
