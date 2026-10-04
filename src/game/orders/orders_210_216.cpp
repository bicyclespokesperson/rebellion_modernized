#include "orders/orders_210_216.h"

#include <cstddef>

#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "data/dat_record.h"
#include "data/game_math.h"
#include "gameobj/key_queue.h"
#include "gameobj/object_database.h"
#include "gameobj/object_queries.h"
#include "orders/order_route_decl.h"

DatRecord* FUN_0051cab0(const ObjectKey* key);  // the data record of a key (via the object-type database)

namespace {

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusRefused = 1;
constexpr int kStatusTooManyCommands = 0x15;
constexpr int kStatusNoDestination = 0x22;
constexpr int kStatusWrongSide = 0x28;
constexpr int kStatusBadDestinationKind = 0x25;
constexpr int kStatusNotRecruited = 0x20;
constexpr int kStatusBusy = 0x21;

constexpr size_t kManagerStateOffset = 0x58;
constexpr size_t kUnitFlagsOffset = 0x50;
constexpr size_t kCommandDestinationOffset = 0x48;
constexpr size_t kCommandPayloadOffset = 0x50;
constexpr size_t kManagerChoicesSlotOffset = 0x1ec;

constexpr uint32_t kUnitFlagRecruited = 4;
constexpr uint32_t kUnitFlagUntraced = 8;
constexpr uint32_t kUnitFlagEnroute = 0x10;

bool isOk(const StatusCode* status) { return status->m_second == static_cast<uint32_t>(kStatusOk); }

void setStatus(StatusCode* status, int code) { *status = StatusCode(kStatusCategory, code); }

bool inFamilies(uint32_t family, uint32_t first, uint32_t end) { return family >= first && family < end; }

bool inFamilies(const ObjectKey& key, uint32_t first, uint32_t end) { return inFamilies(key.Family(), first, end); }

template <class T>
T& fieldAt(void* object, size_t offset) {
    return *reinterpret_cast<T*>(static_cast<char*>(object) + offset);
}

Command* firstCommand(CommandQueue* commands) { return static_cast<Command*>(commands->m_first); }

Command* nextCommand(Command* command) { return static_cast<Command*>(command->m_successor); }

ObjectKey commandKey(Command* command) {
    ObjectKey key;
    FUN_004fa150(command, &key);
    return key;
}

GameObjNotifier* managerOfCommand(Command* command) {
    ObjectKey key = commandKey(command);
    return GetMasterInFamilies_a0_b0(&key);
}

ObjectKey keyOfNode(AvlNode* node) {
    ObjectKey key;
    static_cast<ObjectKeyNode*>(node)->GetKey(&key);
    return key;
}

uint32_t sideIndexOf(uint32_t side) { return side == 1 ? 1 : (side == 2 ? 2 : 0); }

int callManagerVirtual(GameObjNotifier* manager, size_t byteOffset) {
    using Slot = int(__thiscall*)(void*);
    Slot* vtable = *reinterpret_cast<Slot**>(manager);
    return vtable[byteOffset / sizeof(void*)](manager);
}

bool isCapitalShip(const ObjectKey& key) { return inFamilies(key, 0x14, 0x1c); }

bool isSystem(GameObjNotifier* object) { return inFamilies(object->GetFamily(), 0x90, 0x98); }

bool isFleet(GameObjNotifier* object) { return inFamilies(object->GetFamily(), 0x08, 0x10); }

bool isCapitalShip(GameObjNotifier* object) { return inFamilies(object->GetFamily(), 0x14, 0x1c); }

}  // namespace

// ---- DestinationOrder (0x214) ---------------------------------------------------------------------------------

DestinationOrder::DestinationOrder() : OrderBase() {}

DestinationOrder::~DestinationOrder() { REB_KEEP_VPTR_STORE(); }

int DestinationOrder::GetKind() { return 0x214; }

Command* DestinationOrder::CreateCommand() {
    void* memory = ::operator new(0x58);
    return memory ? FUN_0057ddc0(memory) : nullptr;
}

void* CreateDestinationOrder() { return NewOrNull<DestinationOrder>(); }

