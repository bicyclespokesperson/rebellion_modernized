#pragma once
#include "gameobj/game_hooks.h"
#include "mission/mission_leaf.h"

// The part MissionEspionage (family 0x52) and MissionJediTraining (family 0x58) have in common: one more key property
// at +0xa8 (the "ExtraSystem" key of Espionage, the "Teacher" key of Jedi Training), 0xac bytes in all. Both classes
// derive straight from MissionSystemTargetBase; the intermediate class exists only in this reconstruction, it groups
// what the original compiled twice (identical-code-folded where the bodies are alike).
//
// The property follows the pattern of Mission (see mission.h): SetX (master only), ApplyX (master -> side view, raises
// OnXNotified). The two new virtuals continue after slot 161: 162 OnExtraKeyChanged, 163 OnExtraKeyNotified. Each class
// has its own Set/Apply bodies (they call the class's own Apply) and its own ApplyAll body; Read, Write and Checksum
// are the same code in both (0x5730b0, 0x5730e0, 0x573140; the classes' slots 5-7 are forwarders to them).
class MissionExtraKeyBase : public MissionSystemTargetBase {
public:
    MissionExtraKeyBase();
    ~MissionExtraKeyBase() override;

    int Read(istream* in) override;                   // 5, 0x5730b0 (forwarder 0x570560)
    int Write(ostream* out) override;                 // 6, 0x5730e0 (forwarder 0x570570)
    int Checksum(Crc32* crc) override;                // 7, 0x573140 (forwarders 0x572f80, 0x56dee0)
    int Validate() override;                          // 8, shared forwarder 0x56d460
    int CreateViews(int controlKind, NotifyContext* ctx) override;  // 37, shared forwarder 0x576d10
    virtual int OnExtraKeyChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) = 0;   // 162
    virtual int OnExtraKeyNotified(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx) = 0;  // 163

    ObjectKey m_extraKey;  // +0xa8
};

// Set/Apply bodies of the extra key property; the classes' own SetX/ApplyX wrap these. `Class` supplies
// ApplyExtraKey(ctx) for the side views.
template <class Class>
inline int SetExtraKeyProperty(Class* self, ObjectKey* value, NotifyContext* ctx) {
    const int result = self->IsMasterView();
    if (result && self->m_extraKey.m_value != value->m_value) {
        ObjectKey oldKey;
        oldKey.m_value = self->m_extraKey.m_value;
        self->m_extraKey.m_value = value->m_value;
        static_cast<Class*>(self->ViewAs(1))->ApplyExtraKey(ctx);
        static_cast<Class*>(self->ViewAs(2))->ApplyExtraKey(ctx);
        self->OnExtraKeyChanged(&oldKey, &self->m_extraKey, ctx);
    }
    return result;
}

template <class Class>
inline int ApplyExtraKeyProperty(Class* self, NotifyContext* ctx) {
    int result = self->IsSideView();
    if (result) {
        Class* master = static_cast<Class*>(self->ViewAs(0));
        if (self->MatchesSideMask(ctx->m_flag08) && self->m_extraKey.m_value != master->m_extraKey.m_value) {
            ObjectKey oldKey;
            oldKey.m_value = self->m_extraKey.m_value;
            self->m_extraKey.m_value = master->m_extraKey.m_value;
            if (CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(self->ViewSide())))
                result = self->OnExtraKeyNotified(&oldKey, &self->m_extraKey, ctx);
        }
    }
    return result;
}

// The slot 9 body shared in shape by both classes: the rule bases' apply-all (0x586ac0), then, when applying everything
// for a full refresh (ctx->m_flag04 == 1), the extra key.
template <class Class>
inline int ApplyAllWithExtraKey(Class* self, NotifyContext* ctx) {
    const int result = self->ApplyAllImpl(ctx);
    if (result && ctx->m_flag04 == 1) self->ApplyExtraKey(ctx);
    return result;
}
