// SMACKW32.DLL on the decoder (smk_movie.h) and the mixer (dsound/mixer.h). The Smack structure is RAD's: it starts with a copy of the
// file header, so +0 'SMK2', +4 width, +8 height, +0xc frame count, +0x10 frame rate; the player-visible state the game reads sits at
// +0x68 (a palette update comes with the frame about to be decoded), +0x6c (the palette), +0x374 (frame number) and +0x378 (the
// rectangle SmackToBufferRect last reported). Behaviour measured against the real DLL under Wine (smack/tests).
#include "smack_api.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../dsound/mixer.h"
#include "../res/path.h"
#include "smk_movie.h"

#include "port/minwin/gdi32.h"

namespace {

mwsmk::ClockFunction g_clock = mwsmk::SystemClockMs;
mwsmk::GetDcFunction g_getDc = nullptr;
mwsmk::ReleaseDcFunction g_releaseDc = nullptr;

constexpr uint32_t kTrackFlagFirst = 0x2000;  // SMACKTRACK1; track n is 0x2000 << n
constexpr uint32_t kDestReversed = 1;         // SmackToBuffer flag: the buffer is bottom-up
constexpr int kStateOffset = 0x60;            // where the state pointer lives (inside the copy of the file header, unused there)

struct SmackState {
    mwsmk::Movie* movie = nullptr;
    uint32_t trackMask = 0;
    uint32_t current = 0;
    uint8_t* dest = nullptr;
    uint32_t destLeft = 0, destTop = 0, destPitch = 0, destHeight = 0, destFlags = 0;
    double nextDue = 0;
    bool rectPending = false;
    int rectX = 0, rectY = 0, rectW = 0, rectH = 0;
    int64_t audioPushedFor = -1;
    mwds::Stream* streams[mwsmk::kAudioTracks] = {};
    uint32_t volume[mwsmk::kAudioTracks];
    uint32_t pan[mwsmk::kAudioTracks];
};

SmackState* StateOf(Smack* smk) {
    SmackState* s;
    memcpy(&s, smk->m_unk10 + (kStateOffset - 0x10), sizeof s);
    return s;
}

int VolumeToHundredthsDb(uint32_t volume) {
    if (volume == 0) return -10000;
    if (volume >= 32768) return 0;
    int v = static_cast<int>(floor(2000.0 * log10(volume / 32768.0)));
    return v < -10000 ? -10000 : v;
}

int PanToDirectSound(uint32_t pan) {
    int p = static_cast<int>(pan) - 0x8000;
    p = p * 10000 / 0x8000;
    return p < -10000 ? -10000 : (p > 10000 ? 10000 : p);
}

void ApplyVolume(SmackState* s, int track) {
    if (!s->streams[track]) return;
    s->streams[track]->SetVolume(VolumeToHundredthsDb(s->volume[track]));
    s->streams[track]->SetPan(PanToDirectSound(s->pan[track]));
}

void PreparePalette(Smack* smk, SmackState* s) {
    s->movie->PreparePalette(s->current);
    memcpy(smk->m_palette, s->movie->CurrentPalette().rgb, 768);
    smk->m_newPalette = s->movie->PaletteChanged() ? 1 : 0;
    smk->m_frameNumber = s->current;
}

// Copies the blocks the last decode wrote from the decoder's picture into the application's buffer.
void CopyChangedBlocks(SmackState* s) {
    const mwsmk::Movie& m = *s->movie;
    if (!s->dest) return;
    const int width = static_cast<int>(m.Width());
    const int bw = m.BlocksPerRow();
    const std::vector<uint8_t>& changed = m.ChangedBlocks();
    const uint8_t* picture = m.Picture();
    for (size_t blk = 0; blk < changed.size(); ++blk) {
        if (!changed[blk]) continue;
        int bx = static_cast<int>(blk) % bw, by = static_cast<int>(blk) / bw;
        for (int row = 0; row < 4; ++row) {
            int y = by * 4 + row;
            int destRow = static_cast<int>(s->destTop) + y;
            if (s->destFlags & kDestReversed) destRow = static_cast<int>(s->destHeight) - 1 - destRow;
            if (destRow < 0 || destRow >= static_cast<int>(s->destHeight)) continue;
            memcpy(s->dest + static_cast<size_t>(destRow) * s->destPitch + s->destLeft + bx * 4, picture + static_cast<size_t>(y) * width + bx * 4, 4);
        }
    }
}

void PushAudio(SmackState* s) {
    if (s->audioPushedFor == s->movie->LastDecoded()) return;
    s->audioPushedFor = s->movie->LastDecoded();
    for (int t = 0; t < mwsmk::kAudioTracks; ++t) {
        if (!(s->trackMask & (kTrackFlagFirst << t)) || !s->streams[t]) continue;
        const std::vector<uint8_t>& pcm = s->movie->TakeAudio(t);
        if (!pcm.empty()) s->streams[t]->Push(pcm.data(), static_cast<uint32_t>(pcm.size()));
    }
}

struct BufferExtension {
    HWND window;
    uint32_t zoomWidth, zoomHeight;
    uint8_t palette[768];
    uint8_t* pixels;
    bool paletteLoaded;
};

BufferExtension* ExtensionOf(SmackBuf* buf) { return reinterpret_cast<BufferExtension*>(reinterpret_cast<uint8_t*>(buf) + sizeof(SmackBuf)); }

}  // namespace

