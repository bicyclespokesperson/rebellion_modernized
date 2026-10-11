#include "character/character_mgr.h"

#include "character/role.h"
#include "core/checked_new.h"
#include "data/dat_record.h"
#include "data/random.h"
#include "economy/side.h"
#include "events/command_classes.h"
#include "events/event_post.h"
#include "fleet/capship.h"
#include "galaxy/system.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_queries.h"
#include "mission/mission_rules.h"
#include "story/story_characters.h"
#include "story/story_common.h"

extern int g_characterMgrEventKindsReady; // 0x6bb7a0
extern EventKindSet g_characterMgrEventKinds; // 0x6bb798

ObjectKey* GetAiRecordKey(DatRecord* record, ObjectKey* out); // 0x402e40
// The starting number of minor characters of `side` for a galaxy of `size` (1-3); kind 1 is the Alliance's, 2 the Empire's.

namespace
{

constexpr uint32_t kCoruscantKey = 0x90000109;
constexpr uint32_t kYavinKey = 0x92000121;
constexpr uint32_t kEmperorKey = 0x34000280;
constexpr uint32_t kDarthKey = 0x35000281;
constexpr uint32_t kLukeKey = 0x32000242;
constexpr uint32_t kLeiaKey = 0x31000241;
constexpr uint32_t kHanKey = 0x33000243;
constexpr uint32_t kMonMothmaKey = 0x30000240;
constexpr uint32_t kChewbaccaKey = 0x38000343;
constexpr uint32_t kBossKeys[] = {0x38000380, 0x38000381, 0x38000382, 0x38000383, 0x3800038c};
constexpr uint32_t kHanCompanionKeys[] = {0x38000344, 0x38000341};
constexpr int kStateUsable = 1;
constexpr int kStateExisting = 3;
constexpr int kStateAlways = 4;
constexpr uint32_t kViewSideMask = 0x30;
constexpr uint32_t kCreatedFlag = 0x2;
constexpr uint32_t kUsableFlag = 0x1;
constexpr uint32_t kDestroyedFlag = 0x8;
constexpr uint32_t kRecordMajorCharacters = 0x30;
constexpr uint32_t kRecordMinorCharacters = 0x38;
constexpr int kEventSeatOfPower = 0x230;
constexpr int kEventPickupInProgress = 0x231;
constexpr int kFlagUnknownKind = 0x17;

bool isMaster(const GameObjRoot* object)
{
  return (object->m_flags & kViewSideMask) == 0;
}

bool inFamilies(const GameObjNotifier* object, uint32_t first, uint32_t last)
{
  const uint32_t family = const_cast<GameObjNotifier*>(object)->GetFamily();
  return family >= first && family < last;
}

ObjectKey keyOf(uint32_t value)
{
  ObjectKey key;
  key.m_value = value;
  return key;
}

GameObj* asGameObj(GameObjNotifier* object)
{
  return static_cast<GameObj*>(object);
}

} // namespace

// ---- construction -----------------------------------------------------------------------------------------------------

CharacterMgr::CharacterMgr() : m_flags58(0), m_galaxySize(0)
{
  SetId(8);
}

CharacterMgr::~CharacterMgr() = default;

uint32_t CharacterMgr::GetFamily()
{
  return family::kCharacterManager;
}

GameObj* CharacterMgr::CreateViewObject()
{
  return NewOrNull<CharacterMgr>();
}

GameObjNotifier* CharacterMgr::Create()
{
  return NewOrNull<CharacterMgr>();
}

int CharacterMgr::Validate()
{
  return GameObj::Validate();
}

int CharacterMgr::CreateViews(int controlKind, NotifyContext* ctx)
{
  return GameObj::CreateViews(controlKind, ctx);
}

int CharacterMgr::UpdateObservedByAlliance(NotifyContext* ctx)
{
  return SetObservedByAlliance(1, ctx) != 0 ? 1 : 0;
}

int CharacterMgr::UpdateObservedByEmpire(NotifyContext* ctx)
{
  return SetObservedByEmpire(1, ctx) != 0 ? 1 : 0;
}

int CharacterMgr::Virtual48(intptr_t ctxArg)
{
  NotifyContext* ctx = reinterpret_cast<NotifyContext*>(ctxArg);
  const int stub = Virtual47(ctxArg);
  return UpdateSeatOfPower(ctx) && stub ? 1 : 0;
}

// ---- event kinds ------------------------------------------------------------------------------------------------------

