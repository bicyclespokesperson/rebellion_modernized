#include "strategy_ui/object_menu.h"

#include <cstdint>
#include <cstring>
#include <new>
#include <ranges>

#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "ui/cool_app.h"
#include "ui/game_window_object.h"

extern const char* g_gameMenuWindowClassName; // 0x6a8718 "Game Menu Window"

extern PopupMenuWindow* g_menuWindow; // 0x6b28f4
extern HHOOK g_menuHook;              // 0x6b28f8
extern HWND g_menuPreviousFocus;      // 0x6b28fc
extern HWND g_menuClickFocus;         // 0x6b2900

namespace
{

constexpr uint32_t kResourceTypeRcData = 10;
constexpr int kRecordWords = 13;
constexpr uint32_t kSubMenuParentFlag = 8;
constexpr uint32_t kSiblingFlag = 4;
constexpr UINT kClosePopupsMessage = 0x409;
constexpr UINT kDialogClickedMessage = 0x467;
constexpr int kMenuLayoutStart = 2;
constexpr int kHookNoRemove = 3;
constexpr uint32_t kSwapFlags = SWP_NOSIZE | SWP_NOMOVE;
constexpr uint32_t kFrameBitmaps[8] = {0x2779, 0x277a, 0x277b, 0x2778, 0x2775, 0x2774, 0x2777, 0x2776};
constexpr uint32_t kArrowAlliance = 0x2785;
constexpr uint32_t kArrowEmpireDelta = 0xb;
constexpr int kMenuClassNameLength = 0xff;

uint32_t DwordAt(const MenuRecord& record, int byteOffset)
{
  uint32_t value;
  std::memcpy(&value, reinterpret_cast<const uint8_t*>(record.m_words) + byteOffset, sizeof(value));
  return value;
}

} // namespace

// ---- Menu entries --------------------------------------------------------------------------------------------

int ReadMenuRecord(uint32_t command, uint32_t kind, MenuRecord* out)
{
  HMODULE module = CoolAppGetResourceModule(kind);
  int status = 0;
  HGLOBAL data = nullptr;
  if (module) {
    HRSRC info = reb::compat::FindResourceA(module, MAKEINTRESOURCEA(command & 0xffff), MAKEINTRESOURCEA(kResourceTypeRcData));
    if (!info) {
      status = 1;
    } else {
      data = reb::compat::LoadResource(module, info);
    }
  }
  if (data && out) {
    const auto* source = static_cast<const uint16_t*>(reb::compat::LockResource(data));
    for (int i = 0; i < kRecordWords; ++i) {
      out->m_words[i] = source[i];
    }
  }
  reb::compat::FreeResource(data);
  return status;
}

MenuItemList* AddMenuEntry(uint32_t command, uint32_t kind, uint32_t flags, int x, int y, CoolWindow* window,
                           uint32_t color, uint32_t shadowColor, uint32_t disabledColor)
{
  MenuRecord record = {};
  MenuItemList* list = &g_menuRoot;
  if (ReadMenuRecord(command, kind, &record) == 1) {
    return list;
  }
  if (record.m_words[1] != 0) {
    list = AddMenuEntry(record.m_words[1], kind, kSubMenuParentFlag, x, y, window, color, shadowColor, disabledColor);
  } else if (record.m_words[3] != 0) {
    AddMenuEntry(record.m_words[3], kind, kSiblingFlag, x, y, window, color, shadowColor, disabledColor);
  }
  if (!list) {
    return nullptr;
  }
  auto* item = static_cast<MenuItem*>(list->FindById(record.m_words[2]));
  if (!item) {
    const uint16_t pair[2] = {record.m_words[5], record.m_words[11]};
    const uint32_t enabled = (flags & 1) == 0 ? ((static_cast<uint8_t>(~flags) >> 2) & 1) : 0;
    const uint32_t checked = (flags & 2) == 2;
    item = NewOrNull<MenuItem>(
        command, flags, CoolAppGetResourceModule(record.m_words[12]), reinterpret_cast<const ResourceStringId*>(pair),
        0, 0, 0, 0, DwordAt(record, 0x0e), DwordAt(record, 0x10), DwordAt(record, 0x12), DwordAt(record, 0x14),
        static_cast<uint32_t>(record.m_words[2]), window, static_cast<int>(enabled), static_cast<int>(checked));
    if (item) {
      item->SetColors(color, shadowColor, disabledColor);
    }
    if (!item) {
      return list;
    }
    try {
      if (list->AddWithId(item) == 0) {
        delete item;
        return list;
      }
    } catch (...) {
      delete item;
      throw;
    }
  }
  return &item->m_subItems;
}

