#include "ai.h"
#include "physics.h"
#include <algorithm>
#include <queue>

std::vector<Vec2> findPath(const Arena& a, Vec2 from, Vec2 to,
                           const std::vector<Vec2>& avoid, float avoidRadius) {
    int sx = (int)from.x, sy = (int)from.y, gx = (int)to.x, gy = (int)to.y;
    std::vector<Vec2> out;
    if (a.blocksTank(gx, gy) || a.blocksTank(sx, sy)) return out;
    if (sx == gx && sy == gy) return out;
    auto nearHazard = [&](int x, int y) {
        if (avoid.empty() || avoidRadius <= 0) return false;
        Vec2 c{x + 0.5f, y + 0.5f};
        for (auto& hz : avoid)
            if (vlen(c - hz) < avoidRadius) return true;
        return false;
    };
    const int W = a.w, H = a.h;
    std::vector<int> came(W * H, -1), cost(W * H, 1 << 28);
    auto idx = [W](int x, int y) { return y * W + x; };
    auto heur = [&](int x, int y) { return std::abs(x - gx) + std::abs(y - gy); };
    using QN = std::pair<int, int>;                        // (f, cell)
    std::priority_queue<QN, std::vector<QN>, std::greater<QN>> open;
    cost[idx(sx, sy)] = 0;
    open.push({heur(sx, sy), idx(sx, sy)});
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    while (!open.empty()) {
        auto [f, cur] = open.top(); open.pop();
        int cx = cur % W, cy = cur / W;
        if (cx == gx && cy == gy) break;
        for (int k = 0; k < 4; ++k) {
            int nx = cx + dx[k], ny = cy + dy[k];
            if (nx < 0 || ny < 0 || nx >= W || ny >= H || a.blocksTank(nx, ny)) continue;
            if (nearHazard(nx, ny) && !(nx == gx && ny == gy)) continue;   // path around mines
            int nc = cost[cur] + 1;
            if (nc < cost[idx(nx, ny)]) {
                cost[idx(nx, ny)] = nc;
                came[idx(nx, ny)] = cur;
                open.push({nc + heur(nx, ny), idx(nx, ny)});
            }
        }
    }
    if (came[idx(gx, gy)] < 0) return out;
    for (int cur = idx(gx, gy); cur != idx(sx, sy); cur = came[cur])
        out.push_back({cur % W + 0.5f, cur / W + 0.5f});
    std::reverse(out.begin(), out.end());
    return out;
}

static Vec2 randomOpenCell(World& w, Vec2 nearTo, float minDist, float maxDist) {
    for (int tries = 0; tries < 40; ++tries) {
        int x = w.rng.irange(1, w.arena.w - 2);
        int y = w.rng.irange(1, w.arena.h - 2);
        if (w.arena.blocksTank(x, y)) continue;
        Vec2 c{x + 0.5f, y + 0.5f};
        float d = vlen(c - nearTo);
        if (d >= minDist && d <= maxDist) return c;
    }
    return nearTo;
}

static Vec2 followPath(Tank& t) {
    while (t.pathIdx < (int)t.path.size() && vlen(t.path[t.pathIdx] - t.pos) < 0.2f)
        t.pathIdx++;
    if (t.pathIdx >= (int)t.path.size()) return {0, 0};
    return norm(t.path[t.pathIdx] - t.pos);
}

static void replanTo(World& w, Tank& t, Vec2 goal, const std::vector<Vec2>& hazards) {
    t.goal = goal;
    t.path = findPath(w.arena, t.pos, goal, hazards, MINE_PATH_RADIUS);
    if (t.path.empty() && !hazards.empty())            // minefield seals the route: brave it
        t.path = findPath(w.arena, t.pos, goal);       // rather than freeze (repulsion still guards)
    t.pathIdx = 0;
}

