#include "strategy_ui/vote_window.h"
#include <string>

#include <new>

#include "ai/ai_common.h"
#include "app/rebapp_globals.h"
#include "audio/music.h"
#include "character/role.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "economy/facility.h"
#include "events/player_notices.h"
#include "fleet/capship.h"
#include "fleet/fight_squad.h"
#include "fleet/fleet_iterator.h"
#include "fleet/troop_reg.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/battle_in_progress_window.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "tactical/comm2.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_label.h"
#include "ui/cool_tab_control.h"
#include "ui/object_list_item.h"
#include "ui/resource_dll.h"

extern const char* g_gokresDllName; // 0x6a8708 "gokres.dll"

namespace
{

constexpr int kWidth = 0x1d6;
constexpr int kHeight = 0x14b;
constexpr uint32_t kStyle = 0x46000000;
constexpr uint32_t kStringSlot = 0;
constexpr uint32_t kBitmapSlot = 7;
constexpr uint32_t kGokresSlot = 10;
constexpr uint32_t kHelperBitmapKind = 10;
constexpr int kAlliance = 1;
constexpr uint32_t kMessageCategory = 3;

constexpr int kFightButton = 0x65;
constexpr int kRetreatButton = 0x66;
constexpr int kResolveButton = 0x67;
constexpr uint32_t kTabsId = 200;
constexpr uint32_t kListId = 100;
constexpr uint32_t kBannerChildId = 7;

constexpr uint32_t kTextBackground = 0x32;
constexpr uint32_t kListBackground = 0x33;
constexpr int kShow = 5;
constexpr int16_t kStringGroup = 2;
constexpr uint16_t kClickSound = 0x260;

constexpr int kStateUsable = 1;
constexpr int kHeaderFont = 0xb;
constexpr int kFleetFont = 5;
constexpr int kObjectFont = 4;
constexpr UINT kCentered = 1;
constexpr int kPictureOffset = 0x46;
constexpr int kWidePictureOffset = 0x5a;
constexpr int kWidePictureExtra = 0x14;
constexpr COLORREF kListTextColor = 0xffffff;

constexpr int16_t kFacilitiesHeader = 0x1925;
constexpr int16_t kAllianceHeader = 0x1923;
constexpr int16_t kEmpireHeader = 0x1924;

struct SideArt
{
  uint32_t m_backdrop;
  uint32_t m_listOverlay;
  uint32_t m_buttonOverlay;
  COLORREF m_color;
  uint32_t m_fightNormal, m_fightPressed, m_fightDisabled;
  uint32_t m_retreatNormal, m_retreatPressed, m_retreatDisabled;
  uint32_t m_resolveNormal, m_resolvePressed, m_resolveDisabled;
  int m_tabsX, m_tabsY, m_tabsWidth, m_tabsHeight;
  int m_tabWidth, m_tabHeight;
};

constexpr SideArt kAllianceArt = {0x29d6, 0x29d8, 0x29da, 0xff,  0x29e2, 0x29e3, 0x29e4, 0x29e5, 0x29e6, 0x29e7,
                                  0x2ade, 0x2adf, 0x2ae0, 0x1a2, 0x15,   0x29,   0xe1,   0x29,   0x29};
constexpr SideArt kEmpireArt = {0x29d7, 0x29d9, 0x29db, 0xff00, 0x29dc, 0x29dd, 0x29de, 0x29df, 0x29e0, 0x29e1,
                                0x2adb, 0x2adc, 0x2add, 0x1aa,  0x11,   0x2c,   0xe9,   0x2c,   0x2d};

struct TabArt
{
  int m_y;
  uint32_t m_normal;
  uint32_t m_pressed;
  int16_t m_tooltip;
};

constexpr TabArt kAllianceTabs[4] = {{0, 0x29e8, 0x29e9, 0x1922},
                                     {0x3c, 0x29ea, 0x29eb, 0x1923},
                                     {0x7a, 0x29ec, 0x29ed, 0x1924},
                                     {0xb8, 0x29ee, 0x29ef, 0x1925}};
constexpr TabArt kEmpireTabs[4] = {{0, 0x29f2, 0x29f3, 0x1922},
                                   {0x3f, 0x29f4, 0x29f5, 0x1923},
                                   {0x7e, 0x29f6, 0x29f7, 0x1924},
                                   {0xbd, 0x29f8, 0x29f9, 0x1925}};

ResourceStringId StringId(int16_t id)
{
  return ResourceStringId{id, kStringGroup};
}

DibBitmap* NewResourceBitmap(HMODULE module, uint32_t id)
{
  return NewOrNull<DibBitmap>(module, id, kHelperBitmapKind);
}

DibBitmap* NewBitmapLike(DibBitmap* model, HDC dc)
{
  const int height = model->Height();
  const int width = model->Width();
  return NewOrNull<DibBitmap>(static_cast<LONG>(width), static_cast<LONG>(height), model, dc);
}

void DeleteBitmap(DibBitmap* bitmap)
{
  if (bitmap)
    delete bitmap;
}

CoolStrobeButton* NewVoteButton(VoteWindow* window,
                                HMODULE module,
                                int x,
                                uint32_t childId,
                                uint32_t normalId,
                                uint32_t pressedId,
                                uint32_t disabledId,
                                int16_t tooltipId)
{
  CoolStrobeButton* button = NewOrNull<CoolStrobeButton>(module, x, 0x128, 0x86, 0x1b, window, childId, normalId,
                                                         pressedId, window->GetPaintBackgroundIfOwned(), 0, 0);
  if (button)
  {
    button->Create(0);
    button->LoadStateBitmap(2, disabledId);
    const ResourceStringId tooltip = StringId(tooltipId);
    window->AddToolFromString(button, &tooltip);
  }
  return button;
}

void AddTab(VoteWindow* window, HMODULE module, const SideArt& art, const TabArt& tab, uint32_t tabId)
{
  CoolTabControl* tabs = window->m_tabs;
  CoolStrobeButton* button = NewOrNull<CoolStrobeButton>(module, 0, tab.m_y, art.m_tabWidth, art.m_tabHeight, tabs,
                                                         tabId, tab.m_normal, tab.m_pressed, tabs->m_background, 0, 0);
  if (button)
  {
    button->Create(0);
    button->SetClickSound(kClickSound);
    const ResourceStringId tooltip = StringId(tab.m_tooltip);
    window->AddToolFromString(button, &tooltip);
    tabs->AddTab(button, tabId == 1 ? 1 : 0, 0);
  }
}

void SetHeaderLook(ObjectListItem* item, int fontSlot, const char* text)
{
  item->m_text = text;
  item->m_fontSlot = fontSlot;
  item->m_state.m_value |= kListItemDisabled;
  item->m_textFormat = kCentered;
}

} // namespace

