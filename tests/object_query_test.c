#include "game/object.h"
#include "game/obj_find.h"
#include "game/sector.h"
#include <tig/performance.h>
#include <stdio.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)

unsigned int reference_excluded_flags;
int reference_lists;
void reference_object_list_location(int64_t loc, unsigned int flags, ObjectList* objects);

static Sector fixture;
static ObjectNode nodes[64];
static struct { int type; int flags; int64_t loc; bool hidden; bool fire; } objects[65];
struct FindNode { int index; };
static FindNode find_nodes[65];
static bool loaded, lock_success;
static int64_t sector_id;
static int locks, unlocks, allocations, count, reads;

int64_t sector_id_from_loc(int64_t loc) { (void)loc; return sector_id; }
void sector_limits_get(int64_t* x, int64_t* y) { *x = 8; *y = 8; }
bool sector_loaded(int64_t id) { (void)id; return loaded; }
bool sector_lock(int64_t id, Sector** sector) { (void)id; locks++; *sector = &fixture; return lock_success; }
bool sector_unlock(int64_t id) { (void)id; unlocks++; return true; }
int tile_id_from_loc(int64_t loc) { (void)loc; return 7; }
int obj_field_int32_get(int64_t obj, int field) { reads++; return field == OBJ_F_FLAGS ? objects[obj].flags : objects[obj].type; }
int64_t obj_field_int64_get(int64_t obj, int field) { CHECK(field == OBJ_F_LOCATION); return objects[obj].loc; }
bool object_type_is_sector_static(int64_t obj) { return objects[obj].hidden; }
bool obj_find_walk_first(int64_t id, int64_t* obj, FindNode** iter)
{
    (void)id;
    if (count == 0) return false;
    *obj = 1; *iter = &find_nodes[1]; return true;
}
bool obj_find_walk_next(int64_t* obj, FindNode** iter)
{
    if ((*iter)->index >= count) return false;
    *obj = (*iter)->index + 1; (*iter)++; return true;
}
ObjectNode* object_node_create(void) { allocations++; return MALLOC(sizeof(ObjectNode)); }
void object_node_destroy(ObjectNode* node) { FREE(node); }

static bool fire(int64_t obj, void* context)
{
    bool wanted = *(bool*)context;
    return objects[obj].fire == wanted;
}

static int64_t reference(int64_t loc, unsigned int types, ObjectQueryPredicate predicate, void* context)
{
    ObjectList list;
    reference_object_list_location(loc, types, &list);
    int64_t result = OBJ_HANDLE_NULL;
    for (ObjectNode* node = list.head; node != NULL; node = node->next) {
        if (predicate == NULL || predicate(node->obj, context)) { result = node->obj; break; }
    }
    for (ObjectNode* node = list.head; node != NULL;) {
        ObjectNode* next = node->next; object_node_destroy(node); node = next;
    }
    for (int i = 0; i < list.num_sectors; i++) sector_unlock(list.sectors[i]);
    return result;
}

