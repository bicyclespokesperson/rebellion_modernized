#pragma once
#include <cstdint>

#include "core/layout.h"
#include "core/stream_io.h"
#include "platform/win32.h"

// Result codes of the WAVE parsers (0 = ok).
enum WaveParseError : uint32_t {
    kWaveBadRiff = 0xe003,
    kWaveBadFormatChunk = 0xe006,
    kWaveBadFormatOrData = 0xe007,
    kWaveNoSource = 0xe008,
    kWaveStreamClosed = 0xe00b,
};

// Format and data location of a RIFF/WAVE (PCM only), read either from an open file or from memory.
class WaveInfo {
public:
    WaveInfo();
    virtual ~WaveInfo();

    void Reset();

    // Copies `count` bytes from *source to `destination` and advances *source; does nothing for null pointers
    // or a zero count.
    void ReadAdvance(void* destination, const uint8_t** source, uint32_t count);

    // File reader: parses "RIFF....WAVEfmt " and the format fields, then locates the data chunk.
    uint32_t ParseStream(istream* in);
    // Searches the stream for the "data" chunk, starting `base` bytes into it. Returns 1 and leaves the stream at
    // the chunk header if found; otherwise 0, with m_dataSize and m_dataOffset zeroed.
    int FindDataChunk(istream* in, long base);

    // Memory reader: the same for an in-memory image.
    uint32_t ParseMemory(const void* image);
    // *position points into the image; same result convention as FindDataChunk.
    int FindDataChunkInMemory(const uint8_t** position, int unused);

    WAVEFORMATEX m_format;   // +0x04 (packed, 0x12 bytes)
    uint8_t m_pad16[2];      // +0x16
    uint32_t m_dataSize;     // +0x18 length of the "data" chunk
    uint32_t m_dataOffset;   // +0x1c offset of the "data" chunk header from the start of the RIFF image
};
REB_ASSERT_SIZE(WaveInfo, 0x20);
REB_ASSERT_OFFSET(WaveInfo, m_format, 0x04);
REB_ASSERT_OFFSET(WaveInfo, m_dataSize, 0x18);
REB_ASSERT_OFFSET(WaveInfo, m_dataOffset, 0x1c);
