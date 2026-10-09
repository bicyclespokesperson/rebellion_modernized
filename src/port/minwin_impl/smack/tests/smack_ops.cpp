#include "smack_ops.h"

#include <stdlib.h>
#include <string.h>

#include "smack_table.h"

namespace {

uint64_t Fnv(const uint8_t* p, size_t n, uint64_t h = 1469598103934665603ull) {
    for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 1099511628211ull;
    return h;
}

}  // namespace

bool RunSmackScenario(const SmackApiTable& api, const char* path, int maxFrames, int dumpFrame, const char* dumpPath, FILE* out) {
    Smack* smk = api.open(path, 0xfe000, 0xffffffffu);
    if (!smk) {
        fprintf(out, "OPEN FAILED %s\n", path);
        return false;
    }
    HWND window = api.makeWindow();
    SmackBuf* buf = api.bufferOpen(window, 3, smk->m_width, smk->m_height, smk->m_width, smk->m_height);
    if (!buf) {
        fprintf(out, "BUFFER OPEN FAILED\n");
        return false;
    }
    fprintf(out, "movie version=%08x width=%u height=%u frames=%u\n", smk->m_version, smk->m_width, smk->m_height, smk->m_frameCount);
    uint32_t setPaletteResult = api.bufferSetPalette ? api.bufferSetPalette(buf) : 0;
    fprintf(out, "setpalette=%u\n", setPaletteResult);
    fprintf(out, "buffer reversed=%u surface=%u width=%u height=%u\n", buf->m_reversed, buf->m_surfaceType, buf->m_width, buf->m_height);
    if (getenv("SMACK_STRUCT")) {
        const uint8_t* b = reinterpret_cast<const uint8_t*>(smk);
        fprintf(out, "smack:");
        for (int i = 0; i < 0x70; ++i) fprintf(out, "%s%02x", (i % 16) ? "" : "\n  ", b[i]);
        b = reinterpret_cast<const uint8_t*>(buf);
        fprintf(out, "\nsmackbuf:");
        for (int i = 0; i < 0x40; ++i) fprintf(out, "%s%02x", (i % 16) ? "" : "\n  ", b[i]);
        fprintf(out, "\n");
    }
    api.toBuffer(smk, (buf->m_width - smk->m_width) >> 1, (buf->m_height - smk->m_height) >> 1, buf->m_width, buf->m_height, buf->m_buffer,
                 buf->m_reversed);
    const uint8_t* picture = static_cast<const uint8_t*>(buf->m_buffer);
    size_t pictureBytes = static_cast<size_t>(buf->m_width) * buf->m_height;
    std::vector<uint8_t> audio(1 << 20);
    uint32_t frames = smk->m_frameCount;
    if (maxFrames > 0 && static_cast<uint32_t>(maxFrames) < frames) frames = static_cast<uint32_t>(maxFrames);
    fprintf(out, "wait-at-open=%u\n", api.wait(smk) != 0);
    for (uint32_t f = 0; f < frames; ++f) {
        fprintf(out, "frame %u number=%u newpal=%u paltype=%u", f, smk->m_frameNumber, smk->m_newPalette != 0, smk->m_paletteType);
        uint64_t beforePal = Fnv(smk->m_palette, 768);
        if (smk->m_newPalette && api.bufferNewPalette) {
            uint32_t r = api.bufferNewPalette(buf, smk->m_palette, (smk->m_newPalette & 0xffff0000u) | (smk->m_paletteType & 0xffff));
            fprintf(out, " newpalette=%u", r);
        }
        fprintf(out, " palbefore=%016llx", static_cast<unsigned long long>(beforePal));
        uint32_t doFrameResult = api.doFrame(smk);
        fprintf(out, " ret=%u rects=", doFrameResult);
        int guard = 0;
        while (api.toBufferRect(smk, buf->m_surfaceType) && guard++ < 64)
            fprintf(out, "[%d,%d,%d,%d]", smk->m_lastRectX, smk->m_lastRectY, smk->m_lastRectWidth, smk->m_lastRectHeight);
        fprintf(out, " pal=%016llx pic=%016llx", static_cast<unsigned long long>(Fnv(smk->m_palette, 768)),
                static_cast<unsigned long long>(Fnv(picture, pictureBytes)));
        uint32_t audioBytes = api.getTrackData ? api.getTrackData(smk, audio.data(), 0x2000) : 0;
        fprintf(out, " audio=%u:%016llx", audioBytes, static_cast<unsigned long long>(Fnv(audio.data(), audioBytes <= audio.size() ? audioBytes : 0)));
        fprintf(out, " after=%u/%u\n", smk->m_frameNumber, smk->m_newPalette != 0);
        if (static_cast<int>(f) == dumpFrame && dumpPath) {
            if (FILE* d = fopen(dumpPath, "wb")) {
                fwrite(picture, 1, pictureBytes, d);
                fwrite(smk->m_palette, 1, 768, d);
                if (audioBytes) fwrite(audio.data(), 1, audioBytes, d);
                fclose(d);
            }
        }
        if (f + 1 < frames) {
            api.nextFrame(smk);
            if (f == 0) fprintf(out, "wait-after-next=%u\n", api.wait(smk) != 0);
        }
    }
    // looping as the game does: jump back to frame 1 and decode it again
    api.gotoFrame(smk, 1);
    fprintf(out, "goto1 number=%u newpal=%u", smk->m_frameNumber, smk->m_newPalette != 0);
    api.doFrame(smk);
    fprintf(out, " pal=%016llx pic=%016llx\n", static_cast<unsigned long long>(Fnv(smk->m_palette, 768)),
            static_cast<unsigned long long>(Fnv(picture, pictureBytes)));
    api.bufferClose(buf);
    api.close(smk);
    return true;
}
