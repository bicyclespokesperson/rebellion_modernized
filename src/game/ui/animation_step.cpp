#include "core/reb_abi.h"
#include "ui/animation_step.h"

#include <cstring>
#include <new>

#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/game_ui_facade.h"
#include "ui/sprite.h"
#include "ui/sprite_animator.h"
#include "ui/sprite_stage.h"

extern "C" int REB_CDECL atexit(void(REB_CDECL* function)(void));
void DestroyAnimationStep04();
void DestroyAnimationStep05();
void DestroyAnimationStep14();
void DestroyAnimationStep15();
void DestroyAnimationStep11();
void DestroyAnimationStep12();
void DestroyAnimationStep13();

extern AnimationStep04 g_animationStep04;  // 0x6b0628
extern int g_animationStep04Registered;    // 0x6b0620
extern AnimationStep05 g_animationStep05;  // 0x6b0698
extern int g_animationStep05Registered;    // 0x6b0690
extern AnimationStep14 g_animationStep14;  // 0x6b1258
extern int g_animationStep14Registered;    // 0x6b1250
extern AnimationStep15 g_animationStep15;  // 0x6b12c0
extern int g_animationStep15Registered;    // 0x6b12b8
extern AnimationStep11 g_animationStep11;  // 0x6b1328
extern int g_animationStep11Registered;    // 0x6b1320
extern AnimationStep12 g_animationStep12;  // 0x6b1390
extern int g_animationStep12Registered;    // 0x6b1388
extern AnimationStep13 g_animationStep13;  // 0x6b13f0
extern int g_animationStep13Registered;    // 0x6b13e8
extern int g_animationStepSignal;          // 0x6b12b0

namespace {

constexpr int kFirstKind = 1;
constexpr int kLastKind = 0x20;
constexpr uint32_t kBitmapResource = 2;
constexpr int kSoundRequestBytes = 0x18;
constexpr int kKindDelta04 = 4;
constexpr int kKindTimed05 = 5;
constexpr int kKindPositioned11 = 0x11;
constexpr int kKindTimed12 = 0x12;
constexpr int kKindDelta13 = 0x13;
constexpr int kKindDroid14 = 0x14;
constexpr int kKindPositioned15 = 0x15;
constexpr uint32_t kDroidScriptEnd = 0x2d37;

// The RCDATA (type 10) resource that holds the script's words.
struct StepResource {
    HMODULE module;
    HGLOBAL handle;
    const uint16_t* words;

    explicit StepResource(uint32_t script) : module(nullptr), handle(nullptr), words(nullptr) {
        module = FindResourceModule((script >> 16) & 0xffff);
        HRSRC info = FindResourceA(module, MAKEINTRESOURCEA(script & 0xffff), MAKEINTRESOURCEA(10));
        if (info) {
            handle = LoadResource(module, info);
            if (handle) words = static_cast<const uint16_t*>(LockResource(handle));
        }
    }
    void Free() { FreeResource(handle); }
};

void deleteBitmap(DibBitmap* bitmap) {
    bitmap->~DibBitmap();
    ::operator delete(bitmap);
}

// Gives the animator's sprite `index` the bitmap; the sprite is created and handed to stage and animator on first use.
void showBitmap(AnimationStep* step, int index, DibBitmap* bitmap) {
    SpriteAnimator* animator = step->m_animator;
    Sprite* existing = animator->m_sprites[index];
    if (existing == nullptr) {
        Sprite* sprite = NewOrNull<Sprite>(bitmap);
        step->m_stage->AddSprite(sprite);
        animator->SetSprite(index, sprite);
    } else {
        existing->SetBitmap(bitmap);
    }
}

// An array of `count` frame lists with the element count in front, as the compiler lays out new[] of a class with a destructor.
IdAvlTreeElement* newFrameLists(int count) {
    int* raw = static_cast<int*>(::operator new(count * sizeof(IdAvlTreeElement) + sizeof(int)));
    if (raw == nullptr) return nullptr;
    *raw = count;
    IdAvlTreeElement* lists = reinterpret_cast<IdAvlTreeElement*>(raw + 1);
    for (int i = 0; i < count; ++i) new (&lists[i]) IdAvlTreeElement();
    return lists;
}

// The vector deleting destructor (0x404310) the original reaches through the first element's vtable.
void deleteFrameLists(IdAvlTreeElement* lists) {
    int* raw = reinterpret_cast<int*>(lists) - 1;
    for (int i = *raw - 1; i >= 0; --i) lists[i].~IdAvlTreeElement();
    ::operator delete(raw);
}

DibBitmap* loadBitmap(HMODULE module, uint32_t resourceId) { return NewOrNull<DibBitmap>(module, resourceId, kBitmapResource); }

DibBitmap* copyOf(DibBitmap* source) {
    const int width = source->Width();
    const int height = source->Height();
    return NewOrNull<DibBitmap>(width, height, source);
}

}  // namespace

