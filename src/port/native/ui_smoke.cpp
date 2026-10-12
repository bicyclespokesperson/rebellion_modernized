#include "port/native/ui_smoke.h"

#include <cstdint>
#include <iterator>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "app/game_session.h"
#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "character/character.h"
#include "economy/defense_facility.h"
#include "events/player_notices.h"
#include "events/game_step_driver.h"
#include "events/net_messages.h"
#include "gameobj/game_root.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_queries.h"
#include "gfx/dib_bitmap.h"
#include "rules/event_rules.h"
#include "story/story_characters.h"
#include "strategy_ui/battle_window.h"
#include "strategy_ui/fleet_list_item.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/object_menu.h"
#include "strategy_ui/object_status_window.h"
#include "strategy_ui/report_window.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_game_window.h"
#include "ui/cool_label.h"
#include "ui/cool_tab_control.h"
#include "ui/message_template.h"

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
  missingSystem.Virtual08(2, 0, 0);
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
  preDagobah.Virtual08(0x231, 0, 1);
  RequireReport(preDagobah.m_picture[1] == 0x42a, "pre-Dagobah message selects its portrait");
  RequireReport(!preDagobah.MessageText0().empty() && !preDagobah.MessageText1().empty(),
                "pre-Dagobah message renders text");

  luke->m_storyFlags |= Luke::kDagobahCompleted;
  Notice41 postDagobah;
  postDagobah.m_owner = 1;
  postDagobah.Virtual08(0x231, 0, 1);
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
    app.m_strategicWindow->WindowProc(WM_KEYDOWN, '3', 0);
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
            session->m_controllers[2 - app.m_side]->GetKind() == kPlayerAi, "fixed controller slots");
    require(session->m_human->m_speed == originalSpeed && driver->m_pauseTick == originalPause &&
            GetGameTickCount() == originalTick, "speed, pause and tick preserved");
    require(app.m_strategicWindow->m_speed == originalSpeed, "speed immediately displayed");
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
}
