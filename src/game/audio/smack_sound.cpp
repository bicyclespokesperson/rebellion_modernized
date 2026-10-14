#include "audio/smack_sound.h"

extern "C" {
#if !defined(REB_NATIVE)
__declspec(dllimport) void SmackSoundUseDirectSound(void* directSound);
__declspec(dllimport) void SmackBufferClose(void* smackBuffer);
__declspec(dllimport) void SmackClose(void* smack);
#else
void SmackSoundUseDirectSound(void* directSound);
void SmackBufferClose(void* smackBuffer);
void SmackClose(void* smack);
#endif
}

SmackSound::SmackSound()
: m_smackParameter(0),
  m_smack(nullptr),
  m_smackBuffer(nullptr),
  m_window(nullptr),
  m_thread(nullptr),
  m_unk14(0),
  m_unk18(0),
  m_unk1c(0),
  m_unk20(0)
{
}

SmackSound::~SmackSound()
{
  Stop(1);
}

void SmackSound::Close()
{
  if (m_smackBuffer)
  {
    SmackBufferClose(m_smackBuffer);
    m_smackBuffer = nullptr;
  }
  if (m_smack)
  {
    SmackClose(m_smack);
    m_smack = nullptr;
  }
}

void SmackSound::Share(HWND window, int smackParameter, void* directSound)
{
  m_smackParameter = smackParameter;
  m_window = window;
  SmackSoundUseDirectSound(directSound);
}

void SmackSound::Stop(int closeHandles)
{
  if (m_thread)
  {
    TerminateThread(m_thread, 0);
    m_thread = nullptr;
    m_unk14 = 0;
  }
  if (closeHandles == 1 && m_smack)
    Close();
  m_unk18 = 0;
}
