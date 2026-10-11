#pragma once
#include <cstdint>

#include "core/stream_io.h"
#include "iostream/iostream.h"

// Statically linked stream entry points used by the audio system.

namespace stream_access {

constexpr int kSeekBegin = 0;
constexpr int kSeekCurrent = 1;

template <typename Stream>
inline bool IsOpen(Stream* stream) {
    return static_cast<const filebuf*>(iostream_access::IosOf(stream)->m_bp)->m_fd != -1;
}

template <typename Stream>
inline bool AtEof(Stream* stream) {
    return (iostream_access::IosOf(stream)->m_state & iostream_flags::kEof) != 0;
}

inline uint32_t LastReadCount(istream* in) { return static_cast<uint32_t>(in->m_gcount); }

inline void Read(istream* in, void* buffer, uint32_t count) { istream_read(in, buffer, static_cast<int>(count)); }

}  // namespace stream_access
