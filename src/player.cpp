#include "player.h"

Intent playerIntent(const Tank& t, const InputState& in) {
    Intent i;
    if (in.up)    i.move.y -= 1;
    if (in.down)  i.move.y += 1;
    if (in.left)  i.move.x -= 1;
    if (in.right) i.move.x += 1;
    i.move = norm(i.move);
    i.aim = angleTo(t.pos, in.aimWorld);
    i.fire = in.fire;
    i.layMine = in.mine;
    return i;
}
