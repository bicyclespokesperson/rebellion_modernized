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
  if (!reb::compat::GetSystemMetrics(SM_MOUSEPRESENT)) {
    ShowErrorMessage(g_noMouseMessage, nullptr);
    return 0;
  }
  HDC info = reb::compat::CreateICA(g_displayDriverName, nullptr, nullptr, nullptr);
  const int width = reb::compat::GetDeviceCaps(info, HORZRES);
  const int height = reb::compat::GetDeviceCaps(info, VERTRES);
  const int planes = reb::compat::GetDeviceCaps(info, PLANES);
  unsigned int colors = DisplayColorCount(planes, reb::compat::GetDeviceCaps(info, BITSPIXEL));
  reb::compat::DeleteDC(info);
  if (colors == 1) {
    colors = kTrueColorCount;
  }
  if (width < minWidth || height < minHeight || colors < minColors) {
    ShowErrorMessage(g_displayTooSmallMessage, nullptr);
    return 0;
  }
  if (previousInstance) {
    ShowErrorMessage(g_alreadyRunningMessage, nullptr);
    return 0;
  }
  return 1;
}

void ShowErrorMessage(const char* text, const char* caption)
{
  reb::compat::MessageBeep(MB_ICONHAND);
  reb::compat::MessageBoxA(nullptr, text, caption, MB_ICONHAND | MB_SYSTEMMODAL);
}

void InitStringLoader()
{
  if (!g_stringLoaderInstalled) {
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
  if (!module) {
    return 0;
  }
  return reb::compat::LoadStringA(module, static_cast<uint16_t>(id->m_id), buffer, size) != 0;
}
