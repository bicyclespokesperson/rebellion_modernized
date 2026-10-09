#include <cstdlib>

namespace {

bool IsSpace(unsigned char c) { return c == ' ' || (c >= '\t' && c <= '\r'); }
bool IsDigit(unsigned char c) { return c >= '0' && c <= '9'; }

}  // namespace

// VC5 atol (0x619560): leading white space, one optional sign, digits accumulated in 32 bits with wrap-around (no
// overflow clamping, unlike strtol), stops at the first other character. The original classifies through the ctype table
// of the current locale; this uses the C locale classes (the game passes digits, version strings and typed numbers).
extern "C" long __cdecl atol(const char* text) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
    while (IsSpace(*p)) ++p;
    const unsigned char sign = *p++;
    unsigned char c = sign;
    if (sign == '-' || sign == '+') c = *p++;
    unsigned long total = 0;
    while (IsDigit(c)) {
        total = total * 10u + (c - '0');
        c = *p++;
    }
    return static_cast<long>(sign == '-' ? 0ul - total : total);
}

extern "C" int __cdecl atoi(const char* text) { return atol(text); }
