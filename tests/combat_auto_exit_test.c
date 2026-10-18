#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/ai.h"
#include "game/anim.h"
#include "game/combat.h"
#include "game/combat_auto_exit.h"
#include "game/critter.h"
#include "game/gamelib.h"
#include "game/location.h"
#include "game/obj.h"
#include "game/obj_private.h"
#include "game/object.h"
#include "game/object_node.h"
#include "game/player.h"

typedef struct TestCritter {
    int fields[OBJ_F_MAX];
    int64_t location;
    int64_t target;
    bool party;
    bool active;
    bool fighting;
    bool combat;
    bool faction;
    int priority;
} TestCritter;

Settings settings;
static TestCritter critters[5];
static int failures;
static int exits;
static int lists;
static int enabled;
static bool turn_based;
static bool opponents;
static bool may_exit;
static int64_t turn;
static tig_timestamp_t timestamp;

#define CHECK(expr) do { if (!(expr)) { printf("FAIL line %d: %s\n", __LINE__, #expr); failures++; } } while (0)

int settings_get_value(Settings* config, const char* key) { (void)config; (void)key; return enabled; }
int64_t player_get_local_pc_obj(void) { return 1; }
bool obj_handle_is_valid(int64_t obj) { return obj > 0 && obj < 5; }
int obj_field_int32_get(int64_t obj, int field) { return critters[obj].fields[field]; }
int64_t obj_field_int64_get(int64_t obj, int field) { (void)field; return critters[obj].location; }
int64_t obj_field_handle_get(int64_t obj, int field) { (void)field; return critters[obj].target; }
bool critter_is_active(int64_t obj) { return critters[obj].active; }
bool critter_is_dead(int64_t obj) { return !critters[obj].active; }
bool critter_party_same(int64_t a, int64_t b) { return critters[a].party && critters[b].party; }
bool critter_faction_same(int64_t a, int64_t b) { (void)a; return critters[b].faction; }
bool ai_is_fighting(int64_t obj) { return critters[obj].fighting; }
bool combat_critter_is_combat_mode_active(int64_t obj) { return critters[obj].combat; }
bool combat_can_exit_combat_mode(int64_t obj) { (void)obj; return may_exit; }
bool combat_turn_based_is_active(void) { return turn_based; }
int64_t combat_turn_based_whos_turn_get(void) { return turn; }
bool combat_turn_based_has_active_opponents(int64_t pc) { (void)pc; return opponents; }
int anim_foreground_goal_priority_get(int64_t obj) { return critters[obj].priority; }
void combat_critter_deactivate_combat_mode(int64_t obj) { critters[obj].combat = false; exits++; }

static int64_t loc(int x, int y) { return ((int64_t)y << 32) | (unsigned int)x; }
static void append(ObjectList* list, int64_t obj)
{
    ObjectNode* node = malloc(sizeof(*node));
    node->obj = obj;
    node->next = list->head;
    list->head = node;
    lists++;
}
void object_list_team(int64_t pc, ObjectList* list)
{
    (void)pc;
    memset(list, 0, sizeof(*list));
    append(list, 1);
    append(list, 2);
}
void object_list_rect(LocRect* bounds, unsigned int types, ObjectList* list)
{
    CHECK(types == OBJ_TM_NPC);
    memset(list, 0, sizeof(*list));
    for (int i = 2; i < 5; i++) {
        int64_t x = LOCATION_GET_X(critters[i].location);
        int64_t y = LOCATION_GET_Y(critters[i].location);
        if (x >= bounds->x1 && x <= bounds->x2 && y >= bounds->y1 && y <= bounds->y2) append(list, i);
    }
}
void object_list_destroy(ObjectList* list)
{
    while (list->head != NULL) {
        ObjectNode* node = list->head;
        list->head = node->next;
        free(node);
        lists--;
    }
}
static void reset(void)
{
    combat_auto_exit_tick(timestamp, false);
    memset(critters, 0, sizeof(critters));
    for (int i = 1; i < 5; i++) {
        critters[i].active = true;
        critters[i].location = loc(100, 100);
    }
    critters[1].party = critters[2].party = true;
    critters[1].combat = true;
    enabled = 1;
    exits = 0;
    turn_based = opponents = false;
    may_exit = true;
    turn = 1;
}
static void advance(int ticks, bool allowed)
{
    for (int i = 0; i < ticks; i++) {
        timestamp += 250;
        combat_auto_exit_tick(timestamp, allowed);
        CHECK(lists == 0);
    }
}
int main(void)
{
    reset();
    advance(7, true);
    CHECK(exits == 0);
    advance(1, true);
    CHECK(exits == 1);
    advance(8, true);
    CHECK(exits == 1);

    reset(); enabled = 0; advance(12, true); CHECK(exits == 0);
    reset(); advance(12, false); CHECK(exits == 0);
    reset(); may_exit = false; advance(12, true); CHECK(exits == 0);
    reset(); critters[2].priority = PRIORITY_3; advance(12, true); CHECK(exits == 0);
    reset(); turn_based = true; turn = 3; advance(12, true); CHECK(exits == 0);
    reset(); turn_based = opponents = true; advance(12, true); CHECK(exits == 0);
    reset(); critters[3].fighting = true; advance(12, true); CHECK(exits == 0);
    reset(); critters[3].combat = true; advance(12, true); CHECK(exits == 0);
    reset(); critters[3].target = 2; advance(12, true); CHECK(exits == 0);
    reset(); critters[3].fields[OBJ_F_NPC_FLAGS] = ONF_KOS; advance(12, true); CHECK(exits == 0);
    reset(); critters[3].fields[OBJ_F_CRITTER_FLAGS] = OCF_FLEEING; advance(12, true); CHECK(exits == 0);

    reset();
    critters[2].location = loc(500, 500);
    critters[3].location = loc(510, 510);
    critters[3].fighting = true;
    advance(12, true);
    CHECK(exits == 0);

    reset();
    advance(6, true);
    critters[3].fighting = true;
    advance(1, true);
    critters[3].fighting = false;
    advance(6, true);
    CHECK(exits == 0);
    advance(1, true);
    CHECK(exits == 1);

    reset(); advance(6, true); advance(1, false); advance(7, true); CHECK(exits == 0);
    advance(1, true); CHECK(exits == 1);
    reset(); critters[3].fighting = true; critters[3].active = false; advance(8, true); CHECK(exits == 1);
    reset(); critters[3].fighting = true; critters[3].fields[OBJ_F_CRITTER_FLAGS] = OCF_SURRENDERED;
    advance(8, true); CHECK(exits == 1);
    reset(); critters[3].fighting = true; critters[3].fields[OBJ_F_FLAGS] = OF_OFF;
    advance(8, true); CHECK(exits == 1);
    reset(); critters[2].fighting = critters[2].combat = true; advance(8, true); CHECK(exits == 1);
    reset(); critters[3].fighting = true; critters[3].location = loc(300, 300);
    advance(8, true); CHECK(exits == 1);
    reset(); turn_based = true; advance(8, true); CHECK(exits == 1);
    printf("Combat auto exit: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures != 0;
}
