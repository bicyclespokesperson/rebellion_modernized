#include "port/minwin/kernel32.h"
#include "port/minwin/user32.h"
#include "port/minwin/version.h"

#include <stdio.h>
#include <string.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

#include <algorithm>
#include <map>

#include "../config/config.h"
#include "../tests/mw_test.h"
#include "formats.h"
#include "modules.h"
#include "path.h"

using namespace mw::res;

namespace
{

void Put16(std::vector<uint8_t>& v, uint32_t x)
{
  v.push_back(x & 0xff);
  v.push_back((x >> 8) & 0xff);
}
void Put32(std::vector<uint8_t>& v, uint32_t x)
{
  Put16(v, x & 0xffff);
  Put16(v, x >> 16);
}

struct Res {
  ResId type, name;
  std::vector<uint8_t> data;
};

// A PE32 image with a .text-less layout: headers, one .rsrc section at RVA 0x1000, file offset 0x400.
std::vector<uint8_t> BuildPe(const std::vector<Res>& resources)
{
  // group by type (ints sorted, strings first as the PE format requires), then by name
  std::vector<ResId> types;
  for (const Res& r : resources) {
    bool seen = false;
    for (const ResId& t : types) {
      seen |= (t.isString == r.type.isString && t.id == r.type.id && t.name == r.type.name);
    }
    if (!seen) {
      types.push_back(r.type);
    }
  }
  std::sort(types.begin(), types.end(), [](const ResId& a, const ResId& b) {
    return a.id < b.id;
  });

  // Two passes: layout offsets are computed from sizes, so build the tree with placeholders then patch.
  std::vector<uint8_t> rs;
  std::vector<std::pair<size_t, size_t>> dataEntryPatches; // (position of the data entry, index in `resources`)
  auto dirHeader = [&](std::vector<uint8_t>& v, uint32_t named, uint32_t ids) {
    Put32(v, 0);
    Put32(v, 0);
    Put32(v, 0);
    Put16(v, named);
    Put16(v, ids);
  };
  // sizes: root dir, then per type a dir, per (type,name) a dir and a data entry
  std::vector<std::vector<size_t>> perType;
  for (const ResId& t : types) {
    std::vector<size_t> idx;
    for (size_t i = 0; i < resources.size(); ++i) {
      if (resources[i].type.isString == t.isString && resources[i].type.id == t.id
          && resources[i].type.name == t.name) {
        idx.push_back(i);
      }
    }
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) {
      return resources[a].name.id < resources[b].name.id;
    });
    perType.push_back(idx);
  }
  size_t pos = 16 + 8 * types.size();
  std::vector<size_t> typeDirPos, nameDirPos(resources.size()), langDirPos(resources.size()),
      dataEntryPos(resources.size());
  for (size_t t = 0; t < types.size(); ++t) {
    typeDirPos.push_back(pos);
    pos += 16 + 8 * perType[t].size();
  }
  for (size_t t = 0; t < types.size(); ++t) {
    for (size_t i : perType[t]) {
      nameDirPos[i] = pos;
      pos += 16 + 8;
    }
  }
  for (size_t t = 0; t < types.size(); ++t) {
    for (size_t i : perType[t]) {
      dataEntryPos[i] = pos;
      pos += 16;
    }
  }
  // named resources would need name strings; this builder supports integer ids only
  size_t dataStart = (pos + 3) & ~size_t(3);
  std::vector<size_t> dataPos(resources.size());
  size_t dpos = dataStart;
  for (size_t i = 0; i < resources.size(); ++i) {
    dataPos[i] = dpos;
    dpos += (resources[i].data.size() + 3) & ~size_t(3);
  }
  dirHeader(rs, 0, types.size());
  for (size_t t = 0; t < types.size(); ++t) {
    Put32(rs, types[t].id);
    Put32(rs, 0x80000000u | typeDirPos[t]);
  }
  for (size_t t = 0; t < types.size(); ++t) {
    dirHeader(rs, 0, perType[t].size());
    for (size_t i : perType[t]) {
      Put32(rs, resources[i].name.id);
      Put32(rs, 0x80000000u | nameDirPos[i]);
    }
  }
  for (size_t t = 0; t < types.size(); ++t) {
    for (size_t i : perType[t]) {
      dirHeader(rs, 0, 1);
      Put32(rs, 0x409);
      Put32(rs, dataEntryPos[i]);
    }
  }
  for (size_t t = 0; t < types.size(); ++t) {
    for (size_t i : perType[t]) {
      Put32(rs, 0x1000 + dataPos[i]);
      Put32(rs, resources[i].data.size());
      Put32(rs, 1252);
      Put32(rs, 0);
    }
  }
  rs.resize(dataStart, 0);
  for (const Res& r : resources) {
    rs.insert(rs.end(), r.data.begin(), r.data.end());
    while (rs.size() % 4) {
      rs.push_back(0);
    }
  }

  std::vector<uint8_t> pe(0x400, 0);
  memcpy(pe.data(), "MZ", 2);
  pe[0x3c] = 0x80;
  memcpy(pe.data() + 0x80, "PE\0\0", 4);
  auto w16 = [&](size_t at, uint32_t x) {
    pe[at] = x & 0xff;
    pe[at + 1] = (x >> 8) & 0xff;
  };
  auto w32 = [&](size_t at, uint32_t x) {
    w16(at, x & 0xffff);
    w16(at + 2, x >> 16);
  };
  w16(0x84, 0x14c);
  w16(0x86, 1);
  w16(0x94, 224);
  size_t opt = 0x98;
  w16(opt, 0x10b);
  w32(opt + 92, 16);
  w32(opt + 96 + 16, 0x1000);
  w32(opt + 96 + 20, static_cast<uint32_t>(rs.size()));
  size_t sec = opt + 224;
  memcpy(pe.data() + sec, ".rsrc\0\0\0", 8);
  w32(sec + 8, static_cast<uint32_t>(rs.size()));
  w32(sec + 12, 0x1000);
  w32(sec + 16, static_cast<uint32_t>(rs.size()));
  w32(sec + 20, 0x400);
  pe.insert(pe.end(), rs.begin(), rs.end());
  return pe;
}

