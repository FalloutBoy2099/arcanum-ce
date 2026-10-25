#include "game/item.h"
#include "game/object.h"
#include "game/script.h"
#include "game/stat.h"
#include <limits.h>

bool item_ammo_transfer(int64_t from_obj, int64_t to_obj, int qty, int ammo_type, int64_t ammo_obj)
{
    int remaining_qty = 0;
    int destination_qty = 0;
    int64_t destination = OBJ_HANDLE_NULL;
    int64_t created = OBJ_HANDLE_NULL;

    if (qty == 0) {
        return true;
    }
    if (qty < 0 || (tig_net_is_active() && !tig_net_is_host())) {
        return false;
    }
    if (from_obj != OBJ_HANDLE_NULL) {
        if (ammo_obj == OBJ_HANDLE_NULL) {
            ammo_obj = item_ammo_obj(from_obj, ammo_type);
        }
        if (ammo_obj == OBJ_HANDLE_NULL) {
            return false;
        }
        remaining_qty = obj_field_int32_get(ammo_obj, OBJ_F_AMMO_QUANTITY);
        if (remaining_qty < qty) {
            return false;
        }
    }
    if (from_obj != OBJ_HANDLE_NULL && from_obj == to_obj) {
        return true;
    }
    if (to_obj != OBJ_HANDLE_NULL) {
        destination = item_ammo_obj(to_obj, ammo_type);
        if (destination != OBJ_HANDLE_NULL) {
            int64_t bulk_delta;
            int64_t weight_delta;
            destination_qty = obj_field_int32_get(destination, OBJ_F_AMMO_QUANTITY);
            if ((int64_t)destination_qty + qty > INT_MAX) {
                return false;
            }
            bulk_delta = item_bulk_at_quantity(destination, (int64_t)destination_qty + qty) - item_bulk(destination);
            if (item_bulk_capacity(to_obj) > 0 && bulk_delta > 0
                && item_total_bulk(to_obj) + bulk_delta > item_bulk_capacity(to_obj)) {
                item_error_msg(to_obj, ITEM_CANNOT_TOO_BULKY);
                return false;
            }
            weight_delta = item_ammo_weight_change(destination, to_obj, destination_qty, (int64_t)destination_qty + qty);
            if (obj_type_is_critter(obj_field_int32_get(to_obj, OBJ_F_TYPE)) && weight_delta > 0
                && item_total_weight(to_obj) + weight_delta > stat_level_get(to_obj, STAT_CARRY_WEIGHT)) {
                item_error_msg(to_obj, ITEM_CANNOT_TOO_HEAVY);
                return false;
            }
        } else {
            created = item_ammo_create(qty, ammo_type, obj_field_int64_get(to_obj, OBJ_F_LOCATION));
            if (created == OBJ_HANDLE_NULL) {
                return false;
            }
            if (!item_transfer(created, to_obj)) {
                object_destroy(created);
                return false;
            }
        }
    }
    if (destination != OBJ_HANDLE_NULL) {
        obj_field_int32_set(destination, OBJ_F_AMMO_QUANTITY, destination_qty + qty);
        object_script_execute(to_obj, destination, OBJ_HANDLE_NULL, SAP_INSERT_ITEM, 0);
    }
    if (from_obj != OBJ_HANDLE_NULL) {
        if (remaining_qty == qty) {
            object_destroy(ammo_obj);
        } else {
            obj_field_int32_set(ammo_obj, OBJ_F_AMMO_QUANTITY, remaining_qty - qty);
        }
    }
    item_inventory_change_notify(from_obj);
    item_inventory_change_notify(to_obj);
    return true;
}

