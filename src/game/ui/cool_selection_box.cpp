#include "ui/cool_selection_box.h"

#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/vptr_store.h"
#include "gfx/dib_bitmap.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_list_item.h"

namespace {

constexpr uint32_t kSelectionBoxStyle = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
constexpr uint32_t kDropDownStyle = WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN;
constexpr uint32_t kDropDownExStyle = WS_EX_TOPMOST;
constexpr COLORREF kWhite = 0xffffff;

constexpr uint32_t kButtonId = 1000;
constexpr uint32_t kDropDownId = 0x3e9;
constexpr WPARAM kOpenCommand = 0x3ea;
constexpr WPARAM kSelectionChanged = kDragListSelectionChanged;
constexpr uint32_t kListId = 0xbba;
constexpr UINT kCaptureLost = 0x405;
constexpr int kDropDownFrame = 4;
constexpr int kListInset = 2;
constexpr int kDropDownListOptions = kDragListSingleSelect | kDragListNoDrag | kDragListHoverTracking;

const char* ItemText(CoolListItem* item) { return item->m_text.m_text; }

void CopyItems(CoolListItemList* target, CoolListItemList* source) {
    for (CoolListItem* item = source->First(); item; item = item->Next()) target->Insert(NewOrNull<CoolListItem>(*item));
}

void PumpWhileMouseHeld() {
    MSG message;
    while ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 || GetCapture()) {
        if (PeekMessageA(&message, nullptr, 0, 0, PM_NOREMOVE)) {
            GetMessageA(&message, nullptr, 0, 0);
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
    }
}

LPARAM PackPoint(const POINT& point) {
    return static_cast<LPARAM>((static_cast<uint32_t>(point.y) << 16) | (static_cast<uint32_t>(point.x) & 0xffff));
}

POINT UnpackPoint(LPARAM lParam) {
    POINT point;
    point.x = static_cast<int16_t>(lParam);
    point.y = static_cast<int16_t>(lParam >> 16);
    return point;
}

// Hands a mouse message to `target` (the window under `screenPoint`) if it is a child of `hwnd`.
void ForwardToChild(HWND hwnd, HWND target, POINT screenPoint, UINT message, WPARAM wParam) {
    if (!target || target == hwnd || !IsChild(hwnd, target)) return;
    ScreenToClient(target, &screenPoint);
    SendMessageA(target, message, wParam, PackPoint(screenPoint));
}

}  // namespace

// ---- CoolSelectionBox ------------------------------------------------------------------------------------------

CoolSelectionBox::CoolSelectionBox(HINSTANCE instance, CoolWindow* parent, int x, int y, int width, int height,
                                   int visibleRows, CoolListItemList* items, DibBitmap* backdrop,
                                   uint16_t buttonBitmapId, uint16_t buttonPressedBitmapId,
                                   uint16_t scrollResourceSlot, uint16_t scrollResourceId, int sortMode)
    : CoolWindow(instance, "CoolSelectionBoxClass", kSelectionBoxStyle, x, y, width, height, parent, nullptr,
                 static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)), nullptr, nullptr, nullptr, 0, 0),
      m_items(sortMode) {
    m_textColor = kWhite;
    m_textFormat = DT_SINGLELINE | DT_VCENTER;
    m_listFillColor = 0;
    m_textOffsetX = 0;
    m_textOffsetY = 0;
    m_rowHeight = height;
    m_visibleRows = visibleRows;
    m_borderPen = static_cast<HPEN>(GetStockObject(WHITE_PEN));
    m_scrollResourceSlot = scrollResourceSlot;
    m_buttonBitmapId = buttonBitmapId;
    m_dropDownShade = LightGreyColor;
    m_current = nullptr;
    m_dropDown = nullptr;
    m_buttonPressedBitmapId = buttonPressedBitmapId;
    m_scrollResourceId = scrollResourceId;
    m_image = nullptr;
    m_backdrop = backdrop;
    if (backdrop) {
        HDC dc = GetDC(parent->m_hwnd);
        m_image = NewOrNull<DibBitmap>(static_cast<LONG>(width), static_cast<LONG>(height), m_backdrop, dc);
        ReleaseDC(parent->m_hwnd, dc);
    }
    m_dropDownTile = nullptr;
    m_screenLeft = 0;
    m_screenRight = GetSystemMetrics(SM_CXFULLSCREEN);
    m_screenTop = 0;
    m_screenBottom = GetSystemMetrics(SM_CYFULLSCREEN);
    m_sortMode = sortMode;
    if (items) {
        CopyItems(&m_items, items);
        m_current = m_items.First();
    }
}