void LayoutMenu(int x, int y, MenuItemList* list, int* outX, int* outY, uint32_t* outWidth, int* outHeight)
{
  if (!list) {
    return;
  }
  uint32_t maxHeight = 0;
  uint32_t maxWidth = 0;
  *outWidth = 0;
  *outHeight = 0;
  int widestText = 0;
  for (const auto& ownedItem : list->Entries()) {
    auto* item = static_cast<MenuItem*>(ownedItem.get());
    const int width = item->GetPictureWidth();
    if (widestText < width) {
      widestText = width;
    }
  }
  for (const auto& ownedItem : list->Entries()) {
    static_cast<MenuItem*>(ownedItem.get())->SetIndent(widestText);
  }
  for (const auto& ownedItem : list->Entries()) {
    auto* item = static_cast<MenuItem*>(ownedItem.get());
    const uint32_t width = item->GetWidth();
    if (maxWidth < width) {
      maxWidth = width;
    }
    const uint32_t height = item->GetHeight();
    if (maxHeight < height) {
      maxHeight = height;
    }
  }
  int nextY = kMenuLayoutStart;
  for (const auto& ownedItem : list->Entries()) {
    auto* item = static_cast<MenuItem*>(ownedItem.get());
    nextY = item->Layout(nextY, maxWidth, maxHeight);
    if (item->HasSeparatorAfter()) {
      item->TileNormalPicture();
    }
    *outHeight += maxHeight;
  }
  *outWidth = maxWidth;
  *outHeight += kMenuLayoutStart;
  *outX = x;
  *outY = y;
}

PopupMenuWindow* BuildMenuWindow(int x, int y, MenuItemList* list, HMODULE resources, PopupMenuWindow* parentMenu,
                                 CoolWindow* window, CoolWindow* owner, DibBitmap* backdrop)
{
  PopupMenuWindow* menu = nullptr;
  if (list && window) {
    int menuX = x;
    int menuY;
    uint32_t menuWidth;
    int menuHeight;
    LayoutMenu(x, y, list, &menuX, &menuY, &menuWidth, &menuHeight);
    if (window->m_clientHeight < menuY + menuHeight) {
      menuY -= menuHeight;
    }
    if (menuX < 0) {
      menuX += menuWidth;
      if (parentMenu) {
        menuX += parentMenu->m_width;
      }
    } else if (window->m_clientWidth < static_cast<int>(menuWidth) + menuX) {
      menuX -= menuWidth;
      if (parentMenu) {
        menuX -= parentMenu->m_width;
      }
    }
    MenuItem* first = list->Entries().empty() ? nullptr : static_cast<MenuItem*>((*list->Entries().begin()).get());
    if (first) {
      void* memory = ::operator new(sizeof(PopupMenuWindow));
      if (memory) {
        menu = new (memory) PopupMenuWindow(resources, menuX, menuY, menuWidth, menuHeight, parentMenu, window, owner,
                                            first->m_id, backdrop);
      }
      if (menu) {
        menu->SetClassName(g_gameMenuWindowClassName);
        menu->Create(0);
        menu->BuildFrame(resources, kFrameBitmaps[0], kFrameBitmaps[1], kFrameBitmaps[2], kFrameBitmaps[3],
                         kFrameBitmaps[4], kFrameBitmaps[5], kFrameBitmaps[6], kFrameBitmaps[7]);
      }
    }
    while (first && menu) {
      if (list->Remove(first) != 0 && menu->AddItem(first)) {
        PopupMenuWindow* sub = BuildMenuWindow(first->m_area.left, first->m_area.top, &first->m_subItems, resources,
                                               menu, window, owner, backdrop);
        if (sub) {
          const uint32_t arrowDelta = AppGetSide() != 1 ? kArrowEmpireDelta : 0;
          menu->AttachSubMenu(resources, first->m_id, sub, arrowDelta + kArrowAlliance,
                              arrowDelta + kArrowAlliance + 1);
        }
      }
      first = list->Entries().empty() ? nullptr : static_cast<MenuItem*>((*list->Entries().begin()).get());
    }
  }
  return menu;
}

int CleanupMenuTree(MenuItemList& items)
{
  int result = 0;
  for (const auto& ownedItem : std::views::reverse(items.Entries())) {
    auto* item = static_cast<MenuItem*>(ownedItem.get());
    if (!item->m_subItems.Entries().empty()) {
      item->m_enabled = CleanupMenuTree(item->m_subItems);
    }
    if (result == 0) {
      result = item->m_enabled;
    }
  }
  return result;
}

// ---- Opening and closing the menu ------------------------------------------------------------------------

PopupMenuWindow* ShowMenu(int x, int y, HMODULE resources, CoolWindow* window, CoolWindow* owner, DibBitmap* ignored)
{
  if (DismissMenu() != 0) {
    CleanupMenuTree(g_menuRoot);
    g_menuWindow = BuildMenuWindow(x, y, &g_menuRoot, resources, nullptr, window, owner,
                                   static_cast<GameWindowObject*>(window)->GetBackground());
  }
  if (g_menuWindow) {
    g_menuWindow->ShowAndTrack(5);
    reb::compat::SetWindowPos(g_menuWindow->m_hwnd, HWND_TOP, 0, 0, 0, 0, kSwapFlags);
    g_menuWindow->InstallMouseHook();
    g_menuPreviousFocus = reb::compat::GetFocus();
    reb::compat::SetFocus(g_menuWindow->m_hwnd);
  }
  return g_menuWindow;
}

