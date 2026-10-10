#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <sstream>

#if !defined(_WIN32)
#include "port/native/platform.h"
#endif

#if defined(_WIN32)
#include <direct.h>
#define MW_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MW_MKDIR(p) mkdir(p, 0777)
#endif

namespace mw::config {

bool EqualsNoCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = static_cast<char>(x + 32);
        if (y >= 'A' && y <= 'Z') y = static_cast<char>(y + 32);
        if (x != y) return false;
    }
    return true;
}

namespace {

std::string Trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) --e;
    return s.substr(b, e - b);
}

template <typename Entries>
auto* FindEntry(Entries& entries, const std::string& key) {
    for (auto& e : entries)
        if (EqualsNoCase(e.key, key)) return &e;
    return static_cast<decltype(&entries[0])>(nullptr);
}

const char* Env(const char* name) {
    const char* v = getenv(name);
    return (v && *v) ? v : nullptr;
}

}  // namespace

std::string Config::DefaultPath() {
    if (const char* p = Env("REBELLION_CONFIG")) return p;
#if defined(_WIN32)
    if (const char* appdata = Env("APPDATA")) return std::string(appdata) + "\\Rebellion\\config.ini";
    return "config.ini";
#else
    const std::string dir = reb::platform::UserConfigDir();
    return dir.empty() ? "config.ini" : dir + "/config.ini";
#endif
}

void Config::Parse(const std::string& text) {
    m_settings.clear();
    m_registry.clear();
    bool inRegistry = false;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line[0] == '[') {
            inRegistry = EqualsNoCase(Trim(line.substr(1, line.find(']') - 1)), "registry");
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        Entry e{Trim(line.substr(0, eq)), Trim(line.substr(eq + 1))};
        (inRegistry ? m_registry : m_settings).push_back(e);
    }
}

std::string Config::Serialize() const {
    std::string out;
    for (const Entry& e : m_settings) out += e.key + "=" + e.value + "\n";
    out += "[registry]\n";
    for (const Entry& e : m_registry) out += e.key + "=" + e.value + "\n";
    return out;
}

bool Config::Load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    Parse(ss.str());
    return true;
}

bool Config::Save(const std::string& path) const {
    size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos) {
        std::string dir = path.substr(0, slash);
        // directories are created one level at a time so a missing ~/.config/rebellion works without <filesystem>
        for (size_t i = 1; i <= dir.size(); ++i) {
            if (i == dir.size() || dir[i] == '/' || dir[i] == '\\') {
                std::string part = dir.substr(0, i);
                MW_MKDIR(part.c_str());
            }
        }
    }
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << Serialize();
    return static_cast<bool>(f);
}

std::string Config::Get(const std::string& key, const std::string& fallback) const {
    for (const Entry& e : m_settings)
        if (EqualsNoCase(e.key, key)) return e.value;
    return fallback;
}

void Config::Set(const std::string& key, const std::string& value) {
    if (Entry* e = FindEntry(m_settings, key)) e->value = value;
    else m_settings.push_back({key, value});
}

std::string Config::DataDir() const {
    std::string d = Get("data_dir");
    return d.empty() ? InstallDir() : d;
}

int Config::WindowScale() const {
    int s = atoi(Get("window_scale", "0").c_str());
    return s < 1 ? 0 : (s > 8 ? 8 : s);
}

bool Config::GetRegistryValue(const std::string& name, RegistryValue* out) const {
    if (const Entry* e = FindEntry(m_registry, name)) {
        if (e->value.compare(0, 6, "dword:") == 0) {
            out->type = ValueType::Dword;
            out->dword = static_cast<uint32_t>(strtoul(e->value.c_str() + 6, nullptr, 16));
        } else {
            out->type = ValueType::String;
            out->text = e->value;
        }
        return true;
    }
    std::string derived;
    if (EqualsNoCase(name, "TooBigLocation")) derived = DataDir();
    else if (EqualsNoCase(name, "DataLocation") || EqualsNoCase(name, "DllLocation") ||
             EqualsNoCase(name, "InstalledLocation") || EqualsNoCase(name, "EncyLocation"))
        derived = InstallDir();
    if (derived.empty()) return false;
#if !defined(_WIN32)
    // The game drops the first two characters of a location (the drive) and changes drive from the first: hand it a Windows-style name.
    if (derived[0] == '/') derived = "C:" + derived;
#endif
    out->type = ValueType::String;
    out->text = derived;
    return true;
}

void Config::SetRegistryValue(const std::string& name, const RegistryValue& value) {
    std::string text;
    if (value.type == ValueType::Dword) {
        char buf[16];
        snprintf(buf, sizeof buf, "dword:%08x", value.dword);
        text = buf;
    } else {
        text = value.text;
    }
    if (Entry* e = FindEntry(m_registry, name)) e->value = text;
    else m_registry.push_back({name, text});
}

namespace {
Config* g_config = nullptr;
std::string g_path;
}  // namespace

void SetGlobalPath(const std::string& path) {
    delete g_config;
    g_config = new Config;
    g_path = path;
    g_config->Load(path);
}

const std::string& GlobalPath() {
    Global();
    return g_path;
}

Config& Global() {
    if (!g_config) SetGlobalPath(Config::DefaultPath());
    return *g_config;
}

}  // namespace mw::config
