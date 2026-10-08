#include "gameobj/game_obj.h"

// Shared tiny bodies of the mission leaf classes (the linker folded identical forwarders and constant stubs over many
// classes, so these have no single owner): the Read / Write / Checksum slots that call a base body, the CreateViews chain
// and the "return 1" stubs of the virtuals no mission overrides.

int __thiscall FUN_0056cb40(void* self, istream* in);   // MissionResearch::Read
int __thiscall FUN_0056cb80(void* self, ostream* out);  // MissionResearch::Write
int __thiscall FUN_0056cc00(void* self, Crc32* crc);    // MissionResearch::Checksum
int __thiscall FUN_005730b0(void* self, istream* in);   // MissionExtraKeyBase::Read
int __thiscall FUN_005730e0(void* self, ostream* out);  // MissionExtraKeyBase::Write
int __thiscall FUN_00573140(void* self, Crc32* crc);    // MissionExtraKeyBase::Checksum
int __thiscall FUN_00592c70(void* self, NotifyContext* ctx);                  // MissionRuleBase::ApplyAllBody
int __thiscall FUN_00593a80(void* self, int controlKind, NotifyContext* ctx);  // MissionTypeBase::CreateViewsBody

int __thiscall MissionResearchReadForward(void* self, istream* in) { return FUN_0056cb40(self, in); }  // 0x56c390

int __thiscall MissionResearchWriteForward(void* self, ostream* out) { return FUN_0056cb80(self, out); }  // 0x56c3a0

int __thiscall MissionResearchChecksumForward(void* self, Crc32* crc) { return FUN_0056cc00(self, crc); }  // 0x56c3d0

int __thiscall MissionExtraKeyReadForward(void* self, istream* in) { return FUN_005730b0(self, in); }  // 0x570560

int __thiscall MissionExtraKeyWriteForward(void* self, ostream* out) { return FUN_005730e0(self, out); }  // 0x570570

int __thiscall MissionExtraKeyChecksumForward(void* self, Crc32* crc) { return FUN_00573140(self, crc); }  // 0x572f80

int __thiscall MissionRuleBaseApplyAllForward(void* self, NotifyContext* ctx) { return FUN_00592c70(self, ctx); }  // 0x586ac0

int __thiscall MissionTypeCreateViewsForward(void* self, int controlKind, NotifyContext* ctx) {  // 0x576250
    return FUN_00593a80(self, controlKind, ctx);
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
