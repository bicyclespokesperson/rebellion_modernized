#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace reb::native
{

class StartupAssets
{
  public:
  explicit StartupAssets(const std::filesystem::path& path);
  StartupAssets(const StartupAssets&) = delete;
  StartupAssets& operator=(const StartupAssets&) = delete;

  private:
  const char* Text(const char* identifier) const;
  std::map<std::string, std::string, std::less<>> texts_;
};

std::filesystem::path StartupAssetPath();

} // namespace reb::native
