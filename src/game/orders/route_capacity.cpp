#include "orders/route_capacity.h"

#include "gameobj/child_iterator.h"

namespace
{

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNoSuchKind = 0x19;
constexpr int kStatusDestroyed = 0x12;
constexpr int kStatusNotCreated = 0x1a;
constexpr int kStatusNotAccepted = 0x27;
constexpr int kStatusOverCapacity = 0x29;
constexpr uint32_t kFlagCreated = 0x2;
constexpr uint32_t kFlagDestroyed = 0x8;
constexpr uint32_t kStateFilterAll = 4;
constexpr uint32_t kFirstGlobalFamily = 0xf1;
constexpr uint32_t kEndGlobalFamily = 0xf2;

bool isOk(const StatusCode& status)
{
  return status.m_second == static_cast<uint32_t>(kStatusOk);
}

StatusCode okStatus()
{
  return {kStatusCategory, kStatusOk};
}

bool inRange(uint32_t family, const FamilyRange* range)
{
  return family >= range->m_first && family < range->m_second;
}

bool inAnyBucket(uint32_t family)
{
  bool found = false;
  for (int i = 0; i < kCapacityBuckets; ++i) {
    if (inRange(family, GetCapacityBucketRange(i))) {
      found = true;
    }
  }
  return found;
}

intptr_t rangeArgument(const FamilyRange* range)
{
  return reinterpret_cast<intptr_t>(range);
}

} // namespace

// ---- CapacitySlot ---------------------------------------------------------------------------------------------

CapacitySlot::CapacitySlot()
    : m_unlimited(1)
    , m_remaining(0)
    , m_status(okStatus())
    , m_limitStatus(okStatus())
{
}

CapacitySlot::CapacitySlot(const CapacitySlot& other) = default;

CapacitySlot::~CapacitySlot() = default;

int CapacitySlot::MarkOverCapacity()
{
  m_status = StatusCode(kStatusCategory, kStatusOverCapacity);
  return 1;
}

CapacitySlot& CapacitySlot::operator=(const CapacitySlot& other) = default;

int CapacitySlot::Reset()
{
  m_unlimited = 1;
  m_remaining = 0;
  m_status = okStatus();
  m_limitStatus = okStatus();
  return 1;
}

int CapacitySlot::IsReplacedBy(const CapacitySlot& other) const
{
  if (!isOk(m_status)) {
    return 0;
  }
  if (!isOk(other.m_status)) {
    return 1;
  }
  if (!isOk(m_limitStatus)) {
    return 0;
  }
  if (!isOk(other.m_limitStatus)) {
    return 1;
  }
  if (m_unlimited != 0) {
    return other.m_unlimited == 0;
  }
  if (other.m_unlimited != 0) {
    return 0;
  }
  return other.m_remaining < m_remaining;
}

StatusCode* CapacitySlot::GetError(StatusCode* out) const
{
  if (!isOk(m_status)) {
    *out = m_status;
    return out;
  }
  if (!isOk(m_limitStatus)) {
    *out = m_limitStatus;
    return out;
  }
  out->m_first = kStatusCategory;
  out->m_second = static_cast<uint32_t>(kStatusOk);
  return out;
}

int CapacitySlot::Compute(const CapacitySlot& capacity, const UnitCount& count, int consume)
{
  m_unlimited = 1;
  m_remaining = 0;
  m_status = okStatus();
  m_limitStatus = okStatus();
  if (count.m_counted == 0) {
    return 1;
  }
  if (!isOk(capacity.m_status)) {
    m_status = capacity.m_status;
    return 1;
  }
  if (capacity.m_unlimited != 0) {
    return 1;
  }
  m_unlimited = 0;
  const int delta = consume != 0 ? -count.m_amount : count.m_amount;
  m_remaining = capacity.m_remaining + delta;
  if (m_remaining < 0) {
    m_limitStatus = capacity.m_limitStatus;
  }
  return 1;
}

