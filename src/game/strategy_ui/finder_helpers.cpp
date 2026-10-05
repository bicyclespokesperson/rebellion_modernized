#include "strategy_ui/finder_helpers.h"

#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_string_field.h"
#include "ui/cool_tab_control.h"
#include "ui/custom_dialog_box.h"

namespace {

AvlNode* NextTab(AvlNode* tab, WPARAM key) { return key == VK_LEFT ? tab->Predecessor() : tab->m_successor; }

void SelectNeighbourTab(CoolTabControl* tabs, WPARAM key) {
    const uint32_t selectedId = tabs->m_selected ? tabs->m_selected->m_childId : 0;
    AvlNode* node = NextTab(tabs->m_children.FindById(selectedId), key);
    if (node) {
        do {
            CoolWindow* tab = static_cast<CoolWindow*>(node);
            if (IsWindowVisible(tab->m_hwnd)) {
                tabs->SelectTab(tab->m_childId, 1);
                return;
            }
            node = NextTab(node, key);
        } while (node);
        return;
    }
    if (AvlNode* first = tabs->m_children.First()) tabs->SelectTab(static_cast<CoolWindow*>(first)->m_childId, 1);
}

}  // namespace

void FinderOnKeyDown(CustomDialogBox* window, CoolTabControl* tabs, CoolStringField* nameField, CoolDragList* list,
                     WPARAM key, LPARAM lParam) {
    switch (key) {
    case VK_TAB:
        break;
    default:
        window->m_parent->WindowProc(WM_KEYDOWN, key, lParam);
        break;
    case VK_ESCAPE:
        window->PostCloseRequest();
        return;
    case VK_LEFT:
    case VK_RIGHT:
        SelectNeighbourTab(tabs, key);
        break;
    case VK_UP:
        SetFocus(nameField->m_hwnd);
        return;
    case VK_DOWN:
        if (list->m_items->First()) {
            SetFocus(list->m_hwnd);
            PostMessageA(list->m_hwnd, WM_KEYDOWN, key, lParam);
            return;
        }
    }
}
