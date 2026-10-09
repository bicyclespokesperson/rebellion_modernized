#include "strategy_ui/mission_selection_window.h"

#include <new>

#include "ai/ai_common.h"
#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "core/containers.h"
#include "core/game_string.h"
#include "core/vptr_store.h"
#include "data/dat_record.h"
#include "economy/facility_misc.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_name_node.h"
#include "gameobj/object_type_db.h"
#include "gfx/dib_bitmap.h"
#include "orders/mission_orders.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "strategy_ui/sector_window.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_selection_box.h"
#include "ui/cool_tab_control.h"
#include "ui/object_list_item.h"

extern const char g_gokresDllName[];  // 0x6a8708 "gokres.dll"
extern int g_missionKindCounter;      // 0x6a8794 the next row id for a mission kind the table does not know


namespace {

constexpr uint32_t kStringSlot = 0;
constexpr uint32_t kBitmapSlot = 7;
constexpr uint32_t kGokresSlot = 10;
constexpr uint32_t kHelperBitmapKind = 10;
constexpr uint32_t kResourceBitmapKind = 2;
constexpr int16_t kStringGroup = 2;
constexpr int kAlliance = 1;
constexpr int kShow = 5;
constexpr uint32_t kHiddenStyle = 0x10000000;
constexpr int kPageMission = 0x97;
constexpr int kPageTeam = 0x98;

constexpr uint32_t kBackgroundMission = 0x32;
constexpr uint32_t kBackgroundTeam = 0x33;

constexpr uint32_t kCloseId = 100;
constexpr uint32_t kOkId = 0x66;
constexpr uint32_t kCancelId = 0x65;
constexpr uint32_t kInfoId = 0x67;
constexpr uint32_t kBoxButtonId = 0x68;
constexpr uint32_t kTabsId = 0x96;
constexpr uint32_t kAvailableListId = 200;
constexpr uint32_t kSelectedListId = 0xc9;
constexpr uint32_t kMoveRightId = 0xca;
constexpr uint32_t kMoveLeftId = 0xcb;

constexpr int kMissionPictureWidth = 200;
constexpr int kMissionPictureHeight = 0x71;
constexpr int kCharacterPictureWidth = 0x6b;
constexpr int kCharacterPictureHeight = 0x3b;

ResourceStringId StringId(uint16_t id) { return ResourceStringId{static_cast<int16_t>(id), kStringGroup}; }

DibBitmap* LoadBitmap(HMODULE module, uint32_t id, uint32_t kind) { return NewOrNull<DibBitmap>(module, id, kind); }

void DeleteBitmap(DibBitmap* bitmap) {
    if (bitmap) {
        bitmap->~DibBitmap();
        operator delete(bitmap);
    }
}

bool IsAlliance(GalaxyWindow* galaxy) { return galaxy->m_side == kAlliance; }

CoolStrobeButton* NewButton(HMODULE art, int x, int y, int width, int height, CoolWindow* parent, uint32_t id,
                            uint32_t normal, uint32_t pressed, DibBitmap* backdrop) {
    return NewOrNull<CoolStrobeButton>(art, x, y, width, height, parent, id, normal, pressed, backdrop, 0, 0);
}

CoolListItemList* NewItemList(int sortMode) { return NewOrNull<CoolListItemList>(sortMode); }

void ConfigureCharacterList(CoolDragList* list, CoolListItemList* items, HINSTANCE instance) {
    list->m_style &= ~kHiddenStyle;
    list->Create(0);
    list->CreateVerticalScrollBar(instance, list, 7, 0x29fc, list->m_clientHeight, 0, list->m_clientHeight, 0, nullptr);
    list->m_textFormat = 0x11;
    list->SetFont(4);
    list->m_options.m_value |= kDragListNoDrag;
    list->SetItemList(items);
}

void AddHiddenArrow(MissionSelectionWindow* window, HMODULE art, int y, uint32_t id, uint32_t normal, uint32_t pressed,
                    uint16_t tooltip) {
    CoolStrobeButton* arrow =
        NewButton(art, 0x78, y, 0x10, 0x10, window, id, normal, pressed, window->FindOverlayBitmap(kBackgroundTeam));
    if (!arrow) return;
    arrow->m_style &= ~kHiddenStyle;
    arrow->Create(0);
    arrow->Hide();
    const ResourceStringId tip = StringId(tooltip);
    window->AddToolFromString(arrow, &tip);
    window->m_children.Insert(arrow);
}

// A row of `items` for the character `object` (key `key`): its picture over a transparent frame, named after the object.
// BUG(original): the original code spins forever on a team member the human player cannot see.
void AddCharacterRow(MissionSelectionWindow* window, CoolListItemList* items, const ObjectKey& key, GameObjNotifier* object,
                     uint8_t* fillColor) {
    ObjectListItem* item = NewOrNull<ObjectListItem>(&key);
    DibBitmap* picture = CreateObjectPicture(window->m_gokres, &key, object, 0, 1);
    const int left = (kCharacterPictureWidth - picture->Width()) / 2;
    const int top = (kCharacterPictureHeight - picture->Height()) / 2;
    DibBitmap* normal = NewOrNull<DibBitmap>(kCharacterPictureWidth, kCharacterPictureHeight, picture);
    DibBitmap* highlighted = NewOrNull<DibBitmap>(kCharacterPictureWidth, kCharacterPictureHeight, picture);
    *fillColor = picture->m_colorKey;
    normal->FillRect(0, 0, kCharacterPictureWidth, kCharacterPictureHeight, *fillColor);
    highlighted->FillRect(0, 0, kCharacterPictureWidth, kCharacterPictureHeight, *fillColor);
    picture->DrawKeyed(normal, left, top);
    picture->DrawKeyed(highlighted, left, top);
    item->SetImages(normal, highlighted);
    item->m_text = static_cast<GameObj*>(object)->GetName()->m_text;
    item->m_textOffsetY = picture->Height() + top;
    items->Insert(item);
    DeleteBitmap(picture);
}

}  // namespace

