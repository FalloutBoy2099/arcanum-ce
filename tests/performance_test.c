#include <math.h>
#include <stdio.h>
#include <string.h>

#include <tig/memory.h>
#include <tig/performance.h>

static int failures;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); failures++; } } while (0)

int main(void)
{
    const char* path = "performance-test.csv";
    CHECK(SDL_Init(0));
    CHECK(tig_memory_init(NULL) == TIG_OK);
    remove(path);
    CHECK(!tig_performance_is_enabled());
    CHECK(tig_performance_begin() == 0);
    tig_performance_record(TIG_PERF_PATH, 10.0);
    tig_performance_count(TIG_PERF_PATH_EXPANSIONS, 3);
    CHECK(tig_performance_counter(TIG_PERF_PATH_EXPANSIONS) == 0);
    CHECK(!tig_performance_set_enabled(true, "missing-performance-directory/report.csv"));
    CHECK(!tig_performance_is_enabled());
    CHECK(tig_performance_set_enabled(true, path));
    CHECK(tig_performance_is_enabled());
    TigPerformanceSummary summary;
    CHECK(tig_performance_summary(TIG_PERF_PATH, &summary));
    CHECK(summary.count == 0 && summary.sampled == 0 && summary.mean_ms == 0.0);
    CHECK(!tig_performance_summary(TIG_PERF_METRIC_COUNT, &summary));
    CHECK(!tig_performance_summary(TIG_PERF_PATH, NULL));
    tig_performance_record(TIG_PERF_PATH, -1.0);
    tig_performance_record(TIG_PERF_PATH, NAN);
    tig_performance_record(TIG_PERF_PATH, INFINITY);
    tig_performance_record(TIG_PERF_METRIC_COUNT, 1.0);
    for (int i = 100; i > 0; i--) tig_performance_record(TIG_PERF_PATH, i);
    CHECK(tig_performance_summary(TIG_PERF_PATH, &summary));
    CHECK(summary.count == 100 && summary.sampled == 100);
    CHECK(summary.mean_ms == 50.5 && summary.max_ms == 100.0);
    CHECK(summary.p50_ms == 50.0 && summary.p95_ms == 95.0 && summary.p99_ms == 99.0);
    for (int i = 1; i <= 4096; i++) tig_performance_record(TIG_PERF_WORLD, i);
    CHECK(tig_performance_summary(TIG_PERF_WORLD, &summary));
    CHECK(summary.count == 4096 && summary.sampled == 2048);
    CHECK(summary.mean_ms == 2048.5 && summary.max_ms == 4096.0);
    CHECK(summary.p50_ms == 3072.0 && summary.p95_ms == 3994.0 && summary.p99_ms == 4076.0);
    tig_performance_count(TIG_PERF_PATH_EXPANSIONS, 12);
    tig_performance_count(TIG_PERF_PATH_EXPANSIONS, 4);
    CHECK(tig_performance_counter(TIG_PERF_PATH_EXPANSIONS) == 16);
    Uint64 start = tig_performance_begin();
    CHECK(start != 0);
    tig_performance_end(TIG_PERF_TEXT_MEASURE, start);
    CHECK(tig_performance_summary(TIG_PERF_TEXT_MEASURE, &summary));
    CHECK(summary.count == 1 && summary.mean_ms >= 0.0);

    TigMemoryStatistics before, during, after;
    tig_memory_statistics(&before);
    void* memory = tig_memory_alloc(32, __FILE__, __LINE__);
    memory = tig_memory_realloc(memory, 48, __FILE__, __LINE__);
    tig_memory_statistics(&during);
    CHECK(during.allocation_requests - before.allocation_requests == 2);
    CHECK(during.requested_bytes - before.requested_bytes == 80);
    CHECK(during.live_bytes - before.live_bytes == 48);
    CHECK(during.live_blocks - before.live_blocks == 1);
    tig_memory_free(memory, __FILE__, __LINE__);
    tig_memory_statistics(&after);
    CHECK(after.live_bytes == before.live_bytes && after.live_blocks == before.live_blocks);

    tig_performance_flush();
    CHECK(tig_performance_counter(TIG_PERF_PATH_EXPANSIONS) == 0);
    CHECK(tig_performance_summary(TIG_PERF_PATH, &summary) && summary.count == 0);
    CHECK(tig_performance_set_enabled(false, NULL));
    CHECK(!tig_performance_is_enabled());
    CHECK(tig_performance_set_enabled(true, path));
    tig_performance_record(TIG_PERF_JOURNAL, 2.0);
    CHECK(tig_performance_set_enabled(false, NULL));
    FILE* file = fopen(path, "r");
    CHECK(file != NULL);
    if (file != NULL) {
        char line[512];
        int headers = 0, paths = 0, journals = 0;
        while (fgets(line, sizeof(line), file) != NULL) {
            int commas = 0;
            for (const char* p = line; *p != '\0'; p++) if (*p == ',') commas++;
            CHECK(commas == 12);
            if (strncmp(line, "session,", 8) == 0) headers++;
            if (strstr(line, ",timing,path,100,100,") != NULL) paths++;
            if (strstr(line, ",timing,journal_build,1,1,") != NULL) journals++;
        }
        CHECK(headers == 1 && paths == 1 && journals == 1);
        fclose(file);
    }
    remove(path);
    tig_memory_exit();
    SDL_Quit();
    printf("Performance regressions: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures != 0;
}
