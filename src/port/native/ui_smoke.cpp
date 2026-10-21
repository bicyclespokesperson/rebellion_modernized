#include "port/native/ui_smoke.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "ai/ai_common.h"
#include "ai/ai_controller_factory.h"
#include "ai/codex_ai_assault.h"
#include "app/ai_preferences.h"
#include "app/enhancements.h"
#include "app/game_session.h"
#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "character/character.h"
#include "core/game_string.h"
#include "data/dat_record.h"
#include "economy/defense_facility.h"
#include "economy/manu_mgr.h"
#include "economy/manufacturing_facility.h"
#include "economy/procfacil.h"
#include "economy/side.h"
#include "events/command_classes2.h"
#include "events/game_step_driver.h"
#include "events/net_messages.h"
#include "events/player_notices.h"
#include "fleet/capship_subclasses.h"
#include "fleet/fleet.h"
#include "fleet/fleet_iterator.h"
#include "galaxy/system.h"
#include "gameobj/child_iterator.h"
#include "gameobj/game_hooks.h"
#include "gameobj/game_root.h"
#include "gameobj/object_family.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_queries.h"
#include "gfx/dib_bitmap.h"
#include "gfx/indexed_sprite_filter.h"
#include "orders/mission_orders.h"
#include "orders/orders_210_216.h"
#include "port/minwin_impl/config/config.h"
#include "port/minwin_impl/gdi_api.h"
#include "port/minwin_impl/user/input.h"
#include "port/minwin_impl/user/present.h"
#include "rules/event_rules.h"
#include "story/story_characters.h"
#include "strategy_ui/battle_window.h"
#include "strategy_ui/developer_menu.h"
#include "strategy_ui/eta_display.h"
#include "strategy_ui/fleet_list_item.h"
#include "strategy_ui/fleet_window.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "strategy_ui/main_menu_window.h"
#include "strategy_ui/object_menu.h"
#include "strategy_ui/object_status_window.h"
#include "strategy_ui/personnel_finder_window.h"
#include "strategy_ui/report_window.h"
#include "strategy_ui/sector_window.h"
#include "strategy_ui/sector_marker_art.h"
#include "strategy_ui/system_missions_window.h"
#include "strategy_ui/system_forces_window.h"
#include "strategy_ui/system_window.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_environment.h"
#include "ui/cool_game_window.h"
#include "ui/cool_label.h"
#include "ui/cool_selection_box.h"
#include "ui/cool_string_field.h"
#include "ui/cool_tab_control.h"
#include "ui/custom_dialog_box.h"
#include "ui/message_template.h"
#include "ui/object_list_item.h"
#include "ui/sprite.h"
#include "ui/text_scroll_field.h"

extern int g_spriteDllSide;

