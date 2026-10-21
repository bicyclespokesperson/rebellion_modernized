#include "audio/sound_manager.h"

#include <cstdlib>
#include <new>
#include <system_error>

#include "core/checked_new.h"
#include "port/native/audio_context.h"
#include "port/native/platform.h"
#include "ui/cool_window.h"

namespace
{
reb::native::AudioContext& Audio()
{
  return *reb::native::CurrentAudioContext();
}
bool audioContextAvailable()
{
  return reb::native::CurrentAudioContext() != nullptr;
}
} // namespace

extern const char* g_waveResourceType; // 0x6ac954 "WAVE"

// Functions of other modules.
namespace
{

constexpr uint32_t kDefaultPrimaryFormat = 0x2b110110;  // 11025 Hz, 1 channel, 16 bit
constexpr uint32_t kListenerPrimaryFormat = 0x2b110208; // 11025 Hz, 2 channels, 8 bit
constexpr int kWakeEvent = 0;
constexpr int kStopEvent = 1;
constexpr int kMinVolume = -10000;
constexpr uint16_t kBitsPerByte = 8;
constexpr uint8_t kPlayWithDefaultVolume = 0;

// Fills Audio().primaryFormat from a packed format and returns it.
void StorePrimaryFormat(uint32_t packedFormat)
{
  const uint16_t bitsPerSample = packedFormat & 0xff;
  const uint16_t channels = (packedFormat >> 8) & 0xff;
  Audio().primaryFormat = {};
  Audio().primaryFormat.wFormatTag = WAVE_FORMAT_PCM;
  Audio().primaryFormat.nChannels = channels;
  Audio().primaryFormat.wBitsPerSample = bitsPerSample;
  Audio().primaryFormat.nBlockAlign = static_cast<uint16_t>(static_cast<int>(channels * bitsPerSample) / kBitsPerByte);
  Audio().primaryFormat.nSamplesPerSec = packedFormat >> 16;
  Audio().primaryFormat.nAvgBytesPerSec = Audio().primaryFormat.nSamplesPerSec * Audio().primaryFormat.nBlockAlign;
}

} // namespace

// ---- SoundRequest -------------------------------------------------------------------------------------------

SoundRequest::SoundRequest(unsigned kind, int moduleIndex, int soundId, int loop, int keepLoaded, int handle)
{
  m_soundId = soundId;
  m_moduleIndex = moduleIndex;
  m_handle = handle;
  m_kind = kind;
  m_loop = loop;
  m_keepLoaded = keepLoaded;
  m_playing = 0;
  m_bit6 = 0;
}

SoundRequest::SoundRequest(const SoundRequest& other)
{
  m_soundId = other.m_soundId;
  m_moduleIndex = other.m_moduleIndex;
  m_handle = other.m_handle;
  m_kind = other.m_kind;
  m_loop = other.m_loop;
  m_keepLoaded = other.m_keepLoaded;
  m_bit6 = other.m_bit6;
  m_playing = other.m_playing;
}

SoundRequest::~SoundRequest() = default;

// ---- statics ------------------------------------------------------------------------------------------------

// ---- entry points -------------------------------------------------------------------------------------------

int SoundManager::Initialize(HWND window, unsigned flags, int smackParameter)
{
  if (!audioContextAvailable()) {
    return kSoundFailed;
  }
  return InitializeImpl(window, flags, smackParameter);
}

void SoundManager::Shutdown(int force)
{
  if (audioContextAvailable()) {
    ShutdownImpl(force);
  }
}

IDirectSound* SoundManager::GetDirectSound()
{
  return audioContextAvailable() ? Audio().directSound : nullptr;
}

void SoundManager::SetEventCallback(SoundEventCallback callback)
{
  if (audioContextAvailable()) {
    Audio().soundEventCallback = callback;
  }
}

int SoundManager::Play(SoundRequest* request)
{
  return audioContextAvailable() ? PlayImpl(request) : kSoundFailed;
}

int SoundManager::PlayFromFile(SoundRequest* request, const char* fileName, long seekPosition)
{
  return audioContextAvailable() ? PlayFromFileImpl(request, fileName, seekPosition) : kSoundFailed;
}

int SoundManager::Release(SoundRequest* request)
{
  return audioContextAvailable() ? ReleaseImpl(request) : kSoundNoSuchSound;
}

