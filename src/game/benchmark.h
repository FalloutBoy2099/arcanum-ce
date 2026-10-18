#ifndef ARCANUM_GAME_BENCHMARK_H_
#define ARCANUM_GAME_BENCHMARK_H_

#include <stdbool.h>
#include <stdint.h>

#define BENCHMARK_AUTORUN_KEY "benchmark_autorun"
#define BENCHMARK_SCENARIO_VERSION 2

typedef enum BenchmarkAction {
    BENCHMARK_WARMUP,
    BENCHMARK_IDLE,
    BENCHMARK_MOVE,
    BENCHMARK_HIDDEN,
    BENCHMARK_TRANSLUCENT,
    BENCHMARK_ZOOM,
    BENCHMARK_RUMORS,
    BENCHMARK_CONVERSATIONS,
    BENCHMARK_INDEX,
    BENCHMARK_SCHEMATICS,
} BenchmarkAction;

typedef struct BenchmarkStep {
    const char* name;
    BenchmarkAction action;
    int argument;
    unsigned int minimum_frames;
    unsigned int maximum_frames;
} BenchmarkStep;

typedef struct BenchmarkRun {
    int step;
    unsigned int frames;
    bool entered;
    bool finished;
    bool failed;
} BenchmarkRun;

const BenchmarkStep* benchmark_step(int index);
void benchmark_run_init(BenchmarkRun* run);
// Enter is called once, before drawing the first frame of a phase.
bool benchmark_run_enter(BenchmarkRun* run);
// A phase advances only after its minimum frame count and its checkpoint.
// False ready at the maximum frame count marks failure, never success.
bool benchmark_run_advance(BenchmarkRun* run, bool ready);
bool benchmark_route_find(int64_t pc, int64_t route[4]);
// Localization ID for a final/preflight status; zero means no visible notice.
int benchmark_notice_id(const char* status);

#endif
