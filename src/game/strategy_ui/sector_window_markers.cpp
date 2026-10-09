#include "core/reb_abi.h"
#include <cstdint>
#include <new>

#include "ai/ai_common.h"
#include "character/role.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "economy/defense_facility.h"
#include "economy/procfacil.h"
#include "fleet/fight_squad.h"
#include "fleet/fleet.h"
#include "fleet/troop_reg.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/fleet_window.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "strategy_ui/sector_window.h"
#include "strategy_ui/system_forces_window.h"
#include "strategy_ui/system_missions_window.h"
#include "strategy_ui/system_window.h"
#include "ui/bitmap_status_bar.h"
#include "ui/cool_app.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_list_item.h"


namespace {

constexpr uint32_t kArtSlot = 7;
constexpr uint32_t kStyleVisible = 0x10000000;
constexpr uint32_t kSpecialMarkerId = 0x10000;
constexpr uint32_t kStateMarkedSide = 0x1c;

ResourceStringId StringId(uint16_t id) { return ResourceStringId{static_cast<int16_t>(id), 2}; }

bool InFamilies(uint32_t family, uint32_t first, uint32_t end) { return family >= first && family < end; }

void DeleteBitmap(DibBitmap* bitmap) {
    if (!bitmap) return;
    bitmap->~DibBitmap();
    ::operator delete(bitmap);
}

uint32_t ControlBits(GameObjNotifier* object) { return static_cast<GameObjRoot*>(object)->m_flags; }

int ViewSide(int side) { return side == 1 ? 1 : (side == 2 ? 2 : 0); }

int SideOfMarkerState(uint32_t state) {
    const uint32_t bits = state & kStateMarkedSide;
    if (bits == 4) return 1;
    if (bits == 8) return 2;
    return bits != 0x10 ? 0 : 3;
}

}  // namespace

// ---- Pictures --------------------------------------------------------------------------------------------

int SectorSystemPictureId(int index) {
    if (index >= 1 && index <= 23) return 0x27e3 + index;
    switch (index) {
    case 24: return 0x27ff;
    case 25: return 0x27fd;
    case 26: return 0x27fe;
    default: return static_cast<int>(static_cast<uint32_t>((index - 1) >> 16 & 0xffff) << 16);
    }
}

uint32_t REB_STDCALL SectorMarkerImageId(uint32_t kind, int state, int selected) {
    uint32_t normal = 0;
    uint32_t picked = 0;
    uint32_t hovered = 0;
    uint32_t hoveredPicked = 0;
    uint32_t idle = 0;
    uint32_t idlePicked = 0;
    switch (kind) {
    case 4:
        normal = 0x2a13;
        picked = 0x2a14;
        hovered = 0x2a1b;
        hoveredPicked = 0x2a1c;
        idle = 0x2a23;
        idlePicked = 0x2a24;
        break;
    case 8:
        normal = 0x2a15;
        picked = 0x2a16;
        hovered = 0x2a1d;
        hoveredPicked = 0x2a1e;
        idle = 0x2a25;
        idlePicked = 0x2a26;
        break;
    case 0x10:
        normal = 0x2a17;
        picked = 0x2a18;
        hovered = 0x2a1f;
        hoveredPicked = 0x2a20;
        break;
    case 0x40:
        normal = 0x2a19;
        picked = 0x2a1a;
        hovered = 0x2a21;
        hoveredPicked = 0x2a22;
        break;
    }
    switch (state) {
    case 0:
    case 3: return (selected != 0 ? idlePicked : idle) & 0xffff;
    case 1: return (selected != 0 ? picked : normal) & 0xffff;
    case 2: return (selected != 0 ? hoveredPicked : hovered) & 0xffff;
    default: return 0;
    }
}

RECT* REB_STDCALL SectorMarkerLabelRect(RECT* out, ObjectListItem* item) {
    int x = item->m_x;
    int y = item->m_y;
    if (item->m_selectedImage == nullptr) {
        x += 0x14;
    } else {
        x += item->m_selectedImage->Width() / 2;
    }
    y += item->m_textOffsetY;
    out->left = x - 0x32;
    out->top = y;
    out->right = x + 0x32;
    out->bottom = y + 0x1e;
    return out;
}

// ---- Hit tests and windows -------------------------------------------------------------------------------

