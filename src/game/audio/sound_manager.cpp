#include "core/reb_abi.h"
#include "audio/sound_manager.h"
#include "core/vptr_store.h"

#include <cstdlib>
#include <new>

#include "audio/interlocked_import.h"
#include "core/checked_new.h"
#include "ui/cool_window.h"

// Original globals that only this file touches.
extern HANDLE g_soundEvents[2];            // 0x6be630 wake-up event (queue not empty), stop event
extern HANDLE g_soundThread;               // 0x6be688
extern CRITICAL_SECTION g_soundQueueLock;  // 0x6be690
extern int g_soundThreadReady;             // 0x6be6a8 queue lock initialised
extern LONG g_soundDispatchBusy;           // 0x6be6ac the event thread is running a callback
extern int g_soundShuttingDown;            // 0x6be6b0
extern LONG g_sweepBusy;                   // 0x6be660 SweepSources is running
extern const char *g_waveResourceType;    // 0x6ac954 "WAVE"

// Functions of other modules.
namespace {

constexpr uint32_t kDefaultPrimaryFormat = 0x2b110110;   // 11025 Hz, 1 channel, 16 bit
constexpr uint32_t kListenerPrimaryFormat = 0x2b110208;  // 11025 Hz, 2 channels, 8 bit
constexpr int kEventCount = 2;
constexpr int kWakeEvent = 0;
constexpr int kStopEvent = 1;
constexpr DWORD kStopJoinMs = 1500;
constexpr DWORD kBusyJoinMs = 15000;
constexpr DWORD kEventStackBytes = 0x4000;
constexpr int kMinVolume = -10000;
constexpr uint16_t kBitsPerByte = 8;
constexpr uint8_t kPlayWithDefaultVolume = 0;

// Fills g_primaryFormat from a packed format and returns it.
void StorePrimaryFormat(uint32_t packedFormat) {
    const uint16_t bitsPerSample = packedFormat & 0xff;
    const uint16_t channels = (packedFormat >> 8) & 0xff;
    g_primaryFormat = {};
    g_primaryFormat.wFormatTag = WAVE_FORMAT_PCM;
    g_primaryFormat.nChannels = channels;
    g_primaryFormat.wBitsPerSample = bitsPerSample;
    g_primaryFormat.nBlockAlign = static_cast<uint16_t>(static_cast<int>(channels * bitsPerSample) / kBitsPerByte);
    g_primaryFormat.nSamplesPerSec = packedFormat >> 16;
    g_primaryFormat.nAvgBytesPerSec = g_primaryFormat.nSamplesPerSec * g_primaryFormat.nBlockAlign;
}

}  // namespace

// ---- SoundRequest -------------------------------------------------------------------------------------------

SoundRequest::SoundRequest(unsigned kind, int moduleIndex, int soundId, int loop, int keepLoaded, int handle) {
    m_soundId = soundId;
    m_moduleIndex = moduleIndex;
    m_handle = handle;
    m_kind = kind;
    m_loop = loop;
    m_keepLoaded = keepLoaded;
    m_playing = 0;
    m_bit6 = 0;
}

SoundRequest::SoundRequest(const SoundRequest& other) {
    m_soundId = other.m_soundId;
    m_moduleIndex = other.m_moduleIndex;
    m_handle = other.m_handle;
    m_kind = other.m_kind;
    m_loop = other.m_loop;
    m_keepLoaded = other.m_keepLoaded;
    m_bit6 = other.m_bit6;
    m_playing = other.m_playing;
}

SoundRequest::~SoundRequest() { REB_KEEP_VPTR_STORE(); }

// ---- statics ------------------------------------------------------------------------------------------------


// ---- entry points -------------------------------------------------------------------------------------------

int SoundManager::Initialize(HWND window, unsigned flags, int smackParameter) {
    return InitializeImpl(window, flags, smackParameter);
}

void SoundManager::Shutdown(int force) { ShutdownImpl(force); }

IDirectSound* SoundManager::GetDirectSound() { return g_directSound; }

void SoundManager::SetEventCallback(SoundEventCallback callback) { g_soundEventCallback = callback; }

int SoundManager::Play(SoundRequest* request) { return PlayImpl(request); }

