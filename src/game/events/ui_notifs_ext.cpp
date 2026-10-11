#include "events/ui_notifs_ext.h"

#include <new>

#include "app/player_controller_decl.h"
#include "character/role.h"
#include "core/checked_new.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "mission/mission.h"

ObjectKey* GetObjectKey(GameObjNotifier* object, ObjectKey* out);  // 0x4025b0

namespace {

bool inFamilyRange(const ObjectKey& key, uint32_t first, uint32_t last) { return key.Family() >= first && key.Family() < last; }

}  // namespace

// ---- UiNotif200 -----------------------------------------------------------------------------------------------

UiNotif200::UiNotif200() : IntPairPayload(), m_missionKey() { m_leadsMission = 0; }

UiNotif200::~UiNotif200() = default;

void UiNotif200::Read(istream* in) {
    ReadPayload(in);
    m_missionKey.Read(in);
    ReadValue32(in, &m_leadsMission);
}

void UiNotif200::Write(ostream* out) {
    WritePayload(out);
    m_missionKey.Write(out);
    WriteValue32(out, &m_leadsMission);
}

void UiNotif200::Virtual05() {
    Role* character = static_cast<Role*>(GetObjectInFamilies_30_40(m_value18, &m_key));
    if (!character) return;
    m_missionKey = ObjectKey(character->m_missionKey);
    if (!m_missionKey.OrNull()) return;
    Mission* mission = static_cast<Mission*>(GetObjectInFamilies_40_80(m_value18, &m_missionKey));
    if (mission) m_leadsMission = m_key.m_value == mission->m_leaderKey.m_value;
}

void UiNotif200::Dispatch(PlayerController* controller) { controller->Virtual77(&m_key, m_valueB); }

void* CreateUiNotif200() { return NewOrNull<UiNotif200>(); }

// ---- UiNotif1e0 -----------------------------------------------------------------------------------------------

UiNotif1e0::UiNotif1e0() : IntPairPayload(), m_shipKey(), m_grandparentKey(), m_systemKey(), m_locationKey() {}

UiNotif1e0::~UiNotif1e0() = default;

int UiNotif1e0::GetKind() { return 0x1e0; }

void UiNotif1e0::Read(istream* in) {
    ReadPayload(in);
    m_shipKey.Read(in);
    m_grandparentKey.Read(in);
    m_systemKey.Read(in);
    m_locationKey.Read(in);
}

void UiNotif1e0::Write(ostream* out) {
    WritePayload(out);
    m_shipKey.Write(out);
    m_grandparentKey.Write(out);
    m_systemKey.Write(out);
    m_locationKey.Write(out);
}

void UiNotif1e0::Virtual05() {
    GameObj* character = static_cast<GameObj*>(GetObjectInFamilies_30_3c(m_value18, &m_key));
    ObjectKey parentKey;
    ObjectKey ownerKey;
    if (!character) return;
    character->GetValidParentKey(&parentKey);
    character->GetOwnerKey(&ownerKey);
    if (inFamilyRange(parentKey, 0x14, 0x1c)) {
        m_shipKey = ObjectKey(parentKey);
        GameObjNotifier* parent = nullptr;
        GameObjNotifier* grandparent = nullptr;
        character->GetParent(&parent);
        if (parent) static_cast<GameObj*>(parent)->GetParent(&grandparent);
        if (grandparent) {
            ObjectKey scratch;
            m_grandparentKey = ObjectKey(*GetObjectKey(grandparent, &scratch));
        }
    }
    if (inFamilyRange(ownerKey, 0x90, 0x98)) m_systemKey = ObjectKey(ownerKey);
    if (inFamilyRange(ownerKey, 0x98, 0xa0)) m_locationKey = ObjectKey(ownerKey);
}

ObjectKey* UiNotif1e0::CopyGrandparentKey(ObjectKey* out) {
    new (out) ObjectKey(m_grandparentKey);
    return out;
}

ObjectKey* UiNotif1e0::CopySystemKey(ObjectKey* out) {
    new (out) ObjectKey(m_systemKey);
    return out;
}

void UiNotif1e0::Dispatch(PlayerController* controller) { controller->Virtual79(&m_key, m_valueB); }

void* CreateUiNotif1e0() { return NewOrNull<UiNotif1e0>(); }
