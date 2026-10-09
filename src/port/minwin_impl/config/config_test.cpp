#define MINWIN_IMPLEMENTATION 1
#include "port/minwin/advapi32.h"

#include "../tests/mw_test.h"
#include "config.h"

using namespace mw::config;

TEST(parse_and_serialize_roundtrip) {
    Config c;
    c.Parse("# comment\ninstall_dir = /games/rebellion\nwindow_scale=3\n[registry]\nVolume=dword:0000001f\nDllLocation=/x\n");
    CHECK_STR(c.InstallDir().c_str(), "/games/rebellion");
    CHECK_EQ(c.WindowScale(), 3);
    RegistryValue v;
    CHECK(c.GetRegistryValue("volume", &v));
    CHECK(v.type == ValueType::Dword);
    CHECK_EQ(v.dword, 0x1f);
    Config d;
    d.Parse(c.Serialize());
    CHECK_STR(d.Get("window_scale").c_str(), "3");
    CHECK(d.GetRegistryValue("DllLocation", &v));
    CHECK_STR(v.text.c_str(), "/x");
}

TEST(location_defaults_follow_install_and_data_dir) {
    Config c;
    c.Parse("install_dir=/inst\n");
    RegistryValue v;
    CHECK(c.GetRegistryValue("InstalledLocation", &v));
    CHECK_STR(v.text.c_str(), "/inst");
    CHECK(c.GetRegistryValue("TooBigLocation", &v));
    CHECK_STR(v.text.c_str(), "/inst");
    c.Set("data_dir", "/cdrom");
    CHECK(c.GetRegistryValue("TooBigLocation", &v));
    CHECK_STR(v.text.c_str(), "/cdrom");
    CHECK(!c.GetRegistryValue("MusicVolume", &v));
    CHECK_EQ(c.WindowScale(), 1);
}

TEST(registry_api_semantics) {
    SetGlobalPath("/nonexistent/dir/for/test.ini");
    Global().Parse("install_dir=/inst\n");
    HKEY key = nullptr;
    CHECK_EQ(RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Other", 0, KEY_READ, &key), 2);
    CHECK_EQ(RegOpenKeyExA(HKEY_LOCAL_MACHINE, "software\\lucasarts entertainment company llc\\star wars rebellion", 0, KEY_READ, &key), 0);
    DWORD type = 0, size = 0;
    CHECK_EQ(RegQueryValueExA(key, "DllLocation", nullptr, &type, nullptr, &size), 0);
    CHECK_EQ(type, 1);
    CHECK_EQ(size, 6);
    char small[3];
    DWORD smallSize = sizeof small;
    CHECK_EQ(RegQueryValueExA(key, "DllLocation", nullptr, &type, reinterpret_cast<LPBYTE>(small), &smallSize), 234);
    CHECK_EQ(smallSize, 6);
    char buf[16];
    size = sizeof buf;
    CHECK_EQ(RegQueryValueExA(key, "DllLocation", nullptr, &type, reinterpret_cast<LPBYTE>(buf), &size), 0);
    CHECK_STR(buf, "/inst");
    CHECK_EQ(RegQueryValueExA(key, "Volume", nullptr, &type, nullptr, &size), 2);
    DWORD volume = 0x1234;
    CHECK_EQ(RegSetValueExA(key, "Volume", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&volume), 4), 0);
    DWORD read = 0;
    size = 4;
    CHECK_EQ(RegQueryValueExA(key, "Volume", nullptr, &type, reinterpret_cast<LPBYTE>(&read), &size), 0);
    CHECK_EQ(type, REG_DWORD);
    CHECK_EQ(read, 0x1234);
    CHECK_EQ(RegCloseKey(key), 0);
}

TEST(save_creates_directories_and_reloads) {
    Config c;
    c.Set("install_dir", "/a b/c");
    c.SetRegistryValue("MusicSwitch", {ValueType::Dword, "", 1});
    const std::string path = "mw_config_test_dir/sub/config.ini";
    CHECK(c.Save(path));
    Config d;
    CHECK(d.Load(path));
    CHECK_STR(d.InstallDir().c_str(), "/a b/c");
    RegistryValue v;
    CHECK(d.GetRegistryValue("MusicSwitch", &v));
    CHECK_EQ(v.dword, 1);
}
