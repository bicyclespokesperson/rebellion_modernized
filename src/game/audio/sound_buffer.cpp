#include "core/reb_abi.h"
#include "audio/sound_buffer.h"

#include <cstring>

#include "audio/interlocked_import.h"
#include "audio/sound_manager.h"
#include "audio/stream_access.h"
#include "core/vptr_store.h"

using namespace stream_access;

// Thin wrappers the buffer code calls; each forwards to SoundManager.
int CreateDirectBuffer(DSBUFFERDESC1* description, IDirectSoundBuffer** buffer);                      // 0x613f70
int DuplicateDirectBuffer(IDirectSoundBuffer** duplicate, IDirectSoundBuffer** original);               // 0x613f90
void PostSoundEvent(uint32_t message, uint32_t first, uint32_t second);                                 // 0x613fb0
void DispatchSoundEvent(uint32_t message, uint32_t first, uint32_t second);                             // 0x613fd0

namespace {

constexpr HRESULT kFailure = static_cast<HRESULT>(0x80004005);
constexpr HRESULT kInvalidArg = static_cast<HRESULT>(0x80070057);
constexpr HRESULT kNoInterface = static_cast<HRESULT>(0x80004002);
constexpr HRESULT kBufferLost = static_cast<HRESULT>(0x88780096);
constexpr DWORD kJoinTimeoutMs = 1500;
constexpr DWORD kBusyJoinTimeoutMs = 4500;
constexpr DWORD kRefillStackBytes = 0x4000;
constexpr uint32_t kBufferFlags = 0x181e8;  // 3D-less software buffer with volume, pan, frequency and notifications
constexpr uint32_t kStaticFlag = 2;         // DSBCAPS_STATIC
constexpr DWORD kStatusPlayingOrLooping = 5;
constexpr DWORD kStatusLost = 2;
constexpr int kNotificationCount = 2;
constexpr int kEventCount = 3;
constexpr int kStopEvent = 2;
constexpr uint32_t kHalfBufferShift = 1;

}  // namespace

// ---- SoundCallbackTimer -------------------------------------------------------------------------------------

SoundCallbackTimer::SoundCallbackTimer(void (*callback)(void*), void* parameter)
    : CoolTimer(0, 1, 10), m_callback(callback), m_parameter(parameter) {}

SoundCallbackTimer::~SoundCallbackTimer() {
    Stop();
    m_callback = nullptr;
    m_parameter = nullptr;
}

void SoundCallbackTimer::OnTick() {
    if (m_callback) m_callback(m_parameter);
}

// ---- SoundIdTree / SoundBufferList --------------------------------------------------------------------------

void SoundIdTree::Clear() {
    AvlTree::Clear();
    m_nextKey = 1;
}

int SoundIdTree::IsLess(AvlNode* a, AvlNode* b) {
    if (a && b && static_cast<KeyedSoundNode*>(a)->m_key < static_cast<KeyedSoundNode*>(b)->m_key) return 1;
    return 0;
}

KeyedSoundNode* SoundIdTree::Find(uint32_t key) {
    AvlNode* node = m_root;
    while (node && key != static_cast<KeyedSoundNode*>(node)->m_key) {
        if (static_cast<KeyedSoundNode*>(node)->m_key < key)
            node = node->m_right;
        else
            node = node->m_left;
    }
    return static_cast<KeyedSoundNode*>(node);
}

uint32_t SoundIdTree::Add(KeyedSoundNode* node) {
    uint32_t key = 0;
    if (node) {
        key = node->m_key;
        if (key == 0) {
            node->m_key = m_nextKey;
            key = m_nextKey;
            m_nextKey = key + 1;
            Insert(node);
            return key;
        }
        if (m_nextKey <= key) m_nextKey = key + 1;
        Insert(node);
    }
    return key;
}

int SoundIdTree::DeleteNode(KeyedSoundNode* node) {
    int deleted = 0;
    AvlNode* current = First();
    while (current) {
        if (current == node) {
            Remove(node);
            delete node;
            current = nullptr;
            deleted = 1;
        } else {
            current = current->m_successor;
        }
    }
    return deleted;
}

SoundBufferList::SoundBufferList() { m_nextKey = 2; }

SoundBufferList::~SoundBufferList() { SoundBufferList::Clear(); }

void SoundBufferList::Clear() {
    AvlTree::Clear();
    m_nextKey = 2;
}

int SoundBufferList::RemoveNode(KeyedSoundNode* node) {
    int deleted = 0;
    AvlNode* current = First();
    while (current) {
        if (current == node) {
            Remove(node);
            delete node;
            current = nullptr;
            deleted = 1;
        } else {
            current = current->m_successor;
        }
    }
    return deleted;
}