int DestinationOrder::CheckDestination(StatusCode* status) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    ActivityRequest request;
    BuildRequest(&request);
    int ok = OrderBase::CheckDestination(status);
    uint32_t side = 0;
    if (ok != 0 && isOk(status)) ok = FUN_00553b80(m_commands, &side);

    GameObjNotifier* destination = nullptr;
    if (ok != 0) {
        StatusCode resolveStatus(kStatusCategory, kStatusOk);
        if (isOk(status)) {
            StatusCode missing(kStatusCategory, kStatusNoDestination);
            ok = ResolveMasterInFamilies_01_ff(&request.m_target, &destination, &missing, &resolveStatus);
            if (ok != 0 && destination == nullptr) *status = resolveStatus;
        }
        if (ok != 0 && isOk(status)) {
            bool onlyCapitalShips = true;
            for (Command* command = firstCommand(m_commands);
                 command && isOk(status) && onlyCapitalShips; command = nextCommand(command)) {
                GameObjNotifier* manager = managerOfCommand(command);
                if (!manager) continue;
                ObjectKeyValueSet* children = FUN_00528820(manager);
                for (AvlNode* node = children->First(); isOk(status) && node && onlyCapitalShips;
                     node = node->m_successor) {
                    if (!isCapitalShip(keyOfNode(node))) onlyCapitalShips = false;
                }
            }
            if (destination->ControlSide() != side && !onlyCapitalShips) setStatus(status, kStatusWrongSide);
        }
    }

    RouteContext route;
    if (ok != 0) {
        if (isOk(status)) {
            int mode = 3;
            ok = FUN_00551190(&route, &request, side, &mode);
        }
        if (ok != 0) {
            if (isOk(status)) ok = FUN_005513a0(&route, &request.m_target);
            if (ok != 0 && isOk(status)) {
                for (Command* command = firstCommand(m_commands); command && ok != 0 && isOk(status);
                     command = nextCommand(command)) {
                    GameObjNotifier* manager = managerOfCommand(command);
                    // BUG(original): a command whose object is not a manager is dereferenced unchecked.
                    if (fieldAt<int>(manager, kManagerStateOffset) == 0) {
                        GameObjNotifier* target = nullptr;
                        StatusCode missing(kStatusCategory, kStatusNoDestination);
                        ok = ResolveMasterInFamilies_01_ff(&request.m_target, &target, &missing, status);
                        if (ok != 0 && isOk(status)) {
                            if (inFamilies(manager->GetFamily(), 0xa0, 0xa2)) {
                                int mode = request.m_side == 1 ? 1 : (request.m_side == 2 ? 2 : 0);
                                if (isOk(status)) ok = FUN_00553410(target, mode, side, 1, 1, status);
                                if (ok == 0) continue;
                                if (isOk(status) && !isSystem(target)) setStatus(status, kStatusBadDestinationKind);
                            } else {
                                if (isOk(status)) {
                                    uint32_t flags = fieldAt<uint32_t>(target, kUnitFlagsOffset);
                                    if ((flags & kUnitFlagRecruited) == 0) {
                                        setStatus(status, kStatusNotRecruited);
                                    } else if ((flags & kUnitFlagEnroute) != 0) {
                                        setStatus(status, kStatusBusy);
                                    } else if ((flags & kUnitFlagUntraced) != 0 && !isSystem(target)) {
                                        setStatus(status, kStatusNoDestination);
                                    }
                                }
                                if (isOk(status) && !isSystem(target) && !isFleet(target) && !isCapitalShip(target))
                                    setStatus(status, kStatusBadDestinationKind);
                            }
                            if (ok != 0 && isOk(status)) {
                                ObjectKey targetKey;
                                target->GetKey(&targetKey);
                                fieldAt<ObjectKey>(command, kCommandDestinationOffset) = targetKey;
                            }
                        }
                    } else {
                        ok = FUN_00552210(&route, manager, status);
                        ObjectKeyValueSet* children = FUN_00528820(manager);
                        for (AvlNode* node = children->First(); ok != 0 && isOk(status) && node;
                             node = node->m_successor) {
                            ObjectKey legKey;
                            RouteLeg leg;
                            if (ok != 0 && isOk(status)) {
                                ObjectKey unitKey = keyOfNode(node);
                                ok = FUN_00551900(&route, &unitKey, &request.m_target, status, &leg, &legKey);
                            }
                            if (ok != 0 && isOk(status)) {
                                ObjectKey unitKey = keyOfNode(node);
                                ok = fieldAt<ObjectKeyPairSet>(command, kCommandPayloadOffset).AddPair(&legKey, &unitKey);
                            }
                        }
                    }
                }
            }
        }
    }
    if (ok == 0) setStatus(status, kStatusRefused);
    return ok;
}