CoolSelectionBox::~CoolSelectionBox() {
    if (m_dropDown) {
        m_dropDown->DestroyNativeWindow();
        delete m_dropDown;
        m_dropDown = nullptr;
    }
    if (m_borderPen) DeleteObject(m_borderPen);
    if (m_image) delete m_image;
    if (m_dropDownTile) delete m_dropDownTile;
    REB_KEEP_VPTR_STORE();
}

void CoolSelectionBox::PressButton() { PostMessageA(m_hwnd, WM_COMMAND, kButtonId, 0); }

CoolListItem* CoolSelectionBox::GetSelection() { return m_current; }

CoolListItem* CoolSelectionBox::Select(uint32_t id) {
    if (m_current && id != 0 && m_current->m_id != id) {
        m_current = m_items.First();
        while (m_current && m_current->m_id != id) m_current = m_current->Next();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        if (m_parent) PostMessageA(m_parent->m_hwnd, WM_COMMAND, kSelectionChanged, 0);
    }
    CloseDropDown();
    return m_current;
}

void CoolSelectionBox::SetItems(CoolListItemList* items) {
    m_items.Clear();
    if (m_dropDown) CloseDropDown();
    CopyItems(&m_items, items);
    m_current = m_items.First();
}

void CoolSelectionBox::OpenDropDown() {
    POINT origin = {0, 0};
    if (m_dropDown) return;
    ClientToScreen(m_hwnd, &origin);
    int height = m_rowHeight * m_visibleRows + kDropDownFrame;
    int top = m_clientHeight + origin.y;
    if (height + top > m_screenBottom) {
        top = origin.y - height;
        if (top < m_screenTop) {
            height = origin.y - m_screenTop;
            top = m_screenTop;
        }
    }
    m_dropDown = NewOrNull<CoolSelectionDropDown>(m_instance, this, static_cast<int>(origin.x), top, m_clientWidth,
                                                  height, m_borderPen);
    if (!m_dropDown) return;
    m_dropDown->m_childId = kDropDownId;
    m_dropDown->m_sortMode = m_sortMode;
    m_dropDown->Create(1);
    InvalidateRect(m_dropDown->m_hwnd, nullptr, FALSE);
}

void CoolSelectionBox::CloseDropDown() {
    if (!m_dropDown || g_selectionBoxClosing) return;
    g_selectionBoxClosing = 1;
    m_dropDown->DestroyNativeWindow();
    delete m_dropDown;
    m_dropDown = nullptr;
    g_selectionBoxClosing = 0;
    SetFocus(m_hwnd);
}

int CoolSelectionBox::HandleKeyUp(WPARAM key, LPARAM) {
    switch (key) {
        case VK_RETURN:
        case VK_ESCAPE:
            if (!m_dropDown) return 0;
            CloseDropDown();
            return 1;
        case VK_UP:
            if (!m_dropDown) {
                PostMessageA(m_hwnd, WM_COMMAND, kOpenCommand, 0);
                return 1;
            }
            if (m_current && m_current->Previous())
                m_current = m_current->Previous();
            else
                m_current = m_items.First();
            break;
        case VK_DOWN:
            if (!m_dropDown) {
                PostMessageA(m_hwnd, WM_COMMAND, kOpenCommand, 0);
                return 1;
            }
            if (m_current && m_current->Next()) m_current = m_current->Next();
            break;
        default:
            return 0;
    }
    m_dropDown->SyncSelection();
    return 1;
}

