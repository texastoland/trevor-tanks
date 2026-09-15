#pragma once

constexpr float PI_F = 3.14159265f;
constexpr float DT   = 1.0f / 120.0f;

// Geometry (grid cell = 1.0 unit)
constexpr float TANK_RADIUS  = 0.35f;
constexpr float SHELL_RADIUS = 0.10f;
constexpr float BARREL_LEN   = 0.55f;

// Player
constexpr float PLAYER_SPEED         = 2.2f;
constexpr int   PLAYER_MAX_SHELLS    = 5;
constexpr int   PLAYER_MAX_MINES     = 2;
constexpr float PLAYER_FIRE_COOLDOWN = 0.05f;   // wiki: player fire rate is "Controlled" (tap speed)
constexpr float FIRE_MOVE_PAUSE      = 0.12f;   // shorter than the tap interval: stop-AND-GO, not a lock
constexpr float INPUT_BUFFER_TIME    = 0.20f;   // a click stays pending until it can act

// Shells / movement speed tiers
constexpr float SHELL_SLOW   = 3.0f;
constexpr float SHELL_NORMAL = 4.5f;
constexpr float SHELL_FAST   = 7.0f;
constexpr float MOVE_SLOW     = 1.0f;
constexpr float MOVE_NORMAL   = 1.6f;
constexpr float MOVE_FAST     = 2.2f;
constexpr float MOVE_VERYFAST = 2.6f;
constexpr float MOVE_EXTREME  = 3.0f;   // black: "can easily outrun your bullets" (SHELL_SLOW = 3.0)

// Mines
constexpr float MINE_ARM_TIME       = 1.0f;
constexpr float MINE_LIFETIME       = 10.0f;
constexpr float MINE_TRIGGER_RADIUS = 0.9f;
constexpr float MINE_BLAST_RADIUS   = 1.8f;
constexpr float MINE_BODY_RADIUS    = 0.22f;   // for shells shooting a mine

// AI
constexpr float TURRET_TURN_SPEED = 4.0f;      // rad/s for enemies
constexpr float REPLAN_INTERVAL   = 0.5f;
constexpr float BROWN_SCAN_RATE   = 0.9f;      // rad/s: brown turrets sweep, they do not seek
constexpr float DODGE_RADIUS      = 0.9f;
constexpr float MINE_AVOID_RADIUS = 2.6f;      // comfortably > blast kill radius (1.8 + tank 0.35)
constexpr float MINE_PATH_RADIUS  = 1.3f;      // A* treats cells this close to a mine as blocked
constexpr float AIM_FIRE_TOLERANCE = 0.06f;    // rad
constexpr int   RICOCHET_SAMPLES  = 240;
constexpr float RICOCHET_RANGE    = 30.0f;

// Campaign
constexpr int START_LIVES = 3;
