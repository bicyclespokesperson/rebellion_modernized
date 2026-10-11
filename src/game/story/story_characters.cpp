#include "story/story_characters.h"

#include "core/checked_new.h"
#include "events/event_post.h"
#include "gameobj/object_family.h"
#include "mission/mission_rules.h"
#include "story/story_common.h"

extern int g_lukeEventKindsReady; // 0x6bb9e8
extern EventKindSet g_lukeEventKinds; // 0x6bb9f0
extern int g_leiaEventKindsReady; // 0x6bba08
extern EventKindSet g_leiaEventKinds; // 0x6bba00
extern int g_hanEventKindsReady; // 0x6bba58
extern EventKindSet g_hanEventKinds; // 0x6bba50

namespace
{

constexpr uint32_t kFamilyLuke = 0x32;
constexpr uint32_t kFamilyLeia = 0x31;
constexpr uint32_t kFamilyHan = 0x33;
constexpr uint32_t kRecordTypeBitsAfterDagobah = 0x850;
constexpr uint32_t kForceAwareFlag = 0x10;
constexpr int kEventLukeKnowsHeritage = 0x211;
constexpr int kEventLukeDagobahCompleted = 0x210;
constexpr int kEventHanBountyAttack = 0x212;
constexpr int kEventLukeFinalBattleReady = 0x313;

} // namespace

int StoryCharacterCreateViews(Character* self, int controlKind, NotifyContext* ctx)
{
  return CharacterLeafCreateViews(self, controlKind, ctx);
}

// ---- StoryCharacter -----------------------------------------------------------------------------------------------

StoryCharacter::StoryCharacter() : m_storyFlags(0)
{
}

StoryCharacter::~StoryCharacter() = default;

int StoryCharacter::ReadBody(istream* in)
{
  const int result = CharacterLeafRead(this, in);
  if (result)
    ReadValue32(in, &m_storyFlags);
  return result;
}

int StoryCharacter::WriteBody(ostream* out)
{
  const int result = CharacterLeafWrite(this, out);
  if (result)
    WriteValue32(out, &m_storyFlags);
  return result;
}

int StoryCharacter::ChecksumBody(Crc32* crc)
{
  const int result = CharacterLeafChecksum(this, crc);
  if (result)
    crc->AddInt(static_cast<int>(m_storyFlags));
  return result;
}

// ---- Luke -------------------------------------------------------------------------------------------------------

Luke::Luke() = default;

Luke::~Luke() = default;

uint32_t Luke::GetFamily()
{
  return kFamilyLuke;
}

int Luke::Read(istream* in)
{
  return ReadBody(in);
}

int Luke::Write(ostream* out)
{
  return WriteBody(out);
}

int Luke::Checksum(Crc32* crc)
{
  return ChecksumBody(crc);
}

int Luke::Validate()
{
  return ValidateWithForce();
}

int Luke::ValidateWithForce()
{
  const int valid = ValidateBody();
  return valid && m_force != 0;
}

int Luke::ApplyAll(NotifyContext* ctx)
{
  const int isSideView = IsSideView();
  if (isSideView)
    return ApplyAllBody(ctx);
  return isSideView;
}

int Luke::ApplyAllBody(NotifyContext* ctx)
{
  const int result = CharacterLeafApplyAll(this, ctx);
  if (result && ctx->m_flag04 == 1)
  {
    ApplyKnowsHeritage(ctx);
    ApplyFinalBattleReady(ctx);
    ApplyDagobahCompleted(ctx);
  }
  return result;
}

uint32_t Luke::GetRecordTypeBits()
{
  if (m_storyFlags & kDagobahCompleted)
    return kRecordTypeBitsAfterDagobah;
  return GameObj::GetRecordTypeBits() & 0xffff;
}

int Luke::GetEventKinds(int unused, EventKindSet* out)
{
  return story::copyKinds(BuildEventKinds(), g_lukeEventKinds, out);
}

int Luke::CheckOrder(int a, int orderKind, StatusCode* out)
{
  return story::checkOrder(BuildEventKinds(), g_lukeEventKinds, orderKind, out);
}

int Luke::CreateViews(int controlKind, NotifyContext* ctx)
{
  return StoryCharacterCreateViews(this, controlKind, ctx);
}

