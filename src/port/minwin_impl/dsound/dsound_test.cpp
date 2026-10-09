#define MINWIN_IMPLEMENTATION 1
#include "port/minwin/dsound.h"

#include <string.h>

#include <vector>

#include "../tests/mw_test.h"
#include "mixer.h"

using namespace mwds;

namespace {

std::vector<void*> g_fired;
void Record(void* e) { g_fired.push_back(e); }

struct Silencer {
    Silencer() {
        g_fired.clear();
        SetEventSignaler(Record);
    }
    ~Silencer() { SetEventSignaler(nullptr); }
};

Format Mono16(int rate) {
    Format f;
    f.channels = 1;
    f.bitsPerSample = 16;
    f.sampleRate = rate;
    return f;
}

void FillRamp(Voice& v, int frames, int base = 1000) {
    int16_t* s = reinterpret_cast<int16_t*>(v.Data());
    for (int i = 0; i < frames; ++i) s[i] = static_cast<int16_t>(base + i * 10);
}

std::vector<int16_t> RenderFrames(Mixer& m, int frames) {
    std::vector<int16_t> out(static_cast<size_t>(frames) * 2);
    m.Render(out.data(), frames);
    return out;
}

WAVEFORMATEX Wave(int rate, int channels, int bits) {
    WAVEFORMATEX wf = {};
    wf.wFormatTag = 1;
    wf.nChannels = static_cast<WORD>(channels);
    wf.nSamplesPerSec = static_cast<DWORD>(rate);
    wf.wBitsPerSample = static_cast<WORD>(bits);
    wf.nBlockAlign = static_cast<WORD>(channels * bits / 8);
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    return wf;
}

}  // namespace

TEST(gain_table_in_hundredths_of_a_decibel) {
    CHECK_EQ(GainQ15(0), 32768);
    CHECK_EQ(GainQ15(100), 32768);
    CHECK_EQ(GainQ15(-10000), 0);
    CHECK_EQ(GainQ15(-6000), 33);                    // -60 dB = 0.001
    CHECK(GainQ15(-600) > 16380 && GainQ15(-600) < 16430);  // -6 dB = 0.501
    CHECK(GainQ15(-2000) > 3270 && GainQ15(-2000) < 3280);  // -20 dB = 0.1
}

TEST(plays_at_native_rate_exactly_and_stops_at_the_end) {
    Mixer m(22050);
    Voice v(&m, Mono16(22050), 40);  // 20 frames
    FillRamp(v, 20);
    v.Play(false);
    std::vector<int16_t> out = RenderFrames(m, 30);
    for (int i = 0; i < 20; ++i) {
        CHECK_EQ(out[i * 2], 1000 + i * 10);
        CHECK_EQ(out[i * 2 + 1], 1000 + i * 10);
    }
    for (int i = 20; i < 30; ++i) CHECK_EQ(out[i * 2], 0);
    CHECK(!v.IsPlaying());
    uint32_t play, write;
    v.GetCursors(&play, &write);
    CHECK_EQ(play, 0);
    CHECK_EQ(write, 0);
}

TEST(looping_wraps_and_cursors_track) {
    Mixer m(22050);
    Voice v(&m, Mono16(22050), 40);
    FillRamp(v, 20);
    v.Play(true);
    std::vector<int16_t> out = RenderFrames(m, 25);
    CHECK_EQ(out[19 * 2], 1190);
    CHECK_EQ(out[20 * 2], 1000);
    CHECK_EQ(out[24 * 2], 1040);
    uint32_t play, write;
    v.GetCursors(&play, &write);
    CHECK_EQ(play, 10);  // 5 frames * 2 bytes
    CHECK(write > play || write < play);
    CHECK(v.IsLooping());
    CHECK(v.SetPlayCursor(30));
    CHECK(!v.SetPlayCursor(40));
    v.GetCursors(&play, &write);
    CHECK_EQ(play, 30);
}

