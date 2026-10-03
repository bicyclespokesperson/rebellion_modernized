#include "app/app_dialogs.h"

#include "core/checked_new.h"
#include "core/vptr_store.h"

namespace {

constexpr int kDialogWidth = 0x19c;
constexpr int kDialogHeight = 0xb0;
constexpr uint32_t kResourceSlot = 0xc;
constexpr uint32_t kDialogChildId = 0xd;
constexpr uint32_t kDialogStyle = WS_CHILD | WS_CLIPSIBLINGS;
constexpr uint32_t kQuestionBackgroundId = 0x297f;
constexpr uint32_t kNoticeBackgroundId = 0x297e;
constexpr uint32_t kWaitBackgroundId = 0x297d;
constexpr uint32_t kWaitMessage = 0x472;
constexpr UINT kDialogOpenedMessage = 0x479;

constexpr uint32_t kYesButtonId = 0x65;
constexpr uint32_t kNoButtonId = 0x66;
constexpr uint32_t kAcknowledgeButtonId = 0x65;
constexpr uint32_t kButtonBitmapId = 0x2981;
constexpr uint32_t kButtonPressedBitmapId = 0x2980;
constexpr uint32_t kNoButtonBitmapId = 0x2983;
constexpr uint32_t kNoPressedBitmapId = 0x2982;
constexpr UINT kYesTooltipId = 0x1300;
constexpr UINT kNoTooltipId = 0x1301;

constexpr int kTextFontSlot = 5;
constexpr COLORREF kTextColor = 0x02f0fbff;
constexpr UINT kHeadlineFormat = DT_CENTER | DT_WORDBREAK;
constexpr UINT kDetailFormat = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
constexpr RECT kHeadlineArea = {0x2b, 0x19, 0x170, 0x56};
constexpr RECT kDetailBelowHeadlineArea = {0x2b, 0x57, 0x170, 0x70};
constexpr RECT kDetailAloneArea = {0x2b, 0x19, 0x170, 0x70};

void DeleteAndForgetDialog() {
    if (g_appDialog) delete g_appDialog;
    g_appDialog = nullptr;
}

}  // namespace

// ---- AppDialog -----------------------------------------------------------------------------------------------

AppDialog::AppDialog(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent, uint32_t stringSlot,
                     uint32_t bitmapSlot, uint32_t backgroundId, uint32_t childId, uint32_t style, uint32_t flags,
                     uint32_t message, uint32_t headlineId, uint32_t detailId)
    : CoolGameWindow(instance, x, y, width, height, parent, stringSlot, bitmapSlot, backgroundId, childId, style, flags,
                     0, 0) {
    m_enabled = 1;
    g_appDialogMessage = message;
    m_headlineId = headlineId;
    m_detailId = detailId;
}

AppDialog::~AppDialog() {
    g_appDialogMessage = 0;
    REB_KEEP_VPTR_STORE();
}

void AppDialog::OnCreate1() { FocusAndRemember(); }

void AppDialog::OnCreate2() {
    if (!BeginOffscreen(nullptr)) return;
    if (m_headlineId) DrawTextResource(m_headlineId, kHeadlineArea, kTextFontSlot, kTextColor, kHeadlineFormat);
    if (m_detailId) {
        const RECT& area = m_headlineId ? kDetailBelowHeadlineArea : kDetailAloneArea;
        DrawTextResource(m_detailId, area, kTextFontSlot, kTextColor, kDetailFormat);
    }
    EndOffscreen();
}

void AppDialog::OnCreate3() {
    AddButton(0x8a, 0x87, kButtonBitmapId, kButtonPressedBitmapId, kYesButtonId, kYesTooltipId, 0, 0, 0, 0, 0, 1);
    AddButton(0xe5, 0x87, kNoButtonBitmapId, kNoPressedBitmapId, kNoButtonId, kNoTooltipId, 0, 0, 0, 0, 0, 1);
}

void AppDialog::OnCommand(int a, int, int) {
    switch (static_cast<uint16_t>(a)) {
    case kYesButtonId:
        FUN_004011a0(1, m_parent);
        break;
    case kNoButtonId:
        FUN_004011a0(0, m_parent);
        break;
    }
}

void AppDialog::OnKeyDown(WPARAM key, LPARAM) {
    if (key == VK_RETURN)
        FUN_004011a0(1, m_parent);
    else if (key == VK_ESCAPE)
        FUN_004011a0(0, m_parent);
}

int AppDialog::Virtual14() { return m_enabled; }

// ---- AppNoticeDialog -------------------------------------------------------------------------------------------

AppNoticeDialog::AppNoticeDialog(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                                 uint32_t stringSlot, uint32_t bitmapSlot, uint32_t backgroundId, uint32_t childId,
                                 uint32_t style, uint32_t flags, uint32_t message, uint32_t headlineId,
                                 uint32_t detailId, uint32_t buttonTooltipId)
    : AppDialog(instance, x, y, width, height, parent, stringSlot, bitmapSlot, backgroundId, childId, style, flags,
                message, headlineId, detailId) {
    m_buttonTooltipId = buttonTooltipId;
}

