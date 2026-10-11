#pragma once

#include <memory>

class DirectDrawHost;

namespace reb::native {

// Owns startup work that depends on native platform wiring. Typed game globals
// use normal C++ lifetime; the delayed DirectDraw host belongs to this scope.
class GameInitialization {
public:
    GameInitialization();
    ~GameInitialization();

    GameInitialization(const GameInitialization&) = delete;
    GameInitialization& operator=(const GameInitialization&) = delete;

    void Start();
    void Shutdown() noexcept;

private:
    std::unique_ptr<DirectDrawHost> directDrawHost_;
    bool started_ = false;
};

}  // namespace reb::native