std::vector<uint8_t> Utf16Block(const std::vector<const char*>& strings16)
{
  std::vector<uint8_t> v;
  for (size_t i = 0; i < 16; ++i) {
    const char* s = i < strings16.size() ? strings16[i] : "";
    Put16(v, static_cast<uint32_t>(strlen(s)));
    for (const char* p = s; *p; ++p) {
      Put16(v, static_cast<uint8_t>(*p));
    }
  }
  return v;
}

std::vector<uint8_t> TinyDib8(int w, int h, bool withMask)
{
  std::vector<uint8_t> v;
  Put32(v, 40);
  Put32(v, w);
  Put32(v, withMask ? h * 2 : h);
  Put16(v, 1);
  Put16(v, 8);
  Put32(v, 0);
  Put32(v, 0);
  Put32(v, 0);
  Put32(v, 0);
  Put32(v, 2);
  Put32(v, 0);
  Put32(v, 0x000000ff); // index 0: red (BGRA bytes: B=ff? RGBQUAD is B,G,R,A)
  Put32(v, 0x0000ff00); // index 1
  // pixels bottom-up: bottom row first
  size_t stride = (w + 3) & ~3;
  for (int y = h - 1; y >= 0; --y) {
    for (int x = 0; x < w; ++x) {
      v.push_back((x + y) & 1);
    }
    for (size_t p = w; p < stride; ++p) {
      v.push_back(0);
    }
  }
  if (withMask) {
    for (int y = h - 1; y >= 0; --y) {
      v.push_back(y == 0 ? 0x80 : 0x00); // the top-left pixel is transparent
      v.push_back(0);
      v.push_back(0);
      v.push_back(0);
    }
  }
  return v;
}

} // namespace

TEST(synthetic_pe_find_and_strings)
{
  std::vector<Res> res;
  res.push_back({ResId::Int(kString), ResId::Int(1), Utf16Block({"zero", "one", "two", "Hello \xe9", "four"})});
  res.push_back({ResId::Int(kString), ResId::Int(3), Utf16Block({"x", "y"})});
  res.push_back({ResId::Int(kRcData), ResId::Int(7), {'a', 'b', 'c', 'd', 'e'}});
  res.push_back({ResId::Int(kRcData), ResId::Int(9), {1, 2}});
  std::vector<uint8_t> pe = BuildPe(res);
  Module* m = Module::FromMemory(pe.data(), pe.size());
  CHECK(m != nullptr);
  if (!m) {
    return;
  }
  Entry e;
  CHECK(m->Find(ResId::Int(kRcData), ResId::Int(7), &e));
  CHECK_EQ(e.size, 5);
  CHECK_EQ(e.data[4], 'e');
  CHECK_EQ(e.language, 0x409);
  CHECK(!m->Find(ResId::Int(kRcData), ResId::Int(8), &e));
  CHECK(!m->Find(ResId::Int(kBitmap), ResId::Int(7), &e));
  std::string s;
  CHECK(LoadStringText(*m, 3, &s));
  CHECK_STR(s, "Hello \xe9");
  CHECK(LoadStringText(*m, 16 * 2 + 0, &s));
  CHECK_STR(s, "x");
  CHECK(!LoadStringText(*m, 5, &s));  // empty
  CHECK(!LoadStringText(*m, 99, &s)); // no such block
  CHECK_EQ(m->ListTypes().size(), 2);
  CHECK_EQ(m->ListNames(ResId::Int(kRcData)).size(), 2);
  delete m;
}

