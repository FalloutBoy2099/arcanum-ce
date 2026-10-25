#ifndef ARCANUM_EXPERIMENT_PATH_JPS_H_
#define ARCANUM_EXPERIMENT_PATH_JPS_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct PathExperimentGrid {
    int width;
    int height;
    uint8_t blocked[4096];
} PathExperimentGrid;

typedef struct PathExperimentResult {
    bool found;
    int steps;
    int cells[4096];
    uint64_t expanded;
    uint64_t probes;
    uint64_t precomputed_queries;
    int strategy;
} PathExperimentResult;

typedef enum PathExperimentAlgorithm {
    PATH_EXPERIMENT_ASTAR,
    PATH_EXPERIMENT_JPS,
    PATH_EXPERIMENT_JPS_PLUS,
    PATH_EXPERIMENT_DIRECT,
} PathExperimentAlgorithm;

typedef struct PathJpsPrecompute {
    PathExperimentGrid grid;
    int16_t distances[4096][8];
    uint64_t probes;
    bool valid;
} PathJpsPrecompute;

typedef struct PathExperimentPolicy {
    bool variable_costs;
    bool dynamic_obstacles;
    bool directional_collisions;
    int turn_penalty;
} PathExperimentPolicy;

typedef PathExperimentResult (*PathExperimentFallback)(void* context);

PathExperimentResult path_experiment_search(const PathExperimentGrid* grid, int start, int goal, bool jps);
bool path_experiment_precompute(const PathExperimentGrid* grid, PathJpsPrecompute* cache);
PathExperimentResult path_experiment_search_precomputed(const PathExperimentGrid* grid, int start, int goal,
    const PathJpsPrecompute* cache);
PathExperimentResult path_experiment_hybrid(const PathExperimentGrid* grid, int start, int goal,
    const PathExperimentPolicy* policy, const PathJpsPrecompute* cache,
    PathExperimentFallback fallback, void* context);

#endif
