#include <cerrno>
#include <cstdlib>
#include <cctype>

#include "iostream/iostream.h"

using iostream_access::IosOf;

namespace {

constexpr int kRangeError = 34;  // ERANGE
constexpr int kMaxNumberDigits = 15;

void UnlockBufferAndStream(ios* stream) {
    iostream_access::UnlockBuffer(stream->m_bp);
    iostream_access::Unlock(stream);
}

template <class Seek>
void SeekWithFailCheck(ios* stream, Seek seek) {
    iostream_access::LockBuffer(stream->m_bp);
    if (seek(stream->m_bp) == -1) iostream_access::SetState(stream, iostream_flags::kFail);
    iostream_access::UnlockBuffer(stream->m_bp);
}

int NumberBase(unsigned flags) {
    if ((flags & iostream_flags::kDec) != 0) return 10;
    if ((flags & iostream_flags::kHex) != 0) return 16;
    return static_cast<int>((static_cast<int>(static_cast<signed char>(flags)) & 0x20u) >> 2);
}

// The extraction operators read their digits with getint(), convert them with strtol/strtoul and set failbit on overflow.
template <class Convert>
istream* ExtractNumber(istream* in, Convert convert) {
    if (istream_ipfx(in, 0) != 0) {
        char digits[16];
        const int base = istream_getint(in, digits);
        ios* stream = IosOf(in);
        convert(stream, digits, base);
        UnlockBufferAndStream(stream);
    }
    return in;
}

}  // namespace

istream* istream_operator_rs_streambuf(istream* in, streambuf* target) {
    if (istream_ipfx(in, 0) != 0) {
        ios* stream = IosOf(in);
        int c = streambuf_sbumpc(stream->m_bp);
        while (c != -1) {
            if (target->m_pptr < target->m_epptr) {
                *target->m_pptr++ = static_cast<char>(c);
                c &= 0xff;
            } else {
                c = target->Overflow(c);
            }
            if (c == -1) stream->m_state |= iostream_flags::kFail;
            c = streambuf_sbumpc(stream->m_bp);
        }
        UnlockBufferAndStream(stream);
    }
    return in;
}

istream* istream_seekg_pos(istream* in, long position) {
    SeekWithFailCheck(IosOf(in), [&](streambuf* buffer) { return buffer->SeekPosition(position, iostream_flags::kIn); });
    return in;
}

istream* istream_seekg_off(istream* in, long offset, int dir) {
    SeekWithFailCheck(IosOf(in), [&](streambuf* buffer) { return buffer->SeekOffset(offset, dir, iostream_flags::kIn); });
    return in;
}

long istream_tellg(istream* in) {
    long result = 0;
    SeekWithFailCheck(IosOf(in), [&](streambuf* buffer) {
        result = buffer->SeekOffset(0, iostream_flags::kSeekCur, iostream_flags::kIn);
        return result;
    });
    return result;
}

int istream_get(istream* in) {
    if (istream_ipfx(in, 1) != 0) {
        ios* stream = IosOf(in);
        const int c = streambuf_sbumpc(stream->m_bp);
        if (c == -1)
            stream->m_state |= iostream_flags::kEof;
        else
            ++in->m_gcount;
        UnlockBufferAndStream(stream);
        return c;
    }
    return -1;
}

istream* istream_read(istream* in, void* buffer, int count) {
    if (istream_ipfx(in, 1) != 0) {
        ios* stream = IosOf(in);
        streambuf* source = stream->m_bp;
        const unsigned got = static_cast<unsigned>(source->Read(static_cast<char*>(buffer), count));
        in->m_gcount = static_cast<int>(got);
        if (got < static_cast<unsigned>(count)) stream->m_state |= iostream_flags::kFail | iostream_flags::kEof;
        UnlockBufferAndStream(stream);
    }
    return in;
}

