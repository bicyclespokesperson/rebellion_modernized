#include "ai/ai_task_manager.h"

#include "core/checked_new.h"
#include "core/vptr_store.h"

// ---- AiMissionHistory ---------------------------------------------------------------------------------------------------

AiMissionHistory::AiMissionHistory() : AvlTree() {}

AiMissionHistory::~AiMissionHistory() {
    Clear();
    REB_KEEP_VPTR_STORE();
}

int AiMissionHistory::IsLess(AvlNode* a, AvlNode* b) {
    if (a && b && static_cast<AiMissionHistoryEntry*>(a)->m_id < static_cast<AiMissionHistoryEntry*>(b)->m_id) return 1;
    return 0;
}

void AiMissionHistory::Load(istream* in) {
    Clear();
    uint32_t count;
    ReadValue32(in, &count);
    for (uint32_t index = 0; index < count; ++index) {
        AiMissionHistoryEntry* entry = NewOrNull<AiMissionHistoryEntry>();
        entry->Read(in);
        Insert(entry);
    }
}

void AiMissionHistory::Save(ostream* out) {
    uint32_t count = static_cast<uint16_t>(Count());
    WriteValue32(out, &count);
    for (AvlNode* node = First(); node; node = node->m_successor) static_cast<AiMissionHistoryEntry*>(node)->Write(out);
}

int AiMissionHistory::Add(const void* mission) {
    if (!mission) return 0;
    AiMissionHistoryEntry* entry = NewOrNull<AiMissionHistoryEntry>();
    if (!entry) return 0;
    entry->Fill(mission);
    Insert(entry);
    return 1;
}

AiMissionHistoryEntry* AiMissionHistory::Find(const int* key, uint32_t id, int x20) {
    bool found = false;
    AvlNode* node = First();
    while (node && !found) {
        AiMissionHistoryEntry* entry = static_cast<AiMissionHistoryEntry*>(node);
        found = entry->m_id == id && entry->m_key.m_value == static_cast<uint32_t>(*key) && entry->m_x20 == x20;
        if (found) return entry;
        node = id < entry->m_id ? nullptr : node->m_successor;
    }
    return nullptr;
}

// ---- AiTaskManager ------------------------------------------------------------------------------------------------------

AiTaskManager::AiTaskManager() : IdAvlTree() {
    m_view = 0;
    m_current = nullptr;
    m_cursor = nullptr;
    m_last = nullptr;
}

AiTaskManager::~AiTaskManager() {
    m_current = nullptr;
    m_cursor = nullptr;
    m_last = nullptr;
    REB_KEEP_VPTR_STORE();
}

void AiTaskManager::Load(istream* in, AiSideController* owner) {
    Clear();
    ReadValue32(in, &m_view);
    int16_t count;
    ReadValue16(in, &count);
    while (count != 0) {
        int type;
        ReadValue32(in, &type);
        AiTaskBase* task = MakeTask(type);
        if (task) {
            task->SetOwner(owner);
            task->Read(in);
            AddWithId(task);
        }
        --count;
    }
    uint32_t id;
    ReadValue32(in, &id);
    m_current = static_cast<AiTaskBase*>(FindById(id));
    ReadValue32(in, &id);
    m_cursor = static_cast<AiTaskBase*>(FindById(id));
    ReadValue32(in, &id);
    m_last = static_cast<AiTaskBase*>(FindById(id));
}

void AiTaskManager::Save(ostream* out) {
    WriteValue32(out, &m_view);
    int16_t count = Count();
    WriteValue16(out, &count);
    for (AvlNode* node = First(); node; node = node->m_successor) {
        AiTaskBase* task = static_cast<AiTaskBase*>(node);
        uint32_t id = task->m_id;
        WriteValue32(out, &id);
        task->Write(out);
    }
    uint32_t id = m_current ? m_current->m_id : 0;
    WriteValue32(out, &id);
    id = m_cursor ? m_cursor->m_id : 0;
    WriteValue32(out, &id);
    id = m_last ? m_last->m_id : 0;
    WriteValue32(out, &id);
}

uint32_t AiTaskManager::CreateTask(int type, AiSideController* owner) {
    uint32_t id = 0;
    if (FindById(type)) return id;
    AiTaskBase* task = MakeTask(type);
    if (task) {
        task->SetOwner(owner);
        task->m_flags.m_value = 0;
        id = AddWithId(task);
    }
    return id;
}

AiTaskBase* AiTaskManager::SelectNext() {
    m_last = PickBest();
    if (m_last != nullptr) return m_last;
    if (m_current == nullptr || (m_current = static_cast<AiTaskBase*>(m_current->m_successor)) == nullptr)
        m_current = static_cast<AiTaskBase*>(First());
    AiTaskBase* start = m_current;
    bool wrapped = false;
    int wants = m_current->m_flags.Get();
    while (wants != 1) {
        if (wrapped) {
            m_last = nullptr;
            return m_last;
        }
        m_current = static_cast<AiTaskBase*>(m_current->m_successor);
        if (m_current == nullptr) m_current = static_cast<AiTaskBase*>(First());
        wrapped = start == m_current;
        wants = m_current->m_flags.Get();
    }
    if (!wrapped) {
        m_last = m_current;
        return m_current;
    }
    m_last = nullptr;
    return m_last;
}

