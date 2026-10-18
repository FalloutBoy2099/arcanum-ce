#ifndef ARCANUM_GAME_ACTIVITY_TIME_H_
#define ARCANUM_GAME_ACTIVITY_TIME_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum ActivityTimeCategory {
    ACTIVITY_TIME_APPRENTICE,
    ACTIVITY_TIME_EXPERT,
    ACTIVITY_TIME_MASTER,
    ACTIVITY_TIME_EDUCATOR,
    ACTIVITY_TIME_STAT_POINTS,
    ACTIVITY_TIME_SKILL_POINTS,
    ACTIVITY_TIME_SPELL_POINTS,
    ACTIVITY_TIME_TECH_POINTS,
    ACTIVITY_TIME_SCHEMATIC,
    ACTIVITY_TIME_COUNT,
} ActivityTimeCategory;

void activity_time_init(void);
void activity_time_reset(void);
bool activity_time_save(void);
void activity_time_load(void);
void activity_time_record(int64_t actor, ActivityTimeCategory category, int count);
void activity_time_training(int64_t actor, int before, int after);
void activity_time_ping(void);
unsigned int activity_time_parse_hours(const char* text);

#endif
