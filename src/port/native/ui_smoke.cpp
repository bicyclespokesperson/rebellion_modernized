#include "port/native/ui_smoke.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <memory>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "app/game_session.h"
#include "app/ai_preferences.h"
#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "core/game_string.h"
#include "ai/ai_controller_factory.h"
#include "character/character.h"
#include "economy/defense_facility.h"
#include "events/player_notices.h"
#include "events/game_step_driver.h"
#include "events/net_messages.h"
#include "gameobj/game_root.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_queries.h"
#include "gfx/dib_bitmap.h"
#include "gfx/indexed_sprite_filter.h"
#include "port/minwin_impl/gdi_api.h"
#include "port/minwin_impl/config/config.h"
#include "port/minwin_impl/user/input.h"
#include "rules/event_rules.h"
#include "story/story_characters.h"
#include "strategy_ui/battle_window.h"
#include "strategy_ui/fleet_list_item.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/main_menu_window.h"
#include "strategy_ui/object_menu.h"
#include "strategy_ui/object_status_window.h"
#include "strategy_ui/report_window.h"
#include "strategy_ui/sector_window.h"
#include "ui/cool_button.h"
#include "ui/cool_app.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_environment.h"
#include "ui/cool_game_window.h"
#include "ui/cool_label.h"
#include "ui/cool_tab_control.h"
#include "ui/message_template.h"
#include "ui/object_list_item.h"
#include "ui/sprite.h"

extern int g_spriteDllSide;

namespace
{
void Require(bool condition)
{
  if (!condition)
    throw std::runtime_error("Native UI smoke: fleet-row visibility regression");
}

void RequireBattle(bool condition)
{
  if (!condition)
    throw std::runtime_error("Native UI smoke: battle UI regression");
}

void RequireReport(bool condition, const char* check)
{
  if (!condition)
    throw std::runtime_error(std::string("Native UI smoke: report rendering regression: ") + check);
}

void RequireStatus(bool condition, const char* check)
{
  if (!condition)
    throw std::runtime_error(std::string("Native UI smoke: ship status label layout regression: ") + check);
}

FleetListItem* AddRow(CoolListItemList& items, DibBitmap& format, uint32_t id, uint32_t flags, uint8_t color)
{
  const ObjectKey key = ObjectKey::Make((flags & kFleetItemFleet) ? 8 : 0x14, id);
  auto row = std::make_unique<FleetListItem>(&key);
  row->m_flags.m_value = flags;
  auto image = std::make_unique<DibBitmap>(10, 10, &format);
  Require(image->IsLoaded());
  image->FillRect(0, 0, 10, 10, color);
  image->m_colorKey = 0;
  row->SetImages(image.release(), nullptr);
  Require(items.Insert(row.get()));
  return row.release();
}

uint8_t Pixel(const DibBitmap& bitmap, int x, int y)
{
  return bitmap.m_bits[bitmap.RowIndex(y) * bitmap.RowStride() + x];
}

LRESULT REB_STDCALL WindowMoveSmokeProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message == WM_ERASEBKGND)
    return 1;
  if (message == WM_PAINT)
  {
    PAINTSTRUCT paint;
    HDC dc = BeginPaint(hwnd, &paint);
    const HWND parent = GetParent(hwnd);
    const COLORREF color = !parent ? RGB(255, 255, 255)
                           : GetParent(parent) ? RGB(0, 0, 255) : RGB(255, 0, 0);
    HBRUSH brush = CreateSolidBrush(color);
    RECT bounds;
    GetClientRect(hwnd, &bounds);
    FillRect(dc, &bounds, brush);
    DeleteObject(brush);
    EndPaint(hwnd, &paint);
    return 0;
  }
  return DefWindowProcA(hwnd, message, wParam, lParam);
}

void CheckLiveWindowMovePaint(HINSTANCE instance)
{
  auto require = [](bool condition, const char* check)
  {
    if (!condition)
      throw std::runtime_error(std::string("Native UI smoke: live window move regression: ") + check);
  };
  WNDCLASSA cls{};
  cls.hInstance = instance;
  cls.lpfnWndProc = WindowMoveSmokeProc;
  cls.lpszClassName = "NativeWindowMoveSmoke";
  require(RegisterClassA(&cls) != 0, "window class registered");
  constexpr int rootX = 320, rootY = 100;
  HWND root = CreateWindowExA(0, cls.lpszClassName, "", WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
                              rootX, rootY, 120, 100, nullptr, nullptr, instance, nullptr);
  require(root != nullptr, "root created");
  HWND child = CreateWindowExA(0, cls.lpszClassName, "", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                               10, 10, 40, 40, root, nullptr, instance, nullptr);
  require(child != nullptr, "child created");
  HWND grandchild = CreateWindowExA(0, cls.lpszClassName, "", WS_CHILD | WS_VISIBLE,
                                    5, 5, 10, 10, child, nullptr, instance, nullptr);
  require(grandchild != nullptr, "grandchild created");
  const int previousScale = minwin_gdi::DeviceScale();
  minwin_gdi::DeviceScreen(4);
  UpdateWindow(root);
  UpdateWindow(child);
  UpdateWindow(grandchild);
  uint32_t palette[256];
  minwin_gdi::CurrentPalette(palette, nullptr);
  auto colorAt = [&](int x, int y)
  {
    const minwin_gdi::ScreenSurface screen = minwin_gdi::Screen();
    return palette[screen.bits[y * screen.pitch + x]];
  };
  auto deviceColorAt = [&](int x, int y)
  {
    const minwin_gdi::ScreenSurface screen = minwin_gdi::DeviceScreen(4);
    return palette[screen.bits[y * 4 * screen.pitch + x * 4]];
  };
  require(colorAt(rootX + 1, rootY + 1) == 0xffffff, "root painted");
  require(colorAt(rootX + 45, rootY + 40) == 0xff0000, "child painted");
  require(colorAt(rootX + 18, rootY + 18) == 0x0000ff, "grandchild painted");
  for (int newX : {30, 50, 70})
  {
    const int oldX = newX - 20;
    require(SetWindowPos(child, nullptr, newX, 10, 0, 0, SWP_NOSIZE | SWP_NOZORDER), "child moved");
    require(colorAt(rootX + oldX + 1, rootY + 40) == 0xffffff, "uncovered root repainted immediately");
    require(colorAt(rootX + newX + 35, rootY + 40) == 0xff0000, "moved child repainted immediately");
    require(colorAt(rootX + newX + 8, rootY + 18) == 0x0000ff, "moved grandchild repainted immediately");
    require(deviceColorAt(rootX + newX + 35, rootY + 40) == 0xff0000, "device pixels repainted immediately");
  }
  DestroyWindow(root);
  minwin_gdi::DeviceScreen(previousScale);
}

