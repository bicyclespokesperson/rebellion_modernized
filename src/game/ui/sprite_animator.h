#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "platform/win32.h"

class Sprite;
class SpriteAnimator;
class SpriteStage;

// A step of an animation script as the sprite animator runs it (made by the factory 0x403ed0 from a script kind, owned
// by the module that reconstructs the scripts): declared as far as the animator uses it.
class AnimationStep : public LinkNode {
public:
    virtual void Attach(SpriteAnimator* animator);            // slot 3
    virtual void SetScript(const uint32_t* sequence);         // slot 4
    virtual void SetStart(int x, int y);                      // slot 5
    // Moves the script on one frame; sets *changed and the rectangle that changed; non-zero when the script is over.
    virtual int Advance(RECT* area, int* changed);            // slot 6
    virtual void Begin();                                     // slot 7
    virtual void Finish();                                    // slot 8
    virtual void Abort();                                     // slot 9

    SpriteStage* m_stage;  // +0x0c
};

// Runs animation scripts on the sprites of a SpriteStage, one script step after the other (VT 0x65a358, ctor 0x4727e0).
// The animator is an IdNode (the stage keeps it by id); m_current is the step being played and the following ones wait
// in m_steps. When the queue runs dry and an end action has been set, the script is started again.
class SpriteAnimator : public IdNode {
public:
    explicit SpriteAnimator(int spriteCount);  // 0x4727e0
    virtual ~SpriteAnimator();                 // 0x4728a0 (scalar deleting destructor 0x472880)

    // Sets the origin the scripts' positions are relative to.
    virtual void SetOrigin(int x, int y);  // slot 1, 0x472ae0
    // Queues the script `kind` (the factory's index) for `sequence`, started at (b, c); starts playing it if no step is
    // playing, and (unless `noStep`) steps once at once.
    virtual int Start(int kind, const uint32_t* sequence, SpriteStage* stage, int b, int c, int noStep);  // slot 2, 0x472930
    // Advances one frame; sets *changed and the area that changed when the sprites moved or changed.
    virtual void Step(RECT* area, int* changed);  // slot 3, 0x4729c0
    // Hands the animator the sprite of slot `index`.
    virtual void SetSprite(int index, Sprite* sprite);  // slot 4, 0x472b00

    // Remembers the arguments to restart the script with when the queue runs dry.
    void SetEndAction(int kind, const uint32_t* sequence, SpriteStage* stage, int frames, void (*callback)(int));  // 0x472b20
    // Whether one of the animator's sprites has an opaque pixel at stage position (x, y).
    int HitTest(int x, int y);  // 0x472b50
    void Stop();                // 0x472ba0

    AnimationStep* m_current;  // +0x1c
    Sprite** m_sprites;        // +0x20
    int m_spriteCount;         // +0x24
    int m_originX;             // +0x28
    int m_originY;             // +0x2c
    alignas(4) uint8_t m_steps[0x10];  // +0x30 the waiting steps (a NodeQueue subclass, VT 0x65c0a4)
    int m_hasEndAction;        // +0x40
    int m_endKind;             // +0x44
    uint32_t m_endSequence;    // +0x48 (a copy of the first word of the script's sequence)
    SpriteStage* m_endStage;   // +0x4c
    int m_endFrames;           // +0x50
    uint32_t m_endCallback;    // +0x54 the completion hook (a function address)
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(SpriteAnimator, 0x58);
REB_ASSERT_OFFSET(SpriteAnimator, m_current, 0x1c);
REB_ASSERT_OFFSET(SpriteAnimator, m_sprites, 0x20);
REB_ASSERT_OFFSET(SpriteAnimator, m_steps, 0x30);
REB_ASSERT_OFFSET(SpriteAnimator, m_hasEndAction, 0x40);
REB_ASSERT_OFFSET(SpriteAnimator, m_endCallback, 0x54);
#pragma clang diagnostic pop
