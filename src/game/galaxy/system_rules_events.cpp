#include "character/character.h"
#include "data/game_math.h"
#include "data/random.h"
#include "galaxy/system_rules.h"
#include "galaxy/system_rules_1e.h"
#include "galaxy/system_rules_decl.h"

using namespace system_rules;

// Uprising, loyalty, incident and event rules of the System, plus the stores into its SystemData block.

int System::EndUprisingIfQuelled(NotifyContext* ctx)
{
  int result = 1;
  if (m_flags & system_flag::kUprising)
  {
    const uint32_t side = ControlSide();
    const int player = PlayerSide(side);
    if (player != 0)
    {
      ChildIterator troops(this, &family::kTroopRegiments, kStateUsable);
      const int troopCount = troops.Count();
      const uint32_t flags = GameObjRoot::m_flags;
      const int deathStar = (m_flags >> 11) & 1;
      const int share = this->System::GetLoyaltyFor(player);
      const int strength = ComputeRemainingSupport((flags >> 6) & 3, share, deathStar, 1, 0, troopCount);
      if (strength >= 0)
        result = SetUprising(0, ctx);
    }
  }
  return result;
}

int System::AdjustLoyalty(int delta, int side, NotifyContext* ctx)
{
  int result = 1;
  if (side >= 1 && side <= 3 && (m_flags & system_flag::kPopulated))
  {
    const int player = PlayerSide(side);
    result = 0;
    if (player != 0)
    {
      const int shift = DampenFavourableShift(delta, player, (m_flags >> 11) & 1);
      const int target = ClampPercent(this->System::GetLoyaltyFor(player) + shift);
      result = this->System::SetLoyaltyFor(target, player, ctx) != 0;
    }
  }
  return result;
}

int System::EndUprising(NotifyContext* ctx)
{
  if ((m_flags >> 2) & 1)
    return SetUprising(0, ctx);
  return 0;
}

int System::AdjustLoyaltyForUprising(NotifyContext* ctx)
{
  if ((m_flags >> 2) & 1)
    return AdjustLoyalty(g_gnprtb1E11, ControlSide(), ctx);
  return 0;
}

void System::AdjustLoyaltyForAssault(NotifyContext* ctx)
{
  if (((m_flags >> 8) & 1) && ControlSide() == 2 && !(m_flags & system_flag::kUprising))
    AdjustLoyalty(g_gnprtb1E0B, ControlSide(), ctx);
}

void System::AdjustLoyaltyForBlockade(NotifyContext* ctx)
{
  uint32_t flag = 0;
  int blockaded = 0;
  if ((m_flags >> 5) & 1)
    blockaded = this->System::IsBlockaderMoreLoyal(&flag);
  if (blockaded)
  {
    uint32_t blockaderSide;
    if (this->System::GetBlockadingSide(&blockaderSide))
      AdjustLoyalty(flag ? g_gnprtb1E0D : g_gnprtb1E0F, blockaderSide, ctx);
  }
}

int System::PulseUprisingIncident(NotifyContext* ctx)
{
  const uint32_t uprising = (m_flags >> 2) & 1;
  const int raised = SetUprisingIncident(1, ctx) && uprising;
  const int cleared = SetUprisingIncident(0, ctx);
  return cleared && raised;
}

int System::RollInformantIncident(NotifyContext* ctx)
{
  int result = 0;
  if (ControlSide() != 3)
  {
    int occurs = 0;
    result = RollFailure(this->System::GetLoyaltyFor(ControlSide()), &occurs);
    if (result && occurs)
    {
      const int raised = SetInformantIncident(1, ctx);
      const int cleared = SetInformantIncident(0, ctx);
      if (cleared && raised)
        return 1;
      result = 0;
    }
  }
  return result;
}

int System::RollResourceIncident(NotifyContext* ctx)
{
  int energy = m_energy;
  int rawMaterial = m_rawMaterial;
  ChildIterator sites(this, ControlSide(), &kFacilities2cTo2d, kStateUsable);
  const int siteCount = sites.HasCurrent();
  ChildIterator facilities(this, ControlSide(), &family::kFacilities, kStateUsable);
  const int facilityCount = facilities.HasCurrent();
  int result = ApplyRandomCounterShift(&rawMaterial, &energy, siteCount, facilityCount);
  if (result && (m_rawMaterial != rawMaterial || m_energy != energy))
  {
    SetResourceIncident(1, ctx);
    const int rawResult = SetRawMaterial(rawMaterial, ctx);
    const int energyResult = SetEnergy(energy, ctx);
    const int resourcesOk = energyResult && rawResult;
    const int cleared = SetResourceIncident(0, ctx);
    if (cleared && resourcesOk)
      return 1;
    result = 0;
  }
  return result;
}

int System::PulseDisasterIncident(NotifyContext* ctx)
{
  if (m_energy != 0 || m_rawMaterial != 0)
  {
    const int raised = SetDisasterIncident(1, ctx);
    const int cleared = SetDisasterIncident(0, ctx);
    if (cleared && raised)
      return 1;
    return 0;
  }
  return 1;
}

namespace
{

SystemData* dataOf(System* system)
{
  return static_cast<SystemData*>(system->m_data);
}

bool isSideNumber(int side)
{
  return side >= 1 && side <= 3;
}

} // namespace

