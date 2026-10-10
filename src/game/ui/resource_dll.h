#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "platform/win32.h"

// A resource DLL (TEXTCOMM.DLL, STRATEGY.DLL ...) kept loaded for the lifetime of its slot. The node id (+0x0c) is
// the slot number; resources are named across the game as slot << 16 | id.
class ResourceDll : public IdLinkNode {
public:
    explicit ResourceDll(const char* libraryName);  // 0x60ed60: LoadLibraryA
    virtual ~ResourceDll();                         // 0x60edc0 (scalar deleting destructor 0x60ed40): FreeLibrary

    HMODULE m_module;  // +0x10, null if the library could not be loaded
};

// g_resourceDlls: the loaded resource DLLs by slot.
class ResourceDllList : public IdSortedLinkList {
public:
    ResourceDllList();           // 0x604410
    virtual ~ResourceDllList();  // 0x604450 (scalar deleting destructor 0x604430)

    // Unloads every DLL registered under `slot`.
    void UnloadSlot(uint32_t slot);  // 0x604460
};

// Number of font slots a FontTable hands out.
constexpr int kFontSlotCount = 14;

// The game's fonts: one face ("Arial"), slot -> size/weight table (see CreateSlotFont), created on first use.
class FontTable {
public:
    explicit FontTable(const char* faceName);  // 0x60ee20
    ~FontTable();                              // 0x60ee70: deletes the fonts that were created
    HFONT GetFont(uint32_t slot);              // 0x60eea0: null for slot >= kFontSlotCount
    void CreateSlotFont(int slot);             // 0x60eed0 (not CreateFont: windows.h defines that name as a macro)

    HFONT m_fonts[kFontSlotCount];  // +0x00
    char m_faceName[100];           // +0x38
};

HFONT CoolGetFont(int slot);  // 0x6002b0: g_fontTable.GetFont(slot)

extern ResourceDllList g_resourceDlls;  // 0x6be4c8
extern FontTable g_fontTable;           // 0x6be4f8

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(ResourceDll, 0x14);
REB_ASSERT_OFFSET(ResourceDll, m_module, 0x10);
REB_ASSERT_SIZE(ResourceDllList, 0x10);
REB_ASSERT_SIZE(FontTable, 0x9c);
REB_ASSERT_OFFSET(FontTable, m_faceName, 0x38);
#pragma clang diagnostic pop
