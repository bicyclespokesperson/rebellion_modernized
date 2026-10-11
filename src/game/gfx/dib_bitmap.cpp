// DibBitmap: construction, loading from resources and .BMP files, destruction, and GDI output.
#include "gfx/dib_bitmap.h"

#include <algorithm>
#include <cstring>
#include <limits>

extern int16_t g_bmpFileMagic; // 'BM'
extern HPALETTE g_displayPalette;

#include "gfx/rle8_decoder.h"

namespace
{
constexpr uint32_t kBmpFileHeaderSize = 0x0e;
constexpr uint32_t kFileReadChunk = 0x8000;
constexpr int kPaletteEntryCount = 256;
constexpr int kStretchColorOnColor = 3;
constexpr int kBitmapResourceType = 2;
constexpr uint8_t kDefaultKeyBlue = 0xff;

uint32_t ColorTableBytes(const DibBitmap* format)
{
  return format->BitCount() < 9 ? static_cast<uint32_t>(format->m_colorCount) << 2 : 0;
}

bool RowStrideFor(int bitsPerPixel, int width, size_t& stride)
{
  if (bitsPerPixel <= 0 || width <= 0 ||
      static_cast<size_t>(width) > (std::numeric_limits<size_t>::max() - 31) / bitsPerPixel)
    return false;
  stride = ((static_cast<size_t>(bitsPerPixel) * static_cast<size_t>(width) + 31) / 32) * 4;
  return stride <= static_cast<size_t>(std::numeric_limits<int>::max());
}

constexpr size_t kMaximumBitmapBytes = 256u * 1024u * 1024u;

bool NormalizeRle8(std::vector<uint8_t>& bytes, BITMAPINFOHEADER*& info, uint8_t*& bits, int colorCount)
{
  if (info->biWidth <= 0 || info->biHeight == 0 || info->biHeight == INT32_MIN ||
      (info->biBitCount != 8 && info->biBitCount != 24) || info->biPlanes != 1)
    return false;
  if (info->biCompression == 0)
  {
    size_t stride = 0;
    if (!RowStrideFor(info->biBitCount, info->biWidth, stride))
      return false;
    const int height = info->biHeight < 0 ? -info->biHeight : info->biHeight;
    const size_t start = static_cast<size_t>(bits - bytes.data());
    if (stride > (std::numeric_limits<size_t>::max() - start) / static_cast<size_t>(height) ||
        start + stride * static_cast<size_t>(height) > bytes.size())
      return false;
    if (info->biBitCount == 8)
    {
      for (int y = 0; y < height; ++y)
        for (int x = 0; x < info->biWidth; ++x)
          if (bits[static_cast<size_t>(y) * stride + x] >= colorCount)
            return false;
    }
    return true;
  }
  if (info->biBitCount != 8)
    return false;
  if (info->biCompression != 1)
    return false;
  const int height = info->biHeight < 0 ? -info->biHeight : info->biHeight;
  const size_t start = static_cast<size_t>(bits - bytes.data());
  std::vector<uint8_t> decoded;
  if (start > bytes.size() ||
      !DecodeRle8(std::span<const uint8_t>(bytes).subspan(start), info->biWidth, height, info->biHeight < 0, decoded))
    return false;
  for (uint8_t index : decoded)
    if (index >= colorCount)
      return false;
  size_t stride = 0;
  if (!RowStrideFor(8, info->biWidth, stride))
    return false;
  if (start > kMaximumBitmapBytes || stride > (kMaximumBitmapBytes - start) / static_cast<size_t>(height))
    return false;
  bytes.resize(start + stride * static_cast<size_t>(height), 0);
  info = reinterpret_cast<BITMAPINFOHEADER*>(bytes.data());
  bits = bytes.data() + start;
  for (int y = 0; y < height; ++y)
  {
    const int storageRow = info->biHeight < 0 ? y : height - y - 1;
    std::memcpy(bits + static_cast<size_t>(storageRow) * stride,
                decoded.data() + static_cast<size_t>(y) * info->biWidth, static_cast<size_t>(info->biWidth));
  }
  info->biCompression = 0;
  info->biSizeImage = static_cast<uint32_t>(stride * static_cast<size_t>(height));
  return true;
}

bool ValidateDibPayload(DibBitmap& bitmap)
{
  bitmap.m_info = nullptr;
  bitmap.m_bits = nullptr;
  bitmap.m_colorCount = 0;
  if (bitmap.m_storage.size() < sizeof(BITMAPINFOHEADER) || bitmap.m_storage.size() > kMaximumBitmapBytes)
  {
    bitmap.m_storage.clear();
    return false;
  }
  bitmap.m_info = reinterpret_cast<BITMAPINFOHEADER*>(bitmap.m_storage.data());
  const uint32_t headerSize = bitmap.HeaderSize();
  auto reject = [&bitmap]()
  {
    bitmap.m_storage.clear();
    bitmap.m_info = nullptr;
    bitmap.m_bits = nullptr;
    bitmap.m_colorCount = 0;
    return false;
  };
  if (headerSize < sizeof(BITMAPINFOHEADER) || headerSize > bitmap.m_storage.size() || bitmap.m_info->biWidth <= 0 ||
      bitmap.m_info->biHeight == 0 || bitmap.m_info->biHeight == INT32_MIN || bitmap.m_info->biPlanes != 1 ||
      (bitmap.m_info->biBitCount != 8 && bitmap.m_info->biBitCount != 24))
    return reject();
  if (bitmap.m_info->biBitCount == 8 && bitmap.m_info->biClrUsed > 256)
    return reject();
  if (bitmap.m_info->biBitCount == 24 && bitmap.m_info->biClrUsed != 0)
    return reject();
  bitmap.ComputeColorCount();
  if ((bitmap.m_info->biBitCount == 8 && (bitmap.m_colorCount < 1 || bitmap.m_colorCount > 256)) ||
      (bitmap.m_info->biBitCount == 24 && bitmap.m_colorCount != 0) ||
      static_cast<size_t>(headerSize) + static_cast<size_t>(bitmap.m_colorCount) * 4 > bitmap.m_storage.size())
    return reject();
  bitmap.LocateBits();
  if (!NormalizeRle8(bitmap.m_storage, bitmap.m_info, bitmap.m_bits,
                     bitmap.m_info->biBitCount == 8 ? bitmap.m_colorCount : 256))
    return reject();
  bitmap.CacheTransparentColor();
  return true;
}

bool LoadDibBytes(DibBitmap& bitmap, std::span<const uint8_t> dibBytes)
{
  bitmap.m_storage.clear();
  if (dibBytes.size() < sizeof(BITMAPINFOHEADER) || dibBytes.size() > kMaximumBitmapBytes)
    return false;
  bitmap.m_storage.assign(dibBytes.begin(), dibBytes.end());
  if (ValidateDibPayload(bitmap))
    return true;
  bitmap.m_storage.clear();
  bitmap.m_info = nullptr;
  bitmap.m_bits = nullptr;
  bitmap.m_colorCount = 0;
  return false;
}

void ClearBitmapPayload(DibBitmap& bitmap)
{
  bitmap.m_storage.clear();
  bitmap.m_info = nullptr;
  bitmap.m_bits = nullptr;
  bitmap.m_colorCount = 0;
  if (bitmap.m_palette)
    DeleteObject(bitmap.m_palette);
  bitmap.m_palette = nullptr;
}
} // namespace

