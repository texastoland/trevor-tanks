#include "test_framework.h"
#include "arena.h"
#include "combat.h"

static World mineRoom() {
    LevelDef d{{
        "##########",
        "#........#",
        "#P..x...B#",
        "#........#",
        "##########",
    }};
    return makeWorld(d);
}

TEST(mine_lay_and_cap) {
    World w = mineRoom();
    CHECK(tryLayMine(w, 0));
    CHECK(tryLayMine(w, 0));
    CHECK(!tryLayMine(w, 0));               // player cap = 2
    CHECK(w.mines.size() == 2);
    CHECK(w.tanks[0].minesLive == 2);
}

TEST(mine_no_trigger_before_armed) {
    World w = mineRoom();
    tryLayMine(w, 0);                       // player standing right on it
    updateMines(w, 0.5f);                   // younger than MINE_ARM_TIME
    CHECK(w.mines.size() == 1);
    CHECK(w.playerAlive());
}

TEST(mine_proximity_triggers_after_armed) {
    World w = mineRoom();
    tryLayMine(w, 0);                       // player on top of own mine
    updateMines(w, 1.1f);                   // arms...
    updateMines(w, DT);                     // ...and player proximity triggers it
    CHECK(w.mines.empty());
    CHECK(!w.playerAlive());                // own mines are dangerous
    CHECK(w.tanks[0].minesLive == 0);       // count released for (a future) re-lay
}

TEST(mine_timer_detonation) {
    World w = mineRoom();
    w.tanks[0].pos = {1.5f, 1.5f};          // lay, then imagine driving far away
    tryLayMine(w, 0);
    w.tanks[0].pos = {8.5f, 3.5f};          // outside trigger+blast
    for (int i = 0; i < (int)(11.0f / DT); ++i) updateMines(w, DT);
    CHECK(w.mines.empty());                 // 10s timer fired
    CHECK(w.playerAlive());
}

TEST(mine_blast_clears_crate_not_wall) {
    World w = mineRoom();                   // crate at (4,2)
    w.tanks[0].pos = {4.5f, 3.5f};          // adjacent to crate, adjacent to bottom wall
    tryLayMine(w, 0);
    w.tanks[0].pos = {8.5f, 1.5f};          // run away
    Mine& m = w.mines[0];
    m.age = MINE_LIFETIME;                  // force detonation now
    updateMines(w, DT);
    CHECK(w.arena.at(4, 2) == Block::Empty);            // crate gone
    CHECK(w.arena.at(4, 4) == Block::Wall);             // border wall intact
    bool sawCrateBreak = false, sawBoom = false;
    for (auto& e : w.events) {
        if (e.kind == GameEvent::CrateBreak) sawCrateBreak = true;
        if (e.kind == GameEvent::MineExplode) sawBoom = true;
    }
    CHECK(sawCrateBreak);
    CHECK(sawBoom);
}

TEST(mine_shot_by_shell_explodes) {
    World w = mineRoom();
    w.mines.push_back({{3.5f, 2.5f}, 0, 0.0f, true});
    w.tanks[1].pos = {5.5f, 2.5f};          // move brown tank into blast radius
    w.tanks[0].minesLive = 1;
    w.tanks[0].turretAngle = 0;             // fire +x through the mine's cell
    tryFire(w, 0);
    for (int i = 0; i < 240 && !w.mines.empty(); ++i) {
        updateShells(w, DT);
        updateMines(w, DT);
    }
    CHECK(w.mines.empty());                 // shell set it off
    CHECK(!w.tanks[1].alive);               // brown tank was in the blast
}

TEST(mine_chain_reaction) {
    World w = mineRoom();
    w.mines.push_back({{6.0f, 1.5f}, 0, 0.0f, true});
    w.mines.push_back({{7.0f, 1.5f}, 0, 0.0f, true});   // within blast of the first
    w.tanks[0].minesLive = 2;
    w.mines[0].age = MINE_LIFETIME;
    updateMines(w, DT);                     // first explodes, promotes second
    updateMines(w, DT);                     // second explodes
    CHECK(w.mines.empty());
    CHECK(w.tanks[0].minesLive == 0);
}