TEST(write_cursor_leads_the_play_cursor_by_about_15_ms_and_wraps) {
    Mixer m(44100);
    Voice v(&m, Mono16(44100), 44100 * 2);  // one second
    v.Play(true);
    RenderFrames(m, 100);
    uint32_t play, write;
    v.GetCursors(&play, &write);
    CHECK_EQ(play, 200);
    CHECK_EQ(write - play, 44100 * 2 * 15 / 1000 / 2 * 2);
    v.SetPlayCursor(44100 * 2 - 100);
    v.GetCursors(&play, &write);
    CHECK(write < play);  // wrapped
    v.Stop();
    v.GetCursors(&play, &write);
    CHECK_EQ(play, write);  // stopped: both cursors together
}

TEST(resampling_is_linear_and_deterministic) {
    Mixer m(22050);
    Voice v(&m, Mono16(11025), 40);  // plays at half speed
    FillRamp(v, 20, 0);
    v.Play(false);
    std::vector<int16_t> out = RenderFrames(m, 8);
    const int expect[8] = {0, 5, 10, 15, 20, 25, 30, 35};
    for (int i = 0; i < 8; ++i) CHECK_EQ(out[i * 2], expect[i]);
    Voice fast(&m, Mono16(44100), 80);  // double speed
    FillRamp(fast, 40, 0);
    v.Stop();
    fast.Play(false);
    out = RenderFrames(m, 4);
    CHECK_EQ(out[0], 0);
    CHECK_EQ(out[2], 20);
    CHECK_EQ(out[4], 40);
}

TEST(volume_pan_and_master_volume) {
    Mixer m(22050);
    Voice v(&m, Mono16(22050), 8);
    for (int i = 0; i < 4; ++i) reinterpret_cast<int16_t*>(v.Data())[i] = 20000;
    v.Play(true);
    auto at = [&](int vol, int pan, int master = 0) {
        v.SetVolume(vol);
        v.SetPan(pan);
        m.SetMasterVolume(master);
        std::vector<int16_t> o = RenderFrames(m, 1);
        return std::pair<int, int>(o[0], o[1]);
    };
    auto full = at(0, 0);
    CHECK_EQ(full.first, 20000);
    CHECK_EQ(full.second, 20000);
    auto half = at(-600, 0);
    CHECK(half.first > 9990 && half.first < 10060);
    auto panRight = at(0, 6000);  // -> left attenuated by 60 dB
    CHECK(panRight.first < 25);
    CHECK_EQ(panRight.second, 20000);
    auto panLeft = at(0, -10000);
    CHECK_EQ(panLeft.second, 0);
    CHECK_EQ(panLeft.first, 20000);
    auto master = at(0, 0, -600);
    CHECK(master.first > 9990 && master.first < 10060);
    CHECK_EQ(at(-10000, 0).first, 0);
    m.SetMasterVolume(0);
}

TEST(eight_bit_and_stereo_sources) {
    Mixer m(22050);
    Format f8;
    f8.channels = 2;
    f8.bitsPerSample = 8;
    f8.sampleRate = 22050;
    Voice v(&m, f8, 4);
    v.Data()[0] = 128 + 64;  // L +64 -> +16384
    v.Data()[1] = 128 - 64;  // R
    v.Data()[2] = 255;
    v.Data()[3] = 0;
    v.Play(false);
    std::vector<int16_t> out = RenderFrames(m, 2);
    CHECK_EQ(out[0], 64 << 8);
    CHECK_EQ(out[1], -(64 << 8));
    CHECK_EQ(out[2], 127 << 8);
    CHECK_EQ(out[3], -(128 << 8));
}

TEST(mixing_sums_and_clips) {
    Mixer m(22050);
    Voice a(&m, Mono16(22050), 4), b(&m, Mono16(22050), 4);
    reinterpret_cast<int16_t*>(a.Data())[0] = 30000;
    reinterpret_cast<int16_t*>(b.Data())[0] = 30000;
    a.Play(false);
    b.Play(false);
    std::vector<int16_t> out = RenderFrames(m, 1);
    CHECK_EQ(out[0], 32767);
    reinterpret_cast<int16_t*>(a.Data())[0] = -30000;
    reinterpret_cast<int16_t*>(b.Data())[0] = -30000;
    a.SetPlayCursor(0);
    b.SetPlayCursor(0);
    a.Play(false);
    b.Play(false);
    out = RenderFrames(m, 1);
    CHECK_EQ(out[0], -32768);
}