LRESULT __stdcall CoolSelectionBox::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            HINSTANCE instance = m_instance;
            if (m_buttonBitmapId == 0) return 0;
            DibBitmap* picture = NewOrNull<DibBitmap>(instance, static_cast<uint32_t>(m_buttonBitmapId),
                                                      static_cast<uint32_t>(kDibResource));
            // BUG(original): the size is read from the picture even when it could not be allocated.
            const int x = m_clientWidth - picture->Width();
            const int width = picture->Width();
            const int height = picture->Height();
            const int y = (m_clientHeight - height) / 2;
            if (picture) delete picture;
            CoolStrobeButton* button = NewOrNull<CoolStrobeButton>(instance, x, y, width, height, this, kButtonId,
                                                                   m_buttonBitmapId, m_buttonPressedBitmapId, m_image,
                                                                   0, 0);
            // BUG(original): a failed allocation is dereferenced here.
            button->Create(0);
            m_children.Insert(button);
            return 0;
        }
        case WM_DESTROY:
            CloseDropDown();
            return DefWindowProcA(m_hwnd, WM_DESTROY, wParam, lParam);
        case WM_PAINT: {
            HWND hwnd = m_hwnd;
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(hwnd, &paint);
            if (m_image) {
                HPALETTE palette = g_palette;
                HDC memoryDc = CreateCompatibleDC(dc);
                HGDIOBJ previousBitmap = SelectObject(memoryDc, m_image->DibSectionHandle());
                HPALETTE previousMemoryPalette = SelectPalette(memoryDc, palette, FALSE);
                HPALETTE previousPalette = SelectPalette(dc, palette, FALSE);
                m_backdrop->Blit(m_image, 0, 0, m_x, m_y, m_clientWidth, m_clientHeight);
                if (m_current) {
                    if (DibBitmap* picture = m_current->CurrentImage()) picture->DrawKeyed(m_image, 0, 0);
                    if (ItemText(m_current)) {
                        RECT area = {0, 0, m_clientWidth, m_clientHeight};
                        SelectCurrentFont(memoryDc);
                        SetBkMode(memoryDc, TRANSPARENT);
                        SetTextColor(memoryDc, m_textColor);
                        area.left += m_current->m_textOffsetX + m_textOffsetX;
                        area.top += m_current->m_textOffsetY + m_textOffsetY;
                        area.right += m_current->m_textOffsetX + m_textOffsetX;
                        area.bottom += m_current->m_textOffsetY + m_textOffsetY;
                        DrawTextA(memoryDc, ItemText(m_current), -1, &area, m_textFormat);
                        RestoreFont(memoryDc);
                    }
                }
                BitBlt(dc, 0, 0, m_clientWidth, m_clientHeight, memoryDc, 0, 0, SRCCOPY);
                SelectPalette(dc, previousPalette, FALSE);
                SelectPalette(memoryDc, previousMemoryPalette, FALSE);
                SelectObject(memoryDc, previousBitmap);
                DeleteDC(memoryDc);
            }
            EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_SHOWWINDOW:
            if (wParam == 0) CloseDropDown();
            return 0;
        case WM_MOUSEACTIVATE:
            return MA_ACTIVATE;
        case WM_KEYDOWN:
        case WM_CHAR:
        case WM_SYSKEYDOWN:
            return m_parent->WindowProc(message, wParam, lParam);
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (HandleKeyUp(wParam, lParam)) return 0;
            return m_parent->WindowProc(message, wParam, lParam);
        case WM_COMMAND:
            switch (wParam & 0xffff) {
                case kButtonId:
                    PostMessageA(m_hwnd, WM_COMMAND, kOpenCommand, 0);
                    break;
                case kDropDownId:
                    CloseDropDown();
                    break;
                case kOpenCommand:
                    if (m_dropDown) {
                        CloseDropDown();
                        break;
                    }
                    // Let the button's click finish (it holds the capture) before the drop-down takes it.
                    PumpWhileMouseHeld();
                    SetCaptureWindow(m_hwnd);
                    OpenDropDown();
                    break;
            }
            return 0;
        case kCaptureLost:
            if (static_cast<uint16_t>(wParam) == kDropDownId && !IsChild(m_hwnd, reinterpret_cast<HWND>(lParam)))
                CloseDropDown();
            return 0;
        default:
            return CoolWindow::WindowProc(message, wParam, lParam);
    }
}