void CheckHighResolutionPaletteDraw(DibBitmap& format)
{
  auto require = [](bool condition, const char* check)
  {
    if (!condition)
      throw std::runtime_error(std::string("Native UI smoke: HD palette regression: ") + check);
  };
  DibBitmap image(2, 1, &format);
  require(image.IsLoaded() && image.m_colorCount > 2, "indexed bitmap created");
  auto* colors = reinterpret_cast<RGBQUAD*>(reinterpret_cast<uint8_t*>(image.m_info) + image.HeaderSize());
  colors[1] = {255, 255, 255, 0};
  colors[2] = {0, 0, 255, 0};
  image.m_bits[0] = image.m_bits[1] = 1;
  std::vector<uint8_t> detail(2 * 4 * 4, 1);
  detail[0] = 2;
  require(image.SetHighResolutionPixels(4, detail), "detail assigned");

  const int previousScale = minwin_gdi::DeviceScale();
  minwin_gdi::DeviceScreen(4);
  HDC dc = GetDC(nullptr);
  require(dc != nullptr, "screen DC created");
  const UINT previousPaletteUse = SetSystemPaletteUse(dc, SYSPAL_STATIC);
  require(image.DrawToDc(dc, '@', SRCCOPY, 10, 10, 0, 0, 2, 1), "indexed detail drawn");
  uint32_t palette[256];
  minwin_gdi::CurrentPalette(palette, nullptr);
  const minwin_gdi::ScreenSurface logical = minwin_gdi::Screen();
  const minwin_gdi::ScreenSurface device = minwin_gdi::DeviceScreen(4);
  require(palette[logical.bits[10 * logical.pitch + 10]] == 0xffffff, "logical pixel uses source palette");
  require(palette[device.bits[40 * device.pitch + 40]] == 0xff0000, "detail pixel uses source palette");
  require(palette[device.bits[40 * device.pitch + 41]] == 0xffffff, "adjacent detail pixel remains white");
  SetSystemPaletteUse(dc, previousPaletteUse);
  ReleaseDC(nullptr, dc);
  minwin_gdi::DeviceScreen(previousScale);
}

void CheckEmptyBattleList(RebellionApplication& app, DibBitmap& background)
{
  struct BannerCase
  {
    int viewSide;
    int winningSide;
    int retreatSide;
    uint32_t bitmapId;
  };
  constexpr BannerCase bannerCases[] = {
    {1, 0, 0, 0x2d63}, {2, 0, 0, 0x2d63}, {1, 0, 1, 0x2a05}, {2, 0, 2, 0x2a06}, {1, 1, 2, 0x2a06}, {1, 1, 1, 0x2a07},
    {2, 2, 1, 0x2a05}, {2, 2, 2, 0x2a08}, {1, 2, 2, 0x2a08}, {2, 1, 1, 0x2a07}, {1, 2, 0, 0x2a08}, {2, 1, 0, 0x2a07},
  };
  for (const BannerCase& test : bannerCases)
    RequireBattle(BattleSummaryBitmapId(test.viewSide, test.winningSide, test.retreatSide) == test.bitmapId);

  Notice1 notice;
  auto window = std::make_unique<BattleWindow>(app.m_instance, nullptr, 0, &notice);
  window->m_system = nullptr;
  window->m_items = new CoolListItemList(4);
  window->m_list =
    new CoolDragList(app.m_instance, 0, 0, 100, 40, window.get(), &background, 200, 10, 10, 0, 0, 0, 0, 10, 1, nullptr);
  window->m_list->SetItemList(window->m_items);
  window->m_pageTabs = new CoolTabControl(app.m_instance, 0, 0, 10, 10, 0x6e, window.get(), &background);
  CoolStrobeButton alliance(app.m_instance, 0, 0, 1, 1, window->m_pageTabs, 0x70, 0, 0, &background, 0, 0);
  CoolStrobeButton empire(app.m_instance, 0, 0, 1, 1, window->m_pageTabs, 0x71, 0, 0, &background, 0, 0);

  for (CoolStrobeButton* page : {&alliance, &empire})
  {
    window->m_pageTabs->m_selected = page;
    for (int kind : {0x65, 0x66, 0x67, 0x68})
    {
      window->m_list->m_fillColor = 0x7f;
      window->FillList(kind, 1);
      const size_t expectedRows = kind == 0x68 ? 3 : 2;
      RequireBattle(window->m_items->Count() == expectedRows);
      for (const auto& row : window->m_items->Entries())
        RequireBattle(row->m_image == nullptr);
      RequireBattle(window->m_list->m_fillColor == 0x7f);
    }
  }
}

void CheckStatusLayout(DibBitmap& format)
{
  CoolLabel empty("existing text", 0, 0, 0, 0, DT_WORDBREAK, 0);
  empty.MoveTo(13, 40);
  empty.SetText("");
  RequireStatus(empty.m_rect.left == 13 && empty.m_rect.right >= empty.m_rect.left && empty.m_rect.top == 40 &&
                  empty.m_rect.bottom >= empty.m_rect.top,
                "empty string clear preserves nonzero origin");
  empty.SetText(nullptr);
  RequireStatus(empty.m_rect.left == 13 && empty.m_rect.right >= empty.m_rect.left && empty.m_rect.top == 40 &&
                  empty.m_rect.bottom >= empty.m_rect.top,
                "nullptr clear preserves nonzero origin");
  empty.SetText("font reset");
  empty.MoveTo(13, 40);
  empty.SetFont(10);
  RequireStatus(empty.m_rect.left == 13 && empty.m_rect.right >= empty.m_rect.left && empty.m_rect.top == 40 &&
                  empty.m_rect.bottom >= empty.m_rect.top,
                "font reset preserves nonzero origin");
  const ResourceStringId emptyResource{0x7fff, 0x7fff};
  empty.SetTextFromResource(&emptyResource);
  RequireStatus(empty.m_rect.left == 13 && empty.m_rect.right >= empty.m_rect.left && empty.m_rect.top == 40 &&
                  empty.m_rect.bottom >= empty.m_rect.top,
                "resource clear preserves nonzero origin");
  RequireStatus(!empty.GetText() || *empty.GetText() == '\0', "missing resource resolves to empty text");

  constexpr uint32_t kStatusWidth = 0xcf;
  constexpr int kStatusRowCount = 7;
  const std::pair<const char*, const char*> rows[kStatusRowCount] = {
    {"Hull", "120"},
    {"Shields", ""},
    {"", "This longer value should wrap across more than one line in the status panel."},
    {"", ""},
    {"Hyperdrive", "Online"},
    {"Crew", ""},
    {"Capacity", "This second deliberately long value should wrap too."},
  };
  CoolListItemList labels(kSortById);
  CoolListItemList values(kSortById);
  for (uint32_t id = 0; id < kStatusRowCount; ++id)
  {
    const std::string label(rows[id].first);
    const std::string value(rows[id].second);
    AddStatusRow(id, &label, &labels);
    AddStatusRow(id, &value, &values);
  }

  HDC dc = CreateCompatibleDC(nullptr);
  RequireStatus(dc != nullptr, "compatible DC created");
  const int height = LayoutStatusLabels(dc, &labels, &values);
  RequireStatus(height > 0, "total layout height is positive");
  int previousY = -1;
  int shortRowHeight = 0;
  int wrappedRowHeight = 0;
  for (uint32_t id = 0; id < kStatusRowCount; ++id)
  {
    auto* label = labels.FindById(id);
    auto* value = values.FindById(id);
    RequireStatus(label && value, "matching label and value rows exist");
    RequireStatus(label->m_y > previousY, "label row positions increase");
    RequireStatus(label->m_y == value->m_y || value->m_y > label->m_y, "value row is aligned within its label row");
    if (id == 0)
      shortRowHeight = static_cast<CoolListItem*>(labels.FindById(id + 1))->m_y - label->m_y;
    if (id == 2)
      wrappedRowHeight = static_cast<CoolListItem*>(labels.FindById(id + 1))->m_y - label->m_y;
    if (id == 3)
      RequireStatus(static_cast<CoolListItem*>(labels.FindById(id + 1))->m_y - label->m_y == 10,
                    "fully blank status row receives minimum line height");
    previousY = label->m_y;
  }
  RequireStatus(previousY < height, "total height includes all status rows");
  RequireStatus(wrappedRowHeight > shortRowHeight, "long value wraps to multiple lines");

  DibBitmap canvas(kStatusWidth, height, &format, dc);
  RequireStatus(canvas.IsLoaded(), "DIB section canvas created");
  canvas.FillRect(0, 0, kStatusWidth, height, 0);
  HGDIOBJ previousBitmap = SelectObject(dc, canvas.DibSectionHandle());
  DrawStatusLabels(dc, &labels, &values);
  SelectObject(dc, previousBitmap);
  DeleteDC(dc);

  bool hasRenderedText = false;
  for (int y = 0; y < height && !hasRenderedText; ++y)
    for (int x = 0; x < static_cast<int>(kStatusWidth); ++x)
      if (Pixel(canvas, x, y) != 0)
      {
        hasRenderedText = true;
        break;
      }
  RequireStatus(hasRenderedText, "status labels render into the canvas");
}