void SoundManager::ReleaseAll(int everything)
{
  if (audioContextAvailable()) {
    ReleaseAllImpl(everything);
  }
}

int SoundManager::Query(SoundRequest* request)
{
  return audioContextAvailable() ? QueryImpl(request) : kSoundNotLoaded;
}

int SoundManager::Stop(SoundRequest* request, int sweep)
{
  return audioContextAvailable() ? StopImpl(request, sweep) : kSoundNotLoaded;
}

int SoundManager::SetVolume(SoundRequest* request, int volume)
{
  return audioContextAvailable() ? SetVolumeImpl(request, volume) : kSoundNotLoaded;
}

void SoundManager::SetDefaultVolume(int volume)
{
  if (audioContextAvailable()) {
    Audio().soundVolume = volume;
  }
}

void SoundManager::SetMasterVolume(LONG volume)
{
  if (audioContextAvailable()) {
    SetMasterVolumeImpl(volume);
  }
}

// ---- device -------------------------------------------------------------------------------------------------

int SoundManager::InitializeImpl(HWND window, unsigned flags, int smackParameter)
{
  int result;
  Audio().soundWindow = window;
  if (flags & kSoundInitCdAudio) {
    Audio().cdAudio.Open(window);
  }
  if (Audio().directSound == nullptr) {
    if (reb::compat::DirectSoundCreate(nullptr, &Audio().directSound, nullptr) == DS_OK) {
      if (Audio().directSound->SetCooperativeLevel(Audio().soundWindow, DSSCL_EXCLUSIVE) == DS_OK) {
        if (flags & kSoundInitListenerFormat) {
          result = SetPrimaryBufferFormat3D(kListenerPrimaryFormat);
        } else {
          result = SetPrimaryBufferFormat(kDefaultPrimaryFormat);
        }
      } else {
        result = kSoundCooperativeLevelFailed;
      }
    } else {
      result = kSoundNoDevice;
    }
  } else {
    result = kSoundAlreadyInitialized;
  }
  if (flags & kSoundInitShareWithSmacker) {
    Audio().smackSound.Share(window, smackParameter, Audio().directSound);
  }
  StartEventThread();
  ++Audio().soundUseCount;
  return result;
}

void SoundManager::ShutdownImpl(int force)
{
  --Audio().soundUseCount;
  bool lastUser;
  if (force == 1 || Audio().soundUseCount < 0) {
    Audio().soundUseCount = 0;
    lastUser = true;
  } else {
    lastUser = Audio().soundUseCount == 0;
  }
  if (lastUser) {
    StopEventThread();
    ReleaseAllImpl(1);
    ReleasePrimaryBuffer();
    if (Audio().directSound) {
      Audio().directSound->Release();
      Audio().directSound = nullptr;
    }
  }
}

int SoundManager::SetPrimaryBufferFormat3D(uint32_t packedFormat)
{
  StorePrimaryFormat(packedFormat);
  DSBUFFERDESC1 description = {};
  description.dwSize = sizeof(description);
  description.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRL3D | DSBCAPS_GLOBALFOCUS;
  if (Audio().directSound->CreateSoundBuffer(reinterpret_cast<LPCDSBUFFERDESC>(&description), &Audio().primaryBuffer,
                                             nullptr)
      != DS_OK) {
    return kSoundCreatePrimaryFailed;
  }
  if (Audio().primaryBuffer->SetFormat(&Audio().primaryFormat) != DS_OK) {
    ReleasePrimaryBuffer();
    return kSoundSetFormatFailed;
  }
  if (Audio().primaryBuffer->QueryInterface(reb::compat::IID_IDirectSound3DListener, reinterpret_cast<void**>(&Audio().listener))
      != DS_OK) {
    return kSoundQueryListenerFailed;
  }
  if (Audio().primaryBuffer->Play(0, 0, DSBPLAY_LOOPING) != DS_OK) {
    ReleasePrimaryBuffer();
    return kSoundCreatePrimaryFailed;
  }
  return kSoundOk;
}

void SoundManager::ReleasePrimaryBuffer()
{
  if (Audio().primaryBuffer) {
    Audio().primaryBuffer->Stop();
    Audio().primaryBuffer->Release();
    Audio().primaryBuffer = nullptr;
  }
  if (Audio().listener) {
    Audio().listener->Release();
    Audio().listener = nullptr;
  }
}

