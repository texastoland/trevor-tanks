#include "arena.h"

bool charToEnemy(char c, TankType& out) {
    switch (c) {
        case 'B': out = TankType::Brown;  return true;
        case 'G': out = TankType::Grey;   return true;
        case 'T': out = TankType::Teal;   return true;
        case 'Y': out = TankType::Yellow; return true;
        case 'R': out = TankType::Red;    return true;
        case 'N': out = TankType::Green;  return true;
        case 'U': out = TankType::Purple; return true;
        case 'W': out = TankType::White;  return true;
        case 'K': out = TankType::Black;  return true;
        default: return false;
    }
}

std::string validateLevel(const LevelDef& def) {
    const auto& rows = def.rows;
    if (rows.size() < 3) return "fewer than 3 rows";
    size_t w = rows[0].size();
    if (w < 3) return "fewer than 3 columns";
    int players = 0, enemies = 0;
    for (size_t y = 0; y < rows.size(); ++y) {
        if (rows[y].size() != w) return "not rectangular (row " + std::to_string(y) + ")";
        for (size_t x = 0; x < w; ++x) {
            char c = rows[y][x];
            bool border = (x == 0 || y == 0 || x == w - 1 || y == rows.size() - 1);
            if (border && c != '#') return "border not sealed";
            TankType t;
            if (c == 'P') players++;
            else if (charToEnemy(c, t)) enemies++;
            else if (c != '#' && c != 'x' && c != '.' && c != 'o') return std::string("unknown char '") + c + "'";
        }
    }
    if (players != 1) return "need exactly one P, got " + std::to_string(players);
    if (enemies < 1) return "no enemies";
    return "";
}

ParsedLevel parseLevel(const LevelDef& def) {
    ParsedLevel p;
    p.arena.h = (int)def.rows.size();
    p.arena.w = (int)def.rows[0].size();
    p.arena.blocks.assign((size_t)(p.arena.w * p.arena.h), Block::Empty);
    for (int y = 0; y < p.arena.h; ++y) {
        for (int x = 0; x < p.arena.w; ++x) {
            char c = def.rows[y][x];
            Vec2 center{x + 0.5f, y + 0.5f};
            TankType t;
            if (c == '#') p.arena.set(x, y, Block::Wall);
            else if (c == 'x') p.arena.set(x, y, Block::Crate);
            else if (c == 'o') p.arena.set(x, y, Block::Hole);
            else if (c == 'P') p.playerSpawn = center;
            else if (charToEnemy(c, t)) p.enemies.push_back({t, center});
        }
    }
    return p;
}

World makeWorld(const LevelDef& def) {
    ParsedLevel p = parseLevel(def);
    World w;
    w.arena = p.arena;
    Tank player;
    player.type = TankType::Player;
    player.pos = p.playerSpawn;
    w.tanks.push_back(player);
    for (auto& [type, pos] : p.enemies) spawnEnemy(w, type, pos, p.playerSpawn);
    return w;
}

void spawnEnemy(World& w, TankType type, Vec2 pos, Vec2 playerSpawn) {
    Tank t;
    t.type = type;
    t.pos = pos;
    // face roughly toward the player so turrets start plausibly
    t.bodyAngle = t.turretAngle = t.desiredAim = angleTo(pos, playerSpawn);
    t.fireDelay = 1.0f;                // start-of-mission grace: nobody snipes on tick one
    w.tanks.push_back(t);
}

void carryOverDestruction(const Arena& prev, Arena& next) {
    if (prev.w != next.w || prev.h != next.h) return;
    for (int y = 0; y < next.h; ++y)
        for (int x = 0; x < next.w; ++x)
            if (next.at(x, y) == Block::Crate && prev.at(x, y) == Block::Empty)
                next.set(x, y, Block::Empty);
}

void carryOverKills(const World& prev, World& next) {
    if (prev.tanks.size() != next.tanks.size()) return;   // only meaningful for the same mission
    for (size_t i = 1; i < next.tanks.size(); ++i)        // index 0 is the player: their death never carries
        if (!prev.tanks[i].alive) next.tanks[i].alive = false;
}
