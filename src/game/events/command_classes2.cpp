#include "events/command_classes2.h"
#include "events/command_classes2_decl.h"

#include "data/dat_record.h"
#include "events/event_helpers.h"
#include "events/event_post.h"
#include "gameobj/object_lookup.h"
#include "orders/route_capacity.h"
#include "orders/route_context.h"

using events::asIntArg;
using events::callVirtualSlot;
using events::checkOnTarget;
using events::executeOnTarget;
using events::kStatusCategory;
using events::kStatusMissingTarget;
using events::kStatusNoDestination;
using events::kStatusNoRoom;
using events::kStatusOk;
using events::kStatusRefused;
using events::succeeded;
using events::succeedWithoutChecks;

namespace {

constexpr int kManagerStateOffset = 0x58;

template <class T>
T& fieldAt(void* object, int offset) {
    return *reinterpret_cast<T*>(static_cast<char*>(object) + offset);
}

template <class T>
T& fieldAt(GameObjNotifier* object, int offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(object) + offset);
}

// An entry of a manufacture command's item set: the destination key at +0x18 and the item key at +0x1c.
struct ObjectKeyNodeView {
    explicit ObjectKeyNodeView(AvlNode* node) : m_node(static_cast<ObjectKeyPairNode*>(node)) {}
    ObjectKey* destination() const { return &m_node->m_key; }
    ObjectKey* unit() const { return &m_node->m_second; }
    ObjectKeyPairNode* m_node;
};

}  // namespace


MoveCommand::MoveCommand() : Command() {}

MoveCommand::MoveCommand(const MoveCommand& other) : Command(other) {}

MoveCommand::~MoveCommand() { REB_KEEP_VPTR_STORE(); }

MoveCommand* MoveCommand::Clone() { return NewOrNull<MoveCommand>(*this); }

int MoveCommand::CheckEnabled(StatusCode* status) {
    return checkOnTarget(this, ResolveMasterInFamilies_01_ff, status, [this, status](GameObj* object) {
        int ok = object->CheckUnknownImpl(&m_request, status);
        if (ok && succeeded(*status)) {
            ObjectKey key;
            ok = CheckMoveEnabled(object->GetKey(&key), status);
        }
        return ok;
    });
}

int MoveCommand::CheckValid(StatusCode* status) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    GameObjNotifier* object = nullptr;
    int ok = 1;
    if (succeeded(*status)) ok = GetGameRootMember_bc() != nullptr;
    if (ok && succeeded(*status)) {
        const StatusCode missing(kStatusCategory, kStatusMissingTarget);
        ok = ResolveMasterInFamilies_01_ff(&m_key, &object, &missing, status);
    }
    GameObjNotifier* destination = nullptr;
    StatusCode destinationStatus(kStatusCategory, kStatusOk);
    if (ok) {
        if (succeeded(*status)) {
            const StatusCode noDestination(kStatusCategory, kStatusNoDestination);
            ok = ResolveMasterInFamilies_01_ff(&m_request.m_target, &destination, &noDestination, &destinationStatus);
            if (!ok) return 0;
            if (!destination) *status = destinationStatus;
        }
        if (succeeded(*status)) ok = static_cast<GameObj*>(object)->CheckUnknown(&m_request, status);
        if (ok) {
            ObjectKey destinationKey;
            if (succeeded(*status)) ok = FUN_005542f0(object, destination->GetKey(&destinationKey), &m_request, status);
            if (ok && succeeded(*status)) {
                ObjectKey objectKey;
                ObjectKey target;
                ok = CheckMoveValid(object->GetKey(&objectKey), destination->GetKey(&target), status);
            }
        }
    }
    return ok;
}

