#include "strategy_ui/mission_repeat.h"

#include <algorithm>
#include <memory>

#include "ai/ai_common.h"
#include "character/character.h"
#include "core/game_string.h"
#include "core/stream_io.h"
#include "events/game_clock_hooks.h"
#include "events/player_notices.h"
#include "gameobj/child_iterator.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_family.h"
#include "gameobj/object_lookup.h"
#include "mission/mission.h"
#include "orders/mission_orders.h"
#include "strategy_ui/human_player.h"
#include "strategy_ui/ui_handler_table.h"

namespace
{

constexpr uint32_t kStateDestroyed = 0x8;
constexpr uint32_t kStateEnroute = 0x10;
constexpr uint32_t kCapturedFlag = 0x1;
constexpr int kMissionOrderKind = 0x240;
constexpr int kTaskSucceeded = 3;
constexpr int kCompletionAborted = 8;
constexpr uint32_t kMaxSavedEntries = 4096;
// How long a sent repeat waits for its team's arrival report, which comes a few frames after the team is free.
constexpr uint32_t kAnnouncementTicks = 120;
constexpr FamilyRange kPersons(0x30, 0x40);
constexpr FamilyRange kContents(0x08, 0x40);

enum class TargetRule {
  SameSystem,     // the mission's system again
  SameKind,       // the target, else the next enemy object of its family at the system
  EnemyCharacter, // the target, else the next free enemy character at the system
  Captive,        // the target, else the next of the side's characters held at the system
};

struct RepeatKind {
  uint32_t family;
  TargetRule rule;
  bool afterSuccess; // whether a success at the same system leaves anything to repeat
};

// Diplomacy, research and the uprising missions are left out: they carry on until recalled, and their reports ask
// whether to continue.
constexpr RepeatKind kRepeatKinds[] = {
    {0x52, TargetRule::SameSystem, false},    // Espionage
    {0x54, TargetRule::SameSystem, false},    // Reconnaissance
    {0x55, TargetRule::SameSystem, true},     // Recruitment
    {0x58, TargetRule::SameSystem, true},     // Jedi Training
    {0x61, TargetRule::Captive, true},        // Rescue
    {0x62, TargetRule::EnemyCharacter, true}, // Abduction
    {0x63, TargetRule::EnemyCharacter, true}, // Assassination
    {0x69, TargetRule::SameKind, true},       // Sabotage
    {0x6a, TargetRule::SameKind, true},       // Death Star Sabotage
};

const RepeatKind* FindRepeatKind(uint32_t missionFamily)
{
  const auto* found = std::ranges::find(kRepeatKinds, missionFamily, &RepeatKind::family);
  return found == std::end(kRepeatKinds) ? nullptr : found;
}

bool InRange(uint32_t family, const FamilyRange& range)
{
  return family >= range.m_first && family < range.m_second;
}

GameObj* AsGameObj(GameObjNotifier* object)
{
  return static_cast<GameObj*>(object);
}

bool IsDestroyed(GameObjNotifier* object)
{
  return (AsGameObj(object)->m_stateFlags & kStateDestroyed) != 0;
}

bool IsCapturedCharacter(GameObjNotifier* object)
{
  return InRange(object->GetFamily(), family::kCharacters)
         && (static_cast<Character*>(AsGameObj(object))->m_flagsAc & kCapturedFlag) != 0;
}

uint32_t EnemyOf(int side)
{
  return side == 1 ? 2 : 1;
}

GameObjNotifier* SystemOf(GameObjNotifier* object)
{
  for (; object; object = object->m_parent) {
    if (InRange(object->GetFamily(), family::kSystems)) {
      return object;
    }
  }
  return nullptr;
}

// The fleet or system a person can set out from, or null while it (or what carries it) is in transit.
GameObjNotifier* DepartureOf(GameObjNotifier* person)
{
  for (GameObjNotifier* object = person; object; object = object->m_parent) {
    if ((AsGameObj(object)->m_stateFlags & kStateEnroute) != 0) {
      return nullptr;
    }
    const uint32_t family = object->GetFamily();
    if (object != person && (InRange(family, family::kFleets) || InRange(family, family::kSystems))) {
      return object;
    }
  }
  return nullptr;
}

bool Contains(const std::vector<ObjectKey>& keys, const ObjectKey& key)
{
  return std::ranges::any_of(keys, [&](const ObjectKey& k) { return k.m_value == key.m_value; });
}

std::vector<ObjectKey> KeysOf(const ObjectKeySet& set)
{
  std::vector<ObjectKey> keys;
  for (const auto& node : set.Entries()) {
    keys.push_back(node->m_key);
  }
  return keys;
}

void FillSet(ObjectKeyValueSet* set, const std::vector<ObjectKey>& keys)
{
  set->Clear();
  for (const ObjectKey& key : keys) {
    set->AddKeyValue(&key, 0);
  }
}

// A team member that can still take part: the side's own, alive and not a prisoner.
Role* UsableMember(int side, const ObjectKey& key)
{
  if (!InRange(key.Family(), kPersons)) {
    return nullptr;
  }
  GameObjNotifier* object = GetObjectInFamilies_01_ff(side, &key);
  if (!object || IsDestroyed(object) || object->ControlSide() != static_cast<uint32_t>(side)
      || IsCapturedCharacter(object)) {
    return nullptr;
  }
  return static_cast<Role*>(AsGameObj(object));
}

std::vector<ObjectKey> UsableAgents(int side, const MissionRepeatPlan& plan)
{
  std::vector<ObjectKey> agents;
  std::ranges::copy_if(plan.agents, std::back_inserter(agents),
                       [&](const ObjectKey& key) { return UsableMember(side, key) != nullptr; });
  return agents;
}

bool HasUsableAgent(int side, const MissionRepeatPlan& plan)
{
  return std::ranges::any_of(plan.agents, [&](const ObjectKey& key) { return UsableMember(side, key) != nullptr; });
}

enum class TeamState { Away, Home, Lost };

TeamState StateOf(int side, const MissionRepeatPlan& plan)
{
  bool agentLeft = false;
  for (const auto* members : {&plan.agents, &plan.decoys}) {
    for (const ObjectKey& key : *members) {
      Role* member = UsableMember(side, key);
      if (!member) {
        continue;
      }
      agentLeft = agentLeft || members == &plan.agents;
      if (member->m_missionKey.IsSet() || !DepartureOf(member)) {
        return TeamState::Away;
      }
    }
  }
  return agentLeft ? TeamState::Home : TeamState::Lost;
}

bool IsCandidate(TargetRule rule, int side, uint32_t targetFamily, GameObjNotifier* object, GameObjNotifier* system)
{
  if (!object || IsDestroyed(object) || (AsGameObj(object)->m_stateFlags & kStateEnroute) != 0
      || SystemOf(object) != system) {
    return false;
  }
  const uint32_t family = object->GetFamily();
  const uint32_t owner = object->ControlSide();
  switch (rule) {
    case TargetRule::SameKind:
      return family == targetFamily && owner == EnemyOf(side);
    case TargetRule::EnemyCharacter:
      return InRange(family, family::kCharacters) && owner == EnemyOf(side) && !IsCapturedCharacter(object);
    case TargetRule::Captive:
      return owner == static_cast<uint32_t>(side) && IsCapturedCharacter(object);
    case TargetRule::SameSystem:
      break;
  }
  return false;
}

void CollectContents(GameObjNotifier* parent, std::vector<GameObjNotifier*>* out)
{
  ChildIterator children(parent, &kContents, 1);
  for (children.Begin(); children.Current(); children.Advance()) {
    out->push_back(children.Current());
    CollectContents(children.Current(), out);
  }
}

// The targets a repeat can go after, best first: the same one while it is still there, then the others of its kind at
// that system in the order the side sees them.
std::vector<ObjectKey> CandidateTargets(const MissionRepeatPlan& plan, int side)
{
  const RepeatKind* kind = FindRepeatKind(plan.mission.Family());
  GameObjNotifier* system = GetObjectInFamilies_90_98(side, &plan.system);
  if (!kind || !system || IsDestroyed(system)) {
    return {};
  }
  if (kind->rule == TargetRule::SameSystem) {
    GameObjNotifier* target = GetObjectInFamilies_01_ff(side, &plan.target);
    return target && !IsDestroyed(target) ? std::vector<ObjectKey>{plan.target} : std::vector<ObjectKey>{};
  }
  std::vector<ObjectKey> targets;
  const uint32_t targetFamily = plan.target.Family();
  if (IsCandidate(kind->rule, side, targetFamily, GetObjectInFamilies_01_ff(side, &plan.target), system)) {
    targets.push_back(plan.target);
  }
  std::vector<GameObjNotifier*> contents;
  CollectContents(system, &contents);
  for (GameObjNotifier* object : contents) {
    ObjectKey key;
    object->GetKey(&key);
    if (key.m_value != plan.target.m_value && IsCandidate(kind->rule, side, targetFamily, object, system)) {
      targets.push_back(key);
    }
  }
  return targets;
}

std::string NameOf(GameObjNotifier* object)
{
  std::string name;
  if (object) {
    AsGameObj(object)->WriteName(&name, 1);
  }
  return name;
}

// "Sabotage mission to Kessel"
std::string MissionPhrase(const MissionRepeatPlan& plan, int side)
{
  return plan.name + " mission to " + NameOf(GetObjectInFamilies_90_98(side, &plan.system));
}

std::string RepeatingLine(const MissionRepeatPlan& plan, int side)
{
  return "\nNow repeating the " + MissionPhrase(plan, side) + ".";
}

std::string CannotRepeatLine(const MissionRepeatPlan& plan, int side)
{
  return "\nThe " + MissionPhrase(plan, side) + " cannot be repeated.";
}

UiTextNotice* ListedReport(HumanPlayer& player, uint32_t noticeId)
{
  return dynamic_cast<UiTextNotice*>(player.Dispatch()->m_trees[0].FindById(noticeId));
}

const ObjectKey* ReportedMission(UiNotice* notice)
{
  if (auto* report = dynamic_cast<Notice22*>(notice)) {
    return &report->m_key78;
  }
  if (auto* report = dynamic_cast<Notice23*>(notice)) {
    return &report->m_key78;
  }
  return nullptr;
}

bool IsReadyToGo(Role* member, const ActivityRequest& request)
{
  if (InRange(member->GetFamily(), family::kCharacters) && static_cast<Character*>(member)->m_commandKind != 0) {
    return false;
  }
  StatusCode status;
  return member->CheckActivityStatus2(&request, 1, &status) && status.m_second == static_cast<uint32_t>(-1);
}

void WriteKey(ostream* out, const ObjectKey& key)
{
  WriteValue32(out, &key.m_value);
}

void ReadKey(istream* in, ObjectKey* key)
{
  ReadValue32(in, &key->m_value);
}

void WriteKeys(ostream* out, const std::vector<ObjectKey>& keys)
{
  const auto count = static_cast<uint32_t>(keys.size());
  WriteValue32(out, &count);
  for (const ObjectKey& key : keys) {
    WriteKey(out, key);
  }
}

bool ReadKeys(istream* in, std::vector<ObjectKey>* keys)
{
  uint32_t count = 0;
  ReadValue32(in, &count);
  if (count > kMaxSavedEntries) {
    return false;
  }
  keys->resize(count);
  for (ObjectKey& key : *keys) {
    ReadKey(in, &key);
  }
  return true;
}

void WritePlan(ostream* out, const MissionRepeatPlan& plan)
{
  for (const ObjectKey* key : {&plan.mission, &plan.missionType, &plan.target, &plan.system}) {
    WriteKey(out, *key);
  }
  WriteKeys(out, plan.agents);
  WriteKeys(out, plan.decoys);
  WriteLegacyString(out, plan.name);
}

bool ReadPlan(istream* in, MissionRepeatPlan* plan)
{
  for (ObjectKey* key : {&plan->mission, &plan->missionType, &plan->target, &plan->system}) {
    ReadKey(in, key);
  }
  return ReadKeys(in, &plan->agents) && !plan->agents.empty() && ReadKeys(in, &plan->decoys)
         && ReadLegacyString(in, plan->name);
}

} // namespace

