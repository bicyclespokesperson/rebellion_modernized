#include "ui/custom_dialog_box.h"
#include "orders/order_base.h"

#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "core/checked_new.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gameobj/status_code.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "strategy_ui/object_menu.h"
#include "ui/cool_app.h"
#include "ui/cool_label.h"
#include "ui/cool_string_field.h"

extern const char *g_customDialogBoxClassName;  // 0x6a8828 "CustomDialogBox"

namespace {

constexpr UINT kCommitRenameMessage = 0x407;   // CoolStringField: Return pressed
constexpr UINT kDialogClickedMessage = 0x467;  // to the parent: wParam = this window's HWND
constexpr uint32_t kDialogStyle = 0x52000000;  // WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
constexpr uint32_t kChildStyle = 0x56000000;   // ... | WS_VISIBLE
constexpr UINT kTitleDrawFlags = 0x25;         // DT_CENTER | DT_VCENTER | DT_SINGLELINE
constexpr uint32_t kTitleLayout = 3;
constexpr uint32_t kMenuKind = 7;
constexpr uint32_t kMenuBaseColor = 0x20000ff;
constexpr uint32_t kMenuAlternateColorBits = 0xfe01;
constexpr uint32_t kMenuShadowColor = 0x2ffffff;
constexpr uint32_t kMenuDisabledColor = 0x2808080;
constexpr uint32_t kMenuEntryDisabled = 1;
constexpr uint32_t kMenuEntryChecked = 2;
constexpr int kRenameFieldFontSlot = 10;
constexpr int kAlliancePlayer = 1;
CoolWindow* AppMainWindow() { return static_cast<RebellionApplication*>(g_app)->m_activeWindow; }

}  // namespace

// ---- ObjectKeyIdNode / ObjectKeyIdSet ------------------------------------------------------------------------

ObjectKeyIdNode::ObjectKeyIdNode() {}

ObjectKeyIdNode::ObjectKeyIdNode(const ObjectKey* key, uint32_t id) {
    m_key = *key;
    if (id) AddId(id);
}

ObjectKeyIdNode::~ObjectKeyIdNode() {  }

void ObjectKeyIdNode::AddId(uint32_t id) {
    if (m_ids.FindById(id)) return;
    IdNode* node = NewOrNull<IdNode>();
    node->SetId(id);
    m_ids.AddWithId(node);
}

ObjectKeyIdSet::ObjectKeyIdSet() {}

ObjectKeyIdSet::~ObjectKeyIdSet() {  }

void ObjectKeyIdSet::Add(ObjectKeyIdNode* node, uint32_t id) {
    if (!node) return;
    ObjectKey scratch;
    ObjectKey key = *node->GetKey(&scratch);
    if (key.Id() == 0 || key.Family() == 0) return;
    ObjectKeyIdNode* existing = static_cast<ObjectKeyIdNode*>(Find(node->GetKey(&scratch)));
    if (existing && id) {
        existing->AddId(id);
        delete node;
        return;
    }
    Insert(node);
    if (id) node->AddId(id);
}

void ObjectKeyIdSet::Remove(ObjectKeyIdNode* node) {
    if (!node) return;
    ObjectKey scratch;
    Detach(node->GetKey(&scratch));
    delete node;
}

// ---- CustomDialogBox -----------------------------------------------------------------------------------------

CustomDialogBox::CustomDialogBox(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                                 uint32_t childId, DibBitmap* backgroundSource)
    : CoolGameWindow(instance, x, y, width, height, parent, 0, 0, 0, childId, kDialogStyle, 0, 0, 0) {
    SetClassName(g_customDialogBoxClassName);
    m_style = kChildStyle;
    CoolWindow::m_background = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    m_titleLabel = NewOrNull<CoolLabel>(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
    m_titleLabel->MoveTo(2, 2);
    m_titleLabel->m_width = width;
    m_titleLabel->m_layoutFlags = kTitleLayout;
    m_titleLabel->m_drawFlags = kTitleDrawFlags;
    SetRect(&m_titleRect, 2, 2, m_clientWidth - 3, 0x10);
    if (backgroundSource) {
        DibBitmap* background = NewOrNull<DibBitmap>(width, height, backgroundSource);
        backgroundSource->Blit(background, 0, 0, x, y, 0, 0);
        SetPaintBackground(background, 0);
    }
    m_renameField = nullptr;
    m_renameTarget = nullptr;
}

CustomDialogBox::~CustomDialogBox() {

    if (m_renameField) delete m_renameField;
    if (m_renameTarget) delete m_renameTarget;
    if (m_titleLabel) delete m_titleLabel;
}

LRESULT CustomDialogBox::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT:
        return CoolGameWindow::WindowProc(message, wParam, lParam);
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostCloseRequest();
            return 0;
        }
        CoolGameWindow::WindowProc(WM_KEYDOWN, wParam, lParam);
        return 0;
    case kCommitRenameMessage:
        CommitRename();
        return 0;
    case kButtonMessage:
        PostMessageA(m_parent->m_hwnd, kDialogClickedMessage, reinterpret_cast<WPARAM>(m_hwnd), 0);
        [[fallthrough]];
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        if (m_renameField) CancelRename();
        return CoolGameWindow::WindowProc(message, wParam, lParam);
    default:
        return CoolGameWindow::WindowProc(message, wParam, lParam);
    }
}

