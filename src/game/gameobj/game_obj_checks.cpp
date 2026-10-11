#include <cstring>

#include "data/dat_record.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_database.h"


namespace {

constexpr uint32_t kViewSideMask = 0x30;
constexpr uint32_t kEnroute = 0x10;
constexpr uint32_t kEnrouteActive = 0x20;
constexpr uint32_t kCompleted = 0x4;
constexpr uint32_t kDestroyed = 0x8;
constexpr uint32_t kHyperdriveActive = 0x800;
constexpr uint32_t kAutorouting = 0x1000;
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNoSuchOrder = 1;
constexpr int kStatusWrongControl = 0x14;
constexpr int kStatusNotCompleted = 0x10;
constexpr int kStatusDestroyed = 0x12;
constexpr int kStatusEnroute = 0x11;
constexpr int kOrderKindAllowedWhileIncomplete[] = {0x203, 0x215, 0x204, 0x241};

bool isAllowedWhileIncomplete(int kind) {
    for (int allowed : kOrderKindAllowedWhileIncomplete) {
        if (kind == allowed) return true;
    }
    return false;
}

void setStatus(StatusCode* out, int code) { *out = StatusCode(kStatusCategory, code); }

}  // namespace

int GameObj::CreateViews(int controlKind, NotifyContext* ctx) {
    bool ok = true;
    if (controlKind < 1 || controlKind > 3 || (m_flags & kViewSideMask) != 0) ok = false;
    GameObj* alliance = nullptr;
    GameObj* empire = nullptr;
    if (ok) {
        alliance = CreateViewObject();
        ok = alliance != nullptr;
    }
    if (ok) {
        empire = CreateViewObject();
        ok = empire != nullptr;
    }
    int result = 0;
    if (ok) {
        const bool master = AttachMasterObject(this);
        ok = AttachSideObject(alliance, this, 1) && master;
        ok = AttachSideObject(empire, this, 2) && ok;
        ok = DestroyData() && ok;
        ok = InstallData() && ok;
        ok = RegisterObject(this) && ok;
        ok = RegisterObject(alliance) && ok;
        result = RegisterObject(empire) && ok ? 1 : 0;
        SetControlKind(controlKind, ctx);
        uint32_t packed;
        std::memcpy(&packed, &m_record->m_small, sizeof(packed));
        ResourceStringId nameId;
        std::memcpy(&nameId, &packed, sizeof(nameId));
        std::string name = LoadStringResource(&nameId);
        SetName(&name, ctx);
    }
    return result;
}

int GameObj::Read(istream* in) {
    int ok = ReadObject(in);
    if (ok) {
        int hasName;
        ReadValue32(in, &hasName);
        if (hasName == 0) {
            if (m_name) {
                delete m_name;
                m_name = nullptr;
            }
        } else {
            ok = InstallName();
            if (!ok) return 0;
            ok = ReadLegacyString(in, *m_name);
            if (!ok) return 0;
        }
        if (ok) {
            ReadValue32(in, &m_stateFlags);
            m_builderKey.Read(in);
            m_departureKey.Read(in);
            ReadValue32(in, &m_packed);
            ReadValue32(in, &m_eta);
            ReadValue32(in, &m_unk48);
            ReadValue32(in, &m_unk4c);
            if (ok) {
                if ((m_flags & kViewSideMask) == 0) {
                    ok = DestroyData();
                    if (!ok) return 0;
                    ok = InstallData();
                    if (!ok) return 0;
                    ok = m_data->Read(in);
                }
                const bool succeeded = ok != 0;
                ok = 0;
                if (succeeded) ok = RegisterObject(this);
            }
        }
    }
    return ok;
}

int GameObj::Write(ostream* out) {
    int ok = WriteObject(out);
    if (ok) {
        int hasName = m_name != nullptr;
        WriteValue32(out, &hasName);
        if (hasName) ok = WriteLegacyString(out, *m_name);
        WriteValue32(out, &m_stateFlags);
        m_builderKey.Write(out);
        m_departureKey.Write(out);
        WriteValue32(out, &m_packed);
        WriteValue32(out, &m_eta);
        WriteValue32(out, &m_unk48);
        WriteValue32(out, &m_unk4c);
        if (ok && (m_flags & kViewSideMask) == 0) ok = m_data->Write(out);
    }
    return ok;
}

