#include "strategy_ui/gid_menu_window.h"

#include <cstdint>
#include <new>

#include "core/game_string.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/galaxy_window.h"
#include "ui/cool_app.h"
#include "ui/cool_window.h"

namespace {

constexpr uint32_t kSpriteSlot = 7;
constexpr uint32_t kTextSlot = 2;
constexpr uint32_t kBitmapResourceKind = 2;
constexpr int kEntryHeight = 0x1e;
constexpr uint32_t kShadowColor = 0x2ffffff;
constexpr uint32_t kAllianceColor = 0x20000ff;
constexpr uint32_t kEmpireColor = 0x200ff00;
constexpr uint32_t kStringGroup = 2;
constexpr uint32_t kSubMenuWidth = 0xf0;
constexpr int kSubMenuX = 0x86;
constexpr uint32_t kFirstSubMenuId = 0x141;
constexpr uint32_t kCategoryPicture = 0x2783;
constexpr uint32_t kLastEntryText = 0x1614;
constexpr uint32_t kLastEntryCommand = 0x80;
constexpr int kLastEntryY = 0x8e;
constexpr int kLastEntrySpacing = 0x32;
constexpr int kSideEmpireOffset = 9;
constexpr uint32_t kArrowAlliance = 0x2785;
constexpr uint32_t kArrowAlliancePressed = 0x2786;
constexpr int kArrowEmpireOffset = 0xb;

struct MenuEntry {
    uint16_t m_text;
    uint32_t m_command;
    int m_y;
    int m_spacing;
    uint32_t m_alliancePicture;
    uint32_t m_empirePicture;
};

enum class WidthSource { kOwn, kFirstSubMenu };

struct SubMenuSpec {
    int m_y;
    int m_height;
    bool m_instanceIsModule;
    bool m_colorsBeforeFrame;
    WidthSource m_widthSource;
    uint32_t m_categoryCommand;
    uint32_t m_entryCount;
    MenuEntry m_entries[6];
};

constexpr MenuEntry kCategories[6] = {{0x1600, 0x10, 10, 0x14, 0x277e, 0x2787}, {0x1601, 0x20, 0x20, 0x14, 0x277f, 0x2788},
                                      {0x1602, 0x40, 0x36, 0x14, 0x2780, 0x2789}, {0x1605, 0x50, 0x4c, 0x14, 0x2781, 0x278a},
                                      {0x160a, 0x60, 0x62, 0x14, 0x2782, 0x278b}, {0x160f, 0x70, 0x78, 0x14, 0x2783, 0x278c}};

constexpr SubMenuSpec kSubMenus[6] = {
    {0xed, 0x37, false, false, WidthSource::kOwn, 0x10, 2,
     {{0x1615, 0x11, 7, 0x14, 0x277e, 0x2787}, {0x1616, 0x12, 0x1d, 0x14, 0x2d58, 0x2d59}}},
    {0x104, 0x37, false, false, WidthSource::kFirstSubMenu, 0x20, 2,
     {{0x1617, 0x21, 7, 0x14, 0x277f, 0x2788}, {0x1618, 0x22, 0x1d, 0x14, 0x2d5d, 0x2d5e}}},
    {0x104, 0x37, false, false, WidthSource::kOwn, 0x40, 2,
     {{0x161e, 0x44, 0x1d, 0x14, 0x279c, 0x278f}, {0x161d, 0x43, 7, 10, 0x279a, 0x2789}}},
    {0x11d, 100, true, true, WidthSource::kOwn, 0x50, 4,
     {{0x1606, 0x51, 7, 10, 0x2796, 0x2796},
      {0x1607, 0x52, 0x1d, 0x1e, 0x2797, 0x2797},
      {0x1608, 0x53, 0x33, 10, 0x2798, 0x2798},
      {0x1609, 0x54, 0x49, 0x1e, 0x2799, 0x2799}}},
    {0xed, 0x8e, false, true, WidthSource::kOwn, 0x60, 6,
     {{0x160c, 0x62, 7, 0x1e, 0x2793, 0x2793},
      {0x161c, 0x65, 0x1d, 0x1e, 0x2793, 0x2793},
      {0x160d, 0x63, 0x33, 10, 0x2794, 0x2794},
      {0x161b, 0x66, 0x49, 10, 0x2794, 0x2794},
      {0x160e, 0x64, 0x5f, 0x1e, 0x2795, 0x2795},
      {0x161a, 0x67, 0x75, 0x1e, 0x2795, 0x2795}}},
    {0x104, 0x7a, false, true, WidthSource::kOwn, 0x70, 5,
     {{0x1610, 0x71, 0x49, 10, 0x279c, 0x278f},
      {0x1611, 0x72, 0x33, 0x1e, 0x279d, 0x278e},
      {0x1612, 0x73, 0x5f, 10, 0x279e, 0x279e},
      {0x1613, 0x74, 0x1d, 0x1e, 0x279f, 0x278c},
      {0x1619, 0x75, 7, 0x1e, 0x2d5a, 0x2d5a}}},
};

uint32_t Picture(const MenuEntry& entry, int side) { return side == 1 ? entry.m_alliancePicture : entry.m_empirePicture; }

uint32_t BaseColor(int side) { return side != 1 ? kEmpireColor : kAllianceColor; }

void AddEntry(MenuWindowBase* menu, HMODULE module, const MenuEntry& entry, int width, int side) {
    const ResourceStringId text{static_cast<int16_t>(entry.m_text), static_cast<int16_t>(kStringGroup)};
    const uint32_t picture = Picture(entry, side);
    menu->AddTextEntry(module, &text, entry.m_command, kEntryHeight, entry.m_y, width, entry.m_spacing, picture, picture, 0, 0);
}

void BuildFrameOf(CoolGameWindow* window, HMODULE module) {
    window->BuildFrame(module, 0x2779, 0x277a, 0x277b, 0x2778, 0x2775, 0x2774, 0x2777, 0x2776);
}

}  // namespace

