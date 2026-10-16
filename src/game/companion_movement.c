#include "game/companion_movement.h"

#include "game/combat.h"
#include "game/critter.h"
#include "game/location.h"
#include "game/obj.h"
#include "game/path.h"

bool companion_movement_escape(int64_t companion, int64_t requester, int64_t* destination)
{
    if (companion == OBJ_HANDLE_NULL || requester == OBJ_HANDLE_NULL || destination == NULL
        || obj_field_int32_get(companion, OBJ_F_TYPE) != OBJ_TYPE_NPC
        || combat_critter_is_combat_mode_active(companion) || combat_critter_is_combat_mode_active(requester)
        || critter_is_dead(companion) || critter_is_unconscious(companion)) return false;
    int64_t leader = obj_field_int32_get(requester, OBJ_F_TYPE) == OBJ_TYPE_PC ? requester : critter_pc_leader_get(requester);
    if (leader == OBJ_HANDLE_NULL || critter_pc_leader_get(companion) != leader) return false;
    int64_t from = obj_field_int64_get(companion, OBJ_F_LOCATION);
    int64_t requester_loc = obj_field_int64_get(requester, OBJ_F_LOCATION);
    if (from == requester_loc) return false; // Normal overlap handling chooses a free neighbour.
    int away = location_rot(requester_loc, from);
    const int offsets[8] = { 0, 1, -1, 2, -2, 3, -3, 4 };
    for (int distance = 3; distance >= 1; distance--) {
        for (int i = 0; i < 8; i++) {
            int64_t to;
            int8_t rotations[3];
            if (!location_in_range(from, (away + offsets[i] + 8) % 8, distance, &to)
                || location_dist(to, requester_loc) <= location_dist(from, requester_loc)) continue;
            PathCreateInfo path = { 0 };
            path.obj = companion;
            path.from = from;
            path.to = to;
            path.max_rotations = distance;
            path.rotations = rotations;
            path.flags = PATH_FLAG_0x0002 | PATH_FLAG_0x0004 | PATH_FLAG_0x0400;
            if (path_make(&path) == distance) {
                *destination = to;
                return true;
            }
        }
    }
    return false;
}
