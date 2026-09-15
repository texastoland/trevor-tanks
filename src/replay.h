#pragma once
#include "game.h"
#include <string>

// A replay is one mission attempt: the exact starting world state (including
// death-retry carryover), the player-stat override if any, and every
// per-subtick input. The simulation is deterministic, so this reproduces the
// attempt perfectly.
struct ReplayTick { uint8_t buttons = 0; float aimX = 0, aimY = 0; };

struct Replay {
    int mission = 0;
    int arenaW = 0, arenaH = 0;
    std::vector<uint8_t> blocks;      // arena cells at attempt start
    std::vector<uint8_t> alive;       // per-tank alive flags at attempt start
    bool hasOverride = false;
    EnemyParams override_{};
    std::vector<ReplayTick> ticks;
};

void replayCaptureStart(Replay& r, int mission, const World& w, const EnemyParams* ov);
void replayRecordTick(Replay& r, const InputState& in);
InputState replayInput(const Replay& r, size_t i);
World replayStartWorld(const Replay& r);
bool saveReplay(const Replay& r, const std::string& path);
bool loadReplay(Replay& r, const std::string& path);
