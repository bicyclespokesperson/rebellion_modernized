#include "fleet/fleet.h"
#include <new>

#include "character/character.h"
#include "character/role.h"
#include "data/game_math.h"
#include "economy/manu_mgr_leaves.h"
#include "events/event_post.h"
#include "galaxy/system.h"
#include "gameobj/game_obj_iterator.h"
#include "gameobj/object_family.h"
#include "gameobj/object_queries.h"

extern int g_systemEventKindsBuilt; // 0x6b2ba0
extern EventKindSet g_systemEventKinds4; // 0x6b2b90
extern EventKindSet g_systemEventKinds8; // 0x6b2b88
extern EventKindSet g_systemEventKinds10; // 0x6b2ba8
extern EventKindSet g_systemEventKinds40; // 0x6b2b98

namespace
{

constexpr uint32_t kDestroyedState = 0x8;
constexpr uint32_t kUsableState = 0x1;
constexpr uint32_t kLockedState = 0x4000;
constexpr uint32_t kFleetBlockadeFlag = 0x20;
constexpr uint32_t kAlwaysState = 4;
constexpr FamilyRange kFleetRange(0x08, 0x10);
constexpr FamilyRange kFacilityRange(0x20, 0x30);
constexpr FamilyRange kUnitFilterRange(0x10, 0x20);
constexpr FamilyRange kManagerRange(0xa0, 0xb0);
constexpr FamilyRange kRoleRange(0x30, 0x40);
constexpr FamilyRange kMissionRange(0x40, 0x80);
constexpr FamilyRange kTroopRange(0x10, 0x14);
constexpr FamilyRange kFighterRange(0x1c, 0x20);

bool isValidChildFamily(uint32_t family)
{
  const FamilyRange* allowed[] = {&kFacilityRange, &kRoleRange,    &kFighterRange, &kFleetRange,
                                  &kManagerRange,  &kMissionRange, &kTroopRange};
  for (const FamilyRange* range : allowed)
  {
    if (family >= range->m_first && family < range->m_second)
      return true;
  }
  return false;
}

bool sameControl(const GameObjRoot* a, const GameObjRoot* b)
{
  return ((a->m_flags ^ b->m_flags) & 0xc0) == 0;
}

// Children that are neither destroyed-and-locked must be controlled by the same side as the system.
void checkChildrenControl(System* system, ChildIterator& it, bool& ok, bool skipOnMissionRoles)
{
  for (it.Begin(); it.Current() && ok; it.Advance())
  {
    auto* child = static_cast<GameObj*>(it.Current());
    const bool destroyed = (child->m_stateFlags & kDestroyedState) != 0;
    const bool locked = (child->m_stateFlags & kLockedState) != 0;
    if (destroyed && locked)
      continue;
    if (skipOnMissionRoles && (static_cast<Role*>(child)->m_flags78 & role_flag::kOnMission))
      continue;
    ok = sameControl(child, system);
  }
}

} // namespace

int System::Validate()
{
  int result = GameObj::Validate();
  if (result)
  {
    if (!(m_stateFlags & kDestroyedState))
    {
      const int first = CountShipyardManuMgrChildren(this, ControlSide(), 4);
      const int second = CountConstructionYardManuMgrChildren(this, ControlSide(), 4);
      const int third = CountTrainingFacilManuMgrChildren(this, ControlSide(), 4);
      result = first == 1 && second == 1 && third == 1 ? 1 : 0;
    }
    if (result)
    {
      result = IsInRange(m_energyAllocated, 0, m_energy, "Invalid SystemEnergyAllocated value!");
      if (result)
        result = IsInRange(m_rawMaterialAllocated, 0, m_rawMaterial, "Invalid SystemRawMaterialAllocated value!");
    }
  }

  bool ok = false;
  GameObjIterator children(this, kAlwaysState);
  for (children.Begin(); children.Current(); children.Advance())
  {
    ok = false;
    if (result == 0)
      break;
    auto* child = static_cast<GameObj*>(children.Current());
    ObjectKey key;
    result = isValidChildFamily(child->GetKey(&key)->Family()) ? 1 : 0;
  }
  if (children.Current() == nullptr && result != 0)
  {
    ok = true;
    ChildIterator facilities(this, &kFacilityRange, kAlwaysState);
    checkChildrenControl(this, facilities, ok, false);
    ChildIterator combatUnits(this, &kUnitFilterRange, kAlwaysState);
    checkChildrenControl(this, combatUnits, ok, false);
    ChildIterator managers(this, &kManagerRange, kAlwaysState);
    checkChildrenControl(this, managers, ok, false);
    RoleIterator roles(this, kAlwaysState);
    checkChildrenControl(this, roles, ok, true);
  }

  ChildIterator fleets(this, &kFleetRange, 3);
  for (fleets.Begin(); ok && fleets.Current(); fleets.Advance())
  {
    auto* fleet = static_cast<GameObj*>(fleets.Current());
    const uint32_t expectedBlockade =
      (fleet->m_stateFlags & kUsableState) ? ((m_flags & system_flag::kBlockade) != 0 ? 1 : 0) : 0;
    const uint32_t fleetFlags = static_cast<Fleet*>(fleet)->m_fleetFlags;
    ok = ((fleetFlags & kFleetBlockadeFlag) != 0 ? 1 : 0) == expectedBlockade;
  }
  return ok;
}

