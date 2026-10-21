#pragma once
#include "core/reb_abi.h"
#include "platform/smacker.h"

// The Smacker entry points as function pointers, so one scenario drives either the real DLL or the implementation under
// test.
struct SmackApiTable {
  Smack*(REB_STDCALL* open)(const char*, uint32_t, uint32_t) = nullptr;
  void(REB_STDCALL* close)(Smack*) = nullptr;
  uint32_t(REB_STDCALL* doFrame)(Smack*) = nullptr;
  void(REB_STDCALL* nextFrame)(Smack*) = nullptr;
  uint32_t(REB_STDCALL* wait)(Smack*) = nullptr;
  void(REB_STDCALL* gotoFrame)(Smack*, uint32_t) = nullptr;
  void(REB_STDCALL* toBuffer)(Smack*, uint32_t, uint32_t, uint32_t, uint32_t, void*, uint32_t) = nullptr;
  uint32_t(REB_STDCALL* toBufferRect)(Smack*, uint32_t) = nullptr;
  void(REB_STDCALL* volumePan)(Smack*, uint32_t, uint32_t, uint32_t) = nullptr;
  SmackBuf*(REB_STDCALL* bufferOpen)(HWND, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = nullptr;
  void(REB_STDCALL* bufferClose)(SmackBuf*) = nullptr;
  uint32_t(REB_STDCALL* bufferNewPalette)(SmackBuf*, void*, uint32_t) = nullptr;
  uint32_t(REB_STDCALL* bufferSetPalette)(SmackBuf*) = nullptr;
  uint32_t(REB_STDCALL* getTrackData)(Smack*, void*, uint32_t) = nullptr;
  void(REB_STDCALL* soundUseDirectSound)(void*) = nullptr;
  HWND (*makeWindow)() = nullptr;
};
