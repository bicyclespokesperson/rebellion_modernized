#include "core/checked_new.h"
#include "core/utility_classes.h"
#include "data/dat_record.h"
#include "economy/side.h"
#include "economy/side_decl.h"
#include "gameobj/object_family.h"
#include "gameobj/object_queries.h"

namespace
{

constexpr uint32_t kControlSideMask = 0xc0;
constexpr uint32_t kExistingFlag = 0x40;
constexpr uint32_t kUsableFlag = 0x1;
constexpr uint32_t kCompletedFlag = 0x4;
constexpr uint32_t kProcFacilExcludedFlag = 0x1;
constexpr int kAllianceSideKeyId = 0xf;
constexpr int kEmpireSideKeyId = 0x10;

bool sameControlSide(const GameObjRoot* a, const GameObjRoot* b)
{
  return ((a->m_flags ^ b->m_flags) & kControlSideMask) == 0;
}

bool sameValue(const IntPair& a, const IntPair& b)
{
  return a.m_first == b.m_first && a.m_second == b.m_second;
}

void addPair(IntPair* sum, const IntPair& other)
{
  sum->m_first += other.m_first;
  sum->m_second += other.m_second;
}

int readPair(istream* in, IntPair* pair)
{
  ReadValue32(in, &pair->m_first);
  ReadValue32(in, &pair->m_second);
  return 1;
}

int writePair(ostream* out, const IntPair* pair)
{
  WriteValue32(out, &pair->m_first);
  WriteValue32(out, &pair->m_second);
  return 1;
}

int checksumPair(Crc32* crc, const IntPair* pair)
{
  crc->AddInt(static_cast<int>(pair->m_first));
  crc->AddInt(static_cast<int>(pair->m_second));
  return 1;
}

ProductionFacility* asFacility(GameObjNotifier* object)
{
  return static_cast<ProductionFacility*>(static_cast<GameObj*>(object));
}

} // namespace

// ---- ObjectNameList -----------------------------------------------------------------------------------------

ObjectNameList::ObjectNameList() : m_ok(1)
{
}

ObjectNameList::~ObjectNameList()
{
  Clear();
}

// ---- construction -------------------------------------------------------------------------------------------

Side::Side() : GameObj(), m_maintState(), m_mineState(), m_refineryState(), m_statCc(), m_statD4()
{
  SetFlag31(GetFlag31() ? InitState() : 0);
}

Side::~Side()
{
  FreeState();
}

GameObjNotifier* CreateSide()
{
  return new Side();
}

GameObj* Side::CreateViewObject()
{
  return new Side();
}

uint32_t Side::GetFamily()
{
  return family::kSide;
}

int Side::InitState()
{
  m_maintState = IntPair();
  m_mineState = IntPair();
  m_refineryState = IntPair();
  m_maintRequired = 0;
  m_maintRequiredB = 0;
  m_rawMaterials = 0;
  m_refinedMaterials = 0;
  m_waitingRefineries = 0;
  m_waitingManufacturers = 0;
  m_refineryQueue = nullptr;
  m_manufacturerQueue = nullptr;
  m_shipyardRdProgress = 0;
  m_trainingFacilRdProgress = 0;
  m_constructionYardRdProgress = 0;
  m_shipyardRdOrder = 0;
  m_trainingFacilRdOrder = 0;
  m_constructionYardRdOrder = 0;
  m_maintDeficit = 0;
  m_shipyardRdDone = 0;
  m_trainingFacilRdDone = 0;
  m_constructionYardRdDone = 0;
  m_recruitmentDone = 0;
  m_victoryConditions = 0;
  m_rdScalePercent = 100;
  m_unkC4 = 1;
  m_objectRefs = nullptr;
  m_statCc = TrackedStat();
  m_statD4 = TrackedStat();

  m_refineryQueue = new KeyQueue();
  int ok = 0;
  if (m_refineryQueue && m_refineryQueue->m_ok)
  {
    m_manufacturerQueue = new KeyQueue();
    if (m_manufacturerQueue && m_manufacturerQueue->m_ok)
    {
      m_objectRefs = new ObjectRefList();
      if (m_objectRefs)
        ok = m_objectRefs->m_ok;
    }
  }
  return ok;
}

