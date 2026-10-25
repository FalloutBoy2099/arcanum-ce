#include "game/inventory_load.h"

int inventory_load_level(int64_t amount, int64_t capacity)
{
    static const int thresholds[] = { 15, 30, 45, 60, 75, 90 };
    int level;

    if (amount <= 0) {
        return 0;
    }
    if (capacity <= 0 || amount >= capacity) {
        return 6;
    }
    for (level = 0; level < 6; level++) {
        int64_t threshold = capacity / 100 * thresholds[level]
            + capacity % 100 * thresholds[level] / 100;
        if (amount < threshold) {
            return level;
        }
    }
    return 6;
}

int inventory_load_speed_penalty(int weight_level, int bulk_level)
{
    int weight = weight_level == 6 ? 8 : weight_level;
    int bulk = bulk_level == 6 ? 8 : bulk_level;
    return weight + bulk;
}

int inventory_load_fatigue_delay(int level)
{
    static const int delays[] = { 0, 0, 0, 0, 1200000, 120000, 10000 };
    return level >= 0 && level < 7 ? delays[level] : 0;
}

int inventory_load_fatigue_damage(int elapsed_seconds)
{
    int damage = elapsed_seconds / 160;
    return damage > 0 ? damage : 1;
}

int inventory_grid_fits(int location, int width, int height, int capacity)
{
    return location >= 0 && location < capacity && width > 0 && height > 0
        && width <= INVENTORY_COLUMNS - location % INVENTORY_COLUMNS
        && height <= capacity / INVENTORY_COLUMNS - location / INVENTORY_COLUMNS;
}