istream* istream_operator_rs_long(istream* in, int* value) {
    return ExtractNumber(in, [&](ios* stream, const char* digits, int base) {
        *value = static_cast<int>(strtol(digits, nullptr, base));
        if (errno == kRangeError) stream->m_state |= iostream_flags::kFail;
    });
}

istream* istream_operator_rs_ushort(istream* in, unsigned short* value) {
    return ExtractNumber(in, [&](ios* stream, const char* digits, int base) {
        const unsigned long converted = strtoul(digits, nullptr, base);
        if ((converted < 0x10000 || 0xffff7fff < converted) && (converted != 0xffffffff || errno != kRangeError)) {
            *value = static_cast<unsigned short>(converted);
        } else {
            *value = 0xffff;
            stream->m_state |= iostream_flags::kFail;
        }
    });
}

istream* istream_operator_rs_ulong(istream* in, unsigned int* value) {
    return ExtractNumber(in, [&](ios* stream, const char* digits, int base) {
        *value = static_cast<unsigned int>(strtoul(digits, nullptr, base));
        if (*value == 0xffffffffu && errno == kRangeError) stream->m_state |= iostream_flags::kFail;
    });
}

istream* istream_getline(istream* in, char* buffer, int limit, int delimiter) {
    unsigned length = 0;
    if (istream_ipfx(in, 1) != 0) {
        ios* stream = IosOf(in);
        if (limit != 0) {
            if (limit != 1) {
                do {
                    const int c = streambuf_sgetc(stream->m_bp);
                    if (c == -1) {
                        stream->m_state |= iostream_flags::kEof;
                        if (length == 0) stream->m_state |= iostream_flags::kFail;
                        break;
                    }
                    if (c == delimiter) {
                        if (in->m_fGline != 0) {
                            ++in->m_gcount;
                            streambuf_stossc(stream->m_bp);
                        }
                        break;
                    }
                    if (buffer != nullptr) buffer[length] = static_cast<char>(c);
                    streambuf_stossc(stream->m_bp);
                    ++length;
                } while (length < static_cast<unsigned>(limit) - 1u);
            }
            in->m_gcount += static_cast<int>(length);
        }
        UnlockBufferAndStream(stream);
    }
    if (buffer != nullptr && limit != 0) buffer[length] = 0;
    in->m_fGline = 0;
    return in;
}

istream* istream_operator_rs_short(istream* in, short* value) {
    return ExtractNumber(in, [&](ios* stream, const char* digits, int base) {
        const long converted = strtol(digits, nullptr, base);
        if (converted < 0x8000) {
            if (-0x8001 < converted) {
                *value = static_cast<short>(converted);
                return;
            }
            *value = static_cast<short>(0x8000);
        } else {
            *value = 0x7fff;
        }
        stream->m_state |= iostream_flags::kFail;
    });
}

int istream_ipfx(istream* in, int need) {
    ios* stream = IosOf(in);
    iostream_access::Lock(stream);
    if (need != 0) in->m_gcount = 0;
    const int state = stream->m_state;
    if (state != 0) {
        stream->m_state = state | iostream_flags::kFail;
        iostream_access::Unlock(stream);
        return 0;
    }
    if (stream->m_tie != nullptr) {
        bool flush = true;
        if (need != 0) {
            const streambuf* buffer = stream->m_bp;
            const int available = buffer->m_gptr < buffer->m_egptr ? static_cast<int>(buffer->m_egptr - buffer->m_gptr) : 0;
            flush = available < need;
        }
        if (flush) ostream_flush(stream->m_tie);
    }
    iostream_access::LockBuffer(stream->m_bp);
    if (need == 0 && (stream->m_flags & iostream_flags::kSkipWs) != 0) {
        istream_eatwhite(in);
        const int after = stream->m_state;
        if (after != 0) {
            stream->m_state = after | iostream_flags::kFail;
            iostream_access::UnlockBuffer(stream->m_bp);
            iostream_access::Unlock(stream);
            return 0;
        }
    }
    return 1;
}

