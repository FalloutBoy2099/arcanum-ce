#ifndef TIG_PERFORMANCE_H_
#define TIG_PERFORMANCE_H_

#include "tig/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Optional main-thread profiling. Timings are inclusive; nested metrics must
// not be added together. Disabled profiling does not read the clock.
typedef enum TigPerformanceMetric {
    TIG_PERF_FRAME,
    TIG_PERF_SIMULATION,
    TIG_PERF_WORLD,
    TIG_PERF_ZOOM_BLIT,
    TIG_PERF_PRESENT,
    TIG_PERF_PATH,
    TIG_PERF_TEXT_MEASURE,
    TIG_PERF_TEXT_RASTER,
    TIG_PERF_JOURNAL,
    TIG_PERF_CRAFTING,
    TIG_PERF_JOURNAL_LAYOUT,
    TIG_PERF_TEXTURE_UPLOAD,
    TIG_PERF_RENDER_PRESENT,
    TIG_PERF_LIGHTING,
    TIG_PERF_OBJECTS,
    TIG_PERF_COLLISION,
    TIG_PERF_EVENTS,
    TIG_PERF_METRIC_COUNT,
} TigPerformanceMetric;

typedef enum TigPerformanceCounter {
    TIG_PERF_DIRTY_PIXELS,
    TIG_PERF_PATH_EXPANSIONS,
    TIG_PERF_PATH_SCANNED_CELLS,
    TIG_PERF_SEARCH_CACHE_BUILDS,
    TIG_PERF_SEARCH_CACHE_HITS,
    TIG_PERF_HISTORY_CACHE_HITS,
    TIG_PERF_INDEX_CACHE_HITS,
    TIG_PERF_UPLOAD_BYTES,
    TIG_PERF_TEXT_CACHE_HITS,
    TIG_PERF_TEXT_CACHE_MISSES,
    TIG_PERF_OBJECT_QUERIES,
    TIG_PERF_OBJECT_QUERY_VISITS,
    TIG_PERF_EVENT_NODES_NEW,
    TIG_PERF_EVENT_NODES_REUSED,
    TIG_PERF_EVENT_INSERT_STEPS,
    TIG_PERF_COUNTER_COUNT,
} TigPerformanceCounter;

typedef struct TigPerformanceSummary {
    Uint64 count;
    unsigned int sampled;
    double mean_ms;
    double p50_ms;
    double p95_ms;
    double p99_ms;
    double max_ms;
} TigPerformanceSummary;

// Opens the CSV before enabling. Failure leaves profiling disabled. Passing
// NULL selects performance.csv in the current game directory. No game text or
// player data is written. Changing path requires disabling first.
bool tig_performance_set_enabled(bool enabled, const char* path);
bool tig_performance_is_enabled(void);
Uint64 tig_performance_session(void);
unsigned int tig_performance_interval(void);
Uint64 tig_performance_begin(void);
void tig_performance_end(TigPerformanceMetric metric, Uint64 start);
void tig_performance_record(TigPerformanceMetric metric, double milliseconds);
void tig_performance_count(TigPerformanceCounter counter, Uint64 value);
bool tig_performance_summary(TigPerformanceMetric metric, TigPerformanceSummary* summary);
Uint64 tig_performance_counter(TigPerformanceCounter counter);
void tig_performance_flush(void);
void tig_performance_poll(void);

#ifdef __cplusplus
}
#endif

#endif
