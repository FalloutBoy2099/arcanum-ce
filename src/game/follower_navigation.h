#ifndef ARCANUM_GAME_FOLLOWER_NAVIGATION_H_
#define ARCANUM_GAME_FOLLOWER_NAVIGATION_H_

#include "game/anim_private.h"

bool follower_navigation_is_following(const AnimRunInfo* run_info, int64_t leader);
void follower_navigation_turn_flags(AnimRunInfo* run_info, bool pivot_active);

#endif