int GameObj::Checksum(Crc32* crc) {
    int ok = ChecksumObject(crc);
    int result = 0;
    if (ok) {
        crc->AddInt(m_stateFlags);
        ok = m_builderKey.Checksum(crc);
        result = 0;
        if (ok) {
            result = m_departureKey.Checksum(crc);
            if (result) {
                crc->AddInt(m_packed & 0xff);
                crc->AddInt((m_packed >> 8) & 0xff);
                crc->AddInt((m_packed >> 16) & 0xff);
                crc->AddInt(m_eta);
                crc->AddInt(m_unk48);
                crc->AddInt(m_unk4c);
                if (result && (m_flags & kViewSideMask) == 0) result = m_data->Checksum(crc);
            }
        }
    }
    return result;
}

int GameObj::Validate() {
    int complete = NotifyIfComplete();
    int result = 0;
    if (!complete) return result;
    uint32_t ok;
    if ((m_flags & kViewSideMask) == 0) {
        ok = m_data != nullptr;
    } else {
        ok = m_data == nullptr;
    }
    if (!ok) return 0;
    if ((m_flags & kViewSideMask) == 0) ok = m_data->IsValid();
    if (!ok) return 0;
    if (m_stateFlags & kEnrouteActive) ok = (m_stateFlags >> 11) & 1;
    if (!ok) return 0;
    if (m_stateFlags & kHyperdriveActive) ok = (m_stateFlags >> 5) & 1;
    if (!ok) return 0;
    if (m_stateFlags & kEnrouteActive) ok = (m_stateFlags >> 4) & 1;
    if (!ok) return 0;
    GameObjNotifier* parent = nullptr;
    if (IsNotGameRoot()) {
        const int hasParent = GetParent(&parent);
        ok = 0;
        if (hasParent) ok = (static_cast<GameObj*>(parent)->m_stateFlags >> 1) & 1;
        if (ok == 0) return 0;
        if ((m_stateFlags & kEnroute) && !(m_stateFlags & kEnrouteActive)) {
            ok = (static_cast<GameObj*>(parent)->m_stateFlags >> 4) & 1;
        }
    }
    if (!ok) return 0;
    if (m_stateFlags & kAutorouting) return 0;
    bool etaOk;
    if (!(m_stateFlags & kEnroute)) {
        etaOk = m_eta == 0;
    } else {
        const int eta = m_eta;
        if (eta < 1) return 0;
        etaOk = CallGameHook2B08() <= eta;
    }
    if (!etaOk) return 0;
    return m_departureKey.IsSet() == static_cast<int>((m_stateFlags >> 4) & 1);
}

int GameObj::CheckActivityRequest(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusOk);
    const int result = CheckOrder(0, request->m_kind, out);
    if (!result) {
        setStatus(out, kStatusNoSuchOrder);
        return result;
    }
    const bool stillOk = out->m_second == static_cast<uint32_t>(-1);
    if (stillOk) {
        const uint32_t control = ControlSide();
        const int side = control == 1 ? 1 : control == 2 ? 2 : 0;
        if (side == 0 || side != request->m_side) setStatus(out, kStatusWrongControl);
    }
    if (out->m_second == static_cast<uint32_t>(-1) && !(m_stateFlags & kCompleted) &&
        !isAllowedWhileIncomplete(request->m_kind)) {
        setStatus(out, kStatusNotCompleted);
    }
    if (out->m_second == static_cast<uint32_t>(-1) && (m_stateFlags & kDestroyed)) setStatus(out, kStatusDestroyed);
    if (out->m_second == static_cast<uint32_t>(-1) && (m_stateFlags & kEnroute)) setStatus(out, kStatusEnroute);
    return result;
}

int GameObj::CollectDependencies(void* context, int value, ObjectKeyValueSet* out) {
    out->Clear();
    ObjectKey key;
    GetKey(&key);
    return out->AddKeyValue(&key, 0);
}

int InitEventKinds(int unused, EventKindSet* out) {
    EventKindSet kinds;
    *out = kinds;
    return 1;
}