ObjectListItem* SectorWindow::FindSystemAt(int x, int y) {
    CoolListItem* item = m_systems.First();
    while (item) {
        POINT point = {x, y};
        if (PtInRect(&item->m_rect, point)) break;
        item = item->Next();
    }
    return static_cast<ObjectListItem*>(item);
}

ObjectListItem* SectorWindow::HitTestMarkers(CoolListItemList* markers, int x, int y) {
    CoolListItem* item = markers->First();
    bool found = false;
    while (item && !found) {
        const RECT area = item->m_rect;
        POINT point = {x, y};
        found = PtInRect(&area, point) != 0;
        if (found && item->m_id == kSpecialMarkerId && item->m_image &&
            item->m_image->IsPixelOpaque(x - area.left, y - area.top) == 0) {
            found = false;
        }
        if (!found) item = item->Next();
    }
    return static_cast<ObjectListItem*>(item);
}

CoolWindow* SectorWindow::OpenWindowOfType(const ObjectKey* system, int windowType) {
    uint32_t markerKind;
    switch (windowType) {
    case 4: markerKind = 0x10; break;
    case 9: markerKind = 4; break;
    case 10: markerKind = 8; break;
    case 11: markerKind = 0x40; break;
    default: return nullptr;
    }
    ObjectListItem* item = static_cast<ObjectListItem*>(
        m_markers.FindById((markerKind << 16) | static_cast<uint16_t>(system->m_value)));
    if (!item) return nullptr;
    return OpenMarkerWindow(item, item->m_rect.left, item->m_rect.top);
}

CoolWindow* SectorWindow::OpenMarkerWindow(ObjectListItem* item, int x, int y) {
    HINSTANCE instance = m_instance;
    HWND hwnd = m_hwnd;
    ObjectKey key;
    item->GetKey(&key);
    const uint32_t kind = item->m_id >> 16;
    uint32_t childId;
    switch (kind) {
    case 4: childId = ((key.m_value & 0x3ff) << 6) | 9; break;
    case 8: childId = ((key.m_value & 0x3ff) << 6) | 10; break;
    case 0x10: childId = ((key.m_value & 0x3ff) << 6) | 4; break;
    case 0x40: childId = ((key.m_value & 0x3ff) << 6) | 0xb; break;
    default: childId = 0;
    }
    CoolWindow* window = m_galaxy->m_children.FindById(childId);
    if (!window) {
        POINT position = {x, y};
        MapWindowPoints(hwnd, m_galaxy->m_hwnd, &position, 1);
        const RECT limits = m_galaxy->m_panelArea;
        ObjectKey systemKey;
        item->GetKey(&systemKey);
        GameObjNotifier* system = GetObjectInFamilies_90_98(m_galaxy->m_side, &systemKey);
        int advisorMask = 0;
        switch (kind) {
        case 4: {
            void* memory = ::operator new(0x26c);
            window = memory ? static_cast<CoolWindow*>(new (memory) SystemWindow(instance, position.x, position.y, 0xe2, 0x130, m_galaxy, childId, system))
                            : nullptr;
            advisorMask = 2;
            break;
        }
        case 8: {
            void* memory = ::operator new(sizeof(SystemForcesWindow));
            window = memory ? static_cast<CoolWindow*>(new (memory) SystemForcesWindow(instance, position.x, position.y, 0xeb,
                                                                                       0x130, m_galaxy, childId, system))
                            : nullptr;
            advisorMask = 8;
            break;
        }
        case 0x10: {
            void* memory = ::operator new(sizeof(FleetWindow));
            window = memory ? static_cast<CoolWindow*>(
                                  new (memory) FleetWindow(instance, position.x, position.y, m_galaxy, childId, system))
                            : nullptr;
            advisorMask = 4;
            break;
        }
        case 0x40: {
            void* memory = ::operator new(sizeof(SystemMissionsWindow));
            window = memory ? static_cast<CoolWindow*>(new (memory) SystemMissionsWindow(instance, position.x, position.y,
                                                                                         m_galaxy, childId, system))
                            : nullptr;
            advisorMask = 0x10;
            break;
        }
        }
        if (!window) return nullptr;
        CoolGameWindow* gameWindow = static_cast<CoolGameWindow*>(window);
        gameWindow->SetDragLimits(limits, nullptr);
        window->m_style &= ~kStyleVisible;
        gameWindow->SetDragMode(2);
        window->Create(0);
        int left = limits.left;
        if (limits.left <= position.x) {
            left = position.x;
            if (limits.right < position.x + window->m_width) left = limits.right - window->m_width;
        }
        int top = limits.top;
        if (limits.top <= position.y) {
            top = position.y;
            if (limits.bottom < position.y + window->m_height) top = limits.bottom - window->m_height;
        }
        SetWindowPos(window->m_hwnd, nullptr, left, top, 0, 0, 5);
        static_cast<CoolGameWindow*>(window)->ShowAndTrack(5);
        if (advisorMask != 0) UiHumanSetAdvisorMask(advisorMask);
        m_galaxy->TrackChildWindow(window);
        m_galaxy->m_children.Insert(window);
    }
    const int slot = m_galaxy->FindMinimizedSlot(window->m_childId);
    if (slot == 0) {
        if (!IsWindowVisible(window->m_hwnd)) static_cast<CoolGameWindow*>(window)->ShowAndTrack(5);
    } else {
        m_galaxy->RestoreMinimizedWindow(slot);
    }
    SetWindowPos(window->m_hwnd, nullptr, 0, 0, 0, 0, 3);
    return window;
}

