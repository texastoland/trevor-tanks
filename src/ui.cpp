#include "ui.h"
#include "raylib.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <string>

static void centerText(const char* s, int y, int size, Color c) {
    DrawText(s, (GetScreenWidth() - MeasureText(s, size)) / 2, y, size, c);
}

static void drawLifeTanks(int cx, int y, int lives) {
    // little tank silhouettes: tread-hull-tread rectangles
    int w = 26, gap = 10;
    int total = lives * w + (lives - 1) * gap;
    int x = cx - total / 2;
    for (int i = 0; i < lives; ++i, x += w + gap) {
        DrawRectangle(x, y, w, 4, {70, 110, 190, 255});
        DrawRectangle(x + 3, y + 4, w - 6, 8, {70, 110, 190, 255});
        DrawRectangle(x, y + 12, w, 4, {70, 110, 190, 255});
        DrawRectangle(x + w / 2 - 1, y + 2, 12, 3, {40, 60, 110, 255});   // barrel
    }
}

void drawTitle(int savedMission, int contMission, const TitlePanel& p) {
    ClearBackground({25, 20, 15, 255});
    centerText("TANKS!", GetScreenHeight() / 4, 90, RAYWHITE);
    centerText("[Enter] New game", GetScreenHeight() / 2 + 20, 28, LIGHTGRAY);
    if (savedMission > 0)
        centerText(TextFormat("[C] Continue from mission < %d >", contMission + 1),
                   GetScreenHeight() / 2 + 60, 28, LIGHTGRAY);
    if (p.dev || p.tankEdit) {
        // hidden panel: Up/Down pick a row, Left/Right adjust
        // (TextFormat's static buffers rotate, so format and draw one row at a time)
        char buf[64];
        int y0 = GetScreenHeight() / 2 + 104;
        if (p.dev)
            DrawText("DEV", GetScreenWidth() / 2 - 250, y0, 22, {255, 120, 70, 255});
        int n = 0;
        auto row = [&](const char* text) {
            Color c = n == p.editRow ? Color{255, 220, 120, 255} : Color{200, 160, 80, 255};
            if (n == p.editRow)
                DrawText(">", GetScreenWidth() / 2 - 195, y0 + n * 26, 22, c);
            DrawText(text, GetScreenWidth() / 2 - 168, y0 + n * 26, 22, c);
            n++;
        };
        if (p.dev) {
            snprintf(buf, sizeof buf, "Mission        < %d >", p.devMission + 1);
            row(buf);
        }
        static const char* typeNames[] = {"Player", "Brown", "Grey", "Teal", "Yellow",
                                          "Red", "Green", "Purple", "White", "Black"};
        snprintf(buf, sizeof buf, "Tank type      < %s >",
                 p.tankType < 0 ? "Custom" : typeNames[p.tankType]);
        row(buf);
        snprintf(buf, sizeof buf, "Speed          < %.2f >", p.stats.moveSpeed);   row(buf);
        snprintf(buf, sizeof buf, "Bullet speed   < %.1f >", p.stats.shellSpeed);  row(buf);
        snprintf(buf, sizeof buf, "Max bullets    < %d >", p.stats.maxShells);     row(buf);
        snprintf(buf, sizeof buf, "Ricochets      < %d >", p.stats.bounces);       row(buf);
        snprintf(buf, sizeof buf, "Fire cooldown  < %.2f >", p.stats.fireCooldown); row(buf);
        snprintf(buf, sizeof buf, "Max mines      < %d >", p.stats.maxMines);      row(buf);
    }
    centerText("WASD drive - mouse aim - LMB fire - RMB/Space mine - Esc quit",
               GetScreenHeight() - 60, 20, GRAY);
}

void drawMissionIntro(int mission, int enemyCount, int lives) {
    ClearBackground({25, 20, 15, 255});
    centerText(TextFormat("Mission %d", mission + 1), GetScreenHeight() / 3, 60, RAYWHITE);
    centerText(TextFormat("Enemy tanks: %d", enemyCount), GetScreenHeight() / 3 + 80, 30, LIGHTGRAY);
    drawLifeTanks(GetScreenWidth() / 2, GetScreenHeight() / 3 + 140, lives);
}

