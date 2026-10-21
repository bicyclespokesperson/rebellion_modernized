#pragma once

#include "app/player_controller_decl.h"

constexpr int kPlayerLan = 6;

class LanPlayer : public PlayerController
{
  public:
  explicit LanPlayer(int side = 0);
  int GetKind() override { return kPlayerLan; }
  int StepInitialization() override { return 1; }
  int StepPlayer() override { return 1; }
  int RouteOutgoingMessage(PlayerMessage*) override { return 1; }
  void ProcessPlayerFrame() override;
  void HandleNotification(NotifPayload* notice) override;
  void BeginVote(int id, ObjectKey* key) override;
  void ResetVote(int id, ObjectKey* key) override;
  void RequestSyncTarget(unsigned) override { m_syncTarget = 0; }
  void AcknowledgeSync(int) override { m_syncTarget = 0; }
};
