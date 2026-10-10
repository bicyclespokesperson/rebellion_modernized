#include "orders/order_checks.h"

#include "gameobj/child_iterator.h"
#include "gameobj/game_hooks.h"
#include "gameobj/object_database.h"
#include "gameobj/object_queries.h"

namespace {

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusRefused = 1;
constexpr int kStatusMissingObject = 0x12;
constexpr int kStatusWrongSide = 0x14;
constexpr int kStatusNoCommands = 0x16;
constexpr int kStatusNotCompleted = 0x20;
constexpr int kStatusEnroute = 0x21;
constexpr int kStatusDestroyed = 0x22;
constexpr int kStatusWrongControl = 0x24;
constexpr int kStatusBadObjectKind = 0x25;
constexpr int kStatusNotInTeam = 0x26;
constexpr int kStatusNotAccepted = 0x27;
constexpr int kStatusEnemyDestination = 0x28;
constexpr uint32_t kFlagCompleted = 0x4;
constexpr uint32_t kFlagDestroyed = 0x8;
constexpr uint32_t kFlagEnroute = 0x10;

bool isOk(const StatusCode& status) { return status.m_second == static_cast<uint32_t>(kStatusOk); }

StatusCode okStatus() { return StatusCode(kStatusCategory, kStatusOk); }

void setStatus(StatusCode* status, int code) { *status = StatusCode(kStatusCategory, code); }

}  // namespace

int ClassifyCommands(ActivityRequest* request, CommandQueue* commands, int* anyChecked, int* anyUnchecked) {
    *anyChecked = 0;
    *anyUnchecked = 0;
    for (Command* command = commands->FirstCommand(); command; command = static_cast<Command*>(command->m_successor)) {
        if (command->IsChecked() == 0)
            *anyUnchecked = 1;
        else
            *anyChecked = 1;
    }
    return 1;
}

int CheckCommandsEnabled(ActivityRequest* request, CommandQueue* commands, StatusCode* status) {
    int ok = 1;
    *status = okStatus();
    if (isOk(*status) && commands->Count() == 0) setStatus(status, kStatusNoCommands);
    if (!isOk(*status)) return ok;
    int first = 1;
    uint32_t firstSide = 1;
    uint32_t side = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(commands));
    for (Command* command = commands->FirstCommand(); command && ok != 0 && isOk(*status);
         command = static_cast<Command*>(command->m_successor)) {
        ok = command->CheckEnabled(status);
        if (ok != 0 && isOk(*status)) {
            GameObjNotifier* object = nullptr;
            const StatusCode missing(kStatusCategory, kStatusMissingObject);
            StatusCode resolveStatus;
            ok = ResolveMasterInFamilies_01_ff(&command->m_key, &object, &missing, &resolveStatus);
            if (ok != 0) {
                if (object == nullptr)
                    *status = resolveStatus;
                else
                    side = object->ControlSide();
            }
        }
        if (ok != 0 && isOk(*status)) {
            if (first != 0) {
                first = 0;
                firstSide = side;
            }
            if (side != firstSide) setStatus(status, kStatusWrongSide);
        }
    }
    return ok;
}

int CheckCommandsValid(ActivityRequest* request, CommandQueue* commands, StatusCode* status) {
    int ok = 1;
    *status = okStatus();
    for (Command* command = commands->FirstCommand(); command && ok != 0 && isOk(*status);
         command = static_cast<Command*>(command->m_successor)) {
        ok = command->CheckValid(status);
    }
    return ok;
}

int ExecuteCommands(ActivityRequest* request, CommandQueue* commands) {
    while (Command* command = commands->FirstCommand()) {
        commands->Remove(command);
        ActivityRequest copy(command->m_request);
        copy.m_unk04 = request->m_unk04;
        command->SetRequest(&copy);
        command->StampContext();
        CallGameHook2B00(reinterpret_cast<HookArg>(command));
    }
    return 1;
}

