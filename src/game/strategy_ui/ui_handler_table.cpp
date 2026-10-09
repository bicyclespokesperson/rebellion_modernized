#include "strategy_ui/ui_handler_table.h"

#include <cstdint>
#include <new>

#include "core/containers.h"
#include "core/stream_io.h"
#include "core/vptr_store.h"
#include "events/player_notices.h"
#include "strategy_ui/manufacture_order_window.h"

extern "C" int __cdecl atexit(void(__cdecl* function)(void));

// The handler singletons are function-local statics of the original constructor: a guard byte per eight of them,
// constructed on first use with their destructor registered through atexit. They belong to the player-action modules
// (0x48aac0-0x49bda0), which are not reconstructed yet, so objects, constructors and the atexit stubs are declared by address.

extern uint8_t DAT_006b169c;
extern uint8_t DAT_006b1af8;
extern uint8_t DAT_006b1c2c;
extern uint8_t DAT_006b1ed4;
extern uint8_t DAT_006b22ec;
extern uint8_t DAT_006b24ec;

extern uint8_t DAT_006b24f0[];
extern uint8_t DAT_006b2358[];
extern uint8_t DAT_006b1b68[];
extern uint8_t DAT_006b16a0[];
extern uint8_t DAT_006b1bc8[];
extern uint8_t DAT_006b2060[];
extern uint8_t DAT_006b21b0[];
extern uint8_t DAT_006b2830[];
extern uint8_t DAT_006b2570[];
extern uint8_t DAT_006b1e70[];
extern uint8_t DAT_006b1878[];
extern uint8_t DAT_006b2218[];
extern uint8_t DAT_006b1770[];
extern uint8_t DAT_006b14e8[];
extern uint8_t DAT_006b2788[];
extern uint8_t DAT_006b1950[];
extern uint8_t DAT_006b1a20[];
extern uint8_t DAT_006b2130[];
extern uint8_t DAT_006b1d98[];
extern uint8_t DAT_006b1c30[];
extern uint8_t DAT_006b17e0[];
extern uint8_t DAT_006b2720[];
extern uint8_t DAT_006b1708[];
extern uint8_t DAT_006b2488[];
extern uint8_t DAT_006b26b8[];
extern uint8_t DAT_006b20c8[];
extern uint8_t DAT_006b1e08[];
extern uint8_t DAT_006b1b00[];
extern uint8_t DAT_006b25d8[];
extern uint8_t DAT_006b1a90[];
extern uint8_t DAT_006b18e0[];
extern uint8_t DAT_006b1d30[];
extern uint8_t DAT_006b2650[];
extern uint8_t DAT_006b19b8[];
extern uint8_t DAT_006b2288[];
extern uint8_t DAT_006b1630[];
extern uint8_t DAT_006b15c0[];
extern uint8_t DAT_006b1cc8[];
extern uint8_t DAT_006b2420[];
extern uint8_t DAT_006b22f0[];
extern uint8_t DAT_006b1ff8[];
extern uint8_t DAT_006b1ed8[];
extern uint8_t DAT_006b1558[];


void FUN_00437660();
void FUN_00437650();
void FUN_00437640();
void FUN_00437630();
void FUN_00437620();
void FUN_00437610();
void FUN_00437600();
void FUN_004375f0();
void FUN_004375e0();
void FUN_004375d0();
void FUN_004375c0();
void FUN_004375b0();
void FUN_004375a0();
void FUN_00437590();
void FUN_00437580();
void FUN_00437570();
void FUN_00437560();
void FUN_00437550();
void FUN_00437540();
void FUN_00437530();
void FUN_00437520();
void FUN_00437510();
void FUN_00437500();
void FUN_004374f0();
void FUN_004374e0();
void FUN_004374d0();
void FUN_004374c0();
void FUN_004374b0();
void FUN_004374a0();
void FUN_00437490();
void FUN_00437480();
void FUN_00437470();
void FUN_00437460();
void FUN_00437450();
void FUN_00437440();
void FUN_00437430();
void FUN_00437420();
void FUN_00437410();
void FUN_00437400();
void FUN_004373f0();
void FUN_004373e0();
void FUN_004373d0();
void FUN_004373c0();

extern int g_uiHandlerTableNeedsInit;  // 0x6a8704
extern UiHandler* g_uiHandlers[0x2e];  // 0x6b1f40

