#include "game/path.h"
#include "game/path_open_set.h"
#include "game/critter.h"
#include "game/light.h"
#include "game/location.h"
#include "game/object.h"
#include "game/tile.h"
#include "game/trap.h"
#include <tig/performance.h>

static int path_cost_tbl[4096], path_backtrack_tbl[4096];
static PathOpenSet path_open_set;
static tig_timestamp_t path_budget_window_start;
static int path_budget_search_count;
static tig_duration_t path_budget_elapsed_milliseconds;
static int path_limit = 10, path_time_limit = 250;
static int path_dist(int src, int dst, int width);
static int path_grid_direction_between_indices(int from, int to, int width);

int path_weighted_baseline(PathCreateInfo* path_create_info)
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
    uint64_t counted_comparisons = 0;

    if (obj_field_int32_get(path_create_info->obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
        if (path_budget_window_start == 0
            || tig_timer_elapsed(path_budget_window_start) >= 1000) {
            tig_timer_now(&path_budget_window_start);
            path_budget_search_count = 0;
            path_budget_elapsed_milliseconds = 0;
        }

        if (path_budget_search_count >= path_limit) {
            return 0;
        }

        if (path_budget_elapsed_milliseconds >= path_time_limit) {
            return 0;
        }

        path_budget_search_count++;

        tig_timer_now(&timestamp);
    } else {
        timestamp = 0;
    }

    from_x = LOCATION_GET_X(path_create_info->from);
    from_y = LOCATION_GET_Y(path_create_info->from);
    to_x = LOCATION_GET_X(path_create_info->to);
    to_y = LOCATION_GET_Y(path_create_info->to);

    if ((path_create_info->flags & PATH_FLAG_IGNORE_TILE_BLOCKING) != 0) {
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

    if (dx > 32 || dy > 32) {
        return 0;
    }

    origin_x -= (64 - dx) / 2;
    origin_y -= (64 - dy) / 2;

    start_index = (int)(from_x + (from_y - origin_y) * 64 - origin_x);
    target_index = (int)(to_x + (to_y - origin_y) * 64 - origin_x);

    memset(path_cost_tbl, 0, sizeof(path_cost_tbl));
    path_cost_tbl[start_index] = 1;
    path_backtrack_tbl[start_index] = -1;
    path_open_set_init(&path_open_set);
    int start_estimate = 1 + path_dist(start_index, target_index, 64);
    if (start_estimate / 10 <= path_create_info->max_rotations) {
        path_open_set_update(&path_open_set, start_index, start_estimate);
    }

    while (true) {
        tig_performance_count(TIG_PERF_PATH_EXPANSIONS, 1);
        current_index = path_open_set_pop(&path_open_set);
        tig_performance_count(TIG_PERF_PATH_SCANNED_CELLS, path_open_set.comparisons - counted_comparisons);
        counted_comparisons = path_open_set.comparisons;

        if (current_index == -1) {
            if (timestamp != 0) {
                path_budget_elapsed_milliseconds += tig_timer_elapsed(timestamp);
            }

            return 0;
        }

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

            if (path_cost_tbl[neighbor_index] == -32768) {
                continue;
            }

            bool is_window = false;
            if ((path_create_info->flags & PATH_FLAG_IGNORE_BLOCKERS) == 0) {
                if (adjacent_loc != path_create_info->to
                    || (path_create_info->flags & PATH_FLAG_STOP_BEFORE_DESTINATION) == 0) {
                    if ((path_create_info->flags & PATH_FLAG_IGNORE_TILE_BLOCKING) == 0
                        && tile_is_blocking(adjacent_loc, v1)) {
                        path_cost_tbl[neighbor_index] = -32768;
                        path_open_set_remove(&path_open_set, neighbor_index);
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
                            path_open_set_remove(&path_open_set, neighbor_index);
                        }
                        continue;
                    }
                }
            }

            int cost = 10;

            int64_t trap_obj = get_trap_at_location(adjacent_loc);
            if (trap_obj != OBJ_HANDLE_NULL) {
                if (trap_is_spotted(path_create_info->obj, trap_obj)) {
                    cost += 80;
                }
            }

            if (is_window) {
                cost += 50;
            }

            if ((path_create_info->flags & PATH_FLAG_AVOID_FIRE) != 0
                && get_fire_at_location(adjacent_loc) != OBJ_HANDLE_NULL) {
                cost += 80;
            }

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

            cost += path_cost_tbl[current_index];

            if (path_backtrack_tbl[current_index] != -1
                && path_grid_direction_between_indices(path_backtrack_tbl[current_index], current_index, 64) != rot) {
                cost++;
            }

            if ((path_cost_tbl[neighbor_index] > 0 && path_cost_tbl[neighbor_index] > cost)
                || (path_cost_tbl[neighbor_index] < 0 && -path_cost_tbl[neighbor_index] > cost)
                || path_cost_tbl[neighbor_index] == 0) {
                path_cost_tbl[neighbor_index] = cost;
                path_backtrack_tbl[neighbor_index] = current_index;
                int estimate = cost + path_dist(neighbor_index, target_index, 64);
                if (estimate / 10 <= path_create_info->max_rotations) {
                    path_open_set_update(&path_open_set, neighbor_index, estimate);
                } else {
                    path_cost_tbl[neighbor_index] = -32768;
                    path_open_set_remove(&path_open_set, neighbor_index);
                }
            }
        }

        path_cost_tbl[current_index] = -path_cost_tbl[current_index];
    }

    int step = 0;
    int prev_index = path_backtrack_tbl[current_index];
    while (prev_index != -1) {
        prev_index = path_backtrack_tbl[prev_index];
        step++;
    }

    if (step > path_create_info->max_rotations) {
        if (timestamp != 0) {
            path_budget_elapsed_milliseconds += tig_timer_elapsed(timestamp);
        }

        return 0;
    }

    for (int i = step - 1; i >= 0; i--) {
        path_create_info->rotations[i] = path_grid_direction_between_indices(path_backtrack_tbl[target_index], target_index, 64);
        target_index = path_backtrack_tbl[target_index];
    }

    if ((path_create_info->flags & PATH_FLAG_STOP_BEFORE_DESTINATION) != 0) {
        step--;
    }

    if (timestamp != 0) {
        path_budget_elapsed_milliseconds += tig_timer_elapsed(timestamp);
    }

    return step;
}


static int path_dist(int src, int dst, int width)
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

static int path_grid_direction_between_indices(int from_index, int to_index, int grid_width)
{
    int delta_x;
    int delta_y;
    int rot;

    delta_x = to_index % grid_width - from_index % grid_width;
    delta_y = to_index / grid_width - from_index / grid_width;

    if (delta_y >= 0) {
        if (delta_x >= 0) {
            if (delta_y <= 0) {
                if (delta_y > -1)
                    rot = (delta_y < 1) + 4;
                else
                    rot = 6;
            } else {
                rot = (delta_x >= 1) + 3;
            }
        } else if (delta_y > -1) {
            rot = (delta_y >= 1) + 1;
        } else {
            rot = 0;
        }
    } else if (delta_x > -1) {
        rot = (delta_x < 1) + 6;
    } else {
        rot = 0;
    }

    return rot;
}
