// The audio backend of the software DirectSound: pulls interleaved s16 stereo from mwds::Mixer::Global().Render() and plays it
// through an SDL3 audio stream. Started by the first DirectSoundCreate.
//
// When no output device can be opened (a headless machine, the Wine test container) a pacing thread renders at real-time speed
// into nothing, so that play cursors, notifications and the game's refill threads behave as they do with a device.
//   REB_AUDIO_CAPTURE=<file>  additionally (and instead of the device) write everything rendered, raw s16le stereo at the mixer's rate,
//                             to <file>; the mixer is rendered at real-time pace by the pacing thread.
#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>

#include <vector>

#include "port/minwin_impl/dsound/mixer.h"

namespace {

constexpr int kOutputRate = 44100;
constexpr int kBlockFrames = 1024;

SDL_AudioStream* g_stream = nullptr;
SDL_Thread* g_thread = nullptr;
FILE* g_capture = nullptr;
bool g_started = false;

void RenderBlock(int frames, std::vector<int16_t>* block) {
    block->assign(static_cast<size_t>(frames) * 2, 0);
    mwds::Mixer::Global().Render(block->data(), frames);
    if (g_capture) fwrite(block->data(), sizeof(int16_t) * 2, static_cast<size_t>(frames), g_capture);
}

void SDLCALL StreamCallback(void*, SDL_AudioStream* stream, int additionalBytes, int) {
    static std::vector<int16_t> block;
    const int bytesPerFrame = static_cast<int>(sizeof(int16_t)) * 2;
    while (additionalBytes > 0) {
        const int frames = additionalBytes / bytesPerFrame < kBlockFrames ? additionalBytes / bytesPerFrame : kBlockFrames;
        if (frames <= 0) break;
        RenderBlock(frames, &block);
        SDL_PutAudioStreamData(stream, block.data(), frames * bytesPerFrame);
        additionalBytes -= frames * bytesPerFrame;
    }
}

int SDLCALL PacingThread(void*) {
    std::vector<int16_t> block;
    const Uint64 start = SDL_GetTicksNS();
    Uint64 rendered = 0;
    for (;;) {
        const Uint64 due = rendered * 1000000000ull / kOutputRate;
        const Uint64 now = SDL_GetTicksNS() - start;
        if (now < due) {
            SDL_DelayNS(due - now);
            continue;
        }
        RenderBlock(kBlockFrames, &block);
        rendered += kBlockFrames;
        if (g_capture) fflush(g_capture);
    }
}

bool OpenDeviceStream() {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
    SDL_AudioSpec spec = {};
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq = kOutputRate;
    g_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, StreamCallback, nullptr);
    if (!g_stream) return false;
    SDL_ResumeAudioStreamDevice(g_stream);
    return true;
}

void StartBackend() {
    if (g_started) return;
    g_started = true;
    mwds::Mixer::Global().SetOutputRate(kOutputRate);
    const char* capture = getenv("REB_AUDIO_CAPTURE");
    if (capture && *capture) g_capture = fopen(capture, "wb");
    if (!g_capture && OpenDeviceStream()) return;
    g_thread = SDL_CreateThread(PacingThread, "audio-pacing", nullptr);
}

}  // namespace

namespace minwin_audio {

void Install() { mwds::SetDeviceOpener(StartBackend); }

}  // namespace minwin_audio
