#include "strategy_ui/battle_in_progress_window.h"

#include <cstring>
#include <new>

#include "app/app_cdcheck_version.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/stream_io.h"
#include "core/vptr_store.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/galaxy_window.h"
#include "ui/cool_app.h"
#include "ui/cool_label.h"

extern const char g_battleInProgressText[];  // 0x6a879c "Battle in Progress..."

namespace {

constexpr uint32_t kStringSlot = 0;
constexpr uint32_t kBitmapSlot = 7;
constexpr uint32_t kBannerBitmapId = 0x2a11;
constexpr uint32_t kHelperBitmapKind = 10;
constexpr int kAlliance = 1;
constexpr COLORREF kAllianceColor = 0xff;
constexpr COLORREF kEmpireColor = 0xff00;

}  // namespace

BattleInProgressWindow::BattleInProgressWindow(HINSTANCE instance, GalaxyWindow* parent, uint32_t childId)
    : CoolGameWindow(instance, 0, 0, 0x17c, 200, parent, kStringSlot, kBitmapSlot, 0, childId, 0, 1, 0, 0) {
    m_galaxy = parent;
    m_style = 0x46000000;
}

BattleInProgressWindow::~BattleInProgressWindow() { REB_KEEP_VPTR_STORE(); }

void BattleInProgressWindow::SaveState(ostream* out) {
    uint32_t childId = m_childId;
    WriteValue32(out, &childId);
}

void BattleInProgressWindow::LoadState(istream* in) {
    uint32_t childId;
    ReadValue32(in, &childId);
    m_childId = childId;
}

void BattleInProgressWindow::OnCreate1() {
    HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
    CoolLabel text(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
    if (GetVersionCheckResult() == 0) {
        const ResourceStringId resourceText = {0x1a00, 2};
        text.SetTextFromResource(&resourceText);
    } else {
        CoolLabel fixedText(g_battleInProgressText, 0, 0, 0, 0, 0u, 0u);
        // BUG(original): the label is copied bit for bit, so `text` and `fixedText` share one String, which the original
        // deletes twice (and draws after the first delete). The original's small-block heap leaves a freed block
        // intact, so that is invisible there; any heap that reuses or poisons freed memory crashes in the second
        // delete. Only the first delete is dropped (`fixedText` lets go of the String), the sharing is kept.
        std::memcpy(static_cast<void*>(&text), static_cast<const void*>(&fixedText), sizeof(CoolLabel));
        fixedText.m_text = nullptr;
    }
    DibBitmap* banner = NewOrNull<DibBitmap>(art, kBannerBitmapId, kHelperBitmapKind);
    HDC windowDc = GetDC(m_hwnd);
    HDC dc = CreateCompatibleDC(windowDc);
    ReleaseDC(m_hwnd, windowDc);
    DibBitmap* background =
        NewOrNull<DibBitmap>(static_cast<LONG>(banner->Width()), static_cast<LONG>(banner->Height()), banner, dc);
    banner->Blit(background, 0, 0, 0, 0, 0, 0);
    if (banner) delete banner;
    HGDIOBJ saved = SelectObject(dc, background->DibSectionHandle());
    text.MoveTo(0, 0xe);
    text.m_width = m_width;
    text.SetFont(0xb);
    text.m_layoutFlags = kLabelFixedWidth;
    text.SetColor(m_galaxy->m_side == kAlliance ? kAllianceColor : kEmpireColor, nullptr);
    text.m_drawFlags = 1;
    text.Draw(dc);
    SelectObject(dc, saved);
    DeleteDC(dc);
    SetPaintBackground(background, 1);
    CenterIn(m_galaxy->m_panelArea.left, m_galaxy->m_panelArea.top, m_galaxy->m_panelArea.right,
             m_galaxy->m_panelArea.bottom);
}
