#include "strategy_ui/object_menu.h"

#include <cstdint>
#include <cstring>
#include <new>

#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "ui/cool_app.h"
#include "ui/game_window_object.h"

extern "C" int __cdecl atexit(void(__cdecl* function)(void));

extern const char g_gameMenuWindowClassName[];  // 0x6a8718 "Game Menu Window"

extern PopupMenuWindow* g_menuWindow;  // 0x6b28f4
extern HHOOK g_menuHook;               // 0x6b28f8
extern HWND g_menuPreviousFocus;       // 0x6b28fc
extern HWND g_menuClickFocus;          // 0x6b2900

void FUN_004422d0();

namespace {

constexpr uint32_t kResourceTypeRcData = 10;
constexpr int kRecordWords = 13;
constexpr uint32_t kSubMenuParentFlag = 8;
constexpr uint32_t kSiblingFlag = 4;
constexpr UINT kClosePopupsMessage = 0x409;
constexpr UINT kDialogClickedMessage = 0x467;
constexpr size_t kMenuWindowSize = 0x154;
constexpr int kMenuLayoutStart = 2;
constexpr int kHookNoRemove = 3;
constexpr uint32_t kSwapFlags = SWP_NOSIZE | SWP_NOMOVE;
constexpr uint32_t kFrameBitmaps[8] = {0x2779, 0x277a, 0x277b, 0x2778, 0x2775, 0x2774, 0x2777, 0x2776};
constexpr uint32_t kArrowAlliance = 0x2785;
constexpr uint32_t kArrowEmpireDelta = 0xb;
constexpr int kMenuClassNameLength = 0xff;

uint32_t DwordAt(const MenuRecord& record, int byteOffset) {
    uint32_t value;
    std::memcpy(&value, reinterpret_cast<const uint8_t*>(record.m_words) + byteOffset, sizeof(value));
    return value;
}

MenuItem* FirstItem(IdSortedLinkList* list) { return static_cast<MenuItem*>(list->m_head); }

MenuItem* NextItem(MenuItem* item) { return static_cast<MenuItem*>(item->m_next); }

}  // namespace

// ---- Menu entries --------------------------------------------------------------------------------------------

int ReadMenuRecord(uint32_t command, uint32_t kind, MenuRecord* out) {
    HMODULE module = CoolAppGetResourceModule(kind);
    int status = 0;
    HGLOBAL data = nullptr;
    if (module) {
        HRSRC info = FindResourceA(module, reinterpret_cast<LPCSTR>(command & 0xffff),
                                   reinterpret_cast<LPCSTR>(kResourceTypeRcData));
        if (!info) {
            status = 1;
        } else {
            data = LoadResource(module, info);
        }
    }
    if (data && out) {
        const uint16_t* source = static_cast<const uint16_t*>(LockResource(data));
        for (int i = 0; i < kRecordWords; ++i) out->m_words[i] = source[i];
    }
    FreeResource(data);
    return status;
}

IdSortedLinkList* AddMenuEntry(uint32_t command, uint32_t kind, uint32_t flags, int x, int y, CoolWindow* window,
                               uint32_t color, uint32_t shadowColor, uint32_t disabledColor) {
    MenuRecord record = {};
    IdSortedLinkList* list = &g_menuRoot;
    if (ReadMenuRecord(command, kind, &record) == 1) return list;
    if (record.m_words[1] != 0) {
        list = AddMenuEntry(record.m_words[1], kind, kSubMenuParentFlag, x, y, window, color, shadowColor, disabledColor);
    } else if (record.m_words[3] != 0) {
        AddMenuEntry(record.m_words[3], kind, kSiblingFlag, x, y, window, color, shadowColor, disabledColor);
    }
    MenuItem* item = static_cast<MenuItem*>(list->FindById(record.m_words[2]));
    if (!item) {
        const uint16_t pair[2] = {record.m_words[5], record.m_words[11]};
        const uint32_t enabled = (flags & 1) == 0 ? ((static_cast<uint8_t>(~flags) >> 2) & 1) : 0;
        const uint32_t checked = (flags & 2) == 2;
        item = NewOrNull<MenuItem>(command, flags, CoolAppGetResourceModule(record.m_words[12]),
                                   reinterpret_cast<const ResourceStringId*>(pair), 0, 0, 0, 0, DwordAt(record, 0x0e),
                                   DwordAt(record, 0x10), DwordAt(record, 0x12), DwordAt(record, 0x14),
                                   static_cast<uint32_t>(record.m_words[2]), window, static_cast<int>(enabled),
                                   static_cast<int>(checked));
        if (item) item->SetColors(color, shadowColor, disabledColor);
        if (list) list->AddWithId(item);
        if (!item) return list;
    }
    return &item->m_subItems;
}