int DismissMenu()
{
  PopupMenuWindow* menu = g_menuWindow;
  bool restoreFocus = false;
  const bool wasOpen = menu != nullptr;
  g_menuWindow = nullptr;
  if (wasOpen) {
    if (g_menuPreviousFocus) {
      HWND menuHwnd = menu->m_hwnd;
      if (reb::compat::GetFocus() == menuHwnd || reb::compat::GetFocus() == g_menuClickFocus) {
        restoreFocus = true;
      }
    }
    reb::compat::GetFocus();
    HWND parentHwnd = menu->m_parent->m_hwnd;
    RemoveMenuHook();
    reb::compat::DestroyWindow(menu->m_hwnd);
    delete menu;
    if (restoreFocus) {
      reb::compat::SetFocus(g_menuPreviousFocus);
    }
    g_menuPreviousFocus = nullptr;
    reb::compat::UpdateWindow(parentHwnd);
    return 1;
  }
  g_menuWindow = nullptr;
  return 1;
}

LRESULT CALLBACK MenuMouseHook(int code, WPARAM wParam, LPARAM lParam)
{
  char className[kMenuClassNameLength];
  if (code >= 0 && code != kHookNoRemove) {
    const auto* mouse = reinterpret_cast<const MOUSEHOOKSTRUCT*>(lParam);
    if (reb::compat::GetClassNameA(mouse->hwnd, className, kMenuClassNameLength)
        && std::strncmp(className, g_gameMenuWindowClassName, kMenuClassNameLength) != 0) {
      if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN) {
        DismissMenu();
      }
    }
  }
  return reb::compat::CallNextHookEx(g_menuHook, code, wParam, lParam);
}

BOOL RemoveMenuHook()
{
  BOOL result = TRUE;
  if (g_menuHook) {
    result = reb::compat::UnhookWindowsHookEx(g_menuHook);
    g_menuHook = nullptr;
  }
  return result;
}

// ---- PopupMenuWindow -----------------------------------------------------------------------------------------

PopupMenuWindow::PopupMenuWindow(HINSTANCE resources, int x, int y, int width, int height, PopupMenuWindow* parentMenu,
                                 CoolWindow* window, CoolWindow* owner, uint32_t entryId, DibBitmap* backdrop)
    : MenuWindowBase(window->m_instance, x, y, width, height, window, entryId, backdrop)
{
  m_parentMenu = parentMenu;
  m_owner = owner;
}

PopupMenuWindow::~PopupMenuWindow() = default;

void PopupMenuWindow::Hide()
{
  Show(0);
}

void PopupMenuWindow::ShowAndTrack(int command)
{
  CoolWindow::Show(command);
}

void PopupMenuWindow::PostCloseRequest()
{
  if (this == g_menuWindow) {
    reb::compat::PostMessageA(m_hwnd, kCloseChildMessage, reinterpret_cast<WPARAM>(m_hwnd), 0);
    return;
  }
  Hide();
}

LRESULT PopupMenuWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message != kCloseChildMessage) {
    return MenuWindowBase::WindowProc(message, wParam, lParam);
  }
  if (m_hwnd == reinterpret_cast<HWND>(wParam)) {
    reb::compat::PostMessageA(m_parent->m_hwnd, kDialogClickedMessage, 0, 0);
    DismissMenu();
  }
  return 0;
}

int PopupMenuWindow::HandleMouse(MenuItem* item, LONG x, LONG y, int message)
{
  RECT area;
  reb::compat::GetWindowRect(m_hwnd, &area);
  POINT point = {x, y};
  reb::compat::MapWindowPoints(m_hwnd, nullptr, &point, 1);
  const BOOL inside = reb::compat::PtInRect(&area, point);
  CoolWindow* parent = m_parent;
  if (parent) {
    if (!inside) {
      reb::compat::PostMessageA(parent->m_hwnd, kClosePopupsMessage, 0, 0);
      return 1;
    }
    if (item && message == WM_LBUTTONUP) {
      g_menuClickFocus = reb::compat::GetFocus();
      const int command = item->m_command.GetAvailable();
      if (command) {
        if (m_owner) {
          m_owner->OnMenuCommand(command, 0, 0);
        }
        reb::compat::PostMessageA(parent->m_hwnd, kClosePopupsMessage, 0, 0);
      }
    }
  }
  return 1;
}

bool PopupMenuWindow::InstallMouseHook()
{
  if (RemoveMenuHook()) {
    g_menuHook = reb::compat::SetWindowsHookExA(WH_MOUSE, MenuMouseHook, m_instance, GetCurrentThreadId());
  }
  return g_menuHook != nullptr;
}

HHOOK g_menuHook{};
HWND g_menuClickFocus{};
HWND g_menuPreviousFocus{};
const char* g_gameMenuWindowClassName = nullptr;
