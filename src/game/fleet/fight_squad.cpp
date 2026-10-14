#include "fleet/fight_squad.h"

#include <new>

#include "core/utility_classes.h"
#include "data/game_math.h"
#include "data/sd_records.h"
#include "events/event_post.h"
#include "gameobj/game_obj_iterator.h"
#include "gameobj/object_family.h"
#include "gameobj/side_bits.h"

extern int g_fightSquadEventKindsReady; // 0x6b2b38
extern EventKindSet g_fightSquadEventKinds; // 0x6b2b40

namespace
{
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kApplyEverything = 1;
constexpr uint32_t kExistingFlag = 0x40;
constexpr uint32_t kDestroyedFlag = 0x8;
constexpr uint32_t kLockedFlag = 0x4000;
constexpr uint32_t kAlwaysFilter = 4;
constexpr int kSquadSizeDamageEvent = 0x1a0;
constexpr FamilyRange kFighterFamilies(0x1c, 0x20);

FighterSD* fighterRecordOf(FightSquad* squad)
{
  return static_cast<FighterSD*>(squad->m_record);
}
} // namespace

FightSquad::FightSquad()
{
  m_squadSizeDamage = 0;
}

FightSquad::~FightSquad() = default;

uint32_t FightSquad::GetFamily()
{
  return family::kFighterSquadron;
}

int FightSquad::Read(istream* in)
{
  return ReadState(in);
}

int FightSquad::Write(ostream* out)
{
  return WriteState(out);
}

int FightSquad::Checksum(Crc32* crc)
{
  return ChecksumState(crc);
}

int FightSquad::ApplyAll(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
    result = ApplyAllState(ctx);
  return result;
}

int FightSquad::ReadState(istream* in)
{
  const int result = CombatUnit::Read(in);
  if (result)
    ReadValue32(in, &m_squadSizeDamage);
  return result;
}

int FightSquad::WriteState(ostream* out)
{
  const int result = CombatUnit::Write(out);
  if (result)
    WriteValue32(out, &m_squadSizeDamage);
  return result;
}

int FightSquad::ApplyAllState(NotifyContext* ctx)
{
  const int result = CombatUnit::ApplyAll(ctx);
  if (result && ctx->m_flag04 == kApplyEverything)
    ApplySquadSizeDamage(ctx);
  return result;
}

int FightSquad::ChecksumState(Crc32* crc)
{
  const int result = CombatUnit::Checksum(crc);
  if (result)
    crc->AddInt(m_squadSizeDamage);
  return result;
}

int FightSquad::Validate()
{
  uint32_t valid = CombatUnit::Validate();
  GameObjIterator children(this, kAlwaysFilter);
  children.Begin();
  if (children.Current())
    valid = 0;
  if (valid)
  {
    const int damage = m_squadSizeDamage;
    valid = damage >= 0 && static_cast<int>(GetSquadSize()) >= damage;
    if (valid && damage == static_cast<int>(GetSquadSize()))
      valid = (m_stateFlags & kDestroyedFlag) != 0;
  }
  GameObjNotifier* parent = nullptr;
  uint32_t result = 0;
  if (valid && (result = GetParent(&parent)) != 0)
  {
    if ((m_stateFlags & kDestroyedFlag) == 0 || (m_stateFlags & kLockedFlag) == 0)
    {
      result = (~static_cast<GameObj*>(parent)->m_stateFlags >> 3) & 1;
    }
  }
  return result;
}

int FightSquad::GetTransitSpeed(int arg)
{
  int speed = 0;
  if (m_stateFlags & kExistingFlag)
  {
    const int lostField6c = GetDamageField6c();
    speed = static_cast<int>(GetField6c()) - lostField6c;
    if (speed == 0)
    {
      const int lostField70 = GetDamageField70();
      speed = static_cast<int>(GetField70()) - lostField70;
    }
    if (speed == 0)
      speed = GameObj::GetTransitSpeed(arg);
  }
  return speed;
}

int FightSquad::GetEventKinds(int unused, EventKindSet* out)
{
  const int ready = EnsureEventKinds();
  if (ready)
    *out = g_fightSquadEventKinds;
  return ready;
}