// ---- RenameOrder (0x215) --------------------------------------------------------------------------------------

RenameOrder::RenameOrder() : TextMessage() {}

RenameOrder::~RenameOrder() { REB_KEEP_VPTR_STORE(); }

int RenameOrder::GetKind() { return 0x215; }

void* CreateRenameOrder() { return NewOrNull<RenameOrder>(); }

int RenameOrder::CheckEnabled(StatusCode* status) {
    int ownsQueue = static_cast<int>(m_unk1c);
    *status = StatusCode(kStatusCategory, kStatusOk);
    TextMessage::CheckEnabled(status);
    if (ownsQueue != 0 && isOk(status)) {
        if (static_cast<uint16_t>(m_commands->Count()) > 1) setStatus(status, kStatusTooManyCommands);
    }
    return isOk(status);
}

Command* RenameOrder::CreateCommand() {
    void* memory = ::operator new(0x5c);
    return memory ? FUN_0057e460(memory) : nullptr;
}

int RenameOrder::CheckDestination(StatusCode* status) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    ActivityRequest request;
    BuildRequest(&request);
    int ok = OrderBase::CheckDestination(status);
    if (ok != 0) {
        for (Command* command = firstCommand(m_commands); command && isOk(status); command = nextCommand(command))
            fieldAt<String>(command, kCommandPayloadOffset).Assign(m_text);
    }
    if (ok == 0) setStatus(status, kStatusRefused);
    return ok;
}

// ---- ManufactureOrderBase (0x6610f0) --------------------------------------------------------------------------

ManufactureOrderBase::ManufactureOrderBase() : OrderBase() {
    m_item.m_value = 0;
    m_count = 0;
}

ManufactureOrderBase::~ManufactureOrderBase() { REB_KEEP_VPTR_STORE(); }

int ManufactureOrderBase::Read(istream* in) {
    int result = OrderBase::Read(in);
    if (result != 0) {
        StreamRead32(in, &m_item);
        StreamRead32(in, &m_count);
    }
    return result;
}

int ManufactureOrderBase::Write(ostream* out) {
    int result = OrderBase::Write(out);
    if (result != 0) {
        StreamWrite32(out, &m_item);
        StreamWrite32(out, &m_count);
    }
    return result;
}

Command* ManufactureOrderBase::CreateCommand() {
    void* memory = ::operator new(0x58);
    return memory ? FUN_0057f230(memory) : nullptr;
}

uint32_t ManufactureOrderBase::FillChoices(AvlTree* choices) {
    choices->Clear();
    ActivityRequest request;
    BuildRequest(&request);
    uint32_t side = 0;
    ObjectKeyValueSet dependencies;
    StatusCode status(kStatusCategory, kStatusOk);
    uint32_t result = CollectKeyDependencies(&request, GetTeam(), &dependencies, &status);
    if (result != 0 && isOk(&status)) {
        int item = 0;
        bool first = true;
        for (AvlNode* node = dependencies.First(); node; node = node->m_successor) {
            if (inFamilies(keyOfNode(node), 0xa0, 0xb0)) {
                ObjectKey managerKey = keyOfNode(node);
                GameObjNotifier* manager = GetMasterInFamilies_a0_b0(&managerKey);
                result = manager != nullptr;
                if (result != 0 && first) {
                    side = manager->ControlSide();
                    item = callManagerVirtual(manager, kManagerChoicesSlotOffset);
                    first = false;
                }
            }
            if (result == 0) break;
        }
        if (result != 0) {
            result = !first;
            if (result != 0 && side != 3) {
                GameObjNotifier* sideObject = GetGameRootSideMemberThunk(sideIndexOf(side));
                int kind = 0;
                bool known = true;
                if (item == 0)
                    kind = 0x28;
                else if (item == 1)
                    kind = 0x2a;
                else if (item == 2)
                    kind = 0x29;
                else
                    known = false;
                result = known ? FUN_0052e580(sideObject, kind, choices) : 0;
            }
        }
    }
    return result;
}

