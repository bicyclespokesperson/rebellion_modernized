#include "events/event_post.h"

#include "events/notif_event_payloads.h"
#include "events/timer_event.h"
#include "gameobj/factory_registries.h"

extern int g_setupComplete; // 0x6b90e0, set once the game setup has finished

namespace
{

constexpr int kFirstSourceEvent = 0x300;
constexpr int kLastSourceEvent = 0x31f;
constexpr int kFirstTwoKeyEvent = 0x320;
constexpr int kLastTwoKeyEvent = 0x323;
constexpr int kLastPlainEvent = 0x37f;
constexpr int kFirstTimer = 0x380;
constexpr int kLastTimer = 0x396;

bool inBand(int kind, int first, int last)
{
  return kind >= first && kind <= last;
}

bool inBand(uint32_t kind, uint32_t first, uint32_t last)
{
  return kind >= first && kind <= last;
}

ObjectKey keyOf(GameObjNotifier* object)
{
  ObjectKey key;
  object->GetKey(&key);
  return key;
}

} // namespace

// ---- queueing ---------------------------------------------------------------------------------------------

int QueueEvent(EventBase* event)
{
  CallGameHook2AF8(reinterpret_cast<HookArg>(event));
  return 1;
}

int QueueEventConfirmed(EventBase* event)
{
  QueueEvent(event);
  return 1;
}

int IsInRange(int value, int first, int last, const char*)
{
  return first <= value && value <= last;
}

// ---- events -----------------------------------------------------------------------------------------------

int CreateKeyedEvent(int kind, GameObjNotifier* object, NotifyContext* ctx, KeyedEvent** out)
{
  *out = nullptr;
  int result = 1;
  if (g_setupComplete) {
    auto* event = static_cast<KeyedEvent*>(CreateEventById(kind));
    *out = event;
    if (!event) {
      return 0;
    }
    result = event->m_flag;
    if (result == 0) {
      delete event;
      *out = nullptr;
      return 0;
    }
    ObjectKey parentKey;
    int gotParent = 0;
    gotParent = object->GetParentKey(&parentKey);
    ObjectKey ownerKey;
    result = gotParent != 0 ? object->GetOwnerKey(&ownerKey) : 0;
    if (result == 0) {
      delete event;
      *out = nullptr;
      return 0;
    }
    KeyedEvent* built = *out;
    built->m_key = keyOf(object);
    built->m_keyA = parentKey;
    built->m_keyB = ownerKey;
    built->m_context = *ctx;
    built->m_value = static_cast<int>(object->ViewSide());
  }
  if (result == 0 && *out != nullptr) {
    delete *out;
    *out = nullptr;
  }
  return result;
}

int PostSourceEvent(int kind, GameObjNotifier* object, int valueA, int valueB, NotifyContext* ctx)
{
  int result = 0;
  if (inBand(kind, kFirstSourceEvent, kLastSourceEvent)) {
    KeyedEvent* event = nullptr;
    result = CreateKeyedEvent(kind, object, ctx, &event);
    if (result != 0 && event != nullptr) {
      auto* source = static_cast<SourceEvent*>(event);
      source->m_valueA = valueA;
      source->m_valueB = valueB;
      QueueEventConfirmed(source);
    }
  }
  return result;
}

int PostSourceEventFlag(int kind, GameObjNotifier* object, int flag, NotifyContext* ctx)
{
  return PostSourceEvent(kind, object, flag == 0, flag, ctx);
}

int PostTwoKeyEvent(int kind, GameObjNotifier* object, const ObjectKey* keyC, const ObjectKey* keyD, NotifyContext* ctx)
{
  int result = 0;
  if (inBand(kind, kFirstTwoKeyEvent, kLastTwoKeyEvent)) {
    KeyedEvent* event = nullptr;
    result = CreateKeyedEvent(kind, object, ctx, &event);
    if (result != 0 && event != nullptr) {
      auto* twoKey = static_cast<TwoKeyEvent*>(event);
      twoKey->m_keyC = *keyC;
      twoKey->m_keyD = *keyD;
      QueueEventConfirmed(twoKey);
    }
  }
  return result;
}

int PostEvent(int kind, GameObjNotifier* object, NotifyContext* ctx)
{
  EventBase* event = nullptr;
  bool created = false;
  if (!(kind < kFirstSourceEvent || kind > kLastPlainEvent)) {
    event = static_cast<EventBase*>(CreateEventById(kind));
    created = event != nullptr;
  }
  int result = 0;
  if (created) {
    result = event->m_flag;
    if (result == 0) {
      delete event;
    } else {
      event->m_key = keyOf(object);
      event->m_context = *ctx;
      result = QueueEvent(event);
    }
  }
  return result;
}

// ---- timers -----------------------------------------------------------------------------------------------

int ArmTimer(int kind, GameObjNotifier* object, TrackedStat* stat, NotifyContext* ctx)
{
  TimerEvent* timer = nullptr;
  bool created = false;
  if (!(kind < kFirstTimer || kind > kLastTimer)) {
    timer = static_cast<TimerEvent*>(CreateEventById(kind));
    created = timer != nullptr;
  }
  int result = 0;
  if (created) {
    result = timer->m_flag;
    if (result == 0) {
      delete timer;
    } else {
      timer->m_key = keyOf(object);
      timer->m_spec = *stat;
      timer->m_context = *ctx;
      CallGameHook2AD8(reinterpret_cast<HookArg>(timer), timer->RollDelay());
    }
  }
  return result;
}