int MoveCommand::Execute() {
    NotifyContext ctx(m_context);
    GameObjNotifier* object = nullptr;
    StatusCode status(kStatusCategory, kStatusOk);
    const StatusCode missing(kStatusCategory, kStatusMissingTarget);
    int ok = ResolveMasterInFamilies_01_ff(&m_key, &object, &missing, &status);
    GameObjNotifier* destination = nullptr;
    StatusCode destinationStatus(kStatusCategory, kStatusOk);
    if (ok) {
        if (succeeded(status)) {
            const StatusCode noDestination(kStatusCategory, kStatusNoDestination);
            ok = ResolveMasterInFamilies_01_ff(&m_request.m_target, &destination, &noDestination, &destinationStatus);
            if (!ok) return 0;
            if (!destination) status = destinationStatus;
        }
        if (ok && succeeded(status)) {
            ok = CheckValid(&status);
            if (ok && succeeded(status)) {
                ObjectKey destinationKey;
                ObjectKey objectKey;
                ExecuteMove(object->GetKey(&objectKey), destination->GetKey(&destinationKey), &ctx);
            }
        }
    }
    return 0;
}

MoveCommand* CreateMoveCommand() { return NewOrNull<MoveCommand>(); }

RenameCommand::RenameCommand() : Command(), m_name() {}

RenameCommand::RenameCommand(const RenameCommand& other) : Command(other), m_name(other.m_name) {}

RenameCommand::~RenameCommand() { REB_KEEP_VPTR_STORE(); }

RenameCommand* RenameCommand::Clone() { return NewOrNull<RenameCommand>(*this); }

void RenameCommand::Read(istream* in) {
    ReadCommand(in);
    m_name.Read(in);
}

void RenameCommand::Write(ostream* out) {
    WriteCommand(out);
    m_name.Write(out);
}

int RenameCommand::CheckEnabled(StatusCode* status) {
    return checkOnTarget(this, ResolveMasterInFamilies_01_ff, status, [this, status](GameObj* object) { return object->CheckRenameImpl(&m_request, status); });
}

int RenameCommand::CheckValid(StatusCode* status) {
    return checkOnTarget(this, ResolveMasterInFamilies_01_ff, status, [this, status](GameObj* object) { return object->CheckRename(&m_request, asIntArg(&m_name), status); });
}

int RenameCommand::Execute() {
    return executeOnTarget(this, ResolveMasterInFamilies_01_ff, [this](GameObj* object, NotifyContext* ctx) { object->TryRename(&m_request, asIntArg(&m_name), ctx); });
}

RenameCommand* CreateRenameCommand() { return NewOrNull<RenameCommand>(); }

ScrapCommand::ScrapCommand() : Command() {}

ScrapCommand::ScrapCommand(const ScrapCommand& other) : Command(other) {}

ScrapCommand::~ScrapCommand() { REB_KEEP_VPTR_STORE(); }

ScrapCommand* ScrapCommand::Clone() { return NewOrNull<ScrapCommand>(*this); }

int ScrapCommand::CheckEnabled(StatusCode* status) {
    return checkOnTarget(this, ResolveMasterInFamilies_01_ff, status, [this, status](GameObj* object) { return object->CheckRequestImpl(&m_request, status); });
}

int ScrapCommand::CheckValid(StatusCode* status) {
    return checkOnTarget(this, ResolveMasterInFamilies_01_ff, status, [this, status](GameObj* object) { return object->CheckRequest(&m_request, status); });
}

int ScrapCommand::Execute() {
    return executeOnTarget(this, ResolveMasterInFamilies_01_ff, [this](GameObj* object, NotifyContext* ctx) { object->DestroyIfAllowed(&m_request, ctx); });
}

ScrapCommand* CreateScrapCommand() { return NewOrNull<ScrapCommand>(); }

ReadyForDeleteCommand::ReadyForDeleteCommand() : Command() {}

ReadyForDeleteCommand::ReadyForDeleteCommand(const ReadyForDeleteCommand& other) : Command(other) {}

ReadyForDeleteCommand::~ReadyForDeleteCommand() { REB_KEEP_VPTR_STORE(); }

ReadyForDeleteCommand* ReadyForDeleteCommand::Clone() { return NewOrNull<ReadyForDeleteCommand>(*this); }

int ReadyForDeleteCommand::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int ReadyForDeleteCommand::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int ReadyForDeleteCommand::Execute() {
    NotifyContext ctx(m_context);
    GameObj* object = static_cast<GameObj*>(GetObjectInFamilies_01_ff(m_request.m_side, &m_key));
    if (object) object->ReleaseIfDestroyed(&ctx);
    return 0;
}

