#include "port/native/save_slots_smoke.h"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>

#include "app/app_dialogs.h"
#include "app/enhancements.h"
#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "port/native/ui_smoke.h"
#include "strategy_ui/game_options_window.h"
#include "ui/cool_slider.h"
#include "ui/cool_string_field.h"
#include "ui/cool_window.h"

namespace reb::native {

namespace
{

constexpr int kSlotCount = 6;
constexpr uint32_t kSaveFirst = 0x6f;
constexpr uint32_t kLoadFirst = 0x79;
constexpr uint32_t kDeleteFirst = 0xb1;
constexpr uint32_t kMessageSaved = 0x474;
constexpr uint32_t kMessageLoad = 0x477;
constexpr uint32_t kMessageSave = 0x478;
constexpr uint32_t kMessageDelete = 0x47e;
constexpr UINT kDialogAccepted = 0x46a;
constexpr UINT kDialogRejected = 0x46b;

void Require(bool condition, const std::string& check)
{
  if (!condition) {
    throw std::runtime_error(check);
  }
}

void RepaintTree(CoolWindow* window)
{
  reb::compat::InvalidateRect(window->m_hwnd, nullptr, TRUE);
  reb::compat::UpdateWindow(window->m_hwnd);
  for (const auto& child : window->m_children.Entries()) {
    RepaintTree(child.get());
  }
}

void SaveReview(GameOptionsWindow& window, const char* name)
{
  const char* directory = std::getenv("REB_SAVE_SLOTS_REVIEW_DIR");
  if (!directory || !*directory) {
    return;
  }
  std::filesystem::create_directories(std::filesystem::u8path(directory));
  RepaintTree(&window);
  for (CoolWindow* part : {static_cast<CoolWindow*>(window.m_musicSlider), static_cast<CoolWindow*>(window.m_volumeSlider)}) {
    if (part) {
      RepaintTree(part);
    }
  }
  for (CoolStringField* field : window.m_nameFields) {
    if (field) {
      RepaintTree(field);
    }
  }
  if (g_appDialog) {
    RepaintTree(g_appDialog);
  }
  SaveUiReviewFrame(std::filesystem::u8path(directory) / name);
}

} // namespace

bool RunSaveSlotsSmoke(RebellionApplication& app, std::string& error)
{
  OverrideEnhancementsForProcess(true);
  try {
    app.m_state = kStateStrategic;
    app.SetState(kStateOptionsInGame);
    auto* window = dynamic_cast<GameOptionsWindow*>(app.m_activeWindow);
    Require(window != nullptr, "the Saved Games screen opens");
    Require(window->m_battleFlowState != 0, "the screen knows a game is running");
    const auto deleteButton = [&](int slot) { return window->m_children.FindById(kDeleteFirst + slot - 1); };
    const auto occupied = [&](int slot) { return AppGetSavedGameType(slot, 0) != 0; };
    for (int slot = 1; slot <= kSlotCount; ++slot) {
      Require(deleteButton(slot) != nullptr, "every slot has a delete button");
      Require(reb::compat::IsWindowVisible(deleteButton(slot)->m_hwnd) == occupied(slot),
              "a delete button shows exactly on an occupied slot (" + std::to_string(slot) + ")");
    }
    Require(occupied(1) && occupied(2) && occupied(5) && !occupied(6), "the scratch install has saves 1-5 only");
    SaveReview(*window, "screen.bmp");

    const auto command = [&](uint32_t id) { reb::compat::SendMessageA(window->m_hwnd, WM_COMMAND, id, 0); };
    const auto answer = [&](bool yes) {
      const uint32_t message = g_appDialogMessage;
      DestroyAppDialog();
      reb::compat::SendMessageA(window->m_hwnd, yes ? kDialogAccepted : kDialogRejected, message, 0);
    };

    command(kSaveFirst);
    Require(g_appDialogMessage == kMessageSave, "saving over slot 1 asks first");
    Require(g_appDialog->m_headlineText.find("\"JAS1\"") != std::string::npos, "the overwrite question names the save");
    SaveReview(*window, "overwrite.bmp");
    answer(false);
    Require(app.m_pendingSaveSlot == 0, "declining the overwrite saves nothing");

    command(kLoadFirst + 1);
    Require(g_appDialogMessage == kMessageLoad, "loading over the running game asks first");
    Require(g_appDialog->m_detailText.find("\"Rebels_plane\"") != std::string::npos, "the load question names the save");
    SaveReview(*window, "load.bmp");
    answer(false);
    Require(app.m_pendingLoadSlot == 0, "declining the load loads nothing");

    command(kDeleteFirst + 4);
    Require(g_appDialogMessage == kMessageDelete, "deleting asks first");
    SaveReview(*window, "delete.bmp");
    answer(false);
    Require(occupied(5), "declining the delete keeps the save");
    command(kDeleteFirst + 4);
    answer(true);
    Require(!occupied(5), "accepting the delete removes the save file");
    Require(!reb::compat::IsWindowVisible(deleteButton(5)->m_hwnd), "the deleted slot loses its delete button");
    Require(!reb::compat::IsWindowEnabled(window->m_children.FindById(kLoadFirst + 4)->m_hwnd), "the deleted slot cannot be loaded");
    Require(window->m_nameFields[4]->m_text.find("game_jas") == std::string::npos, "the deleted slot's name is cleared");
    SaveReview(*window, "deleted.bmp");

    window->m_nameFields[5]->SetText("Smoke save");
    command(kSaveFirst + 5);
    Require(g_appDialogMessage == 0, "saving into an empty slot does not ask");
    Require(app.m_pendingSaveSlot == 6, "the save is requested");
    app.ProcessPendingSaveLoad();
    Require(g_appDialogMessage == kMessageSaved, "the save reports success");
    DestroyAppDialog();
    Require(occupied(6) && reb::compat::IsWindowVisible(deleteButton(6)->m_hwnd), "the new save gets a delete button");
    command(kSaveFirst + 5);
    Require(g_appDialogMessage == 0, "saving again over this visit's own save does not ask");
    app.m_pendingSaveSlot = 0;
    app.m_pendingSaveName.clear();
    command(kLoadFirst);
    Require(g_appDialogMessage == 0, "after saving this visit, loading does not warn about unsaved progress");
    app.m_pendingLoadSlot = 0;
    SaveReview(*window, "saved.bmp");
    return true;
  } catch (const std::exception& failure) {
    error = failure.what();
    return false;
  }
}

} // namespace reb::native
