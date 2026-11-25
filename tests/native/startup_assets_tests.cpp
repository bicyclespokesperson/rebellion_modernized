#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "native_asset_globals.h"
#include "port/minwin_impl/res/path.h"
#include "port/minwin_impl/res/pe_module.h"
#include "port/native/startup_assets.h"

// Define the asset globals locally for this standalone test target.
const char* g_albriefDllName = nullptr;
const char* g_alreadyRunningMessage = nullptr;
const char* g_alspriteDllName = nullptr;
const char* g_appClassName = nullptr;
const char* g_attachedText = nullptr;
const char* g_autosaveName = nullptr;
const char* g_aviDeviceType = nullptr;
const char* g_battleInProgressText = nullptr;
const char* g_commonDllName = nullptr;
const char* g_coolDragListClassName = nullptr;
const char* g_currentLatencyText = nullptr;
const char* g_customDialogBoxClassName = nullptr;
const char* g_dataLocationName = nullptr;
const char* g_detailCloseText = nullptr;
const char* g_detailOpenText = nullptr;
const char* g_displayDriverName = nullptr;
const char* g_displayOptsName = nullptr;
const char* g_displayTooSmallMessage = nullptr;
const char* g_dllLocationName = nullptr;
const char* g_embriefDllName = nullptr;
const char* g_emspriteDllName = nullptr;
const char* g_encyBitmapDllName = nullptr;
const char* g_encyDefaultDataDirectory = nullptr;
const char* g_encyLocationValueName = nullptr;
const char* g_encyTextDllName = nullptr;
const char* g_fileVersionQueryFormat = nullptr;
const char* g_gameMenuWindowClassName = nullptr;
const char* g_gameOptionsRequestLoadText = nullptr;
const char* g_gokresDllName = nullptr;
const char* g_installedLocationName = nullptr;
const char* g_introMusicFailedText = nullptr;
const char* g_invalidCommandText = nullptr;
const char* g_localMachineRootName = nullptr;
const char* g_mismatchOptsName = nullptr;
const char* g_movieFile000 = nullptr;
const char* g_movieFile001 = nullptr;
const char* g_movieFile002 = nullptr;
const char* g_movieFile003 = nullptr;
const char* g_movieFile004 = nullptr;
const char* g_movieFile005 = nullptr;
const char* g_movieFile101 = nullptr;
const char* g_movieFile102 = nullptr;
const char* g_movieFile103 = nullptr;
const char* g_movieFile104 = nullptr;
const char* g_movieFile105 = nullptr;
const char* g_movieFile106 = nullptr;
const char* g_movieFile107 = nullptr;
const char* g_movieFile108 = nullptr;
const char* g_movieFile201 = nullptr;
const char* g_movieFile202 = nullptr;
const char* g_musicFileNames[16] = {};
const char* g_musicSwitchName = nullptr;
const char* g_musicVolumeName = nullptr;
const char* g_newlineString = nullptr;
const char* g_noForcesText = nullptr;
const char* g_noMouseMessage = nullptr;
const char* g_noText = nullptr;
const char* g_pathUnknownCaption = nullptr;
const char* g_rebdlogDllName = nullptr;
const char* g_registryKeyPath = nullptr;
const char* g_soundManagerCaption = nullptr;
const char* g_spaceString = nullptr;
const char* g_statusSeparator = nullptr;
const char* g_statusText = nullptr;
const char* g_strategyDllName = nullptr;
const char* g_tallySeparator = nullptr;
const char* g_textcommDllName = nullptr;
const char* g_textsFileName = nullptr;
const char* g_textstraDllName = nullptr;
const char* g_tooBigLocationName = nullptr;
const char* g_tooltipClassName = nullptr;
const char* g_transitDaysSeparator = nullptr;
const char* g_translationQuery = nullptr;
const char* g_versionText = nullptr;
const char* g_volumeName = nullptr;
const char* g_vowelCharacters = nullptr;
const char* g_waveResourceType = nullptr;
const char* g_yesText = nullptr;

