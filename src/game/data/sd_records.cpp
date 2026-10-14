#include "data/sd_records.h"

#include "core/checked_new.h"
#include "core/utility_classes.h"

namespace
{

void ParseShortField(istream* in, strstream* token, int* endOfLine, void* value)
{
  istream_operator_rs_short(iostream_access::AsIstream(ReadCsvField(in, token, endOfLine)), static_cast<short*>(value));
}

} // namespace

// ---- BasicSD ------------------------------------------------------------------------------------

BasicSD::BasicSD() = default;

BasicSD::~BasicSD() = default;

void* CreateBasicSD()
{
  return NewOrNull<BasicSD>();
}

int BasicSD::Read(istream* in)
{
  DatRecord::Read(in);
  return 1;
}

int BasicSD::Write(ostream* out)
{
  DatRecord::Write(out);
  return 1;
}

int BasicSD::Checksum(Crc32* crc)
{
  DatRecord::Checksum(crc);
  return 1;
}

int BasicSD::ParseFields(istream* in, int* endOfLine)
{
  DatRecord::ParseFields(in, endOfLine);
  return 1;
}

// ---- TroopSD ------------------------------------------------------------------------------------

TroopSD::TroopSD() : m_bombardmentDefense(0), m_attackStrength(0), m_defenseStrength(0)
{
}

TroopSD::~TroopSD() = default;

void* CreateTroopSD()
{
  return NewOrNull<TroopSD>();
}

int TroopSD::Read(istream* in)
{
  SdRecordExt2::Read(in);
  ReadValue32(in, &m_bombardmentDefense);
  ReadValue32(in, &m_attackStrength);
  ReadValue32(in, &m_defenseStrength);
  return 1;
}

int TroopSD::Write(ostream* out)
{
  SdRecordExt2::Write(out);
  WriteValue32(out, &m_bombardmentDefense);
  WriteValue32(out, &m_attackStrength);
  WriteValue32(out, &m_defenseStrength);
  return 1;
}

int TroopSD::Checksum(Crc32* crc)
{
  SdRecordExt2::Checksum(crc);
  crc->AddInt(m_bombardmentDefense);
  crc->AddInt(m_attackStrength);
  crc->AddInt(m_defenseStrength);
  return 1;
}

int TroopSD::ParseFields(istream* in, int* endOfLine)
{
  ScopedStrstream token;
  SdRecordExt2::ParseFields(in, endOfLine);
  ParseLongField(in, token.get(), endOfLine, &m_bombardmentDefense);
  ParseLongField(in, token.get(), endOfLine, &m_attackStrength);
  ParseLongField(in, token.get(), endOfLine, &m_defenseStrength);
  return 1;
}

// ---- SystemSD -----------------------------------------------------------------------------------

SystemSD::SystemSD() : m_sectorId(0), m_pictureId(0), m_field48(0), m_xPosition(0), m_yPosition(0), m_field50(0)
{
}

SystemSD::~SystemSD() = default;

void* CreateSystemSD()
{
  return NewOrNull<SystemSD>();
}

int SystemSD::Read(istream* in)
{
  DatRecord::Read(in);
  ReadValue32(in, &m_sectorId);
  ReadValue32(in, &m_pictureId);
  ReadValue32(in, &m_field48);
  ReadValue16(in, &m_xPosition);
  ReadValue16(in, &m_yPosition);
  ReadValue32(in, &m_field50);
  return 1;
}

int SystemSD::Write(ostream* out)
{
  DatRecord::Write(out);
  WriteValue32(out, &m_sectorId);
  WriteValue32(out, &m_pictureId);
  WriteValue32(out, &m_field48);
  WriteValue16(out, &m_xPosition);
  WriteValue16(out, &m_yPosition);
  WriteValue32(out, &m_field50);
  return 1;
}

int SystemSD::Checksum(Crc32* crc)
{
  DatRecord::Checksum(crc);
  crc->AddInt(m_sectorId);
  crc->AddInt(m_pictureId);
  crc->AddInt(m_field48);
  crc->AddShort(m_xPosition);
  crc->AddShort(m_yPosition);
  crc->AddInt(m_field50);
  return 1;
}

