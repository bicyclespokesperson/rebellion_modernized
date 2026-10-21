#include "port/native/campaign_checkpoints.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace
{
void Require(bool condition)
{
  if (!condition) std::abort();
}

reb::native::CampaignCheckpointMetadata Metadata(uint32_t tick, int64_t timestamp,
                                                  std::string campaign = "campaign-../unsafe")
{
  return {std::move(campaign), "session-1", tick, 1, "Checkpoint", timestamp, 1, 7};
}

#ifndef _WIN32
void TestConcurrentProcesses(const std::filesystem::path& directory)
{
  using namespace reb::native;
  std::filesystem::create_directories(directory);
  int start[2];
  Require(pipe(start) == 0);
  const pid_t child = fork();
  Require(child >= 0);
  if (child == 0) {
    close(start[1]);
    char ready = 0;
    if (read(start[0], &ready, 1) != 1) _exit(1);
    close(start[0]);
    CampaignCheckpoints storage(directory);
    for (uint32_t index = 0; index < 5; ++index) {
      const std::vector<uint8_t> state(65536, static_cast<uint8_t>(index + 100));
      if (!storage.Save(Metadata(200 + index, 777, "concurrent"), state, true, 20)) _exit(2);
    }
    _exit(0);
  }
  close(start[0]);
  const char ready = 1;
  Require(write(start[1], &ready, 1) == 1);
  close(start[1]);
  CampaignCheckpoints storage(directory);
  for (uint32_t index = 0; index < 5; ++index) {
    const std::vector<uint8_t> state(65536, static_cast<uint8_t>(index + 10));
    Require(static_cast<bool>(storage.Save(Metadata(100 + index, 777, "concurrent"), state, true, 20)));
  }
  int status = 0;
  Require(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
  const auto listed = storage.List();
  Require(listed.size() == 10);
  for (const auto& item : listed) {
    const auto loaded = storage.Load(item.path);
    Require(loaded && loaded->gameState.size() == 65536);
    const uint32_t tick = loaded->metadata.gameTick;
    const uint8_t expected = tick >= 200 ? static_cast<uint8_t>(tick - 200 + 100)
                                         : static_cast<uint8_t>(tick - 100 + 10);
    Require((tick >= 100 && tick <= 104) || (tick >= 200 && tick <= 204));
    Require(loaded->gameState.front() == expected && loaded->gameState.back() == expected);
  }
  for (const auto& entry : std::filesystem::directory_iterator(directory))
    Require(entry.path().extension() == ".rcp");
}
#endif
}

int main()
{
  using namespace reb::native;
  const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto root = std::filesystem::temp_directory_path() / ("reb-checkpoints-" + std::to_string(nonce));
  const auto directory = root / "checkpoints";
  CampaignCheckpoints storage(directory);
  const std::vector<uint8_t> state{0, 1, 2, 3, 255};

  auto first = storage.Save(Metadata(10, 100), state, true, 2);
  Require(static_cast<bool>(first));
  Require(first.path.parent_path() == directory);
  Require(first.path.filename().string().find("unsafe") == std::string::npos);
  auto loaded = storage.Load(first.path);
  Require(loaded && loaded->gameState == state && loaded->metadata.gameTick == 10 && loaded->automatic);

  auto manual = storage.Save(Metadata(11, 101), state, false);
  Require(static_cast<bool>(manual));
  auto second = storage.Save(Metadata(12, 102), state, true, 2);
  Require(static_cast<bool>(second));
  auto third = storage.Save(Metadata(13, 103), state, true, 2);
  Require(static_cast<bool>(third));
  const auto listed = storage.List();
  Require(listed.size() == 3);
  Require(listed.front().metadata.gameTick == 13);
  Require(!std::filesystem::exists(first.path));
  Require(std::filesystem::exists(manual.path));

  auto otherCampaign = storage.Save(Metadata(20, 110, "campaign-2"), state, true, 2);
  Require(static_cast<bool>(otherCampaign));
  auto fourth = storage.Save(Metadata(14, 111), state, true, 2);
  Require(static_cast<bool>(fourth));
  Require(!std::filesystem::exists(second.path));
  Require(std::filesystem::exists(third.path));
  Require(std::filesystem::exists(fourth.path));
  Require(std::filesystem::exists(otherCampaign.path));
  Require(std::filesystem::exists(manual.path));

  CampaignCheckpoints sameSecond(root / "same-second");
  CampaignCheckpointResult latest;
  for (uint32_t tick = 1; tick <= 8; ++tick) {
    latest = sameSecond.Save(Metadata(tick, 999, "same-second"), state, true, 2);
    Require(static_cast<bool>(latest) && std::filesystem::exists(latest.path));
  }
  Require(sameSecond.List().size() == 2);
  Require(sameSecond.Load(latest.path).has_value());

  const auto corrupt = directory / "corrupt.rcp";
  std::filesystem::copy_file(third.path, corrupt);
  {
    std::fstream file(corrupt, std::ios::binary | std::ios::in | std::ios::out);
    file.seekp(20);
    const char changed = 'X';
    file.write(&changed, 1);
  }
  std::string error;
  Require(!storage.Load(corrupt, &error));
  Require(!error.empty());

  const auto blocker = root / "file-blocker";
  { std::ofstream file(blocker); file << "not a directory"; }
  CampaignCheckpoints broken(blocker / "checkpoints");
  const auto failed = broken.Save(Metadata(14, 104), state, true);
  Require(!failed && !failed.error.empty());
  Require(storage.Load(third.path).has_value());

  CampaignCheckpointCadence cadence(std::chrono::minutes(5));
  using namespace std::chrono_literals;
  cadence.Reset(0);
  Require(!cadence.Update(4min, true, false, 1));
  Require(!cadence.Update(1min, false, false, 1));
  Require(!cadence.Update(1min, true, true, 1));
  Require(cadence.Update(1min, true, false, 1));
  Require(cadence.Update(1min, true, false, 1));
  cadence.Saved(1);
  Require(!cadence.Update(10min, true, false, 1));
  Require(cadence.Update(5min, true, false, 2));
  cadence.Reset(2);
  Require(!cadence.Update(5min, true, false, 2));

#ifndef _WIN32
  TestConcurrentProcesses(root / "concurrent");
#endif

  std::filesystem::remove_all(root);
}
