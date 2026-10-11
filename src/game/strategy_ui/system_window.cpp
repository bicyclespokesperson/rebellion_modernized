#include <string>
#include "core/game_string.h"
#include "strategy_ui/system_window.h"

#include <cstdint>
#include <new>

#include "ai/ai_common.h"
#include "orders/order_base.h"
#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "core/stream_io.h"
#include "data/dat_record.h"
#include "economy/manu_mgr.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "strategy_ui/system_missions_window.h"
#include "ui/bitmap_status_bar.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_image.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_label.h"
#include "ui/cool_list_item.h"
#include "ui/cool_string_field.h"
#include "ui/cool_tab_control.h"
#include "ui/cool_window.h"
#include "ui/object_list_item.h"
#include "ui/resource_dll.h"

extern const char *g_gokresDllName;  // 0x6a8708 "gokres.dll"

namespace {

constexpr uint32_t kFirstPage = 0x67;
constexpr int kRenameFontSlot = 10;

void DeleteBitmap(DibBitmap* bitmap) {
    if (!bitmap) return;
    bitmap->~DibBitmap();
    ::operator delete(bitmap);
}

void DeleteLabel(CoolLabel* label) {
    if (!label) return;
    label->~CoolLabel();
    ::operator delete(label);
}

ResourceStringId StringId(uint16_t id) { return ResourceStringId{static_cast<int16_t>(id), 2}; }

}  // namespace

// ---- Construction ------------------------------------------------------------------------------------------

SystemWindow::SystemWindow(HINSTANCE instance, int x, int y, int width, int height, GalaxyWindow* parent,
                           uint32_t childId, GameObjNotifier* system)
    : CustomDialogBox(instance, x, y, width, height, parent, childId, nullptr) {
    ObjectKey key;
    if (system) GetObjectKey(system, &key);
    m_systemKey = key;
    m_galaxy = parent;
    CoolGameWindow::m_flags |= 4;
    m_closeButton = nullptr;
    m_sideButton = nullptr;
    m_sectorButton = nullptr;
    m_page = kFirstPage;
    m_renameField = nullptr;
    m_panelBackground = nullptr;
    m_panelCanvas = nullptr;
    m_countsCanvas = nullptr;
    m_headerActive = nullptr;
    m_headerInactive = nullptr;
    m_selectedSlot = nullptr;
    m_tabIcon = nullptr;
    m_tabs = nullptr;
    m_panelSelected = nullptr;
    m_panelNormal = nullptr;
    m_uiFlags = 1;
    m_selectedCount = 0;
    m_pendingRename = nullptr;
    m_dragImage = nullptr;
    m_dragPictures[0] = nullptr;
    m_dragPictures[1] = nullptr;
    m_dragPictures[2] = nullptr;
    m_countsLabel = nullptr;
    m_pageLabel = nullptr;
    ClearSlots();
    m_gokres = FindResourceModule(10);
    if (!m_gokres) {
        char* dllLocation = GetDllLocation();
        m_gokres = LoadResourceModule(dllLocation, g_gokresDllName, 10);
        if (dllLocation) operator delete(dllLocation);
    }
}

SystemWindow::~SystemWindow() {

    delete m_closeButton;
    delete m_sideButton;
    delete m_sectorButton;
    DeleteBitmap(m_panelBackground);
    DeleteBitmap(m_panelCanvas);
    DeleteBitmap(m_countsCanvas);
    DeleteLabel(m_countsLabel);
    if (m_dragImage) {
        m_dragImage->~CoolDragImage();
        ::operator delete(m_dragImage);
    }
    DeleteBitmap(m_headerActive);
    DeleteBitmap(m_headerInactive);
    DeleteLabel(m_pageLabel);
    DeleteBitmap(m_tabIcon);
    delete m_tabs;
    DeleteBitmap(m_panelSelected);
    DeleteBitmap(m_panelNormal);
    if (m_items) {
        m_items->Clear();
        delete m_items;
    }
    for (DibBitmap*& picture : m_dragPictures) {
        DeleteBitmap(picture);
        picture = nullptr;
    }
    DestroySlots();
}

