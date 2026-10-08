#include <cstdint>
#include <new>

#include "app/player_controller_decl.h"
#include "app/rebapp_globals.h"
#include "audio/sound_manager.h"
#include "core/checked_new.h"
#include "events/game_clock_hooks.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "gameobj/object_lookup.h"
#include "gameobj/status_code.h"
#include "orders/order_base.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "strategy_ui/gid_menu_window.h"
#include "strategy_ui/object_menu.h"
#include "strategy_ui/object_status_window.h"
#include "strategy_ui/order_confirm_window.h"
#include "strategy_ui/panel_drag_button.h"
#include "strategy_ui/report_window.h"
#include "strategy_ui/vote_window.h"
#include "ui/cool_game_window.h"
#include "ui/cool_list_item.h"
#include "ui/cool_window.h"
#include "ui/custom_dialog_box.h"
#include "ui/object_list_item.h"
#include "ui/sprite_stage.h"

ObjectKey* __thiscall FUN_004025b0(GameObjNotifier* object, ObjectKey* out);

namespace {

constexpr UINT kSkipInvisible = 1;
constexpr uint32_t kMouseModeNormal = 1;
constexpr uint32_t kMouseModeTargeting = 2;
constexpr uint32_t kBitmapSlot = 7;
constexpr uint32_t kTypeMask = 0x3f;
constexpr uint32_t kSectorWindowType = 1;
constexpr uint32_t kLegendWindowType = 3;
constexpr uint32_t kFinderWindowIdFirst = 0xd;
constexpr uint32_t kMessageIndexTypeId = 0xd;
constexpr uint32_t kOtherMessageWindowId = 0x19;
constexpr uint32_t kTargetCommandMove = 0x201;
constexpr uint32_t kTargetCommandMoveAlternate = 0x202;
constexpr uint32_t kTargetCommandSelect = 0x214;
constexpr uint32_t kTargetCommandMission = 0x240;
constexpr int kLegendWidth = 0xb4;
constexpr uint32_t kGidSound = 0x259;
constexpr uint32_t kSectorCloseSound = 0x25d;
constexpr uint32_t kDialogCloseSound = 0x25f;
constexpr uint32_t kSpeedMenuWidth = 0xa0;
constexpr uint32_t kGidMenuId = 0x140;
constexpr uint32_t kVoteWindowType = 6;
constexpr uint32_t kMessageIndexWindowType = 0xd;
constexpr uint32_t kOrderConfirmWindowType = 0x10;
constexpr uint32_t kObjectStatusWindowType = 0x1a;
constexpr uint32_t kReportWindowType = 0x1c;
constexpr uint32_t kWindowVisibleStyle = 0x10000000;
constexpr uint32_t kMovieRecordIdOffset = 0x5c;
constexpr uint32_t kMovieRecordNameOffset = 0x50;
constexpr uint32_t kOpenIdMask = 0x3f;

int LowWord(LPARAM value) { return static_cast<int>(static_cast<uint32_t>(value) & 0xffff); }
int HighWord(LPARAM value) { return static_cast<int>(static_cast<uint32_t>(value) >> 16); }
int LowWordSigned(LPARAM value) { return static_cast<int16_t>(static_cast<uint32_t>(value) & 0xffff); }
int HighWordSigned(LPARAM value) { return static_cast<int16_t>(static_cast<uint32_t>(value) >> 16); }

HWND SendToBottom() { return reinterpret_cast<HWND>(1); }

bool InputBlocked() { return static_cast<uint32_t>(GameHookTickCount()) < 2; }

RECT FrameOf(const CoolWindow* window) {
    RECT frame;
    frame.left = window->m_x;
    frame.top = window->m_y;
    frame.right = window->m_clientWidth + window->m_x;
    frame.bottom = window->m_clientHeight + window->m_y;
    return frame;
}

void PlaySpriteSound(uint32_t soundId) {
    SoundRequest request(0, 0, 0, 0, 0, 0);
    request.m_kind = 1;
    request.m_soundId = static_cast<int>(soundId);
    request.m_moduleIndex = kBitmapSlot;
    SoundManager::Play(&request);
}

void ShowUnminimizedChildren(GalaxyWindow* galaxy) {
    for (AvlNode* node = galaxy->m_children.First(); node; node = node->m_successor) {
        CoolWindow* child = static_cast<CoolWindow*>(node);
        if (galaxy->FindMinimizedSlot(child->m_childId) == 0) child->Show(5);
    }
}

HWND DescendToChildAt(HWND start, POINT* point) {
    HWND window = start;
    HWND child = ChildWindowFromPointEx(window, *point, kSkipInvisible);
    HWND previous = window;
    if (child != window) {
        do {
            window = child;
            MapWindowPoints(previous, window, point, 1);
            child = ChildWindowFromPointEx(window, *point, kSkipInvisible);
            previous = window;
        } while (child != window);
    }
    return window;
}

bool CommandAvailable(EventKindSet* commands, int kind) {
    EventKindNode* node = commands->Find(kind);
    return node && node->m_value != 0;
}

bool NoPopupOpen() { return g_popupStack.m_top == nullptr; }

// Climbs the parents of `object` looking for the system it is in; if found, `*key` becomes that system's key.
void ResolveSystemOf(GameObjNotifier* object, ObjectKey* key) {
    ObjectKey systemKey;
    bool found = false;
    do {
        if (found) break;
        ObjectKey objectKey;
        FUN_004025b0(object, &objectKey);
        systemKey = objectKey;
        if (systemKey.Family() >= 0x90 && systemKey.Family() < 0x98) found = true;
        object = object->m_parent;
    } while (object);
    if (found) *key = systemKey;
}

}  // namespace

