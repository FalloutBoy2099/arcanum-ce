#include "path_jps.h"

#include <stdlib.h>
#include <string.h>
#include "game/path_open_set.h"

typedef struct PathExperimentSearch {
    const PathExperimentGrid* grid;
    int goal;
    int costs[4096];
    int parents[4096];
    PathOpenSet open;
    PathExperimentResult result;
    const PathJpsPrecompute* precomputed;
} PathExperimentSearch;

static const int directions[8][2] = { {-1,-1}, {-1,0}, {-1,1}, {0,1}, {1,1}, {1,0}, {1,-1}, {0,-1} };

static int distance(int a, int b, int width)
{
    int x = abs(a % width - b % width);
    int y = abs(a / width - b / width);
    return x > y ? x : y;
}

static bool walkable(PathExperimentSearch* search, int x, int y)
{
    search->result.probes++;
    return x >= 0 && y >= 0 && x < search->grid->width && y < search->grid->height
        && !search->grid->blocked[y * search->grid->width + x];
}

static bool precomputed_matches(const PathExperimentGrid* grid, const PathJpsPrecompute* cache)
{
    return grid && cache && cache->valid && grid->width > 0 && grid->height > 0
        && grid->width <= 64 && grid->height <= 64 && grid->width == cache->grid.width
        && grid->height == cache->grid.height
        && memcmp(grid->blocked, cache->grid.blocked, grid->width * grid->height) == 0;
}

static int direction_index(int dx, int dy)
{
    for (int i = 0; i < 8; i++) if (directions[i][0] == dx && directions[i][1] == dy) return i;
    return -1;
}

static bool forced_neighbor(PathExperimentSearch* search, int x, int y, int dx, int dy)
{
    if (dx && dy) return (!walkable(search,x-dx,y) && walkable(search,x-dx,y+dy))
        || (!walkable(search,x,y-dy) && walkable(search,x+dx,y-dy));
    if (dx) return (!walkable(search,x,y+1) && walkable(search,x+dx,y+1))
        || (!walkable(search,x,y-1) && walkable(search,x+dx,y-1));
    return (!walkable(search,x+1,y) && walkable(search,x+1,y+dy))
        || (!walkable(search,x-1,y) && walkable(search,x-1,y+dy));
}

bool path_experiment_precompute(const PathExperimentGrid* grid, PathJpsPrecompute* cache)
{
    if (!cache) return false;
    memset(cache, 0, sizeof(*cache));
    if (!grid || grid->width <= 0 || grid->height <= 0 || grid->width > 64 || grid->height > 64) return false;
    cache->grid = *grid;
    PathExperimentSearch search;
    memset(&search, 0, sizeof(search));
    search.grid = grid;
    for (int pass = 0; pass < 2; pass++) {
        for (int dir = 0; dir < 8; dir++) {
            int dx = directions[dir][0], dy = directions[dir][1];
            if ((dx && dy) != (pass != 0)) continue;
            for (int yi = 0; yi < grid->height; yi++) {
                int y = dy > 0 ? grid->height-1-yi : yi;
                for (int xi = 0; xi < grid->width; xi++) {
                    int x = dx > 0 ? grid->width-1-xi : xi;
                    int cell = y*grid->width+x;
                    if (grid->blocked[cell] || !walkable(&search,x+dx,y+dy)) continue;
                    int nx=x+dx, ny=y+dy, next=ny*grid->width+nx;
                    bool jump_point = forced_neighbor(&search,nx,ny,dx,dy);
                    if (dx && dy) jump_point = jump_point
                        || cache->distances[next][direction_index(dx,0)] > 0
                        || cache->distances[next][direction_index(0,dy)] > 0;
                    int following = cache->distances[next][dir];
                    cache->distances[cell][dir] = jump_point ? 1 : following > 0 ? following+1 : following-1;
                }
            }
        }
    }
    cache->probes = search.result.probes;
    cache->valid = true;
    return true;
}