void SystemWindow::ClearSlots() {
    for (SystemSlot& slot : m_slots) {
        slot.m_productPicture = nullptr;
        slot.m_headingLabel = nullptr;
        slot.m_statusLabel = nullptr;
        slot.m_characterLabel = nullptr;
        slot.m_countLabel = nullptr;
        slot.m_progressBar = nullptr;
        slot.m_flags = 0;
    }
    SetRect(&m_slots[0].m_rect, 0x37, 0x39, 0xdd, 0x88);
    SetRect(&m_slots[1].m_rect, 0x37, 0x8a, 0xdd, 0xd9);
    SetRect(&m_slots[2].m_rect, 0x37, 0xdb, 0xdd, 0x12a);
}

void SystemWindow::DestroySlots() {
    for (SystemSlot& slot : m_slots) {
        DeleteBitmap(slot.m_productPicture);
        DeleteLabel(slot.m_headingLabel);
        DeleteLabel(slot.m_statusLabel);
        DeleteLabel(slot.m_characterLabel);
        DeleteLabel(slot.m_countLabel);
        delete slot.m_progressBar;
    }
}

// ---- Slots -------------------------------------------------------------------------------------------------

int SystemWindow::SlotOfManager(const ObjectKey* manager) {
    for (int i = 0; i < 3; ++i) {
        if (m_slots[i].m_managerKey.m_value == manager->m_value) return i + 1;
    }
    return 0;
}

ObjectKey* ManuMgr::GetDeploymentKey(ObjectKey* out) {
    *out = m_deploymentKey;
    return out;
}

