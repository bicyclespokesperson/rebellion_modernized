#include <cstdint>
#include <new>

#include "audio/sound_manager.h"
#include "core/reb_x87.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/galaxy_window.h"
#include "ui/cool_app.h"
#include "ui/cool_label.h"
#include "ui/custom_dialog_box.h"

namespace {

constexpr int kMinimizedSlotCount = 12;
constexpr uint32_t kMouseModeRestoring = 4;
constexpr int kStockNullBrush = 5;
constexpr int kStockWhitePen = 6;
constexpr int kZoomSteps = 4;
constexpr DWORD kZoomStepMs = 0x32;
constexpr int kMinimizeSoundId = 0x263;
constexpr int kRestoreSoundId = 0x265;
constexpr uint32_t kIconFontSlot = 10;
constexpr COLORREF kIconLabelColor = 0x200f0f0;

void PlayBarSound(int side, int firstSoundId) {
    SoundRequest request(0, 0, 0, 0, 0, 0);
    request.m_kind = 1;
    request.m_soundId = (firstSoundId - (side != 1)) & 0xffff;
    request.m_moduleIndex = 7;
    SoundManager::Play(&request);
}

void RemoveFromMinimizedOrder(uint32_t* order, int* count, uint32_t childId) {
    for (int i = 0; i < *count; ++i) {
        if (order[i] != childId) continue;
        for (int j = i + 1; j < *count; ++j) order[j - 1] = order[j];
        --*count;
        return;
    }
}

}  // namespace

int GalaxyWindow::MinimizeChildWindow(CoolWindow* window) {
    RECT from;
    RECT to;
    SetRect(&from, window->m_x, window->m_y, window->m_clientWidth + window->m_x,
            window->m_clientHeight + window->m_y);
    int result = 0;
    int index = 0;
    for (; index < kMinimizedSlotCount; ++index) {
        MinimizedWindowSlot& slot = m_minimized[index];
        if (slot.m_childId == 0) {
            CopyRect(&to, &slot.m_area);
            slot.m_iconId = 0;
            slot.m_childId = window->m_childId;
            m_minimizedOrder[m_minimizedCount++] = window->m_childId;
            result = index;
            break;
        }
        if (slot.m_childId == window->m_childId) {
            CopyRect(&to, &slot.m_area);
            result = index;
            break;
        }
    }
    if (index == kMinimizedSlotCount) {
        const uint32_t oldest = m_minimizedOrder[0];
        for (int i = 0; i < m_minimizedCount - 1; ++i) m_minimizedOrder[i] = m_minimizedOrder[i + 1];
        --m_minimizedCount;
        for (int i = 0; i < kMinimizedSlotCount; ++i) {
            MinimizedWindowSlot& slot = m_minimized[i];
            if (slot.m_childId != oldest) continue;
            m_openWindowIds.DeleteById(oldest);
            DestroyChildById(oldest);
            CopyRect(&to, &slot.m_area);
            slot.m_iconId = 0;
            slot.m_childId = window->m_childId;
            m_minimizedOrder[m_minimizedCount++] = window->m_childId;
            result = i;
            break;
        }
    }
    if (!(m_mouseMode & kMouseModeRestoring)) {
        PlayBarSound(m_side, kMinimizeSoundId);
        AnimateZoomRect(m_hwnd, &to, &from);
    }
    if (IsWindowVisible(window->m_hwnd) && !(m_mouseMode & kMouseModeRestoring)) window->Hide();
    UpdateMinimizedIcon(window);
    return result + 1;
}

