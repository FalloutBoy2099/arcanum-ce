#include "game/progression_rules.h"

#include <limits.h>
#include <stdint.h>

int progression_points_add(int points, int change)
{
    int64_t result = (int64_t)points + change;
    if (result < 0) return 0;
    if (result > INT_MAX) return INT_MAX;
    return (int)result;
}

int progression_kill_experience(int worth, int remaining, bool kill_only)
{
    if (worth <= 0) return 0;
    if (!kill_only) return worth / 5;
    if (remaining <= 0) return 0;
    return remaining < worth ? remaining : worth;
}

int progression_training_target(int training, bool master_educator)
{
    if (training < 0 || training > 3) return 0;
    return master_educator ? training : (training > 0 ? training - 1 : 0);
}

unsigned int progression_elapsed_years(unsigned int before, unsigned int after)
{
    if (after < before) return 0;
    return after / 360 - before / 360;
}

unsigned int progression_next_year_day(unsigned int day)
{
    uint64_t next = ((uint64_t)day / 360 + 1) * 360;
    return next <= UINT_MAX ? (unsigned int)next : 0;
}
