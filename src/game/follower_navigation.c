#include "game/follower_navigation.h"

bool follower_navigation_is_following(const AnimRunInfo* run_info, int64_t leader)
{
    if (leader == OBJ_HANDLE_NULL) return false;
    for (int index = 0; index <= run_info->current_goal; index++) {
        const AnimGoalData* goal = &run_info->goals[index];
        // AG_FOLLOW is replaced (pop + push), not retained as an ancestor.
        // Its flags/target are carried to the replacement and its subgoals.
        if (goal->params[AGDATA_TARGET_OBJ].obj == leader
            && (goal->type == AG_FOLLOW
                || ((goal->params[AGDATA_FLAGS_DATA].data & AG_FLAG_FOLLOWING_LEADER) != 0
                    && (goal->type == AG_RUN_NEAR_OBJ || goal->type == AG_MOVE_NEAR_OBJ)))) return true;
    }
    return false;
}

void follower_navigation_turn_flags(AnimRunInfo* run_info, bool pivot_active)
{
    // Clearing bit 0x10 tells the scheduler to pop/rebuild the movement goal.
    // Keep it alive until the pivot has finished, otherwise startup cancels it.
    if (pivot_active) run_info->flags |= 0x10;
    else run_info->flags &= ~0x10;
}

void follower_navigation_blocked_step(AnimRunInfo* run_info)
{
    // Discard the stale path without consuming a tile or an action point.
    // The parent movement goal will rebuild it using the original destination.
    run_info->path.flags = (run_info->path.flags & ~0x02) | 0x05;
    run_info->flags &= ~0x30;
    run_info->extra_target_tile = 0;
}

bool follower_navigation_wait_for_tile(FollowerNavigationWait* wait, AnimRunInfo* run_info,
    int64_t from, int64_t to, tig_timestamp_t now)
{
    if (!wait->active || wait->unique_id != run_info->id.unique_id
        || wait->goal != run_info->current_goal || wait->from != from || wait->to != to) {
        wait->active = true;
        wait->unique_id = run_info->id.unique_id;
        wait->goal = run_info->current_goal;
        wait->from = from;
        wait->to = to;
        wait->started = now;
        wait->pause_ms = run_info->pause_time.milliseconds;
    }
    if ((tig_timestamp_t)(now - wait->started) >= 1200) {
        follower_navigation_clear_wait(wait, run_info);
        follower_navigation_blocked_step(run_info);
        // First search around the occupant. The normal player fallback may
        // still request passage if there is no route through a narrow doorway.
        run_info->flags &= ~0x400;
        return false;
    }
    // Preserve the path and destination while the occupant finishes its step.
    // Bit 0x10 routes the scheduler back to movement, not the stop/pause branch.
    run_info->flags |= 0x10;
    run_info->pause_time.milliseconds = 100;
    return true;
}

void follower_navigation_clear_wait(FollowerNavigationWait* wait, AnimRunInfo* run_info)
{
    if (wait->active && wait->unique_id == run_info->id.unique_id
        && wait->goal == run_info->current_goal) {
        run_info->pause_time.milliseconds = wait->pause_ms;
    }
    wait->active = false;
}
