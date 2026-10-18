#include <stdio.h>
#include <string.h>
#include <tig/performance.h>
#include "game/critter.h"
#include "game/follower_navigation.h"
#include "game/light.h"
#include "game/location.h"
#include "game/movement_turn.h"
#include "game/object.h"
#include "game/path.h"
#include "game/path_open_set.h"
#include "game/sector.h"
#include "game/terrain.h"
#include "game/tile.h"
#include "game/trap.h"

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)
static unsigned char grid[64][64];
static unsigned int random_state = 12345;
static int actor_type = OBJ_TYPE_PC;
static struct { int type; int flags; bool dead; } destination_objects[3];
static int destination_count;
static const int dx[8] = { -1, -1, -1, 0, 1, 1, 1, 0 };
static const int dy[8] = { -1, 0, 1, 1, 1, 0, -1, -1 };
int reference_astar(PathCreateInfo* path);
int path_test_astar(PathCreateInfo* path);

static unsigned int next_random(void) { random_state = random_state * 1664525U + 1013904223U; return random_state; }
static int cell(int64_t loc) { return grid[(int)LOCATION_GET_X(loc)][(int)LOCATION_GET_Y(loc)]; }
int obj_field_int32_get(int64_t obj, int field)
{
    if (obj >= 100 && obj < 103) return field == OBJ_F_TYPE ? destination_objects[obj - 100].type : destination_objects[obj - 100].flags;
    return field == OBJ_F_TYPE ? actor_type : 0;
}
bool critter_is_dead(int64_t obj) { return obj >= 100 && obj < 103 && destination_objects[obj - 100].dead; }
int64_t object_find_first_at_location(int64_t loc, unsigned int flags, ObjectQueryPredicate predicate, void* context)
{
    (void)loc;
    for (int i = 0; i < destination_count; i++) {
        if ((flags & (1U << destination_objects[i].type)) != 0 && (predicate == NULL || predicate(100 + i, context))) return 100 + i;
    }
    return OBJ_HANDLE_NULL;
}
void object_list_location(int64_t loc, unsigned int flags, ObjectList* list) { (void)loc; (void)flags; memset(list, 0, sizeof(*list)); }
void object_list_destroy(ObjectList* list) { (void)list; }
bool tile_is_blocking(int64_t loc, bool ignored) { (void)ignored; return cell(loc) == 1; }
int64_t get_trap_at_location(int64_t loc) { return cell(loc) == 2 ? 2 : 0; }
int64_t get_fire_at_location(int64_t loc) { return cell(loc) == 3 ? 3 : 0; }
bool trap_is_spotted(int64_t actor, int64_t trap) { (void)actor; (void)trap; return true; }
tig_color_t light_luminance_at_location(int64_t loc, int x, int y) { (void)x; (void)y; return (tig_color_t)((LOCATION_GET_X(loc) % 4) * 50); }
int64_t location_dist(int64_t a, int64_t b) { return SDL_max(llabs((int64_t)LOCATION_GET_X(a) - LOCATION_GET_X(b)), llabs((int64_t)LOCATION_GET_Y(a) - LOCATION_GET_Y(b))); }
bool location_in_dir(int64_t loc, int direction, int64_t* next)
{
    int x = (int)LOCATION_GET_X(loc) + dx[direction];
    int y = (int)LOCATION_GET_Y(loc) + dy[direction];
    if (x < 0 || x >= 64 || y < 0 || y >= 64) return false;
    *next = location_make(x, y); return true;
}
bool location_in_range(int64_t loc, int direction, int range, int64_t* next)
{
    for (int i = 0; i < range; i++) if (!location_in_dir(loc, direction, &loc)) return false;
    *next = loc; return true;
}
int location_rot(int64_t a, int64_t b)
{
    int x = (int)LOCATION_GET_X(b) - (int)LOCATION_GET_X(a);
    int y = (int)LOCATION_GET_Y(b) - (int)LOCATION_GET_Y(a);
    if (abs(x) > 2 * abs(y)) y = 0;
    else if (abs(y) > 2 * abs(x)) x = 0;
    x = (x > 0) - (x < 0); y = (y > 0) - (y < 0);
    for (int dir = 0; dir < 8; dir++) if (dx[dir] == x && dy[dir] == y) return dir;
    return 0;
}
void location_xy(int64_t loc, int64_t* x, int64_t* y) { *x = LOCATION_GET_X(loc) * 80; *y = LOCATION_GET_Y(loc) * 40; }
int object_calc_traversal_cost(int64_t actor, int64_t loc, int direction, unsigned int flags,
    int64_t* blocker, int* type, bool* window)
{
    (void)actor;
    int64_t next;
    *blocker = 0; *type = 0;
    if (window != NULL) *window = false;
    if (!location_in_dir(loc, direction, &next)) return 1;
    int value = cell(next);
    if (window != NULL) *window = value == 5;
    if (value == 4 && (flags & OBJ_TRAVERSAL_PORTAL_BLOCKS) != 0) { *type = OBJ_TYPE_PORTAL; *blocker = 4; return 1; }
    if (value == 5 && (flags & OBJ_TRAVERSAL_WINDOW_BLOCKS) != 0) { *type = OBJ_TYPE_WALL; *blocker = 5; return 1; }
    if (value == 6 && (flags & OBJ_TRAVERSAL_IGNORE_CRITTERS) == 0) { *type = OBJ_TYPE_NPC; *blocker = 6; return 1; }
    return 0;
}
bool object_traversal_is_blocked(int64_t obj, int64_t loc, int rot, unsigned int flags, bool* window)
{
    int64_t blocker; int type;
    return object_calc_traversal_cost(obj, loc, rot, flags, &blocker, &type, window) != 0;
}
int sector_rot(int64_t a, int64_t b) { return location_rot(a, b); }
bool sector_in_dir(int64_t sector, int dir, int64_t* next) { return location_in_dir(sector, dir, next); }
bool sector_exists(uint64_t id) { (void)id; return true; }
bool sector_is_blocked(int64_t id) { (void)id; return false; }
bool terrain_is_blocked(int64_t id) { (void)id; return false; }