// ---- AnimationStep ----------------------------------------------------------------------------------------------------

AnimationStep::AnimationStep() : m_stage(nullptr), m_animator(nullptr), m_script(0), m_sound(0, 0, 0, 0, 0, 0), m_effectSound(0, 0, 0, 0, 0, 0) {}

AnimationStep::~AnimationStep() { REB_KEEP_VPTR_STORE(); }

void AnimationStep::Attach(SpriteAnimator* animator) { m_animator = animator; }

void AnimationStep::SetScript(const uint32_t* sequence) { m_script = *sequence; }

void AnimationStep::SetStart(int x, intptr_t y) {}

void AnimationStep::Abort() {}

int AnimationStep::PlayEffectSound(const uint16_t* pair, int loop) {
    if (m_effectSound.m_handle) SoundManager::Stop(&m_effectSound, 1);
    std::memset(&m_effectSound, 0, kSoundRequestBytes);
    m_effectSound.m_kind = kSoundKindWave;
    m_effectSound.m_soundId = pair[0];
    m_effectSound.m_loop = loop & 1;
    m_effectSound.m_moduleIndex = pair[1];
    SoundManager::Play(&m_effectSound);
    return m_effectSound.m_handle;
}

int AnimationStep::PlayMainSound(const uint16_t* pair, int loop) {
    if (m_sound.m_handle) SoundManager::Stop(&m_sound, 1);
    std::memset(&m_sound, 0, kSoundRequestBytes);
    m_sound.m_kind = kSoundKindWave;
    m_sound.m_soundId = pair[0];
    m_sound.m_loop = loop & 1;
    m_sound.m_moduleIndex = pair[1];
    SoundManager::Play(&m_sound);
    return m_sound.m_handle;
}

int AnimationStep::IsSoundPlaying() {
    int result = 0;
    if (m_sound.m_soundId != 0) {
        if (SoundManager::Query(&m_sound) == 0 && m_sound.m_playing) result = 1;
    }
    return result;
}

void AnimationStep::StopEffectSound(int sweep) { SoundManager::Stop(&m_effectSound, sweep); }

void AnimationStep::StopSound(int sweep) { SoundManager::Stop(&m_sound, sweep); }

int RegisterAnimationStep(AnimationStep* prototype) {
    if (prototype) g_animationSteps[prototype->GetKind()] = prototype;
    return 0;
}

AnimationStep* CreateAnimationStep(int kind) {
    AnimationStep* result = nullptr;
    if (kind >= kFirstKind && kind <= kLastKind) result = g_animationSteps[kind]->Create();
    return result;
}

void ScriptStep::SetScript(const uint32_t* sequence) { AnimationStep::SetScript(sequence); }

// ---- BitmapListStep, TimedStep, PositionedStep -----------------------------------------------------------------------------

void BitmapListStep::Attach(SpriteAnimator* animator) {
    AnimationStep::Attach(animator);
    m_spriteCount = animator->m_spriteCount;
    m_bitmaps = static_cast<DibBitmap**>(::operator new(m_spriteCount * sizeof(DibBitmap*)));
    for (int i = 0; i < m_spriteCount; ++i) m_bitmaps[i] = nullptr;
}

void BitmapListStep::ShowBitmap(int index, DibBitmap* bitmap) { showBitmap(this, index, bitmap); }

void TimedStep::SetStart(int x, intptr_t y) {
    m_delay = static_cast<uint32_t>(x);
    m_callback = reinterpret_cast<void (*)(int)>(y);
}

void TimedStep::Abort() { m_delay = 0; }

void PositionedStep::SetStart(int x, intptr_t y) {
    m_x = x;
    m_y = static_cast<int>(y);
}

int PositionedStep::Advance(RECT* area, int* changed) {
    *changed = 0;
    if (m_wait > 0) {
        --m_wait;
        return 0;
    }
    return 1;
}

void PositionedStep::Abort() { m_wait = 0; }

