#include "test_framework.h"
#include "arena.h"
#include "combat.h"
#include "player.h"
#include "levels.h"
#include <algorithm>

static World duelRoom() {
    LevelDef d{{
        "############",
        "#..........#",
        "#P.........#",
        "#.......B..#",
        "#..........#",
        "############",
    }};
    return makeWorld(d);
}

TEST(sim_player_intent_mapping) {
    Tank t; t.pos = {5, 5};
    InputState in;
    in.up = true; in.right = true; in.fire = true; in.aimWorld = {5, 0};
    Intent i = playerIntent(t, in);
    CHECK_NEAR(vlen(i.move), 1.0f, 1e-4f);          // diagonal is normalized
    CHECK(i.move.x > 0 && i.move.y < 0);            // screen-up is -y
    CHECK_NEAR(i.aim, -PI_F / 2, 1e-4f);            // aiming straight up
    CHECK(i.fire);
    CHECK(!i.layMine);
}

TEST(sim_player_moves_and_fires) {
    World w = duelRoom();
    InputState in;
    in.right = true; in.fire = true;
    in.aimWorld = w.tanks[0].pos + Vec2{5, 0};
    float x0 = w.tanks[0].pos.x;
    simTick(w, in, DT);
    CHECK(w.tanks[0].pos.x > x0);
    CHECK(w.shells.size() == 1);
    CHECK_NEAR(vlen(w.tanks[0].vel), PLAYER_SPEED, 0.1f);   // vel recorded for AI lead
}

TEST(sim_player_shell_cap_holds_under_spam) {
    World w = duelRoom();
    InputState in;
    in.fire = true;
    in.aimWorld = w.tanks[0].pos + Vec2{0, -1};     // shoot at top wall, shells poof fast
    int maxSeen = 0;
    for (int i = 0; i < (int)(5.0f / DT); ++i) {
        simTick(w, in, DT);
        maxSeen = std::max(maxSeen, (int)w.shells.size());
    }
    // brown may add its own shell; player contribution alone never exceeds the cap
    CHECK(maxSeen <= PLAYER_MAX_SHELLS + 1);
}

TEST(sim_enemy_turret_speed_capped) {
    World w = duelRoom();
    w.tanks[1].turretAngle = 0;
    float before = w.tanks[1].turretAngle;
    simTick(w, {}, DT);
    CHECK(std::fabs(wrapAngle(w.tanks[1].turretAngle - before)) <= TURRET_TURN_SPEED * DT + 1e-4f);
}

TEST(sim_brown_kills_idle_player) {
    World w = duelRoom();
    InputState idle;
    idle.aimWorld = {1, 1};
    bool playerDied = false;
    for (int i = 0; i < (int)(10.0f / DT) && !playerDied; ++i) {
        simTick(w, idle, DT);
        playerDied = !w.playerAlive();
    }
    CHECK(playerDied);                               // a sitting duck loses within 10s
}

TEST(sim_win_condition_reachable) {
    World w = duelRoom();
    InputState in;
    in.fire = true;
    for (int i = 0; i < (int)(10.0f / DT) && w.enemiesAlive() > 0; ++i) {
        in.aimWorld = w.tanks[1].pos;                // perfect aim at the brown tank
        simTick(w, in, DT);
    }
    CHECK(w.enemiesAlive() == 0);
}

TEST(sim_mine_intent_lays_mine) {
    World w = duelRoom();
    InputState in;
    in.mine = true;
    in.aimWorld = {1, 1};
    simTick(w, in, DT);
    CHECK(w.mines.size() == 1);
}

TEST(sim_fire_pauses_movement) {
    World w = duelRoom();
    w.tanks[1].alive = false;                       // just the player
    InputState in;
    in.right = true;
    in.aimWorld = w.tanks[0].pos + Vec2{0, -5};     // shoot at the top wall
    simTick(w, in, DT);                             // driving, not yet firing
    in.fire = true;
    simTick(w, in, DT);                             // fires this tick
    in.fire = false;
    CHECK(w.shells.size() == 1);
    float x0 = w.tanks[0].pos.x;
    int frozenTicks = (int)(FIRE_MOVE_PAUSE / DT) - 2;
    for (int i = 0; i < frozenTicks; ++i) simTick(w, in, DT);
    CHECK_NEAR(w.tanks[0].pos.x, x0, 1e-4f);        // movement paused after the shot
    for (int i = 0; i < (int)(0.2f / DT); ++i) simTick(w, in, DT);
    CHECK(w.tanks[0].pos.x > x0 + 0.1f);            // and resumes afterwards
}

