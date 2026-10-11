#include "galaxy/system_type90.h"

#include "core/checked_new.h"
#include "economy/production_facility_types.h"
#include "galaxy/system_rules_1e.h"
#include "gameobj/game_hooks.h"
#include "gameobj/object_family.h"
#include "gameobj/object_key.h"

namespace
{

constexpr FamilyRange kType90Families(0x90, 0x92);
constexpr uint32_t kFamilySystemType90 = 0x90;
constexpr int kSystemIdWithoutLoyaltyRoll = 0x109;
constexpr int kAllSides = 3;

void MakeAllViewsContext(NotifyContext* context)
{
  context->m_flag04 = 1;
  context->m_flag08 = kAllSides;
}

} // namespace

SystemType90::SystemType90() = default;

SystemType90::~SystemType90() = default;

uint32_t SystemType90::GetFamily()
{
  return kFamilySystemType90;
}

int SystemType90::Read(istream* in)
{
  return ReadImpl(in);
}

int SystemType90::Write(ostream* out)
{
  return WriteImpl(out);
}

int SystemType90::Checksum(Crc32* crc)
{
  return ChecksumImpl(crc);
}

int SystemType90::Validate()
{
  return ValidateImpl();
}

int SystemType90::ApplyAll(NotifyContext* ctx)
{
  const int isSideView = IsSideView();
  if (isSideView)
    return ApplyAllImpl(ctx);
  return isSideView;
}

int SystemType90::ReadImpl(istream* in)
{
  return this->System::Read(in);
}

int SystemType90::WriteImpl(ostream* out)
{
  return this->System::Write(out);
}

int SystemType90::ApplyAllImpl(NotifyContext* ctx)
{
  const int isSideView = IsSideView();
  if (isSideView)
    this->System::ApplyAll(ctx);
  return isSideView;
}

int SystemType90::ChecksumImpl(Crc32* crc)
{
  return this->System::Checksum(crc);
}

int SystemType90::ValidateImpl()
{
  return this->System::Validate();
}

int SystemType90::CreateViews(int controlKind, NotifyContext* ctx)
{
  return this->System::CreateViews(controlKind, ctx);
}

GameObj* SystemType90::CreateViewObject()
{
  return NewOrNull<SystemType90>();
}

GameObjNotifier* SystemType90::Create()
{
  return NewOrNull<SystemType90>();
}

int SystemType90::Virtual47(intptr_t a)
{
  NotifyContext* ctx = reinterpret_cast<NotifyContext*>(a);
  const int result = System::Virtual47(a);
  if (result)
  {
    const int populated = RollSystemPopulated();
    int loyalty = 0;
    if (populated && m_id != kSystemIdWithoutLoyaltyRoll)
    {
      const uint32_t side = ControlSide();
      loyalty = RollInitialLoyalty(side, this->System::GetDataFlagBit0());
    }
    SetPopulated(populated, ctx);
    if (populated)
      SetLoyalty(loyalty, ctx);
    SetEnergy(RollSystemEnergy(), ctx);
    SetRawMaterial(RollSystemRawMaterial(m_energy), ctx);
    SetNeverBeenControlled(0, ctx);
  }
  return result;
}

int SystemType90::ApplyAllToViewOf(int view, int unused, NotifyContext* ctx)
{
  int ok = System::ApplyAllToViewOf(view, unused, ctx) != 0;
  ok = this->System::SyncViewForEvent(0xc, 1, ctx) && ok;
  ok = this->System::SyncViewForEvent(0xc, 2, ctx) && ok;
  ok = this->System::SyncViewForEvent(4, 1, ctx) && ok;
  return this->System::SyncViewForEvent(4, 2, ctx) && ok ? 1 : 0;
}

int SystemType90::Virtual115(NotifyContext* ctx)
{
  int result = 1;
  const int count = m_energy;
  ObjectKey key;
  const int rawMaterial = m_rawMaterial;
  for (int i = 0; i < count; ++i)
  {
    int created;
    result = RollStartingUnitA(rawMaterial, CountProductionFacilityType2C(this, ControlSide(), 1), &created,
                               reinterpret_cast<NullableKey*>(&key));
    if (result && created)
      result = CreateChild(&key, ControlSide(), ctx, 1, 1) != nullptr;
    if (!result)
      break;
  }
  return result;
}

int SystemType90::OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx)
{
  const int result = System::OnControlKindChanged(oldKind, newKind, ctx);
  if (result)
  {
    NotifyContext allViews(*ctx);
    MakeAllViewsContext(&allViews);
    System* first = SystemView(this, 1);
    first->ApplyControlKind(&allViews);
    first->ApplyLoyaltyCausedCurrentControlKind(&allViews);
    System* second = SystemView(this, 2);
    second->ApplyControlKind(&allViews);
    second->ApplyLoyaltyCausedCurrentControlKind(&allViews);
  }
  return result;
}

int SystemType90::OnLoyaltyChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  const int result = System::OnLoyaltyChanged(oldValue, newValue, ctx);
  if (result)
  {
    NotifyContext allViews(*ctx);
    MakeAllViewsContext(&allViews);
    SystemView(this, 1)->ApplyLoyalty(&allViews);
    SystemView(this, 2)->ApplyLoyalty(&allViews);
  }
  return result;
}

int SystemType90::OnPopulatedChanged(int newValue, NotifyContext* ctx)
{
  const int result = System::OnPopulatedChanged(newValue, ctx);
  if (result)
  {
    NotifyContext allViews(*ctx);
    MakeAllViewsContext(&allViews);
    SystemView(this, 1)->ApplyPopulated(&allViews);
    SystemView(this, 2)->ApplyPopulated(&allViews);
  }
  return result;
}

int SystemType90::OnUprisingChanged(int newValue, NotifyContext* ctx)
{
  const int result = System::OnUprisingChanged(newValue, ctx);
  if (result)
  {
    NotifyContext allViews(*ctx);
    MakeAllViewsContext(&allViews);
    SystemView(this, 1)->ApplyUprising(&allViews);
    SystemView(this, 2)->ApplyUprising(&allViews);
  }
  return result;
}

int UpdateSystemType90Children(GameObj* parent, NotifyContext* ctx)
{
  return parent->UpdateChildren(&kType90Families, ctx);
}

SystemType90Iterator::SystemType90Iterator(GameObjNotifier* parent, uint32_t stateFilter)
: ChildIterator(parent, &kType90Families, stateFilter)
{
}

SystemType90Iterator::~SystemType90Iterator() = default;
