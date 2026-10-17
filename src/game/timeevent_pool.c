#include "game/timeevent_pool.h"
#include <tig/performance.h>

TimeEventNode* timeevent_pool_acquire(TimeEventPool* pool)
{
    TimeEventNode* node;
    if (pool->count != 0) {
        node = pool->available[--pool->count];
        pool->available[pool->count] = NULL;
        tig_performance_count(TIG_PERF_EVENT_NODES_REUSED, 1);
    } else {
        node = (TimeEventNode*)MALLOC(sizeof(*node));
        if (node != NULL) tig_performance_count(TIG_PERF_EVENT_NODES_NEW, 1);
    }
    // In particular, never carry object references or links over from old events.
    if (node != NULL) memset(node, 0, sizeof(*node));
    return node;
}

void timeevent_pool_release(TimeEventPool* pool, TimeEventNode* node)
{
    if (node == NULL) return;
    if (pool->count < TIMEEVENT_POOL_LIMIT) pool->available[pool->count++] = node;
    else FREE(node);
}

void timeevent_pool_clear(TimeEventPool* pool)
{
    while (pool->count != 0) {
        FREE(pool->available[--pool->count]);
        pool->available[pool->count] = NULL;
    }
}
