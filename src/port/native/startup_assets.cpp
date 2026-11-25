#include "port/native/startup_assets.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>

#include "port/minwin_impl/res/path.h"
#include "port/minwin_impl/res/pe_module.h"
#include "port/native/platform.h"

#include "native_asset_globals.h"

namespace reb::native
{
namespace
{

struct AssetBinding {
  const char** destination;
  const char* identifier;
  uint32_t rva;
  bool isPointer;
  uint32_t maxBytes;
};

// Matches tools/startup_assets_v1.json (schema 1): source coordinates inside the original REBEXE.EXE.
constexpr AssetBinding kBindings[] = {
    {&g_coolDragListClassName, "cool-drag-list-class-name/0", 2547896, true, 4096},
    {&g_dllLocationName, "dll-location-name", 2784272, false, 84},
    {&g_dataLocationName, "data-location-name", 2784356, false, 16},
    {&g_tooBigLocationName, "too-big-location-name", 2784372, false, 16},
    {&g_installedLocationName, "installed-location-name", 2784388, false, 20},
    {&g_commonDllName, "common-dll-name", 2784408, false, 480},
    {&g_textsFileName, "texts-file-name", 2784888, false, 16},
    {&g_fileVersionQueryFormat, "file-version-query-format", 2784904, false, 40},
    {&g_translationQuery, "translation-query", 2784944, false, 28},
    {&g_textstraDllName, "textstra-dll-name", 2784972, false, 16},
    {&g_textcommDllName, "textcomm-dll-name", 2784988, false, 16},
    {&g_gameOptionsRequestLoadText, "game-options-request-load-text", 2785004, false, 84},
    {&g_spaceString, "space-string", 2785088, false, 4},
    {&g_versionText, "version-text", 2785092, false, 416},
    {&g_newlineString, "newline-string", 2785648, false, 104},
    {&g_autosaveName, "autosave-name/0", 2785752, true, 4096},
    {&g_volumeName, "volume-name", 2785768, false, 8},
    {&g_displayOptsName, "display-opts-name", 2785776, false, 12},
    {&g_registryKeyPath, "registry-key-path", 2785788, false, 68},
    {&g_mismatchOptsName, "mismatch-opts-name", 2785856, false, 16},
    {&g_rebdlogDllName, "rebdlog-dll-name", 2785872, false, 196},
    {&g_appClassName, "app-class-name", 2786068, false, 20},
    {&g_introMusicFailedText, "intro-music-failed-text", 2786088, false, 32},
    {&g_soundManagerCaption, "sound-manager-caption", 2786120, false, 260},
    {&g_localMachineRootName, "local-machine-root-name", 2786380, false, 20},
    {&g_movieFile000, "movie-file000", 2786400, false, 16},
    {&g_musicFileNames[0], "music-file-names/0", 2786424, true, 4096},
    {&g_musicFileNames[1], "music-file-names/1", 2786428, true, 4096},
    {&g_musicFileNames[2], "music-file-names/2", 2786432, true, 4096},
    {&g_musicFileNames[3], "music-file-names/3", 2786436, true, 4096},
    {&g_musicFileNames[4], "music-file-names/4", 2786440, true, 4096},
    {&g_musicFileNames[5], "music-file-names/5", 2786444, true, 4096},
    {&g_musicFileNames[6], "music-file-names/6", 2786448, true, 4096},
    {&g_musicFileNames[7], "music-file-names/7", 2786452, true, 4096},
    {&g_musicFileNames[8], "music-file-names/8", 2786456, true, 4096},
    {&g_musicFileNames[9], "music-file-names/9", 2786460, true, 4096},
    {&g_musicFileNames[10], "music-file-names/10", 2786464, true, 4096},
    {&g_musicFileNames[11], "music-file-names/11", 2786468, true, 4096},
    {&g_musicFileNames[12], "music-file-names/12", 2786472, true, 4096},
    {&g_musicFileNames[13], "music-file-names/13", 2786476, true, 4096},
    {&g_musicFileNames[14], "music-file-names/14", 2786480, true, 4096},
    {&g_musicFileNames[15], "music-file-names/15", 2786484, true, 4096},
    {&g_musicSwitchName, "music-switch-name", 2786680, false, 12},
    {&g_musicVolumeName, "music-volume-name", 2786692, false, 52},
    {&g_movieFile103, "movie-file103", 2786744, false, 16},
    {&g_movieFile102, "movie-file102", 2786760, false, 16},
    {&g_movieFile104, "movie-file104", 2786776, false, 16},
    {&g_movieFile201, "movie-file201", 2786792, false, 16},
    {&g_movieFile202, "movie-file202", 2786808, false, 16},
    {&g_movieFile101, "movie-file101", 2786824, false, 16},
    {&g_movieFile005, "movie-file005", 2786840, false, 16},
    {&g_movieFile108, "movie-file108", 2786856, false, 16},
    {&g_movieFile107, "movie-file107", 2786872, false, 16},
    {&g_movieFile105, "movie-file105", 2786888, false, 16},
    {&g_movieFile106, "movie-file106", 2786904, false, 16},
    {&g_movieFile004, "movie-file004", 2786920, false, 16},
    {&g_movieFile003, "movie-file003", 2786936, false, 16},
    {&g_movieFile002, "movie-file002", 2786952, false, 16},
    {&g_movieFile001, "movie-file001", 2786968, false, 16},
    {&g_strategyDllName, "strategy-dll-name", 2786984, false, 16},
    {&g_embriefDllName, "embrief-dll-name", 2787000, false, 12},
    {&g_albriefDllName, "albrief-dll-name", 2787012, false, 12},
    {&g_emspriteDllName, "emsprite-dll-name", 2787024, false, 16},
    {&g_alspriteDllName, "alsprite-dll-name", 2787040, false, 16},
    {&g_currentLatencyText, "current-latency-text", 2787056, false, 20},
    {&g_gokresDllName, "gokres-dll-name", 2787080, false, 16},
    {&g_gameMenuWindowClassName, "game-menu-window-class-name", 2787096, false, 20},
    {&g_statusSeparator, "status-separator", 2787116, false, 4},
    {&g_noText, "no-text", 2787120, false, 4},
    {&g_yesText, "yes-text", 2787124, false, 4},
    {&g_attachedText, "attached-text", 2787128, false, 12},
    {&g_statusText, "status-text", 2787140, false, 8},
    {&g_tallySeparator, "tally-separator", 2787148, false, 4},
    {&g_encyBitmapDllName, "ency-bitmap-dll-name", 2787152, false, 16},
    {&g_encyTextDllName, "ency-text-dll-name", 2787168, false, 16},
    {&g_encyDefaultDataDirectory, "ency-default-data-directory", 2787184, false, 8},
    {&g_encyLocationValueName, "ency-location-value-name", 2787192, false, 16},
    {&g_noForcesText, "no-forces-text", 2787208, false, 4},
    {&g_detailCloseText, "detail-close-text", 2787212, false, 4},
    {&g_detailOpenText, "detail-open-text", 2787216, false, 4},
    {&g_transitDaysSeparator, "transit-days-separator", 2787224, false, 4},
    {&g_battleInProgressText, "battle-in-progress-text", 2787228, false, 28},
    {&g_customDialogBoxClassName, "custom-dialog-box-class-name", 2787368, false, 16},
    {&g_invalidCommandText, "invalid-command-text", 2787384, false, 10164},
    {&g_vowelCharacters, "vowel-characters", 2802120, false, 120},
    {&g_pathUnknownCaption, "path-unknown-caption", 2802244, false, 68},
    {&g_aviDeviceType, "avi-device-type", 2802312, false, 12},
    {&g_tooltipClassName, "tooltip-class-name", 2803648, false, 20},
    {&g_displayTooSmallMessage, "display-too-small-message", 2803748, false, 68},
    {&g_alreadyRunningMessage, "already-running-message", 2803816, false, 92},
    {&g_displayDriverName, "display-driver-name", 2803908, false, 8},
    {&g_noMouseMessage, "no-mouse-message", 2803916, false, 136},
    {&g_waveResourceType, "wave-resource-type", 2804052, false, 36},
};

uint32_t ReadNumber(std::istream& stream, unsigned bytes)
{
  uint32_t value = 0;
  for (unsigned i = 0; i < bytes; ++i) {
    const int byte = stream.get();
    if (byte == std::char_traits<char>::eof()) {
      throw std::runtime_error("Truncated startup asset pack");
    }
    value |= static_cast<uint32_t>(byte) << (i * 8);
  }
  return value;
}

uint32_t ReadLe32(const uint8_t* p)
{
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

std::string ReadText(std::istream& stream, size_t length)
{
  std::string value(length, '\0');
  if (!stream.read(value.data(), length)) {
    throw std::runtime_error("Truncated startup asset text");
  }
  if (value.find('\0') != std::string::npos) {
    throw std::runtime_error("Embedded null in startup asset text");
  }
  return value;
}

std::string ExtractAssetFromModule(const mw::res::Module& module, const AssetBinding& binding)
{
  if (module.Is64() || module.SizeOfImage() == 0) {
    throw std::runtime_error("Unsupported REBEXE.EXE image format (expected 32-bit Windows PE executable)");
  }
  bool hasTarget = true;
  uint32_t targetRva = binding.rva;
  if (binding.isPointer) {
    const uint8_t* ptrData = nullptr;
    size_t ptrAvail = 0;
    if (!module.ReadBytesAtRva(binding.rva, 4, &ptrData, &ptrAvail) || ptrAvail < 4) {
      throw std::runtime_error(std::string("Missing startup asset pointer for ") + binding.identifier);
    }
    const uint32_t address = ReadLe32(ptrData);
    if (address == 0) {
      hasTarget = false;
    } else {
      const uint64_t imageBase = module.ImageBase();
      if (address < imageBase || address - imageBase >= module.SizeOfImage()) {
        throw std::runtime_error(std::string("Invalid startup asset pointer for ") + binding.identifier);
      }
      targetRva = static_cast<uint32_t>(address - imageBase);
    }
  }
  if (!hasTarget) {
    return {};
  }
  if (targetRva >= module.SizeOfImage()) {
    throw std::runtime_error(std::string("Invalid startup asset location for ") + binding.identifier);
  }
  const uint8_t* raw = nullptr;
  size_t avail = 0;
  if (!module.ReadBytesAtRva(targetRva, binding.maxBytes, &raw, &avail)) {
    throw std::runtime_error(std::string("Unreadable startup asset location for ") + binding.identifier);
  }
  const void* terminator = std::memchr(raw, '\0', avail);
  if (!terminator) {
    throw std::runtime_error(std::string("Unterminated startup asset text for ") + binding.identifier);
  }
  const auto length = static_cast<size_t>(static_cast<const uint8_t*>(terminator) - raw);
  return std::string(reinterpret_cast<const char*>(raw), length);
}

} // namespace

StartupAssets::StartupAssets(const std::filesystem::path& path)
{
  LoadFromPack(path);
}

void StartupAssets::LoadForInstallation(const std::string& installDir)
{
  const std::filesystem::path packPath = StartupAssetPath();
  std::error_code ec;
  if (std::filesystem::is_regular_file(packPath, ec) && !ec) {
    LoadFromPack(packPath);
    return;
  }
  const std::string exePath = mw::res::ResolvePath(mw::res::JoinPath(installDir, "REBEXE.EXE"));
  LoadFromExecutable(exePath);
}

void StartupAssets::LoadFromPack(const std::filesystem::path& path)
{
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("Cannot open startup asset pack: " + path.string());
  }
  std::array<char, 8> magic{};
  if (!stream.read(magic.data(), magic.size()) || std::string_view(magic.data(), magic.size()) != "RBASSET1") {
    throw std::runtime_error("Unsupported startup asset schema");
  }
  const uint32_t count = ReadNumber(stream, 4);
  if (count > 1024) {
    throw std::runtime_error("Too many startup assets");
  }
  std::map<std::string, std::string, std::less<>> loaded;
  for (uint32_t i = 0; i < count; ++i) {
    const uint32_t identifierLength = ReadNumber(stream, 2);
    const uint32_t textLength = ReadNumber(stream, 4);
    if (identifierLength == 0 || identifierLength > 256 || textLength > 65536) {
      throw std::runtime_error("Invalid startup asset length");
    }
    auto identifier = ReadText(stream, identifierLength);
    auto text = ReadText(stream, textLength);
    if (!loaded.emplace(std::move(identifier), std::move(text)).second) {
      throw std::runtime_error("Duplicate startup asset identifier");
    }
  }
  if (stream.peek() != std::char_traits<char>::eof()) {
    throw std::runtime_error("Trailing startup asset data");
  }
  texts_ = std::move(loaded);
  PublishBindings();
}

void StartupAssets::LoadFromExecutable(const std::filesystem::path& executablePath)
{
  std::unique_ptr<mw::res::Module> module(mw::res::Module::Open(executablePath.string()));
  if (!module) {
    throw std::runtime_error("Cannot open or parse REBEXE.EXE for startup text: " + executablePath.string());
  }
  LoadFromModule(*module);
}

void StartupAssets::LoadFromModule(const mw::res::Module& module)
{
  std::map<std::string, std::string, std::less<>> loaded;
  for (const auto& binding : kBindings) {
    loaded.emplace(binding.identifier, ExtractAssetFromModule(module, binding));
  }
  texts_ = std::move(loaded);
  PublishBindings();
}

void StartupAssets::PublishBindings()
{
  // Validate the complete schema before publishing any borrowed pointers.
  for (const auto& binding : kBindings) {
    Text(binding.identifier);
  }
  for (const auto& binding : kBindings) {
    *binding.destination = Text(binding.identifier);
  }
}

const char* StartupAssets::Text(const char* identifier) const
{
  const auto found = texts_.find(identifier);
  if (found == texts_.end()) {
    throw std::runtime_error(std::string("Missing startup asset: ") + identifier);
  }
  return found->second.c_str();
}

std::filesystem::path StartupAssetPath()
{
  return std::filesystem::path(reb::platform::BundledFontDir()).parent_path() / "startup-assets.bin";
}

} // namespace reb::native
