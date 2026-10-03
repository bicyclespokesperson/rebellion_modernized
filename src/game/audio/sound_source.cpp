#include "audio/sound_source.h"

#include "audio/sound_manager.h"
#include "audio/stream_access.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"

using namespace stream_access;

extern int g_filebufOpenprot;

namespace {

constexpr HRESULT kFailure = static_cast<HRESULT>(0x80004005);
constexpr int kBinaryInputMode = 0xa1;
constexpr size_t kIfstreamSize = 0x5c;
constexpr uint32_t kFirstBufferKey = 1;
constexpr uint32_t kBufferSeconds = 2;
constexpr uint32_t kMillisecondsPerSecond = 1000;
constexpr DWORD kStatusPlaying = 1;
constexpr int kStreamHeaderBytes = 8;  // "data" tag and chunk size

}  // namespace

// ---- SoundSourceList / SoundCatalogList ---------------------------------------------------------------------

SoundSourceList::SoundSourceList() {}

SoundSourceList::~SoundSourceList() { SoundIdTree::Clear(); }

void SoundSourceList::DeleteByKey(uint32_t key) {
    KeyedSoundNode* node = Find(key);
    if (node) {
        Remove(node);
        delete node;
    }
}

SoundCatalogList::SoundCatalogList() {}

SoundCatalogList::~SoundCatalogList() { SoundIdTree::Clear(); }

// ---- SoundEvent ---------------------------------------------------------------------------------------------

SoundEvent::SoundEvent(uint32_t message, uint32_t first, uint32_t second) { Set(message, first, second); }

SoundEvent::~SoundEvent() {
    m_message = 0;
    m_first = 0;
    m_second = 0;
}

void SoundEvent::Set(uint32_t message, uint32_t first, uint32_t second) {
    m_message = message;
    m_first = first;
    m_second = second;
}

void SoundEvent::Get(uint32_t* message, uint32_t* first, uint32_t* second) {
    *message = m_message;
    *first = m_first;
    *second = m_second;
}

void SoundEvent::Read(istream* in) {
    ReadValue32(in, &m_message);
    uint32_t first;
    ReadValue32(in, &first);
    ReadValue32(in, &m_second);
    m_first = first;
}

void SoundEvent::Write(ostream* out) {
    uint32_t first = m_first;
    WriteValue32(out, &m_message);
    WriteValue32(out, &first);
    WriteValue32(out, &m_second);
}

// ---- SoundSource --------------------------------------------------------------------------------------------

SoundSource::SoundSource(const void* waveImage, int mode, SoundRequest* request) {
    LoadFromMemory(waveImage, mode, request);
}

SoundSource::SoundSource(const char* fileName, long seekPosition, int mode, SoundRequest* request) {
    LoadFromFile(fileName, seekPosition, mode, request);
}

SoundSource::~SoundSource() {
    m_buffers.Clear();
    if (m_stream) {
        if (IsOpen(m_stream)) fstreambase_close(m_stream);
        if (m_stream) DeleteStream(m_stream);
    }
    SoundSource::Reset();
}

void SoundSource::Reset() {
    ResetLinks();
    m_key = 0;
    m_users = 0;
    m_streamPosition = 0;
    m_stream = nullptr;
    m_keepLoaded = 0;
}

void SoundSource::LoadFromFile(const char* fileName, long seekPosition, int mode, SoundRequest* request) {
    if (!fileName) return;
    istream* stream = nullptr;
    if (void* memory = ::operator new(kIfstreamSize))
        stream = reinterpret_cast<istream*>(ifstream_ctor_open(memory, fileName, kBinaryInputMode, g_filebufOpenprot, 1));
    if (!stream) return;
    AttachFile(stream, seekPosition, mode, request);
    if (m_stream == nullptr) DeleteStream(stream);
}

void SoundSource::AttachFile(istream* stream, long seekPosition, int mode, SoundRequest* request) {
    Reset();
    m_key = request->SourceKey();
    if (IsOpen(stream)) {
        if (seekPosition) istream_seekg_pos(stream, seekPosition);
        m_streamPosition = istream_tellg(stream);
        m_stream = stream;
        m_wave.ParseStream(stream);
        m_wave.FindDataChunk(m_stream, m_streamPosition);
        request->m_handle = static_cast<int>(CreateFileBuffer(m_stream, &mode));
    }
}

void SoundSource::LoadFromMemory(const void* waveImage, int mode, SoundRequest* request) {
    Reset();
    m_key = request->SourceKey();
    if (waveImage) {
        m_wave.ParseMemory(waveImage);
        const uint8_t* dataChunk = static_cast<const uint8_t*>(waveImage);
        m_wave.FindDataChunkInMemory(&dataChunk, 0);
        request->m_handle = static_cast<int>(CreateMemoryBuffer(dataChunk, &mode));
    }
}