// ---- CoolSelectionDropDown --------------------------------------------------------------------------------------

CoolSelectionDropDown::CoolSelectionDropDown(HINSTANCE instance, CoolSelectionBox* owner, int x, int y, int width,
                                             int height, HPEN borderPen)
    : GameWindowObject(instance, x, y, width, height, nullptr, nullptr, 0) {
    m_pen = borderPen;
    m_owner = owner;
    m_list = nullptr;
    m_image = nullptr;
    m_shade = owner->m_dropDownShade;
    m_style = kDropDownStyle;
    m_exStyle = kDropDownExStyle;
}

CoolSelectionDropDown::~CoolSelectionDropDown() {
    if (m_list) delete m_list;
    REB_KEEP_VPTR_STORE();
}

void CoolSelectionDropDown::SyncSelection() {
    CoolListItem* current = m_owner->GetSelection();
    if (m_list) {
        m_list->SelectById(current->m_id, 1);
        m_list->ScrollIntoView(current->m_id);
    }
}

void CoolSelectionDropDown::BuildBackground() {
    HWND hwnd = m_hwnd;
    HDC dc = GetDC(hwnd);
    HPALETTE palette = g_palette;
    m_image = NewOrNull<DibBitmap>(static_cast<LONG>(m_clientWidth), static_cast<LONG>(m_clientHeight),
                                   m_owner->m_image, dc);
    HGDIOBJ hollowBrush = GetStockObject(NULL_BRUSH);
    HDC memoryDc = CreateCompatibleDC(dc);
    // BUG(original): DibSectionHandle is called even when the bitmap could not be allocated, and in that case the
    // memory DC and both palettes are never restored.
    HGDIOBJ previousBitmap = SelectObject(memoryDc, m_image->DibSectionHandle());
    HPALETTE previousMemoryPalette = SelectPalette(memoryDc, palette, FALSE);
    HPALETTE previousPalette = SelectPalette(dc, palette, FALSE);
    if (m_image) {
        BitBlt(memoryDc, 0, 0, m_clientWidth, m_clientHeight, dc, 0, 0, SRCCOPY);
        if (m_shade) {
            uint8_t* table = m_image->BuildRemapTable(m_shade);
            m_image->ApplyRemapTable(0, 0, static_cast<int16_t>(m_clientWidth), static_cast<int16_t>(m_clientHeight));
            m_image->SetRemapTable(nullptr);
            operator delete(table);
        } else if (DibBitmap* tile = m_owner->m_dropDownTile) {
            if (tile->Height() == m_clientHeight) {
                for (int x = 0; x < m_clientWidth; x += tile->Width()) tile->Blit(m_image, x, 0, 0, 0, 0, 0);
            } else {
                for (int y = 0; y < m_clientHeight; y += tile->Height()) tile->Blit(m_image, 0, y, 0, 0, 0, 0);
            }
        }
        // The frame pen is swapped with the DC's pen, so m_pen ends up holding the pen it replaced.
        m_pen = SelectObject(memoryDc, m_pen);
        HGDIOBJ previousBrush = SelectObject(memoryDc, hollowBrush);
        Rectangle(memoryDc, 0, 0, m_clientWidth - 1, m_clientHeight - 1);
        SelectObject(memoryDc, previousBrush);
        SelectObject(memoryDc, m_pen);
        SelectPalette(dc, previousPalette, FALSE);
        SelectPalette(memoryDc, previousMemoryPalette, FALSE);
        SelectObject(memoryDc, previousBitmap);
        DeleteDC(memoryDc);
        SetBackground(m_image, 0, 0);
    }
    ReleaseDC(hwnd, dc);
}

