#include "battle_ui/battle_sounds.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>

using namespace reb::battle_ui;
using reb::battle::Side;

namespace
{

void Require(bool condition)
{
  if (!condition) {
    std::abort();
  }
}

std::set<uint16_t> Range(uint16_t first, uint16_t last)
{
  std::set<uint16_t> waves;
  for (uint16_t wave = first; wave <= last; ++wave) {
    waves.insert(wave);
  }
  return waves;
}

void TestEffectsCoverEveryWaveOnce()
{
  Require(static_cast<int>(EffectSounds().size()) == kBattleSoundCount);
  std::map<uint8_t, EffectSound> byOriginalId;
  for (int i = 0; i < kBattleSoundCount; ++i) {
    const EffectSound& effect = EffectSoundFor(static_cast<BattleSound>(i));
    Require(effect.sound == static_cast<BattleSound>(i));
    Require(effect.variants >= 1 && effect.volume > 0.0f);
    const auto [it, inserted] = byOriginalId.try_emplace(effect.originalId, effect);
    Require(inserted || (it->second.firstWave == effect.firstWave && it->second.variants == effect.variants));
  }
  Require(byOriginalId.size() == 32);
  std::set<uint16_t> waves;
  for (const auto& [id, effect] : byOriginalId) {
    Require(id < 32);
    for (int variant = 0; variant < effect.variants; ++variant) {
      Require(waves.insert(static_cast<uint16_t>(effect.firstWave + variant)).second);
    }
  }
  Require(waves == Range(13000, 13065));
}

void TestEffectVariantsWrap()
{
  Require(EffectResource(BattleSound::HullHitIon, 0) == SoundResource{SoundModule::Tactical, 13051});
  Require(EffectResource(BattleSound::HullHitIon, 5) == SoundResource{SoundModule::Tactical, 13053});
  Require(EffectResource(BattleSound::SuperlaserImpact, 99) == SoundResource{SoundModule::Tactical, 13054});
  Require(EffectResource(BattleSound::ImpactTurbolaserFighter, 3) == EffectResource(BattleSound::TorpedoLaunch, 3));
}

std::set<uint16_t> VoiceWaves(Side side)
{
  std::set<uint16_t> waves;
  for (const VoiceFamily& family : VoiceFamilies()) {
    if (family.side != side) {
      continue;
    }
    const int clips = family.ships + family.taskForces + family.groups;
    for (int i = 0; i < clips; ++i) {
      Require(waves.insert(static_cast<uint16_t>(family.firstWave + i)).second);
    }
  }
  if (side == Side::Alliance) {
    for (int color = 0; color < kGroupColorCount; ++color) {
      for (int report = 0; report < 4; ++report) {
        const SoundResource clip
            = DeathStarRunVoice(static_cast<GroupColor>(color), static_cast<DeathStarRunReport>(report));
        Require(clip.module == SoundModule::VoiceAlliance);
        Require(waves.insert(clip.waveId).second);
      }
    }
  }
  return waves;
}

void TestVoicesPartitionEveryClipOnce()
{
  std::set<uint16_t> alliance = Range(14001, 14122);
  alliance.merge(Range(15133, 15163));
  Require(alliance.size() == 153);
  Require(VoiceWaves(Side::Alliance) == alliance);
  Require(VoiceWaves(Side::Empire) == Range(15001, 15132));
  Require(VoiceModule(Side::Alliance) == SoundModule::VoiceAlliance);
  Require(VoiceModule(Side::Empire) == SoundModule::VoiceEmpire);
}

void TestVoiceLookup()
{
  const VoiceAddress taskForceThree{Addressee::TaskForce, 2};
  const VoiceAddress gold{Addressee::FighterGroup, static_cast<uint8_t>(GroupColor::Gold)};
  Require(VoiceResource(Side::Alliance, VoiceCue::ManeuverAck, taskForceThree)
          == SoundResource{SoundModule::VoiceAlliance, 14005});
  Require(VoiceResource(Side::Alliance, VoiceCue::ManeuverAck) == SoundResource{SoundModule::VoiceAlliance, 14002});
  Require(VoiceResource(Side::Empire, VoiceCue::TargetDestroyed, gold)
          == SoundResource{SoundModule::VoiceEmpire, 15066});
  Require(VoiceResource(Side::Empire, VoiceCue::FormationAck, {Addressee::TaskForce, 7})
          == SoundResource{SoundModule::VoiceEmpire, 15091});
  Require(VoiceResource(Side::Alliance, VoiceCue::FightersRecovered, {Addressee::FighterGroup, 0})
          == SoundResource{SoundModule::VoiceAlliance, 14104});
  Require(VoiceResource(Side::Empire, VoiceCue::DeathStarCharged) == SoundResource{SoundModule::VoiceEmpire, 15122});
  Require(VoiceResource(Side::Alliance, VoiceCue::DeathStarPilotChatter, {Addressee::Ship, 14})
          == SoundResource{SoundModule::VoiceAlliance, 15147});

  Require(!VoiceResource(Side::Alliance, VoiceCue::FormationAck));
  Require(!VoiceResource(Side::Alliance, VoiceCue::FightersLaunched, gold));
  Require(!VoiceResource(Side::Empire, VoiceCue::DeathStarApproaching));
  Require(!VoiceResource(Side::Alliance, VoiceCue::ManeuverAck, {Addressee::TaskForce, 8}));
  Require(!VoiceResource(Side::Alliance, VoiceCue::ManeuverAck, {Addressee::FighterGroup, 4}));
  Require(!VoiceResource(Side::Alliance, VoiceCue::FleetReady, {Addressee::Ship, 1}));
}

void TestDeathStarRunVoices()
{
  Require(DeathStarRunVoice(GroupColor::Blue, DeathStarRunReport::Ordered).waveId == 15148);
  Require(DeathStarRunVoice(GroupColor::Green, DeathStarRunReport::Begun).waveId == 15153);
  Require(DeathStarRunVoice(GroupColor::Gold, DeathStarRunReport::Failed).waveId == 15158);
  Require(DeathStarRunVoice(GroupColor::Red, DeathStarRunReport::Succeeded).waveId == 15163);
}

void TestModulesAndChannels()
{
  Require(ModuleFileName(SoundModule::Tactical) == "TACTICAL.DLL");
  Require(ModuleFileName(SoundModule::VoiceAlliance) == "VOICEFXA.DLL");
  Require(ModuleFileName(SoundModule::VoiceEmpire) == "VOICEFXE.DLL");
  Require(ChannelFor(VoiceCue::FleetReady) == VoiceChannel::Report);
  Require(ChannelFor(VoiceCue::DeathStarPilotChatter) == VoiceChannel::DeathStarRun);
  Require(kEffectPolyphony == 5 && kVoicePolyphony == 1);
}

} // namespace

int main()
{
  TestEffectsCoverEveryWaveOnce();
  TestEffectVariantsWrap();
  TestVoicesPartitionEveryClipOnce();
  TestVoiceLookup();
  TestDeathStarRunVoices();
  TestModulesAndChannels();
  return 0;
}
