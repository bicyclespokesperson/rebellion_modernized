#include "app/battle_flow.h"

#include "character/character.h"
#include "fleet/capship.h"
#include "fleet/capship_subclasses.h"
#include "fleet/fight_squad.h"
#include "fleet/fleet_iterator.h"
#include "galaxy/system.h"
#include "gameobj/object_queries.h"
#include "tactical/tactical_facade.h"
#include "tactical/tactical_game.h"
#include "tactical/tactical_types.h"

// The tactical -> strategic hand-back (0x40c730-0x40d660): BattleResults reads the tactical engine's results through the
// facade and applies them to the strategic objects the hand-over registered (battle_handoff.cpp).

NotifyContext* __thiscall FUN_00516ee0(GameObjNotifier* gameRoot);  // the game root's notify context (+0x90)
GameObjNotifier* GetMasterInFamilies_14_1c(const ObjectKey* key);   // 0x504f70
uint32_t __thiscall SystemBattleControlKind(System* system);        // 0x40d750
int __thiscall CapShipCurrentHull(CapShip* ship);                   // 0x40d710
int __thiscall FightSquadCurrentSize(FightSquad* squad);            // 0x40d730

namespace {

constexpr uint32_t kBothSidesFilter = 3;
constexpr uint32_t kUsableFilter = 1;
constexpr int kDestroyedByBattle = 0xc;
constexpr uint32_t kSideControlMask = 0xc0;
constexpr uint32_t kBothSidesControl = 3;
constexpr uint32_t kExistingFlag = 0x40;

// The side that holds the system when both sides still had units: the system's battle control field says who was there
// first (3 = both, 1 = Alliance, otherwise the Empire).
int contestedWinner(const System* system) {
    const uint32_t control = (system->m_controlBits >> 6) & 3;
    if (control == kBothSidesControl) return 3;
    return 2 - (control != 1);
}

int winnerFromSurvivors(const System* system, int sideOne, int sideTwo) {
    int winner = 0;
    if (sideOne != 0 && sideTwo != 0) {
        winner = contestedWinner(system);
    } else if (sideOne != 0) {
        winner = 1;
    } else if (sideTwo != 0) {
        winner = 2;
    }
    return winner;
}

}  // namespace

BattleResults::BattleResults() {
    m_driver = nullptr;
    m_system = nullptr;
    m_abstract = 0;
}

BattleResults::~BattleResults() {}