int CapacitySlot::Commit(CapacitySlot* capacity, const UnitCount& count, int consume)
{
  int result = 1;
  if (count.m_counted == 0) {
    return result;
  }
  result = Compute(*capacity, count, consume);
  if (result == 0) {
    return result;
  }
  if (consume == 0) {
    if (isOk(m_status) && capacity->m_unlimited == 0) {
      capacity->m_remaining = m_remaining;
    }
    return result;
  }
  result = 0;
  if (isOk(m_status)) {
    result = isOk(m_limitStatus);
    if (result != 0 && capacity->m_unlimited == 0) {
      capacity->m_remaining = m_remaining;
    }
  }
  return result;
}

CapacityScratch::CapacityScratch() = default;

CapacityScratch::~CapacityScratch() = default;

// ---- UnitCount ------------------------------------------------------------------------------------------------

UnitCount::UnitCount()
    : m_counted(0)
    , m_amount(0)
{
}

UnitCount::UnitCount(const UnitCount& other) = default;

UnitCount::~UnitCount() = default;

int UnitCount::Reset()
{
  m_counted = 0;
  m_amount = 0;
  return 1;
}

int UnitCount::IsNonNegative() const
{
  return m_amount >= 0;
}

// ---- arrays and tables ----------------------------------------------------------------------------------------

CapacitySlotArray::CapacitySlotArray() = default;

CapacitySlotArray::CapacitySlotArray(const CapacitySlotArray& other)
{
  for (int i = 0; i < kCapacityBuckets; ++i) {
    m_slots[i] = other.m_slots[i];
  }
}

CapacitySlotArray::~CapacitySlotArray() = default;

CapacitySlotArray& CapacitySlotArray::operator=(const CapacitySlotArray& other)
{
  for (int i = 0; i < kCapacityBuckets; ++i) {
    m_slots[i] = other.m_slots[i];
  }
  return *this;
}

int CapacitySlotArray::MarkAllOverCapacity()
{
  for (CapacitySlot& slot : m_slots) {
    slot.MarkOverCapacity();
  }
  return 1;
}

UnitCountArray::UnitCountArray() = default;

UnitCountArray::~UnitCountArray() = default;

int UnitCountArray::ResetAll()
{
  for (UnitCount& count : m_counts) {
    count.Reset();
  }
  return 1;
}

int UnitCountArray::AllNonNegative() const
{
  for (const UnitCount& count : m_counts) {
    if (!count.IsNonNegative()) {
      return 0;
    }
  }
  return 1;
}

CapacityTable::CapacityTable() = default;

CapacityTable::~CapacityTable() = default;

UnitCountTable::UnitCountTable() = default;

UnitCountTable::~UnitCountTable() = default;

int UnitCountTable::ResetAll()
{
  if (m_own.ResetAll()) {
    return m_carried.ResetAll();
  }
  return 0;
}

int UnitCountTable::AllNonNegative() const
{
  if (m_own.AllNonNegative()) {
    return m_carried.AllNonNegative();
  }
  return 0;
}

int UnitCountTable::CountObject(GameObj* object, const int* filter, int invert, StatusCode* status)
{
  *status = okStatus();
  if (isOk(*status) && !inAnyBucket(object->GetFamily())) {
    *status = StatusCode(kStatusCategory, kStatusNoSuchKind);
  }
  if (!isOk(*status)) {
    return 1;
  }
  if (object->m_stateFlags & kFlagDestroyed) {
    *status = StatusCode(kStatusCategory, kStatusDestroyed);
  } else if (!(object->m_stateFlags & kFlagCreated)) {
    *status = StatusCode(kStatusCategory, kStatusNotCreated);
  }
  if (!isOk(*status)) {
    return 1;
  }
  const auto counts = [&](GameObj* unit) {
    const int matches = unit->MatchesStateFilter(*filter);
    return invert != 0 ? !matches : matches != 0;
  };
  for (int i = 0; i < kCapacityBuckets; ++i) {
    const FamilyRange* range = GetCapacityBucketRange(i);
    if (inRange(object->GetFamily(), range)) {
      UnitCount own = m_own.m_counts[i];
      own.m_counted = 1;
      if (counts(object)) {
        ++own.m_amount;
      }
      m_own.m_counts[i].m_counted = own.m_counted;
      m_own.m_counts[i].m_amount = own.m_amount;
    }
    UnitCount carried = m_carried.m_counts[i];
    ChildIterator children(object, GetCapacityBucketRange(i), kStateFilterAll);
    children.Begin();
    while (children.Current()) {
      carried.m_counted = 1;
      if (counts(static_cast<GameObj*>(children.Current()))) {
        ++carried.m_amount;
      }
      children.Advance();
    }
    m_carried.m_counts[i].m_counted = carried.m_counted;
    m_carried.m_counts[i].m_amount = carried.m_amount;
  }
  return 1;
}