// ---- kind 5 ---------------------------------------------------------------------------------------------------------------

AnimationStep05::AnimationStep05() {
    m_bitmaps = nullptr;
    m_spriteCount = 0;
    m_callback = nullptr;
    m_delay = 1;
    m_hold = 1;
}

AnimationStep05::~AnimationStep05() {
    REB_KEEP_VPTR_STORE();
    if (m_bitmaps != nullptr) {
        for (int i = 0; i < m_spriteCount; ++i) {
            DibBitmap* bitmap = m_bitmaps[i];
            if (bitmap != nullptr) deleteBitmap(bitmap);
        }
        ::operator delete(m_bitmaps);
    }
}

int AnimationStep05::Advance(RECT* area, int* changed) {
    *changed = 0;
    int done = 0;
    if (m_delay != 0) {
        --m_delay;
    } else {
        done = 1;
    }
    if (m_hold > 0) {
        --m_hold;
        *changed = 1;
        area->left = m_animator->m_originX;
        area->right = area->left + (m_bitmaps[0] != nullptr ? m_bitmaps[0]->Width() : 0);
        area->top = m_animator->m_originY;
        area->bottom = area->top + (m_bitmaps[0] != nullptr ? m_bitmaps[0]->Height() : 0);
    }
    return done;
}

void AnimationStep05::Begin() {
    StepResource resource(m_script);
    if (resource.words != nullptr) {
        const uint16_t* word = resource.words;
        for (int i = 0; i < m_spriteCount; ++i) {
            ++word;
            m_bitmaps[i] = loadBitmap(resource.module, *word);
            showBitmap(this, i, m_bitmaps[i]);
        }
        if (resource.words[0] != 0) {
            const uint16_t pair[2] = {resource.words[0], static_cast<uint16_t>(m_script >> 16)};
            PlayEffectSound(pair, 0);
        }
        resource.Free();
    }
}

void AnimationStep05::Finish() {
    if (m_animator != nullptr) {
        for (int i = 0; i < m_spriteCount; ++i) {
            Sprite* sprite = m_animator->m_sprites[i];
            if (sprite != nullptr) sprite->SetBitmap(nullptr);
        }
    }
    if (m_callback != nullptr) m_callback(static_cast<int>(m_animator->m_id));
}

AnimationStep* AnimationStep05::Create() { return NewOrNull<AnimationStep05>(); }

int AnimationStep05::GetKind() { return kKindTimed05; }

// ---- kind 0x12 ------------------------------------------------------------------------------------------------------------

AnimationStep12::AnimationStep12() {
    m_bitmaps = nullptr;
    m_spriteCount = 0;
    m_callback = nullptr;
    m_delay = 1;
}

AnimationStep12::~AnimationStep12() {
    REB_KEEP_VPTR_STORE();
    if (m_bitmaps != nullptr) {
        for (int i = 0; i < m_spriteCount; ++i) {
            DibBitmap* bitmap = m_bitmaps[i];
            if (bitmap != nullptr) {
                deleteBitmap(bitmap);
                m_bitmaps[i] = nullptr;
            }
        }
        ::operator delete(m_bitmaps);
        m_bitmaps = nullptr;
    }
}

int AnimationStep12::Advance(RECT* area, int* changed) {
    *changed = 0;
    if (m_delay != 0) {
        --m_delay;
        return 0;
    }
    return 1;
}

void AnimationStep12::Begin() {
    StepResource resource(m_script);
    if (resource.words != nullptr) {
        const uint16_t* word = resource.words;
        for (int i = 0; i < m_spriteCount; ++i) {
            ++word;
            m_bitmaps[i] = loadBitmap(resource.module, *word);
            showBitmap(this, i, m_bitmaps[i]);
        }
        if (resource.words[0] != 0) {
            const uint16_t pair[2] = {resource.words[0], static_cast<uint16_t>(m_script >> 16)};
            PlayEffectSound(pair, 0);
        }
        resource.Free();
    }
    UiRequestRedraw(static_cast<int>(m_animator->m_id));
}

void AnimationStep12::Finish() {
    if (m_animator != nullptr) {
        for (int i = 0; i < m_spriteCount; ++i) {
            Sprite* sprite = m_animator->m_sprites[i];
            if (sprite != nullptr) sprite->SetBitmap(nullptr);
        }
    }
}

AnimationStep* AnimationStep12::Create() { return NewOrNull<AnimationStep12>(); }