GameObj* Luke::CreateViewObject()
{
  return NewOrNull<Luke>();
}

GameObjNotifier* Luke::Create()
{
  return NewOrNull<Luke>();
}

int Luke::Virtual130(NotifyContext* ctx)
{
  const int result = MajorCharacter::Virtual130(ctx);
  if (!result)
    return result;
  return SetForceExperience(m_forceExperience + g_gnprtb0C09, ctx);
}

int Luke::OnForceChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  const int result = Character::OnForceChanged(oldValue, newValue, ctx);
  if (!result)
    return result;
  return UpdateFinalBattleReady(ctx);
}

int Luke::UpdateFinalBattleReady(NotifyContext* ctx)
{
  const int ready = (m_storyFlags & kKnowsHeritage) && m_force >= g_gnprtb0C06;
  return SetFinalBattleReady(ready ? 1 : 0, ctx);
}

int Luke::OnKnowsHeritageChanged(int on, NotifyContext* ctx)
{
  return UpdateFinalBattleReady(ctx);
}

int Luke::OnFinalBattleReadyChanged(int on, NotifyContext* ctx)
{
  return PostSourceEventFlag(kEventLukeFinalBattleReady, this, on, ctx);
}

int Luke::SetKnowsHeritage(int on, NotifyContext* ctx)
{
  return story::setFlagProperty(
    this, &StoryCharacter::m_storyFlags, kKnowsHeritage, on,
    [&](Luke* view)
    {
      view->ApplyKnowsHeritage(ctx);
    },
    [&]
    {
      OnKnowsHeritageChanged(on, ctx);
    });
}

int Luke::SetFinalBattleReady(int on, NotifyContext* ctx)
{
  return story::setFlagProperty(
    this, &StoryCharacter::m_storyFlags, kFinalBattleReady, on,
    [&](Luke* view)
    {
      view->ApplyFinalBattleReady(ctx);
    },
    [&]
    {
      OnFinalBattleReadyChanged(on, ctx);
    });
}

int Luke::SetDagobahCompleted(int on, NotifyContext* ctx)
{
  return story::setFlagProperty(
    this, &StoryCharacter::m_storyFlags, kDagobahCompleted, on,
    [&](Luke* view)
    {
      view->ApplyDagobahCompleted(ctx);
    },
    [&]
    {
      OnDagobahCompletedChanged(on, ctx);
    });
}

int Luke::ApplyKnowsHeritage(NotifyContext* ctx)
{
  return story::applyFlagProperty(this, &StoryCharacter::m_storyFlags, kKnowsHeritage, ctx,
                                  [&](int on)
                                  {
                                    return OnKnowsHeritageNotified(on, ctx);
                                  });
}

int Luke::ApplyFinalBattleReady(NotifyContext* ctx)
{
  return story::applyFlagProperty(this, &StoryCharacter::m_storyFlags, kFinalBattleReady, ctx,
                                  [&](int on)
                                  {
                                    return OnFinalBattleReadyNotified(on, ctx);
                                  });
}

int Luke::ApplyDagobahCompleted(NotifyContext* ctx)
{
  return story::applyFlagProperty(this, &StoryCharacter::m_storyFlags, kDagobahCompleted, ctx,
                                  [&](int on)
                                  {
                                    return OnDagobahCompletedNotified(on, ctx);
                                  });
}

int Luke::OnKnowsHeritageNotified(int on, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    LogFlag("(LukeKnowsHeritageNotif)", "KnowsHeritage", on, 0, ctx);
    result = PostPayloadFlag(kEventLukeKnowsHeritage, this, on, ctx);
  }
  return result;
}

int Luke::OnFinalBattleReadyNotified(int on, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
    LogFlag("(LukeFinalBattleReadyNotif)", "FinalBattleReady", on, 0, ctx);
  return result;
}

int Luke::OnDagobahCompletedNotified(int on, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    LogFlag("LukeDagobahCompletedNotif", "DagobahCompleted", on, 0, ctx);
    result = PostPayloadFlag(kEventLukeDagobahCompleted, this, on, ctx);
  }
  return result;
}

int Luke::BuildEventKinds()
{
  return story::buildKindsOnce(&g_lukeEventKindsReady,
                               []
                               {
                                 return MajorCharacter::InitEventKinds(0, &g_lukeEventKinds);
                               });
}