int ManufactureOrderBase::ComputeTotals(int* out1, int* out2, int* out3, int* out4, StatusCode* status) {
    *out1 = 0;
    *out2 = 0;
    *out3 = 0;
    *out4 = 0;
    int ok = 1;
    *status = StatusCode(kStatusCategory, kStatusOk);
    ObjectKey item;
    GetItemKey(&item);
    if (inFamilies(item, 0x10, 0x40)) {
        ObjectKey recordKey;
        DatRecord* record = FUN_0051cab0(GetItemKey(&recordKey));
        ok = record != nullptr;
        if (record) {
            *out1 = static_cast<SdRecord*>(record)->GetField4c() * m_count;
            *out2 = static_cast<SdRecord*>(record)->GetField48() * m_count;
        }
    }
    CheckValid(status);
    GameObjNotifier* gameRoot = GetGameRootMember_bc();
    int result = (gameRoot != nullptr && ok != 0) ? 1 : 0;
    int peakFirst = 0;
    int peakSecond = 0;
    if (isOk(status) && gameRoot) {
        for (Command* command = firstCommand(m_commands); command && isOk(status); command = nextCommand(command)) {
            GameObjNotifier* manager = managerOfCommand(command);
            if (!manager) continue;
            int perUnit = static_cast<uint16_t>(fieldAt<AvlTree>(command, kCommandPayloadOffset).Count());
            int first = 0;
            int second = 0;
            if (isOk(status)) {
                ObjectKey firstItem;
                int accepted = FUN_00555f00(manager, GetItemKey(&firstItem), perUnit, status, &first);
                result = (accepted != 0 && result != 0) ? 1 : 0;
                if (isOk(status)) {
                    ObjectKey secondItem;
                    accepted = FUN_00555f90(manager, GetItemKey(&secondItem), status, &second);
                    result = (accepted != 0 && result != 0) ? 1 : 0;
                    if (isOk(status)) {
                        peakFirst = MaxInt(peakFirst, first);
                        peakSecond = MaxInt(peakSecond, second);
                    }
                }
            }
        }
    }
    if (result == 0) setStatus(status, kStatusRefused);
    if (isOk(status)) {
        *out3 = peakFirst;
        *out4 = peakSecond;
    }
    return result;
}

int ManufactureOrderBase::CheckDestination(StatusCode* status) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    ActivityRequest request;
    BuildRequest(&request);
    int ok = OrderBase::CheckDestination(status);
    uint32_t side = 0;
    if (ok != 0 && isOk(status)) ok = FUN_00553b80(m_commands, &side);

    if (ok != 0 && isOk(status)) {
        for (Command* command = firstCommand(m_commands); command; command = nextCommand(command)) {
            if (!(ok != 0 && isOk(status))) break;
            GameObjNotifier* manager = managerOfCommand(command);
            if (!manager) continue;
            GameObjNotifier* destination = nullptr;
            StatusCode resolveStatus(kStatusCategory, kStatusOk);
            if (isOk(status)) {
                StatusCode missing(kStatusCategory, kStatusNoDestination);
                ObjectKey locationKey;
                FUN_004c27b0(manager, &locationKey);
                ok = ResolveMasterInFamilies_01_ff(&locationKey, &destination, &missing, &resolveStatus);
                if (ok != 0 && destination == nullptr) *status = resolveStatus;
            }
            if (ok != 0 && isOk(status) && destination->ControlSide() != side) {
                ObjectKey itemKey;
                GetItemKey(&itemKey);
                if (!isCapitalShip(itemKey)) setStatus(status, kStatusWrongSide);
            }
        }
    }

    RouteContext route;
    if (ok != 0) {
        if (isOk(status)) {
            int mode = 3;
            ok = FUN_00551190(&route, &request, side, &mode);
        }
        if (ok != 0 && isOk(status)) {
            GameObjNotifier* sideObject = GetGameRootSideMember(sideIndexOf(side));
            ok = sideObject != nullptr;
            if (ok != 0) ok = FUN_0052e970(sideObject, &m_item, m_count, status);
        }
    }

    int capacity = 0;
    if (ok != 0 && isOk(status)) {
        for (Command* command = firstCommand(m_commands); command && ok != 0; command = nextCommand(command)) {
            GameObjNotifier* manager = managerOfCommand(command);
            capacity += FUN_00528040(manager, 1);
            ObjectKey locationKey;
            ok = FUN_005513a0(&route, FUN_004c27b0(manager, &locationKey));
        }
    }

    int share = 0;
    int remainder = 0;
    if (ok != 0 && isOk(status) && capacity != 0) {
        share = m_count / capacity;
        remainder = m_count % capacity;
    }

    if (ok != 0 && isOk(status)) {
        for (Command* command = firstCommand(m_commands); command && ok != 0 && isOk(status);
             command = nextCommand(command)) {
            GameObjNotifier* manager = managerOfCommand(command);
            ok = FUN_00552210(&route, manager, status);
            int toBuild = 0;
            if (ok != 0) {
                int managerCapacity = FUN_00528040(manager, 1);
                int extra = managerCapacity < remainder ? managerCapacity : remainder;
                toBuild = managerCapacity * share + extra;
                remainder -= extra;
            }
            for (int built = 0; ok != 0 && isOk(status) && built < toBuild; ++built) {
                ObjectKey legKey;
                RouteLeg leg;
                if (isOk(status)) {
                    ObjectKey originKey;
                    FUN_004c27b0(manager, &originKey);
                    ObjectKey itemKey;
                    GetItemKey(&itemKey);
                    ok = FUN_00551a20(&route, &itemKey, &originKey, status, &leg, &legKey);
                }
                if (ok != 0 && isOk(status)) {
                    ObjectKey itemKey;
                    ok = FUN_0053e020(&fieldAt<char>(command, kCommandPayloadOffset), &legKey, GetItemKey(&itemKey));
                }
            }
        }
    }
    if (ok == 0) setStatus(status, kStatusRefused);
    return ok;
}

