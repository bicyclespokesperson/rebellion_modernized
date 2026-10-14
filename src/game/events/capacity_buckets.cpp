#include "gameobj/status_code.h"

// The nine family ranges the capacity buckets stand for (orders/route_capacity.h). The original rebuilds the whole
// table on every call, and clears a word at 0x6aafec first (nothing else reads it).
extern FamilyRange g_capacityBucketRanges[9]; // 0x6bb2f0
extern int g_capacityBucketWord; // 0x6aafec

const FamilyRange* GetCapacityBucketRange(int index)
{
  g_capacityBucketWord = 0;
  g_capacityBucketRanges[0] = FamilyRange(0x14, 0x18);
  g_capacityBucketRanges[1] = FamilyRange(0x18, 0x1c);
  g_capacityBucketRanges[2] = FamilyRange(0x1c, 0x20);
  g_capacityBucketRanges[3] = FamilyRange(0x20, 0x30);
  g_capacityBucketRanges[4] = FamilyRange(0x2c, 0x2d);
  g_capacityBucketRanges[5] = FamilyRange(0x30, 0x40);
  g_capacityBucketRanges[6] = FamilyRange(0x10, 0x14);
  g_capacityBucketRanges[7] = FamilyRange(8, 0x10);
  g_capacityBucketRanges[8] = FamilyRange(0x40, 0x80);
  return &g_capacityBucketRanges[index];
}

FamilyRange g_capacityBucketRanges[9]{};
int g_capacityBucketWord = 1;
