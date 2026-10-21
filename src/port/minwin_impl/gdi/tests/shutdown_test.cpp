#include "port/minwin/gdi32.h"

#include <stdio.h>
#include <stdlib.h>

namespace
{
struct GlobalGraphicsOwner {
  HBRUSH brush = nullptr;

  ~GlobalGraphicsOwner()
  {
    if (!reb::compat::DeleteObject(brush)) {
      fprintf(stderr, "Failed to delete a graphics object during static cleanup\n");
      _Exit(1);
    }
  }
};

GlobalGraphicsOwner owner;
} // namespace

int main()
{
  owner.brush = reb::compat::CreateSolidBrush(0x123456);
  return owner.brush ? 0 : 1;
}
