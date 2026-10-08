#include "audio/music.h"

#include <new>

#include "app/rebapp_globals.h"
#include "core/registry.h"

extern "C" int __cdecl atexit(void(__cdecl* function)(void));
extern "C" void __cdecl LAB_00417820();
extern "C" void __cdecl LAB_00417880();
extern "C" void __cdecl LAB_004178d0();

int UiHumanNextAdvice();  // 0x41d3b0

namespace {

constexpr int kMusicTrackCount = 0x10;
constexpr uint32_t kBufferFinished = 0x4c9;
constexpr uint32_t kDisplayOptionMusic = 0x100;
const char kRegistryRoot[] = "HKEY_LOCAL_MACHINE";
const char kRegistryKey[] = "SOFTWARE\\LucasArts Entertainment Company LLC\\Star Wars Rebellion";
const char kVolumeValue[] = "MusicVolume";
const char kSwitchValue[] = "MusicSwitch";

}  // namespace

void AppConfirmWaitDialog() {
    if (g_appDialogMessage == 0x472) AppPostDialogResult(0, nullptr);
}

void InitMusic() {
    g_musicDirectory.Clear();
    char* location = GetTooBigLocation();
    if (location) {
        g_musicDirectory += String(location);
        g_musicDirectory += String("\\");
        ::operator delete(location);
    }
    g_musicDirectory += String("MDATA\\");
    SoundManager::SetEventCallback(MusicEventCallback);
    g_musicVolume = -static_cast<int>(ReadRegistryDword(kRegistryKey, kVolumeValue));
    const int musicOff = static_cast<int>(ReadRegistryDword(kRegistryKey, kSwitchValue));
    g_musicSwitchOn = musicOff == 0;
    if (musicOff != 0) g_musicFlags.m_value |= music_flag::kMuted;
    uint32_t options = AppGetDisplayOptions();
    options = g_musicSwitchOn == 0 ? options & ~kDisplayOptionMusic : options | kDisplayOptionMusic;
    AppSetDisplayOptions(options);
}

void StartMusicTrack(uint32_t index) {
    if (index < kMusicTrackCount && g_musicSwitchOn != 0) {
        SoundManager::Query(&g_musicRequest);
        if (g_musicRequest.m_playing) StopMusic();
        g_musicRequest.m_soundId = static_cast<int>(index) + 1;
        String path(g_musicDirectory);
        path += String(g_musicFileNames[index]);
        SoundManager::PlayFromFile(&g_musicRequest, path.m_text, 0);
        SoundManager::SetVolume(&g_musicRequest, g_musicVolume);
    }
}

void PlayAdviceMusic(uint32_t index) {
    if (index < kMusicTrackCount) {
        StopMusic();
        StartMusicTrack(index);
        g_musicFlags.m_value |= music_flag::kAdviceTrack;
    }
}

void StopMusic() {
    SoundManager::Stop(&g_musicRequest, 1);
    SoundManager::Release(&g_musicRequest);
    g_musicFlags.m_value &= ~music_flag::kAdviceTrack;
}

void SetMusicVolume(int volume) {
    g_musicVolume = volume;
    WriteRegistryDword(kRegistryRoot, kRegistryKey, kVolumeValue, static_cast<uint32_t>(-volume));
    if (!(g_musicFlags.m_value & music_flag::kMuted) && g_musicRequest.m_soundId != 0) {
        SoundManager::Query(&g_musicRequest);
        if (g_musicRequest.m_playing) SoundManager::SetVolume(&g_musicRequest, g_musicVolume);
    }
}

void SetMusicSwitch(int enabled) {
    g_musicSwitchOn = enabled;
    if (enabled == 0) {
        if (!(g_musicFlags.m_value & music_flag::kMuted)) {
            g_musicFlags.m_value |= music_flag::kMuted;
            StopMusic();
        }
    } else if (g_musicFlags.m_value & music_flag::kMuted) {
        g_musicFlags.m_value &= ~music_flag::kMuted;
        StartMusicTrack(static_cast<uint32_t>(UiHumanNextAdvice()));
    }
    WriteRegistryDword(kRegistryRoot, kRegistryKey, kSwitchValue, enabled == 0);
    uint32_t options = AppGetDisplayOptions();
    if (g_musicSwitchOn != 0) {
        AppSetDisplayOptions(options | kDisplayOptionMusic);
    } else {
        AppSetDisplayOptions(options & ~kDisplayOptionMusic);
    }
}

void __cdecl MusicEventCallback(uint32_t message, uint32_t first, uint32_t second) {
    if (message != kBufferFinished) return;
    if ((first & 0xffff) != g_musicRequest.SourceKey()) return;
    SoundManager::Release(&g_musicRequest);
    if (g_musicFlags.m_value & music_flag::kAdviceTrack) {
        StartMusicTrack(static_cast<uint32_t>(g_musicRequest.m_soundId - 1));
    } else {
        StartMusicTrack(static_cast<uint32_t>(UiHumanNextAdvice()));
    }
}

void ConstructMusicFlags() { new (&g_musicFlags) PersistentInt(0); }

void RegisterMusicFlagsAtExit() { atexit(LAB_00417820); }

void InitMusicFlags() {
    ConstructMusicFlags();
    RegisterMusicFlagsAtExit();
}

void ConstructMusicRequest() { new (&g_musicRequest) SoundRequest(1, 0, 0, 0, 0, 0); }

void RegisterMusicRequestAtExit() { atexit(LAB_00417880); }

void InitMusicRequest() {
    ConstructMusicRequest();
    RegisterMusicRequestAtExit();
}

void ConstructMusicDirectory() { new (&g_musicDirectory) String(); }

void RegisterMusicDirectoryAtExit() { atexit(LAB_004178d0); }

void InitMusicDirectory() {
    ConstructMusicDirectory();
    RegisterMusicDirectoryAtExit();
}