int CharacterMgr::BuildEventKinds()
{
  return story::buildKindsOnce(&g_characterMgrEventKindsReady,
                               []
                               {
                                 return ::InitEventKinds(0, &g_characterMgrEventKinds);
                               });
}

int CharacterMgr::GetEventKinds(int unused, EventKindSet* out)
{
  return story::copyKinds(BuildEventKinds(), g_characterMgrEventKinds, out);
}

int CharacterMgr::CheckOrder(int a, int orderKind, StatusCode* out)
{
  return story::checkOrder(BuildEventKinds(), g_characterMgrEventKinds, orderKind, out);
}

// ---- persistence ------------------------------------------------------------------------------------------------------

int CharacterMgr::Read(istream* in)
{
  return ReadBody(in);
}

int CharacterMgr::Write(ostream* out)
{
  return WriteBody(out);
}

int CharacterMgr::Checksum(Crc32* crc)
{
  return ChecksumBody(crc);
}

int CharacterMgr::ApplyAll(NotifyContext* ctx)
{
  const int isSideView = IsSideView();
  if (isSideView)
    return ApplyAllBody(ctx);
  return isSideView;
}

int CharacterMgr::ReadBody(istream* in)
{
  const int result = GameObj::Read(in);
  if (result)
  {
    ReadValue32(in, &m_flags58);
    if (result && isMaster(this))
      ReadValue32(in, &m_galaxySize);
  }
  return result;
}

int CharacterMgr::WriteBody(ostream* out)
{
  const int result = GameObj::Write(out);
  if (result)
  {
    WriteValue32(out, &m_flags58);
    if (result && isMaster(this))
      WriteValue32(out, &m_galaxySize);
  }
  return result;
}

int CharacterMgr::ChecksumBody(Crc32* crc)
{
  const int result = GameObj::Checksum(crc);
  if (result)
  {
    crc->AddInt(static_cast<int>(m_flags58));
    if (result && isMaster(this))
      crc->AddInt(m_galaxySize);
  }
  return result;
}

int CharacterMgr::ApplyAllBody(NotifyContext* ctx)
{
  const int result = GameObj::ApplyAll(ctx);
  if (result)
  {
    if (ctx->m_flag04 == 1)
    {
      ApplySeatOfPower(ctx);
      ApplyPickupInProgress(ctx);
    }
  }
  return result;
}

// ---- the two flags ----------------------------------------------------------------------------------------------------

int CharacterMgr::SetSeatOfPower(int on, NotifyContext* ctx)
{
  return story::setFlagProperty(
    this, &CharacterMgr::m_flags58, character_mgr_flag::kSeatOfPower, on,
    [&](CharacterMgr* view)
    {
      view->ApplySeatOfPower(ctx);
    },
    [&]
    {
      OnSeatOfPowerChanged(on, ctx);
    });
}

int CharacterMgr::SetPickupInProgress(int on, NotifyContext* ctx)
{
  return story::setFlagProperty(
    this, &CharacterMgr::m_flags58, character_mgr_flag::kPickupInProgress, on,
    [&](CharacterMgr* view)
    {
      view->ApplyPickupInProgress(ctx);
    },
    [&]
    {
      OnPickupInProgressChanged(on, ctx);
    });
}

int CharacterMgr::ApplySeatOfPower(NotifyContext* ctx)
{
  return story::applyFlagProperty(this, &CharacterMgr::m_flags58, character_mgr_flag::kSeatOfPower, ctx,
                                  [&](int value)
                                  {
                                    return OnSeatOfPowerNotified(value, ctx);
                                  });
}

int CharacterMgr::ApplyPickupInProgress(NotifyContext* ctx)
{
  return story::applyFlagProperty(this, &CharacterMgr::m_flags58, character_mgr_flag::kPickupInProgress, ctx,
                                  [&](int value)
                                  {
                                    return OnPickupInProgressNotified(value, ctx);
                                  });
}

int CharacterMgr::OnSeatOfPowerChanged(int on, NotifyContext* ctx)
{
  int ok = 1;
  for (GameObjNotifier* person = GetFirstMasterInFamilies_30_3c(); person != nullptr;
       person = NextCharacterSibling(asGameObj(person)))
    ok = UpdateLeadershipAdjustment(static_cast<Character*>(person), ctx) && ok;
  return ok;
}

int CharacterMgr::OnSeatOfPowerNotified(int on, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    LogFlag("CharacterMgrSeatOfPowerNotif", "SeatOfPower", on, 0, ctx);
    result = PostPayloadFlag(kEventSeatOfPower, this, on, ctx);
  }
  return result;
}

