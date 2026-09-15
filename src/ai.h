#pragma once
#include "game.h"
#include "combat.h"

std::vector<Vec2> findPath(const Arena& a, Vec2 from, Vec2 to,
                           const std::vector<Vec2>& avoid = {}, float avoidRadius = 0);
Intent aiThink(World& w, int ti, float dt);
bool hasDirectShot(const Arena& a, Vec2 from, Vec2 target);
bool findRicochetAim(const World& w, int shooter, Vec2 target, int bounces, float range, float& outAngle, bool acceptPlayerHit = true);
bool pathHitsFriendly(const World& w, int shooter, float angle, int bounces, float range);
