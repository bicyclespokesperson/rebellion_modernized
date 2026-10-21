#include "battle_ui/battle_audio.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace reb::battle_ui
{
namespace
{
using namespace battle;

const ViewUnit* Unit(const ViewModel& view, UnitIndex index)
{
  const auto found = std::ranges::find(view.units, index, &ViewUnit::unit);
  return found == view.units.end() ? nullptr : &*found;
}

bool Active(const ViewUnit& unit)
{
  return (unit.flags & (kViewDestroyed | kViewWithdrawn | kViewDocked)) == 0;
}

VoiceAddress Address(const ViewModel& view, const ViewUnit& unit)
{
  const auto found = std::ranges::find(view.groups, unit.group, &ViewGroup::id);
  if (found == view.groups.end()) {
    return {};
  }
  const bool flight = found->flight;
  const auto index = std::ranges::count_if(view.groups, [&](const ViewGroup& group) {
    return group.side == unit.side && group.flight == flight && group.id < found->id;
  });
  if (index >= (flight ? kGroupColorCount : kTaskForceCount)) {
    return {};
  }
  return {flight ? Addressee::FighterGroup : Addressee::TaskForce, static_cast<uint8_t>(index)};
}

auto ShotIdentity(const ViewShot& shot)
{
  return std::tuple{shot.fireTick, shot.shooter, shot.target, shot.targetCraft, shot.kind, shot.side,
                    shot.from.x, shot.from.y, shot.to.x, shot.to.y};
}

auto EffectIdentity(const ViewEffect& effect, float seconds)
{
  const int64_t tick = effect.startTick >= 0 ? effect.startTick : std::llround((seconds - effect.age) * 20.0f);
  return std::tuple{tick, effect.kind, effect.unit, effect.position.x, effect.position.y, effect.size};
}

BattleSound Explosion(const ViewUnit* unit)
{
  if (!unit || unit->kind == ViewUnitKind::Squadron || unit->hullMax < 1100.0f) {
    return BattleSound::ExplosionSmall;
  }
  return unit->hullMax <= 2000.0f ? BattleSound::ExplosionMedium : BattleSound::ExplosionLarge;
}

uint16_t EffectFamily(const AudioCue& cue)
{
  for (const auto& effect : EffectSounds()) {
    if (cue.resource.waveId >= effect.firstWave && cue.resource.waveId < effect.firstWave + effect.variants) {
      return effect.firstWave;
    }
  }
  return cue.resource.waveId;
}

} // namespace

AudioPlacement PlaceAudio(Vec2 position, const Camera& camera)
{
  const Rect viewport = camera.Viewport();
  const Point screen = camera.WorldToScreen(position);
  const float x = (screen.x - viewport.Center().x) / std::max(1.0f, viewport.w * 0.5f);
  const float y = (screen.y - viewport.Center().y) / std::max(1.0f, viewport.h * 0.5f);
  return {std::clamp(x, -1.0f, 1.0f), 1.0f / (1.0f + 0.5f * (x * x + y * y))};
}

void BattleAudioCues::Effect(BattleSound sound, Vec2 position)
{
  if (m_cues.size() < 256) {
    m_cues.push_back({EffectResource(sound, m_variant++), position, false, EffectSoundFor(sound).volume});
  }
}

void BattleAudioCues::Voice(VoiceCue cue, Vec2 position, VoiceAddress address, bool interrupts)
{
  if (m_decided) {
    return;
  }
  auto resource = VoiceResource(m_humanSide, cue, address);
  if (!resource && address.kind != Addressee::Ship) {
    resource = VoiceResource(m_humanSide, cue);
  }
  if (resource) {
    const auto families = VoiceFamilies();
    const auto family = std::ranges::find_if(families, [&](const VoiceFamily& value) {
      return value.side == m_humanSide && value.cue == cue;
    });
    m_cues.push_back({*resource, position, true, family->volume, interrupts});
  }
}

void BattleAudioCues::RunVoice(DeathStarRunReport report, const ViewUnit& unit, const ViewModel& view)
{
  if (m_decided) {
    return;
  }
  const auto address = Address(view, unit);
  if (m_humanSide == Side::Alliance && unit.side == m_humanSide && address.kind == Addressee::FighterGroup) {
    m_cues.push_back({DeathStarRunVoice(static_cast<GroupColor>(address.index), report), unit.position, true, 1.0f});
  }
}

void BattleAudioCues::Impact(const ViewShot& shot, const ViewModel& view)
{
  if (!shot.hit) {
    return;
  }
  const ViewUnit* before = m_previous ? Unit(*m_previous, shot.target) : nullptr;
  if (before && (before->flags & (kViewDestroyed | kViewWithdrawn))) {
    return;
  }
  const ViewUnit* target = Unit(view, shot.target);
  if (target && (target->flags & kViewWithdrawn)) {
    return;
  }
  const bool fighter = target && target->kind == ViewUnitKind::Squadron;
  BattleSound impact = BattleSound::ImpactTurbolaserCapital;
  BattleSound shield = BattleSound::ShieldHitTurbolaser;
  BattleSound hull = BattleSound::HullHitTurbolaser;
  if (shot.kind == ViewShotKind::Laser) {
    impact = !fighter ? BattleSound::ImpactLaserCapital
             : target->side == Side::Alliance ? BattleSound::ImpactLaserFighterAlliance
                                              : BattleSound::ImpactLaserFighterEmpire;
    shield = BattleSound::ShieldHitLaser;
    hull = BattleSound::HullHitLaser;
  } else if (shot.kind == ViewShotKind::Ion) {
    impact = fighter ? BattleSound::ImpactIonFighter : BattleSound::ImpactIonCapital;
    shield = BattleSound::ShieldHitIon;
    hull = BattleSound::HullHitIon;
  } else if (fighter) {
    impact = BattleSound::ImpactTurbolaserFighter;
  }
  Effect(impact, shot.to);
  bool shieldHit = shot.shieldImpact;
  // Resolved damage effects supersede the shield estimate made at fire time.
  for (const ViewEffect& effect : view.effects) {
    if (effect.unit == shot.target && effect.startTick == shot.impactTick) {
      if (effect.kind == ViewEffectKind::HullHit || effect.kind == ViewEffectKind::Explosion) {
        shieldHit = false;
        break;
      }
      if (effect.kind == ViewEffectKind::ShieldFlash) {
        shieldHit = true;
      }
    }
  }
  Effect(shieldHit ? shield : hull, shot.to);
}

void BattleAudioCues::Observe(const ViewModel& view)
{
  if (!m_previous) {
    m_previous = view;
    m_pendingShots = view.shots;
    m_decided = view.decided || view.finished;
    if (!m_decided) {
      Voice(VoiceCue::FleetReady, {});
      if (view.tick == 0) {
        for (const ViewUnit& unit : view.units) {
          if (Active(unit) && unit.kind != ViewUnitKind::DeathStar) {
            Effect(unit.kind == ViewUnitKind::Squadron ? BattleSound::JumpInFighter : BattleSound::JumpInCapital,
                   unit.position);
          }
        }
      }
      for (const ViewUnit& unit : view.units) {
        if (unit.kind == ViewUnitKind::DeathStar && Active(unit)) {
          Voice(VoiceCue::DeathStarApproaching, unit.position);
          if (unit.deathStarShielded) {
            Voice(VoiceCue::DeathStarShielded, unit.position);
          } else if (!view.deathStarRunOpen) {
            Voice(VoiceCue::DeathStarTooManyFighters, unit.position);
          } else {
            Voice(VoiceCue::DeathStarUnshielded, unit.position);
          }
        }
      }
    }
    return;
  }
  const ViewModel& previous = *m_previous;
  std::set<decltype(ShotIdentity(ViewShot{}))> previousShots;
  std::map<decltype(ShotIdentity(ViewShot{})), const ViewShot*> currentShots;
  for (const ViewShot& shot : previous.shots) {
    previousShots.insert(ShotIdentity(shot));
  }
  for (const ViewShot& shot : view.shots) {
    currentShots.emplace(ShotIdentity(shot), &shot);
    if (!previousShots.contains(ShotIdentity(shot))) {
      m_pendingShots.push_back(shot);
      if (shot.kind == ViewShotKind::Torpedo) {
        Effect(BattleSound::TorpedoLaunch, shot.from);
      }
    }
  }
  std::erase_if(m_pendingShots, [&](const ViewShot& shot) {
    const auto found = currentShots.find(ShotIdentity(shot));
    if (found != currentShots.end()) {
      if (found->second->progress < 1.0f) {
        return false;
      }
      // Native shots disappear on resolution. Wait for that so damage effects select shield versus hull.
      if (shot.impactTick >= 0) {
        return false;
      }
    } else if (shot.impactTick >= 0 && view.tick <= shot.impactTick) {
      return true; // withdrawn or cancelled before its arrival
    }
    Impact(shot, view);
    return true;
  });

  std::set<decltype(EffectIdentity(ViewEffect{}, 0.0f))> previousEffects;
  for (const ViewEffect& effect : previous.effects) {
    previousEffects.insert(EffectIdentity(effect, previous.battleSeconds));
  }
  for (const ViewEffect& effect : view.effects) {
    if (previousEffects.contains(EffectIdentity(effect, view.battleSeconds))) {
      continue;
    }
    if (effect.kind == ViewEffectKind::Explosion) {
      Effect(Explosion(Unit(view, effect.unit)), effect.position);
    } else if (effect.kind == ViewEffectKind::Superlaser) {
      Effect(BattleSound::SuperlaserImpact, effect.position);
      Voice(VoiceCue::DeathStarFiringAtUs, effect.position);
    }
  }
  for (const ViewTractor& tractor : view.tractors) {
    if (std::ranges::none_of(previous.tractors, [&](const ViewTractor& old) {
          return old.from == tractor.from && old.to == tractor.to;
        })) {
      if (const ViewUnit* source = Unit(view, tractor.from)) {
        Effect(BattleSound::TractorBeamStart, source->position);
      }
    }
  }

  std::set<GroupId> runBegun;
  std::set<GroupId> runEnded;
  for (const ViewUnit& unit : view.units) {
    const ViewUnit* old = Unit(previous, unit.unit);
    if (!old) {
      if (Active(unit)) {
        Effect(unit.kind == ViewUnitKind::Squadron ? BattleSound::JumpInFighter : BattleSound::JumpInCapital,
               unit.position);
      }
      continue;
    }
    const uint32_t added = unit.flags & ~old->flags;
    const uint32_t removed = old->flags & ~unit.flags;
    if (added & kViewTractored) {
      Effect(BattleSound::TractorTargetHeld, unit.position);
    }
    if (removed & kViewTractored) {
      Effect(BattleSound::TractorTargetReleased, unit.position);
    }
    if (added & kViewWithdrawing) {
      Effect(unit.kind == ViewUnitKind::Squadron ? BattleSound::JumpOutFighter : BattleSound::JumpOutCapital,
             unit.position);
    } else if ((added & kViewWithdrawn) && !(old->flags & kViewWithdrawing)) {
      Effect(unit.kind == ViewUnitKind::Squadron ? BattleSound::JumpOutFighter : BattleSound::JumpOutCapital,
             unit.position);
    }
    if ((removed & kViewDocked) && Active(unit) && unit.kind == ViewUnitKind::Squadron) {
      Effect(BattleSound::FighterLaunch, unit.position);
      if (unit.side == m_humanSide) {
        Voice(VoiceCue::FightersLaunched, unit.position, Address(view, unit));
      }
    }
    if (added & kViewDestroyed) {
      if (std::ranges::none_of(view.effects, [&](const ViewEffect& effect) {
            return effect.kind == ViewEffectKind::Explosion && effect.unit == unit.unit;
          })) {
        Effect(Explosion(&unit), unit.position);
      }
      if (unit.side == m_humanSide) {
        Voice(VoiceCue::ShipLost, unit.position, Address(view, unit));
        if (unit.kind == ViewUnitKind::DeathStar) {
          Voice(VoiceCue::DeathStarDestroyed, unit.position);
        }
      } else {
        const auto killer = std::ranges::find_if(previous.shots, [&](const ViewShot& shot) {
          return shot.hit && shot.target == unit.unit && shot.side == m_humanSide
                 && (shot.impactTick < 0 || shot.impactTick < view.tick);
        });
        const ViewUnit* source = killer != previous.shots.end() ? Unit(view, killer->shooter) : nullptr;
        Voice(VoiceCue::TargetDestroyed, unit.position, source ? Address(view, *source) : VoiceAddress{});
      }
    }
    if (unit.side == m_humanSide) {
      if (unit.jumpBlocked && !old->jumpBlocked && (unit.flags & kViewWithdrawing)) {
        Voice(VoiceCue::WithdrawBlockedShips, unit.position);
      }
      if (unit.inTrench && !old->inTrench && runBegun.insert(unit.group).second) {
        RunVoice(DeathStarRunReport::Begun, unit, view);
      }
      if (old->deathStarRun && (!unit.deathStarRun || (added & kViewDestroyed))
          && runEnded.insert(unit.group).second) {
        const bool destroyed = std::ranges::any_of(view.units, [](const ViewUnit& target) {
          return target.kind == ViewUnitKind::DeathStar && (target.flags & kViewDestroyed);
        });
        RunVoice(destroyed ? DeathStarRunReport::Succeeded : DeathStarRunReport::Failed, unit, view);
      }
    } else if (unit.inTrench && !old->inTrench && runBegun.insert(unit.group).second) {
      Voice(VoiceCue::DeathStarUnderAttack, unit.position);
    } else if (old->deathStarRun && (!unit.deathStarRun || (added & kViewDestroyed))
               && runEnded.insert(unit.group).second) {
      Voice(VoiceCue::DeathStarAttackBrokenOff, unit.position);
    }
  }
  const size_t human = static_cast<size_t>(m_humanSide);
  if (view.sides[human].withdrawing && !previous.sides[human].withdrawing) {
    Voice(VoiceCue::WithdrawOrdered, {});
    if (std::ranges::any_of(view.units, [&](const ViewUnit& unit) {
          return unit.side == m_humanSide && Active(unit) && unit.jumpBlocked;
        })) {
      Voice(VoiceCue::WithdrawBlockedShips, {});
    }
  }
  if (view.superlaserCharge >= 1.0f && previous.superlaserCharge < 1.0f) {
    Voice(VoiceCue::DeathStarCharged, {});
  }
  if (view.superlaserCharge >= 0.0f && view.superlaserCharge < previous.superlaserCharge) {
    Voice(VoiceCue::DeathStarRecharging, {});
  }
  if (view.deathStarRunOpen && !previous.deathStarRunOpen) {
    Voice(VoiceCue::DeathStarOpening, {});
  }
  if ((view.decided || view.finished) && !m_decided) {
    SpeakResult(view);
  }
  m_previous = view;
}

// Once the battle is decided only the result line is spoken. Reports from the final exchange would otherwise queue
// ahead of it and outlast the wind-down.
void BattleAudioCues::SpeakResult(const ViewModel& view)
{
  std::erase_if(m_cues, [](const AudioCue& cue) { return cue.voice; });
  const auto present = [&](bool own) {
    return std::ranges::any_of(view.units, [&](const ViewUnit& unit) {
      return (unit.side == m_humanSide) == own && (unit.flags & (kViewDestroyed | kViewWithdrawn)) == 0;
    });
  };
  const auto withdrew = [&](bool own) {
    return std::ranges::any_of(view.units, [&](const ViewUnit& unit) {
      return (unit.side == m_humanSide) == own && (unit.flags & kViewWithdrawn);
    });
  };
  constexpr bool kInterrupts = true;
  if (!present(true)) {
    Voice(withdrew(true) ? VoiceCue::WithdrawalComplete : VoiceCue::FleetDestroyed, {}, {}, kInterrupts);
  } else if (!present(false)) {
    Voice(withdrew(false) ? VoiceCue::EnemyWithdrew : VoiceCue::EnemyDestroyed, {}, {}, kInterrupts);
  }
  m_decided = true;
}

void BattleAudioCues::Submitted(const Command& command, const ViewModel& view)
{
  if (command.side != m_humanSide || std::holds_alternative<OrderDeploy>(command.order)) {
    return;
  }
  if (std::holds_alternative<OrderFireSuperlaser>(command.order)) {
    Voice(VoiceCue::DeathStarFireAck, {});
    return;
  }
  if (std::holds_alternative<OrderWithdraw>(command.order)) {
    Voice(VoiceCue::WithdrawOrdered, {});
    if (std::ranges::any_of(view.units, [&](const ViewUnit& unit) {
          const bool selected = std::ranges::find(command.target.units, unit.unit) != command.target.units.end()
              || std::ranges::find(command.target.groups, unit.group) != command.target.groups.end();
          return selected && unit.side == m_humanSide && unit.jumpBlocked;
        })) {
      Voice(VoiceCue::WithdrawBlockedShips, {});
    }
    return;
  }
  VoiceCue cue = VoiceCue::MissionAck;
  if (std::holds_alternative<OrderAttack>(command.order) || std::holds_alternative<OrderStrike>(command.order)) {
    cue = VoiceCue::AttackAck;
  } else if (std::holds_alternative<OrderMove>(command.order)
             || std::holds_alternative<OrderApproach>(command.order)
             || std::holds_alternative<OrderHold>(command.order)) {
    cue = VoiceCue::ManeuverAck;
  } else if (const auto* doctrine = std::get_if<OrderSetShipDoctrine>(&command.order);
             doctrine && (doctrine->fields & kShipFormation)) {
    cue = VoiceCue::FormationAck;
  }
  std::set<std::pair<Addressee, uint8_t>> spoken;
  for (const ViewUnit& unit : view.units) {
    const bool selected = std::ranges::find(command.target.units, unit.unit) != command.target.units.end()
                          || std::ranges::find(command.target.groups, unit.group) != command.target.groups.end();
    if (!selected || unit.side != m_humanSide || (unit.flags & (kViewDestroyed | kViewWithdrawn))) {
      continue;
    }
    const VoiceAddress address = Address(view, unit);
    if (!spoken.insert({address.kind, address.index}).second) {
      continue;
    }
    if (std::holds_alternative<OrderDeathStarRun>(command.order)) {
      RunVoice(DeathStarRunReport::Ordered, unit, view);
    } else {
      Voice(cue, unit.position, address);
    }
  }
}

std::vector<AudioCue> BattleAudioCues::Take()
{
  return std::exchange(m_cues, {});
}

void BattleAudioMixer::Update(std::span<const AudioCue> cues, const Camera& camera, double seconds,
                              AudioPlayback& playback)
{
  std::erase_if(m_active, [&](const Active& active) { return !playback.Playing(active.handle); });
  for (const Active& active : m_active) {
    playback.Reposition(active.handle, PlaceAudio(active.cue.position, camera));
  }
  std::erase_if(m_voices, [&](const Waiting& waiting) { return seconds - waiting.time > kVoicePatienceSeconds; });
  // An interrupting voice also supersedes the voices before it in this batch.
  const auto interrupting = [](const AudioCue& cue) { return cue.voice && cue.interrupts; };
  const auto afterInterrupt = std::find_if(cues.rbegin(), cues.rend(), interrupting).base();
  const auto firstVoice = afterInterrupt == cues.begin() ? cues.begin() : afterInterrupt - 1;
  if (afterInterrupt != cues.begin()) {
    SilenceVoices(playback);
  }
  const auto speaking = [&] {
    return std::ranges::any_of(m_active, [](const Active& active) { return active.cue.voice; });
  };
  const auto start = [&](const AudioCue& cue) {
    if (auto handle = playback.Start(cue, PlaceAudio(cue.position, camera))) {
      m_active.push_back({cue, *handle});
    }
  };
  while (!speaking() && !m_voices.empty()) {
    const AudioCue cue = m_voices.front().cue;
    m_voices.pop_front();
    start(cue);
  }
  for (auto it = cues.begin(); it != cues.end(); ++it) {
    const AudioCue& cue = *it;
    if (cue.voice) {
      if (it < firstVoice) {
        continue;
      }
      const bool repeated = std::ranges::any_of(m_active, [&](const Active& active) {
        return active.cue.voice && active.cue.resource == cue.resource;
      }) || std::ranges::any_of(m_voices, [&](const Waiting& waiting) { return waiting.cue.resource == cue.resource; });
      if (repeated) {
        continue;
      }
      if (!speaking()) {
        start(cue);
      } else if (m_voices.size() < kPendingVoices) {
        m_voices.push_back({cue, seconds});
      }
      continue;
    }
    const auto effects = std::ranges::count_if(m_active, [](const Active& active) { return !active.cue.voice; });
    const auto identical = std::ranges::count_if(m_active, [&](const Active& active) {
      return !active.cue.voice && active.cue.resource.module == cue.resource.module
             && EffectFamily(active.cue) == EffectFamily(cue);
    });
    if (effects < kEffectPolyphony && identical < kIdenticalEffects) {
      start(cue);
    }
  }
}

void BattleAudioMixer::SilenceVoices(AudioPlayback& playback)
{
  std::erase_if(m_active, [&](const Active& active) {
    if (active.cue.voice) {
      playback.Cancel(active.handle);
    }
    return active.cue.voice;
  });
  m_voices.clear();
}

void BattleAudioMixer::Stop(AudioPlayback& playback)
{
  playback.Stop();
  m_active.clear();
  m_voices.clear();
}

} // namespace reb::battle_ui
