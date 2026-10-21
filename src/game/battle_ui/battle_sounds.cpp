#include "battle_ui/battle_sounds.h"

#include <algorithm>
#include <array>

namespace reb::battle_ui
{

namespace
{

using reb::battle::Side;

constexpr EffectSound Effect(BattleSound sound, uint8_t originalId, uint16_t firstWave, uint8_t variants,
                             Confidence confidence)
{
  return EffectSound{sound, originalId, firstWave, variants, 1.0f, confidence};
}

constexpr auto kEffects = std::to_array<EffectSound>({
    Effect(BattleSound::ImpactLaserCapital, 0, 13000, 4, Confidence::Medium),
    Effect(BattleSound::ImpactTurbolaserCapital, 1, 13004, 4, Confidence::Medium),
    Effect(BattleSound::ImpactIonCapital, 2, 13008, 4, Confidence::Medium),
    Effect(BattleSound::ImpactLaserFighterAlliance, 3, 13012, 2, Confidence::Medium),
    Effect(BattleSound::ImpactLaserFighterEmpire, 4, 13014, 2, Confidence::Medium),
    Effect(BattleSound::ImpactTurbolaserFighter, 1, 13004, 4, Confidence::Medium),
    Effect(BattleSound::ImpactIonFighter, 5, 13016, 2, Confidence::Medium),
    Effect(BattleSound::TorpedoLaunch, 1, 13004, 4, Confidence::Low),
    Effect(BattleSound::ShieldHitTurbolaser, 12, 13030, 3, Confidence::Medium),
    Effect(BattleSound::ShieldHitLaser, 13, 13033, 3, Confidence::Medium),
    Effect(BattleSound::ShieldHitIon, 14, 13036, 3, Confidence::Medium),
    Effect(BattleSound::HullHitTurbolaser, 17, 13045, 3, Confidence::Medium),
    Effect(BattleSound::HullHitLaser, 18, 13048, 3, Confidence::Medium),
    Effect(BattleSound::HullHitIon, 19, 13051, 3, Confidence::Medium),
    Effect(BattleSound::ExplosionSmall, 9, 13021, 3, Confidence::High),
    Effect(BattleSound::ExplosionMedium, 10, 13024, 3, Confidence::High),
    Effect(BattleSound::ExplosionLarge, 11, 13027, 3, Confidence::High),
    Effect(BattleSound::TractorTargetHeld, 6, 13018, 1, Confidence::High),
    Effect(BattleSound::TractorTargetReleased, 7, 13019, 1, Confidence::High),
    Effect(BattleSound::TractorBeamStart, 8, 13020, 1, Confidence::High),
    Effect(BattleSound::SuperlaserImpact, 20, 13054, 1, Confidence::High),
    Effect(BattleSound::FighterLaunch, 27, 13061, 1, Confidence::High),
    Effect(BattleSound::JumpInFighter, 28, 13062, 1, Confidence::High),
    Effect(BattleSound::JumpOutFighter, 29, 13063, 1, Confidence::High),
    Effect(BattleSound::JumpInCapital, 30, 13064, 1, Confidence::High),
    Effect(BattleSound::JumpOutCapital, 31, 13065, 1, Confidence::High),
    Effect(BattleSound::EngineHumFighterEmpire, 21, 13055, 1, Confidence::Medium),
    Effect(BattleSound::EngineHumFighterAlliance, 22, 13056, 1, Confidence::Medium),
    Effect(BattleSound::EngineHumCapitalLarge, 23, 13057, 1, Confidence::Medium),
    Effect(BattleSound::EngineHumCapitalMedium, 24, 13058, 1, Confidence::Medium),
    Effect(BattleSound::EngineHumCapitalSmall, 25, 13059, 1, Confidence::Medium),
    Effect(BattleSound::UnreferencedImpact0F, 15, 13039, 3, Confidence::Low),
    Effect(BattleSound::UnreferencedImpact10, 16, 13042, 3, Confidence::Low),
    Effect(BattleSound::UnreferencedBeep, 26, 13060, 1, Confidence::Low),
});

static_assert(kEffects.size() == static_cast<size_t>(kBattleSoundCount));
static_assert(kEffects.front().sound == BattleSound::ImpactLaserCapital);
static_assert(std::ranges::adjacent_find(kEffects,
                                         [](const EffectSound& a, const EffectSound& b) {
                                           return a.sound >= b.sound;
                                         })
                  == kEffects.end(),
              "kEffects must list every BattleSound once, in enum order");

constexpr VoiceFamily Voice(Side side, VoiceCue cue, uint16_t firstWave, uint8_t ships, uint8_t taskForces,
                            uint8_t groups, Confidence confidence)
{
  return VoiceFamily{cue, side, firstWave, ships, taskForces, groups, 1.0f, confidence};
}

constexpr VoiceFamily Alliance(VoiceCue cue, uint16_t firstWave, uint8_t ships, uint8_t taskForces, uint8_t groups,
                               Confidence confidence = Confidence::High)
{
  return Voice(Side::Alliance, cue, firstWave, ships, taskForces, groups, confidence);
}

constexpr VoiceFamily Empire(VoiceCue cue, uint16_t firstWave, uint8_t ships, uint8_t taskForces, uint8_t groups,
                             Confidence confidence = Confidence::High)
{
  return Voice(Side::Empire, cue, firstWave, ships, taskForces, groups, confidence);
}

constexpr auto kVoices = std::to_array<VoiceFamily>({
    Alliance(VoiceCue::FleetReady, 14001, 1, 0, 0),
    Alliance(VoiceCue::ManeuverAck, 14002, 1, 8, 4),
    Alliance(VoiceCue::ManeuverComplete, 14015, 1, 8, 4),
    Alliance(VoiceCue::AttackAck, 14028, 1, 8, 4),
    Alliance(VoiceCue::OrdersComplete, 14041, 1, 8, 4, Confidence::Medium),
    Alliance(VoiceCue::TargetDestroyed, 14054, 1, 8, 4),
    Alliance(VoiceCue::ShipLost, 14067, 1, 8, 4),
    Alliance(VoiceCue::FormationAck, 14080, 0, 8, 0),
    Alliance(VoiceCue::MissionAck, 14088, 1, 8, 4),
    Alliance(VoiceCue::CommandCircuitsFull, 14101, 1, 0, 0),
    Alliance(VoiceCue::PersonnelEjecting, 14102, 1, 0, 0, Confidence::Medium),
    Alliance(VoiceCue::PersonnelAbandonShip, 14103, 1, 0, 0),
    Alliance(VoiceCue::FightersRecovered, 14104, 0, 0, 4),
    Alliance(VoiceCue::FightersNotRecovered, 14108, 0, 0, 4),
    Alliance(VoiceCue::WithdrawOrdered, 14112, 1, 0, 0),
    Alliance(VoiceCue::DeathStarApproaching, 14113, 1, 0, 0),
    Alliance(VoiceCue::DeathStarFiringAtUs, 14114, 1, 0, 0),
    Alliance(VoiceCue::DeathStarOpening, 14115, 1, 0, 0, Confidence::Medium),
    Alliance(VoiceCue::DeathStarShielded, 14116, 1, 0, 0),
    Alliance(VoiceCue::DeathStarTooManyFighters, 14117, 1, 0, 0),
    Alliance(VoiceCue::WithdrawBlockedShips, 14118, 1, 0, 0),
    Alliance(VoiceCue::WithdrawalComplete, 14119, 1, 0, 0),
    Alliance(VoiceCue::EnemyWithdrew, 14120, 1, 0, 0),
    Alliance(VoiceCue::EnemyDestroyed, 14121, 1, 0, 0),
    Alliance(VoiceCue::FleetDestroyed, 14122, 1, 0, 0),
    Alliance(VoiceCue::DeathStarPilotChatter, 15133, 15, 0, 0, Confidence::Medium),

    Empire(VoiceCue::FleetReady, 15001, 1, 0, 0),
    Empire(VoiceCue::ManeuverAck, 15002, 1, 8, 4),
    Empire(VoiceCue::ManeuverComplete, 15015, 1, 8, 4),
    Empire(VoiceCue::AttackAck, 15028, 1, 8, 4),
    Empire(VoiceCue::OrdersComplete, 15041, 1, 8, 4, Confidence::Medium),
    Empire(VoiceCue::TargetDestroyed, 15054, 1, 8, 4),
    Empire(VoiceCue::ShipLost, 15067, 1, 8, 4),
    Empire(VoiceCue::FightersLaunched, 15080, 0, 0, 4, Confidence::Medium),
    Empire(VoiceCue::FormationAck, 15084, 0, 8, 0),
    Empire(VoiceCue::MissionAck, 15092, 1, 8, 4),
    Empire(VoiceCue::CommandCircuitsFull, 15105, 1, 0, 0),
    Empire(VoiceCue::PersonnelEjecting, 15106, 1, 0, 0, Confidence::Medium),
    Empire(VoiceCue::PersonnelAbandonShip, 15107, 1, 0, 0),
    Empire(VoiceCue::FightersRecovered, 15108, 0, 0, 4),
    Empire(VoiceCue::FightersNotRecovered, 15112, 0, 0, 4),
    Empire(VoiceCue::WithdrawOrdered, 15116, 1, 0, 0),
    Empire(VoiceCue::WithdrawBlockedShips, 15117, 1, 0, 0),
    Empire(VoiceCue::WithdrawStrandsGroups, 15118, 1, 0, 0, Confidence::Medium),
    Empire(VoiceCue::WithdrawNoDockingSpace, 15119, 1, 0, 0, Confidence::Medium),
    Empire(VoiceCue::DeathStarFireAck, 15120, 1, 0, 0),
    Empire(VoiceCue::DeathStarRecharging, 15121, 1, 0, 0, Confidence::Medium),
    Empire(VoiceCue::DeathStarCharged, 15122, 1, 0, 0),
    Empire(VoiceCue::DeathStarBlocked, 15123, 1, 0, 0, Confidence::Medium),
    Empire(VoiceCue::DeathStarUnderAttack, 15124, 1, 0, 0, Confidence::Medium),
    Empire(VoiceCue::DeathStarAttackBrokenOff, 15125, 1, 0, 0, Confidence::Medium),
    Empire(VoiceCue::DeathStarDestroyed, 15126, 1, 0, 0, Confidence::Medium),
    Empire(VoiceCue::WithdrawalComplete, 15127, 1, 0, 0),
    Empire(VoiceCue::EnemyWithdrew, 15128, 1, 0, 0),
    Empire(VoiceCue::EnemyDestroyed, 15129, 1, 0, 0),
    Empire(VoiceCue::FleetDestroyed, 15130, 1, 0, 0),
    Empire(VoiceCue::DeathStarUnshielded, 15131, 1, 0, 0),
    Empire(VoiceCue::DeathStarAttackContinues, 15132, 1, 0, 0),
});

constexpr uint16_t kDeathStarRunFirstWave = 15148; // Blue's first line; four lines per group, groups in this order
constexpr std::array<uint8_t, kGroupColorCount> kDeathStarRunSlot{3, 0, 1, 2}; // by GroupColor: Red last, Blue first

const VoiceFamily* FindFamily(Side side, VoiceCue cue)
{
  const auto match = std::ranges::find_if(kVoices, [&](const VoiceFamily& f) {
    return f.side == side && f.cue == cue;
  });
  return match == kVoices.end() ? nullptr : &*match;
}

std::optional<uint32_t> ClipOffset(const VoiceFamily& family, VoiceAddress address)
{
  switch (address.kind) {
    case Addressee::Ship:
      return address.index < family.ships ? std::optional<uint32_t>(address.index) : std::nullopt;
    case Addressee::TaskForce:
      return address.index < family.taskForces ? std::optional<uint32_t>(family.ships + address.index) : std::nullopt;
    case Addressee::FighterGroup:
      return address.index < family.groups ? std::optional<uint32_t>(family.ships + family.taskForces + address.index)
                                           : std::nullopt;
  }
  return std::nullopt;
}

} // namespace

std::string_view ModuleFileName(SoundModule module)
{
  switch (module) {
    case SoundModule::Tactical:
      return "TACTICAL.DLL";
    case SoundModule::VoiceAlliance:
      return "VOICEFXA.DLL";
    case SoundModule::VoiceEmpire:
      return "VOICEFXE.DLL";
  }
  return {};
}

std::span<const EffectSound> EffectSounds()
{
  return kEffects;
}

const EffectSound& EffectSoundFor(BattleSound sound)
{
  return kEffects[static_cast<size_t>(sound)];
}

SoundResource EffectResource(BattleSound sound, uint32_t variantRoll)
{
  const EffectSound& effect = EffectSoundFor(sound);
  return {SoundModule::Tactical, static_cast<uint16_t>(effect.firstWave + variantRoll % effect.variants)};
}

std::span<const VoiceFamily> VoiceFamilies()
{
  return kVoices;
}

SoundModule VoiceModule(Side side)
{
  return side == Side::Alliance ? SoundModule::VoiceAlliance : SoundModule::VoiceEmpire;
}

std::optional<SoundResource> VoiceResource(Side side, VoiceCue cue, VoiceAddress address)
{
  const VoiceFamily* family = FindFamily(side, cue);
  if (family == nullptr) {
    return std::nullopt;
  }
  const std::optional<uint32_t> offset = ClipOffset(*family, address);
  if (!offset) {
    return std::nullopt;
  }
  return SoundResource{VoiceModule(side), static_cast<uint16_t>(family->firstWave + *offset)};
}

SoundResource DeathStarRunVoice(GroupColor color, DeathStarRunReport report)
{
  const uint32_t slot = kDeathStarRunSlot[static_cast<size_t>(color)];
  return {SoundModule::VoiceAlliance,
          static_cast<uint16_t>(kDeathStarRunFirstWave + slot * 4 + static_cast<uint32_t>(report))};
}

VoiceChannel ChannelFor(VoiceCue cue)
{
  return cue == VoiceCue::DeathStarPilotChatter ? VoiceChannel::DeathStarRun : VoiceChannel::Report;
}

} // namespace reb::battle_ui