int SoundManager::SetPrimaryBufferFormat(uint32_t packedFormat)
{
  StorePrimaryFormat(packedFormat);
  if (!Audio().primaryBuffer) {
    DSBUFFERDESC1 description = {};
    description.dwSize = sizeof(description);
    description.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME;
    if (!Audio().directSound) {
      return kSoundCreatePrimaryFailed;
    }
    if (Audio().directSound->CreateSoundBuffer(reinterpret_cast<LPCDSBUFFERDESC>(&description), &Audio().primaryBuffer,
                                               nullptr)
        != DS_OK) {
      return kSoundCreatePrimaryFailed;
    }
  }
  return Audio().primaryBuffer->SetFormat(&Audio().primaryFormat) != DS_OK ? kSoundSetFormatFailed : kSoundOk;
}

void SoundManager::SetMasterVolumeImpl(LONG volume)
{
  if (!Audio().primaryBuffer) {
    SetPrimaryBufferFormat(kListenerPrimaryFormat);
    if (!Audio().primaryBuffer) {
      return;
    }
  }
  Audio().primaryBuffer->SetVolume(volume);
}

LONG SoundManager::GetMasterVolume()
{
  if (!audioContextAvailable()) {
    return 0;
  }
  LONG volume = 0;
  if (!Audio().primaryBuffer) {
    SetPrimaryBufferFormat(kListenerPrimaryFormat);
    if (!Audio().primaryBuffer) {
      return volume;
    }
  }
  Audio().primaryBuffer->GetVolume(&volume);
  return volume;
}

// ---- buffers and events -------------------------------------------------------------------------------------

int SoundManager::CreateBuffer(DSBUFFERDESC1* description, IDirectSoundBuffer** buffer)
{
  if (!audioContextAvailable()) {
    return kSoundNoDevice;
  }
  if (!Audio().directSound) {
    return kSoundNoDevice;
  }
  int result = Audio().directSound->CreateSoundBuffer(reinterpret_cast<LPCDSBUFFERDESC>(description), buffer, nullptr);
  if (result != DS_OK) {
    return result;
  }
  if (Audio().soundVolume > 0) {
    if (Audio().soundVolume <= -kMinVolume) {
      // A positive default volume means an attenuation: it is negated once, in place.
      Audio().soundVolume = -Audio().soundVolume;
      (*buffer)->SetVolume(Audio().soundVolume);
      return DS_OK;
    }
    Audio().soundVolume = 0;
  }
  (*buffer)->SetVolume(Audio().soundVolume);
  return DS_OK;
}

int SoundManager::DuplicateBuffer(IDirectSoundBuffer** duplicate, IDirectSoundBuffer** original)
{
  if (!audioContextAvailable()) {
    return kSoundNoDevice;
  }
  if (!Audio().directSound) {
    return kSoundNoDevice;
  }
  return Audio().directSound->DuplicateSoundBuffer(*original, duplicate);
}

int SoundManager::PostBufferEvent(uint32_t message, uint32_t first, uint32_t second)
{
  if (!audioContextAvailable()) {
    return 0;
  }
  QueueEvent(message, first, second);
  return 0;
}

void SoundManager::HandleBufferEvent(uint32_t message, uint32_t sourceKey, uint32_t bufferKey)
{
  if (!audioContextAvailable()) {
    return;
  }
  if (message != kSoundBufferFinishedMessage) {
    return;
  }
  if (KeyedSoundNode* node = Audio().soundSources.Find(sourceKey)) {
    static_cast<SoundSource*>(node)->ReleaseUser(bufferKey);
  }
}

void SoundManager::SweepSources(int)
{
  if (!audioContextAvailable()) {
    return;
  }
  if (Audio().sweepBusy.exchange(1) != 0) {
    return;
  }
  auto& catalog = Audio().soundCatalog;
  for (auto entry = catalog.Entries().begin(); entry != catalog.Entries().end();) {
    auto current = entry++;
    auto* node = current->second.get();
    if (static_cast<SoundSource*>(node)->m_users == 1) {
      Audio().soundCatalog.DeleteNode(static_cast<SoundSource*>(node));
    }
  }
  auto& sources = Audio().soundSources;
  for (auto entry = sources.Entries().begin(); entry != sources.Entries().end();) {
    auto current = entry++;
    auto* node = current->second.get();
    auto* source = static_cast<SoundSource*>(node);
    if (source->m_keepLoaded == 0 && source->IsAnyPlaying() == 0) {
      Audio().soundSources.DeleteNode(source);
    }
  }
  Audio().sweepBusy.exchange(0);
}

