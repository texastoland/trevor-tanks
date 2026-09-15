#include "raylib.h"
#ifdef TANKS_HAVE_GLFW
#include "GLFW/glfw3.h"
#endif
#include "arena.h"
#include "audio.h"
#include "combat.h"
#include "levels.h"
#include "particles.h"
#include "player.h"
#include "render.h"
#include "replay.h"
#include "ui.h"

enum class Phase { Title, Intro, Playing, Paused, Clear, Death, GameOver, Results, Replay };

struct Session {
    Phase phase = Phase::Title;
    int mission = 0;
    int startMission = 0;
    int lives = START_LIVES;
    int savedMission = 0;            // persistence lands in Task 14
    int contMission = 0;             // continue-from picker: any beaten mission or the next
    bool won = false;
    bool dev = false;                // secret mission select: backtick on the title screen
    int devMission = 0;
    bool tankEdit = false;           // secret tank tuning: T on the title screen (dev implies it)
    int editRow = 0;
    int panelType = 0;               // TankType index driving the stat rows; -1 = Custom
    bool cheated = false;            // current run uses modified stats: never touch the save
    EnemyParams custom{};
    Phase pausedFrom = Phase::Playing;
    float phaseTimer = 0;
    CampaignStats stats;
    ::Replay rec;                    // current attempt being recorded
    ::Replay play;                   // attempt being played back
    size_t playIdx = 0;
    Vec2 playAim;
    World world;
    Effects fx;
    std::vector<Vec2> treadAnchors;
    std::vector<uint8_t> treadPhase;   // per-tank two-stroke alternation
};

// Touchpad taps deliver press+release inside a single glfwPollEvents batch;
// raylib only keeps the latest state per frame, so such taps are invisible to
// IsMouseButtonPressed/Down. Latch presses at the GLFW callback level instead.
static bool g_pressLatch[2] = {false, false};   // [0]=left, [1]=right
#ifdef TANKS_HAVE_GLFW
static GLFWmousebuttonfun g_prevMouseCb = nullptr;
static void latchMouseCb(GLFWwindow* win, int button, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) g_pressLatch[0] = true;
        if (button == GLFW_MOUSE_BUTTON_RIGHT) g_pressLatch[1] = true;
    }
    if (g_prevMouseCb) g_prevMouseCb(win, button, action, mods);
}
static void installPressLatch() {
    // GetWindowHandle() returns the *native* handle (an NSWindow* on macOS, HWND on
    // Windows, an XID on X11), not a GLFWwindow*. Casting it to GLFWwindow* hands GLFW
    // a bogus struct and corrupts the native window. Ask GLFW for its own window that
    // owns raylib's current GL context instead.
    GLFWwindow* glfwWin = glfwGetCurrentContext();
    if (glfwWin) g_prevMouseCb = glfwSetMouseButtonCallback(glfwWin, latchMouseCb);
}
#else
static void installPressLatch() {}
#endif

static InputState readInput(const Renderer& r) {
    InputState in;
    in.up = IsKeyDown(KEY_W);
    in.down = IsKeyDown(KEY_S);
    in.left = IsKeyDown(KEY_A);
    in.right = IsKeyDown(KEY_D);
    in.fire = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || g_pressLatch[0];
    in.mine = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || g_pressLatch[1] || IsKeyPressed(KEY_SPACE);
    g_pressLatch[0] = g_pressLatch[1] = false;
    in.aimWorld = mouseToFloor(r);
    return in;
}

static void enterIntro(Session& s, Renderer& renderer, bool keepDestruction = false) {
    World fresh = missionWorld(s.mission);
    if (keepDestruction) {                  // death-retry: the mission remembers your progress
        carryOverDestruction(s.world.arena, fresh.arena);   // blown crates stay gone
        carryOverKills(s.world, fresh);                     // destroyed enemies stay destroyed
    }
    s.world = fresh;
    s.fx = Effects{};
    s.treadAnchors.clear();
    for (auto& t : s.world.tanks) s.treadAnchors.push_back(t.pos);
    s.treadPhase.assign(s.world.tanks.size(), 0);
    rendererSetArena(renderer, s.world.arena);
    replayCaptureStart(s.rec, s.mission, s.world, s.cheated ? &s.custom : nullptr);
    s.phase = Phase::Intro;
    s.phaseTimer = 2.0f;
}

