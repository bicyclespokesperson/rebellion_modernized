#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "platform/win32.h"
#include "ui/animation_step.h"

// The queue of waiting steps: a NodeQueue with its own vtable (0x65c0a4), built by 0x4ad770 and destroyed by 0x4ad7b0.
class AnimationStepQueue : public NodeQueue {
public:
    AnimationStepQueue();           // 0x4ad770
    virtual ~AnimationStepQueue();  // 0x4ad790 (deleting), 0x4ad7b0
};
REB_ASSERT_SIZE(AnimationStepQueue, 0x10);

class Sprite;
class SpriteAnimator;
class SpriteStage;

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
    virtual int Start(int kind, const uint32_t* sequence, SpriteStage* stage, int b, intptr_t c, int noStep);  // slot 2, 0x472930
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
    alignas(void*) uint8_t m_steps[REB_LP64_SCALED(0x10)];  // +0x30 the waiting steps (a NodeQueue subclass, VT 0x65c0a4)
    int m_hasEndAction;        // +0x40
    int m_endKind;             // +0x44
    uint32_t m_endSequence;    // +0x48 (a copy of the first word of the script's sequence)
    SpriteStage* m_endStage;   // +0x4c
    int m_endFrames;           // +0x50
    uintptr_t m_endCallback;   // +0x54 the completion hook (a function address)
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