int Side::FreeState()
{
  if (m_refineryQueue)
  {
    delete m_refineryQueue;
    m_refineryQueue = nullptr;
  }
  if (m_manufacturerQueue)
  {
    delete m_manufacturerQueue;
    m_manufacturerQueue = nullptr;
  }
  if (m_objectRefs)
  {
    delete m_objectRefs;
    m_objectRefs = nullptr;
  }
  return 1;
}

int Side::HasContainers() const
{
  return m_refineryQueue && m_manufacturerQueue && m_objectRefs ? 1 : 0;
}

// ---- views --------------------------------------------------------------------------------------------------

int Side::CreateViews(int controlKind, NotifyContext* ctx)
{
  return CreateViewsImpl(controlKind, ctx);
}

int Side::CreateViewsImpl(int controlKind, NotifyContext* ctx)
{
  const bool valid = controlKind >= 1 && controlKind <= 3;
  if (valid)
    SetId(controlKind == 1 ? kAllianceSideKeyId : kEmpireSideKeyId);
  int result = 0;
  if (valid)
    result = GameObj::CreateViews(controlKind, ctx);
  if (result)
    SetControlKind(controlKind, ctx);
  return result;
}

int Side::ApplyAll(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
    result = ApplyAllImpl(ctx);
  return result;
}

int Side::ApplyAllImpl(NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
  {
    GameObj::ApplyAll(ctx);
    if (ctx->m_flag04 == 1)
    {
      ApplyMaintState(ctx);
      ApplyMineState(ctx);
      ApplyRefineryState(ctx);
      ApplyMaintRequired(ctx);
      ApplyMaintRequiredB(ctx);
      ApplyRawMaterials(ctx);
      ApplyRefinedMaterials(ctx);
      ApplyWaitingRefineries(ctx);
      ApplyWaitingManufacturers(ctx);
      ApplyShipyardRdProgress(ctx);
      ApplyTrainingFacilRdProgress(ctx);
      ApplyConstructionYardRdProgress(ctx);
      ApplyShipyardRdOrder(ctx);
      ApplyTrainingFacilRdOrder(ctx);
      ApplyConstructionYardRdOrder(ctx);
      ApplyMaintDeficit(ctx);
      ApplyShipyardRdDone(ctx);
      ApplyTrainingFacilRdDone(ctx);
      ApplyConstructionYardRdDone(ctx);
      ApplyRecruitmentDone(ctx);
      ApplyVictoryConditions(ctx);
    }
    else if (ctx->m_flag04 == 2)
    {
      ApplyParent(ctx);
      ApplyControlKind(ctx);
    }
  }
  return result;
}

// ---- persistence --------------------------------------------------------------------------------------------

int Side::Read(istream* in)
{
  return ReadImpl(in);
}

int Side::Write(ostream* out)
{
  return WriteImpl(out);
}

int Side::Checksum(Crc32* crc)
{
  return ChecksumImpl(crc);
}

int Side::Validate()
{
  return ValidateImpl();
}

