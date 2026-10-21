#include "space_battle/battle_save.h"

#include <cstring>
#include <type_traits>
#include <utility>

namespace reb::battle
{
namespace
{
constexpr uint32_t kStateMagic = 0x42534252;   // "RBSB"
constexpr uint32_t kCommandMagic = 0x43534252; // "RBSC"
constexpr uint32_t kCommandVersion = 1;

class ByteWriter
{
  public:
  static constexpr bool kReading = false;
  explicit ByteWriter(std::vector<uint8_t>& out) : m_out(out)
  {
  }
  template <class T>
  void Raw(T& value)
  {
    const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
    m_out.insert(m_out.end(), bytes, bytes + sizeof(T));
  }
  size_t Count(size_t count)
  {
    auto stored = static_cast<uint32_t>(count);
    Raw(stored);
    return count;
  }
  bool Ok() const
  {
    return true;
  }
  void Fail()
  {
  }

  private:
  std::vector<uint8_t>& m_out;
};

class ByteReader
{
  public:
  static constexpr bool kReading = true;
  explicit ByteReader(std::span<const uint8_t> data) : m_data(data)
  {
  }
  template <class T>
  void Raw(T& value)
  {
    if (!m_ok || m_position + sizeof(T) > m_data.size()) {
      m_ok = false;
      value = T{};
      return;
    }
    std::memcpy(&value, m_data.data() + m_position, sizeof(T));
    m_position += sizeof(T);
  }
  size_t Count(size_t)
  {
    uint32_t count = 0;
    Raw(count);
    // Every element takes at least one byte: a larger count can only come from a corrupt block.
    if (count > m_data.size() - m_position) {
      m_ok = false;
      return 0;
    }
    return count;
  }
  bool Ok() const
  {
    return m_ok;
  }
  void Fail()
  {
    m_ok = false;
  }
  size_t Position() const
  {
    return m_position;
  }

  private:
  std::span<const uint8_t> m_data;
  size_t m_position = 0;
  bool m_ok = true;
};

template <class Io>
class Archive
{
  public:
  explicit Archive(Io& io) : m_io(io)
  {
  }
  Io& io()
  {
    return m_io;
  }
  // The state block's version, for fields added after version 1.
  uint32_t version = kStateVersion;

  void operator()(bool& value)
  {
    uint8_t byte = value ? 1 : 0;
    m_io.Raw(byte);
    value = byte != 0;
  }
  template <class T>
    requires(std::is_arithmetic_v<T> || std::is_enum_v<T>)
  void operator()(T& value)
  {
    m_io.Raw(value);
  }
  void operator()(std::string& text)
  {
    const size_t count = m_io.Count(text.size());
    if constexpr (Io::kReading) {
      text.resize(count);
    }
    for (char& c : text) {
      m_io.Raw(c);
    }
  }
  template <class T>
  void operator()(std::vector<T>& values)
  {
    const size_t count = m_io.Count(values.size());
    if constexpr (Io::kReading) {
      values.resize(count);
    }
    for (T& value : values) {
      (*this)(value);
    }
  }
  template <class T, size_t N>
  void operator()(std::array<T, N>& values)
  {
    for (T& value : values) {
      (*this)(value);
    }
  }
  template <class T>
  void operator()(std::optional<T>& value)
  {
    bool has = value.has_value();
    (*this)(has);
    if constexpr (Io::kReading) {
      value = has ? std::optional<T>(T{}) : std::nullopt;
    }
    if (has) {
      (*this)(*value);
    }
  }