// ---- WM_COMMAND --------------------------------------------------------------------------------------------

LRESULT GalaxyWindow::OnCommandMessage(WPARAM wParam, LPARAM lParam) {
    const bool blocked = InputBlocked();
    const uint32_t command = static_cast<uint32_t>(wParam) & 0xffff;
    switch (command) {
        case kGalaxySystemFinder:
            if (!blocked) OpenSystemFinder();
            return 0;
        case kGalaxyFleetFinder:
            if (!blocked) OpenFleetFinder();
            return 0;
        case kGalaxyPersonnelFinder:
            if (!blocked) OpenPersonnelFinder();
            return 0;
        case kGalaxyTroopFinder:
            if (!blocked) OpenTroopFinder();
            return 0;
        case kGalaxyEncyclopedia: {
            ObjectKey object;
            NullableKey type;
            type.m_value = 0;
            if (!blocked) OpenEncyclopedia(&object, &type);
            return 0;
        }
        case kGalaxyGidButton:
            if (!blocked) ToggleGidMenu();
            return 0;
        case kGalaxyGameControls:
            if (!blocked) m_optionsRequested = 1;
            return 0;
        case kGalaxyRestoreGidLegend: {
            if (m_displayMode == 0x92 || m_displayMode == 0x93) return 0;
            if (blocked) return 0;
            if (!m_gidLegend) return 0;
            RECT iconFrame = FrameOf(m_gidLegendIcon);
            RECT legendFrame = FrameOf(m_gidLegend);
            AnimateZoomRect(m_hwnd, &legendFrame, &iconFrame);
            m_gidLegend->ShowAndTrack(5);
            BringWindowToTop(m_gidLegend->m_hwnd);
            m_gidLegendIcon->Hide();
            return 0;
        }
        case 0x136:
            OpenMessageIndex(1, 0x7a);
            return 0;
        case 0x137:
            OpenMessageIndex(1, 0x7b);
            return 0;
        case 0x138:
            OpenMessageIndex(1, 0x7c);
            return 0;
        case 0x139:
            OpenMessageIndex(1, 0x7d);
            return 0;
        case 0x13a:
            OpenMessageIndex(1, 0x7e);
            return 0;
        case 0x13b:
            OpenMessageIndex(1, 0x7f);
            return 0;
        case 0x13c:
            OpenMessageIndex(1, 0x80);
            return 0;
        case 0x13d:
            OpenMessageIndex(1, 0x82);
            return 0;
        case 0x13e:
            OpenMessageIndex(1, 0x81);
            return 0;
        case kGalaxySetDisplayMode: {
            if (m_gidMenu) {
                RECT menuFrame = FrameOf(m_gidMenu);
                RECT buttonFrame = FrameOf(m_consoleButtons[5]);
                AnimateZoomRect(m_hwnd, &buttonFrame, &menuFrame);
            }
            const int mode = HighWord(wParam);
            SetDisplayMode(mode, reinterpret_cast<IdAvlTree*>(lParam));
            ShowDisplayLegend(mode);
            for (AvlNode* node = m_children.First(); node; node = node->m_successor) {
                CoolWindow* child = static_cast<CoolWindow*>(node);
                if (FindMinimizedSlot(child->m_childId) == 0) child->Show(5);
            }
            return 0;
        }
        case kGalaxyHelp:
            AppShowHelp();
            return 0;
        case kGalaxyCancelTargeting:
            if (m_unknownAc) return 0;
            if (m_mouseMode == kMouseModeTargeting) {
                delete static_cast<PlayerMessage*>(m_pendingCommand);
                m_pendingCommand = nullptr;
                m_mouseMode = kMouseModeNormal;
                if (GetCapture() == m_hwnd) ReleaseCapture();
            }
            AppSetState(0xb);
            return 0;
        default:
            break;
    }
    CoolWindow* child = m_children.FindById(command);
    if (!child) return 0;
    const uint32_t notification = static_cast<uint32_t>(wParam) >> 16;
    if (notification == 0x29a) {
        OnChildDropped(child, lParam);
    } else if (notification == 0x466) {
        MinimizeChildWindow(child);
        if (m_activeChild == child->m_hwnd) SendMessageA(m_hwnd, kGalaxyActivateChild, 0, 0);
    }
    return 0;
}

