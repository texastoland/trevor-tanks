#include "test_framework.h"
#include "levels.h"

TEST(levels_has_20) { CHECK(levelCount() == 20); }

TEST(levels_all_validate) {
    for (int i = 0; i < levelCount(); ++i) {
        std::string err = validateLevel(level(i));
        if (!err.empty()) std::printf("  level %d: %s\n", i + 1, err.c_str());
        CHECK(err.empty());
    }
}

TEST(levels_mission1_is_single_brown) {
    ParsedLevel p = parseLevel(level(0));
    CHECK(p.enemies.size() == 1);
    CHECK(p.enemies[0].first == TankType::Brown);
}

TEST(levels_every_type_appears) {
    bool seen[(int)TankType::COUNT] = {false};
    for (int i = 0; i < levelCount(); ++i)
        for (auto& [t, pos] : parseLevel(level(i)).enemies) seen[(int)t] = true;
    for (int t = (int)TankType::Brown; t < (int)TankType::COUNT; ++t) CHECK(seen[t]);
}

TEST(levels_difficulty_ramps) {
    // finale has at least 2 Black tanks
    int blacks = 0;
    for (auto& [t, pos] : parseLevel(level(19)).enemies)
        if (t == TankType::Black) blacks++;
    CHECK(blacks >= 2);
}

TEST(missions_extend_to_100) {
    CHECK(MISSION_COUNT == 100);
    CHECK(missionWorld(0).enemiesAlive() == 1);          // handcrafted pass-through
}

TEST(missions_generated_counts) {
    // wiki: 4 tanks from mission 21, 5 from 34, 6 from 61, 7 from 81, 8 from 91
    CHECK(missionWorld(20).enemiesAlive() == 4);
    CHECK(missionWorld(33).enemiesAlive() == 5);
    CHECK(missionWorld(60).enemiesAlive() == 6);
    CHECK(missionWorld(80).enemiesAlive() == 7);
    CHECK(missionWorld(90).enemiesAlive() == 8);
}

TEST(missions_black_gated_at_50) {
    for (int m = 20; m < 49; ++m)
        for (auto& t : missionWorld(m).tanks)
            CHECK(t.type != TankType::Black);
    World w50 = missionWorld(49);                        // wiki: mission 5 arena, two blacks
    int blacks = 0;
    for (auto& t : w50.tanks) blacks += t.type == TankType::Black;
    CHECK(blacks == 2);
    CHECK(w50.enemiesAlive() == 2);
}

TEST(missions_deterministic_and_valid) {
    for (int m : {20, 49, 55, 75, 95}) {
        World a = missionWorld(m), b = missionWorld(m);
        CHECK(a.tanks.size() == b.tanks.size());
        bool same = a.tanks.size() == b.tanks.size();
        for (size_t i = 0; same && i < a.tanks.size(); ++i)
            same = a.tanks[i].type == b.tanks[i].type &&
                   vlen(a.tanks[i].pos - b.tanks[i].pos) < 1e-6f;
        CHECK(same);                                     // retries replay identically
        for (auto& t : a.tanks)
            CHECK(!a.arena.blocksTank((int)t.pos.x, (int)t.pos.y));
        CHECK(a.playerAlive());
    }
}
