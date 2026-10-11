#pragma once
#include <cstdint>

#include "core/containers.h"
#include "platform/win32.h"
#include "ui/cool_timer.h"

// Timer object that calls m_callback(m_param) when it fires. Sound buffers embed one but construct it with period 0,
// so the multimedia timer is never started and OnTick() is never reached.
class SoundCallbackTimer : public CoolTimer {
public:
    SoundCallbackTimer(void (*callback)(void*), void* parameter);
    virtual ~SoundCallbackTimer();
    virtual void OnTick() override;

    void (*m_callback)(void*);  // +0x38
    void* m_parameter;          // +0x3c
};

// Node of the sound trees: the key at +0x18 is what the tree searches and orders by.
class KeyedSoundNode : public AvlNode {
public:
    KeyedSoundNode() : m_key(0) {}

    uint32_t m_key;  // +0x18
};

// AVL tree of KeyedSoundNodes that hands out keys, like IdAvlTree (the original compiled separate copies of its
// members for these trees, so they have their own addresses). Keys given to Add() as 0 are assigned m_nextKey.
class SoundIdTree : public AvlTree {
public:
    SoundIdTree() : m_nextKey(1) {}
    virtual ~SoundIdTree() {}
    virtual void Clear() override;
    virtual int IsLess(AvlNode* a, AvlNode* b) override;

    KeyedSoundNode* Find(uint32_t key);         // 0x612bd0
    uint32_t Add(KeyedSoundNode* node);         // 0x612b30
    int DeleteNode(KeyedSoundNode* node);       // 0x612100: unlinks and deletes `node` if it is in the tree

    uint32_t m_nextKey;  // +0x08
};

enum SoundBufferMode : int {
    kBufferStreamed = 1,  // looping ring buffer topped up from a file by a refill thread
    kBufferStatic = 2,    // whole sound held in the DirectSound buffer
};

// One DirectSound secondary buffer playing a SoundSource (keyed by m_key within the source's buffer list). A streamed
// buffer holds two seconds of audio; a notification at its half and end positions wakes a refill thread that reads the
// next half from the file.
class SoundBuffer : public KeyedSoundNode {
public:
    // A fresh buffer on `format`: `bufferBytes` long, filled from a sound of `dataLength` bytes.
    SoundBuffer(uint32_t sourceKey, WAVEFORMATEX* format, uint32_t bufferBytes, int mode, uint32_t dataLength);
    // A DuplicateSoundBuffer copy that shares the original's sound data.
    SoundBuffer(const SoundBuffer& original);
    virtual ~SoundBuffer();

    uint32_t BytesToFill();                       // free bytes between the write position and the play cursor
    void SetStream(istream* stream, long loopStart);
    void Refill(istream* in, long loopStart);     // streams the next part of the file into the ring buffer
    void FillFromMemory(const void* data);        // copies a whole in-memory sound into the buffer
    HRESULT Play(int loop);
    HRESULT Stop();                               // stops and rewinds
    HRESULT StopIfPlaying();                      // restores a lost buffer, then stops it if it is playing
    HRESULT SetVolume(LONG volume);
    HRESULT GetStatus(DWORD* status);
    int IsPlaying();
    int QueryNotify();                            // IDirectSoundNotify of the buffer; nonzero means unsupported
    HRESULT StartRefillThread();
    void StopRefillThread();
    static void RefillThread(SoundBuffer* buffer);

    uint32_t m_sourceKey;              // +0x1c
    DSBUFFERDESC1 m_description;       // +0x20 (dwSize, dwFlags, dwBufferBytes, dwReserved, lpwfxFormat)
    IDirectSoundBuffer* m_buffer;      // +0x34
    IDirectSoundNotify* m_notify;      // +0x38
    HANDLE m_thread;                   // +0x3c refill thread
    HANDLE m_events[3];                // +0x40 half-way, end, stop
    int m_failed;                      // +0x4c buffer creation or thread start failed
    uint32_t m_size;                   // +0x50 buffer length in bytes
    int m_mode;                        // +0x54 SoundBufferMode
    uint32_t m_writePosition;          // +0x58
    uint32_t m_lockFlags;              // +0x5c flags for IDirectSoundBuffer::Lock
    uint32_t m_totalLength;            // +0x60 bytes in the sound
    uint32_t m_remaining;              // +0x64 bytes of the sound not yet copied
    long m_loopStart;                  // +0x68 file position the sound restarts from when it loops
    istream* m_stream;                 // +0x6c
    LONG m_refillBusy;                 // +0x70 interlocked guard against overlapping refills
    int m_loop;                        // +0x74
    int m_wrapped;                     // +0x78 a non-looping stream reached its end
    SoundCallbackTimer m_timer;        // +0x7c
    int m_volumeSet;                   // +0xbc
    LONG m_volume;                     // +0xc0
};

// The buffers of one SoundSource, keyed by SoundBuffer::m_key. Key 1 is the original buffer; duplicates start at 2.
class SoundBufferList : public SoundIdTree {
public:
    SoundBufferList();
    virtual ~SoundBufferList();
    virtual void Clear() override;

    int RemoveNode(KeyedSoundNode* node);  // 0x612b80: same as DeleteNode
};
