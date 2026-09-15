#include "combat.h"
#include "physics.h"
#include "ai.h"
#include "player.h"
#include <algorithm>

static EnemyParams g_playerOverride;
static bool g_playerCustom = false;

void setPlayerOverride(const EnemyParams& p) { g_playerOverride = p; g_playerCustom = true; }
void clearPlayerOverride() { g_playerCustom = false; }

const EnemyParams& paramsFor(TankType t) {
    if (t == TankType::Player && g_playerCustom) return g_playerOverride;
    // moveSpeed shellSpeed maxShells bounces cooldown maxMines steering aim aimBounces dodges lead
    static const EnemyParams table[(int)TankType::COUNT] = {
        /*Player*/ {PLAYER_SPEED, SHELL_NORMAL, PLAYER_MAX_SHELLS, 1, PLAYER_FIRE_COOLDOWN,
                    PLAYER_MAX_MINES, Steering::None, AimStyle::Direct, 0, false, false},
        // stats per StrategyWiki (more granular than the fandom table where they conflict):
        // speeds Low=1.0 Normal=2.2(player parity) High=2.6 Extreme=3.0; Brown turret only
        // SWEEPS ("does not actively seek"); Black leads its shots and plays defensively
        /*Brown */ {0.0f,          SHELL_NORMAL, 1, 1, 2.5f, 0, Steering::None,          AimStyle::Scan,     1, true,  false},
        /*Grey  */ {MOVE_SLOW,     SHELL_NORMAL, 1, 1, 2.0f, 0, Steering::FleeWhenClose, AimStyle::Ricochet, 1, true,  false},
        /*Teal  */ {MOVE_SLOW,     SHELL_FAST,   1, 0, 3.0f, 0, Steering::FleeWhenClose, AimStyle::Direct,   0, true,  false},
        /*Yellow*/ {MOVE_VERYFAST, SHELL_NORMAL, 1, 1, 2.0f, 4, Steering::Evade,         AimStyle::Direct,   0, false, false},
        /*Red   */ {MOVE_FAST,     SHELL_NORMAL, 3, 1, 1.0f, 0, Steering::Chase,         AimStyle::Direct,   0, true,  false},
        /*Green */ {0.0f,          SHELL_FAST,   2, 2, 1.5f, 0, Steering::None,          AimStyle::Ricochet, 2, true,  true},
        /*Purple*/ {MOVE_VERYFAST, SHELL_NORMAL, 5, 1, 0.9f, 2, Steering::Chase,         AimStyle::Ricochet, 1, true,  false},
        /*White */ {MOVE_FAST,     SHELL_NORMAL, 5, 1, 0.9f, 2, Steering::Chase,         AimStyle::Ricochet, 1, true,  false},
        /*Black */ {MOVE_EXTREME,  SHELL_FAST,   2, 0, 1.2f, 2, Steering::FleeWhenClose, AimStyle::Direct,   0, true,  true},
    };
    return table[(int)t];
}

bool tryFire(World& w, int ti) {
    Tank& t = w.tanks[ti];
    const EnemyParams& p = paramsFor(t.type);
    if (!t.alive || t.fireTimer > 0 || t.shellsLive >= p.maxShells) return false;
    Vec2 dir{std::cos(t.turretAngle), std::sin(t.turretAngle)};
    Shell s;
    s.pos = t.pos + dir * BARREL_LEN;
    s.vel = dir * p.shellSpeed;
    s.bouncesLeft = p.bounces;
    s.owner = ti;
    w.shells.push_back(s);
    t.shellsLive++;
    t.fireTimer = p.fireCooldown;
    w.events.push_back({GameEvent::Fire, s.pos, t.turretAngle});
    return true;
}

void killTank(World& w, int ti) {
    Tank& t = w.tanks[ti];
    if (!t.alive) return;
    t.alive = false;
    w.events.push_back({GameEvent::TankExplode, t.pos});
}

void killShell(World& w, Shell& s, bool poof) {
    if (!s.alive) return;
    s.alive = false;
    w.tanks[s.owner].shellsLive--;
    if (poof) w.events.push_back({GameEvent::ShellPoof, s.pos});
}

static void stepShell(World& w, Shell& s, float dt) {
    float tleft = dt;
    for (int iter = 0; iter < 3 && tleft > 1e-6f && s.alive; ++iter) {
        Vec2 to = s.pos + s.vel * tleft;
        SweepHit h = sweepSegment(w.arena, s.pos, to);
        Vec2 end = h.hit ? h.pos : to;
        for (size_t ti = 0; ti < w.tanks.size(); ++ti) {   // tank hits along the sub-segment
            Tank& t = w.tanks[ti];
            if (!t.alive) continue;
            if (segPointDist(s.pos, end, t.pos) < TANK_RADIUS + SHELL_RADIUS) {
                killTank(w, (int)ti);
                killShell(w, s, false);
                return;
            }
        }
        if (!h.hit) { s.pos = to; return; }
        float seg = vlen(end - s.pos), full = vlen(to - s.pos);
        tleft *= full > 1e-6f ? (1.0f - seg / full) : 0.0f;
        s.pos = end + h.normal * 0.001f;
        bool degenerate = (h.normal.x == 0 && h.normal.y == 0);
        bool crate = w.arena.at(h.cx, h.cy) == Block::Crate;
        if (!degenerate && !crate && s.bouncesLeft > 0) {
            s.bouncesLeft--;
            s.vel = reflect(s.vel, h.normal);
            w.events.push_back({GameEvent::Bounce, s.pos});
        } else {
            killShell(w, s, true);
        }
    }
}