LRESULT __stdcall CoolSelectionDropDown::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            HWND hwnd = m_hwnd;
            HINSTANCE instance = m_instance;
            SetCapture(hwnd);
            GetCapture();
            BuildBackground();
            const int width = m_clientWidth - kDropDownFrame;
            m_list = NewOrNull<CoolDragList>(instance, kListInset, kListInset, width, m_clientHeight - kDropDownFrame,
                                             this, GetBackground(), kListId, width, m_owner->m_rowHeight, 0, 0, 0, 0,
                                             0, 1, nullptr);
            m_list->SetSortMode(m_sortMode);
            m_list->Create(0);
            m_list->CreateVerticalScrollBar(instance, m_list, m_owner->m_scrollResourceSlot,
                                            m_owner->m_scrollResourceId, m_clientHeight, 0, m_clientHeight, 1,
                                            m_list->m_background);
            m_list->m_textOffsetX = m_owner->m_textOffsetX;
            m_list->m_textOffsetY = m_owner->m_textOffsetY;
            m_list->m_textFormat = m_owner->m_textFormat;
            m_list->SetSelectedTextColor(m_owner->m_textColor);
            m_list->SetTextColor(m_owner->m_textColor);
            m_list->m_options.m_value = kDropDownListOptions;
            m_list->m_hoverTextColor = kWhite;
            m_list->m_fillColor = m_owner->m_listFillColor;
            m_list->SetItemList(&m_owner->m_items);
            m_list->Refresh();
            if (m_owner->m_current) m_list->ScrollIntoView(m_owner->m_current->m_id);
            SetParent(hwnd, g_app ? g_app->m_hwnd : nullptr);
            GetCapture();
            return 0;
        }
        case WM_DESTROY:
            if (GetCapture() == m_hwnd) ReleaseCapture();
            return DefWindowProcA(m_hwnd, WM_DESTROY, wParam, lParam);
        case WM_PAINT: {
            HWND hwnd = m_hwnd;
            PAINTSTRUCT paint;
            Draw(BeginPaint(hwnd, &paint));
            EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_COMMAND: {
            const uint32_t command = static_cast<uint32_t>(wParam);
            if ((command & 0xffff) != kListId || (command >> 16) != kSelectionChanged) return 0;
            CoolListItem* item = m_list->FirstItem();
            while (item && (item->m_state.m_value & kListItemSelected) == 0) item = item->Next();
            if (item) m_owner->Select(item->m_id);
            return 0;
        }
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN: {
            // Copied first: the WM_COMMAND below closes the drop-down, which destroys this object.
            HWND hwnd = m_hwnd;
            POINT point = UnpackPoint(lParam);
            const POINT client = point;
            ClientToScreen(hwnd, &point);
            HWND target = WindowFromPoint(point);
            if (client.x < 0 || client.x > m_clientWidth || client.y < 0 || client.y > m_clientHeight)
                SendMessageA(m_owner->m_hwnd, WM_COMMAND, m_childId, 0);
            ForwardToChild(hwnd, target, point, message, wParam);
            return 0;
        }
        case WM_MOUSEMOVE:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONUP: {
            POINT point = UnpackPoint(lParam);
            ClientToScreen(m_hwnd, &point);
            HWND target = WindowFromPoint(point);
            if (message == WM_LBUTTONUP) GetCapture();
            ForwardToChild(m_hwnd, target, point, message, wParam);
            return 0;
        }
        case WM_CAPTURECHANGED: {
            HWND hwnd = m_hwnd;
            GetCapture();
            HWND newCapture = reinterpret_cast<HWND>(lParam);
            if (newCapture == hwnd || IsChild(hwnd, newCapture) || !newCapture) return 0;
            SendMessageA(m_owner->m_hwnd, kCaptureLost, m_childId & 0xffff, lParam);
            return 0;
        }
        default:
            return DefWindowProcA(m_hwnd, message, wParam, lParam);
    }
}

uint32_t LightGreyColor(uint32_t color) { return (color & 0xff000000) | 0xc0c0c0; }
