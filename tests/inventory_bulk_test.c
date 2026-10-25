#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "game/item.h"
#include "game/stat.h"
#include "game/object.h"
#include "game/item_private.h"
#include "game/mp_utils.h"
#include "game/tf.h"
#include "game/script.h"

typedef struct TestObject {
    int type;
    int bulk;
    int quantity;
    int location;
    int count;
    int64_t owner;
    int64_t prototype;
    int64_t items[720];
    int race;
    int strength;
    int weight;
    int magic_weight_adjustment;
    int carry_weight;
    int free_location;
} TestObject;

static TestObject objects[32];
static int failures;
static int inventory_reads;
static int owner_weights[32];
static char transfer_trace[128];
static int transfer_trace_count, transfer_error;
static bool transfer_script_allowed = true, preferred_location_free;
static void trace(char event)
{
    if (transfer_trace_count < (int)sizeof(transfer_trace) - 1) {
        transfer_trace[transfer_trace_count++] = event;
        transfer_trace[transfer_trace_count] = '\0';
    }
}

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #condition); failures++; } } while (0)

int obj_field_int32_get(int64_t object, int field)
{
    switch (field) {
    case OBJ_F_TYPE: return objects[object].type;
    case OBJ_F_ITEM_PAD_I_1: return objects[object].bulk;
    case OBJ_F_AMMO_QUANTITY:
    case OBJ_F_GOLD_QUANTITY: return objects[object].quantity;
    case OBJ_F_CRITTER_INVENTORY_NUM: return objects[object].count;
    case OBJ_F_ITEM_WEIGHT: return objects[object].weight;
    case OBJ_F_ITEM_MAGIC_WEIGHT_ADJ: return objects[object].magic_weight_adjustment;
    default: return 0;
    }
}

int stat_level_get(int64_t object, int field)
{
    if (field == STAT_RACE) return objects[object].race;
    if (field == STAT_STRENGTH) return objects[object].strength;
    if (field == STAT_CARRY_WEIGHT && objects[object].carry_weight > 0) return objects[object].carry_weight;
    return 10000;
}
unsigned int item_armor_size(int race)
{
    switch (race) {
    case RACE_DWARF:
    case RACE_GNOME:
    case RACE_HALFLING:
        return OARF_SIZE_SMALL;
    case RACE_HALF_OGRE:
    case RACE_OGRE:
        return OARF_SIZE_LARGE;
    default:
        return OARF_SIZE_MEDIUM;
    }
}
int item_total_weight(int64_t object) { return owner_weights[object]; }
int item_weight(int64_t item, int64_t owner)
{
    (void)owner;
    if (objects[item].type == OBJ_TYPE_GOLD) return 0;
    int weight = objects[item].weight + objects[item].magic_weight_adjustment;
    return objects[item].type == OBJ_TYPE_AMMO ? weight * (objects[item].quantity / 4) : weight;
}
int background_get(int64_t object) { (void)object; return 0; }
int item_stack_counter_field(int64_t item, int* quantity_field)
{
    int type = objects[item].type;
    if (quantity_field != NULL) *quantity_field = type == OBJ_TYPE_AMMO ? OBJ_F_AMMO_QUANTITY : OBJ_F_GOLD_QUANTITY;
    return type == OBJ_TYPE_AMMO || type == OBJ_TYPE_GOLD ? 1 : -1;
}
int64_t item_wield_get(int64_t owner, int location)
{
    for (int i = 0; i < objects[owner].count; i++) {
        int64_t item = objects[owner].items[i];
        if (objects[item].location == location) return item;
    }
    return OBJ_HANDLE_NULL;
}
int item_find_free_inv_loc_for_insertion(int64_t item, int64_t owner)
{
    (void)item;
    return objects[owner].free_location;
}
int item_adjust_magic(int64_t item, int64_t owner, int value) { (void)item; (void)owner; return value; }
void item_error_msg(int64_t object, int reason) { (void)object; transfer_error = reason; }
void item_inventory_change_notify(int64_t object) { (void)object; }
int64_t obj_field_int64_get(int64_t object, int field) { (void)object; (void)field; return 0; }
void obj_field_int32_set(int64_t object, int field, int value)
{
    if (field == OBJ_F_AMMO_QUANTITY) objects[object].quantity = value;
}
bool object_script_execute(int64_t triggerer, int64_t attached, int64_t extra, int script, int parameter)
{
    (void)triggerer; (void)attached; (void)extra; (void)parameter;
    if (script == SAP_GET || script == SAP_TRANSFER) {
        trace(script == SAP_GET ? 'G' : 'T');
        return transfer_script_allowed;
    }
    return true;
}