void LayoutMenu(int x, int y, IdSortedLinkList* list, int* outX, int* outY, uint32_t* outWidth, int* outHeight) {
    if (!list) return;
    uint32_t maxHeight = 0;
    uint32_t maxWidth = 0;
    *outWidth = 0;
    *outHeight = 0;
    int widestText = 0;
    for (MenuItem* item = FirstItem(list); item; item = NextItem(item)) {
        const int width = item->GetPictureWidth();
        if (widestText < width) widestText = width;
    }
    for (MenuItem* item = FirstItem(list); item; item = NextItem(item)) item->SetIndent(widestText);
    for (MenuItem* item = FirstItem(list); item; item = NextItem(item)) {
        const uint32_t width = item->GetWidth();
        if (maxWidth < width) maxWidth = width;
        const uint32_t height = item->GetHeight();
        if (maxHeight < height) maxHeight = height;
    }
    int nextY = kMenuLayoutStart;
    for (MenuItem* item = FirstItem(list); item; item = NextItem(item)) {
        nextY = item->Layout(nextY, maxWidth, maxHeight);
        if (item->Virtual04()) item->TileNormalPicture();
        *outHeight += maxHeight;
    }
    *outWidth = maxWidth;
    *outHeight += kMenuLayoutStart;
    *outX = x;
    *outY = y;
}

PopupMenuWindow* BuildMenuWindow(int x, int y, IdSortedLinkList* list, HMODULE resources, PopupMenuWindow* parentMenu,
                                 CoolWindow* window, CoolWindow* owner, DibBitmap* backdrop) {
    PopupMenuWindow* menu = nullptr;
    if (list && window) {
        int menuX = x;
        int menuY;
        uint32_t menuWidth;
        int menuHeight;
        LayoutMenu(x, y, list, &menuX, &menuY, &menuWidth, &menuHeight);
        if (window->m_clientHeight < menuY + menuHeight) menuY -= menuHeight;
        if (menuX < 0) {
            menuX += menuWidth;
            if (parentMenu) menuX += parentMenu->m_width;
        } else if (window->m_clientWidth < static_cast<int>(menuWidth) + menuX) {
            menuX -= menuWidth;
            if (parentMenu) menuX -= parentMenu->m_width;
        }
        MenuItem* first = FirstItem(list);
        if (first) {
            void* memory = ::operator new(kMenuWindowSize);
            if (memory) {
                menu = new (memory) PopupMenuWindow(resources, menuX, menuY, menuWidth, menuHeight, parentMenu, window,
                                                    owner, first->m_id, backdrop);
            }
            if (menu) {
                menu->SetClassName(g_gameMenuWindowClassName);
                menu->Create(0);
                menu->BuildFrame(resources, kFrameBitmaps[0], kFrameBitmaps[1], kFrameBitmaps[2], kFrameBitmaps[3],
                                 kFrameBitmaps[4], kFrameBitmaps[5], kFrameBitmaps[6], kFrameBitmaps[7]);
            }
        }
        while (first && menu) {
            if (list->Remove(first) != 0) {
                menu->AddItem(first);
                PopupMenuWindow* sub = BuildMenuWindow(first->m_area.left, first->m_area.top, &first->m_subItems,
                                                       resources, menu, window, owner, backdrop);
                if (sub) {
                    const uint32_t arrowDelta = AppGetSide() != 1 ? kArrowEmpireDelta : 0;
                    menu->AttachSubMenu(resources, first->m_id, sub, arrowDelta + kArrowAlliance,
                                        arrowDelta + kArrowAlliance + 1);
                }
            }
            first = FirstItem(list);
        }
    }
    return menu;
}

LinkNode* GetSubItemsHead(MenuItem* item) {
    LinkNode* head = nullptr;
    if (item) head = item->m_subItems.m_head;
    return head;
}

int CleanupMenuTree(MenuItem* item) {
    int result = 0;
    if (item) {
        MenuItem* firstSub = static_cast<MenuItem*>(GetSubItemsHead(item));
        if (firstSub) item->m_enabled = CleanupMenuTree(firstSub);
        result = CleanupMenuTree(NextItem(item));
        if (result == 0) result = item->m_enabled;
    }
    return result;
}

// ---- Opening and closing the menu ------------------------------------------------------------------------

PopupMenuWindow* ShowMenu(int x, int y, HMODULE resources, CoolWindow* window, CoolWindow* owner, DibBitmap* ignored) {
    if (DismissMenu() != 0) {
        CleanupMenuTree(FirstItem(&g_menuRoot));
        g_menuWindow = BuildMenuWindow(x, y, &g_menuRoot, resources, nullptr, window, owner,
                                       static_cast<GameWindowObject*>(window)->GetBackground());
    }
    if (g_menuWindow) {
        g_menuWindow->ShowAndTrack(5);
        SetWindowPos(g_menuWindow->m_hwnd, HWND_TOP, 0, 0, 0, 0, kSwapFlags);
        g_menuWindow->InstallMouseHook();
        g_menuPreviousFocus = GetFocus();
        SetFocus(g_menuWindow->m_hwnd);
    }
    return g_menuWindow;
}