void CheckHeadquartersVictory()
{
  auto* root = static_cast<GameRoot*>(GetGameRootObjectCached());
  auto* allianceHeadquarters = static_cast<GameObj*>(GetFirstMasterInFamilies_20_22());
  const ObjectKey coruscantKey = ObjectKey::Make(0x90, 0x109);
  auto* coruscant = static_cast<GameObj*>(GetMasterInFamilies_90_98(&coruscantKey));
  constexpr uint32_t storyKeys[] = {0x32000242, 0x30000240, 0x35000281, 0x34000280};
  Character* storyCharacters[std::size(storyKeys)]{};
  for (size_t i = 0; i < std::size(storyKeys); ++i)
  {
    const ObjectKey key = ObjectKey::Make(storyKeys[i] >> 24, storyKeys[i] & 0xffffff);
    storyCharacters[i] = static_cast<Character*>(GetMasterInFamilies_30_3c(&key));
  }
  RequireBattle(root && allianceHeadquarters && coruscant &&
                std::ranges::all_of(storyCharacters,
                                    [](const Character* character)
                                    {
                                      return character != nullptr;
                                    }));

  struct SavedVictoryInputs
  {
    GameRoot* root;
    GameObj* allianceHeadquarters;
    GameObj* coruscant;
    Character** storyCharacters;
    int victoryMode;
    int victoryFlags;
    uint32_t headquartersState;
    uint32_t coruscantState;
    uint32_t coruscantFlags;
    uint32_t storyFlags[4];
    NotifyContext victoryContext;

    ~SavedVictoryInputs()
    {
      root->m_settings.m_field08 = victoryMode;
      root->m_unk74 = victoryFlags;
      root->m_ctx194 = victoryContext;
      allianceHeadquarters->m_stateFlags = headquartersState;
      coruscant->m_stateFlags = coruscantState;
      coruscant->m_flags = coruscantFlags;
      for (size_t i = 0; i < 4; ++i)
        storyCharacters[i]->m_flagsAc = storyFlags[i];
    }
  } saved{root,
          allianceHeadquarters,
          coruscant,
          storyCharacters,
          root->m_settings.m_field08,
          root->m_unk74,
          allianceHeadquarters->m_stateFlags,
          coruscant->m_stateFlags,
          coruscant->m_flags,
          {storyCharacters[0]->m_flagsAc, storyCharacters[1]->m_flagsAc, storyCharacters[2]->m_flagsAc,
           storyCharacters[3]->m_flagsAc},
          root->m_ctx194};

  NotifyContext context;
  root->m_unk74 = 0;
  allianceHeadquarters->m_stateFlags &= ~0x8u;
  coruscant->m_stateFlags &= ~0x8u;
  coruscant->m_flags &= ~0xc0u;
  for (Character* character : storyCharacters)
    character->m_flagsAc &= ~1u;

  root->m_settings.m_field08 = 1;
  RequireBattle(UpdateVictoryState(&context));
  RequireBattle((static_cast<uint32_t>(root->m_unk74) & 0x100000u) == 0);

  allianceHeadquarters->m_stateFlags |= 0x8u;
  RequireBattle(UpdateVictoryState(&context));
  RequireBattle((static_cast<uint32_t>(root->m_unk74) & 0x100000u) != 0);

  allianceHeadquarters->m_stateFlags &= ~0x8u;
  coruscant->m_flags = (coruscant->m_flags & ~0xc0u) | 0x40u;
  root->m_unk74 = 0;
  RequireBattle(UpdateVictoryState(&context));
  RequireBattle((static_cast<uint32_t>(root->m_unk74) & 0x1000000u) != 0);
  RequireBattle((static_cast<uint32_t>(root->m_unk74) & 0x100000u) == 0);

  root->m_settings.m_field08 = 0;
  coruscant->m_flags &= ~0xc0u;
  allianceHeadquarters->m_stateFlags |= 0x8u;
  root->m_unk74 = 0;
  RequireBattle(UpdateVictoryState(&context));
  RequireBattle((static_cast<uint32_t>(root->m_unk74) & 0x100000u) == 0);

  storyCharacters[0]->m_flagsAc |= 1u;
  storyCharacters[1]->m_flagsAc |= 1u;
  RequireBattle(UpdateVictoryState(&context));
  RequireBattle((static_cast<uint32_t>(root->m_unk74) & 0x100000u) != 0);
}

void CheckReportParticipants(RebellionApplication& app, DibBitmap& background, UiTextNotice* notice)
{
  auto window = std::make_unique<ReportWindow>(app.m_instance, nullptr, 0, notice);
  window->m_items = new CoolListItemList(kSortById);
  window->m_list = new CoolDragList(app.m_instance, 0, 0, 200, 100, window.get(), &background, 6, 0x6e, 0x32, 0, 0, 0,
                                    0, 0x32, 1, nullptr);
  window->m_list->SetItemList(window->m_items);
  window->m_pageTabs = new CoolTabControl(app.m_instance, 0, 0, 10, 10, 100, window.get(), &background);
  CoolStrobeButton teamPage(app.m_instance, 0, 0, 1, 1, window->m_pageTabs, 0x66, 0, 0, &background, 0, 0);
  window->m_pageTabs->m_selected = &teamPage;
  window->FillList(0x9c, 1);
  RequireBattle(window->m_notice == notice);
  RequireBattle(window->m_notice->MessageText0() == notice->MessageText0());
  RequireBattle(window->m_notice->MessageText1() == notice->MessageText1());
  RequireBattle(window->m_items->Count() == 3);
}