void object_destroy(int64_t object) { objects[object].quantity = 0; }
int64_t item_ammo_obj(int64_t owner, int ammo_type)
{
    int index;
    (void)ammo_type;
    for (index = 0; index < objects[owner].count; index++) {
        int64_t item = objects[owner].items[index];
        if (objects[item].type == OBJ_TYPE_AMMO) return item;
    }
    return OBJ_HANDLE_NULL;
}
int64_t item_ammo_create(int quantity, int type, int64_t location)
{
    (void)type; (void)location;
    objects[30] = (TestObject){ .type = OBJ_TYPE_AMMO, .bulk = 1, .quantity = quantity, .prototype = 21 };
    return 30;
}
void object_pickup(int64_t object, int64_t owner) { (void)object; (void)owner; trace('P'); }
void item_remove(int64_t object)
{
    trace('R');
    int64_t owner = objects[object].owner;
    for (int index = 0; index < objects[owner].count; index++) {
        if (objects[owner].items[index] == object) {
            memmove(&objects[owner].items[index], &objects[owner].items[index + 1],
                (objects[owner].count - index - 1) * sizeof(objects[owner].items[0]));
            objects[owner].count--;
            break;
        }
    }
    objects[object].owner = 0;
}
void item_insert(int64_t object, int64_t owner, int location)
{
    trace('I');
    CHECK(location != -1);
    int field;
    if (item_stack_counter_field(object, &field) != -1) {
        int64_t existing = item_find_first_matching_prototype(owner, object);
        if (existing != 0) {
            int64_t quantity = (int64_t)objects[existing].quantity + objects[object].quantity;
            CHECK(quantity >= 0 && quantity <= INT_MAX);
            if (quantity < 0 || quantity > INT_MAX) return;
            objects[existing].quantity = (int)quantity;
            objects[object].quantity = 0;
            return;
        }
    }
    objects[object].owner = owner;
    objects[object].location = location;
    objects[owner].items[objects[owner].count++] = object;
}
bool item_cancel_decay(int64_t object) { (void)object; trace('C'); return true; }
bool item_decay(int64_t object, int milliseconds) { (void)object; CHECK(milliseconds == 172800000); trace('D'); return true; }
char* item_error_str(int reason) { transfer_error = reason; return "inventory test"; }
void tf_add(int64_t object, int type, const char* text) { (void)object; (void)type; (void)text; }
void mp_object_id_get_nullable(int64_t object, ObjectID* id) { (void)object; memset(id, 0, sizeof(*id)); }
void item_inventory_slots_get(int64_t owner, int* slots) { (void)owner; memset(slots, 0, INVENTORY_CONTAINER_CELLS * sizeof(*slots)); }
bool item_inventory_slots_has_room_for(int64_t item, int64_t owner, int location, int* slots)
{
    (void)item; (void)owner; (void)location; (void)slots; return preferred_location_free;
}

int64_t obj_field_handle_get(int64_t object, int field)
{
    return field == OBJ_F_PROTOTYPE_HANDLE ? objects[object].prototype : 0;
}

int64_t obj_arrayfield_handle_get(int64_t object, int field, int index)
{
    (void)field;
    inventory_reads++;
    return objects[object].items[index];
}

int item_inventory_location_get(int64_t object) { return objects[object].location; }
void item_inv_icon_size(int64_t object, int* width, int* height)
{
    (void)object;
    *width = 2;
    *height = 2;
}