// ---- Leia -------------------------------------------------------------------------------------------------------

Leia::Leia() = default;

Leia::~Leia() = default;

uint32_t Leia::GetFamily()
{
  return kFamilyLeia;
}

int Leia::Read(istream* in)
{
  return ReadBody(in);
}

int Leia::Write(ostream* out)
{
  return WriteBody(out);
}

int Leia::Checksum(Crc32* crc)
{
  return ChecksumBody(crc);
}

int Leia::Validate()
{
  return ValidateBody();
}

int Leia::ApplyAll(NotifyContext* ctx)
{
  const int isSideView = IsSideView();
  if (isSideView)
    return ApplyAllBody(ctx);
  return isSideView;
}

int Leia::ApplyAllBody(NotifyContext* ctx)
{
  const int result = CharacterLeafApplyAll(this, ctx);
  if (result && ctx->m_flag04 == 1)
    ApplyKnowsHeritage(ctx);
  return result;
}

int Leia::GetEventKinds(int unused, EventKindSet* out)
{
  return story::copyKinds(BuildEventKinds(), g_leiaEventKinds, out);
}

int Leia::CheckOrder(int a, int orderKind, StatusCode* out)
{
  return story::checkOrder(BuildEventKinds(), g_leiaEventKinds, orderKind, out);
}

int Leia::CreateViews(int controlKind, NotifyContext* ctx)
{
  return StoryCharacterCreateViews(this, controlKind, ctx);
}

GameObj* Leia::CreateViewObject()
{
  return NewOrNull<Leia>();
}

GameObjNotifier* Leia::Create()
{
  return NewOrNull<Leia>();
}

int Leia::Virtual130(NotifyContext* ctx)
{
  const int result = MajorCharacter::Virtual130(ctx);
  if (result && (m_flagsAc & kForceAwareFlag))
    return SetForceExperience(m_forceExperience + g_gnprtb0C0A, ctx);
  return result;
}

int Leia::SetKnowsHeritage(int on, NotifyContext* ctx)
{
  return story::setFlagProperty(
    this, &StoryCharacter::m_storyFlags, kKnowsHeritage, on,
    [&](Leia* view)
    {
      view->ApplyKnowsHeritage(ctx);
    },
    [&]
    {
      OnKnowsHeritageChanged(on, ctx);
    });
}

int Leia::ApplyKnowsHeritage(NotifyContext* ctx)
{
  return story::applyFlagProperty(this, &StoryCharacter::m_storyFlags, kKnowsHeritage, ctx,
                                  [&](int on)
                                  {
                                    return OnKnowsHeritageNotified(on, ctx);
                                  });
}

int Leia::OnKnowsHeritageNotified(int on, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
    LogFlag("(LeiaKnowsHeritageNotif)", "KnowsHeritage", on, 0, ctx);
  return result;
}

int Leia::BuildEventKinds()
{
  return story::buildKindsOnce(&g_leiaEventKindsReady,
                               []
                               {
                                 return MajorCharacter::InitEventKinds(0, &g_leiaEventKinds);
                               });
}

// ---- Han --------------------------------------------------------------------------------------------------------

Han::Han() = default;

Han::~Han() = default;

uint32_t Han::GetFamily()
{
  return kFamilyHan;
}

int Han::Read(istream* in)
{
  return ReadBody(in);
}

int Han::Write(ostream* out)
{
  return WriteBody(out);
}

int Han::Checksum(Crc32* crc)
{
  return ChecksumBody(crc);
}

int Han::Validate()
{
  return ValidateBody();
}

int Han::ApplyAll(NotifyContext* ctx)
{
  const int isSideView = IsSideView();
  if (isSideView)
    return ApplyAllBody(ctx);
  return isSideView;
}

int Han::ApplyAllBody(NotifyContext* ctx)
{
  const int result = CharacterLeafApplyAll(this, ctx);
  if (result && ctx->m_flag04 == 1)
  {
    ApplyBountyAttack(ctx);
    ApplyCapturedByBountyHunters(ctx);
  }
  return result;
}

int Han::GetEventKinds(int unused, EventKindSet* out)
{
  return story::copyKinds(BuildEventKinds(), g_hanEventKinds, out);
}

int Han::CheckOrder(int a, int orderKind, StatusCode* out)
{
  return story::checkOrder(BuildEventKinds(), g_hanEventKinds, orderKind, out);
}

