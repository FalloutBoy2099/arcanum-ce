#include <stdio.h>
#include <string.h>
#include <tig/performance.h>

#include "game/companion_movement.h"
#include "game/benchmark.h"
#include "game/crafting.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/gamelib.h"
#include "game/item.h"
#include "game/location.h"
#include "game/obj.h"
#include "game/path.h"
#include "game/player.h"
#include "game/proto.h"
#include "game/tech.h"

typedef struct TestObject {
    int fields[OBJ_F_MAX];
    int64_t loc;
    int64_t prototype;
    int64_t owner;
    int64_t leader;
    int64_t inventory[32];
    int length;
    bool dead;
    bool unconscious;
    bool combat;
    bool destroyed;
    int inv_location;
} TestObject;

Settings settings;
static TestObject objects[256];
static bool party_components;
static int expertise = 50;
static int next_product = 150;
static int fail_creation = -1;
static bool inventory_full;
static bool blocked_grid[20][20];
static int failures;
static bool testing_benchmark_route;
static int occupied_destination_checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)

int settings_get_value(Settings* instance, const char* key) { (void)instance; (void)key; return party_components; }
int64_t player_get_local_pc_obj(void) { return 1; }
int obj_field_int32_get(int64_t obj, int fld) { return objects[obj].fields[fld]; }
int64_t obj_field_int64_get(int64_t obj, int fld) { (void)fld; return objects[obj].loc; }
int64_t obj_field_handle_get(int64_t obj, int fld) { (void)fld; return objects[obj].prototype; }
int64_t obj_arrayfield_handle_get(int64_t obj, int fld, int index)
{
    if (fld == OBJ_F_CRITTER_FOLLOWER_IDX) return index == 0 ? 2 : 0;
    return objects[obj].inventory[index];
}
int obj_arrayfield_length_get(int64_t obj, int fld) { (void)obj; (void)fld; return 1; }
int64_t critter_pc_leader_get(int64_t obj) { return objects[obj].fields[OBJ_F_TYPE] == OBJ_TYPE_NPC ? objects[obj].leader : 0; }
bool critter_is_dead(int64_t obj) { return objects[obj].dead; }
bool critter_is_unconscious(int64_t obj) { return objects[obj].unconscious; }
bool combat_critter_is_combat_mode_active(int64_t obj) { return objects[obj].combat; }
int item_inventory_location_get(int64_t obj) { return objects[obj].inv_location; }
int64_t sub_4685A0(int id) { return id >= 20 && id <= 30 ? id : 0; }
int sub_462410(int64_t obj, int* field)
{
    *field = objects[obj].fields[OBJ_F_TYPE] == OBJ_TYPE_AMMO ? OBJ_F_AMMO_QUANTITY : -1;
    return -1;
}
int tech_degree_level_get(int64_t obj, int tech) { (void)obj; (void)tech; return expertise; }