istream* istream_operator_rs_charptr(istream* in, char* value) {
    if (istream_ipfx(in, 0) != 0) {
        ios* stream = IosOf(in);
        const int width = stream->m_width;
        stream->m_width = 0;
        if (value == nullptr) {
            stream->m_state |= iostream_flags::kFail;
        } else {
            unsigned length = 0;
            bool terminate = false;
            if (width != 1) {
                do {
                    const int c = streambuf_sgetc(stream->m_bp);
                    if (c == -1) {
                        stream->m_state |= iostream_flags::kEof;
                        if (length != 0) {
                            terminate = true;
                            break;
                        }
                        stream->m_state |= iostream_flags::kFail | iostream_flags::kBad;
                        break;
                    }
                    if (isspace(c) != 0) break;
                    value[length] = static_cast<char>(c);
                    streambuf_stossc(stream->m_bp);
                    ++length;
                } while (length < static_cast<unsigned>(width) - 1u);
            }
            if (length != 0 || terminate)
                value[length] = 0;
            else
                stream->m_state |= iostream_flags::kFail;
        }
        UnlockBufferAndStream(stream);
    }
    return in;
}

void istream_eatwhite(istream* in) {
    ios* stream = IosOf(in);
    iostream_access::LockBuffer(stream->m_bp);
    int c = streambuf_sgetc(stream->m_bp);
    while (c != -1) {
        if (isspace(c) == 0) {
            iostream_access::UnlockBuffer(stream->m_bp);
            return;
        }
        c = streambuf_snextc(stream->m_bp);
    }
    iostream_access::SetState(stream, iostream_flags::kEof);
    iostream_access::UnlockBuffer(stream->m_bp);
}

int istream_getint(istream* in, char* buffer) {
    int length = 0;
    int digits = 0;
    int signEnd = 1;
    ios* stream = IosOf(in);
    int base = NumberBase(stream->m_flags);
    if (istream_ipfx(in, 0) != 0) {
        int c = streambuf_sgetc(stream->m_bp);
        bool stopped = false;
        while (c != -1) {
            if (length == 0 && (c == '-' || c == '+')) {
                ++signEnd;
            } else {
                bool hexPrefix = false;
                if (length == signEnd && buffer[length - 1] == '0') {
                    if ((c == 'x' || c == 'X') && (base == 0 || base == 16)) {
                        base = 16;
                        digits = 0;
                        hexPrefix = true;
                    } else if (base == 0) {
                        base = 8;
                    }
                }
                if (!hexPrefix) {
                    if (base == 16) {
                        if (isxdigit(c) == 0) {
                            stopped = true;
                            break;
                        }
                    } else if (isdigit(c) == 0 || (base == 8 && 0x37 < c)) {
                        stopped = true;
                        break;
                    }
                    ++digits;
                }
            }
            buffer[length] = static_cast<char>(c);
            c = streambuf_snextc(stream->m_bp);
            ++length;
            if (kMaxNumberDigits - 1 < length) {
                stopped = true;
                break;
            }
        }
        if (!stopped) stream->m_state |= iostream_flags::kEof;
        if (digits == 0) {
            stream->m_state |= iostream_flags::kFail;
            while (length != 0) {
                --length;
                const char ch = buffer[length];
                streambuf* source = stream->m_bp;
                int pushed;
                if (source->m_eback < source->m_gptr) {
                    *--source->m_gptr = ch;
                    pushed = ch;
                } else {
                    pushed = source->Putback(ch);
                }
                if (pushed == -1) {
                    stream->m_state |= iostream_flags::kBad;
                    break;
                }
                stream->m_state &= ~iostream_flags::kEof;
            }
            length = 0;
        }
        buffer[length] = 0;
        UnlockBufferAndStream(stream);
    }
    if (length == kMaxNumberDigits) stream->m_state |= iostream_flags::kFail;
    return base;
}