bool item_parent(int64_t object, int64_t* owner)
{
    *owner = objects[object].owner;
    return *owner != OBJ_HANDLE_NULL;
}

int64_t item_find_key_ring(int64_t owner)
{
    int index;
    for (index = 0; index < objects[owner].count; index++) {
        int64_t item = objects[owner].items[index];
        if (objects[item].type == OBJ_TYPE_KEY_RING) return item;
    }
    return OBJ_HANDLE_NULL;
}

int64_t item_find_first_matching_prototype(int64_t owner, int64_t object)
{
    int index;
    for (index = 0; index < objects[owner].count; index++) {
        int64_t item = objects[owner].items[index];
        if (objects[item].prototype == objects[object].prototype) return item;
    }
    return OBJ_HANDLE_NULL;
}

static void transfer_test_reset(void)
{
    memset(objects, 0, sizeof(objects));
    memset(owner_weights, 0, sizeof(owner_weights));
    objects[1] = (TestObject){ .type = OBJ_TYPE_PC, .strength = 5, .free_location = 4 };
    objects[2] = (TestObject){ .type = OBJ_TYPE_NPC, .strength = 5, .free_location = 4 };
    objects[4] = (TestObject){ .type = OBJ_TYPE_GENERIC, .bulk = 2, .prototype = 24 };
    transfer_trace_count = 0; transfer_trace[0] = '\0'; transfer_error = 0;
    transfer_script_allowed = true; preferred_location_free = false;
}