AppNoticeDialog::~AppNoticeDialog() { REB_KEEP_VPTR_STORE(); }

void AppNoticeDialog::OnCreate3() {
    AddButton(0xb0, 0x86, kButtonBitmapId, kButtonPressedBitmapId, kAcknowledgeButtonId, m_buttonTooltipId, 0, 0, 0, 0,
              0, 1);
}

void AppNoticeDialog::OnCommand(int a, int, int) {
    if (static_cast<uint16_t>(a) == kAcknowledgeButtonId) FUN_004011a0(1, nullptr);
}

void AppNoticeDialog::OnKeyDown(WPARAM key, LPARAM) {
    if (key == VK_RETURN) FUN_004011a0(1, nullptr);
}

// ---- AppWaitDialog ---------------------------------------------------------------------------------------------

AppWaitDialog::AppWaitDialog(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                             uint32_t stringSlot, uint32_t bitmapSlot, uint32_t backgroundId, uint32_t childId,
                             uint32_t style, uint32_t flags, uint32_t message, uint32_t headlineId,
                             uint32_t detailId)
    : AppDialog(instance, x, y, width, height, parent, stringSlot, bitmapSlot, backgroundId, childId, style, flags,
                message, headlineId, detailId) {}

AppWaitDialog::~AppWaitDialog() { REB_KEEP_VPTR_STORE(); }

void AppWaitDialog::OnCreate3() {}

void AppWaitDialog::OnCommand(int a, int, int) {
    if (static_cast<uint16_t>(a) == kAcknowledgeButtonId) FUN_004011a0(0, nullptr);
}

void AppWaitDialog::OnKeyDown(WPARAM, LPARAM) {}

// ---- Factories -------------------------------------------------------------------------------------------------

int ShowAppDialog(uint32_t message, CoolWindow* app, uint32_t headlineId, uint32_t detailId, int, int) {
    if (!app || g_appDialog) return 0;
    g_appDialog = NewOrNull<AppDialog>(app->m_instance, 0, 0, kDialogWidth, kDialogHeight, app, kResourceSlot,
                                       kResourceSlot, kQuestionBackgroundId, kDialogChildId, kDialogStyle,
                                       kGameWindowPopup, message, headlineId, detailId);
    if (!g_appDialog) return 0;
    if (g_appDialog->Virtual14()) {
        g_appDialog->Create(0);
        g_appDialog->CenterInParent();
        HWND hwnd = g_appDialog->m_hwnd;
        g_appDialog->ShowAndTrack(SW_SHOW);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);
        SendMessageA(app->m_hwnd, kDialogOpenedMessage, 0, 0);
        return 1;
    }
    DeleteAndForgetDialog();
    return 0;
}

int ShowAppNoticeDialog(uint32_t message, CoolWindow* app, uint32_t headlineId, uint32_t detailId,
                        uint32_t buttonTooltipId, int, int) {
    if (g_appDialog) DestroyAppDialog();
    if (!app) return 0;
    g_appDialog = NewOrNull<AppNoticeDialog>(app->m_instance, 0, 0, kDialogWidth, kDialogHeight, app, kResourceSlot,
                                             kResourceSlot, kNoticeBackgroundId, kDialogChildId, kDialogStyle,
                                             kGameWindowPopup, message, headlineId, detailId, buttonTooltipId);
    if (!g_appDialog) return 0;
    if (g_appDialog->Virtual14()) {
        g_appDialog->Create(0);
        g_appDialog->CenterInParent();
        SetWindowPos(g_appDialog->m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        g_appDialog->ShowAndTrack(SW_SHOW);
        return 1;
    }
    DeleteAndForgetDialog();
    return 0;
}

int ShowAppWaitDialog(CoolWindow* app, uint32_t headlineId, uint32_t detailId, int, int) {
    if (g_appDialog) DestroyAppDialog();
    if (!app) return 0;
    g_appDialog = NewOrNull<AppWaitDialog>(app->m_instance, 0, 0, kDialogWidth, kDialogHeight, app, kResourceSlot,
                                           kResourceSlot, kWaitBackgroundId, kDialogChildId, kDialogStyle,
                                           kGameWindowPopup, kWaitMessage, headlineId, detailId);
    if (!g_appDialog) return 0;
    if (g_appDialog->Virtual14()) {
        g_appDialog->Create(0);
        g_appDialog->CenterInParent();
        SetWindowPos(g_appDialog->m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        g_appDialog->ShowAndTrack(SW_SHOW);
        return 1;
    }
    DeleteAndForgetDialog();
    return 0;
}

int DestroyAppDialog() {
    if (!g_appDialog) return 0;
    g_appDialogMessage = 0;
    DestroyWindow(g_appDialog->m_hwnd);
    DeleteAndForgetDialog();
    return 1;
}

void CenterAppDialog() {
    if (g_appDialog) g_appDialog->CenterInParent();
}