  void operator()(Vec2& v)
  {
    (*this)(v.x);
    (*this)(v.y);
  }
  void operator()(CriticalHits& c)
  {
    Fields(c.speed, c.shieldRecharge, c.weaponRecharge, c.tractor, c.hyperdrivePrimary, c.hyperdriveBackup);
  }
  void operator()(TargetPriorities& p)
  {
    Fields(p.tags, p.count, p.finishDamaged);
  }
  void operator()(ShipDoctrine& d)
  {
    Fields(d.role, d.range, d.approach, d.facing, d.priorities, d.allocation, d.subsystem, d.power, d.withdraw,
           d.formation, d.breakFormation, d.allBatteriesOnTarget, d.screenTarget);
  }
  void operator()(FlightDoctrine& d)
  {
    Fields(d.mission, d.priorities, d.dogfight, d.rearm, d.launch, d.escortTarget, d.patrolCenter, d.patrolRadius);
  }
  void operator()(UnitOrder& o)
  {
    Fields(o.kind, o.targets, o.position, o.facing, o.hasFacing, o.approach);
  }
  void operator()(Intent& i)
  {
    Fields(i.kind, i.target, i.priorityRank, i.tag, i.arc, i.timeToKill, i.othersAssigned, i.range, i.progress,
           i.blocker, i.fromOrder);
  }
  void operator()(UnitState& u)
  {
    Fields(u.kind, u.side, u.fate, u.inSpace, u.slot, u.position, u.previousPosition, u.heading, u.previousHeading,
           u.velocity, u.group, u.shipDoctrine, u.flightDoctrine, u.overrides, u.orders, u.intent);
  }
  void operator()(ShipState& s)
  {
    Fields(s.unit, s.hull, s.shield, s.criticals, s.speed, s.desiredHeading, s.desiredSpeed, s.goal, s.slotOffset, s.primaryTarget,
           s.batteryCooldown, s.batteryTarget, s.power, s.powerFrom, s.powerTicks, s.engineOffTicks,
           s.systemsOffTicks, s.ionPool, s.damageControlTicks, s.tractorTarget, s.tractorHeld, s.withdrawing,
           s.spoolTicks, s.spoolTotal, s.launchTicks, s.nextThinkTick, s.lastDamageTick);
  }
  void operator()(SquadronState& s)
  {
    Fields(s.unit, s.firstCraft, s.craftCount, s.alive, s.initialAlive, s.place, s.carrier, s.task, s.target,
           s.anchor, s.stayDocked, s.launchOrdered, s.stayAfterDock, s.noHangar, s.rearmTicks, s.nextThinkTick,
           s.lastAttackedTick, s.centroid, s.spread, s.withdrawing, s.spoolTicks, s.spoolTotal, s.runTicks);
    if (version >= 3) {
      (*this)(s.launched);
    } else if constexpr (Io::kReading) {
      // Only a flight home keeps a planet's squadron aboard; a carrier's, kept aboard to withdraw, have its cover.
      s.launched = s.place != SquadronPlace::Docked || s.rearmTicks > 0 || s.stayDocked;
    }
  }
  void operator()(CraftState& c)
  {
    Fields(c.position, c.previousPosition, c.heading, c.previousHeading, c.hull, c.shield, c.torpedoes, c.alive,
           c.phase, c.phaseTicks, c.breakHeading, c.targetCraft, c.gunTicks, c.torpedoTicks);
  }
  void operator()(StationState& s)
  {
    Fields(s.unit, s.hull, s.chargeTicks, s.firingAt, s.fireDelayTicks, s.launchTicks, s.withdrawing, s.spoolTicks,
           s.spoolTotal, s.runFlight);
  }
  void operator()(ScoredTarget& t)
  {
    Fields(t.unit, t.score, t.stateFactor, t.tags, t.rank, t.tag);
  }
  void operator()(GroupState& g)
  {
    Fields(g.id, g.side, g.flight, g.name, g.members, g.shipDoctrine, g.flightDoctrine, g.orders, g.nextThinkTick,
           g.committed, g.focus, g.approachReference, g.approachTarget, g.goal, g.hasGoal, g.goalHeading, g.speed, g.frameHeading, g.candidates);
  }
  void operator()(SideState& s)
  {
    // Deployment bounds are reconstructed by Initialize from the snapshot, including for version-1 saves.
    Fields(s.admiral, s.commander, s.leadership, s.commanderCombat, s.commandRating, s.signalsReady, s.signalsMax,
           s.signalRechargeTicks, s.concentrateTarget, s.concentrateTicks, s.holdLineTicks, s.scrambleTicks,
           s.coordinatedJumpTicks, s.fleetWithdrawal, s.durableLow, s.strength, s.initialStrength, s.everWithdrew,
           s.planetLaunchTicks);
  }
  void operator()(PendingShot& s)
  {
    Fields(s.fireTick, s.impactTick, s.shooter, s.target, s.targetCraft, s.side, s.kind, s.focus, s.hit,
           s.shieldImpact, s.damage, s.from, s.to);
    if (version >= 2) {
      (*this)(s.battery);
    }
  }
  void operator()(DefenseState& d)
  {
    Fields(d.cooldown, d.target, d.lastFireTick);
  }
  void operator()(BattleEvent& e)
  {
    Fields(e.tick, e.side, e.text, e.unit);
  }

