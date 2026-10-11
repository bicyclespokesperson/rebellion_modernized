// The virtual functions of TacticalObjectManager that the reconstruction declares but does not implement: the 3D tactical code is
// out of scope (docs/code_map.md), no TacticalObjectManager is ever constructed in the native build (battles are auto-resolved), and
// these definitions only complete the class so that its vtable links.
#include "tactical/tactical_objmgr.h"

void TacticalObjectManager::Virtual09()
{
}
void TacticalObjectManager::PostEvent(int, int, int, int)
{
}
void TacticalObjectManager::Virtual13()
{
}
int TacticalObjectManager::Virtual14(void*)
{
  return 0;
}
int TacticalObjectManager::Virtual15(void*)
{
  return 0;
}
void TacticalObjectManager::Virtual16(void*)
{
}
void* TacticalObjectManager::ScalarDeletingDestructor(int)
{
  return nullptr;
}
int TacticalObjectManager::Virtual32(int, int)
{
  return 0;
}
int TacticalObjectManager::Virtual33(int)
{
  return 0;
}
int TacticalObjectManager::Update(float)
{
  return 0;
}
int TacticalObjectManager::Virtual38(float, float)
{
  return 0;
}
