#include "port/native/lan_replica_smoke.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/rebellion_application.h"
#include "core/iostream_lib.h"
#include "core/memory_stream.h"
#include "core/stream_io.h"
#include "events/game_manager.h"
#include "events/game_step_driver.h"
#include "events/net_messages.h"
#include "events/notif_event_payloads.h"
#include "events/ui_notifs.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_database.h"
#include "multiplayer/message_codec.h"
#include "multiplayer/world_replica.h"
#include "orders/order_base.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/object_status_window.h"

namespace
{

void Require(bool condition, const char* message)
{
  if (!condition) {
    throw std::runtime_error(std::string("LAN replica smoke: ") + message);
  }
}

void CheckMessageCodec()
{
  RegisterOrderFactory(4, CreateNetMsg4);
  NetMsg4 original;
  original.m_voteId = 729;
  original.m_voteState = 3;
  original.m_voteTime.m_value = 431;
  original.m_voteKey = ObjectKey::Make(0x90, 17);

  ScopedStrstream legacy;
  const int kind = original.GetKind();
  StreamWrite32(legacy.get()->Out(), &kind);
  Require(original.Write(legacy.get()->Out()) != 0, "legacy message Write failed");
  const size_t legacySize = iostream_access::WrittenBytes(legacy.get()->Ios()->m_bp);
  const char* legacyBytes = strstreambuf_str(legacy.get()->Ios()->m_bp);
  Require(legacyBytes != nullptr, "legacy message produced no bytes");

  MessageCodec codec;
  Require(codec.Serialize(&original) && codec.m_valid && codec.m_size == legacySize,
          "memory message serialization failed");
  Require(std::memcmp(codec.m_buffer, legacyBytes, legacySize) == 0,
          "memory message bytes differ from legacy serialization");
  strstreambuf_freeze(legacy.get()->Ios()->m_bp, 0);

  MessageBase* decoded = nullptr;
  Require(codec.Deserialize(&decoded) && decoded != nullptr, "memory message roundtrip failed");
  std::unique_ptr<MessageBase> owned(decoded);
  auto* vote = dynamic_cast<NetMsg4*>(decoded);
  Require(vote && vote->m_voteId == original.m_voteId && vote->m_voteState == original.m_voteState
              && vote->m_voteTime.m_value == original.m_voteTime.m_value
              && vote->m_voteKey.m_value == original.m_voteKey.m_value,
          "memory message fields differ after roundtrip");

  std::vector<uint8_t> bytes(codec.m_buffer, codec.m_buffer + codec.m_size);
  bytes.push_back(0);
  codec.SetData(bytes.data(), static_cast<uint32_t>(bytes.size()));
  decoded = nullptr;
  Require(!codec.Deserialize(&decoded) && !decoded, "message accepted trailing bytes");
  bytes.pop_back();
  codec.SetData(bytes.data(), static_cast<uint32_t>(bytes.size() - 1));
  Require(!codec.Deserialize(&decoded) && !decoded, "message accepted truncated bytes");
}

class LegacyKeyPairEncoding final : public NullableKeyPairPayload
{
  public:
  int GetKind() override { return 0x300; }
  void Dispatch(PlayerController*) override {}
  void DispatchToSink(UiNotice*) override {}
};

void CheckKeyPairNoticeParity()
{
  for (uint32_t keyValue : {0u, 2u, ObjectKey::Make(0x90, 17).m_value}) {
    UiNotif300 current;
    LegacyKeyPairEncoding legacy;
    current.m_value0c = legacy.m_value0c = 19;
    current.m_value10 = legacy.m_value10 = 23;
    current.m_value14 = legacy.m_value14 = 29;
    current.m_value18 = legacy.m_value18 = 31;
    current.m_value1c = legacy.m_value1c = 37;
    current.m_key.m_value = legacy.m_key.m_value = ObjectKey::Make(0x91, 4).m_value;
    current.m_keyA.m_value = legacy.m_keyA.m_value = keyValue;
    current.m_keyB.m_value = legacy.m_keyB.m_value = keyValue == 0 ? 2 : 0;

    reb::io::MemoryWriter currentBytes;
    reb::io::MemoryWriter legacyBytes;
    current.Write(currentBytes.Out());
    Require(legacy.WritePayload(legacyBytes.Out()) && currentBytes.Good() && legacyBytes.Good()
                && std::ranges::equal(currentBytes.Bytes(), legacyBytes.Bytes()),
            "key-pair UI notice differs from legacy nullable-key encoding");

    UiNotif300 restored;
    reb::io::MemoryReader input(currentBytes.Bytes());
    restored.Read(input.In());
    Require(input.Good() && input.AtEnd() && restored.m_key.m_value == current.m_key.m_value
                && restored.m_keyA.m_value == current.m_keyA.m_value
                && restored.m_keyB.m_value == current.m_keyB.m_value
                && restored.m_value0c == current.m_value0c && restored.m_value1c == current.m_value1c,
            "key-pair UI notice roundtrip failed");
  }
}

void CheckFullSaveBytes(GameStepDriver& driver)
{
  reb::io::MemoryWriter memory(256U * 1024U * 1024U);
  Require(driver.Save(memory.Out()) != 0 && memory.Good(), "memory full-save serialization failed");

  const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto random = std::random_device{}();
  const auto path = std::filesystem::temp_directory_path()
                    / ("reb-full-save-parity-" + std::to_string(nonce) + "-" + std::to_string(random) + ".sav");
  struct RemoveTemporaryFile {
    std::filesystem::path path;
    ~RemoveTemporaryFile()
    {
      std::error_code ignored;
      std::filesystem::remove(path, ignored);
    }
  } cleanup{path};

  {
    ScopedOfstream file;
    const std::string name = path.string();
    ofstream_open(file.get(), name.c_str(), 0x82, 0x180);
    Require(iostream_access::State(file.get()->Out()) == 0, "could not open legacy save stream");
    Require(driver.Save(file.get()->Out()) != 0, "legacy full-save serialization failed");
    fstreambase_close(file.get()->Out());
    Require(iostream_access::State(file.get()->Out()) == 0, "legacy full-save stream failed");
  }

  std::ifstream file(path, std::ios::binary);
  Require(file.good(), "could not read legacy save stream");
  const std::vector<uint8_t> disk{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  Require(!file.bad() && disk.size() == memory.Bytes().size(), "full-save byte counts differ");
  Require(std::equal(disk.begin(), disk.end(), memory.Bytes().begin()), "full-save bytes differ from legacy stream");
  std::fprintf(stderr, "LAN full-save byte parity passed: %zu bytes\n", disk.size());
}

struct Sample
{
  ObjectKey key;
  std::array<GameObj*, 3> views{};
  std::array<GameObjNotifier*, 3> parents{};
};

bool FindLeaf(GameObjNotifier* object, Sample& sample)
{
  if (object != nullptr && object->m_parent && object->HasAllViews()
      && (!object->m_children || object->m_children->Empty())) {
    ObjectKey key;
    object->GetKey(&key);
    bool usable = true;
    for (int view = 0; view < 3; ++view) {
      auto* copy = dynamic_cast<GameObj*>(object->GetViewObject(view));
      if (!copy || !copy->m_parent || (copy->m_children && !copy->m_children->Empty())) {
        usable = false;
        break;
      }
      sample.views[view] = copy;
      sample.parents[view] = copy->m_parent;
    }
    if (usable) {
      sample.key = key;
      return true;
    }
  }
  if (object && object->m_children) {
    for (const auto& child : object->m_children->Entries()) {
      if (FindLeaf(child.get(), sample)) {
        return true;
      }
    }
  }
  return false;
}

GameObjNotifier* FindAlternateParent(GameObjNotifier* object, GameObjNotifier* current, uint32_t family)
{
  if (object && object != current && object->GetFamily() == family && object->HasAllViews()) {
    return object;
  }
  if (object && object->m_children) {
    for (const auto& child : object->m_children->Entries()) {
      if (GameObjNotifier* found = FindAlternateParent(child.get(), current, family)) {
        return found;
      }
    }
  }
  return nullptr;
}

void ReparentViews(const ObjectKey& key, GameObjNotifier* masterParent)
{
  for (int view = 0; view < 3; ++view) {
    GameObjNotifier* object = LookupObject(&key, view);
    GameObjNotifier* parent = masterParent->GetViewObject(view);
    Require(object && parent, "reparent target view is missing");
    GameObjNotifier* previous = nullptr;
    Require(parent->AddChild(object, &previous) != 0, "reparent failed");
  }
}

std::vector<uint8_t> SerializeObject(GameObjNotifier& object)
{
  ScopedStrstream stream;
  Require(object.Write(stream.get()->Out()) != 0, "object Write failed");
  auto* buffer = iostream_access::BufferOf(stream.get());
  const size_t size = iostream_access::WrittenBytes(buffer);
  Require(size >= 8, "object Write produced a short record");
  char* data = strstreambuf_str(buffer);
  Require(data != nullptr, "object Write produced no bytes");
  std::vector<uint8_t> bytes(data, data + size);
  strstreambuf_freeze(buffer, 0);
  return bytes;
}

void DeserializeObject(GameObjNotifier& object, const std::vector<uint8_t>& bytes)
{
  ScopedStrstream stream;
  ostream_write(stream.get()->Out(), bytes.data(), static_cast<int>(bytes.size()));
  Require(iostream_access::State(stream.get()) == 0, "failed to stage object bytes");
  istream_seekg_pos(stream.get()->In(), 0);
  Require(object.Read(stream.get()->In()) != 0 && iostream_access::State(stream.get()) == 0,
          "object Read failed");
}

std::vector<uint8_t> Capture(reb::multiplayer::WorldReplicaState& replica, GameManager& manager)
{
  std::vector<uint8_t> bytes;
  std::string error;
  Require(replica.Capture(manager, bytes, error), error.empty() ? "capture failed" : error.c_str());
  return bytes;
}

void Apply(reb::multiplayer::WorldReplicaState& replica, GameManager& manager, const std::vector<uint8_t>& bytes)
{
  std::string error;
  Require(replica.Apply(manager, bytes, error), error.empty() ? "apply failed" : error.c_str());
}

void CheckRejected(reb::multiplayer::WorldReplicaState& replica, GameManager& manager,
                   std::vector<uint8_t> bytes, const std::vector<uint8_t>& expected)
{
  std::string error;
  Require(!replica.Apply(manager, bytes, error), "malformed packet was accepted");
  Require(!error.empty(), "malformed packet produced no diagnostic");
  Require(Capture(replica, manager) == expected, "malformed packet changed the world");
}

uint32_t ChooseId(uint32_t family)
{
  for (uint32_t id = 0xfffffe; id > 0xff0000; --id) {
    const ObjectKey key = ObjectKey::Make(family, id);
    if (!LookupObject(&key, 0) && !LookupObject(&key, 1) && !LookupObject(&key, 2)) {
      return id;
    }
  }
  throw std::runtime_error("LAN replica smoke: no free temporary object id");
}

void AddClone(const Sample& sample, const ObjectKey& cloneKey)
{
  std::array<std::unique_ptr<GameObjNotifier>, 3> created;
  for (int view = 0; view < 3; ++view) {
    created[view].reset(GameObjRoot::CreateObjectOfFamily(sample.key.Family()));
    Require(created[view] != nullptr, "no factory for sample object");
    std::vector<uint8_t> bytes = SerializeObject(*sample.views[view]);
    const uint32_t id = cloneKey.Id();
    for (int offset = 0; offset < 4; ++offset) {
      bytes[offset] = static_cast<uint8_t>(id >> (offset * 8));
    }
    DeserializeObject(*created[view], bytes);
    Require(LookupObject(&cloneKey, view) == created[view].get(), "clone did not register");
  }
  for (int view = 0; view < 3; ++view) {
    GameObjNotifier* previous = nullptr;
    Require(sample.parents[view]->AddChild(created[view].get(), &previous) != 0,
            "clone did not attach to its parent");
    created[view].release();
  }
}

void RemoveClone(const ObjectKey& cloneKey)
{
  for (int view = 0; view < 3; ++view) {
    GameObjNotifier* object = LookupObject(&cloneKey, view);
    Require(object != nullptr && (!object->m_children || object->m_children->Empty()), "clone disappeared");
    Require(GameObjNotifier::RemoveFromParent(object) != 0, "clone did not detach");
    Require(UnregisterObject(object) != 0, "clone did not unregister");
    delete object;
  }
}

} // namespace

void RunLanReplicaSmoke(RebellionApplication& app)
{
  CheckMessageCodec();
  CheckKeyPairNoticeParity();
  GameStepDriver* driver = GetGameStepDriver();
  Require(driver != nullptr, "requires a loaded step driver");
  CheckFullSaveBytes(*driver);
  GameManager* manager = GetGameManager();
  Require(manager && manager->Root() && manager->Root()->HasAllViews(), "requires a loaded strategic game");
  Sample sample;
  Require(FindLeaf(manager->Root(), sample), "loaded game has no three-view leaf object");
  reb::multiplayer::WorldReplicaState replica;
  const std::vector<uint8_t> baseline = Capture(replica, *manager);
  std::array<int, 3> oldEta{};
  std::array<int, 3> oldAux{};
  const int oldDay = manager->m_day;
  for (int view = 0; view < 3; ++view) {
    oldEta[view] = sample.views[view]->m_eta;
    oldAux[view] = sample.views[view]->m_unk48;
    sample.views[view]->m_eta += 17 + view;
    sample.views[view]->m_unk48 ^= 0x1234;
  }
  manager->m_day += 2;
  const std::vector<uint8_t> changed = Capture(replica, *manager);
  Require(changed != baseline, "field mutation did not change the snapshot");
  for (int view = 0; view < 3; ++view) {
    sample.views[view]->m_eta = oldEta[view];
    sample.views[view]->m_unk48 = oldAux[view];
  }
  manager->m_day = oldDay;
  Require(Capture(replica, *manager) == baseline, "direct field rollback was incomplete");

  std::vector<uint8_t> truncated = changed;
  truncated.pop_back();
  CheckRejected(replica, *manager, std::move(truncated), baseline);
  std::vector<uint8_t> invalidCount = changed;
  invalidCount[40] = 0xff;
  invalidCount[41] = 0xff;
  invalidCount[42] = 0xff;
  invalidCount[43] = 0xff;
  CheckRejected(replica, *manager, std::move(invalidCount), baseline);
  std::vector<uint8_t> invalidLength = changed;
  invalidLength[64] = 0xff;
  invalidLength[65] = 0xff;
  invalidLength[66] = 0xff;
  invalidLength[67] = 0xff;
  CheckRejected(replica, *manager, std::move(invalidLength), baseline);

  Apply(replica, *manager, changed);
  Require(Capture(replica, *manager) == changed, "applied state differs from authoritative snapshot");
  Require(manager->m_day == oldDay + 2, "replicated day did not update");
  for (int view = 0; view < 3; ++view) {
    Require(LookupObject(&sample.key, view) == sample.views[view], "surviving object address changed");
    Require(sample.views[view]->m_parent == sample.parents[view], "surviving object parent changed");
    Require(sample.views[view]->m_entry && sample.views[view]->m_entry->GetObject(view) == sample.views[view],
            "surviving object database linkage broke");
    Require(sample.views[view]->m_eta == oldEta[view] + 17 + view
                && sample.views[view]->m_unk48 == (oldAux[view] ^ 0x1234),
            "replicated object fields differ");
  }
  Apply(replica, *manager, baseline);
  Require(Capture(replica, *manager) == baseline, "baseline did not restore exactly");

  const ObjectKey cloneKey = ObjectKey::Make(sample.key.Family(), ChooseId(sample.key.Family()));
  AddClone(sample, cloneKey);
  Require(LookupObject(&cloneKey, 0)->HasAllViews(), "new object did not link all views");
  const std::vector<uint8_t> withClone = Capture(replica, *manager);
  RemoveClone(cloneKey);
  Require(Capture(replica, *manager) == baseline, "clone removal did not restore baseline");
  Apply(replica, *manager, withClone);
  for (int view = 0; view < 3; ++view) {
    GameObjNotifier* clone = LookupObject(&cloneKey, view);
    Require(clone && clone->m_parent == sample.parents[view]
                && clone->m_entry && clone->m_entry->GetObject(view) == clone,
            "replicated new object is not linked");
  }
  Require(Capture(replica, *manager) == withClone, "new object roundtrip differs");
  GameObjNotifier* alternate = FindAlternateParent(manager->Root(), sample.parents[0],
                                                    sample.parents[0]->GetFamily());
  const bool testedReparent = alternate != nullptr;
  if (alternate) {
    std::array<GameObjNotifier*, 3> cloneAddresses{};
    for (int view = 0; view < 3; ++view) {
      cloneAddresses[view] = LookupObject(&cloneKey, view);
    }
    ReparentViews(cloneKey, alternate);
    const std::vector<uint8_t> moved = Capture(replica, *manager);
    ReparentViews(cloneKey, sample.parents[0]);
    Apply(replica, *manager, moved);
    for (int view = 0; view < 3; ++view) {
      Require(LookupObject(&cloneKey, view) == cloneAddresses[view]
                  && cloneAddresses[view]->m_parent == alternate->GetViewObject(view),
              "reparent changed object identity or chose the wrong parent");
    }
    Require(Capture(replica, *manager) == moved, "reparent roundtrip differs");
    Apply(replica, *manager, withClone);
    for (int view = 0; view < 3; ++view) {
      Require(LookupObject(&cloneKey, view) == cloneAddresses[view]
                  && cloneAddresses[view]->m_parent == sample.parents[view],
              "reverse reparent failed");
    }
  }
  std::unique_ptr<ObjectStatusWindow> heldStatus;
  GameObjNotifier* heldObject = nullptr;
  if (app.m_strategicWindow && (app.m_strategicWindow->m_side == 1 || app.m_strategicWindow->m_side == 2)) {
    const int view = app.m_strategicWindow->m_side;
    heldObject = LookupObject(&cloneKey, view);
    heldStatus = std::make_unique<ObjectStatusWindow>(app.m_strategicWindow->m_instance,
                                                       app.m_strategicWindow, 0x1a, &cloneKey);
    Require(heldStatus->m_object == heldObject, "status window did not retain the clone address");
  }
  Apply(replica, *manager, baseline);
  Require(!LookupObject(&cloneKey, 0) && !LookupObject(&cloneKey, 1) && !LookupObject(&cloneKey, 2),
          "deleted clone remains in object database");
  if (heldStatus) {
    Require(heldStatus->m_object == heldObject && heldObject->m_parent != nullptr,
            "retired object invalidated an open status window pointer");
    const uint32_t flags = static_cast<GameObj*>(heldObject)->m_stateFlags;
    Require((flags & 0x8u) != 0 && (flags & 0x41u) == 0,
            "retired object remains usable in an open status window");
    heldStatus.reset();
  }
  Require(Capture(replica, *manager) == baseline, "deletion did not restore original world");
  replica.Reset();
  std::fprintf(stderr, "LAN replica smoke passed: updates, identity, malformed packets, creation, deletion%s\n",
               testedReparent ? ", reparent" : " (no second compatible parent)");
}
