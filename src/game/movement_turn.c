#include "game/movement_turn.h"

int movement_rotation_distance(int from, int to)
{
    int delta = (to - from + 8) % 8;
    return delta > 4 ? 8 - delta : delta;
}

void movement_turn_begin(MovementTurn* turn, int target_rotation)
{
    turn->active = true;
    turn->target_rotation = target_rotation;
}

static int ease_offset(int offset)
{
    if (offset > 4) return offset - 4;
    if (offset < -4) return offset + 4;
    return 0;
}

bool movement_turn_step(MovementTurn* turn, tig_art_id_t* art_id, int* x, int* y)
{
    if (!turn->active) return false;
    int rotation = tig_art_id_rotation_get(*art_id);
    int delta = (turn->target_rotation - rotation + 8) % 8;
    if (delta != 0) {
        rotation = (rotation + (delta <= 4 ? 1 : 7)) % 8;
        *art_id = tig_art_id_rotation_set(*art_id, rotation);
    }
    *x = ease_offset(*x);
    *y = ease_offset(*y);
    if (rotation == turn->target_rotation && *x == 0 && *y == 0) turn->active = false;
    return true;
}