TEST(synthetic_pe_cursor_icon_accelerators_version)
{
  std::vector<Res> res;
  std::vector<uint8_t> cursorImage;
  Put16(cursorImage, 1); // hotspot x
  Put16(cursorImage, 0); // hotspot y
  std::vector<uint8_t> dib = TinyDib8(4, 4, true);
  cursorImage.insert(cursorImage.end(), dib.begin(), dib.end());
  res.push_back({ResId::Int(kCursor), ResId::Int(5), cursorImage});
  std::vector<uint8_t> group;
  Put16(group, 0);
  Put16(group, 2);
  Put16(group, 1);
  Put16(group, 4);
  Put16(group, 8); // cursor group: height field is doubled
  Put16(group, 1);
  Put16(group, 8);
  Put32(group, static_cast<uint32_t>(cursorImage.size()));
  Put16(group, 5);
  res.push_back({ResId::Int(kGroupCursor), ResId::Int(1001), group});

  std::vector<uint8_t> accel;
  auto addAccel = [&](uint8_t flags, uint16_t key, uint16_t cmd) {
    accel.push_back(flags);
    accel.push_back(0);
    Put16(accel, key);
    Put16(accel, cmd);
    Put16(accel, 0);
  };
  addAccel(1, 0x70, 3001);
  addAccel(0x81, 0x71, 3002);
  res.push_back({ResId::Int(kAccelerator), ResId::Int(11), accel});

  // VS_VERSIONINFO "VS_VERSION_INFO" with StringFileInfo\040904b0\FileVersion = "1.2" and Translation 0409 04b0
  auto node = [](const char* key, uint16_t type, const std::vector<uint8_t>& value, uint16_t valueLen,
                 const std::vector<std::vector<uint8_t>>& kids) {
    std::vector<uint8_t> n;
    Put16(n, 0);
    Put16(n, valueLen);
    Put16(n, type);
    for (const char* p = key; *p; ++p) {
      Put16(n, static_cast<uint8_t>(*p));
    }
    Put16(n, 0);
    while (n.size() % 4) {
      n.push_back(0);
    }
    n.insert(n.end(), value.begin(), value.end());
    while (n.size() % 4) {
      n.push_back(0);
    }
    for (const auto& k : kids) {
      n.insert(n.end(), k.begin(), k.end());
      while (n.size() % 4) {
        n.push_back(0);
      }
    }
    n[0] = n.size() & 0xff;
    n[1] = (n.size() >> 8) & 0xff;
    return n;
  };
  std::vector<uint8_t> text;
  for (const char* p = "1.2"; *p; ++p) {
    Put16(text, static_cast<uint8_t>(*p));
  }
  Put16(text, 0);
  auto fileVersion = node("FileVersion", 1, text, 4, {});
  auto table = node("040904b0", 1, {}, 0, {fileVersion});
  auto sfi = node("StringFileInfo", 1, {}, 0, {table});
  std::vector<uint8_t> trans;
  Put16(trans, 0x409);
  Put16(trans, 0x4b0);
  auto translation = node("Translation", 0, trans, 4, {});
  auto vfi = node("VarFileInfo", 1, {}, 0, {translation});
  std::vector<uint8_t> fixedInfo(52, 0);
  fixedInfo[0] = 0xbd;
  fixedInfo[1] = 0x04;
  fixedInfo[2] = 0xef;
  fixedInfo[3] = 0xfe;
  auto root = node("VS_VERSION_INFO", 0, fixedInfo, 52, {sfi, vfi});
  res.push_back({ResId::Int(kVersion), ResId::Int(1), root});

  std::vector<uint8_t> pe = BuildPe(res);
  Module* m = Module::FromMemory(pe.data(), pe.size());
  CHECK(m != nullptr);
  if (!m) {
    return;
  }

  Image img;
  CHECK(LoadCursorImage(*m, ResId::Int(1001), &img));
  CHECK_EQ(img.width, 4);
  CHECK_EQ(img.height, 4);
  CHECK_EQ(img.hotX, 1);
  CHECK_EQ(img.hotY, 0);
  CHECK_EQ(img.rgba[3], 0);     // masked pixel (0,0) is transparent
  CHECK_EQ(img.rgba[4 + 0], 0); // pixel (1,0): palette index (1+0)&1 = 1 -> green
  CHECK_EQ(img.rgba[4 + 1], 255);
  CHECK_EQ(img.rgba[4 + 3], 255);
  CHECK_EQ(img.rgba[(1 * 4 + 1) * 4 + 3], 255);

  std::vector<Accelerator> table2;
  CHECK(LoadAcceleratorTable(*m, ResId::Int(11), &table2));
  CHECK_EQ(table2.size(), 2);
  CHECK_EQ(table2[0].command, 3001);
  CHECK_EQ(table2[0].key, 0x70);
  CHECK_EQ(table2[1].flags, 1);

  Entry e;
  CHECK(m->Find(ResId::Int(kVersion), ResId::Int(1), &e));
  VersionBlock vroot;
  CHECK(ParseVersionInfo(e.data, e.size, &vroot));
  CHECK_STR(vroot.key, "VS_VERSION_INFO");
  CHECK_EQ(vroot.valueLength, 52);
  const VersionBlock* fv = QueryVersionBlock(vroot, "\\StringFileInfo\\040904B0\\FileVersion");
  CHECK(fv != nullptr);
  if (fv) {
    CHECK_EQ(fv->valueLength, 4);
  }
  const VersionBlock* tr = QueryVersionBlock(vroot, "\\VarFileInfo\\Translation");
  CHECK(tr != nullptr);
  if (tr) {
    CHECK_EQ(tr->valueLength, 4);
  }
  CHECK(QueryVersionBlock(vroot, "\\StringFileInfo\\nope") == nullptr);
  delete m;
}

