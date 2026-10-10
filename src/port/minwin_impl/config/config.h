// minwin_impl/config: the port's configuration file, which replaces the game's registry reads (advapi32 subset).
//
// File: INI-like text, one `key=value` per line, `#`/`;` comments. Keys before any section header are the port's own settings:
//   install_dir    the game installation (REBEXE.EXE, the *.DLL resource libraries, GData, MDATA, EData, SaveGame)
//   data_dir       where MDATA\ lives when it is not in install_dir (the CD, the "TooBig" location); defaults to install_dir
//   font_path      full path of arial.ttf; when empty the text module searches the system and then falls back to Liberation Sans
//   window_scale   integer scale of the 640x480 window, 1-8; 0 or absent: the largest that fits the display
//   fullscreen     1: start fullscreen (letterboxed at an integer scale); Alt+Enter toggles while playing (default 0)
//   renderer       SDL render driver name ("software", "opengl", ...; default: SDL's choice, software when that fails)
//   vsync          1: synchronise presentation with the display (default 0)
//   audio_buffer   device buffer in sample frames (default 1024 at 44.1 kHz)
// The `[registry]` section holds the values the game reads and writes under
//   HKLM\SOFTWARE\LucasArts Entertainment Company LLC\Star Wars Rebellion
// (DataLocation, DllLocation, InstalledLocation, TooBigLocation, EncyLocation, MismatchOpts, DisplayOpts, Volume, MusicVolume,
// MusicSwitch). A DWORD is written `Name=dword:0000001f`, anything else is a string. The five *Location values default from
// install_dir/data_dir when absent.
//
// Location of the file (first that applies): $REBELLION_CONFIG; then
//   Linux    $XDG_CONFIG_HOME/rebellion/config.ini, else ~/.config/rebellion/config.ini  (src/port/native/platform_linux.cpp)
//   macOS    ~/Library/Application Support/Rebellion/config.ini  (src/port/native/platform_macos.cpp)
//   Windows  %APPDATA%\Rebellion\config.ini
#pragma once
#include <stdint.h>

#include <string>
#include <vector>

namespace mw::config {

constexpr const char* kRegistrySubKey = "SOFTWARE\\LucasArts Entertainment Company LLC\\Star Wars Rebellion";

enum class ValueType : uint32_t { None = 0, String = 1, Dword = 4 };

struct RegistryValue {
    ValueType type = ValueType::None;
    std::string text;
    uint32_t dword = 0;
};

class Config {
public:
    static std::string DefaultPath();

    bool Load(const std::string& path);
    bool Save(const std::string& path) const;
    void Parse(const std::string& text);
    std::string Serialize() const;

    std::string Get(const std::string& key, const std::string& fallback = std::string()) const;
    void Set(const std::string& key, const std::string& value);

    std::string InstallDir() const { return Get("install_dir"); }
    std::string DataDir() const;
    std::string FontPath() const { return Get("font_path"); }
    int WindowScale() const;  // 0: automatic

    bool GetRegistryValue(const std::string& name, RegistryValue* out) const;
    void SetRegistryValue(const std::string& name, const RegistryValue& value);

private:
    struct Entry {
        std::string key;
        std::string value;
    };
    std::vector<Entry> m_settings;
    std::vector<Entry> m_registry;
};

// The process-wide configuration, loaded from DefaultPath() on first use. SetGlobalPath() replaces it (tests, --config).
Config& Global();
void SetGlobalPath(const std::string& path);
const std::string& GlobalPath();

bool EqualsNoCase(const std::string& a, const std::string& b);

}  // namespace mw::config
