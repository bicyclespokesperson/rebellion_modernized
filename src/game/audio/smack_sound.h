#pragma once
#include <cstdint>

#include "platform/win32.h"

// State of a Smacker movie player (Smack and SmackBuf handles, playback thread), plus the hook that makes Smacker
// play its audio through the game's DirectSound device. SoundManager::Initialize does that only with
// kSoundInitShareWithSmacker, which no caller passes.
class SmackSound
{
  public:
  SmackSound();
  ~SmackSound(); // 0x611d80: Stop(1)

  // Remembers the window and Smacker parameter and tells Smacker to use `directSound`.
  void Share(HWND window, int smackParameter, void* directSound);
  // Kills the playback thread (TerminateThread); with closeHandles also closes the Smack and SmackBuf.
  void Stop(int closeHandles);
  void Close();

  int m_smackParameter; // +0x00
  void* m_smack;        // +0x04 Smack handle
  void* m_smackBuffer;  // +0x08 SmackBuf handle
  HWND m_window;        // +0x0c
  uint32_t m_unk14;     // +0x14
  uint32_t m_unk18;     // +0x18
  uint32_t m_unk1c;     // +0x1c
  uint32_t m_unk20;     // +0x20
};