int SystemSD::ParseFields(istream* in, int* endOfLine)
{
  ScopedStrstream token;
  DatRecord::ParseFields(in, endOfLine);
  ParseUnsignedField(in, token.get(), endOfLine, &m_sectorId);
  ParseLongField(in, token.get(), endOfLine, &m_pictureId);
  ParseLongField(in, token.get(), endOfLine, &m_field48);
  ParseShortField(in, token.get(), endOfLine, &m_xPosition);
  ParseShortField(in, token.get(), endOfLine, &m_yPosition);
  ParseLongField(in, token.get(), endOfLine, &m_field50);
  return 1;
}

// ---- SpecialForceSD -----------------------------------------------------------------------------

SpecialForceSD::SpecialForceSD() : m_missionId(0)
{
}

SpecialForceSD::~SpecialForceSD() = default;

void* CreateSpecialForceSD()
{
  return NewOrNull<SpecialForceSD>();
}

int SpecialForceSD::Read(istream* in)
{
  SdRecordExt16::Read(in);
  ReadValue32(in, &m_missionId);
  return 1;
}

int SpecialForceSD::Write(ostream* out)
{
  SdRecordExt16::Write(out);
  WriteValue32(out, &m_missionId);
  return 1;
}

int SpecialForceSD::Checksum(Crc32* crc)
{
  SdRecordExt16::Checksum(crc);
  crc->AddInt(m_missionId);
  return 1;
}

int SpecialForceSD::ParseFields(istream* in, int* endOfLine)
{
  ScopedStrstream token;
  SdRecordExt16::ParseFields(in, endOfLine);
  ParseUnsignedField(in, token.get(), endOfLine, &m_missionId);
  return 1;
}

// ---- SectorSD -----------------------------------------------------------------------------------

SectorSD::SectorSD() : m_group(0), m_galaxySize(0), m_xPosition(0), m_yPosition(0)
{
}

SectorSD::~SectorSD() = default;

void* CreateSectorSD()
{
  return NewOrNull<SectorSD>();
}

int SectorSD::Read(istream* in)
{
  DatRecord::Read(in);
  ReadValue32(in, &m_group);
  ReadValue32(in, &m_galaxySize);
  ReadValue16(in, &m_xPosition);
  ReadValue16(in, &m_yPosition);
  return 1;
}

int SectorSD::Write(ostream* out)
{
  DatRecord::Write(out);
  WriteValue32(out, &m_group);
  WriteValue32(out, &m_galaxySize);
  WriteValue16(out, &m_xPosition);
  WriteValue16(out, &m_yPosition);
  return 1;
}

int SectorSD::Checksum(Crc32* crc)
{
  DatRecord::Checksum(crc);
  crc->AddInt(m_group);
  crc->AddInt(m_galaxySize);
  crc->AddShort(m_xPosition);
  crc->AddShort(m_yPosition);
  return 1;
}

int SectorSD::ParseFields(istream* in, int* endOfLine)
{
  ScopedStrstream token;
  DatRecord::ParseFields(in, endOfLine);
  ParseLongField(in, token.get(), endOfLine, &m_group);
  ParseLongField(in, token.get(), endOfLine, &m_galaxySize);
  ParseShortField(in, token.get(), endOfLine, &m_xPosition);
  ParseShortField(in, token.get(), endOfLine, &m_yPosition);
  return 1;
}

// ---- ProductionFacilitySD -----------------------------------------------------------------------

ProductionFacilitySD::ProductionFacilitySD() : m_processingRate(0)
{
}

ProductionFacilitySD::~ProductionFacilitySD() = default;

void* CreateProductionFacilitySD()
{
  return NewOrNull<ProductionFacilitySD>();
}

int ProductionFacilitySD::Read(istream* in)
{
  SdRecordExt1::Read(in);
  ReadValue32(in, &m_processingRate);
  return 1;
}

int ProductionFacilitySD::Write(ostream* out)
{
  SdRecordExt1::Write(out);
  WriteValue32(out, &m_processingRate);
  return 1;
}

int ProductionFacilitySD::Checksum(Crc32* crc)
{
  SdRecordExt1::Checksum(crc);
  crc->AddInt(m_processingRate);
  return 1;
}

int ProductionFacilitySD::ParseFields(istream* in, int* endOfLine)
{
  ScopedStrstream token;
  SdRecordExt1::ParseFields(in, endOfLine);
  ParseLongField(in, token.get(), endOfLine, &m_processingRate);
  return 1;
}

// ---- MissionSD ----------------------------------------------------------------------------------

