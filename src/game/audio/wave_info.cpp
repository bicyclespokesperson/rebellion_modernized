#include "audio/wave_info.h"

#include <cstring>

#include "audio/stream_access.h"

using namespace stream_access;

namespace {

constexpr uint32_t kFourCc(char a, char b, char c, char d) {
    return static_cast<uint32_t>(static_cast<uint8_t>(a)) | static_cast<uint32_t>(static_cast<uint8_t>(b)) << 8 |
           static_cast<uint32_t>(static_cast<uint8_t>(c)) << 16 | static_cast<uint32_t>(static_cast<uint8_t>(d)) << 24;
}

constexpr uint32_t kRiff = kFourCc('R', 'I', 'F', 'F');
constexpr uint32_t kWave = kFourCc('W', 'A', 'V', 'E');
constexpr uint32_t kFmt = kFourCc('f', 'm', 't', ' ');
constexpr uint32_t kData = kFourCc('d', 'a', 't', 'a');

constexpr uint32_t kRiffHeaderBytes = 8;
constexpr uint32_t kWaveTagBytes = 4;
constexpr uint32_t kChunkHeaderBytes = 8;
constexpr uint32_t kFirstChunkOffset = 0xc;
constexpr uint16_t kPcmFormat = 1;

// The tag buffer is zeroed once and then reused by every read, so a short read leaves the previous tag's bytes.
struct TagBuffer {
    uint32_t value = 0;
    bool Is(uint32_t tag) const { return value == tag; }
};

}  // namespace

WaveInfo::WaveInfo() { Reset(); }

WaveInfo::~WaveInfo() {  }

void WaveInfo::Reset() {
    m_format.wFormatTag = 0;
    m_format.nChannels = 0;
    m_format.nSamplesPerSec = 0;
    m_format.nAvgBytesPerSec = 0;
    m_format.nBlockAlign = 0;
    m_format.wBitsPerSample = 0;
    m_format.cbSize = static_cast<WORD>(sizeof(WAVEFORMATEX));
    m_dataSize = 0;
    m_dataOffset = 0;
}

void WaveInfo::ReadAdvance(void* destination, const uint8_t** source, uint32_t count) {
    if (!destination) return;
    if (*source == nullptr || count == 0) return;
    memcpy(destination, *source, count);
    *source += count;
}

uint32_t WaveInfo::ParseStream(istream* in) {
    if (!IsOpen(in)) return kWaveStreamClosed;
    TagBuffer tag;
    uint32_t chunkSize = 0;
    m_dataOffset = 0;
    Read(in, &tag.value, 4);
    if (!tag.Is(kRiff)) return kWaveBadRiff;
    Read(in, &chunkSize, 4);
    m_dataOffset += kRiffHeaderBytes;
    Read(in, &tag.value, 4);
    if (!tag.Is(kWave)) return kWaveBadRiff;
    m_dataOffset += kWaveTagBytes;
    Read(in, &tag.value, 4);
    if (!tag.Is(kFmt)) return kWaveBadFormatChunk;
    Read(in, &chunkSize, 4);
    m_dataOffset += chunkSize + kChunkHeaderBytes;
    Read(in, &m_format.wFormatTag, 2);
    Read(in, &m_format.nChannels, 2);
    Read(in, &m_format.nSamplesPerSec, 4);
    Read(in, &m_format.nAvgBytesPerSec, 4);
    Read(in, &m_format.nBlockAlign, 2);
    Read(in, &m_format.wBitsPerSample, 2);
    if (m_format.wFormatTag == kPcmFormat && FindDataChunk(in, 0)) return 0;
    return kWaveBadFormatOrData;
}

