#include "events/game_clock_hooks.h"

namespace {

template <class T>
T* fromHookArgument(HookArg value) {
    return reinterpret_cast<T*>(value);
}

}  // namespace

void AssignGameTime(GameTime* self, const GameTime* other) {
    if (self != other) self->m_value = other->m_value;
}

void ReadGameTime(GameTime* self, istream* in) { StreamRead32(in, self); }

void ClearGameTime(GameTime* self) { self->m_value = 0; }

void InstallGameManagerHooks() {
    g_gameHook2AF4 = GameHookNextSerialA;
    g_gameHook2AFC = GameHookNextSerialB;
    g_gameHook2B04 = GameHookTickCount;
    g_gameHook2B08 = GameHookDay;
    g_gameHook2AD8 = GameHookScheduleTimer;
    g_gameHook2B00 = GameHookScheduleCommand;
    g_gameHook2AF8 = GameHookScheduleEvent;
    g_gameHook2AD4 = GameHookDispatchPostedEvent;
    g_gameHook2AF0 = GameHookIsSetupDone;
    g_gameHook2ADC = GameHookNoteDispatch;
    g_gameHook2AE4 = GameHookNoteDispatch;
}

int GetGameSettings(GameSettings* out) {
    if (GameManager* manager = GetGameManager()) {
        manager->GetSettings(out);
        return 1;
    }
    GameSettings defaults;
    *out = defaults;
    return 1;
}

int GameHookTickCount() {
    GameManager* manager = GetGameManager();
    return manager ? manager->m_tickCount : 0;
}

int GameHookDay() {
    GameManager* manager = GetGameManager();
    return manager ? manager->m_day : 0;
}

void BuildAvailableOrdersViaManager(int side, ObjectKeyValueSet* team, EventKindSet* orders) {
    if (GameManager* manager = GetGameManager()) manager->BuildAvailableOrders(side, team, orders);
}

int QueryBattleState(int side, uint32_t* a, uint32_t* b, int* c, uint32_t* d) {
    *a = 0;
    *b = 0;
    *c = 0;
    int result = 1;
    *d = 0;
    const int mappedSide = side == 1 ? 1 : (side == 2 ? 2 : 0);
    if (GameManager* manager = GetGameManager()) result = manager->QueryRootBattle(mappedSide, a, b, c, d);
    return result;
}

void GameHookScheduleTimer(HookArg timer, int delay) {
    if (GameManager* manager = GetGameManager()) manager->ScheduleTimer(fromHookArgument<TimedEvent>(timer), delay);
}

void GameHookScheduleCommand(HookArg event) {
    if (GameManager* manager = GetGameManager()) manager->ScheduleCommand(fromHookArgument<EventBase>(event));
}

void GameHookScheduleEvent(HookArg event) {
    if (GameManager* manager = GetGameManager()) manager->ScheduleEvent(fromHookArgument<EventBase>(event));
}

int GameHookDispatchPostedEvent(HookArg event) {
    int result = 1;
    if (GameManager* manager = GetGameManager())
        result = manager->DispatchPostedEvent(fromHookArgument<NotifPayload>(event));
    return result;
}

int GameHookIsSetupDone() {
    int result = 0;
    if (GameManager* manager = GetGameManager()) result = manager->m_setupDone;
    return result;
}

void CopyCurrentSnapshot(GameStateSnapshot* out) {
    if (GameManager* manager = GetGameManager()) *out = manager->m_snapshot[0];
}

int SetSideSnapshot(GameStateSnapshot* snapshot, int which) {
    int result = 1;
    if (GameManager* manager = GetGameManager()) result = manager->SetSnapshot(snapshot, which);
    return result;
}

int GameHookNoteDispatch(HookArg item) {
    int result = 1;
    if (GameManager* manager = GetGameManager()) result = manager->NoteDispatch(fromHookArgument<void>(item));
    return result;
}

namespace {

int readManagerField(int GameManager::*field) {
    GameManager* manager = GetGameManager();
    return manager ? manager->*field : 0;
}

void writeManagerField(int GameManager::*field, int value) {
    if (GameManager* manager = GetGameManager()) manager->*field = value;
}

}  // namespace

int GetManagerStateDiffers() { return readManagerField(&GameManager::m_stateDiffers); }
int GetManagerUnk34() { return readManagerField(&GameManager::m_unk34); }
void SetManagerUnk34(int value) { writeManagerField(&GameManager::m_unk34, value); }
int GetManagerUnk38() { return readManagerField(&GameManager::m_unk38); }
int GetManagerUnk3c() { return readManagerField(&GameManager::m_unk3c); }
void SetManagerUnk3c(int value) { writeManagerField(&GameManager::m_unk3c, value); }
int GetManagerUnk40() { return readManagerField(&GameManager::m_unk40); }
int GetManagerUnk44() { return readManagerField(&GameManager::m_unk44); }
void SetManagerUnk44(int value) { writeManagerField(&GameManager::m_unk44, value); }

int AddManagerNotice(IdNode* notice) {
    if (GameManager* manager = GetGameManager()) {
        if (notice) manager->AddNotice(notice);
    }
    return 1;
}

int GameHookNextSerialA() {
    int result = 0;
    if (GameManager* manager = GetGameManager()) result = ++manager->m_serialA;
    return result;
}

int GameHookNextSerialB() {
    int result = 0;
    if (GameManager* manager = GetGameManager()) result = ++manager->m_serialB;
    return result;
}
