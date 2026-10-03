// DibBitmap: construction, loading from resources and .BMP files, destruction, and GDI output.
#include "gfx/dib_bitmap.h"

#include <cstring>
#include <new>

extern int16_t g_bmpFileMagic;  // 'BM'
extern HPALETTE g_displayPalette;

namespace {
constexpr uint32_t kBmpFileHeaderSize = 0x0e;
constexpr uint32_t kFileReadChunk = 0x8000;
constexpr int kPaletteEntryCount = 256;
constexpr int kColorTableOffset = sizeof(BITMAPINFOHEADER);
constexpr int kStretchColorOnColor = 3;
constexpr int kBitmapResourceType = 2;
constexpr uint8_t kDefaultKeyBlue = 0xff;

uint32_t ColorTableBytes(const DibBitmap* format) {
    return format->BitCount() < 9 ? static_cast<uint32_t>(format->m_colorCount) << 2 : 0;
}

uint32_t RowStrideFor(int bitsPerPixel, int width) { return ((bitsPerPixel * width + 31) >> 3) & ~3; }

BITMAPINFOHEADER* CloneHeaderAndColorTable(const DibBitmap* format, uint32_t byteCount) {
    auto* header = static_cast<BITMAPINFOHEADER*>(operator new(byteCount));
    std::memset(header, 0, byteCount);
    std::memcpy(header, format->m_info, ColorTableBytes(format) + kColorTableOffset);
    return header;
}
}  // namespace

DibBitmap::DibBitmap(HMODULE module, const char* source, uint32_t kind) {
    m_colorKeyRgb = {0, 0, 0};
    Reset(module, static_cast<uint8_t>(kind));
    if (source) {
        switch (kind & kDibKindMask) {
            case kDibFile:
                LoadBmpFile(source);
                break;
            case kDibResource:
                LoadResourceByName(source);
                break;
            default:
                m_kind = 0;
                break;
        }
    }
    m_dibSection = nullptr;
    if (m_bits) CacheTransparentColor();
    m_remapTable = nullptr;
}

DibBitmap::DibBitmap(HMODULE module, uint32_t resourceId, uint32_t kind) {
    m_colorKeyRgb = {0, 0, 0};
    Reset(module, static_cast<uint8_t>(kind));
    LoadResourceById(resourceId);
    if (m_bits) CacheTransparentColor();
    m_dibSection = nullptr;
    m_remapTable = nullptr;
}

DibBitmap::DibBitmap(int width, int height, DibBitmap* format) {
    m_colorKeyRgb = {0, 0, 0};
    const uint32_t rowStride = RowStrideFor(format->BitCount(), width);
    const uint32_t colorTableBytes = ColorTableBytes(format);
    const uint32_t totalBytes = rowStride * height + kColorTableOffset + colorTableBytes;
    Reset(format->m_module, kDibHeap);
    m_colorCount = format->m_colorCount;
    BITMAPINFOHEADER* header = CloneHeaderAndColorTable(format, totalBytes);
    header->biWidth = width;
    header->biHeight = height;
    header->biSizeImage = 0;
    header->biCompression = 0;
    m_info = header;
    m_bits = reinterpret_cast<uint8_t*>(header) + colorTableBytes + kColorTableOffset;
    CacheTransparentColor();
    m_dibSection = nullptr;
    m_remapTable = nullptr;
}

DibBitmap::DibBitmap(LONG width, LONG height, DibBitmap* format, HDC dc) {
    m_colorKeyRgb = {0, 0, 0};
    const uint32_t totalBytes = ColorTableBytes(format) + kColorTableOffset;
    Reset(format->m_module, kDibSection);
    m_colorCount = format->m_colorCount;
    BITMAPINFOHEADER* header = CloneHeaderAndColorTable(format, totalBytes);
    header->biWidth = width;
    header->biHeight = height;
    header->biSizeImage = 0;
    header->biCompression = 0;
    m_info = header;
    m_dibSection = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(header), DIB_RGB_COLORS,
                                    reinterpret_cast<void**>(&m_bits), nullptr, 0);
    GetLastError();
    if (m_bits) CacheTransparentColor();
    m_remapTable = nullptr;
}

DibBitmap::~DibBitmap() {
    switch (m_kind & kDibKindMask) {
        case kDibResource:
            if (m_resource) FreeResource(m_resource);
            break;
        case kDibSection:
            if (m_dibSection) DeleteObject(m_dibSection);
            [[fallthrough]];
        default:
            if (m_info) operator delete(m_info);
            break;
    }
    if (m_palette) DeleteObject(m_palette);
}

bool DibBitmap::IsLoaded() const { return m_info != nullptr; }

int DibBitmap::Width() const { return m_info ? m_info->biWidth : 0; }

int DibBitmap::Height() const { return m_info ? m_info->biHeight : 0; }

