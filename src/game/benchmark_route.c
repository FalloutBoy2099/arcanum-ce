#include "game/benchmark.h"

#include "game/location.h"
#include "game/obj.h"
#include "game/path.h"

bool benchmark_route_find(int64_t pc, int64_t route[4])
{
    if (pc == OBJ_HANDLE_NULL || route == NULL) return false;
    int64_t start = obj_field_int64_get(pc, OBJ_F_LOCATION);
    for (int orientation = 0; orientation < 8; orientation += 2) {
        int64_t from = start;
        bool valid = true;
        for (int leg = 0; leg < 4; leg++) {
            int8_t rotations[32];
            if (!location_in_range(from, (orientation + leg * 2) % 8, 4, &route[leg])) {
                valid = false;
                break;
            }
            PathCreateInfo path = { 0 };
            path.obj = pc;
            path.from = from;
            path.to = route[leg];
            path.max_rotations = SDL_arraysize(rotations);
            path.rotations = rotations;
            path.flags = PATH_FLAG_0x0002 | PATH_FLAG_0x0004 | PATH_FLAG_0x0400;
            // During preflight the player still occupies the start tile.
            // Allow that known destination on the final leg, while checking
            // all intermediate obstacles normally. Live movement is unchanged.
            if (leg == 3 && path.to == start) path.flags |= PATH_FLAG_0x0001;
            if (path_make(&path) == 0) { valid = false; break; }
            from = route[leg];
        }
        if (valid && route[3] == start) return true;
    }
    return false;
}
