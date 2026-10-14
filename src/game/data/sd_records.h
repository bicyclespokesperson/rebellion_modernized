#pragma once
#include <cstdint>

#include "data/dat_record.h"
#include "data/sd_record_bases.h"

// The concrete SD record classes: one per kind of GData/*SD.DAT file (see SeedList). The record factory
// registry (RegisterRecordFactories) creates them by family range. Every class overrides the four stream /
// text operations, calls its base class's version first and then handles its own columns, in file order.
// Field names come from the community documentation of the file formats; columns whose meaning is not
// known keep a neutral name.

// Plain record with only the common DatRecord fields, for the small tables FLEETSD, UNIQUESD, ABODESD, BASICSD and
// MANMGRSD (families 0x08-0x0f, 0x98-0xaf and 0xf0-0xfe).
class BasicSD : public DatRecord
{
public:
  BasicSD();
  ~BasicSD() override;
  int Write(ostream* out) override;
  int Read(istream* in) override;
  int Checksum(Crc32* crc) override;
  int ParseFields(istream* in, int* endOfLine) override;
};

// TROOPSD.DAT (families 0x10-0x13). SdRecordExt2's fields are the uprising defense (+0x58) and the detection
// (+0x5c) of the troops.
class TroopSD : public SdRecordExt2
{
public:
  TroopSD();
  ~TroopSD() override;
  int Write(ostream* out) override;
  int Read(istream* in) override;
  int Checksum(Crc32* crc) override;
  int ParseFields(istream* in, int* endOfLine) override;

  int32_t m_bombardmentDefense; // +0x60
  int32_t m_attackStrength; // +0x64
  int32_t m_defenseStrength; // +0x68
};

// SYSTEMSD.DAT (families 0x90-0x97).
class SystemSD : public DatRecord
{
public:
  SystemSD();
  ~SystemSD() override;
  int Write(ostream* out) override;
  int Read(istream* in) override;
  int Checksum(Crc32* crc) override;
  int ParseFields(istream* in, int* endOfLine) override;

  uint32_t m_sectorId; // +0x40
  int32_t m_pictureId; // +0x44
  int32_t m_field48; // +0x48
  int16_t m_xPosition; // +0x4c
  int16_t m_yPosition; // +0x4e
  int32_t m_field50; // +0x50
};

// SPECFCSD.DAT (families 0x3c-0x3f). SdRecordExt16's fields are the base and variance of the eight skills,
// from diplomacy to loyalty.
class SpecialForceSD : public SdRecordExt16
{
public:
  SpecialForceSD();
  ~SpecialForceSD() override;
  int Write(ostream* out) override;
  int Read(istream* in) override;
  int Checksum(Crc32* crc) override;
  int ParseFields(istream* in, int* endOfLine) override;

  uint32_t m_missionId; // +0x98  // bitmask of the missions the force can perform
};

// SECTORSD.DAT (families 0x80-0x8f).
class SectorSD : public DatRecord
{
public:
  SectorSD();
  ~SectorSD() override;
  int Write(ostream* out) override;
  int Read(istream* in) override;
  int Checksum(Crc32* crc) override;
  int ParseFields(istream* in, int* endOfLine) override;

  int32_t m_group; // +0x40
  int32_t m_galaxySize; // +0x44
  int16_t m_xPosition; // +0x48
  int16_t m_yPosition; // +0x4a
};

// PROFACSD.DAT (families 0x2c-0x2f). Same layout as ManufacturingFacilitySD. SdRecordExt1's field is the
// bombardment defense (+0x58).
class ProductionFacilitySD : public SdRecordExt1
{
public:
  ProductionFacilitySD();
  ~ProductionFacilitySD() override;
  int Write(ostream* out) override;
  int Read(istream* in) override;
  int Checksum(Crc32* crc) override;
  int ParseFields(istream* in, int* endOfLine) override;

  int32_t m_processingRate; // +0x5c
};

// MISSNSD.DAT (families 0x40-0x7f). Unlike the other SD records its first six numbers are not an SdRecord's:
// +0x40/+0x44 are the sides that can run the mission.
class MissionSD : public DatRecord
{
public:
  MissionSD();
  ~MissionSD() override;
  int Write(ostream* out) override;
  int Read(istream* in) override;
  int Checksum(Crc32* crc) override;
  int ParseFields(istream* in, int* endOfLine) override;

  int32_t m_isAlliance; // +0x40
  int32_t m_isEmpire; // +0x44
  uint32_t m_specialForceEligibility; // +0x48  // bitmask of SpecialForceSD::m_missionId
  uint32_t m_targetFlags; // +0x4c  // 0x10000 if enemy systems can be targeted
  int32_t m_timerMinDays; // +0x50
  int32_t m_timerSpreadDays; // +0x54
  int32_t m_repeats; // +0x58
  int32_t m_hidden; // +0x5c
  int32_t m_detectionPhases; // +0x60
  int32_t m_canResign; // +0x64
  int32_t m_field68; // +0x68
  int32_t m_containerLossEnds; // +0x6c
  int32_t m_field70; // +0x70
  int32_t m_needsPopulatedContainer; // +0x74
  int32_t m_targetLossEnds; // +0x78
  int32_t m_ownSideTarget; // +0x7c
  int32_t m_otherSideTarget; // +0x80
  int32_t m_opponentTarget; // +0x84
  int32_t m_revoltingTarget; // +0x88
  int32_t m_calmTarget; // +0x8c
  int32_t m_prisonerTarget; // +0x90
  int32_t m_freeTarget; // +0x94
};

