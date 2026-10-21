#include "galaxy/role_container.h"

#include <new>

#include "character/character.h"
#include "character/role.h"
#include "core/checked_new.h"
#include "gameobj/game_obj_iterator.h"
#include "gameobj/object_family.h"
#include "orders/order_checks.h"

extern int g_roleContainerEventKindsReady;     // 0x6bbb38
extern EventKindSet g_roleContainerEventKinds; // 0x6bbb40

// GameObj's default side/control check used by several container order checks.

namespace
{

constexpr uint32_t kContainerFamily = 0x98;
constexpr uint32_t kFirstRoleFamily = 0x30;
constexpr uint32_t kEndRoleFamily = 0x40;
constexpr uint32_t kOnMissionFlag = 0x80;
constexpr uint32_t kControlSideMask = 0xc0;
constexpr uint32_t kAllStatesFilter = 4;
constexpr uint32_t kUsableOrEnrouteFilter = 3;
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kOrderCheckKind = 0x240;

bool isRoleFamily(uint32_t family)
{
  return family >= kFirstRoleFamily && family < kEndRoleFamily;
}

bool coversOnlyRoleFamilies(const FamilyRange* range)
{
  return !(range->m_first < kFirstRoleFamily || range->m_second > kEndRoleFamily);
}

} // namespace

RoleContainer::RoleContainer()
    : GameObj()
{
}

RoleContainer::~RoleContainer() = default;

uint32_t RoleContainer::GetFamily()
{
  return kContainerFamily;
}

int RoleContainer::GetOwnerKey(ObjectKey* out)
{
  ObjectKey key;
  GetKey(&key);
  new (out) ObjectKey(key);
  return 1;
}

int RoleContainer::Read(istream* in)
{
  return GameObj::Read(in);
}

int RoleContainer::Write(ostream* out)
{
  return GameObj::Write(out);
}

int RoleContainer::Checksum(Crc32* crc)
{
  return GameObj::Checksum(crc);
}

int RoleContainer::Validate()
{
  return ValidateImpl();
}

int RoleContainer::ApplyAll(NotifyContext* ctx)
{
  const int isSideView = IsSideView();
  if (isSideView) {
    return GameObj::ApplyAll(ctx);
  }
  return isSideView;
}

int RoleContainer::SetRecord(DatRecord* record)
{
  if (!record) {
    return 0;
  }
  const int familyMatches = AssignRecord(record);
  if (familyMatches) {
    SetId(record->m_id);
  }
  return familyMatches;
}

int RoleContainer::GetEventKinds(int unused, EventKindSet* out)
{
  const int result = BuildEventKinds();
  if (result) {
    *out = g_roleContainerEventKinds;
  }
  return result;
}

int RoleContainer::CheckOrder(int a, int orderKind, StatusCode* out)
{
  *out = StatusCode(kStatusCategory, kStatusNotSupported);
  const int result = BuildEventKinds();
  if (result && g_roleContainerEventKinds.Contains(orderKind)) {
    *out = StatusCode(kStatusCategory, kStatusOk);
  }
  return result;
}

int RoleContainer::CheckIncomingUnitOrder(int a, int b, int c, intptr_t d, StatusCode* out)
{
  int result = 1;
  *out = StatusCode(kStatusCategory, kStatusOk);
  const bool rangeIsRoles = c == kOrderCheckKind && coversOnlyRoleFamilies(reinterpret_cast<const FamilyRange*>(d));
  if (out->m_second == static_cast<uint32_t>(kStatusOk)) {
    result = CheckControlAndState(this, a, b, 1, 1, out);
    if (result && out->m_second != static_cast<uint32_t>(kStatusOk) && rangeIsRoles) {
      *out = StatusCode(kStatusCategory, kStatusOk);
    }
  }
  return result;
}

int RoleContainer::GetIncomingUnitCapacity(const FamilyRange* range, int b, int* o1, int* o2, int* o3, StatusCode* out)
{
  *o1 = 1;
  *o2 = 0;
  *o3 = 0;
  *out = StatusCode(kStatusCategory, kStatusNotSupported);
  if (coversOnlyRoleFamilies(range)) {
    *o2 = 1;
  } else {
    *o1 = 0;
  }
  return 1;
}

int RoleContainer::CreateViews(int controlKind, NotifyContext* ctx)
{
  return GameObj::CreateViews(controlKind, ctx);
}

GameObj* RoleContainer::CreateViewObject()
{
  return NewOrNull<RoleContainer>();
}

GameObjNotifier* RoleContainer::Create()
{
  return NewOrNull<RoleContainer>();
}

int RoleContainer::CountCharacterChildren(int* out, int unused1, int unused2)
{
  *out = 0;
  CharacterIterator characters(this, kUsableOrEnrouteFilter);
  for (characters.Begin(); characters.Current(); characters.Advance()) {
  }
  return 1;
}

int RoleContainer::ValidateImpl()
{
  int ok = GameObj::Validate();
  GameObjIterator children(this, kAllStatesFilter);
  for (children.Begin(); children.Current(); children.Advance()) {
    if (!ok) {
      return 0;
    }
    ObjectKey key;
    children.Current()->GetKey(&key);
    ok = isRoleFamily(key.Family());
  }
  if (!ok) {
    return 0;
  }

  int count = 0;
  if (!CountCharacterChildren(&count, 0, 0)) {
    return 0;
  }
  ok = count == 0;
  if (!ok) {
    return 0;
  }

  RoleIterator roles(this, kAllStatesFilter);
  for (roles.Begin(); roles.Current() && ok; roles.Advance()) {
    const Role* role = static_cast<Role*>(roles.Current());
    if (!(role->m_flags78 & kOnMissionFlag)) {
      ok = ((role->m_flags ^ m_flags) & kControlSideMask) == 0;
    }
  }
  return ok;
}

int RoleContainer::BuildEventKinds()
{
  int result = 1;
  if (!g_roleContainerEventKindsReady) {
    g_roleContainerEventKindsReady = 1;
    result = ::InitEventKinds(0, &g_roleContainerEventKinds);
  }
  return result;
}
EventKindSet g_roleContainerEventKinds{};
int g_roleContainerEventKindsReady{};