// ---- Per frame -------------------------------------------------------------------------------------------------

void MissionRepeats::Update(HumanPlayer& player)
{
  const auto tick = static_cast<uint32_t>(GameHookTickCount());
  if (tick != m_teamsTick) {
    RecordTeams(player.m_side);
    m_teamsTick = tick;
    std::erase_if(m_announcements,
                  [&](const Announcement& announcement) { return tick - announcement.tick > kAnnouncementTicks; });
  }
  ExamineNewNotices(player);
  if (!m_queued.empty()) {
    SendReady(player);
  }
}

// A finished mission's team is gone from it by the time its report arrives, so the agents and decoys are remembered
// while it is out.
void MissionRepeats::RecordTeams(int side)
{
  std::map<uint32_t, Team> teams;
  for (GameObjNotifier* object = GetFirstObjectInRangeForSide(side, &family::kMissions, 0); object;
       object = AsGameObj(object)->FindNextSibling(&family::kMissions, 0)) {
    if (!FindRepeatKind(object->GetFamily()) || object->ControlSide() != static_cast<uint32_t>(side)) {
      continue;
    }
    ObjectKey key;
    object->GetKey(&key);
    auto* mission = static_cast<Mission*>(AsGameObj(object));
    Team team{KeysOf(mission->m_team), KeysOf(mission->m_decoys)};
    if (team.agents.empty() && team.decoys.empty()) {
      if (auto known = m_teams.find(key.m_value); known != m_teams.end()) {
        teams.insert(*known);
      }
      continue;
    }
    teams[key.m_value] = std::move(team);
  }
  m_teams = std::move(teams);
}

