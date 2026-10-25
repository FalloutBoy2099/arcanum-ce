#include "game/level_progression.h"
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>

static bool level_experience_parse(const char* text, int* value)
{
    if (text == NULL) return false;
    char* end;
    long long number = strtoll(text, &end, 10);
    if (end == text || number < 0 || number > LEVEL_EXPERIENCE_MAX) return false;
    while (isspace((unsigned char)*end)) end++;
    if (*end != '\0') return false;
    *value = (int)number;
    return true;
}

bool level_progression_build(int* thresholds, const char* const* entries, size_t count)
{
    if (thresholds == NULL || entries == NULL || count != LEVEL_LIMIT) return false;
    bool extended = entries[51] != NULL;
    thresholds[0] = 0;
    for (int index = 1; index < LEVEL_LIMIT; index++) {
        int value;
        if (index < 50 || (extended && entries[index] != NULL)) {
            if (!level_experience_parse(entries[index], &value)) return false;
        } else {
            int level = index + 1;
            int step = (level + (level >= 75 ? 3 : 2)) * 1000;
            if (thresholds[index - 1] > LEVEL_EXPERIENCE_MAX - step) return false;
            value = thresholds[index - 1] + step;
        }
        if (value <= thresholds[index - 1]) return false;
        thresholds[index] = value;
    }
    return true;
}

int level_progression_bonus_points(int old_level, int new_level)
{
    if (old_level < 0 || new_level < old_level || new_level > LEVEL_PLAYABLE_MAX) return 0;
    return new_level - old_level + new_level / 5 - old_level / 5;
}

int level_progression_percent(int level, int experience, const int* thresholds)
{
    if (thresholds == NULL || level < 1 || level >= LEVEL_PLAYABLE_MAX) return 0;
    int64_t start = thresholds[level - 1];
    int64_t end = thresholds[level];
    if (end <= start || experience <= start) return 0;
    if (experience >= end) return 1000;
    return (int)(((int64_t)experience - start) * 1000 / (end - start));
}
