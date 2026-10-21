#pragma once
#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <ranges>

#include "audio/sound_manager.h"
#include "platform/win32.h"

class DibBitmap;
class DeltaFrame;

class DeltaFrameList
{
  public:
  DeltaFrameList();
  ~DeltaFrameList();
  DeltaFrameList(const DeltaFrameList&) = delete;
  DeltaFrameList& operator=(const DeltaFrameList&) = delete;
  uint32_t AddWithId(DeltaFrame* frame);
  DeltaFrame* First() const;
  auto Entries() const
  {
    return std::views::values(m_frames);
  }
  auto PositionOf(const DeltaFrame* frame) const
  {
    return std::ranges::find_if(Entries(), [frame](const auto& owned) {
      return owned.get() == frame;
    });
  }

  private:
  std::map<uint32_t, std::unique_ptr<DeltaFrame>> m_frames;
  uint32_t m_nextId;
};
class Sprite;
class SpriteAnimator;
class SpriteStage;

// A step of an animation script as the SpriteAnimator runs it (VT 0x6584e8, 0x48 bytes). The step is created by the
// factory (CreateAnimationStep) from a script kind, attached to the animator, handed the script (the id of an RCDATA
// resource: low word the resource, high word the index of the resource DLL), started (Begin), advanced one frame at a
// time (Advance) until it reports that it is over, and finished (Finish). The script resource lists the bitmaps (and,
// for some kinds, delta frames and sounds) the step shows on the animator's sprites.
//
// The concrete steps (kinds 4, 5, 0x11-0x15) are registered in g_animationSteps by static initializers and cloned by
// Create.
class AnimationStep
{
  public:
  AnimationStep();          // 0x403dd0
  virtual ~AnimationStep(); // 0x403e50 (scalar deleting destructor), 0x403e70

  virtual void Attach(SpriteAnimator* animator);    // slot 3, 0x403e90
  virtual void SetScript(const uint32_t* sequence); // slot 4, 0x403ea0
  virtual void SetStart(int x, intptr_t y);         // slot 5, 0x403ef0 (does nothing)
  // Moves the script on one frame; sets *changed and the rectangle that changed; non-zero when the script is over.
  virtual int Advance(RECT* area, int* changed) = 0; // slot 6
  virtual void Begin() = 0;                          // slot 7
  virtual void Finish() = 0;                         // slot 8
  virtual void Abort();                              // slot 9, 0x404090 (does nothing)
  virtual AnimationStep* Create() = 0;               // slot 10: a new step of the same kind
  virtual int GetKind()
  {
    return 0;
  } // slot 11, the shared "return 0" stub

  // Plays the sound named by `pair` {sound id, resource DLL index} in the effect request (+0x30), stopping a sound that
  // is still playing there; returns its handle.
  int PlayEffectSound(const uint16_t* pair, int loop); // 0x403f00
  // The same for the main request (+0x18).
  int PlayMainSound(const uint16_t* pair, int loop); // 0x403f70
  // Whether the main sound is playing and is of a kind that can be waited for.
  int IsSoundPlaying();            // 0x403fe0
  void StopEffectSound(int sweep); // 0x404020
  void StopSound(int sweep);       // 0x404040

  SpriteStage* m_stage;       // +0x0c
  SpriteAnimator* m_animator; // +0x10
  uint32_t m_script;          // +0x14
  SoundRequest m_sound;       // +0x18
  uint32_t m_pad2c;           // +0x2c (cleared with the request)
  SoundRequest m_effectSound; // +0x30
  uint32_t m_pad44;           // +0x44
};

// The registry of the step kinds: g_animationSteps[kind] is the prototype `Create` clones (kinds 1-32).
extern AnimationStep* g_animationSteps[33];          // 0x6b059c
int RegisterAnimationStep(AnimationStep* prototype); // 0x403eb0, always 0
AnimationStep* CreateAnimationStep(int kind);        // 0x403ed0, null for an unknown kind

// All the concrete steps restart from their script by overriding SetScript.
class ScriptStep : public AnimationStep
{
  public:
  void SetScript(const uint32_t* sequence) override; // 0x411b50
};

// Steps that keep one bitmap per sprite of the animator (+0x48 count, +0x4c the bitmaps).
class BitmapListStep : public ScriptStep
{
  public:
  void Attach(SpriteAnimator* animator) override; // 0x412610
  // Hands bitmap `index` to the animator's sprite (creating the sprite when the animator has none yet).
  void ShowBitmap(int index, DibBitmap* bitmap);

  int m_spriteCount;     // +0x48
  DibBitmap** m_bitmaps; // +0x4c
};

// Waits `m_delay` frames, then ends.
class TimedStep : public BitmapListStep
{
  public:
  void SetStart(int x, intptr_t y) override; // 0x412900 (x: the delay, y: the callback)
  void Abort() override;                     // 0x404b90

  void (*m_callback)(int); // +0x50 called with the animator's id when the step finishes
  uint32_t m_delay;        // +0x54
};

// Steps that wait a number of frames and remember a position.
class PositionedStep : public BitmapListStep
{
  public:
  void SetStart(int x, intptr_t y) override;      // 0x4124b0
  int Advance(RECT* area, int* changed) override; // 0x412260
  void Abort() override;                          // 0x412090