void MissionRepeats::ExamineNewNotices(HumanPlayer& player)
{
  UiRecordTree& listed = player.Dispatch()->m_trees[0];
  bool changed = listed.Count() != m_examined.size();
  for (const auto& entry : listed.Entries()) {
    auto* notice = static_cast<UiNotice*>(entry.get());
    if (m_examined.contains(notice->m_id)) {
      continue;
    }
    changed = true;
    if (const ObjectKey* mission = ReportedMission(notice)) {
      MissionRepeatPlan plan;
      if (MakeOffer(player.m_side, *mission, &plan)) {
        m_offers[notice->m_id] = std::move(plan);
      }
    } else if (dynamic_cast<Notice20*>(notice)) {
      AnnounceArrival(player.m_side, notice->m_id, player);
    }
  }
  if (changed) {
    ForgetListedReports(player);
    std::erase_if(m_offers, [&](const auto& offer) { return !m_examined.contains(offer.first); });
  }
}

bool MissionRepeats::MakeOffer(int side, const ObjectKey& missionKey, MissionRepeatPlan* plan) const
{
  const RepeatKind* kind = FindRepeatKind(missionKey.Family());
  const auto team = m_teams.find(missionKey.m_value);
  GameObjNotifier* object = kind ? GetObjectInFamilies_40_80(side, &missionKey) : nullptr;
  if (!object || team == m_teams.end() || object->ControlSide() != static_cast<uint32_t>(side)) {
    return false;
  }
  auto* mission = static_cast<Mission*>(AsGameObj(object));
  const bool succeeded = mission->m_taskStatus == kTaskSucceeded;
  if (mission->m_completionStatus == kCompletionAborted
      || (kind->rule == TargetRule::SameSystem && succeeded && !kind->afterSuccess)) {
    return false;
  }
  plan->mission = missionKey;
  GetObjectRecordKey(object, &plan->missionType);
  plan->target = mission->m_objectiveKey;
  plan->system = mission->m_targetLocationKey;
  plan->agents = team->second.agents;
  plan->decoys = team->second.decoys;
  plan->name = NameOf(object);
  return HasUsableAgent(side, *plan) && !CandidateTargets(*plan, side).empty();
}

