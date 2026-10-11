#include "port/native/startup_assets.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <stdexcept>

#include "port/native/platform.h"

#include "native_asset_globals.h"

namespace reb::native
{
namespace
{

uint32_t ReadNumber(std::istream& stream, unsigned bytes)
{
  uint32_t value = 0;
  for (unsigned i = 0; i < bytes; ++i)
  {
    const int byte = stream.get();
    if (byte == std::char_traits<char>::eof())
      throw std::runtime_error("Truncated startup asset pack");
    value |= static_cast<uint32_t>(byte) << (i * 8);
  }
  return value;
}

std::string ReadText(std::istream& stream, size_t length)
{
  std::string value(length, '\0');
  if (!stream.read(value.data(), length))
    throw std::runtime_error("Truncated startup asset text");
  if (value.find('\0') != std::string::npos)
    throw std::runtime_error("Embedded null in startup asset text");
  return value;
}

} // namespace

StartupAssets::StartupAssets(const std::filesystem::path& path)
{
  std::ifstream stream(path, std::ios::binary);
  if (!stream)
    throw std::runtime_error("Cannot open startup asset pack: " + path.string());
  std::array<char, 8> magic{};
  if (!stream.read(magic.data(), magic.size()) || std::string_view(magic.data(), magic.size()) != "RBASSET1")
    throw std::runtime_error("Unsupported startup asset schema");
  const uint32_t count = ReadNumber(stream, 4);
  if (count > 1024)
    throw std::runtime_error("Too many startup assets");
  for (uint32_t i = 0; i < count; ++i)
  {
    const uint32_t identifierLength = ReadNumber(stream, 2);
    const uint32_t textLength = ReadNumber(stream, 4);
    if (identifierLength == 0 || identifierLength > 256 || textLength > 65536)
      throw std::runtime_error("Invalid startup asset length");
    auto identifier = ReadText(stream, identifierLength);
    auto text = ReadText(stream, textLength);
    if (!texts_.emplace(std::move(identifier), std::move(text)).second)
      throw std::runtime_error("Duplicate startup asset identifier");
  }
  if (stream.peek() != std::char_traits<char>::eof())
    throw std::runtime_error("Trailing startup asset data");
  struct Binding
  {
    const char** destination;
    const char* identifier;
  };
  const Binding bindings[] = {
    {&g_coolDragListClassName, "cool-drag-list-class-name/0"},
    {&g_dllLocationName, "dll-location-name"},
    {&g_dataLocationName, "data-location-name"},
    {&g_tooBigLocationName, "too-big-location-name"},
    {&g_installedLocationName, "installed-location-name"},
    {&g_commonDllName, "common-dll-name"},
    {&g_textsFileName, "texts-file-name"},
    {&g_fileVersionQueryFormat, "file-version-query-format"},
    {&g_translationQuery, "translation-query"},
    {&g_textstraDllName, "textstra-dll-name"},
    {&g_textcommDllName, "textcomm-dll-name"},
    {&g_gameOptionsRequestLoadText, "game-options-request-load-text"},
    {&g_spaceString, "space-string"},
    {&g_versionText, "version-text"},
    {&g_newlineString, "newline-string"},
    {&g_autosaveName, "autosave-name/0"},
    {&g_volumeName, "volume-name"},
    {&g_displayOptsName, "display-opts-name"},
    {&g_registryKeyPath, "registry-key-path"},
    {&g_mismatchOptsName, "mismatch-opts-name"},
    {&g_rebdlogDllName, "rebdlog-dll-name"},
    {&g_appClassName, "app-class-name"},
    {&g_introMusicFailedText, "intro-music-failed-text"},
    {&g_soundManagerCaption, "sound-manager-caption"},
    {&g_localMachineRootName, "local-machine-root-name"},
    {&g_movieFile000, "movie-file000"},
    {&g_musicFileNames[0], "music-file-names/0"},
    {&g_musicFileNames[1], "music-file-names/1"},
    {&g_musicFileNames[2], "music-file-names/2"},
    {&g_musicFileNames[3], "music-file-names/3"},
    {&g_musicFileNames[4], "music-file-names/4"},
    {&g_musicFileNames[5], "music-file-names/5"},
    {&g_musicFileNames[6], "music-file-names/6"},
    {&g_musicFileNames[7], "music-file-names/7"},
    {&g_musicFileNames[8], "music-file-names/8"},
    {&g_musicFileNames[9], "music-file-names/9"},
    {&g_musicFileNames[10], "music-file-names/10"},
    {&g_musicFileNames[11], "music-file-names/11"},
    {&g_musicFileNames[12], "music-file-names/12"},
    {&g_musicFileNames[13], "music-file-names/13"},
    {&g_musicFileNames[14], "music-file-names/14"},
    {&g_musicFileNames[15], "music-file-names/15"},
    {&g_musicSwitchName, "music-switch-name"},
    {&g_musicVolumeName, "music-volume-name"},
    {&g_movieFile103, "movie-file103"},
    {&g_movieFile102, "movie-file102"},
    {&g_movieFile104, "movie-file104"},
    {&g_movieFile201, "movie-file201"},
    {&g_movieFile202, "movie-file202"},
    {&g_movieFile101, "movie-file101"},
    {&g_movieFile005, "movie-file005"},
    {&g_movieFile108, "movie-file108"},
    {&g_movieFile107, "movie-file107"},
    {&g_movieFile105, "movie-file105"},
    {&g_movieFile106, "movie-file106"},
    {&g_movieFile004, "movie-file004"},
    {&g_movieFile003, "movie-file003"},
    {&g_movieFile002, "movie-file002"},
    {&g_movieFile001, "movie-file001"},
    {&g_strategyDllName, "strategy-dll-name"},
    {&g_embriefDllName, "embrief-dll-name"},
    {&g_albriefDllName, "albrief-dll-name"},
    {&g_emspriteDllName, "emsprite-dll-name"},
    {&g_alspriteDllName, "alsprite-dll-name"},
    {&g_currentLatencyText, "current-latency-text"},
    {&g_gokresDllName, "gokres-dll-name"},
    {&g_gameMenuWindowClassName, "game-menu-window-class-name"},
    {&g_statusSeparator, "status-separator"},
    {&g_noText, "no-text"},
    {&g_yesText, "yes-text"},
    {&g_attachedText, "attached-text"},
    {&g_statusText, "status-text"},
    {&g_tallySeparator, "tally-separator"},
    {&g_encyBitmapDllName, "ency-bitmap-dll-name"},
    {&g_encyTextDllName, "ency-text-dll-name"},
    {&g_encyDefaultDataDirectory, "ency-default-data-directory"},
    {&g_encyLocationValueName, "ency-location-value-name"},
    {&g_noForcesText, "no-forces-text"},
    {&g_detailCloseText, "detail-close-text"},
    {&g_detailOpenText, "detail-open-text"},
    {&g_transitDaysSeparator, "transit-days-separator"},
    {&g_battleInProgressText, "battle-in-progress-text"},
    {&g_customDialogBoxClassName, "custom-dialog-box-class-name"},
    {&g_invalidCommandText, "invalid-command-text"},
    {&g_vowelCharacters, "vowel-characters"},
    {&g_pathUnknownCaption, "path-unknown-caption"},
    {&g_aviDeviceType, "avi-device-type"},
    {&g_tooltipClassName, "tooltip-class-name"},
    {&g_displayTooSmallMessage, "display-too-small-message"},
    {&g_alreadyRunningMessage, "already-running-message"},
    {&g_displayDriverName, "display-driver-name"},
    {&g_noMouseMessage, "no-mouse-message"},
    {&g_waveResourceType, "wave-resource-type"},
  };
  // Validate the complete schema before publishing any borrowed pointers.
  for (const auto& binding : bindings)
    Text(binding.identifier);
  for (const auto& binding : bindings)
    *binding.destination = Text(binding.identifier);
}

const char* StartupAssets::Text(const char* identifier) const
{
  const auto found = texts_.find(identifier);
  if (found == texts_.end())
    throw std::runtime_error(std::string("Missing startup asset: ") + identifier);
  return found->second.c_str();
}

std::filesystem::path StartupAssetPath()
{
  return std::filesystem::path(reb::platform::BundledFontDir()).parent_path() / "startup-assets.bin";
}

} // namespace reb::native
