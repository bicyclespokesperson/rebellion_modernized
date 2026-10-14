#include "events/key_property_helpers.h"

#include <new>

extern ObjectKeyValueSet g_emptyKeyValueSet; // 0x6b2960
extern uint8_t g_emptyKeyValueSetBuilt; // 0x6b2954, bit 0
extern ObjectKey g_emptyKey; // 0x6b2958
extern uint8_t g_emptyKeyBuilt; // 0x6b2950, bit 0

ObjectKeyValueSet* GetEmptyKeyValueSet()
{
  return &g_emptyKeyValueSet;
}

ObjectKey* ConstructUnsetKey(ObjectKey* key)
{
  new (key) ObjectKey();
  return key;
}

ObjectKey* GetEmptyKey()
{
  return &g_emptyKey;
}

void ResetKey(ObjectKey* key)
{
  key->Reset();
}

int CallSlot1(void* object)
{
  struct SlotOne
  {
    virtual void UnusedSlot0()
    {
    }
    virtual void InvokeSlot1()
    {
    }
  };
  static_cast<SlotOne*>(object)->InvokeSlot1();
  return 1;
}
ObjectKey g_emptyKey{};
ObjectKeyValueSet g_emptyKeyValueSet{};
uint8_t g_emptyKeyBuilt{};
uint8_t g_emptyKeyValueSetBuilt{};
