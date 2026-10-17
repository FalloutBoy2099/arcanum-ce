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