ReadyForDeleteCommand* CreateReadyForDeleteCommand() { return NewOrNull<ReadyForDeleteCommand>(); }

RootCommand240::RootCommand240() : Command() {}

RootCommand240::RootCommand240(const RootCommand240& other) : Command(other) {}

RootCommand240::~RootCommand240() { REB_KEEP_VPTR_STORE(); }

RootCommand240* RootCommand240::Clone() { return NewOrNull<RootCommand240>(*this); }

int RootCommand240::IsChecked() {
    int result = 0;
    void* manager = GetGameRootMember_c0();
    if (manager) {
        ObjectKey key = m_key;
        callVirtualSlot<int>(manager, 111, &key, &m_request, &result);
    }
    return result;
}

int RootCommand240::CheckEnabled(StatusCode* status) {
    *status = StatusCode(kStatusCategory, kStatusRefused);
    void* manager = GetGameRootMember_c0();
    int ok = 0;
    if (manager) {
        ObjectKey key = m_key;
        ok = callVirtualSlot<int>(manager, 112, &key, &m_request, status);
    }
    return ok;
}

int RootCommand240::CheckValid(StatusCode* status) {
    *status = StatusCode(kStatusCategory, kStatusRefused);
    void* manager = GetGameRootMember_c0();
    int ok = 0;
    if (manager) {
        ObjectKey key = m_key;
        ok = callVirtualSlot<int>(manager, 113, &key, &m_request, status);
    }
    return ok;
}

int RootCommand240::Execute() {
    NotifyContext ctx(m_context);
    void* manager = GetGameRootMember_c0();
    if (manager) {
        ObjectKey key = m_key;
        callVirtualSlot<int>(manager, 116, &key, &m_request, &ctx);
    }
    return 0;
}

RootCommand240* CreateRootCommand240() { return NewOrNull<RootCommand240>(); }

RootCommand242::RootCommand242() : Command() {}

RootCommand242::RootCommand242(const RootCommand242& other) : Command(other) {}

RootCommand242::~RootCommand242() { REB_KEEP_VPTR_STORE(); }

RootCommand242* RootCommand242::Clone() { return NewOrNull<RootCommand242>(*this); }

int RootCommand242::CheckEnabled(StatusCode* status) {
    *status = StatusCode(kStatusCategory, kStatusRefused);
    void* manager = GetGameRootMember_c0();
    int ok = 0;
    if (manager) {
        ObjectKey key = m_key;
        ok = callVirtualSlot<int>(manager, 114, &key, &m_request, status);
    }
    return ok;
}

int RootCommand242::CheckValid(StatusCode* status) {
    *status = StatusCode(kStatusCategory, kStatusRefused);
    void* manager = GetGameRootMember_c0();
    int ok = 0;
    if (manager) {
        ObjectKey key = m_key;
        ok = callVirtualSlot<int>(manager, 115, &key, &m_request, status);
    }
    return ok;
}

int RootCommand242::Execute() {
    NotifyContext ctx(m_context);
    void* manager = GetGameRootMember_c0();
    if (manager) {
        ObjectKey key = m_key;
        callVirtualSlot<int>(manager, 117, &key, &m_request, &ctx);
    }
    return 0;
}

RootCommand242* CreateRootCommand242() { return NewOrNull<RootCommand242>(); }

DestinationCommand::DestinationCommand() : Command(), m_pairs() {}

DestinationCommand::DestinationCommand(const DestinationCommand& other) : Command(other), m_pairs(other.m_pairs) {}

DestinationCommand::~DestinationCommand() { REB_KEEP_VPTR_STORE(); }

DestinationCommand* DestinationCommand::Clone() { return NewOrNull<DestinationCommand>(*this); }

void DestinationCommand::Read(istream* in) {
    ReadCommand(in);
    m_pairs.Load(in);
}

void DestinationCommand::Write(ostream* out) {
    WriteCommand(out);
    m_pairs.Save(out);
}

void DestinationCommand::Checksum(Crc32* crc) {
    ChecksumCommand(crc);
    m_pairs.Checksum(crc);
}

