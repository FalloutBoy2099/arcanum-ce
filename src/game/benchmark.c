#include "game/benchmark.h"

#include <stddef.h>

static const BenchmarkStep steps[] = {
    { "warmup", BENCHMARK_WARMUP, 0, 120, 120 },
    { "idle_opaque_full", BENCHMARK_IDLE, 0, 180, 180 },
    { "walk_1", BENCHMARK_MOVE, 0, 60, 600 },
    { "walk_2", BENCHMARK_MOVE, 1, 60, 600 },
    { "walk_3", BENCHMARK_MOVE, 2, 60, 600 },
    { "walk_4", BENCHMARK_MOVE, 3, 60, 600 },
    { "idle_opaque_hidden", BENCHMARK_HIDDEN, 0, 180, 180 },
    { "idle_translucent_full", BENCHMARK_TRANSLUCENT, 0, 180, 180 },
    { "zoom_075", BENCHMARK_ZOOM, 0, 180, 180 },
    { "rumors", BENCHMARK_RUMORS, 0, 60, 60 },
    { "conversations", BENCHMARK_CONVERSATIONS, 0, 180, 180 },
    { "quest_index", BENCHMARK_INDEX, 0, 180, 180 },
    { "schematics", BENCHMARK_SCHEMATICS, 0, 180, 180 },
};

const BenchmarkStep* benchmark_step(int index)
{
    return index >= 0 && index < (int)(sizeof(steps) / sizeof(steps[0])) ? &steps[index] : NULL;
}

void benchmark_run_init(BenchmarkRun* run)
{
    *run = (BenchmarkRun){ 0 };
}

bool benchmark_run_enter(BenchmarkRun* run)
{
    if (run->entered || run->finished || run->failed || benchmark_step(run->step) == NULL) return false;
    run->entered = true;
    return true;
}

bool benchmark_run_advance(BenchmarkRun* run, bool ready)
{
    if (!run->entered || run->finished || run->failed) return false;
    const BenchmarkStep* step = benchmark_step(run->step);
    if (step == NULL) { run->failed = true; return false; }
    run->frames++;
    if (run->frames >= step->maximum_frames && !ready) {
        run->failed = true;
        return false;
    }
    if (run->frames < step->minimum_frames || !ready) return false;
    run->step++;
    run->frames = 0;
    run->entered = false;
    run->finished = benchmark_step(run->step) == NULL;
    return true;
}
