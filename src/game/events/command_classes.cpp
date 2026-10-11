#include "events/command_classes.h"

#include "core/checked_new.h"
#include "galaxy/system.h"
#include "gameobj/object_queries.h"
#include "rules/planet_assault.h"
#include "rules/planet_bombard.h"

namespace {

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusMissingTarget = 0x12;
constexpr int kLoyaltySide = 1;

bool isOk(const StatusCode& status) { return status.m_second == static_cast<uint32_t>(kStatusOk); }

// Resolves the system a command names; *system stays null if there is none.
int resolveSystem(const ObjectKey* key, GameObjNotifier** system, StatusCode* status) {
    *system = nullptr;
    *status = StatusCode(kStatusCategory, kStatusOk);
    int found = 1;
    if (isOk(*status)) {
        const StatusCode missing(kStatusCategory, kStatusMissingTarget);
        found = ResolveMasterInFamilies_90_98(key, system, &missing, status);
    }
    return found;
}

}  // namespace

// ---- ActivityRequest ------------------------------------------------------------------------------

ObjectKey* ActivityRequest::GetTarget(ObjectKey* out) const {
    *out = m_target;
    return out;
}

GameObjNotifier* NextCharacterSibling(GameObj* object) {
    const FamilyRange characters(0x30, 0x3c);
    return object->FindNextSibling(&characters, 1);
}

// ---- LoyaltyEvent373 -------------------------------------------------------------------------------------------

LoyaltyEvent373::LoyaltyEvent373() : PlainEvent(), m_delta(0) {}

LoyaltyEvent373::LoyaltyEvent373(const LoyaltyEvent373& other)  = default;

LoyaltyEvent373::~LoyaltyEvent373() = default;

LoyaltyEvent373* LoyaltyEvent373::Clone() { return NewOrNull<LoyaltyEvent373>(*this); }

void LoyaltyEvent373::Read(istream* in) {
    ReadPlain(in);
    ReadValue32(in, &m_delta);
}

void LoyaltyEvent373::Write(ostream* out) {
    WritePlain(out);
    WriteValue32(out, &m_delta);
}

void LoyaltyEvent373::Checksum(Crc32* crc) {
    ChecksumPlain(crc);
    crc->AddInt(m_delta);
}

int LoyaltyEvent373::GetKind() { return 0x373; }

int LoyaltyEvent373::CheckEnabled(StatusCode* status) {
    GameObjNotifier* system;
    return resolveSystem(&m_key, &system, status);
}

int LoyaltyEvent373::CheckValid(StatusCode* status) {
    GameObjNotifier* system;
    return resolveSystem(&m_key, &system, status);
}

int LoyaltyEvent373::Execute() {
    NotifyContext context(m_context);
    GameObjNotifier* system;
    StatusCode status;
    const int found = resolveSystem(&m_key, &system, &status);
    if (found && isOk(status)) static_cast<System*>(system)->AdjustLoyalty(m_delta, kLoyaltySide, &context);
    return 0;
}

void* CreateLoyaltyEvent373() { return NewOrNull<LoyaltyEvent373>(); }

// ---- SystemCommand225 ------------------------------------------------------------------------------------------

SystemCommand225::SystemCommand225() : Command() {}

SystemCommand225::SystemCommand225(const SystemCommand225& other)  = default;

SystemCommand225::~SystemCommand225() = default;

SystemCommand225* SystemCommand225::Clone() { return NewOrNull<SystemCommand225>(*this); }

void SystemCommand225::Read(istream* in) {
    ReadCommand(in);
}

void SystemCommand225::Write(ostream* out) {
    WriteCommand(out);
}

void SystemCommand225::Checksum(Crc32* crc) { ChecksumCommand(crc); }

int SystemCommand225::GetKind() { return 0x225; }

int SystemCommand225::CheckEnabled(StatusCode* status) {
    GameObjNotifier* system;
    int result = resolveSystem(&m_key, &system, status);
    if (result && isOk(*status)) result = static_cast<System*>(system)->Virtual111(&m_request, status);
    return result;
}

int SystemCommand225::CheckValid(StatusCode* status) {
    GameObjNotifier* system;
    int result = resolveSystem(&m_key, &system, status);
    if (result && isOk(*status)) result = static_cast<System*>(system)->Virtual112(&m_request, status);
    return result;
}

int SystemCommand225::Execute() {
    NotifyContext context(m_context);
    GameObjNotifier* system;
    StatusCode status;
    const int found = resolveSystem(&m_key, &system, &status);
    if (found && isOk(status)) {
        AssaultResolver resolver;
        resolver.Run(system, &m_request, &context);
    }
    return 0;
}

void* CreateSystemCommand225() { return NewOrNull<SystemCommand225>(); }

// ---- SystemCommand224 ------------------------------------------------------------------------------------------

SystemCommand224::SystemCommand224() : Command() {}

SystemCommand224::SystemCommand224(const SystemCommand224& other)  = default;

SystemCommand224::~SystemCommand224() = default;

SystemCommand224* SystemCommand224::Clone() { return NewOrNull<SystemCommand224>(*this); }

void SystemCommand224::Read(istream* in) {
    ReadCommand(in);
}

void SystemCommand224::Write(ostream* out) {
    WriteCommand(out);
}

void SystemCommand224::Checksum(Crc32* crc) { ChecksumCommand(crc); }

int SystemCommand224::GetKind() { return 0x224; }

int SystemCommand224::CheckEnabled(StatusCode* status) {
    GameObjNotifier* system;
    int result = resolveSystem(&m_key, &system, status);
    if (result && isOk(*status)) result = static_cast<System*>(system)->Virtual113(&m_request, status);
    return result;
}

int SystemCommand224::CheckValid(StatusCode* status) {
    GameObjNotifier* system;
    int result = resolveSystem(&m_key, &system, status);
    if (result && isOk(*status)) result = static_cast<System*>(system)->Virtual114(&m_request, status);
    return result;
}

int SystemCommand224::Execute() {
    NotifyContext context(m_context);
    GameObjNotifier* system;
    StatusCode status;
    const int found = resolveSystem(&m_key, &system, &status);
    if (found && isOk(status)) {
        BombardResolver resolver;
        resolver.Run(system, &m_request, &context);
    }
    return 0;
}

void* CreateSystemCommand224() { return NewOrNull<SystemCommand224>(); }
