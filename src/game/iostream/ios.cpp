#include "iostream/iostream.h"

extern int g_iosInstanceCount;
extern CRITICAL_SECTION g_iosStaticLock;

void mtx_init(void* criticalSection) { InitializeCriticalSection(static_cast<LPCRITICAL_SECTION>(criticalSection)); }
void mtx_term(void* criticalSection) { DeleteCriticalSection(static_cast<LPCRITICAL_SECTION>(criticalSection)); }
void mtx_lock(void* criticalSection) { EnterCriticalSection(static_cast<LPCRITICAL_SECTION>(criticalSection)); }
void mtx_unlock(void* criticalSection) { LeaveCriticalSection(static_cast<LPCRITICAL_SECTION>(criticalSection)); }

ios::ios() { ios_ctor(this); }
ios::~ios() { Cleanup(); }

void ios::Initialize(streambuf* buffer) {
    if (!m_lockInitialized) ios_ctor(this);
    ios_init(this, buffer);
}

void ios::Cleanup() {
    if (!m_lockInitialized) return;
    m_lockCount = -1;
    if (g_iosInstanceCount > 0 && --g_iosInstanceCount == 0) mtx_term(&g_iosStaticLock);
    m_lockInitialized = false;
    mtx_term(&m_lock);
    if (!m_bufferReleased && m_delbuf && m_bp) {
        m_bufferReleased = true;
        delete m_bp;
    }
    m_bp = nullptr;
    m_state = iostream_flags::kBad;
    m_delbuf = 0;
}

ios* ios_ctor(ios* self) {
    if (self->m_lockInitialized) return self;
    self->m_bp = nullptr;
    self->m_ispecial = 0;
    self->m_ospecial = 0;
    self->m_isfxSpecial = 0;
    self->m_osfxSpecial = 0;
    self->m_tie = nullptr;
    self->m_flags = 0;
    self->m_width = 0;
    self->m_delbuf = 0;
    self->m_state = iostream_flags::kBad;
    self->m_precision = 6;
    self->m_fill = ' ';
    self->m_lockCount = -1;
    self->m_bufferReleased = false;
    mtx_init(&self->m_lock);
    self->m_lockInitialized = true;
    if (g_iosInstanceCount++ == 0) mtx_init(&g_iosStaticLock);
    return self;
}

void ios_dtor(ios* self) { self->Cleanup(); }

void ios_init(ios* self, streambuf* buffer) {
    if (self->m_delbuf && self->m_bp && self->m_bp != buffer) delete self->m_bp;
    self->m_bufferReleased = false;
    self->m_bp = buffer;
    if (buffer) self->m_state &= ~iostream_flags::kBad;
    else self->m_state |= iostream_flags::kBad;
}

int g_iosInstanceCount{};
CRITICAL_SECTION g_iosStaticLock{};
