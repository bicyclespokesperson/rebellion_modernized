#pragma once
// Checks and execution of an order's command queue, and small object checks shared by the orders
// (0x553350-0x553c80, move_route_validation module).
#include "core/game_string.h"
#include "gameobj/child_iterator.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"
#include "orders/command_decl.h"

// Sets *anyChecked when some command reports itself checked (slot 5) and *anyUnchecked when some does not. Returns 1.
int ClassifyCommands(ActivityRequest* request, CommandQueue* commands, int* anyChecked, int* anyUnchecked); // 0x553720
// Runs every command's enabled check (slot 6), then requires that all agree on the side.
int CheckCommandsEnabled(ActivityRequest* request, CommandQueue* commands, StatusCode* status); // 0x553770
// Runs every command's validation (slot 7) while the status stays ok.
int CheckCommandsValid(ActivityRequest* request, CommandQueue* commands, StatusCode* status); // 0x553960
// Takes every command out of the queue, gives it the request's argument and hands it to the game (hook 0x2b00).
int ExecuteCommands(ActivityRequest* request, CommandQueue* commands); // 0x5539f0
// Empties `commands`, then queues one copy of `prototype` for every object `team` depends on.
int CheckCommandForTeam(Command* prototype,
                        ObjectKeyValueSet* team,
                        CommandQueue* commands,
                        StatusCode* status); // 0x5535b0

// {1, 0x25} and {1, 0x27} become {1, 0x26} when the request's target is one of the team.
int RefuseDestinationInTeam(const ActivityRequest* request, ObjectKeySet* team, StatusCode* status); // 0x553aa0
// The side that controls the first command's object; 0 if the queue is empty or the side is not 1-3.
int GetCommandSide(CommandQueue* commands, uint32_t* side); // 0x553b80

// {1, 0x24} unless `object` is controlled by `side`; with `checkOtherSide` also {1, 0x28} unless by `otherSide`; then
// {1, 0x20} (only with `requireCompleted`), {1, 0x22} or {1, 0x21} for an uncompleted, destroyed or travelling object.
int CheckControlAndState(GameObj* object,
                         uint32_t side,
                         uint32_t otherSide,
                         int requireCompleted,
                         int checkOtherSide,
                         StatusCode* status); // 0x553410
// Adds the key of every child the iterator walks (only those of `side` with `onlySide`) to `out`.
int AddChildKeys(ChildIterator* children, ObjectKeyValueSet* out, int onlySide, uint32_t side); // 0x553350
GameObjNotifier* GetMasterObjectForKey(const ObjectKey* key); // 0x553400
// "<name> (ID=n)", or a description of what is missing, replacing the contents of *out.
void AppendObjectDescription(ObjectKey* key, std::string* out); // 0x553c80