// ---- Construction ---------------------------------------------------------------------------------------------

MissionSelectionWindow::MissionSelectionWindow(HINSTANCE instance, CoolWindow* parent, uint32_t childId,
                                               MissionOrder240* order)
    : CoolGameWindow(instance, 0, 0, 0x103, 0x163, parent, kStringSlot, kBitmapSlot, 0, childId, 0, 1, 0, 0),
      m_unusedLabel(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u) {
    m_style = 0x46000000;
    m_galaxy = static_cast<GalaxyWindow*>(parent);
    m_order = order;
    m_tabs = nullptr;
    m_missionItems = nullptr;
    m_missionBox = nullptr;
    m_availableItems = nullptr;
    m_availableList = nullptr;
    m_selectedItems = nullptr;
    m_selectedList = nullptr;
    m_gokres = FindResourceModule(kGokresSlot);
    if (!m_gokres) {
        char* dllLocation = GetDllLocation();
        m_gokres = LoadResourceModule(dllLocation, g_gokresDllName, kGokresSlot);
        if (dllLocation) operator delete(dllLocation);
    }
}

MissionSelectionWindow::~MissionSelectionWindow() {
    if (m_tabs) delete m_tabs;
    if (m_missionItems) {
        for (CoolListItem* item = m_missionItems->First(); item; item = item->Next()) {
            if (item->m_userData) operator delete(reinterpret_cast<void*>(item->m_userData));
        }
        if (m_missionItems) delete m_missionItems;
    }
    if (m_missionBox) delete m_missionBox;
    if (m_availableItems) {
        m_availableItems->Clear();
        if (m_availableItems) delete m_availableItems;
    }
    if (m_availableList) delete m_availableList;
    if (m_selectedItems) {
        m_selectedItems->Clear();
        if (m_selectedItems) delete m_selectedItems;
    }
    if (m_selectedList) delete m_selectedList;
    if (m_order) delete m_order;
    REB_KEEP_VPTR_STORE();
}

// ---- OnCreate1 ------------------------------------------------------------------------------------------------

