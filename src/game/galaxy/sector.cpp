#include "galaxy/sector.h"

#include "core/checked_new.h"
#include "data/sd_records.h"
#include "gameobj/object_family.h"

Sector::Sector() : GameObj()
{
  SetFlag31(GetFlag31());
}

Sector::~Sector() = default;

uint32_t Sector::GetFamily()
{
  return family::kSector;
}

int Sector::Read(istream* in)
{
  return GameObj::Read(in);
}

int Sector::Write(ostream* out)
{
  return GameObj::Write(out);
}

int Sector::Checksum(Crc32* crc)
{
  return GameObj::Checksum(crc);
}

int Sector::Validate()
{
  return GameObj::Validate();
}

int Sector::ApplyAll(NotifyContext* ctx)
{
  const int isSideView = IsSideView();
  if (isSideView)
    return GameObj::ApplyAll(ctx);
  return isSideView;
}

int Sector::SetRecord(DatRecord* record)
{
  if (!record)
    return 0;
  const int familyMatches = AssignRecord(record);
  if (familyMatches)
    SetId(record->m_id);
  return familyMatches;
}

int Sector::CreateViews(int controlKind, NotifyContext* ctx)
{
  return GameObj::CreateViews(controlKind, ctx);
}

GameObj* Sector::CreateViewObject()
{
  return NewOrNull<Sector>();
}

GameObjNotifier* Sector::Create()
{
  return NewOrNull<Sector>();
}

int Sector::GetGroup()
{
  if (!m_record)
    return 1;
  return static_cast<SectorSD*>(m_record)->m_group;
}

void Sector::GetPosition(uint32_t* out)
{
  if (!m_record)
  {
    *out = 0;
    return;
  }
  const SectorSD* record = static_cast<SectorSD*>(m_record);
  const uint32_t x = static_cast<uint16_t>(record->m_xPosition);
  const uint32_t y = static_cast<uint16_t>(record->m_yPosition);
  *out = x | (y << 16);
}

SectorIterator::SectorIterator(GameObjNotifier* parent, uint32_t stateFilter)
: ChildIterator(parent, &family::kSectors, stateFilter)
{
}

SectorIterator::~SectorIterator() = default;
