#include <cstdint>
#include <new>

#include "core/game_string.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/panel_drag_button.h"
#include "ui/cool_button.h"
#include "ui/cool_window.h"

namespace
{

struct StrobeButtonSpec
{
  int m_x;
  int m_y;
  int m_width;
  int m_height;
  uint32_t m_commandId;
  uint32_t m_normalBitmap;
  uint32_t m_pressedBitmap;
};

struct FrameButtonSpec
{
  int m_y;
  uint32_t m_commandId;
  uint32_t m_firstFrameBitmap;
  uint32_t m_pressedBitmap;
  uint16_t m_tooltipStringId;
};

struct DragButtonSpec
{
  int m_x;
  int m_y;
  RECT m_limits;
};

constexpr int kConsoleButtonCount = 7;
constexpr int kMessageButtonCount = 9;
constexpr uint32_t kDragButtonBitmap = 0x27b8;
constexpr uint32_t kDragButtonCommand = 0x134;
constexpr int kDragButtonSize = 0x1e;
constexpr uint32_t kDragButtonFlags = kDragButtonMovable | kDragButtonDoubleClickOpens;
constexpr int kAllianceMessageButtonX = 3;
constexpr int kEmpireMessageButtonX = 0x263;
constexpr int kMessageButtonWidth = 0x1b;
constexpr int kMessageButtonHeight = 0x16;
constexpr uint32_t kMessageButtonFrames = 1;
constexpr uint32_t kDisabledBitmapState = 2;
constexpr uint32_t kConsoleSoundId = 0x260;
constexpr uint32_t kOptionsSoundId = 600;
constexpr uint32_t kFirstConsoleTooltip = 0x1500;
constexpr uint32_t kDragButtonTooltip = 0x173c;
constexpr uint32_t kStringGroup = 2;
constexpr uint32_t kWindowVisibleStyle = 0x10000000;

constexpr StrobeButtonSpec kAllianceConsole[kConsoleButtonCount] = {
  {0x6a, 0x198, 0x1b, 0x10, 0x12d, 0x2712, 0x2711},  {0x9d, 0x197, 0x1b, 0xf, 0x12e, 0x2714, 0x2713},
  {0x102, 0x195, 0x1b, 0x10, 0x12f, 0x2716, 0x2715}, {0xd1, 0x195, 0x1b, 0x10, 0x130, 0x2718, 0x2717},
  {0x18a, 0x195, 0x1b, 0x10, 0x131, 0x271a, 0x2719}, {0x1be, 0x196, 0x1b, 0x10, 0x132, 0x271c, 0x271b},
  {3, 0x163, 0x1b, 0x29, 0x133, 0x271e, 0x271d}};

constexpr StrobeButtonSpec kEmpireConsole[kConsoleButtonCount] = {
  {0x8f, 0x1b2, 0x25, 0x18, 0x12d, 0x2720, 0x271f},  {0xc7, 0x1b2, 0x25, 0x18, 0x12e, 0x2722, 0x2721},
  {0x19c, 0x1b1, 0x22, 0x16, 0x12f, 0x2724, 0x2723}, {0xfd, 0x1b1, 0x22, 0x16, 0x130, 0x2726, 0x2725},
  {0x1d1, 0x1b2, 0x23, 0x18, 0x131, 0x2728, 0x2727}, {0x207, 0x1b2, 0x25, 0x19, 0x132, 0x272a, 0x2729},
  {0x4f, 0xc0, 0x23, 0x39, 0x133, 0x272c, 0x272b}};

constexpr FrameButtonSpec kAllianceMessageButtons[kMessageButtonCount] = {
  {0x11c, 0x13d, 0x2753, 0x2749, 0x8032}, {0x135, 0x13e, 0x2754, 0x274a, 0x8015},
  {0x103, 0x13c, 0x2752, 0x2748, 0x8018}, {0xea, 0x13b, 0x2751, 0x2747, 0x8016},
  {0x86, 0x137, 0x274d, 0x2743, 0x8017},  {0x6d, 0x136, 0x274c, 0x2742, 0x8011},
  {0xd1, 0x13a, 0x2750, 0x2746, 0x8013},  {0x9f, 0x138, 0x274e, 0x2744, 0x8014},
  {0xb8, 0x139, 0x274f, 0x2745, 0x8012}};

constexpr FrameButtonSpec kEmpireMessageButtons[kMessageButtonCount] = {
  {0x11d, 0x13d, 0x273f, 0x2735, 0x8032}, {0x136, 0x13e, 0x2740, 0x2736, 0x8015},
  {0x104, 0x13c, 0x273e, 0x2734, 0x8018}, {0xeb, 0x13b, 0x273d, 0x2733, 0x8016},
  {0x87, 0x137, 0x2739, 0x272f, 0x8017},  {0x6e, 0x136, 0x2738, 0x272e, 0x8011},
  {0xd2, 0x13a, 0x273c, 0x2732, 0x8013},  {0xa0, 0x138, 0x273a, 0x2730, 0x8014},
  {0xb9, 0x139, 0x273b, 0x2731, 0x8012}};

constexpr DragButtonSpec kAllianceDragButton = {0x37, 0x32, {0x37, 0x28, 0x21c, 0x186}};
constexpr DragButtonSpec kEmpireDragButton = {0x71, 0x32, {0x78, 0x28, 600, 0x18b}};

constexpr uint32_t kConsoleTooltips[kConsoleButtonCount] = {0x1501, 0x1502, 0x1503, 0x1504, 0x1505, 0x1506, 0x1500};

HWND SendToBottom()
{
  return HWND_BOTTOM;
}

} // namespace

