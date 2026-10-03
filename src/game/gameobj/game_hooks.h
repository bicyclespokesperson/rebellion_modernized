#pragma once
#include <cstdint>

#include "core/layout.h"
#include "core/stream_io.h"

// Hooks through which the object model reaches the strategic game layer. GameObj's setup code (0x4fcde0) installs
// the callbacks; until then every wrapper does nothing and returns its default. The table's contents are only known by
// address (the callbacks are tiny strategic-layer helpers), so the globals and wrappers are named after it.
extern int (*g_gameHook2AD4)(int);
extern void (*g_gameHook2AD8)(int, int);
extern void (*g_gameHook2AE0)(int);
extern const char* (*g_gameHook2AE8)();
extern int (*g_gameHook2AEC)();
extern int (*g_gameHook2AF0)();
extern int (*g_gameHook2AF4)();
extern void (*g_gameHook2AF8)(int);
extern int (*g_gameHook2AFC)();
extern void (*g_gameHook2B00)(int);
extern int (*g_gameHook2B04)();
extern int (*g_gameHook2B08)();

int CallGameHook2AF4();
int CallGameHook2AFC();
void CallGameHook2AE0(int arg);
// Directory of the game's data files (null if no hook is installed).
const char* GetGameDataDirectory();
int CallGameHook2B04();
int CallGameHook2B08();
void CallGameHook2AD8(int a, int b);
void CallGameHook2B00(int arg);
void CallGameHook2AF8(int arg);
int CallGameHook2AD4(int arg);
// Returns 1 when no hook is installed.
int CallGameHook2AF0();
// The id handed to objects that still have the default id 1 (see GameObjRoot::ApplyDefaultId).
int CallIdHook();

// Asks the rules object for a value (0x555e60 / 0x5560b0 on the object at g_gameState+0xbc); *out is 0 if there is none.
int QueryRuleValueA(int a, int b, int* out);
int QueryRuleValueB(int a, int b, int* out);

// The "who gets told" settings every setter receives (the `context` argument of the validated setters): per-view
// notify switches plus two reserved ints. GetViewFlag/SetViewFlag take the object's ViewSide (1 or 2).
class NotifyContext {
public:
    NotifyContext();
    NotifyContext(const NotifyContext& other);
    virtual ~NotifyContext();

    NotifyContext& operator=(const NotifyContext& other);
    int Read(istream* in);
    int Write(ostream* out);
    int Checksum(class Crc32* crc);
    int GetViewFlag(int view) const;
    void SetViewFlag(int view, int value);

    int m_flag04;      // +0x04
    int m_flag08;      // +0x08
    int m_notifyView1;  // +0x0c
    int m_notifyView2;  // +0x10
    int m_reserved14;  // +0x14
    int m_reserved18;  // +0x18
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(NotifyContext, 0x1c);
REB_ASSERT_OFFSET(NotifyContext, m_notifyView1, 0x0c);
REB_ASSERT_OFFSET(NotifyContext, m_reserved18, 0x18);
#pragma clang diagnostic pop
