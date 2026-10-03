// DirectSound device / primary buffer setup.
#include <windows.h>
#include <mmreg.h>
#include <dsound.h>

#include <cstdint>

extern WAVEFORMATEX g_primaryFormat;
extern IDirectSound* g_directSound;
extern IDirectSoundBuffer* g_primaryBuffer;

enum SoundResult : int {
    kSoundOk = 0,
    kSoundCreatePrimaryFailed = 8,
    kSoundSetFormatFailed = 10,
};

// packedFormat: bits 0-7 bits per sample, 8-15 channel count, 16-31 sample rate.
int SetPrimaryBufferFormat(uint32_t packedFormat) {
    const uint16_t bitsPerSample = packedFormat & 0xff;
    const uint16_t channels = (packedFormat >> 8) & 0xff;
    g_primaryFormat = {};
    g_primaryFormat.wFormatTag = WAVE_FORMAT_PCM;
    g_primaryFormat.nChannels = channels;
    g_primaryFormat.wBitsPerSample = bitsPerSample;
    g_primaryFormat.nBlockAlign = static_cast<uint16_t>(static_cast<int>(channels * bitsPerSample) / 8);
    g_primaryFormat.nSamplesPerSec = packedFormat >> 16;
    g_primaryFormat.nAvgBytesPerSec = g_primaryFormat.nSamplesPerSec * g_primaryFormat.nBlockAlign;

    if (!g_primaryBuffer) {
        DSBUFFERDESC1 desc = {};
        desc.dwSize = sizeof(desc);
        desc.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME;
        if (g_directSound->CreateSoundBuffer(reinterpret_cast<LPCDSBUFFERDESC>(&desc), &g_primaryBuffer, nullptr) != DS_OK)
            return kSoundCreatePrimaryFailed;
    }
    return g_primaryBuffer->SetFormat(&g_primaryFormat) != DS_OK ? kSoundSetFormatFailed : kSoundOk;
}