MissionSD::MissionSD()
: m_isAlliance(0),
  m_isEmpire(0),
  m_specialForceEligibility(0),
  m_targetFlags(0),
  m_timerMinDays(0),
  m_timerSpreadDays(0),
  m_repeats(0),
  m_hidden(0),
  m_detectionPhases(0),
  m_canResign(0),
  m_field68(0),
  m_containerLossEnds(0),
  m_field70(0),
  m_needsPopulatedContainer(0),
  m_targetLossEnds(0),
  m_ownSideTarget(0),
  m_otherSideTarget(0),
  m_opponentTarget(0),
  m_revoltingTarget(0),
  m_calmTarget(0),
  m_prisonerTarget(0),
  m_freeTarget(0)
{
}

MissionSD::~MissionSD() = default;

void* CreateMissionSD()
{
  return NewOrNull<MissionSD>();
}

int MissionSD::Read(istream* in)
{
  DatRecord::Read(in);
  ReadValue32(in, &m_isAlliance);
  ReadValue32(in, &m_isEmpire);
  ReadValue32(in, &m_specialForceEligibility);
  ReadValue32(in, &m_targetFlags);
  ReadValue32(in, &m_timerMinDays);
  ReadValue32(in, &m_timerSpreadDays);
  ReadValue32(in, &m_repeats);
  ReadValue32(in, &m_hidden);
  ReadValue32(in, &m_detectionPhases);
  ReadValue32(in, &m_canResign);
  ReadValue32(in, &m_field68);
  ReadValue32(in, &m_containerLossEnds);
  ReadValue32(in, &m_field70);
  ReadValue32(in, &m_needsPopulatedContainer);
  ReadValue32(in, &m_targetLossEnds);
  ReadValue32(in, &m_ownSideTarget);
  ReadValue32(in, &m_otherSideTarget);
  ReadValue32(in, &m_opponentTarget);
  ReadValue32(in, &m_revoltingTarget);
  ReadValue32(in, &m_calmTarget);
  ReadValue32(in, &m_prisonerTarget);
  ReadValue32(in, &m_freeTarget);
  return 1;
}

int MissionSD::Write(ostream* out)
{
  DatRecord::Write(out);
  WriteValue32(out, &m_isAlliance);
  WriteValue32(out, &m_isEmpire);
  WriteValue32(out, &m_specialForceEligibility);
  WriteValue32(out, &m_targetFlags);
  WriteValue32(out, &m_timerMinDays);
  WriteValue32(out, &m_timerSpreadDays);
  WriteValue32(out, &m_repeats);
  WriteValue32(out, &m_hidden);
  WriteValue32(out, &m_detectionPhases);
  WriteValue32(out, &m_canResign);
  WriteValue32(out, &m_field68);
  WriteValue32(out, &m_containerLossEnds);
  WriteValue32(out, &m_field70);
  WriteValue32(out, &m_needsPopulatedContainer);
  WriteValue32(out, &m_targetLossEnds);
  WriteValue32(out, &m_ownSideTarget);
  WriteValue32(out, &m_otherSideTarget);
  WriteValue32(out, &m_opponentTarget);
  WriteValue32(out, &m_revoltingTarget);
  WriteValue32(out, &m_calmTarget);
  WriteValue32(out, &m_prisonerTarget);
  WriteValue32(out, &m_freeTarget);
  return 1;
}

int MissionSD::Checksum(Crc32* crc)
{
  DatRecord::Checksum(crc);
  crc->AddInt(m_isAlliance);
  crc->AddInt(m_isEmpire);
  crc->AddInt(m_specialForceEligibility);
  crc->AddInt(m_targetFlags);
  crc->AddInt(m_timerMinDays);
  crc->AddInt(m_timerSpreadDays);
  crc->AddInt(m_repeats);
  crc->AddInt(m_hidden);
  crc->AddInt(m_detectionPhases);
  crc->AddInt(m_canResign);
  crc->AddInt(m_field68);
  crc->AddInt(m_containerLossEnds);
  crc->AddInt(m_field70);
  crc->AddInt(m_needsPopulatedContainer);
  crc->AddInt(m_targetLossEnds);
  crc->AddInt(m_ownSideTarget);
  crc->AddInt(m_otherSideTarget);
  crc->AddInt(m_opponentTarget);
  crc->AddInt(m_revoltingTarget);
  crc->AddInt(m_calmTarget);
  crc->AddInt(m_prisonerTarget);
  crc->AddInt(m_freeTarget);
  return 1;
}