VoteFamilyRange::VoteFamilyRange(uint32_t first, uint32_t last)
{
  m_first = first;
  m_last = last;
}

VoteWindow::VoteWindow(HINSTANCE instance, GalaxyWindow* parent, uint32_t childId, Notice2* notice)
: CoolGameWindow(instance, 0, 0, kWidth, kHeight, parent, kStringSlot, kBitmapSlot, 0, childId, 0, 1, 0, 0)
{
  m_style = kStyle;
  m_notice = notice;
  m_galaxy = parent;
  m_list = nullptr;
  m_items = nullptr;
  m_tab = 0;
  m_tabs = nullptr;
  m_fightButton = nullptr;
  m_retreatButton = nullptr;
  m_resolveButton = nullptr;
  m_extra = nullptr;
  m_module = FindResourceModule(kGokresSlot);
  if (!m_module)
  {
    char* dllLocation = GetDllLocation();
    m_module = LoadResourceModule(dllLocation, g_gokresDllName, kGokresSlot);
    if (dllLocation)
      operator delete(dllLocation);
  }
}

VoteWindow::~VoteWindow()
{
  if (m_list)
    delete m_list;
  if (m_items)
  {
    m_items->Clear();
    delete m_items;
  }
  if (m_tabs)
    delete m_tabs;
  if (m_fightButton)
    delete m_fightButton;
  if (m_retreatButton)
    delete m_retreatButton;
  if (m_resolveButton)
    delete m_resolveButton;
  if (m_extra)
    delete m_extra;
}

