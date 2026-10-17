#ifndef ARCANUM_GAME_MOVEMENT_TURN_H_
#define ARCANUM_GAME_MOVEMENT_TURN_H_

#include <stdbool.h>
#include <tig/art.h>

typedef struct MovementTurn {
    bool active;
    int target_rotation;
} MovementTurn;

int movement_rotation_distance(int from, int to);
// Pivot without advancing the gait frame; ease residual tile offsets to zero.
void movement_turn_begin(MovementTurn* turn, int target_rotation);
bool movement_turn_step(MovementTurn* turn, tig_art_id_t* art_id, int* x, int* y);

#endif
