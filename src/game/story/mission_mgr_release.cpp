#include "gameobj/object_key.h"
#include "mission/mission_exec_decl.h"
#include "story/mission_mgr.h"

// Creating missions for an order and freeing persons that are stuck in transit (0x549bf0, 0x54c7d0).

namespace
{

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr uint32_t kControlSideMask = 0xc0;
constexpr uint32_t kFamilyCharactersBegin = 0x30;
constexpr uint32_t kFamilyCharactersEnd = 0x3c;
constexpr uint32_t kStateCompleted = 4;
constexpr uint32_t kStateHyperdriveActive = 0x800;

} // namespace

int MissionMgr::CreateMissionForRequest(const ActivityRequest* request, int userId, int userId2, int side,
                                        ObjectKey* typeKey, ObjectKey* targetKey, ObjectKeyValueSet* team,
                                        ObjectKeyValueSet* decoys, NotifyContext* ctx)
{
  ObjectKey origin;
  StatusCode status(kStatusCategory, kStatusOk);
  int ok = FindCommonOrigin(team, &origin, &status) ? 1 : 0;
  if (origin.OrNull()) {
    ok = CreateMission(userId, userId2, side, const_cast<ObjectKey*>(&request->m_target), typeKey, targetKey, &origin,
                       team, decoys, nullptr, 0, 0, ctx)
                 && ok
             ? 1
             : 0;
  }
  return ok;
}

int MissionMgr::CollectStrandedPersons(GameObj* object, int* found, uint32_t* side, uint32_t* flag, ObjectKey* key,
                                       ObjectKeyValueSet* persons, NotifyContext* ctx)
{
  int ok = 1;
  ObjectKey destination;
  RoleIterator roles(object, 3);
  for (roles.Begin(); roles.Current(); roles.Advance()) {
    Role* role = static_cast<Role*>(roles.Current());
    if (!(role->m_stateFlags & mission_exec::kStateAutorouting) && !(role->m_flags78 & role_flag::kAdrift)) {
      continue;
    }

    bool release = true;
    if (role->GetFamily() >= kFamilyCharactersBegin && role->GetFamily() < kFamilyCharactersEnd) {
      ObjectKey holder;
      if (static_cast<Character*>(role)->GetHolderKey(&holder)->IsSet()) {
        release = false;
      }
    }
    if (release) {
      GameObjNotifier* holderObject = nullptr;
      ok = role->GetParent(&holderObject) && ok ? 1 : 0;
      if (holderObject) {
        auto* parent = static_cast<GameObj*>(holderObject);
        const bool parentLost = (parent->m_stateFlags & mission_exec::kStateDestroyed)
                                || ((role->m_flags ^ parent->m_flags) & kControlSideMask);
        if (!(role->m_stateFlags & mission_exec::kStateDestroyed) && parentLost && (m_stateFlags & kStateCompleted)
            && !(m_stateFlags & kStateHyperdriveActive)) {
          const uint32_t roleSide = role->ControlSide();
          const uint32_t adrift = (role->m_flags78 >> 6) & 1;
          ok = FindPersonDestination(role, 0, &destination) && ok ? 1 : 0;
          if (*found == 0) {
            *found = 1;
            *side = roleSide;
            *flag = adrift;
            *key = destination;
          }
          if (*side == roleSide && *flag == adrift && key->m_value == destination.m_value) {
            ObjectKey personKey;
            ok = persons->AddKeyValue(role->GetKey(&personKey), 0) && ok ? 1 : 0;
          } else {
            release = false;
          }
        }
      }
    }
    if (release) {
      const int cleared = role->SetAutorouting(0, ctx) && ok ? 1 : 0;
      ok = role->SetAdrift(0, ctx) && cleared ? 1 : 0;
    }
  }
  return ok;
}