namespace
{

void Wr16(std::vector<uint8_t>& buf, size_t off, uint16_t v)
{
  buf[off] = static_cast<uint8_t>(v & 0xffu);
  buf[off + 1] = static_cast<uint8_t>((v >> 8) & 0xffu);
}

void Wr32(std::vector<uint8_t>& buf, size_t off, uint32_t v)
{
  buf[off] = static_cast<uint8_t>(v & 0xffu);
  buf[off + 1] = static_cast<uint8_t>((v >> 8) & 0xffu);
  buf[off + 2] = static_cast<uint8_t>((v >> 16) & 0xffu);
  buf[off + 3] = static_cast<uint8_t>((v >> 24) & 0xffu);
}

// Build a synthetic PE32 image with a single .data section covering RVAs [0x260000, 0x2b2000).
constexpr uint32_t kImageBase = 0x00400000u;
constexpr uint32_t kSectionRva = 0x00260000u;
constexpr uint32_t kSectionRawOffset = 0x200u;
constexpr uint32_t kSectionSize = 0x00052000u; // covers 2547896 (0x26e0b8) through 2804052+36 (0x2ac978)
constexpr uint32_t kSizeOfImage = kSectionRva + kSectionSize;

std::vector<uint8_t> BuildSyntheticPe()
{
  std::vector<uint8_t> img(kSectionRawOffset + kSectionSize, 0);
  img[0] = 'M';
  img[1] = 'Z';
  const uint32_t pe = 0x80u;
  Wr32(img, 0x3cu, pe);
  img[pe] = 'P';
  img[pe + 1] = 'E';
  Wr16(img, pe + 6u, 1u);    // NumberOfSections
  Wr16(img, pe + 20u, 224u); // SizeOfOptionalHeader (PE32)
  const uint32_t opt = pe + 24u;
  Wr16(img, opt, 0x10bu); // PE32 magic
  Wr32(img, opt + 28u, kImageBase);
  Wr32(img, opt + 56u, kSizeOfImage);
  Wr32(img, opt + 92u, 16u); // NumberOfRvaAndSizes (all data dirs zero => no resources)

  const uint32_t sec = opt + 224u;
  std::memcpy(img.data() + sec, ".data\0\0\0", 8);
  Wr32(img, sec + 8u, kSectionSize);      // VirtualSize
  Wr32(img, sec + 12u, kSectionRva);      // VirtualAddress
  Wr32(img, sec + 16u, kSectionSize);     // SizeOfRawData
  Wr32(img, sec + 20u, kSectionRawOffset); // PointerToRawData

  auto writeStringAtRva = [&](uint32_t rva, const char* text) {
    assert(rva >= kSectionRva && rva - kSectionRva + std::strlen(text) + 1 <= kSectionSize);
    std::memcpy(img.data() + kSectionRawOffset + (rva - kSectionRva), text, std::strlen(text) + 1);
  };
  auto writePtrAtRva = [&](uint32_t ptrRva, uint32_t targetRva) {
    assert(ptrRva >= kSectionRva && ptrRva - kSectionRva + 4 <= kSectionSize);
    Wr32(img, kSectionRawOffset + (ptrRva - kSectionRva), targetRva ? (kImageBase + targetRva) : 0u);
  };

  // Direct RVA samples (all other direct RVAs are zero-filled so they read as empty strings "").
  writeStringAtRva(2784272u, "DllLocation");
  writeStringAtRva(2786984u, "STRATEGY.DLL");
  writeStringAtRva(2804052u, "WAVE");

  // Pointer RVA samples: point cool-drag-list and music-file-names/0 to strings at the end of the section,
  // and leave autosave-name/0 as a null pointer (which must map to empty string "").
  const uint32_t strPoolRva = 0x2b0000u;
  writeStringAtRva(strPoolRva, "CoolDragList");
  writePtrAtRva(2547896u, strPoolRva);

  writeStringAtRva(strPoolRva + 64u, "MDATA.001");
  writePtrAtRva(2786424u, strPoolRva + 64u);

  writePtrAtRva(2785752u, 0u); // autosave-name/0 -> null pointer -> ""

  return img;
}

void TestSyntheticPeExtraction()
{
  const std::vector<uint8_t> img = BuildSyntheticPe();
  std::unique_ptr<mw::res::Module> module(mw::res::Module::FromMemory(img.data(), img.size()));
  assert(module != nullptr);
  assert(!module->Is64());
  assert(module->ImageBase() == kImageBase);
  assert(module->SizeOfImage() == kSizeOfImage);

  reb::native::StartupAssets assets;
  assets.LoadFromModule(*module);
  assert(assets.Texts().size() == 93);
  assert(std::string(g_coolDragListClassName) == "CoolDragList");
  assert(std::string(g_dllLocationName) == "DllLocation");
  assert(std::string(g_strategyDllName) == "STRATEGY.DLL");
  assert(std::string(g_waveResourceType) == "WAVE");
  assert(std::string(g_musicFileNames[0]) == "MDATA.001");
  assert(std::string(g_autosaveName).empty());
}

void TestInvalidPeRejected()
{
  // Unterminated string (no null byte within max_bytes for dll-location-name).
  std::vector<uint8_t> img = BuildSyntheticPe();
  std::memset(img.data() + kSectionRawOffset + (2784272u - kSectionRva), 'X', 84u);
  std::unique_ptr<mw::res::Module> module(mw::res::Module::FromMemory(img.data(), img.size()));
  assert(module != nullptr);

  bool threw = false;
  try {
    reb::native::StartupAssets assets;
    assets.LoadFromModule(*module);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);

  // Invalid pointer (below ImageBase).
  std::vector<uint8_t> badPtrImg = BuildSyntheticPe();
  Wr32(badPtrImg, kSectionRawOffset + (2547896u - kSectionRva), 0x1234u);
  std::unique_ptr<mw::res::Module> badPtrMod(mw::res::Module::FromMemory(badPtrImg.data(), badPtrImg.size()));
  assert(badPtrMod != nullptr);
  threw = false;
  try {
    reb::native::StartupAssets assets;
    assets.LoadFromModule(*badPtrMod);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
}

void TestOptionalRealExecutableEquivalence()
{
  const char* installDir = std::getenv("MW_INSTALL_DIR");
  if (!installDir || !*installDir) {
    return;
  }
  const std::string exePath = mw::res::ResolvePath(mw::res::JoinPath(installDir, "REBEXE.EXE"));
  reb::native::StartupAssets fromExe;
  fromExe.LoadFromExecutable(exePath);
  assert(fromExe.Texts().size() == 93);
  assert(std::string(g_strategyDllName) == "STRATEGY.DLL");
  assert(std::string(g_waveResourceType) == "WAVE");
}

} // namespace

int main()
{
  TestSyntheticPeExtraction();
  TestInvalidPeRejected();
  TestOptionalRealExecutableEquivalence();
  std::cout << "startup_assets_tests passed\n";
  return 0;
}