// returns true if the shot from `shooter` at `angle` reaches `target` (usually
// the player's position; a led prediction point for Green). acceptPlayerHit
// also counts a direct hit on the player's body — disabled for the led pass so
// prediction takes priority. sets hitsFriendly if a fellow enemy is hit first.
static bool traceShot(const World& w, int shooter, Vec2 target, float angle, int bounces,
                      float range, bool& hitsFriendly, bool acceptPlayerHit) {
    hitsFriendly = false;
    Vec2 dir{std::cos(angle), std::sin(angle)};
    Vec2 p = w.tanks[shooter].pos + dir * BARREL_LEN;
    float remaining = range;
    for (int b = 0; b <= bounces; ++b) {
        Vec2 end = p + dir * remaining;
        SweepHit h = sweepSegment(w.arena, p, end);
        Vec2 segEnd = h.hit ? h.pos : end;
        float bestT = 1e9f;
        int bestTank = -1;
        for (size_t i = 0; i < w.tanks.size(); ++i) {       // earliest tank on this leg
            // the shooter is NOT excluded: a bounced path returning through your
            // own hull is a suicide shot, rejected like a friendly hit. (The
            // outgoing leg starts BARREL_LEN=0.55 from center, outside the 0.45
            // hit radius and moving away, so it can't false-positive on itself.)
            if (!w.tanks[i].alive) continue;
            if (segPointDist(p, segEnd, w.tanks[i].pos) < TANK_RADIUS + SHELL_RADIUS) {
                float tt = dot(w.tanks[i].pos - p, dir);
                if (tt >= 0 && tt < bestT) { bestT = tt; bestTank = (int)i; }
            }
        }
        float targetT = 1e9f;                               // does this leg reach the aim point?
        if (segPointDist(p, segEnd, target) < TANK_RADIUS + SHELL_RADIUS)
            targetT = fmaxf(dot(target - p, dir), 0.0f);
        if (targetT < bestT) return true;
        if (bestTank == 0) return acceptPlayerHit;          // body hit: only counts when allowed
        if (bestTank > 0) { hitsFriendly = true; return false; }
        if (!h.hit) return false;
        remaining -= vlen(segEnd - p);
        if (remaining <= 0) return false;
        if (w.arena.at(h.cx, h.cy) == Block::Crate) return false;   // shells die on crates
        if (h.normal.x == 0 && h.normal.y == 0) return false;
        dir = reflect(dir, h.normal);
        p = segEnd + h.normal * 0.001f;
    }
    return false;
}

bool hasDirectShot(const Arena& a, Vec2 from, Vec2 target) {
    return !sweepSegment(a, from, target).hit;
}

bool pathHitsFriendly(const World& w, int shooter, float angle, int bounces, float range) {
    bool friendly = false;
    traceShot(w, shooter, w.tanks[0].pos, angle, bounces, range, friendly, true);
    return friendly;
}

bool findRicochetAim(const World& w, int shooter, Vec2 target, int bounces, float range,
                     float& outAngle, bool acceptPlayerHit) {
    for (int k = 0; k < RICOCHET_SAMPLES; ++k) {
        float ang = (2 * PI_F * k) / RICOCHET_SAMPLES;
        bool friendly = false;
        if (traceShot(w, shooter, target, ang, bounces, range, friendly, acceptPlayerHit) &&
            !friendly) {
            outAngle = ang;
            return true;
        }
    }
    return false;
}