unsigned BattleResults::Apply(int* inconclusive) {
    *inconclusive = 0;
    m_system = nullptr;
    unsigned result;
    {
        ObjectKey rootKey;
        m_driver->GetRootKey(&rootKey);
        m_system = static_cast<System*>(GetMasterInFamilies_90_98(&rootKey));
        result = m_system != nullptr;
    }
    if (m_driver != nullptr && m_system != nullptr) {
        NotifyContext context;
        int winner = 0;
        BattleOutcome outcome;
        TacticalGetBattleOutcome(&outcome, 0);

        winner = winnerFromSurvivors(m_system, outcome.m_sideAAlive, outcome.m_sideBAlive);
        if (m_abstract == 0 && outcome.m_unfinished != 0 && winner == 0) *inconclusive = 1;
        if (*inconclusive == 0) {
            if (winner == 0) winner = winnerFromSurvivors(m_system, outcome.m_sideAFinal, outcome.m_sideBFinal);
            m_system->SetDataFlag4(1);
            m_system->SetData64(winner);
        }

        GameObjNotifier* root = GetGameRootObjectCached();
        const unsigned rootFound = root != nullptr;
        if (rootFound) context = *FUN_00516ee0(root);
        result = rootFound ? 1 : 0;

        {
            FleetIterator fleets(m_system, kBothSidesFilter);
            for (fleets.Begin(); fleets.Current(); fleets.Advance()) {
                CapShipIterator carriers(fleets.Current(), kUsableFilter);
                for (carriers.Begin(); carriers.Current(); carriers.Advance()) {
                    GameObjNotifier* carrier = carriers.Current();
                    {
                        FightSquadIterator squads(carrier, kUsableFilter);
                        for (squads.Begin(); squads.Current();) {
                            FightSquad* squad = static_cast<FightSquad*>(squads.Current());
                            result = ApplyFightSquad(squad, &context);
                            squads.Advance();
                        }
                    }
                    {
                        CharacterIterator characters(carrier, kUsableFilter);
                        for (characters.Begin(); characters.Current();) {
                            GameObj* character = static_cast<GameObj*>(characters.Current());
                            int handle = 0;
                            bool ok = m_driver != nullptr && m_system != nullptr;
                            if (ok) ok = character->GetDataField04(&handle);
                            if (ok) {
                                CharacterResult unused;
                                TacticalMakeCharacterResult(&unused, handle);
                            }
                            result = ok;
                            characters.Advance();
                        }
                    }
                }
            }
        }
        {
            FightSquadIterator squads(m_system, kUsableFilter);
            for (squads.Begin(); squads.Current();) {
                FightSquad* squad = static_cast<FightSquad*>(squads.Current());
                result = ApplyFightSquad(squad, &context);
                squads.Advance();
            }
        }

        {
            FleetIterator fleets(m_system, kBothSidesFilter);
            for (fleets.Begin(); fleets.Current(); fleets.Advance()) {
                CapShipIterator carriers(fleets.Current(), kUsableFilter);
                for (carriers.Begin(); carriers.Current(); carriers.Advance()) {
                    GameObjNotifier* carrier = carriers.Current();
                    {
                        FightSquadIterator squads(carrier, kUsableFilter);
                        for (squads.Begin(); squads.Current();) {
                            FightSquad* squad = static_cast<FightSquad*>(squads.Current());
                            squads.Advance();
                            result = ApplyCarrierChange(squad, &context);
                        }
                    }
                    {
                        CharacterIterator characters(carrier, kUsableFilter);
                        for (characters.Begin(); characters.Current();) {
                            GameObj* character = static_cast<GameObj*>(characters.Current());
                            characters.Advance();
                            int handle;
                            result = character->GetDataField04(&handle);
                            if (result) {
                                CharacterResult unused;
                                TacticalMakeCharacterResult(&unused, handle);
                            }
                        }
                    }
                }
            }
        }
        {
            FightSquadIterator squads(m_system, kUsableFilter);
            for (squads.Begin(); squads.Current();) {
                FightSquad* squad = static_cast<FightSquad*>(squads.Current());
                squads.Advance();
                result = ApplyCarrierChange(squad, &context);
            }
        }

        {
            FleetIterator fleets(m_system, kBothSidesFilter);
            for (fleets.Begin(); fleets.Current(); fleets.Advance()) {
                GameObjNotifier* fleet = fleets.Current();
                {
                    CapitalShipIterator ships(fleet, kUsableFilter);
                    for (ships.Begin(); ships.Current();) {
                        CapShip* ship = static_cast<CapShip*>(ships.Current());
                        result = ApplyCapitalShip(ship, &context);
                        ships.Advance();
                    }
                }
                {
                    DeathStarIterator deathStars(fleet, kUsableFilter);
                    for (deathStars.Begin(); deathStars.Current();) {
                        CapShip* deathStar = static_cast<CapShip*>(deathStars.Current());
                        result = ApplyDeathStar(deathStar, &context);
                        deathStars.Advance();
                    }
                }
                {
                    CapShipIterator ships(fleet, kUsableFilter);
                    for (ships.Begin(); ships.Current();) {
                        CapShip* ship = static_cast<CapShip*>(ships.Current());
                        result = ApplyHangarOverflow(ship, &context);
                        ships.Advance();
                    }
                }
            }
        }
    }
    DestroyTacticalGame();
    return result;
}

int BattleResults::ApplyCapitalShip(CapShip* ship, NotifyContext* context) {
    int handle = 0;
    CapitalShipResult result;
    bool ok = m_driver != nullptr && m_system != nullptr;
    if (ok) ok = ship->GetDataField04(&handle);
    if (ok) TacticalGetCapitalShipResult(&result, handle);
    int primaryDamage = 0;
    int backupDamage = 0;
    if (ok) {
        int remaining = result.m_hyperdriveDamage;
        if (remaining != 0) {
            if (ship->HasPrimaryHyperdrive()) {
                primaryDamage = remaining > ship->HasPrimaryHyperdrive() ? ship->HasPrimaryHyperdrive() : remaining;
                remaining -= primaryDamage;
            }
            if (remaining != 0 && ship->HasBackupHyperdrive()) {
                backupDamage = remaining > ship->HasBackupHyperdrive() ? ship->HasBackupHyperdrive() : remaining;
                remaining -= backupDamage;
            }
        }
        ok = remaining == 0;
    }
    if (ok) {
        const int currentHull = CapShipCurrentHull(ship);
        const int difference = static_cast<int>(result.m_hull) - currentHull;
        if (difference > 0) {
            ok = ship->RepairHullDamage(difference, context);
        } else if (difference < 0) {
            ok = ship->DamageHull(-difference, kDestroyedByBattle, context);
        }
    }
    int applied = 0;
    if (ok && ship->SetShieldRechargeRateCH(result.m_shieldRechargeCH, context) &&
        ship->SetWeaponRechargeRateCH(result.m_weaponRechargeCH, context) &&
        ship->SetTractorBeamPowerCH(result.m_tractorBeamCH, context) &&
        ship->SetSpeedCH(result.m_speedCH, context) && ship->SetPrimaryHyperdriveCH(primaryDamage, context)) {
        applied = ship->SetBackupHyperdriveCH(backupDamage, context);
    }
    return applied;
}