static void tickEffects(Session& s, Renderer& renderer, float frameDt) {
    audioHandle(s.world.events);
    effectsHandle(s.fx, s.world.events, s.world.rng);
    for (auto& e : s.world.events)
        if (e.kind == GameEvent::TankExplode)
            rendererStampDeathX(renderer, e.pos);   // permanent floor decal
    s.world.events.clear();
    for (size_t i = 0; i < s.world.tanks.size(); ++i) {
        if (!s.world.tanks[i].alive) continue;
        if (vlen(s.world.tanks[i].pos - s.treadAnchors[i]) > 0.16f) {   // dense dotted trail
            rendererStampTread(renderer, s.world.tanks[i].pos, s.world.tanks[i].bodyAngle,
                               s.world.tanks[i].type == TankType::White);  // heavier: their tell
            s.treadPhase[i] ^= 1;              // two-stroke: alternate low/high clack
            audioTreadTick(i == 0, s.treadPhase[i]);
            s.treadAnchors[i] = s.world.tanks[i].pos;
        }
    }
    for (auto& sh : s.world.shells)
        effectsSpawnShellSmoke(s.fx, s.world.rng, sh.pos, sh.vel,
                               vlen(sh.vel) > SHELL_NORMAL + 0.5f);   // fast shots are rockets
    effectsUpdate(s.fx, frameDt);
}