int SoundManager::PlayFromFile(SoundRequest* request, const char* fileName, long seekPosition) {
    return PlayFromFileImpl(request, fileName, seekPosition);
}

int SoundManager::Release(SoundRequest* request) { return ReleaseImpl(request); }

void SoundManager::ReleaseAll(int everything) { ReleaseAllImpl(everything); }

int SoundManager::Query(SoundRequest* request) { return QueryImpl(request); }

int SoundManager::Stop(SoundRequest* request, int sweep) { return StopImpl(request, sweep); }

int SoundManager::SetVolume(SoundRequest* request, int volume) { return SetVolumeImpl(request, volume); }

void SoundManager::SetDefaultVolume(int volume) { g_soundVolume = volume; }

void SoundManager::SetMasterVolume(LONG volume) { SetMasterVolumeImpl(volume); }

// ---- device -------------------------------------------------------------------------------------------------

int SoundManager::InitializeImpl(HWND window, unsigned flags, int smackParameter) {
    int result;
    g_soundWindow = window;
    if (flags & kSoundInitCdAudio) g_cdAudio.Open(window);
    if (g_directSound == nullptr) {
        if (DirectSoundCreate(nullptr, &g_directSound, nullptr) == DS_OK) {
            if (g_directSound->SetCooperativeLevel(g_soundWindow, DSSCL_EXCLUSIVE) == DS_OK) {
                if (flags & kSoundInitListenerFormat)
                    result = SetPrimaryBufferFormat3D(kListenerPrimaryFormat);
                else
                    result = SetPrimaryBufferFormat(kDefaultPrimaryFormat);
            } else {
                result = kSoundCooperativeLevelFailed;
            }
        } else {
            result = kSoundNoDevice;
        }
    } else {
        result = kSoundAlreadyInitialized;
    }
    if (flags & kSoundInitShareWithSmacker) g_smackSound.Share(window, smackParameter, g_directSound);
    StartEventThread();
    ++g_soundUseCount;
    return result;
}

void SoundManager::ShutdownImpl(int force) {
    --g_soundUseCount;
    bool lastUser;
    if (force == 1 || g_soundUseCount < 0) {
        g_soundUseCount = 0;
        lastUser = true;
    } else {
        lastUser = g_soundUseCount == 0;
    }
    if (lastUser) {
        StopEventThread();
        ReleaseAllImpl(1);
        ReleasePrimaryBuffer();
        if (g_directSound) {
            g_directSound->Release();
            g_directSound = nullptr;
        }
    }
}

int SoundManager::SetPrimaryBufferFormat3D(uint32_t packedFormat) {
    StorePrimaryFormat(packedFormat);
    DSBUFFERDESC1 description = {};
    description.dwSize = sizeof(description);
    description.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRL3D | DSBCAPS_GLOBALFOCUS;
    if (g_directSound->CreateSoundBuffer(reinterpret_cast<LPCDSBUFFERDESC>(&description), &g_primaryBuffer, nullptr) != DS_OK)
        return kSoundCreatePrimaryFailed;
    if (g_primaryBuffer->SetFormat(&g_primaryFormat) != DS_OK) {
        ReleasePrimaryBuffer();
        return kSoundSetFormatFailed;
    }
    if (g_primaryBuffer->QueryInterface(IID_IDirectSound3DListener, reinterpret_cast<void**>(&g_listener)) != DS_OK)
        return kSoundQueryListenerFailed;
    if (g_primaryBuffer->Play(0, 0, DSBPLAY_LOOPING) != DS_OK) {
        ReleasePrimaryBuffer();
        return kSoundCreatePrimaryFailed;
    }
    return kSoundOk;
}

void SoundManager::ReleasePrimaryBuffer() {
    if (g_primaryBuffer) {
        g_primaryBuffer->Stop();
        g_primaryBuffer->Release();
        g_primaryBuffer = nullptr;
    }
    if (g_listener) {
        g_listener->Release();
        g_listener = nullptr;
    }
}