void MissionSelectionWindow::OnCreate1() {
    HINSTANCE instance = m_instance;
    HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
    CoolLabel scratchLabel(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
    const bool alliance = IsAlliance(m_galaxy);

    DibBitmap* header = LoadBitmap(art, alliance ? 0x2a31 : 0x2a32, kResourceBitmapKind);
    DibBitmap* titleBar = NewOrNull<DibBitmap>(header->Width() + 0x1e, header->Height(), header);
    header->Blit(titleBar, 0, 0, 0, 0, 0, 0);
    header->Blit(titleBar, 0x1e, 0, 0, 0, 0, 0);
    DeleteBitmap(header);

    HDC windowDc = GetDC(m_hwnd);
    HDC dc = CreateCompatibleDC(windowDc);
    ReleaseDC(m_hwnd, windowDc);
    CoolLabel text(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);

    DibBitmap* missionPanel = LoadBitmap(art, 0x2b5c, kResourceBitmapKind);
    DibBitmap* missionPage = NewOrNull<DibBitmap>(static_cast<LONG>(missionPanel->Width()),
                                                  static_cast<LONG>(missionPanel->Height()), missionPanel, dc);
    missionPanel->Blit(missionPage, 0, 0, 0, 0, 0, 0);
    titleBar->Blit(missionPage, 2, 2, 0, 0, 0xff, 0x10);
    HGDIOBJ saved = SelectObject(dc, missionPage->DibSectionHandle());
    text.SetColor(0, nullptr);
    text.SetFont(5);
    text.MoveTo(5, 2);
    text.m_drawFlags = 0;
    const ResourceStringId missionTitle = StringId(0x8507);
    text.SetTextFromResource(&missionTitle);
    text.Measure(dc);
    text.Draw(dc);
    text.SetColor(0xffffff, nullptr);
    text.MoveTo(0x25, 0xc3);
    const ResourceStringId missionCaption = StringId(0x8504);
    text.SetTextFromResource(&missionCaption);
    text.Measure(dc);
    text.Draw(dc);
    SelectObject(dc, saved);
    SetPaintBackground(missionPage, kBackgroundMission);
    DeleteBitmap(missionPanel);

    DibBitmap* teamPanel = LoadBitmap(art, 0x2b5d, kResourceBitmapKind);
    DibBitmap* teamPage = NewOrNull<DibBitmap>(static_cast<LONG>(teamPanel->Width()), static_cast<LONG>(teamPanel->Height()),
                                               teamPanel, dc);
    teamPanel->Blit(teamPage, 0, 0, 0, 0, 0, 0);
    DeleteBitmap(teamPanel);
    titleBar->Blit(teamPage, 2, 2, 0, 0, 0xff, 0x10);
    saved = SelectObject(dc, teamPage->DibSectionHandle());
    text.SetFont(5);
    text.MoveTo(5, 2);
    text.m_drawFlags = 0;
    text.SetTextFromResource(&missionTitle);
    text.Measure(dc);
    text.Draw(dc);
    DibBitmap* availableCaption = LoadBitmap(art, alliance ? 0x2b71 : 0x2b73, kHelperBitmapKind);
    availableCaption->Blit(teamPage, 8, 0x41, 0, 0, 0, 0);
    DeleteBitmap(availableCaption);
    DibBitmap* selectedCaption = LoadBitmap(art, alliance ? 0x2b72 : 0x2b74, kHelperBitmapKind);
    selectedCaption->Blit(teamPage, 0x88, 0x41, 0, 0, 0, 0);
    DeleteBitmap(selectedCaption);
    SelectObject(dc, saved);
    AddOverlay(teamPage, kBackgroundTeam);
    DeleteBitmap(titleBar);
    CenterIn(m_galaxy->m_panelArea.left, m_galaxy->m_panelArea.top, m_galaxy->m_panelArea.right,
             m_galaxy->m_panelArea.bottom);

    struct PlainButton {
        int x;
        int y;
        int width;
        int height;
        uint32_t id;
        uint32_t normal;
        uint32_t pressed;
        uint16_t tooltip;
    };
    static const PlainButton plainButtons[] = {
        {0xf2, 3, 0xe, 0xe, kCloseId, 0x277c, 0x277d, 0},
        {0x65, 0xae, 0x41, 0x12, kBoxButtonId, 0x296e, 0x296f, 0x8510},
        {0xaa, 0x140, 0x40, 0x21, kCancelId, 0x2964, 0x2965, 0x8503},
        {0x66, 0x140, 0x40, 0x21, kOkId, 0x2962, 0x2963, 0x8502},
        {0x21, 0x140, 0x40, 0x21, kInfoId, 0x2960, 0x2961, 0x8031},
    };
    for (const PlainButton& spec : plainButtons) {
        CoolStrobeButton* button = NewButton(art, spec.x, spec.y, spec.width, spec.height, this, spec.id, spec.normal,
                                             spec.pressed, GetPaintBackgroundIfOwned());
        if (!button) continue;
        button->Create(0);
        if (spec.tooltip != 0) {
            const ResourceStringId tip = StringId(spec.tooltip);
            AddToolFromString(button, &tip);
        }
        m_children.Insert(button);
    }

    m_tabs = NewOrNull<CoolTabControl>(instance, 7, 0x14, 0xfd, 0x21, kTabsId, this, GetPaintBackgroundIfOwned());
    if (m_tabs) {
        m_tabs->Create(0);
        CoolStrobeButton* missionTab = NewOrNull<CoolStrobeButton>(art, 0, 0, 0x74, 0x21, m_tabs, kPageMission,
                                                                   alliance ? 0x2b5f : 0x2b61, alliance ? 0x2b60 : 0x2b62,
                                                                   m_tabs->m_background, 0, 1);
        if (missionTab) {
            missionTab->Create(0);
            m_tabs->AddTab(missionTab, 1, 0);
        }
        CoolStrobeButton* teamTab = NewOrNull<CoolStrobeButton>(art, 0x82, 0, 0x74, 0x21, m_tabs, kPageTeam,
                                                                alliance ? 0x2b63 : 0x2b65, alliance ? 0x2b64 : 0x2b66,
                                                                m_tabs->m_background, 0, 1);
        if (teamTab) {
            teamTab->Create(0);
            m_tabs->AddTab(teamTab, 0, 0);
        }
    }

    m_missionItems = NewItemList(1);
    m_missionBox = NewOrNull<CoolSelectionBox>(art, this, 0x23, 0x3e, 200, 0x71, 1, m_missionItems, GetPaintBackgroundIfOwned(), 0,
                                               0, 7, 0x299a, 4);
    if (m_missionBox) {
        m_missionBox->m_style &= ~kHiddenStyle;
        m_missionBox->Create(0);
        m_missionBox->m_textFormat = 0x111;
        m_missionBox->SetFont(4);
        m_missionBox->m_dropDownShade = nullptr;
        m_missionBox->m_dropDownTile = NewOrNull<DibBitmap>(art, 0x2966u, kResourceBitmapKind);
        m_missionBox->m_rowHeight = 0x71;
        m_missionBox->m_textColor = 0xffffff;
        m_missionBox->SetItems(m_missionItems);
        const ResourceStringId tip = StringId(0x8520);
        AddToolFromString(m_missionBox, &tip);
    }

    m_availableItems = NewItemList(4);
    m_availableList = NewOrNull<CoolDragList>(art, 8, 0x5d, 0x6c, 0xd5, this, FindOverlayBitmap(kBackgroundTeam),
                                              kAvailableListId, 0x6b, 0x3b, 0, 0, 0, 0, 10, 1, nullptr);
    if (m_availableList) ConfigureCharacterList(m_availableList, m_availableItems, m_instance);
    m_selectedItems = NewItemList(4);
    m_selectedList = NewOrNull<CoolDragList>(art, 0x88, 0x5d, 0x6c, 0xd5, this, FindOverlayBitmap(kBackgroundTeam),
                                             kSelectedListId, 0x6b, 0x3b, 0, 0, 0, 0, 10, 1, nullptr);
    if (m_selectedList) ConfigureCharacterList(m_selectedList, m_selectedItems, m_instance);

    AddHiddenArrow(this, art, 0x88, kMoveRightId, 0x2b6d, 0x2b6e, 0x8505);
    AddHiddenArrow(this, art, 0xdd, kMoveLeftId, 0x2b6f, 0x2b70, 0x8506);

    OkIdAvlTree candidates;
    m_order->CollectCandidatesB(reinterpret_cast<ObjectKeyValueSet*>(&candidates));
    for (AvlNode* node = candidates.First(); node; node = node->m_successor) {
        ObjectNameNode* nameNode = static_cast<ObjectNameNode*>(node);
        ObjectKey nameKey;
        ObjectKey candidateKey(*GetObjectNameNodeKey(nameNode, &nameKey));
        DatRecord* record = FindDataRecord(AsLookupKey(reinterpret_cast<NullableKey*>(&candidateKey)));
        uint32_t pictureId = record->m_small & 0xfff;
        if (!alliance) pictureId += 0x1000;
        DibBitmap* picture = NewOrNull<DibBitmap>(m_gokres, pictureId, kResourceBitmapKind);
        DibBitmap* normal = NewOrNull<DibBitmap>(kMissionPictureWidth, kMissionPictureHeight, picture);
        DibBitmap* highlighted = NewOrNull<DibBitmap>(kMissionPictureWidth, kMissionPictureHeight, picture);
        const int left = (kMissionPictureWidth - picture->Width()) / 2;
        const int top = (kMissionPictureHeight - picture->Height()) / 2;
        normal->FillRect(0, 0, normal->Width(), normal->Height(), picture->m_colorKey);
        highlighted->FillRect(0, 0, highlighted->Width(), highlighted->Height(), picture->m_colorKey);
        picture->DrawKeyed(normal, left, top);
        picture->DrawKeyed(highlighted, left, top);
        DeleteBitmap(picture);
        CoolListItem* item = nullptr;
        if (void* memory = ::operator new(sizeof(CoolListItem))) {
            item = new (memory) CoolListItem(MissionKindItemId(record));
        }
        item->SetImages(normal, highlighted);
        item->m_text = nameNode->m_name.m_text;
        item->m_ownColors = 1;
        item->m_color = 0x808080;
        item->m_userData = reinterpret_cast<uint32_t>(NewOrNull<ObjectKey>(candidateKey));
        m_missionItems->Insert(item);
    }
    CoolListItem* firstMission = m_missionItems->First();
    if (firstMission) m_missionBox->m_listFillColor = firstMission->CurrentImage()->m_colorKey;
    m_missionBox->SetItems(m_missionItems);
    m_missionBox->Show(kShow);

    ObjectKey scratchKey;
    ObjectKeyValueSet team(*m_order->GetTeam());
    uint8_t fillColor = 0;
    for (ObjectKeyNode* node = team.First(); node;) {
        ObjectKey nodeKey;
        ObjectKey key(*node->GetKey(&nodeKey));
        GameObjNotifier* object = GetObjectInFamilies_01_ff(m_galaxy->m_side, &key);
        if (!object) continue;
        AddCharacterRow(this, m_availableItems, key, object, &fillColor);
        node = static_cast<ObjectKeyNode*>(node->m_successor);
    }
    m_availableItems->First();
    m_availableList->m_fillColor = fillColor;
    m_availableList->Refresh();

    ObjectKeyValueSet chosen(m_order->m_characters);
    for (ObjectKeyNode* node = chosen.First(); node;) {
        ObjectKey nodeKey;
        ObjectKey key(*node->GetKey(&nodeKey));
        GameObjNotifier* object = GetObjectInFamilies_01_ff(m_galaxy->m_side, &key);
        if (!object) continue;
        uint8_t unusedFill = 0;
        AddCharacterRow(this, m_selectedItems, key, object, &unusedFill);
        // BUG(original): the original advances with the cursor of the first loop (null by now), so it would crash
        // for a selected character; the order has none when the window opens.
        node = static_cast<ObjectKeyNode*>(node->m_successor);
    }
    m_selectedItems->First();
    m_selectedList->m_fillColor = fillColor;
    m_selectedList->Refresh();

    DibBitmap* missionBackground = FindOverlayBitmap(kBackgroundMission);
    ObjectKey targetOut;
    ObjectKey targetKey(*m_order->GetTarget(&targetOut));
    GameObjNotifier* target = GetObjectInFamilies_01_ff(m_galaxy->m_side, &targetKey);
    DibBitmap* targetPicture = nullptr;
    if (target) {
        if (targetKey.Family() >= 0x90 && targetKey.Family() < 0x98) {
            targetPicture = NewOrNull<DibBitmap>(art, static_cast<uint32_t>(SectorSystemPictureId(
                                                          static_cast<System*>(static_cast<GameObj*>(target))->GetRecordField44())),
                                                 kResourceBitmapKind);
        } else {
            targetPicture = CreateObjectPicture(m_gokres, &targetKey, target, 1, 1);
        }
    }
    if (targetPicture && missionBackground) {
        HGDIOBJ previous = SelectObject(dc, missionBackground->DibSectionHandle());
        const int pictureWidth = targetPicture->Width();
        const int pictureHeight = targetPicture->Height();
        targetPicture->DrawKeyed(missionBackground, (0xa5 - pictureWidth) / 2 + 0x33, (0x4f - pictureHeight) / 2 + 0xd3);
        text.SetColor(0xffffff, nullptr);
        text.MoveTo(0x25, 0x126);
        text.SetFont(5);
        text.SetText(static_cast<GameObj*>(target)->GetName()->m_text);
        text.m_drawFlags = 1;
        text.m_layoutFlags = 1;
        text.Measure(dc);
        text.m_width = 0xb9;
        text.Draw(dc);
        SelectObject(dc, previous);
    }
    DeleteBitmap(targetPicture);
    DeleteDC(dc);
    PostMessageA(m_parent->m_hwnd, 0x467, m_childId, 0);
}

// ---- Commands -------------------------------------------------------------------------------------------------

ObjectKeyValueSet* __thiscall MissionOrderCopyTeam(MissionOrder240* order, ObjectKeyValueSet* out) {
    return new (out) ObjectKeyValueSet(*order->GetTeam());
}

ObjectKeyValueSet* __thiscall MissionOrderCopyCharacters(MissionOrder240* order, ObjectKeyValueSet* out) {
    return new (out) ObjectKeyValueSet(order->m_characters);
}

void MissionSelectionWindow::SelectPage(int page) {
    if (m_page == page) return;
    ChildWindowTree* children = &m_children;
    if (page == kPageMission) {
        SetPaintBackgroundById(kBackgroundMission);
        children->FindById(kBoxButtonId)->Show(kShow);
        m_availableList->Hide();
        m_selectedList->Hide();
        children->FindById(kMoveRightId)->Hide();
        children->FindById(kMoveLeftId)->Hide();
        m_missionBox->Show(kShow);
        SetFocus(m_missionBox->m_hwnd);
        m_page = kPageMission;
    } else if (page == kPageTeam) {
        SetPaintBackgroundById(kBackgroundTeam);
        m_missionBox->Hide();
        children->FindById(kBoxButtonId)->Hide();
        m_availableList->Show(kShow);
        m_selectedList->Show(kShow);
        children->FindById(kMoveRightId)->Show(kShow);
        children->FindById(kMoveLeftId)->Show(kShow);
        SetFocus(m_availableList->m_hwnd);
        m_page = kPageTeam;
    }
}

void MissionSelectionWindow::Virtual15() {
    if (m_page == kPageMission) {
        SetFocus(m_missionBox->m_hwnd);
        return;
    }
    SetFocus(m_availableList->m_hwnd);
}

void MissionSelectionWindow::OnKeyDown(WPARAM key, LPARAM lParam) {
    switch (key) {
    case VK_RETURN:
        OnCommand(kOkId, 0, 0);
        PostCloseRequest();
        return;
    case VK_ESCAPE:
        PostCloseRequest();
        return;
    case VK_LEFT:
    case VK_RIGHT:
        break;
    default:
        return;
    }
    if (m_page != kPageMission) {
        m_tabs->SelectTab(kPageMission, 1);
        return;
    }
    m_tabs->SelectTab(kPageTeam, 1);
}

namespace {

// Moves the selected rows of `from` to `to`, keeping the order's two key sets in step; returns whether anything moved.
bool MoveSelectedRows(CoolDragList* sourceList, CoolListItemList* sourceItems, CoolListItemList* targetItems,
                      ObjectKeyValueSet* sourceKeys, ObjectKeyValueSet* targetKeys, CoolListItemList& selection) {
    sourceList->CopySelectionTo(&selection);
    bool moved = false;
    for (LinkNode* node = selection.m_head; node;) {
        ObjectListItem* item = static_cast<ObjectListItem*>(sourceItems->FindById(static_cast<CoolListItem*>(node)->m_id));
        ObjectKey rowKey;
        item->GetKey(&rowKey);
        ObjectKeyNode* keyNode = sourceKeys->Find(&rowKey);
        ObjectKey nodeKey;
        keyNode->GetKey(&nodeKey);
        sourceKeys->Detach(&nodeKey);
        targetKeys->Add(keyNode);
        item->m_state.m_value &= ~1u;
        sourceItems->Remove(item);
        targetItems->Insert(item);
        moved = true;
        node = static_cast<CoolListItem*>(node)->Next();
    }
    return moved;
}

}  // namespace

void MissionSelectionWindow::OnCommand(int a, int b, int c) {
    uint32_t command = static_cast<uint32_t>(a) & 0xffff;
    switch (command) {
    case 0x66: {
        CoolListItem* selection = m_missionBox->GetSelection();
        m_order->m_missionKey = *reinterpret_cast<NullableKey*>(selection->m_userData);
        UiHumanReceiveMessage(reinterpret_cast<PlayerMessage*>(m_order), 0);
        m_order = nullptr;
    }
        [[fallthrough]];
    case 100:
    case 0x65:
        if (m_order) {
            delete m_order;
            m_order = nullptr;
        }
        PostCloseRequest();
        break;
    case 0x67: {
        CoolListItem* selection = m_missionBox->GetSelection();
        ObjectKey type(*reinterpret_cast<ObjectKey*>(selection->m_userData));
        ObjectKey object;
        UiOpenEncyclopedia(&object, reinterpret_cast<NullableKey*>(&type));
        break;
    }
    case 0x68:
        if (m_missionBox) m_missionBox->PressButton();
        break;
    case 0x96:
        SelectPage(static_cast<int>(static_cast<uint32_t>(b) >> 16));
        break;
    case 200:
    case 0xc9:
        if ((static_cast<uint32_t>(b) >> 16) != 0x309) return;
        command = (command != 200 ? 1u : 0u) + 0xca;
        [[fallthrough]];
    case 0xca:
    case 0xcb: {
        CoolListItemList selection(1);
        ObjectKeyValueSet first;
        ObjectKeyValueSet second;
        CoolDragList* sourceList;
        CoolListItemList* targetItems;
        CoolListItemList* sourceItems;
        if (command == 0xca) {
            sourceList = m_availableList;
            targetItems = m_selectedItems;
            sourceItems = m_availableItems;
            ObjectKeyValueSet team;
            first = *MissionOrderCopyTeam(m_order, &team);
            ObjectKeyValueSet characters;
            second = *MissionOrderCopyCharacters(m_order, &characters);
        } else {
            sourceList = m_selectedList;
            targetItems = m_availableItems;
            sourceItems = m_selectedItems;
            ObjectKeyValueSet characters;
            first = *MissionOrderCopyCharacters(m_order, &characters);
            ObjectKeyValueSet team;
            second = *MissionOrderCopyTeam(m_order, &team);
        }
        const bool moved = MoveSelectedRows(sourceList, sourceItems, targetItems, &first, &second, selection);
        if (command == 0xca) {
            m_order->m_characters = second;
            m_order->SetTeam(&first);
        } else {
            m_order->m_characters = first;
            m_order->SetTeam(&second);
        }
        if (moved) {
            m_availableList->Refresh();
            m_selectedList->Refresh();
        }
        break;
    }
    }
}

int __stdcall MissionKindItemId(DatRecord* record) {
    switch (record->m_third) {
    case 0x51:
        return 7;
    case 0x52:
        return 9;
    case 0x53:
        if (record->m_id == 0x20) return 4;
        if (record->m_id == 0x21) return 5;
        if (record->m_id == 0x22) return 6;
        break;
    case 0x54:
        return 10;
    case 0x55:
        return 1;
    case 0x56:
        return 8;
    case 0x57:
        return 3;
    case 0x58:
        return 2;
    case 0x61:
        return 0xd;
    case 0x62:
        return 0xc;
    case 99:
        return 0xb;
    case 0x69:
        return 0xf;
    case 0x6a:
        return 0xe;
    }
    return ++g_missionKindCounter;
}