int CharacterMgr::OnPickupInProgressNotified(int on, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    LogFlag("CharacterMgrPickupInProgressNotif", "PickupInProgress", on, 0, ctx);
    result = PostPayloadFlag(kEventPickupInProgress, this, on, ctx);
  }
  return result;
}

int CharacterMgr::UpdateSeatOfPower(NotifyContext* ctx)
{
  GameObj* emperor = static_cast<GameObj*>(GetMaster_34000280());
  int ok = emperor != nullptr;
  GameObjNotifier* location = nullptr;
  if (emperor != nullptr)
  {
    ok = emperor->GetParent(&location) && ok;
    if (location != nullptr)
    {
      int holdsSeat = 0;
      if (!(static_cast<Character*>(emperor)->m_flagsAc & 1) && (emperor->m_stateFlags & kUsableFlag))
      {
        ObjectKey key;
        location->GetKey(&key);
        if (key.m_value == kCoruscantKey && location->ControlSide() == 2)
          holdsSeat = 1;
      }
      ok = SetSeatOfPower(holdsSeat, ctx) && ok;
    }
  }
  return ok;
}

int CharacterMgr::UpdateLeadershipAdjustment(Character* person, NotifyContext* ctx)
{
  int adjustment = 0;
  if ((m_flags58 & character_mgr_flag::kSeatOfPower) && person->GetRecordSide() == 2)
    adjustment = g_gnprtb0C04;
  return person->SetLeadershipAdjustment(adjustment, ctx) != 0;
}
EventKindSet g_characterMgrEventKinds{};
int g_characterMgrEventKindsReady{};
int g_gnprtb0200{};
int g_gnprtb0201{};
int g_gnprtb0202{};
int g_gnprtb0203{};
int g_gnprtb0204{};
int g_gnprtb0205{};
int g_gnprtb0600{};
int g_gnprtb0601{};
int g_gnprtb0602{};
int g_gnprtb0603{};
int g_gnprtb0604{};
int g_gnprtb0605{};
int g_gnprtb0606{};
int g_gnprtb0C00{};
int g_gnprtb0C01{};
int g_gnprtb0C02{};
int g_gnprtb0C03{};
int g_gnprtb0C04{};
int g_gnprtb0C05{};
int g_gnprtb0C06{};
int g_gnprtb0C07{};
int g_gnprtb0C08{};
int g_gnprtb0C09{};
int g_gnprtb0C0A{};
int g_gnprtb0C0B{};
int g_gnprtb0C0C{};
int g_gnprtb0C0D{};
int g_gnprtb0C0E{};
int g_gnprtb0C0F{};
int g_gnprtb0E00{};
int g_gnprtb0E01{};
int g_gnprtb0E02{};
int g_gnprtb0E03{};
int g_gnprtb0E04{};
int g_gnprtb0E05{};
int g_gnprtb1000{};
int g_gnprtb1800{};
int g_gnprtb1801{};
int g_gnprtb1802{};
int g_gnprtb1803{};
int g_gnprtb1804{};
int g_gnprtb1805{};
int g_gnprtb1806{};
int g_gnprtb1807{};
int g_gnprtb1808{};
int g_gnprtb1809{};
int g_gnprtb180A{};
int g_gnprtb180B{};
int g_gnprtb180C{};
int g_gnprtb180D{};
int g_gnprtb180E{};
int g_gnprtb180F{};
int g_gnprtb1810{};
int g_gnprtb1811{};
int g_gnprtb1812{};
int g_gnprtb1813{};
int g_gnprtb1814{};
int g_gnprtb1815{};
int g_gnprtb1816{};
int g_gnprtb1817{};
int g_gnprtb1818{};
int g_gnprtb1819{};
int g_gnprtb181A{};
int g_gnprtb181B{};
int g_gnprtb181C{};
int g_gnprtb181D{};
int g_gnprtb181E{};
int g_gnprtb181F{};
int g_gnprtb1820{};
int g_gnprtb1821{};
int g_gnprtb1822{};
int g_gnprtb1823{};
int g_gnprtb1824{};
int g_gnprtb1825{};
int g_gnprtb1827{};
int g_gnprtb1828{};
int g_gnprtb1829{};
int g_gnprtb182A{};
int g_gnprtb182B{};
int g_gnprtb182C{};
int g_gnprtb182D{};
int g_gnprtb182E{};
