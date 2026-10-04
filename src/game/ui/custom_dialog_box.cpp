#include "ui/custom_dialog_box.h"

#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "gfx/dib_bitmap.h"
#include "ui/cool_label.h"
#include "ui/cool_string_field.h"

// game_ui_facade: hands a command and the selected objects to the game session.
void FUN_0041cdf0(int command, ObjectKeyValueSet* selection, ObjectKey* key, int unknown);

extern const char g_customDialogBoxClassName[];  // 0x6a8828 "CustomDialogBox"

namespace {

constexpr UINT kCommitRenameMessage = 0x407;   // CoolStringField: Return pressed
constexpr UINT kButtonMessage = 0x40e;         // a child button was clicked
constexpr UINT kDialogClickedMessage = 0x467;  // to the parent: wParam = this window's HWND
constexpr uint32_t kDialogStyle = 0x52000000;  // WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
constexpr uint32_t kChildStyle = 0x56000000;   // ... | WS_VISIBLE
constexpr UINT kTitleDrawFlags = 0x25;         // DT_CENTER | DT_VCENTER | DT_SINGLELINE
constexpr uint32_t kTitleLayout = 3;

}  // namespace

// ---- ObjectKeyIdNode / ObjectKeyIdSet ------------------------------------------------------------------------

ObjectKeyIdNode::ObjectKeyIdNode() {}

ObjectKeyIdNode::ObjectKeyIdNode(const ObjectKey* key, uint32_t id) {
    m_key = *key;
    if (id) AddId(id);
}

ObjectKeyIdNode::~ObjectKeyIdNode() { REB_KEEP_VPTR_STORE(); }

void ObjectKeyIdNode::AddId(uint32_t id) {
    if (m_ids.FindById(id)) return;
    IdNode* node = NewOrNull<IdNode>();
    node->SetId(id);
    m_ids.AddWithId(node);
}

ObjectKeyIdSet::ObjectKeyIdSet() {}

ObjectKeyIdSet::~ObjectKeyIdSet() { REB_KEEP_VPTR_STORE(); }

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
    REB_KEEP_VPTR_STORE();
    if (m_renameField) delete m_renameField;
    if (m_renameTarget) delete m_renameTarget;
    if (m_titleLabel) delete m_titleLabel;
}

LRESULT __stdcall CustomDialogBox::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
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

void CustomDialogBox::Virtual08(int a, int, int) {
    ObjectKey none;
    FUN_0041cdf0(a, &m_selection, &none, 0);
}

void CustomDialogBox::GetSelection(ObjectKeyValueSet*, CoolWindow*) {}

void CustomDialogBox::Virtual23(int, int) {}

void CustomDialogBox::Virtual24(int, int, int, int) {}

void CustomDialogBox::Virtual25(int, int) {}

ObjectKey CustomDialogBox::GetKeyAt(int, int) {
    ObjectKey none;
    return none;
}

void CustomDialogBox::Virtual27(int) {}

ObjectKey CustomDialogBox::GetDropKeyAt(int x, int y) { return GetKeyAt(x, y); }

int CustomDialogBox::Virtual29() { return 0; }

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
