#include <cstdio>
#include <cstring>

#include "iostream/iostream.h"

using iostream_access::IosOf;

// The empty string the library hands to outstr as the prefix of operator<<(const char*) and operator<<(char).
extern const char *g_emptyText;  // 0x6b120c

namespace {

constexpr uintptr_t kStdoutFile = 0x006acca8;  // _iob[1]
constexpr uintptr_t kStderrFile = 0x006accc8;  // _iob[2]

// streambuf::sputc as the library inlines it.
int Sputc(streambuf* buffer, unsigned char c) {
    if (buffer->m_pptr < buffer->m_epptr) {
        *buffer->m_pptr++ = static_cast<char>(c);
        return c;
    }
    return buffer->Overflow(c);
}

void PutFill(ios* stream) {
    if (Sputc(stream->m_bp, static_cast<unsigned char>(stream->m_fill)) == -1) stream->m_state |= iostream_flags::kFail | iostream_flags::kBad;
}

void PutPadding(ios* stream, int count) {
    for (; count > 0; --count) PutFill(stream);
}

void PutText(ios* stream, const char* text, int length) {
    streambuf* buffer = stream->m_bp;
    if (buffer->Write(text, length) != length) stream->m_state |= iostream_flags::kFail | iostream_flags::kBad;
}

void UnlockBufferAndStream(ios* stream) {
    iostream_access::UnlockBuffer(stream->m_bp);
    iostream_access::Unlock(stream);
}

// The ios::seekoff/seekpos wrappers all follow one pattern: take the buffer lock, ask the streambuf, set failbit if it said -1.
template <class Seek>
long SeekWithFailCheck(ios* stream, Seek seek) {
    iostream_access::LockBuffer(stream->m_bp);
    const long result = seek(stream->m_bp);
    if (result == -1) iostream_access::SetState(stream, iostream_flags::kFail);
    iostream_access::UnlockBuffer(stream->m_bp);
    return result;
}

// The leading characters of a formatted number: '+' for showpos, "0" / "0x" for showbase in octal / hex.
struct NumberFormat {
    char m_prefix[4] = {0, 0, 0, 0};
    char m_spec[4];
};

char HexLetter(unsigned flags) { return static_cast<char>(static_cast<unsigned char>(~(flags >> 4) & 0x20) | 0x58); }

constexpr char kLongSpec[4] = {'%', 'l', 'd', 0};
constexpr char kUnsignedLongSpec[4] = {'%', 'l', 'u', 0};
constexpr char kIntSpec[4] = {'%', 'd', 0, 0};

NumberFormat MakeFormat(ios* stream, const char (&decimalSpec)[4], bool isSigned, int value, int specLength) {
    NumberFormat format;
    memcpy(format.m_spec, decimalSpec, sizeof format.m_spec);
    if (value == 0) return format;
    const unsigned flags = stream->m_flags;
    if ((flags & (iostream_flags::kOct | iostream_flags::kHex)) == 0) {
        if ((!isSigned || 0 < value) && (flags & iostream_flags::kShowPos) != 0) format.m_prefix[0] = '+';
        return format;
    }
    if ((flags & iostream_flags::kHex) == 0) {
        format.m_spec[specLength - 1] = 'o';
    } else {
        const char letter = HexLetter(flags);
        format.m_spec[specLength - 1] = letter;
        format.m_prefix[1] = letter;
    }
    if ((stream->m_flags & iostream_flags::kShowBase) != 0) format.m_prefix[0] = '0';
    return format;
}

ostream* PrintNumber(ostream* out, const char (&decimalSpec)[4], bool isSigned, int value, int specLength) {
    if (ostream_opfx(out) != 0) {
        ios* stream = IosOf(out);
        const NumberFormat format = MakeFormat(stream, decimalSpec, isSigned, value, specLength);
        char text[12];
        sprintf(text, format.m_spec, value);
        ostream_outstr(out, format.m_prefix, text);
        ostream_osfx(out);
    }
    return out;
}

}  // namespace

ostream* ostream_operator_ls_char(ostream* out, char value) {
    if (ostream_opfx(out) != 0) {
        ios* stream = IosOf(out);
        if (stream->m_width != 0) {
            const char text[2] = {value, 0};
            ostream_outstr(out, g_emptyText, text);
            ostream_osfx(out);
            return out;
        }
        const unsigned char c = static_cast<unsigned char>(value);
        if (static_cast<unsigned>(Sputc(stream->m_bp, c)) == 0xffffffffu) {
            if (stream->m_bp->Overflow(c) == -1) stream->m_state |= iostream_flags::kFail | iostream_flags::kBad;
        }
        ostream_osfx(out);
    }
    return out;
}

int ostream_opfx(ostream* out) {
    ios* stream = IosOf(out);
    iostream_access::Lock(stream);
    const int state = stream->m_state;
    if (state != 0) {
        stream->m_state = state | iostream_flags::kFail;
        iostream_access::Unlock(stream);
        return 0;
    }
    if (stream->m_tie != nullptr) ostream_flush(stream->m_tie);
    iostream_access::LockBuffer(stream->m_bp);
    return 1;
}

