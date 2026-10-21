#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace reb::native
{

struct CampaignCheckpointMetadata
{
  std::string campaignId;
  std::string sessionId;
  uint32_t gameTick = 0;
  uint8_t hostSide = 1;
  std::string displayName;
  int64_t unixTimestamp = 0;
  uint32_t formatVersion = 1;
  uint32_t protocolVersion = 1;
};

struct CampaignCheckpointInfo
{
  CampaignCheckpointMetadata metadata;
  bool automatic = false;
  std::filesystem::path path;
};

struct CampaignCheckpoint
{
  CampaignCheckpointMetadata metadata;
  bool automatic = false;
  std::filesystem::path path;
  std::vector<uint8_t> gameState;
};

struct CampaignCheckpointResult
{
  std::filesystem::path path;
  std::string error;
  explicit operator bool() const noexcept { return error.empty() && !path.empty(); }
};

class CampaignCheckpoints
{
  public:
  explicit CampaignCheckpoints(std::filesystem::path directory);

  CampaignCheckpointResult Save(const CampaignCheckpointMetadata& metadata, std::span<const uint8_t> gameState,
                                bool automatic, size_t retainAutomatic = 10) const noexcept;
  std::optional<CampaignCheckpoint> Load(const std::filesystem::path& path, std::string* error = nullptr) const noexcept;
  std::vector<CampaignCheckpointInfo> List() const noexcept;

  private:
  std::filesystem::path directory_;
};

// Accumulates only active, connected simulation time. Reset it after loading or
// starting a different campaign; acknowledge the saved tick after a successful write.
class CampaignCheckpointCadence
{
  public:
  using Duration = std::chrono::steady_clock::duration;
  explicit CampaignCheckpointCadence(Duration interval = std::chrono::minutes(5)) noexcept;
  void Reset(uint32_t currentTick = 0) noexcept;
  bool Update(Duration elapsed, bool connected, bool paused, uint32_t currentTick) noexcept;
  void Saved(uint32_t tick) noexcept;

  private:
  Duration interval_;
  Duration activeTime_{};
  uint32_t savedTick_ = 0;
};

} // namespace reb::native
