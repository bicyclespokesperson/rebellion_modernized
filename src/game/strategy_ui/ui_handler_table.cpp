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


void DestroyNotice2Handler();
void DestroyNotice1Handler();
void DestroyNotice8Handler();
void DestroyNotice6Handler();
void DestroyNotice5Handler();
void DestroyNotice4Handler();
void DestroyNotice3Handler();
void DestroyNotice9Handler();
void DestroyNotice10Handler();
void DestroyNotice11Handler();
void DestroyNotice12Handler();
void DestroyNotice13Handler();
void DestroyNotice14Handler();
void DestroyNotice16Handler();
void DestroyNotice15Handler();
void DestroyNotice7Handler();
void DestroyNotice19Handler();
void DestroyNotice20Handler();
void DestroyNotice21Handler();
void DestroyNotice22Handler();
void DestroyNotice23Handler();
void DestroyNotice24Handler();
void DestroyNotice25Handler();
void DestroyNotice26bHandler();
void DestroyNotice26aHandler();
void DestroyNotice30Handler();
void DestroyNotice28Handler();
void DestroyNotice29Handler();
void DestroyNotice32Handler();
void DestroyNotice31Handler();
void DestroyNotice36Handler();
void DestroyNotice35Handler();
void DestroyNotice37Handler();
void DestroyNotice38Handler();
void DestroyNotice39Handler();
void DestroyNotice40Handler();
void DestroyNotice33Handler();
void DestroyNotice34Handler();
void DestroyNotice41Handler();
void DestroyNotice42Handler();
void DestroyNotice43Handler();
void DestroyNotice45Handler();
void DestroyNotice44Handler();

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
    {&DAT_006b24ec, 0x01, Construct<Notice2>, DAT_006b24f0, DestroyNotice2Handler, 2, nullptr},
    {&DAT_006b24ec, 0x02, Construct<Notice1>, DAT_006b2358, DestroyNotice1Handler, 1, nullptr},
    {&DAT_006b24ec, 0x04, Construct<Notice8>, DAT_006b1b68, DestroyNotice8Handler, -1, KindOf<Notice8>},
    {&DAT_006b24ec, 0x08, Construct<Notice6>, DAT_006b16a0, DestroyNotice6Handler, 6, nullptr},
    {&DAT_006b24ec, 0x10, Construct<Notice5>, DAT_006b1bc8, DestroyNotice5Handler, 5, nullptr},
    {&DAT_006b24ec, 0x20, Construct<Notice4>, DAT_006b2060, DestroyNotice4Handler, 4, nullptr},
    {&DAT_006b24ec, 0x40, Construct<Notice3>, DAT_006b21b0, DestroyNotice3Handler, 3, nullptr},
    {&DAT_006b24ec, 0x80, Construct<Notice9>, DAT_006b2830, DestroyNotice9Handler, 9, nullptr},
    {&DAT_006b169c, 0x01, Construct<Notice10>, DAT_006b2570, DestroyNotice10Handler, 10, nullptr},
    {&DAT_006b169c, 0x02, Construct<Notice11>, DAT_006b1e70, DestroyNotice11Handler, 11, nullptr},
    {&DAT_006b169c, 0x04, Construct<Notice12>, DAT_006b1878, DestroyNotice12Handler, 12, nullptr},
    {&DAT_006b169c, 0x08, Construct<Notice13>, DAT_006b2218, DestroyNotice13Handler, 13, nullptr},
    {&DAT_006b169c, 0x10, Construct<Notice14>, DAT_006b1770, DestroyNotice14Handler, 14, nullptr},
    {&DAT_006b169c, 0x20, Construct<Notice16>, DAT_006b14e8, DestroyNotice16Handler, 16, nullptr},
    {&DAT_006b169c, 0x40, Construct<Notice15>, DAT_006b2788, DestroyNotice15Handler, 15, nullptr},
    {&DAT_006b169c, 0x80, Construct<Notice7>, DAT_006b1950, DestroyNotice7Handler, 7, nullptr},
    {&DAT_006b1af8, 0x01, Construct<Notice19>, DAT_006b1a20, DestroyNotice19Handler, 19, nullptr},
    {&DAT_006b1af8, 0x02, Construct<Notice20>, DAT_006b2130, DestroyNotice20Handler, 20, nullptr},
    {&DAT_006b1af8, 0x04, Construct<Notice21>, DAT_006b1d98, DestroyNotice21Handler, 21, nullptr},
    {&DAT_006b1af8, 0x08, Construct<Notice22>, DAT_006b1c30, DestroyNotice22Handler, -1, KindOf<Notice22>},
    {&DAT_006b1af8, 0x10, Construct<Notice23>, DAT_006b17e0, DestroyNotice23Handler, -1, KindOf<Notice23>},
    {&DAT_006b1af8, 0x20, Construct<Notice24>, DAT_006b2720, DestroyNotice24Handler, 24, nullptr},
    {&DAT_006b1af8, 0x40, Construct<Notice25>, DAT_006b1708, DestroyNotice25Handler, 25, nullptr},
    {&DAT_006b1af8, 0x80, Construct<Notice26b>, DAT_006b2488, DestroyNotice26bHandler, 26, nullptr},
    {&DAT_006b1c2c, 0x01, Construct<Notice26a>, DAT_006b26b8, DestroyNotice26aHandler, 26, nullptr},
    {&DAT_006b1c2c, 0x02, Construct<Notice30>, DAT_006b20c8, DestroyNotice30Handler, 30, nullptr},
    {&DAT_006b1c2c, 0x04, Construct<Notice28>, DAT_006b1e08, DestroyNotice28Handler, 28, nullptr},
    {&DAT_006b1c2c, 0x08, Construct<Notice29>, DAT_006b1b00, DestroyNotice29Handler, 29, nullptr},
    {&DAT_006b1c2c, 0x10, Construct<Notice32>, DAT_006b25d8, DestroyNotice32Handler, 32, nullptr},
    {&DAT_006b1c2c, 0x20, Construct<Notice31>, DAT_006b1a90, DestroyNotice31Handler, 31, nullptr},
    {&DAT_006b1c2c, 0x40, Construct<Notice36>, DAT_006b18e0, DestroyNotice36Handler, 36, nullptr},
    {&DAT_006b1c2c, 0x80, Construct<Notice35>, DAT_006b1d30, DestroyNotice35Handler, 35, nullptr},
    {&DAT_006b22ec, 0x01, Construct<Notice37>, DAT_006b2650, DestroyNotice37Handler, 37, nullptr},
    {&DAT_006b22ec, 0x02, Construct<Notice38>, DAT_006b19b8, DestroyNotice38Handler, 38, nullptr},
    {&DAT_006b22ec, 0x04, Construct<Notice39>, DAT_006b2288, DestroyNotice39Handler, 39, nullptr},
    {&DAT_006b22ec, 0x08, Construct<Notice40>, DAT_006b1630, DestroyNotice40Handler, 40, nullptr},
    {&DAT_006b22ec, 0x10, Construct<Notice33>, DAT_006b15c0, DestroyNotice33Handler, 33, nullptr},
    {&DAT_006b22ec, 0x20, Construct<Notice34>, DAT_006b1cc8, DestroyNotice34Handler, 34, nullptr},
    {&DAT_006b22ec, 0x40, Construct<Notice41>, DAT_006b2420, DestroyNotice41Handler, 41, nullptr},
    {&DAT_006b22ec, 0x80, Construct<Notice42>, DAT_006b22f0, DestroyNotice42Handler, 42, nullptr},
    {&DAT_006b1ed4, 0x01, Construct<Notice43>, DAT_006b1ff8, DestroyNotice43Handler, 43, nullptr},
    {&DAT_006b1ed4, 0x02, Construct<Notice45>, DAT_006b1ed8, DestroyNotice45Handler, 45, nullptr},
    {&DAT_006b1ed4, 0x04, Construct<Notice44>, DAT_006b1558, DestroyNotice44Handler, 44, nullptr},
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