void GalaxyWindow::ToggleGidMenu() {
    RECT buttonFrame = FrameOf(m_consoleButtons[5]);
    if (m_gidLegend && IsWindowVisible(m_gidLegend->m_hwnd)) {
        RECT iconFrame = FrameOf(m_gidLegendIcon);
        RECT legendFrame;
        legendFrame.left = m_gidLegend->m_x;
        legendFrame.top = m_gidLegend->m_y;
        legendFrame.right = m_gidLegend->m_clientWidth + m_gidLegend->m_x;
        // BUG(original): the bottom edge forgets the window's y offset.
        legendFrame.bottom = m_gidLegend->m_clientHeight;
        AnimateZoomRect(m_hwnd, &iconFrame, &legendFrame);
        m_gidLegend->Hide();
        m_gidLegendIcon->Show(5);
    }
    for (AvlNode* node = m_children.First(); node; node = node->m_successor) static_cast<CoolWindow*>(node)->Hide();
    if (!m_gidMenu) {
        void* memory = ::operator new(sizeof(GidMenuWindow));
        const int menuX = m_side == 2 ? 0x1a4 : 0x172;
        m_gidMenu = memory ? new (memory) GidMenuWindow(m_instance, menuX, 0xe9, kSpeedMenuWidth, kSpeedMenuWidth,
                                                        m_side, this, kGidMenuId, GetBackground())
                           : nullptr;
        m_gidMenu->Create(0);
    }
    RECT menuFrame = FrameOf(m_gidMenu);
    AnimateZoomRect(m_hwnd, &menuFrame, &buttonFrame);
    m_gidMenu->ShowAndTrack(5);
}