int SoundManager::SetPrimaryBufferFormat(uint32_t packedFormat) {
    StorePrimaryFormat(packedFormat);
    if (!g_primaryBuffer) {
        DSBUFFERDESC1 description = {};
        description.dwSize = sizeof(description);
        description.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME;
        // BUG(original): g_directSound is null when DirectSoundCreate failed (no audio device), and SetMasterVolume and
        // GetMasterVolume reach this without checking it.
        if (g_directSound->CreateSoundBuffer(reinterpret_cast<LPCDSBUFFERDESC>(&description), &g_primaryBuffer, nullptr) != DS_OK)
            return kSoundCreatePrimaryFailed;
    }
    return g_primaryBuffer->SetFormat(&g_primaryFormat) != DS_OK ? kSoundSetFormatFailed : kSoundOk;
}

void SoundManager::SetMasterVolumeImpl(LONG volume) {
    if (!g_primaryBuffer) {
        SetPrimaryBufferFormat(kListenerPrimaryFormat);
        if (!g_primaryBuffer) return;
    }
    g_primaryBuffer->SetVolume(volume);
}

LONG SoundManager::GetMasterVolume() {
    LONG volume = 0;
    if (!g_primaryBuffer) {
        SetPrimaryBufferFormat(kListenerPrimaryFormat);
        if (!g_primaryBuffer) return volume;
    }
    g_primaryBuffer->GetVolume(&volume);
    return volume;
}

// ---- buffers and events -------------------------------------------------------------------------------------

int SoundManager::CreateBuffer(DSBUFFERDESC1* description, IDirectSoundBuffer** buffer) {
    if (!g_directSound) return kSoundNoDevice;
    int result = g_directSound->CreateSoundBuffer(reinterpret_cast<LPCDSBUFFERDESC>(description), buffer, nullptr);
    if (result != DS_OK) return result;
    if (g_soundVolume > 0) {
        if (g_soundVolume <= -kMinVolume) {
            // A positive default volume means an attenuation: it is negated once, in place.
            g_soundVolume = -g_soundVolume;
            (*buffer)->SetVolume(g_soundVolume);
            return DS_OK;
        }
        g_soundVolume = 0;
    }
    (*buffer)->SetVolume(g_soundVolume);
    return DS_OK;
}

int SoundManager::DuplicateBuffer(IDirectSoundBuffer** duplicate, IDirectSoundBuffer** original) {
    if (!g_directSound) return kSoundNoDevice;
    return g_directSound->DuplicateSoundBuffer(*original, duplicate);
}

int SoundManager::PostBufferEvent(uint32_t message, uint32_t first, uint32_t second) {
    QueueEvent(message, first, second);
    return 0;
}

void SoundManager::HandleBufferEvent(uint32_t message, uint32_t sourceKey, uint32_t bufferKey) {
    if (message != kSoundBufferFinishedMessage) return;
    if (KeyedSoundNode* node = g_soundSources.Find(sourceKey)) static_cast<SoundSource*>(node)->ReleaseUser(bufferKey);
}

void SoundManager::SweepSources(int) {
    if (InterlockedExchange(&g_sweepBusy, 1) != 0) return;
    AvlNode* node = g_soundCatalog.First();
    while (node) {
        AvlNode* next = node->m_successor;
        if (static_cast<SoundSource*>(node)->m_users == 1) g_soundCatalog.DeleteNode(static_cast<SoundSource*>(node));
        node = next;
    }
    node = g_soundSources.First();
    while (node) {
        SoundSource* source = static_cast<SoundSource*>(node);
        AvlNode* next = node->m_successor;
        if (source->m_keepLoaded == 0 && source->IsAnyPlaying() == 0) g_soundSources.DeleteNode(source);
        node = next;
    }
    InterlockedExchange(&g_sweepBusy, 0);
}

// ---- sounds -------------------------------------------------------------------------------------------------

int SoundManager::LoadResourceSource(SoundRequest* request) {
    if (g_soundSources.Find(request->SourceKey())) return kSoundAlreadyLoaded;
    SoundSource* source;
    int error = CreateResourceSource(request, FindResourceModule(request->m_moduleIndex), &source);
    if (error != 0 || source == nullptr) return kSoundFailed;
    g_soundSources.Add(source);
    if (request->m_keepLoaded) source->m_keepLoaded = 1;
    return kSoundOk;
}

