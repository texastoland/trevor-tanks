#include "physics.h"

bool circleHitsGrid(const Arena& a, Vec2 c, float r) {
    int x0 = (int)std::floor(c.x - r), x1 = (int)std::floor(c.x + r);
    int y0 = (int)std::floor(c.y - r), y1 = (int)std::floor(c.y + r);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            if (!a.blocksTank(x, y)) continue;
            float px = clampf(c.x, (float)x, (float)x + 1);
            float py = clampf(c.y, (float)y, (float)y + 1);
            float dx = c.x - px, dy = c.y - py;
            if (dx * dx + dy * dy < r * r) return true;
        }
    }
    return false;
}

void moveTank(Tank& t, Vec2 desiredDir, float speed, const Arena& a, float dt) {
    float l = vlen(desiredDir);
    if (l > 1.0f) desiredDir = desiredDir * (1.0f / l);
    Vec2 delta = desiredDir * (speed * dt);
    // axis-separated movement => natural sliding along walls
    Vec2 tryX{t.pos.x + delta.x, t.pos.y};
    if (!circleHitsGrid(a, tryX, TANK_RADIUS)) t.pos.x = tryX.x;
    Vec2 tryY{t.pos.x, t.pos.y + delta.y};
    if (!circleHitsGrid(a, tryY, TANK_RADIUS)) t.pos.y = tryY.y;
    if (l > 0.01f) t.bodyAngle = std::atan2(desiredDir.y, desiredDir.x);
}

void separateTanks(std::vector<Tank>& tanks, const Arena& a) {
    for (size_t i = 0; i < tanks.size(); ++i) {
        for (size_t j = i + 1; j < tanks.size(); ++j) {
            Tank &A = tanks[i], &B = tanks[j];
            if (!A.alive || !B.alive) continue;
            Vec2 d = B.pos - A.pos;
            float dist = vlen(d);
            float minD = 2 * TANK_RADIUS;
            if (dist >= minD) continue;
            if (dist < 1e-4f) { d = {1, 0}; dist = 1e-4f; }
            Vec2 push = d * ((minD - dist) * 0.5f / dist);
            Vec2 an = A.pos - push, bn = B.pos + push;
            if (!circleHitsGrid(a, an, TANK_RADIUS)) A.pos = an;
            if (!circleHitsGrid(a, bn, TANK_RADIUS)) B.pos = bn;
        }
    }
}

SweepHit sweepSegment(const Arena& a, Vec2 from, Vec2 to) {
    SweepHit r;
    r.pos = to;
    Vec2 d = to - from;
    float len = vlen(d);
    int cx = (int)std::floor(from.x), cy = (int)std::floor(from.y);
    if (a.blocksShot(cx, cy)) {              // degenerate: started inside a block
        r.hit = true; r.pos = from; r.normal = {0, 0}; r.cx = cx; r.cy = cy;
        return r;
    }
    if (len < 1e-6f) return r;
    Vec2 dir = d * (1.0f / len);
    int stepX = dir.x > 0 ? 1 : -1, stepY = dir.y > 0 ? 1 : -1;
    float tdx = dir.x != 0 ? std::fabs(1.0f / dir.x) : 1e30f;
    float tdy = dir.y != 0 ? std::fabs(1.0f / dir.y) : 1e30f;
    float fracX = stepX > 0 ? (cx + 1 - from.x) : (from.x - cx);
    float fracY = stepY > 0 ? (cy + 1 - from.y) : (from.y - cy);
    float tmx = dir.x != 0 ? fracX * tdx : 1e30f;
    float tmy = dir.y != 0 ? fracY * tdy : 1e30f;
    float t = 0;
    Vec2 n{0, 0};
    while (true) {
        if (tmx < tmy) { t = tmx; tmx += tdx; cx += stepX; n = {(float)-stepX, 0}; }
        else           { t = tmy; tmy += tdy; cy += stepY; n = {0, (float)-stepY}; }
        if (t > len) return r;                       // reached destination, no hit
        if (a.blocksShot(cx, cy)) {
            r.hit = true;
            r.pos = from + dir * t;
            r.normal = n;
            r.cx = cx; r.cy = cy;
            return r;
        }
    }
}

float segPointDist(Vec2 a, Vec2 b, Vec2 p) {
    Vec2 ab = b - a;
    float len2 = dot(ab, ab);
    if (len2 < 1e-12f) return vlen(p - a);
    float t = clampf(dot(p - a, ab) / len2, 0.0f, 1.0f);
    return vlen(p - (a + ab * t));
}