// ---- SoundBuffer --------------------------------------------------------------------------------------------

SoundBuffer::SoundBuffer(uint32_t sourceKey, WAVEFORMATEX* format, uint32_t bufferBytes, int mode, uint32_t dataLength)
    : m_timer(nullptr, nullptr) {
    m_description.dwSize = 0;
    m_description.dwFlags = 0;
    m_description.dwBufferBytes = 0;
    m_description.dwReserved = 0;
    m_description.lpwfxFormat = nullptr;
    m_key = 0;
    m_sourceKey = sourceKey;
    m_mode = mode;
    m_remaining = dataLength;
    m_totalLength = dataLength;
    m_writePosition = 0;
    m_lockFlags = 0;
    m_loopStart = 0;
    m_buffer = nullptr;
    m_notify = nullptr;
    m_thread = nullptr;
    m_failed = 0;
    m_stream = nullptr;
    m_refillBusy = 0;
    m_loop = 0;
    m_wrapped = 0;
    m_description.dwSize = sizeof(DSBUFFERDESC1);
    m_description.lpwfxFormat = format;
    m_description.dwBufferBytes = bufferBytes;
    m_description.dwFlags = kBufferFlags;
    m_events[0] = nullptr;
    m_events[1] = nullptr;
    m_events[2] = nullptr;
    if (m_mode == kBufferStatic) {
        m_lockFlags = 1;
        m_description.dwFlags |= kStaticFlag;
        // BUG(original): m_size has not been set yet, so this copies uninitialised memory.
        m_remaining = *reinterpret_cast<volatile uint32_t*>(&m_size);
    }
    m_volumeSet = 0;
    m_volume = 0;
    m_buffer = nullptr;
    CreateDirectBuffer(&m_description, &m_buffer);
    if (m_buffer == nullptr) {
        m_size = 0;
        m_failed = 1;
    } else {
        m_size = bufferBytes;
        if (m_mode == kBufferStreamed) {
            if (QueryNotify() == 0) {
                if (StartRefillThread() != 0) m_failed = 1;
            } else {
                m_failed = 1;
            }
        }
    }
}

SoundBuffer::SoundBuffer(const SoundBuffer& original) : m_timer(nullptr, nullptr) {
    // BUG(original): m_totalLength is never initialised for a duplicate; a looping duplicate restarts with whatever the
    // heap block held.
    m_key = 0;
    m_writePosition = 0;
    m_loopStart = 0;
    m_notify = nullptr;
    m_thread = nullptr;
    m_failed = 0;
    m_stream = nullptr;
    m_refillBusy = 0;
    m_loop = 0;
    m_remaining = original.m_remaining;
    m_lockFlags = original.m_lockFlags;
    m_mode = original.m_mode;
    m_size = original.m_size;
    m_description = original.m_description;
    m_wrapped = 0;
    m_sourceKey = original.m_sourceKey;
    m_volumeSet = original.m_volumeSet;
    m_volume = original.m_volume;
    m_events[0] = nullptr;
    m_events[1] = nullptr;
    m_events[2] = nullptr;
    DuplicateDirectBuffer(&m_buffer, const_cast<IDirectSoundBuffer**>(&original.m_buffer));
    if (m_buffer == nullptr) {
        m_size = 0;
        m_failed = 1;
    } else if (m_mode == kBufferStreamed) {
        if (QueryNotify() == 0) {
            if (StartRefillThread() != 0) m_failed = 1;
        } else {
            m_failed = 1;
        }
    }
}

SoundBuffer::~SoundBuffer() {
    if (m_buffer && m_mode == kBufferStreamed) StopRefillThread();
    if (m_buffer) {
        StopIfPlaying();
        m_buffer->Release();
        m_buffer = nullptr;
        m_size = 0;
    }
    if (m_stream && IsOpen(m_stream)) {
        fstreambase_close(m_stream);
        m_stream = nullptr;
    }
}

uint32_t SoundBuffer::BytesToFill() {
    uint32_t bytes = 0;
    if (m_buffer) {
        DWORD playCursor = 0;
        DWORD writeCursor = 0;
        bytes = m_size;
        if (m_mode != kBufferStatic && m_buffer->GetCurrentPosition(&playCursor, &writeCursor) == DS_OK) {
            if (m_writePosition < playCursor) return playCursor - m_writePosition;
            return m_size + playCursor - m_writePosition;
        }
    }
    return bytes;
}

void SoundBuffer::SetStream(istream* stream, long loopStart) {
    m_stream = stream;
    m_loopStart = loopStart;
}

