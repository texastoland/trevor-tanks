#include "test_framework.h"
#include "arena.h"
#include "ai.h"

static Arena corridorArena() {
    LevelDef d{{
        "##########",
        "#P.......#",
        "#.######.#",
        "#........#",
        "#.######.#",
        "#B.......#",
        "##########",
    }};
    return parseLevel(d).arena;
}

TEST(ai_path_straight_line) {
    LevelDef d{{
        "########",
        "#P....B#",
        "########",
    }};
    Arena a = parseLevel(d).arena;
    auto path = findPath(a, {1.5f, 1.5f}, {6.5f, 1.5f});
    CHECK(!path.empty());
    CHECK_NEAR(path.back().x, 6.5f, 1e-4f);
    CHECK(path.size() == 5);                 // one waypoint per cell advanced
}

TEST(ai_path_around_walls) {
    Arena a = corridorArena();
    auto path = findPath(a, {8.5f, 1.5f}, {1.5f, 5.5f});
    CHECK(!path.empty());
    for (auto& wp : path)                    // every waypoint is an open cell
        CHECK(!a.blocksTank((int)wp.x, (int)wp.y));
    CHECK(path.size() >= 11);                // must snake through the S-corridor
}

TEST(ai_path_unreachable) {
    LevelDef d{{
        "#########",
        "#P..#...#",
        "#...#..B#",
        "#########",
    }};
    Arena a = parseLevel(d).arena;
    CHECK(findPath(a, {1.5f, 1.5f}, {7.5f, 2.5f}).empty());
}

TEST(ai_chase_moves_toward_player) {
    LevelDef d{{
        "##########",
        "#P.......#",
        "#........#",
        "#.......R#",
        "##########",
    }};
    World w = makeWorld(d);
    Vec2 before = w.tanks[1].pos;
    for (int i = 0; i < 120; ++i) {          // Red is a slow mover per the wiki table
        Intent in = aiThink(w, 1, DT);
        w.tanks[1].pos = w.tanks[1].pos + in.move * (paramsFor(TankType::Red).moveSpeed * DT);
    }
    float d0 = vlen(before - w.tanks[0].pos);
    float d1 = vlen(w.tanks[1].pos - w.tanks[0].pos);
    CHECK(d1 < d0 - 0.15f);
}

TEST(ai_flee_moves_away_when_close) {
    LevelDef d{{
        "##########",
        "#........#",
        "#..P.G...#",
        "#........#",
        "##########",
    }};
    World w = makeWorld(d);
    Intent in = aiThink(w, 1, DT);
    Vec2 toPlayer = norm(w.tanks[0].pos - w.tanks[1].pos);
    CHECK(vlen(in.move) > 0.1f);
    CHECK(dot(in.move, toPlayer) < 0.0f);    // net away from player
}

TEST(ai_stationary_types_do_not_move) {
    LevelDef d{{
        "########",
        "#P....B#",
        "########",
    }};
    World w = makeWorld(d);
    for (int i = 0; i < 60; ++i) {
        Intent in = aiThink(w, 1, DT);
        CHECK_NEAR(vlen(in.move), 0.0f, 1e-5f);
    }
}

TEST(ai_dodges_incoming_shell) {
    LevelDef d{{
        "##########",
        "#........#",
        "#P.....R.#",
        "#........#",
        "##########",
    }};
    World w = makeWorld(d);
    // shell flying straight at the red tank
    w.shells.push_back({{5.0f, 2.5f}, {SHELL_NORMAL, 0}, 1, 0, true});
    w.tanks[0].shellsLive = 1;
    Intent in = aiThink(w, 1, DT);
    CHECK(vlen(in.move) > 0.1f);
    CHECK(std::fabs(in.move.y) > std::fabs(in.move.x));   // dodging sideways
}

TEST(ai_avoids_nearby_mine) {
    LevelDef d{{
        "##########",
        "#........#",
        "#P.....R.#",
        "#........#",
        "##########",
    }};
    World w = makeWorld(d);
    w.mines.push_back({{6.5f, 2.5f}, 1, 2.0f, true});     // ENEMY-owned armed mine beside Red
    w.tanks[1].minesLive = 1;
    Intent in = aiThink(w, 1, DT);
    CHECK(in.move.x > 0.0f);                              // pushed away (right), not into it
}

