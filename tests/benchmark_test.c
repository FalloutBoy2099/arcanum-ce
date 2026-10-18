#include <stdio.h>
#include <string.h>

#include "game/benchmark.h"

static int failures;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); failures++; } } while (0)

int main(void)
{
    CHECK(benchmark_notice_id("finished") == 1);
    CHECK(benchmark_notice_id("requires_safe_save") == 2);
    CHECK(benchmark_notice_id("route_unavailable") == 3);
    CHECK(benchmark_notice_id("output_unavailable") == 4);
    CHECK(benchmark_notice_id("timeout") == 5);
    CHECK(benchmark_notice_id("action_failed") == 6);
    CHECK(benchmark_notice_id("aborted") == 7);
    CHECK(benchmark_notice_id("interrupted_focus_lost") == 8);
    CHECK(benchmark_notice_id("interrupted_quit") == 9);
    CHECK(benchmark_notice_id("cancelled") == 10);
    CHECK(benchmark_notice_id("interrupted_shutdown") == 0);
    CHECK(benchmark_notice_id(NULL) == 0 && benchmark_notice_id("starting") == 0);
    CHECK(benchmark_step(-1) == NULL && benchmark_step(1000) == NULL);
    BenchmarkRun run;
    benchmark_run_init(&run);
    CHECK(!benchmark_run_advance(&run, true));
    unsigned int total = 0;
    int entered = 0;
    while (!run.finished && !run.failed && total < 10000) {
        const BenchmarkStep* step = benchmark_step(run.step);
        CHECK(step != NULL);
        if (step == NULL) break;
        CHECK(step->minimum_frames > 0 && step->maximum_frames >= step->minimum_frames);
        CHECK(benchmark_run_enter(&run));
        CHECK(!benchmark_run_enter(&run));
        entered++;
        unsigned int frames = 0;
        int old_step = run.step;
        while (run.step == old_step && !run.failed) {
            frames++;
            bool ready = step->action != BENCHMARK_MOVE || frames >= 120;
            bool advanced = benchmark_run_advance(&run, ready);
            total++;
            CHECK(advanced == (frames >= step->minimum_frames && ready));
        }
        CHECK(frames == (step->action == BENCHMARK_MOVE ? 120 : step->minimum_frames));
    }
    CHECK(run.finished && !run.failed && entered == 16);
    CHECK(BENCHMARK_SCENARIO_VERSION == 2);
    CHECK(benchmark_step(13)->action == BENCHMARK_INDEX && benchmark_step(13)->argument == 1);
    CHECK(benchmark_step(14)->action == BENCHMARK_CONVERSATIONS && benchmark_step(14)->argument == 1);
    CHECK(benchmark_step(15)->action == BENCHMARK_SCHEMATICS && benchmark_step(15)->argument == 1);
    for (int i = 0; i < entered; i++) {
        for (int j = i + 1; j < entered; j++) CHECK(strcmp(benchmark_step(i)->name, benchmark_step(j)->name) != 0);
    }
    CHECK(!benchmark_run_enter(&run) && !benchmark_run_advance(&run, true));
    CHECK(strcmp(benchmark_step(0)->name, "warmup") == 0);
    for (int leg = 0; leg < 4; leg++) {
        const BenchmarkStep* step = benchmark_step(2 + leg);
        CHECK(step->action == BENCHMARK_MOVE && step->argument == leg);
    }
    benchmark_run_init(&run);
    run.step = 2;
    CHECK(benchmark_run_enter(&run));
    for (unsigned int frame = 1; frame <= benchmark_step(2)->maximum_frames; frame++) {
        CHECK(!benchmark_run_advance(&run, false));
        CHECK(run.failed == (frame == benchmark_step(2)->maximum_frames));
    }
    CHECK(!run.finished && run.step == 2);
    CHECK(!benchmark_run_enter(&run) && !benchmark_run_advance(&run, true));
    benchmark_run_init(&run);
    run.step = 2;
    CHECK(benchmark_run_enter(&run));
    // Reaching the destination on the last allowed frame is successful.
    for (unsigned int frame = 1; frame < benchmark_step(2)->maximum_frames; frame++) {
        CHECK(!benchmark_run_advance(&run, false));
    }
    CHECK(benchmark_run_advance(&run, true) && !run.failed && run.step == 3);
    printf("Automatic benchmark plan: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures != 0;
}