int CheckCommandForTeam(Command* prototype, ObjectKeyValueSet* team, CommandQueue* commands, StatusCode* status) {
    commands->Clear();
    *status = okStatus();
    ObjectKeyValueSet keys;
    int ok = CollectKeyDependencies(&prototype->m_request, team, &keys, status);
    if (ok != 0 && isOk(*status)) {
        for (ObjectKeyNode* node = keys.First(); node && ok != 0; node = static_cast<ObjectKeyNode*>(node->m_successor)) {
            Command* command = prototype->Clone();
            ok = command != nullptr;
            if (command) {
                ok = command->m_flag;
                if (ok == 0) {
                    delete command;
                } else {
                    ObjectKey key;
                    command->m_key = *node->GetKey(&key);
                    commands->Insert(command);
                }
            }
        }
    }
    if (ok == 0) setStatus(status, kStatusRefused);
    return ok;
}

int RefuseDestinationInTeam(const ActivityRequest* request, ObjectKeySet* team, StatusCode* status) {
    const bool badKind = status->m_first == 1 && (status->m_second == kStatusBadObjectKind || status->m_second == kStatusNotAccepted);
    if (badKind && team->Find(&request->m_target)) setStatus(status, kStatusNotInTeam);
    return 1;
}

int GetCommandSide(CommandQueue* commands, uint32_t* side) {
    Command* command = commands->FirstCommand();
    if (command) {
        GameObjNotifier* object = nullptr;
        const StatusCode missing(kStatusCategory, kStatusMissingObject);
        StatusCode resolveStatus;
        const int ok = ResolveMasterInFamilies_01_ff(&command->m_key, &object, &missing, &resolveStatus);
        if (ok && object != nullptr) {
            *side = object->ControlSide();
            if (*side != 0 && *side < 4) return 1;
        }
    }
    return 0;
}

int CheckControlAndState(GameObj* object, uint32_t side, uint32_t otherSide, int requireCompleted, int checkOtherSide,
                         StatusCode* status) {
    *status = okStatus();
    if (isOk(*status) && object->ControlSide() != side) setStatus(status, kStatusWrongControl);
    if (!isOk(*status)) return 1;
    if (checkOtherSide != 0 && object->ControlSide() != otherSide) setStatus(status, kStatusEnemyDestination);
    if (!isOk(*status)) return 1;
    const uint32_t flags = object->m_stateFlags;
    if (!(flags & kFlagCompleted) && requireCompleted != 0)
        setStatus(status, kStatusNotCompleted);
    else if (flags & kFlagDestroyed)
        setStatus(status, kStatusDestroyed);
    else if (flags & kFlagEnroute)
        setStatus(status, kStatusEnroute);
    return 1;
}

int AddChildKeys(ChildIterator* children, ObjectKeyValueSet* out, int onlySide, uint32_t side) {
    int ok = 1;
    children->Begin();
    do {
        if (children->Current() == nullptr) return ok;
        if (onlySide == 0 || side == children->Current()->ControlSide()) {
            ObjectKey key;
            ok = out->AddKeyValue(children->Current()->GetKey(&key), 0);
        }
        children->Advance();
    } while (ok != 0);
    return 0;
}

GameObjNotifier* GetMasterObjectForKey(const ObjectKey* key) { return GetMasterObject(key); }

void AppendObjectDescription(ObjectKey* key, String* out) {
    out->Assign(String(""));
    if (key->OrNull() == nullptr) {
        out->Assign(String("<empty GameObjKey>"));
        return;
    }
    GameObjNotifier* object = GetMasterObject(key);
    if (object == nullptr) {
        *out += String("<can't find name, type = ");
        out->AppendUnsigned(key->Family());
        *out += String("> ");
    } else {
        *out += *static_cast<GameObj*>(object)->GetName();
    }
    *out += String(" (ID=");
    out->AppendUnsigned(key->Id());
    *out += String(")");
}
