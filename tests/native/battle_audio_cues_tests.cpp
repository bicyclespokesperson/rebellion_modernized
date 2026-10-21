#include "battle_ui/battle_audio.h"

#include <algorithm>
#include <cstdlib>
#include <variant>

#include "battle_ui_screen_driver.h"
#include "battle_ui_test_support.h"

using namespace reb::battle_ui;
using namespace reb::battle_ui::test;
using namespace reb::battle;

namespace
{

int Count(const std::vector<AudioCue>& cues, BattleSound sound)
{
  const auto resource = EffectResource(sound, 0);
  return static_cast<int>(std::ranges::count_if(cues, [&](const AudioCue& cue) {
    const auto& family = EffectSoundFor(sound);
    return !cue.voice && cue.resource.module == resource.module && cue.resource.waveId >= family.firstWave
           && cue.resource.waveId < family.firstWave + family.variants;
  }));
}

bool HasEffectAt(const std::vector<AudioCue>& cues, BattleSound sound, Vec2 position)
{
  const auto& family = EffectSoundFor(sound);
  return std::ranges::any_of(cues, [&](const AudioCue& cue) {
    return !cue.voice && cue.resource.module == SoundModule::Tactical && cue.resource.waveId >= family.firstWave
        && cue.resource.waveId < family.firstWave + family.variants
        && cue.position.x == position.x && cue.position.y == position.y;
  });
}

bool HasVoice(const std::vector<AudioCue>& cues, VoiceCue voice, Side side = Side::Alliance,
              VoiceAddress address = {})
{
  const auto resource = VoiceResource(side, voice, address);
  return resource && std::ranges::any_of(cues, [&](const AudioCue& cue) { return cue.voice && cue.resource == *resource; });
}

class TorpedoScriptSource final : public RecordingSource
{
  public:
  void Step() override
  {
    RecordingSource::Step();
    ViewShot shot;
    shot.kind = ViewShotKind::Torpedo;
    shot.side = Side::Alliance;
    shot.from = {100.0f, 200.0f};
    shot.to = {6000.0f, 0.0f};
    shot.fireTick = 1;
    shot.impactTick = 2;
    shot.shooter = 0;
    shot.target = 2;
    shot.hit = true;
    shot.shieldImpact = true;
    if (steps <= 2) {
      shot.progress = steps == 1 ? 0.5f : 1.0f;
      View().shots = {shot};
    } else if (steps == 3) {
      View().shots.clear();
      View().effects = {{ViewEffectKind::HullHit, shot.to, 0.0f, 0.0f, shot.target, 2}};
    }
  }
};

void TestShotImpactAndMiss()
{
  auto view = SmallBattle();
  BattleAudioCues audio;
  audio.Observe(view);
  audio.Take();

  ViewShot shot;
  shot.kind = ViewShotKind::Laser;
  shot.side = Side::Alliance;
  shot.from = {0.0f, 0.0f};
  shot.to = {6000.0f, 0.0f};
  shot.hit = true;
  shot.shieldImpact = true;
  shot.fireTick = 1;
  shot.impactTick = 2;
  shot.shooter = 0;
  shot.target = 2;
  view.tick = 1;
  view.shots = {shot};
  audio.Observe(view);
  REQUIRE(audio.Take().empty());
  view.tick = 3;
  view.shots.clear();
  audio.Observe(view);
  auto cues = audio.Take();
  REQUIRE(Count(cues, BattleSound::ImpactLaserCapital) == 1);
  REQUIRE(Count(cues, BattleSound::ShieldHitLaser) == 1);
  REQUIRE(Count(cues, BattleSound::HullHitLaser) == 0);
  view.tick = 3;
  audio.Observe(view);
  REQUIRE(audio.Take().empty());

  shot.hit = false;
  shot.fireTick = 4;
  view.tick = 4;
  view.shots = {shot};
  audio.Observe(view);
  audio.Take();
  view.tick = 5;
  view.shots.clear();
  audio.Observe(view);
  REQUIRE(audio.Take().empty());
}

void TestTorpedoLaunchAndResolvedHull()
{
  auto view = SmallBattle();
  BattleAudioCues audio;
  audio.Observe(view);
  audio.Take();
  ViewShot shot;
  shot.kind = ViewShotKind::Torpedo;
  shot.from = {123.0f, 456.0f};
  shot.to = {6000.0f, 0.0f};
  shot.fireTick = 1;
  shot.impactTick = 2;
  shot.shooter = 0;
  shot.target = 2;
  shot.hit = true;
  shot.shieldImpact = true;
  view.tick = 1;
  view.shots = {shot};
  audio.Observe(view);
  auto cues = audio.Take();
  REQUIRE(Count(cues, BattleSound::TorpedoLaunch) == 1);
  for (const auto& cue : cues) {
    if (cue.resource == EffectResource(BattleSound::TorpedoLaunch, 0)) {
      REQUIRE(cue.position.x == shot.from.x && cue.position.y == shot.from.y);
    }
  }
  view.tick = 3;
  view.shots.clear();
  view.effects = {{ViewEffectKind::HullHit, shot.to, 0.0f, 0.0f, shot.target, 2}};
  audio.Observe(view);
  cues = audio.Take();
  REQUIRE(Count(cues, BattleSound::ImpactTurbolaserCapital) == 1);
  REQUIRE(Count(cues, BattleSound::HullHitTurbolaser) == 1);
  REQUIRE(Count(cues, BattleSound::ShieldHitTurbolaser) == 0);
}

void TestExplosionSizeAndFighterSize()
{
  auto view = SmallBattle();
  view.units[0].hullMax = 1099.0f;
  view.units[1].hullMax = 1100.0f;
  view.units[2].hullMax = 2000.0f;
  view.units[3].hullMax = 9000.0f;
  view.units[4].flags = 0;
  view.units[4].hullMax = 2001.0f;
  BattleAudioCues audio;
  audio.Observe(view);
  audio.Take();
  view.tick = 1;
  view.effects = {{ViewEffectKind::Explosion, {}, 1.0f, 0.0f, 0, 1},
                  {ViewEffectKind::Explosion, {}, 1.0f, 0.0f, 1, 1},
                  {ViewEffectKind::Explosion, {}, 1.0f, 0.0f, 2, 1},
                  {ViewEffectKind::Explosion, {}, 1.0f, 0.0f, 3, 1},
                  {ViewEffectKind::Explosion, {}, 1.0f, 0.0f, 4, 1}};
  audio.Observe(view);
  const auto cues = audio.Take();
  REQUIRE(Count(cues, BattleSound::ExplosionSmall) == 2);
  REQUIRE(Count(cues, BattleSound::ExplosionMedium) == 2);
  REQUIRE(Count(cues, BattleSound::ExplosionLarge) == 1);
}

void TestHumanVoiceCommandsAndRun()
{
  auto view = SmallBattle();
  view.groups[0].side = Side::Alliance;
  BattleAudioCues audio(Side::Alliance);
  Command command;
  command.side = Side::Alliance;
  command.target.groups = {0};
  command.order = OrderMove{};
  audio.Submitted(command, view);
  auto cues = audio.Take();
  REQUIRE(cues.size() == 1 && cues[0].voice);
  REQUIRE(cues[0].resource == *VoiceResource(Side::Alliance, VoiceCue::ManeuverAck,
                                              {Addressee::TaskForce, 0}));
  command.side = Side::Empire;
  audio.Submitted(command, view);
  REQUIRE(audio.Take().empty());

  command.side = Side::Alliance;
  command.order = OrderDeathStarRun{};
  command.target.groups = {1};
  audio.Submitted(command, view);
  cues = audio.Take();
  REQUIRE(cues.size() == 1 && cues[0].resource == DeathStarRunVoice(GroupColor::Red, DeathStarRunReport::Ordered));

  view.units[3].inTrench = false;
  audio.Observe(view);
  audio.Take();
  auto next = view;
  next.tick = 1;
  next.units[3].inTrench = true;
  next.units[3].deathStarRun = true;
  audio.Observe(next);
  cues = audio.Take();
  REQUIRE(cues.size() == 1 && cues[0].resource == DeathStarRunVoice(GroupColor::Red, DeathStarRunReport::Begun));
  next.tick = 2;
  next.units[3].deathStarRun = false;
  next.units[2].kind = ViewUnitKind::DeathStar;
  next.units[2].flags = kViewDestroyed;
  audio.Observe(next);
  cues = audio.Take();
  REQUIRE(std::ranges::any_of(cues, [](const AudioCue& cue) {
    return cue.resource == DeathStarRunVoice(GroupColor::Red, DeathStarRunReport::Succeeded);
  }));
}

void TestLaunchSuperlaserAndJumpCues()
{
  auto view = SmallBattle();
  BattleAudioCues audio;
  audio.Observe(view);
  auto cues = audio.Take();
  REQUIRE(Count(cues, BattleSound::JumpInCapital) == 3);
  REQUIRE(Count(cues, BattleSound::JumpInFighter) == 1);
  auto next = view;
  next.tick = 1;
  next.units[3].flags = kViewDocked;
  audio.Observe(next);
  cues = audio.Take();
  REQUIRE(Count(cues, BattleSound::FighterLaunch) == 0);
  next.tick = 2;
  next.units[3].flags = 0;
  audio.Observe(next);
  cues = audio.Take();
  REQUIRE(Count(cues, BattleSound::FighterLaunch) == 1);
  REQUIRE(!HasVoice(cues, VoiceCue::FightersLaunched, Side::Alliance));

  next.tick = 3;
  next.effects = {{ViewEffectKind::Superlaser, {10.0f, 20.0f}, 0.0f, 0.0f, 2, 3}};
  audio.Observe(next);
  cues = audio.Take();
  REQUIRE(Count(cues, BattleSound::SuperlaserImpact) == 1);
  REQUIRE(HasVoice(cues, VoiceCue::DeathStarFiringAtUs));

  Command fire;
  fire.side = Side::Alliance;
  fire.order = OrderFireSuperlaser{};
  audio.Submitted(fire, next);
  REQUIRE(!HasVoice(audio.Take(), VoiceCue::DeathStarFireAck));

  auto empire = SmallBattle();
  empire.units[2].kind = ViewUnitKind::Squadron;
  empire.units[2].group = 4;
  ViewGroup empireFlight;
  empireFlight.id = 4;
  empireFlight.side = Side::Empire;
  empireFlight.flight = true;
  empire.groups.push_back(empireFlight);
  BattleAudioCues empireAudio(Side::Empire);
  empireAudio.Observe(empire);
  empireAudio.Take();
  auto empireNext = empire;
  empireNext.tick = 1;
  empireNext.units[2].flags = kViewDocked;
  empireAudio.Observe(empireNext);
  empireAudio.Take();
  empireNext.tick = 2;
  empireNext.units[2].flags = 0;
  empireAudio.Observe(empireNext);
  REQUIRE(HasVoice(empireAudio.Take(), VoiceCue::FightersLaunched, Side::Empire,
                   {Addressee::FighterGroup, 0}));
  Command empireFire;
  empireFire.side = Side::Empire;
  empireFire.order = OrderFireSuperlaser{};
  empireAudio.Submitted(empireFire, empireNext);
  REQUIRE(HasVoice(empireAudio.Take(), VoiceCue::DeathStarFireAck, Side::Empire));
}

void TestVoiceClipIndicesForTaskForcesAndFlights()
{
  auto view = SmallBattle();
  ViewGroup earlierTaskForce;
  earlierTaskForce.id = -2;
  earlierTaskForce.side = Side::Alliance;
  ViewGroup earlierFlight;
  earlierFlight.id = -1;
  earlierFlight.side = Side::Alliance;
  earlierFlight.flight = true;
  view.groups.insert(view.groups.begin(), earlierFlight);
  view.groups.insert(view.groups.begin(), earlierTaskForce);

  BattleAudioCues audio;
  Command command;
  command.side = Side::Alliance;
  command.order = OrderMove{};
  command.target.groups = {0};
  audio.Submitted(command, view);
  auto cues = audio.Take();
  REQUIRE(cues.size() == 1);
  REQUIRE(cues[0].resource == *VoiceResource(Side::Alliance, VoiceCue::ManeuverAck,
                                              {Addressee::TaskForce, 1}));

  command.target.groups = {1};
  audio.Submitted(command, view);
  cues = audio.Take();
  REQUIRE(cues.size() == 1);
  REQUIRE(cues[0].resource == *VoiceResource(Side::Alliance, VoiceCue::ManeuverAck,
                                              {Addressee::FighterGroup, 1}));
}

void TestTractorWithdrawalDestructionAndBattleResult()
{
  auto view = SmallBattle();
  BattleAudioCues audio;
  audio.Observe(view);
  audio.Take();
  auto next = view;
  next.tick = 1;
  next.tractors = {{0, 2}};
  next.units[2].flags |= kViewTractored;
  audio.Observe(next);
  auto cues = audio.Take();
  REQUIRE(Count(cues, BattleSound::TractorBeamStart) == 1);
  REQUIRE(Count(cues, BattleSound::TractorTargetHeld) == 1);

  next.tick = 2;
  next.tractors.clear();
  next.units[2].flags &= ~kViewTractored;
  audio.Observe(next);
  REQUIRE(Count(audio.Take(), BattleSound::TractorTargetReleased) == 1);

  next.tick = 3;
  next.units[0].flags |= kViewDestroyed;
  audio.Observe(next);
  cues = audio.Take();
  REQUIRE(HasVoice(cues, VoiceCue::ShipLost, Side::Alliance, {Addressee::TaskForce, 0}));

  next.tick = 4;
  next.units[2].flags |= kViewDestroyed;
  next.decided = true;
  audio.Observe(next);
  REQUIRE(HasVoice(audio.Take(), VoiceCue::EnemyDestroyed));
  next.finished = true;
  audio.Observe(next);
  REQUIRE(!HasVoice(audio.Take(), VoiceCue::EnemyDestroyed));

  auto withdrawing = SmallBattle();
  BattleAudioCues withdrawalAudio;
  withdrawalAudio.Observe(withdrawing);
  withdrawalAudio.Take();
  withdrawing.tick = 1;
  withdrawing.units[0].flags |= kViewWithdrawing;
  withdrawalAudio.Observe(withdrawing);
  REQUIRE(Count(withdrawalAudio.Take(), BattleSound::JumpOutCapital) == 1);
}

void TestOnlyTheResultIsSpokenOnceDecided()
{
  ViewShot shot;
  shot.kind = ViewShotKind::Laser;
  shot.side = Side::Alliance;
  shot.from = {0.0f, 0.0f};
  shot.to = {6000.0f, 0.0f};
  shot.hit = true;
  shot.fireTick = 1;
  shot.impactTick = 2;
  shot.shooter = 0;
  shot.target = 2;
  const auto lastKill = [&](bool decides) {
    BattleAudioCues audio;
    auto view = SmallBattle();
    audio.Observe(view);
    view.tick = 1;
    view.shots = {shot};
    audio.Observe(view);
    audio.Take();
    view.tick = 3;
    view.shots.clear();
    view.units[2].flags |= kViewDestroyed;
    view.decided = decides;
    audio.Observe(view);
    return std::pair{audio.Take(), std::move(audio)};
  };
  REQUIRE(HasVoice(lastKill(false).first, VoiceCue::TargetDestroyed, Side::Alliance, {Addressee::TaskForce, 0}));

  auto [cues, audio] = lastKill(true);
  REQUIRE(std::ranges::count_if(cues, &AudioCue::voice) == 1);
  REQUIRE(HasVoice(cues, VoiceCue::EnemyDestroyed));
  REQUIRE(std::ranges::find_if(cues, &AudioCue::voice)->interrupts);
  REQUIRE(Count(cues, BattleSound::ExplosionMedium) + Count(cues, BattleSound::ExplosionSmall) == 1);

  auto view = SmallBattle();
  view.units[2].flags |= kViewDestroyed;
  view.decided = true;
  Command command;
  command.side = Side::Alliance;
  command.order = OrderMove{};
  command.target.groups = {0};
  audio.Submitted(command, view);
  view.tick = 4;
  view.units[0].flags |= kViewDestroyed;
  audio.Observe(view);
  REQUIRE(std::ranges::none_of(audio.Take(), &AudioCue::voice));
}

void TestBattleScreenCollectsTransientShotCues()
{
  TorpedoScriptSource source;
  FixedTextMeasurer text;
  NoSprites sprites;
  BattleScreenOptions options;
  options.startPaused = false;
  BattleScreen screen(source, text, sprites, options);
  screen.Update(0.2);
  const auto cues = screen.TakeAudioCues();
  REQUIRE(source.steps == 4 && source.View().shots.empty());
  REQUIRE(Count(cues, BattleSound::TorpedoLaunch) == 2);
  REQUIRE(HasEffectAt(cues, BattleSound::TorpedoLaunch, {100.0f, 200.0f}));
  REQUIRE(HasEffectAt(cues, BattleSound::ImpactTurbolaserCapital, {6000.0f, 0.0f}));
  REQUIRE(Count(cues, BattleSound::HullHitTurbolaser) == 1);
  REQUIRE(Count(cues, BattleSound::ShieldHitTurbolaser) == 0);
}

void TestBlockedWithdrawalAndDockedLaunchAcknowledgements()
{
  auto view = SmallBattle();
  BattleAudioCues audio;
  view.units[0].jumpBlocked = true;
  Command withdraw;
  withdraw.side = Side::Alliance;
  withdraw.target.groups = {0};
  withdraw.order = OrderWithdraw{};
  audio.Submitted(withdraw, view);
  auto cues = audio.Take();
  REQUIRE(HasVoice(cues, VoiceCue::WithdrawOrdered));
  REQUIRE(HasVoice(cues, VoiceCue::WithdrawBlockedShips));

  view.units[3].flags = kViewDocked;
  Command launch;
  launch.side = Side::Alliance;
  launch.target.groups = {1};
  launch.order = OrderLaunch{};
  audio.Submitted(launch, view);
  cues = audio.Take();
  REQUIRE(cues.size() == 1);
  REQUIRE(cues[0].resource == *VoiceResource(Side::Alliance, VoiceCue::MissionAck,
                                              {Addressee::FighterGroup, 0}));
}

} // namespace

int main()
{
  TestShotImpactAndMiss();
  TestTorpedoLaunchAndResolvedHull();
  TestExplosionSizeAndFighterSize();
  TestHumanVoiceCommandsAndRun();
  TestLaunchSuperlaserAndJumpCues();
  TestVoiceClipIndicesForTaskForcesAndFlights();
  TestTractorWithdrawalDestructionAndBattleResult();
  TestOnlyTheResultIsSpokenOnceDecided();
  TestBattleScreenCollectsTransientShotCues();
  TestBlockedWithdrawalAndDockedLaunchAcknowledgements();
  return 0;
}