static void transfer_test_cases(void)
{
    transfer_test_reset();
    CHECK(item_transfer(4, 1));
    CHECK(strcmp(transfer_trace, "GPIC") == 0 && objects[4].owner == 1 && objects[4].location == 4);
    transfer_test_reset();
    objects[4].owner = 2; objects[2].items[0] = 4; objects[2].count = 1;
    CHECK(item_transfer(4, 1));
    CHECK(strcmp(transfer_trace, "TRICD") == 0 && objects[2].count == 0 && objects[4].owner == 1);
    for (int owned = 0; owned < 2; owned++) {
        transfer_test_reset();
        if (owned) { objects[4].owner = 2; objects[2].items[0] = 4; objects[2].count = 1; }
        transfer_script_allowed = false;
        CHECK(!item_transfer(4, 1));
        CHECK(strcmp(transfer_trace, owned ? "T" : "G") == 0 && objects[4].owner == (owned ? 2 : 0));
        transfer_test_reset();
        objects[5] = (TestObject){ .type = OBJ_TYPE_AMMO, .quantity = INT_MAX, .bulk = 1, .prototype = 21, .owner = 1 };
        objects[4] = (TestObject){ .type = OBJ_TYPE_AMMO, .quantity = 1, .bulk = 1, .prototype = 21, .owner = owned ? 2 : 0 };
        objects[1].items[0] = 5; objects[1].count = 1;
        if (owned) { objects[2].items[0] = 4; objects[2].count = 1; }
        CHECK(!item_transfer_ex(4, 1, ITEM_INV_LOC_WEAPON));
        CHECK(transfer_error == ITEM_CANNOT_NO_ROOM && strcmp(transfer_trace, owned ? "T" : "G") == 0);
        CHECK(objects[5].quantity == INT_MAX && objects[4].quantity == 1 && objects[4].owner == (owned ? 2 : 0));
        transfer_test_reset();
        if (owned) { objects[4].owner = 2; objects[2].items[0] = 4; objects[2].count = 1; }
        objects[1].free_location = -1;
        CHECK(item_transfer_ex(4, 1, ITEM_INV_LOC_WEAPON));
        CHECK(objects[4].location == ITEM_INV_LOC_WEAPON);
    }
    transfer_test_reset();
    objects[4].type = OBJ_TYPE_AMMO; objects[4].quantity = -1;
    CHECK(!item_transfer_ex(4, 1, ITEM_INV_LOC_WEAPON));
    CHECK(transfer_error == ITEM_CANNOT_NO_ROOM && strcmp(transfer_trace, "G") == 0);
    for (int owned = 0; owned < 2; owned++) {
        transfer_test_reset();
        objects[5] = (TestObject){ .type = OBJ_TYPE_AMMO, .quantity = 3, .bulk = 1, .prototype = 21, .owner = 1, .weight = 8 };
        objects[4] = (TestObject){ .type = OBJ_TYPE_AMMO, .quantity = 1, .bulk = 1, .prototype = 21, .owner = owned ? 2 : 0, .weight = 8 };
        objects[1].items[0] = 5; objects[1].count = 1; objects[1].carry_weight = 100; owner_weights[1] = 100;
        if (owned) { objects[2].items[0] = 4; objects[2].count = 1; }
        CHECK(!item_transfer_ex(4, 1, ITEM_INV_LOC_WEAPON));
        CHECK(transfer_error == ITEM_CANNOT_TOO_HEAVY && objects[5].quantity == 3 && objects[4].quantity == 1);
        CHECK(strcmp(transfer_trace, owned ? "T" : "G") == 0);
        owner_weights[1] = 92;
        CHECK(item_transfer_ex(4, 1, ITEM_INV_LOC_WEAPON));
        CHECK(objects[5].quantity == 4 && objects[4].quantity == 0);
    }
    transfer_test_reset();
    objects[5] = (TestObject){ .type = OBJ_TYPE_GENERIC, .bulk = 190, .owner = 1 };
    objects[6] = (TestObject){ .type = OBJ_TYPE_WEAPON, .bulk = 6, .location = ITEM_INV_LOC_WEAPON, .owner = 1 };
    objects[1].items[0] = 5; objects[1].items[1] = 6; objects[1].count = 2;
    CHECK(!item_transfer_ex(4, 1, ITEM_INV_LOC_WEAPON));
    CHECK(transfer_error == ITEM_CANNOT_TOO_BULKY && objects[4].owner == 0 && strcmp(transfer_trace, "G") == 0);
    objects[5].bulk = 180;
    CHECK(item_transfer_ex(4, 1, ITEM_INV_LOC_WEAPON));
    CHECK(objects[4].location == 4 && objects[6].location == ITEM_INV_LOC_WEAPON);
    transfer_test_reset(); preferred_location_free = true;
    CHECK(item_transfer_ex(4, 1, 22)); CHECK(objects[4].location == 22);
    transfer_test_reset();
    CHECK(item_transfer_ex(4, 1, 22)); CHECK(objects[4].location == 4);
    transfer_test_reset(); preferred_location_free = true;
    objects[4].owner = 1; objects[1].items[0] = 4; objects[1].count = 1;
    CHECK(item_transfer_ex(4, 1, 22));
    CHECK(objects[4].owner == 1 && objects[4].location == 22 && objects[1].count == 1);
    CHECK(strcmp(transfer_trace, "TRICD") == 0);
    transfer_test_reset(); objects[3] = (TestObject){ .type = OBJ_TYPE_CONTAINER, .free_location = 4 };
    CHECK(item_transfer(4, 3));
    CHECK(objects[4].owner == 3 && objects[3].count == 1 && strcmp(transfer_trace, "GPIC") == 0);
    transfer_test_reset(); objects[1].free_location = -1;
    CHECK(!item_transfer(4, 1));
    CHECK(transfer_error == ITEM_CANNOT_NO_ROOM && objects[4].owner == 0 && strcmp(transfer_trace, "G") == 0);
}

