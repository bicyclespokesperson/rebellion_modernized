#include "gameobj/game_obj.h"
#include "galaxy/mission_decl.h"
#include "mission/mission_bases.h"
#include "mission/mission_extra_key.h"
#include "mission/mission_forwarders.h"
#include "mission/mission_research.h"

// Shared tiny bodies of the mission leaf classes (the linker folded identical forwarders and constant stubs over many
// classes, so these have no single owner): the Read / Write / Checksum slots that call a base body, the CreateViews chain
// and the "return 1" stubs of the virtuals no mission overrides.


int __thiscall MissionResearchReadForward(void* self, istream* in) { return static_cast<MissionResearch*>(self)->MissionResearch::Read(in); }  // 0x56c390

int __thiscall MissionResearchWriteForward(void* self, ostream* out) { return static_cast<MissionResearch*>(self)->MissionResearch::Write(out); }  // 0x56c3a0

int __thiscall MissionResearchChecksumForward(void* self, Crc32* crc) { return static_cast<MissionResearch*>(self)->MissionResearch::Checksum(crc); }  // 0x56c3d0

int __thiscall MissionExtraKeyReadForward(void* self, istream* in) { return static_cast<MissionExtraKeyBase*>(self)->MissionExtraKeyBase::Read(in); }  // 0x570560

int __thiscall MissionExtraKeyWriteForward(void* self, ostream* out) { return static_cast<MissionExtraKeyBase*>(self)->MissionExtraKeyBase::Write(out); }  // 0x570570

int __thiscall MissionExtraKeyChecksumForward(void* self, Crc32* crc) { return static_cast<MissionExtraKeyBase*>(self)->MissionExtraKeyBase::Checksum(crc); }  // 0x572f80

int __thiscall MissionRuleBaseApplyAllForward(void* self, NotifyContext* ctx) { return static_cast<MissionRuleBase*>(self)->MissionRuleBase::ApplyAllBody(ctx); }  // 0x586ac0

int __thiscall MissionTypeCreateViewsForward(void* self, int controlKind, NotifyContext* ctx) {  // 0x576250
    return static_cast<MissionTypeBase*>(self)->MissionTypeBase::CreateViewsBody(controlKind, ctx);
}

int __thiscall MissionTypeCreateViewsForward2(void* self, int controlKind, NotifyContext* ctx) {  // 0x574080
    return MissionTypeCreateViewsForward(self, controlKind, ctx);
}

int __thiscall MissionTypeCreateViewsForward3(void* self, int controlKind, NotifyContext* ctx) {  // 0x574420
    return MissionTypeCreateViewsForward2(self, controlKind, ctx);
}

int __thiscall MissionTypeCreateViewsForward4(void* self, int controlKind, NotifyContext* ctx) {  // 0x5761b0
    return MissionTypeCreateViewsForward(self, controlKind, ctx);
}

int __thiscall MissionTypeCreateViewsForward5(void* self, int controlKind, NotifyContext* ctx) {  // 0x576d10
    return MissionTypeCreateViewsForward2(self, controlKind, ctx);
}

int __stdcall ReturnOneThreeArgs(int, int, int) { return 1; }  // 0x524b10

int __stdcall ReturnOneTwoArgs(int, NotifyContext*) { return 1; }  // 0x524fb0

int __stdcall ReturnOneSevenArgs(int, int, int, int, int, int, int) { return 1; }  // 0x5897b0

int __thiscall MissionTypeWriteForward(MissionTypeBase* self, ostream* out) {  // 0x56ec40
    return reinterpret_cast<MissionRuleBase*>(self)->MissionRuleBase::WriteBody(out);
}

int __thiscall MissionTypeApplyAllForward(MissionTypeBase* self, NotifyContext* ctx) {  // 0x56ec50
    return self->MissionTypeBase::ApplyAllBody(ctx);
}
