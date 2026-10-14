#pragma once
#include "gameobj/child_iterator.h"

// Iterator over the missions of family 0x57 under a parent (state filter 1), with its own vtable holding just the destructor.

// Family 0x57 (MissionSubdue), VT 0x663e00.
class MissionSubdueIterator : public ChildIterator
{
public:
  explicit MissionSubdueIterator(GameObjNotifier* parent); // 0x569e10
  ~MissionSubdueIterator() override; // 0x569e90 (deleting), 0x569eb0
};

// Family 0x53 (MissionResearch), VT 0x665050 (the code sits after MissionResearch's, at the start of the story range).
class MissionResearchIterator : public ChildIterator
{
public:
  explicit MissionResearchIterator(GameObjNotifier* parent); // 0x56d1b0
  ~MissionResearchIterator() override; // 0x56d230 (deleting), 0x56d250
};

// Family 0x56 (MissionIncite), VT 0x666cf8.
class MissionInciteIterator : public ChildIterator
{
public:
  explicit MissionInciteIterator(GameObjNotifier* parent); // 0x571c20
  ~MissionInciteIterator() override; // 0x571ca0 (deleting), 0x571cc0
};