void SystemWindow::InitSlot(int slot, GameObjNotifier* manager) {
    SystemSlot& panel = Slot(slot);
    const ObjectKey unset;
    ObjectKey managerKey;
    GetObjectKey(manager, &managerKey);
    panel.m_managerKey = managerKey;
    panel.m_productKey = unset;
    panel.m_characterKey = unset;
    HDC dc = GetDC(m_hwnd);
    if (!panel.m_headingLabel) {
        const uint16_t headingId = slot == 1 ? 0x1829 : (slot == 2 ? 0x1831 : 0x1833);
        const ResourceStringId heading = StringId(headingId);
        CoolLabel* label = NewOrNull<CoolLabel>(&heading, 0, 0, 0, 0, 0u, 0u);
        label->MoveTo(5, 1);
        label->SetFont(10);
        label->Measure(dc);
        panel.m_headingLabel = label;
    }
    auto newPlainLabel = [](int y) {
        CoolLabel* label = NewOrNull<CoolLabel>(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
        label->MoveTo(5, y);
        label->SetFont(10);
        label->SetColor(0x2ffffff, nullptr);
        return label;
    };
    if (!panel.m_characterLabel) panel.m_characterLabel = newPlainLabel(0x39);
    if (!panel.m_countLabel) panel.m_countLabel = newPlainLabel(0x2f);
    if (!panel.m_statusLabel) panel.m_statusLabel = newPlainLabel(0x10);
    ObjectKey characterKey;
    static_cast<ManuMgr*>(manager)->GetDeploymentKey(&characterKey);
    SetSlotCharacter(slot, manager, &characterKey);
    SetSlotStatusText(slot, manager, nullptr);
    ReleaseDC(m_hwnd, dc);
}

void SystemWindow::SetSlotCharacter(int slot, GameObjNotifier* manager, const ObjectKey* character) {
    SystemSlot& panel = Slot(slot);
    if (panel.m_characterKey.m_value == character->m_value) return;
    const ResourceStringId prefix = StringId(0x1839);
    std::string text = LoadStringResource(&prefix);
    if (GameObjNotifier* object = GetObjectInFamilies_01_ff(m_galaxy->m_side, character)) {
        std::string name;
        static_cast<GameObj*>(object)->GetNameCopy(&name);
        text += name;
    }
    CoolLabel* label = panel.m_characterLabel;
    panel.m_characterKey = *character;
    label->SetText(text.c_str());
    InvalidateRect(m_hwnd, &panel.m_rect, FALSE);
}

void SystemWindow::SetSlotStatusText(int slot, GameObjNotifier* manager, std::string* text) {
    SystemSlot& panel = Slot(slot);
    CoolLabel* label = panel.m_statusLabel;
    if (text && text->size() != 0) {
        label->SetText(text->c_str());
    } else if ((panel.m_productKey.m_value & 0xff000000) == 0) {
        uint16_t id = 0;
        if (slot == 1) {
            id = 0x1836;
        } else if (slot == 2) {
            id = 0x1837;
        } else if (slot == 3) {
            id = 0x1838;
        }
        if (id != 0) {
            const ResourceStringId resource = StringId(id);
            label->SetTextFromResource(&resource);
        }
    } else if (GameObjNotifier* product = GetObjectInFamilies_01_ff(m_galaxy->m_side, &panel.m_productKey)) {
        std::string name;
        static_cast<GameObj*>(product)->GetNameCopy(&name);
        label->SetText(name.c_str());
    }
    InvalidateRect(m_hwnd, &panel.m_rect, FALSE);
}

void SystemWindow::SetSlotProduction(int slot, GameObjNotifier* manager) {
    ManuMgr* managerObject = static_cast<ManuMgr*>(manager);
    SystemSlot& panel = Slot(slot);
    const int count = managerObject->m_remainingGameObjCount;
    const ObjectKey productKey = managerObject->m_productKey;
    GameObjNotifier* product = GetObjectInFamilies_01_ff(m_galaxy->m_side, &productKey);
    CoolLabel* countLabel = panel.m_countLabel;
    if (panel.m_productPicture) {
        DeleteBitmap(panel.m_productPicture);
        panel.m_productPicture = nullptr;
    }
    if (count == 0 || !product) {
        InitSlot(slot, manager);
    } else {
        const uint32_t recordBits = product->m_record->m_small;
        panel.m_productPicture = NewOrNull<DibBitmap>(m_gokres, recordBits & 0xfff, 10u);
        uint16_t textId = 0;
        if (slot == 1 || slot == 3) {
            textId = 0x1841;
        } else if (slot == 2) {
            textId = 0x1840;
        }
        std::string* countText = nullptr;
        if (textId != 0) {
            const ResourceStringId id = StringId(textId);
            countText = NewOrNull<std::string>(LoadStringResource(&id));
        }
        countText->append(std::to_string(static_cast<long>(count)));
        countLabel->SetText(countText->c_str());
        delete countText;
        panel.m_productKey = productKey;
    }
    ObjectKey characterKey;
    managerObject->GetDeploymentKey(&characterKey);
    SetSlotCharacter(slot, manager, &characterKey);
    SetSlotStatusText(slot, manager, nullptr);
    SetSlotProgress(slot, manager, managerObject->m_completedPointCount, managerObject->m_requiredPointCount);
}

void SystemWindow::SetSlotProgress(int slot, GameObjNotifier* manager, int position, int range) {
    BitmapStatusBar* bar = Slot(slot).m_progressBar;
    if (static_cast<int>(bar->m_range) != range && range > 0) bar->SetRange(range, 0);
    bar->SetPosition(position);
}

void SystemWindow::ClearSlotHighlight(SystemSlot* slot) {
    HWND hwnd = m_hwnd;
    if (slot) {
        slot->m_flags &= ~1u;
        InvalidateRect(hwnd, &slot->m_rect, FALSE);
        UpdateWindow(hwnd);
        return;
    }
    for (SystemSlot& panel : m_slots) {
        if ((panel.m_flags & 1) != 0) {
            panel.m_flags &= ~1u;
            InvalidateRect(hwnd, &panel.m_rect, FALSE);
        }
    }
    UpdateWindow(hwnd);
}

void SystemWindow::HighlightSlot(SystemSlot* slot) {
    slot->m_flags |= 1;
    InvalidateRect(m_hwnd, &slot->m_rect, FALSE);
    if (m_dragPictures[0] != nullptr) {
        m_selectedCount = 0;
        for (const SystemSlot& panel : m_slots) {
            if ((panel.m_flags & 1) != 0) ++m_selectedCount;
        }
    }
    UpdateWindow(m_hwnd);
}

void SystemWindow::ShowProgressBars(int hide) {
    for (SystemSlot& panel : m_slots) {
        if (hide == 0) {
            panel.m_progressBar->Show(5);
        } else {
            panel.m_progressBar->Hide();
        }
    }
}

void SystemWindow::BeginRenameRequest(TextMessage* request) {
    m_pendingRename = request;
    if ((m_uiFlags & 8) == 0) {
        m_uiFlags = (m_uiFlags & ~4u) | 8;
        BeginSlotRename(m_selectedSlot);
    }
}

void SystemWindow::BeginSlotRename(SystemSlot* slot) {
    if (m_renameField) {
        DestroyWindow(m_renameField->m_hwnd);
        delete m_renameField;
        m_renameField = nullptr;
    }
    CoolLabel* label = slot->m_statusLabel;
    const int labelRight = label->m_rect.right;
    const int labelBottom = label->m_rect.bottom;
    const int left = slot->m_rect.left;
    const int top = slot->m_rect.top;
    const int fieldLeft = label->m_rect.left + left;
    const int fieldTop = label->m_rect.top + top;
    m_renameField = NewOrNull<CoolStringField>(m_instance, fieldLeft, fieldTop, (labelRight + 2 + left) - fieldLeft,
                                               (labelBottom + top) - fieldTop, this, GetPaintBackgroundIfOwned(), 0u);
    if (m_renameField) {
        m_renameField->SetFont(kRenameFontSlot);
        m_renameField->Create(0);
        m_renameField->SetText(label->GetText());
        m_renameField->SetSelection(0, -1);
        SetFocus(m_renameField->m_hwnd);
    }
}

void SystemWindow::EndSlotRename(SystemSlot* slot) {
    SetFocus(m_hwnd);
    if (!m_renameField) return;
    std::string current = StringFromText(slot->m_statusLabel->GetText());
    std::string typed(m_renameField->m_text.c_str());
    if (CompareStrings(current, typed) != 0 && typed.size() != 0) {
        if (m_pendingRename == nullptr) {
            ObjectKey unset;
            std::string name(typed.c_str());
            ObjectKeyValueSet keys;
            ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>();
            node->m_key = slot->m_managerKey;
            keys.Add(node);
            UiHumanCommand(0x215, &keys, &unset, &name);
        } else {
            m_pendingRename->m_text = typed;
            UiHumanReceiveMessage(reinterpret_cast<PlayerMessage*>(m_pendingRename), 0);
            m_pendingRename = nullptr;
        }
    }
    if (m_pendingRename) {
        delete m_pendingRename;
        m_pendingRename = nullptr;
    }
    DestroyWindow(m_renameField->m_hwnd);
    delete m_renameField;
    m_renameField = nullptr;
}

// ---- Saved state, selection and keys -----------------------------------------------------------------------

void SystemWindow::SaveState(ostream* out) {
    ObjectKey key;
    if (GameObjNotifier* system = reinterpret_cast<SystemMissionsWindow*>(this)->SystemMissionsWindow::GetSystem()) {
        ObjectKey systemKey;
        GetObjectKey(system, &systemKey);
        key = systemKey;
    }
    uint32_t childId = m_childId;
    key.Write(out);
    WriteValue32(out, &childId);
}

void SystemWindow::LoadState(istream* in) {
    ObjectKey key;
    key.Read(in);
    uint32_t childId;
    ReadValue32(in, &childId);
    m_systemKey = key;
    m_childId = childId;
}

void SystemWindow::GetSelection(ObjectKeyValueSet* out, CoolWindow* source) {
    if (m_page == static_cast<int>(kFirstPage)) {
        for (const SystemSlot& panel : m_slots) {
            if ((panel.m_flags & 1) == 0) continue;
            ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>();
            node->m_key = panel.m_managerKey;
            out->Add(node);
        }
        return;
    }
    CoolListItemList selection(1);
    if (!m_list) return;
    m_list->CopySelectionTo(&selection);
    const ObjectKey unset;
    for (CoolListItem* picked = selection.First(); picked; picked = picked->Next()) {
        ObjectListItem* item = static_cast<ObjectListItem*>(m_items->FindById(picked->m_id));
        ObjectKey key;
        item->GetKey(&key);
        if (unset.m_value != key.m_value) {
            ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(key, 0);
            out->Add(node);
        }
    }
}

ObjectKey SystemWindow::GetKeyAt(int x, int y) {
    ObjectKey result = m_systemKey;
    if (m_page == static_cast<int>(kFirstPage)) return result;
    const ObjectKey unset;
    POINTS point = {static_cast<SHORT>(x - 8), static_cast<SHORT>(y - 0x4d)};
    CoolListItem* item = m_list->HitTest(m_list->ClientToList(point));
    if (!item) return result;
    ObjectKey itemKey;
    static_cast<ObjectListItem*>(item)->GetKey(&itemKey);
    if (itemKey.m_value != unset.m_value) result = itemKey;
    return result;
}

ObjectKey SystemWindow::GetDropKeyAt(int, int) { return m_systemKey; }

int SystemWindow::Virtual29() {
    GameObjNotifier* system = reinterpret_cast<SystemMissionsWindow*>(this)->SystemMissionsWindow::GetSystem();
    if (!system) return 0;
    const uint32_t side = (static_cast<GameObjRoot*>(system)->m_flags >> 6) & 3;
    if (side == 1) return 0x2d0a;
    if (side == 2) return 0x2d0b;
    return 0x2d0c;
}

void SystemWindow::ShowObjectPage(const ObjectKey* key) {
    uint32_t page = 0;
    const uint32_t family = key->Family();
    if (family == 0x2c) {
        page = 0x6c;
    } else if (family == 0x2d) {
        page = 0x6b;
    } else if (family == 0x28) {
        page = 0x68;
    } else if (family == 0x29) {
        page = 0x69;
    } else if (family == 0x2a) {
        page = 0x6a;
    }
    if (page != 0 && page != static_cast<uint32_t>(m_page)) m_tabs->SelectTab(page, 1);
}

void SystemWindow::OnKeyDown(WPARAM key, LPARAM lParam) {
    switch (key) {
    case VK_ESCAPE:
        PostCloseRequest();
        return;
    case VK_LEFT:
    case VK_RIGHT: {
        const bool backwards = key == VK_LEFT;
        AvlNode* tab = m_tabs->m_children.FindById(m_tabs->m_selected ? m_tabs->m_selected->m_childId : 0);
        tab = backwards ? tab->Predecessor() : tab->m_successor;
        if (tab) {
            for (; tab; tab = backwards ? tab->Predecessor() : tab->m_successor) {
                CoolWindow* window = static_cast<CoolWindow*>(tab);
                if (IsWindowVisible(window->m_hwnd)) {
                    m_tabs->SelectTab(window->m_childId, 1);
                    return;
                }
            }
            return;
        }
        if (AvlNode* first = m_tabs->m_children.First()) {
            m_tabs->SelectTab(static_cast<CoolWindow*>(first)->m_childId, 1);
        }
        return;
    }
    case VK_UP:
    case VK_DOWN:
        if (m_page == static_cast<int>(kFirstPage)) {
            if (!m_selectedSlot) {
                HighlightSlot(&m_slots[0]);
                return;
            }
            int index = 0;
            SystemSlot* slot = &m_slots[0];
            while (m_selectedSlot != slot) {
                ++index;
                ++slot;
                if (index > 2) return;
            }
            ClearSlotHighlight(nullptr);
            if (key == VK_UP && index != 0) {
                HighlightSlot(&m_slots[index - 1]);
                return;
            }
            if (key == VK_DOWN && index + 1 < 3) slot = &m_slots[index + 1];
            HighlightSlot(slot);
            return;
        }
        [[fallthrough]];
    default:
        m_parent->WindowProc(WM_KEYDOWN, key, lParam);
    }
}
