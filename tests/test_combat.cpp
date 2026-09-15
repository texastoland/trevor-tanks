#include "test_framework.h"
#include "arena.h"
#include "combat.h"
#include "physics.h"

static World openRoom() {
    LevelDef d{{
        "############",
        "#..........#",
        "#..........#",
        "#P........B#",
        "#..........#",
        "############",
    }};
    return makeWorld(d);
}

static bool hasEvent(const World& w, GameEvent::Kind k) {
    for (auto& e : w.events) if (e.kind == k) return true;
    return false;
}

TEST(combat_params_table_sane) {
    for (int i = 0; i < (int)TankType::COUNT; ++i) {
        const EnemyParams& p = paramsFor((TankType)i);
        CHECK(p.shellSpeed > 0);
        CHECK(p.maxShells >= 1);
        CHECK(p.bounces >= 0);
        CHECK(p.fireCooldown > 0);
    }
    CHECK(paramsFor(TankType::Player).maxShells == PLAYER_MAX_SHELLS);
    CHECK(paramsFor(TankType::Brown).moveSpeed == 0.0f);
    CHECK(paramsFor(TankType::Green).aimBounces == 2);
    CHECK(paramsFor(TankType::Black).bounces == 0);
}

TEST(combat_fire_spawns_shell_at_barrel) {
    World w = openRoom();
    w.tanks[0].turretAngle = 0;                     // aiming +x
    CHECK(tryFire(w, 0));
    CHECK(w.shells.size() == 1);
    CHECK(w.tanks[0].shellsLive == 1);
    CHECK_NEAR(w.shells[0].pos.x, w.tanks[0].pos.x + BARREL_LEN, 1e-4f);
    CHECK_NEAR(vlen(w.shells[0].vel), paramsFor(TankType::Player).shellSpeed, 1e-3f);
    CHECK(w.shells[0].bouncesLeft == 1);
    CHECK(hasEvent(w, GameEvent::Fire));
}

TEST(combat_fire_respects_cap_and_cooldown) {
    World w = openRoom();
    w.tanks[0].turretAngle = 0;
    for (int i = 0; i < 10; ++i) {
        w.tanks[0].fireTimer = 0;                   // bypass cooldown, test cap alone
        tryFire(w, 0);
    }
    CHECK((int)w.shells.size() == PLAYER_MAX_SHELLS);
    // cooldown alone:
    World w2 = openRoom();
    CHECK(tryFire(w2, 0));
    CHECK(!tryFire(w2, 0));                         // fireTimer still hot
}

TEST(combat_shell_flies_straight) {
    World w = openRoom();
    w.tanks[0].turretAngle = 0;
    tryFire(w, 0);
    Vec2 start = w.shells[0].pos;
    updateShells(w, 0.5f);
    CHECK_NEAR(w.shells[0].pos.x, start.x + paramsFor(TankType::Player).shellSpeed * 0.5f, 0.01f);
}

TEST(combat_shell_ricochets_once_then_dies) {
    World w = openRoom();
    w.tanks[1].alive = false;                       // clear the target out of the way
    w.tanks[0].pos = {5.5f, 3.5f};
    w.tanks[0].turretAngle = 0;                     // fire +x at right wall
    tryFire(w, 0);
    w.tanks[0].pos = {5.5f, 1.5f};                  // step off the return line: own ricochet CAN kill you
    for (int i = 0; i < 240; ++i) updateShells(w, DT);   // 2s: hit wall, bounce back left
    CHECK(hasEvent(w, GameEvent::Bounce));
    CHECK(w.shells.size() == 1);
    CHECK(w.shells[0].vel.x < 0);                   // reflected
    for (int i = 0; i < 600; ++i) updateShells(w, DT);   // reaches left wall, no bounces left
    CHECK(w.shells.empty());
    CHECK(w.tanks[0].shellsLive == 0);
    CHECK(hasEvent(w, GameEvent::ShellPoof));
}

TEST(combat_shell_kills_tank) {
    World w = openRoom();
    w.tanks[0].turretAngle = 0;                     // player at y=3.5 aims +x at Brown
    tryFire(w, 0);
    for (int i = 0; i < 600 && !w.shells.empty(); ++i) updateShells(w, DT);
    CHECK(!w.tanks[1].alive);
    CHECK(w.enemiesAlive() == 0);
    CHECK(hasEvent(w, GameEvent::TankExplode));
    CHECK(w.shells.empty());
}