// ---- ManufactureOrder210/211/212 --------------------------------------------------------------------------------

ManufactureOrder210::ManufactureOrder210() : ManufactureOrderBase() {}
ManufactureOrder210::~ManufactureOrder210() { REB_KEEP_VPTR_STORE(); }
int ManufactureOrder210::GetKind() { return 0x210; }
void* CreateManufactureOrder210() { return NewOrNull<ManufactureOrder210>(); }

ManufactureOrder211::ManufactureOrder211() : ManufactureOrderBase() {}
ManufactureOrder211::~ManufactureOrder211() { REB_KEEP_VPTR_STORE(); }
int ManufactureOrder211::GetKind() { return 0x211; }
void* CreateManufactureOrder211() { return NewOrNull<ManufactureOrder211>(); }

ManufactureOrder212::ManufactureOrder212() : ManufactureOrderBase() {}
ManufactureOrder212::~ManufactureOrder212() { REB_KEEP_VPTR_STORE(); }
int ManufactureOrder212::GetKind() { return 0x212; }
void* CreateManufactureOrder212() { return NewOrNull<ManufactureOrder212>(); }

// ---- Order213 -------------------------------------------------------------------------------------------------

Order213::Order213() : OrderBase() {}

Order213::~Order213() { REB_KEEP_VPTR_STORE(); }

int Order213::GetKind() { return 0x213; }

Command* Order213::CreateCommand() {
    void* memory = ::operator new(0x50);
    return memory ? FUN_0057e950(memory) : nullptr;
}

void* CreateOrder213() { return NewOrNull<Order213>(); }

// ---- Order216 -------------------------------------------------------------------------------------------------

Order216::Order216() : OrderBase() {}

Order216::~Order216() { REB_KEEP_VPTR_STORE(); }

int Order216::GetKind() { return 0x216; }

Command* Order216::CreateCommand() {
    void* memory = ::operator new(0x54);
    return memory ? FUN_0057ed20(memory) : nullptr;
}

void* CreateOrder216() { return NewOrNull<Order216>(); }

int Order216::CheckDestination(StatusCode* status) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    ActivityRequest request;
    BuildRequest(&request);
    int ok = OrderBase::CheckDestination(status);
    int flag = 0;
    if (ok != 0) {
        if (isOk(status)) flag = IsChecked() == 0;
        if (isOk(status)) {
            for (Command* command = firstCommand(m_commands); command && isOk(status); command = nextCommand(command))
                fieldAt<int>(command, kCommandPayloadOffset) = flag;
        }
    }
    if (ok == 0) setStatus(status, kStatusRefused);
    return ok;
}
