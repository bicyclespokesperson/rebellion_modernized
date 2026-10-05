#include "ai/ai_records.h"

#include "ai/ai_common.h"
#include "ai/ai_members.h"
#include "ai/ai_session_random.h"
#include "ai/ai_side_controller.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "galaxy/sector.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"

using ai::InFamilies;

namespace {

ObjectKey KeyOfObject(GameObjNotifier* object) {
    ObjectKey key;
    GetObjectKey(object, &key);
    return key;
}

struct SystemMasks {
    uint32_t requireA, requireB, requireC, forbidA, forbidB, forbidC;

    bool Matches(const AiSystemEval* system) const {
        const uint32_t a = system->m_flags28.m_value;
        const uint32_t b = system->m_flags2c.m_value;
        const uint32_t c = system->m_flags30.m_value;
        return (a & requireA) == requireA && (forbidA & a) == 0 && (b & requireB) == requireB && (forbidB & b) == 0 &&
               (c & requireC) == requireC && (forbidC & c) == 0;
    }
};

}  // namespace

AiSectorEval::AiSectorEval()
    : ObjectKeyNode(), m_flags24(0), m_flags28(0), m_flags2c(0) {
    m_view = 0;
    m_owner = nullptr;
    for (AiSystemEval*& system : m_systems) system = nullptr;
    m_dirty = 0;
    m_x38 = 0;
    m_x3c = 0;
    m_x30 = 0;
    m_x34 = 0;
    m_x = 0;
    m_y = 0;
}

AiSectorEval::AiSectorEval(int view) : ObjectKeyNode(), m_flags24(0), m_flags28(0), m_flags2c(0) {
    m_view = view;
    m_owner = nullptr;
    for (AiSystemEval*& system : m_systems) system = nullptr;
    m_x38 = 0;
    m_x3c = 0;
    m_x30 = 0;
    m_x34 = 0;
    m_dirty = 0;
}

AiSectorEval::~AiSectorEval() {
    m_owner = nullptr;
    for (AiSystemEval*& system : m_systems) system = nullptr;
    REB_KEEP_VPTR_STORE();
}

void AiSectorEval::Attach(Sector* sector, AiSystemEvalSet* systems) {
    m_key = KeyOfObject(sector);
    uint32_t position;
    sector->GetPosition(&position);
    m_x = static_cast<int16_t>(position);
    m_y = static_cast<int16_t>(position >> 16);
    m_x30 = 0;
    m_x34 = 0;
    SystemIterator iterator(sector, 4);
    iterator.Begin();
    int index = 0;
    while (iterator.m_current) {
        ObjectKey systemKey = KeyOfObject(iterator.m_current);
        SetSystem(static_cast<uint16_t>(index), &systemKey, systems);
        iterator.Advance();
        ++index;
    }
    m_dirty = 1;
}

void AiSectorEval::ReadRecord(istream* in, AiSystemEvalSet* systems) {
    ObjectKey key;
    ObjectKeyNode::Load(in);
    ReadValue32(in, &m_view);
    ReadValue32(in, &m_dirty);
    ReadValue32(in, &m_x38);
    ReadValue32(in, &m_x3c);
    m_flags24.Read(in);
    m_flags28.Read(in);
    m_flags2c.Read(in);
    ReadValue32(in, &m_x30);
    ReadValue32(in, &m_x34);
    ReadValue16(in, &m_x);
    ReadValue16(in, &m_y);
    for (uint16_t index = 0; index < kSystemCount; ++index) {
        key.Read(in);
        SetSystem(index, &key, systems);
    }
    for (int& value : m_stat) ReadValue32(in, &value);
    m_dirty = 1;
}

void AiSectorEval::Save(ostream* out) {
    ObjectKeyNode::Save(out);
    WriteValue32(out, &m_view);
    WriteValue32(out, &m_dirty);
    WriteValue32(out, &m_x38);
    WriteValue32(out, &m_x3c);
    m_flags24.Write(out);
    m_flags28.Write(out);
    m_flags2c.Write(out);
    WriteValue32(out, &m_x30);
    WriteValue32(out, &m_x34);
    WriteValue16(out, &m_x);
    WriteValue16(out, &m_y);
    for (AiSystemEval* system : m_systems) {
        ObjectKey key;
        system->GetKey(&key);
        key.Write(out);
    }
    for (int& value : m_stat) WriteValue32(out, &value);
}

