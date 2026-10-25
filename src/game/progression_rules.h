#ifndef ARCANUM_GAME_PROGRESSION_RULES_H_
#define ARCANUM_GAME_PROGRESSION_RULES_H_

#include <stdbool.h>

int progression_points_add(int points, int change);
int progression_kill_experience(int worth, int remaining, bool kill_only);
int progression_training_target(int training, bool master_educator);
unsigned int progression_elapsed_years(unsigned int before, unsigned int after);
unsigned int progression_next_year_day(unsigned int day);

#endif