static int jump_precomputed(PathExperimentSearch* search, int x, int y, int dx, int dy)
{
    int dir = direction_index(dx,dy);
    int signed_distance = search->precomputed->distances[y*search->grid->width+x][dir];
    search->result.precomputed_queries++;
    int span = abs(signed_distance);
    if (span == 0) return -1;
    int goal_x = search->goal % search->grid->width;
    int goal_y = search->goal / search->grid->width;
    int gx = goal_x-x, gy=goal_y-y;
    int candidate = 0;
    if (dx && dy) {
        gx *= dx; gy *= dy;
        if (gx > 0 && gy > 0) {
            int along = gx < gy ? gx : gy;
            if (along <= span) {
                int cell = (y+dy*along)*search->grid->width+x+dx*along;
                int remaining = abs(gx-gy);
                if (remaining == 0) candidate = along;
                else {
                    int cardinal = gx > gy ? direction_index(dx,0) : direction_index(0,dy);
                    search->result.precomputed_queries++;
                    if (abs(search->precomputed->distances[cell][cardinal]) >= remaining) candidate = along;
                }
            }
        }
    } else {
        int along = dx ? gx*dx : gy*dy;
        if ((dx ? gy == 0 : gx == 0) && along > 0 && along <= span) candidate = along;
    }
    if (signed_distance > 0 && (candidate == 0 || signed_distance < candidate)) candidate = signed_distance;
    return candidate > 0 ? (y+dy*candidate)*search->grid->width+x+dx*candidate : -1;
}

static int jump(PathExperimentSearch* search, int x, int y, int dx, int dy)
{
    while (true) {
        x += dx; y += dy;
        if (!walkable(search, x, y)) return -1;
        int cell = y * search->grid->width + x;
        if (cell == search->goal) return cell;
        if (dx && dy) {
            if ((!walkable(search, x-dx, y) && walkable(search, x-dx, y+dy))
                || (!walkable(search, x, y-dy) && walkable(search, x+dx, y-dy))) return cell;
            if (jump(search, x, y, dx, 0) != -1 || jump(search, x, y, 0, dy) != -1) return cell;
        } else if (dx) {
            if ((!walkable(search, x, y+1) && walkable(search, x+dx, y+1))
                || (!walkable(search, x, y-1) && walkable(search, x+dx, y-1))) return cell;
        } else {
            if ((!walkable(search, x+1, y) && walkable(search, x+1, y+dy))
                || (!walkable(search, x-1, y) && walkable(search, x-1, y+dy))) return cell;
        }
    }
}

static void offer(PathExperimentSearch* search, int current, int dx, int dy, bool jps)
{
    int x = current % search->grid->width;
    int y = current / search->grid->width;
    int next;
    if (jps) next = search->precomputed ? jump_precomputed(search,x,y,dx,dy) : jump(search, x, y, dx, dy);
    else {
        if (!walkable(search, x+dx, y+dy)) return;
        next = (y+dy) * search->grid->width + x+dx;
    }
    if (next < 0) return;
    int cost = search->costs[current] + 10 * distance(current, next, search->grid->width);
    if (search->costs[next] >= 0 && search->costs[next] <= cost) return;
    search->costs[next] = cost;
    search->parents[next] = current;
    path_open_set_update(&search->open, next, cost + 10 * distance(next, search->goal, search->grid->width));
}

static void expand(PathExperimentSearch* search, int current, bool jps)
{
    int parent = search->parents[current];
    if (!jps || parent == -1) {
        for (int i = 0; i < 8; i++) offer(search, current, directions[i][0], directions[i][1], jps);
        return;
    }
    int x = current % search->grid->width;
    int y = current / search->grid->width;
    int dx = x - parent % search->grid->width;
    int dy = y - parent / search->grid->width;
    dx = (dx > 0) - (dx < 0); dy = (dy > 0) - (dy < 0);
    if (dx && dy) {
        offer(search, current, dx, dy, true);
        offer(search, current, dx, 0, true);
        offer(search, current, 0, dy, true);
        if (!walkable(search, x-dx, y)) offer(search, current, -dx, dy, true);
        if (!walkable(search, x, y-dy)) offer(search, current, dx, -dy, true);
    } else if (dx) {
        offer(search, current, dx, 0, true);
        if (!walkable(search, x, y+1)) offer(search, current, dx, 1, true);
        if (!walkable(search, x, y-1)) offer(search, current, dx, -1, true);
    } else {
        offer(search, current, 0, dy, true);
        if (!walkable(search, x+1, y)) offer(search, current, 1, dy, true);
        if (!walkable(search, x-1, y)) offer(search, current, -1, dy, true);
    }
}