// ---- Highlight ---------------------------------------------------------------------------------------------

void SectorWindow::ClearHighlight(ObjectListItem* item) {
    HWND hwnd = m_hwnd;
    if (!item) {
        const uint8_t color = m_markerColorKey;
        const int height = m_dragLayer->Height();
        const int width = m_dragLayer->Width();
        m_dragLayer->FillRect(0, 0, width, height, color);
        for (CoolListItem* marker = m_markers.First(); marker; marker = marker->Next()) {
            const uint32_t state = marker->m_state.m_value;
            if ((state & kMarkerHighlighted) != 0) {
                RECT area = marker->m_rect;
                marker->m_state.m_value = state & ~static_cast<uint32_t>(kMarkerHighlighted);
                InvalidateRect(hwnd, &area, TRUE);
            }
        }
        m_marked.Clear();
        return;
    }
    const uint32_t state = item->m_state.m_value;
    if ((state & kMarkerHighlighted) == 0) return;
    RECT area = item->m_rect;
    item->m_state.m_value = state & ~static_cast<uint32_t>(kMarkerHighlighted);
    m_dragLayer->FillRect(area.left, area.top, area.right - area.left, area.bottom - area.top, m_markerColorKey);
    InvalidateRect(hwnd, &area, TRUE);
    ObjectKey key;
    item->GetKey(&key);
    if (ObjectKeyNode* node = m_marked.Detach(&key)) delete node;
}

void SectorWindow::Highlight(ObjectListItem* item) {
    HWND hwnd = m_hwnd;
    ObjectKey key;
    item->GetKey(&key);
    GetObjectInFamilies_90_98(m_galaxy->m_side, &key);
    uint32_t* state = reinterpret_cast<uint32_t*>(&item->m_state.m_value);
    if ((item->m_state.m_value & kMarkerVisible) == 0) return;
    RECT area = item->m_rect;
    *state |= kMarkerHighlighted;
    InvalidateRect(hwnd, &area, FALSE);
    const int side = ViewSide(m_galaxy->m_side);
    const int markerSide = SideOfMarkerState(*state);
    DibBitmap* image = nullptr;
    if (side == markerSide && item->m_userData != 0) {
        area = item->m_rect;
        image = item->m_image;
    }
    if (image) image->DrawKeyed(m_dragLayer, area.left, area.top);
    ObjectKey nodeKey;
    item->GetKey(&nodeKey);
    ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(nodeKey, static_cast<int>(item->m_id >> 16));
    m_marked.Add(node);
}

// ---- Status bars -----------------------------------------------------------------------------------------

void SectorWindow::UpdateLoyaltyBar(const ObjectKey* system, int range, int position) {
    const int id = (static_cast<int16_t>(system->m_value) << 6) | 3;
    CoolwinStatusBar* bar = static_cast<CoolwinStatusBar*>(m_children.FindById(id));
    if (!bar) return;
    System* object = static_cast<System*>(GetObjectInFamilies_90_98(m_galaxy->m_side, system));
    if ((object->m_stateFlags & 8) != 0) {
        bar->Hide();
        return;
    }
    if (position >= 0 && static_cast<int>(bar->m_position) != position) bar->SetPosition(position);
    if (static_cast<int>(bar->m_range) != range && range > 0) bar->SetRange(range, 0);
    if (!IsWindowVisible(bar->m_hwnd) && (ControlBits(object) & 0xc0) != 0 && (object->m_flags & 1) != 0) bar->Show(5);
}

