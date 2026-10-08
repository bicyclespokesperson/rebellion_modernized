#include <new>

#include "ui/cool_window.h"
#include "ui/resource_dll.h"

// Static initializers of the UI framework's globals (the three pieces of each: construct, register the destructor with
// atexit, and the chain that runs both).

extern "C" int __cdecl atexit(void(__cdecl* function)(void));
extern "C" void __cdecl LAB_005fefb0();
extern "C" void __cdecl LAB_005ff850();
extern "C" void __cdecl LAB_005ff8a0();
extern "C" void __cdecl LAB_005ff8f0();

void ConstructResourceDllList() { new (&g_resourceDlls) ResourceDllList(); }  // 0x5fef90

void RegisterResourceDllListAtExit() { atexit(LAB_005fefb0); }  // 0x5fefa0

void InitResourceDllList() {  // 0x5fef80
    ConstructResourceDllList();
    RegisterResourceDllListAtExit();
}

void ConstructFontTable() { new (&g_fontTable) FontTable("Arial"); }  // 0x5ff830

void RegisterFontTableAtExit() { atexit(LAB_005ff850); }  // 0x5ff840

void InitFontTable() {  // 0x5ff820
    ConstructFontTable();
    RegisterFontTableAtExit();
}

void ConstructPopupStack() { new (&g_popupStack) NodeStack(); }  // 0x5ff880

void RegisterPopupStackAtExit() { atexit(LAB_005ff8a0); }  // 0x5ff890

void InitPopupStack() {  // 0x5ff870
    ConstructPopupStack();
    RegisterPopupStackAtExit();
}

void ConstructWindowRegistry() { new (&g_windowRegistry) IdAvlTree(); }  // 0x5ff8d0

void RegisterWindowRegistryAtExit() { atexit(LAB_005ff8f0); }  // 0x5ff8e0

void InitWindowRegistry() {  // 0x5ff8c0
    ConstructWindowRegistry();
    RegisterWindowRegistryAtExit();
}
