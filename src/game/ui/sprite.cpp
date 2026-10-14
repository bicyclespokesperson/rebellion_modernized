#include "ui/sprite.h"

#include <cstring>

#include "gfx/dib_bitmap.h"

namespace
{

constexpr uint16_t kDeltaFrameResourceType = 302;

// The resource header has 16-bit fields at odd offsets.
template <class T>
T ReadUnaligned(const uint8_t* at)
{
  T value;
  std::memcpy(&value, at, sizeof(value));
  return value;
}

} // namespace

DeltaFrame::DeltaFrame(HMODULE module, uint32_t resourceId)
{
  m_width = 0;
  m_height = 0;
  m_dataSize = 0;
  m_flag = 0;
  m_left = 0;
  m_right = 0;
  m_top = 0;
  m_bottom = 0;
  m_rows = nullptr;
  m_data = nullptr;
  m_unk38 = 0;
  Load(module, resourceId);
}

DeltaFrame::~DeltaFrame()
{
  if (m_rows)
    operator delete(m_rows);
  if (m_data)
    operator delete(m_data);
}

void DeltaFrame::Load(HMODULE module, uint32_t resourceId)
{
  Reset();
  HRSRC info = FindResourceA(module, MAKEINTRESOURCEA(resourceId & 0xffff), MAKEINTRESOURCEA(kDeltaFrameResourceType));
  if (!info)
    return;
  HGLOBAL resource = LoadResource(module, info);
  if (!resource)
    return;
  Parse(resource);
}

void DeltaFrame::Parse(HGLOBAL resource)
{
  const auto* bytes = static_cast<const uint8_t*>(LockResource(resource));
  if (!bytes)
    return;
  m_width = ReadUnaligned<int16_t>(bytes + 0x0);
  m_height = ReadUnaligned<int16_t>(bytes + 0x2);
  m_dataSize = ReadUnaligned<uint32_t>(bytes + 0x4);
  m_flag = bytes[0x8];
  m_left = ReadUnaligned<int16_t>(bytes + 0x9);
  m_top = ReadUnaligned<int16_t>(bytes + 0xb);
  m_right = ReadUnaligned<int16_t>(bytes + 0xd);
  m_bottom = ReadUnaligned<int16_t>(bytes + 0xf);
  const uint8_t* source = bytes + 0x11;
  m_data = static_cast<uint8_t*>(operator new(m_dataSize));
  m_rows = static_cast<uint8_t**>(operator new(m_height * sizeof(uint8_t*)));
  for (int row = 0; row < m_height; ++row)
  {
    m_rows[row] = m_data + ReadUnaligned<int32_t>(source);
    source += sizeof(int32_t);
  }
  std::memcpy(m_data, source, m_dataSize);
  FreeResource(resource);
  ComputeBounds();
}

int DeltaFrame::ApplyTo(DibBitmap* target)
{
  if (!target || !target->IsLoaded())
    return 0;
  if (target->Width() != static_cast<uint16_t>(m_width))
    return 0;
  if (target->Height() != static_cast<uint16_t>(m_height))
    return 0;
  if (target->BitCount() != 8)
    return 0;
  if (target->Compression() != 0)
    return 0;
  target->NotifyPixelsChanged();
  uint8_t* bits = target->m_bits;
  const int stride = target->RowStride();
  for (int row = 0; row < m_height; ++row)
  {
    const uint8_t* runs = m_rows[row];
    uint8_t* dest = bits + target->RowIndex(row) * stride;
    int skipping = 1;
    int in = 0;
    for (int x = 0; x < m_width;)
    {
      const int count = runs[in++];
      if (skipping)
      {
        skipping = 0;
      }
      else
      {
        for (int i = 0; i < count; ++i)
          dest[i] += runs[in + i];
        in += count;
        skipping = 1;
      }
      dest += count;
      x += count;
    }
  }
  return 1;
}