void CheckReportNoticeKinds(RebellionApplication& app, DibBitmap& background)
{
  Notice7 assault;
  assault.m_owner = 1;
  assault.m_text0 = "System under attack";
  assault.m_text1 = "The report contains no participant sets.";
  assault.m_picture[1] = 0x1234;
  CheckReportParticipants(app, background, &assault);

  Notice15 fleet;
  fleet.m_owner = 2;
  fleet.m_text0 = "Fleet arrival";
  fleet.m_text1 = "The typed report includes participant sets.";
  fleet.m_f64 = 1;
  fleet.m_f68 = 1;
  fleet.m_f6c = 2;
  const ObjectKey outsideListFamily = ObjectKey::Make(0x80, 0x123);
  fleet.m_set78.Add(new ObjectKeyNode(outsideListFamily));
  fleet.m_set88.Add(new ObjectKeyNode(outsideListFamily));
  fleet.m_set90.Add(new ObjectKeyNode(outsideListFamily));
  RequireBattle(fleet.m_set78.Count() == 1 && fleet.m_set88.Count() == 1 && fleet.m_set90.Count() == 1);
  CheckReportParticipants(app, background, &fleet);
  fleet.m_set88.Clear();
}

void CheckReportNoticeGeneration()
{
  Notice15 missingSystem;
  missingSystem.m_owner = 2;
  missingSystem.m_f6c = 2;
  missingSystem.m_f64 = 1;
  missingSystem.m_key = ObjectKey::Make(0x90, 0xffffff);
  missingSystem.OnObjectAndValuePayload(2, 0, 0);
  RequireReport(missingSystem.m_unk40 == 0, "missing assault target suppresses the stale result");
  RequireReport(missingSystem.m_text0.empty() && missingSystem.m_text1.empty(),
                "missing assault target leaves report text empty");

  auto* luke = static_cast<Luke*>(GetObject_32000242(1));
  RequireReport(luke != nullptr, "loaded alliance Luke exists");
  struct RestoreLukeStoryFlags
  {
    Luke* luke;
    uint32_t flags;
    ~RestoreLukeStoryFlags()
    {
      luke->m_storyFlags = flags;
    }
  } restore{luke, luke->m_storyFlags};

  luke->m_storyFlags &= ~Luke::kDagobahCompleted;
  Notice41 preDagobah;
  preDagobah.m_owner = 1;
  preDagobah.OnObjectAndValuePayload(0x231, 0, 1);
  RequireReport(preDagobah.m_picture[1] == 0x42a, "pre-Dagobah message selects its portrait");
  RequireReport(!preDagobah.MessageText0().empty() && !preDagobah.MessageText1().empty(),
                "pre-Dagobah message renders text");

  luke->m_storyFlags |= Luke::kDagobahCompleted;
  Notice41 postDagobah;
  postDagobah.m_owner = 1;
  postDagobah.OnObjectAndValuePayload(0x231, 0, 1);
  RequireReport(postDagobah.m_picture[1] == 0x42b, "post-Dagobah message selects its portrait");
  RequireReport(!postDagobah.MessageText0().empty() && !postDagobah.MessageText1().empty(),
                "post-Dagobah message renders text");
}

class ReportGalaxyFixture
{
public:
  explicit ReportGalaxyFixture(RebellionApplication& app)
  : m_app(app),
    m_previousGalaxy(g_galaxyWindow),
    m_previousAccelerators(app.m_unknown98),
    m_previousSpriteSide(g_spriteDllSide),
    m_session(GetGameSession()),
    m_previousViewReady(m_session ? m_session->m_viewReady : 0),
    m_galaxy(std::make_unique<GalaxyWindow>(app.m_instance, 0, 0, 640, 480, &app, 1, 1))
  {
    WNDCLASSA hostClass{};
    hostClass.hInstance = app.m_instance;
    hostClass.lpfnWndProc = DefWindowProcA;
    hostClass.lpszClassName = "ReportSmokeHost";
    RegisterClassA(&hostClass);
    m_host =
      CreateWindowExA(0, "ReportSmokeHost", "", WS_POPUP, 0, 0, 640, 480, nullptr, nullptr, app.m_instance, nullptr);
    if (!m_host)
    {
      Restore();
      throw std::runtime_error("Native UI smoke: report host window creation failed");
    }
    m_galaxy->m_hwnd = m_host;
  }

  ~ReportGalaxyFixture()
  {
    Restore();
  }

  GalaxyWindow& Galaxy()
  {
    return *m_galaxy;
  }

private:
  void Restore()
  {
    g_galaxyWindow = m_previousGalaxy;
    m_app.m_unknown98 = m_previousAccelerators;
    m_galaxy->m_hwnd = nullptr;
    m_galaxy.reset();
    if (m_session)
      m_session->m_viewReady = m_previousViewReady;
    char* location = GetDllLocation();
    LoadResourceModule(location, "strategy.dll", 7);
    if (m_previousSpriteSide != 0)
    {
      LoadResourceModule(location, m_previousSpriteSide == 1 ? "alsprite.dll" : "emsprite.dll", 9);
      g_spriteDllSide = m_previousSpriteSide;
    }
    ::operator delete(location);
    if (m_host)
      DestroyWindow(m_host);
  }

  RebellionApplication& m_app;
  GalaxyWindow* m_previousGalaxy;
  uintptr_t m_previousAccelerators;
  int m_previousSpriteSide;
  GameSession* m_session;
  int m_previousViewReady;
  std::unique_ptr<GalaxyWindow> m_galaxy;
  HWND m_host = nullptr;
};

struct RenderedReportPage
{
  int width;
  int height;
  std::vector<uint8_t> pixels;
};

RenderedReportPage RenderReportPage(RebellionApplication& app, UiTextNotice* notice, int side)
{
  ReportGalaxyFixture parent(app);
  parent.Galaxy().m_side = side;
  if (side == 2)
    SetRect(&parent.Galaxy().m_panelArea, 0x78, 0x28, 600, 0x18b);

  ReportWindow window(app.m_instance, &parent.Galaxy(), 0x5a, notice);
  RequireBattle(window.Create(0) != 0);
  RequireBattle(window.m_paintBackground && window.m_paintBackground->IsLoaded());
  RequireBattle(window.FindOverlayBitmap(1) == window.m_paintBackground);
  RequireBattle(window.m_paintBackground->Width() > 0x19 && window.m_paintBackground->Height() > 0xe1);

  DibBitmap* rendered = window.m_paintBackground;
  RenderedReportPage out{static_cast<int>(rendered->Width()), static_cast<int>(rendered->Height()), {}};
  for (int y = 0; y < out.height; ++y)
    for (int x = 0; x < out.width; ++x)
      out.pixels.push_back(Pixel(*rendered, x, y));
  window.DestroyNativeWindow();
  return out;
}

void CheckReportMainPage(RebellionApplication& app)
{
  for (int side : {1, 2})
  {
    Notice15 report;
    report.m_owner = side;
    report.m_f6c = side;
    report.m_f64 = 1;
    report.m_picture[1] = side == 1 ? 0x2b98 : 0x2b99;
    report.m_text0 = side == 1 ? "Alliance assault result" : "Empire assault result";
    report.m_text1 = "The station survived the attack. The fleet withdrew.";
    RequireBattle(report.GetClass() == 7);

    const RenderedReportPage rendered = RenderReportPage(app, &report, side);
    report.m_text0.clear();
    report.m_text1.clear();
    const RenderedReportPage withoutText = RenderReportPage(app, &report, side);

    RequireBattle(rendered.width == withoutText.width && rendered.height == withoutText.height);
    const auto differsInRows = [&](int first, int end)
    {
      for (int y = first; y < end; ++y)
        for (int x = 0; x < rendered.width; ++x)
        {
          const size_t offset = static_cast<size_t>(y) * rendered.width + x;
          if (rendered.pixels[offset] != withoutText.pixels[offset])
            return true;
        }
      return false;
    };
    RequireReport(differsInRows(13, 90), "assault report title renders in both faction skins");
    RequireReport(differsInRows(225, rendered.height), "assault report body renders in both faction skins");
  }
}
} // namespace

