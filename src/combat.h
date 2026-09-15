#pragma once
#include "game.h"

struct Intent {
    Vec2 move;
    float aim = 0;
    bool fire = false, layMine = false;
};

const EnemyParams& paramsFor(TankType t);
void setPlayerOverride(const EnemyParams& p);   // secret tank-tuning panel
void clearPlayerOverride();
bool tryFire(World& w, int ti);
void killTank(World& w, int ti);
void killShell(World& w, Shell& s, bool poof);
void updateShells(World& w, float dt);
bool tryLayMine(World& w, int ti);
void explodeMine(World& w, Mine& m);
void updateMines(World& w, float dt);
void applyIntents(World& w, const std::vector<Intent>& intents, float dt);
void simTick(World& w, const InputState& input, float dt);
