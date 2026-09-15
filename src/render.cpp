#include "render.h"
#include "raymath.h"

// Fit by proof, not by formula: push the camera back until every arena corner
// (floor and wall-top height) projects inside the viewport with a margin. The
// whole arena must ALWAYS be on screen — unseen borders read as bullets
// "wrapping" instead of ricocheting.
static void fitCamera(Renderer& r) {
    float cx = r.aw * 0.5f, cz = r.ah * 0.5f;
    const float pitch = 57.0f * DEG2RAD;
    r.cam.target = {cx, 0, cz};
    r.cam.up = {0, 1, 0};
    r.cam.fovy = 45.0f;
    r.cam.projection = CAMERA_PERSPECTIVE;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (sw == r.fitW && sh == r.fitH && r.fitD > 0) {
        r.cam.position = {cx, r.fitD * sinf(pitch), cz + r.fitD * cosf(pitch)};
        return;
    }
    const float margin = 14.0f;
    float d = 5.0f;                                         // start close; first fit = tightest
    for (int i = 0; i < 120; ++i) {
        r.cam.position = {cx, d * sinf(pitch), cz + d * cosf(pitch)};
        bool ok = true;
        for (int corner = 0; corner < 4 && ok; ++corner) {
            float x = (corner & 1) ? (float)r.aw : 0.0f;
            float z = (corner & 2) ? (float)r.ah : 0.0f;
            for (float y = 0.0f; y <= 0.9f && ok; y += 0.9f) {
                Vector2 s = GetWorldToScreen({x, y, z}, r.cam);
                if (s.x < margin || s.y < margin || s.x > sw - margin || s.y > sh - margin)
                    ok = false;
            }
        }
        if (ok) break;
        d *= 1.03f;
    }
    r.fitW = sw; r.fitH = sh; r.fitD = d;
}

Renderer rendererInit(const Arena& a) {
    Renderer r;
    r.models = buildModels();
    rendererSetArena(r, a);
    return r;
}

static const int TRACK_PX = 24;    // floor canvas resolution per cell

void rendererSetArena(Renderer& r, const Arena& a) {
    r.aw = a.w; r.ah = a.h;
    r.fitD = 0;                 // arena changed: recompute the fit
    buildFloor(r.models, a.w, a.h);
    // tracks are PAINTED into the floor: permanent, and re-stamping the same
    // opaque color never darkens — it only fills in, like the original
    if (r.floorRT.id) UnloadRenderTexture(r.floorRT);
    r.floorRT = LoadRenderTexture(a.w * TRACK_PX, a.h * TRACK_PX);
    BeginTextureMode(r.floorRT);
    DrawTexturePro(r.models.floorTex,
                   {0, 0, (float)r.models.floorTex.width, (float)r.models.floorTex.height},
                   {0, 0, (float)a.w * TRACK_PX, (float)a.h * TRACK_PX}, {0, 0}, 0, WHITE);
    EndTextureMode();
    r.models.floor.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = r.floorRT.texture;
    r.pendingTreads.clear();
    r.pendingX.clear();
    fitCamera(r);
}

void rendererStampTread(Renderer& r, Vec2 pos, float angle, bool heavy) {
    r.pendingTreads.push_back({pos, angle, heavy});
}

void rendererStampDeathX(Renderer& r, Vec2 pos) {
    r.pendingX.push_back(pos);
}

static void flushTreads(Renderer& r) {
    if (r.pendingTreads.empty() && r.pendingX.empty()) return;
    BeginTextureMode(r.floorRT);
    for (auto& x : r.pendingX) {
        // turret-sized chunky white cross, permanent like the tracks
        float cx = x.x * TRACK_PX;
        float cy = r.ah * TRACK_PX - x.y * TRACK_PX;
        float L = 0.46f * TRACK_PX, T = 0.16f * TRACK_PX;
        Color white = {246, 246, 244, 255};
        DrawRectanglePro({cx, cy, L, T}, {L / 2, T / 2}, 45, white);
        DrawRectanglePro({cx, cy, L, T}, {L / 2, T / 2}, -45, white);
    }
    r.pendingX.clear();
    for (auto& t : r.pendingTreads) {
        // RT textures sample bottom-up on the mesh: flip world y into canvas y
        float cx = t.pos.x * TRACK_PX;
        float cy = r.ah * TRACK_PX - t.pos.y * TRACK_PX;
        float deg = t.angle * RAD2DEG;                      // y-flip mirrors the rotation
        Color c = t.heavy ? Color{118, 88, 56, 255} : Color{146, 112, 74, 255};
        float w = (t.heavy ? 0.085f : 0.06f) * TRACK_PX;
        float h = (t.heavy ? 0.16f : 0.12f) * TRACK_PX;
        float off = 0.28f * TRACK_PX;
        float s = sinf(t.angle), co = cosf(t.angle);
        DrawRectanglePro({cx - s * off, cy - co * off, w, h}, {w / 2, h / 2}, deg, c);
        DrawRectanglePro({cx + s * off, cy + co * off, w, h}, {w / 2, h / 2}, deg, c);
    }
    EndTextureMode();
    r.pendingTreads.clear();
}