void GalaxyWindow::OnChildDropped(CoolWindow* source, LPARAM cursor) {
    CustomDialogBox* sourceWindow = static_cast<CustomDialogBox*>(source);
    ObjectKeyValueSet dragged;
    sourceWindow->GetSelection(&dragged, nullptr);
    POINT inGalaxy = {LowWord(cursor), HighWord(cursor)};
    ScreenToClient(m_hwnd, &inGalaxy);
    HWND targetHwnd = ChildWindowFromPointEx(m_hwnd, inGalaxy, kSkipInvisible);
    CoolWindow* found = m_children.FindByHwnd(targetHwnd);
    if (targetHwnd && found) {
        POINT inTarget = {LowWord(cursor), HighWord(cursor)};
        ScreenToClient(targetHwnd, &inTarget);
        CustomDialogBox* target = static_cast<CustomDialogBox*>(found);
        const ObjectKey hitKey = target->GetKeyAt(inTarget.x, inTarget.y);
        ObjectKey dropKey = target->GetDropKeyAt(inTarget.x, inTarget.y);
        if (hitKey.Id() != 0 || dropKey.Id() != 0) {
            const int command = ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) + kTargetCommandMove;
            switch (source->m_childId & kTypeMask) {
                case kSectorWindowType: {
                    ObjectKeyValueSet movable;
                    ObjectKeyValueSet others;
                    while (ObjectKeyNode* first = dragged.First()) {
                        ObjectKey firstKey;
                        first->GetKey(&firstKey);
                        ObjectKeyValueNode* node = static_cast<ObjectKeyValueNode*>(dragged.Detach(&firstKey));
                        if (node->m_value & 4) {
                            void* memory = ::operator new(sizeof(ObjectKeyValueNode));
                            ObjectKey nodeKey;
                            node->GetKey(&nodeKey);
                            movable.Add(memory ? new (memory) ObjectKeyValueNode(nodeKey, 4) : nullptr);
                        }
                        node->m_value &= ~4;
                        if (node->m_value != 0) {
                            others.Add(node);
                        } else {
                            delete node;
                        }
                    }
                    if (others.First() && dropKey.IsSet()) UiHumanCommand(command, &others, &dropKey, nullptr);
                    if (movable.First() && dropKey.IsSet()) UiHumanCommand(kTargetCommandSelect, &movable, &dropKey, nullptr);
                    break;
                }
                case 4:
                case 10:
                    if (dropKey.IsSet()) UiHumanCommand(command, &dragged, &dropKey, nullptr);
                    break;
                case 9:
                    if (dropKey.IsSet()) UiHumanCommand(kTargetCommandSelect, &dragged, &dropKey, nullptr);
                    break;
                default:
                    break;
            }
        }
    }
    dragged.Clear();
}

// ---- Mouse buttons -----------------------------------------------------------------------------------------

LRESULT GalaxyWindow::OnLeftButtonUp(WPARAM keys, LPARAM position) {
    if (InputBlocked()) return 0;
    POINT cursor = {LowWord(position), HighWord(position)};
    if (m_mouseMode == kMouseModeNormal) {
        if (m_gidMenu && IsWindowVisible(m_gidMenu->m_hwnd)) {
            PlaySpriteSound(kGidSound);
            m_gidMenu->Hide();
            ShowUnminimizedChildren(this);
        }
        if (m_hoveredSector) {
            if (m_gidLegend && IsWindowVisible(m_gidLegend->m_hwnd)) {
                PostMessageA(m_hwnd, kGalaxyCloseChild, m_gidLegend->m_childId, 0);
            }
            ObjectKey sector;
            m_hoveredSector->GetKey(&sector);
            OpenSectorWindow(&sector);
            return 0;
        }
        if (m_droidStage2->HitTest(cursor.x, cursor.y)) OpenMessageIndex(1, 0x79);
        return 0;
    }
    if (m_mouseMode != kMouseModeTargeting) return 0;
    CompleteTargeting(keys, position);
    return 0;
}

