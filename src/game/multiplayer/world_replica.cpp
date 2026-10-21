#include "multiplayer/world_replica.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "app/rebapp_globals.h"
#include "core/iostream_lib.h"
#include "core/memory_stream.h"
#include "events/game_manager.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_database.h"
#include "strategy_ui/game_ui_facade.h"

namespace reb::multiplayer
{
namespace
{

constexpr uint32_t kMagic = 0x31505257; // WRP1
constexpr uint32_t kVersion = 1;
constexpr size_t kMaximumBytes = 64 * 1024 * 1024;
constexpr uint32_t kMaximumObjects = 500000;
constexpr uint32_t kMaximumObjectBytes = 1024 * 1024;

struct ObjectId
{
  uint32_t key;
  uint32_t view;
  auto operator<=>(const ObjectId&) const = default;
};

struct Record
{
  ObjectId id;
  uint32_t parent;
  uint32_t hasChildren;
  uint32_t childListOk;
  std::vector<uint8_t> payload;
};

struct Clock
{
  uint32_t tick;
  uint32_t day;
  uint32_t serialA;
  uint32_t serialB;
  uint32_t ticksPerDay;
  uint32_t tickOfDay;
  uint32_t dayEnded;
  uint32_t lastBattleTick;
};

void Append32(std::vector<uint8_t>& out, uint32_t value)
{
  for (int shift = 0; shift < 32; shift += 8) {
    out.push_back(static_cast<uint8_t>(value >> shift));
  }
}

bool Read32(std::span<const uint8_t> in, size_t& offset, uint32_t& value)
{
  if (in.size() - offset < 4) {
    return false;
  }
  value = 0;
  for (int shift = 0; shift < 32; shift += 8) {
    value |= static_cast<uint32_t>(in[offset++]) << shift;
  }
  return true;
}

ObjectKey KeyOf(uint32_t value)
{
  ObjectKey key;
  key.m_value = value;
  return key;
}

GameObjNotifier* FindObject(const ObjectId& id)
{
  const ObjectKey key = KeyOf(id.key);
  return LookupObject(&key, static_cast<int>(id.view));
}

bool WriteObject(GameObjNotifier& object, std::vector<uint8_t>& payload)
{
  reb::io::MemoryWriter stream(kMaximumObjectBytes);
  if (!object.Write(stream.Out()) || !stream.Good() || stream.Bytes().empty()) {
    return false;
  }
  payload = stream.TakeBytes();
  return true;
}

bool ReadObject(GameObjNotifier& object, const std::vector<uint8_t>& payload)
{
  reb::io::MemoryReader stream(payload);
  return object.Read(stream.In()) != 0 && stream.Good() && stream.AtEnd();
}

bool Collect(GameObjNotifier* object, uint32_t view, std::vector<Record>& records, std::string& error)
{
  if (!object || records.size() >= kMaximumObjects) {
    error = "World replica object count exceeds limit";
    return false;
  }
  ObjectKey key;
  object->GetKey(&key);
  ObjectKey parent;
  if (object->m_parent) {
    object->m_parent->GetKey(&parent);
  }
  Record record{{key.m_value, view}, object->m_parent ? parent.m_value : 0,
                object->m_children ? 1u : 0u, object->m_children ? static_cast<uint32_t>(object->m_children->m_ok) : 0u,
                {}};
  if (object->ViewSide() != view || !WriteObject(*object, record.payload)) {
    error = "Failed to serialize a world replica object";
    return false;
  }
  records.push_back(std::move(record));
  if (object->m_children) {
    for (const auto& owned : object->m_children->Entries()) {
      if (!Collect(owned.get(), view, records, error)) {
        return false;
      }
    }
  }
  return true;
}

bool Parse(std::span<const uint8_t> bytes, Clock& clock, std::map<ObjectId, Record>& records, std::string& error)
{
  if (bytes.size() > kMaximumBytes) {
    error = "World replica exceeds size limit";
    return false;
  }
  size_t offset = 0;
  uint32_t magic = 0, version = 0, count = 0;
  if (!Read32(bytes, offset, magic) || !Read32(bytes, offset, version) || magic != kMagic || version != kVersion
      || !Read32(bytes, offset, clock.tick) || !Read32(bytes, offset, clock.day)
      || !Read32(bytes, offset, clock.serialA) || !Read32(bytes, offset, clock.serialB)
      || !Read32(bytes, offset, clock.ticksPerDay) || !Read32(bytes, offset, clock.tickOfDay)
      || !Read32(bytes, offset, clock.dayEnded) || !Read32(bytes, offset, clock.lastBattleTick)
      || !Read32(bytes, offset, count) || count > kMaximumObjects || count < 3) {
    error = "Invalid world replica header";
    return false;
  }
  for (uint32_t i = 0; i < count; ++i) {
    Record record{};
    uint32_t size = 0;
    if (!Read32(bytes, offset, record.id.key) || !Read32(bytes, offset, record.id.view)
        || !Read32(bytes, offset, record.parent) || !Read32(bytes, offset, record.hasChildren)
        || !Read32(bytes, offset, record.childListOk) || !Read32(bytes, offset, size)
        || record.id.view > 2 || record.id.key >> 24 == 0 || (record.id.key & 0xffffff) == 0
        || record.hasChildren > 1 || size < 8 || size > kMaximumObjectBytes || bytes.size() - offset < size) {
      error = "Invalid world replica object header";
      return false;
    }
    record.payload.assign(bytes.begin() + static_cast<ptrdiff_t>(offset),
                          bytes.begin() + static_cast<ptrdiff_t>(offset + size));
    offset += size;
    // All object Write implementations begin with id and packed flags.
    size_t bodyOffset = 0;
    uint32_t id = 0, flags = 0;
    std::span<const uint8_t> body(record.payload);
    if (!Read32(body, bodyOffset, id) || !Read32(body, bodyOffset, flags)
        || id != (record.id.key & 0xffffff) || ((flags >> 4) & 3) != record.id.view) {
      error = "World replica object identity differs from serialized fields";
      return false;
    }
    if (!records.emplace(record.id, std::move(record)).second) {
      error = "Duplicate world replica object";
      return false;
    }
  }
  if (offset != bytes.size()) {
    error = "Trailing bytes in world replica";
    return false;
  }
  std::map<uint32_t, uint32_t> views;
  for (const auto& [id, record] : records) {
    views[id.key] |= 1u << id.view;
    if (record.parent == id.key) {
      error = "World replica contains a parent cycle";
      return false;
    }
  }
  for (const auto& [key, mask] : views) {
    if (mask != 7) {
      error = "World replica omits an object view";
      return false;
    }
  }
  for (const auto& [id, record] : records) {
    if (record.parent) {
      auto parent = records.find({record.parent, id.view});
      if (parent == records.end() || !parent->second.hasChildren) {
        error = "World replica references a missing parent";
        return false;
      }
    }
    std::set<uint32_t> ancestors;
    uint32_t current = id.key;
    while (current) {
      if (!ancestors.insert(current).second) {
        error = "World replica contains a parent cycle";
        return false;
      }
      const auto next = records.find({current, id.view});
      if (next == records.end()) {
        break;
      }
      current = next->second.parent;
    }
  }
  return true;
}

Clock GetClock(const GameManager& manager)
{
  return {static_cast<uint32_t>(manager.m_tickCount), static_cast<uint32_t>(manager.m_day),
          static_cast<uint32_t>(manager.m_serialA), static_cast<uint32_t>(manager.m_serialB),
          manager.m_ticksPerDay, manager.m_tickOfDay, static_cast<uint32_t>(manager.m_dayEnded),
          static_cast<uint32_t>(manager.m_lastBattleTick)};
}

void SetClock(GameManager& manager, const Clock& clock)
{
  manager.m_tickCount = static_cast<int>(clock.tick);
  manager.m_day = static_cast<int>(clock.day);
  manager.m_serialA = static_cast<int>(clock.serialA);
  manager.m_serialB = static_cast<int>(clock.serialB);
  manager.m_ticksPerDay = clock.ticksPerDay;
  manager.m_tickOfDay = clock.tickOfDay;
  manager.m_dayEnded = static_cast<int>(clock.dayEnded);
  manager.m_lastBattleTick = static_cast<int>(clock.lastBattleTick);
}

} // namespace

struct WorldReplicaState::Impl
{
  std::map<ObjectId, std::vector<uint8_t>> cache;
  std::vector<std::unique_ptr<GameObjNotifier>> retired;
};

WorldReplicaState::WorldReplicaState() : impl_(std::make_unique<Impl>()) {}
WorldReplicaState::~WorldReplicaState() = default;

void WorldReplicaState::Reset()
{
  impl_->cache.clear();
  impl_->retired.clear();
}

bool WorldReplicaState::Capture(GameManager& manager, std::vector<uint8_t>& bytes, std::string& error)
{
  error.clear();
  bytes.clear();
  GameObjNotifier* root = manager.Root();
  if (!root || !root->HasAllViews()) {
    error = "World replica requires a complete game root";
    return false;
  }
  std::vector<Record> records;
  for (uint32_t view = 0; view < 3; ++view) {
    if (!Collect(view == 0 ? root : root->GetViewObject(static_cast<int>(view)), view, records, error)) {
      return false;
    }
  }
  const Clock clock = GetClock(manager);
  for (uint32_t value : {kMagic, kVersion, clock.tick, clock.day, clock.serialA, clock.serialB,
                         clock.ticksPerDay, clock.tickOfDay, clock.dayEnded, clock.lastBattleTick,
                         static_cast<uint32_t>(records.size())}) {
    Append32(bytes, value);
  }
  for (const Record& record : records) {
    for (uint32_t value : {record.id.key, record.id.view, record.parent, record.hasChildren,
                           record.childListOk, static_cast<uint32_t>(record.payload.size())}) {
      Append32(bytes, value);
    }
    bytes.insert(bytes.end(), record.payload.begin(), record.payload.end());
    if (bytes.size() > kMaximumBytes) {
      bytes.clear();
      error = "World replica exceeds size limit";
      return false;
    }
  }
  return true;
}

bool WorldReplicaState::Apply(GameManager& manager, std::span<const uint8_t> bytes, std::string& error)
{
  error.clear();
  if (!manager.Root() || !manager.Root()->HasAllViews()) {
    error = "World replica requires an initialized local game";
    return false;
  }
  Clock clock{};
  std::map<ObjectId, Record> records;
  if (!Parse(bytes, clock, records, error)) {
    return false;
  }
  ObjectKey rootKey;
  manager.Root()->GetKey(&rootKey);
  for (uint32_t view = 0; view < 3; ++view) {
    const auto it = records.find({rootKey.m_value, view});
    if (it == records.end() || it->second.parent != 0) {
      error = "World replica root does not match local game";
      return false;
    }
  }
  for (const auto& [id, record] : records) {
    if (id.key != rootKey.m_value && record.parent == 0) {
      error = "World replica contains a detached live object";
      return false;
    }
  }
  if (impl_->cache.empty()) {
    std::vector<Record> local;
    for (uint32_t view = 0; view < 3; ++view) {
      if (!Collect(view == 0 ? manager.Root() : manager.Root()->GetViewObject(static_cast<int>(view)),
                   view, local, error)) {
        return false;
      }
    }
    for (auto& record : local) {
      impl_->cache.emplace(record.id, std::move(record.payload));
    }
  }
  // Existing objects are updated first; object identity and all UI pointers remain stable.
  std::set<ObjectId> changed;
  std::set<ObjectId> added;
  std::map<ObjectId, uint32_t> movedFrom;
  for (const auto& [id, record] : records) {
    GameObjNotifier* object = FindObject(id);
    if (!object) {
      continue;
    }
    const auto cached = impl_->cache.find(id);
    if (cached != impl_->cache.end() && cached->second == record.payload) {
      continue;
    }
    GameObjNotifier* parent = object->m_parent;
    ObjectEntry* entry = object->m_entry;
    if (!entry || !entry->RemoveObject(object) || !ReadObject(*object, record.payload)) {
      error = "Failed to update an existing world replica object";
      return false;
    }
    object->m_parent = parent;
    if (object->m_entry != entry || entry->GetObject(static_cast<int>(id.view)) != object) {
      error = "World replica update broke object database linkage";
      return false;
    }
    changed.insert(id);
  }
  // Factories and object reads register newly created views in the existing database.
  for (const auto& [id, record] : records) {
    if (FindObject(id)) {
      continue;
    }
    std::unique_ptr<GameObjNotifier> created(GameObjRoot::CreateObjectOfFamily(id.key >> 24));
    if (!created || !ReadObject(*created, record.payload) || FindObject(id) != created.get()) {
      error = "Failed to create a world replica object";
      return false;
    }
    // The parent adopts ownership below, after every new view is registered.
    impl_->retired.emplace_back(std::move(created));
    changed.insert(id);
    added.insert(id);
  }
  // All objects now exist, so a parent may adopt a child even while itself unattached.
  for (const auto& [id, record] : records) {
    GameObjNotifier* object = FindObject(id);
    GameObjNotifier* parent = record.parent ? FindObject({record.parent, id.view}) : nullptr;
    if (!object || (record.parent && !parent)) {
      error = "World replica object disappeared during reconciliation";
      return false;
    }
    if (object->m_parent != parent) {
      if (!parent) {
        error = "World replica cannot detach a live object from its root";
        return false;
      }
      if (object->m_parent) {
        ObjectKey oldParent;
        object->m_parent->GetKey(&oldParent);
        movedFrom.emplace(id, oldParent.m_value);
      }
      GameObjNotifier* previous = nullptr;
      if (!parent->AddChild(object, &previous)) {
        error = "Failed to reparent a world replica object";
        return false;
      }
      auto owned = std::find_if(impl_->retired.begin(), impl_->retired.end(), [object](const auto& item) {
        return item.get() == object;
      });
      if (owned != impl_->retired.end()) {
        owned->release();
        impl_->retired.erase(owned);
      }
      changed.insert(id);
    }
  }
  // Remove obsolete leaves before their parents. Retain the objects to protect open UI windows.
  std::vector<GameObjNotifier*> obsolete;
  std::vector<std::pair<ObjectId, GameObjNotifier*>> removedForUi;
  for (const auto& [id, payload] : impl_->cache) {
    if (!records.contains(id)) {
      if (GameObjNotifier* object = FindObject(id)) {
        obsolete.push_back(object);
      }
    }
  }
  while (!obsolete.empty()) {
    bool removed = false;
    for (auto it = obsolete.begin(); it != obsolete.end();) {
      GameObjNotifier* object = *it;
      if (object->m_children && !object->m_children->Empty()) {
        ++it;
        continue;
      }
      GameObjNotifier* formerParent = object->m_parent;
      if (object->m_parent && !GameObjNotifier::RemoveFromParent(object)) {
        error = "Failed to detach a retired world replica object";
        return false;
      }
      UnregisterObject(object);
      // An open inspector can still hold this address and read its old location.
      object->m_parent = formerParent;
      if (auto* gameObject = dynamic_cast<GameObj*>(object)) {
        gameObject->m_stateFlags = (gameObject->m_stateFlags & ~0x41u) | 0x8u;
      }
      ObjectKey key;
      object->GetKey(&key);
      removedForUi.emplace_back(ObjectId{key.m_value, object->ViewSide()}, object);
      impl_->retired.emplace_back(object);
      it = obsolete.erase(it);
      removed = true;
    }
    if (!removed) {
      error = "World replica contains an obsolete parent with live children";
      return false;
    }
  }
  for (const auto& [id, record] : records) {
    GameObjNotifier* object = FindObject(id);
    if (record.hasChildren && !object->m_children) {
      object->m_children = new ChildList();
    }
    if (!record.hasChildren && object->m_children && object->m_children->Empty()) {
      delete object->m_children;
      object->m_children = nullptr;
    }
    if (object->m_children) {
      object->m_children->m_ok = static_cast<int>(record.childListOk);
    }
  }
  SetClock(manager, clock);
  impl_->cache.clear();
  for (auto& [id, record] : records) {
    impl_->cache.emplace(id, std::move(record.payload));
  }
  const int localSide = AppGetSide();
  if (localSide == 1 || localSide == 2) {
    for (const ObjectId& id : changed) {
      if (static_cast<int>(id.view) == localSide) {
        ObjectKey key = KeyOf(id.key);
        GameObjNotifier* object = FindObject(id);
        if (const auto moved = movedFrom.find(id); moved != movedFrom.end()) {
          ObjectKey oldParent = KeyOf(moved->second);
          UiBroadcastObjectMoved(&key, object, &oldParent, nullptr);
        }
        if (added.contains(id) || movedFrom.contains(id)) {
          UiBroadcastObjectChanged(&key, object);
        } else {
          UiBroadcastObjectUpdated(&key, object);
        }
      }
    }
    for (const auto& [id, object] : removedForUi) {
      if (static_cast<int>(id.view) == localSide) {
        ObjectKey key = KeyOf(id.key);
        if (object->m_parent) {
          ObjectKey formerParent;
          object->m_parent->GetKey(&formerParent);
          UiBroadcastObjectMoved(&key, nullptr, &formerParent, object->m_parent);
        }
        UiBroadcastObjectUpdated(&key, object);
      }
    }
  }
  return true;
}

} // namespace reb::multiplayer