int AnimationStep12::GetKind() { return kKindTimed12; }

// ---- kinds 0x15 and 0x11 --------------------------------------------------------------------------------------------------

namespace {

void loadBitmapsOrFinish(PositionedStep* step) {
    StepResource resource(step->m_script);
    if (resource.words == nullptr) {
        step->Finish();
        return;
    }
    const uint16_t* word = resource.words;
    for (int i = 0; i < step->m_spriteCount; ++i) {
        step->m_bitmaps[i] = loadBitmap(resource.module, *word);
        showBitmap(step, i, step->m_bitmaps[i]);
        ++word;
    }
    resource.Free();
}

void deleteBitmapList(PositionedStep* step) {
    if (step->m_bitmaps != nullptr) {
        for (int i = 0; i < step->m_spriteCount; ++i) {
            DibBitmap* bitmap = step->m_bitmaps[i];
            if (bitmap != nullptr) deleteBitmap(bitmap);
        }
        ::operator delete(step->m_bitmaps);
    }
}

}  // namespace

AnimationStep15::AnimationStep15() {
    m_bitmaps = nullptr;
    m_spriteCount = 0;
    m_x = 0;
    m_y = 0;
    m_wait = 1;
}

AnimationStep15::~AnimationStep15() {
    REB_KEEP_VPTR_STORE();
    deleteBitmapList(this);
}

void AnimationStep15::Begin() { loadBitmapsOrFinish(this); }

void AnimationStep15::Finish() { g_animationStepSignal = 1; }

AnimationStep* AnimationStep15::Create() { return NewOrNull<AnimationStep15>(); }

int AnimationStep15::GetKind() { return kKindPositioned15; }

AnimationStep11::AnimationStep11() {
    m_bitmaps = nullptr;
    m_spriteCount = 0;
    m_x = 0;
    m_y = 0;
    m_wait = 1;
}

AnimationStep11::~AnimationStep11() {
    REB_KEEP_VPTR_STORE();
    deleteBitmapList(this);
}

void AnimationStep11::Begin() { loadBitmapsOrFinish(this); }

void AnimationStep11::Finish() { UiSetHumanUiTarget(m_x, m_y); }

AnimationStep* AnimationStep11::Create() { return NewOrNull<AnimationStep11>(); }

int AnimationStep11::GetKind() { return kKindPositioned11; }

// ---- kind 0x14 ------------------------------------------------------------------------------------------------------------

AnimationStep14::AnimationStep14() : m_firstWord(0), m_spriteCount(0), m_bitmaps(nullptr), m_aborted(0) {}

AnimationStep14::~AnimationStep14() {
    REB_KEEP_VPTR_STORE();
    if (m_bitmaps != nullptr) {
        for (int i = 0; i < m_spriteCount; ++i) {
            DibBitmap* bitmap = m_bitmaps[i];
            if (bitmap != nullptr) deleteBitmap(bitmap);
        }
        ::operator delete(m_bitmaps);
    }
}

void AnimationStep14::Attach(SpriteAnimator* animator) {
    AnimationStep::Attach(animator);
    m_spriteCount = animator->m_spriteCount;
    m_bitmaps = static_cast<DibBitmap**>(::operator new(m_spriteCount * sizeof(DibBitmap*)));
    for (int i = 0; i < m_spriteCount; ++i) m_bitmaps[i] = nullptr;
}

int AnimationStep14::Advance(RECT* area, int* changed) {
    *changed = 0;
    return g_animationStepSignal;
}

void AnimationStep14::Begin() {
    StepResource resource(m_script);
    if (resource.words != nullptr) {
        m_firstWord = resource.words[0];
        const uint16_t* word = resource.words;
        for (int i = 0; i < m_spriteCount; ++i) {
            ++word;
            m_bitmaps[i] = loadBitmap(resource.module, *word);
            showBitmap(this, i, m_bitmaps[i]);
        }
        g_animationStepSignal = 0;
        UiPlaySecondDroidScript(resource.words[2], 0, 0);
        UiPlaySecondDroidScript(kDroidScriptEnd, 0, 0);
        resource.Free();
    }
}

void AnimationStep14::Finish() {}

void AnimationStep14::Abort() { m_aborted = 1; }

AnimationStep* AnimationStep14::Create() { return NewOrNull<AnimationStep14>(); }

int AnimationStep14::GetKind() { return kKindDroid14; }