int FightSquad::CheckOrder(int a, int orderKind, StatusCode* out)
{
  *out = StatusCode(kStatusCategory, kStatusNotSupported);
  const int ready = EnsureEventKinds();
  if (ready && g_fightSquadEventKinds.Contains(orderKind))
    *out = StatusCode(kStatusCategory, kStatusOk);
  return ready;
}

int FightSquad::CreateViews(int controlKind, NotifyContext* ctx)
{
  return CombatUnit::CreateViews(controlKind, ctx);
}

int FightSquad::HandleControlMismatch(NotifyContext* ctx)
{
  int result = 1;
  if ((m_stateFlags & kDestroyedFlag) == 0)
    result = SetAutorouting(1, ctx);
  return result;
}

GameObj* FightSquad::CreateViewObject()
{
  return new FightSquad();
}

int FightSquad::OnParentChanged(GameObjNotifier* oldParent, GameObjNotifier* newParent, NotifyContext* ctx)
{
  return CombatUnit::OnParentChanged(oldParent, newParent, ctx);
}

int FightSquad::OnUsableChanged(int newValue, NotifyContext* ctx)
{
  return CombatUnit::OnUsableChanged(newValue, ctx);
}

int FightSquad::GetRepairLevel()
{
  return (m_combatFlags & kFastRepairFlag) != 0 ? g_gnprtb1200 : g_gnprtb1201;
}

int FightSquad::GetDefenseLeft()
{
  const int lost = GetBombardmentDefenseLost();
  const int full = static_cast<int>(GetBombardmentDefense());
  return full - lost;
}

uint32_t FightSquad::GetField60()
{
  const uint32_t perFighter = CombatUnit::GetField60();
  return GetSquadSize() * perFighter;
}

uint32_t FightSquad::GetGridColumnTotal(int column)
{
  const uint32_t perFighter = GetRecordColumnTotal(column);
  return GetSquadSize() * perFighter;
}

uint32_t FightSquad::GetGridValue(int row, int column)
{
  const uint32_t perFighter = GetRecordGridValue(row, column);
  return GetSquadSize() * perFighter;
}

uint32_t FightSquad::GetGridTotal()
{
  const uint32_t perFighter = GetRecordTotal();
  return GetSquadSize() * perFighter;
}

uint32_t FightSquad::GetDamageField60()
{
  const uint32_t perFighter = CombatUnit::GetField60();
  return GetSquadSizeDamage() * perFighter;
}

uint32_t FightSquad::GetDamageTotal()
{
  const uint32_t perFighter = GetRecordTotal();
  return GetSquadSizeDamage() * perFighter;
}

uint32_t FightSquad::GetDamageColumnTotal(int column)
{
  const uint32_t perFighter = GetRecordColumnTotal(column);
  return GetSquadSizeDamage() * perFighter;
}

uint32_t FightSquad::GetDamageGridValue(int row, int column)
{
  const uint32_t perFighter = GetRecordGridValue(row, column);
  return GetSquadSizeDamage() * perFighter;
}

int FightSquad::RepairStep(NotifyContext* ctx)
{
  int result = 1;
  if (0 < m_squadSizeDamage)
    result = RepairSquadSizeDamage(1, ctx);
  return result;
}

int FightSquad::GetTotalTorpedoes()
{
  const uint32_t perFighter = GetRecordTorpedoes();
  return GetSquadSize() * perFighter;
}

uint32_t FightSquad::GetSquadSize()
{
  return m_record ? fighterRecordOf(this)->m_squadronSize : 0;
}

uint32_t FightSquad::GetBombardmentDefense()
{
  return m_record ? fighterRecordOf(this)->m_bombardmentDefense : 0;
}

int FightSquad::GetTorpedoesLost()
{
  const uint32_t perFighter = GetRecordTorpedoes();
  return GetSquadSizeDamage() * perFighter;
}

int FightSquad::GetSquadSizeDamage()
{
  return m_squadSizeDamage;
}

int FightSquad::GetBombardmentDefenseLost()
{
  const int squadSize = static_cast<int>(GetSquadSize());
  if (squadSize == 0)
    return 0;
  const int lost = GetSquadSizeDamage();
  return ScaleRatio(static_cast<int>(GetBombardmentDefense()), lost, squadSize);
}

