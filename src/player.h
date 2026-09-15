#pragma once
#include "game.h"
#include "combat.h"

Intent playerIntent(const Tank& t, const InputState& in);

// Holds a click's intent across frames/subticks until the action succeeds or
// the buffer expires — clicks must never be lost to frame/subtick alignment.
struct ClickBuffer {
    float t = 0;
    void press() { t = INPUT_BUFFER_TIME; }
    void hold() { if (t > 0) t = INPUT_BUFFER_TIME; }   // keeps an UNCONSUMED press alive;
    void tick(float dt) { t = t > 0 ? t - dt : 0; }     // never mints a new one (1 click = 1 shot)
    void consume() { t = 0; }
    bool pending() const { return t > 0; }
};
