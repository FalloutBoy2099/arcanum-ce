#ifndef ARCANUM_GAME_COMPANION_MOVEMENT_H_
#define ARCANUM_GAME_COMPANION_MOVEMENT_H_

#include "game/context.h"

bool companion_movement_escape(int64_t companion, int64_t requester, int64_t* destination);
bool companion_movement_escape_route(int64_t companion, int64_t requester,
    const int64_t* route, int count, int64_t* destination);
bool companion_movement_follow_destination(int64_t companion, int64_t leader, int64_t* destination);
bool companion_movement_repath_needed(int64_t planned_leader_tile, int64_t leader_tile, int remaining_steps);
// Revalidate the destination at the moment a party member changes tiles.
// Returns a live blocking critter; no traversal callbacks or scripts execute.
int64_t companion_movement_blocker(int64_t mover, int64_t destination);

#endif