void AiSectorEval::SetTarget(int x, int y) {
    m_x30 = x;
    m_x34 = y;
    m_flags2c.m_value &= 0xdfffffff;
    MarkDirty();
}

void AiSectorEval::ClearTarget() {
    m_x30 = 0;
    m_x34 = 0;
    m_flags2c.m_value &= 0xdfffffff;
    MarkDirty();
}

void AiSectorEval::SetPeer(int value) {
    m_x38 = value;
    MarkDirty();
}

int AiSectorEval::ScoreByAge() {
    int total = 0;
    for (AiSystemEval* system : m_systems) {
        if ((system->m_flags30.m_value & 1) == 0 && (system->m_flags30.m_value & 0x20) != 0) total += system->DaysSinceTouched();
    }
    if (0 < total) total = total / (Stat(0x84) + Stat(0x80));
    return total;
}

AiRankedList* AiSectorEval::RankSystemsByStat(uint32_t requireA, uint32_t requireB, uint32_t requireC, uint32_t forbidA,
                                              uint32_t forbidB, uint32_t forbidC, uint32_t statIndex, int descending) {
    const SystemMasks masks{requireA, requireB, requireC, forbidA, forbidB, forbidC};
    AiRankedList* list = NewOrNull<AiRankedList>(descending);
    if (!list) return list;
    for (AiSystemEval* system : m_systems) {
        if (!masks.Matches(system)) continue;
        ObjectKey key;
        AiRankedItem* item = NewOrNull<AiRankedItem>(*system->GetKey(&key), system->m_stat[statIndex & 0xffff]);
        list->AddItem(item);
    }
    list->Number();
    return list;
}

AiRankedList* AiSectorEval::RankSystemsByAge(uint32_t requireA, uint32_t requireB, uint32_t requireC, uint32_t forbidA,
                                             uint32_t forbidB, uint32_t forbidC, int descending) {
    const SystemMasks masks{requireA, requireB, requireC, forbidA, forbidB, forbidC};
    AiRankedList* list = NewOrNull<AiRankedList>(descending);
    if (!list) return list;
    for (AiSystemEval* system : m_systems) {
        if (!masks.Matches(system)) continue;
        ObjectKey key;
        system->GetKey(&key);
        AiRankedItem* item = NewOrNull<AiRankedItem>(key, system->DaysSinceTouched());
        list->AddItem(item);
    }
    list->Number();
    return list;
}

AiRankedList* AiSectorEval::RankSystemsByDistance(const ObjectKey* target, uint32_t requireA, uint32_t requireB,
                                                  uint32_t requireC, uint32_t forbidA, uint32_t forbidB, uint32_t forbidC,
                                                  int descending) {
    const SystemMasks masks{requireA, requireB, requireC, forbidA, forbidB, forbidC};
    AiRankedList* list = nullptr;
    ObjectKey key = *target;
    bool resolved = false;
    while (!resolved) {
        if (InFamilies(key.Family(), 0x90, 0x98)) break;
        GameObjNotifier* object = GetObjectInFamilies_01_ff(m_view, &key);
        if (!object || !object->m_parent) break;
        ObjectKey parentKey;
        key = *GetObjectKey(object->m_parent, &parentKey);
    }
    if (InFamilies(key.Family(), 0x90, 0x98)) {
        AiSystemEval* targetSystem = m_owner->m_systems.FindEval(key);
        if (targetSystem) {
            const int16_t targetX = static_cast<int16_t>(targetSystem->m_position);
            const int16_t targetY = static_cast<int16_t>(targetSystem->m_position >> 16);
            list = NewOrNull<AiRankedList>(descending);
            if (list) {
                for (AiSystemEval* system : m_systems) {
                    if (!masks.Matches(system)) continue;
                    const int distance = m_owner->DistanceSquared(targetX, targetY, static_cast<int16_t>(system->m_position),
                                                                  static_cast<int16_t>(system->m_position >> 16));
                    ObjectKey systemKey;
                    AiRankedItem* item = NewOrNull<AiRankedItem>(*system->GetKey(&systemKey), distance);
                    list->AddItem(item);
                }
                list->Number();
            }
        }
    }
    return list;
}

