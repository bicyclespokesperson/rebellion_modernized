#include "gameobj/unit_obj_base.h"

#include "data/dat_record.h"

namespace
{
constexpr uint32_t kExistingFlag = 0x40;
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kStatusNoSuchOrder = 1;
} // namespace

// ---- UnitObjData ------------------------------------------------------------------------------------------

UnitObjData::UnitObjData() = default;

UnitObjData::~UnitObjData() = default;

int UnitObjData::Read(istream* in)
{
  return GameObjData::Read(in);
}

int UnitObjData::Write(ostream* out)
{
  return GameObjData::Write(out);
}

int UnitObjData::Checksum(Crc32* crc)
{
  return GameObjData::Checksum(crc);
}

// ---- UnitObjBase ------------------------------------------------------------------------------------------

UnitObjBase::UnitObjBase() = default;

UnitObjBase::~UnitObjBase() = default;

int UnitObjBase::GetEventKinds(int unused, EventKindSet* out)
{
  EventKindSet kinds;
  *out = kinds;
  return 1;
}

int UnitObjBase::CanBeControlledBy(int side)
{
  const bool validSide = side >= 1 && side <= 3;
  if (!validSide || !m_record) {
    return 0;
  }
  if (!(m_stateFlags & kExistingFlag)) {
    return 1;
  }
  const SdRecord* record = static_cast<SdRecord*>(m_record);
  switch (side) {
    case 1:
      return record->m_field40;
    case 2:
      return record->m_field44;
    default:
      return record->m_field40 != 0 && record->m_field44 != 0 ? 1 : 0;
  }
}

int UnitObjBase::CheckOrder(int a, int orderKind, StatusCode* out)
{
  *out = StatusCode(kStatusCategory, kStatusNotSupported);
  return 1;
}

int UnitObjBase::CheckRequestImpl(const ActivityRequest* request, StatusCode* out)
{
  *out = StatusCode(kStatusCategory, kStatusOk);
  const int result = CheckActivityRequest(request, out);
  if (!result) {
    *out = StatusCode(kStatusCategory, kStatusNoSuchOrder);
  }
  return result;
}

int UnitObjBase::InitializeNewObject(intptr_t a)
{
  return GameObj::InitializeNewObject(a);
}

int UnitObjBase::InitializeAfterSetup(intptr_t a)
{
  return GameObj::InitializeNewObject(a);
}

int UnitObjBase::PrepareForDestroyedRelease(intptr_t a)
{
  return GameObj::PrepareForDestroyedRelease(a) != 0;
}

GameObjData* UnitObjBase::CreateData()
{
  return new UnitObjData();
}

int UnitObjBase::OnNameChanged(std::string* oldName, std::string* newName, NotifyContext* ctx)
{
  return GameObj::OnNameChanged(oldName, newName, ctx);
}

int UnitObjBase::OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx)
{
  return GameObj::OnControlKindChanged(oldKind, newKind, ctx);
}

int UnitObjBase::OnParentChanged(GameObjNotifier* oldParent, GameObjNotifier* newParent, NotifyContext* ctx)
{
  return GameObj::OnParentChanged(oldParent, newParent, ctx);
}

int UnitObjBase::OnDestroyedReasonChanged(uint32_t oldValue, uint32_t newValue, NotifyContext* ctx)
{
  return GameObj::OnDestroyedReasonChanged(oldValue, newValue, ctx);
}

int UnitObjBase::OnUsableChanged(int newValue, NotifyContext* ctx)
{
  return GameObj::OnUsableChanged(newValue, ctx);
}

int UnitObjBase::OnCreatedChanged(int newValue, NotifyContext* ctx)
{
  return GameObj::OnCreatedChanged(newValue, ctx);
}

int UnitObjBase::OnCompletedChanged(int newValue, NotifyContext* ctx)
{
  return GameObj::OnCompletedChanged(newValue, ctx);
}

int UnitObjBase::OnDestroyedChanged(int newValue, NotifyContext* ctx)
{
  return GameObj::OnDestroyedChanged(newValue, ctx);
}

int UnitObjBase::OnEnrouteChanged(int newValue, NotifyContext* ctx)
{
  return GameObj::OnEnrouteChanged(newValue, ctx);
}

int UnitObjBase::OnEnrouteActiveChanged(int newValue, NotifyContext* ctx)
{
  return GameObj::OnEnrouteActiveChanged(newValue, ctx);
}

int UnitObjBase::OnExistingChanged(int newValue, NotifyContext* ctx)
{
  return GameObj::OnExistingChanged(newValue, ctx);
}

int UnitObjBase::OnAutoroutingChanged(int newValue, NotifyContext* ctx)
{
  return GameObj::OnAutoroutingChanged(newValue, ctx);
}

uint32_t UnitObjBase::GetRecordField48()
{
  if (!m_record) {
    return 0;
  }
  return static_cast<SdRecord*>(m_record)->GetField48();
}

uint32_t UnitObjBase::GetRecordField4c()
{
  if (!m_record) {
    return 0;
  }
  return static_cast<SdRecord*>(m_record)->GetField4c();
}

int UnitObjBase::ApplyAllToSideView(NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result) {
    GameObj::ApplyAll(ctx);
  }
  return result;
}

int UnitObjBase::InitEventKinds(int unused, EventKindSet* out)
{
  EventKindSet kinds;
  *out = kinds;
  return 1;
}
