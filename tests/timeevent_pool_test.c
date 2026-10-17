#include "game/timeevent_pool.h"
#include <tig/performance.h>
#include <stdio.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)

int main(void)
{
    CHECK(SDL_Init(0));
    CHECK(tig_memory_init(NULL) == TIG_OK);
    static TimeEventPool pool;
    TigMemoryStatistics baseline, before, after;
    tig_memory_statistics(&baseline);
    TimeEventNode* active = timeevent_pool_acquire(&pool);
    CHECK(active != NULL);
    if (active == NULL) return 1;
    memset(active, 0xAB, sizeof(*active));
    TimeEventNode* reusable = timeevent_pool_acquire(&pool);
    memset(reusable, 0xCD, sizeof(*reusable));
    timeevent_pool_release(&pool, reusable);
    tig_memory_statistics(&before);
    for (int i = 0; i < 100000; i++) {
        TimeEventNode* node = timeevent_pool_acquire(&pool);
        CHECK(node == reusable);
        const unsigned char* bytes = (const unsigned char*)node;
        for (int j = 0; j < sizeof(*node); j++) CHECK(bytes[j] == 0);
        memset(node, i % 255 + 1, sizeof(*node));
        timeevent_pool_release(&pool, node);
    }
    tig_memory_statistics(&after);
    CHECK(after.allocation_requests == before.allocation_requests);
    const unsigned char* active_bytes = (const unsigned char*)active;
    for (int i = 0; i < sizeof(*active); i++) CHECK(active_bytes[i] == 0xAB);
    timeevent_pool_clear(&pool);
    CHECK(pool.count == 0);
    tig_memory_statistics(&after);
    CHECK(after.live_blocks == baseline.live_blocks + 1);
    CHECK(after.live_bytes == baseline.live_bytes + sizeof(*active));
    timeevent_pool_release(&pool, active);
    TimeEventNode* many[TIMEEVENT_POOL_LIMIT + 50];
    for (int i = 0; i < SDL_arraysize(many); i++) {
        many[i] = timeevent_pool_acquire(&pool);
        CHECK(many[i] != NULL);
        for (int j = 0; j < i; j++) CHECK(many[i] != many[j]);
    }
    for (int i = 0; i < SDL_arraysize(many); i++) timeevent_pool_release(&pool, many[i]);
    CHECK(pool.count == TIMEEVENT_POOL_LIMIT);
    tig_memory_statistics(&after);
    CHECK(after.live_blocks == baseline.live_blocks + TIMEEVENT_POOL_LIMIT);
    CHECK(after.live_bytes == baseline.live_bytes + TIMEEVENT_POOL_LIMIT * sizeof(TimeEventNode));
    timeevent_pool_clear(&pool);
    timeevent_pool_clear(&pool);
    timeevent_pool_release(&pool, NULL);
    tig_memory_statistics(&after);
    CHECK(after.live_bytes == baseline.live_bytes && after.live_blocks == baseline.live_blocks);
    CHECK(tig_performance_set_enabled(true, "event-pool-test.csv"));
    TimeEventNode* node = timeevent_pool_acquire(&pool);
    timeevent_pool_release(&pool, node);
    node = timeevent_pool_acquire(&pool);
    CHECK(tig_performance_counter(TIG_PERF_EVENT_NODES_NEW) == 1);
    CHECK(tig_performance_counter(TIG_PERF_EVENT_NODES_REUSED) == 1);
    timeevent_pool_release(&pool, node);
    timeevent_pool_clear(&pool);
    tig_performance_set_enabled(false, NULL);
    remove("event-pool-test.csv");
    printf("Event pool: 100000 reuse cycles, zero additional allocations; %s\n", failures ? "FAIL" : "PASS");
    tig_memory_exit();
    SDL_Quit();
    return failures != 0;
}
