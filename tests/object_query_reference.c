// Frozen pre-optimization tile query, used as a differential oracle.
#include "game/object.h"
#include "game/obj_find.h"
#include "game/sector.h"
#include "game/tile.h"
extern unsigned int reference_excluded_flags;
extern int reference_lists;

void reference_object_list_location(int64_t loc, unsigned int flags, ObjectList* objects)
{
    bool types[18];
    int64_t sector_id;
    int64_t limit_x;
    int64_t limit_y;
    Sector* sector;
    ObjectNode* new_node;
    ObjectNode** parent_ptr;

    memset(types, 0, sizeof(types));
    if ((flags & OBJ_TM_WALL) != 0) types[OBJ_TYPE_WALL] = true;
    if ((flags & OBJ_TM_PORTAL) != 0) types[OBJ_TYPE_PORTAL] = true;
    if ((flags & OBJ_TM_CONTAINER) != 0) types[OBJ_TYPE_CONTAINER] = true;
    if ((flags & OBJ_TM_SCENERY) != 0) types[OBJ_TYPE_SCENERY] = true;
    if ((flags & OBJ_TM_PROJECTILE) != 0) types[OBJ_TYPE_PROJECTILE] = true;
    if ((flags & OBJ_TM_WEAPON) != 0) types[OBJ_TYPE_WEAPON] = true;
    if ((flags & OBJ_TM_AMMO) != 0) types[OBJ_TYPE_AMMO] = true;
    if ((flags & OBJ_TM_ARMOR) != 0) types[OBJ_TYPE_ARMOR] = true;
    if ((flags & OBJ_TM_GOLD) != 0) types[OBJ_TYPE_GOLD] = true;
    if ((flags & OBJ_TM_FOOD) != 0) types[OBJ_TYPE_FOOD] = true;
    if ((flags & OBJ_TM_SCROLL) != 0) types[OBJ_TYPE_SCROLL] = true;
    if ((flags & OBJ_TM_KEY) != 0) types[OBJ_TYPE_KEY] = true;
    if ((flags & OBJ_TM_KEY_RING) != 0) types[OBJ_TYPE_KEY_RING] = true;
    if ((flags & OBJ_TM_WRITTEN) != 0) types[OBJ_TYPE_WRITTEN] = true;
    if ((flags & OBJ_TM_GENERIC) != 0) types[OBJ_TYPE_GENERIC] = true;
    if ((flags & OBJ_TM_PC) != 0) types[OBJ_TYPE_PC] = true;
    if ((flags & OBJ_TM_NPC) != 0) types[OBJ_TYPE_NPC] = true;
    if ((flags & OBJ_TM_TRAP) != 0) types[OBJ_TYPE_TRAP] = true;

    objects->num_sectors = 0;
    objects->head = NULL;
    parent_ptr = &(objects->head);

    sector_id = sector_id_from_loc(loc);
    sector_limits_get(&limit_x, &limit_y);

    if (SECTOR_X(sector_id) < 0
        || SECTOR_X(sector_id) >= limit_x
        || SECTOR_Y(sector_id) < 0
        || SECTOR_Y(sector_id) >= limit_y) {
        tig_debug_println("Warning: trying to build a list of objects from\n  a sector that is out of range for this map.");
        return;
    }

    if ((flags & (OBJ_TM_TRAP | OBJ_TM_SCENERY | OBJ_TM_PORTAL | OBJ_TM_WALL)) != 0
        || sector_loaded(sector_id)) {
        if (sector_lock(sector_id, &sector)) {
            ObjectNode* node;

            objects->sectors[objects->num_sectors++] = sector_id;

            node = sector->objects.heads[tile_id_from_loc(loc)];
            while (node != NULL) {
                if ((reference_excluded_flags & obj_field_int32_get(node->obj, OBJ_F_FLAGS)) == 0
                    && types[obj_field_int32_get(node->obj, OBJ_F_TYPE)]) {
                    new_node = object_node_create();
                    new_node->obj = node->obj;
                    new_node->next = NULL;

                    *parent_ptr = new_node;
                    parent_ptr = &((*parent_ptr)->next);
                }
                node = node->next;
            }
        }
    } else {
        int64_t obj;
        FindNode* iter;

        if (obj_find_walk_first(sector_id, &obj, &iter)) {
            do {
                if (!sub_43D940(obj)
                    && (obj_field_int32_get(obj, OBJ_F_FLAGS) & OF_INVENTORY) == 0
                    && obj_field_int64_get(obj, OBJ_F_LOCATION) == loc
                    && (reference_excluded_flags & obj_field_int32_get(obj, OBJ_F_FLAGS)) == 0
                    && types[obj_field_int32_get(obj, OBJ_F_TYPE)]) {
                    new_node = object_node_create();
                    new_node->obj = obj;
                    new_node->next = NULL;

                    *parent_ptr = new_node;
                    parent_ptr = &((*parent_ptr)->next);
                }
            } while (obj_find_walk_next(&obj, &iter));
        }
    }

    reference_lists++;
    return;
}

