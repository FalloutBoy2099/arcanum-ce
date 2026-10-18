#include "game/benchmark.h"

#include <stddef.h>
#include <string.h>

int benchmark_notice_id(const char* status)
{
    if (status == NULL || strcmp(status, "interrupted_shutdown") == 0 || strcmp(status, "starting") == 0) return 0;
    if (strcmp(status, "finished") == 0) return 1;
    if (strcmp(status, "requires_safe_save") == 0) return 2;
    if (strcmp(status, "route_unavailable") == 0) return 3;
    if (strcmp(status, "output_unavailable") == 0) return 4;
    if (strcmp(status, "timeout") == 0) return 5;
    if (strcmp(status, "action_failed") == 0) return 6;
    if (strcmp(status, "interrupted_focus_lost") == 0) return 8;
    if (strcmp(status, "interrupted_quit") == 0) return 9;
    if (strcmp(status, "cancelled") == 0) return 10;
    if (strcmp(status, "aborted") == 0 || strncmp(status, "interrupted_", 12) == 0) return 7;
    return 0;
}

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
    { "quest_index_repeat", BENCHMARK_INDEX, 1, 180, 180 },
    { "conversations_repeat", BENCHMARK_CONVERSATIONS, 1, 180, 180 },
    { "schematics_repeat", BENCHMARK_SCHEMATICS, 1, 180, 180 },
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
