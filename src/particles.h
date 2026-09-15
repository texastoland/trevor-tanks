#pragma once
#include "raylib.h"
#include "game.h"

struct Particle {
    Vector3 pos, vel;
    float life = 0, maxLife = 1, size = 0.1f;
    bool ring = false;                    // rings expand instead of flying
    Color col = WHITE;
};
// structured explosion effects: tank kills get a comic boom star with a
// shockwave ring; mine detonations get a mushroom cloud whose base ring
// expands to the exact kill radius
struct MushPuff { float az, o, h, s, ph; };   // azimuth, radial frac, height frac, size, phase
struct Blast {
    enum Kind { BoomStar, Mushroom } kind = BoomStar;
    Vec2 pos;
    float age = 0;
    float jag[12] = {};                       // star spike length factors
    std::vector<MushPuff> puffs;              // mushroom stem/cap blobs
};

struct Effects {
    std::vector<Particle> parts;
    std::vector<Blast> blasts;
    Rng rng{0x5EED5EEDu};   // presentation-only; the sim rng is never touched
};

// tread marks are painted into the floor texture by the renderer, not here
void effectsHandle(Effects& fx, const std::vector<GameEvent>& events, Rng& rng);
void effectsSpawnShellSmoke(Effects& fx, Rng& rng, Vec2 pos, Vec2 vel, bool rocket);
void effectsUpdate(Effects& fx, float dt);
void effectsDraw(const Effects& fx);
