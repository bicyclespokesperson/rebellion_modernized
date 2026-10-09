#include "core/reb_abi.h"
#include "mission/mission_leaf.h"

int REB_THISCALL MissionStartIfIdle(Mission* mission, NotifyContext* ctx) {
    if (mission->m_taskStatus != 0) return 1;
    return mission->SetTaskStatus(2, ctx) != 0;
}