TEST(notifications_fire_at_offsets_and_at_stop) {
    Silencer s;
    Mixer m(22050);
    Voice v(&m, Mono16(22050), 200);  // 100 frames
    int tokens[3];
    Notification n[3] = {{100, &tokens[0]}, {199, &tokens[1]}, {kOffsetStop, &tokens[2]}};
    v.SetNotifications(n, 3);
    v.Play(false);
    RenderFrames(m, 49);
    CHECK_EQ(g_fired.size(), 0);
    RenderFrames(m, 2);  // crosses frame 50 (byte 100)
    CHECK_EQ(g_fired.size(), 1);
    CHECK(g_fired[0] == &tokens[0]);
    RenderFrames(m, 60);  // reaches the end: second position then the stop notification
    CHECK_EQ(g_fired.size(), 3);
    CHECK(g_fired[1] == &tokens[1]);
    CHECK(g_fired[2] == &tokens[2]);
    CHECK(!v.IsPlaying());
}

TEST(looping_notifications_repeat_every_pass) {
    Silencer s;
    Mixer m(22050);
    Voice v(&m, Mono16(22050), 200);
    int token;
    Notification n = {100, &token};
    v.SetNotifications(&n, 1);
    v.Play(true);
    RenderFrames(m, 320);  // 3.2 passes
    CHECK_EQ(g_fired.size(), 3);
}

TEST(streamed_music_refill_pattern_is_gapless) {
    // The game's music buffer: a ring refilled when the play cursor crosses the half-way mark and the end.
    Silencer s;
    Mixer m(22050);
    const uint32_t size = 400;  // 200 frames
    Voice v(&m, Mono16(22050), size);
    int halfEvent, endEvent;
    Notification n[2] = {{size / 2, &halfEvent}, {size - 1, &endEvent}};
    v.SetNotifications(n, 2);
    int source = 0;
    auto fill = [&](uint32_t offset, uint32_t bytes) {
        Voice::Regions r = v.Lock(offset, bytes);
        int16_t* p = reinterpret_cast<int16_t*>(r.first);
        for (uint32_t i = 0; i < r.firstBytes / 2; ++i) p[i] = static_cast<int16_t>(++source);
        if (r.second) {
            int16_t* q = reinterpret_cast<int16_t*>(r.second);
            for (uint32_t i = 0; i < r.secondBytes / 2; ++i) q[i] = static_cast<int16_t>(++source);
        }
    };
    fill(0, size);
    v.Play(true);
    std::vector<int> heard;
    for (int block = 0; block < 40; ++block) {
        std::vector<int16_t> out = RenderFrames(m, 25);
        for (int i = 0; i < 25; ++i) heard.push_back(out[i * 2]);
        for (void* e : g_fired) {
            if (e == &halfEvent) fill(0, size / 2);
            else fill(size / 2, size / 2);
        }
        g_fired.clear();
    }
    for (size_t i = 0; i < heard.size(); ++i) CHECK_EQ(heard[i], static_cast<int>(i) + 1);
}

TEST(lock_regions_wrap_at_the_end_of_the_buffer) {
    Mixer m(22050);
    Voice v(&m, Mono16(22050), 100);
    Voice::Regions r = v.Lock(20, 30);
    CHECK_EQ(r.firstBytes, 30);
    CHECK(r.second == nullptr);
    r = v.Lock(80, 50);
    CHECK_EQ(r.firstBytes, 20);
    CHECK_EQ(r.secondBytes, 30);
    CHECK(r.second == v.Data());
    CHECK(r.first == v.Data() + 80);
}

TEST(streams_play_queued_pcm_in_order_and_go_silent_when_empty) {
    Mixer m(22050);
    Stream s(&m, Mono16(22050));
    int16_t pcm[10];
    for (int i = 0; i < 10; ++i) pcm[i] = static_cast<int16_t>(100 + i);
    s.Push(reinterpret_cast<uint8_t*>(pcm), sizeof pcm);
    CHECK_EQ(s.BufferedBytes(), 20);
    std::vector<int16_t> out = RenderFrames(m, 6);
    CHECK_EQ(out[0], 100);
    CHECK_EQ(out[10], 105);
    CHECK_EQ(s.BufferedBytes(), 8);
    s.Push(reinterpret_cast<uint8_t*>(pcm), 4);
    out = RenderFrames(m, 8);
    CHECK_EQ(out[0], 106);
    CHECK_EQ(out[2 * 3], 109);
    CHECK_EQ(out[2 * 4], 100);
    CHECK_EQ(out[2 * 6], 0);
    s.Clear();
    CHECK_EQ(s.BufferedBytes(), 0);
}