void DestroyLastShipType();
void DestroyLastTroopType();
void DestroyLastFacilityType();

void StaticInitGlobalA() {
    InitGlobalA();
    RegisterGlobalADestructor();
}
void InitGlobalA() { g_lastShipType.m_value = 0; }
void RegisterGlobalADestructor() { atexit(DestroyLastShipType); }
void StaticInitGlobalB() {
    InitGlobalB();
    RegisterGlobalBDestructor();
}
void InitGlobalB() { g_lastTroopType.m_value = 0; }
void RegisterGlobalBDestructor() { atexit(DestroyLastTroopType); }
void StaticInitGlobalC() {
    InitGlobalC();
    RegisterGlobalCDestructor();
}
void InitGlobalC() { g_lastFacilityType.m_value = 0; }
void RegisterGlobalCDestructor() { atexit(DestroyLastFacilityType); }

// ---- destructors registered with atexit ----------------------------------------------------

extern uint8_t g_lastShipTypeDestroyedGuard;  // 0x6b28a8

void DestroyLastShipType() {
    if (!(g_lastShipTypeDestroyedGuard & 0x1)) {
        g_lastShipTypeDestroyedGuard |= 0x1;
        g_lastShipType.~NullableKey();
    }
}

void DestroyLastTroopType() {
    if (!(g_lastShipTypeDestroyedGuard & 0x2)) {
        g_lastShipTypeDestroyedGuard |= 0x2;
        g_lastTroopType.~NullableKey();
    }
}

