#include "iostream/iostream.h"

void mtx_lock(std::recursive_mutex* mutex)
{
  mutex->lock();
}
void mtx_unlock(std::recursive_mutex* mutex)
{
  mutex->unlock();
}

ios::ios()
{
  ios_ctor(this);
}
ios::~ios()
{
  Cleanup();
}

void ios::Initialize(streambuf* buffer)
{
  if (!m_lockInitialized) {
    ios_ctor(this);
  }
  ios_init(this, buffer);
}

void ios::Cleanup()
{
  if (!m_lockInitialized) {
    return;
  }
  m_lockCount = -1;
  m_lockInitialized = false;
  if (!m_bufferReleased && m_delbuf && m_bp) {
    m_bufferReleased = true;
    delete m_bp;
  }
  m_bp = nullptr;
  m_state = iostream_flags::kBad;
  m_delbuf = 0;
}

ios* ios_ctor(ios* self)
{
  if (self->m_lockInitialized) {
    return self;
  }
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
  self->m_lockInitialized = true;
  return self;
}

void ios_dtor(ios* self)
{
  self->Cleanup();
}

void ios_init(ios* self, streambuf* buffer)
{
  if (self->m_delbuf && self->m_bp && self->m_bp != buffer) {
    delete self->m_bp;
  }
  self->m_bufferReleased = false;
  self->m_bp = buffer;
  if (buffer) {
    self->m_state &= ~iostream_flags::kBad;
  } else {
    self->m_state |= iostream_flags::kBad;
  }
}

