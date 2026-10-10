#pragma once
// Headless simulation harness of the standalone rebellion.exe (docs/phase3_plan.md, P0a). Built into the standalone only.
//   REB_SIM=load:<savefile>,ticks:<N>,save:<path>[,every:<k>][,log:<path>]
// WinMain calls RebSimRun instead of RebellionApplication::Run when the variable is set.
class RebellionApplication;

bool RebSimRequested();
int RebSimRun(RebellionApplication* app);