DibBitmap::DibBitmap(HMODULE module, const char* source, uint32_t kind)
{
  m_colorKeyRgb = {0, 0, 0};
  Reset(module, static_cast<uint8_t>(kind));
  if (source)
  {
    switch (kind & kDibKindMask)
    {
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
  if (m_bits)
    CacheTransparentColor();
  m_remapTable = nullptr;
}

DibBitmap::DibBitmap(HMODULE module, uint32_t resourceId, uint32_t kind)
{
  m_colorKeyRgb = {0, 0, 0};
  Reset(module, static_cast<uint8_t>(kind));
  LoadResourceById(resourceId);
  if (m_bits)
    CacheTransparentColor();
  m_dibSection = nullptr;
  m_remapTable = nullptr;
}

DibBitmap::DibBitmap(int width, int height, DibBitmap* format)
{
  m_colorKeyRgb = {0, 0, 0};
  Reset(nullptr, kDibHeap);
  m_remapTable = nullptr;
  if (!format || !format->IsLoaded() || width <= 0 || height == 0 || height == INT32_MIN ||
      (format->BitCount() != 8 && format->BitCount() != 24))
    return;
  if (format->m_colorCount < 0 || format->m_colorCount > 256 || (format->BitCount() == 8 && format->m_colorCount == 0))
    return;
  size_t rowStride = 0;
  if (!RowStrideFor(format->BitCount(), width, rowStride))
    return;
  const size_t absHeight = static_cast<size_t>(height < 0 ? -height : height);
  const size_t headerSize = format->HeaderSize();
  const size_t colorTableBytes = ColorTableBytes(format);
  if (headerSize > kMaximumBitmapBytes - colorTableBytes ||
      rowStride > (kMaximumBitmapBytes - headerSize - colorTableBytes) / absHeight)
    return;
  const size_t totalBytes = headerSize + colorTableBytes + rowStride * absHeight;
  Reset(format->m_module, kDibHeap);
  m_colorCount = format->m_colorCount;
  m_storage.assign(totalBytes, 0);
  BITMAPINFOHEADER* header = reinterpret_cast<BITMAPINFOHEADER*>(m_storage.data());
  std::memcpy(header, format->m_info, headerSize + colorTableBytes);
  header->biWidth = width;
  header->biHeight = height;
  header->biSizeImage = 0;
  header->biCompression = 0;
  m_info = header;
  m_bits = m_storage.data() + headerSize + colorTableBytes;
  CacheTransparentColor();
  m_dibSection = nullptr;
  m_remapTable = nullptr;
}

DibBitmap::DibBitmap(LONG width, LONG height, DibBitmap* format, HDC dc)
{
  m_colorKeyRgb = {0, 0, 0};
  Reset(nullptr, kDibSection);
  m_remapTable = nullptr;
  if (!format || !format->IsLoaded() || width <= 0 || height == 0 || height == INT32_MIN ||
      (format->BitCount() != 8 && format->BitCount() != 24))
    return;
  const size_t headerSize = format->HeaderSize();
  const size_t colorTableBytes = ColorTableBytes(format);
  if (headerSize < sizeof(BITMAPINFOHEADER) || headerSize + colorTableBytes > kMaximumBitmapBytes)
    return;
  size_t rowStride = 0;
  if (!RowStrideFor(format->BitCount(), width, rowStride) ||
      rowStride > kMaximumBitmapBytes / static_cast<size_t>(height < 0 ? -height : height))
    return;
  Reset(format->m_module, kDibSection);
  m_colorCount = format->m_colorCount;
  m_storage.resize(headerSize + colorTableBytes);
  BITMAPINFOHEADER* header = reinterpret_cast<BITMAPINFOHEADER*>(m_storage.data());
  std::memcpy(header, format->m_info, headerSize + colorTableBytes);
  header->biWidth = width;
  header->biHeight = height;
  header->biSizeImage = 0;
  header->biCompression = 0;
  m_info = header;
  m_dibSection = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(header), DIB_RGB_COLORS,
                                  reinterpret_cast<void**>(&m_bits), nullptr, 0);
  if (!m_dibSection || !m_bits)
  {
    if (m_dibSection)
      DeleteObject(m_dibSection);
    m_dibSection = nullptr;
    m_bits = nullptr;
    m_info = nullptr;
    m_storage.clear();
    return;
  }
  CacheTransparentColor();
  m_remapTable = nullptr;
}

DibBitmap::~DibBitmap()
{
  switch (m_kind & kDibKindMask)
  {
    case kDibResource:
      if (m_resource)
        FreeResource(m_resource);
      break;
    case kDibSection:
      if (m_dibSection)
        DeleteObject(m_dibSection);
      break;
    default:
      break;
  }
  if (m_palette)
    DeleteObject(m_palette);
}

int DibBitmap::IsLoaded() const
{
  return m_info != nullptr;
}

int DibBitmap::Width() const
{
  return m_info ? m_info->biWidth : 0;
}

int DibBitmap::Height() const
{
  return m_info ? m_info->biHeight : 0;
}

uint16_t DibBitmap::BitCount() const
{
  return m_info ? m_info->biBitCount : 0;
}

uint32_t DibBitmap::Compression() const
{
  return m_info ? m_info->biCompression : 0;
}

uint32_t DibBitmap::HeaderSize() const
{
  return m_info ? m_info->biSize : 0;
}

HBITMAP DibBitmap::DibSectionHandle() const
{
  return (m_kind & kDibSection) ? m_dibSection : nullptr;
}

int DibBitmap::RowStride() const
{
  size_t stride = 0;
  return RowStrideFor(BitCount(), Width(), stride) ? static_cast<int>(stride) : 0;
}

int DibBitmap::RowIndex(int y) const
{
  const int height = Height();
  return height < 0 ? y : height - y - 1;
}

void DibBitmap::Reset(HMODULE module, uint8_t kind)
{
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

void DibBitmap::ComputeColorCount()
{
  const uint16_t bitsPerPixel = m_info->biBitCount;
  if (bitsPerPixel < 24)
  {
    m_colorCount = static_cast<int32_t>(m_info->biClrUsed);
    if (m_colorCount == 0)
      m_colorCount = 1 << (bitsPerPixel & 0x1f);
  }
  else
  {
    m_colorCount = 0;
  }
}

void DibBitmap::LocateBits()
{
  if (!m_info)
  {
    m_bits = nullptr;
    return;
  }
  m_bits = reinterpret_cast<uint8_t*>(m_info) + HeaderSize() + m_colorCount * 4;
}

void DibBitmap::CacheTransparentColor()
{
  if (BitCount() == 8)
  {
    if (!m_info || !m_bits)
      return;
    m_colorKey = m_bits[0];
  }
  else if (BitCount() == 24)
  {
    m_colorKeyRgb.b = m_bits[0];
    m_colorKeyRgb.g = m_bits[1];
    m_colorKeyRgb.r = m_bits[2];
  }
}

int DibBitmap::LoadResourceById(uint32_t resourceId)
{
  ClearBitmapPayload(*this);
  if (!m_module)
    return false;
  HRSRC resourceInfo =
    FindResourceA(m_module, MAKEINTRESOURCEA(resourceId & 0xffff), MAKEINTRESOURCEA(kBitmapResourceType));
  if (!resourceInfo)
    return false;
  m_resource = LoadResource(m_module, resourceInfo);
  const DWORD resourceSize = SizeofResource(m_module, resourceInfo);
  auto* locked = m_resource ? static_cast<uint8_t*>(LockResource(m_resource)) : nullptr;
  bool loaded = locked && LoadDibBytes(*this, std::span<const uint8_t>(locked, resourceSize));
  if (m_resource)
    FreeResource(m_resource);
  m_resource = nullptr;
  if (!loaded)
    ClearBitmapPayload(*this);
  return loaded;
}

int DibBitmap::LoadResourceByName(const char* name)
{
  ClearBitmapPayload(*this);
  if (!m_module || !name)
    return false;
  HRSRC resourceInfo = FindResourceA(m_module, name, MAKEINTRESOURCEA(kBitmapResourceType));
  if (!resourceInfo)
    return false;
  m_resource = LoadResource(m_module, resourceInfo);
  const DWORD resourceSize = SizeofResource(m_module, resourceInfo);
  auto* locked = m_resource ? static_cast<uint8_t*>(LockResource(m_resource)) : nullptr;
  bool loaded = locked && LoadDibBytes(*this, std::span<const uint8_t>(locked, resourceSize));
  if (m_resource)
    FreeResource(m_resource);
  m_resource = nullptr;
  if (!loaded)
    ClearBitmapPayload(*this);
  return loaded;
}

int DibBitmap::LoadBmpFile(const char* path)
{
  ClearBitmapPayload(*this);
  HFILE file = _lopen(path, OF_SHARE_DENY_WRITE);
  if (file == HFILE_ERROR)
    return false;

  uint8_t rawHeader[kBmpFileHeaderSize];
  if (_lread(file, rawHeader, kBmpFileHeaderSize) != kBmpFileHeaderSize)
  {
    _lclose(file);
    return false;
  }
  int16_t magic;
  int32_t fileSize;
  uint32_t pixelOffset;
  std::memcpy(&magic, rawHeader, sizeof(magic));
  std::memcpy(&fileSize, rawHeader + 2, sizeof(fileSize));
  std::memcpy(&pixelOffset, rawHeader + 10, sizeof(pixelOffset));
  if (magic != g_bmpFileMagic)
  {
    _lclose(file);
    return false;
  }

  if (fileSize < static_cast<int32_t>(kBmpFileHeaderSize + sizeof(BITMAPINFOHEADER)) || fileSize > 256 * 1024 * 1024)
  {
    _lclose(file);
    return false;
  }
  const uint32_t remaining = static_cast<uint32_t>(fileSize) - kBmpFileHeaderSize;
  m_storage.resize(remaining);
  uint32_t offset = 0;
  while (offset < remaining)
  {
    const uint32_t chunk = std::min(kFileReadChunk, remaining - offset);
    if (_lread(file, m_storage.data() + offset, chunk) != chunk)
    {
      _lclose(file);
      m_storage.clear();
      return false;
    }
    offset += chunk;
  }
  _lclose(file);

  m_info = reinterpret_cast<BITMAPINFOHEADER*>(m_storage.data());
  const uint32_t headerSize = HeaderSize();
  if (headerSize < sizeof(BITMAPINFOHEADER) || headerSize > m_storage.size() || m_info->biWidth <= 0 ||
      m_info->biHeight == 0 || m_info->biHeight == INT32_MIN || m_info->biPlanes != 1 ||
      (m_info->biBitCount != 8 && m_info->biBitCount != 24) || (m_info->biBitCount == 8 && m_info->biClrUsed > 256) ||
      (m_info->biBitCount == 24 && m_info->biClrUsed != 0))
  {
    ClearBitmapPayload(*this);
    return false;
  }
  ComputeColorCount();
  if ((m_info->biBitCount == 8 && (m_colorCount < 1 || m_colorCount > 256)) ||
      (m_info->biBitCount == 24 && m_colorCount != 0) ||
      static_cast<size_t>(headerSize) + static_cast<size_t>(m_colorCount) * 4 > m_storage.size())
  {
    ClearBitmapPayload(*this);
    return false;
  }
  const size_t dibPixelsOffset = static_cast<size_t>(headerSize) + static_cast<size_t>(m_colorCount) * 4;
  if (pixelOffset < kBmpFileHeaderSize + dibPixelsOffset || pixelOffset - kBmpFileHeaderSize > m_storage.size())
  {
    ClearBitmapPayload(*this);
    return false;
  }
  const size_t filePixelsOffset = pixelOffset - kBmpFileHeaderSize;
  if (filePixelsOffset > dibPixelsOffset)
    m_storage.erase(m_storage.begin() + dibPixelsOffset, m_storage.begin() + filePixelsOffset);
  m_info = reinterpret_cast<BITMAPINFOHEADER*>(m_storage.data());
  if (ValidateDibPayload(*this))
    return true;
  ClearBitmapPayload(*this);
  return false;
}

HPALETTE DibBitmap::CreatePaletteFromColorTable()
{
  if (!m_info || BitCount() != 8 || m_colorCount < 1 || m_colorCount > 256)
    return nullptr;
  const auto* colorTable = reinterpret_cast<const RGBQUAD*>(reinterpret_cast<const uint8_t*>(m_info) + HeaderSize());
  LOGPALETTE logicalPalette;
  PALETTEENTRY entries[kPaletteEntryCount];
  for (int i = 0; i < kPaletteEntryCount; ++i)
  {
    const int sourceIndex = std::min(i, m_colorCount - 1);
    entries[i].peRed = colorTable[sourceIndex].rgbRed;
    entries[i].peGreen = colorTable[sourceIndex].rgbGreen;
    entries[i].peBlue = colorTable[sourceIndex].rgbBlue;
    entries[i].peFlags = 0;
  }
  logicalPalette.palVersion = 0x300;
  logicalPalette.palNumEntries = 1;
  HPALETTE palette = CreatePalette(&logicalPalette);
  if (palette && ResizePalette(palette, kPaletteEntryCount))
    SetPaletteEntries(palette, 0, kPaletteEntryCount, entries);
  return palette;
}

int DibBitmap::DrawToDc(HDC dc, char mode, DWORD rop, int destX, int destY, int srcX, int srcY, int width, int height)
{
  if (!m_info)
    return 0;
  const int bitmapWidth = Width();
  const int bitmapHeight = Height();
  const int absoluteHeight = bitmapHeight < 0 ? -bitmapHeight : bitmapHeight;
  if (width == 0)
    width = bitmapWidth - srcX;
  if (height == 0)
    height = absoluteHeight - srcY;
  if (srcX < 0)
  {
    width += srcX;
    destX -= srcX;
    srcX = 0;
  }
  if (srcY < 0)
  {
    height += srcY;
    destY -= srcY;
    srcY = 0;
  }
  if (destX < 0)
  {
    width += destX;
    srcX -= destX;
    destX = 0;
  }
  if (destY < 0)
  {
    height += destY;
    srcY -= destY;
    destY = 0;
  }
  if (srcX >= bitmapWidth || srcY >= absoluteHeight || width <= 0 || height <= 0)
    return 0;
  if (width > bitmapWidth - srcX)
    width = bitmapWidth - srcX;
  if (height > absoluteHeight - srcY)
    height = absoluteHeight - srcY;
  if (width <= 0 || height <= 0)
    return 0;
  if (mode == 0x10)
    return 1;

  if (mode != '@')
  {
    SetStretchBltMode(dc, kStretchColorOnColor);
    SetDIBitsToDevice(dc, destX, destY, width, height, srcX, srcY, 0, height, m_bits,
                      reinterpret_cast<BITMAPINFO*>(m_info), DIB_RGB_COLORS);
    return 1;
  }

  HDC memoryDc = CreateCompatibleDC(dc);
  if (BitCount() == 8 && !m_palette)
    m_palette = CreatePaletteFromColorTable();
  if (m_kind == kDibSection)
  {
    HGDIOBJ previousBitmap = SelectObject(memoryDc, DibSectionHandle());
    HPALETTE previousPalette = SelectPalette(memoryDc, m_palette, FALSE);
    BitBlt(dc, destX, destY, width, height, memoryDc, srcX, srcY, rop);
    SelectPalette(memoryDc, previousPalette, FALSE);
    SelectObject(memoryDc, previousBitmap);
  }
  else
  {
    HBITMAP deviceBitmap = nullptr;
    if (m_info)
    {
      deviceBitmap =
        CreateDIBitmap(dc, m_info, CBM_INIT, m_bits, reinterpret_cast<BITMAPINFO*>(m_info), DIB_RGB_COLORS);
    }
    HGDIOBJ toDelete = nullptr;
    if (deviceBitmap)
    {
      BITMAP bitmapInfo;
      GetObjectA(deviceBitmap, sizeof(bitmapInfo), &bitmapInfo);
      HGDIOBJ previousBitmap = SelectObject(memoryDc, deviceBitmap);
      HPALETTE previousPalette = SelectPalette(memoryDc, m_palette, FALSE);
      HPALETTE previousDcPalette = nullptr;
      if (g_displayPalette)
      {
        previousDcPalette = SelectPalette(dc, g_displayPalette, FALSE);
        RealizePalette(dc);
      }
      BitBlt(dc, destX, destY, width, height, memoryDc, srcX, srcY, rop);
      if (g_displayPalette)
        SelectPalette(dc, previousDcPalette, FALSE);
      SelectPalette(memoryDc, previousPalette, FALSE);
      toDelete = SelectObject(memoryDc, previousBitmap);
    }
    DeleteObject(toDelete);
  }
  DeleteDC(memoryDc);
  return 1;
}

int16_t g_bmpFileMagic = 19778;
