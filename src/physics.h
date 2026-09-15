#pragma once
#include "game.h"

bool circleHitsGrid(const Arena& a, Vec2 c, float r);
void moveTank(Tank& t, Vec2 desiredDir, float speed, const Arena& a, float dt);
void separateTanks(std::vector<Tank>& tanks, const Arena& a);

struct SweepHit {
    bool hit = false;
    Vec2 pos, normal;   // normal {0,0} => started inside solid
    int cx = -1, cy = -1;
};
SweepHit sweepSegment(const Arena& a, Vec2 from, Vec2 to);
float segPointDist(Vec2 a, Vec2 b, Vec2 p);
