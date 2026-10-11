#include "core/utility_classes.h"
#include "fleet/unit_data_decl.h"

DetectorData::DetectorData() = default;

DetectorData::~DetectorData() = default;

CombatUnitData::CombatUnitData() = default;

CombatUnitData::~CombatUnitData() = default;

int CombatUnitData::Read(istream* in)
{
  int result = UnitObjData::Read(in);
  if (result)
    result = m_repairStat.Read(in);
  return result;
}

int CombatUnitData::Write(ostream* out)
{
  int result = UnitObjData::Write(out);
  if (result)
    result = m_repairStat.Write(out);
  return result;
}

int CombatUnitData::Checksum(Crc32* crc)
{
  int result = UnitObjData::Checksum(crc);
  if (result)
    result = m_repairStat.Checksum(crc);
  return result;
}
