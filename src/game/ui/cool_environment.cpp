#include "ui/cool_environment.h"

#include "core/game_string.h"
#include "ui/cool_window.h"

namespace
{

constexpr unsigned int kTrueColorCount = 0x1000000;

// 1 << (planes * bits) as the x86 shift computes it: the count is taken modulo 32, so a 32-bit display gives 1.
unsigned int DisplayColorCount(int planes, int bitsPerPixel)
{
  return 1u << ((planes * bitsPerPixel) & 31);
}

} // namespace

int CheckDisplayRequirements(HINSTANCE previousInstance, int minWidth, int minHeight, unsigned int minColors)
{
  if (!GetSystemMetrics(SM_MOUSEPRESENT))
  {
    ShowErrorMessage(g_noMouseMessage, nullptr);
    return 0;
  }
  HDC info = CreateICA(g_displayDriverName, nullptr, nullptr, nullptr);
  const int width = GetDeviceCaps(info, HORZRES);
  const int height = GetDeviceCaps(info, VERTRES);
  const int planes = GetDeviceCaps(info, PLANES);
  unsigned int colors = DisplayColorCount(planes, GetDeviceCaps(info, BITSPIXEL));
  DeleteDC(info);
  if (colors == 1)
    colors = kTrueColorCount;
  if (width < minWidth || height < minHeight || colors < minColors)
  {
    ShowErrorMessage(g_displayTooSmallMessage, nullptr);
    return 0;
  }
  if (previousInstance)
  {
    ShowErrorMessage(g_alreadyRunningMessage, nullptr);
    return 0;
  }
  return 1;
}

void ShowErrorMessage(const char* text, const char* caption)
{
  MessageBeep(MB_ICONHAND);
  MessageBoxA(nullptr, text, caption, MB_ICONHAND | MB_SYSTEMMODAL);
}

void InitStringLoader()
{
  if (!g_stringLoaderInstalled)
  {
    InstallStringLoader();
    g_stringLoaderInstalled = 1;
  }
}

void InstallStringLoader()
{
  SetStringLoader(&LoadGameString);
}

int LoadGameString(const ResourceStringId* id, int size, LPSTR buffer)
{
  HINSTANCE module = FindResourceModule(static_cast<uint16_t>(id->m_group));
  if (!module)
    return 0;
  return LoadStringA(module, static_cast<uint16_t>(id->m_id), buffer, size) != 0;
}