Intent aiThink(World& w, int ti, float dt) {
    Intent in;
    Tank& t = w.tanks[ti];
    const EnemyParams& p = paramsFor(t.type);
    Vec2 playerPos = w.tanks[0].pos;
    t.replanTimer -= dt;
    t.wanderTimer -= dt;

    std::vector<Vec2> hazards;                          // mines this tank can sense
    for (auto& m : w.mines) {
        if (!m.alive) continue;
        if (m.owner == 0 && p.maxMines == 0) continue;  // non-miners can't detect player mines
        hazards.push_back(m.pos);
    }

    switch (p.steering) {
        case Steering::None: break;
        case Steering::Chase:
            if (t.replanTimer <= 0) { replanTo(w, t, playerPos, hazards); t.replanTimer = REPLAN_INTERVAL; }
            in.move = followPath(t);
            break;
        case Steering::Wander:
            if (t.wanderTimer <= 0 || t.pathIdx >= (int)t.path.size()) {
                replanTo(w, t, randomOpenCell(w, t.pos, 2.0f, 6.0f), hazards);
                t.wanderTimer = w.rng.range(2.0f, 4.0f);
            }
            in.move = followPath(t);
            break;
        case Steering::Evade:
            if (t.replanTimer <= 0 || t.pathIdx >= (int)t.path.size() ||
                vlen(playerPos - t.pos) < 3.0f) {
                replanTo(w, t, randomOpenCell(w, playerPos, 6.0f, 100.0f), hazards);
                t.replanTimer = REPLAN_INTERVAL * 2;
            }
            in.move = followPath(t);
            break;
        case Steering::FleeWhenClose: {
            float d = vlen(playerPos - t.pos);
            if (d < 5.0f) {
                Vec2 away = norm(t.pos - playerPos);
                Vec2 target = t.pos + away * 3.0f;
                target.x = clampf(target.x, 1.5f, w.arena.w - 1.5f);
                target.y = clampf(target.y, 1.5f, w.arena.h - 1.5f);
                if (t.replanTimer <= 0) { replanTo(w, t, target, hazards); t.replanTimer = REPLAN_INTERVAL; }
                in.move = followPath(t);
                if (vlen(in.move) < 0.1f) in.move = away;   // no path? back straight up
            } else if (t.wanderTimer <= 0 || t.pathIdx >= (int)t.path.size()) {
                replanTo(w, t, randomOpenCell(w, t.pos, 1.0f, 3.0f), hazards);
                t.wanderTimer = w.rng.range(3.0f, 5.0f);
                in.move = followPath(t);
            } else {
                in.move = followPath(t);
            }
            break;
        }
    }

    // threat avoidance overrides pathing (Yellow is "Incautious": never dodges)
    if (p.dodgesShells) {
        for (auto& s : w.shells) {
            if (!s.alive || s.owner == ti) continue;
            Vec2 ahead = s.pos + norm(s.vel) * 3.0f;
            if (segPointDist(s.pos, ahead, t.pos) < DODGE_RADIUS && p.moveSpeed > 0) {
                Vec2 dir = norm(s.vel);
                Vec2 perp{-dir.y, dir.x};
                if (dot(perp, t.pos - s.pos) < 0) perp = perp * -1.0f;   // dodge to the open side
                in.move = perp;
            }
        }
    }
    // mine repulsion: WEIGHTED SUM over every sensed mine — a per-mine override
    // would flee one mine straight into its neighbor in a cluster
    if (p.moveSpeed > 0 && !hazards.empty()) {
        Vec2 flee{0, 0};
        bool anyNear = false;
        for (auto& hz : hazards) {
            float d = vlen(hz - t.pos);
            if (d >= MINE_AVOID_RADIUS) continue;
            anyNear = true;
            Vec2 away = t.pos - hz;
            // a just-laid mine sits exactly at the layer's position: keep driving
            if (d < 0.05f) away = {std::cos(t.bodyAngle), std::sin(t.bodyAngle)};
            flee = flee + norm(away) * (MINE_AVOID_RADIUS - d);   // closer repels harder
        }
        if (anyNear)
            in.move = vlen(flee) > 0.01f ? norm(flee)
                      : Vec2{std::cos(t.bodyAngle), std::sin(t.bodyAngle)};
    }

    // ---- mine deployment (Yellow rapidly; Purple/White/Black occasionally) ----
    if (p.maxMines > 0 && p.moveSpeed > 0) {
        if (t.mineTimer == 0.0f)                            // unseeded: stagger the first lay
            t.mineTimer = w.rng.range(1.5f, 5.0f);          // (no mines before the fight starts)
        t.mineTimer -= dt;
        // never lay unless the escape run ahead is genuinely drivable: the layer
        // flees along its heading, and a wall inside the trigger radius is death
        bool clearAhead = true;
        Vec2 heading{std::cos(t.bodyAngle), std::sin(t.bodyAngle)};
        for (float step = 0.5f; step <= 2.0f && clearAhead; step += 0.5f)
            if (circleHitsGrid(w.arena, t.pos + heading * step, TANK_RADIUS)) clearAhead = false;
        bool spaced = true;                                 // keep mines apart: clusters
        for (auto& hz : hazards)                            // pocket their owner in
            if (vlen(hz - t.pos) < 3.2f) { spaced = false; break; }
        if (t.mineTimer <= 0 && t.minesLive < p.maxMines && vlen(in.move) > 0.1f &&
            clearAhead && spaced &&
            vlen(playerPos - t.pos) > MINE_BLAST_RADIUS + 1.0f) {
            in.layMine = true;
            t.mineTimer = t.type == TankType::Yellow ? w.rng.range(2.5f, 4.0f)
                                                     : w.rng.range(6.0f, 9.0f);
        }
    }

    // ---- aiming & trigger discipline ----
    // wiki: only Green predicts the player's movement; everyone else aims at
    // the current position
    t.aimReplanTimer -= dt;
    t.fireDelay -= dt;
    if (w.playerAlive()) {
        Vec2 target = playerPos;
        if (p.leadAim) {
            float dist = vlen(playerPos - t.pos);
            target = playerPos + w.tanks[0].vel * (dist / p.shellSpeed * 0.75f);
        }
        if (p.aim == AimStyle::Scan) {
            // Brown: the turret does not seek — it sweeps randomly and fires
            // whenever the current heading happens to reach the player
            if (t.wanderTimer <= 0) {                       // occasionally reverse the sweep
                t.scanDir = w.rng.next01() < 0.5f ? -1.0f : 1.0f;
                t.wanderTimer = w.rng.range(2.0f, 5.0f);
            }
            t.desiredAim = wrapAngle(t.desiredAim + t.scanDir * BROWN_SCAN_RATE * dt);
            bool fr = false;
            t.hasShot = traceShot(w, ti, playerPos, t.turretAngle, p.aimBounces,
                                  RICOCHET_RANGE, fr, true) && !fr;
        } else if (p.aim == AimStyle::Direct) {
            if (hasDirectShot(w.arena, t.pos, target)) {
                t.desiredAim = angleTo(t.pos, target); t.hasShot = true;
            } else t.hasShot = false;
        } else {                                            // AimStyle::Ricochet
            if (t.aimReplanTimer <= 0) {
                float ang;
                if (p.leadAim) {
                    // Green: predicted point takes priority (through its 2 ricochets);
                    // only fall back to a current-position shot if no led path exists
                    t.hasShot = findRicochetAim(w, ti, target, p.aimBounces, RICOCHET_RANGE,
                                                ang, false);
                    if (!t.hasShot)
                        t.hasShot = findRicochetAim(w, ti, playerPos, p.aimBounces,
                                                    RICOCHET_RANGE, ang, true);
                } else {
                    t.hasShot = findRicochetAim(w, ti, target, p.aimBounces, RICOCHET_RANGE,
                                                ang, true);
                }
                if (t.hasShot) t.desiredAim = ang;
                t.aimReplanTimer = 0.3f;
            }
        }
    } else t.hasShot = false;

    in.aim = p.aim == AimStyle::Scan
                 ? t.desiredAim                                     // sweepers never track
                 : (t.hasShot ? t.desiredAim : angleTo(t.pos, playerPos));
    float aimErr = std::fabs(wrapAngle(t.desiredAim - t.turretAngle));
    if (t.hasShot && aimErr < AIM_FIRE_TOLERANCE && t.fireTimer <= 0 && t.fireDelay <= 0 &&
        !pathHitsFriendly(w, ti, t.turretAngle, p.aimBounces, RICOCHET_RANGE)) {
        in.fire = true;
        t.fireDelay = w.rng.range(0.1f, 0.4f);              // desync volleys
    }

    return in;
}
