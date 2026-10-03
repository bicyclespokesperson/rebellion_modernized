#pragma once
#include <cstdint>

#include "gameobj/object_key.h"
#include "gameobj/status_code.h"

// Object families: the top byte of an ObjectKey says what kind of object it names. The object database, iterators and
// lookups work on half-open ranges [first, last) of families (FamilyRange). The families that have a registered factory
// (0x51ebf0) are the single values below; the ranges are the groupings the typed lookups (object_lookup.h) use.
namespace family {

constexpr uint32_t kFleet = 0x08;
constexpr uint32_t kTroopRegiment = 0x10;
constexpr uint32_t kCapitalShip = 0x14;
constexpr uint32_t kCapitalShipDeathStar = 0x18;
constexpr uint32_t kFighterSquadron = 0x1c;
constexpr uint32_t kSector = 0x80;
constexpr uint32_t kSystem = 0x90;
constexpr uint32_t kSystemUnexplored = 0x92;
constexpr uint32_t kGameRoot = 0xf1;
constexpr uint32_t kSideRoot = 0xf2;
constexpr uint32_t kSide = 0xf3;
constexpr uint32_t kMissionManager = 0xf8;
constexpr uint32_t kCharacterManager = 0xfa;

constexpr FamilyRange kFleets(0x08, 0x10);
constexpr FamilyRange kTroopRegiments(0x10, 0x14);
constexpr FamilyRange kCapitalShips(0x14, 0x1c);
constexpr FamilyRange kFighterSquadrons(0x1c, 0x20);
constexpr FamilyRange kFacilities(0x20, 0x30);
constexpr FamilyRange kCharacters(0x30, 0x3c);
constexpr FamilyRange kSpecialForces(0x3c, 0x40);
constexpr FamilyRange kUnits(0x10, 0x40);
constexpr FamilyRange kMissions(0x40, 0x80);
constexpr FamilyRange kSectors(0x80, 0x90);
constexpr FamilyRange kSystems(0x90, 0x98);
constexpr FamilyRange kManufacturingManagers(0xa0, 0xb0);

}  // namespace family