int Han::CreateViews(int controlKind, NotifyContext* ctx)
{
  return StoryCharacterCreateViews(this, controlKind, ctx);
}

GameObj* Han::CreateViewObject()
{
  return NewOrNull<Han>();
}

GameObjNotifier* Han::Create()
{
  return NewOrNull<Han>();
}

int Han::OnCapturedChanged(int on, NotifyContext* ctx)
{
  int result = Character::OnCapturedChanged(on, ctx);
  if (result && on == 0)
    result = SetCapturedByBountyHunters(on, ctx);
  return result;
}

int Han::OnBountyAttackChanged(int on, NotifyContext* ctx)
{
  NotifyContext everySide(*ctx);
  everySide.m_flag04 = 1;
  everySide.m_flag08 = 3;
  static_cast<Han*>(ViewAs(story::kViewAlliance))->ApplyBountyAttack(&everySide);
  static_cast<Han*>(ViewAs(story::kViewEmpire))->ApplyBountyAttack(&everySide);
  return 1;
}

int Han::SetBountyAttack(int on, NotifyContext* ctx)
{
  return story::setFlagProperty(
    this, &StoryCharacter::m_storyFlags, kBountyAttack, on,
    [&](Han* view)
    {
      view->ApplyBountyAttack(ctx);
    },
    [&]
    {
      OnBountyAttackChanged(on, ctx);
    });
}

int Han::SetCapturedByBountyHunters(int on, NotifyContext* ctx)
{
  return story::setFlagProperty(
    this, &StoryCharacter::m_storyFlags, kCapturedByBountyHunters, on,
    [&](Han* view)
    {
      view->ApplyCapturedByBountyHunters(ctx);
    },
    [&]
    {
      OnCapturedByBountyHuntersChanged(on, ctx);
    });
}

int Han::ApplyBountyAttack(NotifyContext* ctx)
{
  return story::applyFlagProperty(this, &StoryCharacter::m_storyFlags, kBountyAttack, ctx,
                                  [&](int on)
                                  {
                                    return OnBountyAttackNotified(on, ctx);
                                  });
}

int Han::ApplyCapturedByBountyHunters(NotifyContext* ctx)
{
  return story::applyFlagProperty(this, &StoryCharacter::m_storyFlags, kCapturedByBountyHunters, ctx,
                                  [&](int on)
                                  {
                                    return OnCapturedByBountyHuntersNotified(on, ctx);
                                  });
}

int Han::OnBountyAttackNotified(int on, NotifyContext* ctx)
{
  int result = IsSideView();
  if (result)
  {
    LogFlag("HanBountyAttackNotif", "BountyAttack", on, 0, ctx);
    result = PostPayloadFlag(kEventHanBountyAttack, this, on, ctx);
  }
  return result;
}

int Han::OnCapturedByBountyHuntersNotified(int on, NotifyContext* ctx)
{
  const int result = IsSideView();
  if (result)
    LogFlag("(HanCapturedByBountyHuntersNotif)", "CapturedByBountyHunters", on, 0, ctx);
  return result;
}

int Han::BuildEventKinds()
{
  return story::buildKindsOnce(&g_hanEventKindsReady,
                               []
                               {
                                 return MajorCharacter::InitEventKinds(0, &g_hanEventKinds);
                               });
}

int GetHanHyperdriveModifier()
{
  return g_gnprtb0C0B;
}

// ---- MinorCharacterIterator ---------------------------------------------------------------------------------------

namespace
{
constexpr FamilyRange kMinorCharacters(0x38, 0x3c);
}

MinorCharacterIterator::MinorCharacterIterator(GameObjNotifier* parent, uint32_t stateFilter)
: ChildIterator(parent, &kMinorCharacters, stateFilter)
{
}

MinorCharacterIterator::MinorCharacterIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
: ChildIterator(parent, side, &kMinorCharacters, stateFilter)
{
}

MinorCharacterIterator::~MinorCharacterIterator() = default;
EventKindSet g_hanEventKinds{};
EventKindSet g_leiaEventKinds{};
EventKindSet g_lukeEventKinds{};
int g_hanEventKindsReady{};
int g_leiaEventKindsReady{};
int g_lukeEventKindsReady{};
