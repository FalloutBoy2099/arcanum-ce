// Historical linear-scan A* oracle, copied before the heap change.
// Intentionally independent of path_open_set; preserve it for differential tests.
#include "game/path.h"
#include "game/critter.h"
#include "game/light.h"
#include "game/location.h"
#include "game/object.h"
#include "game/tile.h"
#include "game/trap.h"
#include <tig/performance.h>
static int path_cost_tbl[4096], path_backtrack_tbl[4096];
static tig_timestamp_t dword_5DE5F8;
static int dword_5DE5FC;
static tig_duration_t dword_5DE600;
static int path_limit=10, path_time_limit=250;
static int reference_path_dist(int src, int dst, int width);
static int reference_rotation(int a1, int a2, int a3);
int reference_astar(PathCreateInfo* path_create_info)
{
    tig_timestamp_t timestamp;
    bool v1 = false;
    unsigned int flags;
    int64_t from_x;
    int64_t from_y;
    int64_t to_x;
    int64_t to_y;
    int64_t origin_x;
    int64_t origin_y;
    int64_t dx;
    int64_t dy;
    int start_index;
    int target_index;
    int current_index;
    int best_estimated_cost;

    if (obj_field_int32_get(path_create_info->obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
        if (dword_5DE5F8 == 0
            || tig_timer_elapsed(dword_5DE5F8) >= 1000) {
            tig_timer_now(&dword_5DE5F8);
            dword_5DE5FC = 0;
            dword_5DE600 = 0;
        }

        if (dword_5DE5FC > path_limit) {
            return 0;
        }

        if (dword_5DE600 > path_time_limit) {
            return 0;
        }

        dword_5DE5FC++;

        tig_timer_now(&timestamp);
    } else {
        timestamp = 0;
    }

    from_x = LOCATION_GET_X(path_create_info->from);
    from_y = LOCATION_GET_Y(path_create_info->from);
    to_x = LOCATION_GET_X(path_create_info->to);
    to_y = LOCATION_GET_Y(path_create_info->to);

    if ((path_create_info->flags & PATH_FLAG_0x0008) != 0) {
        v1 = true;
    }

    flags = path_translate_flags(path_create_info->flags);

    if (from_x > to_x) {
        dx = from_x - to_x;
        origin_x = to_x;
    } else {
        dx = to_x - from_x;
        origin_x = from_x;
    }

    if (from_y > to_y) {
        dy = from_y - to_y;
        origin_y = to_y;
    } else {
        dy = to_y - from_y;
        origin_y = from_y;
    }

    // Check if the locations are too far apart.
    if (dx > 32 || dy > 32) {
        return 0;
    }

    origin_x -= (64 - dx) / 2;
    origin_y -= (64 - dy) / 2;

    start_index = (int)(from_x + (from_y - origin_y) * 64 - origin_x);
    target_index = (int)(to_x + (to_y - origin_y) * 64 - origin_x);

    // Initialize the cost array to zero (unprocessed state).
    memset(path_cost_tbl, 0, sizeof(path_cost_tbl));
    path_cost_tbl[start_index] = 1;
    path_backtrack_tbl[start_index] = -1;

    while (true) {
        current_index = -1;

        // Grab open node with minimal cost.
        tig_performance_count(TIG_PERF_PATH_EXPANSIONS, 1);
        tig_performance_count(TIG_PERF_PATH_SCANNED_CELLS, 4096);
        for (int i = 0; i < 4096; i++) {
            // Check if node is open, i.e. it has a positive cost (negative cost
            // are closed nodes, zero cost are unprocessed nodes).
            if (path_cost_tbl[i] > 0) {
                int estimated_cost = path_cost_tbl[i] + reference_path_dist(i, target_index, 64);
                if (estimated_cost / 10 <= path_create_info->max_rotations) {
                    // If no candidate node selected yet, or this candidate has
                    // a lower cost, update current candidate.
                    if (current_index == -1 || estimated_cost < best_estimated_cost) {
                        best_estimated_cost = estimated_cost;
                        current_index = i;
                    }
                } else {
                    // Mark node as unreachable.
                    path_cost_tbl[i] = -32768;
                }
            }
        }

        // If no node has been found, path is not reachable.
        if (current_index == -1) {
            if (timestamp != 0) {
                dword_5DE600 += tig_timer_elapsed(timestamp);
            }

            return 0;
        }

        // Check if we have reached the target.
        if (current_index == target_index) {
            break;
        }

        for (int rot = 0; rot < 8; rot++) {
            int64_t loc = location_make(origin_x + current_index % 64, origin_y + current_index / 64);
            int64_t adjacent_loc;
            if (!location_in_dir(loc, rot, &adjacent_loc)) {
                continue;
            }

            dx = LOCATION_GET_X(adjacent_loc) - origin_x;
            dy = LOCATION_GET_Y(adjacent_loc) - origin_y;
            if (dx < 0 || dx >= 64 || dy < 0 || dy >= 64) {
                continue;
            }

            int neighbor_index = (int)dx + (int)dy * 64;

            // Skip if the neighbor has already been marked as unreachable.
            if (path_cost_tbl[neighbor_index] == -32768) {
                continue;
            }

            bool is_window = false;
            if ((path_create_info->flags & PATH_FLAG_IGNORE_BLOCKERS) == 0) {
                if (adjacent_loc != path_create_info->to
                    || (path_create_info->flags & PATH_FLAG_STOP_BEFORE_DESTINATION) == 0) {
                    if ((path_create_info->flags & PATH_FLAG_0x0008) == 0
                        && tile_is_blocking(adjacent_loc, v1)) {
                        path_cost_tbl[neighbor_index] = -32768;
                        continue;
                    }

                    int64_t block_obj;
                    int block_obj_type;
                    if (object_calc_traversal_cost(path_create_info->obj, loc, rot, flags, &block_obj, &block_obj_type, &is_window)
                        || block_obj != OBJ_HANDLE_NULL) {
                        if ((rot & 1) != 0
                            && block_obj != OBJ_HANDLE_NULL
                            && block_obj_type != OBJ_TYPE_WALL
                            && block_obj_type != OBJ_TYPE_PORTAL) {
                            path_cost_tbl[neighbor_index] = -32768;
                        }
                        continue;
                    }
                }
            }

            // Base movement cost.
            int cost = 10;

            // Evade spotted traps.
            int64_t trap_obj = get_trap_at_location(adjacent_loc);
            if (trap_obj != OBJ_HANDLE_NULL) {
                if (trap_is_spotted(path_create_info->obj, trap_obj)) {
                    cost += 80;
                }
            }

            if (is_window) {
                cost += 50;
            }

            // Evade fire.
            if ((path_create_info->flags & PATH_FLAG_AVOID_FIRE) != 0
                && get_fire_at_location(adjacent_loc) != OBJ_HANDLE_NULL) {
                cost += 80;
            }

            // Evade differently lit areas.
            if ((path_create_info->flags & PATH_FLAG_AVOID_SPOTTED_TRAPS) != 0) {
                uint8_t src_lum = light_luminance_at_location(loc, 0, 0);
                uint8_t dst_lum = light_luminance_at_location(adjacent_loc, 0, 0);
                if (dst_lum > src_lum) {
                    if (dst_lum - src_lum > 20) {
                        cost += 40;
                    }
                } else {
                    if (src_lum - dst_lum > 20) {
                        cost -= 6;
                    }
                }
            }

            // Add accumulated cost to move the current node.
            cost += path_cost_tbl[current_index];

            // Add additional cost in case direction change is needed.
            if (path_backtrack_tbl[current_index] != -1
                && reference_rotation(path_backtrack_tbl[current_index], current_index, 64) != rot) {
                cost++;
            }

            // If this neighbor has not been reached or a lower cost is now
            // available, update its cost and record the current node as its
            // predecessor.
            if ((path_cost_tbl[neighbor_index] > 0 && path_cost_tbl[neighbor_index] > cost)
                || (path_cost_tbl[neighbor_index] < 0 && -path_cost_tbl[neighbor_index] > cost)
                || path_cost_tbl[neighbor_index] == 0) {
                path_cost_tbl[neighbor_index] = cost;
                path_backtrack_tbl[neighbor_index] = current_index;
            }
        }

        // Mark the current node as processed by negating its cost.
        path_cost_tbl[current_index] = -path_cost_tbl[current_index];
    }

    // Count the number of steps from the current location up to the origin.
    int step = 0;
    int prev_index = path_backtrack_tbl[current_index];
    while (prev_index != -1) {
        prev_index = path_backtrack_tbl[prev_index];
        step++;
    }

    // If the number of steps exceeds allowed maximum, there is no valid path.
    if (step > path_create_info->max_rotations) {
        if (timestamp != 0) {
            dword_5DE600 += tig_timer_elapsed(timestamp);
        }

        return 0;
    }

    // Reconstruct the path backwards from dst to src.
    for (int i = step - 1; i >= 0; i--) {
        path_create_info->rotations[i] = reference_rotation(path_backtrack_tbl[target_index], target_index, 64);
        target_index = path_backtrack_tbl[target_index];
    }

    if ((path_create_info->flags & PATH_FLAG_STOP_BEFORE_DESTINATION) != 0) {
        step--;
    }

    if (timestamp != 0) {
        dword_5DE600 += tig_timer_elapsed(timestamp);
    }

    // Return the number of steps in the computed path.
    return step;
}

static int reference_path_dist(int src, int dst, int width)
{
    int dx;
    int dy;

    dx = src % width - dst % width;
    if (dx < 0) {
        dx = -dx;
    }

    dy = src / width - dst / width;
    if (dy < 0) {
        dy = -dy;
    }

    if (dx > dy) {
        return 10 * dx;
    } else {
        return 10 * dy;
    }
}

static int reference_rotation(int a1, int a2, int a3)
{
    int v3;
    int v4;
    int rot;

    v3 = a2 % a3 - a1 % a3;
    v4 = a2 / a3 - a1 / a3;

    if (v4 >= 0) {
        if (v3 >= 0) {
            if (v4 <= 0) {
                if (v4 > -1)
                    rot = (v4 < 1) + 4;
                else
                    rot = 6;
            } else {
                rot = (v3 >= 1) + 3;
            }
        } else if (v4 > -1) {
            rot = (v4 >= 1) + 1;
        } else {
            rot = 0;
        }
    } else if (v3 > -1) {
        rot = (v3 < 1) + 6;
    } else {
        rot = 0;
    }

    return rot;
}