int Side::ReadImpl(istream* in)
{
  if (!GameObj::Read(in))
    return 0;
  if (!FreeState())
    return 0;
  if (!InitState())
    return 0;
  if (!readPair(in, &m_maintState))
    return 0;
  if (!readPair(in, &m_mineState))
    return 0;
  if (!readPair(in, &m_refineryState))
    return 0;
  ReadValue32(in, &m_maintRequired);
  ReadValue32(in, &m_maintRequiredB);
  ReadValue32(in, &m_rawMaterials);
  ReadValue32(in, &m_refinedMaterials);
  ReadValue32(in, &m_waitingRefineries);
  ReadValue32(in, &m_waitingManufacturers);
  if (!m_refineryQueue->Read(in))
    return 0;
  if (m_refineryQueue->m_ok == 0)
    return 0;
  if (!m_manufacturerQueue->Read(in))
    return 0;
  if (m_manufacturerQueue->m_ok == 0)
    return 0;
  ReadValue32(in, &m_shipyardRdProgress);
  ReadValue32(in, &m_trainingFacilRdProgress);
  ReadValue32(in, &m_constructionYardRdProgress);
  ReadValue32(in, &m_shipyardRdOrder);
  ReadValue32(in, &m_trainingFacilRdOrder);
  ReadValue32(in, &m_constructionYardRdOrder);
  ReadValue32(in, &m_maintDeficit);
  ReadValue32(in, &m_shipyardRdDone);
  ReadValue32(in, &m_trainingFacilRdDone);
  ReadValue32(in, &m_constructionYardRdDone);
  ReadValue32(in, &m_recruitmentDone);
  ReadValue32(in, &m_victoryConditions);
  ReadValue32(in, &m_rdScalePercent);
  ReadValue32(in, &m_unkC4);
  if (!m_objectRefs->Read(in))
    return 0;
  if (m_objectRefs->m_ok == 0)
    return 0;
  if (!m_statCc.Read(in))
    return 0;
  return m_statD4.Read(in);
}

int Side::WriteImpl(ostream* out)
{
  if (!GameObj::Write(out))
    return 0;
  if (!HasContainers())
    return 0;
  if (!writePair(out, &m_maintState))
    return 0;
  if (!writePair(out, &m_mineState))
    return 0;
  if (!writePair(out, &m_refineryState))
    return 0;
  WriteValue32(out, &m_maintRequired);
  WriteValue32(out, &m_maintRequiredB);
  WriteValue32(out, &m_rawMaterials);
  WriteValue32(out, &m_refinedMaterials);
  WriteValue32(out, &m_waitingRefineries);
  WriteValue32(out, &m_waitingManufacturers);
  if (!m_refineryQueue->Write(out))
    return 0;
  if (!m_manufacturerQueue->Write(out))
    return 0;
  WriteValue32(out, &m_shipyardRdProgress);
  WriteValue32(out, &m_trainingFacilRdProgress);
  WriteValue32(out, &m_constructionYardRdProgress);
  WriteValue32(out, &m_shipyardRdOrder);
  WriteValue32(out, &m_trainingFacilRdOrder);
  WriteValue32(out, &m_constructionYardRdOrder);
  WriteValue32(out, &m_maintDeficit);
  WriteValue32(out, &m_shipyardRdDone);
  WriteValue32(out, &m_trainingFacilRdDone);
  WriteValue32(out, &m_constructionYardRdDone);
  WriteValue32(out, &m_recruitmentDone);
  WriteValue32(out, &m_victoryConditions);
  WriteValue32(out, &m_rdScalePercent);
  WriteValue32(out, &m_unkC4);
  if (!m_objectRefs->Write(out))
    return 0;
  if (!m_statCc.Write(out))
    return 0;
  return m_statD4.Write(out);
}

int Side::ChecksumImpl(Crc32* crc)
{
  if (!GameObj::Checksum(crc))
    return 0;
  if (!HasContainers())
    return 0;
  if (!checksumPair(crc, &m_maintState))
    return 0;
  if (!checksumPair(crc, &m_mineState))
    return 0;
  if (!checksumPair(crc, &m_refineryState))
    return 0;
  crc->AddInt(m_maintRequired);
  crc->AddInt(m_maintRequiredB);
  crc->AddInt(m_rawMaterials);
  crc->AddInt(m_refinedMaterials);
  crc->AddInt(m_waitingRefineries);
  crc->AddInt(m_waitingManufacturers);
  if (!m_refineryQueue->Checksum(crc))
    return 0;
  if (!m_manufacturerQueue->Checksum(crc))
    return 0;
  crc->AddInt(m_shipyardRdProgress);
  crc->AddInt(m_trainingFacilRdProgress);
  crc->AddInt(m_constructionYardRdProgress);
  crc->AddInt(m_shipyardRdOrder);
  crc->AddInt(m_trainingFacilRdOrder);
  crc->AddInt(m_constructionYardRdOrder);
  crc->AddInt(m_maintDeficit);
  crc->AddInt(m_shipyardRdDone);
  crc->AddInt(m_trainingFacilRdDone);
  crc->AddInt(m_constructionYardRdDone);
  crc->AddInt(m_recruitmentDone);
  crc->AddInt(m_victoryConditions);
  crc->AddInt(m_rdScalePercent);
  crc->AddInt(m_unkC4);
  if (!m_objectRefs->Checksum(crc))
    return 0;
  if (!m_statCc.Checksum(crc))
    return 0;
  return m_statD4.Checksum(crc);
}

