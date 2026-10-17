#include "game/path_open_set.h"

#include <assert.h>
#include <string.h>

static int less(PathOpenSet* set, int a, int b)
{
    set->comparisons++;
    return set->estimates[a] < set->estimates[b]
        || (set->estimates[a] == set->estimates[b] && a < b);
}

static void swap(PathOpenSet* set, int a, int b)
{
    int cell = set->cells[a];
    set->cells[a] = set->cells[b];
    set->cells[b] = cell;
    set->positions[set->cells[a]] = a;
    set->positions[set->cells[b]] = b;
}

static int sift_up(PathOpenSet* set, int pos)
{
    while (pos > 0) {
        int parent = (pos - 1) / 2;
        if (!less(set, set->cells[pos], set->cells[parent])) break;
        swap(set, pos, parent);
        pos = parent;
    }
    return pos;
}

static void sift_down(PathOpenSet* set, int pos)
{
    while (2 * pos + 1 < set->count) {
        int child = 2 * pos + 1;
        if (child + 1 < set->count && less(set, set->cells[child + 1], set->cells[child])) child++;
        if (!less(set, set->cells[child], set->cells[pos])) break;
        swap(set, child, pos);
        pos = child;
    }
}

void path_open_set_init(PathOpenSet* set)
{
    memset(set->positions, -1, sizeof(set->positions));
    set->count = 0;
    set->comparisons = 0;
}

void path_open_set_update(PathOpenSet* set, int cell, int estimate)
{
    assert(cell >= 0 && cell < PATH_OPEN_SET_CAPACITY);
    int pos = set->positions[cell];
    if (pos < 0) {
        assert(set->count < PATH_OPEN_SET_CAPACITY);
        pos = set->count++;
        set->cells[pos] = cell;
        set->positions[cell] = pos;
    }
    set->estimates[cell] = estimate;
    pos = sift_up(set, pos);
    sift_down(set, pos);
}

void path_open_set_remove(PathOpenSet* set, int cell)
{
    assert(cell >= 0 && cell < PATH_OPEN_SET_CAPACITY);
    int pos = set->positions[cell];
    if (pos < 0) return;
    set->positions[cell] = -1;
    set->count--;
    if (pos == set->count) return;
    set->cells[pos] = set->cells[set->count];
    set->positions[set->cells[pos]] = pos;
    pos = sift_up(set, pos);
    sift_down(set, pos);
}

int path_open_set_pop(PathOpenSet* set)
{
    if (set->count == 0) return -1;
    int cell = set->cells[0];
    path_open_set_remove(set, cell);
    return cell;
}