void MissionRepeats::AnnounceArrival(int side, uint32_t noticeId, HumanPlayer& player)
{
  auto* arrival = static_cast<Notice20*>(player.Dispatch()->m_trees[0].FindById(noticeId));
  const ObjectKey& arrived = arrival->m_unitKey;
  const auto queued = std::ranges::find_if(m_queued, [&](const MissionRepeatPlan& plan) {
    return Contains(plan.agents, arrived) || Contains(plan.decoys, arrived);
  });
  if (queued != m_queued.end()) {
    m_announcedEarly.insert(queued->agents.front().m_value);
    arrival->m_text1 += HasUsableAgent(side, *queued) && !CandidateTargets(*queued, side).empty()
                            ? RepeatingLine(*queued, side)
                            : CannotRepeatLine(*queued, side);
    return;
  }
  const auto announced = std::ranges::find_if(
      m_announcements, [&](const Announcement& announcement) { return Contains(announcement.members, arrived); });
  if (announced != m_announcements.end()) {
    arrival->m_text1 += announced->line;
    m_announcements.erase(announced);
  }
}

void MissionRepeats::SendReady(HumanPlayer& player)
{
  const auto tick = static_cast<uint32_t>(GameHookTickCount());
  for (size_t index = 0; index < m_queued.size();) {
    const TeamState state = StateOf(player.m_side, m_queued[index]);
    if (state == TeamState::Away) {
      ++index;
      continue;
    }
    const MissionRepeatPlan plan = m_queued[index];
    m_queued.erase(m_queued.begin() + static_cast<std::ptrdiff_t>(index));
    const bool sent = state == TeamState::Home && Send(player, plan);
    Announce(plan, sent ? RepeatingLine(plan, player.m_side) : CannotRepeatLine(plan, player.m_side), tick);
  }
}

