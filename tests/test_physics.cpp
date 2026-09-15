#include "test_framework.h"
#include "arena.h"
#include "physics.h"

// 10x6 room, one interior wall cell at (5,2)
static Arena room() {
    LevelDef d{{
        "##########",
        "#........#",
        "#....#...#",
        "#........#",
        "#P......B#",
        "##########",
    }};
    return parseLevel(d).arena;
}

TEST(phys_circle_grid_basic) {
    Arena a = room();
    CHECK(!circleHitsGrid(a, {2.5f, 1.5f}, TANK_RADIUS));      // open space
    CHECK(circleHitsGrid(a, {1.2f, 1.2f}, TANK_RADIUS));       // overlapping border corner
    CHECK(circleHitsGrid(a, {5.5f, 2.5f}, 0.1f));              // inside wall cell
}

TEST(phys_move_blocked_by_wall) {
    Arena a = room();
    Tank t; t.pos = {1.5f, 1.5f};
    for (int i = 0; i < 240; ++i) moveTank(t, {-1, 0}, 2.0f, a, DT);  // drive left 2s
    CHECK_NEAR(t.pos.x, 1.0f + TANK_RADIUS, 0.02f);            // stopped at wall face
    CHECK_NEAR(t.pos.y, 1.5f, 1e-4f);
}

TEST(phys_move_slides_along_wall) {
    Arena a = room();
    Tank t; t.pos = {2.5f, 1.5f};
    Vec2 diag = norm(Vec2{-1, 1});
    for (int i = 0; i < 240; ++i) moveTank(t, diag, 2.0f, a, DT);
    CHECK_NEAR(t.pos.x, 1.0f + TANK_RADIUS, 0.02f);            // pinned to left wall...
    CHECK(t.pos.y > 3.0f);                                     // ...but slid down
}

TEST(phys_move_sets_body_angle) {
    Arena a = room();
    Tank t; t.pos = {5.0f, 4.0f};
    moveTank(t, {0, -1}, 2.0f, a, DT);
    CHECK_NEAR(t.bodyAngle, -PI_F / 2, 1e-4f);
    moveTank(t, {0, 0}, 2.0f, a, DT);                          // idle keeps angle
    CHECK_NEAR(t.bodyAngle, -PI_F / 2, 1e-4f);
}

TEST(phys_separate_overlapping_tanks) {
    Arena a = room();
    std::vector<Tank> ts(2);
    ts[0].pos = {4.0f, 4.0f};
    ts[1].pos = {4.3f, 4.0f};
    separateTanks(ts, a);
    CHECK(vlen(ts[1].pos - ts[0].pos) >= 2 * TANK_RADIUS - 1e-3f);
}

TEST(phys_sweep_straight_hit) {
    Arena a = room();
    SweepHit h = sweepSegment(a, {2.5f, 2.5f}, {8.5f, 2.5f});  // runs into wall cell (5,2)
    CHECK(h.hit);
    CHECK(h.cx == 5); CHECK(h.cy == 2);
    CHECK_NEAR(h.pos.x, 5.0f, 1e-4f);
    CHECK_NEAR(h.normal.x, -1.0f, 1e-5f);
    CHECK_NEAR(h.normal.y, 0.0f, 1e-5f);
}

TEST(phys_sweep_miss) {
    Arena a = room();
    SweepHit h = sweepSegment(a, {2.5f, 3.5f}, {7.5f, 3.5f});  // passes under the wall cell
    CHECK(!h.hit);
    CHECK_NEAR(h.pos.x, 7.5f, 1e-5f);
}

TEST(phys_sweep_no_tunnel_long_segment) {
    Arena a = room();
    SweepHit h = sweepSegment(a, {2.5f, 2.5f}, {200.0f, 2.5f}); // very fast shell, one tick
    CHECK(h.hit);
    CHECK(h.cx == 5);
}

TEST(phys_sweep_starts_inside_solid) {
    Arena a = room();
    SweepHit h = sweepSegment(a, {5.5f, 2.5f}, {6.5f, 2.5f});
    CHECK(h.hit);
    CHECK_NEAR(h.normal.x, 0.0f, 1e-5f);   // zero normal marks degenerate start
    CHECK_NEAR(h.normal.y, 0.0f, 1e-5f);
}

TEST(phys_seg_point_dist) {
    CHECK_NEAR(segPointDist({0, 0}, {10, 0}, {5, 3}), 3.0f, 1e-5f);
    CHECK_NEAR(segPointDist({0, 0}, {10, 0}, {-4, 3}), 5.0f, 1e-5f);  // clamps to endpoint
    CHECK_NEAR(segPointDist({2, 2}, {2, 2}, {5, 6}), 5.0f, 1e-5f);    // degenerate segment
}

TEST(phys_holes_block_tanks_not_shells) {
    LevelDef d{{
        "########",
        "#P.o..B#",
        "########",
    }};
    Arena a = parseLevel(d).arena;
    Tank t; t.pos = {1.5f, 1.5f};
    for (int i = 0; i < 240; ++i) moveTank(t, {1, 0}, 2.0f, a, DT);
    CHECK_NEAR(t.pos.x, 3.0f - TANK_RADIUS, 0.02f);      // tank stops at the hole edge
    SweepHit h = sweepSegment(a, {1.5f, 1.5f}, {6.5f, 1.5f});
    CHECK(!h.hit);                                        // shell crosses it untouched
}