static PathExperimentResult search_internal(const PathExperimentGrid* grid, int start, int goal, bool jps,
    const PathJpsPrecompute* cache)
{
    PathExperimentSearch search;
    memset(&search, 0, sizeof(search));
    search.precomputed = cache;
    search.result.strategy = cache ? PATH_EXPERIMENT_JPS_PLUS : jps ? PATH_EXPERIMENT_JPS : PATH_EXPERIMENT_ASTAR;
    if (!grid || grid->width <= 0 || grid->height <= 0 || grid->width > 64 || grid->height > 64
        || start < 0 || goal < 0 || start >= grid->width * grid->height || goal >= grid->width * grid->height
        || grid->blocked[start] || grid->blocked[goal]) return search.result;
    search.grid = grid;
    search.goal = goal;
    memset(search.costs, -1, sizeof(search.costs));
    memset(search.parents, -1, sizeof(search.parents));
    path_open_set_init(&search.open);
    search.costs[start] = 0;
    path_open_set_update(&search.open, start, 10 * distance(start, goal, grid->width));
    int current;
    while ((current = path_open_set_pop(&search.open)) != -1) {
        search.result.expanded++;
        if (current == goal) {
            search.result.found = true;
            int chain[4096];
            int count = 0;
            for (int cell = goal; cell != -1 && count < 4096; cell = search.parents[cell]) chain[count++] = cell;
            int from = start;
            search.result.cells[0] = start;
            for (int i = count - 2; i >= 0; i--) {
                int to = chain[i];
                int dx = (to % grid->width > from % grid->width) - (to % grid->width < from % grid->width);
                int dy = (to / grid->width > from / grid->width) - (to / grid->width < from / grid->width);
                while (from != to) {
                    if (search.result.steps >= 4095) { search.result.found = false; return search.result; }
                    from += dy * grid->width + dx;
                    search.result.cells[++search.result.steps] = from;
                }
            }
            return search.result;
        }
        expand(&search, current, jps);
    }
    return search.result;
}

PathExperimentResult path_experiment_search(const PathExperimentGrid* grid, int start, int goal, bool jps)
{
    return search_internal(grid,start,goal,jps,NULL);
}

PathExperimentResult path_experiment_search_precomputed(const PathExperimentGrid* grid, int start, int goal,
    const PathJpsPrecompute* cache)
{
    return search_internal(grid,start,goal,true,precomputed_matches(grid,cache) ? cache : NULL);
}

PathExperimentResult path_experiment_hybrid(const PathExperimentGrid* grid, int start, int goal,
    const PathExperimentPolicy* policy, const PathJpsPrecompute* cache,
    PathExperimentFallback fallback, void* context)
{
    if (!policy || policy->variable_costs || policy->directional_collisions || policy->turn_penalty != 0) {
        PathExperimentResult result = fallback ? fallback(context) : (PathExperimentResult){ 0 };
        result.strategy = PATH_EXPERIMENT_ASTAR;
        return result;
    }
    if (!grid || grid->width <= 0 || grid->height <= 0 || grid->width > 64 || grid->height > 64
        || start < 0 || goal < 0 || start >= grid->width*grid->height || goal >= grid->width*grid->height
        || grid->blocked[start] || grid->blocked[goal]) return (PathExperimentResult){ 0 };
    PathExperimentResult direct = { 0 };
    direct.strategy = PATH_EXPERIMENT_DIRECT;
    int cell = start;
    direct.cells[0] = cell;
    while (cell != goal) {
        int x=cell%grid->width, y=cell/grid->width;
        int gx=goal%grid->width, gy=goal/grid->width;
        x += (gx>x)-(gx<x); y += (gy>y)-(gy<y);
        cell = y*grid->width+x;
        direct.probes++;
        if (grid->blocked[cell]) break;
        direct.cells[++direct.steps] = cell;
    }
    if (cell == goal) { direct.found = true; return direct; }
    PathExperimentResult result;
    if (distance(start,goal,grid->width) <= 8) result = path_experiment_search(grid,start,goal,false);
    else if (policy->dynamic_obstacles) result = path_experiment_search(grid,start,goal,true);
    else result = path_experiment_search_precomputed(grid,start,goal,cache);
    result.probes += direct.probes;
    return result;
}