void GalaxyWindow::CreateConsoleButtons(HMODULE module)
{
  const bool alliance = m_side == 1;
  if (!alliance && m_side != 2)
  {
    if (m_unknownButton150)
    {
      m_unknownButton150->ClearStateFlags(kButtonPixelHit);
      m_unknownButton150->m_background = static_cast<HBRUSH>(GetStockObject(5));
      m_unknownButton150->Create(0);
    }
    return;
  }
  const StrobeButtonSpec* console = alliance ? kAllianceConsole : kEmpireConsole;
  const FrameButtonSpec* frames = alliance ? kAllianceMessageButtons : kEmpireMessageButtons;
  const DragButtonSpec& drag = alliance ? kAllianceDragButton : kEmpireDragButton;

  for (int i = 0; i < kConsoleButtonCount; ++i)
  {
    const StrobeButtonSpec& spec = console[i];
    void* memory = ::operator new(sizeof(CoolStrobeButton));
    m_consoleButtons[i] =
      memory ? new (memory)
                 CoolStrobeButton(module, spec.m_x, spec.m_y, spec.m_width, spec.m_height, this, spec.m_commandId,
                                  spec.m_normalBitmap, spec.m_pressedBitmap, GetBackground(), 0, 0) :
               nullptr;
  }
  void* dragMemory = ::operator new(sizeof(PanelDragButton));
  m_gidLegendIcon = dragMemory ? new (dragMemory)
                                   PanelDragButton(module, drag.m_x, drag.m_y, kDragButtonSize, kDragButtonSize, this,
                                                   kDragButtonCommand, kDragButtonBitmap, kDragButtonBitmap,
                                                   GetBackground(), drag.m_limits.left, drag.m_limits.top,
                                                   drag.m_limits.right, drag.m_limits.bottom, kDragButtonFlags) :
                                 nullptr;
  const int frameX = alliance ? kAllianceMessageButtonX : kEmpireMessageButtonX;
  for (int i = 0; i < kMessageButtonCount; ++i)
  {
    const FrameButtonSpec& spec = frames[i];
    void* memory = ::operator new(sizeof(CoolFrameButton));
    CoolFrameButton* button =
      memory ? new (memory) CoolFrameButton(module, frameX, spec.m_y, kMessageButtonWidth, kMessageButtonHeight, this,
                                            spec.m_commandId, spec.m_firstFrameBitmap, spec.m_pressedBitmap,
                                            kMessageButtonFrames, GetBackground(), 0, 0) :
               nullptr;
    m_messageButtons[i] = button;
    if (!button)
      continue;
    const ResourceStringId tooltip{static_cast<int16_t>(spec.m_tooltipStringId), kStringGroup};
    button->SetStateFlags(kButtonHighlight);
    button->LoadStateBitmap(kDisabledBitmapState, spec.m_pressedBitmap);
    button->Create(0);
    // The first Alliance button clears the pixel hit flag before registering its tooltip, all the others after.
    const bool clearFirst = alliance && i == 0;
    if (clearFirst)
      button->ClearStateFlags(kButtonPixelHit);
    AddToolFromString(button, &tooltip);
    if (!clearFirst)
      button->ClearStateFlags(kButtonPixelHit);
  }
  if (m_unknownButton150)
  {
    m_unknownButton150->ClearStateFlags(kButtonPixelHit);
    m_unknownButton150->m_background = static_cast<HBRUSH>(GetStockObject(5));
    m_unknownButton150->Create(0);
  }
}

void GalaxyWindow::CreateConsoleButtonWindows(HMODULE stringModule)
{
  g_stringModule = stringModule;
  for (int i = 0; i < kConsoleButtonCount; ++i)
  {
    CoolStrobeButton* button = m_consoleButtons[i];
    if (!button)
      continue;
    button->Create(0);
    button->SetClickSound(i == 5 ? kOptionsSoundId : kConsoleSoundId);
    SetWindowPos(button->m_hwnd, SendToBottom(), 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
    AddToolFromResource(button, kConsoleTooltips[i]);
  }
  if (m_gidLegendIcon)
  {
    m_gidLegendIcon->m_style &= ~kWindowVisibleStyle;
    m_gidLegendIcon->Create(0);
    SetWindowPos(m_gidLegendIcon->m_hwnd, SendToBottom(), 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
    AddToolFromResource(m_gidLegendIcon, kDragButtonTooltip);
  }
}
