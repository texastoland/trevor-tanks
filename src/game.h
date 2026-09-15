#pragma once
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "tuning.h"

struct Vec2 { float x = 0, y = 0; };
inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, float s) { return {a.x * s, a.y * s}; }
inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float vlen(Vec2 a) { return std::sqrt(dot(a, a)); }
inline Vec2 norm(Vec2 a) { float l = vlen(a); return l > 1e-6f ? a * (1.0f / l) : Vec2{0, 0}; }
inline Vec2 reflect(Vec2 v, Vec2 n) { return v - n * (2.0f * dot(v, n)); }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float angleTo(Vec2 from, Vec2 to) { return std::atan2(to.y - from.y, to.x - from.x); }
inline float wrapAngle(float a) {
    while (a > PI_F) a -= 2 * PI_F;
    while (a < -PI_F) a += 2 * PI_F;
    return a;
}

struct Rng {
    uint32_t s = 0x12345678u;
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float next01() { return (next() >> 8) * (1.0f / 16777216.0f); }
    float range(float a, float b) { return a + (b - a) * next01(); }
    int irange(int a, int b) { return a + (int)(next() % (uint32_t)(b - a + 1)); }
};

enum class Block : uint8_t { Empty, Wall, Crate, Hole };
enum class TankType : uint8_t { Player, Brown, Grey, Teal, Yellow, Red, Green, Purple, White, Black, COUNT };
enum class Steering : uint8_t { None, Wander, Evade, Chase, FleeWhenClose };
enum class AimStyle : uint8_t { Direct, Ricochet, Scan };

struct EnemyParams {
    float moveSpeed, shellSpeed;
    int maxShells, bounces;
    float fireCooldown;
    int maxMines;
    Steering steering;
    AimStyle aim;
    int aimBounces;
    bool dodgesShells;      // wiki: Yellow is "Incautious" — ignores incoming fire
    bool leadAim;           // wiki: only Green predicts where the player is heading
};

struct Arena {
    int w = 0, h = 0;
    std::vector<Block> blocks;
    Block at(int x, int y) const {
        if (x < 0 || y < 0 || x >= w || y >= h) return Block::Wall;
        return blocks[y * w + x];
    }
    void set(int x, int y, Block b) { if (x >= 0 && y >= 0 && x < w && y < h) blocks[y * w + x] = b; }
    bool blocksTank(int x, int y) const { return at(x, y) != Block::Empty; }
    bool blocksShot(int x, int y) const {          // shells fly over holes
        Block b = at(x, y);
        return b == Block::Wall || b == Block::Crate;
    }
};

struct Tank {
    TankType type = TankType::Player;
    Vec2 pos, vel, goal;
    float bodyAngle = 0, turretAngle = 0;
    bool alive = true;
    int shellsLive = 0, minesLive = 0;
    float fireTimer = 0;
    float moveFreeze = 0;      // >0 = movement paused (just fired)
    float mineTimer = 0;       // AI mine-laying cooldown
    float scanDir = 1;         // Scan aim: current turret sweep direction
    // AI state
    std::vector<Vec2> path;
    int pathIdx = 0;
    float replanTimer = 0, wanderTimer = 0, aimReplanTimer = 0, fireDelay = 0;
    float desiredAim = 0;
    bool hasShot = false;      // aim solution currently valid
};

struct Shell { Vec2 pos, vel; int bouncesLeft = 0, owner = 0; bool alive = true; };
struct Mine  { Vec2 pos; int owner = 0; float age = 0; bool alive = true; };

struct GameEvent {
    enum Kind { Fire, Bounce, ShellPoof, TankExplode, MineLay, MineExplode, CrateBreak } kind;
    Vec2 pos;
    float angle = 0;
};

struct World {
    Arena arena;
    std::vector<Tank> tanks;   // tanks[0] = player
    std::vector<Shell> shells;
    std::vector<Mine> mines;
    std::vector<GameEvent> events;
    Rng rng;
    Tank& player() { return tanks[0]; }
    bool playerAlive() const { return !tanks.empty() && tanks[0].alive; }
    int enemiesAlive() const {
        int n = 0;
        for (size_t i = 1; i < tanks.size(); ++i) if (tanks[i].alive) n++;
        return n;
    }
};

struct InputState {
    bool up = false, down = false, left = false, right = false, fire = false, mine = false;
    Vec2 aimWorld;
};
