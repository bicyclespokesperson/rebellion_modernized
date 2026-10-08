#pragma once
#include <cstdint>

#include "ai/ai_planner.h"
#include "core/containers.h"
#include "core/layout.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"

// Declaration of the AI's mission planner base class (VT 0x65c3b0, base ctor 0x47aae0), needed by AiPlannerSet,
// AiSystemRecord and the side controller. The planners (one per mission kind 0x51-0x6a, created by AiPlannerSet::CreatePlanner)
// belong to part 2 of the AI module; this is a stand-in meant to be replaced by the full header. Slots keep their order,
// unknown ones are VirtualNN.

// The planner and agent constructors of part 2 (called with the memory from operator new): (view, owner, characters).
AiMissionPlanner* __thiscall FUN_00481460(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x51 diplomacy
AiMissionPlanner* __thiscall FUN_00480d10(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x52 subtype 6
AiMissionPlanner* __thiscall FUN_0047f550(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x54 subtype 5
AiMissionPlanner* __thiscall FUN_0047e130(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x61
AiMissionPlanner* __thiscall FUN_00483140(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x62
