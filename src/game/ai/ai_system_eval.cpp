#include <algorithm>

#include "ai/ai_character_records.h"
#include "ai/ai_class_records.h"
#include "ai/ai_common.h"
#include "ai/ai_members.h"
#include "ai/ai_records.h"
#include "ai/ai_side_controller.h"
#include "character/role.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "economy/defense_facility.h"
#include "economy/manufacturing_facility.h"
#include "economy/production_facility_types.h"
#include "events/game_clock_hooks.h"
#include "fleet/fight_squad.h"
#include "fleet/fleet_iterator.h"
#include "fleet/troop_reg.h"
#include "galaxy/system.h"
#include "gameobj/child_iterator.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"

namespace {

ObjectKey KeyOfObject(GameObjNotifier* object) {
    ObjectKey key;
    GetObjectKey(object, &key);
    return key;
}

uint32_t StateOf(GameObjNotifier* object) { return static_cast<GameObj*>(object)->m_stateFlags; }

constexpr uint32_t kSystemPopulated = 1;
constexpr uint32_t kSystemExplored = 2;
constexpr uint32_t kSystemUprising = 4;

}  // namespace

AiSystemEval::AiSystemEval() : ObjectKeyNode(), m_flags28(0), m_flags2c(0), m_flags30(0) {
    m_view = 0;
    m_x20 = 0;
    m_dirty = 0;
    m_touchedDay = 0;
    m_combatDay = 0;
    m_sector = nullptr;
    m_classes = nullptr;
    m_position = 0;
    for (int& value : m_stat) value = 0;
}

AiSystemEval::AiSystemEval(int view, System* system)
    : ObjectKeyNode(KeyOfObject(system)), m_flags28(0), m_flags2c(0), m_flags30(0) {
    m_view = view;
    m_x20 = 0;
    m_dirty = 0;
    m_touchedDay = 0;
    m_combatDay = 0;
    uint32_t position;
    system->GetRecordField4c(&position);
    m_position = static_cast<int>(position);
    m_sector = nullptr;
    m_classes = nullptr;
    for (int& value : m_stat) value = 0;
}

AiSystemEval::~AiSystemEval() {
    m_sector = nullptr;
    m_classes = nullptr;
    REB_KEEP_VPTR_STORE();
}

void AiSystemEval::Load(istream* in) {
    ObjectKeyNode::Load(in);
    ReadValue32(in, &m_view);
    ReadValue32(in, &m_x20);
    ReadValue32(in, &m_dirty);
    ReadValue32(in, &m_touchedDay);
    ReadValue32(in, &m_combatDay);
    ReadValue16(in, &m_position);
    ReadValue16(in, reinterpret_cast<uint8_t*>(&m_position) + 2);
    m_flags28.Read(in);
    m_flags30.Read(in);
    m_flags2c.Read(in);
    for (int& value : m_stat) ReadValue32(in, &value);
    m_dirty = 1;
}

void AiSystemEval::Save(ostream* out) {
    ObjectKeyNode::Save(out);
    WriteValue32(out, &m_view);
    WriteValue32(out, &m_x20);
    WriteValue32(out, &m_dirty);
    WriteValue32(out, &m_touchedDay);
    WriteValue32(out, &m_combatDay);
    WriteValue16(out, &m_position);
    WriteValue16(out, reinterpret_cast<uint8_t*>(&m_position) + 2);
    m_flags28.Write(out);
    m_flags30.Write(out);
    m_flags2c.Write(out);
    for (int& value : m_stat) WriteValue32(out, &value);
}

void AiSystemEval::NoteTouched() {
    m_touchedDay = GameHookDay();
    MarkChanged();
}

int AiSystemEval::DaysSinceTouched() {
    if (m_flags30.m_value & 1) return 0;
    return GameHookDay() - m_touchedDay;
}

void AiSystemEval::MarkDirty() { m_dirty = 1; }

void AiSystemEval::NoteCombat() {
    m_combatDay = GameHookDay();
    MarkChanged();
}

void AiSystemEval::MarkChanged() {
    m_dirty = 1;
    m_sector->MarkDirty();
}