int UnitCountTable::AddFamily(uint32_t family, int delta, StatusCode* status)
{
  *status = okStatus();
  if (isOk(*status) && !inAnyBucket(family)) {
    *status = StatusCode(kStatusCategory, kStatusNoSuchKind);
  }
  if (isOk(*status)) {
    for (int i = 0; i < kCapacityBuckets; ++i) {
      if (!inRange(family, GetCapacityBucketRange(i))) {
        continue;
      }
      UnitCount count = m_own.m_counts[i];
      count.m_amount += delta;
      count.m_counted = 1;
      m_own.m_counts[i].m_counted = count.m_counted;
      m_own.m_counts[i].m_amount = count.m_amount;
    }
  }
  return 1;
}

// ---- CapacityLeg ----------------------------------------------------------------------------------------------

CapacityLeg::CapacityLeg()
    : ObjectKeyNode()
    , m_flag(1)
    , m_parentKey()
    , m_table()
{
}

CapacityLeg::~CapacityLeg() = default;

int CapacityLeg::MarkAllIncomingOverCapacity()
{
  return m_table.m_incoming.MarkAllOverCapacity();
}

int CapacityLeg::MinRemaining(const CapacitySlotArray* slots, const UnitCountArray* counts, int consume,
                              CapacitySlot* out)
{
  *out = CapacityScratch();
  int ok = 1;
  CapacityScratch current;
  for (int i = 0; i < kCapacityBuckets && ok; ++i) {
    ok = current.Compute(slots->m_slots[i], counts->m_counts[i], consume);
    if (ok && out->IsReplacedBy(current)) {
      *out = current;
    }
  }
  return ok;
}

int CapacityLeg::CommitAll(CapacitySlotArray* slots, const UnitCountArray* counts, int consume)
{
  for (int i = 0; i < kCapacityBuckets; ++i) {
    CapacityScratch result;
    CapacitySlot working(slots->m_slots[i]);
    const int ok = result.Commit(&working, counts->m_counts[i], consume);
    if (ok) {
      slots->m_slots[i] = working;
    }
    if (!ok) {
      return 0;
    }
  }
  return 1;
}

int CapacityLeg::MinRemainingBoth(const UnitCountTable* counts, int consume, CapacitySlot* tightest,
                                  CapacitySlot* incoming)
{
  *tightest = CapacityScratch();
  *incoming = CapacityScratch();
  CapacityScratch current;
  int ok = MinRemaining(&m_table.m_incoming, &counts->m_own, consume, &current);
  if (!ok) {
    return ok;
  }
  if (tightest->IsReplacedBy(current)) {
    *tightest = current;
  }
  *incoming = current;
  ok = MinRemaining(&m_table.m_outgoing, &counts->m_carried, consume, &current);
  if (ok && tightest->IsReplacedBy(current)) {
    *tightest = current;
  }
  return ok;
}

int CapacityLeg::MinRemainingOutgoing(const UnitCountTable* counts, int consume, CapacitySlot* out)
{
  *out = CapacityScratch();
  CapacityScratch current;
  const int ok = MinRemaining(&m_table.m_outgoing, &counts->m_own, consume, &current);
  if (ok && out->IsReplacedBy(current)) {
    *out = current;
  }
  return ok;
}