static void CheckDroidDeltaDetail(GalaxyWindow& galaxy)
{
  auto require = [](bool condition, const char* check)
  {
    if (!condition)
      throw std::runtime_error(std::string("Native UI smoke: droid delta detail regression: ") + check);
  };
  require(galaxy.m_droidStage && galaxy.m_droidStage2, "gameplay droid stages created");
  HMODULE module = CoolAppGetResourceModule(9);
  require(module != nullptr && g_spriteDllSide == galaxy.m_side, "current faction sprite module loaded");
  DibBitmap base(module, 2001, kDibResource);
  require(base.IsLoaded() && base.BitCount() == 8 && base.Compression() == 0, "base frame loaded");
  DibBitmap decoded(base.Width(), base.Height(), &base);
  require(base.Blit(&decoded, 0, 0, 0, 0, 0, 0) != 0, "base frame copied");
  const int width = decoded.Width();
  const int height = decoded.Height();
  std::vector<uint8_t> initial;
  initial.reserve(static_cast<size_t>(width) * height);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      initial.push_back(Pixel(decoded, x, y));

  IndexedSpriteFilter filter;
  require(filter.EnsureDetail(decoded) && decoded.m_hdScale == 4, "base detail generated");
  DeltaFrame delta(module, 2002);
  require(delta.m_rows && delta.m_data, "first delta loaded");
  require(delta.ApplyTo(&decoded), "first delta applied");
  require(decoded.m_hdPixels.empty(), "delta invalidated old detail");
  require(filter.EnsureDetail(decoded) && decoded.m_hdScale == 4, "changed frame detail regenerated");

  bool changed = false;
  bool keyed = false;
  bool opaque = false;
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
    {
      const uint8_t logical = Pixel(decoded, x, y);
      changed |= logical != initial[static_cast<size_t>(y) * width + x];
      keyed |= logical == decoded.m_colorKey;
      opaque |= logical != decoded.m_colorKey;
      for (int dy = 0; dy < 4; ++dy)
        for (int dx = 0; dx < 4; ++dx)
        {
          const uint8_t detailed = decoded.m_hdPixels[static_cast<size_t>(y * 4 + dy) * width * 4 + x * 4 + dx];
          require((detailed == decoded.m_colorKey) == (logical == decoded.m_colorKey),
                  "detail keeps decoded key mask");
        }
    }
  require(changed && keyed && opaque, "real delta changes a visible keyed frame");
}

