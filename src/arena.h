#pragma once
#include "game.h"

// Level legend: '#' wall, 'x' crate, 'o' hole (blocks tanks, not shells), '.' empty,
// 'P' player spawn, enemies: B=Brown G=Grey T=Teal Y=Yellow R=Red N=Green U=Purple W=White K=Black
struct LevelDef { std::vector<std::string> rows; };
struct ParsedLevel {
    Arena arena;
    Vec2 playerSpawn;
    std::vector<std::pair<TankType, Vec2>> enemies;
};

bool charToEnemy(char c, TankType& out);
std::string validateLevel(const LevelDef& def);  // "" if valid, else error message
ParsedLevel parseLevel(const LevelDef& def);     // call only on validated levels
World makeWorld(const LevelDef& def);            // parse + spawn all tanks
void spawnEnemy(World& w, TankType type, Vec2 pos, Vec2 playerSpawn);
void carryOverDestruction(const Arena& prev, Arena& next);  // destroyed crates stay destroyed on retry
void carryOverKills(const World& prev, World& next);        // destroyed enemies stay destroyed on retry
