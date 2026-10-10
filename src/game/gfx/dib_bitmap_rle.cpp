// DibBitmap blitters and hit test for RLE8-compressed sources (BITMAPINFOHEADER compression 1).
// The stream is read as 16-bit opcodes: 00 00 ends a line, 00 01 ends the bitmap, 00 02 dx dy moves the
// pen, 00 n (n >= 3) is followed by n literal pixels padded to an even length, n v repeats v n times.
#include <cstring>

#include "gfx/dib_bitmap.h"

namespace {
constexpr uint16_t kRleEndOfLine = 0x0000;
constexpr uint16_t kRleEndOfBitmap = 0x0100;
constexpr uint16_t kRleDelta = 0x0200;

uint16_t RleOpcode(const uint8_t* stream) { return static_cast<uint16_t>(stream[0] | (stream[1] << 8)); }

int AbsoluteHeight(int signedHeight) { return signedHeight < 0 ? -signedHeight : signedHeight; }

// BUG(original): a negative `available` is passed on and later used as an unsigned length.
int ClippedLength(uint8_t count, int available) { return count > available ? available : count; }

// Fast-forwards over the stream lines that precede `limitRow`, moving `row` by `direction` per line.
// Returns false when the stream ends first.
// BUG(original): the stream is stepped two bytes at a time, so run payloads are decoded as opcodes.
bool SkipRleLines(const uint8_t*& stream, int& row, int direction, int limitRow) {
    while (direction < 0 ? row > limitRow : row < limitRow) {
        switch (RleOpcode(stream)) {
            case kRleEndOfLine:
                row += direction;
                break;
            case kRleEndOfBitmap:
                return false;
            case kRleDelta:
                stream += 2;
                row += direction * stream[1];
                break;
        }
        stream += 2;
    }
    return true;
}

enum class RunPaint { kCopy, kKeyed, kRemapped };

// Decodes RLE8 lines into an uncompressed 8-bit destination, clipped horizontally to [clipLeft, clipRight).
struct RleLineDecoder {
    DibBitmap* dest;
    uint8_t* destBits;
    int destStride;
    int xOrigin;  // destination column of source column 0
    int clipLeft;
    int clipRight;
    int lengthLimit;  // runs are cut to `lengthLimit - x` pixels
    bool skipEncodedRunTwice;
    RunPaint paint;
    uint8_t colorKey;
    const DibBitmap* source;  // owner of the remap table

    uint8_t* PixelAt(int row, int x) const {
        return destBits + static_cast<ptrdiff_t>(dest->RowIndex(row)) * destStride + x;
    }

    void FillRun(uint8_t* out, uint8_t value, int length) const {
        switch (paint) {
            case RunPaint::kCopy:
                std::memset(out, value, static_cast<size_t>(length));
                break;
            case RunPaint::kKeyed:
                if (value != colorKey) std::memset(out, value, static_cast<size_t>(length));
                break;
            case RunPaint::kRemapped:
                if (value != colorKey) std::memset(out, source->m_remapTable[value], static_cast<size_t>(length));
                break;
        }
    }

    void CopyRun(uint8_t* out, const uint8_t* pixels, int length) const {
        if (paint == RunPaint::kCopy) {
            std::memcpy(out, pixels, static_cast<size_t>(length));
            return;
        }
        for (uint32_t remaining = static_cast<uint32_t>(length); remaining != 0; --remaining) {
            const uint8_t pixel = *pixels++;
            if (pixel != colorKey) *out = paint == RunPaint::kRemapped ? source->m_remapTable[pixel] : pixel;
            ++out;
        }
    }

    // Moves a run that starts left of the clip region onto its edge; returns the pixels left to draw.
    uint8_t ClipRunLeft(uint8_t count, int& x, uint8_t*& out, const uint8_t** pixels) const {
        if (x >= clipLeft) return count;
        const int hidden = clipLeft - x;
        if (count > hidden) {
            count = static_cast<uint8_t>(count + x - clipLeft);
            out += hidden;
            if (pixels) *pixels += hidden;
            x = clipLeft;
            return count;
        }
        x += count;
        out += count;
        if (pixels) *pixels += count;
        return 0;
    }

    const uint8_t* DrawEncodedRun(const uint8_t* stream, int& x, uint8_t*& out) const {
        const uint8_t* value = stream + 1;
        const uint8_t* next = value + 1;
        if (x < clipRight) {
            const uint8_t count = ClipRunLeft(stream[0], x, out, nullptr);
            if (count != 0) {
                FillRun(out, *value, ClippedLength(count, lengthLimit - x));
                out += count;
                x += count;
                // BUG(original): the keyed top-down decoders also skip `count` stream bytes after a drawn run.
                if (skipEncodedRunTwice) next += count;
            }
        }
        return next;
    }

    const uint8_t* DrawAbsoluteRun(const uint8_t* stream, int& x, uint8_t*& out) const {
        const uint8_t length = stream[1];
        const uint8_t* pixels = stream + 2;
        // BUG(original): a run starting at or right of the clip edge leaves its pixels in the stream.
        if (x < clipRight) {
            const uint8_t count = ClipRunLeft(length, x, out, &pixels);
            if (count != 0) {
                CopyRun(out, pixels, ClippedLength(count, lengthLimit - x));
                out += count;
                pixels += count;
                x += count;
            }
        }
        if (length & 1) ++pixels;
        return pixels;
    }