static void CheckGalaxyResize(RebellionApplication& app)
{
  auto require = [](bool condition, const char* check)
  {
    if (!condition)
      throw std::runtime_error(std::string("Native UI smoke: galaxy resize regression: ") + check);
  };
  GalaxyWindow* galaxy = app.m_strategicWindow;
  require(galaxy && galaxy->m_hwnd && galaxy->m_created, "created strategic window");
  CheckDroidDeltaDetail(*galaxy);
  DibBitmap* const backdrop = galaxy->GetBackground();
  require(backdrop && backdrop->IsLoaded(), "initial background");
  const int previousMode = galaxy->m_displayMode;
  const std::set<uint32_t> previousSelection = galaxy->m_selectedIds;
  const ResourceStringId previousTitle = galaxy->m_activeTitleStringId;
  const ResourceStringId previousPendingTitle = galaxy->m_titleStringId;
  const ResourceStringId customTitle{0x1616, 2};
  const ResourceStringId pendingTitle{0x1617, 2};
  std::unique_ptr<char, void (*)(char*)> dllLocation(GetDllLocation(), [](char* value) { ::operator delete(value); });
  require(LoadResourceModule(dllLocation.get(), "textstra.dll", 2) != nullptr, "GID string module loaded");
  InitStringLoader();
  require(!LoadStringResource(&customTitle).empty() && !LoadStringResource(&pendingTitle).empty(),
          "GID title resources available");
  galaxy->SetDisplayMode(0x80, nullptr);
  galaxy->m_titleStringId = customTitle;
  galaxy->SetDisplayMode(0x11, nullptr);
  require(galaxy->m_displayMode == 0x11 && galaxy->m_activeTitleStringId.m_id == customTitle.m_id &&
          galaxy->m_activeTitleStringId.m_group == customTitle.m_group,
          "custom GID title activated");
  galaxy->m_titleStringId = pendingTitle;
  SectorWindow* rightSector = nullptr;

  const auto checkSectorDrag = [&]()
  {
    ObjectListItem* sectorItem = nullptr;
    for (const auto& node : galaxy->m_sectorItems.Entries())
    {
      sectorItem = static_cast<ObjectListItem*>(node.get());
      break;
    }
    require(sectorItem != nullptr, "sector item available");
    ObjectKey sectorKey;
    sectorItem->GetKey(&sectorKey);
    auto* sector = dynamic_cast<SectorWindow*>(galaxy->OpenSectorWindow(&sectorKey));
    require(sector && sector->m_hwnd, "sector window created");
    require(!sector->HitTestMarkers(&sector->m_markers, 1, 30), "empty sector body point");
    SendMessageA(sector->m_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, (30 << 16) | 1);
    require(!sector->CoolGameWindow::m_dragging, "empty body does not start window drag");
    SendMessageA(sector->m_hwnd, WM_LBUTTONUP, 0, (30 << 16) | 1);
    const int oldX = sector->m_x;
    SendMessageA(sector->m_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, (5 << 16) | 10);
    require(sector->CoolGameWindow::m_dragging && GetCapture() == sector->m_hwnd, "title drag captured");
    SendMessageA(sector->m_hwnd, WM_MOUSEMOVE, MK_LBUTTON, (25 << 16) | 40);
    SendMessageA(sector->m_hwnd, WM_LBUTTONUP, 0, (25 << 16) | 40);
    require(!sector->CoolGameWindow::m_dragging && GetCapture() != sector->m_hwnd,
            "title drag released capture");
    require(sector->m_x > oldX, "title drag moves sector window");
    require(sector->m_x >= galaxy->m_panelArea.left && sector->m_y >= galaxy->m_panelArea.top &&
            sector->m_x + sector->m_clientWidth <= galaxy->m_panelArea.right &&
            sector->m_y <= std::max(galaxy->m_panelArea.top,
                                    galaxy->m_panelArea.bottom - sector->m_clientHeight),
            "title drag respects panel limits");
    for (const auto& node : sector->m_markers.Entries())
    {
      auto* marker = static_cast<ObjectListItem*>(node.get());
      const int x = marker->m_rect.left + 1;
      const int y = marker->m_rect.top + 1;
      if (sector->HitTestMarkers(&sector->m_markers, x, y) != marker)
        continue;
      SendMessageA(sector->m_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, (y << 16) | x);
      require(sector->m_hitItem == marker && !sector->CoolGameWindow::m_dragging,
              "marker press retains marker selection path");
      SendMessageA(sector->m_hwnd, WM_LBUTTONUP, 0, (y << 16) | x);
      break;
    }
    require(sector->m_hitItem != nullptr, "hittable sector marker available");

    auto dispatchInput = []()
    {
      MSG message;
      for (int count = 0; count < 1000 && PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE); ++count)
      {
        TranslateMessage(&message);
        DispatchMessageA(&message);
      }
    };
    POINT start = {10, 5};
    ClientToScreen(sector->m_hwnd, &start);
    static unsigned testTime = 1000000;
    testTime += 1000;
    minwin::InputMouseMotion(start.x, start.y);
    dispatchInput();
    const int queuedStartX = sector->m_x;
    minwin::InputMouseButton(1, true, testTime, start.x, start.y);
    dispatchInput();
    require(sector->CoolGameWindow::m_dragging, "queued title press starts drag");
    minwin::InputMouseMotion(start.x + 30, start.y);
    dispatchInput();
    minwin::InputMouseButton(1, false, testTime + 50, start.x + 30, start.y);
    dispatchInput();
    require(!sector->CoolGameWindow::m_dragging && sector->m_x > queuedStartX,
            "queued held-button motion moves sector window");

    SendMessageA(sector->m_hwnd, WM_COMMAND, 0x139, 0);
    require(sector->m_x == galaxy->SectorSlotX(false), "side button returns dragged sector to left slot");
    SendMessageA(sector->m_hwnd, WM_COMMAND, 0x139, 0);
    require(sector->m_x == galaxy->SectorSlotX(true), "side button moves sector to wide right slot");
    rightSector = sector;

    bool checkedSecondSector = false;
    for (const auto& node : galaxy->m_sectorItems.Entries())
    {
      auto* nextItem = static_cast<ObjectListItem*>(node.get());
      ObjectKey nextKey;
      nextItem->GetKey(&nextKey);
      if (nextKey.Id() == sectorKey.Id())
        continue;
      auto* nextSector = dynamic_cast<SectorWindow*>(galaxy->OpenSectorWindow(&nextKey));
      require(nextSector && nextSector->m_x == galaxy->SectorSlotX(false),
              "new sector uses free wide left slot");
      checkedSecondSector = true;
      break;
    }
    require(checkedSecondSector, "second sector available for slot check");
  };

  for (int width : {854, 1280, 640})
  {
    require(minwin_gdi::SetScreenSize(width, 480), "logical screen resized");
    app.WindowProc(WM_SIZE, 0, (static_cast<LPARAM>(480) << 16) | width);
    require(galaxy->m_clientWidth == width && galaxy->m_canvasLayoutWidth == width, "galaxy client resized");
    require(galaxy->GetBackground() == backdrop && backdrop->IsLoaded() && backdrop->Width() == width,
            "borrowed background resized in place");
    require(galaxy->m_offscreen && galaxy->m_offscreen->Width() == width, "offscreen rebuilt");
    require(galaxy->m_displayMode == 0x11 && galaxy->m_activeTitleStringId.m_id == customTitle.m_id &&
            galaxy->m_activeTitleStringId.m_group == customTitle.m_group,
            "active GID title survives resize");
    require(galaxy->m_titleStringId.m_id == pendingTitle.m_id &&
            galaxy->m_titleStringId.m_group == pendingTitle.m_group,
            "pending GID title survives resize");
    if (rightSector)
      require(rightSector->m_x == galaxy->SectorSlotX(true), "existing sector follows right slot on resize");
    if (width > 640)
    {
      bool paintedBeyondClassic = false;
      for (int y = 0; y < 480 && !paintedBeyondClassic; ++y)
        for (int x = 640; x < width; ++x)
          if (Pixel(*backdrop, x, y) != 0)
          {
            paintedBeyondClassic = true;
            break;
          }
      require(paintedBeyondClassic, "new logical canvas contains artwork beyond 640");
    }
    InvalidateRect(galaxy->m_hwnd, nullptr, FALSE);
    UpdateWindow(galaxy->m_hwnd);
    for (CoolStrobeButton* button : galaxy->m_consoleButtons)
    {
      require(button && button->m_backdrop == backdrop, "console button backdrop");
      InvalidateRect(button->m_hwnd, nullptr, FALSE);
      UpdateWindow(button->m_hwnd);
    }
    for (CoolFrameButton* button : galaxy->m_messageButtons)
    {
      require(button && button->m_backdrop == backdrop, "message button backdrop");
      InvalidateRect(button->m_hwnd, nullptr, FALSE);
      UpdateWindow(button->m_hwnd);
    }
    if (width == 854)
      checkSectorDrag();
  }
  galaxy->SetDisplayMode(0x12, nullptr);
  require(galaxy->m_activeTitleStringId.m_id == pendingTitle.m_id &&
          galaxy->m_activeTitleStringId.m_group == pendingTitle.m_group && galaxy->m_titleStringId.m_id == 0,
          "next GID mode consumes pending title");
  galaxy->SetDisplayMode(0x80, nullptr);
  galaxy->m_titleStringId = previousTitle;
  if (previousMode != 0x80)
    galaxy->SetDisplayMode(previousMode, &previousSelection);
  galaxy->m_activeTitleStringId = previousTitle;
  galaxy->m_titleStringId = previousPendingTitle;
  galaxy->m_selectedIds = previousSelection;
}