void SectorWindow::UpdateEnergyBar(const ObjectKey* system, int range, int position) {
    const int id = (static_cast<int16_t>(system->m_value) << 6) | 1;
    CoolwinStatusBar* bar = static_cast<CoolwinStatusBar*>(m_children.FindById(id));
    if (!bar) return;
    System* object = static_cast<System*>(GetObjectInFamilies_90_98(m_galaxy->m_side, system));
    if ((object->m_stateFlags & 8) != 0) {
        bar->Hide();
        return;
    }
    bool changed = false;
    int shownRange = range;
    if (static_cast<int>(bar->m_range) == range || range < 1) {
        shownRange = object->m_energy;
    } else {
        SetWindowPos(bar->m_hwnd, nullptr, 0, 0, range * 3, 3, 6);
        bar->SetRange(range, 1);
        changed = true;
    }
    int shownPosition = position;
    if (position < 0 || static_cast<int>(bar->m_position) == position) {
        shownPosition = static_cast<int>(bar->m_position);
    } else {
        bar->SetPosition(position);
        changed = true;
    }
    if (!IsWindowVisible(bar->m_hwnd) && (object->m_flags & 2) != 0) {
        bar->Show(5);
        changed = true;
    }
    if (changed) {
        const ResourceStringId titleId = StringId(0x1822);
        String tip(&titleId);
        const ResourceStringId separatorId = StringId(0x1824);
        String separator(&separatorId);
        const ResourceStringId endId = StringId(0x1825);
        String end(&endId);
        tip += separator;
        tip.AppendSigned(shownPosition);
        tip += end;
        tip.AppendSigned(shownRange);
        UpdateToolText(bar, tip.m_text);
    }
}

void SectorWindow::UpdateRawMaterialBar(const ObjectKey* system, int range, int position) {
    const int id = (static_cast<int16_t>(system->m_value) << 6) | 2;
    CoolwinStatusBar* bar = static_cast<CoolwinStatusBar*>(m_children.FindById(id));
    if (!bar) return;
    System* object = static_cast<System*>(GetObjectInFamilies_90_98(m_galaxy->m_side, system));
    if ((object->m_stateFlags & 8) != 0) {
        bar->Hide();
        return;
    }
    bool changed = false;
    int shownRange = range;
    if (static_cast<int>(bar->m_range) == range || range < 1) {
        shownRange = object->m_rawMaterial;
    } else {
        SetWindowPos(bar->m_hwnd, nullptr, 0, 0, range * 3, 3, 6);
        bar->SetRange(range, 1);
        changed = true;
    }
    int shownPosition = position;
    if (position < 0 || static_cast<int>(bar->m_position) == position) {
        shownPosition = static_cast<int>(bar->m_position);
    } else {
        bar->SetPosition(position);
        changed = true;
    }
    if (!IsWindowVisible(bar->m_hwnd) && (object->m_flags & 2) != 0) {
        bar->Show(5);
        changed = true;
    }
    if (changed) {
        const ResourceStringId titleId = StringId(0x1823);
        String tip(&titleId);
        const ResourceStringId separatorId = StringId(0x1824);
        String separator(&separatorId);
        const ResourceStringId endId = StringId(0x1825);
        String end(&endId);
        tip += separator;
        tip.AppendSigned(shownPosition);
        tip += end;
        tip.AppendSigned(shownRange);
        UpdateToolText(bar, tip.m_text);
    }
}

// ---- The system icon and its markers -----------------------------------------------------------------------

