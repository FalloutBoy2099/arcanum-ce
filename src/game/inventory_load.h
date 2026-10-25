#ifndef ARCANUM_GAME_INVENTORY_LOAD_H_
#define ARCANUM_GAME_INVENTORY_LOAD_H_

#include <stdint.h>

#define INVENTORY_BULK_CAPACITY 190
#define INVENTORY_CRITTER_ROWS 72
#define INVENTORY_CONTAINER_ROWS 96
#define INVENTORY_COLUMNS 10
#define INVENTORY_CRITTER_CELLS (INVENTORY_COLUMNS * INVENTORY_CRITTER_ROWS)
#define INVENTORY_CONTAINER_CELLS (INVENTORY_COLUMNS * INVENTORY_CONTAINER_ROWS)

int inventory_load_level(int64_t amount, int64_t capacity);
int inventory_load_speed_penalty(int weight_level, int bulk_level);
int inventory_load_fatigue_delay(int level);
int inventory_load_fatigue_damage(int elapsed_seconds);
int inventory_grid_fits(int location, int width, int height, int capacity);

#endif
