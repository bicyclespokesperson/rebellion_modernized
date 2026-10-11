#include <new>

#include "data/dat_record.h"
#include "economy/manu_mgr.h"
#include "economy/manufacturing_facility.h"
#include "gameobj/object_database.h"
#include "gameobj/object_type_db.h"

extern const char* g_emptyText; // 0x6b120c
extern int g_manuMgrEventKindsReady; // 0x6b9008
extern ObjectKeyValueSet g_manuMgrEmptyQueue; // 0x6b9010
extern EventKindSet g_manuMgrEventKinds; // 0x6b9018

namespace
{

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kKindEvent213 = 0x213;
constexpr int kKindEvent214 = 0x214;
constexpr int kKindEvent216 = 0x216;
constexpr int kDefaultEventKindValue = 1;
constexpr uint32_t kViewBits = 0x30;
constexpr uint32_t kControlBits = 0xc0;
constexpr uint32_t kUsable = 0x1;
constexpr uint32_t kCreated = 0x2;
constexpr uint32_t kFinishedBits = 0x1c;
constexpr int kWriteNameSeedRecord = 5;
constexpr int kWriteNameQueueLength = 6;

} // namespace

// ---- construction -----------------------------------------------------------------------------------------

ManuMgr::ManuMgr()
: GameObj(),
  m_remainingGameObjCount(0),
  m_completedPointCount(0),
  m_overflowPointCount(0),
  m_requiredPointCount(0),
  m_totalRequiredPointCount(0),
  m_reserved(0),
  m_productName(g_emptyText)
{
  m_seedKey.m_value = 0;
}

ManuMgr::~ManuMgr() = default;

GameObjData* ManuMgr::CreateData()
{
  void* memory = operator new(sizeof(ManuMgrData));
  if (!memory)
    return nullptr;
  return new (memory) ManuMgrData();
}

// ---- names ---------------------------------------------------------------------------------------------------

void AppendManuMgrName(ObjectBase* base, std::string* out, int kind)
{
  ManuMgr* self = static_cast<ManuMgr*>(static_cast<GameObj*>(base));
  if (kind == kWriteNameSeedRecord)
  {
    ObjectKey key;
    self->GetSeedKey(&key);
    DatRecord* record = FindDataRecord(AsLookupKey(&key));
    if (record)
      *out += record->m_name;
    return;
  }
  if (kind != kWriteNameQueueLength)
  {
    self->GameObj::WriteName(out, kind);
    return;
  }
  out->append(std::to_string(self->m_remainingGameObjCount));
}

void ManuMgr::WriteName(std::string* out, int kind)
{
  AppendManuMgrName(this, out, kind);
}

ObjectKey* ManuMgr::GetSeedKey(ObjectKey* out)
{
  out->m_value = m_seedKey.m_value;
  return out;
}

// ---- event kinds ---------------------------------------------------------------------------------------------

int ManuMgr::EnsureEventKinds()
{
  int result = 1;
  if (g_manuMgrEventKindsReady == 0)
  {
    g_manuMgrEventKindsReady = 1;
    result = ::InitEventKinds(0, &g_manuMgrEventKinds) &&
             g_manuMgrEventKinds.Add(kKindEvent213, kDefaultEventKindValue) &&
             g_manuMgrEventKinds.Add(kKindEvent214, kDefaultEventKindValue) &&
             g_manuMgrEventKinds.Add(kKindEvent216, kDefaultEventKindValue);
    g_manuMgrEventKindsReady = 1;
  }
  return result;
}

int ManuMgr::GetEventKinds(int unused, EventKindSet* out)
{
  const int result = EnsureEventKinds();
  if (result)
    *out = g_manuMgrEventKinds;
  return result;
}

int ManuMgr::InitEventKinds(int unused, EventKindSet* out)
{
  const int result = EnsureEventKinds();
  *out = g_manuMgrEventKinds;
  return result;
}

int ManuMgr::CheckOrder(int a, int orderKind, StatusCode* out)
{
  *out = StatusCode(kStatusCategory, kStatusNotSupported);
  const int result = EnsureEventKinds();
  if (result && g_manuMgrEventKinds.Contains(orderKind))
    *out = StatusCode(kStatusCategory, kStatusOk);
  return result;
}

// ---- queue and facilities --------------------------------------------------------------------------------------

