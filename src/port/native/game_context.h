#pragma once

#include <memory>
#include <cstdint>
#include "core/game_string.h"
#include "platform/win32.h"

class DirectDrawHost;
class BattleFlow;
class RebellionApplication;
struct UiResources;

namespace reb::native {

class AudioContext;

// Owns the application and its platform-dependent display service.
class GameContext {
public:
    GameContext();
    ~GameContext();

    GameContext(const GameContext&) = delete;
    GameContext& operator=(const GameContext&) = delete;

    void Start();
    void Shutdown() noexcept;
    RebellionApplication& CreateApplication(HINSTANCE instance, uint32_t flags, const std::string& commandLine);
    void ReleaseApplication() noexcept;
    AudioContext& GetAudioContext() noexcept;
    BattleFlow* GetBattleFlow() const noexcept;
    void SetBattleFlow(std::unique_ptr<BattleFlow> battleFlow) noexcept;

private:
    std::unique_ptr<AudioContext> audio_;
    std::unique_ptr<UiResources> uiResources_;
    std::unique_ptr<DirectDrawHost> directDrawHost_;
    std::unique_ptr<RebellionApplication> application_;
    std::unique_ptr<BattleFlow> battleFlow_;
    bool started_ = false;
};

}  // namespace reb::native
