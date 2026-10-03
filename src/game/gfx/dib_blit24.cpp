#include "gfx/dib_bitmap.h"

Rgb24::Rgb24(uint8_t red, uint8_t green, uint8_t blue) {
    r = red;
    g = green;
    b = blue;
}

Rgb24::Rgb24(uint32_t rgbQuad) {
    r = static_cast<uint8_t>(rgbQuad >> 16);
    g = static_cast<uint8_t>(rgbQuad >> 8);
    b = static_cast<uint8_t>(rgbQuad);
}

int Rgb24::DiffersFrom(uint32_t rgbQuad) {
    return static_cast<uint8_t>(rgbQuad >> 16) != r || static_cast<uint8_t>(rgbQuad >> 8) != g ||
           static_cast<uint8_t>(rgbQuad) != b;
}

int DibBitmap::Blit24RleStub(DibBitmap*, int, int, int, int, int, int) { return 0; }
