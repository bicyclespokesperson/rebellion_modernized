// RAD Smacker (SMACKW32.DLL) declarations, as far as the game uses them. The layouts follow RAD's smack.h of the
// 1997-98 era: a field the game reads has its original offset, everything else is padding.
// Hybrid build: the game's own import table (REBEXE.EXE imports these from the proxy SMACKW32.DLL).
// Port: implemented on a native Smacker decoder.
#pragma once
#include <cstdint>

#include "core/layout.h"
#include "platform/win32.h"

constexpr uint32_t kSmackAllAudioTracks = 0xfe000;  // SMACKTRACK1..7: open flag and SmackVolumePan track mask
constexpr uint32_t kSmackAutoExtraBuffer = 0xffffffff;

// An open movie (RAD `Smack`).
struct Smack {
    uint32_t m_version;       // +0x00
    uint32_t m_width;         // +0x04
    uint32_t m_height;        // +0x08
    uint32_t m_frameCount;    // +0x0c
    uint8_t m_unk10[0x58];    // +0x10 (frame rate, flags, track sizes, ...)
    uint32_t m_newPalette;    // +0x68, set while the current frame brings a palette
    uint8_t m_palette[0x304]; // +0x6c
    uint32_t m_paletteType;   // +0x370
    uint32_t m_frameNumber;   // +0x374
    int32_t m_lastRectX;      // +0x378, rectangle changed by the last SmackToBufferRect
    int32_t m_lastRectY;      // +0x37c
    int32_t m_lastRectWidth;  // +0x380
    int32_t m_lastRectHeight; // +0x384
};

// A Smacker output buffer bound to a window (RAD `SmackBuf`).
struct SmackBuf {
    uint8_t m_reversed;       // +0x00 (the game passes this byte as SmackToBuffer's flags)
    uint8_t m_unk01[3];
    uint32_t m_surfaceType;   // +0x04, the `SmackSurface` argument of SmackToBufferRect
    uint8_t m_unk08[8];
    uint32_t m_width;         // +0x10
    uint32_t m_height;        // +0x14
    uint8_t m_unk18[0x430];
    void* m_buffer;           // +0x448, pixel memory SmackToBuffer decodes into
};

// The BlitType the game passes to SmackBufferOpen (RAD smack.h value not identified).
constexpr uint32_t kSmackBufferBlitType = 3;

extern "C" {
__declspec(dllimport) void __stdcall SmackSoundUseDirectSound(void* directSound);
__declspec(dllimport) Smack* __stdcall SmackOpen(const char* name, uint32_t flags, uint32_t extraBuffer);
__declspec(dllimport) void __stdcall SmackClose(Smack* smack);
__declspec(dllimport) uint32_t __stdcall SmackDoFrame(Smack* smack);
__declspec(dllimport) void __stdcall SmackNextFrame(Smack* smack);
__declspec(dllimport) uint32_t __stdcall SmackWait(Smack* smack);
__declspec(dllimport) void __stdcall SmackGoto(Smack* smack, uint32_t frame);
__declspec(dllimport) void __stdcall SmackToBuffer(Smack* smack, uint32_t left, uint32_t top, uint32_t pitch,
                                                   uint32_t destHeight, void* buffer, uint32_t flags);
__declspec(dllimport) uint32_t __stdcall SmackToBufferRect(Smack* smack, uint32_t smackSurface);
__declspec(dllimport) void __stdcall SmackVolumePan(Smack* smack, uint32_t trackFlags, uint32_t volume, uint32_t pan);
__declspec(dllimport) SmackBuf* __stdcall SmackBufferOpen(HWND window, uint32_t blitType, uint32_t width,
                                                          uint32_t height, uint32_t zoomWidth, uint32_t zoomHeight);
__declspec(dllimport) void __stdcall SmackBufferClose(SmackBuf* buffer);
__declspec(dllimport) uint32_t __stdcall SmackBufferBlit(SmackBuf* buffer, HDC dc, int32_t windowX, int32_t windowY,
                                                         int32_t subX, int32_t subY, int32_t subWidth,
                                                         int32_t subHeight);
__declspec(dllimport) uint32_t __stdcall SmackBufferNewPalette(SmackBuf* buffer, void* palette, uint32_t paletteType);
__declspec(dllimport) uint32_t __stdcall SmackBufferSetPalette(SmackBuf* buffer);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_OFFSET(Smack, m_frameCount, 0x0c);
REB_ASSERT_OFFSET(Smack, m_newPalette, 0x68);
REB_ASSERT_OFFSET(Smack, m_palette, 0x6c);
REB_ASSERT_OFFSET(Smack, m_paletteType, 0x370);
REB_ASSERT_OFFSET(Smack, m_frameNumber, 0x374);
REB_ASSERT_OFFSET(Smack, m_lastRectX, 0x378);
REB_ASSERT_OFFSET(Smack, m_lastRectHeight, 0x384);
REB_ASSERT_OFFSET(SmackBuf, m_surfaceType, 0x04);
REB_ASSERT_OFFSET(SmackBuf, m_width, 0x10);
REB_ASSERT_OFFSET(SmackBuf, m_height, 0x14);
REB_ASSERT_OFFSET(SmackBuf, m_buffer, 0x448);
#pragma clang diagnostic pop