static void drawScene(Session& s, Renderer& renderer, Vec2 aim) {
    ClearBackground({25, 20, 15, 255});
    renderWorld(renderer, s.world, aim);
    BeginMode3D(renderer.cam);
    effectsDraw(s.fx);
    EndMode3D();
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Tanks!");
    if (!IsWindowReady()) { TraceLog(LOG_ERROR, "window init failed"); return 1; }
    if (!audioInit()) { TraceLog(LOG_ERROR, "audio init failed"); CloseWindow(); return 1; }
    installPressLatch();
    SetExitKey(KEY_ESCAPE);

    Session s;
    const EnemyParams playerDefaults = paramsFor(TankType::Player);   // before any override
    s.custom = playerDefaults;
    s.savedMission = loadProgress();
    s.contMission = s.savedMission;
    s.world = missionWorld(0);                      // renderer needs an arena to start
    Renderer renderer = rendererInit(s.world.arena);
    float acc = 0;
    ClickBuffer fireBuf, mineBuf;

    while (!WindowShouldClose()) {
        float frameDt = GetFrameTime();
        InputState in = readInput(renderer);
        bool anyKey = GetKeyPressed() != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        if (s.phase != Phase::Paused) s.phaseTimer -= frameDt;   // pause freezes timers

        switch (s.phase) {
            case Phase::Title: {
                if (IsKeyPressed(KEY_GRAVE)) s.dev = !s.dev;    // secret mission select (backtick)
                if (IsKeyPressed(KEY_T)) s.tankEdit = !s.tankEdit;   // secret tank tuning
                if (!s.dev && !s.tankEdit) {    // menus closed: back to a normal game
                    s.custom = playerDefaults;  // (regular tank, mission 1 / continue)
                    s.panelType = 0;
                    s.editRow = 0;
                }
                if (s.dev || s.tankEdit) {
                    int rowCount = (s.dev ? 1 : 0) + 7;
                    if (s.editRow >= rowCount) s.editRow = 0;
                    if (IsKeyPressed(KEY_UP))   s.editRow = (s.editRow + rowCount - 1) % rowCount;
                    if (IsKeyPressed(KEY_DOWN)) s.editRow = (s.editRow + 1) % rowCount;
                    int dir = (IsKeyPressed(KEY_RIGHT) ? 1 : 0) - (IsKeyPressed(KEY_LEFT) ? 1 : 0);
                    if (dir != 0) {
                        auto ci = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };
                        int r = s.editRow - (s.dev ? 1 : 0);     // -1 = mission row
                        if (r >= 1) s.panelType = -1;            // touching a stat = Custom
                        switch (r) {
                            case -1: s.devMission = (s.devMission + MISSION_COUNT + dir) % MISSION_COUNT; break;
                            case 0: {                            // Tank type presets the stats
                                int n = (int)TankType::COUNT;
                                s.panelType = s.panelType < 0 ? (dir > 0 ? 0 : n - 1)
                                                              : (s.panelType + n + dir) % n;
                                // Player must preset TRUE defaults even while an
                                // override from a previous cheated run is active
                                s.custom = s.panelType == 0 ? playerDefaults
                                                            : paramsFor((TankType)s.panelType);
                                break;
                            }
                            case 1: s.custom.moveSpeed = clampf(s.custom.moveSpeed + dir * 0.25f, 0.5f, 6.0f); break;
                            case 2: s.custom.shellSpeed = clampf(s.custom.shellSpeed + dir * 0.5f, 1.0f, 12.0f); break;
                            case 3: s.custom.maxShells = ci(s.custom.maxShells + dir, 1, 10); break;
                            case 4: s.custom.bounces = ci(s.custom.bounces + dir, 0, 5); break;
                            case 5: s.custom.fireCooldown = clampf(s.custom.fireCooldown + dir * 0.05f, 0.0f, 1.0f); break;
                            case 6: s.custom.maxMines = ci(s.custom.maxMines + dir, 0, 8); break;
                        }
                    }
                }
                if (!s.dev && !s.tankEdit && s.savedMission > 0) {
                    // continue-from picker: any beaten mission plus the next one
                    int n = (s.savedMission < MISSION_COUNT ? s.savedMission : MISSION_COUNT - 1) + 1;
                    if (s.contMission >= n) s.contMission = n - 1;
                    int dir = (IsKeyPressed(KEY_RIGHT) ? 1 : 0) - (IsKeyPressed(KEY_LEFT) ? 1 : 0);
                    if (dir != 0) s.contMission = (s.contMission + n + dir) % n;
                }
                if (IsKeyPressed(KEY_V) && loadReplay(s.play, replayPath())) {
                    s.playIdx = 0;
                    s.playAim = {0, 0};
                    s.mission = s.play.mission;
                    s.world = replayStartWorld(s.play);
                    if (s.play.hasOverride) setPlayerOverride(s.play.override_);
                    else clearPlayerOverride();
                    s.fx = Effects{};
                    s.treadAnchors.clear();
                    for (auto& tk : s.world.tanks) s.treadAnchors.push_back(tk.pos);
                    s.treadPhase.assign(s.world.tanks.size(), 0);
                    rendererSetArena(renderer, s.world.arena);
                    s.phase = Phase::Replay;
                    acc = 0;
                    break;
                }
                bool start = IsKeyPressed(KEY_ENTER);
                bool cont = s.savedMission > 0 && IsKeyPressed(KEY_C);
                if (start || cont) {
                    const EnemyParams& d = playerDefaults;
                    s.cheated = s.custom.moveSpeed != d.moveSpeed ||
                                s.custom.shellSpeed != d.shellSpeed ||
                                s.custom.maxShells != d.maxShells ||
                                s.custom.bounces != d.bounces ||
                                s.custom.fireCooldown != d.fireCooldown ||
                                s.custom.maxMines != d.maxMines;
                    if (s.cheated) setPlayerOverride(s.custom);
                    else clearPlayerOverride();
                    if (start) s.mission = s.dev ? s.devMission : 0;
                    else s.mission = s.contMission;
                    s.startMission = s.mission;
                    s.contMission = s.savedMission;   // next title visit: furthest again
                    s.lives = START_LIVES; s.stats = CampaignStats{};
                    enterIntro(s, renderer);
                }
                break;
            }
            case Phase::Intro:
                if (IsKeyPressed(KEY_P)) {          // pausable before the level starts too
                    s.pausedFrom = Phase::Intro;
                    s.phase = Phase::Paused;
                    break;
                }
                if (s.phaseTimer <= 0 || anyKey) { s.phase = Phase::Playing; acc = 0; }
                break;
            case Phase::Playing: {
                if (IsKeyPressed(KEY_P)) {          // pause: freeze the whole simulation
                    fireBuf.consume(); mineBuf.consume();
                    s.pausedFrom = Phase::Playing;
                    s.phase = Phase::Paused;
                    break;
                }
                if (in.fire) fireBuf.press();       // buffer clicks: a press must never be
                if (in.mine) mineBuf.press();       // lost to frame/subtick misalignment
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
                    fireBuf.hold();                 // held click stays queued until it fires once
                acc += frameDt;
                if (acc > 0.25f) acc = 0.25f;
                while (acc >= DT) {
                    InputState tick = in;
                    tick.fire = fireBuf.pending();
                    tick.mine = mineBuf.pending();
                    replayRecordTick(s.rec, tick);
                    int beforeAlive = s.world.enemiesAlive();
                    float ftPrev = s.world.playerAlive() ? s.world.tanks[0].fireTimer : 0;
                    int mlPrev = s.world.playerAlive() ? s.world.tanks[0].minesLive : 0;
                    simTick(s.world, tick, DT);
                    if (s.world.playerAlive()) {
                        // fireTimer only jumps up on a successful shot; minesLive on a lay
                        if (s.world.tanks[0].fireTimer > ftPrev) fireBuf.consume();
                        if (s.world.tanks[0].minesLive > mlPrev) mineBuf.consume();
                    }
                    fireBuf.tick(DT);
                    mineBuf.tick(DT);
                    s.stats.kills[s.mission] += beforeAlive - s.world.enemiesAlive();
                    acc -= DT;
                }
                if (s.world.enemiesAlive() == 0) {
                    saveReplay(s.rec, replayPath());                // keep the last attempt
                    if ((s.mission + 1) % 5 == 0) s.lives++;        // spec: +1 life every 5
                    if (!s.dev && !s.cheated && s.mission + 1 > s.savedMission) {
                        s.savedMission = s.mission + 1;             // dev runs never touch the save
                        s.contMission = s.savedMission;
                        saveProgress(s.savedMission);
                    }
                    s.phase = Phase::Clear; s.phaseTimer = 2.0f;
                } else if (!s.world.playerAlive()) {
                    saveReplay(s.rec, replayPath());                // deaths are replays too
                    s.stats.deaths++; s.lives--;
                    s.phase = Phase::Death; s.phaseTimer = 2.0f;
                }
                break;
            }
            case Phase::Paused:
                if (IsKeyPressed(KEY_P)) { s.phase = s.pausedFrom; acc = 0; }
                if (IsKeyPressed(KEY_Q)) s.phase = Phase::Title;    // abandon the run
                break;
            case Phase::Replay: {
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_V) || IsKeyPressed(KEY_Q) ||
                    s.playIdx >= s.play.ticks.size()) {
                    clearPlayerOverride();
                    s.phase = Phase::Title;
                    break;
                }
                acc += frameDt;
                if (acc > 0.25f) acc = 0.25f;
                while (acc >= DT && s.playIdx < s.play.ticks.size()) {
                    InputState rin = replayInput(s.play, s.playIdx++);
                    s.playAim = rin.aimWorld;
                    simTick(s.world, rin, DT);
                    acc -= DT;
                }
                break;
            }
            case Phase::Clear:
                if (s.phaseTimer <= 0) {
                    if (s.dev) s.phase = Phase::Title;      // dev: one mission, then back
                    else if (s.mission == MISSION_COUNT - 1) { s.won = true; s.phase = Phase::Results; }
                    else { s.mission++; enterIntro(s, renderer); }
                }
                break;
            case Phase::Death:
                if (s.phaseTimer <= 0) {
                    if (s.lives > 0) enterIntro(s, renderer, true);   // retry keeps destroyed crates
                    else { s.phase = Phase::GameOver; s.phaseTimer = 2.5f; }
                }
                break;
            case Phase::GameOver:
                if (s.phaseTimer <= 0) {
                    if (s.dev) s.phase = Phase::Title;      // dev: skip the results screen
                    else { s.won = false; s.phase = Phase::Results; }
                }
                break;
            case Phase::Results:
                if (IsKeyPressed(KEY_ENTER)) s.phase = Phase::Title;
                break;
        }

        if (s.phase == Phase::Playing || s.phase == Phase::Clear ||
            s.phase == Phase::Death || s.phase == Phase::GameOver ||
            s.phase == Phase::Replay)
            tickEffects(s, renderer, frameDt);

        bool typeAlive[(int)TankType::COUNT] = {false};
        bool musicOn = (s.phase == Phase::Playing || s.phase == Phase::Replay) &&
                       s.world.playerAlive();
        if (musicOn)
            for (auto& tk : s.world.tanks)
                if (tk.alive) typeAlive[(int)tk.type] = true;
        audioMusicTick(frameDt, typeAlive, musicOn);
        float bed = 0;
        if (musicOn)
            for (size_t i = 0; i < s.world.tanks.size(); ++i)
                if (s.world.tanks[i].alive && vlen(s.world.tanks[i].vel) > 0.2f)
                    bed += i == 0 ? 0.6f : 0.15f;
        audioTreadBed(bed);

        BeginDrawing();
        switch (s.phase) {
            case Phase::Title:
                drawTitle(s.savedMission, s.contMission,
                          {s.dev, s.devMission, s.tankEdit, s.editRow, s.panelType, s.custom});
                break;
            case Phase::Intro:   drawMissionIntro(s.mission, s.world.enemiesAlive(), s.lives); break;
            case Phase::Playing: {
                drawScene(s, renderer, in.aimWorld);
                int score = 0;
                for (int k : s.stats.kills) score += k;
                drawHud(s.mission, s.world.enemiesAlive(), score);
                if (s.world.playerAlive()) drawReticle(renderer, in.aimWorld);
                break;
            }
            case Phase::Paused:
                if (s.pausedFrom == Phase::Intro)   // no free scouting: keep the card up
                    drawMissionIntro(s.mission, s.world.enemiesAlive(), s.lives);
                else
                    drawScene(s, renderer, in.aimWorld);
                drawPaused();
                break;
            case Phase::Replay:
                drawScene(s, renderer, s.playAim);
                if (s.world.playerAlive()) drawReticle(renderer, s.playAim);
                drawReplayBanner(s.mission);
                break;
            case Phase::Clear:
                drawScene(s, renderer, in.aimWorld);
                drawMissionClear(s.mission);
                break;
            case Phase::Death:
                drawScene(s, renderer, in.aimWorld);
                drawPlayerDeath(s.lives);
                break;
            case Phase::GameOver:
                drawScene(s, renderer, in.aimWorld);
                drawGameOver();
                break;
            case Phase::Results: drawResults(s.stats, s.startMission, s.mission, s.won); break;
        }
        EndDrawing();
    }
    rendererUnload(renderer);
    audioShutdown();
    CloseWindow();
    return 0;
}
