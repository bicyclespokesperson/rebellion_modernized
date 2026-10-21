#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "port/native/campaign_checkpoints.h"
#include "port/native/lan_transport.h"

class MessageBase;
class NotifPayload;
class RebellionApplication;

namespace reb::native
{

struct LanSettings {
  int hostSide = 1;
  int galaxySize = 1;
  int victoryMode = 1;
  int seed = 0;
};

class LanGame
{
  public:
  LanGame();
  ~LanGame();
  LanGame(const LanGame&) = delete;
  LanGame& operator=(const LanGame&) = delete;

  bool Host(const std::string& name, uint16_t port = lan::kDefaultPort);
  bool Join(const std::string& address, uint16_t port = lan::kDefaultPort);
  void Discover(uint16_t port = lan::kDefaultPort);
  void Close();
  void Poll(RebellionApplication& app);
  void AfterStep(RebellionApplication& app);
  void FinishGame(int movieState);
  void SetReady(bool ready);
  void SetSettings(const LanSettings& settings);
  bool StartNew(RebellionApplication& app);
  bool Resume(RebellionApplication& app, const std::filesystem::path& checkpoint);
  bool SaveCheckpoint(RebellionApplication& app, bool automatic = false);
  bool SaveBeforeExit(RebellionApplication& app);
  bool Submit(MessageBase* message, int senderSide);
  bool ForwardNotification(NotifPayload* notice);
  bool RequestBattleVote(int voteId, uint32_t objectKey, bool done);
  bool SubmitBattleVote();
  void TogglePause();
  void Reconnect();

  bool Active() const;
  bool IsHost() const;
  bool IsGuest() const;
  bool Connected() const;
  bool Playing() const;
  bool CanSimulate() const;
  bool Ready() const;
  bool PeerReady() const;
  const LanSettings& Settings() const;
  const std::string& StatusText() const;
  const std::string& LastSaveText() const;
  std::vector<lan::Advertisement> NearbyGames() const;
  std::vector<CampaignCheckpointInfo> Checkpoints() const;
  uint16_t Port() const;
  bool Paused() const;
  uint64_t AcceptedOrders() const;
  uint32_t ReplicaTick() const;

  private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  bool WriteCheckpoint(RebellionApplication& app, bool automatic, bool requireCompleteStep);
};

LanGame* CurrentLanGame();

} // namespace reb::native
