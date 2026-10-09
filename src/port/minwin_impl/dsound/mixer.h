// minwin_impl/dsound: the software mixer behind the DirectSound emulation. SDL-free and platform-free: the audio backend pulls
// interleaved signed 16-bit stereo with Render(). Everything is integer fixed-point arithmetic (volume tables aside), so the
// output is deterministic.
//
// Voices model DirectSound secondary buffers: a circular byte buffer with a play cursor, a write cursor a little ahead of it,
// looping, volume (hundredths of a dB, 0 = full, -10000 = silent), pan, frequency and position notifications. Streams model
// the push-style audio of the Smacker decoder: PCM is queued and played in order.
#pragma once
#include <stdint.h>

#include <memory>
#include <vector>

namespace mwds {

struct Format {
    int channels = 2;
    int bitsPerSample = 16;
    int sampleRate = 22050;
    int BlockAlign() const { return channels * (bitsPerSample / 8); }
};

constexpr uint32_t kOffsetStop = 0xffffffffu;  // DSBPN_OFFSETSTOP

struct Notification {
    uint32_t offset;  // byte offset in the buffer, or kOffsetStop
    void* event;      // handed to the event signaler
};

// Called (outside the mixer's lock) for every notification event that fires. Installed by the integration layer: the port
// points it at its SetEvent. Tests record the calls.
using EventSignaler = void (*)(void* event);
void SetEventSignaler(EventSignaler signaler);

// Called by DirectSoundCreate (every time): the integration layer starts its audio backend here, on the first call.
using DeviceOpener = void (*)();
void SetDeviceOpener(DeviceOpener opener);
void OpenDevice();

// A plain mutex (CRITICAL_SECTION / pthread): std::mutex would pull msvcp140.dll into the i686-windows builds, which Wine lacks.
class Mutex {
public:
    Mutex();
    ~Mutex();
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
    void lock();
    void unlock();

private:
    alignas(8) unsigned char m_storage[64];
};

class Guard {
public:
    explicit Guard(Mutex& m) : m_mutex(m) { m_mutex.lock(); }
    ~Guard() { m_mutex.unlock(); }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;

private:
    Mutex& m_mutex;
};

class Mixer;

// A DirectSound secondary buffer.
class Voice {
public:
    Voice(Mixer* mixer, const Format& format, uint32_t sizeBytes);
    Voice(Mixer* mixer, const Voice& original);  // DuplicateSoundBuffer: shares the sample memory
    ~Voice();

    const Format& GetFormat() const { return m_format; }
    uint32_t SizeBytes() const { return m_size; }
    uint8_t* Data() { return m_data->data(); }

    void Play(bool loop);
    void Stop();
    bool IsPlaying() const;
    bool IsLooping() const;
    void GetCursors(uint32_t* play, uint32_t* write) const;
    bool SetPlayCursor(uint32_t bytes);
    void SetVolume(int hundredthsDb);
    int Volume() const { return m_volume; }
    void SetPan(int pan);
    int Pan() const { return m_pan; }
    void SetFrequency(uint32_t hz);  // 0 restores the format's own rate
    uint32_t Frequency() const { return m_frequency ? m_frequency : static_cast<uint32_t>(m_format.sampleRate); }
    void SetNotifications(const Notification* list, uint32_t count);

    // Lock arithmetic (no state): the two regions of a circular range.
    struct Regions {
        uint8_t* first;
        uint32_t firstBytes;
        uint8_t* second;
        uint32_t secondBytes;
    };
    Regions Lock(uint32_t offset, uint32_t bytes);

private:
    friend class Mixer;
    Mixer* m_mixer;
    Format m_format;
    uint32_t m_size;
    std::shared_ptr<std::vector<uint8_t>> m_data;
    bool m_playing = false, m_looping = false;
    uint64_t m_pos = 0;  // play position in frames, Q32
    int m_volume = 0, m_pan = 0;
    uint32_t m_frequency = 0;
    std::vector<Notification> m_notifications;
};

// A queue of PCM played in order (Smacker audio).
class Stream {
public:
    Stream(Mixer* mixer, const Format& format);
    ~Stream();
    void Push(const uint8_t* pcm, uint32_t bytes);
    uint32_t BufferedBytes() const;
    void Clear();
    void SetVolume(int hundredthsDb);
    void SetPan(int pan);

private:
    friend class Mixer;
    Mixer* m_mixer;
    Format m_format;
    std::vector<uint8_t> m_queue;
    uint64_t m_pos = 0;  // Q32 frames into m_queue
    int m_volume = 0, m_pan = 0;
};

class Mixer {
public:
    explicit Mixer(int outputRate = 44100) : m_outputRate(outputRate) {}
    int OutputRate() const { return m_outputRate; }
    void SetOutputRate(int rate);

    // The DirectSound primary buffer's volume applies to everything.
    void SetMasterVolume(int hundredthsDb);

    // Interleaved stereo s16. Voices that run to the end stop; their notifications are signalled after the lock is released.
    void Render(int16_t* out, int frames);

    // Frames rendered since creation (tests, A/V sync).
    uint64_t FramesRendered() const;

    static Mixer& Global();

private:
    friend class Voice;
    friend class Stream;
    void Add(Voice* v);
    void Remove(Voice* v);
    void Add(Stream* s);
    void Remove(Stream* s);
    Guard Lock() const { return Guard(m_mutex); }
    void RenderVoice(Voice* v, int32_t* acc, int frames, std::vector<void*>* events);
    void RenderStream(Stream* s, int32_t* acc, int frames);

    mutable Mutex m_mutex;
    int m_outputRate;
    int m_masterVolume = 0;
    uint64_t m_frames = 0;
    std::vector<Voice*> m_voices;
    std::vector<Stream*> m_streams;
};

// 10^(hundredthsDb/2000) as Q15 (0 for -10000 or lower).
int32_t GainQ15(int hundredthsDb);

}  // namespace mwds