void SectorWindow::UpdateSystemMarkers(const ObjectKey* systemKey, int side) {
    bool changed = false;
    HMODULE art = CoolAppGetResourceModule(kArtSlot);
    System* system = static_cast<System*>(GetObjectInFamilies_90_98(m_galaxy->m_side, systemKey));
    ObjectListItem* icon = static_cast<ObjectListItem*>(m_systems.FindById(systemKey->m_value & 0xffffff));
    AllFacilityIterator facilities(system, 3);
    uint32_t flags;
    if ((system->m_stateFlags & 8) == 0) {
        flags = side == 1 ? 2 : (side == 2 ? 4 : 1);
        if ((system->m_flags & 4) != 0) {
            if (side == 1) {
                flags |= 0x10;
            } else if (side == 2) {
                flags |= 0x20;
            }
        }
        facilities.Begin();
        if (facilities.Current() != nullptr) flags |= 8;
    } else {
        flags = 0x40;
    }
    RECT area = icon->m_rect;
    RECT captionArea;
    SectorMarkerLabelRect(&captionArea, icon);
    RECT combined;
    UnionRect(&combined, &area, &captionArea);
    area = combined;
    if ((flags & 0x40) != 0) {
        icon->SetImageIds(0, 0x2800, art);
        icon->m_userData = 0x40;
        icon->m_ownColors = 1;
        icon->m_color = 0x2ffff00;
        InvalidateRect(m_hwnd, &area, FALSE);
        return;
    }
    if (((flags ^ icon->m_userData) & 7) != 0) {
        if ((flags & 7) == 2) {
            icon->m_ownColors = 1;
            icon->m_color = 0x20000ff;
        } else {
            icon->m_ownColors = 1;
            icon->m_color = (flags & 7) == 4 ? 0x200ff00 : 0x2ffff00;
        }
        changed = true;
    }
    DibBitmap* overlay = nullptr;
    if ((flags & 0x30) == 0x10) {
        overlay = NewOrNull<DibBitmap>(art, 0x389u, 2u);
    } else if ((flags & 0x30) == 0x20) {
        overlay = NewOrNull<DibBitmap>(art, 0x38au, 2u);
    }
    const uint32_t difference = flags ^ icon->m_userData;
    if ((difference & 0x30) != 0) changed = true;
    if ((difference & 8) != 0) {
        ObjectListItem* special = static_cast<ObjectListItem*>(m_markers.FindById(kSpecialMarkerId));
        if ((flags & 8) == 0) {
            if (special) {
                ObjectKey specialKey;
                special->GetKey(&specialKey);
                GameObjNotifier* object = GetObjectInFamilies_01_ff(m_galaxy->m_side, &specialKey);
                uint32_t sameSector = 0;
                if (object) {
                    GameObjNotifier* sector = object->m_parent ? object->m_parent->m_parent : nullptr;
                    if (sector) {
                        ObjectKey windowSectorKey;
                        ObjectKey objectSectorKey;
                        GetObjectKey(m_sector, &windowSectorKey);
                        GetObjectKey(sector, &objectSectorKey);
                        sameSector = objectSectorKey.m_value == windowSectorKey.m_value;
                    }
                }
                const uint32_t alive = object ? (~static_cast<GameObj*>(object)->m_stateFlags >> 3) & 1 : 0;
                if ((alive & sameSector) == 0 && m_markers.Remove(special)) delete special;
            }
        } else {
            GameObjNotifier* facility = facilities.Current();
            if (!special) {
                ObjectKey facilityKey;
                GetObjectKey(facility, &facilityKey);
                special = NewOrNull<ObjectListItem>(kSpecialMarkerId, &facilityKey);
                m_markers.Insert(special);
                special->SetImageIds(0x388, 0, art);
                special->m_userData = 1;
                special->m_state.m_value = 6;
            }
            special->m_rect = icon->m_rect;
        }
        changed = true;
    }
    DibBitmap* markerImage = m_galaxy->GetMarkerImage(system);
    if (markerImage != icon->m_markerImage) {
        changed = true;
        icon->m_markerImage = markerImage;
    }
    if (changed) {
        DibBitmap picture(art, static_cast<uint32_t>(SectorSystemPictureId(static_cast<int>(system->GetRecordField44()))), 2u);
        DibBitmap* surface = NewOrNull<DibBitmap>(picture.Width(), picture.Height() + 4, &picture);
        m_markerColorKey = picture.m_colorKey;
        const int surfaceHeight = surface->Height();
        const int surfaceWidth = surface->Width();
        surface->FillRect(0, 0, surfaceWidth, surfaceHeight, picture.m_colorKey);
        picture.Blit(surface, 0, 0, 0, 0, 0, 0);
        if (overlay) overlay->DrawKeyed(surface, 0, 0);
        if (DibBitmap* marker = icon->m_markerImage) {
            const int top = surface->Height() - marker->Height();
            marker->DrawKeyed(surface, 0, top);
        }
        icon->SetImages(nullptr, surface);
        icon->m_userData = flags;
        InvalidateRect(m_hwnd, &area, FALSE);
    }
    DeleteBitmap(overlay);
}

