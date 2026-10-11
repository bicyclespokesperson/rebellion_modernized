#include "gameobj/game_hooks.h"

#include "core/utility_classes.h"
#include "gameobj/object_queries.h"
#include "rules/rules_mgr.h"

// 0x506e60 returns the rules object (field +0xbc of the game-state global 0x6b2bb0), or null.

int QueryRuleValueA(GameObjNotifier* manager, StatusCode* status, int* out) {
    *out = 0;
    if (void* rules = GetGameRootMember_bc()) static_cast<RulesMgr*>(rules)->RulesMgr::QueryLastBuildDay(manager, status, out);
    return 1;
}

int QueryRuleValueB(GameObj* unit, StatusCode* status, int* out) {
    *out = 0;
    if (void* rules = GetGameRootMember_bc()) static_cast<RulesMgr*>(rules)->RulesMgr::QueryArrivalDay(unit, status, out);
    return 1;
}

int CallGameHook2AF4() {
    if (g_gameHook2AF4) return g_gameHook2AF4();
    return 0;
}

int CallGameHook2AFC() {
    if (g_gameHook2AFC) return g_gameHook2AFC();
    return 0;
}

void CallGameHook2AE0(HookArg arg) {
    if (g_gameHook2AE0) g_gameHook2AE0(arg);
}

const char* GetGameDataDirectory() {
    if (g_gameHook2AE8) return g_gameHook2AE8();
    return nullptr;
}

int CallGameHook2B04() {
    if (g_gameHook2B04) return g_gameHook2B04();
    return 0;
}

int CallGameHook2B08() {
    if (g_gameHook2B08) return g_gameHook2B08();
    return 0;
}

void CallGameHook2AD8(HookArg a, int b) {
    if (g_gameHook2AD8) g_gameHook2AD8(a, b);
}

void CallGameHook2B00(HookArg arg) {
    if (g_gameHook2B00) g_gameHook2B00(arg);
}

void CallGameHook2AF8(HookArg arg) {
    if (g_gameHook2AF8) g_gameHook2AF8(arg);
}

int CallGameHook2AD4(HookArg arg) {
    if (g_gameHook2AD4) return g_gameHook2AD4(arg);
    return 1;
}

int CallGameHook2AF0() {
    if (g_gameHook2AF0) return g_gameHook2AF0();
    return 1;
}

int CallIdHook() {
    if (g_gameHook2AEC) return g_gameHook2AEC();
    return 0;
}

// ---- NotifyContext -----------------------------------------------------------------------------------------

NotifyContext::NotifyContext()
    : m_flag04(1), m_flag08(1), m_notifyView1(1), m_notifyView2(1), m_reserved14(0), m_reserved18(0) {}

NotifyContext::NotifyContext(const NotifyContext& other)
    : m_flag04(other.m_flag04),
      m_flag08(other.m_flag08),
      m_notifyView1(other.m_notifyView1),
      m_notifyView2(other.m_notifyView2),
      m_reserved14(other.m_reserved14),
      m_reserved18(other.m_reserved18) {}

NotifyContext::~NotifyContext() {  }

NotifyContext& NotifyContext::operator=(const NotifyContext& other) {
    if (this != &other) {
        m_flag04 = other.m_flag04;
        m_flag08 = other.m_flag08;
        m_notifyView1 = other.m_notifyView1;
        m_notifyView2 = other.m_notifyView2;
        m_reserved14 = other.m_reserved14;
        m_reserved18 = other.m_reserved18;
    }
    return *this;
}

int NotifyContext::Read(istream* in) {
    ReadValue32(in, &m_flag04);
    ReadValue32(in, &m_flag08);
    ReadValue32(in, &m_notifyView1);
    ReadValue32(in, &m_notifyView2);
    ReadValue32(in, &m_reserved14);
    ReadValue32(in, &m_reserved18);
    return 1;
}

int NotifyContext::Write(ostream* out) {
    WriteValue32(out, &m_flag04);
    WriteValue32(out, &m_flag08);
    WriteValue32(out, &m_notifyView1);
    WriteValue32(out, &m_notifyView2);
    WriteValue32(out, &m_reserved14);
    WriteValue32(out, &m_reserved18);
    return 1;
}

int NotifyContext::Checksum(Crc32* crc) {
    crc->AddInt(m_flag04);
    crc->AddInt(m_flag08);
    crc->AddInt(m_notifyView1);
    crc->AddInt(m_notifyView2);
    crc->AddInt(m_reserved14);
    crc->AddInt(m_reserved18);
    return 1;
}

int NotifyContext::GetViewFlag(int view) const {
    if (view == 1) return m_notifyView1;
    if (view == 2) return m_notifyView2;
    return 0;
}

void NotifyContext::SetViewFlag(int view, int value) {
    if (view == 1) {
        m_notifyView1 = value;
        return;
    }
    if (view == 2) m_notifyView2 = value;
}