static int64_t loc(int x, int y) { return ((int64_t)y << 32) | (unsigned int)x; }
int64_t location_dist(int64_t a, int64_t b)
{
    int dx = abs((int)(a & 0xFFFFFFFF) - (int)(b & 0xFFFFFFFF));
    int dy = abs((int)(a >> 32) - (int)(b >> 32));
    return SDL_max(dx, dy);
}
int64_t object_dist(int64_t a, int64_t b) { return location_dist(objects[a].loc, objects[b].loc); }
static const int dx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
static const int dy[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
bool location_in_range(int64_t from, int dir, int distance, int64_t* to)
{
    CHECK(dir >= 0 && dir < 8);
    if (dir < 0 || dir >= 8) return false;
    int x = (int)(from & 0xFFFFFFFF) + dx[dir] * distance;
    int y = (int)(from >> 32) + dy[dir] * distance;
    if (x < 0 || x >= 20 || y < 0 || y >= 20) return false;
    *to = loc(x, y); return true;
}
int location_rot(int64_t from, int64_t to)
{
    int x = (int)(to & 0xFFFFFFFF) - (int)(from & 0xFFFFFFFF);
    int y = (int)(to >> 32) - (int)(from >> 32);
    x = (x > 0) - (x < 0); y = (y > 0) - (y < 0);
    for (int i = 0; i < 8; i++) if (dx[i] == x && dy[i] == y) return i;
    return 0;
}
int path_make(PathCreateInfo* path)
{
    CHECK((path->flags & (PATH_FLAG_0x0002 | PATH_FLAG_0x0004)) == (PATH_FLAG_0x0002 | PATH_FLAG_0x0004));
    CHECK((path->flags & PATH_FLAG_0x0010) == 0);
    CHECK(path->max_rotations <= (testing_benchmark_route ? 32 : 3));
    if (path->to == objects[1].loc) {
        occupied_destination_checks++;
        if ((path->flags & PATH_FLAG_0x0001) == 0) return 0;
    }
    int distance[20][20];
    for (int x = 0; x < 20; x++) for (int y = 0; y < 20; y++) distance[x][y] = -1;
    int64_t queue[400]; int head = 0, tail = 0;
    queue[tail++] = path->from;
    distance[(int)(path->from & 0xFFFFFFFF)][(int)(path->from >> 32)] = 0;
    while (head < tail) {
        int64_t current = queue[head++];
        int steps = distance[(int)(current & 0xFFFFFFFF)][(int)(current >> 32)];
        if (current == path->to) return steps;
        if (steps >= path->max_rotations) continue;
        for (int dir = 0; dir < 8; dir++) {
            int64_t next;
            if (!location_in_range(current, dir, 1, &next)) continue;
            int x = (int)(next & 0xFFFFFFFF), y = (int)(next >> 32);
            bool occupied = next == objects[1].loc
                && !(next == path->to && (path->flags & PATH_FLAG_0x0001) != 0);
            if (distance[x][y] != -1 || blocked_grid[x][y] || occupied) continue;
            distance[x][y] = steps + 1;
            queue[tail++] = next;
        }
    }
    return 0;
}

static void give(int64_t owner, int64_t item, int64_t prototype, int units)
{
    objects[item].fields[OBJ_F_TYPE] = objects[prototype].fields[OBJ_F_TYPE];
    objects[item].fields[OBJ_F_DESCRIPTION] = objects[prototype].fields[OBJ_F_DESCRIPTION];
    objects[item].fields[OBJ_F_AMMO_QUANTITY] = units;
    objects[item].fields[OBJ_F_AMMO_TYPE] = 2;
    objects[item].prototype = prototype;
    objects[item].owner = owner;
    objects[owner].inventory[objects[owner].length++] = item;
    objects[owner].fields[OBJ_F_CRITTER_INVENTORY_NUM] = objects[owner].length;
}
void object_destroy(int64_t obj)
{
    CHECK(!objects[obj].destroyed);
    objects[obj].destroyed = true;
    int64_t owner = objects[obj].owner;
    if (owner != 0) {
        for (int i = 0; i < objects[owner].length; i++) {
            if (objects[owner].inventory[i] != obj) continue;
            memmove(&objects[owner].inventory[i], &objects[owner].inventory[i + 1], (objects[owner].length - i - 1) * sizeof(int64_t));
            objects[owner].fields[OBJ_F_CRITTER_INVENTORY_NUM] = --objects[owner].length;
            break;
        }
    }
}
bool object_create(int64_t prototype, int64_t position, int64_t* obj)
{
    if (fail_creation == 0) return false;
    if (fail_creation > 0) fail_creation--;
    *obj = next_product++;
    objects[*obj].prototype = prototype;
    objects[*obj].loc = position;
    return true;
}
bool item_transfer(int64_t obj, int64_t owner) { if (inventory_full) return false; objects[obj].owner = owner; return true; }
bool item_ammo_transfer(int64_t from, int64_t to, int qty, int type, int64_t obj)
{
    (void)to;
    CHECK(from == objects[obj].owner);
    CHECK(type == 2); // The ammo type must not be confused with stack quantity.
    CHECK(objects[obj].fields[OBJ_F_AMMO_QUANTITY] >= qty);
    objects[obj].fields[OBJ_F_AMMO_QUANTITY] -= qty;
    if (objects[obj].fields[OBJ_F_AMMO_QUANTITY] == 0) object_destroy(obj);
    return true;
}
bool item_gold_transfer(int64_t from, int64_t to, int qty, int64_t obj)
{
    (void)from; (void)to; (void)qty; (void)obj; return false;
}

static void reset(void)
{
    memset(objects, 0, sizeof(objects)); memset(blocked_grid, 0, sizeof(blocked_grid));
    objects[1].fields[OBJ_F_TYPE] = OBJ_TYPE_PC; objects[1].loc = loc(9, 10);
    objects[2].fields[OBJ_F_TYPE] = OBJ_TYPE_NPC; objects[2].leader = 1; objects[2].loc = loc(10, 10);
    for (int i = 20; i <= 30; i++) {
        objects[i].fields[OBJ_F_TYPE] = OBJ_TYPE_GENERIC;
        objects[i].fields[OBJ_F_DESCRIPTION] = 30000 + i;
        objects[i].fields[OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY] = -5;
    }
    party_components = false; expertise = 50; next_product = 150; fail_creation = -1; inventory_full = false;
}

static void test_crafting(void)
{
    CHECK(!crafting_learned_schematic_known(2000, 0));
    CHECK(crafting_learned_schematic_known(2000, 1));
    CHECK(!crafting_learned_schematic_known(2010, 1));
    CHECK(crafting_learned_schematic_known(2060, 7));
    CHECK(crafting_learned_schematic_known(2200, 1));
    CHECK(!crafting_learned_schematic_known(2260, 6));
    CHECK(!crafting_learned_schematic_known(4000, 7));
    SchematicInfo recipe = { 0 }; recipe.item1[0] = 20; recipe.item2[0] = 21; recipe.prod[0] = 24; recipe.qty = 1;
    CraftingIngredient pair[2];
    reset(); give(1, 100, 20, 1); give(2, 101, 21, 1);
    CHECK(!crafting_find_pair(1, &recipe, pair));
    party_components = true;
    CHECK(crafting_find_pair(1, &recipe, pair)); CHECK(pair[1].owner == 2);
    objects[101].inv_location = FIRST_WEAR_INV_LOC;
    CHECK(!crafting_find_pair(1, &recipe, pair));
    objects[101].inv_location = 0; objects[2].unconscious = true;
    CHECK(!crafting_find_pair(1, &recipe, pair));
    objects[2].unconscious = false; objects[2].loc = loc(19, 19);
    objects[1].loc = loc(0, 0); CHECK(!crafting_find_pair(1, &recipe, pair));
    objects[1].loc = loc(9, 10); objects[2].loc = loc(10, 10);
    expertise = 0; CHECK(!crafting_process(1, &recipe)); CHECK(!objects[100].destroyed);
    expertise = 50; recipe.qty = 3; fail_creation = 1;
    CHECK(!crafting_process(1, &recipe)); CHECK(!objects[100].destroyed && !objects[101].destroyed); CHECK(objects[150].destroyed);
    fail_creation = -1; recipe.qty = 1;
    CHECK(crafting_process(1, &recipe)); CHECK(objects[100].destroyed && objects[101].destroyed);
    CHECK(objects[151].owner == 1); CHECK(!crafting_products_dropped());

    reset(); recipe.item2[0] = 20; give(1, 100, 20, 1);
    CHECK(!crafting_process(1, &recipe)); CHECK(!objects[100].destroyed);
    give(1, 101, 20, 1); CHECK(crafting_process(1, &recipe)); CHECK(objects[100].destroyed && objects[101].destroyed);
    reset(); objects[20].fields[OBJ_F_TYPE] = OBJ_TYPE_AMMO; give(1, 100, 20, 2);
    CHECK(crafting_component_count(1, recipe.item1) == 2);
    CHECK(crafting_process(1, &recipe)); CHECK(objects[100].destroyed);

    // Component 1 must choose a different alternative when its first choice
    // is the only item that can satisfy component 2.
    reset(); recipe.item1[0] = 20; recipe.item1[1] = 21; recipe.item2[0] = 20; recipe.prod[1] = 25;
    give(1, 100, 20, 1); give(1, 101, 21, 1);
    CHECK(crafting_find_pair(1, &recipe, pair)); CHECK(pair[0].alternative == 1);
    inventory_full = true; CHECK(crafting_process(1, &recipe)); CHECK(crafting_products_dropped()); CHECK(objects[150].owner == 0);
}

static void test_crafting_preview(void)
{
    SchematicInfo recipe = { 0 };
    recipe.item1[0] = 20; recipe.item2[0] = 21; recipe.prod[0] = 24; recipe.qty = 1;
    reset(); give(1, 100, 20, 1); give(2, 101, 21, 1);
    CraftingPreview preview;
    CHECK(tig_performance_set_enabled(true, "crafting-preview-test.csv"));
    crafting_preview(1, &recipe, &preview);
    TigPerformanceSummary scans;
    CHECK(tig_performance_summary(TIG_PERF_CRAFTING, &scans));
    CHECK(scans.count == 1);
    tig_performance_set_enabled(false, NULL);
    remove("crafting-preview-test.csv");
    CHECK(preview.stock[0] == 1 && preview.stock[1] == 0 && !preview.pair_available);
    CHECK(preview.prototypes[0] == 20 && preview.prototypes[1] == 21);
    CHECK(preview.component_available[0] && !preview.component_available[1]);
    CHECK(preview.expertise_available[0] && preview.expertise_available[1]);
    party_components = true;
    crafting_preview(1, &recipe, &preview);
    CHECK(preview.pair_available && preview.stock[1] == 1 && preview.components[1].owner == 2);
    CHECK(preview.complexity[0] == 5);
    objects[2].unconscious = true;
    crafting_preview(1, &recipe, &preview);
    CHECK(!preview.pair_available && preview.stock[1] == 0);
    objects[2].unconscious = false;
    expertise = 0;
    crafting_preview(1, &recipe, &preview);
    CHECK(!preview.expertise_available[0] && !preview.expertise_available[1]);
    expertise = 50;
    crafting_preview(1, &recipe, &preview);
    CHECK(preview.pair_available);
    object_destroy(101);
    CHECK(!crafting_process(1, &recipe)); // A previously ready preview is not authority to consume.
    CHECK(!objects[100].destroyed);
    crafting_preview(1, &recipe, &preview);
    CHECK(!preview.pair_available && preview.stock[1] == 0);

    reset(); recipe.item2[0] = 20; give(1, 100, 20, 1);
    crafting_preview(1, &recipe, &preview);
    CHECK(preview.component_available[0] && preview.component_available[1] && !preview.pair_available);
    give(1, 101, 20, 1);
    crafting_preview(1, &recipe, &preview);
    CHECK(preview.pair_available && preview.stock[0] == 2 && preview.stock[1] == 2);
    reset(); objects[20].fields[OBJ_F_TYPE] = OBJ_TYPE_AMMO; give(1, 100, 20, 2);
    crafting_preview(1, &recipe, &preview);
    CHECK(preview.pair_available && preview.stock[0] == 2);
    recipe.item1[1] = 20; // Duplicate alternatives must not double-count stock.
    crafting_preview(1, &recipe, &preview);
    CHECK(preview.stock[0] == 2 && preview.components[0].alternative == 0);
    objects[100].fields[OBJ_F_AMMO_QUANTITY] = 100000;
    crafting_preview(1, &recipe, &preview);
    CHECK(preview.stock[0] == 99999);
    recipe.item1[0] = 999; recipe.item1[1] = 0;
    crafting_preview(1, &recipe, &preview);
    CHECK(preview.prototypes[0] == OBJ_HANDLE_NULL && !preview.expertise_available[0]);
    crafting_preview(OBJ_HANDLE_NULL, &recipe, &preview);
    CHECK(preview.stock[0] == 0 && !preview.pair_available);
    crafting_preview(1, NULL, &preview);
    CHECK(preview.stock[0] == 0 && !preview.pair_available);
}

static void test_benchmark_route(void)
{
    reset();
    testing_benchmark_route = true;
    occupied_destination_checks = 0;
    int64_t route[4], repeated[4];
    CHECK(benchmark_route_find(1, route));
    CHECK(route[3] == objects[1].loc && occupied_destination_checks == 1);
    CHECK(route[0] == loc(9, 6));
    CHECK(benchmark_route_find(1, repeated));
    CHECK(memcmp(route, repeated, sizeof(route)) == 0);
    blocked_grid[9][6] = true;
    CHECK(benchmark_route_find(1, route));
    CHECK(route[0] == loc(13, 10));
    memset(blocked_grid, 1, sizeof(blocked_grid));
    blocked_grid[9][10] = false;
    CHECK(!benchmark_route_find(1, route));
    CHECK(!benchmark_route_find(OBJ_HANDLE_NULL, route));
    CHECK(!benchmark_route_find(1, NULL));
    testing_benchmark_route = false;
}

static void test_passages(void)
{
    reset(); int64_t destination = 0;
    CHECK(companion_movement_escape(2, 1, &destination));
    CHECK(location_dist(objects[2].loc, destination) == 3);
    CHECK(location_dist(objects[1].loc, destination) > location_dist(objects[1].loc, objects[2].loc));
    objects[2].combat = true; CHECK(!companion_movement_escape(2, 1, &destination));
    objects[2].combat = false; objects[1].combat = true; CHECK(!companion_movement_escape(2, 1, &destination));
    objects[1].combat = false; objects[2].leader = 0; CHECK(!companion_movement_escape(2, 1, &destination));
    objects[2].leader = 1; objects[2].unconscious = true; CHECK(!companion_movement_escape(2, 1, &destination));
    objects[2].unconscious = false;
    memset(blocked_grid, 1, sizeof(blocked_grid)); blocked_grid[10][10] = false;
    CHECK(!companion_movement_escape(2, 1, &destination));
    blocked_grid[11][10] = false;
    CHECK(companion_movement_escape(2, 1, &destination)); CHECK(destination == loc(11, 10));
    CHECK(!companion_movement_escape(1, 2, &destination));
}

int main(void)
{
    CHECK(SDL_Init(0)); tig_memory_init(NULL);
    test_crafting(); test_crafting_preview(); test_benchmark_route(); test_passages();
    tig_memory_exit(); SDL_Quit();
    printf("Crafting and companion movement: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures != 0;
}