int DestinationCommand::CheckEnabled(StatusCode* status) {
    return checkOnTarget(this, ResolveMasterInFamilies_a0_b0, status, [this, status](GameObj* object) { return callVirtualSlot<int>(object, 120, &m_request, status); });
}

int DestinationCommand::CheckValid(StatusCode* status) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    GameObjNotifier* manager = nullptr;
    int ok = 1;
    if (succeeded(*status)) {
        const StatusCode missing(kStatusCategory, kStatusMissingTarget);
        ok = ResolveMasterInFamilies_a0_b0(&m_key, &manager, &missing, status);
    }
    GameObjNotifier* destination = nullptr;
    StatusCode destinationStatus(kStatusCategory, kStatusOk);
    if (!ok) return 0;
    if (succeeded(*status)) {
        const StatusCode noDestination(kStatusCategory, kStatusNoDestination);
        ok = ResolveMasterInFamilies_01_ff(&m_request.m_target, &destination, &noDestination, &destinationStatus);
        if (!ok) return 0;
        if (!destination) *status = destinationStatus;
    }
    if (!ok || !succeeded(*status)) return ok;
    ok = callVirtualSlot<int>(manager, 121, &m_request, &m_pairs, status);
    if (!ok || !succeeded(*status) || fieldAt<int>(manager, kManagerStateOffset) == 0) return ok;
    RouteContext route;
    if (succeeded(*status)) {
        int mode = 3;
        ok = route.InitFromRequest(&m_request, manager->ControlSide(), &mode);
        if (ok) {
            ObjectKey destinationKey;
            ok = route.AddContainerLegs(destination->GetKey(&destinationKey));
            if (ok) ok = route.FreeCarriedParents(static_cast<GameObj*>(manager), status);
        }
    }
    AvlNode* node = m_pairs.First();
    if (!ok) {
        *status = StatusCode(kStatusCategory, kStatusRefused);
    } else {
        do {
            if (!succeeded(*status) || !node) break;
            CapacityScratch slot;
            if (ok && succeeded(*status)) {
                ObjectKeyPairNode* pair = static_cast<ObjectKeyPairNode*>(node);
                ok = route.CheckUnitArrival(&pair->m_second, &pair->m_key, status, &slot);
            }
            node = node->m_successor;
        } while (ok);
        if (!ok) *status = StatusCode(kStatusCategory, kStatusRefused);
    }
    return ok;
}

int DestinationCommand::Execute() {
    return executeOnTarget(this, ResolveMasterInFamilies_a0_b0, [this](GameObj* object, NotifyContext* ctx) { callVirtualSlot<int>(object, 128, &m_request, &m_pairs, ctx); });
}

DestinationCommand* CreateDestinationCommand() { return NewOrNull<DestinationCommand>(); }

ManagerRenameCommand::ManagerRenameCommand() : Command(), m_name() {}

ManagerRenameCommand::ManagerRenameCommand(const ManagerRenameCommand& other) : Command(other), m_name(other.m_name) {}

ManagerRenameCommand::~ManagerRenameCommand() { REB_KEEP_VPTR_STORE(); }

ManagerRenameCommand* ManagerRenameCommand::Clone() { return NewOrNull<ManagerRenameCommand>(*this); }

void ManagerRenameCommand::Read(istream* in) {
    ReadCommand(in);
    m_name.Read(in);
}

void ManagerRenameCommand::Write(ostream* out) {
    WriteCommand(out);
    m_name.Write(out);
}

int ManagerRenameCommand::CheckEnabled(StatusCode* status) {
    return checkOnTarget(this, ResolveMasterInFamilies_a0_b0, status, [this, status](GameObj* object) { return callVirtualSlot<int>(object, 113, &m_request, status); });
}

int ManagerRenameCommand::CheckValid(StatusCode* status) {
    return checkOnTarget(this, ResolveMasterInFamilies_a0_b0, status, [this, status](GameObj* object) { return callVirtualSlot<int>(object, 114, &m_request, &m_name, status); });
}