int SoundManager::LoadFileSource(SoundRequest* request, const char* fileName, long seekPosition) {
    if (g_soundSources.Find(request->SourceKey())) return kSoundAlreadyLoaded;
    SoundSource* source;
    if (CreateFileSource(request, fileName, seekPosition, &source) != 0 || source == nullptr) return kSoundFailed;
    source->m_key = request->SourceKey();
    g_soundSources.Add(source);
    if (request->m_keepLoaded) source->m_keepLoaded = 1;
    return kSoundOk;
}

int SoundManager::StartSource(SoundRequest* request) {
    int result;
    if (KeyedSoundNode* node = g_soundSources.Find(request->SourceKey())) {
        result = static_cast<SoundSource*>(node)->Play(&request->m_handle, request->m_loop);
        if (result == 0) request->m_playing = 1;
    } else {
        result = kSoundNotLoaded;
    }
    SweepSources(0);
    return result;
}

int SoundManager::PlayImpl(SoundRequest* request) {
    int result = kSoundFailed;
    if (g_soundSources.Find(request->SourceKey()) || LoadResourceSource(request) == 0) result = StartSource(request);
    SweepSources(0);
    return result;
}

int SoundManager::PlayFromFileImpl(SoundRequest* request, const char* fileName, long seekPosition) {
    int result = kSoundFailed;
    if (g_soundSources.Find(request->SourceKey()) || LoadFileSource(request, fileName, seekPosition) == 0)
        result = StartSource(request);
    SweepSources(0);
    return result;
}

int SoundManager::ReleaseImpl(SoundRequest* request) {
    int result = kSoundNoSuchSound;
    if (g_soundSources.Find(request->SourceKey())) {
        result = kSoundOk;
        do {
            g_soundSources.DeleteByKey(request->SourceKey());
        } while (g_soundSources.Find(request->SourceKey()));
    }
    return result;
}

void SoundManager::ReleaseAllImpl(int everything) {
    if (everything == 1) {
        g_soundSources.SoundIdTree::Clear();
        return;
    }
    AvlNode* node = g_soundSources.First();
    while (node) {
        SoundSource* source = static_cast<SoundSource*>(node);
        AvlNode* next = node->m_successor;
        if (source->m_keepLoaded == 0) g_soundSources.DeleteNode(source);
        node = next;
    }
}

int SoundManager::QueryImpl(SoundRequest* request) {
    if (KeyedSoundNode* node = g_soundSources.Find(request->SourceKey())) return static_cast<SoundSource*>(node)->QueryPlaying(request);
    request->m_keepLoaded = 0;
    request->m_playing = 0;
    return kSoundNotLoaded;
}

int SoundManager::StopImpl(SoundRequest* request, int sweep) {
    int result = kSoundNotLoaded;
    if (KeyedSoundNode* node = g_soundSources.Find(request->SourceKey()))
        result = static_cast<SoundSource*>(node)->Stop(&request->m_handle, sweep);
    if (sweep != 0) SweepSources(0);
    return result;
}

int SoundManager::SetVolumeImpl(SoundRequest* request, int volume) {
    if (volume > 0 || volume < kMinVolume) return kSoundFailed;
    if (KeyedSoundNode* node = g_soundSources.Find(request->SourceKey()))
        return static_cast<SoundSource*>(node)->SetVolume(request->m_handle, volume);
    return kSoundNotLoaded;
}

int SoundManager::CreateResourceSource(SoundRequest* request, HMODULE module, SoundSource** source) {
    int result = kSoundFailed;
    if (request->m_soundId == 0 || module == nullptr) return result;
    HRSRC resource = FindResourceA(module, MAKEINTRESOURCEA(static_cast<WORD>(request->m_soundId)), g_waveResourceType);
    if (!resource) return result;
    HGLOBAL loaded = LoadResource(module, resource);
    if (!loaded) return result;
    void* image = LockResource(loaded);
    if (!image) return result;
    if (request->m_kind == kSoundKindWave) {
        result = kSoundOk;
        *source = NewOrNull<SoundSource>(static_cast<const void*>(image), static_cast<int>(kBufferStatic), request);
    } else {
        result = kSoundUnsupportedKind;
    }
    FreeResource(loaded);
    return result;
}

int SoundManager::CreateFileSource(SoundRequest* request, const char* fileName, long seekPosition, SoundSource** source) {
    if (!fileName) return kSoundFailed;
    if (request->m_kind != kSoundKindWave) return kSoundUnsupportedKind;
    *source = NewOrNull<SoundSource>(fileName, seekPosition, static_cast<int>(kBufferStreamed), request);
    return kSoundOk;
}

