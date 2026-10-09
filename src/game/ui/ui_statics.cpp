#include "core/reb_abi.h"
#include <new>

#include "ui/cool_window.h"
#include "ui/resource_dll.h"

// Static initializers of the UI framework's globals (the three pieces of each: construct, register the destructor with
// atexit, and the chain that runs both).

extern "C" int REB_CDECL atexit(void(REB_CDECL* function)(void));
void DestroyResourceDlls();
void DestroyFontTable();
void DestroyPopupStack();
void DestroyWindowRegistry();

void ConstructResourceDllList() { new (&g_resourceDlls) ResourceDllList(); }  // 0x5fef90

void RegisterResourceDllListAtExit() { atexit(DestroyResourceDlls); }  // 0x5fefa0

void InitResourceDllList() {  // 0x5fef80
    ConstructResourceDllList();
    RegisterResourceDllListAtExit();
}

void ConstructFontTable() { new (&g_fontTable) FontTable("Arial"); }  // 0x5ff830

void RegisterFontTableAtExit() { atexit(DestroyFontTable); }  // 0x5ff840

void InitFontTable() {  // 0x5ff820
    ConstructFontTable();
    RegisterFontTableAtExit();
}

void ConstructPopupStack() { new (&g_popupStack) NodeStack(); }  // 0x5ff880

void RegisterPopupStackAtExit() { atexit(DestroyPopupStack); }  // 0x5ff890

void InitPopupStack() {  // 0x5ff870
    ConstructPopupStack();
    RegisterPopupStackAtExit();
}

void ConstructWindowRegistry() { new (&g_windowRegistry) IdAvlTree(); }  // 0x5ff8d0

void RegisterWindowRegistryAtExit() { atexit(DestroyWindowRegistry); }  // 0x5ff8e0

void InitWindowRegistry() {  // 0x5ff8c0
    ConstructWindowRegistry();
    RegisterWindowRegistryAtExit();
}

// ---- destructors registered with atexit ----------------------------------------------------

extern uint8_t g_resourceDllsDestroyedGuard;  // 0x6be4d8

void DestroyResourceDlls() {
    if (!(g_resourceDllsDestroyedGuard & 0x1)) {
        g_resourceDllsDestroyedGuard |= 0x1;
        g_resourceDlls.~ResourceDllList();
    }
}

extern uint8_t g_fontTableDestroyedGuard;  // 0x6be594

void DestroyFontTable() {
    if (!(g_fontTableDestroyedGuard & 0x1)) {
        g_fontTableDestroyedGuard |= 0x1;
        g_fontTable.~FontTable();
    }
}

void DestroyPopupStack() {
    if (!(g_fontTableDestroyedGuard & 0x2)) {
        g_fontTableDestroyedGuard |= 0x2;
        g_popupStack.~NodeStack();
    }
}

void DestroyWindowRegistry() {
    if (!(g_fontTableDestroyedGuard & 0x4)) {
        g_fontTableDestroyedGuard |= 0x4;
        g_windowRegistry.~IdAvlTree();
    }
}