int MissionSD::ParseFields(istream* in, int* endOfLine)
{
  ScopedStrstream token;
  DatRecord::ParseFields(in, endOfLine);
  ParseLongField(in, token.get(), endOfLine, &m_isAlliance);
  ParseLongField(in, token.get(), endOfLine, &m_isEmpire);
  ParseUnsignedField(in, token.get(), endOfLine, &m_specialForceEligibility);
  ParseUnsignedField(in, token.get(), endOfLine, &m_targetFlags);
  ParseLongField(in, token.get(), endOfLine, &m_timerMinDays);
  ParseLongField(in, token.get(), endOfLine, &m_timerSpreadDays);
  ParseLongField(in, token.get(), endOfLine, &m_repeats);
  ParseLongField(in, token.get(), endOfLine, &m_hidden);
  ParseLongField(in, token.get(), endOfLine, &m_detectionPhases);
  ParseLongField(in, token.get(), endOfLine, &m_canResign);
  ParseLongField(in, token.get(), endOfLine, &m_field68);
  ParseLongField(in, token.get(), endOfLine, &m_containerLossEnds);
  ParseLongField(in, token.get(), endOfLine, &m_field70);
  ParseLongField(in, token.get(), endOfLine, &m_needsPopulatedContainer);
  ParseLongField(in, token.get(), endOfLine, &m_targetLossEnds);
  ParseLongField(in, token.get(), endOfLine, &m_ownSideTarget);
  ParseLongField(in, token.get(), endOfLine, &m_otherSideTarget);
  ParseLongField(in, token.get(), endOfLine, &m_opponentTarget);
  ParseLongField(in, token.get(), endOfLine, &m_revoltingTarget);
  ParseLongField(in, token.get(), endOfLine, &m_calmTarget);
  ParseLongField(in, token.get(), endOfLine, &m_prisonerTarget);
  ParseLongField(in, token.get(), endOfLine, &m_freeTarget);
  return 1;
}

// ---- MinorCharacterSD ---------------------------------------------------------------------------

MinorCharacterSD::MinorCharacterSD() = default;

MinorCharacterSD::~MinorCharacterSD() = default;

void* CreateMinorCharacterSD()
{
  return NewOrNull<MinorCharacterSD>();
}

int MinorCharacterSD::Read(istream* in)
{
  SdRecordExt25::Read(in);
  return 1;
}

int MinorCharacterSD::Write(ostream* out)
{
  SdRecordExt25::Write(out);
  return 1;
}

int MinorCharacterSD::Checksum(Crc32* crc)
{
  SdRecordExt25::Checksum(crc);
  return 1;
}

int MinorCharacterSD::ParseFields(istream* in, int* endOfLine)
{
  // The original constructs a token stream here although nothing uses it.
  ScopedStrstream token;
  SdRecordExt25::ParseFields(in, endOfLine);
  return 1;
}

// ---- ManufacturingFacilitySD --------------------------------------------------------------------

ManufacturingFacilitySD::ManufacturingFacilitySD() : m_processingRate(0)
{
}

ManufacturingFacilitySD::~ManufacturingFacilitySD() = default;

void* CreateManufacturingFacilitySD()
{
  return NewOrNull<ManufacturingFacilitySD>();
}

int ManufacturingFacilitySD::Read(istream* in)
{
  SdRecordExt1::Read(in);
  ReadValue32(in, &m_processingRate);
  return 1;
}

int ManufacturingFacilitySD::Write(ostream* out)
{
  SdRecordExt1::Write(out);
  WriteValue32(out, &m_processingRate);
  return 1;
}

int ManufacturingFacilitySD::Checksum(Crc32* crc)
{
  SdRecordExt1::Checksum(crc);
  crc->AddInt(m_processingRate);
  return 1;
}

int ManufacturingFacilitySD::ParseFields(istream* in, int* endOfLine)
{
  ScopedStrstream token;
  SdRecordExt1::ParseFields(in, endOfLine);
  ParseLongField(in, token.get(), endOfLine, &m_processingRate);
  return 1;
}

// ---- MajorCharacterSD ---------------------------------------------------------------------------

MajorCharacterSD::MajorCharacterSD() = default;

MajorCharacterSD::~MajorCharacterSD() = default;

void* CreateMajorCharacterSD()
{
  return NewOrNull<MajorCharacterSD>();
}

