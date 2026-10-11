#pragma once
#include <cstdint>
#include <deque>
#include <memory>

#include "core/containers.h"
#include "platform/win32.h"
#include "ui/animation_step.h"

class Sprite;
class SpriteAnimator;
class SpriteStage;

// Runs animation scripts on the sprites of a SpriteStage, one script step after the other (VT 0x65a358, ctor 0x4727e0).
// The animator is an IdNode (the stage keeps it by id); m_current is the step being played and the following ones wait
// in m_steps. When the queue runs dry and an end action has been set, the script is started again.
class SpriteAnimator : public IdNode
{
public:
  explicit SpriteAnimator(int spriteCount); // 0x4727e0
  ~SpriteAnimator() override; // 0x4728a0 (scalar deleting destructor 0x472880)

  // Sets the origin the scripts' positions are relative to.
  virtual void SetOrigin(int x, int y); // slot 1, 0x472ae0
  // Queues the script `kind` (the factory's index) for `sequence`, started at (b, c); starts playing it if no step is
  // playing, and (unless `noStep`) steps once at once.
  virtual int
  Start(int kind, const uint32_t* sequence, SpriteStage* stage, int b, intptr_t c, int noStep); // slot 2, 0x472930
  // Advances one frame; sets *changed and the area that changed when the sprites moved or changed.
  virtual void Step(RECT* area, int* changed); // slot 3, 0x4729c0
  // Hands the animator the sprite of slot `index`.
  virtual void SetSprite(int index, Sprite* sprite); // slot 4, 0x472b00

  // Remembers the arguments to restart the script with when the queue runs dry.
  void
  SetEndAction(int kind, const uint32_t* sequence, SpriteStage* stage, int frames, void (*callback)(int)); // 0x472b20
  // Whether one of the animator's sprites has an opaque pixel at stage position (x, y).
  int HitTest(int x, int y); // 0x472b50
  void Stop(); // 0x472ba0

  AnimationStep* m_current; // +0x1c
  Sprite** m_sprites; // +0x20
  int m_spriteCount; // +0x24
  int m_originX; // +0x28
  int m_originY; // +0x2c
  std::deque<std::unique_ptr<AnimationStep>> m_steps; // +0x30 waiting script steps
  int m_hasEndAction; // +0x40
  int m_endKind; // +0x44
  uint32_t m_endSequence; // +0x48 (a copy of the first word of the script's sequence)
  SpriteStage* m_endStage; // +0x4c
  int m_endFrames; // +0x50
  uintptr_t m_endCallback; // +0x54 the completion hook (a function address)
};