void GalaxyWindow::UpdateMinimizedIcon(CoolWindow* window) {
    const int slotNumber = FindMinimizedSlot(window->m_childId);
    CustomDialogBox* dialog = static_cast<CustomDialogBox*>(window);
    ObjectKey key = dialog->GetKeyAt(0, 0);
    GameObjNotifier* object = GetObjectInFamilies_90_98(m_side, &key);
    if (slotNumber == 0) return;
    MinimizedWindowSlot& slot = m_minimized[slotNumber - 1];
    const int iconId = dialog->Virtual29();
    const RECT area = slot.m_area;
    if (iconId == 0 || static_cast<uint32_t>(iconId) == slot.m_iconId) return;

    HDC screen = GetDC(m_hwnd);
    HDC memoryDc = CreateCompatibleDC(screen);
    ReleaseDC(m_hwnd, screen);
    CoolLabel label(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0, 0);
    DibBitmap picture(CoolAppGetResourceModule(7), static_cast<uint32_t>(iconId), 2);
    DibBitmap* background = GetBackground();
    if (!slot.m_icon) {
        void* memory = ::operator new(sizeof(DibBitmap));
        slot.m_icon = memory ? new (memory) DibBitmap(area.right - area.left, area.bottom - area.top, background,
                                                      memoryDc)
                             : nullptr;
    }
    slot.m_iconId = static_cast<uint32_t>(iconId);
    background->Blit(slot.m_icon, 0, 0, area.left, area.top, area.right - area.left, area.bottom - area.top);
    picture.DrawKeyed(slot.m_icon, 0, 0);
    HGDIOBJ previousBitmap = SelectObject(memoryDc, slot.m_icon->DibSectionHandle());
    label.SetText(static_cast<GameObj*>(object)->GetName()->c_str());
    label.SetFont(kIconFontSlot);
    label.MoveTo(picture.Width(), 0);
    label.SetColor(kIconLabelColor, nullptr);
    label.Draw(memoryDc);
    SelectObject(memoryDc, previousBitmap);
    DeleteDC(memoryDc);
    InvalidateRect(m_hwnd, &area, FALSE);
}

void GalaxyWindow::RestoreMinimizedWindow(int slotNumber) {
    if (slotNumber == 0 || slotNumber > kMinimizedSlotCount) return;
    MinimizedWindowSlot& slot = m_minimized[slotNumber - 1];
    CoolWindow* window = m_children.FindById(slot.m_childId);
    if (!window) return;
    RECT from;
    RECT to;
    CopyRect(&from, &slot.m_area);
    SetRect(&to, window->m_x, window->m_y, window->m_clientWidth + window->m_x, window->m_clientHeight + window->m_y);
    PlayBarSound(m_side, kRestoreSoundId);
    AnimateZoomRect(m_hwnd, &to, &from);
    const int countBefore = m_minimizedCount;
    RemoveFromMinimizedOrder(m_minimizedOrder, &m_minimizedCount, slot.m_childId);
    if (m_minimizedCount != countBefore) InvalidateRect(m_hwnd, &slot.m_area, FALSE);
    slot.m_childId = 0;
    slot.m_iconId = 0;
    if (slot.m_icon) {
        slot.m_icon->~DibBitmap();
        ::operator delete(slot.m_icon);
        slot.m_icon = nullptr;
    }
    SendMessageA(m_hwnd, kGalaxyActivateChild, reinterpret_cast<WPARAM>(window->m_hwnd), 0);
}

void AnimateZoomRect(HWND hwnd, const RECT* to, const RECT* from) {
    int left = from->left;
    int top = from->top;
    const int deltaLeft = to->left - from->left;
    const int deltaTop = to->top - from->top;
    const float slope = deltaLeft == 0 ? 0.0f
                                       : static_cast<float>(static_cast<double>(deltaTop) /
                                                            static_cast<double>(deltaLeft));
    const int intercept = to->top - reb_ftol(static_cast<double>(to->left) * slope);
    int width = from->right - from->left;
    int height = from->bottom - from->top;
    const int stepLeft = deltaLeft / 4;
    const int stepTop = deltaTop / 4;
    const int stepWidth = ((to->right - width) - to->left) / 4;
    const int stepHeight = ((to->bottom - height) - to->top) / 4;

    HDC dc = GetDC(hwnd);
    HGDIOBJ previousBrush = SelectObject(dc, GetStockObject(kStockNullBrush));
    HGDIOBJ previousPen = SelectObject(dc, GetStockObject(kStockWhitePen));
    SetROP2(dc, R2_XORPEN);
    Rectangle(dc, left, top, left + width, top + height);
    for (int step = 0; step < kZoomSteps; ++step) {
        Rectangle(dc, left, top, left + width, top + height);
        left += stepLeft;
        top = slope == 0.0f ? top + stepTop : reb_ftol(static_cast<double>(left) * slope) + intercept;
        height += stepHeight;
        width += stepWidth;
        Rectangle(dc, left, top, left + width, top + height);
        SleepEx(kZoomStepMs, FALSE);
    }
    Rectangle(dc, left, top, left + width, top + height);
    SelectObject(dc, previousBrush);
    SelectObject(dc, previousPen);
    ReleaseDC(hwnd, dc);
}