void updateShells(World& w, float dt) {
    for (auto& s : w.shells) if (s.alive) stepShell(w, s, dt);
    for (size_t i = 0; i < w.shells.size(); ++i) {          // shell vs shell
        for (size_t j = i + 1; j < w.shells.size(); ++j) {
            Shell &A = w.shells[i], &B = w.shells[j];
            if (!A.alive || !B.alive) continue;
            if (vlen(A.pos - B.pos) < 2 * SHELL_RADIUS) {
                killShell(w, A, true);
                killShell(w, B, true);
            }
        }
    }
    for (auto& s : w.shells) {              // shell vs mine
        if (!s.alive) continue;
        for (auto& m : w.mines) {
            if (m.alive && vlen(s.pos - m.pos) < MINE_BODY_RADIUS + SHELL_RADIUS) {
                explodeMine(w, m);
                if (s.alive) killShell(w, s, false);   // blast may already have taken it
                break;
            }
        }
    }
    w.shells.erase(std::remove_if(w.shells.begin(), w.shells.end(),
                                  [](const Shell& s) { return !s.alive; }),
                   w.shells.end());
}

bool tryLayMine(World& w, int ti) {
    Tank& t = w.tanks[ti];
    if (!t.alive || t.minesLive >= paramsFor(t.type).maxMines) return false;
    w.mines.push_back({t.pos, ti, 0.0f, true});
    t.minesLive++;
    w.events.push_back({GameEvent::MineLay, t.pos});
    return true;
}

void explodeMine(World& w, Mine& m) {
    if (!m.alive) return;
    m.alive = false;
    w.tanks[m.owner].minesLive--;
    w.events.push_back({GameEvent::MineExplode, m.pos});
    for (size_t i = 0; i < w.tanks.size(); ++i) {
        if (w.tanks[i].alive && vlen(w.tanks[i].pos - m.pos) < MINE_BLAST_RADIUS + TANK_RADIUS)
            killTank(w, (int)i);
    }
    for (auto& s : w.shells) {
        if (s.alive && vlen(s.pos - m.pos) < MINE_BLAST_RADIUS) killShell(w, s, false);
    }
    for (auto& other : w.mines) {           // chain: promote, detonates next update
        if (other.alive && &other != &m && vlen(other.pos - m.pos) < MINE_BLAST_RADIUS)
            other.age = MINE_LIFETIME;
    }
    int x0 = (int)std::floor(m.pos.x - MINE_BLAST_RADIUS), x1 = (int)std::floor(m.pos.x + MINE_BLAST_RADIUS);
    int y0 = (int)std::floor(m.pos.y - MINE_BLAST_RADIUS), y1 = (int)std::floor(m.pos.y + MINE_BLAST_RADIUS);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            if (w.arena.at(x, y) != Block::Crate) continue;
            Vec2 c{x + 0.5f, y + 0.5f};
            if (vlen(c - m.pos) < MINE_BLAST_RADIUS + 0.5f) {
                w.arena.set(x, y, Block::Empty);
                w.events.push_back({GameEvent::CrateBreak, c});
            }
        }
    }
}

void updateMines(World& w, float dt) {
    for (auto& m : w.mines) {
        if (!m.alive) continue;
        m.age += dt;
        bool boom = m.age >= MINE_LIFETIME;
        if (!boom && m.age >= MINE_ARM_TIME) {
            for (auto& t : w.tanks) {
                if (t.alive && vlen(t.pos - m.pos) < MINE_TRIGGER_RADIUS + TANK_RADIUS) { boom = true; break; }
            }
        }
        if (boom) explodeMine(w, m);
    }
    w.mines.erase(std::remove_if(w.mines.begin(), w.mines.end(),
                                 [](const Mine& m) { return !m.alive; }),
                  w.mines.end());
}

void applyIntents(World& w, const std::vector<Intent>& intents, float dt) {
    for (size_t i = 0; i < w.tanks.size(); ++i) {
        Tank& t = w.tanks[i];
        if (!t.alive) continue;
        const Intent& in = intents[i];
        const EnemyParams& p = paramsFor(t.type);
        Vec2 prev = t.pos;
        t.moveFreeze -= dt;
        if (p.moveSpeed > 0 && t.moveFreeze <= 0) moveTank(t, in.move, p.moveSpeed, w.arena, dt);
        t.vel = (t.pos - prev) * (1.0f / dt);
        if (i == 0) {
            t.turretAngle = in.aim;                          // player turret snaps to mouse
        } else {
            float err = wrapAngle(in.aim - t.turretAngle);
            float step = TURRET_TURN_SPEED * dt;
            t.turretAngle = wrapAngle(t.turretAngle + clampf(err, -step, step));
        }
        t.fireTimer -= dt;
        if (in.fire && tryFire(w, (int)i)) t.moveFreeze = FIRE_MOVE_PAUSE;
        if (in.layMine) tryLayMine(w, (int)i);
    }
}

void simTick(World& w, const InputState& input, float dt) {
    std::vector<Intent> intents(w.tanks.size());
    if (w.playerAlive()) intents[0] = playerIntent(w.tanks[0], input);
    for (size_t i = 1; i < w.tanks.size(); ++i)
        if (w.tanks[i].alive) intents[i] = aiThink(w, (int)i, dt);
    applyIntents(w, intents, dt);
    separateTanks(w.tanks, w.arena);
    updateShells(w, dt);
    updateMines(w, dt);
}
