#include "strategy_ui/order_confirm_window.h"

#include "core/checked_new.h"
#include "events/player_notices.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_label.h"
#include "ui/text_scroll_field.h"

namespace {

constexpr int kWidth = 0x1a8;
constexpr int kHeight = 0x14b;
constexpr uint32_t kStyle = 0x44000000;
constexpr uint32_t kSpriteSlot = 7;
constexpr uint32_t kBitmapKind = 2;
constexpr int kYesButton = 0x14;
constexpr int kNoButton = 0x15;
constexpr uint32_t kAllianceBackdrop = 0x2b75;
constexpr uint32_t kEmpireBackdrop = 0x2b76;
constexpr int kFontSlot = 4;

CoolStrobeButton* AddConfirmButton(OrderConfirmWindow* window, HMODULE module, int x, int y, uint32_t childId,
                            uint32_t normalId, uint32_t pressedId, int16_t tooltipId) {
    CoolStrobeButton* button = NewOrNull<CoolStrobeButton>(module, x, y, 0x33, 0x23, window, childId, normalId, pressedId,
                                                           window->GetPaintBackgroundIfOwned(), 0, 0);
    if (button) {
        button->Create(0);
        const ResourceStringId tooltip = {tooltipId, 2};
        window->AddToolFromString(button, &tooltip);
        window->m_children.Insert(button);
    }
    return button;
}

}  // namespace

OrderConfirmWindow::OrderConfirmWindow(HINSTANCE instance, GalaxyWindow* parent, uint32_t childId,
                                       Notice8* request)
    : CoolGameWindow(instance, 0, 0, kWidth, kHeight, parent, 0, kSpriteSlot, 0, childId, 0, 1, 0, 0) {
    m_galaxy = parent;
    m_style = kStyle;
    m_request = request;
    m_textField = nullptr;
    m_titleLabel = nullptr;
}

OrderConfirmWindow::~OrderConfirmWindow() {

    if (m_titleLabel) {
        m_titleLabel->~CoolLabel();
        operator delete(m_titleLabel);
    }
    if (m_textField) delete m_textField;
    if (m_request) delete m_request;
}

void OrderConfirmWindow::OnCreate1() {
    HMODULE module = CoolAppGetResourceModule(kSpriteSlot);
    CoolAppGetResourceModule(2);
    GalaxyWindow* galaxy = m_galaxy;
    CenterIn(galaxy->m_panelArea.left, galaxy->m_panelArea.top, galaxy->m_panelArea.right, galaxy->m_panelArea.bottom);
    DibBitmap* backdrop = NewOrNull<DibBitmap>(module, m_galaxy->m_side == 1 ? kAllianceBackdrop : kEmpireBackdrop, kBitmapKind);
    HDC dc = GetDC(m_hwnd);
    const int height = backdrop->Height();
    const int width = backdrop->Width();
    DibBitmap* surface = NewOrNull<DibBitmap>(width, height, backdrop);
    backdrop->Blit(surface, 0, 0, 0, 0, 0, 0);
    if (backdrop) {
        backdrop->~DibBitmap();
        operator delete(backdrop);
    }
    SetPaintBackground(surface, 10);

    m_textField = NewOrNull<TextScrollField>(m_instance, 0x18, 0xf2, 0x142, 0x46, this, GetPaintBackgroundIfOwned(), 0);
    m_textField->Create(0);
    m_textField->SetTextFont(kFontSlot);
    m_textField->m_text = m_request->m_text1;
    m_textField->m_caret.m_index = 0;
    m_textField->Reset();

    m_titleLabel = NewOrNull<CoolLabel>(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
    m_titleLabel->m_layoutFlags = kLabelFixedWidth;
    m_titleLabel->MoveTo(0x15, 0xe);
    m_titleLabel->SetFont(kFontSlot);
    m_titleLabel->m_drawFlags = 1;
    m_titleLabel->SetText(m_request->m_text0.c_str());
    HGDIOBJ previous = SelectObject(dc, surface->DibSectionHandle());
    m_titleLabel->Draw(dc);
    SelectObject(dc, previous);
    ReleaseDC(m_hwnd, dc);

    AddConfirmButton(this, module, 0x163, 0xf4, kYesButton, 0x2aae, 0x2aaf, 0x1300);
    AddConfirmButton(this, module, 0x163, 0x119, kNoButton, 0x2ab1, 0x2ab2, 0x1301);

    DibBitmap* picture = NewOrNull<DibBitmap>(module, static_cast<uint32_t>(m_request->m_picture[1]), kBitmapKind);
    if (picture) {
        picture->Blit(GetPaintBackgroundIfOwned(), 0xc, 0x1e, 0, 0, 0, 0);
        picture->~DibBitmap();
        operator delete(picture);
    }
    SetFocus(m_hwnd);
}

void OrderConfirmWindow::OnCommand(int a, int b, int c) {
    const int16_t command = static_cast<int16_t>(a);
    if (command == kYesButton) {
        UiHumanReceiveMessage(reinterpret_cast<PlayerMessage*>(m_request->m_subject), 1);
    } else {
        if (command != kNoButton) return;
        PlayerMessage* order = reinterpret_cast<PlayerMessage*>(m_request->m_subject);
        if (order) {
            delete order;
            PostCloseRequest();
            return;
        }
    }
    PostCloseRequest();
}

void OrderConfirmWindow::OnKeyDown(WPARAM key, LPARAM lParam) {
    if (key == VK_RETURN) {
        OnCommand(kYesButton, 0, 0);
    } else if (key == VK_ESCAPE) {
        OnCommand(kNoButton, 0, 0);
    }
}