namespace
{

void RequireAiMenu(bool condition, const char* check)
{
  if (!condition)
    throw std::runtime_error(std::string("Native AI menu smoke: ") + check);
}

MenuItem* FindAiMenuItem(const std::string& label)
{
  RequireAiMenu(g_menuWindow != nullptr, "AI menu is open");
  std::string choices;
  for (const auto& ownedItem : g_menuWindow->m_items.Entries())
  {
    auto* item = static_cast<MenuItem*>(ownedItem.get());
    RequireAiMenu(item->m_label != nullptr, "AI menu choice has a label");
    const char* text = item->m_label->GetText();
    if (text && std::string(text).ends_with(label))
      return item;
    if (text)
      choices += std::string(text) + "; ";
  }
  throw std::runtime_error("Native AI menu smoke: missing " + label + " among " + choices);
}

void ClickAiMenuItem(MenuItem* item)
{
  RequireAiMenu(item && item->m_area.bottom > item->m_area.top, "clickable menu choice");
  g_menuWindow->HandleMouse(item, item->m_area.left + 1, item->m_area.top + 1, WM_LBUTTONUP);
  DismissMenu();
}

void CheckLiveAiSelection(RebellionApplication& app)
{
  GameSession* session = GetGameSession();
  RequireAiMenu(session && session->m_human, "loaded human session");
  const int aiSide = 3 - session->GetHumanSide();
  const int originalKind = session->m_controllers[aiSide - 1]->GetKind();
  RequireAiMenu(AiControllerName(originalKind) != nullptr, "AI-controlled opponent");
  const char* faction = aiSide == 1 ? "Alliance" : "Empire";
  constexpr uint32_t kQueuedTestId = 0x7fffffef;
  auto* order = new NetMsg280;
  order->m_id = kQueuedTestId;
  order->m_side = aiSide;
  order->m_value = 42;
  session->m_controllers[aiSide - 1]->m_pending.Adopt(order);
  const size_t pendingCount = session->m_controllers[aiSide - 1]->m_pending.Count();
  const unsigned syncId = session->m_controllers[aiSide - 1]->m_syncId;
  const int unmatchedKind = PreferredAiKind(aiSide) == kPlayerAiCodex ? kPlayerAi : kPlayerAiCodex;
  RequireAiMenu(session->SetAiControllerKind(aiSide, unmatchedKind), "prepare controller/preference mismatch");
  app.m_strategicWindow->WindowProc(WM_KEYDOWN, 'D', 0);
  const auto unmatchedDefinitions = AiControllerDefinitions();
  const auto unmatchedDefinition = std::ranges::find_if(unmatchedDefinitions, [unmatchedKind](const auto& entry)
  {
    return entry.kind == unmatchedKind;
  });
  RequireAiMenu(unmatchedDefinition != unmatchedDefinitions.end(), "mismatched AI definition available");
  MenuItem* currentItem = FindAiMenuItem(std::string(faction) + " AI: " + unmatchedDefinition->displayName);
  RequireAiMenu(currentItem->m_label != nullptr, "current AI menu choice has a label");
  RequireAiMenu(currentItem->m_label->GetText()[0] == '*', "running controller marked despite saved preference");
  DismissMenu();
  for (const int kind : {kPlayerAiCodex, kPlayerAi})
  {
    app.m_strategicWindow->WindowProc(WM_KEYDOWN, 'D', 0);
    RequireAiMenu(g_menuWindow != nullptr, "strategic key D opens AI menu");
    const auto definitions = AiControllerDefinitions();
    const auto definition = std::ranges::find_if(definitions, [kind](const auto& entry) { return entry.kind == kind; });
    RequireAiMenu(definition != definitions.end(), "AI definition available");
    const std::string label = std::string(faction) + " AI: " + definition->displayName;
    ClickAiMenuItem(FindAiMenuItem(label));
    RequireAiMenu(session->m_controllers[aiSide - 1]->GetKind() == kind, "live AI personality changes");
    RequireAiMenu(session->m_controllers[aiSide - 1]->m_syncId == syncId &&
                    session->m_controllers[aiSide - 1]->m_pending.Count() == pendingCount,
                  "queued controller state survives replacement");
  }
  if (originalKind != kPlayerAi)
    RequireAiMenu(session->SetAiControllerKind(aiSide, originalKind), "restore original AI");
  std::unique_ptr<MessageBase> savedOrder(session->m_controllers[aiSide - 1]->m_pending.Detach(kQueuedTestId));
  RequireAiMenu(savedOrder && savedOrder->GetKind() == 0x280 &&
                  static_cast<NetMsg280*>(savedOrder.get())->m_value == 42,
                "queued order payload survives replacement");
}

void CheckCockpitAiMenu(RebellionApplication& app)
{
  namespace fs = std::filesystem;
  const std::string originalConfig = mw::config::GlobalPath();
  const fs::path temporaryConfig = fs::temp_directory_path() /
    ("rebellion_ai_menu_smoke_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
     ".ini");
  RequireAiMenu(mw::config::Global().Save(temporaryConfig.string()), "temporary config created");
  struct RestoreConfig
  {
    std::string original;
    fs::path temporary;
    ~RestoreConfig()
    {
      mw::config::SetGlobalPath(original);
      std::error_code ignored;
      fs::remove(temporary, ignored);
    }
  } restore{originalConfig, temporaryConfig};
  mw::config::SetGlobalPath(temporaryConfig.string());
  RequireAiMenu(SetPreferredAiKind(1, kPlayerAi) && SetPreferredAiKind(2, kPlayerAi), "Classic defaults saved");
  const int originalGameType = app.m_gameType;
  app.m_gameType = 0;
  const bool commonWasLoaded = CoolAppGetResourceModule(8) != nullptr;
  {
    MainMenuWindow cockpit(app.m_instance, 0, 0, 640, 480, &app);
    RequireAiMenu(cockpit.Create(0) != 0, "cockpit created");
    cockpit.WindowProc(WM_KEYDOWN, 'D', 0);
    RequireAiMenu(g_menuWindow && g_menuWindow->m_frame && g_menuWindow->m_frame->IsLoaded(),
                  "cockpit key D renders framed menu");
    RequireAiMenu(g_menuWindow->m_x >= 0 && g_menuWindow->m_x + g_menuWindow->m_width <= cockpit.m_clientWidth,
                  "cockpit menu fits window width");
    for (const auto& ownedItem : g_menuWindow->m_items.Entries())
    {
      auto* item = static_cast<MenuItem*>(ownedItem.get());
      RequireAiMenu(item->m_label->m_rect.right <= g_menuWindow->m_clientWidth,
                    "cockpit choice text fits menu");
    }
    bool paintedFrame = false;
    DibBitmap* frame = g_menuWindow->m_frame;
    for (int y = 0; y < frame->Height() && !paintedFrame; ++y)
      for (int x = 0; x < frame->Width(); ++x)
        if (Pixel(*frame, x, y) != frame->m_colorKey)
        {
          paintedFrame = true;
          break;
        }
    RequireAiMenu(paintedFrame, "menu frame has visible pixels");
    ClickAiMenuItem(FindAiMenuItem("Alliance AI: Codex Strategist"));
    RequireAiMenu(PreferredAiKind(1) == kPlayerAiCodex && PreferredAiKind(2) == kPlayerAi,
                  "Alliance selection persists independently");
    cockpit.WindowProc(WM_KEYDOWN, 'D', 0);
    ClickAiMenuItem(FindAiMenuItem("Empire AI: Codex Strategist"));
    RequireAiMenu(PreferredAiKind(1) == kPlayerAiCodex && PreferredAiKind(2) == kPlayerAiCodex,
                  "Empire selection persists independently");
    GameSession newGame(1, 0);
    RequireAiMenu(newGame.m_controllers[0] && newGame.m_controllers[1] &&
                    newGame.m_controllers[0]->GetKind() == kPlayerAiCodex &&
                    newGame.m_controllers[1]->GetKind() == kPlayerAiCodex,
                  "new game constructs preferred controllers for both sides");
    DestroyWindow(cockpit.m_hwnd);
  }
  if (!commonWasLoaded)
    CoolUnloadResourceSlot(8);
  app.m_gameType = originalGameType;
}

} // namespace

