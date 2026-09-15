#include "particles.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>

// world-space sizes for the structured blasts
static const float KILL_R = MINE_BLAST_RADIUS + TANK_RADIUS;   // a mine kills inside this
static const float BOOM_LIFE = 0.55f, BOOM_R = 1.4f;
static const float MUSH_LIFE = 2.1f;

static float clamp01f(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static float frontEase(float t) { return 1.0f - powf(1.0f - clamp01f(t), 2.6f); }  // fast out, decelerating
static Color withA(Color c, float a) { c.a = (unsigned char)(255 * clamp01f(a)); return c; }

static void burst(Effects& fx, Rng& rng, Vec2 at, int n, float speed, float size,
                  float life, Color col, float baseY = 0.3f) {
    for (int i = 0; i < n; ++i) {
        float a = rng.range(0, 2 * PI_F), s = rng.range(speed * 0.3f, speed);
        Particle p;
        p.pos = {at.x, baseY + rng.range(0, 0.2f), at.y};
        p.vel = {std::cos(a) * s, rng.range(0.5f, 2.5f), std::sin(a) * s};
        p.maxLife = p.life = rng.range(life * 0.5f, life);
        p.size = rng.range(size * 0.6f, size);
        p.col = col;
        fx.parts.push_back(p);
    }
}

void effectsHandle(Effects& fx, const std::vector<GameEvent>& events, Rng& rng) {
    for (auto& e : events) {
        switch (e.kind) {
            case GameEvent::Fire: {       // muzzle flash: a few sparks along the barrel dir
                Vec2 at = e.pos;
                burst(fx, rng, at, 5, 1.5f, 0.07f, 0.15f, {255, 220, 120, 255}, 0.35f);
                break;
            }
            case GameEvent::Bounce:
                burst(fx, rng, e.pos, 6, 1.2f, 0.05f, 0.25f, {200, 180, 140, 255});
                break;
            case GameEvent::ShellPoof:
                burst(fx, rng, e.pos, 8, 0.8f, 0.08f, 0.4f, {230, 230, 230, 255});
                break;
            case GameEvent::TankExplode: {
                // (the persistent white X decal is stamped into the floor by the renderer)
                burst(fx, rng, e.pos, 18, 2.5f, 0.14f, 0.7f, {255, 150, 40, 255});
                burst(fx, rng, e.pos, 12, 1.2f, 0.18f, 1.1f, {90, 90, 90, 255});
                Blast b;
                b.kind = Blast::BoomStar;
                b.pos = e.pos;
                for (float& j : b.jag) j = fx.rng.range(0.75f, 1.25f);
                fx.blasts.push_back(std::move(b));
                break;
            }
            case GameEvent::MineExplode: {
                burst(fx, rng, e.pos, 24, 3.5f, 0.16f, 0.8f, {255, 170, 50, 255});
                Blast b;
                b.kind = Blast::Mushroom;
                b.pos = e.pos;
                b.puffs.reserve(46);
                for (int i = 0; i < 46; ++i)
                    b.puffs.push_back({fx.rng.range(0, 2 * PI_F), fx.rng.next01(),
                                       fx.rng.next01(), fx.rng.range(0.22f, 0.55f),
                                       fx.rng.range(0, 2 * PI_F)});
                fx.blasts.push_back(std::move(b));
                break;
            }
            case GameEvent::MineLay:
                burst(fx, rng, e.pos, 4, 0.5f, 0.05f, 0.3f, {160, 140, 100, 255});
                break;
            case GameEvent::CrateBreak:
                burst(fx, rng, e.pos, 10, 2.0f, 0.10f, 0.6f, {205, 175, 120, 255});
                break;
        }
    }
}

void effectsSpawnShellSmoke(Effects& fx, Rng& rng, Vec2 pos, Vec2 vel, bool rocket) {
    Vec2 back = norm(vel) * -0.18f;                 // puff just behind the projectile
    Particle p;
    p.pos = {pos.x + back.x + rng.range(-0.03f, 0.03f),
             0.35f + rng.range(-0.02f, 0.04f),
             pos.y + back.y + rng.range(-0.03f, 0.03f)};
    p.vel = {0, rng.range(0.2f, 0.5f), 0};          // drifts gently upward
    if (rocket) {
        // fast shots are rockets: heavy grey plume + orange exhaust flame
        p.maxLife = p.life = rng.range(0.30f, 0.50f);
        p.size = rng.range(0.06f, 0.12f);
        p.col = {190, 190, 190, 170};
        fx.parts.push_back(p);
        Particle f;
        f.pos = {pos.x + back.x * 1.4f + rng.range(-0.04f, 0.04f),
                 0.35f + rng.range(-0.02f, 0.03f),
                 pos.y + back.y * 1.4f + rng.range(-0.04f, 0.04f)};
        f.vel = {0, 0.15f, 0};
        f.maxLife = f.life = rng.range(0.08f, 0.16f);
        f.size = rng.range(0.06f, 0.11f);
        f.col = rng.next01() < 0.5f ? Color{255, 185, 55, 255} : Color{255, 120, 30, 255};
        fx.parts.push_back(f);
    } else {
        p.maxLife = p.life = rng.range(0.15f, 0.30f);
        p.size = rng.range(0.04f, 0.08f);
        p.col = {205, 205, 205, 150};
        fx.parts.push_back(p);
    }
}

void effectsUpdate(Effects& fx, float dt) {
    for (auto& p : fx.parts) {
        p.life -= dt;
        if (p.ring) { p.size += 3.0f * dt; continue; }
        p.pos = Vector3{p.pos.x + p.vel.x * dt, p.pos.y + p.vel.y * dt, p.pos.z + p.vel.z * dt};
        p.vel.y -= 6.0f * dt;             // light gravity
        if (p.pos.y < 0.02f) { p.pos.y = 0.02f; p.vel = {0, 0, 0}; }
    }
    fx.parts.erase(std::remove_if(fx.parts.begin(), fx.parts.end(),
                                  [](const Particle& p) { return p.life <= 0; }),
                   fx.parts.end());
    for (auto& b : fx.blasts) b.age += dt;
    fx.blasts.erase(std::remove_if(fx.blasts.begin(), fx.blasts.end(),
                                   [](const Blast& b) {
                                       return b.age >= (b.kind == Blast::BoomStar ? BOOM_LIFE
                                                                                  : MUSH_LIFE);
                                   }),
                    fx.blasts.end());
}

// ---- structured blast drawing (flat ground geometry + spheres) ----

static void drawFlatDisc(Vec2 c, float radius, float y, Color col) {
    const int N = 32;
    Vector3 ctr{c.x, y, c.y};
    for (int i = 0; i < N; ++i) {
        float a0 = i * 2 * PI_F / N, a1 = (i + 1) * 2 * PI_F / N;
        DrawTriangle3D(ctr, {c.x + cosf(a0) * radius, y, c.y + sinf(a0) * radius},
                       {c.x + cosf(a1) * radius, y, c.y + sinf(a1) * radius}, col);
    }
}

static void drawFlatRing(Vec2 c, float radius, float width, float y, Color col) {
    const int N = 40;
    Vector3 pts[2 * (N + 1)];
    float r0 = radius - width * 0.5f, r1 = radius + width * 0.5f;
    if (r0 < 0) r0 = 0;
    for (int i = 0; i <= N; ++i) {
        float a = i * 2 * PI_F / N;
        float ca = cosf(a), sa = sinf(a);
        pts[2 * i] = {c.x + ca * r1, y, c.y + sa * r1};
        pts[2 * i + 1] = {c.x + ca * r0, y, c.y + sa * r0};
    }
    DrawTriangleStrip3D(pts, 2 * (N + 1), col);
}

static void drawStarLayer(Vec2 c, const float* jag, float radius, float rot, float y, Color col) {
    Vector3 v[12];
    for (int i = 0; i < 12; ++i) {
        float a = i * 2 * PI_F / 12 + rot;
        float rad = radius * jag[i] * (i % 2 ? 0.55f : 1.0f);   // alternate spike/notch
        v[i] = {c.x + cosf(a) * rad, y, c.y + sinf(a) * rad};
    }
    Vector3 ctr{c.x, y, c.y};
    for (int i = 0; i < 12; ++i) DrawTriangle3D(ctr, v[i], v[(i + 1) % 12], col);
}

static void drawBoomStar(const Blast& b) {
    float t = b.age / BOOM_LIFE;
    // shockwave ring races out ahead of the star
    float rf = frontEase(t * 1.15f);
    float ringA = clamp01f(1.4f - t * 1.7f);
    drawFlatRing(b.pos, rf * BOOM_R, 0.10f * (1 - t * 0.6f), 0.050f,
                 withA({255, 255, 255, 255}, ringA));
    drawFlatRing(b.pos, rf * BOOM_R * 0.87f, 0.16f * (1 - t * 0.7f), 0.045f,
                 withA({255, 183, 99, 255}, ringA * 0.55f));
    // comic star: punches out, then relaxes while fading
    float sf = (t < 0.5f ? frontEase(t / 0.5f) : 1 - (t - 0.5f) * 0.5f) * 0.88f;
    float sA = clamp01f(2.2f - t * 2.4f);
    float base = BOOM_R * 1.08f * sf;
    drawStarLayer(b.pos, b.jag, base, 0.13f + t * 0.5f, 0.060f, withA({226, 87, 43, 255}, sA));
    drawStarLayer(b.pos, b.jag, base * 0.80f, -0.08f + t * 0.35f, 0.070f, withA({255, 155, 38, 255}, sA));
    drawStarLayer(b.pos, b.jag, base * 0.58f, 0.22f + t * 0.2f, 0.080f, withA({255, 216, 77, 255}, sA));
    drawStarLayer(b.pos, b.jag, base * 0.33f, -0.15f, 0.090f, withA({255, 251, 232, 255}, sA));
    // brief light pillar at detonation
    if (t < 0.22f) {
        float a = 1 - t / 0.22f;
        DrawCylinder({b.pos.x, 0, b.pos.y}, 0.07f * a, 0.10f * a, 3.0f, 10,
                     withA({255, 255, 255, 255}, a * 0.9f));
        DrawCylinder({b.pos.x, 0, b.pos.y}, 0.15f * a, 0.20f * a, 2.6f, 10,
                     withA({255, 217, 138, 255}, a * 0.5f));
    }
}

static void drawMushroom(const Blast& b) {
    float t = b.age / MUSH_LIFE;
    float f = frontEase(t);
    float fade = clamp01f((1 - t) * 4);       // everything resolves by the end
    // ground flash
    if (t < 0.15f) {
        float a = 1 - t / 0.15f;
        float fr = KILL_R * (0.4f + 3 * t);
        drawFlatDisc(b.pos, fr < KILL_R * 1.1f ? fr : KILL_R * 1.1f, 0.040f,
                     withA({255, 255, 255, 255}, a));
        DrawSphere({b.pos.x, 0.3f, b.pos.y}, 0.55f * a, withA({255, 255, 255, 255}, a));
    }
    // temporary scorch under the cloud
    drawFlatDisc(b.pos, KILL_R * 0.55f, 0.035f,
                 withA({58, 44, 32, 255}, clamp01f(t * 2) * 0.5f * fade));
    // base ring expands to the EXACT kill radius (mine blast + tank radius)
    drawFlatRing(b.pos, KILL_R * frontEase(t * 1.15f), 0.22f * (1 - t * 0.6f), 0.050f,
                 withA({232, 211, 168, 255}, clamp01f(1.2f - t) * 0.85f));
    // stem climbs, cap curls outward
    float rise = f * 2.3f;
    for (auto& p : b.puffs) {
        float y = 0.25f + p.h * rise;
        bool cap = p.h > 0.62f;
        float spread = cap ? (p.h - 0.62f) * 3.2f * f + 0.40f
                           : 0.30f + 0.10f * sinf(p.h * 9 + t * 8);
        float wob = sinf(p.ph + t * 6) * 0.06f;
        Vector3 pp{b.pos.x + cosf(p.az) * (p.o * spread + wob), y,
                   b.pos.y + sinf(p.az) * (p.o * spread + wob)};
        float heat = clamp01f(1.1f - t - p.h * 0.5f);
        Color col = heat > 0.5f ? Color{255, 176, 84, 255}
                    : heat > 0.25f ? Color{200, 106, 48, 255} : Color{107, 90, 74, 255};
        float s = p.s * (cap ? 1.25f : 0.8f) * (0.5f + f);
        DrawSphereEx(pp, s, 8, 10, withA(col, clamp01f(1.7f - t * 1.5f) * 0.95f * fade));
    }
}

void effectsDraw(const Effects& fx) {
    for (auto& p : fx.parts) {
        float k = p.life / p.maxLife;
        Color c = p.col;
        c.a = (unsigned char)(c.a * k);
        if (p.ring) DrawCircle3D(p.pos, p.size, {1, 0, 0}, 90.0f, c);
        else DrawCube(p.pos, p.size, p.size, p.size, c);
    }
    if (!fx.blasts.empty()) {
        // culling toggles change GL state NOW but batched triangles draw at
        // flush time — flush at both edges so the off-window really covers
        // the blast geometry (flat fans are wound away from the camera)
        rlDrawRenderBatchActive();
        rlDisableBackfaceCulling();
        for (auto& b : fx.blasts)
            b.kind == Blast::BoomStar ? drawBoomStar(b) : drawMushroom(b);
        rlDrawRenderBatchActive();
        rlEnableBackfaceCulling();
    }
}
