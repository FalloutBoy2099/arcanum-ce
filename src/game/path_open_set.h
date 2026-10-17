#ifndef ARCANUM_GAME_PATH_OPEN_SET_H_
#define ARCANUM_GAME_PATH_OPEN_SET_H_

#include <stdint.h>

#define PATH_OPEN_SET_CAPACITY 4096

// Indexed min-heap: one entry per cell, ordered by (estimated cost, cell ID).
// The cell ID tie-break preserves the previous linear scan's deterministic order.
typedef struct PathOpenSet {
    int cells[PATH_OPEN_SET_CAPACITY];
    int positions[PATH_OPEN_SET_CAPACITY];
    int estimates[PATH_OPEN_SET_CAPACITY];
    int count;
    uint64_t comparisons;
} PathOpenSet;

void path_open_set_init(PathOpenSet* set);
void path_open_set_update(PathOpenSet* set, int cell, int estimate);
void path_open_set_remove(PathOpenSet* set, int cell);
int path_open_set_pop(PathOpenSet* set);

#endif