int ManagerRenameCommand::Execute() {
    return executeOnTarget(this, ResolveMasterInFamilies_a0_b0, [this](GameObj* object, NotifyContext* ctx) { callVirtualSlot<int>(object, 125, &m_request, &m_name, ctx); });
}

ManagerRenameCommand* CreateManagerRenameCommand() { return NewOrNull<ManagerRenameCommand>(); }

ManagerCommand213::ManagerCommand213() : Command() {}

ManagerCommand213::ManagerCommand213(const ManagerCommand213& other) : Command(other) {}

ManagerCommand213::~ManagerCommand213() { REB_KEEP_VPTR_STORE(); }

ManagerCommand213* ManagerCommand213::Clone() { return NewOrNull<ManagerCommand213>(*this); }

int ManagerCommand213::CheckEnabled(StatusCode* status) {
    return checkOnTarget(this, ResolveMasterInFamilies_a0_b0, status, [this, status](GameObj* object) { return callVirtualSlot<int>(object, 115, &m_request, status); });
}

int ManagerCommand213::CheckValid(StatusCode* status) {
    return checkOnTarget(this, ResolveMasterInFamilies_a0_b0, status, [this, status](GameObj* object) { return callVirtualSlot<int>(object, 116, &m_request, status); });
}

int ManagerCommand213::Execute() {
    return executeOnTarget(this, ResolveMasterInFamilies_a0_b0, [this](GameObj* object, NotifyContext* ctx) { callVirtualSlot<int>(object, 126, &m_request, ctx); });
}

ManagerCommand213* CreateManagerCommand213() { return NewOrNull<ManagerCommand213>(); }

ManagerCommand216::ManagerCommand216() : Command(), m_value(0) {}

ManagerCommand216::ManagerCommand216(const ManagerCommand216& other) : Command(other), m_value(other.m_value) {}

ManagerCommand216::~ManagerCommand216() { REB_KEEP_VPTR_STORE(); }

ManagerCommand216* ManagerCommand216::Clone() { return NewOrNull<ManagerCommand216>(*this); }

void ManagerCommand216::Read(istream* in) {
    ReadCommand(in);
    ReadValue32(in, &m_value);
}

void ManagerCommand216::Write(ostream* out) {
    WriteCommand(out);
    WriteValue32(out, &m_value);
}

void ManagerCommand216::Checksum(Crc32* crc) {
    ChecksumCommand(crc);
    crc->AddInt(m_value);
}

int ManagerCommand216::IsChecked() {
    int result = 0;
    GameObjNotifier* manager = nullptr;
    StatusCode status(kStatusCategory, kStatusOk);
    const StatusCode missing(kStatusCategory, kStatusMissingTarget);
    const int ok = ResolveMasterInFamilies_a0_b0(&m_key, &manager, &missing, &status);
    if (ok && succeeded(status)) callVirtualSlot<int>(manager, 117, &m_request, &result);
    return result;
}

int ManagerCommand216::CheckEnabled(StatusCode* status) {
    return checkOnTarget(this, ResolveMasterInFamilies_a0_b0, status, [this, status](GameObj* object) { return callVirtualSlot<int>(object, 118, &m_request, m_value, status); });
}

int ManagerCommand216::CheckValid(StatusCode* status) {
    return checkOnTarget(this, ResolveMasterInFamilies_a0_b0, status, [this, status](GameObj* object) { return callVirtualSlot<int>(object, 119, &m_request, m_value, status); });
}

int ManagerCommand216::Execute() {
    return executeOnTarget(this, ResolveMasterInFamilies_a0_b0, [this](GameObj* object, NotifyContext* ctx) { callVirtualSlot<int>(object, 127, &m_request, m_value, ctx); });
}

ManagerCommand216* CreateManagerCommand216() { return NewOrNull<ManagerCommand216>(); }

ManufactureCommand::ManufactureCommand() : Command(), m_items() {}

ManufactureCommand::ManufactureCommand(const ManufactureCommand& other) : Command(other), m_items(other.m_items) {}

ManufactureCommand::~ManufactureCommand() { REB_KEEP_VPTR_STORE(); }

