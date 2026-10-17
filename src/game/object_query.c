#include "game/object_query.h"

#include "game/object.h"
#include "game/obj_find.h"
#include "game/sector.h"
#include "game/tile.h"
#include <tig/performance.h>

// Object type masks follow the original 18-entry lookup table exactly.
static_assert(OBJ_TYPE_WALL == 0 && OBJ_TYPE_TRAP == 17, "object query mask ordering");

static bool matches(int64_t obj, unsigned int types, unsigned int excluded_flags,
    ObjectQueryPredicate predicate, void* context)
{
    if ((obj_field_int32_get(obj, OBJ_F_FLAGS) & excluded_flags) != 0) return false;
    int type = obj_field_int32_get(obj, OBJ_F_TYPE);
    return type >= 0 && type <= OBJ_TYPE_TRAP && (types & (1U << type)) != 0
        && (predicate == NULL || predicate(obj, context));
}

int64_t object_query_first(int64_t loc, unsigned int types, unsigned int excluded_flags,
    ObjectQueryPredicate predicate, void* context)
{
    tig_performance_count(TIG_PERF_OBJECT_QUERIES, 1);
    int64_t sector_id = sector_id_from_loc(loc);
    int64_t limit_x, limit_y;
    sector_limits_get(&limit_x, &limit_y);
    if (SECTOR_X(sector_id) >= limit_x || SECTOR_Y(sector_id) >= limit_y) return OBJ_HANDLE_NULL;

    if ((types & (OBJ_TM_TRAP | OBJ_TM_SCENERY | OBJ_TM_PORTAL | OBJ_TM_WALL)) != 0
        || sector_loaded(sector_id)) {
        Sector* sector;
        if (!sector_lock(sector_id, &sector)) return OBJ_HANDLE_NULL;
        int64_t result = OBJ_HANDLE_NULL;
        for (ObjectNode* node = sector->objects.heads[tile_id_from_loc(loc)]; node != NULL; node = node->next) {
            tig_performance_count(TIG_PERF_OBJECT_QUERY_VISITS, 1);
            if (matches(node->obj, types, excluded_flags, predicate, context)) {
                result = node->obj;
                break;
            }
        }
        sector_unlock(sector_id);
        return result;
    }

    int64_t obj;
    FindNode* iter;
    if (obj_find_walk_first(sector_id, &obj, &iter)) {
        do {
            tig_performance_count(TIG_PERF_OBJECT_QUERY_VISITS, 1);
            if (!sub_43D940(obj)
                && (obj_field_int32_get(obj, OBJ_F_FLAGS) & OF_INVENTORY) == 0
                && obj_field_int64_get(obj, OBJ_F_LOCATION) == loc
                && matches(obj, types, excluded_flags, predicate, context)) return obj;
        } while (obj_find_walk_next(&obj, &iter));
    }
    return OBJ_HANDLE_NULL;
}
