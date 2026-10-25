#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "path_jps.h"

static int failures;
static unsigned int random_state = 0x20261018;
static const int directions[8][2] = { {-1,-1}, {-1,0}, {-1,1}, {0,1}, {1,1}, {1,0}, {1,-1}, {0,-1} };
static unsigned int random_value(void) { random_state = random_state * 1664525U + 1013904223U; return random_state; }

static int breadth_first(const PathExperimentGrid* grid, int start, int goal)
{
    int queue[4096], costs[4096];
    memset(costs, -1, sizeof(costs));
    int first = 0, last = 0;
    queue[last++] = start; costs[start] = 0;
    while (first < last) {
        int cell = queue[first++];
        if (cell == goal) return costs[cell];
        int x = cell % grid->width, y = cell / grid->width;
        for (int i = 0; i < 8; i++) {
            int nx = x + directions[i][0], ny = y + directions[i][1];
            if (nx < 0 || ny < 0 || nx >= grid->width || ny >= grid->height) continue;
            int next = ny * grid->width + nx;
            if (grid->blocked[next] || costs[next] != -1) continue;
            costs[next] = costs[cell] + 1;
            queue[last++] = next;
        }
    }
    return -1;
}

static void validate(const PathExperimentGrid* grid, int start, int goal, const PathJpsPrecompute* cache)
{
    int shortest = breadth_first(grid, start, goal);
    for (int algorithm = 0; algorithm < 4; algorithm++) {
        PathExperimentPolicy policy = { 0 };
        PathExperimentResult result = algorithm < 2 ? path_experiment_search(grid, start, goal, algorithm != 0)
            : algorithm == 2 ? path_experiment_search_precomputed(grid,start,goal,cache)
                            : path_experiment_hybrid(grid,start,goal,&policy,cache,NULL,NULL);
        if (result.found != (shortest >= 0) || (result.found && result.steps != shortest)) {
            if (failures < 5) fprintf(stderr, "%s mismatch %dx%d %d->%d: got %d, expected %d\n",
                algorithm == 0 ? "A*" : algorithm == 1 ? "JPS" : algorithm == 2 ? "JPS+" : "hybrid",
                grid->width, grid->height, start, goal, result.found ? result.steps : -1, shortest);
            failures++;
            continue;
        }
        if (!result.found) continue;
        if (result.cells[0] != start || result.cells[result.steps] != goal) failures++;
        for (int i = 1; i <= result.steps; i++) {
            int from = result.cells[i-1], to = result.cells[i];
            int dx = abs(from % grid->width - to % grid->width);
            int dy = abs(from / grid->width - to / grid->width);
            if (to < 0 || to >= grid->width * grid->height || grid->blocked[to] || dx > 1 || dy > 1 || dx + dy == 0) failures++;
        }
    }
}

static void correctness(void)
{
    PathExperimentGrid grid = { 3, 3, { 0 } };
    PathJpsPrecompute cache;
    int cases = 0;
    for (int mask = 0; mask < 512; mask++) {
        for (int i = 0; i < 9; i++) grid.blocked[i] = (mask >> i) & 1;
        if (!path_experiment_precompute(&grid,&cache)) failures++;
        for (int start = 0; start < 9; start++) {
            for (int goal = 0; goal < 9; goal++) {
                if (grid.blocked[start] || grid.blocked[goal]) continue;
                validate(&grid, start, goal, &cache); cases++;
            }
        }
    }
    grid.width = grid.height = 64;
    for (int trial = 0; trial < 2000; trial++) {
        int density = (trial % 5) * 10;
        for (int i = 0; i < 4096; i++) grid.blocked[i] = (random_value() >> 16) % 100 < (unsigned int)density;
        int start = random_value() % 4096, goal = random_value() % 4096;
        grid.blocked[start] = grid.blocked[goal] = 0;
        if (!path_experiment_precompute(&grid,&cache)) failures++;
        validate(&grid, start, goal, &cache); cases++;
    }
    printf("BFS correctness comparisons: %d cases, %d failures\n", cases, failures);
}

static int compare_time(const void* a, const void* b)
{
    double x=*(const double*)a, y=*(const double*)b;
    return (x>y)-(x<y);
}

static PathExperimentResult benchmark_search(const PathExperimentGrid* grid, int start, int goal,
    PathJpsPrecompute* cache, int algorithm)
{
    PathExperimentPolicy policy = { 0 };
    policy.dynamic_obstacles = algorithm == 4;
    if (algorithm == 5) path_experiment_precompute(grid,cache);
    if (algorithm < 2) return path_experiment_search(grid,start,goal,algorithm != 0);
    if (algorithm == 2 || algorithm == 5) return path_experiment_search_precomputed(grid,start,goal,cache);
    return path_experiment_hybrid(grid,start,goal,&policy,cache,NULL,NULL);
}

