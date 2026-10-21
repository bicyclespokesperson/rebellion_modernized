#include "economy/facility.h"

#include <new>

#include "core/checked_new.h"
#include "data/sd_record_bases.h"
#include "data/sd_records.h"
#include "gameobj/game_obj_iterator.h"
#include "gameobj/object_family.h"

extern EventKindSet g_facilityEventKinds; // 0x6b9038
extern int g_facilityEventKindsReady;     // 0x6b9040

namespace
{
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kKindFacilityOrder = 0x200;
constexpr uint32_t kDestroyed = 0x8;
constexpr uint32_t kLocked = 0x4000;
constexpr uint32_t kAlwaysFilter = 4;
constexpr FamilyRange kFacilityFamilies(0x20, 0x30);

SdRecordExt1* recordOf(Facility* facility)
{
  return static_cast<SdRecordExt1*>(facility->m_record);
}
} // namespace

Facility::Facility() = default;

Facility::~Facility() = default;

int Facility::GetEventKinds(int unused, EventKindSet* out)
{
  const int result = BuildEventKinds();
  if (result) {
    *out = g_facilityEventKinds;
  }
  return result;
}

int Facility::CheckOrder(int a, int orderKind, StatusCode* out)
{
  *out = StatusCode(kStatusCategory, kStatusNotSupported);
  const int result = BuildEventKinds();
  if (result && g_facilityEventKinds.Contains(orderKind)) {
    *out = StatusCode(kStatusCategory, kStatusOk);
  }
  return result;
}

GameObjData* Facility::CreateData()
{
  return NewOrNull<FacilityData>();
}

FacilityData::FacilityData() = default;

FacilityData::~FacilityData() = default;

int Facility::OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx)
{
  return UnitObjBase::OnControlKindChanged(oldKind, newKind, ctx);
}

int Facility::OnUsableChanged(int newValue, NotifyContext* ctx)
{
  return UnitObjBase::OnUsableChanged(newValue, ctx);
}

int Facility::OnExistingChanged(int newValue, NotifyContext* ctx)
{
  return UnitObjBase::OnExistingChanged(newValue, ctx);
}

uint32_t Facility::GetRecordField58()
{
  return m_record ? recordOf(this)->m_field58 : 0;
}

uint32_t Facility::GetRecordField5c()
{
  if (const auto* record = dynamic_cast<ProductionFacilitySD*>(m_record)) {
    return record->m_processingRate;
  }
  if (const auto* record = dynamic_cast<ManufacturingFacilitySD*>(m_record)) {
    return record->m_processingRate;
  }
  if (const auto* record = dynamic_cast<DefenseFacilitySD*>(m_record)) {
    return record->m_attackStrength;
  }
  return 0;
}

int Facility::ApplyAllImpl(NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    ApplyAllToSideView(ctx);
  }
  return result;
}

int Facility::ValidateImpl()
{
  int consistent = GameObj::Validate();
  GameObjIterator children(this, kAlwaysFilter);
  children.Begin();
  while (children.Current() && consistent != 0) {
    consistent = 0;
    children.Advance();
  }
  GameObjNotifier* parent = nullptr;
  int result = 0;
  if (consistent != 0) {
    result = GetParent(&parent);
    if (result && (!(m_stateFlags & kDestroyed) || !(m_stateFlags & kLocked))) {
      result = (~static_cast<GameObj*>(parent)->m_stateFlags >> 3) & 1;
    }
  }
  return result;
}

int Facility::BuildEventKinds()
{
  if (g_facilityEventKindsReady != 0) {
    return 1;
  }
  g_facilityEventKindsReady = 1;
  int result = UnitObjBase::InitEventKinds(0, &g_facilityEventKinds) && g_facilityEventKinds.Add(kKindFacilityOrder, 1);
  g_facilityEventKindsReady = 1;
  return result;
}

int UpdateFacilityChildren(GameObj* object, NotifyContext* ctx)
{
  return object->UpdateChildren(&kFacilityFamilies, ctx);
}

FacilityIterator::FacilityIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kFacilityFamilies, stateFilter)
{
}

FacilityIterator::FacilityIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
    : ChildIterator(parent, side, &kFacilityFamilies, stateFilter)
{
}

FacilityIterator::~FacilityIterator() = default;
EventKindSet g_facilityEventKinds{};
int g_facilityEventKindsReady{};
