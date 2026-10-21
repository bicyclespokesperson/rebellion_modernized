#pragma once
// Headless simulation harness of the native executable.
//   REB_SIM=load:<savefile>,ticks:<N>,save:<path>[,every:<k>][,log:<path>]
// WinMain calls RebSimRun instead of RebellionApplication::Run when the variable is set, then follows the normal
// battle-flow, application and process cleanup path.
class RebellionApplication;

bool RebSimRequested();
int RebSimRun(RebellionApplication* app);
