#include "replay.h"
#include "levels.h"
#include <cstdint>
#include <fstream>

void replayCaptureStart(Replay& r, int mission, const World& w, const EnemyParams* ov) {
    r = Replay{};
    r.mission = mission;
    r.arenaW = w.arena.w;
    r.arenaH = w.arena.h;
    r.blocks.assign(w.arena.blocks.size(), 0);
    for (size_t i = 0; i < w.arena.blocks.size(); ++i) r.blocks[i] = (uint8_t)w.arena.blocks[i];
    r.alive.assign(w.tanks.size(), 0);
    for (size_t i = 0; i < w.tanks.size(); ++i) r.alive[i] = w.tanks[i].alive ? 1 : 0;
    if (ov) { r.hasOverride = true; r.override_ = *ov; }
}

void replayRecordTick(Replay& r, const InputState& in) {
    ReplayTick t;
    t.buttons = (in.up << 0) | (in.down << 1) | (in.left << 2) | (in.right << 3) |
                (in.fire << 4) | (in.mine << 5);
    t.aimX = in.aimWorld.x;
    t.aimY = in.aimWorld.y;
    r.ticks.push_back(t);
}

InputState replayInput(const Replay& r, size_t i) {
    InputState in;
    if (i >= r.ticks.size()) return in;
    const ReplayTick& t = r.ticks[i];
    in.up = t.buttons & 1;
    in.down = t.buttons & 2;
    in.left = t.buttons & 4;
    in.right = t.buttons & 8;
    in.fire = t.buttons & 16;
    in.mine = t.buttons & 32;
    in.aimWorld = {t.aimX, t.aimY};
    return in;
}

World replayStartWorld(const Replay& r) {
    World w = missionWorld(r.mission);
    if ((int)r.blocks.size() == w.arena.w * w.arena.h)
        for (size_t i = 0; i < r.blocks.size(); ++i) w.arena.blocks[i] = (Block)r.blocks[i];
    for (size_t i = 0; i < r.alive.size() && i < w.tanks.size(); ++i)
        w.tanks[i].alive = r.alive[i] != 0;
    return w;
}

// simple same-machine binary format; not meant for cross-version exchange
static const char MAGIC[4] = {'T', 'N', 'K', 'R'};

template <typename T> static void put(std::ofstream& f, const T& v) {
    f.write(reinterpret_cast<const char*>(&v), sizeof v);
}
template <typename T> static bool get(std::ifstream& f, T& v) {
    f.read(reinterpret_cast<char*>(&v), sizeof v);
    return (bool)f;
}

bool saveReplay(const Replay& r, const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(MAGIC, 4);
    put(f, (uint8_t)1);                            // version
    put(f, (int32_t)r.mission);
    put(f, (int32_t)r.arenaW);
    put(f, (int32_t)r.arenaH);
    put(f, (uint32_t)r.blocks.size());
    f.write(reinterpret_cast<const char*>(r.blocks.data()), r.blocks.size());
    put(f, (uint32_t)r.alive.size());
    f.write(reinterpret_cast<const char*>(r.alive.data()), r.alive.size());
    put(f, (uint8_t)r.hasOverride);
    put(f, r.override_);
    put(f, (uint32_t)r.ticks.size());
    for (const auto& t : r.ticks) { put(f, t.buttons); put(f, t.aimX); put(f, t.aimY); }
    return (bool)f;
}

bool loadReplay(Replay& r, const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    char magic[4];
    f.read(magic, 4);
    uint8_t ver = 0;
    if (!f || magic[0] != 'T' || magic[1] != 'N' || magic[2] != 'K' || magic[3] != 'R') return false;
    if (!get(f, ver) || ver != 1) return false;
    int32_t mission = 0, aw = 0, ah = 0;
    uint32_t nb = 0, na = 0, nt = 0;
    if (!get(f, mission) || !get(f, aw) || !get(f, ah) || !get(f, nb)) return false;
    if (mission < 0 || mission >= MISSION_COUNT || nb > 100000u) return false;
    r = Replay{};
    r.mission = mission; r.arenaW = aw; r.arenaH = ah;
    r.blocks.resize(nb);
    f.read(reinterpret_cast<char*>(r.blocks.data()), nb);
    if (!get(f, na) || na > 64u) return false;
    r.alive.resize(na);
    f.read(reinterpret_cast<char*>(r.alive.data()), na);
    uint8_t ov = 0;
    if (!get(f, ov)) return false;
    r.hasOverride = ov != 0;
    if (!get(f, r.override_)) return false;
    if (!get(f, nt) || nt > 10u * 1000u * 1000u) return false;
    r.ticks.resize(nt);
    for (auto& t : r.ticks)
        if (!get(f, t.buttons) || !get(f, t.aimX) || !get(f, t.aimY)) return false;
    return true;
}
