#include "core/utility_classes.h"
#include "economy/manu_mgr.h"

namespace {

constexpr int kApplyEverything = 1;
constexpr int kApplyLocationOnly = 2;

ObjectKey* seedKeyAsObjectKey(NullableKey* key) { return reinterpret_cast<ObjectKey*>(key); }

}  // namespace

int ManuMgr::Read(istream* in) {
    const int result = GameObj::Read(in);
    if (result) {
        ReadValue32(in, &m_remainingGameObjCount);
        ReadValue32(in, &m_completedPointCount);
        ReadValue32(in, &m_overflowPointCount);
        seedKeyAsObjectKey(&m_seedKey)->Read(in);
        ReadValue32(in, &m_requiredPointCount);
        ReadValue32(in, &m_totalRequiredPointCount);
        ReadValue32(in, &m_reserved);
        m_productKey.Read(in);
        m_deploymentKey.Read(in);
        m_targetKey.Read(in);
        m_productName.Read(in);
    }
    return result;
}

int ManuMgr::Write(ostream* out) {
    const int result = GameObj::Write(out);
    if (result) {
        WriteValue32(out, &m_remainingGameObjCount);
        WriteValue32(out, &m_completedPointCount);
        WriteValue32(out, &m_overflowPointCount);
        seedKeyAsObjectKey(&m_seedKey)->Write(out);
        WriteValue32(out, &m_requiredPointCount);
        WriteValue32(out, &m_totalRequiredPointCount);
        WriteValue32(out, &m_reserved);
        m_productKey.Write(out);
        m_deploymentKey.Write(out);
        m_targetKey.Write(out);
        m_productName.Write(out);
    }
    return result;
}

int ManuMgr::Checksum(Crc32* crc) {
    const int result = GameObj::Checksum(crc);
    if (result) {
        crc->AddInt(m_remainingGameObjCount);
        crc->AddInt(m_completedPointCount);
        crc->AddInt(m_overflowPointCount);
        seedKeyAsObjectKey(&m_seedKey)->Checksum(crc);
        crc->AddInt(m_requiredPointCount);
        crc->AddInt(m_totalRequiredPointCount);
        crc->AddInt(m_reserved);
        m_productKey.Checksum(crc);
        m_deploymentKey.Checksum(crc);
        m_targetKey.Checksum(crc);
    }
    return result;
}

int ManuMgr::Validate() {
    int result = GameObj::Validate();
    if (result) result = IsQueueLengthConsistent();
    return result;
}

int ManuMgr::ApplyAll(NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) {
        GameObj::ApplyAll(ctx);
        if (ctx->m_flag04 == kApplyEverything) {
            ApplyRemainingGameObjCount(ctx);
            ApplyCompletedPointCount(ctx);
            ApplyOverflowPointCount(ctx);
            ApplySeedKey(ctx);
            ApplyRequiredPointCount(ctx);
            ApplyTotalRequiredPointCount(ctx);
            ApplyReserved(ctx);
            ApplyProductKey(ctx);
            ApplyDeploymentKey(ctx);
            ApplyTargetKey(ctx);
            ApplyProductName(ctx);
        } else if (ctx->m_flag04 == kApplyLocationOnly) {
            ApplyParent(ctx);
            ApplyDeploymentKey(ctx);
            ApplyTargetKey(ctx);
        }
    }
    return result;
}