AiTaskBase* AiTaskManager::MakeTask(int type) {
    void* memory;
    switch (type) {
    case 1:
        memory = ::operator new(0x78);
        if (memory) return FUN_004da950(memory, m_view);
        break;
    case 2:
        memory = ::operator new(0x80);
        if (memory) return FUN_004e1e50(memory, m_view);
        break;
    case 3:
        memory = ::operator new(0x7c);
        if (memory) return FUN_004d6b20(memory, m_view);
        break;
    case 4:
        memory = ::operator new(0x60);
        if (memory) return FUN_004d2220(memory, m_view);
        break;
    case 5:
        memory = ::operator new(0x88);
        if (memory) return FUN_004cfb20(memory, m_view);
        break;
    case 6:
        memory = ::operator new(0x8c);
        if (memory) return FUN_004dd460(memory, m_view);
        break;
    case 7:
        memory = ::operator new(0x7c);
        if (memory) return FUN_004d2ef0(memory, m_view);
        break;
    case 8:
        memory = ::operator new(0x58);
        if (memory) return FUN_004cf410(memory, m_view);
        break;
    case 9:
        memory = ::operator new(0x58);
        if (memory) return FUN_004cf0a0(memory, m_view);
        break;
    case 10:
        memory = ::operator new(0x84);
        if (memory) return FUN_004cc6b0(memory, m_view);
        break;
    case 11:
        memory = ::operator new(0x88);
        if (memory) return FUN_004c8820(memory, m_view);
        break;
    case 12:
        memory = ::operator new(0x60);
        if (memory) return FUN_004c8260(memory, m_view);
        break;
    case 13:
        memory = ::operator new(0x70);
        if (memory) return FUN_004bf0d0(memory, m_view);
        break;
    case 14:
        memory = ::operator new(0x4c);
        if (memory) return FUN_004d2810(memory, m_view);
        break;
    }
    return nullptr;
}

AiTaskBase* AiTaskManager::PickBest() {
    AiTaskBase* best = nullptr;
    uint32_t bestPriority = 0;
    if (m_cursor == nullptr || (m_cursor = static_cast<AiTaskBase*>(m_cursor->m_successor)) == nullptr)
        m_cursor = static_cast<AiTaskBase*>(First());
    AiTaskBase* task = m_cursor;
    while (task) {
        if (task->m_flags.Get() == 1 && bestPriority < static_cast<uint32_t>(task->m_priority)) {
            bestPriority = task->m_priority;
            best = task;
        }
        task = static_cast<AiTaskBase*>(task->m_successor);
        if (task == nullptr) task = static_cast<AiTaskBase*>(First());
        if (task == m_cursor) task = nullptr;
    }
    return best;
}

// ---- AiNoticeQueue ------------------------------------------------------------------------------------------------------

AiNoticeQueue::AiNoticeQueue() : IdAvlTree() { m_x0c = 1; }

AiNoticeQueue::~AiNoticeQueue() { REB_KEEP_VPTR_STORE(); }

void AiNoticeQueue::Load(istream* in) {
    Clear();
    uint32_t count;
    ReadValue32(in, &count);
    for (; count != 0; --count) {
        AiNoticeGroup* group = NewOrNull<AiNoticeGroup>();
        group->Load(in);  // BUG(original): the group is never added to the queue, so a loaded queue is empty (and leaks)
    }
}

void AiNoticeQueue::Save(ostream* out) {
    uint32_t count = static_cast<uint16_t>(Count());
    WriteValue32(out, &count);
    for (AvlNode* node = First(); node; node = node->m_successor)
        static_cast<AiNoticeGroup*>(static_cast<IdNode*>(node))->Save(out);
}

void AiNoticeQueue::Add(AiNotice* notice) {
    if (!notice) return;
    if (notice->m_groupId == 0) {
        delete notice;
        return;
    }
    IdNode* node = FindById(notice->m_groupId);
    if (node)
        static_cast<AiNoticeGroup*>(node)->Add(notice);
    else
        NewGroup(notice);
}

AiNoticeGroup* AiNoticeQueue::DetachFinished() {
    bool detached = false;
    AiNoticeGroup* group = nullptr;
    AvlNode* node = First();
    if (node) group = static_cast<AiNoticeGroup*>(static_cast<IdNode*>(node));
    while (group && !detached) {
        if (group->m_finished != 0) {
            DetachById(static_cast<IdNode*>(group)->m_id);
            detached = true;
        } else {
            AvlNode* next = static_cast<IdNode*>(group)->m_successor;
            group = next ? static_cast<AiNoticeGroup*>(static_cast<IdNode*>(next)) : nullptr;
        }
    }
    return group;
}

void AiNoticeQueue::NewGroup(AiNotice* notice) {
    AiNoticeGroup* group = NewOrNull<AiNoticeGroup>();
    if (group) {
        group->Add(notice);
        AddWithId(group);
    }
}
