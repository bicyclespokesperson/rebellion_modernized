#include "ui/sprite_animator.h"

#include <cstdint>

#include "core/vptr_store.h"
#include "gfx/dib_bitmap.h"
#include "ui/sprite.h"
#include "ui/sprite_stage.h"

AnimationStepQueue::AnimationStepQueue() {}

AnimationStepQueue::~AnimationStepQueue() { REB_KEEP_VPTR_STORE(); }

namespace {

NodeQueue* QueueOf(SpriteAnimator* animator) { return &animator->m_steps; }

}  // namespace

SpriteAnimator::SpriteAnimator(int spriteCount) {
    *reinterpret_cast<int16_t*>(&m_endSequence) = 0;
    *(reinterpret_cast<int16_t*>(&m_endSequence) + 1) = 0;
    if (spriteCount < 1) spriteCount = 1;
    m_sprites = static_cast<Sprite**>(::operator new(spriteCount * sizeof(Sprite*)));
    for (int i = 0; i < spriteCount; ++i) m_sprites[i] = nullptr;
    m_spriteCount = spriteCount;
    m_current = nullptr;
    m_originX = 0;
    m_originY = 0;
    m_hasEndAction = 0;
}

SpriteAnimator::~SpriteAnimator() {
    if (m_sprites) {
        operator delete(m_sprites);
        m_sprites = nullptr;
    }
    if (m_current) {
        delete m_current;
        m_current = nullptr;
    }
    REB_KEEP_VPTR_STORE();
}

void SpriteAnimator::SetOrigin(int x, int y) {
    m_originX = x;
    m_originY = y;
}

int SpriteAnimator::Start(int kind, const uint32_t* sequence, SpriteStage* stage, int b, intptr_t c, int noStep) {
    AnimationStep* step = CreateAnimationStep(kind);
    if (step) {
        step->Attach(this);
        step->m_stage = stage;
        step->SetScript(sequence);
        step->SetStart(b, c);
    }
    QueueOf(this)->Enqueue(step);
    if (m_current) {
        m_current->Abort();
        return 1;
    }
    if (noStep == 0) {
        RECT area;
        int changed;
        Step(&area, &changed);
    }
    return 1;
}

void SpriteAnimator::Step(RECT* area, int* changed) {
    if (m_current && m_current->Advance(area, changed)) {
        m_current->Finish();
        if (m_current) delete m_current;
        m_current = nullptr;
    }
    if (m_current) return;
    m_current = static_cast<AnimationStep*>(QueueOf(this)->Dequeue());
    if (!m_current && m_hasEndAction) {
        Start(m_endKind, &m_endSequence, m_endStage, m_endFrames, static_cast<intptr_t>(m_endCallback), 1);
        m_current = static_cast<AnimationStep*>(QueueOf(this)->Dequeue());
    }
    if (!m_current) return;
    m_current->Begin();
    if (m_current->m_stage) m_current->m_stage->Resync();
    if (m_sprites && m_sprites[0]) {
        *changed = 1;
        Sprite* sprite = m_sprites[0];
        area->left = sprite->m_x;
        DibBitmap* bitmap = sprite->m_bitmap;
        area->right = area->left + (bitmap ? bitmap->Width() : 0);
        area->top = sprite->m_y;
        if (bitmap) {
            area->top = bitmap->Height();
            area->bottom = area->top;
        } else {
            area->top = 0;
            area->bottom = 0;
        }
    }
}

void SpriteAnimator::SetSprite(int index, Sprite* sprite) {
    // BUG(original): the lower bound is `index < 1`, so only slot 0 (and negative indices) can be set.
    if (index < 1 && index < m_spriteCount) m_sprites[index] = sprite;
}

void SpriteAnimator::SetEndAction(int kind, const uint32_t* sequence, SpriteStage* stage, int frames,
                                  void (*callback)(int)) {
    m_endKind = kind;
    m_endSequence = *sequence;
    m_endStage = stage;
    m_endFrames = frames;
    m_endCallback = reinterpret_cast<uintptr_t>(callback);
    m_hasEndAction = 1;
}

int SpriteAnimator::HitTest(int x, int y) {
    int hit = 0;
    if (m_sprites) {
        for (int i = 0; i < m_spriteCount; ++i) {
            if (hit) return hit;
            if (m_sprites[i]) hit = m_sprites[i]->HitTest(x, y);
        }
    }
    return hit;
}

void SpriteAnimator::Stop() {
    QueueOf(this)->Clear();
    if (m_current) m_current->Abort();
}