namespace {

using Constructor = void (*)(void*);
using KindFn = int (*)(void*);

template <class T>
void Construct(void* memory) {
    new (memory) T();
}

template <class T>
int KindOf(void* object) {
    return static_cast<T*>(object)->GetKind();
}


struct HandlerInit {
    uint8_t* m_guard;
    uint8_t m_guardBit;
    Constructor m_construct;
    uint8_t* m_object;
    void(__cdecl* m_destroyAtExit)();
    int m_slot;       // -1: ask m_kindOf
    KindFn m_kindOf;
};

const HandlerInit kHandlerInits[] = {
    {&DAT_006b24ec, 0x01, Construct<Notice2>, DAT_006b24f0, FUN_00437660, 2, nullptr},
    {&DAT_006b24ec, 0x02, Construct<Notice1>, DAT_006b2358, FUN_00437650, 1, nullptr},
    {&DAT_006b24ec, 0x04, Construct<Notice8>, DAT_006b1b68, FUN_00437640, -1, KindOf<Notice8>},
    {&DAT_006b24ec, 0x08, Construct<Notice6>, DAT_006b16a0, FUN_00437630, 6, nullptr},
    {&DAT_006b24ec, 0x10, Construct<Notice5>, DAT_006b1bc8, FUN_00437620, 5, nullptr},
    {&DAT_006b24ec, 0x20, Construct<Notice4>, DAT_006b2060, FUN_00437610, 4, nullptr},
    {&DAT_006b24ec, 0x40, Construct<Notice3>, DAT_006b21b0, FUN_00437600, 3, nullptr},
    {&DAT_006b24ec, 0x80, Construct<Notice9>, DAT_006b2830, FUN_004375f0, 9, nullptr},
    {&DAT_006b169c, 0x01, Construct<Notice10>, DAT_006b2570, FUN_004375e0, 10, nullptr},
    {&DAT_006b169c, 0x02, Construct<Notice11>, DAT_006b1e70, FUN_004375d0, 11, nullptr},
    {&DAT_006b169c, 0x04, Construct<Notice12>, DAT_006b1878, FUN_004375c0, 12, nullptr},
    {&DAT_006b169c, 0x08, Construct<Notice13>, DAT_006b2218, FUN_004375b0, 13, nullptr},
    {&DAT_006b169c, 0x10, Construct<Notice14>, DAT_006b1770, FUN_004375a0, 14, nullptr},
    {&DAT_006b169c, 0x20, Construct<Notice16>, DAT_006b14e8, FUN_00437590, 16, nullptr},
    {&DAT_006b169c, 0x40, Construct<Notice15>, DAT_006b2788, FUN_00437580, 15, nullptr},
    {&DAT_006b169c, 0x80, Construct<Notice7>, DAT_006b1950, FUN_00437570, 7, nullptr},
    {&DAT_006b1af8, 0x01, Construct<Notice19>, DAT_006b1a20, FUN_00437560, 19, nullptr},
    {&DAT_006b1af8, 0x02, Construct<Notice20>, DAT_006b2130, FUN_00437550, 20, nullptr},
    {&DAT_006b1af8, 0x04, Construct<Notice21>, DAT_006b1d98, FUN_00437540, 21, nullptr},
    {&DAT_006b1af8, 0x08, Construct<Notice22>, DAT_006b1c30, FUN_00437530, -1, KindOf<Notice22>},
    {&DAT_006b1af8, 0x10, Construct<Notice23>, DAT_006b17e0, FUN_00437520, -1, KindOf<Notice23>},
    {&DAT_006b1af8, 0x20, Construct<Notice24>, DAT_006b2720, FUN_00437510, 24, nullptr},
    {&DAT_006b1af8, 0x40, Construct<Notice25>, DAT_006b1708, FUN_00437500, 25, nullptr},
    {&DAT_006b1af8, 0x80, Construct<Notice26b>, DAT_006b2488, FUN_004374f0, 26, nullptr},
    {&DAT_006b1c2c, 0x01, Construct<Notice26a>, DAT_006b26b8, FUN_004374e0, 26, nullptr},
    {&DAT_006b1c2c, 0x02, Construct<Notice30>, DAT_006b20c8, FUN_004374d0, 30, nullptr},
    {&DAT_006b1c2c, 0x04, Construct<Notice28>, DAT_006b1e08, FUN_004374c0, 28, nullptr},
    {&DAT_006b1c2c, 0x08, Construct<Notice29>, DAT_006b1b00, FUN_004374b0, 29, nullptr},
    {&DAT_006b1c2c, 0x10, Construct<Notice32>, DAT_006b25d8, FUN_004374a0, 32, nullptr},
    {&DAT_006b1c2c, 0x20, Construct<Notice31>, DAT_006b1a90, FUN_00437490, 31, nullptr},
    {&DAT_006b1c2c, 0x40, Construct<Notice36>, DAT_006b18e0, FUN_00437480, 36, nullptr},
    {&DAT_006b1c2c, 0x80, Construct<Notice35>, DAT_006b1d30, FUN_00437470, 35, nullptr},
    {&DAT_006b22ec, 0x01, Construct<Notice37>, DAT_006b2650, FUN_00437460, 37, nullptr},
    {&DAT_006b22ec, 0x02, Construct<Notice38>, DAT_006b19b8, FUN_00437450, 38, nullptr},
    {&DAT_006b22ec, 0x04, Construct<Notice39>, DAT_006b2288, FUN_00437440, 39, nullptr},
    {&DAT_006b22ec, 0x08, Construct<Notice40>, DAT_006b1630, FUN_00437430, 40, nullptr},
    {&DAT_006b22ec, 0x10, Construct<Notice33>, DAT_006b15c0, FUN_00437420, 33, nullptr},
    {&DAT_006b22ec, 0x20, Construct<Notice34>, DAT_006b1cc8, FUN_00437410, 34, nullptr},
    {&DAT_006b22ec, 0x40, Construct<Notice41>, DAT_006b2420, FUN_00437400, 41, nullptr},
    {&DAT_006b22ec, 0x80, Construct<Notice42>, DAT_006b22f0, FUN_004373f0, 42, nullptr},
    {&DAT_006b1ed4, 0x01, Construct<Notice43>, DAT_006b1ff8, FUN_004373e0, 43, nullptr},
    {&DAT_006b1ed4, 0x02, Construct<Notice45>, DAT_006b1ed8, FUN_004373d0, 45, nullptr},
    {&DAT_006b1ed4, 0x04, Construct<Notice44>, DAT_006b1558, FUN_004373c0, 44, nullptr},
};

void InitializeHandler(const HandlerInit& init) {
    if ((*init.m_guard & init.m_guardBit) == 0) {
        *init.m_guard |= init.m_guardBit;
        init.m_construct(init.m_object);
        atexit(init.m_destroyAtExit);
    }
    const int slot = init.m_kindOf ? init.m_kindOf(init.m_object) : init.m_slot;
    g_uiHandlers[slot] = reinterpret_cast<UiHandler*>(init.m_object);
}

}  // namespace