namespace mwsmk {
void SetClock(ClockFunction clock) { g_clock = clock ? clock : mwsmk::SystemClockMs; }
void SetDeviceContextProvider(GetDcFunction get, ReleaseDcFunction release) {
    g_getDc = get;
    g_releaseDc = release;
}
}  // namespace mwsmk

extern "C" {

void REB_STDCALL SmackSoundUseDirectSound(void*) {}

Smack* REB_STDCALL SmackOpen(const char* name, uint32_t flags, uint32_t) {
    if (!name) return nullptr;
    mwsmk::Movie* movie = mwsmk::Movie::Open(mw::res::ResolvePath(name));
    if (!movie) return nullptr;
    Smack* smk = static_cast<Smack*>(calloc(1, sizeof(Smack)));
    SmackState* s = new SmackState;
    s->movie = movie;
    s->trackMask = flags & (0x7fu * kTrackFlagFirst);
    memcpy(smk, movie->RawHeader(), 0x68);
    memcpy(smk->m_unk10 + (kStateOffset - 0x10), &s, sizeof s);
    for (int t = 0; t < mwsmk::kAudioTracks; ++t) {
        s->volume[t] = 0x8000;
        s->pan[t] = 0x8000;
        const mwsmk::AudioTrackInfo& info = movie->Track(t);
        if ((s->trackMask & (kTrackFlagFirst << t)) && info.present) {
            mwds::Format f;
            f.channels = info.stereo ? 2 : 1;
            f.bitsPerSample = info.bits16 ? 16 : 8;
            f.sampleRate = static_cast<int>(info.sampleRate);
            s->streams[t] = new mwds::Stream(&mwds::Mixer::Global(), f);
        }
    }
    s->nextDue = g_clock();
    PreparePalette(smk, s);
    return smk;
}

void REB_STDCALL SmackClose(Smack* smk) {
    if (!smk) return;
    SmackState* s = StateOf(smk);
    for (mwds::Stream* st : s->streams) delete st;
    delete s->movie;
    delete s;
    free(smk);
}

uint32_t REB_STDCALL SmackDoFrame(Smack* smk) {
    SmackState* s = StateOf(smk);
    if (!s->movie->DecodeFrame(s->current)) return 0;
    CopyChangedBlocks(s);
    PushAudio(s);
    const mwsmk::Movie::Rect r = s->movie->ChangedRect();
    s->rectPending = r.w > 0;
    if (s->rectPending) {
        // the changed area, widened to whole 64-pixel columns as the DLL reports it
        int x0 = r.x & ~63, x1 = (r.x + r.w + 63) & ~63;
        if (x1 > static_cast<int>(s->movie->Width())) x1 = static_cast<int>(s->movie->Width());
        s->rectX = x0;
        s->rectY = r.y;
        s->rectW = x1 - x0;
        s->rectH = r.h;
    }
    return 0;
}

void REB_STDCALL SmackNextFrame(Smack* smk) {
    SmackState* s = StateOf(smk);
    if (s->current + 1 < s->movie->FrameCount()) ++s->current;
    PreparePalette(smk, s);
    const double ms = s->movie->MsPerFrame();
    s->nextDue += ms;
    double now = g_clock();
    if (now - s->nextDue > 4 * ms) s->nextDue = now;  // far behind: resynchronise instead of racing to catch up
}

uint32_t REB_STDCALL SmackWait(Smack* smk) {
    SmackState* s = StateOf(smk);
    return g_clock() >= s->nextDue ? 0 : 1;
}

void REB_STDCALL SmackGoto(Smack* smk, uint32_t frame) {
    SmackState* s = StateOf(smk);
    frame = frame ? frame - 1 : 0;  // the argument is 1-based: SmackGoto(smk, 1) is the first frame
    if (frame >= s->movie->FrameCount()) frame = s->movie->FrameCount() - 1;
    uint8_t before[768];
    memcpy(before, smk->m_palette, 768);
    s->movie->Seek(frame);
    s->current = frame;
    for (mwds::Stream* st : s->streams)
        if (st) st->Clear();
    s->nextDue = g_clock();
    PreparePalette(smk, s);
    smk->m_newPalette = memcmp(before, smk->m_palette, 768) != 0 ? 1 : 0;  // announced after a jump only if it changed
}

void REB_STDCALL SmackToBuffer(Smack* smk, uint32_t left, uint32_t top, uint32_t pitch, uint32_t destHeight, void* buffer, uint32_t flags) {
    SmackState* s = StateOf(smk);
    s->dest = static_cast<uint8_t*>(buffer);
    s->destLeft = left;
    s->destTop = top;
    s->destPitch = pitch;
    s->destHeight = destHeight;
    s->destFlags = flags;
}

uint32_t REB_STDCALL SmackToBufferRect(Smack* smk, uint32_t) {
    SmackState* s = StateOf(smk);
    if (!s->rectPending) return 0;
    s->rectPending = false;
    smk->m_lastRectX = s->rectX;
    smk->m_lastRectY = s->rectY;
    smk->m_lastRectWidth = s->rectW;
    smk->m_lastRectHeight = s->rectH;
    return 1;
}

void REB_STDCALL SmackVolumePan(Smack* smk, uint32_t trackFlags, uint32_t volume, uint32_t pan) {
    SmackState* s = StateOf(smk);
    for (int t = 0; t < mwsmk::kAudioTracks; ++t) {
        if (!(trackFlags & (kTrackFlagFirst << t))) continue;
        s->volume[t] = volume;
        s->pan[t] = pan;
        ApplyVolume(s, t);
    }
}

uint32_t REB_STDCALL SmackGetTrackData(Smack* smk, void* dest, uint32_t trackFlag) {
    SmackState* s = StateOf(smk);
    for (int t = 0; t < mwsmk::kAudioTracks; ++t) {
        if (!(trackFlag & (kTrackFlagFirst << t))) continue;
        const std::vector<uint8_t>& pcm = s->movie->TakeAudio(t);
        if (dest && !pcm.empty()) memcpy(dest, pcm.data(), pcm.size());
        return static_cast<uint32_t>(pcm.size());
    }
    return 0;
}

SmackBuf* REB_STDCALL SmackBufferOpen(HWND window, uint32_t blitType, uint32_t width, uint32_t height, uint32_t zoomWidth, uint32_t zoomHeight) {
    if (width == 0 || height == 0 || width > 8192 || height > 8192) return nullptr;
    uint8_t* block = static_cast<uint8_t*>(calloc(1, sizeof(SmackBuf) + sizeof(BufferExtension)));
    SmackBuf* buf = reinterpret_cast<SmackBuf*>(block);
    BufferExtension* ext = ExtensionOf(buf);
    ext->window = window;
    ext->zoomWidth = zoomWidth ? zoomWidth : width;
    ext->zoomHeight = zoomHeight ? zoomHeight : height;
    ext->pixels = static_cast<uint8_t*>(calloc(static_cast<size_t>(width) * height, 1));
    buf->m_reversed = 1;
    buf->m_surfaceType = 1;
    memcpy(buf->m_unk08, &blitType, sizeof blitType);
    buf->m_width = width;
    buf->m_height = height;
    buf->m_buffer = ext->pixels;
    return buf;
}

void REB_STDCALL SmackBufferClose(SmackBuf* buf) {
    if (!buf) return;
    free(ExtensionOf(buf)->pixels);
    free(buf);
}

uint32_t REB_STDCALL SmackBufferNewPalette(SmackBuf* buf, void* palette, uint32_t) {
    if (!buf || !palette) return 0;
    BufferExtension* ext = ExtensionOf(buf);
    memcpy(ext->palette, palette, 768);
    ext->paletteLoaded = true;
    return 0;
}

uint32_t REB_STDCALL SmackBufferSetPalette(SmackBuf* buf) {
    if (!buf) return 0;
    BufferExtension* ext = ExtensionOf(buf);
    if (!g_getDc) return 0;
    HDC dc = g_getDc(ext->window);
    if (!dc) return 0;
    struct {
        WORD version, count;
        PALETTEENTRY entries[256];
    } lp;
    lp.version = 0x300;
    lp.count = 256;
    for (int i = 0; i < 256; ++i) {
        lp.entries[i].peRed = ext->palette[i * 3];
        lp.entries[i].peGreen = ext->palette[i * 3 + 1];
        lp.entries[i].peBlue = ext->palette[i * 3 + 2];
        lp.entries[i].peFlags = PC_NOCOLLAPSE;
    }
    HPALETTE palette = CreatePalette(reinterpret_cast<LOGPALETTE*>(&lp));
    SelectPalette(dc, palette, FALSE);
    RealizePalette(dc);
    if (g_releaseDc) g_releaseDc(ext->window, dc);
    return 0;
}

uint32_t REB_STDCALL SmackBufferBlit(SmackBuf* buf, HDC dc, int32_t windowX, int32_t windowY, int32_t subX, int32_t subY, int32_t subWidth, int32_t subHeight) {
    if (!buf || !dc) return 0;
    BufferExtension* ext = ExtensionOf(buf);
    const int width = static_cast<int>(buf->m_width), height = static_cast<int>(buf->m_height);
    if (subX < 0) {
        subWidth += subX;
        subX = 0;
    }
    if (subY < 0) {
        subHeight += subY;
        subY = 0;
    }
    if (subX + subWidth > width) subWidth = width - subX;
    if (subY + subHeight > height) subHeight = height - subY;
    if (subWidth <= 0 || subHeight <= 0) return 0;
    struct {
        BITMAPINFOHEADER header;
        RGBQUAD colors[256];
    } info;
    memset(&info, 0, sizeof info);
    info.header.biSize = sizeof(BITMAPINFOHEADER);
    info.header.biWidth = width;
    info.header.biHeight = height;  // bottom-up, as the reversed buffer is stored
    info.header.biPlanes = 1;
    info.header.biBitCount = 8;
    info.header.biClrUsed = 256;
    for (int i = 0; i < 256; ++i) {
        info.colors[i].rgbRed = ext->palette[i * 3];
        info.colors[i].rgbGreen = ext->palette[i * 3 + 1];
        info.colors[i].rgbBlue = ext->palette[i * 3 + 2];
    }
    const int scaleX = static_cast<int>(ext->zoomWidth / buf->m_width ? ext->zoomWidth / buf->m_width : 1);
    const int scaleY = static_cast<int>(ext->zoomHeight / buf->m_height ? ext->zoomHeight / buf->m_height : 1);
    if (scaleX == 1 && scaleY == 1) {
        SetDIBitsToDevice(dc, windowX + subX, windowY + subY, static_cast<DWORD>(subWidth), static_cast<DWORD>(subHeight), subX,
                          height - subY - subHeight, 0, static_cast<UINT>(height), ext->pixels, reinterpret_cast<BITMAPINFO*>(&info), DIB_RGB_COLORS);
        return 0;
    }
    // zoomed: replicate the sub-rectangle's pixels into a temporary bottom-up DIB
    const int outW = subWidth * scaleX, outH = subHeight * scaleY;
    uint8_t* scaled = static_cast<uint8_t*>(malloc(static_cast<size_t>(outW) * outH));
    for (int y = 0; y < outH; ++y) {
        const uint8_t* src = ext->pixels + static_cast<size_t>(height - 1 - (subY + y / scaleY)) * width + subX;
        uint8_t* row = scaled + static_cast<size_t>(outH - 1 - y) * outW;
        for (int x = 0; x < outW; ++x) row[x] = src[x / scaleX];
    }
    info.header.biWidth = outW;
    info.header.biHeight = outH;
    SetDIBitsToDevice(dc, windowX + subX * scaleX, windowY + subY * scaleY, static_cast<DWORD>(outW), static_cast<DWORD>(outH), 0, 0, 0,
                      static_cast<UINT>(outH), scaled, reinterpret_cast<BITMAPINFO*>(&info), DIB_RGB_COLORS);
    free(scaled);
    return 0;
}

}  // extern "C"
