#include "mission/mission_bases.h"

#include "core/vptr_store.h"

MissionRuleBase::MissionRuleBase() {}

MissionRuleBase::~MissionRuleBase() { REB_KEEP_VPTR_STORE(); }

MissionSystemTargetBase::MissionSystemTargetBase() {}

MissionSystemTargetBase::~MissionSystemTargetBase() { REB_KEEP_VPTR_STORE(); }

MissionCharacterTargetBase::MissionCharacterTargetBase() {}

MissionCharacterTargetBase::~MissionCharacterTargetBase() { REB_KEEP_VPTR_STORE(); }

MissionObjectTargetBase::MissionObjectTargetBase() {}

MissionObjectTargetBase::~MissionObjectTargetBase() { REB_KEEP_VPTR_STORE(); }

MissionStoryBase::MissionStoryBase() {}

MissionStoryBase::~MissionStoryBase() { REB_KEEP_VPTR_STORE(); }