static void test_heap(void)
{
    static PathOpenSet set;
    static int estimates[4096];
    for (int i = 0; i < 4096; i++) estimates[i] = -1;
    path_open_set_init(&set);
    for (int iteration = 0; iteration < 40000; iteration++) {
        int cell_id = (int)(next_random() % 4096);
        switch (next_random() % 4) {
        case 0: estimates[cell_id] = -1; path_open_set_remove(&set, cell_id); break;
        case 1: {
            int expected = -1;
            for (int i = 0; i < 4096; i++) if (estimates[i] >= 0 && (expected < 0 || estimates[i] < estimates[expected])) expected = i;
            CHECK(path_open_set_pop(&set) == expected);
            if (expected >= 0) estimates[expected] = -1;
            break;
        }
        default: estimates[cell_id] = (int)(next_random() % 500); path_open_set_update(&set, cell_id, estimates[cell_id]); break;
        }
    }
    // Full capacity, ties, removal and reopening.
    path_open_set_init(&set);
    for (int i = 4095; i >= 0; i--) path_open_set_update(&set, i, 10);
    for (int i = 0; i < 4096; i++) CHECK(path_open_set_pop(&set) == i);
    CHECK(path_open_set_pop(&set) == -1);
}

static void test_paths(void)
{
    Uint64 old_checks = 0, new_checks = 0, old_time = 0, new_time = 0;
    CHECK(tig_performance_set_enabled(true, "pathfinding-test.csv"));
    for (int trial = 0; trial < 600; trial++) {
        for (int x = 0; x < 64; x++) for (int y = 0; y < 64; y++) {
            unsigned int value = next_random() % 100;
            grid[x][y] = value < 20 ? 1 : value < 24 ? 2 : value < 28 ? 3 : value < 30 ? 4 : value < 32 ? 5 : value < 34 ? 6 : 0;
        }
        int64_t from = location_make(16 + next_random() % 16, 16 + next_random() % 16);
        int64_t to = location_make(32 + next_random() % 16, 32 + next_random() % 16);
        grid[LOCATION_GET_X(from)][LOCATION_GET_Y(from)] = 0;
        grid[LOCATION_GET_X(to)][LOCATION_GET_Y(to)] = 0;
        int8_t old_path[200], new_path[200];
        PathCreateInfo path = { 0 };
        path.obj = 1; path.from = from; path.to = to;
        path.max_rotations = trial % 5 == 0 ? 8 : 100;
        path.flags = (trial % 2 ? PATH_FLAG_AVOID_FIRE : 0) | (trial % 3 ? PATH_FLAG_PORTALS_BLOCK : 0)
            | (trial % 4 ? PATH_FLAG_WINDOWS_BLOCK : 0) | (trial % 7 == 0 ? PATH_FLAG_AVOID_SPOTTED_TRAPS : 0)
            | (trial % 11 == 0 ? PATH_FLAG_STOP_BEFORE_DESTINATION : 0);
        path.rotations = old_path;
        Uint64 count = tig_performance_counter(TIG_PERF_PATH_SCANNED_CELLS);
        Uint64 start = SDL_GetPerformanceCounter();
        int old_length = reference_astar(&path);
        old_time += SDL_GetPerformanceCounter() - start;
        old_checks += tig_performance_counter(TIG_PERF_PATH_SCANNED_CELLS) - count;
        path.rotations = new_path;
        count = tig_performance_counter(TIG_PERF_PATH_SCANNED_CELLS);
        start = SDL_GetPerformanceCounter();
        int new_length = path_test_astar(&path);
        new_time += SDL_GetPerformanceCounter() - start;
        new_checks += tig_performance_counter(TIG_PERF_PATH_SCANNED_CELLS) - count;
        CHECK(new_length == old_length);
        if (new_length == old_length && new_length > 0) CHECK(memcmp(new_path, old_path, new_length) == 0);
    }
    tig_performance_set_enabled(false, NULL);
    CHECK(new_checks < old_checks / 10);
    printf("A* 600 maps: old %.2f ms / heap %.2f ms; selection checks %llu / %llu\n",
        old_time * 1000.0 / SDL_GetPerformanceFrequency(), new_time * 1000.0 / SDL_GetPerformanceFrequency(),
        (unsigned long long)old_checks, (unsigned long long)new_checks);
}

