#include "gfx/rle8_decoder.h"

#include <algorithm>
#include <limits>

bool DecodeRle8(std::span<const uint8_t> encoded, int width, int height, bool topDown,
                std::vector<uint8_t>& pixels) {
    pixels.clear();
    if (width <= 0 || height <= 0 || static_cast<size_t>(width) >
        std::numeric_limits<size_t>::max() / static_cast<size_t>(height)) return false;
    if (static_cast<size_t>(width) * static_cast<size_t>(height) > 256u * 1024u * 1024u) return false;
    pixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
    size_t offset = 0;
    int x = 0;
    int y = 0; // stream order: bottom-to-top for positive-height DIBs
    bool ended = false;
    const auto put = [&](int px, int py, uint8_t value) {
        const int visualY = topDown ? py : height - py - 1;
        pixels[static_cast<size_t>(visualY) * width + px] = value;
    };
    while (offset < encoded.size()) {
        if (encoded.size() - offset < 2) { pixels.clear(); return false; }
        const uint8_t count = encoded[offset++];
        const uint8_t value = encoded[offset++];
        if (count) {
            if (y >= height || count > width - x) { pixels.clear(); return false; }
            for (int i = 0; i < count; ++i) put(x++, y, value);
            continue;
        }
        if (value == 0) { // end of line
            if (y >= height) { pixels.clear(); return false; }
            x = 0;
            ++y;
        } else if (value == 1) { // end of bitmap
            ended = true;
            break;
        } else if (value == 2) { // delta
            if (encoded.size() - offset < 2) { pixels.clear(); return false; }
            const int dx = encoded[offset++];
            const int dy = encoded[offset++];
            if (x + dx > width || y + dy >= height) {
                pixels.clear(); return false;
            }
            x += dx;
            y += dy;
        } else { // absolute run, padded to a word boundary
            const size_t literalCount = value;
            const size_t paddedCount = literalCount + (literalCount & 1);
            if (y >= height || literalCount > static_cast<size_t>(width - x) ||
                paddedCount > encoded.size() - offset) { pixels.clear(); return false; }
            for (size_t i = 0; i < literalCount; ++i) put(x++, y, encoded[offset + i]);
            offset += paddedCount;
        }
    }
    if (!ended) pixels.clear();
    return ended;
}