ObjectKeyValueSet* ManuMgr::GetQueue()
{
  if ((m_flags & kViewBits) == 0)
    return &static_cast<ManuMgrData*>(m_data)->m_queue;
  return &g_manuMgrEmptyQueue;
}

int ManuMgr::GetQueueCopy(ObjectKeyValueSet* out)
{
  const int isMaster = (m_flags & kViewBits) == 0;
  if (isMaster)
    *out = static_cast<ManuMgrData*>(m_data)->m_queue;
  return isMaster;
}

int ManuMgr::IsQueued(const ObjectKey* key)
{
  int queued = 0;
  if ((m_flags & kViewBits) == 0)
    queued = static_cast<ManuMgrData*>(m_data)->m_queue.Find(key) != nullptr;
  return queued;
}

int ManuMgr::IsQueueLengthConsistent()
{
  const int expected = m_remainingGameObjCount;
  const int length = static_cast<ManuMgrData*>(m_data)->m_queue.Count() & 0xffff;
  return length == expected;
}

GameObjNotifier* ManuMgr::GetHostSystem()
{
  return m_parent;
}

GameObjNotifier* ManuMgr::GetProduct()
{
  ObjectKey key(m_productKey);
  if (!key.IsSet())
    return nullptr;
  ObjectKey lookupKey(m_productKey);
  return GetMasterObject(&lookupKey);
}

int ManuMgr::IsAcceptableProduct(GameObj* product)
{
  if ((product->m_stateFlags & kCreated) && !(product->m_stateFlags & kFinishedBits))
  {
    ObjectKey self;
    GetKey(&self);
    ObjectKey builder;
    product->GetBuilderKey(&builder);
    return builder.m_value == self.m_value;
  }
  return 0;
}

int ManuMgr::IsCompatibleFacility(GameObj* facility)
{
  if (!facility)
    return 0;
  const int facilityKind = static_cast<ManufacturingFacility*>(facility)->GetManufacturingKind();
  const int myKind = GetFacilityKind();
  if (facilityKind != myKind || !(facility->m_stateFlags & kUsable))
    return 0;
  if (((m_flags ^ facility->m_flags) & kControlBits) != 0)
    return 0;
  return 1;
}

int ManuMgr::CountHostFacilities(int includeOutOfService)
{
  GameObjNotifier* host = GetHostSystem();
  int count = 0;
  if (host)
  {
    ManufacturingFacilityIterator facilities(host);
    facilities.Begin();
    while (facilities.Current())
    {
      GameObjNotifier* facility = facilities.Current();
      if (IsCompatibleFacility(static_cast<GameObj*>(facility)) &&
          ((AsProcFacil(facility)->m_procFlags & 1) == 0 || includeOutOfService != 0))
      {
        ++count;
      }
      facilities.Advance();
    }
  }
  return count;
}

int ManuMgr::HasHostFacility(int includeOutOfService)
{
  GameObjNotifier* host = GetHostSystem();
  int found = 0;
  if (host)
  {
    ManufacturingFacilityIterator facilities(host);
    facilities.Begin();
    do
    {
      if (!facilities.Current())
        break;
      GameObjNotifier* facility = facilities.Current();
      if (!IsCompatibleFacility(static_cast<GameObj*>(facility)))
      {
        found = 0;
      }
      else if ((AsProcFacil(facilities.Current())->m_procFlags & 1) != 0 && includeOutOfService == 0)
      {
        found = 0;
      }
      else
      {
        found = 1;
      }
      facilities.Advance();
    } while (found == 0);
  }
  return found;
}

int UpdateManuMgrChildren(GameObj* parent, NotifyContext* ctx)
{
  const FamilyRange range(0xa0, 0xb0);
  return parent->UpdateChildren(&range, ctx);
}

// ---- iterator ------------------------------------------------------------------------------------------------

ManuMgrIterator::ManuMgrIterator(GameObjNotifier* parent) : ChildIterator(parent, &family::kManufacturingManagers, 1)
{
}

ManuMgrIterator::ManuMgrIterator(GameObjNotifier* parent, uint32_t stateFilter)
: ChildIterator(parent, &family::kManufacturingManagers, stateFilter)
{
}

ManuMgrIterator::~ManuMgrIterator() = default;
EventKindSet g_manuMgrEventKinds{};
ObjectKeyValueSet g_manuMgrEmptyQueue{};
ObjectTypeDb* g_objectTypeDb{};
int g_manuMgrEventKindsReady{};