void MissionRepeats::DropQueuedIf(const std::function<bool(const MissionRepeatPlan&)>& drop)
{
  std::erase_if(m_queued, [&](const MissionRepeatPlan& plan) {
    if (!drop(plan)) {
      return false;
    }
    m_announcedEarly.erase(plan.agents.front().m_value);
    return true;
  });
}

void MissionRepeats::Announce(const MissionRepeatPlan& plan, std::string line, uint32_t tick)
{
  if (m_announcedEarly.erase(plan.agents.front().m_value) != 0) {
    return;
  }
  Announcement announcement{plan.agents, std::move(line), tick};
  std::ranges::copy(plan.decoys, std::back_inserter(announcement.members));
  m_announcements.push_back(std::move(announcement));
}

// Sends the members that are free and together with the first such agent against the best target the game accepts,
// and tells whether it did. When nothing is accepted the order still goes out, so the advisor explains why.
bool MissionRepeats::Send(HumanPlayer& player, const MissionRepeatPlan& plan)
{
  const int side = player.m_side;
  const std::vector<ObjectKey> targets = CandidateTargets(plan, side);
  const std::vector<ObjectKey> usableAgents = UsableAgents(side, plan);
  if (targets.empty() || usableAgents.empty()) {
    return false;
  }
  ObjectKeyValueSet team;
  FillSet(&team, usableAgents);
  ObjectKey firstTarget = targets.front();
  std::unique_ptr<OrderBase> order(player.CreateOrder(kMissionOrderKind, &team, &firstTarget));
  auto* missionOrder = dynamic_cast<MissionOrder240*>(order.get());
  if (!missionOrder) {
    return false;
  }
  missionOrder->m_missionKey.m_value = plan.missionType.m_value;
  ActivityRequest request;
  missionOrder->BuildRequest(&request);

  GameObjNotifier* departure = nullptr;
  std::vector<ObjectKey> agents;
  std::vector<ObjectKey> decoys;
  for (const auto* members : {&usableAgents, &plan.decoys}) {
    for (const ObjectKey& key : *members) {
      Role* member = UsableMember(side, key);
      if (!member || !IsReadyToGo(member, request)) {
        continue;
      }
      if (!departure && members == &usableAgents) {
        departure = DepartureOf(member);
      }
      if (departure && DepartureOf(member) == departure) {
        (members == &usableAgents ? agents : decoys).push_back(key);
      }
    }
  }
  bool accepted = false;
  if (!agents.empty()) {
    FillSet(&team, agents);
    missionOrder->SetTeam(&team);
    FillSet(&missionOrder->m_characters, decoys);
    const auto target = std::ranges::find_if(targets, [&](const ObjectKey& candidate) {
      missionOrder->SetTarget(&candidate);
      StatusCode status;
      return missionOrder->CheckValid(&status) && status.m_second == static_cast<uint32_t>(-1);
    });
    accepted = target != targets.end();
    missionOrder->SetTarget(accepted ? &*target : &firstTarget);
  }
  m_submitting = true;
  player.SubmitOrder(static_cast<PlayerMessage*>(order.release()), 0);
  m_submitting = false;
  return accepted;
}

// ---- Requests from the message index ---------------------------------------------------------------------------

bool MissionRepeats::HasOffer(uint32_t noticeId) const
{
  return m_offers.contains(noticeId);
}

