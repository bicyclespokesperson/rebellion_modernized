#pragma once
#include <cstdint>

#include "platform/win32.h"

struct Rgb24 {
    Rgb24() = default;
    Rgb24(uint8_t red, uint8_t green, uint8_t blue);  // 0x60e5e0
    explicit Rgb24(uint32_t rgbQuad);                 // 0x60e600: from an RGBQUAD read as a dword (0x00RRGGBB)
    int DiffersFrom(uint32_t rgbQuad);                // 0x60e620: compares with an RGBQUAD/BGR pixel read as a dword

    uint8_t r;
    uint8_t g;
    uint8_t b;
};

// How a DibBitmap's pixel memory is owned (low byte of m_kind with bit 0x08 masked off).
enum DibBitmapKind : uint8_t {
    kDibFile = 0x01,        // BITMAPINFO + pixels read from a .BMP file into one heap block
    kDibResource = 0x02,    // points into a locked RT_BITMAP resource
    kDibHeap = 0x04,        // heap block built from another bitmap's format
    kDibSection = 0x10,     // GDI DIB section
    kDibKindMask = 0xf7,
};

// An 8-bit (occasionally 24-bit) device-independent bitmap with the game's software blitter.
// Pixel rows are bottom-up when the header height is positive.
class DibBitmap {
public:
    DibBitmap(HMODULE module, const char* source, uint32_t kind);   // 0x5fbc80: file (kind 1) or resource name (kind 2)
    DibBitmap(HMODULE module, uint32_t resourceId, uint32_t kind);  // 0x5fbd20: RT_BITMAP resource by id
    DibBitmap(int width, int height, DibBitmap* format);            // 0x5fbda0: zeroed heap bitmap in `format`'s layout
    DibBitmap(LONG width, LONG height, DibBitmap* format, HDC dc);  // 0x5fbea0: DIB section in `format`'s layout
    ~DibBitmap();

    int IsLoaded() const;
    int Width() const;
    int Height() const;  // negative for top-down bitmaps
    uint16_t BitCount() const;
    uint32_t Compression() const;  // 1 = RLE8
    uint32_t HeaderSize() const;
    HBITMAP DibSectionHandle() const;

    int RowStride() const;
    int RowIndex(int y) const;  // storage row holding visual row y

    void Reset(HMODULE module, uint8_t kind);
    void ComputeColorCount();
    void LocateBits();
    void CacheTransparentColor();
    int LoadResourceById(uint32_t resourceId);
    int LoadResourceByName(const char* name);
    int LoadBmpFile(const char* path);
    HPALETTE CreatePaletteFromColorTable();

    // Copies to a screen/memory DC. mode '@' selects BitBlt (rop in `rop`), 0x10 does nothing, others use
    // SetDIBitsToDevice. Zero width/height mean "whole bitmap".
    int DrawToDc(HDC dc, char mode, DWORD rop, int destX, int destY, int srcX, int srcY, int width, int height);

    // Source-to-`dest` copies; `this` is always the source. A width or height of 0 means "as much as fits".
    uint32_t Blit(DibBitmap* dest, int destX, int destY, int srcX, int srcY, int width, int height);
    int BlitKeyed(DibBitmap* dest, int destX, int destY, uint8_t colorKey, int srcX, int srcY, int width, int height);
    int DrawKeyed(DibBitmap* dest, int destX, int destY);
    int BlitRle(DibBitmap* dest, int destX, int destY, int srcX, int srcY, int width, int height);
    int BlitKeyedRle(DibBitmap* dest, int destX, int destY, uint8_t colorKey, int srcX, int srcY, int width, int height);
    int BlitRemapDestination(DibBitmap* dest, int destX, int destY, int srcX, int srcY, int width, int height);
    int BlitRemapped(DibBitmap* dest, int destX, int destY, int srcX, int srcY, int width, int height);
    int BlitRemappedOverlay(DibBitmap* dest, int destX, int destY, int srcX, int srcY, int width, int height);
    int BlitRemappedRle(DibBitmap* dest, int destX, int destY, uint8_t colorKey, int srcX, int srcY, int width,
                        int height);
    int BlitRemapDestinationRleStub(DibBitmap* dest, int destX, int destY, uint8_t colorKey, int srcX, int srcY,
                                    int width, int height);
    int FillRect(int x, int y, int width, int height, uint8_t value);

    int IsPixelOpaque(int x, int y);
    int IsPixelOpaqueRle(int x, uint32_t y);

    uint8_t* BuildRemapTable(uint32_t (*transform)(uint32_t));
    void SetRemapTable(uint8_t* table);
    void ApplyRemapTable(int16_t x, int16_t y, int16_t width, int16_t height);

    // 24-bit-pixel counterparts (0x60e5e0 range, not part of this file).
    uint32_t Blit24(DibBitmap* dest, int destX, int destY, int srcX, int srcY, int width, int height);
    void BlitKeyed24(DibBitmap* dest, int destX, int destY, uint32_t paletteIndexOrColor, int srcX, int srcY,
                     int width, int height);
    void BlitKeyedRgb24(DibBitmap* dest, int destX, int destY, Rgb24 colorKey, int srcX, int srcY, int width,
                        int height);
    // What Blit24 does for an RLE source: nothing, returns 0.
    int Blit24RleStub(DibBitmap* dest, int destX, int destY, int srcX, int srcY, int width, int height);  // 0x60ed00

    BITMAPINFOHEADER* m_info;  // +0x00 header followed by the colour table; null when nothing is loaded
    uint8_t* m_bits;           // +0x04
    uint8_t m_colorKey;        // +0x08 first pixel (8-bit): the transparent index
    Rgb24 m_colorKeyRgb;       // +0x09 first pixel (24-bit)
    uint8_t m_kind;            // +0x0c DibBitmapKind, bit 0x08 is carried along but ignored
    uint8_t m_unk0d[3];        // +0x0d
    int32_t m_colorCount;      // +0x10
    HGLOBAL m_resource;        // +0x14 locked RT_BITMAP resource
    HMODULE m_module;          // +0x18 resource module
    HBITMAP m_dibSection;      // +0x1c
    HPALETTE m_palette;        // +0x20 lazily built by DrawToDc
    uint8_t* m_remapTable;     // +0x24 256-entry colour remap (BuildRemapTable / SetRemapTable)
};
