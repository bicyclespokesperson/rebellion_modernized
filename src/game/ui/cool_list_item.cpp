#include "ui/cool_list_item.h"

#include "core/checked_new.h"
#include "gfx/dib_bitmap.h"

namespace
{

constexpr COLORREF kWhite = 0xffffff;

DibBitmap* DuplicateBitmap(DibBitmap* source)
{
  auto* copy = NewOrNull<DibBitmap>(source->Width(), source->Height(), source);
  source->Blit(copy, 0, 0, 0, 0, 0, 0);
  return copy;
}

// Replaces `image` by RT_BITMAP `id` of `module` (none for id 0).
void ReloadImage(DibBitmap*& image, uint16_t id, HMODULE module)
{
  if (image)
    delete image;
  image =
    id != 0 ? NewOrNull<DibBitmap>(module, static_cast<uint32_t>(id), static_cast<uint32_t>(kDibResource)) : nullptr;
}

} // namespace

CoolListItem::CoolListItem(uint32_t id) : LinkNode(), m_text(), m_state(0)
{
  m_id = id;
  m_image = nullptr;
  m_selectedImage = nullptr;
  m_imageId = 0;
  m_selectedImageId = 0;
  m_x = 0;
  m_y = 0;
  m_textOffsetX = 0;
  m_textOffsetY = 0;
  m_textFormat = 0;
  m_rect = {};
  m_fontSlot = 0;
  m_userPointer = 0;
  m_ownColors = 0;
  m_selectedColor = kWhite;
  m_color = kWhite;
  m_hoverColor = kWhite;
}

CoolListItem::CoolListItem(const CoolListItem& other) : LinkNode(), m_text(), m_state(0)
{
  m_id = other.m_id;
  m_imageId = other.m_imageId;
  m_selectedImageId = other.m_selectedImageId;
  m_x = other.m_x;
  m_y = other.m_y;
  m_textOffsetX = other.m_textOffsetX;
  m_textOffsetY = other.m_textOffsetY;
  m_fontSlot = other.m_fontSlot;
  m_textFormat = other.m_textFormat;
  m_state.m_value = other.m_state.m_value;
  m_userPointer = other.m_userPointer;
  m_ownColors = other.m_ownColors;
  m_selectedColor = other.m_selectedColor;
  m_color = other.m_color;
  m_hoverColor = other.m_hoverColor;
  m_image = nullptr;
  m_selectedImage = nullptr;
  CopyRect(&m_rect, &other.m_rect);
  m_text = other.m_text;
  if (other.m_image)
    m_image = DuplicateBitmap(other.m_image);
  if (other.m_selectedImage)
    m_selectedImage = DuplicateBitmap(other.m_selectedImage);
}

CoolListItem::~CoolListItem()
{
  if (m_image)
    delete m_image;
  if (m_selectedImage)
    delete m_selectedImage;
}

DibBitmap* CoolListItem::CurrentImage()
{
  if ((m_state.m_value & kListItemDisabled) == 0 && (m_state.m_value & kListItemSelected) != 0)
    return m_selectedImage;
  return m_image;
}

void CoolListItem::SetImageIds(uint16_t imageId, uint16_t selectedImageId, HMODULE module)
{
  if (m_imageId != imageId)
  {
    m_imageId = imageId;
    ReloadImage(m_image, m_imageId, module);
  }
  if (m_selectedImageId != selectedImageId)
  {
    m_selectedImageId = selectedImageId;
    ReloadImage(m_selectedImage, m_selectedImageId, module);
  }
}

void CoolListItem::SetImages(DibBitmap* image, DibBitmap* selectedImage)
{
  if (image)
  {
    if (m_image)
      delete m_image;
    m_image = image;
  }
  if (selectedImage)
  {
    if (m_selectedImage)
      delete m_selectedImage;
    m_selectedImage = selectedImage;
  }
}
