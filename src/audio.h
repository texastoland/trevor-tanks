#pragma once
#include "game.h"

bool audioInit();
void audioHandle(const std::vector<GameEvent>& events);
// layered marching percussion: one instrument per tank type still alive,
// mixed live on a shared 2-second bar clock (like the original)
void audioMusicTick(float dt, const bool typeAlive[], bool active);
void audioTreadTick(bool isPlayer, bool highTone);   // alternating two-stroke tread clack
void audioTreadBed(float level);   // continuous grind while armor moves (0..1.2)
void audioShutdown();