#if !defined(_WIN32)
TEST(path_translation_matches_case_and_separators)
{
  std::string dir = "mw_res_path_test";
  std::string cmd = "mkdir -p " + dir + "/GData/Sub && echo x > " + dir + "/GData/Sub/Foo.Dat";
  CHECK_EQ(system(cmd.c_str()), 0);
  char cwd[1024];
  CHECK(getcwd(cwd, sizeof cwd) != nullptr);
  std::string absolute = std::string(cwd) + "/" + dir;
  std::string drive = "Z:" + absolute;
  for (char& c : drive) {
    if (c == '/') {
      c = '\\';
    }
  }
  CHECK_STR(ResolvePath(drive + "\\gdata\\SUB\\foo.dat"), absolute + "/GData/Sub/Foo.Dat");
  CHECK_STR(ResolvePath(dir + "\\gdata\\SUB\\foo.dat"), dir + "/GData/Sub/Foo.Dat");
  CHECK_STR(ResolvePath(dir + "/gdata/missing/x.dat"), dir + "/GData/missing/x.dat");
  CHECK_STR(ResolvePath(dir + "/GDATA"), dir + "/GData");
  CHECK(FileExists(ResolvePath(dir + "\\GDATA\\sub\\FOO.DAT")));
  CHECK_STR(JoinPath("a", "b"), "a/b");
  CHECK_STR(JoinPath("a/", "b"), "a/b");
}
#endif

