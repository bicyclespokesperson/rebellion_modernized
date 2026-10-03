#include "ui/cool_scrollable.h"

#include "core/checked_new.h"
#include "core/vptr_store.h"

namespace {

constexpr int kBarPlaceholderPosition = 1;

}  // namespace

CoolScrollable::CoolScrollable() : m_verticalBar(nullptr), m_horizontalBar(nullptr) {}

CoolScrollable::~CoolScrollable() {
    if (m_verticalBar) delete m_verticalBar;
    if (m_horizontalBar) delete m_horizontalBar;
    REB_KEEP_VPTR_STORE();
}

int CoolScrollable::PageUp() { return 0; }

int CoolScrollable::PageDown() { return 0; }

int CoolScrollable::LineUp() { return 0; }

int CoolScrollable::LineDown() { return 0; }

int CoolScrollable::ScrollVertical(int) { return 0; }

int CoolScrollable::PageLeft() { return 0; }

int CoolScrollable::PageRight() { return 0; }

int CoolScrollable::LineRight() { return 0; }

int CoolScrollable::LineLeft() { return 0; }

int CoolScrollable::ScrollHorizontal(int) { return 0; }

void CoolScrollable::OnScrollEnd() {}

int CoolScrollable::CreateVerticalScrollBar(HINSTANCE instance, CoolWindow* parent, uint16_t resourceSlot,
                                            uint16_t resourceId, int total, int first, int last, uint32_t unknownEc,
                                            DibBitmap* background) {
    int result = 0;
    m_verticalBar = NewOrNull<CoolVerticalScrollBar>(
        instance, kBarPlaceholderPosition, kBarPlaceholderPosition, kBarPlaceholderPosition, kBarPlaceholderPosition,
        parent, this, resourceSlot, resourceId, total, first, last, unknownEc);
    if (m_verticalBar) {
        m_verticalBar->Create(0);
        result = m_verticalBar->Refresh();
        m_verticalBar->SetBackground(background);
        if (result && m_horizontalBar) result = m_horizontalBar->Refresh();
    }
    return result;
}

int CoolScrollable::CreateHorizontalScrollBar(HINSTANCE instance, CoolWindow* parent, uint16_t resourceSlot,
                                              uint16_t resourceId, int total, int first, int last,
                                              uint32_t unknownEc,
                                              DibBitmap* background) {
    int result = 0;
    m_horizontalBar = NewOrNull<CoolHorizontalScrollBar>(
        instance, kBarPlaceholderPosition, kBarPlaceholderPosition, kBarPlaceholderPosition, kBarPlaceholderPosition,
        parent, this, resourceSlot, resourceId, total, first, last, unknownEc);
    if (m_horizontalBar) {
        m_horizontalBar->Create(0);
        result = m_horizontalBar->Refresh();
        m_horizontalBar->SetBackground(background);
        if (result && m_verticalBar) result = m_verticalBar->Refresh();
    }
    return result;
}

bool CoolScrollable::ShowVerticalScrollBar() {
    CoolScrollBar* bar = m_verticalBar;
    if (bar) bar->Show(SW_SHOW);
    return bar != nullptr;
}

bool CoolScrollable::HideVerticalScrollBar() {
    CoolScrollBar* bar = m_verticalBar;
    if (bar) bar->Hide();
    return bar != nullptr;
}

int CoolScrollable::SetVerticalRange(int total) { return m_verticalBar ? m_verticalBar->SetRange(total) : 0; }

int CoolScrollable::SetVerticalVisibleRange(int start, int end) {
    if (!m_verticalBar) return 0;
    if (!m_verticalBar->SetVisibleRange(start, end)) return 0;
    return m_verticalBar->Refresh();
}

int CoolScrollable::ScrollVerticalBar(int delta) {
    if (!m_verticalBar) return 0;
    if (!m_verticalBar->ScrollBy(delta)) return 0;
    return m_verticalBar->Refresh();
}

int CoolScrollable::GetVerticalVisibleRange(int* start, int* end) {
    if (!m_verticalBar) return 0;
    if ((*start = m_verticalBar->m_first) == 0) return 0;
    if ((*end = m_verticalBar->m_last) == 0) return 0;
    return 1;
}

int CoolScrollable::SetHorizontalRange(int total) { return m_horizontalBar ? m_horizontalBar->SetRange(total) : 0; }

int CoolScrollable::GetHorizontalVisibleRange(int* start, int* end) {
    if (!m_horizontalBar) return 0;
    if ((*start = m_horizontalBar->m_first) == 0) return 0;
    if ((*end = m_horizontalBar->m_last) == 0) return 0;
    return 1;
}