int Side::ValidateImpl()
{
  int result = GameObj::Validate();
  if (result)
    result = HasContainers();
  if (result)
    result = MineStateMatches();
  if (result)
    result = RefineryStateMatches();
  if (result)
    result = MaintRequiredMatches();
  if (result)
    result = MaintRequiredBMatches();
  if (result)
    result = QueuesMatchFacilities();
  return result;
}

// ---- consistency checks -------------------------------------------------------------------------------------

int Side::CountQueuedA(KeyQueueNode* probe)
{
  return m_refineryQueue ? m_refineryQueue->CountMatching(probe) : 0;
}

int Side::CountQueuedB(KeyQueueNode* probe)
{
  return m_manufacturerQueue ? m_manufacturerQueue->CountMatching(probe) : 0;
}

namespace
{

int stateMatchesSum(Side* side, GameObjNotifier* first, const FamilyRange& range, const IntPair& state)
{
  IntPair sum;
  for (GameObjNotifier* object = first; object; object = static_cast<GameObj*>(object)->FindNextSibling(&range, 1))
  {
    ProductionFacility* facility = asFacility(object);
    if ((facility->m_stateFlags & kUsableFlag) && !(facility->m_procFlags & kProcFacilExcludedFlag) &&
        sameControlSide(facility, side))
    {
      IntPair pair;
      facility->GetWork(&pair);
      addPair(&sum, pair);
    }
  }
  return sameValue(state, sum) ? 1 : 0;
}

} // namespace

int Side::MineStateMatches()
{
  return stateMatchesSum(this, GetFirstMasterInFamilies_2c_2d(), FamilyRange(0x2c, 0x2d), m_mineState);
}

int Side::RefineryStateMatches()
{
  return stateMatchesSum(this, GetFirstMasterInFamilies_2d_2e(), FamilyRange(0x2d, 0x2e), m_refineryState);
}

GameObjNotifier* FindNextUnit(GameObjNotifier* unit)
{
  const FamilyRange units(0x10, 0x40);
  return static_cast<GameObj*>(unit)->FindNextSibling(&units, 1);
}

namespace
{

int maintSumEquals(Side* side, int expected)
{
  int total = 0;
  for (GameObjNotifier* object = GetFirstMasterInFamilies_10_40(); object; object = FindNextUnit(object))
  {
    GameObj* unit = static_cast<GameObj*>(object);
    if ((unit->m_stateFlags & kExistingFlag) && sameControlSide(unit, side))
    {
      total += static_cast<UnitObjBase*>(unit)->GetRecordField4c();
    }
  }
  return expected == total ? 1 : 0;
}

} // namespace

int Side::MaintRequiredMatches()
{
  return maintSumEquals(this, m_maintRequired);
}

int Side::MaintRequiredBMatches()
{
  return maintSumEquals(this, m_maintRequiredB);
}

