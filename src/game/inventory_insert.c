#include "game/item.h"

#include <limits.h>
#include "game/background.h"
#include "game/stat.h"

int64_t item_ammo_weight_change(int64_t item, int64_t owner, int64_t before, int64_t after)
{
    int64_t unit = obj_field_int32_get(item, OBJ_F_ITEM_WEIGHT);
    unit += item_adjust_magic(item, owner, obj_field_int32_get(item, OBJ_F_ITEM_MAGIC_WEIGHT_ADJ));
    return unit * (after / 4 - before / 4);
}

int item_check_insert(int64_t item_obj, int64_t parent_obj, int* inventory_location_ptr)
{
    return item_check_insert_at(item_obj, parent_obj, -1, inventory_location_ptr);
}

int item_check_insert_at(int64_t item_obj, int64_t parent_obj, int location, int* inventory_location_ptr)
{
    int inventory_location = -1;
    int64_t previous_owner = OBJ_HANDLE_NULL;
    int64_t existing = OBJ_HANDLE_NULL;
    int quantity_field;
    bool merges = false;
    bool critter = obj_type_is_critter(obj_field_int32_get(parent_obj, OBJ_F_TYPE));
    if (item_stack_counter_field(item_obj, &quantity_field) != -1) {
        int incoming = obj_field_int32_get(item_obj, quantity_field);
        if (incoming < 0) return ITEM_CANNOT_NO_ROOM;
        existing = item_find_first_matching_prototype(parent_obj, item_obj);
        merges = existing != OBJ_HANDLE_NULL && existing != item_obj;
        if (merges) {
            int current = obj_field_int32_get(existing, quantity_field);
            if (current < 0 || (int64_t)current + incoming > INT_MAX) return ITEM_CANNOT_NO_ROOM;
        }
    }
    if (critter) {
        bool equips = IS_WEAR_INV_LOC(location) && item_wield_get(parent_obj, location) == OBJ_HANDLE_NULL && !merges;
        int reason = item_check_bulk_insert(item_obj, parent_obj, equips ? location : -1);
        if (reason != ITEM_CANNOT_OK) return reason;
        if (equips) inventory_location = location;
        if (obj_field_int32_get(item_obj, OBJ_F_TYPE) == OBJ_TYPE_KEY && item_find_key_ring(parent_obj) != OBJ_HANDLE_NULL) {
            inventory_location = 0;
        } else if (!item_parent(item_obj, &previous_owner) || previous_owner != parent_obj) {
            int64_t delta;
            if (obj_field_int32_get(item_obj, OBJ_F_TYPE) == OBJ_TYPE_AMMO) {
                int64_t before = merges ? obj_field_int32_get(existing, OBJ_F_AMMO_QUANTITY) : 0;
                int64_t after = before + obj_field_int32_get(item_obj, OBJ_F_AMMO_QUANTITY);
                delta = item_ammo_weight_change(merges ? existing : item_obj, parent_obj, before, after);
            } else {
                delta = item_weight(item_obj, parent_obj);
            }
            if (delta > 0 && (int64_t)item_total_weight(parent_obj) + delta > stat_level_get(parent_obj, STAT_CARRY_WEIGHT)) {
                return ITEM_CANNOT_TOO_HEAVY;
            }
        }
        if (obj_field_int32_get(item_obj, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY) < 0
            && background_get(parent_obj) == BACKGROUND_TECHNOPHOBIA) return ITEM_CANNOT_PICKUP_TECH_ITEMS;
    }
    if (merges) inventory_location = 0;
    if (inventory_location == -1) {
        inventory_location = item_find_free_inv_loc_for_insertion(item_obj, parent_obj);
        if (inventory_location == -1) return ITEM_CANNOT_NO_ROOM;
    }
    if (inventory_location_ptr != NULL) *inventory_location_ptr = inventory_location;
    return ITEM_CANNOT_OK;
}