    // Decodes lines while `row` (counting down) stays above `stopRow`.
    void DrawLines(const uint8_t* stream, int row, int stopRow) const {
        int x = xOrigin;
        uint8_t* out = PixelAt(row, x);
        while (row > stopRow) {
            const uint16_t opcode = RleOpcode(stream);
            if (opcode == kRleEndOfLine) {
                x = xOrigin;
                --row;
                out = PixelAt(row, x);
                stream += 2;
                continue;
            }
            if (opcode == kRleEndOfBitmap) {
                row = stopRow;
                continue;
            }
            if (opcode == kRleDelta) {
                row -= stream[3];
                x += stream[2];
                out = PixelAt(row, x);
                stream += 4;
            }
            stream = stream[0] != 0 ? DrawEncodedRun(stream, x, out) : DrawAbsoluteRun(stream, x, out);
        }
    }
};

// Shared body of the three RLE blitters; `decoder` arrives with its paint mode, colour key and source set.
int BlitRleLines(const DibBitmap* source, RleLineDecoder& decoder, DibBitmap* dest, int destX, int destY, int srcX,
                 int srcY, int width, int height, bool clipLengthToWidth) {
    const int srcWidth = source->Width();
    decoder.dest = dest;
    decoder.destBits = dest->m_bits;
    const uint8_t* stream = source->m_bits;
    const int destHeight = AbsoluteHeight(dest->Height());
    const int destWidth = dest->Width();
    decoder.destStride = dest->RowStride();
    const int srcSignedHeight = source->Height();
    const int srcHeight = AbsoluteHeight(srcSignedHeight);

    if (width == 0) width = destWidth - destX;
    if (height == 0) height = destHeight - destY;
    if (height > srcHeight - srcY) height = srcHeight - srcY;
    if (width > srcWidth - srcX) width = srcWidth - srcX;
    if (height <= 0) return 0;

    decoder.xOrigin = destX - srcX;
    decoder.clipLeft = destX;
    decoder.clipRight = destX + width;
    // BUG(original): BlitRle cuts runs at `width - x` instead of `destX + width - x`.
    decoder.lengthLimit = clipLengthToWidth ? width : decoder.clipRight;

    if (srcSignedHeight >= 0) {
        int row = srcHeight + destY - srcY - 1;
        if (!SkipRleLines(stream, row, -1, height + destY - 1)) return 0;
        decoder.DrawLines(stream, row, destY - 1);
    } else {
        // BUG(original): top-down sources are skipped forwards but decoded backwards with a stop row below
        // the clip region, so almost nothing is drawn.
        int row = destY - srcY;
        if (!SkipRleLines(stream, row, 1, destY)) return 0;
        decoder.skipEncodedRunTwice = decoder.paint != RunPaint::kCopy;
        decoder.DrawLines(stream, row, height + destY);
    }
    dest->CacheTransparentColor();
    return 1;
}

RleLineDecoder MakeDecoder(RunPaint paint, uint8_t colorKey, const DibBitmap* source) {
    RleLineDecoder decoder = {};
    decoder.paint = paint;
    decoder.colorKey = colorKey;
    decoder.source = source;
    return decoder;
}
}  // namespace

int DibBitmap::BlitRle(DibBitmap* dest, int destX, int destY, int srcX, int srcY, int width, int height) {
    RleLineDecoder decoder = MakeDecoder(RunPaint::kCopy, 0, this);
    return BlitRleLines(this, decoder, dest, destX, destY, srcX, srcY, width, height, true);
}

int DibBitmap::BlitKeyedRle(DibBitmap* dest, int destX, int destY, uint8_t colorKey, int srcX, int srcY, int width,
                            int height) {
    RleLineDecoder decoder = MakeDecoder(RunPaint::kKeyed, colorKey, this);
    return BlitRleLines(this, decoder, dest, destX, destY, srcX, srcY, width, height, false);
}

int DibBitmap::BlitRemappedRle(DibBitmap* dest, int destX, int destY, uint8_t colorKey, int srcX, int srcY,
                               int width, int height) {
    RleLineDecoder decoder = MakeDecoder(RunPaint::kRemapped, colorKey, this);
    return BlitRleLines(this, decoder, dest, destX, destY, srcX, srcY, width, height, false);
}

int DibBitmap::IsPixelOpaqueRle(int x, uint32_t y) {
    int opaque = 0;
    const int signedHeight = Height();
    const int direction = signedHeight > 0 ? -1 : 1;
    int row = direction == -1 ? signedHeight - 1 : 0;
    const uint8_t* stream = m_bits;
    const int targetRow = static_cast<int>(y);
    // BUG(original): column 0 and row 0 are never reported as opaque.
    if (targetRow >= AbsoluteHeight(signedHeight) || targetRow <= 0) return opaque;
    if (x >= Width() || x <= 0) return opaque;

    while (row != targetRow) {
        switch (RleOpcode(stream)) {
            case kRleEndOfLine:
                row += direction;
                break;
            case kRleEndOfBitmap:
                return false;
            case kRleDelta:
                stream += 2;
                row += stream[1] * direction;
                break;
            default:
                if (stream[0] == 0) stream += ((stream[1] + 1) >> 1) * 2;
                break;
        }
        stream += 2;
    }

    // BUG(original): the scan stops before column x, so the pixel left of x decides.
    int column = 0;
    while (column < x) {
        const uint16_t opcode = RleOpcode(stream);
        if (opcode == kRleEndOfLine || opcode == kRleEndOfBitmap) {
            column = x;
        } else if (opcode == kRleDelta) {
            // BUG(original): a delta that stays on the line only skips its opcode, not its operands.
            stream += 2;
            column += stream[0];
            if (stream[1] != 0) column = x;
        } else if (stream[0] != 0) {
            opaque = stream[1] != m_colorKey;
            column += stream[0];
            stream += 2;
        } else {
            const uint8_t length = stream[1];
            stream += 2;
            for (uint8_t left = length; left != 0 && column < x; --left) {
                opaque = *stream++ != m_colorKey;
                ++column;
            }
            if (length & 1) ++stream;
        }
    }
    return opaque;
}
