#include "game/companion_movement.h"

#include "game/combat.h"
#include "game/critter.h"
#include "game/location.h"
#include "game/obj.h"
#include "game/path.h"
#include "game/object_query.h"
#include "game/object.h"
#include "game/player.h"

static bool live_other_critter(int64_t obj, void* context)
{
    return obj != *(int64_t*)context && !critter_is_dead(obj);
}

int64_t companion_movement_blocker(int64_t mover, int64_t destination)
{
    if (mover == OBJ_HANDLE_NULL || obj_field_int64_get(mover, OBJ_F_LOCATION) == destination
        || (obj_field_int32_get(mover, OBJ_F_FLAGS) & OF_NO_BLOCK) != 0) return OBJ_HANDLE_NULL;
    int64_t pc = player_get_local_pc_obj();
    if (pc == OBJ_HANDLE_NULL || (mover != pc && critter_pc_leader_get(mover) != pc)) return OBJ_HANDLE_NULL;
    return object_query_first(destination, OBJ_TM_CRITTER,
        OF_OFF | OF_DESTROYED | OF_INVENTORY | OF_NO_BLOCK, live_other_critter, &mover);
}

bool companion_movement_repath_needed(int64_t planned_leader_tile, int64_t leader_tile, int remaining_steps)
{
    // A one-tile leader movement need not invalidate an otherwise usable path.
    // Always refresh at its end, so a stationary leader is not left behind.
    return planned_leader_tile != leader_tile
        && (remaining_steps <= 1 || location_dist(planned_leader_tile, leader_tile) > 1);
}

bool companion_movement_follow_destination(int64_t companion, int64_t leader, int64_t* destination)
{
    if (companion == OBJ_HANDLE_NULL || leader == OBJ_HANDLE_NULL || destination == NULL
        || obj_field_int32_get(companion, OBJ_F_TYPE) != OBJ_TYPE_NPC
        || combat_critter_is_combat_mode_active(companion) || combat_critter_is_combat_mode_active(leader)) return false;
    int count = obj_arrayfield_length_get(leader, OBJ_F_CRITTER_FOLLOWER_IDX);
    for (int index = 0; index < count; index++) {
        if (obj_arrayfield_handle_get(leader, OBJ_F_CRITTER_FOLLOWER_IDX, index) != companion) continue;
        // Stable world-space slots: turning the leader must not swap everyone's positions.
        static const int directions[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };
        int range = (obj_field_int32_get(companion, OBJ_F_NPC_FLAGS) & ONF_AI_SPREAD_OUT) != 0 ? 5 : 2;
        // Additional rings avoid duplicate destinations for unusually large parties.
        range -= (index / 8) % range;
        return location_in_range(obj_field_int64_get(leader, OBJ_F_LOCATION), directions[index % 8], range, destination);
    }
    return false;
}

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
            path.flags = PATH_FLAG_PORTALS_BLOCK | PATH_FLAG_WINDOWS_BLOCK | PATH_FLAG_AVOID_FIRE;
            if (path_make(&path) == distance) {
                *destination = to;
                return true;
            }
        }
    }
    return false;
}

bool companion_movement_escape_route(int64_t companion, int64_t requester,
    const int64_t* route, int count, int64_t* destination)
{
    if (route == NULL || count < 2 || destination == NULL
        || companion == OBJ_HANDLE_NULL || requester == OBJ_HANDLE_NULL
        || obj_field_int32_get(companion, OBJ_F_TYPE) != OBJ_TYPE_NPC
        || critter_pc_leader_get(companion) != requester
        || combat_critter_is_combat_mode_active(companion) || combat_critter_is_combat_mode_active(requester)
        || critter_is_dead(companion) || critter_is_unconscious(companion)) return false;
    int64_t from = obj_field_int64_get(companion, OBJ_F_LOCATION);
    int direction = location_rot(route[0], route[1]);
    // Prefer a short lateral step, rather than running ahead along the player's
    // corridor. In a doorway a diagonal/backward retreat may be necessary.
    const int offsets[8] = { 2, -2, 3, -3, 1, -1, 4, 0 };
    for (int distance = 1; distance <= 3; distance++) {
        for (int i = 0; i < 8; i++) {
            int64_t to;
            if (!location_in_range(from, (direction + offsets[i] + 8) % 8, distance, &to)) continue;
            int8_t rotations[3];
            PathCreateInfo path = { 0 };
            path.obj = companion;
            path.from = from;
            path.to = to;
            path.max_rotations = distance;
            path.rotations = rotations;
            path.flags = PATH_FLAG_PORTALS_BLOCK | PATH_FLAG_WINDOWS_BLOCK | PATH_FLAG_AVOID_FIRE;
            int steps = path_make(&path);
            if (steps <= 0 || steps > distance) continue;
            int64_t cell = from;
            bool clear = true;
            for (int step = 0; step < steps && clear; step++) {
                if (!location_in_dir(cell, rotations[step], &cell)) {
                    clear = false;
                    break;
                }
                for (int r = 0; r < count; r++) {
                    if (cell == route[r]) { clear = false; break; }
                }
            }
            if (clear && cell == to) {
                *destination = to;
                return true;
            }
        }
    }
    return false;
}
