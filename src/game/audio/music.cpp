#include "audio/music.h"

#include <new>

#include "app/rebapp_globals.h"
#include "core/registry.h"
#include "port/native/audio_context.h"


int UiHumanNextAdvice();  // 0x41d3b0

extern const char *g_localMachineRootName;  // 0x6a844c "HKEY_LOCAL_MACHINE"
extern const char *g_registryKeyPath;       // 0x6a81fc
extern const char *g_musicVolumeName;       // 0x6a8584 "MusicVolume"
extern const char *g_musicSwitchName;       // 0x6a8578 "MusicSwitch"

namespace {

reb::native::AudioContext& MusicAudio() { return *reb::native::CurrentAudioContext(); }

constexpr int kMusicTrackCount = 0x10;
constexpr uint32_t kBufferFinished = 0x4c9;
constexpr uint32_t kDisplayOptionMusic = 0x100;

}  // namespace

void AppConfirmWaitDialog() {
    if (g_appDialogMessage == 0x472) AppPostDialogResult(0, nullptr);
}

void InitMusic() {
    if (!reb::native::CurrentAudioContext()) return;
    MusicAudio().musicDirectory.clear();
    char* location = GetTooBigLocation();
    if (location) {
        MusicAudio().musicDirectory += StringFromText(location);
        MusicAudio().musicDirectory += std::string("\\");
        ::operator delete(location);
    }
    MusicAudio().musicDirectory += std::string("MDATA\\");
    SoundManager::SetEventCallback(MusicEventCallback);
    MusicAudio().musicVolume = -static_cast<int>(ReadRegistryDword(g_registryKeyPath, g_musicVolumeName));
    const int musicOff = static_cast<int>(ReadRegistryDword(g_registryKeyPath, g_musicSwitchName));
    MusicAudio().musicSwitchOn = musicOff == 0;
    if (musicOff != 0) MusicAudio().musicFlags.m_value |= music_flag::kMuted;
    uint32_t options = AppGetDisplayOptions();
    options = MusicAudio().musicSwitchOn == 0 ? options & ~kDisplayOptionMusic : options | kDisplayOptionMusic;
    AppSetDisplayOptions(options);
}

void StartMusicTrack(uint32_t index) {
    if (!reb::native::CurrentAudioContext()) return;
    if (index < kMusicTrackCount && MusicAudio().musicSwitchOn != 0) {
        SoundManager::Query(&MusicAudio().musicRequest);
        if (MusicAudio().musicRequest.m_playing) StopMusic();
        MusicAudio().musicRequest.m_soundId = static_cast<int>(index) + 1;
        std::string path(MusicAudio().musicDirectory);
        path += StringFromText(g_musicFileNames[index]);
        SoundManager::PlayFromFile(&MusicAudio().musicRequest, path.c_str(), 0);
        SoundManager::SetVolume(&MusicAudio().musicRequest, MusicAudio().musicVolume);
    }
}

void PlayAdviceMusic(uint32_t index) {
    if (!reb::native::CurrentAudioContext()) return;
    if (index < kMusicTrackCount) {
        StopMusic();
        StartMusicTrack(index);
        MusicAudio().musicFlags.m_value |= music_flag::kAdviceTrack;
    }
}

void StopMusic() {
    if (!reb::native::CurrentAudioContext()) return;
    SoundManager::Stop(&MusicAudio().musicRequest, 1);
    SoundManager::Release(&MusicAudio().musicRequest);
    MusicAudio().musicFlags.m_value &= ~music_flag::kAdviceTrack;
}

void SetMusicVolume(int volume) {
    if (!reb::native::CurrentAudioContext()) return;
    MusicAudio().musicVolume = volume;
    WriteRegistryDword(g_localMachineRootName, g_registryKeyPath, g_musicVolumeName, static_cast<uint32_t>(-volume));
    if (!(MusicAudio().musicFlags.m_value & music_flag::kMuted) && MusicAudio().musicRequest.m_soundId != 0) {
        SoundManager::Query(&MusicAudio().musicRequest);
        if (MusicAudio().musicRequest.m_playing) SoundManager::SetVolume(&MusicAudio().musicRequest, MusicAudio().musicVolume);
    }
}

void SetMusicSwitch(int enabled) {
    if (!reb::native::CurrentAudioContext()) return;
    MusicAudio().musicSwitchOn = enabled;
    if (enabled == 0) {
        if (!(MusicAudio().musicFlags.m_value & music_flag::kMuted)) {
            MusicAudio().musicFlags.m_value |= music_flag::kMuted;
            StopMusic();
        }
    } else if (MusicAudio().musicFlags.m_value & music_flag::kMuted) {
        MusicAudio().musicFlags.m_value &= ~music_flag::kMuted;
        StartMusicTrack(static_cast<uint32_t>(UiHumanNextAdvice()));
    }
    WriteRegistryDword(g_localMachineRootName, g_registryKeyPath, g_musicSwitchName, enabled == 0);
    uint32_t options = AppGetDisplayOptions();
    if (MusicAudio().musicSwitchOn != 0) {
        AppSetDisplayOptions(options | kDisplayOptionMusic);
    } else {
        AppSetDisplayOptions(options & ~kDisplayOptionMusic);
    }
}

void MusicEventCallback(uint32_t message, uint32_t first, uint32_t second) {
    if (!reb::native::CurrentAudioContext()) return;
    if (message != kBufferFinished) return;
    if ((first & 0xffff) != MusicAudio().musicRequest.SourceKey()) return;
    SoundManager::Release(&MusicAudio().musicRequest);
    if (MusicAudio().musicFlags.m_value & music_flag::kAdviceTrack) {
        StartMusicTrack(static_cast<uint32_t>(MusicAudio().musicRequest.m_soundId - 1));
    } else {
        StartMusicTrack(static_cast<uint32_t>(UiHumanNextAdvice()));
    }
}
const char *g_musicSwitchName = nullptr;
const char *g_musicVolumeName = nullptr;