GidMenuWindow::GidMenuWindow(HINSTANCE instance, int x, int y, int width, int height, int side, CoolWindow* parent,
                             uint32_t childId, DibBitmap* backdrop)
    : MenuWindowBase(instance, x, y, width, height, parent, childId, backdrop) {
    m_backdrop = backdrop;
    m_side = side;
    m_subMenus[2] = nullptr;
    m_subMenus[3] = nullptr;
    m_subMenus[4] = nullptr;
    m_subMenus[5] = nullptr;
    m_owner = parent;
}

GidMenuWindow::~GidMenuWindow() = default;

void GidMenuWindow::OnCreate1() {
    GalaxyWindow* galaxy = GetGalaxyWindow();
    MenuWindowBase::OnCreate1();
    HINSTANCE instance = m_instance;
    HMODULE spriteModule = CoolAppGetResourceModule(kSpriteSlot);
    CoolAppGetResourceModule(kTextSlot);
    BuildFrameOf(this, spriteModule);
    for (const MenuEntry& category : kCategories) AddEntry(this, spriteModule, category, m_width, m_side);

    void* pictureMemory = ::operator new(sizeof(DibBitmap));
    DibBitmap* categoryPicture =
        pictureMemory ? new (pictureMemory) DibBitmap(spriteModule, kCategoryPicture, kBitmapResourceKind) : nullptr;
    const int pictureWidth = categoryPicture->Width();
    if (categoryPicture) {
        categoryPicture->~DibBitmap();
        ::operator delete(categoryPicture);
    }
    const ResourceStringId lastText{static_cast<int16_t>(kLastEntryText), static_cast<int16_t>(kStringGroup)};
    AddTextEntry(spriteModule, &lastText, kLastEntryCommand, pictureWidth + kEntryHeight, kLastEntryY, m_width,
                 kLastEntrySpacing, 0, 0, 0, 0);
    SetEntryColors(BaseColor(m_side), kShadowColor, 0);

    for (int i = 0; i < 6; ++i) {
        const SubMenuSpec& spec = kSubMenus[i];
        void* memory = ::operator new(sizeof(MenuWindowBase));
        HINSTANCE subInstance = spec.m_instanceIsModule ? reinterpret_cast<HINSTANCE>(spriteModule) : instance;
        MenuWindowBase* sub = memory ? new (memory) MenuWindowBase(subInstance, kSubMenuX, spec.m_y, kSubMenuWidth,
                                                                   spec.m_height, m_owner, kFirstSubMenuId + i,
                                                                   m_backdrop)
                                     : nullptr;
        m_subMenus[i] = sub;
        if (!sub) continue;
        sub->Create(0);
        sub->Hide();
        const int width = spec.m_widthSource == WidthSource::kOwn ? sub->m_width : m_subMenus[0]->m_width;
        const int pictureSide = (i == 2 || i == 5) ? galaxy->m_side : m_side;
        for (uint32_t entry = 0; entry < spec.m_entryCount; ++entry) {
            AddEntry(sub, spriteModule, spec.m_entries[entry], width, pictureSide);
        }
        if (spec.m_colorsBeforeFrame) {
            sub->SetEntryColors(BaseColor(m_side), kShadowColor, 0);
            BuildFrameOf(sub, spriteModule);
        } else {
            BuildFrameOf(sub, spriteModule);
            sub->SetEntryColors(BaseColor(m_side), kShadowColor, 0);
        }
        const int arrowOffset = m_side != 1 ? kArrowEmpireOffset : 0;
        AttachSubMenu(spriteModule, spec.m_categoryCommand, sub, kArrowAlliance + arrowOffset,
                      kArrowAlliancePressed + arrowOffset);
    }
    SetFocus(m_hwnd);
}
