#ifndef ARCANUM_GAME_ENCOUNTER_OPTIONS_H_
#define ARCANUM_GAME_ENCOUNTER_OPTIONS_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum RandomEncounterMode {
    RANDOM_ENCOUNTERS_NORMAL,
    RANDOM_ENCOUNTERS_INTERESTING,
    RANDOM_ENCOUNTERS_LESS_FREQUENT,
} RandomEncounterMode;

static inline int encounter_frequency_roll_max(int mode)
{
    return mode == RANDOM_ENCOUNTERS_LESS_FREQUENT ? 270 : 100;
}

static inline int encounter_frequency_threshold(int frequency)
{
    // Preserve the original effective cap before dividing by 2.7.
    return frequency < 0 ? 0 : frequency > 100 ? 100 : frequency;
}

static inline bool encounter_group_is_trivial(int player_level, int party_level,
    int strongest_enemy, int64_t enemy_levels)
{
    // Avoid suppressing early-game challenges or large low-level groups.
    return player_level >= 6 && strongest_enemy > 0 && enemy_levels > 0
        && (int64_t)strongest_enemy * 2 <= player_level
        && enemy_levels * 2 <= party_level;
}

#endif