void GalaxyWindow::CompleteTargeting(WPARAM keys, LPARAM position) {
    POINT cursor = {LowWord(position), HighWord(position)};
    const bool shift = (keys & MK_SHIFT) != 0;
    HWND hit = ChildWindowFromPointEx(m_hwnd, cursor, kSkipInvisible);
    CoolWindow* target = m_children.FindByHwnd(hit);
    bool proceed = true;
    if (!target) {
        target = m_children.FindByHwnd(GetParent(hit));
        if (!target && !shift) proceed = false;
    }
    if (proceed) {
        MapWindowPoints(m_hwnd, hit, &cursor, 1);
        if (shift) {
            HWND receiver = DescendToChildAt(hit, &cursor);
            m_mouseMode = kMouseModeNormal;
            const LPARAM packed = static_cast<LPARAM>((cursor.y & 0xffff) << 16 | (cursor.x & 0xffff));
            SendMessageA(receiver, WM_LBUTTONDOWN, 0, packed);
            SendMessageA(receiver, WM_LBUTTONUP, 0, packed);
            m_mouseMode = kMouseModeTargeting;
            SetCaptureWindow(m_hwnd);
        } else {
            m_currentCursor = m_normalCursor;
            ObjectKey key;
            OrderBase* pending = static_cast<OrderBase*>(m_pendingCommand);
            CustomDialogBox* dialog = static_cast<CustomDialogBox*>(target);
            if (!pending) {
                key = dialog->GetDropKeyAt(cursor.x, cursor.y);
            } else {
                const int kind = pending->GetKind();
                if (kind == kTargetCommandMove || kind == kTargetCommandSelect || kind == kTargetCommandMoveAlternate) {
                    key = dialog->GetDropKeyAt(cursor.x, cursor.y);
                } else {
                    key = dialog->GetKeyAt(cursor.x, cursor.y);
                }
            }
            if (!pending) {
                UiHumanAdvisorNotify(&key);
            } else if (key.Id() != 0) {
                switch (pending->GetKind()) {
                    case kTargetCommandMove:
                    case kTargetCommandMoveAlternate:
                    case kTargetCommandSelect:
                        if (key.IsSet()) {
                            pending->SetTarget(&key);
                            UiHumanReceiveMessage(reinterpret_cast<PlayerMessage*>(pending), 0);
                            m_pendingCommand = nullptr;
                        }
                        break;
                    case kTargetCommandMission:
                        if (key.IsSet()) {
                            if (pending->GetTeam()->Find(&key)) {
                                GameObjNotifier* object = GetObjectInFamilies_01_ff(m_side, &key);
                                if (object) ResolveSystemOf(object, &key);
                            }
                            pending->SetTarget(&key);
                            IssueTargetedCommand(pending);
                            m_pendingCommand = nullptr;
                        }
                        break;
                    default:
                        break;
                }
                delete static_cast<PlayerMessage*>(m_pendingCommand);
                m_pendingCommand = nullptr;
            }
        }
    }
    if (proceed && shift) return;
    delete static_cast<PlayerMessage*>(m_pendingCommand);
    m_pendingCommand = nullptr;
    m_mouseMode = kMouseModeNormal;
    if (GetCapture() == m_hwnd) ReleaseCapture();
}

LRESULT GalaxyWindow::OnLeftDoubleClick(WPARAM keys, LPARAM position) {
    (void)keys;
    if (InputBlocked()) return 0;
    POINT cursor = {LowWord(position), HighWord(position)};
    if (m_mouseMode == kMouseModeTargeting) {
        HWND receiver = DescendToChildAt(m_hwnd, &cursor);
        m_mouseMode = kMouseModeNormal;
        SendMessageA(receiver, WM_LBUTTONDBLCLK, 0, static_cast<LPARAM>((cursor.y & 0xffff) << 16 | (cursor.x & 0xffff)));
        m_mouseMode = kMouseModeTargeting;
        SetCaptureWindow(m_hwnd);
        return 0;
    }
    bool restored = false;
    for (int i = 0; i < 12; ++i) {
        if (restored) return 0;
        if (PtInRect(&m_minimized[i].m_area, cursor)) {
            RestoreMinimizedWindow(i + 1);
            restored = true;
        }
    }
    return 0;
}

LRESULT GalaxyWindow::OnRightButtonUp(WPARAM keys, LPARAM position) {
    (void)keys;
    if (InputBlocked()) return 0;
    const int x = LowWordSigned(position);
    const int y = HighWordSigned(position);
    RECT speedArea;
    if (m_side == 1) {
        SetRect(&speedArea, 0x67, 0x15, 0xb9, 0x21);
    } else {
        SetRect(&speedArea, 499, 0x13, 0x244, 0x1f);
    }
    if (uint32_t kind = m_droidStage->HitTest(x, y)) {
        ShowObjectMenu(x, y, kind);
        return 0;
    }
    if (uint32_t kind = m_droidStage2->HitTest(x, y)) {
        ShowObjectMenu(x, y, kind);
        return 0;
    }
    POINT point = {x, y};
    if (!PtInRect(&speedArea, point)) return 0;
    ShowSpeedMenu(x, y);
    return 0;
}

// ---- Child windows -----------------------------------------------------------------------------------------