int BattleResults::ApplyDeathStar(CapShip* deathStar, NotifyContext* context) {
    int handle = 0;
    DeathStarResult result;
    bool ok = deathStar->GetDataField04(&handle);
    if (ok) {
        TacticalGetDeathStarResult(&result);
        ok = m_driver != nullptr && m_system != nullptr;
    }
    if (ok) {
        const int currentHull = CapShipCurrentHull(deathStar);
        const int difference = static_cast<int>(result.m_hull) - currentHull;
        if (difference > 0) {
            ok = deathStar->RepairHullDamage(difference, context);
        } else if (difference < 0) {
            ok = deathStar->DamageHull(-difference, kDestroyedByBattle, context);
        }
    }
    if (ok && result.m_alive == 0 && (deathStar->m_stateFlags & kExistingFlag)) {
        ok = deathStar->ChangeDestroyedReason(kDestroyedByBattle, context);
    }
    return ok;
}

int BattleResults::ApplyFightSquad(FightSquad* squad, NotifyContext* context) {
    int handle = 0;
    int applied = 0;
    bool ok = m_driver != nullptr && m_system != nullptr;
    if (ok) ok = squad->GetDataField04(&handle);
    if (ok) {
        FightSquadResult result;
        TacticalGetFightSquadResult(&result, handle);
        applied = ok;
        const int difference = result.m_squadSize - FightSquadCurrentSize(squad);
        if (difference > 0) return squad->RepairSquadSizeDamage(difference, context);
        if (difference < 0) applied = squad->InflictSquadSizeDamage(-difference, kDestroyedByBattle, context);
    }
    return applied;
}

int BattleResults::ApplyCarrierChange(FightSquad* squad, NotifyContext* context) {
    int handle = 0;
    int applied = 0;
    bool ok = m_driver != nullptr && m_system != nullptr;
    if (ok) ok = squad->GetDataField04(&handle);
    if (ok) {
        FightSquadResult squadResult;
        TacticalGetFightSquadResult(&squadResult, handle);
        ObjectKey oldCarrier;
        ObjectKey newCarrier;
        applied = squad->GetValidParentKey(&oldCarrier);
        if (applied != 0) {
            if (squadResult.m_unk14 != 0) {
                CapitalShipResult carrierResult;
                TacticalGetCapitalShipResult(&carrierResult, squadResult.m_unk10);
                ObjectKey candidate = ObjectKey::Make(carrierResult.m_unk08, carrierResult.m_unk0c);
                GameObj* carrier = static_cast<GameObj*>(GetMasterInFamilies_14_1c(&candidate));
                if (carrier && ((carrier->m_flags ^ squad->m_flags) & kSideControlMask) == 0) newCarrier = candidate;
            }
            if (!newCarrier.IsSet()) {
                ObjectKey systemKey;
                m_system->GetKey(&systemKey);
                newCarrier = systemKey;
            }
            if (applied != 0 && oldCarrier.m_value != newCarrier.m_value) applied = squad->Reparent(&newCarrier, context);
        }
    }
    return applied;
}

int BattleResults::ApplyHangarOverflow(CapShip* ship, NotifyContext* context) {
    int usableCount;
    int existingCount;
    {
        FightSquadIterator existing(ship, kBothSidesFilter);
        existingCount = existing.Count();
    }
    {
        FightSquadIterator usable(ship, kUsableFilter);
        usableCount = usable.Count();
    }
    int overflow = existingCount - ship->GetFighterCapacity();
    if (overflow > 0) {
        int toMove = 0;
        if (overflow <= usableCount) {
            toMove = overflow;
        } else if (usableCount != 0) {
            toMove = usableCount;
        }
        FightSquadIterator squads(ship, kUsableFilter);
        squads.Begin();
        for (; squads.Current() && toMove != 0; --toMove) {
            FightSquad* squad = static_cast<FightSquad*>(squads.Current());
            squads.Advance();
            ObjectKey systemKey;
            m_system->GetKey(&systemKey);
            squad->Reparent(&systemKey, context);
        }
    }
    return 1;
}
