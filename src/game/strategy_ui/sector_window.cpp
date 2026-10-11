#include <string>
#include "core/reb_x87.h"
#include "strategy_ui/sector_window.h"

#include <cstdint>
#include <new>

#include "ai/ai_common.h"
#include "app/rebapp_globals.h"
#include "audio/sound_manager.h"
#include "core/checked_new.h"
#include "core/stream_io.h"
#include "galaxy/sector.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/galaxy_window.h"
#include "ui/bitmap_status_bar.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_image.h"
#include "ui/cool_label.h"
#include "ui/cool_list_item.h"
#include "ui/cool_window.h"

extern HPALETTE g_palette;
extern const int16_t g_sectorGridWidth;   // 0x659ef8
extern const int16_t g_sectorGridHeight;  // 0x659efa

uint32_t DimMenuColor(uint32_t color);

namespace {

constexpr uint32_t kArtSlot = 7;
constexpr uint32_t kStringSlot = 2;
constexpr uint32_t kCloseId = 0x138;
constexpr uint32_t kSideId = 0x139;
constexpr uint32_t kStyleVisible = 0x10000000;
constexpr int kNoDragStart = -1000;
constexpr int kDragSlack = 4;
constexpr uint32_t kSoundId = 0x25c;
constexpr uint32_t kSoundModule = 7;
constexpr UINT kGalaxyCloseSector = 0x405;
constexpr UINT kDragNotify = 0x29a;
constexpr double kSectorScaleX = 13.0;
constexpr double kSectorScaleY = 10.0;

void DeleteBitmap(DibBitmap* bitmap) {
    if (!bitmap) return;
    bitmap->~DibBitmap();
    ::operator delete(bitmap);
}

ResourceStringId StringId(uint16_t id) { return ResourceStringId{static_cast<int16_t>(id), 2}; }

int ScaledOffset(int delta, int gridSize, double scale, int extent) {
    return reb_ftol(static_cast<double>(delta) / (static_cast<double>(gridSize) + 1.0) * scale * extent);
}

bool InFamilies(uint32_t family, uint32_t first, uint32_t end) { return family >= first && family < end; }

}  // namespace

// ---- Construction ------------------------------------------------------------------------------------------

SectorWindow::SectorWindow(HINSTANCE instance, int x, int y, int width, int height, GalaxyWindow* parent,
                           DibBitmap* background, uint32_t childId, GameObjNotifier* sector)
    : CustomDialogBox(instance, x, y, width, height, parent, childId, background), m_systems(1), m_markers(1) {
    m_closeButton = nullptr;
    m_sideButton = nullptr;
    m_unknown154 = nullptr;
    m_dragging = 0;
    m_inputBlocked = 0;
    m_sector = sector;
    m_dragImage = nullptr;
    m_dragLayer = nullptr;
    m_dragStartX = kNoDragStart;
    m_dragStartY = kNoDragStart;
    m_hitItem = nullptr;
    m_memoryDc = nullptr;
    m_galaxy = parent;
}

SectorWindow::~SectorWindow() {

    delete m_sideButton;
    delete m_closeButton;
    DeleteBitmap(m_unknown154);
    if (m_dragImage) {
        m_dragImage->~CoolDragImage();
        ::operator delete(m_dragImage);
    }
    DeleteBitmap(m_dragLayer);
    if (m_remapTable) ::operator delete(m_remapTable);
}