int WaveInfo::FindDataChunk(istream* in, long base) {
    int found = 0;
    if (IsOpen(in)) {
        TagBuffer tag;
        uint32_t chunkSize = 0;
        istream_seekg_off(in, base + static_cast<long>(m_dataOffset), kSeekBegin);
        Read(in, &tag.value, 4);
        bool atData = tag.Is(kData);
        if (!atData) {
            m_dataOffset = kFirstChunkOffset;
            istream_seekg_off(in, base + kFirstChunkOffset, kSeekBegin);
            while (!AtEof(in)) {
                Read(in, &tag.value, 4);
                if (tag.Is(kData)) {
                    atData = true;
                    break;
                }
                Read(in, &chunkSize, 4);
                istream_seekg_off(in, static_cast<long>(chunkSize), kSeekCurrent);
                m_dataOffset += chunkSize + 4;
            }
        }
        if (atData) {
            Read(in, &m_dataSize, 4);
            istream_seekg_off(in, -static_cast<long>(kChunkHeaderBytes), kSeekCurrent);
            found = 1;
        }
    }
    if (found != 1) {
        m_dataOffset = 0;
        m_dataSize = 0;
    }
    return found;
}

uint32_t WaveInfo::ParseMemory(const void* image) {
    if (!image) return kWaveNoSource;
    const uint8_t* cursor = static_cast<const uint8_t*>(image);
    TagBuffer tag;
    uint32_t chunkSize = 0;
    m_dataOffset = 0;
    ReadAdvance(&tag.value, &cursor, 4);
    if (!tag.Is(kRiff)) return kWaveBadRiff;
    ReadAdvance(&chunkSize, &cursor, 4);
    m_dataOffset += kRiffHeaderBytes;
    ReadAdvance(&tag.value, &cursor, 4);
    if (!tag.Is(kWave)) return kWaveBadRiff;
    m_dataOffset += kWaveTagBytes;
    ReadAdvance(&tag.value, &cursor, 4);
    if (!tag.Is(kFmt)) return kWaveBadFormatChunk;
    ReadAdvance(&chunkSize, &cursor, 4);
    m_dataOffset += chunkSize + kChunkHeaderBytes;
    ReadAdvance(&m_format.wFormatTag, &cursor, 2);
    ReadAdvance(&m_format.nChannels, &cursor, 2);
    ReadAdvance(&m_format.nSamplesPerSec, &cursor, 4);
    ReadAdvance(&m_format.nAvgBytesPerSec, &cursor, 4);
    ReadAdvance(&m_format.nBlockAlign, &cursor, 2);
    ReadAdvance(&m_format.wBitsPerSample, &cursor, 2);
    cursor = static_cast<const uint8_t*>(image);
    if (m_format.wFormatTag == kPcmFormat && FindDataChunkInMemory(&cursor, 0)) return 0;
    return kWaveBadFormatOrData;
}

int WaveInfo::FindDataChunkInMemory(const uint8_t** position, int) {
    int found = 0;
    const uint8_t* const image = *position;
    if (image) {
        TagBuffer tag;
        uint32_t riffSize = 0;
        *position = image + 4;
        ReadAdvance(&riffSize, position, 4);
        *position = image + m_dataOffset;
        riffSize += kRiffHeaderBytes;
        ReadAdvance(&tag.value, position, 4);
        if (tag.Is(kData)) {
            ReadAdvance(&m_dataSize, position, 4);
            found = 1;
            *position -= kChunkHeaderBytes;
        } else {
            uint32_t chunkSize = 0;
            const uint8_t* const end = image + riffSize;
            *position = image + kFirstChunkOffset;
            m_dataOffset = kFirstChunkOffset;
            while (*position < end) {
                ReadAdvance(&tag.value, position, 4);
                if (tag.Is(kData)) {
                    ReadAdvance(&m_dataSize, position, 4);
                    found = 1;
                    *position -= kChunkHeaderBytes;
                    break;
                }
                ReadAdvance(&chunkSize, position, 4);
                *position += chunkSize;
                m_dataOffset += chunkSize + 4;
            }
        }
    }
    if (found != 1) {
        m_dataOffset = 0;
        m_dataSize = 0;
    }
    return found;
}
