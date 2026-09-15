#include "test_framework.h"
#include "combat.h"
#include "levels.h"
#include "replay.h"
#include <cstdio>

TEST(replay_roundtrip_reproduces_the_attempt) {
    World live = missionWorld(1);                 // mission 2: two browns
    live.arena.set(4, 4, Block::Empty);           // pretend death-retry carryover
    live.tanks[2].alive = false;
    Replay rec;
    replayCaptureStart(rec, 1, live, nullptr);
    InputState in;
    for (int i = 0; i < (int)(5.0f / DT); ++i) {  // scripted 5s attempt
        in.up = (i / 60) % 2 == 0;
        in.right = (i / 90) % 2 == 1;
        in.fire = (i % 45) == 0;
        in.aimWorld = {2.0f + i * 0.01f, 3.0f};
        replayRecordTick(rec, in);
        simTick(live, in, DT);
    }
    CHECK(saveReplay(rec, "replay_test.tmp"));
    Replay back;
    CHECK(loadReplay(back, "replay_test.tmp"));
    std::remove("replay_test.tmp");
    CHECK(back.ticks.size() == rec.ticks.size());
    World re = replayStartWorld(back);
    CHECK(!re.tanks[2].alive);                    // carryover restored
    CHECK(re.arena.at(4, 4) == Block::Empty);
    for (size_t i = 0; i < back.ticks.size(); ++i)
        simTick(re, replayInput(back, i), DT);
    CHECK_NEAR(re.tanks[0].pos.x, live.tanks[0].pos.x, 1e-5f);   // bit-identical playback
    CHECK_NEAR(re.tanks[0].pos.y, live.tanks[0].pos.y, 1e-5f);
    CHECK(re.shells.size() == live.shells.size());
    CHECK(re.enemiesAlive() == live.enemiesAlive());
    CHECK(re.mines.size() == live.mines.size());
}