int ArmTimerAfterSetFlag(int kind, int flag, GameObjNotifier* object, TrackedStat* stat, NotifyContext* ctx)
{
  const int changesBefore = stat->m_changeCount;
  int result = 1;
  stat->SetFlag(flag);
  if (stat->m_changeCount != changesBefore && stat->Flag()) {
    result = ArmTimer(kind, object, stat, ctx);
  }
  return result;
}

int ArmTimerAfterSetLevel(int kind, int level, GameObjNotifier* object, TrackedStat* stat, NotifyContext* ctx)
{
  const int changesBefore = stat->m_changeCount;
  int result = 1;
  stat->SetLevel(level);
  if (stat->m_changeCount != changesBefore && stat->Flag()) {
    result = ArmTimer(kind, object, stat, ctx);
  }
  return result;
}

// ---- notification payloads --------------------------------------------------------------------------------

int CreatePayload(int kind, GameObjNotifier* object, NotifyContext* ctx, KeyedNotifPayload** out)
{
  *out = nullptr;
  int created = 1;
  if (g_setupComplete) {
    auto* payload = static_cast<KeyedNotifPayload*>(CreateKeyedObject(kind));
    created = payload != nullptr;
    *out = payload;
    if (created) {
      payload->m_value18 = static_cast<int>(object->ViewSide());
      payload->m_key = keyOf(object);
      payload->m_value0c = 0;
      payload->m_value10 = ctx->m_reserved14;
      payload->m_value14 = ctx->m_reserved18;
    }
  }
  return created;
}

int PostPayloadInts(uint32_t kind, GameObjNotifier* object, int valueA, int valueB, NotifyContext* ctx)
{
  int result = 0;
  if (inBand(kind, 0x100u, 0x242u)) {
    KeyedNotifPayload* payload = nullptr;
    result = CreatePayload(kind, object, ctx, &payload);
    if (result != 0 && payload != nullptr) {
      auto* ints = static_cast<IntPairPayload*>(payload);
      ints->m_valueA = valueA;
      ints->m_valueB = valueB;
      CallGameHook2AD4(reinterpret_cast<HookArg>(payload));
    }
  }
  return result;
}

int PostPayloadFourInts(uint32_t kind, GameObjNotifier* object, int valueA, int valueB, int valueC, int valueD,
                        NotifyContext* ctx)
{
  int result = 0;
  if (inBand(kind, 0x600u, 0x621u)) {
    KeyedNotifPayload* payload = nullptr;
    result = CreatePayload(kind, object, ctx, &payload);
    if (result != 0 && payload != nullptr) {
      auto* ints = static_cast<IntQuadPayload*>(payload);
      ints->m_valueA = valueA;
      ints->m_valueB = valueB;
      ints->m_valueC = valueC;
      ints->m_valueD = valueD;
      CallGameHook2AD4(reinterpret_cast<HookArg>(payload));
    }
  }
  return result;
}

int PostPayloadFlag(uint32_t kind, GameObjNotifier* object, int flag, NotifyContext* ctx)
{
  int result = 0;
  if (inBand(kind, 0x100u, 0x242u)) {
    KeyedNotifPayload* payload = nullptr;
    result = CreatePayload(kind, object, ctx, &payload);
    if (result != 0 && payload != nullptr) {
      auto* ints = static_cast<IntPairPayload*>(payload);
      ints->m_valueA = flag == 0;
      ints->m_valueB = flag;
      CallGameHook2AD4(reinterpret_cast<HookArg>(payload));
    }
  }
  return result;
}

int PostPayloadKeysA(uint32_t kind, GameObjNotifier* object, const ObjectKey* keyA, const ObjectKey* keyB,
                     NotifyContext* ctx)
{
  int result = 0;
  if (inBand(kind, 0x300u, 0x391u)) {
    KeyedNotifPayload* payload = nullptr;
    result = CreatePayload(kind, object, ctx, &payload);
    if (result != 0 && payload != nullptr) {
      auto* keys = static_cast<KeyPairPayload*>(payload);
      keys->m_keyA = *keyA;
      keys->m_keyB = *keyB;
      CallGameHook2AD4(reinterpret_cast<HookArg>(payload));
    }
  }
  return result;
}

int PostPayloadKeysB(uint32_t kind, GameObjNotifier* object, const ObjectKey* keyA, const ObjectKey* keyB,
                     NotifyContext* ctx)
{
  int result = 0;
  if (inBand(kind, 0x400u, 0x421u)) {
    KeyedNotifPayload* payload = nullptr;
    result = CreatePayload(kind, object, ctx, &payload);
    if (result != 0 && payload != nullptr) {
      auto* keys = static_cast<NullableKeyPairPayload*>(payload);
      keys->m_keyA.m_value = keyA->m_value;
      keys->m_keyB.m_value = keyB->m_value;
      CallGameHook2AD4(reinterpret_cast<HookArg>(payload));
    }
  }
  return result;
}

int PostPayloadStrings(uint32_t kind, GameObjNotifier* object, const std::string* textA, const std::string* textB,
                       NotifyContext* ctx)
{
  int result = 0;
  if (inBand(kind, 0x500u, 0x541u)) {
    KeyedNotifPayload* payload = nullptr;
    result = CreatePayload(kind, object, ctx, &payload);
    if (result != 0 && payload != nullptr) {
      auto* strings = static_cast<StringPairPayload*>(payload);
      (strings->m_textA = *textA);
      (strings->m_textB = *textB);
      CallGameHook2AD4(reinterpret_cast<HookArg>(payload));
    }
  }
  return result;
}
