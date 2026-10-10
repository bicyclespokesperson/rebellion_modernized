#include "core/reb_abi.h"
#include "iostream/iostream.h"

#include "platform/win32.h"

extern int g_iosInstanceCount;         // 0x6be740 number of live ios objects
extern uint8_t g_iosStaticLock[kCriticalSectionBytes];  // 0x6be708 CRITICAL_SECTION shared by every ios

void REB_CDECL mtx_init(void* criticalSection) { InitializeCriticalSection(static_cast<LPCRITICAL_SECTION>(criticalSection)); }

void REB_CDECL mtx_term(void* criticalSection) { DeleteCriticalSection(static_cast<LPCRITICAL_SECTION>(criticalSection)); }

void REB_CDECL mtx_lock(void* criticalSection) { EnterCriticalSection(static_cast<LPCRITICAL_SECTION>(criticalSection)); }

void REB_CDECL mtx_unlock(void* criticalSection) { LeaveCriticalSection(static_cast<LPCRITICAL_SECTION>(criticalSection)); }

ios* REB_THISCALL ios_ctor(ios* self) {
    self->m_bp = nullptr;
    self->m_ispecial = 0;
    self->m_ospecial = 0;
    self->m_tie = nullptr;
    self->m_flags = 0;
    self->m_width = 0;
    self->m_delbuf = 0;
    self->m_vftable = &ios::vftable;
    self->m_state = iostream_flags::kBad;
    self->m_precision = 6;
    self->m_fill = ' ';
    self->m_lockCount = -1;
    mtx_init(self->m_lock);
    if (g_iosInstanceCount++ == 0) mtx_init(g_iosStaticLock);
    return self;
}

void REB_THISCALL ios_dtor(ios* self) {
    self->m_vftable = &ios::vftable;
    self->m_lockCount = -1;
    if (--g_iosInstanceCount == 0) mtx_term(g_iosStaticLock);
    mtx_term(self->m_lock);
    if (self->m_delbuf != 0 && self->m_bp != nullptr) self->m_bp->m_vftable->m_scalarDeletingDtor(self->m_bp, 1);
    self->m_bp = nullptr;
    self->m_state = iostream_flags::kBad;
}

void REB_THISCALL ios_init(ios* self, streambuf* buffer) {
    if (self->m_delbuf != 0 && self->m_bp != nullptr) self->m_bp->m_vftable->m_scalarDeletingDtor(self->m_bp, 1);
    self->m_bp = buffer;
    if (buffer != nullptr)
        self->m_state &= ~iostream_flags::kBad;
    else
        self->m_state |= iostream_flags::kBad;
}