void VoteWindow::OnCreate1()
{
  CommClearCategory(kMessageCategory);
  HINSTANCE instance = m_instance;
  HMODULE module = CoolAppGetResourceModule(kBitmapSlot);
  const bool alliance = m_galaxy->m_side == kAlliance;
  const SideArt& art = alliance ? kAllianceArt : kEmpireArt;
  const TabArt* tabArt = alliance ? kAllianceTabs : kEmpireTabs;

  DibBitmap* frame = NewResourceBitmap(module, art.m_backdrop);
  HDC windowDc = GetDC(m_hwnd);
  HDC dc = CreateCompatibleDC(windowDc);
  ReleaseDC(m_hwnd, windowDc);

  CoolLabel headline(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
  CoolLabel details(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
  headline.SetText(m_notice->m_text54.c_str());
  headline.SetFont(kHeaderFont);
  headline.m_drawFlags = kCentered;
  headline.MoveTo(0, 0x10);
  headline.SetColor(art.m_color, nullptr);
  headline.m_width = 400;
  headline.m_layoutFlags = kLabelFixedWidth;

  DibBitmap* listOverlay = NewResourceBitmap(module, art.m_listOverlay);
  DibBitmap* textBackground = NewBitmapLike(frame, dc);
  listOverlay->Blit(textBackground, 0xc, 0xd, 0, 0, 0, 0);
  frame->DrawKeyed(textBackground, 0, 0);
  HGDIOBJ previous = SelectObject(dc, textBackground->DibSectionHandle());
  headline.Draw(dc);
  details.SetText(m_notice->m_text60.c_str());
  details.m_layoutFlags = kLabelFixedWidth;
  details.SetFont(6);
  details.m_drawFlags = 0x110;
  details.m_width = 0x148;
  details.MoveTo(0x25, 0xd9);
  details.SetColor(0, nullptr);
  details.Draw(dc);
  details.MoveTo(0x24, 0xd8);
  details.Measure(dc);
  details.SetColor(art.m_color, nullptr);
  details.Draw(dc);
  SetPaintBackground(textBackground, kTextBackground);
  DeleteBitmap(listOverlay);

  DibBitmap* buttonOverlay = NewResourceBitmap(module, art.m_buttonOverlay);
  DibBitmap* listBackground = NewBitmapLike(frame, dc);
  buttonOverlay->Blit(listBackground, 0xc, 0xd, 0, 0, 0, 0);
  frame->DrawKeyed(listBackground, 0, 0);
  SelectObject(dc, listBackground->DibSectionHandle());
  headline.Draw(dc);
  AddOverlay(listBackground, kListBackground);
  DeleteBitmap(frame);
  DeleteBitmap(buttonOverlay);
  SelectObject(dc, previous);
  DeleteDC(dc);

  m_items = NewOrNull<CoolListItemList>(4);
  m_list = NewOrNull<CoolDragList>(instance, 0x53, 0x28, 0x100, 0xea, this, FindOverlayBitmap(kListBackground), kListId,
                                   0xf0, 0x28, 0, 0, 0, 0, 10, 1, nullptr);
  if (m_list)
  {
    m_list->m_options.m_value = 0x30000;
    m_list->m_style &= ~0x10000000u;
    m_list->Create(0);
    m_list->CreateVerticalScrollBar(instance, m_list, kBitmapSlot, 0x29fc, m_list->m_clientHeight, 0,
                                    m_list->m_clientHeight, 0, nullptr);
    m_list->m_textFormat = 4;
    m_list->SetItemList(m_items);
    m_list->SetTextColor(kListTextColor);
  }

  m_tabs = NewOrNull<CoolTabControl>(module, art.m_tabsX, art.m_tabsY, art.m_tabsWidth, art.m_tabsHeight, kTabsId, this,
                                     GetPaintBackgroundIfOwned());
  if (m_tabs)
  {
    m_tabs->Create(0);
    for (uint32_t index = 0; index < 4; ++index)
      AddTab(this, module, art, tabArt[index], index + 1);
  }

  m_fightButton =
    NewVoteButton(this, module, 0x92, kFightButton, art.m_fightNormal, art.m_fightPressed, art.m_fightDisabled, 0x1926);
  m_retreatButton = NewVoteButton(this, module, 0x118, kRetreatButton, art.m_retreatNormal, art.m_retreatPressed,
                                  art.m_retreatDisabled, 0x1927);
  m_resolveButton = NewVoteButton(this, module, 0xc, kResolveButton, art.m_resolveNormal, art.m_resolvePressed,
                                  art.m_resolveDisabled, 0x194a);
  if (m_notice->m_attack == 0)
    m_resolveButton->Disable();
  else
    m_resolveButton->Enable();

  SetTab(1);
  PlayAdviceMusic(alliance ? 10 : 0xb);
  CenterIn(m_galaxy->m_panelArea.left, m_galaxy->m_panelArea.top, m_galaxy->m_panelArea.right,
           m_galaxy->m_panelArea.bottom);
}

void VoteWindow::OnCommand(int a, int b, int c)
{
  int choice, timed;
  switch (static_cast<int16_t>(a))
  {
    case kFightButton:
      timed = 0;
      choice = 1;
      break;
    case kRetreatButton:
      timed = 0;
      choice = 0;
      break;
    case kResolveButton:
      timed = 1;
      choice = 1;
      break;
    case kTabsId:
      SetTab(static_cast<uint32_t>(b) >> 16);
      return;
    default:
      return;
  }
  UiHumanSetVote(choice, timed, m_notice);
  StopMusic();
  PostCloseRequest();
  OpenBattleBanner();
}

void VoteWindow::OnKeyDown(WPARAM key, LPARAM lParam)
{
  m_parent->WindowProc(WM_KEYDOWN, key, lParam);
}

void VoteWindow::SetTab(int tab)
{
  if (tab == m_tab)
    return;
  m_items->Clear();
  switch (tab)
  {
    case 1:
      m_list->Hide();
      SetPaintBackgroundById(kTextBackground);
      if (m_extra)
        m_extra->Show(kShow);
      break;
    case 2:
    case 3:
      SetPaintBackgroundById(kListBackground);
      FillList(tab - 1);
      m_list->Refresh();
      m_list->Show(kShow);
      if (m_extra)
        m_extra->Hide();
      break;
    case 4:
      SetPaintBackgroundById(kListBackground);
      FillList(0);
      m_list->Refresh();
      m_list->Show(kShow);
      if (m_extra)
        m_extra->Hide();
      break;
  }
  m_tab = tab;
}

void VoteWindow::FillList(int page)
{
  ObjectKey unset;
  ObjectListItem* header = NewOrNull<ObjectListItem>(&unset);
  ObjectKey systemKey(m_notice->m_systemKey);
  GameObjNotifier* system = GetObjectInFamilies_90_98(m_galaxy->m_side, &systemKey);

  std::string title;
  const ResourceStringId headerId = StringId(page == 0 ? kFacilitiesHeader :
                                             page == 1 ? kAllianceHeader :
                                                         kEmpireHeader);
  title = LoadStringResource(&headerId);
  SetHeaderLook(header, kHeaderFont, title.c_str());
  m_items->Insert(header);

  if (page == 0)
  {
    FacilityIterator facilities(system, kStateUsable);
    AddItems(&facilities, VoteFamilyRange(0, 0), 0);
    TroopRegIterator troops(system, kStateUsable);
    AddItems(&troops, VoteFamilyRange(0, 0), 0);
    RoleIterator roles(system, kStateUsable);
    AddItems(&roles, VoteFamilyRange(0, 0), 0);
    return;
  }

  FleetIterator fleets(system, kStateUsable, page);
  for (fleets.Begin(); fleets.Current(); fleets.Advance())
  {
    AddItem(fleets.Current(), kFleetFont, 0, 0);
    CapShipIterator ships(fleets.Current(), kStateUsable);
    for (ships.Begin(); ships.Current(); ships.Advance())
    {
      GameObjNotifier* ship = ships.Current();
      AddItem(ship, kFleetFont, 0, 1);
      FightSquadIterator squadrons(ship, kStateUsable);
      AddItems(&squadrons, VoteFamilyRange(0, 0), 1);
      TroopRegIterator troops(ship, kStateUsable);
      AddItems(&troops, VoteFamilyRange(0, 0), 1);
      RoleIterator roles(ship, kStateUsable);
      AddItems(&roles, VoteFamilyRange(0, 0), 1);
    }
  }

  FightSquadIterator defenders(system, kStateUsable, page);
  if (defenders.Count() != 0)
  {
    ObjectKey key;
    ObjectListItem* item = NewOrNull<ObjectListItem>(GetObjectKey(system, &key));
    item->m_text = static_cast<GameObj*>(system)->GetName()->c_str();
    item->m_fontSlot = kFleetFont;
    item->m_state.m_value |= kListItemDisabled;
    item->m_textFormat = kCentered;
    m_items->Insert(item);
    AddItems(&defenders, VoteFamilyRange(0, 0), 0);
  }
}

void VoteWindow::AddItems(ChildIterator* objects, VoteFamilyRange families, int widePicture)
{
  const bool acceptAll = families.m_first == 0 && families.m_last == 0;
  for (objects->Begin(); objects->Current(); objects->Advance())
  {
    ObjectKey key;
    GetObjectKey(objects->Current(), &key);
    const uint32_t family = key.Family();
    if (acceptAll || (family >= families.m_first && family < families.m_last))
      AddItem(objects->Current(), kObjectFont, widePicture, 1);
  }
}

void VoteWindow::AddItem(GameObjNotifier* object, int fontSlot, int widePicture, int withPicture)
{
  DibBitmap* image = nullptr;
  ObjectKey itemKey;
  ObjectListItem* item = NewOrNull<ObjectListItem>(GetObjectKey(object, &itemKey));
  item->m_textFormat = 1;
  if (withPicture)
  {
    ObjectKey pictureKey;
    DibBitmap* picture = CreateObjectPicture(m_module, GetObjectKey(object, &pictureKey), object, 0, 1);
    item->m_textOffsetX = kPictureOffset;
    image = picture;
    if (widePicture)
    {
      const int height = picture->Height();
      const int width = picture->Width();
      DibBitmap* wide = NewOrNull<DibBitmap>(width + kWidePictureExtra, height, picture);
      const uint8_t background = picture->m_colorKey;
      const int wideHeight = wide->Height();
      const int wideWidth = wide->Width();
      wide->FillRect(0, 0, wideWidth, wideHeight, background);
      picture->DrawKeyed(wide, kWidePictureExtra, 0);
      delete picture;
      item->m_textOffsetX = kWidePictureOffset;
      image = wide;
    }
    item->m_textFormat = 0;
  }
  item->m_fontSlot = fontSlot;
  item->SetImages(image, nullptr);
  item->m_text = static_cast<GameObj*>(object)->GetName()->c_str();
  item->m_state.m_value |= kListItemDisabled;
  m_items->Insert(item);
}

void VoteWindow::ShowButtons(int which, int disabled)
{
  CoolStrobeButton* button = which == 0 ? m_fightButton : m_retreatButton;
  if (disabled)
  {
    button->Disable();
    m_resolveButton->Disable();
    return;
  }
  button->Enable();
  if (m_notice->m_attack != 0)
    m_resolveButton->Enable();
  else
    m_resolveButton->Disable();
}

void VoteWindow::OpenBattleBanner()
{
  EndPopup();
  BattleInProgressWindow* banner = NewOrNull<BattleInProgressWindow>(m_instance, m_galaxy, kBannerChildId);
  if (banner)
  {
    banner->Create(0);
    banner->ShowAndTrack(kShow);
    m_galaxy->m_children.Insert(banner);
  }
}