void GalaxyWindow::OnCloseChild(WPARAM childId) {
    bool destroy = true;
    m_openWindowIds.DeleteById(static_cast<uint32_t>(childId));
    uint32_t soundId = 0;
    switch (childId & kTypeMask) {
        case kSectorWindowType:
            soundId = kSectorCloseSound;
            break;
        case kLegendWindowType:
            if (m_gidLegend) {
                RECT iconFrame = FrameOf(m_gidLegendIcon);
                RECT legendFrame = FrameOf(m_gidLegend);
                AnimateZoomRect(m_hwnd, &iconFrame, &legendFrame);
                m_gidLegend->Hide();
                m_gidLegendIcon->Show(5);
            }
            destroy = false;
            break;
        case 4:
        case 9:
        case 10:
        case 0xb:
            soundId = kDialogCloseSound;
            break;
        default:
            break;
    }
    if (soundId != 0) PlaySpriteSound(soundId);
    if (destroy) {
        DestroyChildById(static_cast<uint32_t>(childId));
        PostMessageA(m_hwnd, kGalaxyActivateChild, 0, 0);
    }
}

void GalaxyWindow::OnActivateChild(WPARAM window) {
    HWND popup = CurrentPopupHwnd();
    PostMessageA(m_hwnd, kGalaxyClosePopups, 0, 0);
    HWND target = popup ? popup : reinterpret_cast<HWND>(window);
    CoolWindow* targetWindow = m_children.FindByHwnd(target);
    if (m_activeChild == target) {
        if (!target) SetFocus(m_hwnd);
        return;
    }
    if (targetWindow) {
        if (m_mouseMode & 4) return;
        if (FindMinimizedSlot(targetWindow->m_childId) != 0) return;
    }
    if (IsWindow(m_activeChild)) {
        PostMessageA(m_activeChild, kGalaxyActivateChild, reinterpret_cast<WPARAM>(m_activeChild),
                     reinterpret_cast<LPARAM>(target));
    }
    if (target) {
        m_previousActiveChild = m_activeChild;
        m_activeChild = target;
    } else {
        m_activeChild = GetWindow(m_hwnd, GW_CHILD);
        bool found = false;
        while (m_activeChild && !found) {
            CoolWindow* candidate = m_children.FindByHwnd(m_activeChild);
            if (candidate && m_openWindowIds.FindById(candidate->m_childId) &&
                FindMinimizedSlot(candidate->m_childId) == 0) {
                found = true;
            }
            if (!found) m_activeChild = GetWindow(m_activeChild, GW_HWNDNEXT);
        }
    }
    if (!m_activeChild) {
        SetFocus(m_hwnd);
        return;
    }
    PostMessageA(m_activeChild, kGalaxyActivateChild, 0, reinterpret_cast<LPARAM>(m_activeChild));
    ShowWindow(m_activeChild, SW_SHOW);
    BringWindowToTop(m_activeChild);
    SetFocus(m_activeChild);
}

CoolWindow* GalaxyWindow::CreateModalWindow(WPARAM wParam, LPARAM lParam) {
    CoolWindow* window = nullptr;
    switch (wParam & kTypeMask) {
        case kVoteWindowType: {
            m_optionsRequested = 0;
            void* memory = ::operator new(sizeof(VoteWindow));
            window = memory ? new (memory) VoteWindow(m_instance, this, static_cast<uint32_t>(wParam),
                                                      reinterpret_cast<Notice2*>(lParam))
                            : nullptr;
            break;
        }
        case kMessageIndexWindowType: {
            int tab = 1;
            int kind = 0x79;
            if (lParam != 0) {
                tab = LowWord(lParam);
                kind = HighWord(lParam);
            }
            OpenMessageIndex(tab, kind);
            break;
        }
        case kOrderConfirmWindowType: {
            void* memory = ::operator new(sizeof(OrderConfirmWindow));
            window = memory ? new (memory) OrderConfirmWindow(m_instance, this, static_cast<uint32_t>(wParam),
                                                              reinterpret_cast<ConfirmRequest*>(lParam))
                            : nullptr;
            break;
        }
        case kObjectStatusWindowType: {
            ObjectKey* key = reinterpret_cast<ObjectKey*>(lParam);
            if (GetObjectInFamilies_01_ff(m_side, key) || !key->IsSet()) {
                void* memory = ::operator new(sizeof(ObjectStatusWindow));
                window = memory ? new (memory) ObjectStatusWindow(m_instance, this, static_cast<uint32_t>(wParam), key)
                                : nullptr;
            }
            if (key) ::operator delete(key);
            break;
        }
        case kReportWindowType: {
            void* memory = ::operator new(sizeof(ReportWindow));
            window = memory ? new (memory) ReportWindow(m_instance, this, static_cast<uint32_t>(wParam),
                                                        reinterpret_cast<ReportMessage*>(lParam))
                            : nullptr;
            break;
        }
        default:
            break;
    }
    return window;
}

