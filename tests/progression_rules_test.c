#include "game/progression_rules.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static int failures;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #value); failures++; } } while (0)

int main(void)
{
    CHECK(progression_points_add(156, 1) == 157);
    CHECK(progression_points_add(5000, 2) == 5002);
    CHECK(progression_points_add(INT_MAX - 1, 2) == INT_MAX);
    CHECK(progression_points_add(0, -1) == 0);
    CHECK(progression_points_add(INT_MAX, INT_MAX) == INT_MAX);
    CHECK(progression_kill_experience(100, 100, false) == 20);
    CHECK(progression_kill_experience(100, 100, true) == 100);
    CHECK(progression_kill_experience(100, 60, true) == 60);
    CHECK(progression_kill_experience(100, 0, true) == 0);
    CHECK(progression_kill_experience(100, 500, true) == 100);
    CHECK(progression_kill_experience(INT_MAX, INT_MAX, true) == INT_MAX);
    CHECK(progression_kill_experience(-1, 100, true) == 0);
    CHECK(progression_training_target(3, false) == 2);
    CHECK(progression_training_target(3, true) == 3);
    CHECK(progression_training_target(1, false) == 0);
    CHECK(progression_training_target(1, true) == 1);
    CHECK(progression_training_target(4, true) == 0);
    CHECK(progression_elapsed_years(0, 359) == 0);
    CHECK(progression_elapsed_years(359, 360) == 1);
    CHECK(progression_elapsed_years(359, 1080) == 3);
    CHECK(progression_elapsed_years(360, 359) == 0);
    CHECK(progression_next_year_day(0) == 360);
    CHECK(progression_next_year_day(359) == 360);
    CHECK(progression_next_year_day(360) == 720);
    CHECK(progression_next_year_day(UINT_MAX) == 0);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
