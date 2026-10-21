#include "lan_battle_smoke.h"

#include "app/battle_flow.h"
#include "app/game_session.h"
#include "events/game_manager.h"
#include "fleet/capship.h"
#include "fleet/fleet.h"
#include "fleet/fleet_iterator.h"
#include "galaxy/system.h"
#include "gameobj/game_hooks.h"
#include "gameobj/object_queries.h"
#include "strategy_ui/human_player.h"
#include "lan_game.h"

namespace reb::native {
namespace {

bool IsUsableFleetWithShips(GameObjNotifier* object)
{
  if (!object || (static_cast<Fleet*>(object)->m_stateFlags & 1) == 0) return false;
  CapShipIterator ships(object, 1, object->ControlSide());
  return ships.HasCurrent() != 0;
}

System* ParentSystem(GameObjNotifier* fleet)
{
  GameObjNotifier* parent = fleet ? fleet->m_parent : nullptr;
  if (!parent || parent->GetFamily() < family::kSystem || parent->GetFamily() >= family::kSystem + 8) return nullptr;
  return static_cast<System*>(parent);
}

} // namespace

bool PrepareLanBattleSmoke(std::string& error)
{
  error.clear();
  BattleFlow* flow = GetBattleFlow();
  GameManager* manager = GetGameManager();
  if (!flow || flow->m_state != kFlowStrategic || !manager || !manager->Root()
      || !flow->StepDriver() || flow->StepDriver()->m_phase != 13) {
    error = "Prepare the battle after the strategic scheduler, before the finish phase.";
    return false;
  }

  Fleet* alliance = nullptr;
  Fleet* empire = nullptr;
  for (GameObjNotifier* object = GetFirstMasterInFamilies_08_10(); object;
       object = static_cast<GameObj*>(object)->FindNextSibling(&family::kFleets, 1)) {
    if (!IsUsableFleetWithShips(object) || !ParentSystem(object)) continue;
    if (object->ControlSide() == 1 && !alliance) alliance = static_cast<Fleet*>(object);
    if (object->ControlSide() == 2 && !empire) empire = static_cast<Fleet*>(object);
    if (alliance && empire) break;
  }
  if (!alliance || !empire) {
    error = "The starting campaign has no opposing usable fleets with capital ships.";
    return false;
  }

  System* destination = ParentSystem(alliance);
  if (!destination) {
    error = "The Alliance fleet has no system.";
    return false;
  }
  ObjectKey destinationKey;
  destination->GetKey(&destinationKey);
  NotifyContext ctx;
  ctx.m_flag04 = 1;
  ctx.m_flag08 = 3;
  if (ParentSystem(empire) != destination && !empire->Reparent(&destinationKey, &ctx)) {
    error = "Could not move the Empire fleet into the battle system.";
    return false;
  }
  if (!destination->HasFleetsOrFighterSquadrons(1) || !destination->HasFleetsOrFighterSquadrons(2)
      || !destination->RecomputeBlockadeAndBattlePending(&ctx)
      || (destination->m_flags & system_flag::kBattlePending) == 0 || !destination->GetDataFlagBit3()) {
    error = "The opposing fleets did not create a pending battle.";
    return false;
  }
  // The posted system event normally sets this latch during the next event phase.
  // The smoke setup runs between phases, so make the pending battle visible to the next frame.
  manager->Root()->m_unk70 = 1;
  return true;
}

// Answers as "Simulate Results" (HumanPlayer::SetVote(1, 0)): accepted with no withdrawal threshold.
void AnswerLanBattleSmokeVotes()
{
  GameSession* session = GetGameSession();
  BattleFlow* flow = GetBattleFlow();
  if (!session || !session->m_human || !flow) return;
  HumanPlayer& human = *session->m_human;

  if (flow->m_state == kFlowBattleWaitForType) {
    if (human.m_voteState == 0) {
      human.m_voteState = 1;
      human.m_voteTime.m_value = 0.0f;
    }
    return;
  }
  if (flow->m_state == kFlowBattleWaitForDone) {
    human.m_voteState = 3;
    human.m_voteTime.m_value = 0.0f;
    return;
  }

  LanGame* game = CurrentLanGame();
  if (!game || game->IsHost() || !game->Playing() || human.m_voteState != 0) return;
  human.m_voteState = 1;
  human.m_voteTime.m_value = 0.0f;
  if (game->SubmitBattleVote()) return;
  human.m_voteState = 3;
  human.m_voteTime.m_value = 0.0f;
  if (game->SubmitBattleVote()) return;
  human.m_voteState = 0;
}

} // namespace reb::native
