#pragma once
#include <cstdint>

#include "core/containers.h"
#include "platform/win32.h"

// Stand-in declaration of the sprite animator (VT 0x65a358, constructor 0x4727e0, range 0x4727e0-0x472bc0, owned by
// the module that reconstructs that range): drives a set of Sprites on a SpriteStage frame by frame. Only what
// SpriteStage uses is declared; drop this header for the real one when that range is reconstructed.
class Sprite;
class SpriteStage;

class SpriteAnimator : public IdNode {
public:
    virtual ~SpriteAnimator();     // slot 0, 0x472880 (scalar deleting destructor)
    virtual void Virtual01();      // slot 1, 0x472ae0
    virtual void Start(int a, void* sequence, SpriteStage* stage, int b, int c, int d);  // slot 2, 0x472930
    // Advances one frame; sets *changed and the area that changed when the sprites moved or changed.
    virtual void Step(RECT* area, int* changed);  // slot 3, 0x4729c0
    virtual void Virtual04();      // slot 4, 0x472b00

    // Whether one of the animator's sprites has an opaque pixel at stage position (x, y).
    int HitTest(int x, int y);  // 0x472b50
    void Stop();                // 0x472ba0

    void* m_unk1c;          // +0x1c
    Sprite** m_sprites;     // +0x20
    int m_spriteCount;      // +0x24
};