void SectorWindow::OnCreate1() {
    std::string unusedText;
    std::string emptyText;
    HMODULE art = CoolAppGetResourceModule(kArtSlot);
    HMODULE strings = CoolAppGetResourceModule(kStringSlot);
    HDC window = GetDC(m_hwnd);
    HPALETTE previousPalette = SelectPalette(window, g_palette, 0);
    m_memoryDc = CreateCompatibleDC(window);
    DibBitmap* surface = NewOrNull<DibBitmap>(static_cast<LONG>(m_width), static_cast<LONG>(m_height),
                                              m_galaxy->m_offscreen, window);
    SetPaintBackground(surface, 1);
    m_remapTable = surface->BuildRemapTable(DimMenuColor);
    g_stringModule = strings;
    m_dragImage = NewOrNull<CoolDragImage>(m_galaxy);
    m_dragLayer = NewOrNull<DibBitmap>(static_cast<LONG>(m_clientWidth), static_cast<LONG>(m_clientHeight),
                                       m_paintBackground, window);
    SelectPalette(window, previousPalette, 0);
    ReleaseDC(m_hwnd, window);

    m_titleLabel->m_height = 0x14;
    m_titleLabel->MoveTo(2, 2);
    m_titleLabel->m_drawFlags = 0x25;
    m_titleLabel->SetText(static_cast<GameObj*>(m_sector)->GetName()->c_str());
    m_titleLabel->SetFont(4);
    m_titleLabel->SetColor(0x200f0f0, nullptr);

    DibBitmap* closeImage = NewOrNull<DibBitmap>(art, 0x277cu, 2u);
    if (closeImage) {
        const int imageWidth = closeImage->Width();
        m_closeButton = NewOrNull<CoolStrobeButton>(art, (m_width - imageWidth) - 3, 2, closeImage->Width(),
                                                    closeImage->Height(), this, kCloseId, 0x277cu, 0x277du, surface, 0, 1);
        m_closeButton->Create(0);
        DeleteBitmap(closeImage);
    }
    DibBitmap* sideImage = NewOrNull<DibBitmap>(art, 0x27e2u, 2u);
    if (sideImage) {
        const int left = m_closeButton->m_x;
        m_sideButton = NewOrNull<CoolStrobeButton>(art, left - sideImage->Width(), 2, sideImage->Width(),
                                                   sideImage->Height(), this, kSideId, 0x27e2u, 0x27e3u, surface, 0, 1);
        m_sideButton->Create(0);
        DeleteBitmap(sideImage);
    }
    BuildFrame(art, 0x2779, 0x277a, 0x277b, 0x2778, 0x2775, 0x2774, 0x2777, 0x2776);

    uint32_t originPosition;
    static_cast<Sector*>(m_sector)->GetPosition(&originPosition);
    int iconWidth;
    int iconHeight;
    {
        DibBitmap* icon = NewOrNull<DibBitmap>(art, static_cast<uint32_t>(SectorSystemPictureId(1)), 10u);
        iconWidth = icon->Width();
        iconHeight = icon->Height();
        DeleteBitmap(icon);
    }
    int markerWidth;
    int markerHeight;
    {
        DibBitmap* marker = NewOrNull<DibBitmap>(art, 0x2a13u, 10u);
        markerWidth = marker->Width() + 1;
        markerHeight = marker->Height() + 1;
        DeleteBitmap(marker);
    }

    SystemIterator systems(m_sector, 4);
    for (systems.Begin(); systems.Current(); systems.Advance()) {
        GameObjNotifier* system = systems.Current();
        ObjectKey key;
        GetObjectKey(system, &key);
        ObjectKeyIdNode* node = NewOrNull<ObjectKeyIdNode>();
        node->m_key = key;
        if (node) m_objects.Add(node, 0);

        ObjectListItem* icon = NewOrNull<ObjectListItem>(&key);
        if (icon) {
            uint32_t position;
            static_cast<System*>(system)->GetRecordField4c(&position);
            const int deltaX = static_cast<int16_t>(position) - static_cast<int16_t>(originPosition);
            const int x = ScaledOffset(deltaX, g_sectorGridWidth, kSectorScaleX, m_width);
            const int deltaY = static_cast<int16_t>(position >> 16) - static_cast<int16_t>(originPosition >> 16);
            const int y = ScaledOffset(deltaY, g_sectorGridHeight, kSectorScaleY, m_height);
            RECT area;
            SetRect(&area, x, y, iconWidth + x, iconHeight + y);
            icon->m_x = x;
            icon->m_y = y;
            icon->m_rect = area;
            icon->m_text = static_cast<GameObj*>(system)->GetName()->c_str();
            icon->m_textOffsetY = iconHeight + 0xb;
            m_systems.Insert(icon);

            BitmapStatusBar* energyBar = NewOrNull<BitmapStatusBar>(area.left, area.bottom + 2, area.right - area.left, 3,
                                                                    this, 0x2ffffff, 0x2ff0000, nullptr, 0, 0);
            if (energyBar) {
                energyBar->m_style &= ~kStyleVisible;
                energyBar->SetTickColor(0x808080);
                energyBar->Create(0);
                energyBar->m_childId = ((key.m_value & 0x3ff) << 6) | 1;
                m_children.Insert(energyBar);
                const ResourceStringId tip = StringId(0x1822);
                AddToolFromString(energyBar, &tip);
            }
            BitmapStatusBar* materialBar = NewOrNull<BitmapStatusBar>(
                energyBar->m_x, energyBar->m_y + 1 + energyBar->m_height, energyBar->m_width, 3, this, 0x200ffff,
                0x20f5cf9, nullptr, 0, 0);
            if (materialBar) {
                materialBar->m_style &= ~kStyleVisible;
                materialBar->SetTickColor(0x808080);
                materialBar->Create(0);
                materialBar->m_childId = ((key.m_value & 0x3ff) << 6) | 2;
                m_children.Insert(materialBar);
                const ResourceStringId tip = StringId(0x1823);
                AddToolFromString(materialBar, &tip);
            }
            const bool alliance = m_galaxy->m_side == 1;
            BitmapStatusBar* loyaltyBar = NewOrNull<BitmapStatusBar>(
                materialBar->m_x, materialBar->m_height + 1 + materialBar->m_y, materialBar->m_width, 3, this,
                alliance ? 0x20000ffu : 0x200ff00u, alliance ? 0x200ff00u : 0x20000ffu, nullptr, 0, 0);
            if (loyaltyBar) {
                loyaltyBar->m_style &= ~kStyleVisible;
                loyaltyBar->m_childId = ((key.m_value & 0x3ff) << 6) | 3;
                loyaltyBar->Create(0);
                const ResourceStringId tip = StringId(0x1821);
                AddToolFromString(loyaltyBar, &tip);
                m_children.Insert(loyaltyBar);
            }

            const int centerX = iconWidth / 2 + area.left;
            const int centerY = iconHeight / 2 + area.top;
            struct MarkerSlot {
                uint32_t m_kindBits;
                int m_left;
                int m_top;
            };
            const MarkerSlot slots[] = {
                {0x40000, centerX - markerWidth, centerY - markerHeight},
                {0x80000, centerX - markerWidth, centerY + 1},
                {0x100000, centerX + 1, centerY - markerHeight},
                {0x400000, centerX + 1, centerY + 1},
            };
            for (const MarkerSlot& slot : slots) {
                ObjectListItem* marker = NewOrNull<ObjectListItem>((key.m_value & 0xffff) | slot.m_kindBits, &key);
                if (!marker) continue;
                marker->m_rect.left = slot.m_left;
                marker->m_rect.top = slot.m_top;
                marker->m_rect.right = slot.m_left + markerWidth;
                marker->m_rect.bottom = slot.m_top + markerHeight;
                marker->m_state.m_value = marker->m_state.m_value & ~static_cast<uint32_t>(kMarkerVisible);
                marker->m_text = emptyText.c_str();
                m_markers.Insert(marker);
            }
            ObjectKey updatedKey;
            GetObjectKey(system, &updatedKey);
            OnObjectUpdated(&updatedKey, system);
        }
    }
    if ((m_galaxy->m_mouseMode & 4) == 0) {
        SoundRequest request(0, 0, 0, 0, 0, 0);
        request.m_kind = 1;
        request.m_soundId = kSoundId;
        request.m_moduleIndex = kSoundModule;
        SoundManager::Play(&request);
    }
}