void SectorWindow::SetMarkerState(ObjectListItem* marker, int side, int count) {
    if (!marker) return;
    HMODULE art = CoolAppGetResourceModule(kArtSlot);
    ClearHighlight(marker);
    uint32_t* state = reinterpret_cast<uint32_t*>(&marker->m_state.m_value);
    bool changed = false;
    if (side != SideOfMarkerState(*state)) {
        *state = marker->m_state.Get() & 0xffffffe3;
        if (side == 1) {
            *state |= 4;
        } else if (side == 2) {
            *state |= 8;
        } else if (side == 3) {
            *state |= 0x10;
        }
        if (marker->m_userData != 0) {
            const uint32_t kind = marker->m_id >> 16;
            marker->SetImageIds(static_cast<uint16_t>(SectorMarkerImageId(kind, side, 0)),
                                static_cast<uint16_t>(SectorMarkerImageId(kind, side, 1)), art);
        }
        changed = true;
    }
    if (static_cast<int>(marker->m_userData) != count) {
        changed = true;
        marker->m_userData = static_cast<uint32_t>(count);
        if (count == 0) {
            RemoveRectTool(marker->m_id);
            *state = marker->m_state.Get() & 0xfffffffd;
        } else {
            *state |= 2;
            const uint32_t kind = marker->m_id >> 16;
            marker->SetImageIds(static_cast<uint16_t>(SectorMarkerImageId(kind, side, 0)),
                                static_cast<uint16_t>(SectorMarkerImageId(kind, side, 1)), art);
        }
    }
    if (changed) InvalidateRect(m_hwnd, &marker->m_rect, FALSE);
}

void SectorWindow::UpdateFleetMarker(GameObjNotifier* system, GameObjNotifier*) {
    ObjectKey key;
    GetObjectKey(system, &key);
    ObjectListItem* marker = static_cast<ObjectListItem*>(m_markers.FindById((key.m_value & 0xffff) | 0x100000));
    if (!marker) return;
    int side = ViewSide(m_galaxy->m_side);
    int count = CountFleets(static_cast<GameObj*>(system), side, 3);
    if (count == 0) {
        side = m_galaxy->m_side == 1 ? 2 : (m_galaxy->m_side == 2 ? 1 : 0);
        count = CountFleets(static_cast<GameObj*>(system), side, 3);
        if (count == 0) {
            side = (static_cast<GameObjRoot*>(system)->m_flags >> 6) & 3;
            count = CountFleets(static_cast<GameObj*>(system), side, 3);
        }
    }
    SetMarkerState(marker, side, count);
}

void SectorWindow::UpdateFacilityMarker(GameObjNotifier* system, GameObjNotifier*) {
    ObjectKey key;
    GetObjectKey(system, &key);
    ObjectListItem* marker = static_cast<ObjectListItem*>(m_markers.FindById((key.m_value & 0xffff) | 0x40000));
    int side = (static_cast<GameObjRoot*>(system)->m_flags >> 6) & 3;
    int count;
    {
        ProcFacilIterator facilities(system, 3);
        count = facilities.Count();
    }
    if (count == 0) side = 0;
    SetMarkerState(marker, side, count);
}

void SectorWindow::UpdateDefenseMarker(GameObjNotifier* system, GameObjNotifier*) {
    ObjectKey key;
    GetObjectKey(system, &key);
    ObjectListItem* marker = static_cast<ObjectListItem*>(m_markers.FindById((key.m_value & 0xffff) | 0x80000));
    if (!marker) return;
    const uint32_t systemFlags = static_cast<GameObjRoot*>(system)->m_flags;
    int count;
    {
        DefenseFacilityIterator defenses(system, 3);
        TroopRegIterator troops(system, 3);
        FightSquadIterator squadrons(system, 3);
        const int defenseCount = defenses.Count();
        const int troopCount = troops.Count();
        const int squadronCount = squadrons.Count();
        count = defenseCount + troopCount + squadronCount;
    }
    RoleIterator roles(system, 3);
    for (roles.Begin(); roles.Current(); roles.Advance()) {
        Role* role = static_cast<Role*>(roles.Current());
        ObjectKey missionKey;
        role->GetMissionKey(&missionKey);
        if (!missionKey.IsSet() || ((role->m_flags78 >> 8) & 1) != 0) ++count;
    }
    SetMarkerState(marker, (systemFlags >> 6) & 3, count);
}