int FightSquad::UpdateDamaged(NotifyContext* ctx)
{
  return SetDamaged(GetSquadSizeDamage() != 0, ctx);
}

int FightSquad::OnSquadSizeDamageChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  int result = newValue <= static_cast<int>(GetSquadSize());
  if (result)
    result = UpdateDamaged(ctx);
  return result;
}

int FightSquad::OnSquadSizeDamageNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    LogInt("FightSquadSquadSizeDamageNotif", "SquadSizeDamage", newValue, oldValue, 0, ctx);
    result = PostPayloadInts(kSquadSizeDamageEvent, this, oldValue, newValue, ctx);
  }
  return result;
}

uint32_t FightSquad::GetRecordTorpedoes()
{
  return m_record ? fighterRecordOf(this)->m_torpedoes : 0;
}

int FightSquad::SetSquadSizeDamage(int value, NotifyContext* ctx)
{
  const int master = IsMasterView();
  int result = 0;
  if (master)
    result = IsInRange(value, 0, static_cast<int>(GetSquadSize()), " Invalid SquadSizeDamage value!");
  if (result && m_squadSizeDamage != value)
  {
    const int oldValue = m_squadSizeDamage;
    m_squadSizeDamage = value;
    static_cast<FightSquad*>(GetViewObject(1))->ApplySquadSizeDamage(ctx);
    static_cast<FightSquad*>(GetViewObject(2))->ApplySquadSizeDamage(ctx);
    OnSquadSizeDamageChanged(oldValue, m_squadSizeDamage, ctx);
  }
  return result;
}

int FightSquad::ApplySquadSizeDamage(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    auto* master = static_cast<FightSquad*>(GetViewObject(0));
    if (MatchesSideMask(ctx->m_flag08))
    {
      const int oldValue = m_squadSizeDamage;
      if (oldValue != master->m_squadSizeDamage)
      {
        m_squadSizeDamage = master->m_squadSizeDamage;
        if (CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(ViewSide())))
        {
          result = OnSquadSizeDamageNotified(oldValue, m_squadSizeDamage, ctx);
        }
      }
    }
  }
  return result;
}

int FightSquad::InflictSquadSizeDamage(int amount, int destroyedReason, NotifyContext* ctx)
{
  int result = 0;
  if (amount >= 0)
  {
    int total = m_squadSizeDamage + amount;
    if (total > static_cast<int>(GetSquadSize()))
      total = static_cast<int>(GetSquadSize());
    result = SetSquadSizeDamage(total, ctx);
  }
  if (result)
  {
    const int lost = GetSquadSizeDamage();
    if (static_cast<int>(GetSquadSize()) - lost == 0)
      result = ChangeDestroyedReason(destroyedReason, ctx);
  }
  return result;
}

int FightSquad::RepairSquadSizeDamage(int amount, NotifyContext* ctx)
{
  int result = 0;
  if (amount >= 0)
  {
    int remaining = m_squadSizeDamage - amount;
    if (remaining < 0)
      remaining = 0;
    result = SetSquadSizeDamage(remaining, ctx);
  }
  return result;
}

int FightSquad::EnsureEventKinds()
{
  if (g_fightSquadEventKindsReady == 0)
  {
    g_fightSquadEventKindsReady = 1;
    CombatUnit::InitEventKinds(0, &g_fightSquadEventKinds);
  }
  return 1;
}

int CountFightSquads(GameObj* parent, uint32_t side, uint32_t filter)
{
  return parent->CountChildren(&kFighterFamilies, side, filter);
}

GameObj* CreateFightSquad()
{
  return new FightSquad();
}

FightSquadIterator::FightSquadIterator(GameObjNotifier* parent, uint32_t stateFilter)
: ChildIterator(parent, &kFighterFamilies, stateFilter)
{
}

FightSquadIterator::FightSquadIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
: ChildIterator(parent, side, &kFighterFamilies, stateFilter)
{
}

FightSquadIterator::~FightSquadIterator() = default;
EventKindSet g_fightSquadEventKinds{};
int g_fightSquadEventKindsReady{};