uint16_t DibBitmap::BitCount() const { return m_info ? m_info->biBitCount : 0; }

uint32_t DibBitmap::Compression() const { return m_info ? m_info->biCompression : 0; }

uint32_t DibBitmap::HeaderSize() const { return m_info ? m_info->biSize : 0; }

HBITMAP DibBitmap::DibSectionHandle() const { return (m_kind & kDibSection) ? m_dibSection : nullptr; }

int DibBitmap::RowStride() const { return RowStrideFor(BitCount(), Width()); }

int DibBitmap::RowIndex(int y) const {
    const int height = Height();
    return height < 0 ? y : height - y - 1;
}

void DibBitmap::Reset(HMODULE module, uint8_t kind) {
    m_module = module;
    m_info = nullptr;
    m_bits = nullptr;
    m_resource = nullptr;
    m_dibSection = nullptr;
    m_palette = nullptr;
    m_kind = kind;
    m_colorCount = 0;
    m_colorKeyRgb.b = kDefaultKeyBlue;
}

void DibBitmap::ComputeColorCount() {
    const uint16_t bitsPerPixel = m_info->biBitCount;
    if (bitsPerPixel < 24) {
        m_colorCount = static_cast<int32_t>(m_info->biClrUsed);
        if (m_colorCount == 0) m_colorCount = 1 << (bitsPerPixel & 0x1f);
    } else {
        m_colorCount = 0;
    }
}

void DibBitmap::LocateBits() {
    if (!m_info) {
        m_bits = nullptr;
        return;
    }
    m_bits = reinterpret_cast<uint8_t*>(m_info) + HeaderSize() + m_colorCount * 4;
}

void DibBitmap::CacheTransparentColor() {
    if (BitCount() == 8) {
        if (!m_info || !m_bits) return;
        if (m_info->biCompression != 1) {
            m_colorKey = m_bits[0];
            return;
        }
        m_colorKey = 0;
        if (m_bits[0] != 0) {
            m_colorKey = m_bits[1];
        } else if (m_bits[1] > 2) {
            m_colorKey = m_bits[2];
        }
    } else if (BitCount() == 24) {
        m_colorKeyRgb.b = m_bits[0];
        m_colorKeyRgb.g = m_bits[1];
        m_colorKeyRgb.r = m_bits[2];
    }
}

bool DibBitmap::LoadResourceById(uint32_t resourceId) {
    if (!m_module) return false;
    if (m_resource) m_resource = nullptr;
    HRSRC resourceInfo = FindResourceA(m_module, MAKEINTRESOURCEA(resourceId & 0xffff),
                                       MAKEINTRESOURCEA(kBitmapResourceType));
    if (!resourceInfo) GetLastError();
    m_resource = LoadResource(m_module, resourceInfo);
    if (!m_resource) GetLastError();
    if (m_resource) m_info = static_cast<BITMAPINFOHEADER*>(LockResource(m_resource));
    if (!m_info) return false;
    ComputeColorCount();
    LocateBits();
    CacheTransparentColor();
    return true;
}

bool DibBitmap::LoadResourceByName(const char* name) {
    if (!m_module || !name) return false;
    if (m_resource) m_resource = nullptr;
    HRSRC resourceInfo = FindResourceA(m_module, name, MAKEINTRESOURCEA(kBitmapResourceType));
    m_resource = LoadResource(m_module, resourceInfo);
    if (m_resource) m_info = static_cast<BITMAPINFOHEADER*>(LockResource(m_resource));
    if (!m_info) return false;
    ComputeColorCount();
    LocateBits();
    return true;
}

bool DibBitmap::LoadBmpFile(const char* path) {
    if (m_info) {
        operator delete(m_info);
        m_info = nullptr;
    }
    HFILE file = _lopen(path, OF_SHARE_DENY_WRITE);
    if (file == HFILE_ERROR) return false;

    uint8_t rawHeader[kBmpFileHeaderSize];
    if (_lread(file, rawHeader, kBmpFileHeaderSize) != kBmpFileHeaderSize) {
        _lclose(file);
        return false;
    }
    int16_t magic;
    int32_t fileSize;
    std::memcpy(&magic, rawHeader, sizeof(magic));
    std::memcpy(&fileSize, rawHeader + 2, sizeof(fileSize));
    if (magic != g_bmpFileMagic) {
        _lclose(file);
        return false;
    }

    uint32_t remaining = fileSize - kBmpFileHeaderSize;
    m_info = static_cast<BITMAPINFOHEADER*>(operator new(remaining));
    if (!m_info) {
        _lclose(file);
        return false;
    }
    uint32_t offset = 0;
    while (remaining != 0) {
        const uint32_t chunk = remaining < kFileReadChunk + 1 ? remaining : kFileReadChunk;
        if (_lread(file, reinterpret_cast<uint8_t*>(m_info) + offset, chunk) != chunk) {
            _lclose(file);
            operator delete(m_info);
            m_info = nullptr;
            return false;
        }
        offset += chunk;
        remaining -= chunk;
    }
    _lclose(file);

    const uint32_t headerSize = HeaderSize();
    const bool plausibleHeader = headerSize > 0xb && (headerSize < 0xd || headerSize > 0xf);
    if (plausibleHeader) {
        ComputeColorCount();
        LocateBits();
        return true;
    }
    // BUG(original): m_info is left dangling after this delete.
    operator delete(m_info);
    return false;
}