UiRecordTree::UiRecordTree() {
    if (g_uiHandlerTableNeedsInit == 0) return;
    for (UiHandler*& handler : g_uiHandlers) handler = nullptr;
    for (const HandlerInit& init : kHandlerInits) InitializeHandler(init);
    g_uiHandlerTableNeedsInit = 0;
}

UiRecordTree::~UiRecordTree() { REB_KEEP_VPTR_STORE(); }

void UiRecordTree::Read(istream* in) {
    int16_t count;
    ReadValue16(in, &count);
    while (count != 0) {
        --count;
        uint32_t kind;
        ReadValue32(in, &kind);
        UiRecord* record = CreateUiRecord(kind);
        record->Read(in);
        AddWithId(record);
    }
}

void UiRecordTree::Write(ostream* out) {
    int16_t count = Count();
    WriteValue16(out, &count);
    for (AvlNode* node = First(); node; node = node->m_successor) {
        UiRecord* record = static_cast<UiRecord*>(node);
        int kind = record->GetKind();
        WriteValue32(out, &kind);
        record->Write(out);
    }
}

UiRecord* CreateUiRecord(int kind) { return g_uiHandlers[kind]->CreateRecord(); }

// ---- Static initializers of three UI globals (a zeroed pointer each, destroyed at exit) -----------------------

void __thiscall FUN_0042dbe0(void* self);
void FUN_004377c0();
void FUN_00437810();
void FUN_00437860();

void StaticInitGlobalA() {
    InitGlobalA();
    RegisterGlobalADestructor();
}
void InitGlobalA() { FUN_0042dbe0(&g_lastShipType); }
void RegisterGlobalADestructor() { atexit(FUN_004377c0); }
void StaticInitGlobalB() {
    InitGlobalB();
    RegisterGlobalBDestructor();
}
void InitGlobalB() { FUN_0042dbe0(&g_lastTroopType); }
void RegisterGlobalBDestructor() { atexit(FUN_00437810); }
void StaticInitGlobalC() {
    InitGlobalC();
    RegisterGlobalCDestructor();
}
void InitGlobalC() { FUN_0042dbe0(&g_lastFacilityType); }
void RegisterGlobalCDestructor() { atexit(FUN_00437860); }
