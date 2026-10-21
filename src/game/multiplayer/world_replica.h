#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

class GameManager;
class GameObjNotifier;

namespace reb::multiplayer
{

// A complete strategic object snapshot. Apply keeps the addresses of surviving objects stable.
// Reset before destroying/reloading the GameManager to release retired UI-facing objects.
class WorldReplicaState
{
public:
  WorldReplicaState();
  ~WorldReplicaState();
  WorldReplicaState(const WorldReplicaState&) = delete;
  WorldReplicaState& operator=(const WorldReplicaState&) = delete;
  bool Capture(GameManager& manager, std::vector<uint8_t>& bytes, std::string& error);
  bool Apply(GameManager& manager, std::span<const uint8_t> bytes, std::string& error);
  void Reset();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace reb::multiplayer
