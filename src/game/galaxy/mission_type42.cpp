#include "galaxy/mission_type42.h"

#include "core/checked_new.h"
#include "gameobj/game_hooks.h"
#include "mission/mission_bases.h"
#include "mission/mission_forwarders.h"

extern int g_mission42EventKindsReady;     // 0x6bb930
extern EventKindSet g_mission42EventKinds; // 0x6bb938

namespace
{

constexpr uint32_t kFamilyMission42 = 0x42;
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;

} // namespace

Mission42::Mission42() = default;

Mission42::~Mission42() = default;

uint32_t Mission42::GetFamily()
{
  return kFamilyMission42;
}

int Mission42::Read(istream* in)
{
  return ReadImpl(in);
}

int Mission42::Write(ostream* out)
{
  return MissionTypeWriteForward(this, out);
}

int Mission42::Checksum(Crc32* crc)
{
  return ChecksumImpl(crc);
}

int Mission42::Validate()
{
  return ValidateImpl();
}

int Mission42::ApplyAll(NotifyContext* ctx)
{
  if (IsSideView()) {
    return MissionTypeApplyAllForward(this, ctx);
  }
  return 0;
}

int Mission42::CreateViews(int controlKind, NotifyContext* ctx)
{
  return MissionTypeCreateViewsForward(this, controlKind, ctx);
}

int Mission42::ReadImpl(istream* in)
{
  return this->Mission::ReadImpl(in);
}

int Mission42::ChecksumImpl(Crc32* crc)
{
  return this->MissionTypeBase::ChecksumBody(crc);
}

int Mission42::ValidateImpl()
{
  return static_cast<Mission*>(this)->Mission::ValidateImpl();
}

int Mission42::GetEventKinds(int unused, EventKindSet* out)
{
  const int result = BuildEventKinds();
  if (result) {
    *out = g_mission42EventKinds;
  }
  return result;
}

int Mission42::CheckOrder(int a, int orderKind, StatusCode* out)
{
  *out = StatusCode(kStatusCategory, kStatusNotSupported);
  const int result = BuildEventKinds();
  if (result && g_mission42EventKinds.Contains(orderKind)) {
    *out = StatusCode(kStatusCategory, kStatusOk);
  }
  return result;
}

GameObj* Mission42::CreateViewObject()
{
  return NewOrNull<Mission42>();
}

GameObjNotifier* Mission42::Create()
{
  return NewOrNull<Mission42>();
}

int Mission42::BuildEventKinds()
{
  int result = 1;
  if (!g_mission42EventKindsReady) {
    g_mission42EventKindsReady = 1;
    result = MissionTypeBase::InitEventKinds(0, &g_mission42EventKinds);
  }
  return result;
}
EventKindSet g_mission42EventKinds{};
int g_mission42EventKindsReady{};
