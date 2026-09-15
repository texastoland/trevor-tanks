#pragma once
#include "raylib.h"
#include "game.h"

struct Models {
    Model hull, hullTop, tread, dome, barrel, muzzle;   // tank parts (offsets baked into transforms)
    Model wall, crate, shell, shellNose, mine;    // shell = casing, shellNose = pointed tip
    Texture2D floorTex;
    Model floor;                          // rebuilt per arena by the renderer
    Shader lit;                           // directional light: models are flat-shaded without it
};

Color tankColor(TankType t);
Models buildModels();
void buildFloor(Models& m, int aw, int ah);   // (re)creates floorTex + floor plane
void unloadModels(Models& m);
