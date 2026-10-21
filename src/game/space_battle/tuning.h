#pragma once
#include <array>
#include <cstdint>
#include <numbers>

#include "space_battle/battle.h"

// Every tuning constant of the space battle simulator (tactical_combat.md §6.14), in one place for calibration.
// Units: meters, seconds, radians. "Legacy step" means one step of the retired auto-resolve, whose rates the record
// values are expressed in.
namespace reb::battle::tuning
{

inline constexpr float kDegrees = std::numbers::pi_v<float> / 180.0f;

// ---- scale and pacing (§6.1) ----------------------------------------------------------------------------------------
// The design's distances and speeds assume 100 m per range point. Ships are drawn at the original's compressed lengths
// (below), which made weapon reach only two to five hull lengths at that scale, so the battle is laid out
// kWorldScale times larger: every design distance and speed here is multiplied by it, which keeps closing times,
// turning circles and pacing while ships become smaller relative to their reach. Ship lengths are not scaled.
inline constexpr float kWorldScale = kMetersPerRangePoint / 100.0f;
constexpr float Meters(float designMeters)
{
  return designMeters * kWorldScale;
}
inline constexpr float kRangePointMeters = kMetersPerRangePoint;
inline constexpr float kCapitalSpeedPerSublight = Meters(60.0f);
inline constexpr float kFighterSpeedPerSublight = Meters(100.0f);
inline constexpr float kTurnRatePerPoint = 10.0f * kDegrees;
inline constexpr float kCapitalAccelerationSeconds = 10.0f; // time to reach full speed from rest
inline constexpr float kCapitalDecelerationFactor = 2.0f;   // braking is this much faster than accelerating
// Converts the records' per-step weapon and shield recharge rates to per-second rates. One global scale, held fixed
// for calibration once the pacing targets hold.
inline constexpr float kLegacyStepsPerSecond = 2.0f;

// ---- commanders (§5.6, §6.2) ----------------------------------------------------------------------------------------
inline constexpr float kAdmiralMaxSpeedBonus = 0.15f;
inline constexpr float kAdmiralTurnTermMin = 1.0f; // no admiral, as in the original
inline constexpr float kAdmiralTurnTermMax = 3.0f;
inline constexpr float kLeadershipForFullRating = 100.0f;
inline constexpr float kAdmiralTransitionReduction = 0.4f; // a perfect admiral shortens transitions by this share
inline constexpr float kCommanderRadius = Meters(5000.0f);
inline constexpr float kCommanderAccuracyBonus = 0.25f; // at combat skill 100
inline constexpr float kCommanderEvasionBonus = 0.25f;
inline constexpr float kCombatForFullBonus = 100.0f;

// ---- hull size (§7.1) ---------------------------------------------------------------------------------------------------
// Class lengths in meters, keyed by the tactical ship-type table index (a presentation key only): the original's
// TACTICAL.DLL mesh lengths x 800, the compressed scale the sprites use. A unit's radius is half its length.
struct ClassLength {
  int32_t typeIndex;
  float length;
};
inline constexpr std::array<ClassLength, 30> kClassLengths{{
    {0, 1750.0f},  {1, 1258.0f},  {2, 1138.0f},  {3, 805.0f},   {4, 1414.0f},  {5, 796.0f},   {6, 1193.0f},
    {7, 1047.0f},  {8, 746.0f},   {9, 1056.0f},  {10, 996.0f},  {11, 862.0f},  {12, 1559.0f}, {13, 907.0f},
    {14, 1600.0f}, {15, 889.0f},  {16, 807.0f},  {17, 1024.0f}, {18, 903.0f},  {19, 1469.0f}, {20, 1602.0f},
    {21, 4795.0f}, {22, 535.0f},  {23, 819.0f},  {24, 1469.0f}, {25, 1602.0f}, {26, 927.0f},  {27, 1171.0f},
    {28, 1056.0f}, {37, 9600.0f},
}};
// Classes with no type index: length = scale * (hull + shields)^exponent, fitted to the table's middle.
inline constexpr float kFallbackLengthScale = 100.0f;
inline constexpr float kFallbackLengthExponent = 0.35f;
inline constexpr float kMinShipRadius = 150.0f;
inline constexpr float kCraftRadius = 8.0f;
inline constexpr float kCraftMissOffset = 60.0f; // a fighter's miss passes this far beside its target craft
inline constexpr float kDeathStarRadius = 4800.0f; // when the station has no type index

// ---- archetypes (§7.2) ------------------------------------------------------------------------------------------------
inline constexpr float kBowFacingRatio = 1.25f;        // fore anti-capital firepower over the stronger broadside
inline constexpr float kPicketLaserShare = 0.6f;       // laser points share at or above which an armed ship is a picket
inline constexpr float kEscortLaserShare = 0.3f;       // ... at or above which it is an escort frigate
inline constexpr int32_t kCarrierArchetypeHangars = 4; // unarmed-but-lasers ships with this many hangars are carriers
inline constexpr float kLightMassLimit = 1300.0f;      // size tags by hull + shields
inline constexpr float kMediumMassLimit = 2600.0f;
inline constexpr float kHeavyMassLimit = 4000.0f;
inline constexpr float kDamagedHullFraction = 0.5f;
inline constexpr float kShieldsDownEventShare = 0.25f; // "shields down" is reported only after shields above this share
inline constexpr float kShieldsCollapsedShare = 0.05f; // Withdraw "when shields collapse": below this share

// ---- weapons (§6.3-6.4) -------------------------------------------------------------------------------------------------
inline constexpr float kArcHalfWidth = 50.0f * kDegrees; // 100-degree arcs, overlapping by 10 degrees at the corners
inline constexpr float kLaserVsCapital = 1.0f / 6.0f;
inline constexpr float kTurbolaserVsFighter = 0.5f;
inline constexpr float kTorpedoVsShields = 0.25f;
inline constexpr float kEvasionFloor = 0.1f;
inline constexpr std::array<float, 3> kSalvoSeconds{2.0f, 2.5f, 0.6f}; // turbolaser, ion, laser
inline constexpr std::array<float, 3> kBoltSpeed{Meters(3000.0f), Meters(2500.0f), Meters(4000.0f)};
inline constexpr float kBatteryRetargetSeconds = 0.5f;  // idle battery re-check delay at detection 0
inline constexpr float kAccuracyNear = 0.92f;           // capital-ship target at point blank
inline constexpr float kAccuracyFalloff = 0.25f;        // lost at maximum range, reduced by detection
inline constexpr float kDetectionForNoFalloff = 100.0f; // detection rating that removes the range falloff
inline constexpr float kPointDefenceDamageScale = 0.25f;
inline constexpr float kFighterTargetAccuracy = 0.75f;  // base accuracy against craft, before evasion
inline constexpr float kPrimaryTargetBonus = 3.0f;      // a battery's preference for its ship's assigned target
inline constexpr float kHullOutputFloor = 0.0f;         // output = floor + (1 - floor) * hull fraction
inline constexpr float kMaxHitChance = 0.99f;
inline constexpr float kIonValueShieldsDown = 0.5f; // what ion fire is worth to scoring once it only disrupts

// ---- shields, hull, critical systems (§6.5) ---------------------------------------------------------------------------
inline constexpr float kCritChancePerHullPoint = 0.0025f;  // chance of a critical hit per point of hull damage
inline constexpr float kShieldCritChancePerPoint = 0.0004f; // shield-recharge critical per point absorbed by shields
inline constexpr float kSubsystemHullShare = 0.7f;         // precision fire keeps this share of the hull damage
inline constexpr float kSubsystemCritMultiplier = 3.0f;
inline constexpr float kMaxCritChance = 0.9f;       // per hull hit
inline constexpr float kMaxShieldCritChance = 0.5f; // per shield hit
inline constexpr float kCritSystemLoss = 0.25f;
inline constexpr int32_t kCritSystemMax = 4;
// Relative chance of each system taking a critical hit, from the legacy severity table: shield recharge, weapon
// recharge, tractor, engines, hyperdrive.
inline constexpr std::array<float, 5> kCritWeights{15.0f, 15.0f, 10.0f, 5.0f, 5.0f};
inline constexpr float kDamageControlSeconds = 50.0f;

// ---- ion (§6.6) -----------------------------------------------------------------------------------------------------------
inline constexpr float kIonDisruptionShare = 0.02f; // ion overflow per disruption event, as a share of hull + shields
inline constexpr float kIonDisruptionMinimum = 10.0f;
inline constexpr float kIonEngineChance = 0.35f; // otherwise a weapon arc is discharged
inline constexpr float kIonEngineMinSeconds = 30.0f;
inline constexpr float kIonEngineMaxSeconds = 50.0f;
inline constexpr float kIonSystemsSeconds = 12.0f; // tractor and gravity-well projectors stay down this long
inline constexpr float kIonPoolDecay = 0.05f;      // disruption build-up lost per second, as a share of one event

// ---- movement, facing and formations (§5.2, §6.2-6.3) ------------------------------------------------------------------
inline constexpr float kShipThinkSeconds = 0.5f;
inline constexpr float kGroupThinkSeconds = 1.0f;
inline constexpr int32_t kFacingSamples = 24;
inline constexpr float kFacingMovementWeight = 0.6f;
inline constexpr float kFacingTurnCostPerSecond = 0.03f;
inline constexpr float kFacingArcMargin = 15.0f * kDegrees; // Auto facing values an arc fully this far inside its edge
inline constexpr float kGoalUrgencyDistance = Meters(2500.0f);
inline constexpr float kStationSpeedFraction = 0.1f;
inline constexpr float kRakeSpeedFraction = 0.4f;
inline constexpr float kRakeMinAlignment = -0.5f;   // a broadside ship keeps raking unless its goal is well behind
inline constexpr float kTurningSpeedFraction = 0.2f; // while turning toward a goal behind the ship
inline constexpr float kArrivalSpeedFraction = 0.5f; // attackers drop out of hyperspace moving
inline constexpr float kFacingSecondaryWeight = 0.35f; // other targets' weight against the primary in Auto facing
inline constexpr int32_t kFacingSecondaryTargets = 3;
inline constexpr float kTractorStation = 0.7f;        // a tractor order holds station at this share of tractor range
inline constexpr float kSlowingDistance = Meters(1500.0f);
inline constexpr float kFormationSpacing = Meters(500.0f);
inline constexpr float kFormationLookaheadSeconds = 4.0f;
inline constexpr float kStationKeepingGain = 0.25f; // m/s of extra speed per meter a ship lags its formation place
inline constexpr float kStationKeepingFloor = 0.5f; // a ship ahead of its place slows to at most this share
inline constexpr float kOptimalRangeShare = 0.8f;   // optimal: the longest range keeping this share of firepower
inline constexpr float kRangeMargin = 0.9f;         // stay this far inside a weapon's reach
inline constexpr float kStandOffMargin = Meters(600.0f);    // outside the enemy's reach by this much
inline constexpr float kHookAngle = 70.0f * kDegrees;
inline constexpr float kScreenDistance = Meters(1500.0f);   // screens keep station this far toward the threat
inline constexpr float kPicketStandoff = Meters(2500.0f);   // pickets and interdictors behind the line
inline constexpr float kCarrierStandoff = Meters(6000.0f);  // carriers behind the line
inline constexpr float kSeparationPadding = Meters(60.0f);  // capital ships keep at least this gap between hulls
inline constexpr float kAvoidLookaheadSeconds = 4.0f; // ships steer around hulls they would reach this soon
inline constexpr float kStoppedTurnFactor = 0.1f;   // turn rate of a ship held by tractors
inline constexpr float kMoveArrivalDistance = Meters(250.0f);
inline constexpr float kMinEngagementRange = Meters(300.0f);
inline constexpr float kStandOffScan = Meters(10000.0f);    // enemies this close to the target set the stand-off reach
inline constexpr float kScreenThreatRange = Meters(20000.0f);
inline constexpr float kWellEscapeMargin = Meters(1500.0f);  // a trapped ship runs this far past a well's edge
inline constexpr float kGarrisonApproach = Meters(3000.0f);  // an attacker facing only a docked garrison closes to this
inline constexpr float kFacingRangeSlack = Meters(1000.0f);  // Auto facing half-counts targets this far out of reach

// ---- target scoring and allocation (§5.3) -----------------------------------------------------------------------------
inline constexpr std::array<float, 4> kPriorityWeights{8.0f, 4.0f, 2.0f, 1.0f};
inline constexpr float kUnlistedTargetWeight = 0.5f;
inline constexpr float kFinishSeconds = 10.0f;      // "finish damaged targets": killable within this time
inline constexpr float kFinishBoost = 3.0f;
inline constexpr float kOverkillWindowSeconds = 20.0f; // the allocator commits enough fire to kill within this time
inline constexpr float kRangeScoreFalloff = Meters(3000.0f);   // score halves this far outside weapon range
inline constexpr float kTurnScoreSeconds = 10.0f;      // score halves when the best arc needs this long to bear
inline constexpr float kThreatScanRange = Meters(3000.0f);     // beyond a ship's reach, threats it still considers
inline constexpr float kStrongestThreatRange = Meters(12000.0f);

// ---- power routing (§5.5): weapon recharge, shield recharge, speed and turn rate -------------------------------------------
struct PowerFactors {
  float weapons;
  float shields;
  float engines;
};
inline constexpr std::array<PowerFactors, 4> kPowerRouting{{
    {1.0f, 1.0f, 1.0f}, // Balanced
    {1.3f, 0.5f, 0.9f}, // Weapons
    {0.7f, 1.8f, 0.9f}, // Shields
    {0.8f, 0.8f, 1.4f}, // Engines
}};
inline constexpr float kPowerSwitchSeconds = 5.0f;

// ---- command signals (§5.6) ----------------------------------------------------------------------------------------------
inline constexpr float kSignalLeadershipPerCharge = 30.0f; // max charges = 1 + leadership / this, while an admiral leads
inline constexpr int32_t kSignalMaxCharges = 4;
inline constexpr float kSignalRechargeSeconds = 120.0f;
inline constexpr float kConcentrateSeconds = 20.0f;
inline constexpr float kConcentrateRange = Meters(15000.0f);
inline constexpr float kConcentrateAccuracyBonus = 0.15f;
inline constexpr float kHoldTheLineSeconds = 30.0f;
inline constexpr float kScrambleSeconds = 30.0f;
inline constexpr float kScrambleRate = 3.0f;
inline constexpr float kCoordinatedJumpSeconds = 15.0f;
inline constexpr float kCoordinatedJumpSpeedup = 0.4f;

// ---- fighters and carriers (§6.7-6.8) -------------------------------------------------------------------------------------
inline constexpr float kCraftHull = 8.0f;
inline constexpr float kCraftLegacyOutput = 1.0f;          // a craft's weapon rating per legacy step (3.75 * 0.2667)
inline constexpr float kCraftShieldRechargePerSecond = 0.1f;
inline constexpr float kCraftGunSeconds = 0.5f;
inline constexpr float kCraftGunCone = 12.0f * kDegrees;   // half-angle off the nose within which a craft fires
inline constexpr float kCraftTorpedoCone = 25.0f * kDegrees;
inline constexpr float kTorpedoSeconds = 1.2f;             // between torpedoes of one craft
inline constexpr float kTorpedoDamage = 12.0f;             // hull damage per torpedo
inline constexpr int32_t kTorpedoSalvo = 2;                // torpedoes a craft fires per pass
inline constexpr float kTorpedoSpeed = Meters(900.0f);
inline constexpr float kTorpedoAccuracy = 0.9f;
inline constexpr float kCraftVsCraftAccuracy = 0.6f;
inline constexpr float kDogfightDamageScale = 0.5f;
inline constexpr float kCraftVsShipAccuracy = 0.9f;
inline constexpr float kCraftDutyCycle = 0.35f;            // share of the time a craft has a target in its sights
inline constexpr float kSquadronEngageSlack = Meters(2000.0f);     // estimates count a squadron this close as engaging
inline constexpr float kBreakOffDistance = Meters(350.0f);         // beyond the target's hull
inline constexpr float kExtendDistance = Meters(1800.0f);
inline constexpr float kExtendSeconds = 4.0f;
inline constexpr float kBreakOffAngle = 75.0f * kDegrees;
inline constexpr float kOvershootDistance = Meters(120.0f);
inline constexpr float kOvershootTurn = 40.0f * kDegrees;   // a craft that overshoots its quarry breaks this way
inline constexpr float kOvershootSeconds = 2.0f;
inline constexpr float kDogfightRetargetSeconds = 2.0f;     // a craft re-picks the nearest enemy craft this often
inline constexpr float kLoiterSpeedFraction = 0.7f;         // circling squadrons fly at this share of top speed
inline constexpr float kSquadronSpacing = Meters(60.0f);
inline constexpr float kSquadronThinkSeconds = 1.0f;
inline constexpr float kFighterTargetDistance = Meters(8000.0f); // fighter target scores halve at this distance
inline constexpr float kFighterScreenAhead = Meters(2000.0f);    // idle fighters hold this far toward the enemy
inline constexpr float kLoiterOrbit = Meters(700.0f);
inline constexpr float kDogfightEngageRange = Meters(2500.0f);     // Dogfight: Engage reacts to enemy fighters this close
inline constexpr float kEscortEngageRange = Meters(3000.0f);       // escorts engage fighters this close to the escorted unit
inline constexpr float kEscortStation = Meters(600.0f);
inline constexpr float kInterceptZone = Meters(9000.0f);           // intercept missions hunt within this radius of the fleet
inline constexpr float kPatrolDefaultRadius = Meters(5000.0f);
inline constexpr float kAttackedMemorySeconds = 4.0f;      // AvoidUnlessAttacked fights back this long after a hit
inline constexpr float kLaunchBaseSeconds = 5.0f;          // the original's launch interval
inline constexpr int32_t kLaunchRandomSeconds = 7;         // plus 0-7 s
inline constexpr float kLaunchOffset = Meters(300.0f);             // craft appear this far beyond the carrier's hull
inline constexpr float kDockDistance = Meters(250.0f);
inline constexpr float kRearmSeconds = 20.0f;
inline constexpr float kCommitRange = Meters(12000.0f);            // "when enemy fighters commit": within this of our fleet

// ---- tractors, wells, withdrawal (§6.9-6.10) -----------------------------------------------------------------------------
inline constexpr float kTractorBreakFactor = 1.1f; // locks break beyond this share of the tractor range
inline constexpr float kGravityWellRadius = Meters(15000.0f);
inline constexpr float kSpoolBaseSeconds = 30.0f;
inline constexpr float kSpoolSecondsPerRetreatDay = 0.4f; // the original passed retreat days x 0.4 as seconds
inline constexpr float kSpoolMinSeconds = 8.0f;
inline constexpr float kSpoolMaxSeconds = 60.0f;
inline constexpr float kHyperdriveReferenceRating = 100.0f;
inline constexpr float kDeathStarSpoolFactor = 3.0f;
inline constexpr float kDeathStarOwnSpoolSeconds = 120.0f;
inline constexpr float kDeathStarStandoff = Meters(2500.0f); // behind its side's back row

// ---- planetary batteries (§6.13) -------------------------------------------------------------------------------------------
// A side's batteries fire from the planet's surface at enemy capital ships within this distance of it: the cover spans
// the defender's deployment zone and the approach to it, out to about the battle's centre.
inline constexpr float kPlanetaryCoverReach = Meters(10000.0f);
// The records' attack strengths (LNR Series I 800, KDY-150 2,000, LNR Series II 5,000) are on the bombardment scale,
// where one hit cripples a ship. In space a battery's damage per second grows with their square root, so the LNR
// Series II is 2.5 times the LNR Series I rather than 6: dps = scale x sqrt(attack strength).
inline constexpr float kPlanetaryDpsPerRootAttack = 0.35f;
inline constexpr float kPlanetarySalvoSeconds = 3.0f; // fewer, heavier bolts than a ship's battery
inline constexpr float kPlanetaryDetection = 50.0f;   // accuracy falls with range as for a ship of this detection
// Emplacements stand on the limb facing the battle, spread this far either side of the direction to its centre.
inline constexpr float kPlanetarySiteSpread = 55.0f * kDegrees;
// Under the guns: a group stays this far inside its cover's edge.
inline constexpr float kCoverMargin = Meters(800.0f);
// The defender's battle AI leaves its cover to fight once it is this much stronger, batteries included.
inline constexpr std::array<float, 3> kSallyStrengthRatio{1.25f, 1.5f, 2.0f}; // aggressive, balanced, cautious

// ---- Death Star (§6.12) ------------------------------------------------------------------------------------------------------
inline constexpr float kSuperlaserRechargeSeconds = 300.0f;
inline constexpr float kSuperlaserDelaySeconds = 1.0f;
inline constexpr float kDeathStarFirepower = 150.0f; // strength estimate only: "large but finite" (§8.4 #3)
inline constexpr float kTrenchRunSeconds = 20.0f;
inline constexpr float kTrenchEntryDistance = Meters(1500.0f); // beyond the station's radius
inline constexpr float kTrenchDepth = Meters(100.0f);          // craft fly this far above the surface
inline constexpr float kTrenchTorpedoFactor = 0.5333f;         // the legacy torpedo strength per torpedo
inline constexpr float kTrenchShotScale = 1.0f;                // per-craft success chance scale on the legacy formula
inline constexpr float kTrenchCombatPerSkill = 20.0f;          // the original's commander skill: combat / 20
// A craft survives the run with chance (turn points + commander skill) / 9: the original rolled 1-9 against turn rate.
inline constexpr float kTrenchFullSurvivalPoints = 9.0f;
inline constexpr float kTrenchMinSurvival = 0.05f;

// ---- ending (§6.11) ----------------------------------------------------------------------------------------------------------
inline constexpr float kStalemateSeconds = 120.0f;
inline constexpr float kPressAttackSeconds = 40.0f; // a stronger attacker commits everything after this long a lull
inline constexpr float kStalemateProgressShare = 0.01f;

// ---- deployment (§4) ----------------------------------------------------------------------------------------------------------
inline constexpr float kDefaultAttackerBearing = 0.0f; // the attacker arrives from +x
inline constexpr float kAttackerDeployDistance = Meters(9000.0f);
inline constexpr float kDefenderDeployDistance = Meters(6000.0f);
inline constexpr float kPlanetDistance = Meters(16000.0f);
inline constexpr float kPlanetRadius = Meters(6000.0f);
inline constexpr float kBattleHalfExtent = kBattlePlaneHalfExtent;
inline constexpr float kDeploymentRowSpacing = Meters(2400.0f);
inline constexpr float kDeployZoneRadius = Meters(7000.0f); // minimum briefing radius; crowded fleets grow rearward after wrapping
inline constexpr float kMaxDeployZoneGrowth = 8.0f * kBattleHalfExtent; // the zone covers the whole plane long before this
inline constexpr int32_t kMaxTaskForceShips = 6;
inline constexpr int32_t kMaxFlightSquadrons = 4;

// ---- view model ---------------------------------------------------------------------------------------------------------------
inline constexpr int32_t kEventLogSize = 64;
inline constexpr float kEffectSeconds = 2.0f;

} // namespace reb::battle::tuning