int main(void)
{
    objects[1].type = OBJ_TYPE_PC;
    objects[2].type = OBJ_TYPE_NPC;
    objects[1].strength = objects[2].strength = 5;
    objects[3].type = OBJ_TYPE_CONTAINER;
    CHECK(item_bulk_capacity(1) == 190);
    CHECK(item_bulk_capacity(2) == 190);
    CHECK(item_bulk_capacity(3) == 0);
    {
        static const int capacities[RACE_COUNT] = { 190, 150, 190, 190, 150, 150, 190, 230, 190, 230, 190 };
        int race;
        for (race = 0; race < RACE_COUNT; race++) {
            static const int strengths[] = { 1, 5, 10, 20 };
            int index;
            objects[1].race = race;
            objects[2].race = race;
            CHECK(item_bulk_capacity(1) == capacities[race]);
            CHECK(item_bulk_capacity(2) == capacities[race]);
            for (index = 0; index < 4; index++) {
                objects[1].strength = strengths[index];
                objects[2].strength = strengths[index];
                CHECK(item_bulk_capacity(1) == capacities[race] + (strengths[index] - 5) * 5);
                CHECK(item_bulk_capacity(2) == capacities[race] + (strengths[index] - 5) * 5);
            }
            objects[1].strength = objects[2].strength = 5;
        }
        objects[1].race = -1;
        CHECK(item_bulk_capacity(1) == 190);
        objects[2].race = RACE_COUNT;
        CHECK(item_bulk_capacity(2) == 190);
        objects[1].race = objects[2].race = RACE_HUMAN;
    }
    objects[4] = (TestObject){ .type = OBJ_TYPE_WRITTEN, .bulk = 4, .prototype = 20 };
    objects[20].bulk = 4;
    CHECK(item_bulk(4) == 4);
    objects[4].bulk = 0;
    CHECK(item_bulk(4) == 4);
    objects[20].bulk = 0;
    CHECK(item_bulk(4) == 4);
    objects[5] = (TestObject){ .type = OBJ_TYPE_AMMO, .bulk = 1, .quantity = 1, .prototype = 21 };
    CHECK(item_bulk(5) == 1);
    objects[5].quantity = 4;
    CHECK(item_bulk(5) == 1);
    objects[5].quantity = 5;
    CHECK(item_bulk(5) == 2);
    CHECK(item_bulk_at_quantity(5, 2147483647) == 536870912);
    objects[6] = (TestObject){ .type = OBJ_TYPE_ARMOR, .bulk = 8, .location = ITEM_INV_LOC_ARMOR, .owner = 1 };
    objects[7] = (TestObject){ .type = OBJ_TYPE_GOLD, .quantity = 10000, .owner = 1 };
    objects[8] = (TestObject){ .type = OBJ_TYPE_GENERIC, .bulk = 186, .owner = 1 };
    objects[1].count = 3;
    objects[1].items[0] = 6;
    objects[1].items[1] = 7;
    objects[1].items[2] = 8;
    CHECK(item_total_bulk(1) == 186);
    CHECK(item_check_bulk_insert(4, 1, -1) == ITEM_CANNOT_OK);
    CHECK(item_check_bulk_insert(6, 1, -1) == ITEM_CANNOT_TOO_BULKY);
    CHECK(item_check_bulk_insert(6, 1, ITEM_INV_LOC_ARMOR) == ITEM_CANNOT_OK);
    objects[8].bulk = 191;
    CHECK(item_check_bulk_insert(4, 1, -1) == ITEM_CANNOT_TOO_BULKY);
    CHECK(item_check_bulk_insert(8, 1, 20) == ITEM_CANNOT_OK);
    objects[8].bulk = 189;
    objects[5].quantity = 1;
    objects[5].owner = 1;
    objects[1].items[3] = 5;
    objects[1].count = 4;
    objects[9] = (TestObject){ .type = OBJ_TYPE_AMMO, .bulk = 1, .quantity = 3, .prototype = 21 };
    CHECK(item_total_bulk(1) == 190);
    CHECK(item_check_bulk_insert(9, 1, -1) == ITEM_CANNOT_OK);
    objects[9].quantity = 4;
    CHECK(item_check_bulk_insert(9, 1, -1) == ITEM_CANNOT_TOO_BULKY);
    objects[10] = (TestObject){ .type = OBJ_TYPE_KEY_RING, .bulk = 1, .owner = 1 };
    objects[11] = (TestObject){ .type = OBJ_TYPE_KEY, .bulk = 1 };
    objects[1].items[4] = 10;
    objects[1].count = 5;
    CHECK(item_check_bulk_insert(11, 1, -1) == ITEM_CANNOT_OK);
    objects[1].count = 4;
    objects[2].items[0] = 9;
    objects[2].count = 1;
    objects[9].owner = 2;
    objects[9].quantity = 4;
    CHECK(!item_ammo_transfer(2, 1, 4, 0, 9));
    CHECK(objects[9].quantity == 4 && objects[5].quantity == 1);
    CHECK(item_ammo_transfer(2, 1, 3, 0, 9));
    CHECK(objects[9].quantity == 1 && objects[5].quantity == 4);
    CHECK(!item_ammo_transfer(2, 1, 1, 0, 9));
    CHECK(objects[9].quantity == 1 && objects[5].quantity == 4);
    CHECK(item_ammo_transfer(2, 2, 1, 0, 9));
    CHECK(objects[9].quantity == 1);
    CHECK(!item_ammo_transfer(2, 1, -1, 0, 9));
    objects[1].count = 3;
    objects[8].bulk = 190;
    CHECK(!item_ammo_transfer(2, 1, 1, 0, 9));
    CHECK(objects[9].quantity == 1);
    objects[8].bulk = 189;
    CHECK(item_ammo_transfer(2, 1, 1, 0, 9));
    CHECK(objects[9].quantity == 0 && objects[30].quantity == 1);
    objects[1].count = 3;
    objects[1].race = RACE_DWARF;
    objects[1].strength = 8;
    objects[8].bulk = 162;
    CHECK(item_check_bulk_insert(4, 1, -1) == ITEM_CANNOT_TOO_BULKY);
    objects[8].bulk = 161;
    CHECK(item_check_bulk_insert(4, 1, -1) == ITEM_CANNOT_OK);
    objects[1].race = RACE_HALF_OGRE;
    objects[8].bulk = 241;
    CHECK(item_check_bulk_insert(4, 1, -1) == ITEM_CANNOT_OK);
    objects[8].bulk = 242;
    CHECK(item_check_bulk_insert(4, 1, -1) == ITEM_CANNOT_TOO_BULKY);
    objects[1].race = RACE_HUMAN;
    objects[1].strength = 5;
    objects[1].count = 2;
    objects[1].items[0] = 6;
    objects[1].items[1] = 8;
    objects[8].bulk = 190;
    objects[4].bulk = 4;
    objects[4].owner = 0;
    int location = -99;
    TestObject snapshot[32];
    memcpy(snapshot, objects, sizeof(objects));
    CHECK(item_check_insert_at(4, 1, ITEM_INV_LOC_ARMOR, &location) == ITEM_CANNOT_TOO_BULKY);
    CHECK(location == -99 && memcmp(snapshot, objects, sizeof(objects)) == 0);
    objects[4].owner = 2;
    CHECK(item_check_insert_at(4, 1, ITEM_INV_LOC_ARMOR, NULL) == ITEM_CANNOT_TOO_BULKY);
    objects[4].owner = 0;
    CHECK(item_check_insert_at(4, 1, ITEM_INV_LOC_BOOTS, &location) == ITEM_CANNOT_OK);
    CHECK(location == ITEM_INV_LOC_BOOTS);
    objects[1].free_location = -1;
    CHECK(item_check_insert_at(4, 1, ITEM_INV_LOC_BOOTS, &location) == ITEM_CANNOT_OK);
    objects[1].free_location = 0;
    objects[8].bulk = 186;
    CHECK(item_check_insert_at(4, 1, ITEM_INV_LOC_ARMOR, &location) == ITEM_CANNOT_OK);
    CHECK(!IS_WEAR_INV_LOC(location));
    objects[8].bulk = 190;
    CHECK(item_check_insert_at(6, 1, ITEM_INV_LOC_ARMOR, NULL) == ITEM_CANNOT_TOO_BULKY);
    objects[1].count = 1;
    objects[1].items[0] = 5;
    objects[5].quantity = 3;
    objects[5].weight = 8;
    objects[5].owner = 1;
    objects[9].quantity = 1;
    objects[9].weight = 8;
    objects[9].owner = 2;
    objects[1].carry_weight = 100;
    owner_weights[1] = 100;
    CHECK(item_ammo_weight_change(5, 1, 3, 4) == 8);
    CHECK(item_check_insert_at(9, 1, -1, NULL) == ITEM_CANNOT_TOO_HEAVY);
    CHECK(!item_ammo_transfer(2, 1, 1, 0, 9));
    CHECK(objects[5].quantity == 3 && objects[9].quantity == 1);
    owner_weights[1] = 92;
    CHECK(item_check_insert(9, 1, NULL) == ITEM_CANNOT_OK);
    CHECK(item_ammo_transfer(2, 1, 1, 0, 9));
    CHECK(objects[5].quantity == 4 && objects[9].quantity == 0);
    objects[5].quantity = 7;
    objects[9].quantity = 1;
    owner_weights[1] = 100;
    CHECK(item_check_insert(9, 1, NULL) == ITEM_CANNOT_TOO_HEAVY);
    objects[5].quantity = 2;
    CHECK(item_check_insert(9, 1, NULL) == ITEM_CANNOT_OK);
    objects[5].quantity = 3;
    objects[5].magic_weight_adjustment = 4;
    owner_weights[1] = 92;
    CHECK(item_ammo_weight_change(5, 1, 3, 4) == 12);
    CHECK(item_check_insert(9, 1, NULL) == ITEM_CANNOT_TOO_HEAVY);
    objects[5].quantity = INT_MAX;
    CHECK(item_check_insert(9, 1, NULL) == ITEM_CANNOT_NO_ROOM);
    objects[1].items[0] = 8;
    objects[8].bulk = 190;
    objects[9].quantity = 1;
    CHECK(item_check_insert_at(9, 1, ITEM_INV_LOC_WEAPON, &location) == ITEM_CANNOT_OK);
    objects[1].items[1] = 5;
    objects[1].count = 2;
    objects[5].quantity = 4;
    objects[9].quantity = 1;
    CHECK(item_check_insert_at(9, 1, ITEM_INV_LOC_WEAPON, NULL) == ITEM_CANNOT_TOO_BULKY);
    memset(objects, 0, sizeof(objects));
    objects[1] = (TestObject){ .type = OBJ_TYPE_PC, .count = 720 };
    objects[2] = (TestObject){ .type = OBJ_TYPE_KEY, .bulk = 1, .weight = 2 };
    objects[3] = (TestObject){ .type = OBJ_TYPE_KEY_RING, .bulk = 3, .weight = 4 };
    for (int index = 0; index < 720; index++) objects[1].items[index] = 2;
    int total_weight;
    int64_t total_bulk;
    inventory_reads = 0;
    item_inventory_load_get(1, &total_weight, &total_bulk);
    CHECK(total_weight == 1440 && total_bulk == 720 && inventory_reads == 720);
    objects[1].items[719] = 3;
    inventory_reads = 0;
    item_inventory_load_get(1, &total_weight, &total_bulk);
    CHECK(total_weight == 1442 && total_bulk == 3 && inventory_reads == 720);
    objects[3].location = ITEM_INV_LOC_WEAPON;
    CHECK(item_total_bulk(1) == 0);
    objects[2].type = OBJ_TYPE_GENERIC;
    CHECK(item_total_bulk(1) == 719);
    objects[1].items[0] = OBJ_HANDLE_NULL;
    CHECK(item_total_bulk(1) == 718);
    objects[1].count = 1; objects[1].items[0] = 2;
    objects[2].weight = INT_MAX;
    objects[1].count = 2; objects[1].items[1] = 2;
    item_inventory_load_get(1, &total_weight, &total_bulk);
    CHECK(total_weight == INT_MAX && total_bulk == 2);
    item_inventory_load_get(0, &total_weight, &total_bulk);
    CHECK(total_weight == 0 && total_bulk == 0);
    transfer_test_cases();
    return failures != 0;
}
