#include "multiplayer/lan_player.h"

#include "gameobj/notif_payload.h"
#include "port/native/lan_game.h"

LanPlayer::LanPlayer(int side) : PlayerController(side) {}

void LanPlayer::ProcessPlayerFrame()
{
  m_syncTarget = 0;
}

void LanPlayer::HandleNotification(NotifPayload* notice)
{
  if (auto* game = reb::native::CurrentLanGame(); game && game->ForwardNotification(notice)) {
    return;
  }
  delete notice;
}

void LanPlayer::BeginVote(int id, ObjectKey* key)
{
  m_voteId = id;
  m_voteKey = *key;
  m_voteState = 0;
  m_voteTime.m_value = 0;
  if (auto* game = reb::native::CurrentLanGame()) {
    game->RequestBattleVote(id, key->m_value, false);
  }
}

void LanPlayer::ResetVote(int id, ObjectKey* key)
{
  m_voteId = id;
  m_voteKey = *key;
  m_voteState = 0;
  m_voteTime.m_value = 0;
  if (auto* game = reb::native::CurrentLanGame()) {
    game->RequestBattleVote(id, key->m_value, true);
  }
}
