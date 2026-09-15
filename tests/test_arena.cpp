#include "test_framework.h"
#include "arena.h"

static LevelDef tiny() {
    return {{
        "#######",
        "#P..x.#",
        "#..#..#",
        "#....B#",
        "#######",
    }};
}

TEST(arena_validates_good_level) { CHECK(validateLevel(tiny()) == ""); }

TEST(arena_rejects_bad_levels) {
    CHECK(validateLevel({{ "###", "#P#", "###" }}) != "");                 // no enemy
    CHECK(validateLevel({{ "####", "#B.#", "####" }}) != "");              // no player
    CHECK(validateLevel({{ "#####", "#PPB#", "#####" }}) != "");           // two players
    CHECK(validateLevel({{ "#####", "#P.B#", "####" }}) != "");            // not rectangular
    CHECK(validateLevel({{ "#####", ".P.B#", "#####" }}) != "");           // unsealed border
    CHECK(validateLevel({{ "#####", "#PQB#", "#####" }}) != "");           // unknown char
}

TEST(arena_parses_blocks_and_spawns) {
    ParsedLevel p = parseLevel(tiny());
    CHECK(p.arena.w == 7);
    CHECK(p.arena.h == 5);
    CHECK(p.arena.at(0, 0) == Block::Wall);
    CHECK(p.arena.at(4, 1) == Block::Crate);
    CHECK(p.arena.at(3, 2) == Block::Wall);
    CHECK(p.arena.at(1, 1) == Block::Empty);        // spawn cells are empty
    CHECK_NEAR(p.playerSpawn.x, 1.5f, 1e-5f);       // cell center
    CHECK_NEAR(p.playerSpawn.y, 1.5f, 1e-5f);
    CHECK(p.enemies.size() == 1);
    CHECK(p.enemies[0].first == TankType::Brown);
}

TEST(arena_out_of_bounds_is_wall) {
    ParsedLevel p = parseLevel(tiny());
    CHECK(p.arena.at(-1, 2) == Block::Wall);
    CHECK(p.arena.at(2, 99) == Block::Wall);
}

TEST(arena_makeWorld_spawns_tanks) {
    World w = makeWorld(tiny());
    CHECK(w.tanks.size() == 2);
    CHECK(w.tanks[0].type == TankType::Player);
    CHECK(w.tanks[1].type == TankType::Brown);
    CHECK(w.playerAlive());
    CHECK(w.enemiesAlive() == 1);
}

TEST(arena_charToEnemy_full_legend) {
    TankType t;
    CHECK(charToEnemy('B', t) && t == TankType::Brown);
    CHECK(charToEnemy('G', t) && t == TankType::Grey);
    CHECK(charToEnemy('T', t) && t == TankType::Teal);
    CHECK(charToEnemy('Y', t) && t == TankType::Yellow);
    CHECK(charToEnemy('R', t) && t == TankType::Red);
    CHECK(charToEnemy('N', t) && t == TankType::Green);
    CHECK(charToEnemy('U', t) && t == TankType::Purple);
    CHECK(charToEnemy('W', t) && t == TankType::White);
    CHECK(charToEnemy('K', t) && t == TankType::Black);
    CHECK(!charToEnemy('Q', t));
}

TEST(arena_parses_holes) {
    LevelDef d{{
        "#####",
        "#P.o#",
        "#..B#",
        "#####",
    }};
    CHECK(validateLevel(d) == "");
    ParsedLevel p = parseLevel(d);
    CHECK(p.arena.at(3, 1) == Block::Hole);
    CHECK(p.arena.blocksTank(3, 1));         // tanks cannot cross holes
    CHECK(!p.arena.blocksShot(3, 1));        // bullets fly over them
}

TEST(arena_carry_over_destruction) {
    LevelDef d{{
        "######",
        "#P.x.#",
        "#..xB#",
        "######",
    }};
    Arena damaged = parseLevel(d).arena;
    damaged.set(3, 1, Block::Empty);         // crate blown up on the failed attempt
    Arena retry = parseLevel(d).arena;
    carryOverDestruction(damaged, retry);
    CHECK(retry.at(3, 1) == Block::Empty);   // stays destroyed on retry
    CHECK(retry.at(3, 2) == Block::Crate);   // undamaged crate still there
    CHECK(retry.at(0, 0) == Block::Wall);
}

TEST(arena_carry_over_kills) {
    LevelDef d{{
        "########",
        "#P.B..G#",
        "########",
    }};
    World prev = makeWorld(d);
    prev.tanks[1].alive = false;             // Brown died on the failed attempt
    prev.tanks[0].alive = false;             // ...and then so did the player
    World retry = makeWorld(d);
    carryOverKills(prev, retry);
    CHECK(retry.playerAlive());              // the player's own death does NOT carry
    CHECK(!retry.tanks[1].alive);            // dead Brown stays dead
    CHECK(retry.tanks[2].alive);             // untouched Grey respawns normally
    CHECK(retry.enemiesAlive() == 1);
}