AiRankedList* AiSectorEval::RankSystemsRandomly(uint32_t requireA, uint32_t requireB, uint32_t requireC, uint32_t forbidA,
                                                uint32_t forbidB, uint32_t forbidC, int descending) {
    const SystemMasks masks{requireA, requireB, requireC, forbidA, forbidB, forbidC};
    AiRankedList* list = NewOrNull<AiRankedList>(descending);
    if (!list) return list;
    for (AiSystemEval* system : m_systems) {
        if (!masks.Matches(system)) continue;
        ObjectKey key;
        system->GetKey(&key);
        AiRankedItem* item = NewOrNull<AiRankedItem>(key, SessionRandomNext(10));
        list->AddItem(item);
    }
    list->Number();
    return list;
}

ObjectKey* AiSectorEval::GetSystemKey(ObjectKey* out, uint32_t index) {
    ObjectKey key;
    AiSystemEval* system = m_systems[index & 0xffff];
    if (system) key = system->m_key;
    *out = key;
    return out;
}

void AiSectorEval::MarkDirty() {
    m_dirty = 1;
    m_owner->MarkDirty();
}

void AiSectorEval::Refresh() {
    if (m_dirty == 0) return;
    m_dirty = 0;
    m_flags24.m_value = 0;
    m_flags28.m_value &= 0x30000000;
    m_flags2c.m_value &= 0x28000000;
    for (int& value : m_stat) value = 0;
    Stat(0x144) = 100000;
    for (AiSystemEval* system : m_systems) {
        system->Refresh();
        Accumulate(system->m_stat, &system->m_flags28.m_value, &system->m_flags30.m_value, &system->m_flags2c.m_value);
    }
    if ((m_flags2c.m_value & 0x100) == 0) Stat(0x144) = 0;
    if (0 < Stat(0x7c)) Stat(0x8c) = Stat(0x8c) / Stat(0x7c);
    if (Stat(0x70) == 10) m_flags28.m_value |= 0x40000000;
    if (Stat(0x74) == 10) m_flags2c.m_value |= 1;
    if (Stat(0x78) == 10) m_flags2c.m_value |= 2;
    if (0 < Stat(0x19c)) m_flags2c.m_value |= 0x80;
    if (m_x30 == 0) {
        if (m_x38 != 0) Stat(0x18c) = 1;
    } else {
        m_flags2c.m_value |= 0x4000000;
        if (m_x38 == 0) {
            Stat(0x18c) = -1;
            m_flags2c.m_value |= 0x10000;
            return;
        }
    }
}

void AiSectorEval::SetSystem(uint16_t index, const ObjectKey* key, AiSystemEvalSet* systems) {
    AiSystemEval* system = systems->FindEval(*key);
    if (index < kSystemCount) {
        m_systems[index] = system;
        system->m_sector = this;
    }
}