// ---- event thread -------------------------------------------------------------------------------------------

void REB_CDECL SoundManager::EventThreadProc() {
    if (!g_soundThreadReady) ExitThread(99);
    DWORD signalled = WaitForMultipleObjects(kEventCount, g_soundEvents, FALSE, INFINITE);
    while (signalled != WAIT_FAILED) {
        if (signalled == WAIT_OBJECT_0 + kStopEvent) ExitThread(0);
        if (InterlockedExchange(&g_soundDispatchBusy, 1) == 0) {
            if (g_soundEventCallback && !g_soundShuttingDown && g_soundEventQueue.m_count != 0) {
                do {
                    EnterCriticalSection(&g_soundQueueLock);
                    SoundEvent* event = static_cast<SoundEvent*>(g_soundEventQueue.Dequeue());
                    LeaveCriticalSection(&g_soundQueueLock);
                    if (!event) break;
                    uint32_t message;
                    uint32_t first;
                    uint32_t second;
                    event->Get(&message, &first, &second);
                    delete event;
                    g_soundEventCallback(message, first, second);
                } while (g_soundEventQueue.m_count != 0);
            }
            InterlockedExchange(&g_soundDispatchBusy, 0);
        }
        signalled = WaitForMultipleObjects(kEventCount, g_soundEvents, FALSE, INFINITE);
    }
    ExitThread(0x56);
}

int SoundManager::QueueEvent(uint32_t message, uint32_t first, uint32_t second) {
    int result = 0;
    if (g_soundShuttingDown) return 0;
    if (g_soundEventCallback && g_soundEvents[kWakeEvent] && g_soundThreadReady) {
        SoundEvent* event = NewOrNull<SoundEvent>(message, first, second);
        EnterCriticalSection(&g_soundQueueLock);
        if (g_soundEventQueue.Enqueue(event) == 0) {
            if (event) delete event;
        } else {
            result = SetEvent(g_soundEvents[kWakeEvent]);
        }
        LeaveCriticalSection(&g_soundQueueLock);
    }
    return result;
}

int SoundManager::StartEventThread() {
    int error = 0;
    if (g_soundThread != nullptr || g_soundThreadReady != 0) return kSoundAlreadyInitialized;
    InitializeCriticalSection(&g_soundQueueLock);
    g_soundThreadReady = 1;
    g_soundShuttingDown = 0;
    for (HANDLE& event : g_soundEvents) {
        event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        if (!event) error = 1;
    }
    if (error == 0) {
        DWORD threadId;
        g_soundThread = CreateThread(nullptr, kEventStackBytes, reinterpret_cast<LPTHREAD_START_ROUTINE>(&EventThreadProc),
                                     nullptr, 0, &threadId);
        if (!g_soundThread)
            error = 1;
        else
            SetThreadPriority(g_soundThread, THREAD_PRIORITY_ABOVE_NORMAL);
        if (error == 0) return 0;
    }
    StopEventThread();
    return error;
}

void SoundManager::StopEventThread() {
    g_soundShuttingDown = 1;
    if (g_soundThread) {
        if (g_soundEvents[kStopEvent]) SetEvent(g_soundEvents[kStopEvent]);
        if (WaitForSingleObject(g_soundThread, kStopJoinMs) == WAIT_TIMEOUT && g_soundDispatchBusy != 0)
            WaitForSingleObject(g_soundThread, kBusyJoinMs);
        CloseHandle(g_soundThread);
        g_soundThread = nullptr;
    }
    for (HANDLE& event : g_soundEvents) {
        if (event) {
            CloseHandle(event);
            event = nullptr;
        }
    }
    if (g_soundThreadReady) {
        DeleteCriticalSection(&g_soundQueueLock);
        g_soundThreadReady = 0;
    }
}
CRITICAL_SECTION g_soundQueueLock{};
HANDLE g_soundEvents[2]{};
HANDLE g_soundThread{};
LONG g_soundDispatchBusy{};
LONG g_sweepBusy{};
const char *g_waveResourceType = nullptr;
int g_soundShuttingDown{};
int g_soundThreadReady{};
