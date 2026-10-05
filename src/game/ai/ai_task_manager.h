#pragma once
#include <cstdint>

#include "ai/ai_task_base.h"
#include "app/player_controller_decl.h"
#include "core/containers.h"
#include "core/layout.h"
#include "core/stream_io.h"
#include "gameobj/object_key.h"

// The collections an AiPlayer keeps (0x4be580-0x4bf0b0): the history of the missions it ran, its tasks and the notes it has
// been sent.

class AiSideController;

// ---- AiMissionHistory ---------------------------------------------------------------------------------------------------

// One remembered mission (VT 0x65c6b4, 0x2c bytes; reconstructed with the task types at 0x4c8000, part 2 of the AI module).
class AiMissionHistoryEntry : public AvlNode {
public:
    AiMissionHistoryEntry();                    // 0x4c8000
    virtual ~AiMissionHistoryEntry();           // 0: 0x4c8060 (deleting), 0x4c8080
    void Read(istream* in);                     // 0x4c80e0
    void Write(ostream* out);                   // 0x4c8130
    // Fills the entry from a finished mission (an object of the mission module: id +0x20, kind +0x24, time +0x28, key +0x2c);
    // true if it names a mission against a system.
    int Fill(const void* mission);              // 0x4c8180

    uint32_t m_id;       // +0x18
    int m_x1c;           // +0x1c
    int m_x20;           // +0x20
    GameTime m_time;     // +0x24
    ObjectKey m_key;     // +0x28
};

// The tree of AiMissionHistoryEntry (VT 0x65c450): ordered by id like an IdAvlTree.
class AiMissionHistory : public AvlTree {
public:
    AiMissionHistory();                         // 0x4be580
    virtual ~AiMissionHistory();                // 0: 0x4be5a0 (deleting), 0x4be5c0
    virtual int IsLess(AvlNode* a, AvlNode* b) override;  // 2: 0x5f5560 (shared with IdAvlTree)
    void Load(istream* in);                     // 0x4be610
    void Save(ostream* out);                    // 0x4be6b0
    int Add(const void* mission);               // 0x4be700: remembers a mission; 1 if it was added
    AiMissionHistoryEntry* Find(const int* key, uint32_t id, int x20);  // 0x4be780
};

// ---- AiTaskManager ------------------------------------------------------------------------------------------------------

// The AI player's tasks (VT 0x65c460, 0x1c bytes): one task of each of the 14 types, ids 1-14. AiPlayer asks them in turn.
class AiTaskManager : public IdAvlTree {
public:
    static constexpr int kTaskTypes = 14;

    AiTaskManager();                            // 0x4be7e0
    virtual ~AiTaskManager();                   // 0: 0x4be800 (deleting), 0x4be820
    void Load(istream* in, AiSideController* owner);  // 0x4be840
    void Save(ostream* out);                    // 0x49de40
    uint32_t CreateTask(int type, AiSideController* owner);  // 0x4be930: adds the task of that type unless there is one; returns its id
    // The next task that wants to run: the one with the highest priority, else the next one in turn that wants to run.
    AiTaskBase* SelectNext();                   // 0x4be980
    AiTaskBase* MakeTask(int type);             // 0x4bea20: a new task of the type (ids 1-14), null otherwise
    AiTaskBase* PickBest();                     // 0x4bede0: the task with the highest priority that wants to run

    int m_view;               // +0x0c
    AiTaskBase* m_current;    // +0x10 the task AiPlayer is running
    AiTaskBase* m_cursor;     // +0x14 where PickBest continues
    AiTaskBase* m_last;       // +0x18 the task SelectNext returned
};

// ---- AiNoticeQueue ------------------------------------------------------------------------------------------------------

// The notes the AI player was sent, in AiNoticeGroups by group id (VT 0x65c470, 0x10 bytes). The tree holds the IdNode part
// of the groups.
class AiNoticeQueue : public IdAvlTree {
public:
    AiNoticeQueue();                            // 0x4bee50
    virtual ~AiNoticeQueue();                   // 0: 0x4bee70 (deleting), 0x4bee90
    void Load(istream* in);                     // 0x4beea0
    void Save(ostream* out);                    // 0x4bef30
    void Add(AiNotice* notice);                 // 0x4bef90: notes without a group are dropped
    AiNoticeGroup* DetachFinished();            // 0x4befe0: the first finished group, removed from the queue
    void NewGroup(AiNotice* notice);            // 0x4bf030

    int m_x0c;                // +0x0c always 1
};

// The constructors of the 14 task types (part 2): (view) on memory from operator new.
AiTaskBase* __thiscall FUN_004da950(void* memory, int view);  // type 1 (0x78)
AiTaskBase* __thiscall FUN_004e1e50(void* memory, int view);  // type 2 (0x80)
AiTaskBase* __thiscall FUN_004d6b20(void* memory, int view);  // type 3 (0x7c)
AiTaskBase* __thiscall FUN_004d2220(void* memory, int view);  // type 4 (0x60)
AiTaskBase* __thiscall FUN_004cfb20(void* memory, int view);  // type 5 (0x88)
AiTaskBase* __thiscall FUN_004dd460(void* memory, int view);  // type 6 (0x8c)
AiTaskBase* __thiscall FUN_004d2ef0(void* memory, int view);  // type 7 (0x7c)
AiTaskBase* __thiscall FUN_004cf410(void* memory, int view);  // type 8 (0x58)
AiTaskBase* __thiscall FUN_004cf0a0(void* memory, int view);  // type 9 (0x58)
AiTaskBase* __thiscall FUN_004cc6b0(void* memory, int view);  // type 10 (0x84)
AiTaskBase* __thiscall FUN_004c8820(void* memory, int view);  // type 11 (0x88)
AiTaskBase* __thiscall FUN_004c8260(void* memory, int view);  // type 12 (0x60)
AiTaskBase* __thiscall FUN_004bf0d0(void* memory, int view);  // type 13 (0x70)
AiTaskBase* __thiscall FUN_004d2810(void* memory, int view);  // type 14 (0x4c)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(AiMissionHistoryEntry, 0x2c);
REB_ASSERT_SIZE(AiMissionHistory, 0x08);
REB_ASSERT_SIZE(AiTaskManager, 0x1c);
REB_ASSERT_OFFSET(AiTaskManager, m_current, 0x10);
REB_ASSERT_SIZE(AiNoticeQueue, 0x10);
#pragma clang diagnostic pop
