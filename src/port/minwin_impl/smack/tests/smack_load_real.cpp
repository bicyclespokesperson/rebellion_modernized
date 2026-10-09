// Loads the real SMACKW32.DLL by name (Windows, i.e. Wine).
#include <windows.h>

#include "smack_ops.h"
#include "smack_table.h"

namespace {
HWND MakeRealWindow() { return CreateWindowExA(0, "STATIC", "smack", WS_POPUP, 0, 0, 640, 480, nullptr, nullptr, nullptr, nullptr); }
}  // namespace

bool LoadSmackApi(SmackApiTable* api, const char* dllPath) {
    HMODULE h = LoadLibraryA(dllPath ? dllPath : "SMACKW32.DLL");
    if (!h) return false;
#define BIND(field, name) api->field = reinterpret_cast<decltype(api->field)>(GetProcAddress(h, "_" name))
    BIND(open, "SmackOpen@12");
    BIND(close, "SmackClose@4");
    BIND(doFrame, "SmackDoFrame@4");
    BIND(nextFrame, "SmackNextFrame@4");
    BIND(wait, "SmackWait@4");
    BIND(gotoFrame, "SmackGoto@8");
    BIND(toBuffer, "SmackToBuffer@28");
    BIND(toBufferRect, "SmackToBufferRect@8");
    BIND(volumePan, "SmackVolumePan@16");
    BIND(bufferOpen, "SmackBufferOpen@24");
    BIND(bufferClose, "SmackBufferClose@4");
    BIND(bufferNewPalette, "SmackBufferNewPalette@12");
    BIND(bufferSetPalette, "SmackBufferSetPalette@4");
    BIND(getTrackData, "SmackGetTrackData@12");
    BIND(soundUseDirectSound, "SmackSoundUseDirectSound@4");
#undef BIND
    api->makeWindow = MakeRealWindow;
    if (api->soundUseDirectSound) api->soundUseDirectSound(nullptr);
    return api->open && api->doFrame && api->toBuffer;
}
