#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "audio/sound_manager.h"
#include "core/game_string.h"
#include "core/utility_classes.h"

// The game's background music: one SoundRequest (g_musicRequest) plays the file MDATA.3nn of the music directory chosen by
// StartMusicTrack. A track that ends (the sound manager's buffer-finished event) is replaced by the next one the advisor
// asks for. The two music options live in the registry (MusicVolume, MusicSwitch).

namespace music_flag {
constexpr int kMuted = 0x1;          // the music switch is off
constexpr int kAdviceTrack = 0x2;    // the current track was started by PlayAdviceMusic and repeats
}  // namespace music_flag

extern SoundRequest g_musicRequest;      // 0x6b1478
extern PersistentInt g_musicFlags;       // 0x6b1490
extern String g_musicDirectory;          // 0x6b1498
extern int g_musicSwitchOn;              // 0x6a8470
extern int g_musicVolume;                // 0x6b14a8
extern const char* g_musicFileNames[16];  // 0x6a8478

void AppConfirmWaitDialog();                    // 0x4173a0
void InitMusic();                               // 0x4173c0: music directory, event callback, saved options
void StartMusicTrack(uint32_t index);           // 0x417520
void PlayAdviceMusic(uint32_t index);           // 0x417610: replaces the current track and marks it as the advice track
void StopMusic();                               // 0x417640
void SetMusicVolume(int volume);                // 0x417670
void SetMusicSwitch(int enabled);               // 0x4176e0
void REB_CDECL MusicEventCallback(uint32_t message, uint32_t first, uint32_t second);  // 0x417780

void InitMusicFlags();         // 0x4177f0
void ConstructMusicFlags();    // 0x417800
void RegisterMusicFlagsAtExit();  // 0x417810
void InitMusicRequest();       // 0x417840
void ConstructMusicRequest();  // 0x417850
void RegisterMusicRequestAtExit();  // 0x417870
void InitMusicDirectory();     // 0x4178a0
void ConstructMusicDirectory();  // 0x4178b0
void RegisterMusicDirectoryAtExit();  // 0x4178c0