int ReapplySystemCharacterViews(GameObj* self, NotifyContext* ctx)
{
  ObjectKey ownKey;
  self->GetKey(&ownKey);
  if (GameObjNotifier* master = GetMasterInFamilies_01_ff(&ownKey))
  {
    CharacterIterator masterCharacters(master, 3);
    for (masterCharacters.Begin(); masterCharacters.Current(); masterCharacters.Advance())
    {
      auto* character = static_cast<Character*>(masterCharacters.Current());
      if (character->m_commandKind != 0)
        character->ApplyAllToView(self->ViewSide(), ctx);
    }
  }
  CharacterIterator characters(self, 3);
  for (characters.Begin(); characters.Current();)
  {
    auto* character = static_cast<Character*>(characters.Current());
    characters.Advance();
    if (character->m_commandKind == 0)
      continue;
    ObjectKey characterKey;
    character->GetKey(&characterKey);
    GameObjNotifier* masterCharacter = GetMasterInFamilies_30_3c(&characterKey);
    if (!masterCharacter)
      continue;
    ObjectKey masterParent;
    ObjectKey ownParent;
    static_cast<GameObj*>(masterCharacter)->GetValidParentKey(&masterParent);
    character->GetValidParentKey(&ownParent);
    if (!masterParent.IsSet() || masterParent.m_value != ownParent.m_value)
      character->UpdateSelfAndDescendants(ctx);
    else
      character->ApplyAllToView(self->ViewSide(), ctx);
  }
  return 1;
}

// ---- static event-kind sets ---------------------------------------------------------------------------

int System::BuildEventKinds()
{
  if (g_systemEventKindsBuilt)
    return 1;
  g_systemEventKindsBuilt = 1;
  return InitEventKinds(0, &g_systemEventKinds4) && g_systemEventKinds4.Add(0x200, 1) != nullptr &&
         g_systemEventKinds4.Add(0x214, 1) != nullptr && g_systemEventKinds4.Add(0x216, 1) != nullptr &&
         InitEventKinds(0, &g_systemEventKinds8) && g_systemEventKinds8.Add(0x201, 1) != nullptr &&
         g_systemEventKinds8.Add(0x202, 1) != nullptr && g_systemEventKinds8.Add(0x200, 1) != nullptr &&
         InitEventKinds(0, &g_systemEventKinds10) && g_systemEventKinds10.Add(0x201, 1) != nullptr &&
         g_systemEventKinds10.Add(0x202, 1) != nullptr && g_systemEventKinds10.Add(0x200, 1) != nullptr &&
         g_systemEventKinds10.Add(0x220, 1) != nullptr && g_systemEventKinds10.Add(0x221, 1) != nullptr &&
         g_systemEventKinds10.Add(0x222, 1) != nullptr && g_systemEventKinds10.Add(0x223, 1) != nullptr &&
         g_systemEventKinds10.Add(0x234, 1) != nullptr && InitEventKinds(0, &g_systemEventKinds40) &&
         g_systemEventKinds40.Add(0x250, 1) != nullptr;
}

int System::SelectEventKinds(uint32_t type, EventKindSet** out)
{
  *out = nullptr;
  if (BuildEventKinds())
  {
    switch (type & ~1u)
    {
      case 4:
        *out = &g_systemEventKinds4;
        return 1;
      case 8:
        *out = &g_systemEventKinds8;
        return 1;
      case 0x10:
        *out = &g_systemEventKinds10;
        return 1;
      case 0x40:
        *out = &g_systemEventKinds40;
        return 1;
      default:
        break;
    }
  }
  return 0;
}
EventKindSet g_systemEventKinds10{};
EventKindSet g_systemEventKinds40{};
EventKindSet g_systemEventKinds4{};
EventKindSet g_systemEventKinds8{};
int g_systemEventKindsBuilt{};
