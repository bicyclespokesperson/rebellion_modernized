#pragma once
#include <cstdint>

#include "audio/sound_manager.h"
#include "core/game_string.h"
#include "core/utility_classes.h"

// The game's background music state lives in AudioContext. A SoundRequest plays MDATA.3nn from the selected music
// directory, and completed tracks advance to the next advisor track. The two music options live in the registry.

namespace music_flag
{
constexpr int kMuted = 0x1; // the music switch is off
constexpr int kAdviceTrack = 0x2; // the current track was started by PlayAdviceMusic and repeats
} // namespace music_flag

extern const char* g_musicFileNames[16]; // 0x6a8478

void AppConfirmWaitDialog(); // 0x4173a0
void InitMusic(); // 0x4173c0: music directory, event callback, saved options
void StartMusicTrack(uint32_t index); // 0x417520
void PlayAdviceMusic(uint32_t index); // 0x417610: replaces the current track and marks it as the advice track
void StopMusic(); // 0x417640
void SetMusicVolume(int volume); // 0x417670
void SetMusicSwitch(int enabled); // 0x4176e0
void MusicEventCallback(uint32_t message, uint32_t first, uint32_t second); // 0x417780