// ---- kinds 4 and 0x13 -----------------------------------------------------------------------------------------------------

void DeltaFrameStep::AttachImpl(SpriteAnimator* animator) {
    AnimationStep::Attach(animator);
    m_spriteCount = animator->m_spriteCount;
    m_workBitmaps = static_cast<DibBitmap**>(::operator new(m_spriteCount * sizeof(DibBitmap*)));
    m_bitmaps = static_cast<DibBitmap**>(::operator new(m_spriteCount * sizeof(DibBitmap*)));
    m_frameLists = newFrameLists(m_spriteCount);
    m_cursors = static_cast<DeltaFrame**>(::operator new(m_spriteCount * sizeof(DeltaFrame*)));
    for (int i = 0; i < m_spriteCount; ++i) {
        m_workBitmaps[i] = nullptr;
        m_cursors[i] = nullptr;
        m_bitmaps[i] = nullptr;
    }
}


void DeltaFrameStep::Finish() { StopEffectSound(1); }

void DeltaFrameStep::Abort() { m_stopRequested = 1; }

void DeltaFrameStep::LoadFrames() {
    StepResource resource(m_script);
    if (resource.words != nullptr) {
        m_frameCount = resource.words[3];
        const uint16_t* word = resource.words + 4;
        for (int i = 0; i < m_spriteCount; ++i, ++word) {
            m_bitmaps[i] = loadBitmap(resource.module, *word);
            for (int frame = 1; frame < m_frameCount; ++frame) {
                DeltaFrame* delta = NewOrNull<DeltaFrame>(resource.module, static_cast<uint32_t>(static_cast<uint16_t>(frame + *word)));
                delta->SetId(frame);
                m_frameLists[i].AddWithId(delta);
            }
            m_workBitmaps[i] = copyOf(m_bitmaps[i]);
            m_bitmaps[i]->Blit(m_workBitmaps[i], 0, 0, 0, 0, 0, 0);
            showBitmap(this, i, m_workBitmaps[i]);
        }
        if (resource.words[0] != 0) {
            const uint16_t pair[2] = {resource.words[0], static_cast<uint16_t>(m_script >> 16)};
            PlayEffectSound(pair, 0);
        }
        if (resource.words[1] != 0) {
            const uint16_t pair[2] = {resource.words[1], resource.words[2]};
            PlayMainSound(pair, 0);
        }
        resource.Free();
    }
    for (int i = 0; i < m_spriteCount; ++i) m_cursors[i] = static_cast<DeltaFrame*>(m_frameLists[i].First());
}

namespace {

void destroyDeltaFrameStep(DeltaFrameStep* step, bool deleteBitmapsArrayInsideLoop) {
    if (step->m_workBitmaps != nullptr) {
        for (int i = 0; i < step->m_spriteCount; ++i) {
            DibBitmap* work = step->m_workBitmaps[i];
            if (work != nullptr) deleteBitmap(work);
            DibBitmap* original = step->m_bitmaps[i];
            if (original != nullptr) deleteBitmap(original);
        }
        ::operator delete(step->m_workBitmaps);
        if (deleteBitmapsArrayInsideLoop) ::operator delete(step->m_bitmaps);
    }
    if (!deleteBitmapsArrayInsideLoop && step->m_bitmaps != nullptr) ::operator delete(step->m_bitmaps);
    if (step->m_frameLists != nullptr) deleteFrameLists(step->m_frameLists);
    if (step->m_cursors != nullptr) ::operator delete(step->m_cursors);
}

int advanceDeltaFrames(DeltaFrameStep* step, RECT* area, int* changed, bool stopSoundsOnRequest) {
    *changed = 0;
    int result = 0;
    int replace = 1;
    for (int i = 0; i < step->m_spriteCount; ++i) {
        DeltaFrame* frame = step->m_cursors[i];
        if (frame != nullptr) {
            *changed = 1;
            frame->ApplyTo(step->m_workBitmaps[i]);
            frame->GetBounds(area, replace);
            replace = 0;
            step->m_cursors[i] = static_cast<DeltaFrame*>(static_cast<AvlNode*>(frame)->m_successor);
        }
        if (stopSoundsOnRequest) {
            if (step->m_stopRequested != 0) {
                step->StopEffectSound(1);
                step->StopSound(1);
                result = 1;
            }
            if (step->m_cursors[i] == nullptr && !step->IsSoundPlaying()) result = 1;
        } else if (step->m_cursors[i] == nullptr) {
            result = step->IsSoundPlaying() ? step->m_stopRequested : 1;
        }
    }
    return result;
}

}  // namespace

