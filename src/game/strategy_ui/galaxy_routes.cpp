#include <cstdint>

#include "app/rebapp_globals.h"
#include "strategy_ui/game_ui_facade.h"
#include "ui/cool_app.h"
#include "ui/resource_dll.h"

extern const char g_albriefDllName[];    // 0x6a86c4 "albrief.dll"
extern const char g_embriefDllName[];    // 0x6a86b8 "embrief.dll"
extern const char g_alspriteDllName[];   // 0x6a86e0 "alsprite.dll"
extern const char g_emspriteDllName[];   // 0x6a86d0 "emsprite.dll"

extern int g_spriteDllSide;    // 0x6b14d0 the side whose sprite DLL (slot 9) is loaded, 0 for none
extern int g_briefingDllSide;  // 0x6b14d4 the side whose briefing DLL (slot 0xd) is loaded, 0 for none

namespace {
constexpr uint32_t kSpriteSlot = 9;
constexpr uint32_t kBriefingSlot = 0xd;
}  // namespace

void UiLoadSideSpriteDlls(int side, int loadBriefing) { LoadSideSpriteDlls(side, loadBriefing); }

void LoadSideSpriteDlls(int side, int loadBriefing) {
    HMODULE spriteModule = CoolAppGetResourceModule(kSpriteSlot);
    HMODULE briefingModule = CoolAppGetResourceModule(kBriefingSlot);
    char* dllLocation = GetDllLocation();
    if (side != g_spriteDllSide) {
        g_spriteDllSide = side;
        if (spriteModule) g_resourceDlls.UnloadSlot(kSpriteSlot);
        CoolAppRegisterResourceDll(dllLocation, side != 1 ? g_emspriteDllName : g_alspriteDllName, kSpriteSlot);
    }
    if (loadBriefing == 0) {
        if (briefingModule) g_resourceDlls.UnloadSlot(kBriefingSlot);
        g_briefingDllSide = 0;
    } else if (g_briefingDllSide != side) {
        g_briefingDllSide = side;
        if (briefingModule) g_resourceDlls.UnloadSlot(kBriefingSlot);
        CoolAppRegisterResourceDll(dllLocation, side != 1 ? g_embriefDllName : g_albriefDllName, kBriefingSlot);
    }
    if (dllLocation) operator delete(dllLocation);
}

void UnloadSpriteDll() {
    g_resourceDlls.UnloadSlot(kSpriteSlot);
    g_spriteDllSide = 0;
}

void UnloadBriefingDll() {
    g_resourceDlls.UnloadSlot(kBriefingSlot);
    g_briefingDllSide = 0;
}
