#include <stdio.h>
#include <stdint.h>
#include "game/inventory_load.h"

static int failures;
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #condition); failures++; } } while (0)

int main(void)
{
    int level;
    static const int thresholds[] = { 28, 57, 85, 114, 142, 171 };
    for (level = 0; level < 6; level++) {
        CHECK(inventory_load_level(thresholds[level] - 1, 190) == level);
        CHECK(inventory_load_level(thresholds[level], 190) == level + 1);
    }
    CHECK(inventory_load_level(0, 0) == 0);
    CHECK(inventory_load_level(1, 0) == 6);
    CHECK(inventory_load_level(INT64_MAX, 190) == 6);
    CHECK(inventory_load_level(INT64_MAX / 2, INT64_MAX) == 3);
    CHECK(inventory_load_level(190, 190) == 6);
    for (level = 0; level < 7; level++) {
        int original_speed_penalty = (level == 6 ? 8 : level) * 2;
        CHECK(inventory_load_speed_penalty(level, 0) * 2 == original_speed_penalty);
        CHECK(inventory_load_speed_penalty(0, level) * 2 == original_speed_penalty);
        CHECK(inventory_load_speed_penalty(level, level) == original_speed_penalty);
        CHECK(inventory_load_fatigue_delay(level) == 0 || level >= 4);
    }
    CHECK(inventory_load_fatigue_delay(4) == 1200000);
    CHECK(inventory_load_fatigue_delay(5) == 120000);
    CHECK(inventory_load_fatigue_delay(6) == 10000);
    CHECK(inventory_load_fatigue_damage(1200) == 7);
    CHECK(inventory_load_fatigue_damage(120) == 1);
    CHECK(inventory_load_fatigue_damage(10) == 1);
    CHECK(INVENTORY_CRITTER_CELLS == 720);
    CHECK(INVENTORY_CONTAINER_CELLS == 960);
    CHECK(inventory_grid_fits(719, 1, 1, 720));
    CHECK(!inventory_grid_fits(719, 2, 1, 720));
    CHECK(!inventory_grid_fits(719, 1, 2, 720));
    CHECK(!inventory_grid_fits(-1, 1, 1, 720));
    CHECK(!inventory_grid_fits(720, 1, 1, 720));
    CHECK(inventory_grid_fits(958, 2, 1, 960));
    return failures != 0;
}
