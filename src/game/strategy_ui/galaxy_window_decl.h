#pragma once
#include <cstdint>

#include "core/layout.h"
#include "platform/win32.h"
#include "ui/game_window_object.h"

// Stand-in declaration (drop at merge): the galaxy view, the main strategy window (ctor 0x421c70, VT 0x658bf8,
// WndProc 0x422ce0), owned by the ui_strategic module. Only the members the encyclopedia window reads.
class CoolListItemList;

class GalaxyWindow : public GameWindowObject {
public:
    int m_side;                              // +0x9c 1 = Alliance, 2 = Empire (the object view of the player)
    uint8_t m_unknownA0[0xcc - 0xa0];        // +0xa0
    RECT m_panelArea;                        // +0xcc area child panels are centred in
    uint8_t m_unknownDc[0x474 - 0xdc];       // +0xdc
    CoolListItemList* m_encyclopediaItems;   // +0x474 every encyclopedia entry (ObjectListItems, id 0x1000 + record id)
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_OFFSET(GalaxyWindow, m_side, 0x9c);
REB_ASSERT_OFFSET(GalaxyWindow, m_panelArea, 0xcc);
REB_ASSERT_OFFSET(GalaxyWindow, m_encyclopediaItems, 0x474);
#pragma clang diagnostic pop
