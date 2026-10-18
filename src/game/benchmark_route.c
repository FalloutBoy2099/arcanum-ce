#include "game/benchmark.h"

#include "game/location.h"
#include "game/obj.h"
#include "game/path.h"

static bool route_valid(int64_t pc, int64_t start, const int64_t route[4])
{
    int64_t from = start;
    for (int leg = 0; leg < 4; leg++) {
        int8_t rotations[32];
        PathCreateInfo path = { 0 };
        path.obj = pc;
        path.from = from;
        path.to = route[leg];
        path.max_rotations = SDL_arraysize(rotations);
        path.rotations = rotations;
        path.flags = PATH_FLAG_PORTALS_BLOCK | PATH_FLAG_WINDOWS_BLOCK | PATH_FLAG_AVOID_FIRE;
        // During preflight the player still occupies the start tile.
        // Allow that known destination on the final leg, while checking
        // all intermediate obstacles normally. Live movement is unchanged.
        if (leg == 3 && path.to == start) path.flags |= PATH_FLAG_STOP_BEFORE_DESTINATION;
        if (path_make(&path) == 0) return false;
        from = route[leg];
    }
    return route[3] == start;
}

bool benchmark_route_find(int64_t pc, int64_t route[4])
{
    if (pc == OBJ_HANDLE_NULL || route == NULL) return false;
    int64_t start = obj_field_int64_get(pc, OBJ_F_LOCATION);
    const int sizes[] = { 4, 2 };
    for (int size = 0; size < SDL_arraysize(sizes); size++) {
        for (int orientation = 0; orientation < 8; orientation += 2) {
            int64_t from = start;
            bool valid = true;
            for (int leg = 0; leg < 4; leg++) {
                if (!location_in_range(from, (orientation + leg * 2) % 8, sizes[size], &route[leg])) {
                    valid = false;
                    break;
                }
                from = route[leg];
            }
            if (valid && route_valid(pc, start, route)) return true;
        }
    }
    // A corridor may have no square at all. Four two-tile legs out and back
    // still return to the exact starting position and keep every leg nonempty.
    for (int direction = 0; direction < 8; direction++) {
        if (!location_in_range(start, direction, 2, &route[0])
            || !location_in_range(start, direction, 4, &route[1])) continue;
        route[2] = route[0];
        route[3] = start;
        if (route_valid(pc, start, route)) return true;
    }
    return false;
}
