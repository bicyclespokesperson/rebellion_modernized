#include "economy/manu_mgr.h"

extern const char *g_emptyText;  // 0x6b120c

// The virtual OnXChanged events of the master view (and the GameObj events ManuMgr overrides): they keep the queue, the
// current product and the derived properties in step.

namespace {

constexpr int kNeutralControlKind = 3;
constexpr uint32_t kCreated = 0x2;

}  // namespace

int ManuMgr::OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx) {
    int result = GameObj::OnControlKindChanged(oldKind, newKind, ctx);
    if (result) {
        if (m_stateFlags & kCreated) result = DestroyProducts(ctx);
        if (result) result = ClearReserved(0, ctx);
    }
    return result;
}

int ManuMgr::OnExistingChanged(int newValue, NotifyContext* ctx) {
    int result = GameObj::OnExistingChanged(newValue, ctx);
    if (result && newValue == 0) {
        result = DestroyProducts(ctx);
        if (result) {
            result = SetControlKind(kNeutralControlKind, ctx);
            if (result) result = ClearReserved(newValue, ctx);
        }
    }
    return result;
}

int ManuMgr::OnCreatedChanged(int newValue, NotifyContext* ctx) {
    GameObj::OnCreatedChanged(newValue, ctx);
    GameObjNotifier* host = GetHostSystem();
    int result = host != nullptr;
    if (result) {
        ObjectKey hostKey;
        host->GetKey(&hostKey);
        result = SetTargetKey(&hostKey, ctx);
        if (result) result = UpdateDeploymentKey(ctx);
    }
    return result;
}

int ManuMgr::OnRemainingGameObjCountChanged(int oldValue, int newValue, NotifyContext* ctx) {
    GameObjNotifier* host = GetHostSystem();
    int result = host != nullptr;
    if (result && newValue == 0) {
        ObjectKey hostKey;
        host->GetKey(&hostKey);
        result = SetTargetKey(&hostKey, ctx);
        if (result) {
            result = SetCompletedPointCount(0, ctx);
            if (result) result = SetOverflowPointCount(0, ctx);
        }
    }
    return result;
}

int ManuMgr::OnCompletedPointCountChanged(int oldValue, int newValue, NotifyContext* ctx) {
    GameObjNotifier* host = GetHostSystem();
    int result = host != nullptr;
    if (result && m_completedPointCount != 0 && m_completedPointCount >= m_requiredPointCount) {
        GameObjNotifier* product = GetProduct();
        result = product != nullptr;
        if (result) result = CompleteProduct(static_cast<GameObj*>(product), ctx);
    }
    return result;
}

int ManuMgr::OnSeedKeyChanged(NullableKey* oldKey, NullableKey* newKey, NotifyContext* ctx) {
    return UpdateRequiredFromSeed(ctx);
}

int ManuMgr::OnProductKeyChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) {
    int result = UpdateSeedKey(ctx);
    if (result) {
        result = UpdateDeploymentKey(ctx);
        if (result) result = UpdateProductName(ctx);
    }
    return result;
}

int ManuMgr::OnTargetKeyChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) {
    return UpdateDeploymentKey(ctx);
}

int ManuMgr::OnProductNameChanged(String* oldName, String* newName, NotifyContext* ctx) {
    GameObjNotifier* product = GetProduct();
    if (product) return static_cast<GameObj*>(product)->SetName(newName, ctx);
    const String empty(g_emptyText);
    return newName->Compare(empty) == 0;
}

const char *g_emptyText = "";