static void test_turns(void)
{
    for (int from = 0; from < 8; from++) for (int to = 0; to < 8; to++) {
        tig_art_id_t art_id;
        CHECK(tig_art_critter_id_create(0, 0, 0, 0, 7, from, TIG_ART_ANIM_RUN, 0, 0, &art_id) == TIG_OK);
        MovementTurn turn = { 0 };
        movement_turn_begin(&turn, to);
        int x = 11, y = -7, steps = 0;
        while (turn.active && steps < 10) {
            int before = tig_art_id_rotation_get(art_id), old_x = x, old_y = y;
            CHECK(movement_turn_step(&turn, &art_id, &x, &y));
            CHECK(movement_rotation_distance(before, tig_art_id_rotation_get(art_id)) <= 1);
            CHECK(abs(x - old_x) <= 4 && abs(y - old_y) <= 4);
            CHECK(tig_art_id_frame_get(art_id) == 7);
            CHECK(tig_art_id_anim_get(art_id) == TIG_ART_ANIM_RUN);
            steps++;
        }
        CHECK(!turn.active && steps <= 4);
        CHECK(tig_art_id_rotation_get(art_id) == to && x == 0 && y == 0);
        CHECK(!movement_turn_step(&turn, &art_id, &x, &y));
    }
}

static void test_destination_blockers(void)
{
    memset(grid, 0, sizeof(grid));
    int8_t rotations[100];
    PathCreateInfo path = { 0 };
    path.obj = 1; path.from = location_make(20, 20); path.to = location_make(21, 21);
    path.rotations = rotations; path.max_rotations = 100;
    destination_count = 1;
    destination_objects[0].type = OBJ_TYPE_NPC;
    CHECK(path_make(&path) == 0);
    destination_objects[0].dead = true;
    CHECK(path_make(&path) == 1);
    destination_objects[0].dead = false;
    destination_objects[0].flags = OF_NO_BLOCK;
    CHECK(path_make(&path) == 1);
    destination_objects[0].flags = 0;
    destination_objects[0].type = OBJ_TYPE_SCENERY;
    CHECK(path_make(&path) == 0);
    destination_objects[0].type = OBJ_TYPE_CONTAINER;
    CHECK(path_make(&path) == 0);
    destination_objects[0].flags = OF_NO_BLOCK;
    destination_count = 2;
    destination_objects[1].type = OBJ_TYPE_PC;
    CHECK(path_make(&path) == 0); // Continue past a nonblocking first object.
    destination_objects[1].dead = true;
    CHECK(path_make(&path) == 1);
    destination_objects[1].dead = false;
    path.flags = PATH_FLAG_STOP_BEFORE_DESTINATION;
    CHECK(path_make(&path) == 0); // Stop adjacent to the occupied destination.
    path.flags = PATH_FLAG_IGNORE_BLOCKERS;
    CHECK(path_make(&path) == 1); // Explicitly unchecked paths preserve their flags.
    path.flags = 0;
    destination_count = 0;
    CHECK(path_make(&path) == 1); // State changes are seen on the next request.
}

