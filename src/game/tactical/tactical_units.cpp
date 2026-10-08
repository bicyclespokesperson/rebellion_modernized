#include "tactical/tactical_units.h"

TacticalCharacterUnit::TacticalCharacterUnit(uint32_t family, uint32_t id, uint16_t spriteId, String name,
                                             int notAlliance, int isMajor, int isFirstCommander,
                                             int isSecondCommander, float scaleA, float scaleB,
                                             int shipHandle, int injured) {
    m_channelA.m_data = nullptr;
    m_channelB.m_data = nullptr;
    m_family = family;
    m_id = id;
    m_spriteId = spriteId;
    m_name.Assign(name);
    m_notAlliance = notAlliance;
    m_isMajor = isMajor;
    m_scaleA = scaleA;
    m_scaleB = scaleB;
    m_isFirstCommander = isFirstCommander;
    m_isSecondCommander = isSecondCommander;
    m_shipHandle = shipHandle;
    m_injured = injured;
}

TacticalCharacterUnit::~TacticalCharacterUnit() {}

TacticalSpecialForceUnit::TacticalSpecialForceUnit(uint32_t family, uint32_t id, uint16_t spriteId, String name,
                                                   int notAlliance, int shipHandle) {
    m_channelA.m_data = nullptr;
    m_channelB.m_data = nullptr;
    m_family = family;
    m_id = id;
    m_spriteId = spriteId;
    m_name.Assign(name);
    m_notAlliance = notAlliance;
    m_shipHandle = shipHandle;
}

TacticalSpecialForceUnit::~TacticalSpecialForceUnit() {}