void DestroyLastFacilityType() {
    if (!(g_lastShipTypeDestroyedGuard & 0x4)) {
        g_lastShipTypeDestroyedGuard |= 0x4;
        g_lastFacilityType.~NullableKey();
    }
}

// ---- destructors of the handler singletons registered with atexit ---------------------------------------------

namespace {

template <class T>
void DestroyHandler(uint8_t* object) {
    reinterpret_cast<T*>(object)->~T();
}

}  // namespace

void DestroyNotice44Handler() { DestroyHandler<Notice44>(DAT_006b1558); }
void DestroyNotice45Handler() { DestroyHandler<Notice45>(DAT_006b1ed8); }
void DestroyNotice43Handler() { DestroyHandler<Notice43>(DAT_006b1ff8); }
void DestroyNotice42Handler() { DestroyHandler<Notice42>(DAT_006b22f0); }
void DestroyNotice41Handler() { DestroyHandler<Notice41>(DAT_006b2420); }
void DestroyNotice34Handler() { DestroyHandler<Notice34>(DAT_006b1cc8); }
void DestroyNotice33Handler() { DestroyHandler<Notice33>(DAT_006b15c0); }
void DestroyNotice40Handler() { DestroyHandler<Notice40>(DAT_006b1630); }
void DestroyNotice39Handler() { DestroyHandler<Notice39>(DAT_006b2288); }
void DestroyNotice38Handler() { DestroyHandler<Notice38>(DAT_006b19b8); }
void DestroyNotice37Handler() { DestroyHandler<Notice37>(DAT_006b2650); }
void DestroyNotice35Handler() { DestroyHandler<Notice35>(DAT_006b1d30); }
void DestroyNotice36Handler() { DestroyHandler<Notice36>(DAT_006b18e0); }
void DestroyNotice31Handler() { DestroyHandler<Notice31>(DAT_006b1a90); }
void DestroyNotice32Handler() { DestroyHandler<Notice32>(DAT_006b25d8); }
void DestroyNotice29Handler() { DestroyHandler<Notice29>(DAT_006b1b00); }
void DestroyNotice28Handler() { DestroyHandler<Notice28>(DAT_006b1e08); }
void DestroyNotice30Handler() { DestroyHandler<Notice30>(DAT_006b20c8); }
void DestroyNotice26aHandler() { DestroyHandler<Notice26a>(DAT_006b26b8); }
void DestroyNotice26bHandler() { DestroyHandler<Notice26b>(DAT_006b2488); }
void DestroyNotice25Handler() { DestroyHandler<Notice25>(DAT_006b1708); }
void DestroyNotice24Handler() { DestroyHandler<Notice24>(DAT_006b2720); }
void DestroyNotice23Handler() { DestroyHandler<Notice23>(DAT_006b17e0); }
void DestroyNotice22Handler() { DestroyHandler<Notice22>(DAT_006b1c30); }
void DestroyNotice21Handler() { DestroyHandler<Notice21>(DAT_006b1d98); }
void DestroyNotice20Handler() { DestroyHandler<Notice20>(DAT_006b2130); }
void DestroyNotice19Handler() { DestroyHandler<Notice19>(DAT_006b1a20); }
void DestroyNotice7Handler() { DestroyHandler<Notice7>(DAT_006b1950); }
void DestroyNotice15Handler() { DestroyHandler<Notice15>(DAT_006b2788); }
void DestroyNotice16Handler() { DestroyHandler<Notice16>(DAT_006b14e8); }
void DestroyNotice14Handler() { DestroyHandler<Notice14>(DAT_006b1770); }
void DestroyNotice13Handler() { DestroyHandler<Notice13>(DAT_006b2218); }
void DestroyNotice12Handler() { DestroyHandler<Notice12>(DAT_006b1878); }
void DestroyNotice11Handler() { DestroyHandler<Notice11>(DAT_006b1e70); }
void DestroyNotice10Handler() { DestroyHandler<Notice10>(DAT_006b2570); }
void DestroyNotice9Handler() { DestroyHandler<Notice9>(DAT_006b2830); }
void DestroyNotice3Handler() { DestroyHandler<Notice3>(DAT_006b21b0); }
void DestroyNotice4Handler() { DestroyHandler<Notice4>(DAT_006b2060); }
void DestroyNotice5Handler() { DestroyHandler<Notice5>(DAT_006b1bc8); }
void DestroyNotice6Handler() { DestroyHandler<Notice6>(DAT_006b16a0); }
void DestroyNotice8Handler() { DestroyHandler<Notice8>(DAT_006b1b68); }
void DestroyNotice1Handler() { DestroyHandler<Notice1>(DAT_006b2358); }
void DestroyNotice2Handler() { DestroyHandler<Notice2>(DAT_006b24f0); }