uint32_t SoundSource::CreateFileBuffer(istream* stream, int* mode) {
    SoundBuffer* buffer = AcquireBuffer(mode);
    if (!buffer) return 0;
    istream_seekg_off(stream, kStreamHeaderBytes, kSeekCurrent);
    buffer->Refill(stream, static_cast<long>(m_wave.m_dataOffset) + kStreamHeaderBytes);
    if (*mode == kBufferStatic) {
        fstreambase_close(stream);
        m_stream = nullptr;
        m_streamPosition = 0;
        return buffer->m_key;
    }
    buffer->SetStream(m_stream, m_streamPosition + static_cast<long>(m_wave.m_dataOffset) + kStreamHeaderBytes);
    return buffer->m_key;
}

uint32_t SoundSource::CreateMemoryBuffer(const void* data, int* mode) {
    SoundBuffer* buffer = AcquireBuffer(mode);
    if (!buffer) return 0;
    buffer->FillFromMemory(static_cast<const uint8_t*>(data) + kStreamHeaderBytes);
    return buffer->m_key;
}

int SoundSource::Play(int* handle, int loop) {
    HRESULT result = kFailure;
    if (SoundBuffer* buffer = static_cast<SoundBuffer*>(m_buffers.Find(*handle))) {
        result = buffer->Play(loop);
    } else if (*handle == 0) {
        if (SoundBuffer* first = static_cast<SoundBuffer*>(m_buffers.Find(kFirstBufferKey))) {
            if (first->IsPlaying() == 1) {
                int mode = first->m_mode;
                if (SoundBuffer* duplicate = AcquireBuffer(&mode)) {
                    *handle = static_cast<int>(duplicate->m_key);
                    result = duplicate->Play(loop);
                }
            } else {
                result = first->Play(loop);
                *handle = static_cast<int>(first->m_key);
            }
        }
    }
    if (result == 0) ++m_users;
    return result;
}

int SoundSource::Stop(int* handle, int release) {
    HRESULT result = kFailure;
    if (SoundBuffer* buffer = static_cast<SoundBuffer*>(m_buffers.Find(*handle))) {
        result = buffer->Stop();
        if (release == 1 && *handle != static_cast<int>(kFirstBufferKey)) {
            m_buffers.RemoveNode(buffer);
            *handle = 0;
        }
    }
    if (result == 0) --m_users;
    return result;
}

int SoundSource::SetVolume(int handle, LONG volume) {
    HRESULT result = kFailure;
    if (SoundBuffer* buffer = static_cast<SoundBuffer*>(m_buffers.Find(handle))) result = buffer->SetVolume(volume);
    return result;
}

SoundBuffer* SoundSource::AcquireBuffer(int* mode) {
    SoundBuffer* first = static_cast<SoundBuffer*>(m_buffers.Find(kFirstBufferKey));
    if (!first) {
        uint32_t bufferBytes = m_wave.m_format.nAvgBytesPerSec * (kBufferSeconds * kMillisecondsPerSecond) / kMillisecondsPerSecond;
        if (*mode == kBufferStatic || m_wave.m_dataSize < bufferBytes) {
            bufferBytes = m_wave.m_dataSize;
            *mode = kBufferStatic;
        }
        SoundBuffer* buffer = NewOrNull<SoundBuffer>(m_key, &m_wave.m_format, bufferBytes, *mode, m_wave.m_dataSize);
        if (buffer) {
            buffer->m_key = kFirstBufferKey;
            m_buffers.Add(buffer);
        }
        return buffer;
    }
    if (SoundBuffer* duplicate = NewOrNull<SoundBuffer>(*first)) {
        m_buffers.Add(duplicate);
        return duplicate;
    }
    return first;
}

int SoundSource::QueryPlaying(SoundRequest* request) {
    HRESULT result = kFailure;
    if (SoundBuffer* buffer = static_cast<SoundBuffer*>(m_buffers.Find(static_cast<uint32_t>(request->m_handle)))) {
        DWORD status;
        result = buffer->GetStatus(&status);
        if (result == 0) {
            request->m_loop = 0;
            request->m_playing = status & kStatusPlaying;
        }
    }
    return result;
}

int SoundSource::IsAnyPlaying() {
    int anyPlaying = 0;
    for (AvlNode* node = m_buffers.First(); node; node = node->m_successor) {
        DWORD status;
        if (static_cast<SoundBuffer*>(node)->GetStatus(&status) == 0 && (anyPlaying || (status & kStatusPlaying)))
            anyPlaying = 1;
    }
    return anyPlaying;
}

int SoundSource::ReleaseUser(int) {
    --m_users;
    return 0;
}