TEST(combat_shells_destroy_each_other) {
    World w = openRoom();
    w.shells.push_back({{4.0f, 1.5f}, {2, 0}, 1, 0, true});
    w.shells.push_back({{6.0f, 1.5f}, {-2, 0}, 1, 1, true});
    w.tanks[0].shellsLive = 1; w.tanks[1].shellsLive = 1;
    for (int i = 0; i < 120; ++i) updateShells(w, DT);
    CHECK(w.shells.empty());
    CHECK(w.tanks[0].shellsLive == 0);
    CHECK(w.tanks[1].shellsLive == 0);
}

TEST(combat_crate_detonates_shell_but_survives) {
    LevelDef d{{
        "########",
        "#P..x.B#",
        "########",
    }};
    World w = makeWorld(d);
    w.tanks[0].turretAngle = 0;
    tryFire(w, 0);
    for (int i = 0; i < 240; ++i) updateShells(w, DT);
    CHECK(w.shells.empty());                        // died on the crate
    CHECK(w.arena.at(4, 1) == Block::Crate);        // crate intact
    CHECK(w.tanks[1].alive);                        // shielded the brown tank
}

TEST(combat_pointblank_wall_detonates) {
    World w = openRoom();
    w.tanks[0].pos = {1.0f + TANK_RADIUS, 3.5f};
    w.tanks[0].turretAngle = PI_F;                  // barrel tip inside left wall
    tryFire(w, 0);
    updateShells(w, DT);
    CHECK(w.shells.empty());
    CHECK(w.tanks[0].shellsLive == 0);
}

TEST(combat_own_ricochet_can_kill_you) {
    World w = openRoom();
    w.tanks[1].alive = false;                       // just the player and its shell
    w.tanks[0].pos = {5.5f, 3.5f};
    w.tanks[0].turretAngle = 0;                     // fire +x; bounce returns along the same row
    tryFire(w, 0);
    for (int i = 0; i < 1200 && w.tanks[0].alive; ++i) updateShells(w, DT);
    CHECK(!w.tanks[0].alive);
    CHECK(w.shells.empty());
}

TEST(combat_params_match_wiki_table) {
    // stats follow StrategyWiki's tank table where sources conflict
    CHECK(paramsFor(TankType::Brown).shellSpeed == SHELL_NORMAL);
    CHECK(paramsFor(TankType::Red).moveSpeed == MOVE_FAST);       // "normal speed" = player parity
    CHECK(paramsFor(TankType::White).moveSpeed == MOVE_FAST);
    CHECK(paramsFor(TankType::Yellow).moveSpeed == MOVE_VERYFAST);
    CHECK(paramsFor(TankType::Purple).moveSpeed == MOVE_VERYFAST);
    CHECK(paramsFor(TankType::Black).moveSpeed == MOVE_EXTREME);
    CHECK(paramsFor(TankType::Black).maxShells == 2);             // "fire two rockets at a time"
    CHECK(paramsFor(TankType::White).fireCooldown == paramsFor(TankType::Purple).fireCooldown);
}

TEST(combat_params_match_wiki_behaviours) {
    CHECK(paramsFor(TankType::Teal).steering == Steering::FleeWhenClose);   // "Defensive"
    CHECK(!paramsFor(TankType::Yellow).dodgesShells);                       // "Incautious"
    CHECK(paramsFor(TankType::Red).dodgesShells);
    CHECK(paramsFor(TankType::Brown).aim == AimStyle::Scan);      // turret sweeps, never seeks
    CHECK(paramsFor(TankType::Green).leadAim);                    // predicts through ricochets
    CHECK(!paramsFor(TankType::Teal).leadAim);                    // "won't aim for where you will be"
    CHECK(paramsFor(TankType::Black).leadAim);                    // "aim for where you will be"
    CHECK(paramsFor(TankType::Black).steering == Steering::FleeWhenClose);  // "defensive, runs away"
}

TEST(combat_player_override) {
    EnemyParams base = paramsFor(TankType::Player);
    EnemyParams mod = base;
    mod.maxShells = 8;
    mod.shellSpeed = SHELL_FAST;
    mod.bounces = 3;
    setPlayerOverride(mod);
    CHECK(paramsFor(TankType::Player).maxShells == 8);
    CHECK(paramsFor(TankType::Player).shellSpeed == SHELL_FAST);
    CHECK(paramsFor(TankType::Player).bounces == 3);
    CHECK(paramsFor(TankType::Brown).maxShells == 1);    // enemies untouched
    clearPlayerOverride();
    CHECK(paramsFor(TankType::Player).maxShells == base.maxShells);
}