TEST(sim_held_fire_dumps_full_cap) {
    World w = duelRoom();
    w.tanks[1].alive = false;
    InputState in;
    in.fire = true;
    in.aimWorld = w.tanks[0].pos + Vec2{9, 0};      // fire down the long axis
    int maxLive = 0;
    for (int i = 0; i < (int)(1.0f / DT); ++i) {    // hold fire intent for 1s
        simTick(w, in, DT);
        maxLive = std::max(maxLive, (int)w.shells.size());
    }
    CHECK(maxLive == PLAYER_MAX_SHELLS);            // all 5 shells get airborne
}

TEST(sim_click_buffer_lifecycle) {
    ClickBuffer b;
    CHECK(!b.pending());
    b.press();
    CHECK(b.pending());                             // click intent persists...
    for (int i = 0; i < (int)(INPUT_BUFFER_TIME / DT) + 2; ++i) b.tick(DT);
    CHECK(!b.pending());                            // ...until it expires...
    b.press();
    b.consume();
    CHECK(!b.pending());                            // ...or the shot succeeds
}

TEST(sim_click_buffer_hold_is_one_shot) {
    ClickBuffer b;
    b.hold();                                       // holding without a press queues nothing
    CHECK(!b.pending());
    b.press();
    for (int i = 0; i < (int)(1.0f / DT); ++i) b.hold(), b.tick(DT);
    CHECK(b.pending());                             // held click outlives normal expiry...
    b.consume();
    b.hold();
    CHECK(!b.pending());                            // ...but once fired, holding mints no new shot
}

TEST(sim_yellow_deploys_mines) {
    LevelDef d{{
        "####################",
        "#..................#",
        "#P..............Y..#",
        "#..................#",
        "####################",
    }};
    World w = makeWorld(d);
    InputState idle;
    idle.aimWorld = {1, 1};
    bool laid = false;
    for (int i = 0; i < (int)(6.0f / DT) && !laid; ++i) {
        simTick(w, idle, DT);
        for (auto& m : w.mines) laid = laid || m.owner == 1;
    }
    CHECK(laid);                                         // yellow lays mines unprompted
}

TEST(sim_miners_survive_their_own_mines) {
    LevelDef d{{
        "####################",
        "#..................#",
        "#P..............Y..#",
        "#..................#",
        "####################",
    }};
    World w = makeWorld(d);
    InputState idle;
    idle.aimWorld = {1, 1};
    for (int i = 0; i < (int)(8.0f / DT); ++i) simTick(w, idle, DT);
    CHECK(w.tanks[1].alive);                 // laying a mine must not be suicide
}

TEST(sim_mission5_yellows_survive_their_mines) {
    World w = makeWorld(level(4));                   // the real mission 5 crate field
    w.tanks[3].alive = false;                        // isolate mines from brown's stray ricochets
    InputState idle;
    idle.aimWorld = {1, 1};
    bool laid = false;
    for (int i = 0; i < (int)(30.0f / DT); ++i) {
        simTick(w, idle, DT);
        laid = laid || !w.mines.empty();
    }
    CHECK(laid);                                     // they still actually mine the field
    CHECK(w.tanks[1].alive);                         // and both yellows outlive
    CHECK(w.tanks[2].alive);                         // their own minefield
}

TEST(sim_player_override_flows_into_combat) {
    EnemyParams mod = paramsFor(TankType::Player);
    mod.maxShells = 8;
    setPlayerOverride(mod);
    World w = duelRoom();
    w.tanks[1].alive = false;
    InputState in;
    in.fire = true;
    in.aimWorld = w.tanks[0].pos + Vec2{9, 0};
    int maxLive = 0;
    for (int i = 0; i < (int)(1.0f / DT); ++i) {
        simTick(w, in, DT);
        maxLive = std::max(maxLive, (int)w.shells.size());
    }
    clearPlayerOverride();                           // never leak into other tests
    CHECK(maxLive == 8);
}