Vec2 mouseToFloor(const Renderer& r) {
    Ray ray = GetScreenToWorldRay(GetMousePosition(), r.cam);   // GetMouseRay on raylib 5.0
    if (fabsf(ray.direction.y) < 1e-6f) return {0, 0};
    float t = -ray.position.y / ray.direction.y;
    return {ray.position.x + ray.direction.x * t, ray.position.z + ray.direction.z * t};
}

static Color scaled(Color c, float k) {
    return {(unsigned char)clampf(c.r * k, 0, 255), (unsigned char)clampf(c.g * k, 0, 255),
            (unsigned char)clampf(c.b * k, 0, 255), 255};
}

static void drawTank(Renderer& r, const Tank& t) {
    Color body = tankColor(t.type);
    Color top = scaled(body, 1.28f);                       // lighter roof plate, two-tone look
    Color treadCol = {58, 52, 46, 255};                    // near-black rubber, like the original
    Color metal = {84, 78, 70, 255};
    float bodyDeg = -t.bodyAngle * RAD2DEG, turretDeg = -t.turretAngle * RAD2DEG;
    Vector3 p = toWorld(t.pos);
    // shadow blob
    DrawCylinder({p.x, 0.015f, p.z}, 0.42f, 0.42f, 0.01f, 16, {0, 0, 0, 60});
    // treads (offset along the hull's local z, i.e. perpendicular to facing)
    Vector3 side = {sinf(t.bodyAngle) * 0.28f, 0.11f, cosf(t.bodyAngle) * 0.28f};
    DrawModelEx(r.models.tread, {p.x + side.x, side.y, p.z - side.z},
                {0, 1, 0}, bodyDeg, {1, 1, 1}, treadCol);
    DrawModelEx(r.models.tread, {p.x - side.x, side.y, p.z + side.z},
                {0, 1, 0}, bodyDeg, {1, 1, 1}, treadCol);
    DrawModelEx(r.models.hull, p, {0, 1, 0}, bodyDeg, {1, 1, 1}, body);
    DrawModelEx(r.models.hullTop, p, {0, 1, 0}, bodyDeg, {1, 1, 1}, top);
    DrawModelEx(r.models.dome, p, {0, 1, 0}, turretDeg, {1, 1, 1}, body);
    DrawModelEx(r.models.barrel, p, {0, 1, 0}, turretDeg, {1, 1, 1}, metal);
    DrawModelEx(r.models.muzzle, p, {0, 1, 0}, turretDeg, {1, 1, 1}, scaled(body, 0.55f));
}