void DeltaFrame::Reset()
{
  m_width = 0;
  m_height = 0;
  m_dataSize = 0;
  m_flag = 0;
  if (m_rows)
    operator delete(m_rows);
  m_rows = nullptr;
  if (m_data)
    operator delete(m_data);
  m_data = nullptr;
  m_unk38 = 0;
}

void DeltaFrame::ComputeBounds()
{
  m_right = 0;
  m_bottom = 0;
  const int16_t width = m_width;
  int16_t rowNumber = m_height;
  m_left = width;
  m_top = rowNumber;
  if (rowNumber > 0)
  {
    uint8_t** rows = m_rows;
    for (int remaining = rowNumber; remaining != 0; --remaining, --rowNumber, ++rows)
    {
      const uint8_t* runs = *rows;
      const int16_t firstSkip = runs[0];
      int16_t x = 0;
      int16_t end = 0;
      int skipping = 1;
      if (width > 0)
      {
        do
        {
          const uint16_t count = *runs++;
          x += count;
          if (skipping)
          {
            skipping = 0;
          }
          else
          {
            if (count != 0)
              end = x;
            runs += static_cast<int16_t>(count);
            skipping = 1;
            if (rowNumber < m_top)
              m_top = rowNumber;
            if (rowNumber > m_bottom)
              m_bottom = rowNumber;
          }
        } while (x < width);
      }
      if (firstSkip < m_left)
        m_left = firstSkip;
      if (end > m_right)
        m_right = end;
    }
  }
  ++m_right;
  ++m_bottom;
}

void DeltaFrame::GetBounds(RECT* rect, int replace)
{
  if (replace)
  {
    rect->left = m_left;
    rect->right = m_right;
    rect->top = m_top;
    rect->bottom = m_bottom;
    return;
  }
  if (m_left < rect->left)
    rect->left = m_left;
  if (m_right > rect->right)
    rect->right = m_right;
  if (m_bottom > rect->bottom)
    rect->bottom = m_bottom;
  if (m_top < rect->top)
    rect->top = m_top;
}

Sprite::Sprite(DibBitmap* bitmap)
{
  m_id = 0;
  m_x = 0;
  m_y = 0;
  m_depth = 0;
  m_unk28 = 0;
  m_bitmap = bitmap;
  m_remapTable = nullptr;
  m_drawMode = kSpriteKeyed;
}

Sprite::~Sprite() = default;

DibBitmap* Sprite::GetBitmap()
{
  return m_bitmap;
}

void Sprite::SetBitmap(DibBitmap* bitmap)
{
  m_bitmap = bitmap;
}

int Sprite::HitTest(int x, int y)
{
  int hit = 0;
  if (m_bitmap && m_bitmap->IsLoaded())
    hit = m_bitmap->IsPixelOpaque(x - m_x, y - m_y);
  return hit;
}

int Sprite::DrawOnto(DibBitmap* dest, int originX, int originY, RECT* drawn)
{
  const int x = m_x - originX;
  const int y = m_y - originY;
  if (x < 0 || x >= dest->Width() || y < 0 || y >= dest->Height())
    return 0;
  if (m_drawMode == kSpriteHidden || !m_bitmap || !m_bitmap->IsLoaded())
    return 0;
  drawn->left = x;
  drawn->top = y;
  drawn->right = m_bitmap->Width() + x;
  drawn->bottom = m_bitmap->Height() + y;
  switch (m_drawMode)
  {
    case kSpriteRemapDestination:
      m_bitmap->SetRemapTable(m_remapTable);
      m_bitmap->BlitRemapDestination(dest, x, y, 0, 0, 0, 0);
      break;
    case kSpriteRemapped:
      m_bitmap->SetRemapTable(m_remapTable);
      m_bitmap->BlitRemapped(dest, x, y, 0, 0, 0, 0);
      break;
    case kSpriteRemappedOverlay:
      m_bitmap->SetRemapTable(m_remapTable);
      m_bitmap->BlitRemappedOverlay(dest, x, y, 0, 0, 0, 0);
      break;
    default:
      m_bitmap->DrawKeyed(dest, x, y);
      break;
  }
  return 1;
}