void AiSystemEval::Refresh() {
    if (m_dirty == 0) return;
    m_dirty = 0;
    uint32_t& f28 = reinterpret_cast<uint32_t&>(m_flags28.m_value);
    uint32_t& f2c = reinterpret_cast<uint32_t&>(m_flags2c.m_value);
    uint32_t& f30 = reinterpret_cast<uint32_t&>(m_flags30.m_value);
    AiSideController* owner = m_sector->m_owner;
    ObjectKey ownKey;
    GetKey(&ownKey);
    System* system = static_cast<System*>(GetObjectInFamilies_90_98(m_view, &ownKey));
    for (int& value : m_stat) value = 0;
    f28 &= 0x3e00000;
    f30 &= 0x35ff0000;
    f2c &= 0x60000000;
    uint32_t viewSide = 0;
    if (m_view == 1)
        viewSide = 1;
    else if (m_view == 2)
        viewSide = 2;
    const uint32_t otherSide = m_view == 1 ? 2 : 1;
    m_x20 = static_cast<int>(system->ControlSide());
    if (system->m_stateFlags & 8) {
        f30 |= 0x10;
        f28 = 0;
    }
    if (ai::KeyInFamilies(m_key, 0x92, 0x94)) f30 |= 0x40;
    if (static_cast<uint32_t>(m_x20) == viewSide) {
        f30 |= 1;
        f30 |= 0x20;
    } else if (static_cast<uint32_t>(m_x20) == otherSide) {
        f30 |= 4;
        f30 |= 0x20;
    } else {
        f30 |= 2;
    }
    if ((system->m_flags & kSystemExplored) == 0) {
        f2c |= 0x80;
        return;
    }
    f2c |= 0x8000000;
    Stat(0x4c) = system->m_energy;
    Stat(0x84) = system->m_energy - system->m_energyAllocated;
    Stat(0x80) = system->m_rawMaterial - system->m_rawMaterialAllocated;
    if ((system->m_flags & 0x20) != 0) {
        f28 |= 2;
        if ((f30 & 6) != 0) m_touchedDay = GameHookDay();
    }
    if ((m_key.m_value & 0xffffff) == 0x109) {
        f28 |= 0x80000000;
        f30 |= 0x40000000;
    }
    if ((m_key.m_value & 0xffffff) == 0x121) f30 |= 0x8000000;
    if (CountAllFacilities(system, m_x20, 3) != 0) {
        f28 |= 0x80000000;
        f30 |= 0x80000000;
        f2c |= 0x2000000;
    }
    if (f30 & 1) Stat(0x48) = 100;

    auto classOf = [this](GameObjNotifier* object) -> AiClassRecordBase* {
        ObjectKey recordKey;
        GetObjectRecordKey(object, &recordKey);
        return m_classes->Find(&recordKey);
    };

    {
        TroopRegIterator troops(system, 3);
        troops.Begin();
        while (troops.m_current) {
            AiTroopClass* troopClass = static_cast<AiTroopClass*>(classOf(troops.m_current));
            if (f30 & 1) {
                if ((f28 & 0x83e00000) == 0) {
                    if ((troopClass->m_flags.m_value & 0x1000) != 0) {
                        f30 |= 0x4000;
                        if ((troopClass->m_flags.m_value & 0x2000) != 0) f30 |= 0x8000;
                    } else {
                        ++Stat(0x8c);
                        f28 |= 0x20000000;
                    }
                } else if (troopClass->m_defenseStrength < m_classes->m_bestDefense) {
                    if ((troopClass->m_flags.m_value & 0x1000) != 0) f30 |= 0x4000;
                    if ((troopClass->m_flags.m_value & 0x2000) != 0) f30 |= 0x8000;
                } else {
                    ++Stat(0x8c);
                    if ((troopClass->m_flags.m_value & 0x1000) != 0) f28 |= 0x10000000;
                    if ((troopClass->m_flags.m_value & 0x2000) != 0) f28 |= 0x20000000;
                }
            }
            ++Stat(0x88);
            Stat(0xc0) += troopClass->m_valueB;
            if (StateOf(troops.m_current) & 1) ++Stat(0x90);
            troops.Advance();
        }
    }

    {
        FleetIterator fleets(system, 3);
        fleets.Begin();
        while (fleets.m_current) {
            AiFleetEval* fleet = owner->m_fleets.FindEval(KeyOfObject(fleets.m_current));
            if (fleet) {
                const uint32_t fleetFlags = fleet->m_flags.m_value;
                if ((fleetFlags & 1) == 0 && (fleet->m_flags.m_value & 0x4000) != 0) {
                    Stat(0xe4) += fleet->Stat(0xb4);
                    Stat(0xe8) += fleet->Stat(0xb8);
                    Stat(0xec) += fleet->Stat(0x60);
                    Stat(0xf4) += fleet->Stat(0x70);
                    Stat(0xbc) += fleet->Stat(0xa4);
                    f30 |= 0x200;
                    f2c |= 0x800000;
                    if ((fleetFlags & 0x10) != 0) {
                        f30 |= 0x400;
                        if (m_view == 1) f2c |= 0x1000000;
                    }
                }
                if (fleetFlags & 0x200000) f2c |= 0x2800;
                if (fleetFlags & 0x400000) f2c |= 0x800;
                if (fleetFlags & 0x2000000) f30 |= 0x1000;
                if (fleetFlags & 0x1000000) f2c |= 0x8000;
            }
            fleets.Advance();
        }
    }

    {
        RoleIterator roles(system, 3);
        roles.Begin();
        while (roles.m_current) {
            ObjectKey roleKey = KeyOfObject(roles.m_current);
            AiCharacterRecord* record = static_cast<AiCharacterRecord*>(owner->m_characters.Find(&roleKey));
            if (record) {
                const uint32_t cf = record->m_flags.m_value;
                if ((cf & 4) == 0) {
                    if ((cf & 0x20000000) != 0) {
                        if ((cf & 0x40) != 0) {
                            f2c |= 0x4000;
                            ++Stat(0x100);
                        } else if ((cf & 8) == 0) {
                            ++Stat(0xfc);
                        } else {
                            ++Stat(0xf8);
                        }
                    }
                } else if ((cf & 0x20000000) != 0) {
                    if ((cf & 0x40) == 0) {
                        if ((cf & 0x20) == 0) f2c |= 0x400;
                        if ((cf & 0x7000000) != 0) {
                            f30 |= 0x1000;
                            if ((cf & 0x2000000) != 0) f30 |= 0x2000;
                        }
                        if ((cf & 8) == 0) {
                            ++Stat(0x108);
                        } else {
                            f2c |= 0x1000;
                            ++Stat(0x104);
                            if ((cf & 0x40000000) != 0) ++Stat(0x110);
                        }
                    } else {
                        f2c |= 0x4000000;
                        ++Stat(0x100);
                    }
                }
            }
            roles.Advance();
        }
    }

    if ((f30 & 1) != 0 && 0 < Stat(0x84)) {
        f28 |= 0x20;
        if (0 < Stat(0x80)) f28 |= 0x40;
    }
    Stat(0x50) = CountProductionFacilityType2C(system, m_x20, 3);
    Stat(0x54) = CountProductionFacilityType2D(system, m_x20, 3);

    uint32_t usable28 = 0;
    {
        ManufacturingFacility28Iterator facilities(system, 3);
        facilities.Begin();
        while (facilities.m_current) {
            AiClassRecordBase* facilityClass = classOf(facilities.m_current);
            usable28 |= StateOf(facilities.m_current) & 1;
            ++Stat(0x58);
            Stat(0xcc) += facilityClass->m_valueB;
            Stat(0xd0) += facilityClass->m_valueB;
            facilities.Advance();
        }
    }
    if (usable28 != 0) {
        f28 |= 0x1000;
        if (0 < Stat(0x58) && (f28 & 3) == 0) {
            GameObjNotifier* manager = system->FindManufacturingManagerBySlot123(0);
            if (manager && *reinterpret_cast<const int*>(reinterpret_cast<const uint8_t*>(manager) + 0x58) == 0)
                f28 |= 0x200;
        }
    }

    uint32_t usable29 = 0;
    {
        ManufacturingFacility29Iterator facilities(system, 3);
        facilities.Begin();
        while (facilities.m_current) {
            AiClassRecordBase* facilityClass = classOf(facilities.m_current);
            usable29 |= StateOf(facilities.m_current) & 1;
            ++Stat(0x5c);
            Stat(0xcc) += facilityClass->m_valueB;
            Stat(0xd4) += facilityClass->m_valueB;
            facilities.Advance();
        }
    }
    if (usable29 != 0) {
        f28 |= 0x1000;
        if (0 < Stat(0x5c) && (f28 & 3) == 0) {
            GameObjNotifier* manager = system->FindManufacturingManagerBySlot123(2);
            if (manager && *reinterpret_cast<const int*>(reinterpret_cast<const uint8_t*>(manager) + 0x58) == 0)
                f28 |= 0x400;
        }
    }

    uint32_t usable2a = 0;
    {
        ManufacturingFacility2AIterator facilities(system, 3);
        facilities.Begin();
        while (facilities.m_current) {
            AiClassRecordBase* facilityClass = classOf(facilities.m_current);
            usable2a |= StateOf(facilities.m_current) & 1;
            ++Stat(0x60);
            Stat(0xcc) += facilityClass->m_valueB;
            Stat(0xd8) += facilityClass->m_valueB;
            facilities.Advance();
        }
    }
    if (usable2a != 0) {
        f28 |= 0x1000;
        if (0 < Stat(0x60) && (f28 & 3) == 0) {
            GameObjNotifier* manager = system->FindManufacturingManagerBySlot123(1);
            if (manager && *reinterpret_cast<const int*>(reinterpret_cast<const uint8_t*>(manager) + 0x58) == 0)
                f28 |= 0x800;
        }
    }

    {
        DefenseFacility24Iterator defenses(system, 3);
        defenses.Begin();
        while (defenses.m_current) {
            AiDefenseClass* defenseClass = static_cast<AiDefenseClass*>(classOf(defenses.m_current));
            ++Stat(0x64);
            Stat(0xc4) += defenseClass->m_valueB;
            if (StateOf(defenses.m_current) & 1) Stat(0x68) += defenseClass->m_shieldStrength;
            defenses.Advance();
        }
    }
    if (1 < Stat(0x64)) f30 |= 0x100;

    {
        DefenseFacility25Iterator defenses(system, 3);
        defenses.Begin();
        while (defenses.m_current) {
            AiClassRecordBase* defenseClass = classOf(defenses.m_current);
            ++Stat(0x70);
            Stat(0xc4) += defenseClass->m_valueB;
            if (StateOf(defenses.m_current) & 1) f30 |= 0x80;
            defenses.Advance();
        }
    }

    {
        DefenseFacility23Iterator defenses(system, 3);
        defenses.Begin();
        while (defenses.m_current) {
            AiDefenseClass* defenseClass = static_cast<AiDefenseClass*>(classOf(defenses.m_current));
            ++Stat(0x74);
            Stat(0xc4) += defenseClass->m_valueB;
            if (StateOf(defenses.m_current) & 1) Stat(0x78) += defenseClass->m_attackStrength;
            defenses.Advance();
        }
    }

    {
        DefenseFacility22Iterator defenses(system, 3);
        defenses.Begin();
        while (defenses.m_current) {
            AiDefenseClass* defenseClass = static_cast<AiDefenseClass*>(classOf(defenses.m_current));
            ++Stat(0x74);
            Stat(0xc4) += defenseClass->m_valueB;
            if (StateOf(defenses.m_current) & 1) Stat(0x78) += defenseClass->m_attackStrength;
            defenses.Advance();
        }
    }
    if ((system->m_flags & kSystemPopulated) == 0) {
        if ((f28 & 0x20) != 0) {
            f28 |= 0x100;
            if ((f28 & 0x40) != 0) f28 |= 0x80;
        }
    } else {
        if (system->m_flags & kSystemUprising) f28 |= 1;
        f30 |= 8;
        f30 |= 0x20;
        Stat(0x48) = system->GetLoyaltyFor(m_view);
        if (Stat(0x48) < 0x46) f2c |= 0x10000000;
        FightSquadIterator squads(system, 3);
        squads.Begin();
        while (squads.m_current) {
            AiFighterClass* squadClass = static_cast<AiFighterClass*>(classOf(squads.m_current));
            FightSquad* squad = static_cast<FightSquad*>(squads.m_current);
            ++Stat(0xa4);
            Stat(0xc8) += squadClass->m_valueB;
            if ((f30 & 1) != 0 && ((f28 & 0x83e00000) == 0 || m_classes->m_x28 <= squadClass->m_x34)) ++Stat(0xa8);
            if (StateOf(squads.m_current) & 1) {
                auto liveSize = [squad] {
                    const int damage = squad->GetSquadSizeDamage();
                    return static_cast<int>(squad->GetSquadSize()) - damage;
                };
                Stat(0xb0) += squadClass->ScaleX30(liveSize());
                Stat(0xb4) += squadClass->ScaleX34(liveSize());
                Stat(0xb8) += squadClass->ScaleX38(liveSize());
                ++Stat(0xac);
                if (f30 & 1) f28 |= 0x80000;
            }
            squads.Advance();
        }
        if ((f30 & 1) != 0 && (f28 & 3) == 0 && (f28 & 0x3e00000) == 0) {
            f28 |= 0x4000000;
            int built = Stat(0x74) + Stat(0x70) + Stat(0x54) + Stat(0x50) + Stat(0x64);
            if (f30 & 0x80000000) ++built;
            if (built < Stat(0x4c)) {
                const uint32_t before = f28;
                f28 = before | 0x100;
                if (0 < Stat(0x80)) f28 = before | 0x180;
            }
        }
        const uint32_t control = f30;
        if ((control & 1) == 0) {
            if ((control & 4) == 0) {
                if ((control & 2) != 0) {
                    const int unrest = 0x5a - Stat(0x48);
                    if (0 < unrest) {
                        Stat(0x98) += (99 - Stat(0x48)) / 10;
                        if (6 < Stat(0x98)) Stat(0x98) = 6;
                    }
                    Stat(0x118) = Stat(0x74) + Stat(0x98);
                    if ((control & 0x200) == 0) f2c |= 0x40;
                    if (f28 & 0x80000000) ++Stat(0x110);
                }
            } else {
                const int unrest = -Stat(0x48);
                if (0 < unrest + 0x50) {
                    Stat(0x98) += (unrest + 0x59) / 10;
                    if (6 < Stat(0x98)) Stat(0x98) = 6;
                }
                Stat(0x118) = Stat(0x88) + Stat(0x98) + Stat(0x74);
                if (0x32 < Stat(0x48)) f2c |= 0x100;
                Stat(0xbc) = Stat(0xbc) + Stat(0x90) + Stat(0xac);
                if (f28 & 0x80000000) ++Stat(0x110);
                if (0 < Stat(0x68)) f2c |= 0x40000;
                if (f30 & 0x80) f2c |= 0x80000;
                if (0 < Stat(0x78)) f2c |= 0x100000;
                if (0 < Stat(0x90)) f2c |= 0x200000;
                if (0 < Stat(0xac)) f2c |= 0x400000;
                const bool hasBuildings = 0 < Stat(0x50) || 0 < Stat(0x54) || 0 < Stat(0x58) || 0 < Stat(0x5c) ||
                                          0 < Stat(0x60) || 0 < Stat(0x70) || 0 < Stat(0x64);
                if (hasBuildings && Stat(0x74) == 0) f30 |= 0x800;
                if (0 < Stat(0x58) || 0 < Stat(0x5c) || 0 < Stat(0x60)) f2c |= 0x20000;
                if (0 < Stat(0x50) || 0 < Stat(0x54)) f2c |= 0x10000;
            }
        } else {
            int primary = 0;
            int secondary = 0;
            int reserve = 0;
            const int* budget = owner->m_budget;
            if (0 < 0x46 - Stat(0x48)) {
                Stat(0x98) += (0x4f - Stat(0x48)) / 10;
                if (6 < Stat(0x98)) Stat(0x98) = 6;
            }
            if ((control & 0x40000000) == 0) {
                if ((f28 & 0x3e00000) == 0) {
                    if (0 < Stat(0x98)) {
                        reserve = budget[5];
                        if (reserve + budget[4] < Stat(0x4c)) {
                            primary = budget[4];
                            secondary = reserve;
                        }
                    }
                } else {
                    reserve = budget[3];
                    if (reserve + budget[2] < Stat(0x4c)) {
                        primary = budget[2];
                        secondary = reserve;
                    } else if (budget[4] < Stat(0x4c)) {
                        primary = budget[4];
                    }
                }
            } else {
                if (m_view == 2) {
                    reserve = 1;
                    if ((owner->m_flags.Get() & 0x1000000) != 0 && (owner->m_flags.Get() & 4) != 0 && Stat(0x70) == 0)
                        f28 |= 0x40000000;
                }
                const int energy = Stat(0x4c);
                if (energy < reserve + 2) {
                    primary = energy - reserve;
                    secondary = 0;
                } else {
                    primary = budget[1];
                    secondary = budget[0];
                    if (energy < reserve + budget[0] + primary) secondary = (energy - primary) - reserve;
                }
            }
            Stat(0x6c) = primary;
            const int defenseGap = Stat(0x64) - primary;
            const int shieldGap = Stat(0x74) - secondary;
            Stat(0x7c) = secondary;
            if (defenseGap < 1) {
                if (defenseGap < 0) f28 |= 0x8000;
                if (shieldGap < 1) {
                    Stat(0xdc) = defenseGap + shieldGap;
                    if (defenseGap + shieldGap < 0) f28 |= 0x4000;
                } else {
                    Stat(0xdc) = shieldGap;
                }
            } else if (shieldGap < 1) {
                Stat(0xdc) = defenseGap;
            } else {
                Stat(0xdc) = shieldGap + defenseGap;
            }
            const uint32_t before = f28;
            if (before & 1) Stat(0x98) <<= 1;
            if ((before & 0x80000000) == 0)
                Stat(0xa0) = (before & 0x3e00000) == 0 ? budget[6] : budget[7];
            else
                Stat(0xa0) = budget[8];
            if (Stat(0xa0) < Stat(0x98)) Stat(0xa0) = Stat(0x98);
            if ((before & 2) == 0 || (f30 & 0x30000) == 0) {
                const int wanted = Stat(0xa0);
                bool short_ = false;
                if (Stat(0x90) != wanted) {
                    if (Stat(0x90) <= wanted) {
                        f28 |= 0x2000;
                        Stat(0x9c) = Stat(0xa0) - Stat(0x90);
                        short_ = true;
                    } else {
                        f28 |= 0x10000;
                        Stat(0x94) = Stat(0x90) - Stat(0xa0);
                    }
                }
                if (!short_ && Stat(0x8c) < wanted) {
                    f28 |= 0x20000;
                    Stat(0x9c) = Stat(0xa0) - Stat(0x8c);
                }
            }
            if ((f28 & 0x80000000) == 0) {
                if ((f28 & 0x3e00000) == 0) {
                    if (Stat(0x8c) < 1) {
                        Stat(0xe0) = Stat(0xa4);
                        Stat(0x11c) = Stat(0xa4);
                    } else {
                        Stat(0xe0) = Stat(0xa8) - budget[9];
                        Stat(0x11c) = Stat(0xac) - budget[9];
                    }
                } else {
                    Stat(0xe0) = Stat(0xa8) - budget[10];
                    Stat(0x11c) = Stat(0xac) - budget[10];
                }
            } else {
                Stat(0xe0) = Stat(0xa8) - budget[11];
                Stat(0x11c) = Stat(0xac) - budget[11];
            }
            if (Stat(0xe0) < 0) f28 |= 0x40000;
            if (Stat(0x11c) < 0) f28 |= 0x100000;
            const uint32_t snapshot = f28;
            int facilities = 0;
            if (snapshot & 0x3800000) facilities = Stat(0x60);
            if (snapshot & 0x200000) facilities += Stat(0x58);
            if (snapshot & 0x400000) facilities += Stat(0x5c);
            const int defenses = std::max(Stat(0x64), Stat(0x6c));
            const int shields = std::max(Stat(0x74), Stat(0x7c));
            int committed = facilities + defenses + shields;
            if (f30 & 0x80000000) ++committed;
            Stat(0x114) = Stat(0x4c) - committed;
            if ((snapshot & 3) == 0) {
                if (0 < owner->m_characters.m_usableCount && (f30 & 0x8000000) == 0) f2c |= 2;
                if ((f28 & 0x200000) != 0 && (owner->m_flags.Get() & 0x2000000) == 0 && usable28 != 0 && 0 < Stat(0x90))
                    f2c |= 4;
                if ((f28 & 0x400000) != 0 && (owner->m_flags.Get() & 0x4000000) == 0 && usable29 != 0 && 0 < Stat(0x90))
                    f2c |= 8;
                if ((f28 & 0x3800000) != 0 && (owner->m_flags.Get() & 0x8000000) == 0 && usable2a != 0 && 0 < Stat(0x90))
                    f2c |= 0x10;
            }
            if ((m_flags28.m_value & 1) != 0 && (f30 & 0x200) == 0) f2c |= 0x20;
            if ((f2c & 0x10000000) != 0 && (f30 & 0x200) == 0 && (f28 & 1) == 0) f2c |= 0x40;
        }
    }
    if ((f30 & 1) == 0) {
        f2c |= 0x80;
        if ((f30 & 4) == 0) return;
    } else {
        f2c |= 1;
    }
    f2c |= 0x200;
}