  int m_wait; // +0x50
  int m_x;    // +0x54
  int m_y;    // +0x58
};

// Kind 5: shows its bitmaps for one frame; at the end hides them and calls the callback.
class AnimationStep05 : public TimedStep
{
  public:
  AnimationStep05();                              // 0x404830
  ~AnimationStep05() override;                    // 0x404860 (deleting), 0x404880
  int Advance(RECT* area, int* changed) override; // 0x404910
  void Begin() override;                          // 0x4049a0
  void Finish() override;                         // 0x404b40
  AnimationStep* Create() override;               // 0x404ba0
  int GetKind() override;                         // 0x404c20

  int m_hold; // +0x58
};

// Kind 0x12: like 5 without the hold, hides the sprites at the end and asks the UI to redraw.
class AnimationStep12 : public TimedStep
{
  public:
  AnimationStep12();                              // 0x412520
  ~AnimationStep12() override;                    // 0x412550 (deleting), 0x412570
  int Advance(RECT* area, int* changed) override; // 0x412660
  void Begin() override;                          // 0x412680
  void Finish() override;                         // 0x412830
  AnimationStep* Create() override;               // 0x412870
  int GetKind() override;                         // 0x4128f0
};

// Kind 0x15 and kind 0x11: show their bitmaps for a number of frames; 0x15 ends by setting g_stepSignal, 0x11 by
// pointing the human player's UI at its position.
class AnimationStep15 : public PositionedStep
{
  public:
  AnimationStep15();                // 0x411e20
  ~AnimationStep15() override;      // 0x411e50 (deleting), 0x411e70
  void Begin() override;            // 0x411f00
  void Finish() override;           // 0x412080
  AnimationStep* Create() override; // 0x4120a0
  int GetKind() override;           // 0x412120
};

class AnimationStep11 : public PositionedStep
{
  public:
  AnimationStep11();                // 0x412180
  ~AnimationStep11() override;      // 0x4121b0 (deleting), 0x4121d0
  void Begin() override;            // 0x412280
  void Finish() override;           // 0x412400
  AnimationStep* Create() override; // 0x412420
  int GetKind() override;           // 0x4124a0
};

// Kind 0x14: a droid script step; its Advance reports g_stepSignal.
class AnimationStep14 : public ScriptStep
{
  public:
  AnimationStep14();                              // 0x411a30
  ~AnimationStep14() override;                    // 0x411a50 (deleting), 0x411a70
  void Attach(SpriteAnimator* animator) override; // 0x411b00
  int Advance(RECT* area, int* changed) override; // 0x411b60
  void Begin() override;                          // 0x411b80
  void Finish() override;                         // 0x619730 (shared empty function)
  void Abort() override;                          // 0x411d30
  AnimationStep* Create() override;               // 0x411d40
  int GetKind() override;                         // 0x411dc0

  int m_firstWord;       // +0x48 the first word of the script resource
  int m_spriteCount;     // +0x4c
  DibBitmap** m_bitmaps; // +0x50
  int m_aborted;         // +0x54
};

// Kinds 4 and 0x13: play a script of delta frames over the bitmaps (one frame list per sprite); 4 can be aborted.
class DeltaFrameStep : public ScriptStep
{
  public:
  void Finish() override; // 0x404730
  void Abort() override;  // 0x404740

  // The bodies shared by the two kinds.
  void AttachImpl(SpriteAnimator* animator);
  void LoadFrames();

  int m_spriteCount;            // +0x48
  int m_frameCount;             // +0x4c
  int m_unk50;                  // +0x50
  DibBitmap** m_bitmaps;        // +0x54 the original bitmaps
  DeltaFrameList* m_frameLists; // +0x58 per sprite the delta frames 1..m_frameCount-1
  DeltaFrame** m_cursors;       // +0x5c per sprite the next frame to apply
  DibBitmap** m_workBitmaps;    // +0x60 the copies the frames are applied to and the sprites show
  int m_stopRequested;          // +0x64
};

class AnimationStep04 : public DeltaFrameStep
{
  public:
  AnimationStep04();                              // 0x4040f0
  ~AnimationStep04() override;                    // 0x404120 (deleting), 0x404140
  void Attach(SpriteAnimator* animator) override; // 0x404220
  int Advance(RECT* area, int* changed) override; // 0x404360
  void Begin() override;                          // 0x404440
  AnimationStep* Create() override;               // 0x404750
  int GetKind() override;                         // 0x4047d0
};

class AnimationStep13 : public DeltaFrameStep
{
  public:
  AnimationStep13();                              // 0x412970
  ~AnimationStep13() override;                    // 0x4129a0 (deleting), 0x4129c0
  void Attach(SpriteAnimator* animator) override; // 0x412a90
  int Advance(RECT* area, int* changed) override; // 0x412b80
  void Begin() override;                          // 0x412c40
  AnimationStep* Create() override;               // 0x412f30
  int GetKind() override;                         // 0x412fb0
};

void RegisterAnimationStep04(); // 0x4040d0
void RegisterAnimationStep05(); // 0x404810
void RegisterAnimationStep14(); // 0x411a10
void RegisterAnimationStep15(); // 0x411e00
void RegisterAnimationStep11(); // 0x412160
void RegisterAnimationStep12(); // 0x412500
void RegisterAnimationStep13(); // 0x412950