static void benchmark(void)
{
    const char* names[] = { "open", "wall_detour", "corridors", "dense", "unreachable" };
    PathExperimentGrid grid = { 64, 64, { 0 } };
    PathJpsPrecompute cache;
    printf("scenario,algorithm,median_us,min_us,max_us,expanded,probes,steps\n");
    for (int scenario = 0; scenario < 5; scenario++) {
        memset(grid.blocked, 0, sizeof(grid.blocked));
        if (scenario == 1 || scenario == 4) {
            for (int y = 0; y < 64; y++) grid.blocked[y*64+32] = scenario == 4 || y != 55;
        } else if (scenario == 2) {
            for (int x = 8; x < 60; x += 8) {
                for (int y = 0; y < 64; y++) grid.blocked[y*64+x] = y != ((x/8)&1 ? 50 : 10);
            }
        } else if (scenario == 3) {
            for (int i = 0; i < 4096; i++) grid.blocked[i] = (random_value() >> 16) % 100 < 30;
        }
        int start = 10*64+10, goal = 40*64+40;
        grid.blocked[start] = grid.blocked[goal] = 0;
        double prep_samples[9];
        for (int batch = 0; batch < 9; batch++) {
            Uint64 before = SDL_GetPerformanceCounter();
            for (int repeat = 0; repeat < 10; repeat++) if (!path_experiment_precompute(&grid,&cache)) failures++;
            prep_samples[batch] = (SDL_GetPerformanceCounter()-before) * 1000000.0 / SDL_GetPerformanceFrequency() / 10;
        }
        qsort(prep_samples,9,sizeof(double),compare_time);
        printf("PRECOMPUTE,%s,%.3f,%zu\n",names[scenario],prep_samples[4],sizeof(cache));
        validate(&grid, start, goal, &cache);
        const char* algorithms[] = { "A*", "JPS", "JPS+", "hybrid_static", "hybrid_dynamic", "JPS+_cold" };
        double samples[6][9];
        PathExperimentResult results[6];
        for (int batch = 0; batch < 9; batch++) {
            for (int offset = 0; offset < 6; offset++) {
                int algorithm = (batch+offset)%6;
                int iterations = algorithm == 5 ? 20 : 100;
                Uint64 before = SDL_GetPerformanceCounter();
                for (int iteration = 0; iteration < iterations; iteration++) results[algorithm] = benchmark_search(&grid,start,goal,&cache,algorithm);
                samples[algorithm][batch] = (SDL_GetPerformanceCounter()-before) * 1000000.0 / SDL_GetPerformanceFrequency() / iterations;
            }
        }
        for (int algorithm = 0; algorithm < 6; algorithm++) {
            qsort(samples[algorithm],9,sizeof(double),compare_time);
            PathExperimentResult result = results[algorithm];
            printf("%s,%s,%.3f,%.3f,%.3f,%llu,%llu,%d\n", names[scenario], algorithms[algorithm],
                samples[algorithm][4],samples[algorithm][0],samples[algorithm][8],
                (unsigned long long)result.expanded, (unsigned long long)result.probes, result.found ? result.steps : -1);
        }
    }
}

static void dynamic_changes(void)
{
    PathExperimentGrid grid = { 64, 64, { 0 } };
    PathJpsPrecompute cache;
    PathExperimentPolicy dynamic = { 0 };
    dynamic.dynamic_obstacles = true;
    int start = 10*64+10, goal = 40*64+40;
    int changed = 25*64+25;
    path_experiment_precompute(&grid,&cache);
    grid.blocked[changed] = 1;
    PathExperimentResult stale = path_experiment_search_precomputed(&grid,start,goal,&cache);
    if (stale.strategy != PATH_EXPERIMENT_JPS || stale.precomputed_queries != 0) failures++;
    validate(&grid,start,goal,&cache);
    PathExperimentResult result = path_experiment_hybrid(&grid,start,goal,&dynamic,&cache,NULL,NULL);
    if (result.strategy != PATH_EXPERIMENT_JPS || !result.found) failures++;
    path_experiment_precompute(&grid,&cache);
    validate(&grid,start,goal,&cache);
    grid.blocked[changed] = 0;
    validate(&grid,start,goal,&cache);
    for (int change = 0; change < 100; change++) {
        int cell = random_value()%4096;
        if (cell == start || cell == goal) continue;
        grid.blocked[cell] ^= 1;
        validate(&grid,start,goal,&cache);
        path_experiment_precompute(&grid,&cache);
        validate(&grid,start,goal,&cache);
    }
    printf("Dynamic edits: stale-cache rejection, rebuilt cache and hybrid checked\n");
}

int main(void)
{
    correctness();
    dynamic_changes();
    if (failures == 0) benchmark();
    return failures != 0;
}
