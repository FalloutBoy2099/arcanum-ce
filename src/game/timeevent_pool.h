#ifndef ARCANUM_GAME_TIMEEVENT_POOL_H_
#define ARCANUM_GAME_TIMEEVENT_POOL_H_

#include "game/timeevent_node.h"

#define TIMEEVENT_POOL_LIMIT 1024
typedef struct TimeEventPool {
    TimeEventNode* available[TIMEEVENT_POOL_LIMIT];
    int count;
} TimeEventPool;

TimeEventNode* timeevent_pool_acquire(TimeEventPool* pool);
void timeevent_pool_release(TimeEventPool* pool, TimeEventNode* node);
// Frees only available nodes; active nodes remain owned by the scheduler.
void timeevent_pool_clear(TimeEventPool* pool);

#endif