void ostream_osfx(ostream* out) {
    ios* stream = IosOf(out);
    stream->m_width = 0;
    if ((stream->m_flags & iostream_flags::kUnitBuf) != 0) {
        if (stream->m_bp->Sync() == -1) stream->m_state = iostream_flags::kFail | iostream_flags::kBad;
    }
    if ((stream->m_flags & iostream_flags::kStdio) != 0) {
        if (fflush(reinterpret_cast<FILE*>(kStdoutFile)) == -1) stream->m_state |= iostream_flags::kFail;
        if (fflush(reinterpret_cast<FILE*>(kStderrFile)) == -1) stream->m_state |= iostream_flags::kFail;
    }
    UnlockBufferAndStream(stream);
}

ostream* ostream_operator_ls_cstr(ostream* out, const char* text) {
    if (ostream_opfx(out) != 0) {
        ostream_outstr(out, g_emptyText, text);
        ostream_osfx(out);
    }
    return out;
}

ostream* ostream_flush(ostream* out) {
    ios* stream = IosOf(out);
    iostream_access::Lock(stream);
    iostream_access::LockBuffer(stream->m_bp);
    if (stream->m_bp->Sync() == -1) stream->m_state |= iostream_flags::kFail;
    UnlockBufferAndStream(stream);
    return out;
}

void ostream_outstr(ostream* out, const char* prefix, const char* text) {
    ios* stream = IosOf(out);
    const int prefixLength = static_cast<int>(strlen(prefix));
    const int textLength = static_cast<int>(strlen(text));
    const unsigned width = static_cast<unsigned>(stream->m_width);
    int padding = static_cast<unsigned>(prefixLength + textLength) < width ? static_cast<int>(width) - textLength - prefixLength : 0;
    if ((stream->m_flags & (iostream_flags::kLeft | iostream_flags::kInternal)) == 0) {
        const int leading = padding--;
        if (0 < leading) {
            PutPadding(stream, leading);
            padding -= leading;
        }
    }
    if (prefixLength != 0) PutText(stream, prefix, prefixLength);
    if ((stream->m_flags & iostream_flags::kInternal) != 0) {
        const int inner = padding--;
        if (0 < inner) {
            PutPadding(stream, inner);
            padding -= inner;
        }
    }
    PutText(stream, text, textLength);
    if ((stream->m_flags & iostream_flags::kLeft) != 0 && 0 < padding) PutPadding(stream, padding);
}

ostream* ostream_seekp_pos(ostream* out, long position) {
    ios* stream = IosOf(out);
    SeekWithFailCheck(stream, [&](streambuf* buffer) { return static_cast<long>(buffer->SeekPosition(position, iostream_flags::kOut)); });
    return out;
}

ostream* ostream_seekp_off(ostream* out, long offset, int dir) {
    ios* stream = IosOf(out);
    SeekWithFailCheck(stream, [&](streambuf* buffer) { return static_cast<long>(buffer->SeekOffset(offset, dir, iostream_flags::kOut)); });
    return out;
}

long ostream_tellp(ostream* out) {
    ios* stream = IosOf(out);
    return SeekWithFailCheck(stream, [&](streambuf* buffer) {
        return static_cast<long>(buffer->SeekOffset(0, iostream_flags::kSeekCur, iostream_flags::kOut));
    });
}

ostream* ostream_operator_ls_streambuf(ostream* out, streambuf* source) {
    if (ostream_opfx(out) != 0) {
        ios* stream = IosOf(out);
        unsigned c = static_cast<unsigned>(streambuf_sbumpc(source));
        if (c != 0xffffffffu) {
            for (;;) {
                streambuf* buffer = stream->m_bp;
                if (buffer->m_pptr < buffer->m_epptr) {
                    *buffer->m_pptr++ = static_cast<char>(c);
                    c &= 0xff;
                } else {
                    c = static_cast<unsigned>(buffer->Overflow(static_cast<int>(c)));
                }
                if (c == 0xffffffffu) break;
                c = static_cast<unsigned>(streambuf_sbumpc(source));
                if (c == 0xffffffffu) {
                    ostream_osfx(out);
                    return out;
                }
            }
            stream->m_state |= iostream_flags::kFail;
        }
        ostream_osfx(out);
    }
    return out;
}

ostream* ostream_operator_ls_long(ostream* out, long value) { return PrintNumber(out, kLongSpec, true, value, 3); }

ostream* ostream_write(ostream* out, const void* buffer, int count) {
    if (ostream_opfx(out) != 0) {
        ios* stream = IosOf(out);
        streambuf* target = stream->m_bp;
        if (target->Write(static_cast<const char*>(buffer), count) != count) stream->m_state |= iostream_flags::kFail | iostream_flags::kBad;
        ostream_osfx(out);
    }
    return out;
}

ostream* ostream_operator_ls_ulong(ostream* out, unsigned long value) { return PrintNumber(out, kUnsignedLongSpec, false, static_cast<int>(value), 3); }

ostream* ostream_operator_ls_int(ostream* out, int value) { return PrintNumber(out, kIntSpec, true, value, 2); }