LRESULT GalaxyWindow::OnModalRequest(UINT message, WPARAM wParam, LPARAM lParam) {
    if (InputBlocked()) return 0;
    PostMessageA(m_hwnd, kGalaxyClosePopups, 0, 0);
    if (m_gidMenu) m_gidMenu->Hide();
    if (!DismissPopup()) {
        do {
            m_inModalLoop = 1;
            MSG pending;
            while (PeekMessageA(&pending, nullptr, 0, 0, PM_NOREMOVE)) {
                if (pending.message == WM_QUIT) return 0;
                if (pending.hwnd == m_hwnd && pending.message == WM_DESTROY) return 0;
                GetMessageA(&pending, nullptr, 0, 0);
                TranslateMessage(&pending);
                DispatchMessageA(&pending);
            }
            int stepped = 1;
            if (!g_galaxyWindow) return 0;
            g_galaxyWindow->Virtual0B();
            AppStepGame(&stepped);
        } while (!DismissPopup());
    }
    m_inModalLoop = 0;
    SetCaptureWindow(m_hwnd);
    if (message != kGalaxyOpenModal) {
        if (wParam == 0) {
            AppSetState(static_cast<int>(lParam));
        } else if (wParam == 1) {
            SoundManager::Shutdown(0);
            const uint8_t* record = reinterpret_cast<const uint8_t*>(lParam);
            AppPlayMovie(*reinterpret_cast<const int*>(record + kMovieRecordIdOffset),
                         static_cast<int>(reinterpret_cast<uintptr_t>(record + kMovieRecordNameOffset)), 0);
        }
        return 0;
    }
    CoolWindow* window = CreateModalWindow(wParam, lParam);
    ReleaseCapture();
    if (window) {
        window->m_style &= ~kWindowVisibleStyle;
        window->Create(0);
        window->Show(5);
        m_children.Insert(window);
        SetFocus(window->m_hwnd);
    }
    return 0;
}

// ---- Accelerators ------------------------------------------------------------------------------------------

