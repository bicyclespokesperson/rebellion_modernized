#pragma once
#include "strategy_ui/advisor_notices.h"

// The records the message index lists (game_ui_facade 0x41d1b0, UiGetHumanOrderTree) are the human player's notices
// (strategy_ui/advisor_notices.h): an id-keyed node with the category bits at +0x34 and the message text at +0x44.
// UiKeyNotice shares the text layout.
using MessageRecord = UiTextNotice;
