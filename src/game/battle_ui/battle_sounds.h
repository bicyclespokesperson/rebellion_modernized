#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "space_battle/snapshot.h"

// Which original sound belongs to which battle event (docs/battle_audio.md records how each mapping was found). The
// audio itself is never part of the repository: a resource is a WAVE id inside one of three installed DLLs, read at
// runtime like the other original assets.
namespace reb::battle_ui
{

enum class SoundModule : uint8_t { Tactical, VoiceAlliance, VoiceEmpire };

// "TACTICAL.DLL", "VOICEFXA.DLL" or "VOICEFXE.DLL". Look a WAVE up in this module, never by id range: the Alliance
// voice module also holds ids 15133-15163, which continue the Empire module's numbering.
std::string_view ModuleFileName(SoundModule module);

struct SoundResource {
  SoundModule module = SoundModule::Tactical;
  uint16_t waveId = 0;

  bool operator==(const SoundResource&) const = default;
};

// How well the mapping is established. High: the original code that plays it was disassembled and the clip content
// agrees. Medium: the trigger is identified but the event naming is an inference, or the clip's meaning is certain but
// no play site was found. Low: no reference in the original code; a guess from the clip itself.
enum class Confidence : uint8_t { High, Medium, Low };

// ---- effects (TACTICAL.DLL) -----------------------------------------------------------------------------------------

// Events in the vocabulary of the new simulator's view model. Several share a recording because the original did.
enum class BattleSound : uint8_t {
  // A shot of this weapon reaches its target: the sound the target makes (original ShipBase::PlayHitSoundA/B/C).
  ImpactLaserCapital,
  ImpactTurbolaserCapital,
  ImpactIonCapital,
  ImpactLaserFighterAlliance,
  ImpactLaserFighterEmpire,
  ImpactTurbolaserFighter,
  ImpactIonFighter,
  TorpedoLaunch, // the original plays it at the firing ship
  // Damage landing on a ship. The torpedo uses the turbolaser rows.
  ShieldHitTurbolaser,
  ShieldHitLaser,
  ShieldHitIon,
  HullHitTurbolaser,
  HullHitLaser,
  HullHitIon,
  // A unit is destroyed; capital ships by maximum hull (< 1100 small, <= 2000 medium, else large), fighters small.
  ExplosionSmall,
  ExplosionMedium,
  ExplosionLarge,
  TractorTargetHeld,     // the first tractor beam locks on a ship
  TractorTargetReleased, // the last tractor beam lets go
  TractorBeamStart,      // a ship starts tractoring
  SuperlaserImpact,      // the Death Star's shot reaches a ship
  FighterLaunch,
  // Hyperspace, by unit class: In is the fleets arriving at the start of the battle, Out is a ship jumping away.
  JumpInFighter,
  JumpOutFighter,
  JumpInCapital,
  JumpOutCapital,
  // Steady hums the original starts for the unit in the player's view, by unit class.
  EngineHumFighterEmpire,
  EngineHumFighterAlliance,
  EngineHumCapitalLarge,
  EngineHumCapitalMedium,
  EngineHumCapitalSmall,
  // Recordings the original never plays; kept so every one of the 66 WAVEs is accounted for.
  UnreferencedImpact0F,
  UnreferencedImpact10,
  UnreferencedBeep,
};

inline constexpr int kBattleSoundCount = static_cast<int>(BattleSound::UnreferencedBeep) + 1;

struct EffectSound {
  BattleSound sound = BattleSound::ImpactLaserCapital;
  uint8_t originalId = 0; // the original sound manager's id (0-31); events with the same id share their WAVEs
  uint16_t firstWave = 0; // TACTICAL.DLL WAVE id of the first variant; variants are consecutive ids
  uint8_t variants = 1;   // the original plays one at random
  float volume = 1.0f;    // the original has no per-sound gain, so every effect defaults to 1
  Confidence confidence = Confidence::High;
};

std::span<const EffectSound> EffectSounds(); // indexed by BattleSound
const EffectSound& EffectSoundFor(BattleSound sound);
SoundResource EffectResource(BattleSound sound, uint32_t variantRoll); // variantRoll selects a variant modulo the count

// ---- voices (VOICEFXA.DLL / VOICEFXE.DLL) --------------------------------------------------------------------------

// A voice report or acknowledgement. Most exist for one task force (1-8) or fighter group (Red, Blue, Green, Gold) and
// as a generic "Ship ..." clip for a unit without a group.
enum class VoiceCue : uint8_t {
  FleetReady,       // the battle opens
  ManeuverAck,      // a maneuver order was received
  ManeuverComplete, // the maneuver is finished and the group engages
  AttackAck,        // an attack order was received
  OrdersComplete,   // the group has finished its orders and requests new ones
  TargetDestroyed,  // a ship of this group destroyed an enemy
  ShipLost,         // a ship of this group was destroyed; the Empire's wording for task forces is "ship destroyed"
  FormationAck,     // task forces only
  MissionAck,       // a mission was assigned
  FightersLaunched, // Empire only, fighter groups only
  CommandCircuitsFull,
  PersonnelEjecting, // characters left fighters that were lost
  PersonnelAbandonShip,
  FightersRecovered,    // fighter groups only: the last fighter of the group is aboard
  FightersNotRecovered, // fighter groups only: not enough docking space
  WithdrawOrdered,
  WithdrawBlockedShips,     // some ships cannot jump and would be left behind (Alliance) or scuttled (Empire)
  WithdrawStrandsGroups,    // Empire only: fighter groups not recovered would be stranded
  WithdrawNoDockingSpace,   // Empire only: fighters left without docking space would be stranded
  WithdrawalComplete,       // end of battle: our fleet jumped away
  EnemyWithdrew,            // end of battle
  EnemyDestroyed,           // end of battle
  FleetDestroyed,           // end of battle
  DeathStarApproaching,     // Alliance only
  DeathStarFiringAtUs,      // Alliance only
  DeathStarOpening,         // Alliance only: the Death Star can be attacked
  DeathStarShielded,        // Alliance only: a shield generator makes an attack impossible
  DeathStarTooManyFighters, // Alliance only: Imperial fighters block the attack
  DeathStarFireAck,         // Empire only, from here to DeathStarAttackContinues
  DeathStarRecharging,
  DeathStarCharged,
  DeathStarBlocked,
  DeathStarUnderAttack,
  DeathStarAttackBrokenOff,
  DeathStarDestroyed,
  DeathStarUnshielded, // too few fighters to shield it
  DeathStarAttackContinues,
  DeathStarPilotChatter, // Alliance only: fifteen trench-run lines, selected by VoiceAddress::index
};

inline constexpr int kVoiceCueCount = static_cast<int>(VoiceCue::DeathStarPilotChatter) + 1;
inline constexpr int kTaskForceCount = 8;

enum class GroupColor : uint8_t { Red, Blue, Green, Gold };
inline constexpr int kGroupColorCount = 4;

enum class Addressee : uint8_t { Ship, TaskForce, FighterGroup };

// index: task force 0-7 (clip "Task Force one" is 0), GroupColor for a fighter group, and for Addressee::Ship the line
// number among the cue's generic clips (always 0 except DeathStarPilotChatter).
struct VoiceAddress {
  Addressee kind = Addressee::Ship;
  uint8_t index = 0;
};

// One cue's clips for one side: `ships` generic clips, then `taskForces` (0 or 8) and `groups` (0 or 4: Red, Blue,
// Green, Gold) addressed ones, all with consecutive WAVE ids from `firstWave`.
struct VoiceFamily {
  VoiceCue cue = VoiceCue::FleetReady;
  reb::battle::Side side = reb::battle::Side::Alliance;
  uint16_t firstWave = 0;
  uint8_t ships = 0;
  uint8_t taskForces = 0;
  uint8_t groups = 0;
  float volume = 1.0f;
  Confidence confidence = Confidence::High;
};

std::span<const VoiceFamily> VoiceFamilies();
SoundModule VoiceModule(reb::battle::Side side);

// nullopt when the side has no such clip (for example Empire formation acknowledgement of a lone ship).
std::optional<SoundResource> VoiceResource(reb::battle::Side side, VoiceCue cue, VoiceAddress address = {});

// How an Alliance fighter group reports on its run against the Death Star, in the order the original plays them.
enum class DeathStarRunReport : uint8_t { Ordered, Begun, Failed, Succeeded };
SoundResource DeathStarRunVoice(GroupColor color, DeathStarRunReport report);

// The original sound manager's channels. A request that finds its channel full is dropped: every battle call site asks
// for that (the manager's waiting queue exists but is never used). Effects play up to kEffectPolyphony at once; each
// voice channel speaks one clip at a time.
inline constexpr int kEffectPolyphony = 5;
inline constexpr int kVoicePolyphony = 1;

enum class VoiceChannel : uint8_t { Report, DeathStarRun };

// DeathStarPilotChatter and DeathStarRunVoice clips share the DeathStarRun channel, so they overlap the reports.
VoiceChannel ChannelFor(VoiceCue cue);

} // namespace reb::battle_ui