void renderWorld(Renderer& r, const World& w, Vec2 aimWorld) {
    flushTreads(r);                                 // paint new tracks before the 3D pass
    fitCamera(r);                                   // cheap; adapts to window resizes
    BeginMode3D(r.cam);
    DrawModel(r.models.floor, {r.aw * 0.5f, 0, r.ah * 0.5f}, 1.0f, WHITE);
    for (int y = 0; y < w.arena.h; ++y) {
        for (int x = 0; x < w.arena.w; ++x) {
            Block b = w.arena.at(x, y);
            Vector3 c = {x + 0.5f, 0, y + 0.5f};
            if (b == Block::Wall) {
                // crenellated look from the original: alternate tall/short blocks
                float hscale = ((x + y) & 1) ? 0.62f : 1.0f;
                DrawModelEx(r.models.wall, {c.x, 0.45f * hscale, c.z}, {0, 1, 0}, 0,
                            {1, hscale, 1}, {184, 150, 106, 255});
            }
            else if (b == Block::Crate)
                DrawModel(r.models.crate, {c.x, 0.35f, c.z}, 1.0f, {206, 152, 128, 255});
            else if (b == Block::Hole) {
                DrawCylinder({c.x, 0.012f, c.z}, 0.46f, 0.46f, 0.004f, 20, {12, 8, 6, 255});
                DrawCircle3D({c.x, 0.02f, c.z}, 0.46f, {1, 0, 0}, 90.0f, {60, 42, 28, 255});
            }
        }
    }
    for (auto& t : w.tanks) {
        if (!t.alive) continue;
        if (t.type == TankType::White) continue;    // the invisible tank
        drawTank(r, t);
    }
    for (auto& s : w.shells) {
        float deg = -std::atan2(s.vel.y, s.vel.x) * RAD2DEG;
        Vector3 p = toWorld(s.pos, 0.35f);
        DrawModelEx(r.models.shell, p, {0, 1, 0}, deg, {1, 1, 1}, {105, 100, 92, 255});
        DrawModelEx(r.models.shellNose, p, {0, 1, 0}, deg, {1, 1, 1}, {70, 66, 60, 255});
        if (vlen(s.vel) > SHELL_NORMAL + 0.5f) {
            // rocket exhaust per the reference: a big opaque cartoon flame -
            // yellow-white teardrop core at the nozzle with jagged orange
            // lobes flaring behind, flickering, then the smoke takes over
            Vec2 bd = norm(s.vel);
            Vec2 pp{-bd.y, bd.x};
            float ph = (float)(((size_t)(&s - w.shells.data())) * 1.7f);
            float tnow = (float)GetTime();
            float f1 = 0.85f + 0.30f * sinf(tnow * 55.0f + ph * 9.0f);
            float f2 = 0.85f + 0.30f * sinf(tnow * 71.0f + ph * 5.0f);
            auto at = [&](float back, float side, float y) {
                return Vector3{s.pos.x - bd.x * back + pp.x * side, y,
                               s.pos.y - bd.y * back + pp.y * side};
            };
            DrawSphere(at(0.15f, 0.0f, 0.35f), 0.105f * f1, {255, 236, 130, 255});
            DrawSphere(at(0.28f, 0.035f, 0.36f), 0.115f * f2, {255, 190, 55, 250});
            DrawSphere(at(0.40f, -0.06f, 0.34f), 0.085f * f1, {255, 140, 30, 235});
            DrawSphere(at(0.50f, 0.05f, 0.35f), 0.065f * f2, {250, 108, 24, 215});
        }
    }
    double now = GetTime();
    for (auto& m : w.mines) {
        // the original's mine is a little red ball; it pulses once armed and
        // flashes urgently in its final seconds
        bool urgent = m.age > MINE_LIFETIME - 2.0f;
        bool on = fmod(now, urgent ? 0.2 : 0.8) < (urgent ? 0.1 : 0.4);
        Color c = m.age < MINE_ARM_TIME ? Color{150, 90, 90, 255}
                  : (on ? Color{240, 60, 50, 255} : Color{185, 45, 40, 255});
        DrawModel(r.models.mine, toWorld(m.pos, 0.13f), 1.0f, c);
    }
    if (w.playerAlive()) {                          // faint line-of-fire hint on the floor
        const Tank& pl = w.tanks[0];
        Vector3 tip = {pl.pos.x + cosf(pl.turretAngle) * BARREL_LEN, 0.35f,
                       pl.pos.y + sinf(pl.turretAngle) * BARREL_LEN};
        DrawLine3D(tip, toWorld(aimWorld, 0.05f), {255, 255, 255, 45});
    }
    EndMode3D();
}

void drawReticle(const Renderer& r, Vec2 aimWorld) {
    // screen-space, drawn after everything 3D: bold and always on top of tanks
    Vector2 c = GetWorldToScreen(toWorld(aimWorld, 0.04f), r.cam);
    Color blue = {60, 120, 235, 255};
    Color halo = {255, 255, 255, 200};
    float g = 9, len = 15, th = 6;                  // gap, prong length, thickness
    for (int i = 0; i < 4; ++i) {
        float dx = (i == 0) - (i == 1), dy = (i == 2) - (i == 3);
        Vector2 a = {c.x + dx * g, c.y + dy * g};
        Vector2 b = {c.x + dx * (g + len), c.y + dy * (g + len)};
        DrawLineEx(a, b, th + 3, halo);
        DrawLineEx(a, b, th, blue);
    }
    DrawCircleV(c, 3.5f, halo);
    DrawCircleV(c, 2.2f, blue);
}

void rendererUnload(Renderer& r) {
    if (r.floorRT.id) UnloadRenderTexture(r.floorRT);
    unloadModels(r.models);
}
