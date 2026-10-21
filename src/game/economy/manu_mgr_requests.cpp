#include "economy/manu_mgr.h"
#include "economy/manufacturing_facility.h"
#include "gameobj/object_lookup.h"

// The request checks (slots 111-121) and the orders (slots 124-128). A check fills a StatusCode; the "Impl" variants
// leave a refusal reason in it, the wrappers start from {1,-1} and turn an outright refusal into {1,1}.

namespace
{

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusRefused = 1;
constexpr int kManuStatusCategory = 0xa0;
constexpr int kManuNoFacility = 3;
constexpr int kManuNoProduct = 1;
constexpr int kRenameEventKind = 0x203;
constexpr int kEvent215Kind = 0x215;
constexpr uint32_t kOutOfService = 0x1;

bool isOk(const StatusCode* status)
{
  return status->m_second == static_cast<uint32_t>(kStatusOk);
}

void setStatus(StatusCode* out, int category, int code)
{
  *out = StatusCode(category, code);
}

void setOk(StatusCode* out)
{
  setStatus(out, kStatusCategory, kStatusOk);
}

void setRefused(StatusCode* out)
{
  setStatus(out, kStatusCategory, kStatusRefused);
}

const ObjectKey* requestKey(const ActivityRequest* request)
{
  return &request->m_target;
}

} // namespace

int ManuMgr::CheckFacilityAvailableImpl(const ActivityRequest* request, StatusCode* out)
{
  setOk(out);
  int result = CheckActivityRequest(request, out);
  if (result) {
    if (isOk(out)) {
      GameObjNotifier* host = GetHostSystem();
      result = host != nullptr;
      int available = 0;
      ManufacturingFacilityIterator facilities(host);
      facilities.Begin();
      while (facilities.Current()) {
        int usable = 0;
        if (IsCompatibleFacility(static_cast<GameObj*>(facilities.Current()))
            && (AsProcFacil(facilities.Current())->m_procFlags & kOutOfService) == 0) {
          usable = 1;
        }
        available += usable;
        facilities.Advance();
      }
      if (available == 0) {
        setStatus(out, kManuStatusCategory, kManuNoFacility);
      }
    }
    if (result) {
      return result;
    }
  }
  setRefused(out);
  return 0;
}

int ManuMgr::CheckFacilityAvailable(const ActivityRequest* request, intptr_t unused, StatusCode* out)
{
  setOk(out);
  const int result = CheckFacilityAvailableImpl(request, out);
  if (!result) {
    setRefused(out);
  }
  return result;
}

int ManuMgr::CheckProductActionImpl(const ActivityRequest* request, StatusCode* out)
{
  setOk(out);
  int result = CheckActivityRequest(request, out);
  if (result && isOk(out)) {
    GameObjNotifier* product = GetProduct();
    if (!product) {
      setStatus(out, kManuStatusCategory, kManuNoProduct);
    } else {
      ActivityRequest forwarded = *request;
      forwarded.m_kind = kRenameEventKind;
      result = static_cast<GameObj*>(product)->CheckRenameImpl(&forwarded, out);
    }
  }
  return result;
}

int ManuMgr::CheckProductAction(const ActivityRequest* request, intptr_t unused, StatusCode* out)
{
  setOk(out);
  const int result = CheckProductActionImpl(request, out);
  if (!result) {
    setRefused(out);
  }
  return result;
}

int ManuMgr::CheckHasProductImpl(const ActivityRequest* request, StatusCode* out)
{
  setOk(out);
  const int result = CheckActivityRequest(request, out);
  if (result && isOk(out) && GetProduct() == nullptr) {
    setStatus(out, kManuStatusCategory, kManuNoProduct);
  }
  if (!result) {
    setRefused(out);
  }
  return result;
}

int ManuMgr::CheckHasProduct(const ActivityRequest* request, StatusCode* out)
{
  setOk(out);
  const int result = CheckHasProductImpl(request, out);
  if (!result) {
    setRefused(out);
  }
  return result;
}

int ManuMgr::GetReserved(intptr_t unused, int* out)
{
  *out = m_reserved;
  return 1;
}

int ManuMgr::CheckPlainImpl(const ActivityRequest* request, int unused, StatusCode* out)
{
  setOk(out);
  const int result = CheckActivityRequest(request, out);
  if (!result) {
    setRefused(out);
  }
  return result;
}

int ManuMgr::CheckPlain(const ActivityRequest* request, int unused, StatusCode* out)
{
  setOk(out);
  const int result = CheckPlainImpl(request, unused, out);
  if (!result) {
    setRefused(out);
  }
  return result;
}

int ManuMgr::CheckPlain2(const ActivityRequest* request, StatusCode* out)
{
  setOk(out);
  const int result = CheckActivityRequest(request, out);
  if (!result) {
    setRefused(out);
  }
  return result;
}

int ManuMgr::CheckRequestedObject(const ActivityRequest* request, intptr_t unused, StatusCode* out)
{
  setOk(out);
  int result = CheckPlain2(request, out);
  if (result) {
    if (isOk(out)) {
      result = GetObjectInFamilies_01_ff(request->m_side, requestKey(request)) != nullptr;
    }
    if (result) {
      return result;
    }
  }
  setRefused(out);
  return 0;
}

int ManuMgr::CheckProductEvent215(int unused, int side, StatusCode* out)
{
  ActivityRequest request;
  request.m_side = side;
  request.m_unk04 = 0;
  request.m_target = ObjectKey();
  request.m_kind = kEvent215Kind;
  return CheckProductActionImpl(&request, out);
}

int ManuMgr::ReplaceQueue(const ActivityRequest* request, const ObjectKeyIntSet& products, NotifyContext* ctx)
{
  const int destroyed = DestroyProducts(ctx) != 0;
  const int created = CreateProducts(products, ctx);
  return created && destroyed;
}

int ManuMgr::RenameProduct(const ActivityRequest* request, std::string* name, NotifyContext* ctx)
{
  StatusCode status(kStatusCategory, kStatusOk);
  int result = CheckProductAction(request, reinterpret_cast<intptr_t>(name), &status);
  if (result && isOk(&status)) {
    result = RenameProductIfAny(name, ctx);
  }
  return result;
}

int ManuMgr::CancelProducts(const ActivityRequest* request, NotifyContext* ctx)
{
  StatusCode status(kStatusCategory, kStatusOk);
  int result = CheckHasProduct(request, &status);
  if (result && isOk(&status)) {
    result = DestroyProducts(ctx);
  }
  return result;
}

int ManuMgr::ReserveChecked(const ActivityRequest* request, int value, NotifyContext* ctx)
{
  StatusCode status(kStatusCategory, kStatusOk);
  int result = CheckPlain(request, value, &status);
  if (result && isOk(&status)) {
    result = ClearReserved(value, ctx);
  }
  return result;
}

int ManuMgr::ReparentProducts(const ActivityRequest* request, const ObjectKeyPairSet& products, NotifyContext* ctx)
{
  return ReparentQueue(products, requestKey(request), ctx);
}
