#include "game/item.h"

#include <limits.h>
#include "game/stat.h"

static int item_unit_bulk(int64_t item_obj)
{
    int bulk;
    int width;
    int height;
    int64_t prototype;

    if (obj_field_int32_get(item_obj, OBJ_F_TYPE) == OBJ_TYPE_GOLD) {
        return 0;
    }
    bulk = obj_field_int32_get(item_obj, OBJ_F_ITEM_PAD_I_1);
    if (bulk <= 0) {
        prototype = obj_field_handle_get(item_obj, OBJ_F_PROTOTYPE_HANDLE);
        if (prototype != OBJ_HANDLE_NULL && prototype != item_obj) {
            bulk = obj_field_int32_get(prototype, OBJ_F_ITEM_PAD_I_1);
        }
    }
    if (bulk <= 0) {
        item_inv_icon_size(item_obj, &width, &height);
        bulk = width * height;
    }
    return bulk > 0 ? bulk : 1;
}

void item_inventory_load_get(int64_t obj, int* weight, int64_t* bulk)
{
    int64_t total_weight = 0;
    int64_t total_bulk = 0;
    int64_t key_bulk = 0;
    bool has_key_ring = false;
    if (weight != NULL) *weight = 0;
    if (bulk != NULL) *bulk = 0;
    if (obj == OBJ_HANDLE_NULL || !obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))) return;
    int count = obj_field_int32_get(obj, OBJ_F_CRITTER_INVENTORY_NUM);
    for (int index = 0; index < count; index++) {
        int64_t item = obj_arrayfield_handle_get(obj, OBJ_F_CRITTER_INVENTORY_LIST_IDX, index);
        if (item == OBJ_HANDLE_NULL) continue;
        int type = obj_field_int32_get(item, OBJ_F_TYPE);
        if (weight != NULL) total_weight += item_weight(item, obj);
        if (bulk == NULL) continue;
        if (type == OBJ_TYPE_KEY_RING) has_key_ring = true;
        if (IS_WEAR_INV_LOC(item_inventory_location_get(item))) continue;
        if (type == OBJ_TYPE_KEY) key_bulk += item_bulk(item);
        else total_bulk += item_bulk(item);
    }
    if (weight != NULL) *weight = total_weight > INT_MAX ? INT_MAX : total_weight < INT_MIN ? INT_MIN : (int)total_weight;
    if (bulk != NULL) *bulk = total_bulk + (has_key_ring ? 0 : key_bulk);
}

int64_t item_total_bulk(int64_t obj)
{
    int64_t bulk;
    item_inventory_load_get(obj, NULL, &bulk);
    return bulk;
}

int64_t item_bulk_at_quantity(int64_t item_obj, int64_t quantity)
{
    int bulk = item_unit_bulk(item_obj);
    if (obj_field_int32_get(item_obj, OBJ_F_TYPE) == OBJ_TYPE_AMMO) {
        return quantity > 0 ? ((int64_t)bulk * quantity + 3) / 4 : 0;
    }
    return bulk;
}

int64_t item_bulk(int64_t item_obj)
{
    return item_bulk_at_quantity(item_obj,
        obj_field_int32_get(item_obj, OBJ_F_TYPE) == OBJ_TYPE_AMMO
            ? obj_field_int32_get(item_obj, OBJ_F_AMMO_QUANTITY) : 1);
}

int item_check_bulk_insert(int64_t item_obj, int64_t owner, int location)
{
    int64_t previous_owner;
    int64_t delta;
    int64_t existing;

    if (item_bulk_capacity(owner) == 0 || IS_WEAR_INV_LOC(location)
        || (obj_field_int32_get(item_obj, OBJ_F_TYPE) == OBJ_TYPE_KEY && item_find_key_ring(owner) != OBJ_HANDLE_NULL)) {
        return ITEM_CANNOT_OK;
    }
    delta = item_bulk(item_obj);
    if (item_parent(item_obj, &previous_owner) && previous_owner == owner) {
        if (!IS_WEAR_INV_LOC(item_inventory_location_get(item_obj))) {
            delta = 0;
        }
    } else if (obj_field_int32_get(item_obj, OBJ_F_TYPE) == OBJ_TYPE_AMMO) {
        existing = item_find_first_matching_prototype(owner, item_obj);
        if (existing != OBJ_HANDLE_NULL && existing != item_obj) {
            int64_t quantity = (int64_t)obj_field_int32_get(existing, OBJ_F_AMMO_QUANTITY)
                + obj_field_int32_get(item_obj, OBJ_F_AMMO_QUANTITY);
            delta = item_bulk_at_quantity(existing, quantity) - item_bulk(existing);
        }
    }
    return delta > 0 && item_total_bulk(owner) + delta > item_bulk_capacity(owner)
        ? ITEM_CANNOT_TOO_BULKY : ITEM_CANNOT_OK;
}

int item_bulk_capacity(int64_t obj)
{
    int race;
    int capacity = INVENTORY_BULK_CAPACITY;

    if (obj == OBJ_HANDLE_NULL || !obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))) {
        return 0;
    }
    race = stat_level_get(obj, STAT_RACE);
    if (race >= 0 && race < RACE_COUNT) {
        switch (item_armor_size(race)) {
        case OARF_SIZE_SMALL:
            capacity -= 40;
            break;
        case OARF_SIZE_LARGE:
            capacity += 40;
            break;
        }
    }
    return capacity + (stat_level_get(obj, STAT_STRENGTH) - 5) * 5;
}