int CustomDialogBox::OnRightButtonUp(int x, int y, CoolWindow* sender) {
    if (!g_app || !sender || GetCapture()) return 0;
    CoolWindow* mainWindow = AppMainWindow();
    uint32_t color = kMenuBaseColor + (AppGetSide() != kAlliancePlayer ? kMenuAlternateColorBits : 0);
    m_selection.Clear();
    GetSelection(&m_selection, sender);
    POINT point = {x, y};
    if (mainWindow) MapWindowPoints(sender->m_hwnd, mainWindow->m_hwnd, &point, 1);
    EventKindSet commands;
    UiBuildAvailableOrders(AppGetSide(), &m_selection, &commands);
    for (EventKindNode* command = static_cast<EventKindNode*>(commands.First()); command;
         command = static_cast<EventKindNode*>(command->m_successor)) {
        uint32_t flags = (command->m_value == 0 ? kMenuEntryDisabled : 0) | (command->m_unknown != 0 ? kMenuEntryChecked : 0);
        AddMenuEntry(command->m_key, kMenuKind, flags, point.x, point.y, mainWindow, color, kMenuShadowColor,
                     kMenuDisabledColor);
    }
    ShowMenu(point.x, point.y, CoolAppGetResourceModule(kMenuKind), mainWindow, this, nullptr);
    return 0;
}

void CustomDialogBox::Virtual08(int a, int, int) {
    ObjectKey none;
    UiHumanCommand(a, &m_selection, &none, 0);
}

void CustomDialogBox::GetSelection(ObjectKeyValueSet*, CoolWindow*) {}

void CustomDialogBox::OnChildAdded(const ObjectKey*, GameObjNotifier*) {}

void CustomDialogBox::OnChildMoved(const ObjectKey*, GameObjNotifier*, const ObjectKey*, GameObjNotifier*) {}

void CustomDialogBox::OnObjectUpdated(const ObjectKey*, GameObjNotifier*) {}

ObjectKey CustomDialogBox::GetKeyAt(int, int) {
    ObjectKey none;
    return none;
}

void CustomDialogBox::ShowObjectPage(const ObjectKey*) {}

ObjectKey CustomDialogBox::GetDropKeyAt(int x, int y) { return GetKeyAt(x, y); }

int CustomDialogBox::Virtual29() { return 0; }

void CustomDialogBox::BeginRename(TextMessage* target) {
    GalaxyWindow* galaxy = GetGalaxyWindow();
    ObjectKeyNode* firstKey = static_cast<ObjectKeyNode*>(target->GetTeam()->First());
    ObjectKey key;
    firstKey->GetKey(&key);
    int view = galaxy ? galaxy->m_side : static_cast<int>(reinterpret_cast<intptr_t>(target));
    GameObj* object = static_cast<GameObj*>(GetObjectInFamilies_01_ff(view, &key));
    if (!object) return;
    m_renameTarget = target;
    CancelRename();
    RECT rect;
    GetRenameRect(&rect);
    m_renameField = NewOrNull<CoolStringField>(m_instance, rect.left, rect.top, rect.right - rect.left,
                                               rect.bottom - rect.top, this, GetPaintBackgroundIfOwned(), 0u);
    if (!m_renameField) return;
    m_renameField->SetFont(kRenameFieldFontSlot);
    {
        String name;
        object->GetNameCopy(&name);
        m_renameField->SetText(name.m_text);
    }
    m_renameField->Create(0);
    m_renameField->SetSelection(0, -1);
    SetWindowPos(m_renameField->m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
    SetFocus(m_renameField->m_hwnd);
}

void CustomDialogBox::CommitRename() {
    SetFocus(m_hwnd);
    if (!m_renameField || !m_renameTarget) return;
    if (m_renameField->m_text.Length() == 0) return;
    {
        String name(m_renameField->m_text.m_text);
        m_renameTarget->m_text.Assign(name);
    }
    UiHumanReceiveMessage(reinterpret_cast<PlayerMessage*>(m_renameTarget), 0);
    m_renameTarget = nullptr;
    m_renameField->DestroyNativeWindow();
    if (m_renameField) delete m_renameField;
    m_renameField = nullptr;
}

void CustomDialogBox::CancelRename() {
    SetFocus(m_hwnd);
    if (!m_renameField) return;
    DestroyWindow(m_renameField->m_hwnd);
    if (m_renameField) delete m_renameField;
    m_renameField = nullptr;
}

int CustomDialogBox::GetFirstSelectedKey(ObjectKey* out) {
    ObjectKeyValueSet selection;
    GetSelection(&selection, nullptr);
    ObjectKeyNode* first = selection.First();
    if (first) {
        ObjectKey scratch;
        *out = *first->GetKey(&scratch);
    }
    return first != nullptr;
}

RECT* CustomDialogBox::GetRenameRect(RECT* out) {
    out->left = 0;
    out->top = 0;
    out->right = 75;
    out->bottom = 20;
    return out;
}

void CustomDialogBox::Virtual35() {}

const char *g_customDialogBoxClassName = nullptr;
