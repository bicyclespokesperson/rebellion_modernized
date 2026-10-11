#include <cstdint>

#include "app/rebapp_globals.h"
#include "strategy_ui/game_ui_facade.h"
#include "ui/cool_app.h"
#include "ui/resource_dll.h"

extern const char *g_alspriteDllName;  // 0x6a86e0 "alsprite.dll"
extern const char *g_emspriteDllName;  // 0x6a86d0 "emsprite.dll"
extern const char *g_albriefDllName;   // 0x6a86c4 "albrief.dll"
extern const char *g_embriefDllName;   // 0x6a86b8 "embrief.dll"
extern int g_spriteDllSide;             // 0x6b14d0 the side whose sprite DLL (slot 9) is loaded, 0 for none
extern int g_briefingDllSide;           // 0x6b14d4 the side whose briefing DLL (slot 0xd) is loaded, 0 for none

namespace {

constexpr uint32_t kSpriteDllSlot = 9;
constexpr uint32_t kBriefingDllSlot = 0xd;
constexpr int kAllianceSide = 1;

}  // namespace

void UiLoadSideSpriteDlls(int side, int loadBriefing) { LoadSideSpriteDlls(side, loadBriefing); }

void LoadSideSpriteDlls(int side, int loadBriefing) {
    HMODULE spriteModule = CoolAppGetResourceModule(kSpriteDllSlot);
    HMODULE briefingModule = CoolAppGetResourceModule(kBriefingDllSlot);
    char* dllLocation = GetDllLocation();
    if (side != g_spriteDllSide) {
        g_spriteDllSide = side;
        if (spriteModule) g_resourceDlls.UnloadSlot(kSpriteDllSlot);
        CoolAppRegisterResourceDll(dllLocation, side == kAllianceSide ? g_alspriteDllName : g_emspriteDllName,
                                   kSpriteDllSlot);
    }
    if (loadBriefing == 0) {
        if (briefingModule) g_resourceDlls.UnloadSlot(kBriefingDllSlot);
        g_briefingDllSide = 0;
    } else if (g_briefingDllSide != side) {
        g_briefingDllSide = side;
        if (briefingModule) g_resourceDlls.UnloadSlot(kBriefingDllSlot);
        CoolAppRegisterResourceDll(dllLocation, side == kAllianceSide ? g_albriefDllName : g_embriefDllName,
                                   kBriefingDllSlot);
    }
    if (dllLocation) ::operator delete(dllLocation);
}

void UnloadSpriteDll() {
    g_resourceDlls.UnloadSlot(kSpriteDllSlot);
    g_spriteDllSide = 0;
}

void UnloadBriefingDll() {
    g_resourceDlls.UnloadSlot(kBriefingDllSlot);
    g_briefingDllSide = 0;
}