void drawHud(int mission, int enemiesAlive, int score) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    // bottom-center red mission banner, like the original
    const char* label = TextFormat("Mission %d      x %d", mission + 1, enemiesAlive);
    int tw = MeasureText(label, 28);
    Rectangle banner = {sw / 2.0f - tw / 2.0f - 34, sh - 64.0f, tw + 68.0f, 44.0f};
    DrawRectangleRounded(banner, 0.5f, 8, {168, 34, 30, 235});
    DrawRectangleRoundedLines(banner, 0.5f, 8, {212, 176, 110, 255});
    DrawText(label, (int)(banner.x + 34), (int)(banner.y + 9), 28, {248, 234, 200, 255});
    // little tank glyph in front of the "x N" counter
    int gx = (int)(banner.x + 34) + MeasureText("Mission 88   ", 28), gy = (int)banner.y + 16;
    DrawRectangle(gx, gy + 8, 24, 4, {248, 234, 200, 255});
    DrawRectangle(gx + 3, gy + 2, 18, 7, {248, 234, 200, 255});
    DrawRectangle(gx + 12, gy - 1, 13, 3, {248, 234, 200, 255});
    // P1 score panel, bottom-left
    Rectangle panel = {-18, sh - 96.0f, 150, 84};
    DrawRectangleRounded(panel, 0.6f, 8, {235, 240, 248, 235});
    DrawText("P1", 24, sh - 90, 26, {40, 90, 200, 255});
    DrawText(TextFormat("%d", score), 34, sh - 56, 40, {70, 140, 235, 255});
}

void drawMissionClear(int mission) {
    centerText(TextFormat("Mission %d cleared!", mission + 1),
               GetScreenHeight() / 2 - 30, 48, RAYWHITE);
}

void drawPaused() {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {15, 10, 8, 150});
    centerText("PAUSED", GetScreenHeight() / 2 - 40, 54, RAYWHITE);
    centerText("[P] resume    [Q] quit to title", GetScreenHeight() / 2 + 30, 24, LIGHTGRAY);
}

void drawReplayBanner(int mission) {
    char buf[48];
    snprintf(buf, sizeof buf, "REPLAY - Mission %d", mission + 1);
    centerText(buf, 24, 30, {255, 210, 90, 255});
    centerText("[Enter] back to title", GetScreenHeight() - 44, 20, GRAY);
}

void drawPlayerDeath(int livesLeft) {
    centerText("Your tank is destroyed!", GetScreenHeight() / 2 - 30, 44, {255, 120, 100, 255});
    if (livesLeft > 0)
        centerText(TextFormat("%d live(s) remaining", livesLeft),
                   GetScreenHeight() / 2 + 30, 26, LIGHTGRAY);
}

void drawGameOver() {
    centerText("GAME OVER", GetScreenHeight() / 2 - 30, 60, {255, 100, 80, 255});
}

void drawResults(const CampaignStats& stats, int firstMission, int lastMission, bool won) {
    ClearBackground({25, 20, 15, 255});
    centerText(won ? "Campaign complete!" : "Campaign over", 50, 48,
               won ? Color{140, 220, 140, 255} : RAYWHITE);
    int total = 0;
    int colW = 130, rows = 5;
    if (lastMission - firstMission > 19) firstMission = lastMission - 19;   // last 20 fit on screen
    int x0 = GetScreenWidth() / 2 - 2 * colW, y0 = 140;
    for (int m = firstMission; m <= lastMission && m < MISSION_COUNT; ++m) {
        int i = m - firstMission;
        int cx = x0 + (i / rows) * colW, cy = y0 + (i % rows) * 34;
        DrawText(TextFormat("M%-2d  %d", m + 1, stats.kills[m]), cx, cy, 24, LIGHTGRAY);
        total += stats.kills[m];
    }
    centerText(TextFormat("Total kills: %d    Deaths: %d", total, stats.deaths),
               y0 + rows * 34 + 30, 28, RAYWHITE);
    centerText("[Enter] Title", GetScreenHeight() - 70, 24, GRAY);
}

static std::string progressPath() {
    const char* xdg = std::getenv("XDG_DATA_HOME");
    std::string base;
    if (xdg && *xdg) base = xdg;
    else {
        const char* home = std::getenv("HOME");
        if (!home) return "";
        base = std::string(home) + "/.local/share";
    }
    return base + "/tanks/progress.txt";
}

std::string replayPath() {
    std::string p = progressPath();
    if (p.empty()) return "";
    return p.substr(0, p.rfind('/')) + "/last.replay";
}

int loadProgress() {
    std::string p = progressPath();
    if (p.empty()) return 0;
    std::ifstream f(p);
    int m = 0;
    if (f >> m && m >= 0 && m <= MISSION_COUNT) return m;
    return 0;
}

void saveProgress(int mission) {
    std::string p = progressPath();
    if (p.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(p).parent_path(), ec);
    if (ec) return;
    std::ofstream f(p);                  // best-effort: ignore failures entirely
    if (f) f << mission << "\n";
}