TEST(com_device_and_buffer_contract) {
    Silencer s;
    IDirectSound* ds = nullptr;
    CHECK_EQ(DirectSoundCreate(nullptr, &ds, nullptr), 0);
    CHECK(ds != nullptr);
    if (!ds) return;
    CHECK_EQ(ds->SetCooperativeLevel(nullptr, 0), static_cast<HRESULT>(0x80070057));
    CHECK_EQ(ds->SetCooperativeLevel(nullptr, 3), 0);

    // primary buffer: no size, no format; CTRL3D gives a listener; SetFormat needs priority level
    WAVEFORMATEX wf = Wave(11025, 1, 16);
    DSBUFFERDESC1 d = {};
    d.dwSize = sizeof d;
    d.dwFlags = 0x1 | 0x10 | 0x8000;
    IDirectSoundBuffer* primary = nullptr;
    CHECK_EQ(ds->CreateSoundBuffer(reinterpret_cast<LPCDSBUFFERDESC>(&d), &primary, nullptr), 0);
    CHECK_EQ(primary->SetFormat(&wf), 0);
    WAVEFORMATEX bad = Wave(11025, 1, 16);
    bad.wFormatTag = 2;
    CHECK_EQ(primary->SetFormat(&bad), static_cast<HRESULT>(0x88780064));
    void* listener = nullptr;
    CHECK_EQ(primary->QueryInterface(IID_IDirectSound3DListener, &listener), 0);
    static_cast<IUnknown*>(listener)->Release();
    CHECK_EQ(primary->Play(0, 0, DSBPLAY_LOOPING), 0);
    DWORD status = 0;
    primary->GetStatus(&status);
    CHECK_EQ(status, 5);
    IDirectSoundBuffer* again = nullptr;
    CHECK_EQ(ds->CreateSoundBuffer(reinterpret_cast<LPCDSBUFFERDESC>(&d), &again, nullptr), 0);
    CHECK(again == primary);
    again->Release();
    d.lpwfxFormat = &wf;
    IDirectSoundBuffer* bogus = nullptr;
    CHECK_EQ(ds->CreateSoundBuffer(reinterpret_cast<LPCDSBUFFERDESC>(&d), &bogus, nullptr), static_cast<HRESULT>(0x80070057));

    // secondary buffer
    DSBUFFERDESC1 sd = {};
    sd.dwSize = sizeof sd;
    sd.dwFlags = 0x181e8;
    sd.dwBufferBytes = 200;
    sd.lpwfxFormat = &wf;
    IDirectSoundBuffer* buf = nullptr;
    CHECK_EQ(ds->CreateSoundBuffer(reinterpret_cast<LPCDSBUFFERDESC>(&sd), &buf, nullptr), 0);
    CHECK(buf != nullptr);
    void *p1, *p2;
    DWORD b1, b2;
    CHECK_EQ(buf->Lock(150, 100, &p1, &b1, &p2, &b2, 0), 0);
    CHECK_EQ(b1, 50);
    CHECK_EQ(b2, 50);
    CHECK_EQ(buf->Unlock(p1, b1, p2, b2), 0);
    CHECK_EQ(buf->Unlock(reinterpret_cast<void*>(0x10), 4, nullptr, 0), static_cast<HRESULT>(0x80070057));
    CHECK_EQ(buf->Lock(200, 10, &p1, &b1, &p2, &b2, 0), static_cast<HRESULT>(0x80070057));
    CHECK_EQ(buf->Lock(0, 0, &p1, &b1, &p2, &b2, 2), 0);
    CHECK_EQ(b1, 200);
    CHECK(p2 == nullptr);
    CHECK_EQ(buf->SetVolume(-1200), 0);
    CHECK_EQ(buf->SetVolume(5), static_cast<HRESULT>(0x80070057));
    CHECK_EQ(buf->SetVolume(-10001), static_cast<HRESULT>(0x80070057));
    LONG vol = 0;
    buf->GetVolume(&vol);
    CHECK_EQ(vol, -1200);
    CHECK_EQ(buf->SetPan(2000), 0);
    CHECK_EQ(buf->SetFrequency(22050), 0);
    DWORD hz = 0;
    buf->GetFrequency(&hz);
    CHECK_EQ(hz, 22050);

    IDirectSoundNotify* notify = nullptr;
    CHECK_EQ(buf->QueryInterface(IID_IDirectSoundNotify, reinterpret_cast<void**>(&notify)), 0);
    int tokens[2];
    DSBPOSITIONNOTIFY pos[2] = {{100, &tokens[0]}, {199, &tokens[1]}};
    CHECK_EQ(notify->SetNotificationPositions(2, pos), 0);
    DSBPOSITIONNOTIFY badPos[1] = {{200, &tokens[0]}};
    CHECK_EQ(notify->SetNotificationPositions(1, badPos), static_cast<HRESULT>(0x80070057));

    // duplicate shares the samples
    IDirectSoundBuffer* dup = nullptr;
    CHECK_EQ(ds->DuplicateSoundBuffer(buf, &dup), 0);
    CHECK(dup != nullptr);
    reinterpret_cast<int16_t*>(p1)[0] = 1234;
    void *q1, *q2;
    DWORD c1, c2;
    CHECK_EQ(dup->Lock(0, 4, &q1, &c1, &q2, &c2, 0), 0);
    CHECK_EQ(reinterpret_cast<int16_t*>(q1)[0], 1234);

    // play through the global mixer
    Mixer::Global().SetOutputRate(11025);
    CHECK_EQ(buf->SetFrequency(0), 0);
    CHECK_EQ(buf->SetVolume(0), 0);
    CHECK_EQ(buf->SetPan(0), 0);
    buf->SetCurrentPosition(0);
    CHECK_EQ(buf->Play(0, 0, 0), 0);
    buf->GetStatus(&status);
    CHECK_EQ(status & 1, 1);
    std::vector<int16_t> out(100 * 2);
    Mixer::Global().Render(out.data(), 49);
    CHECK_EQ(out[0], 1234);
    CHECK_EQ(g_fired.size(), 0);
    Mixer::Global().Render(out.data(), 2);  // the play cursor reaches byte 100
    CHECK_EQ(g_fired.size(), 1);
    CHECK_EQ(buf->Stop(), 0);
    buf->GetStatus(&status);
    CHECK_EQ(status & 1, 0);

    dup->Release();
    notify->Release();
    CHECK_EQ(buf->Release(), 0);
    CHECK_EQ(primary->Release(), 0);
    CHECK_EQ(ds->Release(), 0);
}