ManufactureCommand* ManufactureCommand::Clone() { return NewOrNull<ManufactureCommand>(*this); }

void ManufactureCommand::Read(istream* in) {
    ReadCommand(in);
    m_items.Load(in);
}

void ManufactureCommand::Write(ostream* out) {
    WriteCommand(out);
    m_items.Save(out);
}

void ManufactureCommand::Checksum(Crc32* crc) {
    ChecksumCommand(crc);
    m_items.Checksum(crc);
}

int ManufactureCommand::CheckEnabled(StatusCode* status) {
    return checkOnTarget(this, ResolveMasterInFamilies_a0_b0, status, [this, status](GameObj* object) { return callVirtualSlot<int>(object, 111, &m_request, status); });
}

int ManufactureCommand::CheckValid(StatusCode* status) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    GameObjNotifier* manager = nullptr;
    int ok = 1;
    if (succeeded(*status)) {
        const StatusCode missing(kStatusCategory, kStatusMissingTarget);
        ok = ResolveMasterInFamilies_a0_b0(&m_key, &manager, &missing, status);
    }
    if (ok) {
        if (succeeded(*status)) {
            ok = callVirtualSlot<int>(manager, 112, &m_request, &m_items, status);
            if (ok && succeeded(*status)) {
                RouteContext route;
                ObjectKey managerKey;
                if (succeeded(*status)) {
                    int mode = 3;
                    ok = route.InitFromRequest(&m_request, manager->ControlSide(), &mode);
                    if (ok) {
                        ok = route.AddContainerLegs(FUN_004c27b0(manager, &managerKey));
                        if (ok) ok = route.FreeCarriedParents(static_cast<GameObj*>(manager), status);
                    }
                }
                ObjectKey previous;
                previous.m_value = 0;
                int total = 0;
                // BUG(original): the per-item count shares its stack slot with the manager key above and keeps that
                // value for items that neither differ from the previous one nor have a record.
                int count = static_cast<int>(managerKey.m_value);
                for (AvlNode* node = m_items.First(); ok && succeeded(*status) && node; node = node->m_successor) {
                    CapacityScratch slot;
                    ObjectKeyNodeView item(node);
                    if (ok) {
                        if (succeeded(*status)) ok = route.CheckFamilyArrival(item.unit(), item.destination(), status, &slot);
                        if (ok && succeeded(*status)) {
                            if (item.unit()->m_value != previous.m_value) {
                                previous = *item.unit();
                                if (previous.Family() >= 0x10 && previous.Family() < 0x40) {
                                    SdRecord* record = static_cast<SdRecord*>(FUN_0051cab0(&previous));
                                    ok = record != nullptr;
                                    if (ok) count = static_cast<int>(record->GetField4c());
                                }
                            }
                            if (ok) total += count;
                        }
                    }
                }
                if (ok && succeeded(*status)) {
                    void* sideObject = nullptr;
                    ok = FUN_00518750(manager, &sideObject);
                    if (total != 0 && fieldAt<int>(sideObject, 0x58) - fieldAt<int>(sideObject, 0x5c) < total)
                        *status = StatusCode(kStatusNoRoom, kStatusRefused);
                }
            }
        }
        if (ok) return ok;
    }
    *status = StatusCode(kStatusCategory, kStatusRefused);
    return 0;
}

int ManufactureCommand::Execute() {
    return executeOnTarget(this, ResolveMasterInFamilies_a0_b0, [this](GameObj* object, NotifyContext* ctx) { callVirtualSlot<int>(object, 124, &m_request, &m_items, ctx); });
}

ManufactureCommand* CreateManufactureCommand() { return NewOrNull<ManufactureCommand>(); }

FleetCommand221::FleetCommand221() : Command() {}

FleetCommand221::FleetCommand221(const FleetCommand221& other) : Command(other) {}

FleetCommand221::~FleetCommand221() { REB_KEEP_VPTR_STORE(); }

FleetCommand221* FleetCommand221::Clone() { return NewOrNull<FleetCommand221>(*this); }

