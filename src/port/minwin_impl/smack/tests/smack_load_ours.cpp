// Binds the Smacker API table to minwin_impl/smack.
#include "../smack_api.h"
#include "smack_ops.h"
#include "smack_table.h"

namespace
{
HWND MakeFakeWindow()
{
  return reinterpret_cast<HWND>(static_cast<uintptr_t>(0x1000));
}
} // namespace

bool LoadSmackApi(SmackApiTable* api)
{
  api->open = SmackOpen;
  api->close = SmackClose;
  api->doFrame = SmackDoFrame;
  api->nextFrame = SmackNextFrame;
  api->wait = SmackWait;
  api->gotoFrame = SmackGoto;
  api->toBuffer = SmackToBuffer;
  api->toBufferRect = SmackToBufferRect;
  api->volumePan = SmackVolumePan;
  api->bufferOpen = SmackBufferOpen;
  api->bufferClose = SmackBufferClose;
  api->bufferNewPalette = SmackBufferNewPalette;
  api->bufferSetPalette = SmackBufferSetPalette;
  api->getTrackData = SmackGetTrackData;
  api->soundUseDirectSound = SmackSoundUseDirectSound;
  api->makeWindow = MakeFakeWindow;
  return true;
}