int MajorCharacterSD::Read(istream* in)
{
  SdRecordExt25::Read(in);
  return 1;
}

int MajorCharacterSD::Write(ostream* out)
{
  SdRecordExt25::Write(out);
  return 1;
}

int MajorCharacterSD::Checksum(Crc32* crc)
{
  SdRecordExt25::Checksum(crc);
  return 1;
}

int MajorCharacterSD::ParseFields(istream* in, int* endOfLine)
{
  // The original constructs a token stream here although nothing uses it.
  ScopedStrstream token;
  SdRecordExt25::ParseFields(in, endOfLine);
  return 1;
}

// ---- FighterSD ----------------------------------------------------------------------------------

FighterSD::FighterSD() : m_torpedoes(0), m_torpedoRange(0), m_squadronSize(0), m_bombardmentDefense(0)
{
}

FighterSD::~FighterSD() = default;

void* CreateFighterSD()
{
  return NewOrNull<FighterSD>();
}

int FighterSD::Read(istream* in)
{
  SdRecordGrid::Read(in);
  ReadValue32(in, &m_torpedoes);
  ReadValue32(in, &m_torpedoRange);
  ReadValue32(in, &m_squadronSize);
  ReadValue32(in, &m_bombardmentDefense);
  return 1;
}

int FighterSD::Write(ostream* out)
{
  SdRecordGrid::Write(out);
  WriteValue32(out, &m_torpedoes);
  WriteValue32(out, &m_torpedoRange);
  WriteValue32(out, &m_squadronSize);
  WriteValue32(out, &m_bombardmentDefense);
  return 1;
}

int FighterSD::Checksum(Crc32* crc)
{
  SdRecordGrid::Checksum(crc);
  crc->AddInt(m_torpedoes);
  crc->AddInt(m_torpedoRange);
  crc->AddInt(m_squadronSize);
  crc->AddInt(m_bombardmentDefense);
  return 1;
}

int FighterSD::ParseFields(istream* in, int* endOfLine)
{
  ScopedStrstream token;
  SdRecordGrid::ParseFields(in, endOfLine);
  ParseLongField(in, token.get(), endOfLine, &m_torpedoes);
  ParseLongField(in, token.get(), endOfLine, &m_torpedoRange);
  ParseLongField(in, token.get(), endOfLine, &m_squadronSize);
  ParseLongField(in, token.get(), endOfLine, &m_bombardmentDefense);
  return 1;
}

// ---- DefenseFacilitySD --------------------------------------------------------------------------

DefenseFacilitySD::DefenseFacilitySD() : m_attackStrength(0), m_shieldStrength(0)
{
}

DefenseFacilitySD::~DefenseFacilitySD() = default;

void* CreateDefenseFacilitySD()
{
  return NewOrNull<DefenseFacilitySD>();
}

int DefenseFacilitySD::Read(istream* in)
{
  SdRecordExt1::Read(in);
  ReadValue32(in, &m_attackStrength);
  ReadValue32(in, &m_shieldStrength);
  return 1;
}

int DefenseFacilitySD::Write(ostream* out)
{
  SdRecordExt1::Write(out);
  WriteValue32(out, &m_attackStrength);
  WriteValue32(out, &m_shieldStrength);
  return 1;
}

int DefenseFacilitySD::Checksum(Crc32* crc)
{
  SdRecordExt1::Checksum(crc);
  crc->AddInt(m_attackStrength);
  crc->AddInt(m_shieldStrength);
  return 1;
}

int DefenseFacilitySD::ParseFields(istream* in, int* endOfLine)
{
  ScopedStrstream token;
  SdRecordExt1::ParseFields(in, endOfLine);
  ParseLongField(in, token.get(), endOfLine, &m_attackStrength);
  ParseLongField(in, token.get(), endOfLine, &m_shieldStrength);
  return 1;
}

// ---- CapitalShipSD ------------------------------------------------------------------------------

CapitalShipSD::CapitalShipSD()
: m_hull(0),
  m_tractorBeamPower(0),
  m_tractorBeamRange(0),
  m_gravityWellProjector(0),
  m_interdictionStrength(0),
  m_bombardmentModifier(0),
  m_damageControl(0),
  m_weaponRechargeRate(0),
  m_shieldRechargeRate(0),
  m_fighterCapacity(0),
  m_troopCapacity(0),
  m_field51(0)
{
}

CapitalShipSD::~CapitalShipSD() = default;