// MNCHARSD.DAT (families 0x38-0x3b). Same layout as MajorCharacterSD, which has no fields of its own
// either: SdRecordExt16's fields are the skills, SdRecordExt25's the Jedi attributes and abilities.
class MinorCharacterSD : public SdRecordExt25
{
public:
  MinorCharacterSD();
  ~MinorCharacterSD() override;
  int Write(ostream* out) override;
  int Read(istream* in) override;
  int Checksum(Crc32* crc) override;
  int ParseFields(istream* in, int* endOfLine) override;
};

// MANFACSD.DAT (families 0x28-0x2b). SdRecordExt1's field is the bombardment defense (+0x58).
class ManufacturingFacilitySD : public SdRecordExt1
{
public:
  ManufacturingFacilitySD();
  ~ManufacturingFacilitySD() override;
  int Write(ostream* out) override;
  int Read(istream* in) override;
  int Checksum(Crc32* crc) override;
  int ParseFields(istream* in, int* endOfLine) override;

  int32_t m_processingRate; // +0x5c
};

// MJCHARSD.DAT (families 0x30-0x37).
class MajorCharacterSD : public SdRecordExt25
{
public:
  MajorCharacterSD();
  ~MajorCharacterSD() override;
  int Write(ostream* out) override;
  int Read(istream* in) override;
  int Checksum(Crc32* crc) override;
  int ParseFields(istream* in, int* endOfLine) override;
};

// FIGHTSD.DAT (families 0x1c-0x1f).
class FighterSD : public SdRecordGrid
{
public:
  FighterSD();
  ~FighterSD() override;
  int Write(ostream* out) override;
  int Read(istream* in) override;
  int Checksum(Crc32* crc) override;
  int ParseFields(istream* in, int* endOfLine) override;

  int32_t m_torpedoes; // +0xc0
  int32_t m_torpedoRange; // +0xc4
  int32_t m_squadronSize; // +0xc8
  int32_t m_bombardmentDefense; // +0xcc
};

// DEFFACSD.DAT (families 0x22-0x27). SdRecordExt1's field is the bombardment defense (+0x58).
class DefenseFacilitySD : public SdRecordExt1
{
public:
  DefenseFacilitySD();
  ~DefenseFacilitySD() override;
  int Write(ostream* out) override;
  int Read(istream* in) override;
  int Checksum(Crc32* crc) override;
  int ParseFields(istream* in, int* endOfLine) override;

  int32_t m_attackStrength; // +0x5c
  int32_t m_shieldStrength; // +0x60
};

// CAPSHPSD.DAT (families 0x14-0x1b).
class CapitalShipSD : public SdRecordGrid
{
public:
  CapitalShipSD();
  ~CapitalShipSD() override;
  int Write(ostream* out) override;
  int Read(istream* in) override;
  int Checksum(Crc32* crc) override;
  int ParseFields(istream* in, int* endOfLine) override;

  int32_t m_hull; // +0xc0
  int32_t m_tractorBeamPower; // +0xc4
  int32_t m_tractorBeamRange; // +0xc8
  int32_t m_gravityWellProjector; // +0xcc
  int32_t m_interdictionStrength; // +0xd0
  int32_t m_bombardmentModifier; // +0xd4
  int32_t m_damageControl; // +0xd8
  int32_t m_weaponRechargeRate; // +0xdc
  int32_t m_shieldRechargeRate; // +0xe0
  int32_t m_fighterCapacity; // +0xe4
  int32_t m_troopCapacity; // +0xe8
  int32_t m_field51; // +0xec
};

// ALLFACSD.DAT (families 0x20-0x21).
class AllFacilitySD : public SdRecordExt1
{
public:
  AllFacilitySD();
  ~AllFacilitySD() override;
  int Write(ostream* out) override;
  int Read(istream* in) override;
  int Checksum(Crc32* crc) override;
  int ParseFields(istream* in, int* endOfLine) override;
};

// Factories of the record factory registry.
void* CreateBasicSD();
void* CreateTroopSD();
void* CreateSystemSD();
void* CreateSpecialForceSD();
void* CreateSectorSD();
void* CreateProductionFacilitySD();
void* CreateMissionSD();
void* CreateMinorCharacterSD();
void* CreateManufacturingFacilitySD();
void* CreateMajorCharacterSD();
void* CreateFighterSD();
void* CreateDefenseFacilitySD();
void* CreateCapitalShipSD();
void* CreateAllFacilitySD();
