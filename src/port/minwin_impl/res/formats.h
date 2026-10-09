// Decoders for the standard resource types the game uses: RT_STRING, RT_GROUP_CURSOR/RT_CURSOR, RT_GROUP_ICON/RT_ICON,
// RT_ACCELERATOR and RT_VERSION, plus a DIB (RT_BITMAP) header reader. All take a Module and return plain data.
#pragma once
#include <stdint.h>

#include <string>
#include <vector>

#include "pe_module.h"

namespace mw::res {

// RT_STRING: strings are stored 16 per block, block number (id / 16) + 1, each as u16 length + UTF-16. The result is the
// Windows-1252 text (code points above 0xFF become '?'), what LoadStringA returns.
bool LoadStringText(const Module& module, uint32_t id, std::string* out);

// A 32-bit RGBA image with a hotspot; row 0 is the top.
struct Image {
    int width = 0, height = 0;
    int hotX = 0, hotY = 0;
    std::vector<uint8_t> rgba;  // width * height * 4, R G B A
};

// RT_GROUP_CURSOR / RT_GROUP_ICON `name`: the entry closest to the preferred size (0 = first / largest).
bool LoadCursorImage(const Module& module, const ResId& name, Image* out, int preferredSize = 0);
bool LoadIconImage(const Module& module, const ResId& name, Image* out, int preferredSize = 0);
// Decode a DIB with an AND mask appended (icon / cursor image data, height field doubled) or without (a plain bitmap).
bool DecodeDibToRgba(const uint8_t* dib, size_t size, bool hasMask, Image* out);

struct Accelerator {
    uint8_t flags;  // FVIRTKEY 1, FSHIFT 4, FCONTROL 8, FALT 0x10
    uint16_t key;
    uint16_t command;
};
bool LoadAcceleratorTable(const Module& module, const ResId& name, std::vector<Accelerator>* out);

// RT_VERSION blocks: VS_VERSIONINFO tree.
struct VersionBlock {
    std::string key;
    const uint8_t* value = nullptr;  // points into the raw data
    uint32_t valueLength = 0;        // in characters for text values, bytes for binary
    bool isText = false;
    std::vector<VersionBlock> children;
};
bool ParseVersionInfo(const uint8_t* data, size_t size, VersionBlock* root);
// "\\" -> the VS_FIXEDFILEINFO, "\\VarFileInfo\\Translation" -> the language/codepage pairs, "\\StringFileInfo\\040904b0\\FileVersion"...
const VersionBlock* QueryVersionBlock(const VersionBlock& root, const std::string& path);

// The Win32 VerLanguageName table for the common language ids.
std::string LanguageName(uint32_t languageId);

}  // namespace mw::res
