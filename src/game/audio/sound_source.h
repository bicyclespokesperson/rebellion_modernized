#pragma once
#include <cstdint>

#include "audio/sound_buffer.h"
#include "audio/wave_info.h"
#include "core/containers.h"
#include "core/stream_io.h"

class SoundRequest;

// A loaded sound (a WAVE resource, or a file the game streams) with the DirectSound buffers playing it. Sources live
// in g_soundSources keyed by (module index << 16) + resource id.
class SoundSource : public KeyedSoundNode {
public:
    // From an in-memory WAVE image (a locked resource) that is loaded whole into a static buffer.
    SoundSource(const void* waveImage, int mode, SoundRequest* request);
    // From a file, `seekPosition` bytes in (0 = start), streamed when long enough.
    SoundSource(const char* fileName, long seekPosition, int mode, SoundRequest* request);
    virtual ~SoundSource();
    virtual void Reset();

    void LoadFromFile(const char* fileName, long seekPosition, int mode, SoundRequest* request);
    void AttachFile(istream* stream, long seekPosition, int mode, SoundRequest* request);
    void LoadFromMemory(const void* waveImage, int mode, SoundRequest* request);
    // Both return the key of the first buffer (0 if none could be made) and store it in request->m_handle.
    uint32_t CreateFileBuffer(istream* stream, int* mode);
    uint32_t CreateMemoryBuffer(const void* data, int* mode);

    // Handle = buffer key; *handle 0 means "use or create the first buffer". Returns an HRESULT-like code.
    int Play(int* handle, int loop);
    int Stop(int* handle, int release);
    int SetVolume(int handle, LONG volume);
    // The buffer to play on: the original while it is idle, else a duplicate. *mode may be changed to static.
    SoundBuffer* AcquireBuffer(int* mode);
    int QueryPlaying(SoundRequest* request);
    int IsAnyPlaying();
    int ReleaseUser(int bufferKey);

    int m_users;              // +0x1c buffers currently playing or paused
    WaveInfo m_wave;          // +0x20
    SoundBufferList m_buffers;  // +0x40 keyed by SoundBuffer::m_key
    istream* m_stream;        // +0x4c the file a streamed source reads, if still open
    long m_streamPosition;    // +0x50 where the WAVE image starts in that file
    int m_keepLoaded;         // +0x54 survives SoundManager::ReleaseAll(0)
};

// Tree of all loaded SoundSources.
class SoundSourceList : public SoundIdTree {
public:
    SoundSourceList();
    virtual ~SoundSourceList();

    void DeleteByKey(uint32_t key);  // 0x6120d0
};

// A second tree of the same kind, constructed at start-up but never filled (SoundManager::SweepSources empties it).
class SoundCatalogList : public SoundIdTree {
public:
    SoundCatalogList();
    virtual ~SoundCatalogList();
};

// A queued notification for the sound-event thread (see SoundManager::QueueEvent).
class SoundEvent : public LinkNode {
public:
    SoundEvent(uint32_t message, uint32_t first, uint32_t second);
    virtual ~SoundEvent();
    virtual void Read(istream* in) override;
    virtual void Write(ostream* out) override;

    void Set(uint32_t message, uint32_t first, uint32_t second);
    void Get(uint32_t* message, uint32_t* first, uint32_t* second);

    uint32_t m_message;  // +0x0c
    uint32_t m_first;    // +0x10
    uint32_t m_second;   // +0x14
};
