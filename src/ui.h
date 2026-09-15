#pragma once
#include "levels.h"
#include <string>

struct CampaignStats {
    int kills[MISSION_COUNT] = {0};
    int deaths = 0;
};

struct TitlePanel {
    bool dev = false; int devMission = 0;
    bool tankEdit = false; int editRow = 0;
    int tankType = 0;                 // index into TankType; -1 = Custom
    EnemyParams stats{};
};
void drawTitle(int savedMission, int contMission, const TitlePanel& panel);
void drawMissionIntro(int mission, int enemyCount, int lives);
void drawHud(int mission, int enemiesAlive, int score);
void drawMissionClear(int mission);
void drawPaused();
void drawReplayBanner(int mission);
void drawPlayerDeath(int livesLeft);
void drawGameOver();
void drawResults(const CampaignStats& stats, int firstMission, int lastMission, bool won);

int loadProgress();
std::string replayPath();          // last attempt's replay file
void saveProgress(int mission);