int main(void)
{
    CHECK(SDL_Init(0));
    CHECK(tig_memory_init(NULL) == TIG_OK);
    unsigned int random = 48271;
    for (int i = 0; i < 65; i++) find_nodes[i].index = i;
    int removed_copies = 0;
    for (int run = 0; run < 3000; run++) {
        random = random * 1664525U + 1013904223U;
        count = random % 65;
        loaded = (run % 2) != 0;
        lock_success = run % 7 != 0;
        sector_id = SECTOR_MAKE((int64_t)(run % 17 == 0 ? 9 : 2), (int64_t)3);
        fixture.objects.heads[7] = count ? &nodes[0] : NULL;
        for (int i = 1; i <= count; i++) {
            random = random * 1664525U + 1013904223U;
            objects[i].type = random % 18;
            objects[i].flags = (random % 3 == 0 ? OF_OFF : 0)
                | (random % 5 == 0 ? OF_DESTROYED : 0) | (random % 7 == 0 ? OF_INVENTORY : 0);
            objects[i].loc = random % 4 == 0 ? 101 : 100;
            objects[i].hidden = random % 6 == 0;
            objects[i].fire = random % 5 == 0;
            nodes[i - 1].obj = i;
            nodes[i - 1].next = i < count ? &nodes[i] : NULL;
        }
        reference_excluded_flags = run % 3 == 0 ? 0 : OF_OFF | OF_DESTROYED;
        unsigned int types = run % 4 == 0 ? OBJ_TM_TRAP : run % 4 == 1 ? OBJ_TM_SCENERY : run % 4 == 2 ? OBJ_TM_CRITTER : random & OBJ_TM_ALL;
        bool wanted = run % 3 == 0;
        ObjectQueryPredicate predicate = run % 3 != 0 ? fire : NULL;
        locks = unlocks = allocations = 0;
        int64_t expected = reference(100, types, predicate, &wanted);
        removed_copies += allocations;
        int old_locks = locks, old_unlocks = unlocks;
        locks = unlocks = allocations = 0;
        int64_t actual = object_query_first(100, types, reference_excluded_flags, predicate, &wanted);
        CHECK(actual == expected);
        CHECK(locks == old_locks && unlocks == old_unlocks);
        CHECK(allocations == 0);
    }
    // A trap query stops on the first match instead of inspecting/copying all
    // other objects. Both the lock and its release are still required.
    count = 64; loaded = false; lock_success = true;
    sector_id = SECTOR_MAKE((int64_t)2, (int64_t)3);
    reference_excluded_flags = 0;
    fixture.objects.heads[7] = nodes;
    for (int i = 1; i <= count; i++) {
        objects[i].type = OBJ_TYPE_TRAP;
        objects[i].flags = 0;
        nodes[i - 1].obj = i;
        nodes[i - 1].next = i < count ? &nodes[i] : NULL;
    }
    reads = allocations = 0;
    CHECK(reference(100, OBJ_TM_TRAP, NULL, NULL) == 1);
    CHECK(allocations == 64 && reads == 128);
    CHECK(tig_performance_set_enabled(true, "object-query-test.csv"));
    reads = allocations = locks = unlocks = 0;
    CHECK(object_query_first(100, OBJ_TM_TRAP, 0, NULL, NULL) == 1);
    CHECK(reads == 2 && allocations == 0 && locks == 1 && unlocks == 1);
    CHECK(tig_performance_counter(TIG_PERF_OBJECT_QUERIES) == 1);
    CHECK(tig_performance_counter(TIG_PERF_OBJECT_QUERY_VISITS) == 1);
    tig_performance_set_enabled(false, NULL);
    remove("object-query-test.csv");
    // Restore paths previously discarded an ALL-types list after loading its
    // sector. Preserve locking behavior, including failures and map bounds.
    for (int mode = 0; mode < 4; mode++) {
        sector_id = SECTOR_MAKE((int64_t)(mode == 3 ? 9 : 2), (int64_t)3);
        lock_success = mode != 2;
        loaded = mode == 0;
        locks = unlocks = reads = allocations = 0;
        reference(100, OBJ_TM_ALL, NULL, NULL);
        int expected_locks = locks, expected_unlocks = unlocks;
        locks = unlocks = reads = allocations = 0;
        CHECK(object_query_load_sector(100) == (mode < 2));
        CHECK(locks == expected_locks && unlocks == expected_unlocks);
        CHECK(reads == 0 && allocations == 0);
    }
    TigMemoryStatistics memory;
    tig_memory_statistics(&memory);
    CHECK(memory.live_blocks == 0);
    printf("Object queries: 3000 differential fixtures, %d temporary nodes avoided; %s\n", removed_copies, failures ? "FAIL" : "PASS");
    tig_memory_exit();
    SDL_Quit();
    return failures != 0;
}