  // Orders and commands.
  void operator()(OrderAttack& o)
  {
    (*this)(o.targets);
  }
  void operator()(OrderMove& o)
  {
    Fields(o.position, o.facing);
  }
  void operator()(OrderApproach& o)
  {
    Fields(o.target, o.approach);
  }
  void operator()(OrderHold&)
  {
  }
  void operator()(OrderEscort& o)
  {
    (*this)(o.target);
  }
  void operator()(OrderTractor& o)
  {
    (*this)(o.target);
  }
  void operator()(OrderLaunch&)
  {
  }
  void operator()(OrderRecall&)
  {
  }
  void operator()(OrderStrike& o)
  {
    (*this)(o.target);
  }
  void operator()(OrderRecover&)
  {
  }
  void operator()(OrderWithdraw&)
  {
  }
  void operator()(OrderSetPower& o)
  {
    (*this)(o.power);
  }
  void operator()(OrderSetShipDoctrine& o)
  {
    Fields(o.doctrine, o.fields);
  }
  void operator()(OrderSetFlightDoctrine& o)
  {
    Fields(o.doctrine, o.fields);
  }
  void operator()(OrderAssignGroup& o)
  {
    (*this)(o.group);
  }
  void operator()(OrderSignal& o)
  {
    Fields(o.signal, o.target);
  }
  void operator()(OrderFireSuperlaser& o)
  {
    (*this)(o.target);
  }
  void operator()(OrderDeathStarRun&)
  {
  }
  void operator()(OrderDeploy& o)
  {
    Fields(o.position, o.facing);
  }
  void operator()(Command& c)
  {
    Fields(c.tick, c.side, c.target.units, c.target.groups, c.queue);
    auto index = static_cast<uint8_t>(c.order.index());
    (*this)(index);
    if constexpr (Io::kReading) {
      if (index >= std::variant_size_v<OrderPayload>) {
        Fail();
        return;
      }
      Emplace(c.order, index, std::make_index_sequence<std::variant_size_v<OrderPayload>>{});
    }
    std::visit([&](auto& order) { (*this)(order); }, c.order);
  }

  void Fail()
  {
    m_io.Fail();
  }

  private:
  template <class... Ts>
  void Fields(Ts&... values)
  {
    ((*this)(values), ...);
  }
  template <size_t... I>
  static void Emplace(OrderPayload& order, size_t index, std::index_sequence<I...>)
  {
    ((index == I ? (void)order.template emplace<I>() : (void)0), ...);
  }

  Io& m_io;
};

template <class Io>
void VisitState(Archive<Io>& ar, BattleState& state)
{
  ar(state.tick);
  ar(state.rng.StateWord());
  ar(state.rng.IncrementWord());
  ar(state.units);
  ar(state.ships);
  ar(state.squadrons);
  ar(state.craft);
  ar(state.stations);
  if (ar.version >= 2) {
    ar(state.defenses); // version 1 predates planetary batteries: its snapshots have none
  }
  ar(state.groups);
  ar(state.sides);
  ar(state.shots);
  ar(state.pending);
  ar(state.commandLog);
  ar(state.events);
  ar(state.lastDurableDropTick);
  ar(state.finished);
  ar(state.endReason);
}

uint32_t SnapshotCheck(const BattleState& state)
{
  return static_cast<uint32_t>(state.snapshot.units.size()) * 2654435761U ^ state.snapshot.params.seed;
}

} // namespace

void WriteState(const BattleState& state, std::vector<uint8_t>& out, uint32_t version)
{
  ByteWriter io(out);
  Archive ar(io);
  uint32_t magic = kStateMagic;
  uint32_t check = SnapshotCheck(state);
  ar(magic);
  ar(version);
  ar(check);
  ar.version = version;
  VisitState(ar, const_cast<BattleState&>(state));
}

bool ReadState(BattleState& state, std::span<const uint8_t> data, size_t* consumed)
{
  ByteReader io(data);
  Archive ar(io);
  uint32_t magic = 0;
  uint32_t version = 0;
  uint32_t check = 0;
  ar(magic);
  ar(version);
  ar(check);
  if (!io.Ok() || magic != kStateMagic || version < 1 || version > kStateVersion || check != SnapshotCheck(state)) {
    return false;
  }
  ar.version = version;
  VisitState(ar, state);
  if (!io.Ok() || state.units.size() != state.snapshot.units.size()
      || state.defenses.size() != state.snapshot.defenses.size()) {
    return false;
  }
  *consumed = io.Position();
  return true;
}

std::vector<uint8_t> EncodeCommands(std::span<const Command> commands)
{
  std::vector<uint8_t> out;
  ByteWriter io(out);
  Archive ar(io);
  uint32_t magic = kCommandMagic;
  uint32_t version = kCommandVersion;
  ar(magic);
  ar(version);
  std::vector<Command> copy(commands.begin(), commands.end());
  ar(copy);
  return out;
}

std::optional<std::vector<Command>> DecodeCommands(std::span<const uint8_t> data)
{
  ByteReader io(data);
  Archive ar(io);
  uint32_t magic = 0;
  uint32_t version = 0;
  ar(magic);
  ar(version);
  std::vector<Command> commands;
  ar(commands);
  if (!io.Ok() || magic != kCommandMagic || version != kCommandVersion || io.Position() != data.size()) {
    return std::nullopt;
  }
  return commands;
}

uint64_t HashBytes(std::span<const uint8_t> data)
{
  uint64_t hash = 0xcbf29ce484222325ULL;
  for (uint8_t byte : data) {
    hash = (hash ^ byte) * 0x100000001b3ULL;
  }
  return hash;
}

} // namespace reb::battle