void SoundBuffer::Refill(istream* in, long loopStart) {
    if (!m_buffer || !in || !IsOpen(in) || AtEof(in)) return;
    const uint32_t bytesToFill = BytesToFill();
    // Left uninitialised as in the original: Lock fills them in on success.
    void* first;
    void* second;
    DWORD firstLength;
    DWORD secondLength;
    HRESULT result =
        m_buffer->Lock(m_writePosition, bytesToFill, &first, &firstLength, &second, &secondLength, m_lockFlags);
    if (result == kBufferLost) {
        m_buffer->Restore();
        result = m_buffer->Lock(m_writePosition, bytesToFill, &first, &firstLength, &second, &secondLength, m_lockFlags);
    }
    if (result != DS_OK) return;

    uint32_t count = m_remaining < firstLength ? m_remaining : firstLength;
    Read(in, first, count);
    uint32_t read = LastReadCount(in);
    m_remaining -= read;
    if (m_remaining == 0 || read >= bytesToFill || second == nullptr) {
        if (m_remaining == 0 && m_mode == kBufferStreamed) {
            istream_seekg_off(in, loopStart, kSeekBegin);
            m_remaining = m_totalLength;
            if (read < firstLength) {
                Read(in, static_cast<uint8_t*>(first) + read, firstLength - read);
                m_remaining -= LastReadCount(in);
            }
            if (m_loop == 0) m_wrapped = 1;
        }
    } else {
        count = m_remaining < secondLength ? m_remaining : secondLength;
        Read(in, second, count);
        read = LastReadCount(in);
        m_remaining -= read;
        if (read < secondLength && m_remaining == 0) {
            istream_seekg_off(in, loopStart, kSeekBegin);
            Read(in, static_cast<uint8_t*>(second) + read, secondLength - read);
            m_remaining = m_totalLength - LastReadCount(in);
        }
    }
    if (m_buffer->Unlock(first, firstLength, second, secondLength) == DS_OK)
        m_writePosition = (firstLength + secondLength + m_writePosition) % m_size;
}

void SoundBuffer::FillFromMemory(const void* data) {
    if (!data || !m_buffer) return;
    void* first = nullptr;
    void* second = nullptr;
    DWORD firstLength = 0;
    DWORD secondLength = 0;
    HRESULT result = m_buffer->Lock(0, m_totalLength, &first, &firstLength, &second, &secondLength, 0);
    if (result == kBufferLost) {
        m_buffer->Restore();
        result = m_buffer->Lock(0, m_totalLength, &first, &firstLength, &second, &secondLength, 0);
    }
    if (result != DS_OK) return;
    memcpy(first, data, firstLength);
    m_buffer->Unlock(first, firstLength, second, secondLength);
}

HRESULT SoundBuffer::Play(int loop) {
    HRESULT result = kFailure;
    if (m_failed != 0) return result;
    if (m_buffer) {
        StopIfPlaying();
        m_loop = loop;
        DWORD flags = DSBPLAY_LOOPING;
        if (m_mode != kBufferStreamed && loop == 0) flags = 0;
        const LONG volume = m_volumeSet ? m_volume : static_cast<LONG>(GetDefaultVolume());
        m_buffer->SetVolume(volume);
        result = m_buffer->Play(0, 0, flags);
    }
    return result;
}

HRESULT SoundBuffer::Stop() {
    HRESULT result = kFailure;
    if (m_buffer) {
        if (StopIfPlaying() == 0) result = m_buffer->SetCurrentPosition(0);
    }
    return result;
}

HRESULT SoundBuffer::StopIfPlaying() {
    HRESULT result = kFailure;
    if (m_buffer) {
        DWORD status = 0;
        result = m_buffer->GetStatus(&status);
        if (result == DS_OK) {
            if (status & kStatusLost) result = m_buffer->Restore();
            if (result == DS_OK && (status & kStatusPlayingOrLooping)) result = m_buffer->Stop();
        }
    }
    return result;
}

HRESULT SoundBuffer::SetVolume(LONG volume) {
    if (!m_buffer) return kFailure;
    m_volumeSet = 1;
    m_volume = volume;
    return m_buffer->SetVolume(volume);
}

HRESULT SoundBuffer::GetStatus(DWORD* status) {
    if (!m_buffer) return kFailure;
    *status = 0;
    return m_buffer->GetStatus(status);
}

int SoundBuffer::IsPlaying() {
    // BUG(original): without a DirectSound buffer GetStatus leaves the status untouched, and the uninitialised
    // local happens to hold `this`.
    DWORD status = static_cast<DWORD>(reinterpret_cast<uintptr_t>(this));
    GetStatus(&status);
    return (status & kStatusPlayingOrLooping) != 0;
}