HPALETTE DibBitmap::CreatePaletteFromColorTable() {
    if (!m_info) return nullptr;
    const auto* colorTable = reinterpret_cast<const RGBQUAD*>(reinterpret_cast<const uint8_t*>(m_info) + kColorTableOffset);
    LOGPALETTE logicalPalette;
    PALETTEENTRY entries[kPaletteEntryCount];
    for (int i = 0; i < kPaletteEntryCount; ++i) {
        entries[i].peRed = colorTable[i].rgbRed;
        entries[i].peGreen = colorTable[i].rgbGreen;
        entries[i].peBlue = colorTable[i].rgbBlue;
        entries[i].peFlags = 0;
    }
    logicalPalette.palVersion = 0x300;
    logicalPalette.palNumEntries = 1;
    HPALETTE palette = CreatePalette(&logicalPalette);
    if (palette && ResizePalette(palette, kPaletteEntryCount)) SetPaletteEntries(palette, 0, kPaletteEntryCount, entries);
    return palette;
}

int DibBitmap::DrawToDc(HDC dc, char mode, DWORD rop, int destX, int destY, int srcX, int srcY, int width, int height) {
    if (!m_info) return 0;
    const int bitmapWidth = Width();
    const int bitmapHeight = Height();
    if (width == 0) width = bitmapWidth;
    if (height == 0) height = bitmapHeight;
    if (srcX < 0) {
        width += srcX;
        destX -= srcX;
        srcX = 0;
    }
    if (srcY < 0) {
        height += srcY;
        destY -= srcY;
        srcY = 0;
    }
    if (destX < 0) {
        width += destX;
        srcX -= destX;
        destX = 0;
    }
    if (destY < 0) {
        height += destY;
        srcY -= destY;
        destY = 0;
    }
    if (mode == 0x10) return 1;

    if (mode != '@') {
        SetStretchBltMode(dc, kStretchColorOnColor);
        // BUG(original): the destination position of this path is taken from the width/height parameters.
        SetDIBitsToDevice(dc, width, height, bitmapWidth, bitmapHeight, srcX, srcY, 0, bitmapHeight, m_bits,
                          reinterpret_cast<BITMAPINFO*>(m_info), DIB_RGB_COLORS);
        return 1;
    }

    HDC memoryDc = CreateCompatibleDC(dc);
    if (BitCount() == 8 && !m_palette) m_palette = CreatePaletteFromColorTable();
    if (m_kind == kDibSection) {
        HGDIOBJ previousBitmap = SelectObject(memoryDc, DibSectionHandle());
        HPALETTE previousPalette = SelectPalette(memoryDc, m_palette, FALSE);
        BitBlt(dc, destX, destY, width, height, memoryDc, srcX, srcY, rop);
        SelectPalette(memoryDc, previousPalette, FALSE);
        SelectObject(memoryDc, previousBitmap);
    } else {
        HBITMAP deviceBitmap = nullptr;
        if (m_info) {
            deviceBitmap = CreateDIBitmap(dc, m_info, CBM_INIT, m_bits, reinterpret_cast<BITMAPINFO*>(m_info),
                                          DIB_RGB_COLORS);
        }
        HGDIOBJ toDelete = nullptr;
        if (deviceBitmap) {
            BITMAP bitmapInfo;
            GetObjectA(deviceBitmap, sizeof(bitmapInfo), &bitmapInfo);
            HGDIOBJ previousBitmap = SelectObject(memoryDc, deviceBitmap);
            HPALETTE previousPalette = SelectPalette(memoryDc, m_palette, FALSE);
            HPALETTE previousDcPalette = nullptr;
            if (g_displayPalette) {
                previousDcPalette = SelectPalette(dc, g_displayPalette, FALSE);
                RealizePalette(dc);
            }
            BitBlt(dc, destX, destY, width, height, memoryDc, srcX, srcY, rop);
            if (g_displayPalette) SelectPalette(dc, previousDcPalette, FALSE);
            SelectPalette(memoryDc, previousPalette, FALSE);
            toDelete = SelectObject(memoryDc, previousBitmap);
        }
        DeleteObject(toDelete);
    }
    DeleteDC(memoryDc);
    return 1;
}