// ---- sounds -------------------------------------------------------------------------------------------------

int SoundManager::LoadResourceSource(SoundRequest* request)
{
  if (Audio().soundSources.Find(request->SourceKey())) {
    return kSoundAlreadyLoaded;
  }
  SoundSource* source;
  int error = CreateResourceSource(request, FindResourceModule(request->m_moduleIndex), &source);
  if (error != 0 || source == nullptr) {
    return kSoundFailed;
  }
  Audio().soundSources.Add(source);
  if (request->m_keepLoaded) {
    source->m_keepLoaded = 1;
  }
  return kSoundOk;
}

int SoundManager::LoadFileSource(SoundRequest* request, const char* fileName, long seekPosition)
{
  if (Audio().soundSources.Find(request->SourceKey())) {
    return kSoundAlreadyLoaded;
  }
  SoundSource* source;
  if (CreateFileSource(request, fileName, seekPosition, &source) != 0 || source == nullptr) {
    return kSoundFailed;
  }
  source->m_key = request->SourceKey();
  Audio().soundSources.Add(source);
  if (request->m_keepLoaded) {
    source->m_keepLoaded = 1;
  }
  return kSoundOk;
}

int SoundManager::StartSource(SoundRequest* request)
{
  int result;
  if (KeyedSoundNode* node = Audio().soundSources.Find(request->SourceKey())) {
    result = static_cast<SoundSource*>(node)->Play(&request->m_handle, request->m_loop);
    if (result == 0) {
      request->m_playing = 1;
    }
  } else {
    result = kSoundNotLoaded;
  }
  SweepSources(0);
  return result;
}

int SoundManager::PlayImpl(SoundRequest* request)
{
  int result = kSoundFailed;
  if (Audio().soundSources.Find(request->SourceKey()) || LoadResourceSource(request) == 0) {
    result = StartSource(request);
  }
  SweepSources(0);
  return result;
}

int SoundManager::PlayFromFileImpl(SoundRequest* request, const char* fileName, long seekPosition)
{
  int result = kSoundFailed;
  if (Audio().soundSources.Find(request->SourceKey()) || LoadFileSource(request, fileName, seekPosition) == 0) {
    result = StartSource(request);
  }
  SweepSources(0);
  return result;
}

int SoundManager::ReleaseImpl(SoundRequest* request)
{
  int result = kSoundNoSuchSound;
  if (Audio().soundSources.Find(request->SourceKey())) {
    result = kSoundOk;
    do {
      Audio().soundSources.DeleteByKey(request->SourceKey());
    } while (Audio().soundSources.Find(request->SourceKey()));
  }
  return result;
}

void SoundManager::ReleaseAllImpl(int everything)
{
  if (everything == 1) {
    Audio().soundSources.SoundIdTree::Clear();
    return;
  }
  auto& sources = Audio().soundSources;
  for (auto entry = sources.Entries().begin(); entry != sources.Entries().end();) {
    auto current = entry++;
    auto* node = current->second.get();
    auto* source = static_cast<SoundSource*>(node);
    if (source->m_keepLoaded == 0) {
      Audio().soundSources.DeleteNode(source);
    }
  }
}

int SoundManager::QueryImpl(SoundRequest* request)
{
  if (KeyedSoundNode* node = Audio().soundSources.Find(request->SourceKey())) {
    return static_cast<SoundSource*>(node)->QueryPlaying(request);
  }
  request->m_keepLoaded = 0;
  request->m_playing = 0;
  return kSoundNotLoaded;
}

int SoundManager::StopImpl(SoundRequest* request, int sweep)
{
  int result = kSoundNotLoaded;
  if (KeyedSoundNode* node = Audio().soundSources.Find(request->SourceKey())) {
    result = static_cast<SoundSource*>(node)->Stop(&request->m_handle, sweep);
  }
  if (sweep != 0) {
    SweepSources(0);
  }
  return result;
}