int SoundBuffer::QueryNotify() {
    if (!m_buffer) return kFailure;
    HRESULT result = m_buffer->QueryInterface(IID_IDirectSoundNotify, reinterpret_cast<void**>(&m_notify));
    if (result == kInvalidArg || result == kFailure || result == kNoInterface) result = -1;
    return result;
}

HRESULT SoundBuffer::StartRefillThread() {
    HRESULT result = 0;
    if (!m_buffer || !m_notify) return kFailure;
    for (int i = 0; i < kEventCount; ++i) {
        m_events[i] = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        if (!m_events[i]) result = kFailure;
    }
    if (result == 0) {
        DSBPOSITIONNOTIFY notifications[kNotificationCount];
        notifications[0].dwOffset = m_size >> kHalfBufferShift;
        notifications[0].hEventNotify = m_events[0];
        notifications[1].dwOffset = m_size - 1;
        notifications[1].hEventNotify = m_events[1];
        DWORD threadId;
        m_thread = CreateThread(nullptr, kRefillStackBytes, reinterpret_cast<LPTHREAD_START_ROUTINE>(&RefillThread),
                                this, 0, &threadId);
        if (!m_thread) result = kFailure;
        if (result == 0) {
            result = m_notify->SetNotificationPositions(kNotificationCount, notifications);
            SetThreadPriority(m_thread, THREAD_PRIORITY_ABOVE_NORMAL);
            if (result == 0) return 0;
        }
    }
    StopRefillThread();
    return result;
}

void SoundBuffer::StopRefillThread() {
    if (!m_buffer) return;
    if (m_thread) {
        if (m_events[kStopEvent]) SetEvent(m_events[kStopEvent]);
        if (WaitForSingleObject(m_thread, kJoinTimeoutMs) == WAIT_TIMEOUT && m_refillBusy != 0)
            WaitForSingleObject(m_thread, kBusyJoinTimeoutMs);
        CloseHandle(m_thread);
        m_thread = nullptr;
    }
    for (int i = 0; i < kEventCount; ++i) {
        if (m_events[i]) {
            CloseHandle(m_events[i]);
            m_events[i] = nullptr;
        }
    }
    if (m_notify) {
        if (m_notify->Release() == 0) m_notify = nullptr;
    }
}

void REB_CDECL SoundBuffer::RefillThread(SoundBuffer* buffer) {
    if (!buffer) return;
    if (buffer->m_failed != 0) {
        PostSoundEvent(kSoundBufferFinishedMessage, buffer->m_sourceKey, buffer->m_key);
        if (buffer->m_mode == kBufferStatic) DispatchSoundEvent(kSoundBufferFinishedMessage, buffer->m_sourceKey, buffer->m_key);
        return;
    }
    for (;;) {
        const DWORD signalled = WaitForMultipleObjects(kEventCount, buffer->m_events, FALSE, INFINITE);
        if (signalled == WAIT_FAILED) {
            PostSoundEvent(kSoundBufferFinishedMessage, buffer->m_sourceKey, buffer->m_key);
            ExitThread(0x56);
        }
        if (signalled == WAIT_OBJECT_0 + kStopEvent) ExitThread(0);
        if (InterlockedExchange(&buffer->m_refillBusy, 1) != 0) continue;
        REB_TRY {
            if (signalled <= WAIT_OBJECT_0 + 1) {
                if (buffer->m_mode == kBufferStreamed && buffer->m_wrapped != 0) {
                    buffer->m_wrapped = 0;
                    buffer->StopIfPlaying();
                    PostSoundEvent(kSoundBufferFinishedMessage, buffer->m_sourceKey, buffer->m_key);
                } else {
                    buffer->Refill(buffer->m_stream, buffer->m_loopStart);
                }
            }
        } REB_FINALLY {
            if (REB_ABNORMAL_TERMINATION()) {
                InterlockedExchange(&buffer->m_refillBusy, 0);
                ExitThread(99);
            }
        }
        InterlockedExchange(&buffer->m_refillBusy, 0);
    }
}

// ---- wrappers -----------------------------------------------------------------------------------------------

int CreateDirectBuffer(DSBUFFERDESC1* description, IDirectSoundBuffer** buffer) {
    return SoundManager::CreateBuffer(description, buffer);
}

int DuplicateDirectBuffer(IDirectSoundBuffer** duplicate, IDirectSoundBuffer** original) {
    return SoundManager::DuplicateBuffer(duplicate, original);
}

void PostSoundEvent(uint32_t message, uint32_t first, uint32_t second) { SoundManager::PostBufferEvent(message, first, second); }

void DispatchSoundEvent(uint32_t message, uint32_t first, uint32_t second) {
    SoundManager::HandleBufferEvent(message, first, second);
}

uint32_t GetDefaultVolume() { return static_cast<uint32_t>(g_soundVolume); }
