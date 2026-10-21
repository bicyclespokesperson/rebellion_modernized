#pragma once

#include <string_view>

// The port's quality-of-life additions to the original interface: ETAs and completion days, speed keys 1-4, the
// Repeat mission button, the mission-with-decoys shortcut, extra ship and fleet status detail, and similar. Off,
// those views and controls behave as in the original game.
//
// A per-player display preference, on by default and saved as `enhancements` in config.ini. It only changes what the
// local interface shows and which shortcuts it offers: the simulation, saves and LAN traffic never depend on it.
bool EnhancementsEnabled();
bool SetEnhancementsEnabled(bool enabled);

// "0", "false", "off" and "no" (any case) disable the enhancements; anything else, including no value, enables them.
bool ParseEnhancementsSetting(std::string_view value);

// For smoke checks: forces the setting for this process without touching config.ini.
void OverrideEnhancementsForProcess(bool enabled);
