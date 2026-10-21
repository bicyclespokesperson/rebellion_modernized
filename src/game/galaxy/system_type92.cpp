#include "galaxy/system_type92.h"

#include "core/checked_new.h"
#include "economy/production_facility_types.h"
#include "galaxy/system_rules_1e.h"
#include "gameobj/game_hooks.h"
#include "gameobj/object_key.h"

namespace
{

constexpr uint32_t kFamilySystemType92 = 0x92;
constexpr int kSpecialSystemId = 0x121;
constexpr int kAllianceSide = 1;
constexpr int kEmpireSide = 2;
constexpr int kStartingLoyalty = 100;

} // namespace

SystemType92::SystemType92() = default;

SystemType92::~SystemType92() = default;

uint32_t SystemType92::GetFamily()
{
  return kFamilySystemType92;
}

int SystemType92::Read(istream* in)
{
  return this->System::Read(in);
}

int SystemType92::Write(ostream* out)
{
  return this->System::Write(out);
}

int SystemType92::Checksum(Crc32* crc)
{
  return this->System::Checksum(crc);
}

int SystemType92::Validate()
{
  return this->System::Validate();
}

int SystemType92::ApplyAll(NotifyContext* ctx)
{
  if (IsSideView()) {
    return ApplyAllImpl(ctx);
  }
  return 0;
}

int SystemType92::ApplyAllImpl(NotifyContext* ctx)
{
  return this->System::ApplyAll(ctx);
}

int SystemType92::CreateViews(int controlKind, NotifyContext* ctx)
{
  return CreateViewsImpl(controlKind, ctx);
}

int SystemType92::CreateViewsImpl(int controlKind, NotifyContext* ctx)
{
  return this->System::CreateViews(controlKind, ctx);
}

GameObj* SystemType92::CreateViewObject()
{
  return NewOrNull<SystemType92>();
}

GameObjNotifier* SystemType92::Create()
{
  return NewOrNull<SystemType92>();
}

int SystemType92::InitializeNewObject(intptr_t a)
{
  auto* ctx = reinterpret_cast<NotifyContext*>(a);
  const int result = System::InitializeNewObject(a);
  if (!result) {
    return 0;
  }

  if (m_id == kSpecialSystemId) {
    SetPopulated(1, ctx);
    SetLoyalty(kStartingLoyalty, ctx);
    NotifyContext empireOnly(*ctx);
    empireOnly.m_flag04 = 1;
    empireOnly.m_flag08 = kEmpireSide;
    SystemView(this, 2)->ApplyControlKind(&empireOnly);
    SystemView(this, 2)->ApplyLoyalty(&empireOnly);
    SystemView(this, 2)->ApplyPopulated(&empireOnly);
    SystemView(this, 2)->ApplyExplored(&empireOnly);
    SetEnergy(RollSystemEnergyB(), ctx);
  } else if (ControlSide() == kAllianceSide) {
    SetPopulated(1, ctx);
    SetLoyalty(kStartingLoyalty, ctx);
    SetEnergy(RollSystemEnergyB(), ctx);
  } else {
    SetPopulated(RollSystemPopulatedB(), ctx);
    if (m_flags & system_flag::kPopulated) {
      SetLoyalty(RollCenteredPercent(), ctx);
    }
    SetEnergy(RollSystemEnergyB(), ctx);
  }
  SetRawMaterial(RollSystemRawMaterialB(m_energy), ctx);
  return result;
}

int SystemType92::CreateStartingUnits(NotifyContext* ctx)
{
  int result = 1;
  if (m_flags & system_flag::kPopulated) {
    const int count = m_energy;
    const int rawMaterial = m_rawMaterial;
    for (int i = 0; i < count; ++i) {
      int created;
      NullableKey unitKey;
      result = RollStartingUnitB(rawMaterial, CountProductionFacilityType2C(this, ControlSide(), 1), &created,
                                 &unitKey);
      if (result && created) {
        ObjectKey key;
        key.m_value = unitKey.m_value;
        result = CreateChild(&key, ControlSide(), ctx, 1, 1) != nullptr;
      }
      if (!result) {
        break;
      }
    }
  }
  return result;
}