void SectorWindow::UpdateMissionMarker(GameObjNotifier* system, GameObjNotifier*) {
    if (!system) return;
    ObjectKey key;
    GetObjectKey(system, &key);
    ObjectListItem* marker = static_cast<ObjectListItem*>(m_markers.FindById((key.m_value & 0xffff) | 0x400000));
    const int controlSide = DominantMissionSide(system, m_galaxy->m_side);
    const int count = (controlSide == 0 || controlSide == 3) ? 0 : 1;
    SetMarkerState(marker, controlSide, count);
}

// ---- Changes of the objects shown --------------------------------------------------------------------------

void SectorWindow::OnChildAdded(const ObjectKey* key, GameObjNotifier* object) {
    ObjectKey parentKey;
    GetObjectKey(object->m_parent, &parentKey);
    const uint32_t family = key->Family();
    const bool fleetOrFacility = InFamilies(family, 8, 0x10) || InFamilies(family, 0x28, 0x30);
    const bool unit = InFamilies(family, 0x1c, 0x20) || InFamilies(family, 0x22, 0x28) || InFamilies(family, 0x10, 0x14) ||
                      InFamilies(family, 0x20, 0x22) || InFamilies(family, 0x30, 0x40);
    if (!unit && !fleetOrFacility) return;
    if (m_objects.Find(key)) return;
    if (InFamilies(key->Family(), 0x20, 0x22)) {
        GameObjNotifier* parent = object->m_parent;
        ObjectKey updatedKey;
        GetObjectKey(parent, &updatedKey);
        OnObjectUpdated(&updatedKey, parent);
        return;
    }
    ObjectKeyIdNode* node = NewOrNull<ObjectKeyIdNode>();
    node->m_key = *key;
    m_objects.Add(node, 0);
    OnObjectUpdated(key, object);
}

void SectorWindow::OnChildMoved(const ObjectKey* key, GameObjNotifier* object, const ObjectKey* otherKey,
                                GameObjNotifier* otherObject) {
    GameObjNotifier* target = otherObject;
    if (!target) target = GetObjectInFamilies_01_ff(m_galaxy->m_side, otherKey);
    if (!InFamilies(otherKey->Family(), 0x90, 0x98)) return;
    ObjectKey copy(*otherKey);
    OnObjectUpdated(&copy, target);
    if (ObjectKeyNode* node = m_objects.Find(key)) m_objects.Remove(static_cast<ObjectKeyIdNode*>(node));
}

void SectorWindow::OnObjectUpdated(const ObjectKey* key, GameObjNotifier* object) {
    GameObjNotifier* parent = object->m_parent;
    const uint32_t family = key->Family();
    ObjectKey parentKey;
    GetObjectKey(parent, &parentKey);
    if (InFamilies(family, 0x90, 0x98)) {
        const int side = m_galaxy->m_side;
        System* system = static_cast<System*>(object ? object : GetObjectInFamilies_90_98(side, key));
        const int loyalty = system->GetLoyaltyFor(side);
        UpdateLoyaltyBar(key, 100, loyalty);
        UpdateEnergyBar(key, system->m_energy, system->m_energyAllocated);
        UpdateRawMaterialBar(key, system->m_rawMaterial, system->m_rawMaterialAllocated);
        UpdateSystemMarkers(key, (ControlBits(system) >> 6) & 3);
        UpdateFleetMarker(system, nullptr);
        UpdateFacilityMarker(system, nullptr);
        UpdateDefenseMarker(system, nullptr);
        UpdateMissionMarker(system, nullptr);
        return;
    }
    const bool parentIsSystem = InFamilies(parentKey.Family(), 0x90, 0x98);
    if (InFamilies(family, 0x28, 0x30) && parentIsSystem) {
        UpdateFacilityMarker(parent, object);
        return;
    }
    if (InFamilies(family, 0x1c, 0x20) || InFamilies(family, 0x22, 0x28) || InFamilies(family, 0x10, 0x14)) {
        UpdateDefenseMarker(parent, object);
        return;
    }
    if (InFamilies(family, 0x30, 0x40)) {
        UpdateDefenseMarker(parent, object);
        UpdateMissionMarker(parent, object);
        return;
    }
    if (InFamilies(family, 8, 0x10) && parentIsSystem) {
        UpdateFleetMarker(parent, object);
        return;
    }
    if (InFamilies(family, 0x40, 0x80)) UpdateMissionMarker(parent, object);
}