// ---- Messages ----------------------------------------------------------------------------------------------

LRESULT SectorWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    HWND hwnd = m_hwnd;
    HWND galaxyWindow = m_galaxy->m_hwnd;
    if (message < 0x10) {
        if (message == WM_PAINT) {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(hwnd, &paint);
            HPALETTE previousPalette = SelectPalette(dc, g_palette, 0);
            m_inputBlocked = 0;
            m_galaxy->m_offscreen->Blit(m_paintBackground, 0, 0, m_x, m_y, m_clientWidth, m_clientHeight);
            m_paintBackground->ApplyRemapTable(0, 0, static_cast<int16_t>(m_clientWidth),
                                               static_cast<int16_t>(m_clientHeight));
            m_frame->DrawKeyed(m_paintBackground, 0, 0);
            (void)m_systems.First();
            HGDIOBJ previousBitmap = SelectObject(m_memoryDc, m_paintBackground->DibSectionHandle());
            CoolLabel label(static_cast<const char*>(nullptr), 0, 0, 100, 0, 1u, 1u);
            label.SetFont(10);
            label.m_drawFlags = 1;
            for (CoolListItem* item = m_systems.First(); item; item = item->Next()) {
                DibBitmap* image = item->m_selectedImage;
                if (!image) continue;
                int x = item->m_x;
                int y = item->m_y;
                image->DrawKeyed(m_paintBackground, x, y);
                label.SetText(item->m_text.c_str());
                label.SetColor(item->m_color, nullptr);
                x = (x + image->Width() / 2) - 0x32;
                y = y + item->m_textOffsetY;
                label.MoveTo(x, y);
                label.Draw(m_memoryDc);
            }
            SelectObject(m_memoryDc, previousBitmap);
            ObjectKey unusedKey;
            for (CoolListItem* marker = m_markers.First(); marker; marker = marker->Next()) {
                if ((marker->m_state.m_value & kMarkerVisible) == 0) continue;
                DibBitmap* image = marker->m_image;
                if ((marker->m_state.m_value & kMarkerHighlighted) != 0 && marker->m_selectedImage) {
                    image = marker->m_selectedImage;
                }
                if (image) image->DrawKeyed(m_paintBackground, marker->m_rect.left, marker->m_rect.top);
            }
            m_paintBackground->DrawToDc(dc, 0x40, SRCCOPY, 0, 0, 0, 0, 0, 0);
            m_titleLabel->Draw(dc);
            SelectPalette(dc, previousPalette, 0);
            EndPaint(hwnd, &paint);
            for (HWND child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
                UpdateWindow(child);
            }
            return 0;
        }
        if (message == WM_DESTROY) {
            if (!m_memoryDc) return 0;
            DeleteDC(m_memoryDc);
            return 0;
        }
        return CustomDialogBox::WindowProc(message, wParam, lParam);
    }

    if (message < 0x112) {
        if (message == WM_COMMAND) {
            if ((wParam & 0xffff) == kCloseId) {
                PostCloseRequest();
                return 0;
            }
            if ((wParam & 0xffff) != kSideId) return CustomDialogBox::WindowProc(WM_COMMAND, wParam, lParam);
            int x = m_x;
            int y = m_y;
            if (m_galaxy->m_side == 1) {
                x = x != 0x3c ? 0x3c : 300;
            } else if (m_galaxy->m_side == 2) {
                x = x != 0x78 ? 0x78 : 0x16d;
            }
            for (AvlNode* node = m_galaxy->m_children.First(); node; node = node->m_successor) {
                CoolWindow* other = static_cast<CoolWindow*>(node);
                if ((other->m_childId & 0x3f) == 1 && other->m_childId != m_childId) {
                    x = other->m_x;
                    y = other->m_y;
                    SendMessageA(galaxyWindow, kGalaxyCloseSector, other->m_childId, 0);
                    break;
                }
            }
            Hide();
            SetWindowPos(hwnd, nullptr, x, y, m_width, m_height, 0x305);
            ShowAndTrack(5);
            UpdateWindow(hwnd);
            return 0;
        }
        if (message == WM_KEYDOWN) {
            m_parent->WindowProc(WM_KEYDOWN, wParam, lParam);
            return 0;
        }
        return CustomDialogBox::WindowProc(message, wParam, lParam);
    }

    if (message < 0x216) {
        if (message != 0x215) {
            const int pointX = static_cast<int>(lParam & 0xffff);
            const int pointY = static_cast<int>(static_cast<uint32_t>(lParam) >> 16);
            switch (message) {
            case WM_MOUSEMOVE: {
                POINT point = {pointX, pointY};
                if (m_inputBlocked != 0) return 0;
                if (GetCapture() == hwnd && (wParam & MK_LBUTTON) != 0 && m_dragging == 0 && m_hitItem != nullptr) {
                    m_dragging = 1;
                    UpdateWindow(hwnd);
                    if (m_dragStartX == kNoDragStart || m_dragStartY == kNoDragStart) {
                        m_dragStartX = point.x;
                        m_dragStartY = point.y;
                    }
                    if ((wParam & MK_CONTROL) != 0) {
                        if (ObjectListItem* hit = HitTestMarkers(&m_markers, m_dragStartX, m_dragStartY)) {
                            if ((hit->m_state.m_value & kMarkerHighlighted) == 0) {
                                Highlight(hit);
                                m_hitItem = hit;
                            }
                        }
                    }
                    if ((m_hitItem->m_state.m_value & kMarkerHighlighted) != 0) {
                        m_dragImage->ClearImage();
                        m_dragImage->SetImage(m_dragLayer);
                        m_dragImage->SetHotspot(m_dragStartX, m_dragStartY);
                        POINT start = {m_dragStartX, m_dragStartY};
                        MapWindowPoints(hwnd, galaxyWindow, &start, 1);
                        m_dragImage->BeginDrag(start);
                        return 0;
                    }
                    m_dragging = 0;
                    return 0;
                }
                if (m_dragging == 0) return 0;
                MapWindowPoints(hwnd, galaxyWindow, &point, 1);
                m_dragImage->MoveTo(&point);
                return 0;
            }
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN: {
                if (m_inputBlocked != 0) return 0;
                if (GetCapture() != hwnd && message == WM_LBUTTONDOWN) {
                    SetCaptureWindow(hwnd);
                    m_dragStartX = pointX;
                    m_dragStartY = pointY;
                }
                m_hitItem = HitTestMarkers(&m_markers, pointX, pointY);
                if (!m_hitItem) return 0;
                if (m_dragging == 0) {
                    if ((m_hitItem->m_state.m_value & kMarkerHighlighted) == 0 && (wParam & MK_CONTROL) == 0) {
                        ClearHighlight(nullptr);
                        Highlight(m_hitItem);
                    }
                }
                if (message == WM_LBUTTONDOWN) return 0;
                return CustomDialogBox::WindowProc(message, wParam, lParam);
            }
            case WM_LBUTTONUP: {
                if (m_dragging == 0) {
                    if (GetCapture() == hwnd && m_hitItem != nullptr && (wParam & MK_CONTROL) != 0) {
                        if ((m_hitItem->m_state.m_value & kMarkerHighlighted) == 0) {
                            Highlight(m_hitItem);
                        } else {
                            ClearHighlight(m_hitItem);
                            m_hitItem = nullptr;
                        }
                    }
                } else {
                    const int distanceX = m_dragStartX < pointX ? pointX - m_dragStartX : m_dragStartX - pointX;
                    const int distanceY = m_dragStartY < pointY ? pointY - m_dragStartY : m_dragStartY - pointY;
                    if (distanceX > kDragSlack || distanceY > kDragSlack) {
                        PostMessageA(m_galaxy->m_hwnd, WM_COMMAND, (m_childId & 0xffff) | (kDragNotify << 16),
                                     static_cast<LPARAM>(GetMessagePos()));
                    }
                }
                if (GetCapture() != hwnd) return 0;
                ReleaseCapture();
                if (m_dragging == 0) return 0;
                m_dragImage->EndDrag();
                m_dragging = 0;
                return 0;
            }
            case WM_LBUTTONDBLCLK: {
                if (m_inputBlocked != 0) return 0;
                ObjectListItem* item = HitTestMarkers(&m_markers, pointX, pointY);
                if (!item) return 0;
                if ((item->m_state.m_value & kMarkerVisible) != 0) {
                    ClearHighlight(nullptr);
                    Highlight(item);
                    OpenMarkerWindow(item, pointX, pointY);
                }
                if (GetCapture() != hwnd) return 0;
                ReleaseCapture();
                return 0;
            }
            default:
                return CustomDialogBox::WindowProc(message, wParam, lParam);
            }
        }
    } else if (message != 0x40d) {
        if (message == 0x40e) return 0;
        return CustomDialogBox::WindowProc(message, wParam, lParam);
    }
    if (m_dragging != 0) {
        m_dragImage->EndDrag();
        m_dragging = 0;
    }
    if (GetCapture() != hwnd) return 0;
    ReleaseCapture();
    return 0;
}

