#include "test_framework.h"
#include "arena.h"
#include "ai.h"
#include <cmath>

TEST(aim_direct_shot_open_and_blocked) {
    LevelDef d{{
        "#########",
        "#.......#",
        "#B..#..P#",
        "#.......#",
        "#########",
    }};
    Arena a = parseLevel(d).arena;
    CHECK(!hasDirectShot(a, {1.5f, 2.5f}, {7.5f, 2.5f}));   // wall cell (4,2) blocks
    CHECK(hasDirectShot(a, {1.5f, 1.5f}, {7.5f, 1.5f}));    // open row above it
}

TEST(aim_finds_one_bounce_solution) {
    LevelDef d{{
        "#########",
        "#.......#",
        "#B..#..P#",
        "#.......#",
        "#########",
    }};
    World w = makeWorld(d);
    float ang = 0;
    CHECK(findRicochetAim(w, 1, w.tanks[0].pos, 1, RICOCHET_RANGE, ang));
    CHECK(std::fabs(std::sin(ang)) > 0.1f);                 // must be a banked shot, not straight
}

TEST(aim_no_solution_when_sealed) {
    LevelDef d{{
        "#########",
        "#...#...#",
        "#B..#..P#",
        "#...#...#",
        "#########",
    }};
    World w = makeWorld(d);
    float ang = 0;
    CHECK(!findRicochetAim(w, 1, w.tanks[0].pos, 1, RICOCHET_RANGE, ang));  // full wall: no 1-bounce path
}

TEST(aim_friendly_blocks_shot) {
    LevelDef d{{
        "###########",
        "#P...G...B#",
        "###########",
    }};
    World w = makeWorld(d);
    int shooter = 2;                                        // the Brown at the right end
    CHECK(w.tanks[shooter].type == TankType::Brown);
    CHECK(pathHitsFriendly(w, shooter, PI_F, 1, RICOCHET_RANGE));   // Grey sits in the way
    float ang = 0;
    // a needle-threading bounce over G's collision circle can legitimately exist;
    // the contract is that any returned solution must avoid the friendly
    if (findRicochetAim(w, shooter, w.tanks[0].pos, 1, RICOCHET_RANGE, ang)) {
        CHECK(!pathHitsFriendly(w, shooter, ang, 1, RICOCHET_RANGE));
    }
}

TEST(aim_brown_eventually_fires) {
    LevelDef d{{
        "##########",
        "#........#",
        "#P.....B.#",
        "#........#",
        "##########",
    }};
    World w = makeWorld(d);
    w.tanks[1].turretAngle = PI_F / 2;                      // start aiming the wrong way
    bool fired = false;
    for (int i = 0; i < (int)(12.0f / DT) && !fired; ++i) {   // sweeping turret needs a revolution
        Intent in = aiThink(w, 1, DT);
        // mimic applyIntents: rotate turret toward requested aim, tick cooldown
        float err = wrapAngle(in.aim - w.tanks[1].turretAngle);
        float step = TURRET_TURN_SPEED * DT;
        w.tanks[1].turretAngle += clampf(err, -step, step);
        w.tanks[1].fireTimer -= DT;
        fired = in.fire;
    }
    CHECK(fired);
}

TEST(aim_does_not_fire_without_solution) {
    LevelDef d{{
        "#########",
        "#...#...#",
        "#P..#..B#",
        "#...#...#",
        "#########",
    }};
    World w = makeWorld(d);
    for (int i = 0; i < (int)(3.0f / DT); ++i) {
        Intent in = aiThink(w, 1, DT);
        w.tanks[1].turretAngle = in.aim;                    // instant turret, worst case
        w.tanks[1].fireTimer -= DT;
        CHECK(!in.fire);
        if (in.fire) break;
    }
}

TEST(aim_rejects_path_through_self) {
    LevelDef d{{
        "##########",
        "#........#",
        "#P.....B.#",
        "#........#",
        "##########",
    }};
    World w = makeWorld(d);
    // banking off the wall right behind B returns straight through B itself
    CHECK(pathHitsFriendly(w, 1, 0.0f, 1, RICOCHET_RANGE));
    float ang = 0;
    if (findRicochetAim(w, 1, w.tanks[0].pos, 1, RICOCHET_RANGE, ang)) {
        CHECK(!pathHitsFriendly(w, 1, ang, 1, RICOCHET_RANGE));
        CHECK(std::fabs(std::sin(ang)) > 0.05f);   // must not be the straight-back suicide bank
    }
}