void* CreateCapitalShipSD()
{
  return NewOrNull<CapitalShipSD>();
}

int CapitalShipSD::Read(istream* in)
{
  SdRecordGrid::Read(in);
  ReadValue32(in, &m_hull);
  ReadValue32(in, &m_tractorBeamPower);
  ReadValue32(in, &m_tractorBeamRange);
  ReadValue32(in, &m_gravityWellProjector);
  ReadValue32(in, &m_interdictionStrength);
  ReadValue32(in, &m_bombardmentModifier);
  ReadValue32(in, &m_damageControl);
  ReadValue32(in, &m_weaponRechargeRate);
  ReadValue32(in, &m_shieldRechargeRate);
  ReadValue32(in, &m_fighterCapacity);
  ReadValue32(in, &m_troopCapacity);
  ReadValue32(in, &m_field51);
  return 1;
}

int CapitalShipSD::Write(ostream* out)
{
  SdRecordGrid::Write(out);
  WriteValue32(out, &m_hull);
  WriteValue32(out, &m_tractorBeamPower);
  WriteValue32(out, &m_tractorBeamRange);
  WriteValue32(out, &m_gravityWellProjector);
  WriteValue32(out, &m_interdictionStrength);
  WriteValue32(out, &m_bombardmentModifier);
  WriteValue32(out, &m_damageControl);
  WriteValue32(out, &m_weaponRechargeRate);
  WriteValue32(out, &m_shieldRechargeRate);
  WriteValue32(out, &m_fighterCapacity);
  WriteValue32(out, &m_troopCapacity);
  WriteValue32(out, &m_field51);
  return 1;
}

int CapitalShipSD::Checksum(Crc32* crc)
{
  SdRecordGrid::Checksum(crc);
  crc->AddInt(m_hull);
  crc->AddInt(m_tractorBeamPower);
  crc->AddInt(m_tractorBeamRange);
  crc->AddInt(m_gravityWellProjector);
  crc->AddInt(m_interdictionStrength);
  crc->AddInt(m_bombardmentModifier);
  crc->AddInt(m_damageControl);
  crc->AddInt(m_weaponRechargeRate);
  crc->AddInt(m_shieldRechargeRate);
  crc->AddInt(m_fighterCapacity);
  crc->AddInt(m_troopCapacity);
  crc->AddInt(m_field51);
  return 1;
}

int CapitalShipSD::ParseFields(istream* in, int* endOfLine)
{
  ScopedStrstream token;
  SdRecordGrid::ParseFields(in, endOfLine);
  ParseLongField(in, token.get(), endOfLine, &m_hull);
  ParseLongField(in, token.get(), endOfLine, &m_tractorBeamPower);
  ParseLongField(in, token.get(), endOfLine, &m_tractorBeamRange);
  ParseLongField(in, token.get(), endOfLine, &m_gravityWellProjector);
  ParseLongField(in, token.get(), endOfLine, &m_interdictionStrength);
  ParseLongField(in, token.get(), endOfLine, &m_bombardmentModifier);
  ParseLongField(in, token.get(), endOfLine, &m_damageControl);
  ParseLongField(in, token.get(), endOfLine, &m_weaponRechargeRate);
  ParseLongField(in, token.get(), endOfLine, &m_shieldRechargeRate);
  ParseLongField(in, token.get(), endOfLine, &m_fighterCapacity);
  ParseLongField(in, token.get(), endOfLine, &m_troopCapacity);
  ParseLongField(in, token.get(), endOfLine, &m_field51);
  return 1;
}

// ---- AllFacilitySD ------------------------------------------------------------------------------

AllFacilitySD::AllFacilitySD() = default;

AllFacilitySD::~AllFacilitySD() = default;

void* CreateAllFacilitySD()
{
  return NewOrNull<AllFacilitySD>();
}

int AllFacilitySD::Read(istream* in)
{
  SdRecordExt1::Read(in);
  return 1;
}

int AllFacilitySD::Write(ostream* out)
{
  SdRecordExt1::Write(out);
  return 1;
}

int AllFacilitySD::Checksum(Crc32* crc)
{
  SdRecordExt1::Checksum(crc);
  return 1;
}

int AllFacilitySD::ParseFields(istream* in, int* endOfLine)
{
  // The original constructs a token stream here although nothing uses it.
  ScopedStrstream token;
  SdRecordExt1::ParseFields(in, endOfLine);
  return 1;
}