static void test_limits_and_public_paths(void)
{
    memset(grid, 0, sizeof(grid));
    int8_t rotations[200];
    PathCreateInfo path = { 0 };
    path.obj = 1; path.from = location_make(20, 20); path.to = location_make(40, 40);
    path.rotations = rotations; path.max_rotations = 100;
    CHECK(path_make(&path) == 8); // Existing segment-length contract.
    grid[21][21] = 1;
    CHECK(path_make(&path) == 8); // Detour through the actual heap-backed solver.
    path.flags = PATH_FLAG_DIRECT_ONLY;
    CHECK(path_make(&path) == 0); // Formation probing cannot start an expensive detour.
    path.flags = PATH_FLAG_LONG_RANGE_FALLBACK;
    CHECK(path_make(&path) == 8); // Previously unfinished branch now returns a valid length.
    memset(grid, 0, sizeof(grid));
    path.flags = 0; path.to = location_make(21, 21);
    actor_type = OBJ_TYPE_NPC;
    CHECK(path_set_limit(2));
    CHECK(path_test_astar(&path) == 1);
    CHECK(path_test_astar(&path) == 1);
    CHECK(path_test_astar(&path) == 0); // Exactly two admitted requests, not three.
    actor_type = OBJ_TYPE_PC;
    CHECK(path_test_astar(&path) == 1); // NPC budget never blocks player navigation.
    CHECK(path_set_limit(10));
}

static void test_replaced_follow_goal_and_pivot_scheduler(void)
{
    AnimRunInfo run = { 0 };
    run.current_goal = 0;
    run.goals[0].type = AG_FOLLOW;
    run.goals[0].params[AGDATA_TARGET_OBJ].obj = 42;
    run.goals[0].params[AGDATA_FLAGS_DATA].data = AG_FLAG_FOLLOWING_LEADER;
    CHECK(follower_navigation_is_following(&run, 42));
    // The real 0x70000000 transition replaces the root rather than pushing it.
    run.goals[0].type = AG_RUN_NEAR_OBJ;
    CHECK(follower_navigation_is_following(&run, 42));
    run.current_goal = 1;
    run.goals[1] = run.goals[0];
    run.goals[1].type = AG_ATTEMPT_MOVE;
    CHECK(follower_navigation_is_following(&run, 42));
    CHECK(!follower_navigation_is_following(&run, 43));
    CHECK(!follower_navigation_is_following(&run, OBJ_HANDLE_NULL));
    run.goals[0].type = AG_MOVE_NEAR_OBJ;
    run.goals[1].type = AG_ATTEMPT_MOVE_NEAR;
    CHECK(follower_navigation_is_following(&run, 42));
    run.goals[0].params[AGDATA_FLAGS_DATA].data = 0;
    CHECK(!follower_navigation_is_following(&run, 42)); // Manual order, not follow.
    run.current_goal = -1;
    CHECK(!follower_navigation_is_following(&run, 42));

    tig_art_id_t art_id;
    CHECK(tig_art_critter_id_create(0, 0, 0, 0, 7, 0, TIG_ART_ANIM_RUN, 0, 0, &art_id) == TIG_OK);
    MovementTurn turn = { 0 };
    int x = 7, y = -3;
    run.flags = 0x30;
    movement_turn_begin(&turn, 4);
    CHECK(movement_turn_step(&turn, &art_id, &x, &y));
    follower_navigation_turn_flags(&run, turn.active);
    CHECK((run.flags & 0x10) != 0); // Scheduler must keep the movement goal alive.
    int ticks = 1;
    while (turn.active && ticks < 8) {
        CHECK((run.flags & 0x10) != 0);
        CHECK(movement_turn_step(&turn, &art_id, &x, &y));
        follower_navigation_turn_flags(&run, true); // Yield even on the last pivot tick.
        CHECK(tig_art_id_frame_get(art_id) == 7);
        ticks++;
    }
    CHECK(ticks == 4 && !turn.active);
    CHECK(tig_art_id_rotation_get(art_id) == 4 && x == 0 && y == 0);
    CHECK((run.flags & 0x10) != 0);
    follower_navigation_turn_flags(&run, false); // Normal tile completion resumes.
    CHECK(run.flags == 0x20);
    run.path.curr = 3; run.path.max = 7; run.path.flags = 0x0A;
    run.flags = 0x430; run.extra_target_tile = 123;
    follower_navigation_blocked_step(&run);
    CHECK(run.path.curr == 3 && run.path.max == 7);
    CHECK((run.path.flags & 0x07) == 0x05 && (run.path.flags & 0x08) != 0);
    CHECK(run.flags == 0x400 && run.extra_target_tile == 0);
}