TEST(controls_require_their_buffer_flags) {
    IDirectSound* ds = nullptr;
    DirectSoundCreate(nullptr, &ds, nullptr);
    WAVEFORMATEX wf = Wave(22050, 2, 16);
    DSBUFFERDESC1 sd = {};
    sd.dwSize = sizeof sd;
    sd.dwFlags = 0x2;  // static, no controls
    sd.dwBufferBytes = 400;
    sd.lpwfxFormat = &wf;
    IDirectSoundBuffer* buf = nullptr;
    CHECK_EQ(ds->CreateSoundBuffer(reinterpret_cast<LPCDSBUFFERDESC>(&sd), &buf, nullptr), 0);
    CHECK_EQ(buf->SetVolume(-100), static_cast<HRESULT>(0x8878001e));
    CHECK_EQ(buf->SetPan(0), static_cast<HRESULT>(0x8878001e));
    CHECK_EQ(buf->SetFrequency(11025), static_cast<HRESULT>(0x8878001e));
    void* notify = nullptr;
    CHECK_EQ(buf->QueryInterface(IID_IDirectSoundNotify, &notify), static_cast<HRESULT>(0x80004002));
    sd.dwBufferBytes = 2;
    IDirectSoundBuffer* tiny = nullptr;
    CHECK_EQ(ds->CreateSoundBuffer(reinterpret_cast<LPCDSBUFFERDESC>(&sd), &tiny, nullptr), static_cast<HRESULT>(0x80070057));
    buf->Release();
    ds->Release();
}
