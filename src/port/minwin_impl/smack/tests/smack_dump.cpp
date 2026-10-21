// smack_dump <out.txt> <maxFrames> [--frame N --frame-file F] <movie.smk>...
// Plays each movie through the Smacker API and writes one line per frame (see smack_ops.cpp).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smack_ops.h"
#include "smack_table.h"

int main(int argc, char** argv)
{
  const char* frameFile = nullptr;
  int dumpFrame = -1;
  std::vector<const char*> positional;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--frame") && i + 1 < argc) {
      dumpFrame = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--frame-file") && i + 1 < argc) {
      frameFile = argv[++i];
    } else {
      positional.push_back(argv[i]);
    }
  }
  if (positional.size() < 3) {
    fprintf(stderr, "usage: smack_dump out.txt maxFrames movie...\n");
    return 2;
  }
  SmackApiTable api;
  if (!LoadSmackApi(&api)) {
    fprintf(stderr, "cannot load the Smacker API\n");
    return 1;
  }
  FILE* out = fopen(positional[0], "wb");
  if (!out) {
    return 1;
  }
  int maxFrames = atoi(positional[1]);
  for (size_t i = 2; i < positional.size(); ++i) {
    fprintf(out, "== %s\n", positional[i]);
    if (!RunSmackScenario(api, positional[i], maxFrames, dumpFrame, frameFile, out)) {
      return 1;
    }
  }
  fclose(out);
  return 0;
}