int System::SetData48(int value, int side)
{
  const bool valid = isSideNumber(side);
  if (valid)
    dataOf(this)->m_word48[side - 1] = value;
  return valid;
}

int System::SetData54(int value, int side)
{
  const bool valid = isSideNumber(side);
  if (valid)
    dataOf(this)->m_word54[side - 1] = value;
  return valid;
}

int System::SetData60(int side)
{
  const bool store = isSideNumber(side) && side != 3;
  if (store)
    dataOf(this)->m_word60 = side;
  return store;
}

int System::SetData64(int side)
{
  if (!isSideNumber(side) && side != 0)
    return 0;
  dataOf(this)->m_word64 = side;
  return 1;
}

int System::SetData68(int value, int side)
{
  const bool valid = isSideNumber(side);
  if (valid)
    dataOf(this)->m_word68[side - 1] = value;
  return valid;
}

int System::SetData74(int value)
{
  dataOf(this)->m_word74 = value;
  return 1;
}

int System::SetData78(int value)
{
  dataOf(this)->m_word78 = value;
  return 1;
}

int System::SetData7c(int value)
{
  dataOf(this)->m_word7c = value;
  return 1;
}

int System::SetDataFlag1(int on)
{
  UpdateFlagBit(1, on, &dataOf(this)->m_flags80);
  return 1;
}

int System::SetDataFlag2(int on)
{
  UpdateFlagBit(2, on, &dataOf(this)->m_flags80);
  return 1;
}

int System::SetDataFlag4(int on)
{
  UpdateFlagBit(4, on, &dataOf(this)->m_flags80);
  return 1;
}

int System::SetDataFlag8(int on)
{
  UpdateFlagBit(8, on, &dataOf(this)->m_flags80);
  return 1;
}

int System::ResolveUprisingTurn(NotifyContext* ctx)
{
  const uint32_t side = ControlSide();
  int player;
  if (side == 1)
    player = 1;
  else if (side == 2)
    player = 2;
  else
    return 1;
  SetInformantIncident(1, ctx);
  SetInformantIncident(0, ctx);
  int firstEvent = 0;
  int secondEvent = 0;
  const int deathStar = (m_flags >> 11) & 1;
  const int v7 = this->System::GetDataWord78();
  const int v6 = this->System::GetDataWord74();
  const int v5 = this->System::CountEmpireRegimentsOfKey6();
  const int troops = this->System::CountTroopRegiments(side, 1);
  const int share = this->System::GetLoyaltyFor(player);
  RollUprisingIncident(side, share, deathStar, troops, v5, v6, v7, &firstEvent, &secondEvent);
  ApplyUprisingEvent(firstEvent, side, ctx);
  ApplyUprisingEvent(secondEvent, side, ctx);
  return AdjustLoyalty(this->System::GetDataWord7c(), side, ctx);
}

int System::ApplyUprisingEvent(int event, int side, NotifyContext* ctx)
{
  int result = 1;
  switch (event)
  {
    case 0:
      break;
    case 1:
    {
      ChildIterator facilities(this, side, &family::kFacilities, kStateUsable);
      result = SelectRandomChild(&facilities);
      if (result && facilities.Current())
        result = static_cast<GameObj*>(facilities.Current())->ChangeDestroyedReason(8, ctx);
      break;
    }
    case 2:
    {
      ChildIterator troops(this, side, &family::kTroopRegiments, kStateUsable);
      result = SelectRandomChild(&troops);
      if (result && troops.Current())
        result = static_cast<GameObj*>(troops.Current())->ChangeDestroyedReason(8, ctx);
      break;
    }
    case 3:
    case 4:
    {
      const uint32_t wantedCaptured = event == 4;
      ChildIterator characters(this, side, &family::kCharacters, kStateUsable);
      int matching = 0;
      characters.Begin();
      while (characters.Current())
      {
        matching += (static_cast<Character*>(characters.Current())->m_flagsAc & 1) == wantedCaptured;
        characters.Advance();
      }
      if (matching != 0)
      {
        const int skip = GameRandom(matching - 1);
        characters.Begin();
        uint32_t flags = 0;
        int passed = 0;
        for (; characters.Current() && passed < skip; passed += (flags & 1) == wantedCaptured)
        {
          flags = static_cast<Character*>(characters.Current())->m_flagsAc;
          characters.Advance();
        }
        result = characters.Current() != nullptr;
      }
      if (result && characters.Current())
      {
        Character* chosen = static_cast<Character*>(characters.Current());
        result = event == 3 ? chosen->Virtual185(ctx) : chosen->Virtual133(ctx);
      }
      break;
    }
    case 5:
    {
      ChildIterator characters(this, side, &family::kCharacters, kStateUsable);
      for (characters.Begin(); characters.Current(); characters.Advance())
      {
        Character* character = static_cast<Character*>(characters.Current());
        if (character->m_flagsAc & 1)
          result = character->Virtual133(ctx);
      }
      break;
    }
    default:
      result = 0;
  }
  return result;
}

int System::RollAndSyncEvent(NotifyContext* ctx)
{
  if (IsMasterView() && ControlSide() != 3)
  {
    int event = 0;
    const int rolled = ::RollInformantIncident(&event);
    const uint32_t side = ControlSide();
    const int view = side == 1 ? 2 : 1;
    const int synced = SyncViewForEvent(event, view, ctx);
    if (synced && rolled)
      return 1;
  }
  return 0;
}