AnimationStep04::AnimationStep04() {
    m_workBitmaps = nullptr;
    m_frameLists = nullptr;
    m_spriteCount = 0;
    m_stopRequested = 0;
    m_unk50 = 0;
    m_cursors = nullptr;
    m_bitmaps = nullptr;
}

AnimationStep04::~AnimationStep04() {
    REB_KEEP_VPTR_STORE();
    destroyDeltaFrameStep(this, false);
}

void AnimationStep04::Attach(SpriteAnimator* animator) { AttachImpl(animator); }

int AnimationStep04::Advance(RECT* area, int* changed) { return advanceDeltaFrames(this, area, changed, true); }

void AnimationStep04::Begin() { LoadFrames(); }

AnimationStep* AnimationStep04::Create() { return NewOrNull<AnimationStep04>(); }

int AnimationStep04::GetKind() { return kKindDelta04; }

AnimationStep13::AnimationStep13() {
    m_workBitmaps = nullptr;
    m_frameLists = nullptr;
    m_spriteCount = 0;
    m_stopRequested = 0;
    m_unk50 = 0;
    m_cursors = nullptr;
    m_bitmaps = nullptr;
}

AnimationStep13::~AnimationStep13() {
    REB_KEEP_VPTR_STORE();
    destroyDeltaFrameStep(this, true);
}

void AnimationStep13::Attach(SpriteAnimator* animator) { AttachImpl(animator); }

int AnimationStep13::Advance(RECT* area, int* changed) { return advanceDeltaFrames(this, area, changed, false); }

void AnimationStep13::Begin() { LoadFrames(); }

AnimationStep* AnimationStep13::Create() { return NewOrNull<AnimationStep13>(); }

int AnimationStep13::GetKind() { return kKindDelta13; }

// ---- static initializers (the instance of every kind, and its registration) ----------------------------------------------

void InitAnimationStep04() {
    new (&g_animationStep04) AnimationStep04();
    atexit(DestroyAnimationStep04);
}

void RegisterAnimationStep04() { g_animationStep04Registered = RegisterAnimationStep(&g_animationStep04); }

void InitAnimationStep05() {
    new (&g_animationStep05) AnimationStep05();
    atexit(DestroyAnimationStep05);
}

void RegisterAnimationStep05() { g_animationStep05Registered = RegisterAnimationStep(&g_animationStep05); }

void InitAnimationStep14() {
    new (&g_animationStep14) AnimationStep14();
    atexit(DestroyAnimationStep14);
}

void RegisterAnimationStep14() { g_animationStep14Registered = RegisterAnimationStep(&g_animationStep14); }

void InitAnimationStep15() {
    new (&g_animationStep15) AnimationStep15();
    atexit(DestroyAnimationStep15);
}

void RegisterAnimationStep15() { g_animationStep15Registered = RegisterAnimationStep(&g_animationStep15); }

void InitAnimationStep11() {
    new (&g_animationStep11) AnimationStep11();
    atexit(DestroyAnimationStep11);
}

void RegisterAnimationStep11() { g_animationStep11Registered = RegisterAnimationStep(&g_animationStep11); }

void InitAnimationStep12() {
    new (&g_animationStep12) AnimationStep12();
    atexit(DestroyAnimationStep12);
}

void RegisterAnimationStep12() { g_animationStep12Registered = RegisterAnimationStep(&g_animationStep12); }

void InitAnimationStep13() {
    new (&g_animationStep13) AnimationStep13();
    atexit(DestroyAnimationStep13);
}

void RegisterAnimationStep13() { g_animationStep13Registered = RegisterAnimationStep(&g_animationStep13); }

// ---- destructors registered with atexit ----------------------------------------------------

void DestroyAnimationStep04() { g_animationStep04.~AnimationStep04(); }

void DestroyAnimationStep05() { g_animationStep05.~AnimationStep05(); }

void DestroyAnimationStep14() { g_animationStep14.~AnimationStep14(); }

void DestroyAnimationStep15() { g_animationStep15.~AnimationStep15(); }

void DestroyAnimationStep11() { g_animationStep11.~AnimationStep11(); }

void DestroyAnimationStep12() { g_animationStep12.~AnimationStep12(); }

void DestroyAnimationStep13() { g_animationStep13.~AnimationStep13(); }
