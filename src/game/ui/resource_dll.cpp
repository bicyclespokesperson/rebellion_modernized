#include "ui/resource_dll.h"
#include "ui/cool_app.h"

#include <cstring>

ResourceDll::ResourceDll(const char* libraryName)
{
  m_module = LoadLibraryA(libraryName);
  if (!m_module)
    GetLastError();
}

ResourceDll::~ResourceDll()
{
  if (m_module)
    FreeLibrary(m_module);
}

ResourceDllList::ResourceDllList() = default;

ResourceDllList::~ResourceDllList() = default;

void ResourceDllList::UnloadSlot(uint32_t slot)
{
  for (IdLinkNode* node = DetachById(slot); node; node = DetachById(slot))
    delete node;
}

FontTable::FontTable(const char* faceName)
{
  std::memset(m_faceName, 0, sizeof(m_faceName));
  std::memcpy(m_faceName, faceName, std::strlen(faceName) + 1);
  std::memset(m_fonts, 0, sizeof(m_fonts));
}

FontTable::~FontTable()
{
  Clear();
}

void FontTable::Clear()
{
  for (HFONT& font : m_fonts)
  {
    if (font)
      DeleteObject(font);
    font = nullptr;
  }
}

HFONT FontTable::GetFont(uint32_t slot)
{
  if (slot >= kFontSlotCount)
    return nullptr;
  if (!m_fonts[slot])
    CreateSlotFont(static_cast<int>(slot));
  return m_fonts[slot];
}

void FontTable::CreateSlotFont(int slot)
{
  LOGFONTA font;
  std::memset(&font, 0, sizeof(font));
  font.lfQuality = PROOF_QUALITY;
  std::strncpy(font.lfFaceName, m_faceName, sizeof(font.lfFaceName) - 1);
  font.lfFaceName[sizeof(font.lfFaceName) - 1] = '\0';
  switch (slot)
  {
    case 1:
    case 5:
      font.lfHeight = 16;
      font.lfWeight = FW_BOLD;
      break;
    case 2:
      font.lfHeight = 14;
      font.lfWeight = FW_BOLD;
      break;
    case 3:
      font.lfHeight = 8;
      font.lfWeight = FW_NORMAL;
      break;
    case 6:
      font.lfHeight = 18;
      font.lfWeight = FW_NORMAL;
      break;
    case 7:
      font.lfHeight = 18;
      font.lfWeight = FW_BOLD;
      break;
    case 8:
    case 9:
      font.lfHeight = 12;
      font.lfWeight = FW_NORMAL;
      break;
    case 10:
      font.lfHeight = 14;
      font.lfWeight = FW_NORMAL;
      break;
    case 11:
      font.lfHeight = 24;
      font.lfWeight = FW_NORMAL;
      break;
    case 12:
      font.lfHeight = 30;
      font.lfWeight = FW_NORMAL;
      break;
    case 13:
      font.lfHeight = 12;
      font.lfWeight = FW_BLACK;
      break;
    default:
      font.lfHeight = 16;
      font.lfWeight = FW_NORMAL;
      break;
  }
  m_fonts[slot] = CreateFontIndirectA(&font);
}

UiResources* CurrentUiResources()
{
  return g_app ? &g_app->m_resources : nullptr;
}

HFONT CoolGetFont(int slot)
{
  auto* resources = CurrentUiResources();
  return resources ? resources->fonts.GetFont(slot) : nullptr;
}
