#pragma once

// Side 1 is the Alliance and side 2 is the Empire. An absent or unknown preference uses Classic.
int PreferredAiKind(int side);
bool SetPreferredAiKind(int side, int kind);