void AiSectorEval::Accumulate(const int* p, const int* f28, const int* f30, const int* f2c)
{
  uint32_t uVar1;
  
  if ((*f30 & 1) == 0) {
    if ((*f30 & 4) == 0) {
      m_flags2c.m_value |= 0x10;
      Stat(0x84) += 1;
      Stat(0x190) += p[0x27];
      Stat(0x194) += p[0x28];
      Stat(0x198) += p[0x2b];
      Stat(0x11c) += p[4];
      Stat(0x120) += p[5];
      Stat(0x124) += p[6];
      if ((*f28 & 0x80000000) != 0) {
        m_flags24.m_value |= 0x40000000;
      }
      if ((*f30 & 0x20) == 0) goto LAB_00432b38;
      uVar1 = m_flags2c.m_value | 0x40;
    }
    else {
      m_flags2c.m_value |= 8;
      Stat(0x80) += 1;
      Stat(0x114) += p[2];
      Stat(0x118) += p[3];
      Stat(0x128) += p[7];
      Stat(0x12c) += p[10];
      Stat(0x110) += (p[1] - p[0xf]);
      Stat(0x190) += p[0x27];
      Stat(0x194) += p[0x28];
      Stat(0x198) += p[0x2b];
      Stat(0xa0) += p[0x34];
      Stat(0x130) += p[0x21];
      Stat(0x11c) += p[4];
      Stat(0x120) += p[5];
      Stat(0x124) += p[6];
      if (Stat(0x134) < p[0x34]) {
        Stat(0x134) = p[0x34];
      }
      if (Stat(0x138) < p[0x17]) {
        Stat(0x138) = p[0x17];
      }
      if (Stat(0x13c) < p[0x1b]) {
        Stat(0x13c) = p[0x1b];
      }
      if (Stat(0x140) < p[0x1d]) {
        Stat(0x140) = p[0x1d];
      }
      if ((*f28 & 0x80000000) != 0) {
        m_flags24.m_value |= 0x80000000;
      }
      if ((*f30 & 0x800) != 0) {
        m_flags2c.m_value |= 0x100;
        if (p[8] < Stat(0x144)) {
          Stat(0x144) = p[8];
        }
      }
      if ((*f30 & 0x100) == 0) {
        m_flags2c.m_value |= 0x400;
      }
      if ((*f30 & 0x2000) == 0) {
        m_flags2c.m_value |= 0x400000;
      }
      if ((*f2c & 0x2f70000) == 0) goto LAB_00432b38;
      uVar1 = m_flags2c.m_value | 0x80000;
    }
    m_flags2c.m_value = uVar1;
    goto LAB_00432b38;
  }
  m_flags2c.m_value |= 4;
  Stat(0x7c) += 1;
  Stat(0x8c) += *p;
  Stat(0x90) += p[1];
  Stat(0xa8) += p[2];
  Stat(0xac) += p[3];
  Stat(0xc8) += p[7];
  Stat(0xcc) += p[10];
  Stat(0xd0) += p[0xb];
  Stat(0xd4) += p[0x17];
  Stat(0x98) += p[0xe];
  Stat(0x94) += p[0xf];
  Stat(0x9c) += p[0x16];
  Stat(0xf0) += p[0x10];
  Stat(0xfc) += p[0x1e];
  Stat(0x100) += p[0x1f];
  Stat(0x104) += p[0x20];
  Stat(0x108) += p[0x21];
  Stat(0xdc) += p[0x1a];
  Stat(0x190) += p[0x27];
  Stat(0x194) += p[0x28];
  Stat(0x198) += p[0x2b];
  if ((*f30 & 0x40000) != 0) {
    m_flags2c.m_value |= 0x800000;
    if ((*f30 & 0x10000) == 0) {
      if ((*f30 & 0x20000) == 0) goto LAB_0043251a;
      uVar1 = m_flags2c.m_value | 0x2000000;
    }
    else {
      uVar1 = m_flags2c.m_value | 0x1000000;
    }
    m_flags2c.m_value = uVar1;
  }
LAB_0043251a:
  if ((*f28 & 2) == 0) {
    m_flags24.m_value |= 8;
  }
  if ((((*f28 & 3) == 0) && ((*f30 & 0x8000000) == 0)) && (0 < p[0x33])) {
    if ((*f28 & 0x200000) == 0) {
      m_flags24.m_value |= 0x20;
    }
    if ((*f28 & 0x400000) == 0) {
      m_flags24.m_value |= 0x40;
    }
    if ((*f28 & 0x3800000) == 0) {
      m_flags24.m_value |= 0x80;
    }
  }
  if (((*f28 & 0x80) != 0) && ((*f28 & 3) == 0)) {
    m_flags24.m_value |= 0x100;
  }
  if (((*f28 & 0x100) != 0) && ((*f28 & 3) == 0)) {
    m_flags24.m_value |= 0x200;
  }
  if (((*f28 & 0x20) != 0) && ((*f28 & 3) == 0)) {
    m_flags24.m_value |= 0x400;
  }
  if (((*f28 & 0x40) != 0) && ((*f28 & 3) == 0)) {
    m_flags24.m_value |= 0x800;
  }
  uVar1 = *f28;
  if ((((uVar1 & 0x800000) != 0) && ((uVar1 & 0x800) != 0)) && ((uVar1 & 3) == 0)) {
    m_flags24.m_value |= 0x1000;
  }
  if (((*f28 & 0x800) != 0) && ((*f28 & 0x3800000) == 0)) {
    m_flags24.m_value |= 0x2000;
  }
  if ((*f28 & 0x3e00000) == 0) {
LAB_00432660:
    if ((*f28 & 0x3800000) == 0) goto LAB_00432668;
  }
  else {
    m_flags24.m_value |= 0x20000000;
    if ((*f28 & 0x200000) != 0) {
      if ((*f28 & 2) == 0) {
        m_flags2c.m_value |= 0x20000;
      }
      Stat(0xb0) += p[4];
    }
    if ((*f28 & 0x400000) != 0) {
      Stat(0xb8) += p[5];
    }
    if ((*f28 & 0x3800000) != 0) {
      Stat(0xc0) += p[6];
      goto LAB_00432660;
    }
LAB_00432668:
    Stat(0xc4) += p[6];
  }
  if ((*f28 & 0x400000) == 0) {
    Stat(0xbc) += p[5];
  }
  if ((*f28 & 0x200000) == 0) {
    Stat(0xb4) += p[4];
  }
  if (((*f30 & 0x10000000) != 0) &&
     (Stat(0x188) = Stat(0x188) + 1, (*f30 & 0x20000000) == 0))
  {
    m_flags2c.m_value |= 0x10000000;
  }
  if (p[0x25] < 1) {
    if (p[0x25] < 0) {
      m_flags24.m_value |= 0x8000;
      Stat(0xe0) += 1;
      if ((*f28 & 0x3e00000) != 0) {
        m_flags24.m_value |= 0x4000;
      }
    }
  }
  else {
    m_flags24.m_value |= 0x10000;
    Stat(0xe4) += 1;
  }
  if ((((*f28 & 0x80000000) == 0) && (0 < p[0xb])) && (0 < p[7])) {
    m_flags24.m_value |= 0x20000;
  }
  if ((*f28 & 0x10000) != 0) {
    m_flags24.m_value |= 0x80000;
    Stat(0xec) += p[0x13];
    if ((*f28 & 0x10000000) != 0) {
      m_flags24.m_value |= 0x100000;
    }
    if ((*f28 & 0x20000000) != 0) {
      m_flags24.m_value |= 0x200000;
    }
    if ((*f30 & 0x4000) != 0) {
      m_flags2c.m_value |= 0x800;
    }
    if ((*f30 & 0x8000) != 0) {
      m_flags2c.m_value |= 0x1000;
    }
  }
  if ((*f28 & 0x2000) != 0) {
    m_flags24.m_value |= 0x40000;
    Stat(0xe8) += p[0x15];
    if ((*f28 & 0x3e00000) != 0) {
      m_flags24.m_value |= 0x800000;
    }
  }
  if ((*f28 & 0x20000) != 0) {
    m_flags24.m_value |= 0x400000;
  }
  if (((0 < p[0x35]) && ((*f30 & 0x40000) == 0)) && ((*f28 & 2) == 0)) {
    m_flags24.m_value |= 0x2000000;
    Stat(0xf8) += p[0x35];
  }
  if (p[0x26] < 0) {
    m_flags24.m_value |= 0x1000000;
    Stat(0xf4) += 1;
    if ((*f28 & 0x3e00000) != 0) {
      m_flags24.m_value |= 0x4000000;
    }
    if ((*f28 & 0x100000) != 0) {
      m_flags24.m_value |= 0x10000000;
    }
  }
  if ((*f28 & 0x80000) != 0) {
    m_flags24.m_value |= 0x8000000;
  }
  if ((*f28 & 1) != 0) {
    m_flags24.m_value |= 0x10;
  }
  if ((*f28 & 0x80000000) != 0) {
    m_flags24.m_value |= 0x40000000;
    if ((*f30 & 0x40000000) != 0) {
      m_flags2c.m_value |= 0x200;
    }
    if ((*f28 & 2) == 0) {
      Stat(0x1a0) += 1;
    }
  }
  if ((*f2c & 0x10000000) != 0) {
    m_flags28.m_value |= 0x8000000;
  }
LAB_00432b38:
  Stat(0x19c) += p[0x32];
  if ((*f2c & 0x8000000) != 0) {
    Stat(0x70) += 1;
  }
  if ((*f30 & 8) != 0) {
    Stat(0x74) += 1;
  }
  if ((*f30 & 0x20) != 0) {
    Stat(0x78) += 1;
  }
  if ((*f30 & 0x10) != 0) {
    Stat(0x88) += 1;
  }
  if ((*f30 & 0x40) != 0) {
    m_flags2c.m_value |= 0x20;
  }
  if ((*f30 & 0x40000000) != 0) {
    m_flags2c.m_value |= 0x40000000;
  }
  if ((*f30 & 0x80000000) != 0) {
    m_flags2c.m_value |= 0x80000000;
  }
  if ((*f30 & 0x200) != 0) {
    m_flags2c.m_value |= 0x40000;
  }
  if ((*f2c & 1) != 0) {
    Stat(0x150) += 1;
  }
  if ((*f2c & 2) != 0) {
    Stat(0x160) += 1;
    m_flags28.m_value |= 8;
  }
  if ((*f2c & 4) != 0) {
    Stat(0x154) += 1;
    m_flags28.m_value |= 1;
  }
  if ((*f2c & 8) != 0) {
    Stat(0x158) += 1;
    m_flags28.m_value |= 2;
  }
  if ((*f2c & 0x10) != 0) {
    Stat(0x15c) += 1;
    m_flags28.m_value |= 4;
  }
  if ((*f2c & 0x20) != 0) {
    Stat(0x164) += 1;
    m_flags28.m_value |= 0x10;
  }
  if ((*f2c & 0x40) != 0) {
    Stat(0x168) += 1;
    uVar1 = m_flags28.m_value;
    m_flags28.m_value = uVar1 | 0x20;
    if ((*f2c & 0x20000000) == 0) {
      m_flags28.m_value = uVar1 | 0x4000020;
    }
  }
  if ((*f2c & 0x80) != 0) {
    Stat(0x16c) += 1;
    m_flags28.m_value |= 0x40;
  }
  if ((*f2c & 0xc00) != 0) {
    Stat(0x170) += 1;
    m_flags28.m_value |= 0x80;
  }
  if ((*f2c & 0x3000) != 0) {
    Stat(0x174) += 1;
    m_flags28.m_value |= 0x100;
  }
  if ((*f2c & 0x100) != 0) {
    Stat(0x178) += 1;
    m_flags28.m_value |= 0x200;
  }
  if ((*f2c & 0x200) != 0) {
    Stat(0x17c) += 1;
    m_flags28.m_value |= 0x400;
  }
  if ((*f2c & 0x10000) != 0) {
    m_flags28.m_value |= 0x800;
  }
  if ((*f2c & 0x20000) != 0) {
    m_flags28.m_value |= 0x1000;
  }
  if ((*f2c & 0x800000) != 0) {
    m_flags28.m_value |= 0x2000;
  }
  if ((*f2c & 0xc000) != 0) {
    Stat(0x180) += 1;
    m_flags28.m_value |= 0x4000;
  }
  if ((*f2c & 0x1000000) != 0) {
    Stat(0x184) += 1;
    m_flags28.m_value |= 0x8000;
  }
  return;
}



