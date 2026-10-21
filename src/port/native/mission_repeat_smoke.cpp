#include "port/native/mission_repeat_smoke.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "ai/ai_common.h"
#include "app/enhancements.h"
#include "app/game_session.h"
#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "character/character.h"
#include "core/game_string.h"
#include "core/memory_stream.h"
#include "core/stream_io.h"
#include "economy/side.h"
#include "events/player_notices.h"
#include "gameobj/child_iterator.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_family.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_name_node.h"
#include "mission/mission.h"
#include "mission/mission_external.h"
#include "orders/mission_orders.h"
#include "rules/rules_mgr.h"
#include "port/native/ui_smoke.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "strategy_ui/human_player.h"
#include "strategy_ui/message_index_window.h"
#include "strategy_ui/ui_handler_table.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_window.h"
#include "ui/message_list_item.h"
#include "ui/message_template.h"
#include "ui/text_scroll_field.h"

namespace reb::native {

namespace
{

constexpr uint32_t kSabotageFamily = 0x69;
constexpr uint32_t kTroopFamily = 0x10;
constexpr uint32_t kStateDestroyed = 0x8;
constexpr uint32_t kStateEnroute = 0x10;
constexpr int kMissionOrderKind = 0x240;
constexpr uint32_t kTickStep = 20;
constexpr uint32_t kTickLimit = 120000;
constexpr uint32_t kMessageIndexChildId = 0xd;
constexpr uint32_t kRepeatButtonId = 0x9b;
constexpr int kAllMessagesTab = 0x79;
constexpr int kDecoyDetourDays = 10;

void Require(bool condition, const std::string& check)
{
  if (!condition) {
    throw std::runtime_error(check);
  }
}

std::string Hex(uint32_t value)
{
  char buffer[16];
  std::snprintf(buffer, sizeof buffer, "%08x", value);
  return buffer;
}

HumanPlayer& Human()
{
  GameSession* session = GetGameSession();
  Require(session && session->m_human, "the loaded game has a human player");
  return *session->m_human;
}

GameObj* AsGameObj(GameObjNotifier* object)
{
  return static_cast<GameObj*>(object);
}

ObjectKey KeyOf(GameObjNotifier* object)
{
  ObjectKey key;
  object->GetKey(&key);
  return key;
}

uint32_t EnemyOf(int side)
{
  return side == 1 ? 2 : 1;
}

GameObjNotifier* SystemOf(GameObjNotifier* object)
{
  for (; object; object = object->m_parent) {
    if (object->GetFamily() >= family::kSystems.m_first && object->GetFamily() < family::kSystems.m_second) {
      return object;
    }
  }
  return nullptr;
}

Character* CharacterOf(int side, const ObjectKey& key)
{
  GameObjNotifier* object = GetObjectInFamilies_30_3c(side, &key);
  return object ? static_cast<Character*>(AsGameObj(object)) : nullptr;
}

bool IsFree(Character* character)
{
  return (character->m_stateFlags & kStateDestroyed) == 0 && (character->m_flagsAc & 1) == 0;
}

bool IsIdleAtSystem(Character* character, int side)
{
  return character->ControlSide() == static_cast<uint32_t>(side) && IsFree(character)
         && !character->m_missionKey.IsSet() && character->m_injury == 0 && character->m_commandKind == 0
         && (character->m_stateFlags & kStateEnroute) == 0 && character->m_parent
         && character->m_parent == SystemOf(character);
}

std::vector<GameObjNotifier*> EnemyTroopsAt(GameObjNotifier* system, int side)
{
  std::vector<GameObjNotifier*> troops;
  const FamilyRange range(kTroopFamily, kTroopFamily + 1);
  ChildIterator children(system, &range, 1);
  for (children.Begin(); children.Current(); children.Advance()) {
    if (children.Current()->ControlSide() == EnemyOf(side)) {
      troops.push_back(children.Current());
    }
  }
  return troops;
}

struct Scenario {
  ObjectKey agent;
  ObjectKey decoy;
  ObjectKey system;
  ObjectKey troop;
  int days = 0;
};

// Idle agents at a system (with a decoy from the same system when there is one) against enemy systems with at least
// two known troop regiments, the shortest trips first.
std::vector<Scenario> Scenarios(int side)
{
  std::vector<Character*> idle;
  for (GameObjNotifier* object = GetFirstObjectInRangeForSide(side, &family::kCharacters, 0); object;
       object = AsGameObj(object)->FindNextSibling(&family::kCharacters, 0)) {
    auto* character = static_cast<Character*>(AsGameObj(object));
    if (IsIdleAtSystem(character, side)) {
      idle.push_back(character);
    }
  }
  std::vector<Scenario> scenarios;
  for (GameObjNotifier* system = GetFirstObjectInFamilies_90_98(side); system;
       system = AsGameObj(system)->FindNextSibling(&family::kSystems, 1)) {
    const std::vector<GameObjNotifier*> troops = EnemyTroopsAt(system, side);
    if (system->ControlSide() != EnemyOf(side) || troops.size() < 2) {
      continue;
    }
    for (Character* agent : idle) {
      Scenario scenario{KeyOf(agent), {}, KeyOf(system), KeyOf(troops.front())};
      const ObjectKey from = KeyOf(agent->m_parent);
      if (!GetRulesMgr()->RulesMgr::ComputeUnitTravelDays(agent, &from, &scenario.system, &scenario.days)) {
        continue;
      }
      const auto decoy = std::ranges::find_if(
          idle, [&](Character* other) { return other != agent && other->m_parent == agent->m_parent; });
      if (decoy != idle.end()) {
        scenario.decoy = KeyOf(*decoy);
      }
      scenarios.push_back(scenario);
    }
  }
  // A decoy covers more, so a scenario with one goes first unless it is much further away.
  std::ranges::stable_sort(scenarios, {}, &Scenario::days);
  const int nearest = scenarios.empty() ? 0 : scenarios.front().days;
  std::ranges::stable_partition(scenarios, [&](const Scenario& scenario) {
    return scenario.decoy.IsSet() && scenario.days <= nearest + kDecoyDetourDays;
  });
  return scenarios;
}

std::optional<ObjectKey> SabotageType(MissionOrder240& order)
{
  ObjectNameList names;
  order.CollectCandidatesB(&names);
  for (const auto& owned : names.Entries()) {
    ObjectKey key;
    GetObjectNameNodeKey(owned.get(), &key);
    if (key.Family() == kSabotageFamily) {
      return key;
    }
  }
  return std::nullopt;
}

// Orders the sabotage mission of the first scenario the game accepts.
Scenario SendFirstSabotage(int side, const MissionRepeatSmokeHooks& hooks)
{
  const std::vector<Scenario> scenarios = Scenarios(side);
  hooks.log("scenarios: " + std::to_string(scenarios.size()) + ", with a decoy: "
            + std::to_string(std::ranges::count_if(scenarios, [](const Scenario& s) { return s.decoy.IsSet(); })));
  for (const Scenario& scenario : scenarios) {
    ObjectKeyValueSet team;
    team.AddKeyValue(&scenario.agent, 0);
    ObjectKey troop = scenario.troop;
    std::unique_ptr<OrderBase> order(Human().CreateOrder(kMissionOrderKind, &team, &troop));
    auto* missionOrder = dynamic_cast<MissionOrder240*>(order.get());
    Require(missionOrder != nullptr, "a mission order can be created");
    if (scenario.decoy.IsSet()) {
      missionOrder->m_characters.AddKeyValue(&scenario.decoy, 0);
    }
    const std::optional<ObjectKey> type = SabotageType(*missionOrder);
    if (!type) {
      continue;
    }
    missionOrder->m_missionKey.m_value = type->m_value;
    StatusCode status;
    if (!missionOrder->CheckValid(&status) || status.m_second != static_cast<uint32_t>(-1)) {
      continue;
    }
    hooks.log("sabotage: agent " + Hex(scenario.agent.m_value) + " decoy " + Hex(scenario.decoy.m_value) + " troop "
              + Hex(scenario.troop.m_value) + " at " + Hex(scenario.system.m_value) + ", "
              + std::to_string(scenario.days) + " days away");
    Human().SubmitOrder(static_cast<PlayerMessage*>(order.release()), 0);
    return scenario;
  }
  throw std::runtime_error("no idle agent can sabotage an enemy troop regiment");
}

// Advances until `done` holds, checking every few ticks.
template <class Done>
void AdvanceUntil(const MissionRepeatSmokeHooks& hooks, const char* what, Done done)
{
  for (uint32_t ticks = 0; !done(); ticks += kTickStep) {
    Require(ticks < kTickLimit, std::string("timed out waiting for ") + what);
    Require(hooks.advance(kTickStep), std::string("the game advances while waiting for ") + what);
  }
}

template <class Notice, class Match>
Notice* FindListed(Match match)
{
  for (const auto& entry : Human().Dispatch()->m_trees[0].Entries()) {
    auto* notice = dynamic_cast<Notice*>(static_cast<UiNotice*>(entry.get()));
    if (notice && match(*notice)) {
      return notice;
    }
  }
  return nullptr;
}

UiTextNotice* FindReport(const ObjectKey& mission)
{
  if (auto* report = FindListed<Notice22>([&](Notice22& n) { return n.m_key78.m_value == mission.m_value; })) {
    return report;
  }
  return FindListed<Notice23>([&](Notice23& n) { return n.m_key78.m_value == mission.m_value; });
}

bool Contains(const std::vector<ObjectKey>& keys, const ObjectKey& key)
{
  return std::ranges::any_of(keys, [&](const ObjectKey& k) { return k.m_value == key.m_value; });
}

bool EndsWith(const std::string& text, const std::string& suffix)
{
  return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Loads `repeats` with `plan` as its only queued repeat, through the saved form.
void QueueThroughSavedForm(MissionRepeats& repeats, const MissionRepeatPlan& plan)
{
  MissionRepeats source;
  {
    io::MemoryWriter out;
    const uint32_t noOffers = 0;
    const uint32_t oneQueued = 1;
    WriteValue32(out.Out(), &noOffers);
    WriteValue32(out.Out(), &oneQueued);
    for (const ObjectKey* key : {&plan.mission, &plan.missionType, &plan.target, &plan.system}) {
      WriteValue32(out.Out(), &key->m_value);
    }
    for (const auto* members : {&plan.agents, &plan.decoys}) {
      const auto count = static_cast<uint32_t>(members->size());
      WriteValue32(out.Out(), &count);
      for (const ObjectKey& key : *members) {
        WriteValue32(out.Out(), &key.m_value);
      }
    }
    WriteLegacyString(out.Out(), plan.name);
    const std::vector<uint8_t> bytes = out.TakeBytes();
    io::MemoryReader in(bytes);
    Require(source.Read(in.In()) && in.Good() && in.AtEnd(), "a queued repeat reads from its saved form");
  }
  io::MemoryWriter out;
  source.Write(out.Out());
  const std::vector<uint8_t> bytes = out.TakeBytes();
  io::MemoryReader in(bytes);
  Require(repeats.Read(in.In()) && in.Good() && in.AtEnd(), "a queued repeat reads back what it wrote");
  Require(repeats.Queued().size() == 1 && repeats.Queued().front().name == plan.name
              && repeats.Queued().front().decoys.size() == plan.decoys.size(),
          "the saved form keeps the queued repeat");
}

std::unique_ptr<OrderBase> MissionOrderFor(const ObjectKey& agent, const ObjectKey* decoy, const ObjectKey& target)
{
  ObjectKeyValueSet team;
  team.AddKeyValue(&agent, 0);
  ObjectKey destination = target;
  std::unique_ptr<OrderBase> order(Human().CreateOrder(kMissionOrderKind, &team, &destination));
  if (decoy) {
    static_cast<MissionOrder240*>(order.get())->m_characters.AddKeyValue(decoy, 0);
  }
  return order;
}

// A queued repeat is dropped when its members get other orders, and only then.
void CheckOtherOrdersCancel(MissionRepeatPlan plan)
{
  const ObjectKey stranger = ObjectKey::Make(0x38, 0xfffff0);
  if (plan.decoys.empty()) {
    plan.decoys.push_back(ObjectKey::Make(0x38, 0xfffff1));
  }
  MissionRepeats repeats;
  QueueThroughSavedForm(repeats, plan);
  repeats.OnPlayerOrder(MissionOrderFor(stranger, nullptr, plan.system).get());
  Require(repeats.Queued().size() == 1, "orders for other characters leave a queued repeat alone");
  repeats.OnPlayerOrder(MissionOrderFor(plan.agents.front(), nullptr, plan.system).get());
  Require(repeats.Queued().empty(), "ordering a queued repeat's agent elsewhere drops the repeat");
  QueueThroughSavedForm(repeats, plan);
  repeats.OnPlayerOrder(MissionOrderFor(stranger, &plan.decoys.front(), plan.system).get());
  Require(repeats.Queued().empty(), "taking a queued repeat's decoy for another mission drops the repeat");
}

void RepaintTree(CoolWindow* window)
{
  reb::compat::InvalidateRect(window->m_hwnd, nullptr, TRUE);
  reb::compat::UpdateWindow(window->m_hwnd);
  for (const auto& child : window->m_children.Entries()) {
    RepaintTree(child.get());
  }
}

void SaveReview(MessageIndexWindow& index, const char* name)
{
  const char* directory = std::getenv("REB_MISSION_REPEAT_REVIEW_DIR");
  if (!directory || !*directory) {
    return;
  }
  std::filesystem::create_directories(std::filesystem::u8path(directory));
  RepaintTree(&index);
  RepaintTree(index.m_text); // owned by the window but not one of its listed children
  SaveUiReviewFrame(std::filesystem::u8path(directory) / name);
}

void ShowListedMessage(MessageIndexWindow& index, uint32_t noticeId)
{
  index.SetMode(kIndexModeList);
  index.m_list->ClearSelection();
  const auto row = std::ranges::find_if(index.m_items->Entries(), [&](const auto& entry) {
    return static_cast<MessageListItem*>(entry.get())->m_record->m_id == noticeId;
  });
  Require(row != index.m_items->Entries().end(), "the message index lists the message");
  index.m_list->SelectById((*row)->m_id, 1);
  index.SetMode(kIndexModeMessage);
  Require(index.m_current && index.m_current->m_record->m_id == noticeId, "the message index shows the message");
}

// Opens the report in the galaxy view's message index and presses its Repeat mission button.
void PressRepeatButton(RebellionApplication& app, uint32_t reportId)
{
  const int originalState = app.m_state;
  app.m_state = kStateStrategic;
  app.ShowStrategicWindow(0);
  Require(app.m_strategicWindow != nullptr, "the galaxy view opens");
  struct CloseGalaxy {
    RebellionApplication& app;
    int state;
    ~CloseGalaxy()
    {
      app.DestroyStrategicWindow();
      app.m_state = state;
    }
  } closeGalaxy{app, originalState};
  GalaxyWindow* galaxy = app.m_strategicWindow;
  galaxy->InitializeGameDisplay();
  galaxy->OpenMessageIndex(kIndexModeList, kAllMessagesTab);
  auto* index = static_cast<MessageIndexWindow*>(galaxy->m_children.FindById(kMessageIndexChildId));
  Require(index != nullptr, "the message index opens");
  CoolWindow* repeat = index->m_children.FindById(kRepeatButtonId);
  Require(repeat != nullptr, "the message index has a Repeat mission button");

  const auto other = std::ranges::find_if(Human().Dispatch()->m_trees[0].Entries(), [&](const auto& entry) {
    return entry->m_id != reportId && !UiHumanCanRepeatMission(entry->m_id);
  });
  if (other != Human().Dispatch()->m_trees[0].Entries().end()) {
    ShowListedMessage(*index, (*other)->m_id);
    Require(!reb::compat::IsWindowVisible(repeat->m_hwnd), "messages without an offer show no Repeat mission button");
  }

  ShowListedMessage(*index, reportId);
  Require(reb::compat::IsWindowVisible(repeat->m_hwnd), "the report shows the Repeat mission button");
  SaveReview(*index, "repeat-offered.bmp");
  index->OnCommand(static_cast<int>(kRepeatButtonId), 0, 0);
  Require(!reb::compat::IsWindowVisible(repeat->m_hwnd), "the button goes away once pressed");
  Require(EndsWith(index->m_text->m_text, "The mission will be repeated as soon as the team is back."),
          "the shown report says the repeat is queued");
  SaveReview(*index, "repeat-queued.bmp");
  ShowListedMessage(*index, reportId);
  Require(!reb::compat::IsWindowVisible(repeat->m_hwnd), "a used offer shows no button when the report is reopened");
  index->PostCloseRequest();
}

void RunScenario(RebellionApplication& app, const MissionRepeatSmokeHooks& hooks)
{
  const int side = Human().m_side;
  const Scenario scenario = SendFirstSabotage(side, hooks);
  AdvanceUntil(hooks, "the sabotage mission to start", [&] {
    Character* agent = CharacterOf(side, scenario.agent);
    return agent && agent->m_missionKey.Family() == kSabotageFamily;
  });
  const ObjectKey mission = CharacterOf(side, scenario.agent)->m_missionKey;
  hooks.log("sabotage mission " + Hex(mission.m_value));

  UiTextNotice* report = nullptr;
  AdvanceUntil(hooks, "the sabotage report", [&] { return (report = FindReport(mission)) != nullptr; });
  const uint32_t reportId = report->m_id;
  hooks.log("report " + std::to_string(reportId) + ": " + report->MessageText0() + " | " + report->MessageText1());
  Character* agent = CharacterOf(side, scenario.agent);
  if (!agent || !IsFree(agent)) {
    Require(!Human().m_missionRepeats.HasOffer(reportId), "a team without a surviving agent offers no repeat");
    hooks.log("the agent did not come back, so only the missing offer is checked");
    return;
  }

  const MissionRepeatPlan* offer = Human().m_missionRepeats.Offer(reportId);
  Require(offer != nullptr, "the report of a sabotage whose agent came back offers a repeat");
  Require(UiHumanCanRepeatMission(reportId), "the message index sees the offer");
  Require(offer->mission.m_value == mission.m_value && offer->missionType.Family() == kSabotageFamily,
          "the offer repeats the same kind of mission");
  Require(offer->target.m_value == scenario.troop.m_value && offer->system.m_value == scenario.system.m_value,
          "the offer remembers the target and its system");
  Require(offer->agents.size() == 1 && offer->agents.front().m_value == scenario.agent.m_value,
          "the offer's agents are the mission's agents");
  Require(!scenario.decoy.IsSet()
              || (offer->decoys.size() == 1 && offer->decoys.front().m_value == scenario.decoy.m_value),
          "the offer keeps the decoys apart from the agents");
  const MissionRepeatPlan plan = *offer;

  PressRepeatButton(app, reportId);
  Require(!Human().m_missionRepeats.HasOffer(reportId), "a requested offer is used up");
  Require(Human().m_missionRepeats.Queued().size() == 1, "the repeat waits for the team to come home");
  Require(EndsWith(FindReport(mission)->MessageText1(), "The mission will be repeated as soon as the team is back."),
          "the report says the repeat is queued");

  Require(hooks.saveAndReload(), "the game saves and reloads with a queued repeat");
  Require(Human().m_missionRepeats.Queued().size() == 1, "the queued repeat survives saving and loading");
  const MissionRepeatPlan& reloaded = Human().m_missionRepeats.Queued().front();
  Require(reloaded.mission.m_value == plan.mission.m_value && reloaded.agents.size() == plan.agents.size()
              && reloaded.decoys.size() == plan.decoys.size() && reloaded.name == plan.name,
          "the reloaded repeat is the one that was queued");
  Require(!Human().m_missionRepeats.HasOffer(reportId), "a used offer stays used after loading");

  Notice20* arrival = nullptr;
  AdvanceUntil(hooks, "the repeated mission", [&] {
    if (!arrival) {
      arrival = FindListed<Notice20>(
          [&](Notice20& n) { return Contains(plan.agents, n.m_unitKey) || Contains(plan.decoys, n.m_unitKey); });
    }
    Character* member = CharacterOf(side, scenario.agent);
    return member && member->m_missionKey.Family() == kSabotageFamily
           && member->m_missionKey.m_value != mission.m_value;
  });
  Require(Human().m_missionRepeats.Queued().empty(), "a sent repeat is no longer queued");
  Require(arrival != nullptr, "the team's arrival is reported");
  hooks.log("arrival: " + arrival->MessageText1());
  Require(arrival->MessageText1().find("\nNow repeating the ") != std::string::npos,
          "the arrival report says the mission is being repeated");

  const ObjectKey repeatedKey = CharacterOf(side, scenario.agent)->m_missionKey;
  GameObjNotifier* repeatedObject = GetObjectInFamilies_40_80(side, &repeatedKey);
  Require(repeatedObject != nullptr, "the repeated mission exists");
  auto* repeated = static_cast<Mission*>(AsGameObj(repeatedObject));
  GameObjNotifier* firstTroop = GetObjectInFamilies_01_ff(side, &scenario.troop);
  const bool firstTroopThere = firstTroop && (AsGameObj(firstTroop)->m_stateFlags & kStateDestroyed) == 0
                               && SystemOf(firstTroop) && KeyOf(SystemOf(firstTroop)).m_value == scenario.system.m_value;
  hooks.log("repeated mission " + Hex(repeatedKey.m_value) + " against " + Hex(repeated->m_objectiveKey.m_value)
            + (firstTroopThere ? " (the first troop is still there)" : " (the first troop is gone)"));
  Require(repeated->m_targetLocationKey.m_value == scenario.system.m_value, "the repeat goes to the same system");
  Require(repeated->m_objectiveKey.Family() == kTroopFamily, "the repeat sabotages a troop regiment again");
  Require(firstTroopThere ? repeated->m_objectiveKey.m_value == scenario.troop.m_value
                          : repeated->m_objectiveKey.m_value != scenario.troop.m_value,
          "the repeat retries the same troop while it is there, otherwise moves on to the next one");
  Require(repeated->m_team.Find(&scenario.agent) != nullptr, "the agent goes again as an agent");
  Character* decoy = scenario.decoy.IsSet() ? CharacterOf(side, scenario.decoy) : nullptr;
  if (decoy && IsFree(decoy)) {
    Require(repeated->m_decoys.Find(&scenario.decoy) != nullptr, "the decoy goes again as a decoy");
  }

  CheckOtherOrdersCancel(plan);
}

} // namespace

bool RunMissionRepeatSmoke(RebellionApplication& app, const MissionRepeatSmokeHooks& hooks, std::string& error)
{
  OverrideEnhancementsForProcess(true);
  if (!GetGameSession() || !GetGameSession()->m_human) {
    error = "the loaded game has no human player";
    return false;
  }
  // Notices without text are dropped, so the reports need the message templates the galaxy view would load. The
  // galaxy view replaces them when it opens, so they are installed the way it does.
  const bool hadTemplates = g_messageTemplate != nullptr;
  if (!hadTemplates) {
    char* location = GetDllLocation();
    SetMessageTemplateSource(0xbb9, 0xbb8, LoadResourceModule(location, "textstra.dll", 2));
    ::operator delete(location);
  }
  struct RestoreTemplates {
    bool had;
    ~RestoreTemplates()
    {
      if (!had) {
        SetMessageTemplateSource(0xbb9, 0xbb8, nullptr);
      }
    }
  } restoreTemplates{hadTemplates};
  try {
    RunScenario(app, hooks);
  } catch (const std::exception& failure) {
    error = failure.what();
    return false;
  }
  return true;
}

} // namespace reb::native