int FleetCommand221::CheckEnabled(StatusCode* status) {
    return checkOnTarget(this, ResolveMasterInFamilies_08_10, status, [this, status](GameObj* object) { return callVirtualSlot<int>(object, 111, &m_request, status); });
}

int FleetCommand221::CheckValid(StatusCode* status) {
    return checkOnTarget(this, ResolveMasterInFamilies_08_10, status, [this, status](GameObj* object) { return callVirtualSlot<int>(object, 112, &m_request, status); });
}

int FleetCommand221::Execute() {
    return executeOnTarget(this, ResolveMasterInFamilies_08_10, [this](GameObj* object, NotifyContext* ctx) { callVirtualSlot<int>(object, 116, &m_request, ctx); });
}

FleetCommand221* CreateFleetCommand221() { return NewOrNull<FleetCommand221>(); }

FleetCommand220::FleetCommand220() : Command() {}

FleetCommand220::FleetCommand220(const FleetCommand220& other) : Command(other) {}

FleetCommand220::~FleetCommand220() { REB_KEEP_VPTR_STORE(); }

FleetCommand220* FleetCommand220::Clone() { return NewOrNull<FleetCommand220>(*this); }

int FleetCommand220::CheckEnabled(StatusCode* status) {
    return checkOnTarget(this, ResolveMasterInFamilies_08_10, status, [this, status](GameObj* object) { return callVirtualSlot<int>(object, 113, &m_request, status); });
}

int FleetCommand220::CheckValid(StatusCode* status) {
    return checkOnTarget(this, ResolveMasterInFamilies_08_10, status, [this, status](GameObj* object) { return callVirtualSlot<int>(object, 114, &m_request, status); });
}

int FleetCommand220::Execute() {
    return executeOnTarget(this, ResolveMasterInFamilies_08_10, [this](GameObj* object, NotifyContext* ctx) { callVirtualSlot<int>(object, 117, &m_request, ctx); });
}

FleetCommand220* CreateFleetCommand220() { return NewOrNull<FleetCommand220>(); }

PayloadEvent3f0::PayloadEvent3f0() : TimedEvent(), m_valueA(0), m_valueB(0), m_valueC(0) {}

PayloadEvent3f0::PayloadEvent3f0(const PayloadEvent3f0& other) : TimedEvent(other), m_valueA(other.m_valueA), m_valueB(other.m_valueB), m_valueC(other.m_valueC) {}

PayloadEvent3f0::~PayloadEvent3f0() { REB_KEEP_VPTR_STORE(); }

PayloadEvent3f0* PayloadEvent3f0::Clone() { return NewOrNull<PayloadEvent3f0>(*this); }

void PayloadEvent3f0::Read(istream* in) {
    ReadTimed(in);
    ReadValue32(in, &m_valueA);
    ReadValue32(in, &m_valueB);
    ReadValue32(in, &m_valueC);
}

void PayloadEvent3f0::Write(ostream* out) {
    WriteTimed(out);
    WriteValue32(out, &m_valueA);
    WriteValue32(out, &m_valueB);
    WriteValue32(out, &m_valueC);
}

void PayloadEvent3f0::Checksum(Crc32* crc) {
    ChecksumTimed(crc);
    crc->AddInt(m_valueA);
    crc->AddInt(m_valueB);
    crc->AddInt(m_valueC);
}

int PayloadEvent3f0::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int PayloadEvent3f0::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int PayloadEvent3f0::Execute() {
    GameObjNotifier* target = nullptr;
    StatusCode status(kStatusCategory, kStatusOk);
    const StatusCode missing(kStatusCategory, kStatusMissingTarget);
    ResolveMasterInFamilies_01_ff(&m_key, &target, &missing, &status);
    if (target) {
        NotifyContext ctx(m_context);
        PostPayloadFourInts(1, target, 0, 0, m_valueA, m_valueB, &ctx);
    }
    return 0;
}

PayloadEvent3f0::PayloadEvent3f0(int valueA, int valueB, int valueC)
    : TimedEvent(), m_valueA(valueA), m_valueB(valueB), m_valueC(valueC) {}

PayloadEvent3f0* CreatePayloadEvent3f0() { return NewOrNull<PayloadEvent3f0>(); }