void RunNativeSwitchSidesSmoke(RebellionApplication& app)
{
  auto require = [](bool condition, const char* check)
  {
    if (!condition)
      throw std::runtime_error(std::string("Native switch sides smoke: ") + check);
  };
  GameStepDriver* driver = GetGameStepDriver();
  GameSession* session = GetGameSession();
  require(driver && session && session->m_human, "loaded session");
  const int originalState = app.m_state;
  const int originalPhase = driver->m_phase;
  const int originalPause = driver->m_pauseTick;
  const int originalTick = GetGameTickCount();
  const int originalSpeed = session->m_human->m_speed;
  const int originalViewReady = session->m_viewReady;
  app.m_state = kStateStrategic;
  app.ShowStrategicWindow(0);
  app.m_strategicWindow->InitializeGameDisplay();
  CheckGalaxyResize(app);
  for (int attempt = 0; attempt != 3; ++attempt)
  {
    const int previousSide = session->GetHumanSide();
    constexpr uint32_t kQueuedTestId = 0x7ffffff0;
    for (int index = 0; index != 2; ++index)
    {
      require(!session->m_controllers[index]->m_pending.FindById(kQueuedTestId), "test order ID available");
      auto* order = new NetMsg280;
      order->m_id = kQueuedTestId;
      order->m_side = index + 1;
      order->m_value = 60;
      session->m_controllers[index]->m_pending.Adopt(order);
    }
    const unsigned syncIds[] = {session->m_controllers[0]->m_syncId, session->m_controllers[1]->m_syncId};
    const size_t pendingCounts[] = {session->m_controllers[0]->m_pending.Count(),
                                    session->m_controllers[1]->m_pending.Count()};
    app.m_strategicWindow->WindowProc(WM_KEYDOWN, 'D', 0);
    require(g_menuWindow != nullptr, "3 opens menu");
    auto* item = static_cast<MenuItem*>(g_menuWindow->m_items.FindById(kGalaxyDevSwitchSides));
    require(item && std::string(item->m_label->GetText()) == "Switch sides", "first menu option");
    require(item->m_area.bottom > item->m_area.top, "menu item has height");
    g_menuWindow->HandleMouse(item, item->m_area.left + 1, item->m_area.top + 1, WM_LBUTTONUP);
    driver->m_phase = 8;
    app.ProcessDeveloperActions();
    require(session->GetHumanSide() == previousSide, "deferred during AI phase");
    driver->m_phase = attempt % 2 == 0 ? 0 : 2;
    app.ProcessDeveloperActions();
    require(session->GetHumanSide() == 3 - previousSide, "control changes sides");
    require(app.m_side == session->GetHumanSide() && app.m_strategicWindow->m_side == app.m_side,
            "application and rebuilt view use new faction");
    require(session->m_human == session->m_controllers[app.m_side - 1] &&
            AiControllerName(session->m_controllers[2 - app.m_side]->GetKind()) != nullptr,
            "fixed controller slots");
    require(session->m_human->m_speed == originalSpeed && driver->m_pauseTick == originalPause &&
            GetGameTickCount() == originalTick, "speed, pause and tick preserved");
    require(app.m_strategicWindow->m_speed == originalSpeed, "speed immediately displayed");
    CheckGalaxyResize(app);
    for (int index = 0; index != 2; ++index)
    {
      require(session->m_controllers[index]->m_syncId == syncIds[index] &&
              session->m_controllers[index]->m_pending.Count() == pendingCounts[index], "sync and orders preserved");
      std::unique_ptr<MessageBase> order(session->m_controllers[index]->m_pending.Detach(kQueuedTestId));
      require(order && order->GetKind() == 0x280, "queued order survives replacement");
      auto* speedOrder = static_cast<NetMsg280*>(order.get());
      require(speedOrder->m_side == index + 1 && speedOrder->m_value == 60, "queued order retains side and payload");
    }
  }
  driver->m_phase = originalPhase;
  app.DestroyStrategicWindow();
  session->m_viewReady = originalViewReady;
  app.m_state = originalState;
}

void RunNativeUiSmoke(RebellionApplication& app)
{
  GameSession* session = GetGameSession();
  const int originalState = app.m_state;
  const int originalViewReady = session ? session->m_viewReady : 0;
  app.m_state = kStateStrategic;
  app.ShowStrategicWindow(0);
  Require(app.m_strategicWindow != nullptr);
  app.m_strategicWindow->InitializeGameDisplay();
  CheckGalaxyResize(app);
  CheckLiveAiSelection(app);
  app.DestroyStrategicWindow();
  if (session)
    session->m_viewReady = originalViewReady;
  app.m_state = originalState;
  auto releaseLocation = [](char* value)
  {
    ::operator delete(value);
  };
  std::unique_ptr<char, decltype(releaseLocation)> location(GetDllLocation(), releaseLocation);
  HMODULE textModule = LoadResourceModule(location.get(), "textstra.dll", 2);
  MessageTemplate templates(0xbb9, 0xbb8, textModule);
  struct RestoreTemplates
  {
    MessageTemplate* previous;
    ~RestoreTemplates()
    {
      g_messageTemplate = previous;
    }
  } restoreTemplates{g_messageTemplate};
  g_messageTemplate = &templates;
  HMODULE art = LoadResourceModule(location.get(), "strategy.dll", 7);
  DibBitmap background(art, 0x2b5du, kDibResource);
  Require(background.IsLoaded());
  CheckHighResolutionPaletteDraw(background);
  CoolGameWindow parent(app.m_instance, 0, 0, 100, 40, &app, 0, 7, 0, 1, 0, 1, 0, 0);
  CoolDragList list(art, 0, 0, 100, 40, &parent, &background, 2, 10, 10, 0, 0, 0, 0, 10, 1, nullptr);
  auto& items = *list.m_items;
  FleetListItem* fleet = AddRow(items, background, 1, kFleetItemFleet, 1);
  FleetListItem* ship = AddRow(items, background, 2, kFleetItemHidden, 2);
  FleetListItem* nextFleet = AddRow(items, background, 3, kFleetItemFleet, 3);

  Require((fleet->m_flags.m_value & kFleetItemExpanded) == 0);
  list.Refresh();
  Require(items.Count() == 3 && std::ranges::distance(items.VisibleEntries()) == 2);
  Require(nextFleet->m_y == 10 && list.HitTest(POINTS{5, 15}) == nextFleet);
  Require(Pixel(*list.m_canvas, 5, 15) == 3);

  fleet->m_flags.m_value |= kFleetItemExpanded;
  ship->m_flags.m_value &= ~kFleetItemHidden;
  list.Refresh();
  Require(std::ranges::distance(items.VisibleEntries()) == 3);
  Require(ship->m_y == 10 && nextFleet->m_y == 20 && list.HitTest(POINTS{5, 15}) == ship);
  Require(Pixel(*list.m_canvas, 5, 15) == 2);

  fleet->m_flags.m_value &= ~kFleetItemExpanded;
  ship->m_flags.m_value |= kFleetItemHidden;
  list.Refresh();
  Require(nextFleet->m_y == 10 && list.HitTest(POINTS{5, 15}) == nextFleet);
  Require(Pixel(*list.m_canvas, 5, 15) == 3);
  ship->m_state.m_value |= kListItemSelected;
  CoolListItemList selected(kSortById);
  list.CopySelectionTo(&selected);
  Require(selected.Empty());
  list.ClearSelection();
  Require((ship->m_state.m_value & kListItemSelected) == 0);
  list.SelectById(ship->m_id, 0);
  Require((ship->m_state.m_value & kListItemSelected) == 0);
  list.SelectById(fleet->m_id, 0);
  list.WindowProc(WM_KEYDOWN, VK_DOWN, 0);
  Require((nextFleet->m_state.m_value & kListItemSelected) != 0);
  Require((ship->m_state.m_value & kListItemSelected) == 0);
  list.WindowProc(WM_KEYDOWN, VK_UP, 0);
  Require((fleet->m_state.m_value & kListItemSelected) != 0);
  list.ClearSelection();

  list.SetSortMode(kSortById);
  Require(list.m_items->Count() == 3 && std::ranges::distance(list.m_items->VisibleEntries()) == 2);
  Require(list.HitTest(POINTS{5, 15}) == nextFleet);

  CheckEmptyBattleList(app, background);
  CheckStatusLayout(background);
  CheckHeadquartersVictory();
  CheckReportNoticeKinds(app, background);
  CheckReportNoticeGeneration();
  CheckReportMainPage(app);
  CheckLiveWindowMovePaint(app.m_instance);
  CheckCockpitAiMenu(app);
}
