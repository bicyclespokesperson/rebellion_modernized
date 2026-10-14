#include "economy/manu_mgr.h"

// SetX: validated setters of the master view. Each stores the value, lets both side views copy it (ApplyX) and then
// raises the virtual OnXChanged event (see game_obj.h for the scheme).

namespace
{

constexpr int kAllianceView = 1;
constexpr int kEmpireView = 2;

ManuMgr* viewOf(ManuMgr* self, int view)
{
  return static_cast<ManuMgr*>(self->ViewAs(view));
}

} // namespace

int ManuMgr::SetRemainingGameObjCount(int value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result)
  {
    const int oldValue = m_remainingGameObjCount;
    if (oldValue != value)
    {
      m_remainingGameObjCount = value;
      viewOf(this, kAllianceView)->ApplyRemainingGameObjCount(ctx);
      viewOf(this, kEmpireView)->ApplyRemainingGameObjCount(ctx);
      OnRemainingGameObjCountChanged(oldValue, m_remainingGameObjCount, ctx);
    }
  }
  return result;
}

int ManuMgr::SetCompletedPointCount(int value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result)
  {
    const int oldValue = m_completedPointCount;
    if (oldValue != value)
    {
      m_completedPointCount = value;
      viewOf(this, kAllianceView)->ApplyCompletedPointCount(ctx);
      viewOf(this, kEmpireView)->ApplyCompletedPointCount(ctx);
      OnCompletedPointCountChanged(oldValue, m_completedPointCount, ctx);
    }
  }
  return result;
}

int ManuMgr::SetOverflowPointCount(int value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result)
  {
    const int oldValue = m_overflowPointCount;
    if (oldValue != value)
    {
      m_overflowPointCount = value;
      viewOf(this, kAllianceView)->ApplyOverflowPointCount(ctx);
      viewOf(this, kEmpireView)->ApplyOverflowPointCount(ctx);
      OnOverflowPointCountChanged(oldValue, m_overflowPointCount, ctx);
    }
  }
  return result;
}

int ManuMgr::SetSeedKey(NullableKey* value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && m_seedKey.m_value != value->m_value)
  {
    NullableKey oldKey = m_seedKey;
    m_seedKey = *value;
    viewOf(this, kAllianceView)->ApplySeedKey(ctx);
    viewOf(this, kEmpireView)->ApplySeedKey(ctx);
    OnSeedKeyChanged(&oldKey, &m_seedKey, ctx);
  }
  return result;
}

int ManuMgr::SetRequiredPointCount(int value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result)
  {
    const int oldValue = m_requiredPointCount;
    if (oldValue != value)
    {
      m_requiredPointCount = value;
      viewOf(this, kAllianceView)->ApplyRequiredPointCount(ctx);
      viewOf(this, kEmpireView)->ApplyRequiredPointCount(ctx);
      OnRequiredPointCountChanged(oldValue, m_requiredPointCount, ctx);
    }
  }
  return result;
}

int ManuMgr::SetTotalRequiredPointCount(int value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result)
  {
    const int oldValue = m_totalRequiredPointCount;
    if (oldValue != value)
    {
      m_totalRequiredPointCount = value;
      viewOf(this, kAllianceView)->ApplyTotalRequiredPointCount(ctx);
      viewOf(this, kEmpireView)->ApplyTotalRequiredPointCount(ctx);
      OnTotalRequiredPointCountChanged(oldValue, m_totalRequiredPointCount, ctx);
    }
  }
  return result;
}

int ManuMgr::SetReserved(int value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result)
  {
    const int oldValue = m_reserved;
    if (oldValue != value)
    {
      m_reserved = value;
      viewOf(this, kAllianceView)->ApplyReserved(ctx);
      viewOf(this, kEmpireView)->ApplyReserved(ctx);
      OnReservedChanged(oldValue, m_reserved, ctx);
    }
  }
  return result;
}

int ManuMgr::SetProductKey(ObjectKey* value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && m_productKey.m_value != value->m_value)
  {
    ObjectKey oldKey(m_productKey);
    m_productKey = *value;
    viewOf(this, kAllianceView)->ApplyProductKey(ctx);
    viewOf(this, kEmpireView)->ApplyProductKey(ctx);
    OnProductKeyChanged(&oldKey, &m_productKey, ctx);
  }
  return result;
}

int ManuMgr::SetDeploymentKey(ObjectKey* value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && m_deploymentKey.m_value != value->m_value)
  {
    ObjectKey oldKey(m_deploymentKey);
    m_deploymentKey = *value;
    viewOf(this, kAllianceView)->ApplyDeploymentKey(ctx);
    viewOf(this, kEmpireView)->ApplyDeploymentKey(ctx);
    OnDeploymentKeyChanged(&oldKey, &m_deploymentKey, ctx);
  }
  return result;
}

int ManuMgr::SetTargetKey(ObjectKey* value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && m_targetKey.m_value != value->m_value)
  {
    ObjectKey oldKey(m_targetKey);
    m_targetKey = *value;
    viewOf(this, kAllianceView)->ApplyTargetKey(ctx);
    viewOf(this, kEmpireView)->ApplyTargetKey(ctx);
    OnTargetKeyChanged(&oldKey, &m_targetKey, ctx);
  }
  return result;
}

int ManuMgr::SetProductName(std::string* value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && CompareStrings(m_productName, *value) != 0)
  {
    std::string oldName(m_productName);
    (m_productName = *value);
    viewOf(this, kAllianceView)->ApplyProductName(ctx);
    viewOf(this, kEmpireView)->ApplyProductName(ctx);
    OnProductNameChanged(&oldName, &m_productName, ctx);
  }
  return result;
}
