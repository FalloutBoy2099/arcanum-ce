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
    TIG_PERF_OBJECT_SORT,
    TIG_PERF_ART_EVICTION,
    TIG_PERF_ART_LOAD,
    TIG_PERF_ART_PREPARE,
    TIG_PERF_ART_BLIT,
    TIG_PERF_ART_FILE_OPEN,
    TIG_PERF_ART_METADATA,
    TIG_PERF_ART_PIXELS,
    TIG_PERF_SECTOR_LOAD,
    TIG_PERF_SECTOR_SAVE,
    TIG_PERF_SECTOR_ACTIVATE,
    TIG_PERF_SECTOR_ART_PRECACHE,
    TIG_PERF_ARCHIVE_READ,
    TIG_PERF_ARCHIVE_INFLATE,
    TIG_PERF_AI_DECISION,
    TIG_PERF_EVENT_LATENESS,
    TIG_PERF_FILE_READ_NATIVE,
    TIG_PERF_FRAME_INTERVAL,
    TIG_PERF_MAIN_TAIL,
    TIG_PERF_MESSAGE_DISPATCH,
    TIG_PERF_INPUT_DISPATCH,
    TIG_PERF_MENU_MODAL,
    TIG_PERF_PROFILE_FLUSH,
    TIG_PERF_BENCHMARK_CONTROL,
    TIG_PERF_NEIGHBOR_PREFETCH,
    TIG_PERF_SIMULATION_MODULE,
    TIG_PERF_SAVE_GAME,
    TIG_PERF_TELEPORT,
    TIG_PERF_VISUAL_FADE,
    TIG_PERF_EVENT_RECOVERY,
    TIG_PERF_EVENT_CALLBACK,
    TIG_PERF_UI_ASSET_WARMUP,
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
    TIG_PERF_OBJECT_SECTOR_LOADS,
    TIG_PERF_OBJECT_BLITS,
    TIG_PERF_ART_EVICTIONS,
    TIG_PERF_ZOOM_SCALED_PIXELS,
    TIG_PERF_ZOOM_FULL_COPIES,
    TIG_PERF_SECTOR_HITS,
    TIG_PERF_SECTOR_MISSES,
    TIG_PERF_SECTOR_EVICTIONS,
    TIG_PERF_SECTOR_LOAD_FAILURES,
    TIG_PERF_ART_HITS,
    TIG_PERF_ART_MISSES,
    TIG_PERF_ARCHIVE_READ_BYTES,
    TIG_PERF_ARCHIVE_DECODED_BYTES,
    TIG_PERF_FRAME_OVER_16MS,
    TIG_PERF_FRAME_OVER_33MS,
    TIG_PERF_TRACE_DROPPED,
    TIG_PERF_FILE_READ_NATIVE_BYTES,
    TIG_PERF_PREFETCH_REQUESTS,
    TIG_PERF_PREFETCH_HITS,
    TIG_PERF_PREFETCH_MISSES,
    TIG_PERF_PREFETCH_READY_BYTES,
    TIG_PERF_PREFETCH_SOURCE_REJECTIONS,
    TIG_PERF_PREFETCH_SOURCE_LIMIT_REJECTIONS,
    TIG_PERF_NEIGHBOR_REQUESTS,
    TIG_PERF_NEIGHBOR_READY,
    TIG_PERF_NEIGHBOR_FAILURES,
    TIG_PERF_NEIGHBOR_ART_REQUESTS,
    TIG_PERF_TRACE_SAMPLED_OUT,
    TIG_PERF_PREFETCH_DECODED_ART,
    TIG_PERF_PREFETCH_ARCHIVE_READY_BYTES,
    TIG_PERF_PREFETCH_ARCHIVE_HITS,
    TIG_PERF_PREFETCH_EVICTIONS,
    TIG_PERF_PREFETCH_EVICTED_BYTES,
    TIG_PERF_COUNTER_COUNT,
} TigPerformanceCounter;

typedef enum TigPerformanceEvent {
    TIG_PERF_EVENT_ATTACK_BEGIN,
    TIG_PERF_EVENT_ATTACK_RELEASE,
    TIG_PERF_EVENT_ATTACK_END,
    TIG_PERF_EVENT_DAMAGE_RESOLVE,
    TIG_PERF_EVENT_ATTACK_SCHEDULE,
    TIG_PERF_EVENT_AI_ACTION,
    TIG_PERF_EVENT_LATE_DISPATCH,
    TIG_PERF_EVENT_ATTACK_STATS,
    TIG_PERF_EVENT_ATTACK_ART,
    TIG_PERF_EVENT_PAUSE_STATE,
    TIG_PERF_EVENT_SLOW_CALLBACK,
    TIG_PERF_EVENT_UTILITY_AI,
    TIG_PERF_EVENT_UTILITY_AI_DETAIL,
    TIG_PERF_EVENT_COUNT,
} TigPerformanceEvent;

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
// Also identifies operations >= 5 ms in <capture>.details.csv. Detail logging
// is best-effort and includes only resource paths, never game text.
void tig_performance_end_detail(TigPerformanceMetric metric, Uint64 start, const char* detail);
void tig_performance_record_detail(TigPerformanceMetric metric, double milliseconds, const char* detail);
void tig_performance_record(TigPerformanceMetric metric, double milliseconds);
void tig_performance_count(TigPerformanceCounter counter, Uint64 value);
bool tig_performance_summary(TigPerformanceMetric metric, TigPerformanceSummary* summary);
Uint64 tig_performance_counter(TigPerformanceCounter counter);
void tig_performance_flush(void);
void tig_performance_poll(void);
void tig_performance_trace(TigPerformanceEvent event, Uint64 subject, Uint64 correlation, Sint64 value, Sint64 auxiliary);

#ifdef __cplusplus
}
#endif

#endif