// ---- Saved state and selection ------------------------------------------------------------------------------

void SectorWindow::SaveState(ostream* out) {
    ObjectKey key;
    if (m_sector) {
        ObjectKey sectorKey;
        GetObjectKey(m_sector, &sectorKey);
        key = sectorKey;
    }
    uint32_t childId = m_childId;
    key.Write(out);
    WriteValue32(out, &childId);
}

void SectorWindow::LoadState(istream* in) {
    ObjectKey key;
    key.Read(in);
    uint32_t childId;
    ReadValue32(in, &childId);
    m_sector = GetObjectInFamilies_80_90(AppGetSide(), &key);
    m_childId = childId;
}

void SectorWindow::GetSelection(ObjectKeyValueSet* out, CoolWindow* source) {
    out->Clear();
    *out = m_marked;
}

ObjectKey SectorWindow::GetKeyAt(int x, int y) {
    ObjectListItem* system = FindSystemAt(x, y);
    ObjectKey result;
    if (!system) return result;
    ObjectListItem* special = static_cast<ObjectListItem*>(m_markers.FindById(0x10000));
    system->GetKey(&result);
    if (!special) return result;
    POINT point = {x, y};
    if (!PtInRect(&special->m_rect, point)) return result;
    DibBitmap* image = special->m_image;
    if (!image || !image->IsPixelOpaque(x - special->m_rect.left, y - special->m_rect.top)) return result;
    special->GetKey(&result);
    return result;
}

ObjectKey SectorWindow::GetDropKeyAt(int x, int y) {
    ObjectListItem* system = FindSystemAt(x, y);
    ObjectKey result;
    if (system) system->GetKey(&result);
    return result;
}

const int16_t g_sectorGridHeight = 1023;
const int16_t g_sectorGridWidth = 1023;
