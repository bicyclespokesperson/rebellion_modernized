#pragma once
// Checks and execution of an order's command queue (0x553410-0x553b80, move_route_validation module).
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"
#include "orders/command_decl.h"

// Sets *anyChecked when some command reports itself checked (slot 5) and *anyUnchecked when some does not. Returns 1.
int ClassifyCommands(ActivityRequest* request, CommandQueue* commands, int* anyChecked, int* anyUnchecked);  // 0x553720
// Runs every command's enabled check (slot 6), then requires that all agree on the side.
int CheckCommandsEnabled(ActivityRequest* request, CommandQueue* commands, StatusCode* status);  // 0x553770
// Runs every command's validation (slot 7) while the status stays ok.
int CheckCommandsValid(ActivityRequest* request, CommandQueue* commands, StatusCode* status);  // 0x553960
// Pops every command, gives it the request and the current time, runs it and hands it on.
int ExecuteCommands(ActivityRequest* request, CommandQueue* commands);  // 0x5539f0
// Builds one command per team member from `prototype` and queues it.
int CheckCommandForTeam(Command* prototype, ObjectKeyValueSet* team, CommandQueue* commands, StatusCode* status);  // 0x5535b0