TEST(exports_resource_functions_on_synthetic_dll)
{
  std::vector<Res> res;
  res.push_back({ResId::Int(kRcData), ResId::Int(12), {9, 8, 7}});
  res.push_back({ResId::Int(kString), ResId::Int(1), Utf16Block({"first", "second string"})});
  std::vector<uint8_t> pe = BuildPe(res);
  FILE* f = fopen("mw_res_synth.dll", "wb");
  CHECK(f != nullptr);
  if (!f) {
    return;
  }
  fwrite(pe.data(), 1, pe.size(), f);
  fclose(f);

  HMODULE h = reb::compat::LoadLibraryA("mw_res_synth.dll");
  CHECK(h != nullptr);
  CHECK(reb::compat::LoadLibraryA("MW_RES_SYNTH.DLL") == h);
  HRSRC r = reb::compat::FindResourceA(h, MAKEINTRESOURCEA(12), RT_RCDATA);
  CHECK(r != nullptr);
  CHECK_EQ(reb::compat::SizeofResource(h, r), 3);
  const unsigned char* p = static_cast<const unsigned char*>(reb::compat::LockResource(reb::compat::LoadResource(h, r)));
  CHECK(p && p[0] == 9 && p[2] == 7);
  CHECK(reb::compat::FreeResource(reb::compat::LoadResource(h, r)) == FALSE);
  CHECK(reb::compat::FindResourceA(h, MAKEINTRESOURCEA(13), RT_RCDATA) == nullptr);
  CHECK(reb::compat::FindResourceA(h, "#12", MAKEINTRESOURCEA(10)) == r);
  char buf[8];
  CHECK_EQ(reb::compat::LoadStringA(h, 1, buf, sizeof buf), 7);
  CHECK_STR(std::string(buf), "second ");
  CHECK_EQ(reb::compat::LoadStringA(h, 3, buf, sizeof buf), 0);
  CHECK_EQ(reb::compat::LoadStringA(h, 0, buf, sizeof buf), 5);
  CHECK(reb::compat::GetModuleHandleA("mw_res_synth.dll") == h);
  CHECK(reb::compat::FreeLibrary(h));
  CHECK(reb::compat::FreeLibrary(h));
  CHECK(reb::compat::GetModuleHandleA("mw_res_synth.dll") == nullptr);
  CHECK(reb::compat::LoadLibraryA("does_not_exist.dll") == nullptr);
  HCURSOR arrow = reb::compat::LoadCursorA(nullptr, IDC_ARROW);
  CHECK(arrow != nullptr);
  CHECK(arrow == reb::compat::LoadCursorA(nullptr, IDC_ARROW));
  CHECK_EQ(CursorFromHandle(arrow)->systemId, 32512);
  char lang[64];
  CHECK_EQ(reb::compat::VerLanguageNameA(0x409, lang, sizeof lang), 23);
  CHECK_STR(std::string(lang), "English (United States)");
}

// Against the user's installation (MW_INSTALL_DIR); skipped when absent.
TEST(real_installation_resources)
{
  const char* dir = getenv("MW_INSTALL_DIR");
  if (!dir) {
    printf("  (skipped: MW_INSTALL_DIR not set)\n");
    return;
  }
  mw::config::SetGlobalPath("mw_res_real.ini");
  mw::config::Global().Set("install_dir", dir);
  HMODULE comm = reb::compat::LoadLibraryA("textcomm.dll");
  CHECK(comm != nullptr);
  HACCEL accel = reb::compat::LoadAcceleratorsA(comm, MAKEINTRESOURCEA(0xb));
  CHECK(accel != nullptr);
  if (accel) {
    CHECK(AcceleratorsFromHandle(accel)->size() > 10);
  }
  char buf[256];
  int n = 0;
  for (UINT id = 0; id < 5000 && n == 0; ++id) {
    n = reb::compat::LoadStringA(comm, id, buf, sizeof buf);
  }
  CHECK(n > 0);
  HMODULE exe = reb::compat::GetModuleHandleA(nullptr);
  CHECK(exe != nullptr);
  HCURSOR c = reb::compat::LoadCursorA(exe, MAKEINTRESOURCEA(0x3e9));
  CHECK(c != nullptr);
  if (c) {
    CHECK(CursorFromHandle(c)->image.width > 0);
  }
  CHECK(reb::compat::LoadIconA(exe, MAKEINTRESOURCEA(0x3e8)) != nullptr);
  HMODULE strategy = reb::compat::LoadLibraryA("STRATEGY.DLL");
  CHECK(strategy != nullptr);
  std::string path = std::string(dir) + "/TEXTSTRA.DLL";
  DWORD h = 0;
  DWORD size = reb::compat::GetFileVersionInfoSizeA(path.c_str(), &h);
  CHECK(size > 0);
  std::vector<char> block(size);
  CHECK(reb::compat::GetFileVersionInfoA(path.c_str(), 0, size, block.data()));
  WORD* tr = nullptr;
  UINT len = 0;
  CHECK(reb::compat::VerQueryValueA(block.data(), "\\VarFileInfo\\Translation", reinterpret_cast<LPVOID*>(&tr), &len));
  if (tr) {
    printf("  TEXTSTRA translation %04x %04x, language %s\n", tr[0], tr[1], LanguageName(tr[0]).c_str());
    char sub[128];
    snprintf(sub, sizeof sub, "\\StringFileInfo\\%04x%04x\\FileVersion", tr[0], tr[1]);
    char* ver = nullptr;
    CHECK(reb::compat::VerQueryValueA(block.data(), sub, reinterpret_cast<LPVOID*>(&ver), &len));
    if (ver) {
      printf("  FileVersion \"%s\"\n", ver);
    }
  }
}
