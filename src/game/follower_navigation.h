#ifndef ARCANUM_GAME_FOLLOWER_NAVIGATION_H_
#define ARCANUM_GAME_FOLLOWER_NAVIGATION_H_

#include "game/anim_private.h"

bool follower_navigation_is_following(const AnimRunInfo* run_info, int64_t leader);
void follower_navigation_turn_flags(AnimRunInfo* run_info, bool pivot_active);
void follower_navigation_blocked_step(AnimRunInfo* run_info);

// Runtime-only state: keep serialized animation/save layouts unchanged.
typedef struct FollowerNavigationWait {
    bool active;
    int unique_id;
    int goal;
    int64_t from;
    int64_t to;
    tig_timestamp_t started;
    int pause_ms;
    tig_art_id_t moving_aid;
} FollowerNavigationWait;

bool follower_navigation_wait_for_tile(FollowerNavigationWait* wait, AnimRunInfo* run_info,
    int64_t from, int64_t to, tig_timestamp_t now);
void follower_navigation_clear_wait(FollowerNavigationWait* wait, AnimRunInfo* run_info);

#endif