static void test_transient_occupancy_wait(void)
{
    AnimRunInfo run = { 0 };
    FollowerNavigationWait wait = { 0 };
    run.id.unique_id = 17;
    run.current_goal = 1;
    run.goals[0].type = AG_RUN_TO_TILE;
    run.goals[0].params[AGDATA_TARGET_TILE].loc = 999;
    run.path.curr = 3; run.path.max = 7; run.path.flags = 0x02;
    run.flags = 0x421;
    run.pause_time.milliseconds = 65;
    run.extra_target_tile = 456;

    CHECK(follower_navigation_wait_for_tile(&wait, &run, 123, 456, 1000));
    CHECK(follower_navigation_wait_for_tile(&wait, &run, 123, 456, 2199));
    CHECK(wait.started == 1000 && wait.active);
    CHECK((run.flags & 0x10) != 0 && run.pause_time.milliseconds == 100);
    CHECK(run.path.curr == 3 && run.path.max == 7 && run.path.flags == 0x02);
    CHECK(run.goals[0].params[AGDATA_TARGET_TILE].loc == 999);
    // Occupant leaves: recover the original animation cadence and path.
    follower_navigation_clear_wait(&wait, &run);
    CHECK(!wait.active && run.pause_time.milliseconds == 65);
    CHECK(run.path.curr == 3 && run.path.flags == 0x02);

    CHECK(follower_navigation_wait_for_tile(&wait, &run, 123, 456, 3000));
    CHECK(!follower_navigation_wait_for_tile(&wait, &run, 123, 456, 4200));
    CHECK(!wait.active && run.pause_time.milliseconds == 65);
    CHECK((run.path.flags & 0x07) == 0x05 && (run.flags & 0x430) == 0);
    CHECK(run.extra_target_tile == 0 && run.path.curr == 3 && run.path.max == 7);
    CHECK(run.current_goal == 1 && run.goals[0].params[AGDATA_TARGET_TILE].loc == 999);

    // A recycled slot/new goal/new tile must not inherit the old deadline.
    CHECK(follower_navigation_wait_for_tile(&wait, &run, 123, 456, 5000));
    run.id.unique_id++;
    run.pause_time.milliseconds = 70;
    CHECK(follower_navigation_wait_for_tile(&wait, &run, 123, 456, 7000));
    CHECK(wait.started == 7000 && wait.pause_ms == 70);
    run.current_goal++;
    CHECK(follower_navigation_wait_for_tile(&wait, &run, 123, 456, 9000));
    CHECK(wait.started == 9000);
    CHECK(follower_navigation_wait_for_tile(&wait, &run, 456, 789, 11000));
    CHECK(wait.started == 11000);
    follower_navigation_clear_wait(&wait, &run);

    // The millisecond timer wraps; neither immediate expiry nor infinite wait.
    CHECK(follower_navigation_wait_for_tile(&wait, &run, 123, 456, 0xFFFFFF00U));
    CHECK(follower_navigation_wait_for_tile(&wait, &run, 123, 456, 0x00000050U));
    CHECK(!follower_navigation_wait_for_tile(&wait, &run, 123, 456, 0x00000400U));
}

int main(void)
{
    CHECK(SDL_Init(0));
    test_heap(); test_paths(); test_turns(); test_destination_blockers(); test_limits_and_public_paths(); test_replaced_follow_goal_and_pivot_scheduler();
    test_transient_occupancy_wait();
    SDL_Quit();
    printf("Pathfinding and pivots: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures != 0;
}
