#pragma once
#include "arena.h"

int levelCount();
const LevelDef& level(int idx);   // 0-based; idx in [0, levelCount())

// the full 100-mission game: 0-19 handcrafted; 20+ deterministically generated
// from earlier arenas per the original's documented rules
constexpr int MISSION_COUNT = 100;
World missionWorld(int m);        // 0-based mission index
