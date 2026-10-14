#pragma once
#include <cstdint>

#include "audio/cd_audio.h"
#include "audio/smack_sound.h"
#include "audio/sound_buffer.h"
#include "audio/sound_source.h"
#include "core/entry_identity.h"
#include "platform/win32.h"

class SoundSource;

enum SoundKind : unsigned
{
  kSoundKindWave = 1, // the only kind SoundManager can play
};

// Result codes returned by the SoundManager entry points (0 = ok).
enum SoundResult : int
{
  kSoundOk = 0,
  kSoundFailed = 1,
  kSoundUnsupportedKind = 3,
  kSoundNoSuchSound = 5,
  kSoundNoDevice = 6,
  kSoundAlreadyInitialized = 7,
  kSoundCreatePrimaryFailed = 8,
  kSoundQueryListenerFailed = 9,
  kSoundSetFormatFailed = 10,
  kSoundCooperativeLevelFailed = 14,
  kSoundAlreadyLoaded = 15,
  kSoundNotLoaded = 16,
};

// Flags of SoundManager::Initialize.
enum SoundInitFlags : unsigned
{
  kSoundInitShareWithSmacker = 0x1000,
  kSoundInitCdAudio = 0x10000,
  kSoundInitListenerFormat = 0x100000,
};

constexpr uint32_t kSoundBufferFinishedMessage = 0x4c9;

// What a caller hands to SoundManager to play, query, stop or release one sound. Identifies the sound (resource id
// and the resource module it lives in) and receives the handle of the DirectSound buffer playing it.
class SoundRequest
{
public:
  SoundRequest(unsigned kind, int moduleIndex, int soundId, int loop, int keepLoaded, int handle); // 0x610b20
  SoundRequest(const SoundRequest& other); // 0x610b70
  virtual ~SoundRequest(); // 0x404070 (shared)

  // Key of the SoundSource in the active AudioContext.
  uint32_t SourceKey() const
  {
    return ((static_cast<uint32_t>(m_moduleIndex) & 0xffff) << 16) + (static_cast<uint32_t>(m_soundId) & 0xffff);
  }

  int m_soundId; // +0x04 (low 16 bits are the WAVE resource id)
  int m_moduleIndex; // +0x08 which resource DLL (see 0x5fefd0)
  int m_handle; // +0x0c key of the playing buffer, 0 if none
  uint32_t m_kind : 3; // +0x10 SoundKind
  uint32_t m_loop : 1;
  uint32_t m_keepLoaded : 1; // keep the loaded sound when everything else is released
  uint32_t m_playing : 1; // set when playback started, refreshed by SoundManager::Query
  uint32_t m_bit6 : 1;
};

using SoundEventCallback = void (*)(uint32_t message, uint32_t first, uint32_t second);

// The game's SoundManager API operates on the active application's AudioContext; callers pass a SoundRequest.
class SoundManager
{
public:
  // Each screen initialises with its window and shuts down again; the device lives while the use count is positive.
  static int Initialize(HWND window, unsigned flags, int smackParameter); // 0x610be0
  static void Shutdown(int force); // 0x610c00
  static IDirectSound* GetDirectSound(); // 0x610c10
  static void SetEventCallback(SoundEventCallback callback); // 0x610c20
  static int Play(SoundRequest* request); // 0x610c30
  static int PlayFromFile(SoundRequest* request, const char* fileName, long seekPosition); // 0x610c40
  static int Release(SoundRequest* request); // 0x610c60
  static void ReleaseAll(int everything); // 0x610c70
  static int Query(SoundRequest* request); // 0x610c80
  static int Stop(SoundRequest* request, int sweep); // 0x610c90
  static int SetVolume(SoundRequest* request, int volume); // 0x610cb0
  static void SetDefaultVolume(int volume); // 0x610cd0
  static void SetMasterVolume(LONG volume); // 0x610ce0
  static LONG GetMasterVolume(); // 0x6112f0 (0x610cf0 jumps here)

  static int InitializeImpl(HWND window, unsigned flags, int smackParameter); // 0x610d00
  static void ShutdownImpl(int force); // 0x610dd0
  static int SetPrimaryBufferFormat3D(uint32_t packedFormat); // 0x610e30
  static void ReleasePrimaryBuffer(); // 0x610f70
  // packedFormat: bits 0-7 bits per sample, 8-15 channel count, 16-31 sample rate.
  static int SetPrimaryBufferFormat(uint32_t packedFormat); // 0x611330

  static int CreateBuffer(DSBUFFERDESC1* description, IDirectSoundBuffer** buffer); // 0x611140
  static int DuplicateBuffer(IDirectSoundBuffer** duplicate, IDirectSoundBuffer** original); // 0x6111b0
  static int PostBufferEvent(uint32_t message, uint32_t first, uint32_t second); // 0x6111e0
  static void HandleBufferEvent(uint32_t message, uint32_t sourceKey, uint32_t bufferKey); // 0x611200
  static void SweepSources(int unused); // 0x611230
  static void SetMasterVolumeImpl(LONG volume); // 0x6112c0

  static int LoadResourceSource(SoundRequest* request); // 0x611440
  static int LoadFileSource(SoundRequest* request, const char* fileName, long seekPosition); // 0x6114d0
  static int StartSource(SoundRequest* request); // 0x611580
  static int PlayImpl(SoundRequest* request); // 0x6115f0
  static int PlayFromFileImpl(SoundRequest* request, const char* fileName, long seekPosition); // 0x611650
  static int ReleaseImpl(SoundRequest* request); // 0x6116c0
  static void ReleaseAllImpl(int everything); // 0x611740
  static int QueryImpl(SoundRequest* request); // 0x611780
  static int StopImpl(SoundRequest* request, int sweep); // 0x6117d0
  static int SetVolumeImpl(SoundRequest* request, int volume); // 0x611830
  static int CreateResourceSource(SoundRequest* request, HMODULE module, SoundSource** source); // 0x611890
  static int
  CreateFileSource(SoundRequest* request, const char* fileName, long seekPosition, SoundSource** source); // 0x611960

  static DWORD WINAPI EventThreadProc(void*); // 0x6119f0
  static int QueueEvent(uint32_t message, uint32_t first, uint32_t second); // 0x611b00
  static int StartEventThread(); // 0x611bf0
  static void StopEventThread(); // 0x611cb0
};

uint32_t GetDefaultVolume(); // 0x613ff0
