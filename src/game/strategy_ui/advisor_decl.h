#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/stream_io.h"
#include "strategy_ui/advisor_droid.h"

// Functions of the AI helper module (0x49c6a0-0x49ecb0) and of the player dispatch that the advisor droid calls.
// The helper objects live inside the advisor: the task tree (+0x18), the AI helper (+0x3c) and the cue tree (+0x138).
void __thiscall FUN_0049dcf0(void* taskTree);
void __thiscall FUN_0049dd30(void* taskTree);
void __thiscall FUN_0049dd50(void* taskTree, istream* in, void* aiHelper);
void __thiscall FUN_0049de40(void* taskTree, ostream* out);
void __thiscall FUN_0049df20(void* taskTree, int taskId, void* aiHelper);
AdvisorTask* __thiscall FUN_0049df70(void* taskTree);

void __thiscall FUN_0049c780(void* aiHelper);
void __thiscall FUN_0049c810(void* aiHelper);
void __thiscall FUN_0049c880(void* aiHelper, int side);
void __thiscall FUN_0049c8a0(void* aiHelper, istream* in);
void __thiscall FUN_0049c9f0(void* aiHelper, ostream* out);
int __thiscall FUN_0049cb30(void* aiHelper);
int __thiscall FUN_0049cc80(void* aiHelper);
void __thiscall FUN_0049cd70(void* aiHelper, int value);
void __thiscall FUN_0049cd90(void* aiHelper, int value);

void __thiscall FUN_0049c6a0(void* cueTree);
void __thiscall FUN_0049c6e0(void* cueTree);
void __thiscall FUN_0049c6f0(void* cueTree, istream* in);
void __thiscall FUN_004e5210(void* cueTree, ostream* out);

void __thiscall FUN_0049e380(AdvisorTask* task, int value);
void __thiscall FUN_0049e360(AdvisorTask* task, int value);

void __thiscall FUN_0048a590(PlayerDispatch* dispatch, IdValueNode* notice);
int __cdecl FUN_0048b460(int side, IdValueTree* notices);