int Side::QueuesMatchFacilities()
{
  bool ok = true;
  const FamilyRange refineries(0x2d, 0x2e);
  for (GameObjNotifier* object = GetFirstMasterInFamilies_2d_2e(); object && ok;
       object = static_cast<GameObj*>(object)->FindNextSibling(&refineries, 1))
  {
    ProductionFacility* facility = asFacility(object);
    int queued;
    int waiting;
    {
      KeyQueueNode node;
      facility->GetKey(&node.m_key);
      node.m_value = facility->GetCycleCount();
      queued = CountQueuedA(&node);
      waiting = facility->m_state == 1 && sameControlSide(facility, this) ? 1 : 0;
    }
    ok = waiting == queued;
  }
  const FamilyRange manufacturers(0x28, 0x2c);
  for (GameObjNotifier* object = GetFirstMasterInFamilies_28_2c(); object && ok;
       object = static_cast<GameObj*>(object)->FindNextSibling(&manufacturers, 1))
  {
    ProductionFacility* facility = asFacility(object);
    int queued;
    int waiting;
    {
      KeyQueueNode node;
      facility->GetKey(&node.m_key);
      node.m_value = facility->GetCycleCount();
      queued = CountQueuedB(&node);
      waiting = facility->m_state == 1 && sameControlSide(facility, this) ? 1 : 0;
    }
    ok = waiting == queued;
  }
  return ok;
}

// ---- helpers ------------------------------------------------------------------------------------------------

int SideNumberFromKey(const ObjectKey* key)
{
  if ((key->m_value & 0xffffff) == kAllianceSideKeyId)
    return 1;
  if ((key->m_value & 0xffffff) != kEmpireSideKeyId)
    return 0;
  return 2;
}

int FormatVictoryConditions(uint32_t conditions, std::string* out)
{
  struct Entry
  {
    uint32_t bit;
    const char* name;
  };
  static const Entry entries[] = {{0x1, "AD "},       {0x10, "LC "},      {0x20, "MC "},   {0x100, "CA "},
                                  {0x200, "CD "},     {0x1000, "DC "},    {0x2000, "EC "}, {0x100000, "EW "},
                                  {0x1000000, "AW "}, {0x10000000, "GO "}};
  out->clear();
  for (const Entry& entry : entries)
  {
    const std::string text((conditions & entry.bit) ? entry.name : "   ");
    *out += text;
  }
  return 1;
}

// ---- the secondary vtable's name function ---------------------------------------------------------------

namespace
{

// The name of the Side: its owner's name for "player" style requests, with an article for kinds 0x20/0x21.
// The resource ids come in pairs (Alliance, Empire) per kind.
int nameResourceFor(int kind)
{
  switch (kind)
  {
    case 1:
    case 0x20:
      return 0x4d50;
    case 2:
    case 0x21:
      return 0x4d60;
    case 4:
      return 0x4d70;
    case 0x10:
      return 0x4d80;
    case 0x11:
      return 0x4d82;
    case 0x12:
      return 0x4d84;
    case 0x13:
      return 0x4d86;
    case 0x14:
      return 0x4d88;
    case 0x15:
      return 0x4d8a;
    case 0x16:
      return 0x4d8c;
    case 0x17:
      return 0x4d8e;
    case 0x18:
      return 0x4d90;
    case 0x19:
      return 0x4d92;
    default:
      return 0;
  }
}

constexpr uint32_t kAllianceControl = 0x40;
constexpr int kArticleAnResource = 0x4da1;
constexpr int kArticleAResource = 0x4da2;

} // namespace

// The standalone's own vtables reach the free function through the override; the hybrid's original vtables point at it directly.
void Side::WriteName(std::string* out, int kind)
{
  AppendSideName(this, out, kind);
}

void AppendSideName(ObjectBase* base, std::string* out, int kind)
{
  const int baseResource = nameResourceFor(kind);
  if (baseResource == 0)
    return;
  const uint32_t flags = static_cast<GameObj*>(base)->m_flags;
  const int resource = baseResource + ((flags & kControlSideMask) == kAllianceControl ? 0 : 1);
  std::string name;
  if (kind != 0x20 && kind != 0x21)
  {
    ResourceStringId id{static_cast<int16_t>(resource), 2};
    *out += LoadStringResource(&id);
    return;
  }
  ResourceStringId nameId{static_cast<int16_t>(resource), 2};
  (name = LoadStringResource(&nameId));
  const int article = StartsWithNonVowel(name) == 0 ? kArticleAResource : kArticleAnResource;
  ResourceStringId articleId{static_cast<int16_t>(article), 2};
  *out += LoadStringResource(&articleId);
  *out += name;
}