namespace
{
void Require(bool condition)
{
  if (!condition) {
    throw std::runtime_error("Native UI smoke: fleet-row visibility regression");
  }
}

void RequireBattle(bool condition)
{
  if (!condition) {
    throw std::runtime_error("Native UI smoke: battle UI regression");
  }
}

void RequireReport(bool condition, const char* check)
{
  if (!condition) {
    throw std::runtime_error(std::string("Native UI smoke: report rendering regression: ") + check);
  }
}

void RequireStatus(bool condition, const char* check)
{
  if (!condition) {
    throw std::runtime_error(std::string("Native UI smoke: ship status label layout regression: ") + check);
  }
}

struct HdReviewTarget {
  std::string module, category, label;
  uint32_t resourceId;
};

std::vector<HdReviewTarget> ReadHdReviewTargets(const std::filesystem::path& directory)
{
  std::ifstream input(directory / "targets.tsv");
  if (!input) {
    throw std::runtime_error("Native HD review: cannot open targets.tsv");
  }
  std::vector<HdReviewTarget> targets;
  std::string line;
  size_t lineNumber = 0;
  while (std::getline(input, line)) {
    ++lineNumber;
    if (line.empty() || line[0] == '#') {
      continue;
    }
    std::string fields[4];
    size_t start = 0;
    bool valid = true;
    for (int field = 0; field < 4; ++field) {
      const size_t tab = line.find('\t', start);
      if ((field < 3 && tab == std::string::npos) || (field == 3 && tab != std::string::npos)) {
        valid = false;
        break;
      }
      fields[field] = line.substr(start, tab == std::string::npos ? tab : tab - start);
      start = tab == std::string::npos ? line.size() : tab + 1;
    }
    if (valid && fields[0] == "module_filename") {
      continue;
    }
    if (!valid || fields[0].empty() || fields[2].empty() || fields[3].empty()) {
      throw std::runtime_error("Native HD review: malformed targets.tsv line " + std::to_string(lineNumber));
    }
    size_t parsed = 0;
    const auto id = static_cast<uint32_t>(std::stoul(fields[1], &parsed, 0));
    if (parsed != fields[1].size()) {
      throw std::runtime_error("Native HD review: invalid resource id on line " + std::to_string(lineNumber));
    }
    targets.push_back({fields[0], fields[2], fields[3], id});
  }
  if (targets.empty()) {
    throw std::runtime_error("Native HD review: targets.tsv contains no targets");
  }
  return targets;
}

void SaveHdReviewFrame(const std::filesystem::path& path)
{
  uint32_t palette[256];
  minwin_gdi::CurrentPalette(palette, nullptr);
  const auto screen = minwin_gdi::DeviceScreen(4);
  std::vector<uint32_t> pixels(static_cast<size_t>(screen.width) * screen.height);
  for (int y = 0; y < screen.height; ++y) {
    for (int x = 0; x < screen.width; ++x) {
      pixels[static_cast<size_t>(y) * screen.width + x] = palette[screen.bits[y * screen.pitch + x]];
    }
  }
  minwin::present::Present(pixels.data(), screen.width, screen.height);
  if (!minwin::present::CaptureScreenshotPixels(pixels.data(), screen.width, screen.height, path.string().c_str())) {
    throw std::runtime_error("Native HD review: failed to write " + path.string());
  }
}

void RunNativeHdArtworkReview(const char* basePath, DibBitmap& format)
{
  const char* configured = std::getenv("REB_HD_REVIEW_DIR");
  if (!configured || !*configured) {
    return;
  }
  const std::filesystem::path output = std::filesystem::u8path(configured);
  std::filesystem::create_directories(output);
  const auto targets = ReadHdReviewTargets(output);
  auto& config = mw::config::Global();
  const std::string previousHdDirectory = config.Get("hd_asset_dir");
  const char* assetDirectory = std::getenv("REB_HD_ASSET_DIR");
  config.Set("hd_asset_dir", assetDirectory && *assetDirectory ? assetDirectory : output.string());
  struct RestoreHdDirectory {
    mw::config::Config& config;
    std::string value;
    ~RestoreHdDirectory()
    {
      config.Set("hd_asset_dir", value);
    }
  } restoreDirectory{config, previousHdDirectory};
  const int previousScale = minwin_gdi::DeviceScale();
  minwin_gdi::DeviceScreen(4);
  std::map<std::string, uint32_t> moduleSlots;
  std::map<std::string, std::vector<const HdReviewTarget*>> categories;
  for (const auto& target : targets) {
    categories[target.category].push_back(&target);
  }
  constexpr int columns = 2;
  constexpr int rows = 4;
  constexpr int pageSize = columns * rows;
  constexpr int cellWidth = 640 / columns;
  constexpr int cellHeight = 480 / rows;
  DibBitmap canvas(640, 480, &format);
  for (const auto& [category, categoryTargets] : categories) {
    for (size_t pageStart = 0, page = 1; pageStart < categoryTargets.size(); pageStart += pageSize, ++page) {
      canvas.FillRect(0, 0, 640, 480, 0);
      std::vector<std::pair<size_t, std::string>> pageLabels;
      for (size_t pageIndex = 0; pageIndex < pageSize && pageStart + pageIndex < categoryTargets.size(); ++pageIndex) {
        const HdReviewTarget& target = *categoryTargets[pageStart + pageIndex];
        auto slot = moduleSlots.find(target.module);
        if (slot == moduleSlots.end()) {
          slot = moduleSlots.emplace(target.module, static_cast<uint32_t>(0x7000 + moduleSlots.size())).first;
        }
        HMODULE module = LoadResourceModule(basePath, target.module.c_str(), slot->second);
        DibBitmap image(module, target.resourceId, kDibResource);
        if (target.module == "STRATEGY.DLL") {
          AddSectorMarkerDetail(image, static_cast<uint16_t>(target.resourceId));
        }
        if (!image.IsLoaded() || image.BitCount() != 8 || image.m_hdPixels.empty() || image.m_hdScale != 4) {
          throw std::runtime_error("Native HD review: 4x companion missing for " + target.module + ":" + target.label);
        }
        std::vector<uint8_t> logical(static_cast<size_t>(image.RowStride()) * image.Height());
        std::copy_n(image.m_bits, logical.size(), logical.begin());
        const auto detail = image.m_hdPixels;
        const bool detailKeyed = image.m_hdKeyed;
        bool hasDetailChanges = false;
        const int detailScale = image.m_hdScale;
        const int height = image.Height() < 0 ? -image.Height() : image.Height();
        for (int y = 0; y < height && !hasDetailChanges; ++y) {
          for (int x = 0; x < image.Width() && !hasDetailChanges; ++x) {
            const uint8_t basePixel = image.m_bits[image.RowIndex(y) * image.RowStride() + x];
            for (int dy = 0; dy < detailScale && !hasDetailChanges; ++dy) {
              for (int dx = 0; dx < detailScale; ++dx) {
                const size_t detailIndex
                    = static_cast<size_t>(y * detailScale + dy) * image.Width() * detailScale + x * detailScale + dx;
                if (detail[detailIndex] != basePixel) {
                  hasDetailChanges = true;
                  break;
                }
              }
            }
          }
        }
        if (!hasDetailChanges) {
          throw std::runtime_error("Native HD review: 4x detail matches every logical pixel for " + target.label);
        }
        const int column = static_cast<int>(pageIndex % columns);
        const int row = static_cast<int>(pageIndex / columns);
        const int cellX = column * cellWidth;
        const int cellY = row * cellHeight;
        const int imageTop = cellY + 27;
        constexpr int slotWidth = 150;
        const int originalX = cellX + 5 + (slotWidth - image.Width()) / 2;
        const int hdX = cellX + 165 + (slotWidth - image.Width()) / 2;
        const int imageY = imageTop + (85 - height) / 2;
        image.m_hdPixels.clear();
        image.m_hdScale = 1;
        image.m_hdKeyed = false;
        image.DrawKeyed(&canvas, originalX, imageY);
        image.m_hdPixels = detail;
        image.m_hdScale = detailScale;
        image.m_hdKeyed = detailKeyed;
        DibBitmap rowImage(image.Width(), height, &format);
        rowImage.FillRect(0, 0, rowImage.Width(), height, image.m_colorKey);
        image.DrawKeyed(&rowImage, 0, 0);
        DibBitmap selectedImage(image.Width(), height, &format);
        rowImage.Blit(&selectedImage, 0, 0, 0, 0, 0, 0);
        if (detailKeyed && (!rowImage.m_hdKeyed || !selectedImage.m_hdKeyed)) {
          throw std::runtime_error("Native HD review: row composition lost detail transparency for " + target.label);
        }
        selectedImage.DrawKeyed(&canvas, hdX, imageY);
        if (!std::equal(logical.begin(), logical.end(), image.m_bits)) {
          throw std::runtime_error("Native HD review: composition changed logical source pixels for " + target.label);
        }

        std::string label = std::to_string(target.resourceId) + " " + target.module + " " + target.label;
        if (label.size() > 38) {
          label = label.substr(0, 35) + "...";
        }
        pageLabels.emplace_back(pageIndex, std::move(label));
      }
      HDC dc = reb::compat::GetDC(nullptr);
      if (!dc) {
        throw std::runtime_error("Native HD review: screen DC unavailable");
      }
      canvas.DrawToDc(dc, '@', SRCCOPY, 0, 0, 0, 0, 0, 0);
      reb::compat::SetBkMode(dc, TRANSPARENT);
      reb::compat::SetTextColor(dc, RGB(255, 255, 255));
      for (const auto& [pageIndex, label] : pageLabels) {
        const int cellX = static_cast<int>(pageIndex % columns) * cellWidth;
        const int cellY = static_cast<int>(pageIndex / columns) * cellHeight;
        const int imageTop = cellY + 27;
        reb::compat::TextOutA(dc, cellX + 4, cellY + 3, label.c_str(), static_cast<int>(label.size()));
        reb::compat::TextOutA(dc, cellX + 45, imageTop + 88, "ORIGINAL", 8);
        reb::compat::TextOutA(dc, cellX + 210, imageTop + 88, "HD 4x", 5);
      }
      reb::compat::ReleaseDC(nullptr, dc);
      std::string safeCategory = category;
      for (char& ch : safeCategory) {
        if (!std::isalnum(static_cast<unsigned char>(ch))) {
          ch = '_';
        }
      }
      char pageName[32];
      std::snprintf(pageName, sizeof(pageName), "%03zu", page);
      SaveHdReviewFrame(output / (safeCategory + "_page_" + pageName + ".bmp"));
    }
  }
  minwin_gdi::DeviceScreen(previousScale);
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
  if (message == WM_ERASEBKGND) {
    return 1;
  }
  if (message == WM_PAINT) {
    PAINTSTRUCT paint;
    HDC dc = reb::compat::BeginPaint(hwnd, &paint);
    const HWND parent = reb::compat::GetParent(hwnd);
    const COLORREF color = !parent ? RGB(255, 255, 255) : reb::compat::GetParent(parent) ? RGB(0, 0, 255) : RGB(255, 0, 0);
    HBRUSH brush = reb::compat::CreateSolidBrush(color);
    RECT bounds;
    reb::compat::GetClientRect(hwnd, &bounds);
    reb::compat::FillRect(dc, &bounds, brush);
    reb::compat::DeleteObject(brush);
    reb::compat::EndPaint(hwnd, &paint);
    return 0;
  }
  return reb::compat::DefWindowProcA(hwnd, message, wParam, lParam);
}

void CheckLiveWindowMovePaint(HINSTANCE instance)
{
  auto require = [](bool condition, const char* check) {
    if (!condition) {
      throw std::runtime_error(std::string("Native UI smoke: live window move regression: ") + check);
    }
  };
  WNDCLASSA cls{};
  cls.hInstance = instance;
  cls.lpfnWndProc = WindowMoveSmokeProc;
  cls.lpszClassName = "NativeWindowMoveSmoke";
  require(reb::compat::RegisterClassA(&cls) != 0, "window class registered");
  constexpr int rootX = 320, rootY = 100;
  HWND root = reb::compat::CreateWindowExA(0, cls.lpszClassName, "", WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN, rootX, rootY, 120, 100,
                              nullptr, nullptr, instance, nullptr);
  require(root != nullptr, "root created");
  HWND child = reb::compat::CreateWindowExA(0, cls.lpszClassName, "", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 10, 10, 40, 40, root,
                               nullptr, instance, nullptr);
  require(child != nullptr, "child created");
  HWND grandchild = reb::compat::CreateWindowExA(0, cls.lpszClassName, "", WS_CHILD | WS_VISIBLE, 5, 5, 10, 10, child, nullptr,
                                    instance, nullptr);
  require(grandchild != nullptr, "grandchild created");
  const int previousScale = minwin_gdi::DeviceScale();
  minwin_gdi::DeviceScreen(4);
  reb::compat::UpdateWindow(root);
  reb::compat::UpdateWindow(child);
  reb::compat::UpdateWindow(grandchild);
  uint32_t palette[256];
  minwin_gdi::CurrentPalette(palette, nullptr);
  auto colorAt = [&](int x, int y) {
    const minwin_gdi::ScreenSurface screen = minwin_gdi::Screen();
    return palette[screen.bits[y * screen.pitch + x]];
  };
  auto deviceColorAt = [&](int x, int y) {
    const minwin_gdi::ScreenSurface screen = minwin_gdi::DeviceScreen(4);
    return palette[screen.bits[y * 4 * screen.pitch + x * 4]];
  };
  require(colorAt(rootX + 1, rootY + 1) == 0xffffff, "root painted");
  require(colorAt(rootX + 45, rootY + 40) == 0xff0000, "child painted");
  require(colorAt(rootX + 18, rootY + 18) == 0x0000ff, "grandchild painted");
  for (int newX : {30, 50, 70}) {
    const int oldX = newX - 20;
    require(reb::compat::SetWindowPos(child, nullptr, newX, 10, 0, 0, SWP_NOSIZE | SWP_NOZORDER), "child moved");
    require(colorAt(rootX + oldX + 1, rootY + 40) == 0xffffff, "uncovered root repainted immediately");
    require(colorAt(rootX + newX + 35, rootY + 40) == 0xff0000, "moved child repainted immediately");
    require(colorAt(rootX + newX + 8, rootY + 18) == 0x0000ff, "moved grandchild repainted immediately");
    require(deviceColorAt(rootX + newX + 35, rootY + 40) == 0xff0000, "device pixels repainted immediately");
  }
  reb::compat::DestroyWindow(root);
  minwin_gdi::DeviceScreen(previousScale);
}

void CheckHighResolutionPaletteDraw(DibBitmap& format)
{
  auto require = [](bool condition, const char* check) {
    if (!condition) {
      throw std::runtime_error(std::string("Native UI smoke: HD palette regression: ") + check);
    }
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
  HDC dc = reb::compat::GetDC(nullptr);
  require(dc != nullptr, "screen DC created");
  const UINT previousPaletteUse = reb::compat::SetSystemPaletteUse(dc, SYSPAL_STATIC);
  require(image.DrawToDc(dc, '@', SRCCOPY, 10, 10, 0, 0, 2, 1), "indexed detail drawn");
  uint32_t palette[256];
  minwin_gdi::CurrentPalette(palette, nullptr);
  const minwin_gdi::ScreenSurface logical = minwin_gdi::Screen();
  const minwin_gdi::ScreenSurface device = minwin_gdi::DeviceScreen(4);
  require(palette[logical.bits[10 * logical.pitch + 10]] == 0xffffff, "logical pixel uses source palette");
  require(palette[device.bits[40 * device.pitch + 40]] == 0xff0000, "detail pixel uses source palette");
  require(palette[device.bits[40 * device.pitch + 41]] == 0xffffff, "adjacent detail pixel remains white");
  reb::compat::SetSystemPaletteUse(dc, previousPaletteUse);
  reb::compat::ReleaseDC(nullptr, dc);
  minwin_gdi::DeviceScreen(previousScale);
}

void CheckEmptyBattleList(RebellionApplication& app, DibBitmap& background)
{
  struct BannerCase {
    int viewSide;
    int winningSide;
    int retreatSide;
    uint32_t bitmapId;
  };
  constexpr BannerCase bannerCases[] = {
      {1, 0, 0, 0x2d63}, {2, 0, 0, 0x2d63}, {1, 0, 1, 0x2a05}, {2, 0, 2, 0x2a06}, {1, 1, 2, 0x2a06}, {1, 1, 1, 0x2a07},
      {2, 2, 1, 0x2a05}, {2, 2, 2, 0x2a08}, {1, 2, 2, 0x2a08}, {2, 1, 1, 0x2a07}, {1, 2, 0, 0x2a08}, {2, 1, 0, 0x2a07},
  };
  for (const BannerCase& test : bannerCases) {
    RequireBattle(BattleSummaryBitmapId(test.viewSide, test.winningSide, test.retreatSide) == test.bitmapId);
  }

  Notice1 notice;
  auto window = std::make_unique<BattleWindow>(app.m_instance, nullptr, 0, &notice);
  window->m_system = nullptr;
  window->m_items = new CoolListItemList(4);
  window->m_list = new CoolDragList(app.m_instance, 0, 0, 100, 40, window.get(), &background, 200, 10, 10, 0, 0, 0, 0,
                                    10, 1, nullptr);
  window->m_list->SetItemList(window->m_items);
  window->m_pageTabs = new CoolTabControl(app.m_instance, 0, 0, 10, 10, 0x6e, window.get(), &background);
  CoolStrobeButton alliance(app.m_instance, 0, 0, 1, 1, window->m_pageTabs, 0x70, 0, 0, &background, 0, 0);
  CoolStrobeButton empire(app.m_instance, 0, 0, 1, 1, window->m_pageTabs, 0x71, 0, 0, &background, 0, 0);

  for (CoolStrobeButton* page : {&alliance, &empire}) {
    window->m_pageTabs->m_selected = page;
    for (int kind : {0x65, 0x66, 0x67, 0x68}) {
      window->m_list->m_fillColor = 0x7f;
      window->FillList(kind, 1);
      const size_t expectedRows = kind == 0x68 ? 3 : 2;
      RequireBattle(window->m_items->Count() == expectedRows);
      for (const auto& row : window->m_items->Entries()) {
        RequireBattle(row->m_image == nullptr);
      }
      RequireBattle(window->m_list->m_fillColor == 0x7f);
    }
  }
}

void CheckStatusLayout(DibBitmap& format)
{
  CoolLabel empty("existing text", 0, 0, 0, 0, DT_WORDBREAK, 0);
  empty.MoveTo(13, 40);
  empty.SetText("");
  RequireStatus(empty.m_rect.left == 13 && empty.m_rect.right >= empty.m_rect.left && empty.m_rect.top == 40
                    && empty.m_rect.bottom >= empty.m_rect.top,
                "empty string clear preserves nonzero origin");
  empty.SetText(nullptr);
  RequireStatus(empty.m_rect.left == 13 && empty.m_rect.right >= empty.m_rect.left && empty.m_rect.top == 40
                    && empty.m_rect.bottom >= empty.m_rect.top,
                "nullptr clear preserves nonzero origin");
  empty.SetText("font reset");
  empty.MoveTo(13, 40);
  empty.SetFont(10);
  RequireStatus(empty.m_rect.left == 13 && empty.m_rect.right >= empty.m_rect.left && empty.m_rect.top == 40
                    && empty.m_rect.bottom >= empty.m_rect.top,
                "font reset preserves nonzero origin");
  const ResourceStringId emptyResource{0x7fff, 0x7fff};
  empty.SetTextFromResource(&emptyResource);
  RequireStatus(empty.m_rect.left == 13 && empty.m_rect.right >= empty.m_rect.left && empty.m_rect.top == 40
                    && empty.m_rect.bottom >= empty.m_rect.top,
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
  for (uint32_t id = 0; id < kStatusRowCount; ++id) {
    const std::string label(rows[id].first);
    const std::string value(rows[id].second);
    AddStatusRow(id, &label, &labels);
    AddStatusRow(id, &value, &values);
  }

  HDC dc = reb::compat::CreateCompatibleDC(nullptr);
  RequireStatus(dc != nullptr, "compatible DC created");
  const int height = LayoutStatusLabels(dc, &labels, &values);
  RequireStatus(height > 0, "total layout height is positive");
  int previousY = -1;
  int shortRowHeight = 0;
  int wrappedRowHeight = 0;
  for (uint32_t id = 0; id < kStatusRowCount; ++id) {
    auto* label = labels.FindById(id);
    auto* value = values.FindById(id);
    RequireStatus(label && value, "matching label and value rows exist");
    RequireStatus(label->m_y > previousY, "label row positions increase");
    RequireStatus(label->m_y == value->m_y || value->m_y > label->m_y, "value row is aligned within its label row");
    if (id == 0) {
      shortRowHeight = static_cast<CoolListItem*>(labels.FindById(id + 1))->m_y - label->m_y;
    }
    if (id == 2) {
      wrappedRowHeight = static_cast<CoolListItem*>(labels.FindById(id + 1))->m_y - label->m_y;
    }
    if (id == 3) {
      RequireStatus(static_cast<CoolListItem*>(labels.FindById(id + 1))->m_y - label->m_y == 10,
                    "fully blank status row receives minimum line height");
    }
    previousY = label->m_y;
  }
  RequireStatus(previousY < height, "total height includes all status rows");
  RequireStatus(wrappedRowHeight > shortRowHeight, "long value wraps to multiple lines");

  DibBitmap canvas(kStatusWidth, height, &format, dc);
  RequireStatus(canvas.IsLoaded(), "DIB section canvas created");
  canvas.FillRect(0, 0, kStatusWidth, height, 0);
  HGDIOBJ previousBitmap = reb::compat::SelectObject(dc, canvas.DibSectionHandle());
  DrawStatusLabels(dc, &labels, &values);
  reb::compat::SelectObject(dc, previousBitmap);
  reb::compat::DeleteDC(dc);

  bool hasRenderedText = false;
  for (int y = 0; y < height && !hasRenderedText; ++y) {
    for (int x = 0; x < static_cast<int>(kStatusWidth); ++x) {
      if (Pixel(canvas, x, y) != 0) {
        hasRenderedText = true;
        break;
      }
    }
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
  for (size_t i = 0; i < std::size(storyKeys); ++i) {
    const ObjectKey key = ObjectKey::Make(storyKeys[i] >> 24, storyKeys[i] & 0xffffff);
    storyCharacters[i] = static_cast<Character*>(GetMasterInFamilies_30_3c(&key));
  }
  RequireBattle(root && allianceHeadquarters && coruscant
                && std::ranges::all_of(storyCharacters, [](const Character* character) {
                     return character != nullptr;
                   }));

  struct SavedVictoryInputs {
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
      for (size_t i = 0; i < 4; ++i) {
        storyCharacters[i]->m_flagsAc = storyFlags[i];
      }
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
  for (Character* character : storyCharacters) {
    character->m_flagsAc &= ~1u;
  }

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
  struct RestoreLukeStoryFlags {
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
      : m_app(app)
      , m_previousGalaxy(g_galaxyWindow)
      , m_previousAccelerators(app.m_unknown98)
      , m_previousSpriteSide(g_spriteDllSide)
      , m_session(GetGameSession())
      , m_previousViewReady(m_session ? m_session->m_viewReady : 0)
      , m_galaxy(std::make_unique<GalaxyWindow>(app.m_instance, 0, 0, 640, 480, &app, 1, 1))
  {
    WNDCLASSA hostClass{};
    hostClass.hInstance = app.m_instance;
    hostClass.lpfnWndProc = reb::compat::DefWindowProcA;
    hostClass.lpszClassName = "ReportSmokeHost";
    reb::compat::RegisterClassA(&hostClass);
    m_host = reb::compat::CreateWindowExA(0, "ReportSmokeHost", "", WS_POPUP, 0, 0, 640, 480, nullptr, nullptr, app.m_instance,
                             nullptr);
    if (!m_host) {
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
    if (m_session) {
      m_session->m_viewReady = m_previousViewReady;
    }
    char* location = GetDllLocation();
    LoadResourceModule(location, "strategy.dll", 7);
    if (m_previousSpriteSide != 0) {
      LoadResourceModule(location, m_previousSpriteSide == 1 ? "alsprite.dll" : "emsprite.dll", 9);
      g_spriteDllSide = m_previousSpriteSide;
    }
    ::operator delete(location);
    if (m_host) {
      reb::compat::DestroyWindow(m_host);
    }
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

struct RenderedReportPage {
  int width;
  int height;
  std::vector<uint8_t> pixels;
};

RenderedReportPage RenderReportPage(RebellionApplication& app, UiTextNotice* notice, int side)
{
  ReportGalaxyFixture parent(app);
  parent.Galaxy().m_side = side;
  if (side == 2) {
    reb::compat::SetRect(&parent.Galaxy().m_panelArea, 0x78, 0x28, 600, 0x18b);
  }

  ReportWindow window(app.m_instance, &parent.Galaxy(), 0x5a, notice);
  RequireBattle(window.Create(0) != 0);
  RequireBattle(window.m_paintBackground && window.m_paintBackground->IsLoaded());
  RequireBattle(window.FindOverlayBitmap(1) == window.m_paintBackground);
  RequireBattle(window.m_paintBackground->Width() > 0x19 && window.m_paintBackground->Height() > 0xe1);

  DibBitmap* rendered = window.m_paintBackground;
  RenderedReportPage out{static_cast<int>(rendered->Width()), static_cast<int>(rendered->Height()), {}};
  for (int y = 0; y < out.height; ++y) {
    for (int x = 0; x < out.width; ++x) {
      out.pixels.push_back(Pixel(*rendered, x, y));
    }
  }
  window.DestroyNativeWindow();
  return out;
}

void CheckReportMainPage(RebellionApplication& app)
{
  for (int side : {1, 2}) {
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
    const auto differsInRows = [&](int first, int end) {
      for (int y = first; y < end; ++y) {
        for (int x = 0; x < rendered.width; ++x) {
          const size_t offset = static_cast<size_t>(y) * rendered.width + x;
          if (rendered.pixels[offset] != withoutText.pixels[offset]) {
            return true;
          }
        }
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
  auto require = [](bool condition, const char* check) {
    if (!condition) {
      throw std::runtime_error(std::string("Native UI smoke: droid delta detail regression: ") + check);
    }
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
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      initial.push_back(Pixel(decoded, x, y));
    }
  }

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
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const uint8_t logical = Pixel(decoded, x, y);
      changed |= logical != initial[static_cast<size_t>(y) * width + x];
      keyed |= logical == decoded.m_colorKey;
      opaque |= logical != decoded.m_colorKey;
      for (int dy = 0; dy < 4; ++dy) {
        for (int dx = 0; dx < 4; ++dx) {
          const uint8_t detailed = decoded.m_hdPixels[static_cast<size_t>(y * 4 + dy) * width * 4 + x * 4 + dx];
          require((detailed == decoded.m_colorKey) == (logical == decoded.m_colorKey), "detail keeps decoded key mask");
        }
      }
    }
  }
  require(changed && keyed && opaque, "real delta changes a visible keyed frame");
}

static void CheckGalaxyResize(RebellionApplication& app)
{
  auto require = [](bool condition, const char* check) {
    if (!condition) {
      throw std::runtime_error(std::string("Native UI smoke: galaxy resize regression: ") + check);
    }
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
  std::unique_ptr<char, void (*)(char*)> dllLocation(GetDllLocation(), [](char* value) {
    ::operator delete(value);
  });
  require(LoadResourceModule(dllLocation.get(), "textstra.dll", 2) != nullptr, "GID string module loaded");
  InitStringLoader();
  require(!LoadStringResource(&customTitle).empty() && !LoadStringResource(&pendingTitle).empty(),
          "GID title resources available");
  galaxy->SetDisplayMode(0x80, nullptr);
  galaxy->m_titleStringId = customTitle;
  galaxy->SetDisplayMode(0x11, nullptr);
  require(galaxy->m_displayMode == 0x11 && galaxy->m_activeTitleStringId.m_id == customTitle.m_id
              && galaxy->m_activeTitleStringId.m_group == customTitle.m_group,
          "custom GID title activated");
  galaxy->m_titleStringId = pendingTitle;
  SectorWindow* rightSector = nullptr;

  const auto checkSectorDrag = [&]() {
    ObjectListItem* sectorItem = nullptr;
    for (const auto& node : galaxy->m_sectorItems.Entries()) {
      sectorItem = static_cast<ObjectListItem*>(node.get());
      break;
    }
    require(sectorItem != nullptr, "sector item available");
    ObjectKey sectorKey;
    sectorItem->GetKey(&sectorKey);
    auto* sector = dynamic_cast<SectorWindow*>(galaxy->OpenSectorWindow(&sectorKey));
    require(sector && sector->m_hwnd, "sector window created");
    require(!sector->HitTestMarkers(&sector->m_markers, 1, 30), "empty sector body point");
    reb::compat::SendMessageA(sector->m_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, (30 << 16) | 1);
    require(!sector->CoolGameWindow::m_dragging, "empty body does not start window drag");
    reb::compat::SendMessageA(sector->m_hwnd, WM_LBUTTONUP, 0, (30 << 16) | 1);
    const int oldX = sector->m_x;
    reb::compat::SendMessageA(sector->m_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, (5 << 16) | 10);
    require(sector->CoolGameWindow::m_dragging && reb::compat::GetCapture() == sector->m_hwnd, "title drag captured");
    reb::compat::SendMessageA(sector->m_hwnd, WM_MOUSEMOVE, MK_LBUTTON, (25 << 16) | 40);
    reb::compat::SendMessageA(sector->m_hwnd, WM_LBUTTONUP, 0, (25 << 16) | 40);
    require(!sector->CoolGameWindow::m_dragging && reb::compat::GetCapture() != sector->m_hwnd, "title drag released capture");
    require(sector->m_x > oldX, "title drag moves sector window");
    require(sector->m_x >= galaxy->m_panelArea.left && sector->m_y >= galaxy->m_panelArea.top
                && sector->m_x + sector->m_clientWidth <= galaxy->m_panelArea.right
                && sector->m_y
                       <= std::max(galaxy->m_panelArea.top, galaxy->m_panelArea.bottom - sector->m_clientHeight),
            "title drag respects panel limits");
    for (const auto& node : sector->m_markers.Entries()) {
      auto* marker = static_cast<ObjectListItem*>(node.get());
      if (marker->m_imageId >= 0x2a13 && marker->m_imageId <= 0x2a26) {
        require(marker->m_image && marker->m_image->m_hdScale == 4 && marker->m_image->m_hdKeyed,
                "sector marker has geometric detail");
        require(marker->m_selectedImage && marker->m_selectedImage->m_hdScale == 4
                    && marker->m_selectedImage->m_hdKeyed,
                "selected sector marker has geometric detail");
      }
    }
    for (const auto& node : sector->m_markers.Entries()) {
      auto* marker = static_cast<ObjectListItem*>(node.get());
      const int x = marker->m_rect.left + 1;
      const int y = marker->m_rect.top + 1;
      if (sector->HitTestMarkers(&sector->m_markers, x, y) != marker) {
        continue;
      }
      reb::compat::SendMessageA(sector->m_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, (y << 16) | x);
      require(sector->m_hitItem == marker && !sector->CoolGameWindow::m_dragging,
              "marker press retains marker selection path");
      reb::compat::SendMessageA(sector->m_hwnd, WM_LBUTTONUP, 0, (y << 16) | x);
      break;
    }
    require(sector->m_hitItem != nullptr, "hittable sector marker available");

    auto dispatchInput = []() {
      MSG message;
      for (int count = 0; count < 1000 && reb::compat::PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE); ++count) {
        reb::compat::TranslateMessage(&message);
        reb::compat::DispatchMessageA(&message);
      }
    };
    POINT start = {10, 5};
    reb::compat::ClientToScreen(sector->m_hwnd, &start);
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

    reb::compat::SendMessageA(sector->m_hwnd, WM_COMMAND, 0x139, 0);
    require(sector->m_x == galaxy->SectorSlotX(false), "side button returns dragged sector to left slot");
    reb::compat::SendMessageA(sector->m_hwnd, WM_COMMAND, 0x139, 0);
    require(sector->m_x == galaxy->SectorSlotX(true), "side button moves sector to wide right slot");
    rightSector = sector;

    bool checkedSecondSector = false;
    for (const auto& node : galaxy->m_sectorItems.Entries()) {
      auto* nextItem = static_cast<ObjectListItem*>(node.get());
      ObjectKey nextKey;
      nextItem->GetKey(&nextKey);
      if (nextKey.Id() == sectorKey.Id()) {
        continue;
      }
      auto* nextSector = dynamic_cast<SectorWindow*>(galaxy->OpenSectorWindow(&nextKey));
      require(nextSector && nextSector->m_x == galaxy->SectorSlotX(false), "new sector uses free wide left slot");
      checkedSecondSector = true;
      break;
    }
    require(checkedSecondSector, "second sector available for slot check");
  };

  for (int width : {854, 1280, 640}) {
    require(minwin_gdi::SetScreenSize(width, 480), "logical screen resized");
    app.WindowProc(WM_SIZE, 0, (static_cast<LPARAM>(480) << 16) | width);
    require(galaxy->m_clientWidth == width && galaxy->m_canvasLayoutWidth == width, "galaxy client resized");
    require(galaxy->GetBackground() == backdrop && backdrop->IsLoaded() && backdrop->Width() == width,
            "borrowed background resized in place");
    require(galaxy->m_offscreen && galaxy->m_offscreen->Width() == width, "offscreen rebuilt");
    require(galaxy->m_displayMode == 0x11 && galaxy->m_activeTitleStringId.m_id == customTitle.m_id
                && galaxy->m_activeTitleStringId.m_group == customTitle.m_group,
            "active GID title survives resize");
    require(galaxy->m_titleStringId.m_id == pendingTitle.m_id
                && galaxy->m_titleStringId.m_group == pendingTitle.m_group,
            "pending GID title survives resize");
    if (rightSector) {
      require(rightSector->m_x == galaxy->SectorSlotX(true), "existing sector follows right slot on resize");
    }
    if (width > 640) {
      bool paintedBeyondClassic = false;
      for (int y = 0; y < 480 && !paintedBeyondClassic; ++y) {
        for (int x = 640; x < width; ++x) {
          if (Pixel(*backdrop, x, y) != 0) {
            paintedBeyondClassic = true;
            break;
          }
        }
      }
      require(paintedBeyondClassic, "new logical canvas contains artwork beyond 640");
    }
    reb::compat::InvalidateRect(galaxy->m_hwnd, nullptr, FALSE);
    reb::compat::UpdateWindow(galaxy->m_hwnd);
    for (CoolStrobeButton* button : galaxy->m_consoleButtons) {
      require(button && button->m_backdrop == backdrop, "console button backdrop");
      reb::compat::InvalidateRect(button->m_hwnd, nullptr, FALSE);
      reb::compat::UpdateWindow(button->m_hwnd);
    }
    for (CoolFrameButton* button : galaxy->m_messageButtons) {
      require(button && button->m_backdrop == backdrop, "message button backdrop");
      reb::compat::InvalidateRect(button->m_hwnd, nullptr, FALSE);
      reb::compat::UpdateWindow(button->m_hwnd);
    }
    if (width == 854) {
      checkSectorDrag();
    }
  }
  galaxy->SetDisplayMode(0x12, nullptr);
  require(galaxy->m_activeTitleStringId.m_id == pendingTitle.m_id
              && galaxy->m_activeTitleStringId.m_group == pendingTitle.m_group && galaxy->m_titleStringId.m_id == 0,
          "next GID mode consumes pending title");
  galaxy->SetDisplayMode(0x80, nullptr);
  galaxy->m_titleStringId = previousTitle;
  if (previousMode != 0x80) {
    galaxy->SetDisplayMode(previousMode, &previousSelection);
  }
  galaxy->m_activeTitleStringId = previousTitle;
  galaxy->m_titleStringId = previousPendingTitle;
  galaxy->m_selectedIds = previousSelection;
}

namespace
{

void RequireAiMenu(bool condition, const char* check)
{
  if (!condition) {
    throw std::runtime_error(std::string("Native AI menu smoke: ") + check);
  }
}

MenuItem* FindAiMenuItem(const std::string& label)
{
  RequireAiMenu(g_menuWindow != nullptr, "AI menu is open");
  std::string choices;
  for (const auto& ownedItem : g_menuWindow->m_items.Entries()) {
    auto* item = static_cast<MenuItem*>(ownedItem.get());
    RequireAiMenu(item->m_label != nullptr, "AI menu choice has a label");
    const char* text = item->m_label->GetText();
    if (text && std::string(text).ends_with(label)) {
      return item;
    }
    if (text) {
      choices += std::string(text) + "; ";
    }
  }
  throw std::runtime_error("Native AI menu smoke: missing " + label + " among " + choices);
}

void ClickAiMenuItem(MenuItem* item)
{
  RequireAiMenu(item && item->m_area.bottom > item->m_area.top, "clickable menu choice");
  g_menuWindow->HandleMouse(item, item->m_area.left + 1, item->m_area.top + 1, WM_LBUTTONUP);
  DismissMenu();
}

void CheckFleetLabelsAndDrag(RebellionApplication& app)
{
  GalaxyWindow* galaxy = app.m_strategicWindow;
  GameObjNotifier* host = nullptr;
  for (auto* object = GetFirstObjectInRangeForSide(galaxy->m_side, &family::kSystems, 0); object;
       object = static_cast<GameObj*>(object)->FindNextSibling(&family::kSystems, 0)) {
    FleetIterator fleets(object, 3, galaxy->m_side);
    fleets.Begin();
    if (fleets.Current()) {
      host = object;
      break;
    }
  }
  RequireAiMenu(host != nullptr, "fleet available for layout check");
  FleetWindow panel(app.m_instance, 20, 20, galaxy, 0xfff3, host);
  RequireAiMenu(panel.Create(0) != 0, "fleet panel created");
  auto* fleet = panel.m_fleetItems->First();
  RequireAiMenu(fleet != nullptr, "fleet row exists");
  panel.m_fleetList->SelectById(fleet->m_id, 0);
  panel.UpdateShipList();
  auto* list = panel.m_shipList;
  auto* first = list->FirstItem();
  RequireAiMenu(first != nullptr, "fleet contains a unit");
  first->m_text = "Imperial Star Destroyer\n(ETA 413)";
  list->Refresh();
  list->SelectById(first->m_id, 0);
  const RECT text = list->GetItemTextRect(first);
  RequireAiMenu(text.top >= first->m_rect.top + 25 && text.bottom <= first->m_rect.bottom,
                "multiline unit label fits below picture");
  first->m_text = "Imperial Star Destroyer";
  const RECT singleLineText = list->GetItemTextRect(first);
  RequireAiMenu(singleLineText.top == text.top && singleLineText.bottom < text.bottom,
                "unit names keep the same position with and without an ETA");
  first->m_text = "Imperial Star Destroyer\n(ETA 413)";
  auto* second = list->m_items->NextVisible(first);
  if (second) {
    list->SelectById(second->m_id, 0);
  }
  const LPARAM pressed = MAKEWPARAM(20, 10);
  list->WindowProc(WM_LBUTTONDOWN, MK_LBUTTON | MK_SHIFT, pressed);
  list->WindowProc(WM_MOUSEMOVE, MK_LBUTTON | MK_SHIFT, MAKEWPARAM(21, 11));
  RequireAiMenu((list->m_options.m_value & kDragListDragging) == 0, "click jitter does not begin drag");
  list->WindowProc(WM_LBUTTONUP, MK_SHIFT, MAKEWPARAM(21, 11));
  RequireAiMenu((first->m_state.m_value & kListItemSelected) != 0, "jitter preserves selection");
  list->WindowProc(WM_LBUTTONDOWN, MK_LBUTTON, pressed);
  list->WindowProc(WM_MOUSEMOVE, MK_LBUTTON, MAKEWPARAM(28, 10));
  RequireAiMenu((list->m_options.m_value & kDragListDragging) != 0, "deliberate movement begins drag");
  list->WindowProc(WM_CAPTURECHANGED, 0, 0);
  RequireAiMenu((list->m_options.m_value & kDragListDragging) == 0, "cancel clears drag");
  RequireAiMenu((first->m_state.m_value & kListItemSelected) != 0, "cancel preserves selection");
  const DibBitmap* selectedImage = first->m_selectedImage;
  RequireAiMenu(selectedImage != nullptr, "selected artwork exists");
  bool checkedHighlight = false;
  for (int y = 0; y < selectedImage->Height(); ++y) {
    for (int x = 0; x < selectedImage->Width(); ++x) {
      const auto selectedPixel = Pixel(*selectedImage, x, y);
      if (selectedPixel != selectedImage->m_colorKey && selectedPixel != Pixel(*first->m_image, x, y)) {
        RequireAiMenu(Pixel(*list->m_canvas, first->m_x + x, first->m_y + y) == selectedPixel,
                      "selection highlight survives cancelled drag");
        checkedHighlight = true;
      }
    }
  }
  RequireAiMenu(checkedHighlight, "selection has visible highlight pixels");
  reb::compat::ShowWindow(panel.m_hwnd, SW_SHOW);
  reb::compat::UpdateWindow(galaxy->m_hwnd);
  reb::compat::UpdateWindow(panel.m_hwnd);
  reb::compat::UpdateWindow(panel.m_fleetList->m_hwnd);
  reb::compat::UpdateWindow(list->m_hwnd);
  if (const char* path = std::getenv("REB_FLEET_LAYOUT_IMAGE")) {
    const auto capture = [&](const std::string& outputPath) {
      const auto screen = minwin_gdi::DeviceScreen(minwin_gdi::DeviceScale());
      uint32_t palette[256];
      minwin_gdi::CurrentPalette(palette, nullptr);
      std::ofstream output(outputPath, std::ios::binary);
      output << "P6\n" << screen.width << ' ' << screen.height << "\n255\n";
      for (int y = 0; y < screen.height; ++y) {
        for (int x = 0; x < screen.width; ++x) {
          const auto rgb = palette[screen.bits[y * screen.pitch + x]];
          const char bytes[] = {static_cast<char>(rgb >> 16), static_cast<char>(rgb >> 8), static_cast<char>(rgb)};
          output.write(bytes, 3);
        }
      }
      RequireAiMenu(output.good(), "fleet screenshot written");
    };
    capture(path);
    first->m_text = "Imperial Star Destroyer";
    list->Refresh();
    reb::compat::UpdateWindow(list->m_hwnd);
    capture(std::string(path) + ".no-eta.ppm");
  }
  panel.Hide();
  OverrideEnhancementsForProcess(false);
  {
    FleetWindow original(app.m_instance, 20, 20, galaxy, 0xfff4, host);
    RequireAiMenu(original.Create(0) != 0, "original-layout fleet panel created");
    RequireAiMenu(!original.m_compactLabels && original.RowHeight() == 0x32
                      && (original.m_shipList->m_textFormat & DT_SINGLELINE) != 0,
                  "enhancements off keeps the original fleet layout");
    if (const char* path = std::getenv("REB_FLEET_LAYOUT_IMAGE")) {
      original.m_fleetList->SelectById(original.m_fleetItems->First()->m_id, 0);
      original.UpdateShipList();
      reb::compat::ShowWindow(original.m_hwnd, SW_SHOW);
      reb::compat::UpdateWindow(galaxy->m_hwnd);
      reb::compat::UpdateWindow(original.m_hwnd);
      reb::compat::UpdateWindow(original.m_fleetList->m_hwnd);
      reb::compat::UpdateWindow(original.m_shipList->m_hwnd);
      SaveUiReviewFrame(std::string(path) + ".original.bmp");
    }
    reb::compat::DestroyWindow(original.m_hwnd);
  }
  OverrideEnhancementsForProcess(true);
  reb::compat::DestroyWindow(panel.m_hwnd);
}

void CheckForcesAndFleetTabArtwork(RebellionApplication& app)
{
  const char* configured = std::getenv("REB_FORCES_TAB_REVIEW_DIR");
  if (!configured || !*configured) {
    return;
  }
  const std::filesystem::path output = std::filesystem::u8path(configured);
  std::filesystem::create_directories(output);
  GalaxyWindow* galaxy = app.m_strategicWindow;
  auto repaintTree = [](auto&& self, CoolWindow* window) -> void {
    reb::compat::InvalidateRect(window->m_hwnd, nullptr, TRUE);
    reb::compat::UpdateWindow(window->m_hwnd);
    for (const auto& child : window->m_children.Entries()) {
      self(self, child.get());
    }
  };
  auto saveWindow = [&](CoolWindow& window, CoolTabControl& tabs, const std::string& name) {
    repaintTree(repaintTree, &window);
    repaintTree(repaintTree, &tabs);
    SaveHdReviewFrame(output / (name + ".bmp"));
  };
  auto requireHdState = [](CoolStrobeButton& button, int state, const char* description) {
    const DibBitmap* bitmap = button.GetStateBitmap(state);
    RequireAiMenu(bitmap && bitmap->IsLoaded() && !bitmap->m_hdPixels.empty(), description);
  };
  auto selectState = [](CoolStrobeButton& button, uint32_t state) {
    button.ClearStateFlags(kButtonPressed | kButtonDisabled | kButtonChecked | kButtonHighlight);
    button.m_hot = (state & kButtonPressed) != 0;
    button.SetStateFlags(state);
  };

  const int originalViewSide = galaxy->m_side;
  struct RestoreGalaxySide {
    GalaxyWindow& galaxy;
    int side;
    ~RestoreGalaxySide() { galaxy.m_side = side; }
  } restoreSide{*galaxy, originalViewSide};
  GameObjNotifier* systems[2]{};
  GameObjNotifier* fleetSystems[2][2]{};
  GameObjNotifier* fleetsForReview[2][2]{};
  for (int viewSide = 1; viewSide <= 2; ++viewSide) {
    galaxy->m_side = viewSide;
    for (auto* system = GetFirstObjectInFamilies_90_98(AppGetSide()); system;
         system = static_cast<GameObj*>(system)->FindNextSibling(&family::kSystems, 1)) {
      const uint32_t controlSide = system->ControlSide();
      if ((controlSide == 1 || controlSide == 2) && !systems[controlSide - 1]) {
        systems[controlSide - 1] = system;
      }
      FleetIterator fleetIterator(system, 3);
      for (fleetIterator.Begin(); fleetIterator.Current(); fleetIterator.Advance()) {
        const uint32_t fleetSide = fleetIterator.Current()->ControlSide();
        if (fleetSide != 1 && fleetSide != 2) {
          continue;
        }
        const int group = fleetSide == static_cast<uint32_t>(viewSide) ? 0 : 1;
        if (!fleetSystems[viewSide - 1][group]) {
          fleetSystems[viewSide - 1][group] = system;
          fleetsForReview[viewSide - 1][group] = fleetIterator.Current();
        }
      }
    }
  }
  galaxy->m_side = originalViewSide;
  RequireAiMenu(systems[0] || systems[1], "loaded game has a controlled system for tab review");
  if (!systems[0]) systems[0] = systems[1];
  if (!systems[1]) systems[1] = systems[0];
  RequireAiMenu((fleetSystems[0][0] || fleetSystems[0][1]) && (fleetSystems[1][0] || fleetSystems[1][1]),
                "loaded game has a visible fleet for tab review");

  const int defenseTabs[] = {5, 4, 3, 2, 1};
  const char* defenseNames[] = {"weapons", "shields", "fighters", "troops", "personnel"};
  for (int side = 1; side <= 2; ++side) {
    SystemForcesWindow panel(app.m_instance, 80, 55, 235, 331, galaxy, 0xfff4, systems[side - 1]);
    RequireAiMenu(panel.Create(0) != 0, "system forces panel created");
    panel.m_side = side;
    panel.LoadHeaderBitmaps();
    panel.Show(SW_SHOW);
    for (size_t index = 0; index < std::size(defenseTabs); ++index) {
      const int tabId = defenseTabs[index];
      panel.m_tabs->SelectTab(static_cast<uint32_t>(tabId), 1);
      panel.ShowTab(tabId);
      RequireAiMenu(panel.m_tab == tabId, "system forces defense tab selected");
      auto* button = static_cast<CoolStrobeButton*>(panel.m_tabs->m_children.FindById(tabId));
      RequireAiMenu(button != nullptr, "system forces defense tab exists");
      const uint32_t dimResources[2][5] = {{0x2938, 0x293b, 0x293e, 0x2945, 0x294c},
                                         {0x2938, 0x293b, 0x2941, 0x2948, 0x294f}};
      const uint32_t dimResource = dimResources[side - 1][index];
      button->LoadStateBitmap(0, dimResource - 2);
      requireHdState(*button, 0, "defense tab normal artwork loads HD pixels");
      requireHdState(*button, 1, "defense tab pressed artwork loads HD pixels");
      selectState(*button, 0);
      saveWindow(panel, *panel.m_tabs, "forces-side-" + std::to_string(side) + "-" + defenseNames[index] + "-normal");
      selectState(*button, kButtonPressed);
      saveWindow(panel, *panel.m_tabs, "forces-side-" + std::to_string(side) + "-" + defenseNames[index] + "-pressed");
      button->LoadStateBitmap(0, dimResource);
      requireHdState(*button, 0, "defense tab dim artwork loads HD pixels");
      selectState(*button, 0);
      saveWindow(panel, *panel.m_tabs, "forces-side-" + std::to_string(side) + "-" + defenseNames[index] + "-dim");
    }
    reb::compat::DestroyWindow(panel.m_hwnd);
  }

  constexpr int fleetPages[] = {0x66, 0x67, 0x68, 0x69};
  const char* fleetNames[] = {"ships", "fighters", "troops", "personnel"};
  for (int side = 1; side <= 2; ++side) {
    galaxy->m_side = side;
    for (int group = 0; group != 2; ++group) {
      if (!fleetSystems[side - 1][group]) {
        continue;
      }
      FleetWindow panel(app.m_instance, 80, 55, galaxy, 0xfff5, fleetSystems[side - 1][group]);
      RequireAiMenu(panel.Create(0) != 0, "fleet panel created");
      panel.Show(SW_SHOW);
      const auto checkTabGroup = [&](CoolTabControl* tabGroup) {
        RequireAiMenu(tabGroup != nullptr, "fleet tab group exists");
        for (const uint32_t page : fleetPages) {
          auto* tab = static_cast<CoolStrobeButton*>(tabGroup->m_children.FindById(page));
          RequireAiMenu(tab != nullptr, "fleet kind tab exists in both tab groups");
          requireHdState(*tab, 0, "fleet tab normal artwork loads HD pixels");
          requireHdState(*tab, 1, "fleet tab pressed artwork loads HD pixels");
          requireHdState(*tab, 4, "fleet tab selected artwork loads HD pixels");
          if (tab->GetStateBitmap(2)) {
            requireHdState(*tab, 2, "fleet tab dim artwork loads HD pixels");
          }
        }
      };
      checkTabGroup(panel.m_ownTabs);
      checkTabGroup(panel.m_otherTabs);
      ObjectKey selectedFleetKey;
      fleetsForReview[side - 1][group]->GetKey(&selectedFleetKey);
      auto* row = static_cast<FleetListItem*>(panel.m_fleetItems->FindById(selectedFleetKey.Id()));
      RequireAiMenu(row != nullptr, "selected fleet has a row in its panel");
      const bool fleetIsOwn = (row->m_flags.m_value & kFleetItemOwn) != 0;
      RequireAiMenu(fleetIsOwn == (group == 0), "tab group matches selected fleet control side");
      row->m_state.m_value |= kListItemSelected;
      row->m_userData = 0;
      panel.m_uiFlags.m_value &= ~(kFleetOwnSelected | kFleetOtherSelected | kFleetShipSelected);
      panel.m_uiFlags.m_value |= fleetIsOwn ? kFleetOwnSelected : kFleetOtherSelected;
      panel.m_page = fleetPages[0];
      panel.UpdateTabs();
      CoolTabControl* tabs = fleetIsOwn ? panel.m_ownTabs : panel.m_otherTabs;
      CoolTabControl* hiddenTabs = fleetIsOwn ? panel.m_otherTabs : panel.m_ownTabs;
      RequireAiMenu(reb::compat::IsWindowVisible(tabs->m_hwnd) && !reb::compat::IsWindowVisible(hiddenTabs->m_hwnd),
                    "selected fleet side has the visible tab group");
      const char* groupName = fleetIsOwn ? "own" : "other";
      for (size_t index = 0; index < std::size(fleetPages); ++index) {
        const uint32_t page = fleetPages[index];
        auto* button = static_cast<CoolStrobeButton*>(tabs->m_children.FindById(page));
        tabs->SelectTab(page, 1);
        selectState(*button, 0);
        saveWindow(panel, *tabs, "fleet-side-" + std::to_string(side) + "-" + groupName + "-" + fleetNames[index]
                              + "-normal");
        selectState(*button, kButtonPressed);
        saveWindow(panel, *tabs, "fleet-side-" + std::to_string(side) + "-" + groupName + "-" + fleetNames[index]
                              + "-pressed");
        selectState(*button, kButtonHighlight);
        saveWindow(panel, *tabs, "fleet-side-" + std::to_string(side) + "-" + groupName + "-" + fleetNames[index]
                              + "-dim");
        selectState(*button, kButtonChecked);
        saveWindow(panel, *tabs, "fleet-side-" + std::to_string(side) + "-" + groupName + "-" + fleetNames[index]
                              + "-selected");
      }
      reb::compat::DestroyWindow(panel.m_hwnd);
    }
  }
}

void CheckArrivalDisplay(RebellionApplication& app)
{
  std::vector<Character*> reviewCharacters;
  Character* character = nullptr;
  for (GameObjNotifier* object = GetFirstObjectInRangeForSide(app.m_strategicWindow->m_side, &family::kCharacters, 0);
       object; object = static_cast<GameObj*>(object)->FindNextSibling(&family::kCharacters, 0)) {
    auto* candidate = static_cast<Character*>(object);
    ObjectKey recordKey;
    GetObjectRecordKey(candidate, &recordKey);
    if (candidate->GetRecordSide() == app.m_strategicWindow->m_side && recordKey.m_value != 0x3c000003
        && recordKey.m_value != 0x3c000005 && reviewCharacters.size() < 2) {
      reviewCharacters.push_back(candidate);
    }
    if (!character && (candidate->m_stateFlags & 2) && candidate->GetRecordSide() == app.m_strategicWindow->m_side) {
      character = candidate;
    }
  }
  RequireAiMenu(character != nullptr, "character available for ETA check");
  struct RestoreTravel {
    GameObj* object;
    uint32_t flags;
    int eta;
    ObjectKey departure;
    ~RestoreTravel()
    {
      object->m_stateFlags = flags;
      object->m_eta = eta;
      object->m_departureKey = departure;
    }
  } restore{character, character->m_stateFlags, character->m_eta, character->m_departureKey};
  constexpr uint32_t kEnroute = strategy_ui::kEnrouteState;
  const std::string name = character->GetName()->c_str();
  character->m_stateFlags &= ~kEnroute;
  RequireAiMenu(strategy_ui::DisplayName(character) == name, "stationary name has no ETA");
  ObjectKey owner;
  RequireAiMenu(character->GetOwnerKey(&owner) != 0 && owner.IsSet(), "character has travel owner");
  character->m_departureKey = owner;
  character->m_stateFlags |= kEnroute;
  character->m_eta = 413;
  RequireAiMenu(strategy_ui::DisplayName(character) == name + " (ETA 413)", "en-route name shows arrival day");
  OverrideEnhancementsForProcess(false);
  RequireAiMenu(strategy_ui::DisplayName(character) == name, "enhancements off shows the original name");
  OverrideEnhancementsForProcess(true);
  character->m_eta = 417;
  RequireAiMenu(strategy_ui::DisplayName(character) == name + " (ETA 417)", "changed ETA updates display");
  character->m_stateFlags &= ~kEnroute;
  RequireAiMenu(strategy_ui::DisplayName(character) == name, "arrival removes ETA suffix");
  RequireAiMenu(*character->GetName() == name, "ETA display preserves original name");
  GalaxyWindow* galaxy = app.m_strategicWindow;
  galaxy->OpenPersonnelFinder();
  auto* finder = static_cast<PersonnelFinderWindow*>(galaxy->m_children.FindById(0x17));
  RequireAiMenu(finder != nullptr && galaxy->m_openWindowIds.contains(0x17), "personnel finder receives updates");
  finder->m_pageTabs->SelectTab(3, 1);
  ObjectKey key;
  character->GetKey(&key);
  auto characterText = [&]() -> std::string {
    for (const auto& ownedRow : finder->m_characterRows->Entries()) {
      auto* row = static_cast<ObjectListItem*>(ownedRow.get());
      if (row->m_key.m_value == key.m_value) {
        return row->m_text;
      }
    }
    throw std::runtime_error("Native ETA smoke: missing character row");
  };
  character->m_stateFlags |= kEnroute;
  character->m_eta = 413;
  galaxy->BroadcastObjectUpdated(&key, character);
  RequireAiMenu(characterText().find("(ETA 413)") != std::string::npos, "personnel row updates on travel");
  character->m_stateFlags &= ~kEnroute;
  galaxy->BroadcastObjectUpdated(&key, character);
  RequireAiMenu(characterText().find("(ETA ") == std::string::npos, "personnel row removes ETA on arrival");
  if (const char* reviewDirectory = std::getenv("REB_CHARACTER_TRANSIT_REVIEW_DIR");
      reviewDirectory && *reviewDirectory) {
    RequireAiMenu(reviewCharacters.size() == 2, "character transit review has two ordinary characters");
    for (Character* reviewCharacter : reviewCharacters) {
      struct RestoreReviewCharacter {
        Character& object;
        uint32_t stateFlags;
        int eta;
        ObjectKey departure;
        uint32_t characterFlags;
        ~RestoreReviewCharacter()
        {
          object.m_stateFlags = stateFlags;
          object.m_eta = eta;
          object.m_departureKey = departure;
          object.m_flagsAc = characterFlags;
        }
      } restoreReviewCharacter{*reviewCharacter, reviewCharacter->m_stateFlags, reviewCharacter->m_eta,
                               reviewCharacter->m_departureKey, reviewCharacter->m_flagsAc};
      ObjectKey reviewKey;
      reviewCharacter->GetKey(&reviewKey);
      reviewCharacter->m_flagsAc &= ~1u;
      reviewCharacter->m_stateFlags &= ~kEnroute;
      auto* stationaryPicture = CreateObjectPicture(CoolAppGetResourceModule(10), &reviewKey, reviewCharacter, 0, 1);
      reviewCharacter->m_stateFlags |= kEnroute;
      reviewCharacter->m_eta = 413;
      auto* picture = CreateObjectPicture(CoolAppGetResourceModule(10), &reviewKey, reviewCharacter, 0, 1);
      std::unique_ptr<DibBitmap> transitPicture(picture);
      std::unique_ptr<DibBitmap> stationaryComposite(stationaryPicture);
      RequireAiMenu(stationaryComposite && transitPicture, "ordinary character transit composites load from GOKRES");
      RequireAiMenu(picture && picture->Width() > 0 && picture->Height() > 0,
                    "ordinary character transit picture composes backdrop and portrait");
      RequireAiMenu(!transitPicture->m_hdPixels.empty(), "character transit composite carries high-resolution pixels");
      RequireAiMenu(stationaryComposite->Width() == transitPicture->Width()
                        && stationaryComposite->Height() == transitPicture->Height(),
                    "transit overlay is clipped to the character picture bounds");
      RequireAiMenu(stationaryComposite->m_hdPixels != transitPicture->m_hdPixels,
                    "transit state adds HD detail to the composed character picture");
      const uint32_t portraitId = static_cast<GameObj*>(reviewCharacter)->GetRecordTypeBits() & 0xfff;
      DibBitmap portrait(CoolAppGetResourceModule(10), portraitId + 0x4000, 10);
      RequireAiMenu(portrait.IsLoaded() && !portrait.m_hdPixels.empty()
                        && portrait.m_hdScale == transitPicture->m_hdScale,
                    "ordinary character portrait detail loads at the transit composite scale");
      bool opaquePortraitPixelsPreserved = portrait.Width() == transitPicture->Width()
                                           && portrait.Height() == transitPicture->Height()
                                           && portrait.m_hdPixels.size() == transitPicture->m_hdPixels.size();
      for (size_t pixel = 0; pixel < portrait.m_hdPixels.size() && opaquePortraitPixelsPreserved; ++pixel) {
        if (portrait.m_hdPixels[pixel] != portrait.m_colorKey
            && portrait.m_hdPixels[pixel] != transitPicture->m_hdPixels[pixel]) {
          opaquePortraitPixelsPreserved = false;
        }
      }
      RequireAiMenu(opaquePortraitPixelsPreserved, "transit backdrop leaves every opaque portrait pixel unchanged");
    }
  }
  {
    const auto originalFlags = character->m_flagsAc;
    auto* originalParent = character->m_parent;
    character->m_flagsAc |= 1;
    character->m_parent = nullptr;
    ObjectListItem capturedRow(&key);
    finder->DescribeCharacter(character, &capturedRow);
    character->m_parent = originalParent;
    character->m_flagsAc = originalFlags;
    RequireAiMenu(capturedRow.m_visible && capturedRow.m_text.find("Captured - location unknown") != std::string::npos,
                  "captured character without a visible location remains identifiable");
  }
  galaxy->m_openWindowIds.erase(0x17);
  galaxy->DestroyChildById(0x17);
}

void CheckFacilityTransitArtwork(RebellionApplication& app)
{
  const char* reviewDirectory = std::getenv("REB_FACILITY_TRANSIT_REVIEW_DIR");
  if (!reviewDirectory || !*reviewDirectory) {
    return;
  }

  const auto requireOpaquePixelsPreserved = [](const DibBitmap& stationary, const DibBitmap& transit,
                                               const DibBitmap& foreground, const char* description) {
    RequireAiMenu(stationary.Width() == transit.Width() && stationary.Height() == transit.Height(),
                  "stationary and en-route transit pictures have matching dimensions");
    RequireAiMenu(foreground.Width() == transit.Width() && foreground.Height() == transit.Height(),
                  "foreground and en-route transit pictures have matching dimensions");
    RequireAiMenu(transit.m_hdScale > 1 && !transit.m_hdPixels.empty(), "transit composite has HD pixels");
    const int scale = transit.m_hdScale;
    const int hdWidth = transit.Width() * scale;
    RequireAiMenu(foreground.m_hdPixels.empty() || foreground.m_hdScale == scale,
                  "opaque foreground and transit composite use matching HD scales");
    RequireAiMenu(stationary.m_hdPixels.empty() || stationary.m_hdScale == scale,
                  "stationary and transit composites use matching HD scales");
    bool backgroundChanged = false;
    for (int y = 0; y < transit.Height() * scale; ++y) {
      for (int x = 0; x < hdWidth; ++x) {
        const uint8_t sourcePixel = foreground.m_hdPixels.empty()
                                        ? Pixel(foreground, x / scale, y / scale)
                                        : foreground.m_hdPixels[static_cast<size_t>(y) * hdWidth + x];
        if (sourcePixel != foreground.m_colorKey) {
          RequireAiMenu(transit.m_hdPixels[static_cast<size_t>(y) * hdWidth + x] == sourcePixel, description);
        } else {
          const uint8_t stationaryPixel = stationary.m_hdPixels.empty()
                                              ? Pixel(stationary, x / scale, y / scale)
                                              : stationary.m_hdPixels[static_cast<size_t>(y) * hdWidth + x];
          backgroundChanged |= stationaryPixel != transit.m_hdPixels[static_cast<size_t>(y) * hdWidth + x];
        }
      }
    }
    RequireAiMenu(backgroundChanged, description);
  };
  const auto saveComparison
      = [&](const std::filesystem::path& path, const DibBitmap& stationary, const DibBitmap& transit) {
          RequireAiMenu(transit.m_hdScale > 1 && !transit.m_hdPixels.empty(), "transit review image has HD pixels");
          const int scale = transit.m_hdScale;
          const int imageWidth = stationary.Width() * scale;
          const int imageHeight = stationary.Height() * scale;
          uint32_t palette[256];
          minwin_gdi::CurrentPalette(palette, nullptr);
          std::vector<uint32_t> pixels(static_cast<size_t>(imageWidth * 2) * imageHeight);
          for (int y = 0; y < imageHeight; ++y) {
            for (int x = 0; x < imageWidth; ++x) {
              const uint8_t stationaryPixel = stationary.m_hdPixels.empty()
                                                  ? Pixel(stationary, x / scale, y / scale)
                                                  : stationary.m_hdPixels[static_cast<size_t>(y) * imageWidth + x];
              const uint8_t transitPixel = transit.m_hdPixels[static_cast<size_t>(y) * imageWidth + x];
              pixels[static_cast<size_t>(y) * imageWidth * 2 + x]
                  = stationaryPixel == stationary.m_colorKey ? 0 : palette[stationaryPixel];
              pixels[static_cast<size_t>(y) * imageWidth * 2 + imageWidth + x]
                  = transitPixel == transit.m_colorKey ? 0 : palette[transitPixel];
            }
          }
          std::filesystem::create_directories(path.parent_path());
          RequireAiMenu(minwin::present::CaptureScreenshotPixels(pixels.data(), imageWidth * 2, imageHeight,
                                                                 path.string().c_str()),
                        "transit review composite image saved");
        };

  GameObj* facility = nullptr;
  GameObjNotifier* facilitySystem = nullptr;
  for (auto* system = GetFirstObjectInFamilies_90_98(app.m_strategicWindow->m_side); system && !facility;
       system = static_cast<GameObj*>(system)->FindNextSibling(&family::kSystems, 1)) {
    ManufacturingFacilityIterator facilities(system);
    for (facilities.Begin(); facilities.Current(); facilities.Advance()) {
      auto* candidate = static_cast<GameObj*>(facilities.Current());
      const uint32_t flags = candidate->m_stateFlags;
      candidate->m_stateFlags = (flags & ~(strategy_ui::kEnrouteState | 0x8u)) | 0x5u;
      std::unique_ptr<DibBitmap> picture(SystemObjectPicture(candidate));
      candidate->m_stateFlags = flags;
      if (picture && picture->IsLoaded() && !picture->m_hdPixels.empty()) {
        facility = candidate;
        facilitySystem = system;
        break;
      }
    }
  }
  RequireAiMenu(facility != nullptr, "completed usable system facility has an HD picture");
  const uint32_t originalFacilityFlags = facility->m_stateFlags;
  struct RestoreFacilityFlags {
    GameObj& object;
    uint32_t flags;
    ~RestoreFacilityFlags()
    {
      object.m_stateFlags = flags;
    }
  } restoreFacility{*facility, originalFacilityFlags};
  facility->m_stateFlags = (facility->m_stateFlags & ~(strategy_ui::kEnrouteState | 0x8u)) | 0x5u;
  std::unique_ptr<DibBitmap> facilityStationary(SystemObjectPicture(facility));
  facility->m_stateFlags |= strategy_ui::kEnrouteState;
  std::unique_ptr<DibBitmap> facilityTransit(SystemObjectPicture(facility));
  RequireAiMenu(facilityStationary != nullptr && facilityStationary->IsLoaded(),
                "system facility stationary picture loads");
  RequireAiMenu(facilityTransit != nullptr && facilityTransit->IsLoaded(), "system facility transit picture loads");
  RequireAiMenu(!facilityStationary->m_hdPixels.empty() && !facilityTransit->m_hdPixels.empty(),
                "system facility stationary and transit pictures have HD detail");
  RequireAiMenu(facilityStationary->m_hdPixels != facilityTransit->m_hdPixels,
                "system facility en-route state adds the shared transit backdrop");
  requireOpaquePixelsPreserved(*facilityStationary, *facilityTransit, *facilityStationary,
                               "facility transit preserves opaque HD foreground pixels");
  saveComparison(std::filesystem::u8path(reviewDirectory) / "facility-transit.bmp", *facilityStationary,
                 *facilityTransit);
  ObjectKey facilityRecordKey;
  GetObjectRecordKey(facility, &facilityRecordKey);
  const int facilityPage = facilityRecordKey.Family() == 0x28   ? 0x68
                           : facilityRecordKey.Family() == 0x29 ? 0x69
                           : facilityRecordKey.Family() == 0x2a ? 0x6a
                           : facilityRecordKey.Family() == 0x2c ? 0x6c
                           : facilityRecordKey.Family() == 0x2d ? 0x6b
                                                                : 0;
  RequireAiMenu(facilityPage != 0, "transit facility belongs to a facilities page");
  SystemWindow facilityPanel(app.m_instance, 100, 70, 235, 331, app.m_strategicWindow, 0xfff3, facilitySystem);
  RequireAiMenu(facilityPanel.Create(0) != 0, "transit facilities window created");
  facilityPanel.m_tabs->SelectTab(facilityPage, 1);
  RequireAiMenu(facilityPanel.m_page == facilityPage, "transit facilities page selected");
  facilityPanel.Show(SW_SHOW);
  reb::compat::UpdateWindow(app.m_strategicWindow->m_hwnd);
  auto repaintTree = [](auto&& self, CoolWindow* window) -> void {
    reb::compat::InvalidateRect(window->m_hwnd, nullptr, TRUE);
    reb::compat::UpdateWindow(window->m_hwnd);
    for (const auto& child : window->m_children.Entries()) {
      self(self, child.get());
    }
  };
  repaintTree(repaintTree, &facilityPanel);
  reb::compat::InvalidateRect(facilityPanel.m_tabs->m_hwnd, nullptr, TRUE);
  reb::compat::UpdateWindow(facilityPanel.m_tabs->m_hwnd);
  for (const auto& tab : facilityPanel.m_tabs->m_children.Entries()) {
    reb::compat::InvalidateRect(tab->m_hwnd, nullptr, TRUE);
    reb::compat::UpdateWindow(tab->m_hwnd);
  }
  SaveHdReviewFrame(std::filesystem::u8path(reviewDirectory) / "facility-window-transit.bmp");
  reb::compat::DestroyWindow(facilityPanel.m_hwnd);

  const auto checkTransitFamily = [&](FamilyRange range, const char* description) {
    GameObj* object = nullptr;
    std::unique_ptr<DibBitmap> foreground;
    std::unique_ptr<DibBitmap> stationary;
    std::unique_ptr<DibBitmap> transit;
    for (auto* candidate = GetFirstObjectInRangeForSide(app.m_strategicWindow->m_side, &range, 0); candidate;
         candidate = static_cast<GameObj*>(candidate)->FindNextSibling(&range, 0)) {
      auto* gameObject = static_cast<GameObj*>(candidate);
      if (!gameObject->m_record || gameObject->GetRecordTypeBits() == 0) {
        continue;
      }
      ObjectKey candidateKey;
      gameObject->GetKey(&candidateKey);
      std::unique_ptr<DibBitmap> candidateForeground(
          CreateObjectPicture(CoolAppGetResourceModule(10), &candidateKey, gameObject, 0, 0));
      if (!candidateForeground) {
        continue;
      }
      const uint32_t flags = gameObject->m_stateFlags;
      gameObject->m_stateFlags = (flags & ~(strategy_ui::kEnrouteState | 0x8u | 0x200u)) | 0x5u;
      std::unique_ptr<DibBitmap> candidateStationary(
          CreateObjectPicture(CoolAppGetResourceModule(10), &candidateKey, gameObject, 0, 1));
      gameObject->m_stateFlags |= strategy_ui::kEnrouteState;
      std::unique_ptr<DibBitmap> candidateTransit(
          CreateObjectPicture(CoolAppGetResourceModule(10), &candidateKey, gameObject, 0, 1));
      gameObject->m_stateFlags = flags;
      if (candidateStationary && candidateTransit && !candidateTransit->m_hdPixels.empty()) {
        object = gameObject;
        foreground = std::move(candidateForeground);
        stationary = std::move(candidateStationary);
        transit = std::move(candidateTransit);
        break;
      }
    }
    RequireAiMenu(object != nullptr, description);
    RequireAiMenu(stationary != nullptr, "stationary transit comparison picture exists");
    RequireAiMenu(transit != nullptr, "en-route transit comparison picture exists");
    RequireAiMenu(foreground != nullptr, "undecorated transit foreground picture exists");
    RequireAiMenu(!transit->m_hdPixels.empty(), "en-route transit picture has HD detail");
    RequireAiMenu(stationary->Width() == transit->Width() && stationary->Height() == transit->Height(),
                  "stationary and en-route transit pictures share dimensions");
    requireOpaquePixelsPreserved(*stationary, *transit, *foreground, description);
    const char* fileName = range.m_first == 0x20 ? "defense-transit.bmp" : "shield-transit.bmp";
    saveComparison(std::filesystem::u8path(reviewDirectory) / fileName, *stationary, *transit);
  };
  checkTransitFamily({0x20, 0x22}, "defense transit preserves opaque foreground pixels");
  checkTransitFamily({0x22, 0x28}, "shield transit preserves opaque foreground pixels");
}

void CheckMissionCharacterEta(RebellionApplication& app)
{
  const char* characterTransitReviewDirectory = std::getenv("REB_CHARACTER_TRANSIT_REVIEW_DIR");
  const bool reviewTransit = characterTransitReviewDirectory && *characterTransitReviewDirectory;
  auto* galaxy = app.m_strategicWindow;
  GameObjNotifier* host = nullptr;
  Role* character = nullptr;
  for (auto* system = GetFirstObjectInFamilies_90_98(galaxy->m_side); system && !character;
       system = static_cast<GameObj*>(system)->FindNextSibling(&family::kSystems, 1)) {
    RoleIterator roles(system, 3);
    for (roles.Begin(); roles.Current(); roles.Advance()) {
      auto* role = static_cast<Role*>(roles.Current());
      ObjectKey recordKey;
      GetObjectRecordKey(role, &recordKey);
      if (role->ControlSide() == galaxy->m_side && role->m_missionKey.IsSet()
          && !(role->m_flags78 & role_flag::kOnHiddenMission)
          && (!reviewTransit || (recordKey.m_value != 0x3c000003 && recordKey.m_value != 0x3c000005))) {
        host = system;
        character = role;
        break;
      }
    }
  }
  RequireAiMenu(character != nullptr, "mission character available for ETA check");
  std::string labelName = "Luke Skywalker";
  struct RestoreTravel {
    Role& character;
    uint32_t flags;
    int eta;
    ObjectKey departure;
    std::string* name;
    ~RestoreTravel()
    {
      character.m_stateFlags = flags;
      character.m_eta = eta;
      character.m_departureKey = departure;
      character.m_name = name;
    }
  } restore{*character, character->m_stateFlags, character->m_eta, character->m_departureKey, character->m_name};
  character->m_name = &labelName;
  host->GetKey(&character->m_departureKey);
  character->m_stateFlags |= strategy_ui::kEnrouteState;
  character->m_eta = 80641;
  SystemMissionsWindow panel(app.m_instance, 20, 20, galaxy, 0xfff2, host);
  RequireAiMenu(panel.Create(0) != 0, "mission panel created for ETA check");
  auto* mission = static_cast<ObjectListItem*>(panel.m_missions->FindById(character->m_missionKey.Id()));
  RequireAiMenu(mission != nullptr, "character mission appears in mission panel");
  panel.SelectMission(mission, 1);
  const auto selectCharacterTab = [&] {
    panel.FillCharacterList((character->m_flags78 & role_flag::kDecoy) ? 0x18 : 0x17, 1);
  };
  selectCharacterTab();
  const auto characterRow = [&] {
    auto* row = panel.m_characters->FindById(character->m_id);
    RequireAiMenu(row != nullptr, "character appears in mission panel");
    return row;
  };
  const std::string name = character->GetName()->c_str();
  RequireAiMenu(characterRow()->m_text == name + " (ETA 80641)", "mission panel shows travel ETA on opening");
  auto* list = panel.m_characterList;
  HDC dc = reb::compat::GetDC(list->m_hwnd);
  list->SetFont(characterRow()->m_fontSlot);
  list->SelectCurrentFont(dc);
  for (const std::string label : {characterRow()->m_text, std::string("Drayson (ETA 80641)")}) {
    RECT measured{0, 0, list->m_itemWidth - 4, 0};
    reb::compat::DrawTextA(dc, label.c_str(), -1, &measured, DT_WORDBREAK | DT_CALCRECT);
    RequireAiMenu(measured.right <= list->m_itemWidth - 4
                      && measured.bottom + characterRow()->m_textOffsetY <= list->m_itemHeight,
                  "mission name and ETA fit below portrait within row");
  }
  list->RestoreFont(dc);
  reb::compat::ReleaseDC(list->m_hwnd, dc);
  ObjectKey key;
  character->GetKey(&key);
  character->m_eta = 80645;
  panel.OnObjectUpdated(&key, character);
  selectCharacterTab();
  RequireAiMenu(characterRow()->m_text == name + " (ETA 80645)", "mission panel updates changed travel ETA");
  if (reviewTransit) {
    const std::filesystem::path output = std::filesystem::u8path(characterTransitReviewDirectory);
    std::filesystem::create_directories(output);
    reb::compat::ShowWindow(panel.m_hwnd, SW_SHOW);
    auto repaintMissionPanel = [&] {
      reb::compat::InvalidateRect(panel.m_hwnd, nullptr, TRUE);
      reb::compat::UpdateWindow(panel.m_hwnd);
      reb::compat::InvalidateRect(list->m_hwnd, nullptr, TRUE);
      reb::compat::UpdateWindow(list->m_hwnd);
    };
    repaintMissionPanel();
    SaveHdReviewFrame(output / "character-transit-enroute.bmp");
  }
  const auto capture = [&](const std::string& suffix) {
    const char* path = std::getenv("REB_MISSION_ETA_IMAGE");
    if (!path) {
      return;
    }
    reb::compat::ShowWindow(panel.m_hwnd, SW_SHOW);
    reb::compat::UpdateWindow(galaxy->m_hwnd);
    reb::compat::UpdateWindow(panel.m_hwnd);
    reb::compat::UpdateWindow(panel.m_missionList->m_hwnd);
    reb::compat::UpdateWindow(panel.m_tabs->m_hwnd);
    reb::compat::UpdateWindow(list->m_hwnd);
    const auto screen = minwin_gdi::DeviceScreen(minwin_gdi::DeviceScale());
    uint32_t palette[256];
    minwin_gdi::CurrentPalette(palette, nullptr);
    std::ofstream output(std::string(path) + suffix, std::ios::binary);
    output << "P6\n" << screen.width << ' ' << screen.height << "\n255\n";
    for (int y = 0; y < screen.height; ++y) {
      for (int x = 0; x < screen.width; ++x) {
        const auto rgb = palette[screen.bits[y * screen.pitch + x]];
        const char bytes[] = {static_cast<char>(rgb >> 16), static_cast<char>(rgb >> 8), static_cast<char>(rgb)};
        output.write(bytes, 3);
      }
    }
    RequireAiMenu(output.good(), "mission ETA screenshot written");
  };
  capture("");
  character->m_stateFlags &= ~strategy_ui::kEnrouteState;
  panel.OnObjectUpdated(&key, character);
  selectCharacterTab();
  RequireAiMenu(characterRow()->m_text == name, "mission panel removes ETA on arrival");
  if (reviewTransit) {
    reb::compat::InvalidateRect(panel.m_hwnd, nullptr, TRUE);
    reb::compat::UpdateWindow(panel.m_hwnd);
    reb::compat::InvalidateRect(list->m_hwnd, nullptr, TRUE);
    reb::compat::UpdateWindow(list->m_hwnd);
    SaveHdReviewFrame(std::filesystem::u8path(characterTransitReviewDirectory) / "character-transit-stationary.bmp");
  }
  capture(".arrived.ppm");
  reb::compat::DestroyWindow(panel.m_hwnd);
}

void CheckMissionWithDecoys(RebellionApplication& app)
{
  auto* galaxy = app.m_strategicWindow;
  const int side = galaxy->m_side;
  GameObjNotifier* host = nullptr;
  GameObjNotifier* otherHost = nullptr;
  Character* agent = nullptr;
  Character* peer = nullptr;
  host = GetFirstObjectInFamilies_90_98(side);
  for (auto* object = GetFirstObjectInRangeForSide(side, &family::kCharacters, 0); object;
       object = static_cast<GameObj*>(object)->FindNextSibling(&family::kCharacters, 0)) {
    auto* candidate = static_cast<Character*>(object);
    if (candidate->ControlSide() != static_cast<uint32_t>(side)) {
      continue;
    }
    if (!agent) {
      agent = candidate;
    } else {
      peer = candidate;
      break;
    }
  }
  for (auto* system = GetFirstObjectInFamilies_90_98(side); system;
       system = static_cast<GameObj*>(system)->FindNextSibling(&family::kSystems, 1)) {
    if (system != host) {
      otherHost = system;
      break;
    }
  }
  RequireAiMenu(agent && peer && host && otherHost, "mission decoy smoke has two characters and two systems");

  struct CharacterState {
    Character& character;
    uint32_t flags;
    uint32_t stateFlags;
    uint32_t roleFlags;
    uint32_t characterFlags;
    int16_t injury;
    int16_t commandKind;
    ObjectKey missionKey;
    GameObjNotifier* parent;
    ~CharacterState()
    {
      character.m_flags = flags;
      character.m_stateFlags = stateFlags;
      character.m_flags78 = roleFlags;
      character.m_flagsAc = characterFlags;
      character.m_injury = injury;
      character.m_commandKind = commandKind;
      character.m_missionKey = missionKey;
      character.m_parent = parent;
    }
  } restoreAgent{*agent,          agent->m_flags,       agent->m_stateFlags, agent->m_flags78, agent->m_flagsAc,
                 agent->m_injury, agent->m_commandKind, agent->m_missionKey, agent->m_parent},
      restorePeer{*peer,          peer->m_flags,       peer->m_stateFlags, peer->m_flags78, peer->m_flagsAc,
                  peer->m_injury, peer->m_commandKind, peer->m_missionKey, peer->m_parent};

  const auto makeAvailable = [side](Character& character, GameObjNotifier* parent) {
    character.m_parent = parent;
    character.m_flags = (character.m_flags & ~0xc0u) | (static_cast<uint32_t>(side) << 6);
    character.m_stateFlags = (character.m_stateFlags & ~(0x10u | 0x8u)) | 0x47u;
    character.m_flags78 &= ~(role_flag::kOnMission | role_flag::kMovingBetweenMissions);
    character.m_flagsAc &= ~1u;
    character.m_injury = 0;
    character.m_commandKind = 0;
    character.m_missionKey.Reset();
  };
  makeAvailable(*agent, host);
  makeAvailable(*peer, host);

  ObjectKey agentKey;
  agent->GetKey(&agentKey);
  ObjectKeyValueSet team;
  team.AddKeyValue(&agentKey, 0);
  MissionOrder240 order;
  order.m_side = side;
  order.SetTeam(&team);
  RequireAiMenu(order.PopulateCharacterDecoys(), "mission decoy helper accepts an available character agent");
  ObjectKey peerKey;
  peer->GetKey(&peerKey);
  RequireAiMenu(order.m_characters.Find(&peerKey) != nullptr, "healthy same-system character is listed as a decoy");
  RequireAiMenu(order.m_characters.Find(&agentKey) == nullptr && order.GetTeam()->Count() == 1,
                "agent stays the only mission team member and is not its own decoy");

  const auto expectExcluded = [&](const char* label) {
    RequireAiMenu(order.PopulateCharacterDecoys(), "mission decoy helper remains available during eligibility checks");
    RequireAiMenu(order.m_characters.Find(&peerKey) == nullptr, label);
  };
  peer->m_commandKind = 1;
  expectExcluded("character on command duty is excluded from decoys");
  peer->m_commandKind = 0;
  peer->m_injury = 1;
  expectExcluded("injured character is excluded from decoys");
  peer->m_injury = 0;
  peer->m_flagsAc |= 1u;
  expectExcluded("captured character is excluded from decoys");
  peer->m_flagsAc &= ~1u;
  peer->m_stateFlags |= 0x10u;
  expectExcluded("transiting character is excluded from decoys");
  peer->m_stateFlags &= ~0x10u;
  peer->m_flags78 |= role_flag::kOnMission;
  expectExcluded("character already on a mission is excluded from decoys");
  peer->m_flags78 &= ~role_flag::kOnMission;
  peer->m_flags = (peer->m_flags & ~0xc0u) | (static_cast<uint32_t>(side == 1 ? 2 : 1) << 6);
  expectExcluded("foreign-side character is excluded from decoys");
  peer->m_flags = (peer->m_flags & ~0xc0u) | (static_cast<uint32_t>(side) << 6);
  peer->m_parent = otherHost;
  expectExcluded("character at another planet is excluded from decoys");
  peer->m_parent = host;

  {
    Fleet fleet;
    Fleet otherFleet;
    CapitalShip firstShip;
    CapitalShip secondShip;
    firstShip.m_parent = &fleet;
    secondShip.m_parent = &fleet;
    fleet.m_parent = host;
    otherFleet.m_parent = host;
    agent->m_parent = &firstShip;
    peer->m_parent = &secondShip;
    RequireAiMenu(order.PopulateCharacterDecoys() && order.m_characters.Find(&peerKey),
                  "characters on different ships of one fleet are included as decoys");
    peer->m_parent = host;
    expectExcluded("planet characters are excluded from a fleet mission");
    peer->m_parent = &otherFleet;
    expectExcluded("characters in a different fleet at the same planet are excluded");
    peer->m_parent = &secondShip;
    fleet.m_stateFlags |= 0x10u;
    RequireAiMenu(!order.PopulateCharacterDecoys(), "characters in an en-route fleet cannot start the shortcut");
    fleet.m_stateFlags &= ~0x10u;
    agent->m_parent = host;
    expectExcluded("fleet characters are excluded from a planet mission");
    peer->m_parent = host;
  }
  agent->m_injury = 1;
  RequireAiMenu(!order.PopulateCharacterDecoys(), "an unavailable clicked agent cannot start the shortcut");
  agent->m_injury = 0;

  class ContextMenuDialog final : public CustomDialogBox
  {
public:
    ContextMenuDialog(HINSTANCE instance, CoolWindow* parent, ObjectKey clicked, ObjectKey selected)
        : CustomDialogBox(instance, 12, 12, 100, 80, parent, 0xf301, nullptr)
        , m_clicked(clicked)
        , m_selected(selected)
    {
    }
    void GetSelection(ObjectKeyValueSet* out, CoolWindow*) override
    {
      out->AddKeyValue(&m_clicked, 0);
      out->AddKeyValue(&m_selected, 0);
    }
    ObjectKey GetKeyAt(int, int) override
    {
      return m_clicked;
    }
    ObjectKey m_clicked;
    ObjectKey m_selected;
  } dialog(app.m_instance, galaxy, agentKey, peerKey);
  RequireAiMenu(dialog.Create(0) != 0, "decoy context-menu fixture created");
  dialog.OnRightButtonUp(8, 8, &dialog);
  RequireAiMenu(dialog.m_contextCharacter.m_value == agentKey.m_value,
                "context menu remembers only the character under the click");
  const auto findCommand = [&](auto&& self, MenuItemList& items) -> MenuItem* {
    for (const auto& ownedItem : items.Entries()) {
      auto* item = static_cast<MenuItem*>(ownedItem.get());
      if (item->m_command.m_command == kMissionWithDecoysCommand) {
        return item;
      }
      if (auto* nested = self(self, item->m_subItems)) {
        return nested;
      }
    }
    return nullptr;
  };
  MenuItem* decoyItem = g_menuWindow ? findCommand(findCommand, g_menuWindow->m_items) : nullptr;
  RequireAiMenu(decoyItem != nullptr, "right-click menu contains the decoy mission entry");
  MenuRecord missionRecord = {};
  RequireAiMenu(ReadMenuRecord(0x240, 7, &missionRecord) == 0
                    && decoyItem->m_id == static_cast<uint32_t>(missionRecord.m_words[2]) + 1,
                "decoy mission entry is placed beside the ordinary mission entry");
  RequireAiMenu(*decoyItem->m_label->m_text == "Msn w/ decoys", "decoy shortcut uses the requested label");
  if (const char* reviewDirectory = std::getenv("REB_MISSION_DECOYS_REVIEW_DIR"); reviewDirectory && *reviewDirectory) {
    std::filesystem::create_directories(reviewDirectory);
    reb::compat::InvalidateRect(g_menuWindow->m_hwnd, nullptr, TRUE);
    reb::compat::UpdateWindow(g_menuWindow->m_hwnd);
    SaveHdReviewFrame(std::filesystem::u8path(reviewDirectory) / "menu.bmp");
  }
  DismissMenu();
  RequireAiMenu(galaxy->m_pendingCommand == nullptr, "no targeting command is pending before menu dispatch");
  const uint32_t oldMouseMode = galaxy->m_mouseMode;
  const HCURSOR oldCursor = galaxy->m_currentCursor;
  dialog.OnMenuCommand(kMissionWithDecoysCommand, 0, 0);
  auto* queued = static_cast<MissionOrder240*>(galaxy->m_pendingCommand);
  RequireAiMenu(queued && queued->GetTeam()->Count() == 1 && queued->GetTeam()->Find(&agentKey) != nullptr
                    && queued->GetTeam()->Find(&peerKey) == nullptr && queued->m_characters.Find(&peerKey) != nullptr,
                "decoy menu dispatch starts targeting with the clicked agent and its decoy");
  delete queued;
  galaxy->m_pendingCommand = nullptr;
  dialog.OnMenuCommand(0x240, 0, 0);
  auto* normalMission = static_cast<MissionOrder240*>(galaxy->m_pendingCommand);
  RequireAiMenu(normalMission && normalMission->GetTeam()->Count() == 2 && normalMission->m_characters.Empty(),
                "ordinary mission command keeps the selected team and starts without decoys");
  delete normalMission;
  galaxy->m_pendingCommand = nullptr;
  galaxy->m_mouseMode = oldMouseMode;
  galaxy->m_currentCursor = oldCursor;
  if (reb::compat::GetCapture() == galaxy->m_hwnd) {
    reb::compat::ReleaseCapture();
  }
  reb::compat::DestroyWindow(dialog.m_hwnd);
}

void CheckCodexOccupationThreshold()
{
  RequireAiMenu(ai::RequiredOccupationTroops(4) == 6, "Codex occupation troop target includes safety reserve");
  RequireAiMenu(!ai::CanLaunchOccupation(5, 4, true), "Codex refuses an assault below its occupation troop target");
  RequireAiMenu(ai::CanLaunchOccupation(6, 4, true), "Codex accepts an assault at its occupation troop target");
  RequireAiMenu(!ai::CanLaunchOccupation(6, 4, false), "Codex still requires ground superiority");
  RequireAiMenu(ai::RequiredOccupationTroops(-20) == 4 && ai::RequiredOccupationTroops(99) == 16,
                "Codex occupation troop target stays within planner bounds");
}

void CheckManufacturingEta(RebellionApplication& app)
{
  GalaxyWindow* galaxy = app.m_strategicWindow;
  ManuMgr* manager = nullptr;
  System* host = nullptr;
  int slot = 0;
  GameObj* product = nullptr;
  constexpr FamilyRange products[] = {{0x14, 0x1c}, {0x20, 0x22}, {0x22, 0x30}};
  constexpr int kinds[] = {0, 2, 1};
  for (GameObjNotifier* object = GetFirstObjectInFamilies_90_98(galaxy->m_side); object && !manager;
       object = static_cast<GameObj*>(object)->FindNextSibling(&family::kSystems, 1)) {
    auto* system = static_cast<System*>(object);
    for (int index = 0; index < 3; ++index) {
      auto* candidate = static_cast<ManuMgr*>(system->FindManufacturingManagerByKind(kinds[index]));
      auto* candidateProduct = static_cast<GameObj*>(GetFirstObjectInRangeForSide(galaxy->m_side, &products[index], 0));
      if (candidate && candidate->ControlSide() == static_cast<uint32_t>(galaxy->m_side)
          && candidate->HasHostFacility(0) && candidateProduct && candidateProduct->m_record) {
        product = candidateProduct;
        manager = candidate;
        host = system;
        slot = index + 1;
        break;
      }
    }
  }
  if (!manager) {
    return;
  }
  struct RestoreProduction {
    ManuMgr* manager;
    int count;
    int completed;
    int overflow;
    int required;
    NullableKey seed;
    ObjectKey product;
    ~RestoreProduction()
    {
      manager->m_remainingGameObjCount = count;
      manager->m_completedPointCount = completed;
      manager->m_overflowPointCount = overflow;
      manager->m_requiredPointCount = required;
      manager->m_seedKey = seed;
      manager->m_productKey = product;
    }
  } restore{manager,
            manager->m_remainingGameObjCount,
            manager->m_completedPointCount,
            manager->m_overflowPointCount,
            manager->m_requiredPointCount,
            manager->m_seedKey,
            manager->m_productKey};
  struct FacilityState {
    ProcFacil* master;
    ManufacturingFacility* view;
    int state;
  };
  std::vector<FacilityState> originalFacilityStates;
  NotifyContext stateContext;
  ManufacturingFacilityIterator facilities(host);
  for (facilities.Begin(); facilities.Current(); facilities.Advance()) {
    auto* view = static_cast<ManufacturingFacility*>(facilities.Current());
    if (manager->IsCompatibleFacility(static_cast<GameObj*>(view))) {
      ObjectKey key;
      view->GetKey(&key);
      auto* master = static_cast<ProcFacil*>(GetMasterInFamilies_28_30(&key));
      if (master) {
        originalFacilityStates.push_back({master, view, master->m_state});
      }
    }
  }
  struct RestoreFacilityStates {
    std::vector<FacilityState>& states;
    NotifyContext* context;
    ~RestoreFacilityStates()
    {
      for (const auto& facility : states) {
        facility.master->m_state = facility.state;
        facility.view->ApplyState(context);
      }
    }
  } restoreFacilityStates{originalFacilityStates, &stateContext};
  manager->m_seedKey.m_value = (product->m_record->m_third << 24) | (product->m_record->m_id & 0xffffff);
  product->GetKey(&manager->m_productKey);
  manager->m_remainingGameObjCount = 1;
  manager->m_completedPointCount = 0;
  manager->m_overflowPointCount = 0;
  manager->m_requiredPointCount = static_cast<SdRecord*>(product->m_record)->GetField48();
  for (const auto& facility : originalFacilityStates) {
    facility.master->m_state = ProcFacil::kStateIdle;
    facility.view->ApplyState(&stateContext);
    RequireAiMenu(facility.view->m_state == ProcFacil::kStateIdle, "idle production state reaches its UI view");
  }
  StatusCode freshStatus(1, static_cast<uint32_t>(-1));
  int freshDays = 0;
  RequireAiMenu(manager->EstimateBuildTimeFresh(&manager->m_seedKey, 1, &freshStatus, &freshDays)
                    && freshStatus.m_second == static_cast<uint32_t>(-1) && freshDays >= 0,
                "idle compatible facilities estimate a fresh build");
  for (const auto& facility : originalFacilityStates) {
    facility.master->m_state = ProcFacil::kStateWaiting;
    facility.view->ApplyState(&stateContext);
    RequireAiMenu(facility.view->m_state == ProcFacil::kStateWaiting, "material wait reaches its UI view");
  }
  std::string waitingEta;
  strategy_ui::AppendManufacturingEtaSuffix(&waitingEta, manager);
  RequireAiMenu(waitingEta == " (Waiting for materials)", "active manufacturing reports material shortage");
  for (const auto& facility : originalFacilityStates) {
    facility.master->m_state = ProcFacil::kStateProcessing;
    facility.view->ApplyState(&stateContext);
    RequireAiMenu(facility.view->m_state == ProcFacil::kStateProcessing, "resumed production reaches its UI view");
  }
  std::string resumedEta;
  strategy_ui::AppendManufacturingEtaSuffix(&resumedEta, manager);
  RequireAiMenu(resumedEta.find(" (ETA ") == 0, "manufacturing ETA resumes when facilities can process");
  std::string eta;
  strategy_ui::AppendManufacturingEtaSuffix(&eta, manager);
  RequireAiMenu(!eta.empty(), "real facility estimates next manufactured product");
  manager->m_remainingGameObjCount += 5;
  std::string longerQueueEta;
  strategy_ui::AppendManufacturingEtaSuffix(&longerQueueEta, manager);
  RequireAiMenu(eta == longerQueueEta, "manufacturing ETA is next product, independent of queue length");
  const char* reviewDirectory = std::getenv("REB_FACILITY_REVIEW_DIR");
  SystemWindow panel(app.m_instance, reviewDirectory && *reviewDirectory ? 100 : 0,
                     reviewDirectory && *reviewDirectory ? 70 : 0, 235, 331, galaxy, 0xfff2, host);
  RequireAiMenu(panel.Create(0) != 0, "manufacturing panel created");
  if (reviewDirectory && *reviewDirectory) {
    const std::filesystem::path output = std::filesystem::u8path(reviewDirectory);
    std::filesystem::create_directories(output);
    panel.Show(SW_SHOW);
    const int pages[] = {0x68, 0x69, 0x6a, 0x6b, 0x6c};
    ObjectKey systemKey;
    host->GetKey(&systemKey);
    for (const int page : pages) {
      panel.m_tabs->SelectTab(static_cast<uint32_t>(page), 1);
      reb::compat::UpdateWindow(galaxy->m_hwnd);
      for (const auto& tab : panel.m_tabs->m_children.Entries()) {
        const auto* button = static_cast<const CoolStrobeButton*>(tab.get());
        if (button->m_normalId >= 0x2850 && button->m_normalId <= 0x285e) {
          RequireAiMenu(button->m_normal && !button->m_normal->m_hdPixels.empty() && button->m_pressed
                            && !button->m_pressed->m_hdPixels.empty() && button->m_checked
                            && !button->m_checked->m_hdPixels.empty(),
                        "facility tab normal, pressed and selected artwork loads HD pixels");
        }
      }
      auto paintTree = [](auto&& self, CoolWindow* window) -> void {
        reb::compat::InvalidateRect(window->m_hwnd, nullptr, TRUE);
        reb::compat::UpdateWindow(window->m_hwnd);
        for (const auto& child : window->m_children.Entries()) {
          self(self, child.get());
        }
      };
      paintTree(paintTree, &panel);
      reb::compat::InvalidateRect(panel.m_tabs->m_hwnd, nullptr, TRUE);
      reb::compat::UpdateWindow(panel.m_tabs->m_hwnd);
      for (const auto& tab : panel.m_tabs->m_children.Entries()) {
        reb::compat::InvalidateRect(tab->m_hwnd, nullptr, TRUE);
        reb::compat::UpdateWindow(tab->m_hwnd);
      }
      SaveHdReviewFrame(output
                        / ("system-" + std::to_string(systemKey.Id()) + "-facility-" + std::to_string(page) + ".bmp"));
    }
  }
  CoolLabel* label = panel.Slot(slot).m_countLabel;
  RequireAiMenu(label->m_fontSlot == strategy_ui::kEtaFontSlot, "manufacturing ETA uses small text");
  RequireAiMenu(std::string(label->GetText()).ends_with(eta), "production quantity displays next completion day");
  manager->m_completedPointCount = manager->m_requiredPointCount;
  manager->m_overflowPointCount = 0;
  panel.SetSlotProgress(slot, manager, manager->m_completedPointCount, manager->m_requiredPointCount);
  const std::string completeEta = " (ETA " + std::to_string(CallGameHook2B08()) + ")";
  RequireAiMenu(std::string(label->GetText()).ends_with(completeEta), "manufacturing progress refreshes ETA");
  manager->m_remainingGameObjCount = 0;
  panel.SetSlotProgress(slot, manager, 0, manager->m_requiredPointCount);
  RequireAiMenu(label->GetText()[0] == '\0', "idle manufacturing clears quantity and ETA");
  reb::compat::DestroyWindow(panel.m_hwnd);
}

void CheckProductionDestinationMenu(RebellionApplication& app)
{
  auto require = [](bool condition, const char* check) {
    if (!condition) {
      throw std::runtime_error(std::string("Native UI smoke: production destination: ") + check);
    }
  };
  GalaxyWindow* galaxy = app.m_strategicWindow;
  auto dispatchInput = []() {
    MSG message;
    for (int count = 0; count < 1000 && reb::compat::PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE); ++count) {
      reb::compat::TranslateMessage(&message);
      reb::compat::DispatchMessageA(&message);
    }
  };
  auto click = [&](HWND window, int x, int y, int button) {
    POINT point{x, y};
    reb::compat::ClientToScreen(window, &point);
    static unsigned time = 3000000;
    time += 1000;
    minwin::InputMouseMotion(point.x, point.y);
    dispatchInput();
    minwin::InputMouseButton(button, true, time, point.x, point.y);
    dispatchInput();
    minwin::InputMouseButton(button, false, time + 50, point.x, point.y);
    dispatchInput();
  };
  auto mapClickTargets = [&](const ObjectKey& key) {
    auto* marker = galaxy->m_systemItems.FindById(key.Id());
    if (!marker) {
      return false;
    }
    const POINT point{(marker->m_rect.left + marker->m_rect.right) / 2,
                      (marker->m_rect.top + marker->m_rect.bottom) / 2};
    for (const auto& entry : galaxy->m_systemItems.Entries()) {
      auto* candidate = static_cast<ObjectListItem*>(entry.get());
      const bool visible
          = (candidate->m_state.m_value & 0x80)
                ? galaxy->m_gidIcons != nullptr
                : candidate->m_image != nullptr || ((candidate->m_state.m_value & 8) && galaxy->m_gidIcons2);
      if (visible && reb::compat::PtInRect(&candidate->m_rect, point)) {
        ObjectKey hitKey;
        candidate->GetKey(&hitKey);
        return hitKey.m_value == key.m_value;
      }
    }
    return false;
  };
  const int previousDisplayMode = galaxy->m_displayMode;
  const auto previousSelection = galaxy->m_selectedIds;
  const ResourceStringId previousTitle = galaxy->m_activeTitleStringId;
  const ResourceStringId previousPendingTitle = galaxy->m_titleStringId;
  galaxy->SetDisplayMode(0x11, nullptr);
  constexpr int kinds[] = {0, 2, 1};
  for (int iteration = 0; iteration < 6; ++iteration) {
    const int index = iteration % 3;
    const bool targetMap = iteration >= 3;
    System* host = nullptr;
    ManuMgr* manager = nullptr;
    for (auto* object = GetFirstObjectInFamilies_90_98(galaxy->m_side); object;
         object = static_cast<GameObj*>(object)->FindNextSibling(&family::kSystems, 1)) {
      auto* system = static_cast<System*>(object);
      auto* candidate = static_cast<ManuMgr*>(system->FindManufacturingManagerByKind(kinds[index]));
      if (candidate && candidate->ControlSide() == static_cast<uint32_t>(galaxy->m_side)) {
        host = system;
        manager = candidate;
        if (candidate->m_remainingGameObjCount > 0) {
          break;
        }
      }
    }
    require(host && manager, "production destination manager exists");
    constexpr uint32_t panelId = 0xffd9;
    auto* panel = new SystemWindow(app.m_instance, 20, 20, 235, 331, galaxy, panelId, host);
    require(panel->Create(0) != 0, "production destination panel created");
    galaxy->m_children.Insert(panel);
    panel->Show(SW_SHOW);
    SystemSlot& slot = panel->Slot(index + 1);
    click(panel->m_hwnd, slot.m_rect.left + 10, slot.m_rect.top + 10, 2);
    require(g_menuWindow != nullptr, "right click opens production menu");
    auto findDestination = [](auto&& self, PopupMenuWindow* menu) -> std::pair<PopupMenuWindow*, MenuItem*> {
      for (const auto& entry : menu->m_items.Entries()) {
        auto* item = static_cast<MenuItem*>(entry.get());
        if (item->m_command.GetAvailable() == 0x214) {
          return {menu, item};
        }
        if (item->m_subMenu) {
          auto found = self(self, static_cast<PopupMenuWindow*>(item->m_subMenu));
          if (found.second) {
            return found;
          }
        }
      }
      return {};
    };
    auto [menu, item] = findDestination(findDestination, g_menuWindow);
    require(item != nullptr, "production menu offers enabled Destination");
    menu->Show(SW_SHOW);
    click(menu->m_hwnd, item->m_area.left + 2, item->m_area.top + 2, 1);
    require(galaxy->m_mouseMode == 2 && galaxy->m_pendingCommand, "Destination menu starts target selection");
    require(reb::compat::GetCapture() == galaxy->m_hwnd, "destination targeting retains mouse capture");
    auto* pending = static_cast<OrderBase*>(galaxy->m_pendingCommand);
    ObjectKey managerKey;
    manager->GetKey(&managerKey);
    require(pending->GetKind() == 0x214 && pending->GetTeam()->Find(&managerKey),
            "destination target selection preserves selected production manager");
    System* destination = nullptr;
    ObjectKey destinationKey;
    for (auto* object = GetFirstObjectInFamilies_90_98(galaxy->m_side); object;
         object = static_cast<GameObj*>(object)->FindNextSibling(&family::kSystems, 1)) {
      if (object->ControlSide() != static_cast<uint32_t>(galaxy->m_side)) {
        continue;
      }
      object->GetKey(&destinationKey);
      if (targetMap && !mapClickTargets(destinationKey)) {
        continue;
      }
      DestinationOrder trial;
      trial.m_side = galaxy->m_side;
      trial.SetTeam(pending->GetTeam());
      trial.SetTarget(&destinationKey);
      StatusCode status;
      if (trial.CheckValid(&status)) {
        destination = static_cast<System*>(object);
        break;
      }
    }
    require(destination != nullptr, "valid production destination exists");
    constexpr uint32_t targetPanelId = 0xffda;
    auto* targetPanel = new SectorWindow(app.m_instance, 270, 20, 235, 360, galaxy, galaxy->GetBackground(),
                                         targetPanelId, destination->m_parent);
    require(targetPanel->Create(0) != 0, "production destination target panel created");
    galaxy->m_children.Insert(targetPanel);
    targetPanel->Show(SW_SHOW);
    HumanPlayer* human = GetGameSession()->m_human;
    const int messageId = human->m_nextMessageId;
    ObjectListItem* targetPlanet = nullptr;
    for (const auto& entry : targetPanel->m_systems.Entries()) {
      auto* planet = static_cast<ObjectListItem*>(entry.get());
      ObjectKey planetKey;
      planet->GetKey(&planetKey);
      if (planetKey.m_value == destinationKey.m_value) {
        targetPlanet = planet;
        break;
      }
    }
    require(targetPlanet != nullptr, "destination planet marker exists");
    if (targetMap) {
      std::vector<CoolWindow*> visibleChildren;
      for (const auto& entry : galaxy->m_children.Entries()) {
        auto* child = static_cast<CoolWindow*>(entry.get());
        if (reb::compat::IsWindowVisible(child->m_hwnd)) {
          visibleChildren.push_back(child);
          child->Hide();
        }
      }
      auto* marker = galaxy->m_systemItems.FindById(destinationKey.Id());
      const int mapX = (marker->m_rect.left + marker->m_rect.right) / 2;
      const int mapY = (marker->m_rect.top + marker->m_rect.bottom) / 2;
      if (index == 0) {
        galaxy->SetDisplayMode(0x80, nullptr);
        click(galaxy->m_hwnd, mapX, mapY, 1);
        require(human->m_nextMessageId == messageId && !galaxy->m_pendingCommand,
                "disabled galaxy display does not target stale planet markers");
        galaxy->SetDisplayMode(0x11, nullptr);
        ObjectKeyValueSet team;
        team.AddKeyValue(&managerKey, 0);
        ObjectKey noTarget;
        galaxy->BeginTargeting(human->CreateOrder(0x214, &team, &noTarget));
      }
      click(galaxy->m_hwnd, mapX, mapY, 1);
      for (auto* child : visibleChildren) {
        child->Show(SW_SHOW);
      }
    } else {
      click(targetPanel->m_hwnd, (targetPlanet->m_rect.left + targetPlanet->m_rect.right) / 2,
            (targetPlanet->m_rect.top + targetPlanet->m_rect.bottom) / 2, 1);
    }
    require(galaxy->m_mouseMode == 1 && !galaxy->m_pendingCommand,
            "target click completes production destination selection");
    std::unique_ptr<MessageBase> submitted(human->m_pending.Detach(messageId));
    human->m_nextMessageId = messageId;
    require(submitted && submitted->GetKind() == 0x214, "target click submits production destination order");
    ObjectKey selectedTarget;
    static_cast<OrderBase*>(submitted.get())->GetTarget(&selectedTarget);
    require(selectedTarget.m_value == destinationKey.m_value, "destination order targets clicked system");
    auto* master = static_cast<ManuMgr*>(GetMasterInFamilies_a0_b0(&managerKey));
    const ObjectKey previousTarget = master->m_targetKey;
    const int previousProgress = master->m_completedPointCount;
    ObjectKeyPairSet previousParents;
    for (const auto& entry : master->GetQueue()->Entries()) {
      ObjectKey productKey;
      static_cast<ObjectKeyNode*>(entry.get())->GetKey(&productKey);
      auto* product = static_cast<GameObj*>(GetMasterInFamilies_01_ff(&productKey));
      ObjectKey parentKey;
      require(product && product->GetValidParentKey(&parentKey), "queued product has a parent");
      previousParents.AddPair(&parentKey, &productKey);
    }
    auto* order = static_cast<OrderBase*>(submitted.get());
    StatusCode status;
    require(order->CheckValid(&status), "submitted destination remains valid");
    for (const auto& entry : order->m_commands->Entries()) {
      auto* command = static_cast<DestinationCommand*>(entry.get());
      command->Execute();
      for (const auto& pair : command->m_pairs.TypedEntries()) {
        auto* product = static_cast<GameObj*>(GetMasterInFamilies_01_ff(&pair.m_second));
        ObjectKey parentKey;
        require(product && product->GetValidParentKey(&parentKey) && parentKey.m_value == pair.m_key.m_value,
                "destination command moves every queued product to its assigned parent");
      }
    }
    require(master->m_targetKey.m_value == destinationKey.m_value, "production destination matches submitted order");
    require(master->m_completedPointCount == previousProgress, "changing destination preserves production progress");
    NotifyContext context;
    require(master->ReparentQueue(previousParents, &previousTarget, &context) != 0,
            "production destination restored after check");
    galaxy->DestroyChildById(targetPanelId);
    galaxy->DestroyChildById(panelId);
  }
  galaxy->m_titleStringId = previousTitle;
  galaxy->SetDisplayMode(previousDisplayMode, &previousSelection);
  galaxy->m_titleStringId = previousPendingTitle;
}

void CheckSideEconomyBehavior()
{
  Side* side = nullptr;
  Side* sideView = nullptr;
  int sideNumber = 0;
  ProcFacil* facility = nullptr;
  ManufacturingFacility* facilityView = nullptr;
  for (int candidateSideNumber = 1; candidateSideNumber <= 2 && !facility; ++candidateSideNumber) {
    const ObjectKey sideKey = ObjectKey::Make(0xf3, candidateSideNumber == 1 ? 0xf : 0x10);
    auto* candidateSide = static_cast<Side*>(GetMasterObject(&sideKey));
    if (!candidateSide || candidateSide->m_maintDeficit != 0 || !candidateSide->m_manufacturerQueue) {
      continue;
    }
    constexpr int kinds[] = {0, 2, 1};
    for (auto* system = GetFirstObjectInRangeForSide(candidateSideNumber, &family::kSystems, 0); system && !facility;
         system = static_cast<GameObj*>(system)->FindNextSibling(&family::kSystems, 0)) {
      for (int kind : kinds) {
        auto* manager = static_cast<ManuMgr*>(static_cast<System*>(system)->FindManufacturingManagerByKind(kind));
        if (!manager || manager->ControlSide() != static_cast<uint32_t>(candidateSideNumber)) {
          continue;
        }
        ManufacturingFacilityIterator facilities(system);
        for (facilities.Begin(); facilities.Current(); facilities.Advance()) {
          auto* view = static_cast<ManufacturingFacility*>(facilities.Current());
          if (!manager->IsCompatibleFacility(static_cast<GameObj*>(view))
              || (view->m_procFlags & ProcFacil::kSuspended)) {
            continue;
          }
          ObjectKey key;
          view->GetKey(&key);
          auto* master = static_cast<ProcFacil*>(GetMasterInFamilies_28_30(&key));
          if (master && master->m_data && !(master->m_procFlags & ProcFacil::kSuspended)
              && (master->m_state == ProcFacil::kStateWaiting || master->m_state == ProcFacil::kStateIdle)) {
            side = candidateSide;
            sideNumber = candidateSideNumber;
            facility = master;
            facilityView = view;
            break;
          }
        }
        if (facility) {
          break;
        }
      }
    }
  }
  RequireAiMenu(side && facility && facilityView,
                "economy smoke has an in-service waiting or idle manufacturing facility");
  sideView = static_cast<Side*>(GetSideObject(sideNumber, sideNumber));
  RequireAiMenu(sideView != nullptr, "economy smoke has a side view for the selected faction");

  ProcFacilData* facilityData = static_cast<ProcFacilData*>(facility->m_data);
  RequireAiMenu(facilityData != nullptr, "economy smoke facility has processing data");
  NotifyContext context;
  struct RestoreEconomyState {
    Side* side;
    Side* sideView;
    ProcFacil* facility;
    ManufacturingFacility* facilityView;
    ProcFacilData* facilityData;
    NotifyContext* context;
    int refinedMaterials;
    int waitingManufacturers;
    std::vector<std::unique_ptr<KeyQueueNode>> manufacturerQueue;
    int facilityState;
    int facilityProgress;
    uint32_t facilityFlags;
    int facilityCycle;
    TrackedStat processingStat;
    int maintRequired;
    int maintRequiredB;
    int maintDeficit;
    ~RestoreEconomyState()
    {
      if (facility->m_procFlags & ProcFacil::kPointPresent) {
        facility->SetPointPresent(0, context);
      }
      if (facility->m_state != facilityState) {
        facility->SetState(facilityState, context);
      }
      facility->m_state = facilityState;
      facility->m_etc = facilityProgress;
      facility->m_procFlags = facilityFlags;
      facilityData->m_cycleCount = facilityCycle;
      facilityData->m_processingStat = processingStat;
      facilityView->ApplyAll(context);

      side->m_manufacturerQueue->Clear();
      for (auto& node : manufacturerQueue) {
        side->m_manufacturerQueue->Add(std::move(node));
      }
      side->m_waitingManufacturers = waitingManufacturers;
      side->m_refinedMaterials = refinedMaterials;
      sideView->ApplyRefinedMaterials(context);
      sideView->ApplyWaitingManufacturers(context);
      side->SetMaintRequired(maintRequired, context);
      side->SetMaintRequiredB(maintRequiredB, context);
      side->SetMaintDeficit(maintDeficit, context);
    }
  } restore{side,
            sideView,
            facility,
            facilityView,
            facilityData,
            &context,
            side->m_refinedMaterials,
            side->m_waitingManufacturers,
            {},
            facility->m_state,
            facility->m_etc,
            facility->m_procFlags,
            facilityData->m_cycleCount,
            facilityData->m_processingStat,
            side->m_maintRequired,
            side->m_maintRequiredB,
            side->m_maintDeficit};

  while (KeyQueueNode* node = side->m_manufacturerQueue->Dequeue()) {
    restore.manufacturerQueue.emplace_back(node);
  }
  side->m_waitingManufacturers = 0;
  RequireAiMenu(sideView->ApplyWaitingManufacturers(&context), "isolate the material-wait queue fixture");

  side->m_refinedMaterials = 0;
  RequireAiMenu(sideView->ApplyRefinedMaterials(&context), "zero refined stock for shortage scenario");
  RequireAiMenu(facility->SetPointPresent(0, &context), "clear facility input before shortage scenario");
  RequireAiMenu(facility->SetState(ProcFacil::kStateWaiting, &context), "put facility into input-waiting state");
  RequireAiMenu(side->AcquireRefinedMaterial(facility, &context), "refined material request is accepted");
  RequireAiMenu(side->m_waitingManufacturers == 1 && side->m_manufacturerQueue->Entries().size() == 1
                    && facility->m_state == ProcFacil::kStateWaiting
                    && (facility->m_procFlags & ProcFacil::kPointPresent) == 0,
                "zero refined stock queues manufacturing without advancing production");

  side->m_refinedMaterials = 1;
  int more = 0;
  RequireAiMenu(side->ServeManufacturerQueue(&more, &context), "refilled material serves manufacturer queue");
  RequireAiMenu(side->m_refinedMaterials == 0 && side->m_waitingManufacturers == 0
                    && side->m_manufacturerQueue->Entries().empty() && facility->m_state == ProcFacil::kStateProcessing
                    && (facility->m_procFlags & ProcFacil::kPointPresent) != 0,
                "refined stock resumes queued production and is consumed");

  RequireAiMenu(side->m_maintDeficit == 0, "maintenance test starts without a deficit");
  RequireAiMenu(side->m_maintState.m_first < static_cast<uint32_t>(std::numeric_limits<int>::max()),
                "maintenance capacity has room for a test demand");
  const int excessDemand = static_cast<int>(side->m_maintState.m_first) + 1;
  RequireAiMenu(side->SetMaintRequired(excessDemand, &context), "maintenance demand can exceed capacity");
  RequireAiMenu(side->m_maintDeficit != 0, "maintenance deficit is raised above capacity");
  RequireAiMenu(side->SetMaintRequired(restore.maintRequired, &context), "maintenance demand is restored");
  RequireAiMenu(side->SetMaintRequiredB(restore.maintRequiredB, &context), "secondary maintenance demand is restored");
  RequireAiMenu(side->m_maintDeficit == restore.maintDeficit, "maintenance deficit clears after demand is restored");
}

void CheckSpeedShortcuts(RebellionApplication& app)
{
  GameSession* session = GetGameSession();
  RequireAiMenu(session && session->m_human, "human speed controller available");
  GalaxyWindow* galaxy = app.m_strategicWindow;
  const int originalSpeed = session->m_human->m_speed;
  const int originalNextId = session->m_human->m_nextMessageId;
  constexpr int ticksBySpeed[] = {600, 60, 12, 4};
  for (int speed = 1; speed <= 4; ++speed) {
    const int messageId = session->m_human->m_nextMessageId;
    galaxy->WindowProc(WM_KEYDOWN, '0' + speed, 0);
    RequireAiMenu(session->m_human->m_speed == speed, "number key selects speed");
    RequireAiMenu(g_menuWindow == nullptr, "number key does not open developer menu");
    std::unique_ptr<MessageBase> message(session->m_human->m_pending.Detach(messageId));
    RequireAiMenu(message && message->GetKind() == 0x280, "speed shortcut submits speed order");
    auto* order = static_cast<NetMsg280*>(message.get());
    RequireAiMenu(order->m_side == session->GetHumanSide() && order->m_value == ticksBySpeed[speed - 1],
                  "speed shortcut sends correct interval and side");
    const int nextId = session->m_human->m_nextMessageId;
    galaxy->WindowProc(WM_KEYDOWN, '0' + speed, static_cast<LPARAM>(1u << 30));
    RequireAiMenu(session->m_human->m_nextMessageId == nextId, "held speed key does not resubmit");
  }
  {
    OverrideEnhancementsForProcess(false);
    const int idBefore = session->m_human->m_nextMessageId;
    const int speedBefore = session->m_human->m_speed;
    galaxy->WindowProc(WM_KEYDOWN, speedBefore == 1 ? '2' : '1', 0);
    OverrideEnhancementsForProcess(true);
    RequireAiMenu(session->m_human->m_speed == speedBefore && session->m_human->m_nextMessageId == idBefore,
                  "enhancements off ignores speed keys");
  }
  galaxy->m_inModalLoop = true;
  const int modalMessageId = session->m_human->m_nextMessageId;
  galaxy->WindowProc(WM_KEYDOWN, '1', 0);
  RequireAiMenu(session->m_human->m_speed == 1, "modal window allows speed shortcut");
  std::unique_ptr<MessageBase> modalOrder(session->m_human->m_pending.Detach(modalMessageId));
  RequireAiMenu(modalOrder && modalOrder->GetKind() == 0x280, "modal shortcut submits speed order");
  {
    CoolGameWindow panel(app.m_instance, 0, 0, 100, 60, galaxy, 2, 7, 0, 0xfff0, 0, kGameWindowPopup, 0, 0);
    RequireAiMenu(panel.Create(0) != 0, "modal panel created");
    CoolGameWindow content(app.m_instance, 0, 0, 80, 40, &panel, 2, 7, 0, 0xfff1, 0, 0, 0, 0);
    RequireAiMenu(content.Create(0) != 0, "nested panel control created");
    auto checkFilteredSpeed = [&](HWND target, int speed) {
      const int id = session->m_human->m_nextMessageId;
      MSG key{};
      key.hwnd = target;
      key.message = WM_KEYDOWN;
      key.wParam = '0' + speed;
      RequireAiMenu(app.FilterMessage(&key) == 0, "panel speed key handled before child dispatch");
      RequireAiMenu(session->m_human->m_speed == speed, "panel shortcut sets speed");
      std::unique_ptr<MessageBase> order(session->m_human->m_pending.Detach(id));
      RequireAiMenu(order && order->GetKind() == 0x280, "panel shortcut submits speed order");
    };
    checkFilteredSpeed(panel.m_hwnd, 3);
    checkFilteredSpeed(content.m_hwnd, 4);
    DibBitmap backdrop(100, 60, galaxy->GetBackground());
    TextScrollField messageText(app.m_instance, 0, 0, 80, 40, &panel, &backdrop, 0);
    RequireAiMenu(messageText.Create(0) != 0, "message text control created");
    checkFilteredSpeed(messageText.m_hwnd, 2);
    messageText.SetEditable(1);
    MSG typing{};
    typing.hwnd = messageText.m_hwnd;
    typing.message = WM_KEYDOWN;
    typing.wParam = '3';
    RequireAiMenu(app.FilterMessage(&typing) != 0 && session->m_human->m_speed == 2,
                  "editable message keeps number typing");
    CoolStringField name(app.m_instance, 0, 0, 80, 20, &panel, &backdrop, 0);
    RequireAiMenu(name.Create(0) != 0, "name field created");
    typing.hwnd = name.m_hwnd;
    RequireAiMenu(app.FilterMessage(&typing) != 0 && session->m_human->m_speed == 2, "name field keeps number typing");
    reb::compat::SetFocus(name.m_hwnd);
    RequireAiMenu(IsTextInputActive(), "focused name field is recognized as text input");
    const uint32_t popupFlags = panel.m_flags;
    const bool modalLoop = galaxy->m_inModalLoop;
    panel.m_flags &= ~kGameWindowPopup;
    galaxy->m_inModalLoop = false;
    galaxy->WindowProc(WM_KEYDOWN, 'D', 0);
    RequireAiMenu(g_menuWindow == nullptr, "D does not open developer menu while a text field is active");
    reb::compat::SetFocus(messageText.m_hwnd);
    RequireAiMenu(IsTextInputActive(), "editable text scroll field is recognized as text input");
    galaxy->WindowProc(WM_KEYDOWN, 'D', 0);
    RequireAiMenu(g_menuWindow == nullptr, "D does not open developer menu while editable narrative text is active");
    panel.m_flags = popupFlags;
    galaxy->m_inModalLoop = modalLoop;
    reb::compat::DestroyWindow(name.m_hwnd);
    reb::compat::DestroyWindow(messageText.m_hwnd);
    reb::compat::DestroyWindow(content.m_hwnd);
    reb::compat::DestroyWindow(panel.m_hwnd);
  }
  galaxy->m_inModalLoop = false;
  galaxy->WindowProc(WM_KEYDOWN, 'D', 0);
  RequireAiMenu(g_menuWindow != nullptr, "D opens developer menu");
  galaxy->WindowProc(WM_KEYDOWN, '1', 0);
  RequireAiMenu(session->m_human->m_speed == 2, "popup blocks speed shortcut");
  DismissMenu();
  session->m_human->m_speed = originalSpeed;
  session->m_human->m_nextMessageId = originalNextId;
}

void CheckSyncMovieCompletion(RebellionApplication& app)
{
  GameSession* session = GetGameSession();
  GalaxyWindow* galaxy = app.m_strategicWindow;
  RequireAiMenu(session && session->m_human && galaxy, "sync movie completion context available");

  HumanPlayer* human = session->m_human;
  const int messageId = human->m_nextMessageId;
  const int humanSyncCount = human->m_unk4c;
  PlayerController* other = session->m_controllers[human->m_side == 1 ? 1 : 0];
  const int otherSyncCount = other->m_unk4c;
  const int dispatchFlags = human->Dispatch()->m_flags.m_value;
  const int musicPending = galaxy->m_musicPending;
  struct RestoreSyncSmokeState {
    HumanPlayer* human;
    GalaxyWindow* galaxy;
    PlayerController* other;
    int messageId;
    int humanSyncCount;
    int otherSyncCount;
    int dispatchFlags;
    int musicPending;
    ~RestoreSyncSmokeState()
    {
      human->m_nextMessageId = messageId;
      human->m_unk4c = humanSyncCount;
      other->m_unk4c = otherSyncCount;
      human->Dispatch()->m_flags.m_value = dispatchFlags;
      galaxy->m_musicPending = musicPending;
    }
  } restore{human, galaxy, other, messageId, humanSyncCount, otherSyncCount, dispatchFlags, musicPending};

  RequireAiMenu(galaxy->m_pendingMovie == 0, "no earlier movie is pending");
  galaxy->m_pendingMovie = new Notice31();
  galaxy->OnMovieDone();
  RequireAiMenu(galaxy->m_pendingMovie == 0, "movie completion clears its pending notice");
  std::unique_ptr<MessageBase> message(human->m_pending.Detach(messageId));
  RequireAiMenu(message && message->GetKind() == 5, "movie completion submits a sync-finished marker");
  RequireAiMenu(human->m_nextMessageId == messageId + 1, "movie completion assigns one message ID");
  RequireAiMenu(human->m_unk4c == humanSyncCount + 1 && other->m_unk4c == otherSyncCount + 1,
                "movie completion notifies both controllers that sync finished");
  RequireAiMenu(static_cast<SidedMessage*>(message.get())->m_side == human->m_side,
                "sync-finished marker carries the human side");
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
  const auto unmatchedDefinition = std::ranges::find_if(unmatchedDefinitions, [unmatchedKind](const auto& entry) {
    return entry.kind == unmatchedKind;
  });
  RequireAiMenu(unmatchedDefinition != unmatchedDefinitions.end(), "mismatched AI definition available");
  MenuItem* currentItem = FindAiMenuItem(std::string(faction) + " AI: " + unmatchedDefinition->displayName);
  RequireAiMenu(currentItem->m_label != nullptr, "current AI menu choice has a label");
  RequireAiMenu(currentItem->m_label->GetText()[0] == '*', "running controller marked despite saved preference");
  DismissMenu();
  for (const int kind : {kPlayerAiCodex, kPlayerAi}) {
    app.m_strategicWindow->WindowProc(WM_KEYDOWN, 'D', 0);
    RequireAiMenu(g_menuWindow != nullptr, "strategic key D opens AI menu");
    const auto definitions = AiControllerDefinitions();
    const auto definition = std::ranges::find_if(definitions, [kind](const auto& entry) {
      return entry.kind == kind;
    });
    RequireAiMenu(definition != definitions.end(), "AI definition available");
    const std::string label = std::string(faction) + " AI: " + definition->displayName;
    ClickAiMenuItem(FindAiMenuItem(label));
    RequireAiMenu(session->m_controllers[aiSide - 1]->GetKind() == kind, "live AI personality changes");
    RequireAiMenu(session->m_controllers[aiSide - 1]->m_syncId == syncId
                      && session->m_controllers[aiSide - 1]->m_pending.Count() == pendingCount,
                  "queued controller state survives replacement");
  }
  if (originalKind != kPlayerAi) {
    RequireAiMenu(session->SetAiControllerKind(aiSide, originalKind), "restore original AI");
  }
  std::unique_ptr<MessageBase> savedOrder(session->m_controllers[aiSide - 1]->m_pending.Detach(kQueuedTestId));
  RequireAiMenu(savedOrder && savedOrder->GetKind() == 0x280
                    && static_cast<NetMsg280*>(savedOrder.get())->m_value == 42,
                "queued order payload survives replacement");
}

void CheckCockpitAiMenu(RebellionApplication& app)
{
  namespace fs = std::filesystem;
  const std::string originalConfig = mw::config::GlobalPath();
  const fs::path temporaryConfig
      = fs::temp_directory_path()
        / ("rebellion_ai_menu_smoke_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
           + ".ini");
  RequireAiMenu(mw::config::Global().Save(temporaryConfig.string()), "temporary config created");
  struct RestoreConfig {
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
    for (const auto& ownedItem : g_menuWindow->m_items.Entries()) {
      auto* item = static_cast<MenuItem*>(ownedItem.get());
      RequireAiMenu(item->m_label->m_rect.right <= g_menuWindow->m_clientWidth, "cockpit choice text fits menu");
    }
    bool paintedFrame = false;
    DibBitmap* frame = g_menuWindow->m_frame;
    for (int y = 0; y < frame->Height() && !paintedFrame; ++y) {
      for (int x = 0; x < frame->Width(); ++x) {
        if (Pixel(*frame, x, y) != frame->m_colorKey) {
          paintedFrame = true;
          break;
        }
      }
    }
    RequireAiMenu(paintedFrame, "menu frame has visible pixels");
    ClickAiMenuItem(FindAiMenuItem("Alliance AI: Codex Strategist"));
    RequireAiMenu(PreferredAiKind(1) == kPlayerAiCodex && PreferredAiKind(2) == kPlayerAi,
                  "Alliance selection persists independently");
    cockpit.WindowProc(WM_KEYDOWN, 'D', 0);
    ClickAiMenuItem(FindAiMenuItem("Empire AI: Codex Strategist"));
    RequireAiMenu(PreferredAiKind(1) == kPlayerAiCodex && PreferredAiKind(2) == kPlayerAiCodex,
                  "Empire selection persists independently");
    RequireAiMenu(EnhancementsEnabled(), "smoke runs with the enhancements on");
    cockpit.WindowProc(WM_KEYDOWN, 'D', 0);
    ClickAiMenuItem(FindAiMenuItem("* Enhancements: on"));
    mw::config::Config saved;
    RequireAiMenu(!EnhancementsEnabled() && saved.Load(temporaryConfig.string()) && saved.Get("enhancements") == "0",
                  "enhancements toggle turns them off and persists");
    cockpit.WindowProc(WM_KEYDOWN, 'D', 0);
    ClickAiMenuItem(FindAiMenuItem("  Enhancements: off"));
    RequireAiMenu(EnhancementsEnabled() && saved.Load(temporaryConfig.string()) && saved.Get("enhancements") == "1",
                  "enhancements toggle turns them back on");
    GameSession newGame(1, 0);
    RequireAiMenu(newGame.m_controllers[0] && newGame.m_controllers[1]
                      && newGame.m_controllers[0]->GetKind() == kPlayerAiCodex
                      && newGame.m_controllers[1]->GetKind() == kPlayerAiCodex,
                  "new game constructs preferred controllers for both sides");
    reb::compat::DestroyWindow(cockpit.m_hwnd);
  }
  if (!commonWasLoaded) {
    CoolUnloadResourceSlot(8);
  }
  app.m_gameType = originalGameType;
}

} // namespace

void RunNativeSwitchSidesSmoke(RebellionApplication& app)
{
  auto require = [](bool condition, const char* check) {
    if (!condition) {
      throw std::runtime_error(std::string("Native switch sides smoke: ") + check);
    }
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
  for (int attempt = 0; attempt != 3; ++attempt) {
    const int previousSide = session->GetHumanSide();
    constexpr uint32_t kQueuedTestId = 0x7ffffff0;
    for (int index = 0; index != 2; ++index) {
      require(!session->m_controllers[index]->m_pending.FindById(kQueuedTestId), "test order ID available");
      auto* order = new NetMsg280;
      order->m_id = kQueuedTestId;
      order->m_side = index + 1;
      order->m_value = 60;
      session->m_controllers[index]->m_pending.Adopt(order);
    }
    const unsigned syncIds[] = {session->m_controllers[0]->m_syncId, session->m_controllers[1]->m_syncId};
    const size_t pendingCounts[]
        = {session->m_controllers[0]->m_pending.Count(), session->m_controllers[1]->m_pending.Count()};
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
    require(session->m_human == session->m_controllers[app.m_side - 1]
                && AiControllerName(session->m_controllers[2 - app.m_side]->GetKind()) != nullptr,
            "fixed controller slots");
    require(session->m_human->m_speed == originalSpeed && driver->m_pauseTick == originalPause
                && GetGameTickCount() == originalTick,
            "speed, pause and tick preserved");
    require(app.m_strategicWindow->m_speed == originalSpeed, "speed immediately displayed");
    CheckGalaxyResize(app);
    for (int index = 0; index != 2; ++index) {
      require(session->m_controllers[index]->m_syncId == syncIds[index]
                  && session->m_controllers[index]->m_pending.Count() == pendingCounts[index],
              "sync and orders preserved");
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
  OverrideEnhancementsForProcess(true);
  GameSession* session = GetGameSession();
  const int originalState = app.m_state;
  const int originalViewReady = session ? session->m_viewReady : 0;
  app.m_state = kStateStrategic;
  app.ShowStrategicWindow(0);
  Require(app.m_strategicWindow != nullptr);
  app.m_strategicWindow->InitializeGameDisplay();
  CheckGalaxyResize(app);
  CheckFleetLabelsAndDrag(app);
  CheckArrivalDisplay(app);
  CheckFacilityTransitArtwork(app);
  CheckForcesAndFleetTabArtwork(app);
  CheckMissionCharacterEta(app);
  CheckMissionWithDecoys(app);
  CheckCodexOccupationThreshold();
  CheckManufacturingEta(app);
  CheckSideEconomyBehavior();
  CheckProductionDestinationMenu(app);
  CheckSpeedShortcuts(app);
  CheckSyncMovieCompletion(app);
  CheckLiveAiSelection(app);
  app.DestroyStrategicWindow();
  if (session) {
    session->m_viewReady = originalViewReady;
  }
  app.m_state = originalState;
  auto releaseLocation = [](char* value) {
    ::operator delete(value);
  };
  std::unique_ptr<char, decltype(releaseLocation)> location(GetDllLocation(), releaseLocation);
  HMODULE textModule = LoadResourceModule(location.get(), "textstra.dll", 2);
  MessageTemplate templates(0xbb9, 0xbb8, textModule);
  struct RestoreTemplates {
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
  RunNativeHdArtworkReview(location.get(), background);
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

void SaveUiReviewFrame(const std::filesystem::path& path)
{
  SaveHdReviewFrame(path);
}