int DismissMenu() {
    PopupMenuWindow* menu = g_menuWindow;
    bool restoreFocus = false;
    const bool wasOpen = menu != nullptr;
    g_menuWindow = nullptr;
    if (wasOpen) {
        if (g_menuPreviousFocus) {
            HWND menuHwnd = menu->m_hwnd;
            if (GetFocus() == menuHwnd || GetFocus() == g_menuClickFocus) restoreFocus = true;
        }
        GetFocus();
        HWND parentHwnd = menu->m_parent->m_hwnd;
        RemoveMenuHook();
        DestroyWindow(menu->m_hwnd);
        delete menu;
        if (restoreFocus) SetFocus(g_menuPreviousFocus);
        g_menuPreviousFocus = nullptr;
        UpdateWindow(parentHwnd);
        return 1;
    }
    g_menuWindow = nullptr;
    return 1;
}

LRESULT CALLBACK MenuMouseHook(int code, WPARAM wParam, LPARAM lParam) {
    char className[kMenuClassNameLength];
    if (code >= 0 && code != kHookNoRemove) {
        const MOUSEHOOKSTRUCT* mouse = reinterpret_cast<const MOUSEHOOKSTRUCT*>(lParam);
        if (GetClassNameA(mouse->hwnd, className, kMenuClassNameLength) &&
            std::strncmp(className, g_gameMenuWindowClassName, kMenuClassNameLength) != 0) {
            if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN) DismissMenu();
        }
    }
    return CallNextHookEx(g_menuHook, code, wParam, lParam);
}

BOOL RemoveMenuHook() {
    BOOL result = TRUE;
    if (g_menuHook) {
        result = UnhookWindowsHookEx(g_menuHook);
        g_menuHook = nullptr;
    }
    return result;
}

// ---- PopupMenuWindow -----------------------------------------------------------------------------------------

PopupMenuWindow::PopupMenuWindow(HINSTANCE resources, int x, int y, int width, int height, PopupMenuWindow* parentMenu,
                                 CoolWindow* window, CoolWindow* owner, uint32_t entryId, DibBitmap* backdrop)
    : MenuWindowBase(window->m_instance, x, y, width, height, window, entryId, backdrop) {
    m_parentMenu = parentMenu;
    m_owner = owner;
}

PopupMenuWindow::~PopupMenuWindow() { REB_KEEP_VPTR_STORE(); }

void PopupMenuWindow::Hide() { Show(0); }

void PopupMenuWindow::ShowAndTrack(int command) { CoolWindow::Show(command); }

void PopupMenuWindow::PostCloseRequest() {
    if (this == g_menuWindow) {
        PostMessageA(m_hwnd, kCloseChildMessage, reinterpret_cast<WPARAM>(m_hwnd), 0);
        return;
    }
    Hide();
}

LRESULT __stdcall PopupMenuWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message != kCloseChildMessage) return MenuWindowBase::WindowProc(message, wParam, lParam);
    if (m_hwnd == reinterpret_cast<HWND>(wParam)) {
        PostMessageA(m_parent->m_hwnd, kDialogClickedMessage, 0, 0);
        DismissMenu();
    }
    return 0;
}

int PopupMenuWindow::HandleMouse(MenuItem* item, LONG x, LONG y, int message) {
    RECT area;
    GetWindowRect(m_hwnd, &area);
    POINT point = {x, y};
    MapWindowPoints(m_hwnd, nullptr, &point, 1);
    const BOOL inside = PtInRect(&area, point);
    CoolWindow* parent = m_parent;
    if (parent) {
        if (!inside) {
            PostMessageA(parent->m_hwnd, kClosePopupsMessage, 0, 0);
            return 1;
        }
        if (item && message == WM_LBUTTONUP) {
            g_menuClickFocus = GetFocus();
            const int command = item->m_command.GetAvailable();
            if (command) {
                if (m_owner) m_owner->Virtual08(command, 0, 0);
                PostMessageA(parent->m_hwnd, kClosePopupsMessage, 0, 0);
            }
        }
    }
    return 1;
}

bool PopupMenuWindow::InstallMouseHook() {
    if (RemoveMenuHook()) g_menuHook = SetWindowsHookExA(WH_MOUSE, MenuMouseHook, m_instance, GetCurrentThreadId());
    return g_menuHook != nullptr;
}

// ---- Static initializer ----------------------------------------------------------------------------------

void StaticInitMenuList() {
    InitMenuList();
    RegisterMenuListDestructor();
}

void InitMenuList() { new (&g_menuRoot) MenuItemList(); }

void RegisterMenuListDestructor() { atexit(FUN_004422d0); }