int CapacityLeg::CommitBoth(UnitCountTable* counts, int consume)
{
  CapacitySlotArray incoming(m_table.m_incoming);
  const int incomingOk = CommitAll(&incoming, &counts->m_own, consume);
  if (incomingOk) {
    m_table.m_incoming = incoming;
  }
  int outgoingOk = 0;
  if (incomingOk) {
    CapacitySlotArray outgoing(m_table.m_outgoing);
    outgoingOk = CommitAll(&outgoing, &counts->m_carried, consume);
    if (outgoingOk) {
      m_table.m_outgoing = outgoing;
    }
  }
  return outgoingOk;
}

int CapacityLeg::CommitOutgoing(UnitCountTable* counts, int consume)
{
  CapacitySlotArray outgoing(m_table.m_outgoing);
  const int ok = CommitAll(&outgoing, &counts->m_own, consume);
  if (ok) {
    m_table.m_outgoing = outgoing;
  }
  return ok;
}

namespace
{

struct BucketAnswer {
  int m_allowed = 0;
  int m_unlimited = 1;
  int m_remaining = 0;
  StatusCode m_limitStatus = okStatus();
  StatusCode m_status = okStatus();
};

int queryBucket(GameObj* unit, int side, int arg, int kind, const int* filter, const FamilyRange* range, bool incoming,
                BucketAnswer* answer)
{
  int ok = incoming ? unit->CheckIncomingUnitOrder(side, arg, kind, rangeArgument(range), &answer->m_status)
                    : unit->CheckOutgoingUnitOrder(side, arg, kind, rangeArgument(range), &answer->m_status);
  if (ok && isOk(answer->m_status)) {
    ok = incoming ? unit->GetIncomingUnitCapacity(range, *filter, &answer->m_allowed, &answer->m_unlimited,
                                                  &answer->m_remaining, &answer->m_limitStatus)
                  : unit->GetOutgoingUnitCapacity(range, *filter, &answer->m_allowed, &answer->m_unlimited,
                                                  &answer->m_remaining, &answer->m_limitStatus);
  }
  return ok;
}

} // namespace

int CapacityLeg::Build(int side, int arg, int kind, const int* filter, GameObj* unit)
{
  int ok = 1;
  ObjectKey key;
  m_key = *unit->GetKey(&key);
  ObjectKey parentKey;
  const uint32_t family = unit->GetFamily();
  const bool isGlobalRoot = family >= kFirstGlobalFamily && family < kEndGlobalFamily;
  if (!isGlobalRoot) {
    ok = unit->GetValidParentKey(&parentKey);
  }
  if (!ok) {
    return ok;
  }
  m_parentKey = parentKey;
  for (int pass = 0; pass < 2 && ok; ++pass) {
    const bool incoming = pass == 0;
    CapacitySlotArray& slots = incoming ? m_table.m_incoming : m_table.m_outgoing;
    for (int i = 0; i < kCapacityBuckets && ok; ++i) {
      BucketAnswer answer;
      const FamilyRange* range = GetCapacityBucketRange(i);
      ok = queryBucket(unit, side, arg, kind, filter, range, incoming, &answer);
      if (!ok) {
        break;
      }
      if (answer.m_allowed == 0 && isOk(answer.m_status)) {
        answer.m_status = StatusCode(kStatusCategory, kStatusNotAccepted);
      }
      CapacitySlot slot;
      slot.m_unlimited = answer.m_unlimited;
      slot.m_remaining = answer.m_remaining;
      slot.m_limitStatus = answer.m_limitStatus;
      slot.m_status = answer.m_status;
      slots.m_slots[i] = slot;
    }
  }
  return ok;
}

int CapacityTable::MarkAllIncomingOverCapacity()
{
  return m_incoming.MarkAllOverCapacity();
}