int SoundManager::SetVolumeImpl(SoundRequest* request, int volume)
{
  if (volume > 0 || volume < kMinVolume) {
    return kSoundFailed;
  }
  if (KeyedSoundNode* node = Audio().soundSources.Find(request->SourceKey())) {
    return static_cast<SoundSource*>(node)->SetVolume(request->m_handle, volume);
  }
  return kSoundNotLoaded;
}

int SoundManager::CreateResourceSource(SoundRequest* request, HMODULE module, SoundSource** source)
{
  int result = kSoundFailed;
  if (request->m_soundId == 0 || module == nullptr) {
    return result;
  }
  HRSRC resource = reb::compat::FindResourceA(module, MAKEINTRESOURCEA(static_cast<WORD>(request->m_soundId)), g_waveResourceType);
  if (!resource) {
    return result;
  }
  HGLOBAL loaded = reb::compat::LoadResource(module, resource);
  if (!loaded) {
    return result;
  }
  void* image = reb::compat::LockResource(loaded);
  if (!image) {
    return result;
  }
  if (request->m_kind == kSoundKindWave) {
    result = kSoundOk;
    *source = NewOrNull<SoundSource>(static_cast<const void*>(image), static_cast<int>(kBufferStatic), request);
  } else {
    result = kSoundUnsupportedKind;
  }
  reb::compat::FreeResource(loaded);
  return result;
}

int SoundManager::CreateFileSource(SoundRequest* request, const char* fileName, long seekPosition, SoundSource** source)
{
  if (!fileName) {
    return kSoundFailed;
  }
  if (request->m_kind != kSoundKindWave) {
    return kSoundUnsupportedKind;
  }
  *source = NewOrNull<SoundSource>(fileName, seekPosition, static_cast<int>(kBufferStreamed), request);
  return kSoundOk;
}

// ---- event thread -------------------------------------------------------------------------------------------

void SoundManager::EventThreadProc()
{
  reb::platform::RaiseCurrentThreadPriority();
  if (!Audio().soundThreadReady) {
    return;
  }
  for (;;) {
    if (Audio().soundEvents.Wait() == kStopEvent) {
      return;
    }
    if (Audio().soundDispatchBusy.exchange(1) == 0) {
      if (Audio().soundEventCallback && !Audio().soundShuttingDown) {
        while (true) {
          Audio().soundQueueLock.lock();
          std::unique_ptr<SoundEvent> event;
          if (!Audio().soundEventQueue.empty()) {
            event = std::move(Audio().soundEventQueue.front());
            Audio().soundEventQueue.pop_front();
          }
          Audio().soundQueueLock.unlock();
          if (!event) {
            break;
          }
          uint32_t message;
          uint32_t first;
          uint32_t second;
          event->Get(&message, &first, &second);
          Audio().soundEventCallback(message, first, second);
        }
      }
      Audio().soundDispatchBusy.exchange(0);
    }
  }
}

int SoundManager::QueueEvent(uint32_t message, uint32_t first, uint32_t second)
{
  int result = 0;
  if (Audio().soundShuttingDown) {
    return 0;
  }
  if (Audio().soundEventCallback && Audio().soundThreadReady) {
    std::unique_ptr<SoundEvent> event(NewOrNull<SoundEvent>(message, first, second));
    if (!event) {
      return 0;
    }
    Audio().soundQueueLock.lock();
    try {
      Audio().soundEventQueue.push_back(std::move(event));
      Audio().soundEvents.At(kWakeEvent)->Set();
      result = 1;
    } catch (...) {
      result = 0;
    }
    Audio().soundQueueLock.unlock();
  }
  return result;
}

int SoundManager::StartEventThread()
{
  if (Audio().soundThread.joinable() || Audio().soundThreadReady != 0) {
    return kSoundAlreadyInitialized;
  }
  Audio().soundThreadReady = 1;
  Audio().soundShuttingDown = 0;
  Audio().soundEvents.Reset();
  try {
    Audio().soundThread = std::thread(&EventThreadProc);
  } catch (const std::system_error&) {
    StopEventThread();
    return 1;
  }
  return 0;
}

void SoundManager::StopEventThread()
{
  Audio().soundShuttingDown = 1;
  if (Audio().soundThread.joinable()) {
    Audio().soundEvents.At(kStopEvent)->Set();
    Audio().soundThread.join();
  }
  Audio().soundThreadReady = 0;
}
const char* g_waveResourceType = nullptr;
