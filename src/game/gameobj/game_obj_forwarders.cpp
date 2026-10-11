#include "gameobj/game_obj_forwarders.h"
#include "gameobj/game_obj.h"

// The tiny shared bodies the leaf classes' Read / Write / Checksum / ApplyAll slots forward to (0x569840-0x5698b0,
// 0x4f29d0): the linker folded them over many classes, so they have no owner. Each calls the base body of GameObj.

int GameObjReadForward(GameObj* self, istream* in)
{
  return self->GameObj::Read(in);
} // 0x569890

int GameObjWriteForward(GameObj* self, ostream* out)
{
  return self->GameObj::Write(out);
} // 0x4f29d0

int GameObjChecksumForward(GameObj* self, Crc32* crc)
{
  return self->GameObj::Checksum(crc);
} // 0x5698b0

int GameObjApplyAllForward(GameObj* self, NotifyContext* ctx)
{
  return self->GameObj::ApplyAll(ctx);
} // 0x5698a0

int LeafReadForward(GameObj* self, istream* in)
{
  return GameObjReadForward(self, in);
} // 0x569840

int LeafWriteForward(GameObj* self, ostream* out)
{
  return GameObjWriteForward(self, out);
} // 0x569850

int LeafChecksumForward(GameObj* self, Crc32* crc)
{
  return GameObjChecksumForward(self, crc);
} // 0x569860
