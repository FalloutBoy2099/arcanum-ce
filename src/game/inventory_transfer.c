#include "game/item_private.h"

#include "game/mp_utils.h"
#include "game/multiplayer.h"
#include "game/object.h"
#include "game/script.h"
#include "game/tf.h"

static void item_transfer_error_report(int64_t source, int64_t destination, int reason)
{
    if (source != OBJ_HANDLE_NULL) {
        if (obj_field_int32_get(source, OBJ_F_TYPE) == OBJ_TYPE_PC) item_error_msg(source, reason);
        if (obj_field_int32_get(destination, OBJ_F_TYPE) == OBJ_TYPE_PC) item_error_msg(destination, reason);
    } else {
        switch (obj_field_int32_get(destination, OBJ_F_TYPE)) {
        case OBJ_TYPE_PC:
            item_error_msg(destination, reason);
            break;
        case OBJ_TYPE_NPC:
            tf_add(destination, TF_TYPE_WHITE, item_error_str(reason));
            break;
        }
    }
}

static int item_transfer_destination_get(int64_t item, int64_t destination, int requested, int fallback, int64_t equipped)
{
    if (requested == -1) return fallback;
    if (IS_WEAR_INV_LOC(requested)) return equipped == OBJ_HANDLE_NULL ? requested : fallback;
    int slots[INVENTORY_CONTAINER_CELLS];
    item_inventory_slots_get(destination, slots);
    return item_inventory_slots_has_room_for(item, destination, requested, slots) ? requested : fallback;
}

bool item_transfer(int64_t item_obj, int64_t critter_obj)
{
    return item_transfer_ex(item_obj, critter_obj, -1);
}

bool item_transfer_ex(int64_t item_obj, int64_t critter_obj, int inventory_location)
{
    int64_t source = OBJ_HANDLE_NULL;
    int64_t equipped = OBJ_HANDLE_NULL;
    int fallback = -1;
    if (tig_net_is_active() && !tig_net_is_host()) {
        Packet28 packet;
        packet.type = 28;
        mp_object_id_get_nullable(item_obj, &packet.item_oid);
        mp_object_id_get_nullable(critter_obj, &packet.critter_oid);
        packet.inventory_location = inventory_location;
        tig_net_send_app_all(&packet, sizeof(packet));
        return true;
    }
    if (item_parent(item_obj, &source)) {
        if (!object_script_execute(source, critter_obj, item_obj, SAP_TRANSFER, 0)) return false;
    } else {
        source = OBJ_HANDLE_NULL;
        if (!object_script_execute(critter_obj, item_obj, OBJ_HANDLE_NULL, SAP_GET, 0)) return false;
    }
    if (IS_WEAR_INV_LOC(inventory_location)) equipped = item_wield_get(critter_obj, inventory_location);
    int reason = item_check_insert_at(item_obj, critter_obj, inventory_location, &fallback);
    if (reason != ITEM_CANNOT_OK) {
        item_transfer_error_report(source, critter_obj, reason);
        return false;
    }
    if (source != OBJ_HANDLE_NULL) item_remove(item_obj);
    else object_pickup(item_obj, critter_obj);
    int destination = item_transfer_destination_get(item_obj, critter_obj, inventory_location, fallback, equipped);
    item_insert(item_obj, critter_obj, destination);
    item_cancel_decay(item_obj);
    if (source != OBJ_HANDLE_NULL) item_decay(item_obj, 172800000);
    return true;
}