TEST(ai_nonminer_ignores_player_mines) {
    LevelDef d{{
        "##########",
        "#........#",
        "#P.....R.#",
        "#........#",
        "##########",
    }};
    World w = makeWorld(d);                  // Red lays no mines itself
    w.mines.push_back({{6.5f, 2.5f}, 0, 2.0f, true});    // PLAYER-owned armed mine
    w.tanks[0].minesLive = 1;
    Intent in = aiThink(w, 1, DT);
    CHECK(in.move.x < 0.0f);                 // keeps chasing right through it
}

TEST(ai_miner_avoids_player_mines) {
    LevelDef d{{
        "##########",
        "#........#",
        "#P.....U.#",
        "#........#",
        "##########",
    }};
    World w = makeWorld(d);                  // Purple lays mines, detects all mines
    w.mines.push_back({{6.5f, 2.5f}, 0, 2.0f, true});    // PLAYER-owned armed mine
    w.tanks[0].minesLive = 1;
    Intent in = aiThink(w, 1, DT);
    CHECK(in.move.x > 0.0f);                 // repelled away from it
}

TEST(ai_yellow_ignores_incoming_shells) {
    LevelDef d{{
        "##########",
        "#........#",
        "#P.....Y.#",
        "#........#",
        "##########",
    }};
    World a = makeWorld(d), b = makeWorld(d);            // identical worlds, same rng seed
    b.shells.push_back({{5.0f, 2.5f}, {SHELL_NORMAL, 0}, 1, 0, true});
    b.tanks[0].shellsLive = 1;
    Intent ia = aiThink(a, 1, DT), ib = aiThink(b, 1, DT);
    CHECK_NEAR(ia.move.x, ib.move.x, 1e-5f);             // the shell changes nothing
    CHECK_NEAR(ia.move.y, ib.move.y, 1e-5f);
}

TEST(ai_green_leads_moving_player) {
    LevelDef d{{
        "##########",
        "#........#",
        "#N.....P.#",
        "#........#",
        "##########",
    }};
    World still = makeWorld(d), moving = makeWorld(d);   // identical worlds, same rng
    moving.tanks[0].vel = {0, 1.5f};                     // player strafing "down"
    Intent is = aiThink(still, 1, DT), im = aiThink(moving, 1, DT);
    CHECK(still.tanks[1].hasShot);
    CHECK(moving.tanks[1].hasShot);
    // prediction must change the firing solution when the player moves...
    CHECK(std::fabs(wrapAngle(im.aim - is.aim)) > 0.02f);
    // ...tilting toward where the player is heading (+y here)
    CHECK(wrapAngle(im.aim - is.aim) > 0.0f);
}

TEST(ai_path_avoids_mines) {
    LevelDef d{{
        "##########",
        "#P.......#",
        "#........#",
        "#.......B#",
        "##########",
    }};
    Arena a = parseLevel(d).arena;
    std::vector<Vec2> hazards{{4.5f, 1.5f}};             // mine sitting on the straight route
    auto path = findPath(a, {1.5f, 1.5f}, {8.5f, 1.5f}, hazards, 1.3f);
    CHECK(!path.empty());                                // detours instead of threading through
    for (auto& wp : path) CHECK(vlen(wp - hazards[0]) > 1.2f);
}

TEST(ai_miner_flees_closest_mine_in_cluster) {
    LevelDef d{{
        "############",
        "#..........#",
        "#P.....Y...#",
        "#..........#",
        "############",
    }};
    World w = makeWorld(d);                              // yellow at (7.5, 2.5)
    w.mines.push_back({{5.5f, 2.5f}, 1, 2.0f, true});    // own mine, 2.0 away (left)
    w.mines.push_back({{8.5f, 2.5f}, 1, 2.0f, true});    // own mine, 1.0 away (right)
    w.tanks[1].minesLive = 2;
    Intent in = aiThink(w, 1, DT);
    CHECK(vlen(in.move) > 0.1f);
    CHECK(in.move.x < 0.0f);                             // net flight away from the CLOSER mine
}