void GalaxyWindow::OnAccelerator(WPARAM wParam) {
    const bool noPopup = NoPopupOpen();
    EventKindSet commands;
    UiHumanRefreshMenus(&commands, 0);
    ObjectKeyValueSet selection;
    ObjectKey target;
    PostMessageA(m_hwnd, kGalaxyClosePopups, 0, 0);
    const bool idle = m_mouseMode == kMouseModeNormal;
    auto issue = [&](int kind) {
        if (noPopup && idle && CommandAvailable(&commands, kind)) UiHumanCommand(kind, &selection, &target, nullptr);
    };
    switch (static_cast<uint32_t>(wParam) & 0xffff) {
        case 0xbb9:
            if (noPopup) {
                for (AvlNode* node = m_children.First(); node; node = node->m_successor) {
                    CoolWindow* child = static_cast<CoolWindow*>(node);
                    if (FindMinimizedSlot(child->m_childId) == 0) static_cast<CoolGameWindow*>(child)->PostCloseRequest();
                }
            }
            break;
        case 0xbba:
            if (m_speed == 1) UiHumanCommand(0x22, &selection, &target, nullptr);
            else if (m_speed == 2) UiHumanCommand(0x23, &selection, &target, nullptr);
            else if (m_speed == 3) UiHumanCommand(0x24, &selection, &target, nullptr);
            break;
        case 0xbbb:
            if (m_speed == 2) UiHumanCommand(0x21, &selection, &target, nullptr);
            else if (m_speed == 3) UiHumanCommand(0x22, &selection, &target, nullptr);
            else if (m_speed == 4) UiHumanCommand(0x23, &selection, &target, nullptr);
            break;
        case 0xbbc:
            if (m_speed != 0) UiHumanCommand(0x20, &selection, &target, nullptr);
            break;
        case 0xbbd:
            if (noPopup && AppGetGameType() != 1) OpenMessageIndex(0x68, 0x79);
            break;
        case 0xbbe: {
            CoolWindow* messages = m_children.FindById(kMessageIndexTypeId);
            if (!messages) messages = m_children.FindById(kOtherMessageWindowId);
            if (messages) PostMessageA(messages->m_hwnd, kGalaxyAccelerator, 0xbbe, 0);
            break;
        }
        case 0xbbf:
            if (noPopup) {
                CoolWindow* active = m_children.FindByHwnd(m_activeChild);
                if (active) {
                    static_cast<CustomDialogBox*>(active)->GetSelection(&selection, nullptr);
                    if (selection.Count() == 1) {
                        EventKindSet available;
                        UiBuildAvailableOrders(AppGetSide(), &selection, &available);
                        if (CommandAvailable(&available, 0x103)) UiHumanCommand(0x103, &selection, &target, nullptr);
                    }
                }
            }
            break;
        case 0xbc0:
            if (noPopup) {
                CoolWindow* active = m_children.FindByHwnd(m_activeChild);
                if (active) {
                    static_cast<CustomDialogBox*>(active)->GetSelection(&selection, nullptr);
                    if (ObjectKeyNode* first = selection.First()) {
                        ObjectKey firstKey;
                        first->GetKey(&firstKey);
                        if (firstKey.Family() >= 0x30 && firstKey.Family() < 0x40) {
                            EventKindSet available;
                            UiBuildAvailableOrders(AppGetSide(), &selection, &available);
                            if (CommandAvailable(&available, 0x240)) UiHumanCommand(0x240, &selection, &target, nullptr);
                        }
                    }
                }
            }
            break;
        case 0xbc1:
            issue(0x110);
            break;
        case 0xbc2:
            issue(0x111);
            break;
        case 0xbc3:
            issue(0x112);
            break;
        case 0xbc4:
            issue(0x113);
            break;
        case 0xbc5:
            issue(0x115);
            break;
        case 0xbc6:
            issue(0x116);
            break;
        case 0xbc8:
            issue(0x11b);
            break;
        case 0xbc9:
            issue(0x11e);
            break;
        case 0xbca:
            SetDisplayMode(0x11, nullptr);
            break;
        case 0xbcb:
            SetDisplayMode(0x12, nullptr);
            break;
        case 0xbcc:
            SetDisplayMode(0x21, nullptr);
            break;
        case 0xbcd:
            SetDisplayMode(0x22, nullptr);
            break;
        case 0xbce:
            SetDisplayMode(0x43, nullptr);
            break;
        case 0xbcf:
            SetDisplayMode(0x44, nullptr);
            break;
        case 0xbd0:
            SetDisplayMode(0x65, nullptr);
            break;
        case 0xbd1:
            SetDisplayMode(0x66, nullptr);
            break;
        case 0xbd2:
            SetDisplayMode(0x67, nullptr);
            break;
        case 0xbd3:
        case 0xbd4:
            if (IsWindow(m_activeChild) && IsWindowVisible(m_activeChild) && noPopup) {
                CoolWindow* active = m_children.FindByHwnd(m_activeChild);
                IdNode* openId = active ? m_openWindowIds.FindById(active->m_childId) : nullptr;
                if (active && openId) ActivateNextOpenWindow(openId, (wParam & 0xffff) == 0xbd4);
            }
            break;
        case 0xbd5:
            AppSetState(0xe);
            break;
        case 0xbd7:
            issue(0x114);
            break;
        default:
            break;
    }
}

void GalaxyWindow::ActivateNextOpenWindow(IdNode* current, bool backwards) {
    const bool forwards = !backwards;
    bool wrapped = false;
    IdNode* found = nullptr;
    IdNode* node = current;
    do {
        node = static_cast<IdNode*>(forwards ? node->m_successor : node->Predecessor());
        if (!node) {
            if (!wrapped) {
                node = static_cast<IdNode*>(forwards ? m_openWindowIds.First() : m_openWindowIds.Last());
                wrapped = true;
            }
            if (!node && wrapped) break;
        }
        if ((node->m_id & kOpenIdMask) != kSectorWindowType && FindMinimizedSlot(node->m_id) == 0) found = node;
    } while (!found);
    if (!found) return;
    CoolWindow* window = m_children.FindById(found->m_id);
    SendMessageA(m_hwnd, kGalaxyActivateChild, reinterpret_cast<WPARAM>(window->m_hwnd), 0);
}