const MissionRepeatPlan* MissionRepeats::Offer(uint32_t noticeId) const
{
  const auto offer = m_offers.find(noticeId);
  return offer == m_offers.end() ? nullptr : &offer->second;
}

MissionRepeats::Result MissionRepeats::Request(HumanPlayer& player, uint32_t noticeId)
{
  const auto offer = m_offers.find(noticeId);
  if (offer == m_offers.end()) {
    return Result::Unavailable;
  }
  const MissionRepeatPlan plan = offer->second;
  std::erase_if(m_offers, [&](const auto& other) { return other.second.mission.m_value == plan.mission.m_value; });
  DropQueuedIf([&](const MissionRepeatPlan& other) {
    return std::ranges::any_of(other.agents, [&](const ObjectKey& key) {
      return Contains(plan.agents, key) || Contains(plan.decoys, key);
    }) || std::ranges::any_of(other.decoys, [&](const ObjectKey& key) {
      return Contains(plan.agents, key) || Contains(plan.decoys, key);
    });
  });

  Result result = Result::Queued;
  if (StateOf(player.m_side, plan) == TeamState::Away) {
    m_queued.push_back(plan);
  } else {
    result = Send(player, plan) ? Result::Sent : Result::Unavailable;
  }
  if (UiTextNotice* report = ListedReport(player, noticeId)) {
    switch (result) {
      case Result::Queued:
        report->m_text1 += "\nThe mission will be repeated as soon as the team is back.";
        break;
      case Result::Sent:
        report->m_text1 += "\nThe mission is being repeated.";
        break;
      case Result::Unavailable:
        report->m_text1 += "\nThe mission can no longer be repeated.";
        break;
    }
  }
  return result;
}

void MissionRepeats::OnPlayerOrder(OrderBase* order)
{
  if (m_submitting || (m_offers.empty() && m_queued.empty() && m_announcements.empty()) || !order) {
    return;
  }
  std::vector<ObjectKey> ordered = KeysOf(*order->GetTeam());
  if (auto* missionOrder = dynamic_cast<MissionOrder*>(order)) {
    std::ranges::copy(KeysOf(missionOrder->m_characters), std::back_inserter(ordered));
  }
  const auto involves = [&](const MissionRepeatPlan& plan) {
    return std::ranges::any_of(ordered, [&](const ObjectKey& key) {
      return Contains(plan.agents, key) || Contains(plan.decoys, key);
    });
  };
  std::erase_if(m_offers, [&](const auto& offer) { return involves(offer.second); });
  DropQueuedIf(involves);
  std::erase_if(m_announcements, [&](const Announcement& announcement) {
    return std::ranges::any_of(ordered, [&](const ObjectKey& key) { return Contains(announcement.members, key); });
  });
}

// ---- Saving ----------------------------------------------------------------------------------------------------

void MissionRepeats::ForgetListedReports(HumanPlayer& player)
{
  m_examined.clear();
  for (const auto& entry : player.Dispatch()->m_trees[0].Entries()) {
    m_examined.insert(static_cast<UiNotice*>(entry.get())->m_id);
  }
}

void MissionRepeats::Write(ostream* out) const
{
  const auto offers = static_cast<uint32_t>(m_offers.size());
  WriteValue32(out, &offers);
  for (const auto& [noticeId, plan] : m_offers) {
    WriteValue32(out, &noticeId);
    WritePlan(out, plan);
  }
  const auto queued = static_cast<uint32_t>(m_queued.size());
  WriteValue32(out, &queued);
  for (const MissionRepeatPlan& plan : m_queued) {
    WritePlan(out, plan);
  }
}

bool MissionRepeats::Read(istream* in)
{
  m_offers.clear();
  m_queued.clear();
  uint32_t offers = 0;
  ReadValue32(in, &offers);
  if (offers > kMaxSavedEntries) {
    return false;
  }
  for (uint32_t index = 0; index < offers; ++index) {
    uint32_t noticeId = 0;
    ReadValue32(in, &noticeId);
    if (!ReadPlan(in, &m_offers[noticeId])) {
      return false;
    }
  }
  uint32_t queued = 0;
  ReadValue32(in, &queued);
  if (queued > kMaxSavedEntries) {
    return false;
  }
  m_queued.resize(queued);
  return std::ranges::all_of(m_queued, [&](MissionRepeatPlan& plan) { return ReadPlan(in, &plan); });
}
