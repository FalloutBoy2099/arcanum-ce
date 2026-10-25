#ifndef ARCANUM_LEVEL_PROGRESSION_H_
#define ARCANUM_LEVEL_PROGRESSION_H_
#include <stdbool.h>
#include <limits.h>
#include <stddef.h>

enum {
    LEVEL_PLAYABLE_MAX = 127,
    LEVEL_LIMIT = LEVEL_PLAYABLE_MAX + 1,
    LEVEL_UNSPENT_POINTS_MAX = INT_MAX,
    LEVEL_EXPERIENCE_MAX = 2000000000,
};

bool level_progression_build(int* thresholds, const char* const* entries, size_t count);
int level_progression_bonus_points(int old_level, int new_level);
int level_progression_percent(int level, int experience, const int* thresholds);

#endif
