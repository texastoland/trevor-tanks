#pragma once
#include "raylib.h"
#include "game.h"
#include "models.h"

struct TreadStamp { Vec2 pos; float angle; bool heavy; };

struct Renderer {
    Camera3D cam{};
    Models models{};
    int aw = 0, ah = 0;
    int fitW = 0, fitH = 0;     // screen size the cached fit distance was computed for
    float fitD = 0;             // 0 = needs refit
    RenderTexture2D floorRT{};  // parquet + tracks + death X's painted permanently into it
    std::vector<TreadStamp> pendingTreads;
    std::vector<Vec2> pendingX;
};

Renderer rendererInit(const Arena& a);
void rendererSetArena(Renderer& r, const Arena& a);
void rendererStampTread(Renderer& r, Vec2 pos, float angle, bool heavy);
void rendererStampDeathX(Renderer& r, Vec2 pos);
void renderWorld(Renderer& r, const World& w, Vec2 aimWorld);   // inside BeginDrawing
void drawReticle(const Renderer& r, Vec2 aimWorld);             // 2D pass: always on top
Vec2 mouseToFloor(const Renderer& r);
void rendererUnload(Renderer& r);

inline Vector3 toWorld(Vec2 p, float y = 0) { return {p.x, y, p.y}; }
