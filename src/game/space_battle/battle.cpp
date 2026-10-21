#include "space_battle/battle.h"

#include <algorithm>
#include <cstring>

#include "space_battle/battle_save.h"
#include "space_battle/battle_state.h"
#include "space_battle/sim_internal.h"

namespace reb::battle
{
namespace
{
std::unique_ptr<Commander> MakeCommander(const BattleOptions& options, Side side)
{
  if (options.makeCommander) {
    if (auto commander = options.makeCommander(side)) {
      return commander;
    }
  }
  return MakeBattleCommander(options.commanders[SideIndex(side)]);
}

void AppendBlob(std::vector<uint8_t>& out, const std::vector<uint8_t>& blob)
{
  const auto size = static_cast<uint32_t>(blob.size());
  const auto* bytes = reinterpret_cast<const uint8_t*>(&size);
  out.insert(out.end(), bytes, bytes + sizeof(size));
  out.insert(out.end(), blob.begin(), blob.end());
}

bool ReadBlob(std::span<const uint8_t>& data, std::span<const uint8_t>* blob)
{
  uint32_t size = 0;
  if (data.size() < sizeof(size)) {
    return false;
  }
  std::memcpy(&size, data.data(), sizeof(size));
  data = data.subspan(sizeof(size));
  if (data.size() < size) {
    return false;
  }
  *blob = data.first(size);
  data = data.subspan(size);
  return true;
}

std::unique_ptr<BattleState> InitialState(const BattleSnapshot& snapshot, const BattleOptions& options)
{
  auto state = std::make_unique<BattleState>();
  state->snapshot = snapshot;
  state->options = options;
  sim::Initialize(*state);
  return state;
}

} // namespace

Battle::Battle(const BattleSnapshot& snapshot, const BattleOptions& options) : Battle(InitialState(snapshot, options))
{
}

Battle::Battle(std::unique_ptr<BattleState> state) : m_state(std::move(state))
{
  for (Side side : {Side::Alliance, Side::Empire}) {
    m_commanders[SideIndex(side)] = MakeCommander(m_state->options, side);
  }
}

Battle::~Battle() = default;

bool Battle::Submit(const Command& command)
{
  if (m_state->finished || m_state->endReason != EndReason::None) {
    return false;
  }
  Command accepted = command;
  accepted.tick = std::max(accepted.tick, m_state->tick);
  if (!sim::ValidateCommand(*m_state, accepted)) {
    return false;
  }
  // A command for now takes effect at once, so doctrine and deployment changes show while paused. One for a later
  // tick (a replay submitted ahead) waits for the start of that tick's Step: the same point in the order.
  m_state->commandLog.push_back(accepted);
  if (accepted.tick <= m_state->tick) {
    sim::ApplyCommand(*m_state, accepted);
    return true;
  }
  auto& pending = m_state->pending;
  const auto at = std::upper_bound(pending.begin(), pending.end(), accepted.tick,
                                   [](int64_t tick, const Command& queued) { return tick < queued.tick; });
  pending.insert(at, accepted);
  return true;
}

void Battle::RunCommanders()
{
  std::vector<Command> commands;
  for (Side side : {Side::Alliance, Side::Empire}) {
    const int index = SideIndex(side);
    if (!m_state->options.aiControlled[index] || !m_commanders[index]) {
      continue;
    }
    commands.clear();
    m_commanders[index]->Update(*this, side, commands);
    for (Command& command : commands) {
      command.side = side;
      Submit(command);
    }
  }
}

void Battle::ApplyDueCommands()
{
  auto& pending = m_state->pending;
  const auto end = std::find_if(pending.begin(), pending.end(),
                                [&](const Command& command) { return command.tick > m_state->tick; });
  std::vector<Command> due(pending.begin(), end);
  pending.erase(pending.begin(), end);
  for (const Command& command : due) {
    // Checked again: an earlier command this tick may have used the charge or the unit it needed.
    if (sim::ValidateCommand(*m_state, command)) {
      sim::ApplyCommand(*m_state, command);
    }
  }
}

void Battle::Step()
{
  if (m_state->finished) {
    return;
  }
  if (m_state->endReason == EndReason::None) {
    RunCommanders();
    ApplyDueCommands();
  }
  sim::Step(*m_state);
}

bool Battle::RunHeadless(int64_t maxTicks)
{
  for (int64_t i = 0; i < maxTicks && !m_state->finished; ++i) {
    Step();
  }
  return m_state->finished;
}

void Battle::SetOutroSeconds(float seconds)
{
  m_state->outroTicks = sim::Ticks(std::max(0.0f, seconds));
}

void Battle::SkipOutro()
{
  if (m_state->endReason != EndReason::None) {
    m_state->finished = true;
  }
}

bool Battle::Finished() const
{
  return m_state->finished;
}

int64_t Battle::Tick() const
{
  return m_state->tick;
}

const BattleSnapshot& Battle::Snapshot() const
{
  return m_state->snapshot;
}

const BattleOptions& Battle::Options() const
{
  return m_state->options;
}

void Battle::SetAiControlled(Side side, bool ai)
{
  if (ai && !m_state->options.aiControlled[SideIndex(side)]) {
    m_commanders[SideIndex(side)] = MakeCommander(m_state->options, side);
  }
  m_state->options.aiControlled[SideIndex(side)] = ai;
}

BattleResultSet Battle::Results() const
{
  return sim::BuildResults(*m_state);
}

std::span<const Command> Battle::CommandLog() const
{
  return m_state->commandLog;
}

std::vector<uint8_t> Battle::SaveState() const
{
  std::vector<uint8_t> out;
  WriteState(*m_state, out);
  for (const auto& commander : m_commanders) {
    std::vector<uint8_t> blob;
    if (commander) {
      commander->Save(blob);
    }
    AppendBlob(out, blob);
  }
  return out;
}

std::unique_ptr<Battle> Battle::RestoreState(const BattleSnapshot& snapshot, const BattleOptions& options,
                                             std::span<const uint8_t> data)
{
  std::unique_ptr<BattleState> state = InitialState(snapshot, options);
  size_t consumed = 0;
  if (!ReadState(*state, data, &consumed)) {
    return nullptr;
  }
  data = data.subspan(consumed);
  std::unique_ptr<Battle> battle(new Battle(std::move(state)));
  for (auto& commander : battle->m_commanders) {
    std::span<const uint8_t> blob;
    if (!ReadBlob(data, &blob) || !commander->Restore(blob)) {
      return nullptr;
    }
  }
  return data.empty() ? std::move(battle) : nullptr;
}

uint64_t Battle::StateHash() const
{
  return HashBytes(SaveState());
}

const BattleState& Battle::State() const
{
  return *m_state;
}

} // namespace reb::battle
