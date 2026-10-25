#include "tig/performance.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tig/memory.h"

#define SAMPLE_CAPACITY 2048

typedef struct PerformanceSamples {
    Uint64 count;
    double total;
    double maximum;
    double values[SAMPLE_CAPACITY];
} PerformanceSamples;

static const char* metric_names[TIG_PERF_METRIC_COUNT] = {
    "frame_work", "simulation", "world_render", "zoom_blit", "present_with_vsync",
    "path", "text_measure", "text_raster", "journal_build", "crafting_scan",
    "journal_paginate",
    "texture_upload", "render_present_with_vsync",
    "lighting", "object_render", "collision", "event_dispatch",
    "object_sort", "art_eviction",
    "art_load", "art_prepare", "art_blit",
    "art_file_open", "art_metadata", "art_pixels",
    "sector_load", "sector_save", "sector_activate", "sector_art_precache",
    "archive_read", "archive_inflate", "ai_decision", "event_lateness_real",
    "file_read_native", "frame_interval",
    "main_tail", "message_dispatch", "input_dispatch", "menu_modal", "profile_flush",
    "benchmark_control", "neighbor_prefetch",
    "simulation_module", "save_game", "teleport", "visual_fade",
    "event_recovery", "event_callback", "ui_asset_warmup",
};
static const char* counter_names[TIG_PERF_COUNTER_COUNT] = {
    "dirty_pixels_requested", "path_expansions", "path_scanned_cells",
    "search_cache_builds", "search_cache_hits", "history_cache_hits", "index_cache_hits",
    "texture_upload_bytes", "text_cache_hits", "text_cache_misses",
    "object_queries", "object_query_visits", "event_nodes_new", "event_nodes_reused", "event_insert_steps",
    "object_sector_loads",
    "object_blits", "art_evictions",
    "zoom_scaled_pixels", "zoom_full_copies",
    "sector_cache_hits", "sector_cache_misses", "sector_evictions", "sector_load_failures",
    "art_cache_hits", "art_cache_misses", "archive_read_bytes", "archive_decoded_bytes",
    "frame_over_16_67ms", "frame_over_33_33ms", "trace_events_dropped",
    "file_read_native_bytes",
    "prefetch_requests", "prefetch_hits", "prefetch_misses", "prefetch_ready_bytes",
    "prefetch_source_rejections", "prefetch_source_limit_rejections",
    "neighbor_requests", "neighbor_ready", "neighbor_failures", "neighbor_art_requests", "trace_sampled_out",
    "prefetch_decoded_art", "prefetch_archive_ready_bytes", "prefetch_archive_hits",
    "prefetch_evictions", "prefetch_evicted_bytes",
};
#define TRACE_CAPACITY 4096

typedef struct PerformanceTrace {
    Uint64 timestamp;
    TigPerformanceEvent event;
    Uint64 subject;
    Uint64 correlation;
    Sint64 value;
    Sint64 auxiliary;
} PerformanceTrace;

static const char* event_names[TIG_PERF_EVENT_COUNT] = {
    "attack_begin", "attack_release", "attack_end", "damage_resolve",
    "attack_schedule", "ai_action", "late_dispatch",
    "attack_stats", "attack_art", "pause_state", "slow_callback",
    "utility_ai",
    "utility_ai_detail",
};
static PerformanceTrace traces[TRACE_CAPACITY];
static unsigned int trace_count;
static unsigned int noisy_trace_count[3];
static FILE* event_output;

static PerformanceSamples samples[TIG_PERF_METRIC_COUNT];
static Uint64 counters[TIG_PERF_COUNTER_COUNT];
static FILE* output;
static FILE* details;
static Uint64 frequency;
static Uint64 window_start;
static Uint64 session;
static unsigned int interval;
#ifdef TIG_DEBUG_MEMORY
static TigMemoryStatistics memory_start;
#endif

static int compare_samples(const void* a, const void* b)
{
    double left = *(const double*)a;
    double right = *(const double*)b;
    return (left > right) - (left < right);
}

static void reset_window(void)
{
    trace_count = 0;
    memset(noisy_trace_count, 0, sizeof(noisy_trace_count));
    memset(samples, 0, sizeof(samples));
    memset(counters, 0, sizeof(counters));
    window_start = SDL_GetPerformanceCounter();
#ifdef TIG_DEBUG_MEMORY
    tig_memory_statistics(&memory_start);
#endif
}

bool tig_performance_set_enabled(bool enabled, const char* path)
{
    if (!enabled) {
        if (output != NULL) {
            tig_performance_flush();
            fclose(output);
            output = NULL;
        }
        if (details != NULL) { fclose(details); details = NULL; }
        if (event_output != NULL) { fclose(event_output); event_output = NULL; }
        return true;
    }
    if (output != NULL) return true;
    frequency = SDL_GetPerformanceFrequency();
    if (frequency == 0) return false;
    output = fopen(path != NULL ? path : "performance.csv", "a+");
    if (output == NULL) return false;
    if (fseek(output, 0, SEEK_END) != 0) {
        fclose(output);
        output = NULL;
        return false;
    }
    if (ftell(output) == 0) {
        fputs("session,interval,window_ms,kind,name,count,sampled,mean_ms,p50_ms,p95_ms,p99_ms,max_ms,value\n", output);
    }
    session = SDL_GetPerformanceCounter();
    interval = 0;
    char details_path[1024];
    int size = snprintf(details_path, sizeof(details_path), "%s.details.csv", path != NULL ? path : "performance.csv");
    if (size > 0 && size < sizeof(details_path)) {
        details = fopen(details_path, "a+");
        if (details != NULL) {
            if (fseek(details, 0, SEEK_END) == 0 && ftell(details) == 0)
                fputs("session,interval,metric,milliseconds,resource\n", details);
        }
    }
    size = snprintf(details_path, sizeof(details_path), "%s.events.csv", path != NULL ? path : "performance.csv");
    if (size > 0 && size < sizeof(details_path)) {
        event_output = fopen(details_path, "a+");
        if (event_output != NULL) {
            if (fseek(event_output, 0, SEEK_END) == 0 && ftell(event_output) == 0)
                fputs("session,interval,timestamp_ms,event,subject,correlation,value,auxiliary\n", event_output);
        }
    }
    reset_window();
    return true;
}

bool tig_performance_is_enabled(void)
{
    return output != NULL;
}

Uint64 tig_performance_session(void) { return session; }
unsigned int tig_performance_interval(void) { return interval; }

Uint64 tig_performance_begin(void)
{
    return output != NULL ? SDL_GetPerformanceCounter() : 0;
}

void tig_performance_end(TigPerformanceMetric metric, Uint64 start)
{
    if (output != NULL && start != 0) {
        Uint64 now = SDL_GetPerformanceCounter();
        if (now >= start) tig_performance_record(metric, (double)(now - start) * 1000.0 / (double)frequency);
    }
}

void tig_performance_end_detail(TigPerformanceMetric metric, Uint64 start, const char* detail)
{
    if (output == NULL || start == 0) return;
    Uint64 now = SDL_GetPerformanceCounter();
    if (now >= start) tig_performance_record_detail(metric, (double)(now - start) * 1000.0 / (double)frequency, detail);
}

void tig_performance_record_detail(TigPerformanceMetric metric, double milliseconds, const char* detail)
{
    tig_performance_record(metric, milliseconds);
    if (output == NULL || details == NULL || metric < 0 || metric >= TIG_PERF_METRIC_COUNT
        || !isfinite(milliseconds) || milliseconds < 5.0 || detail == NULL) return;
    fprintf(details, "%llu,%u,%s,%.6f,\"", (unsigned long long)session, interval, metric_names[metric], milliseconds);
    for (const char* p = detail; *p != '\0'; p++) {
        if (*p == '"') fputc('"', details);
        fputc(*p == '\n' || *p == '\r' ? ' ' : *p, details);
    }
    fputs("\"\n", details);
}

void tig_performance_record(TigPerformanceMetric metric, double milliseconds)
{
    if (output == NULL || metric < 0 || metric >= TIG_PERF_METRIC_COUNT
        || !isfinite(milliseconds) || milliseconds < 0.0) return;
    if (metric == TIG_PERF_FRAME_INTERVAL) {
        if (milliseconds > 1000.0 / 60.0) counters[TIG_PERF_FRAME_OVER_16MS]++;
        if (milliseconds > 1000.0 / 30.0) counters[TIG_PERF_FRAME_OVER_33MS]++;
    }
    PerformanceSamples* bucket = &samples[metric];
    bucket->values[bucket->count % SAMPLE_CAPACITY] = milliseconds;
    bucket->count++;
    bucket->total += milliseconds;
    if (milliseconds > bucket->maximum) bucket->maximum = milliseconds;
}

void tig_performance_trace(TigPerformanceEvent event, Uint64 subject, Uint64 correlation, Sint64 value, Sint64 auxiliary)
{
    if (output == NULL || event_output == NULL || event < 0 || event >= TIG_PERF_EVENT_COUNT) return;
    if (event == TIG_PERF_EVENT_AI_ACTION || event == TIG_PERF_EVENT_LATE_DISPATCH
        || event == TIG_PERF_EVENT_UTILITY_AI || event == TIG_PERF_EVENT_UTILITY_AI_DETAIL) {
        int bucket = event == TIG_PERF_EVENT_AI_ACTION ? 0 : event == TIG_PERF_EVENT_LATE_DISPATCH ? 1 : 2;
        if (noisy_trace_count[bucket]++ >= 512) {
            tig_performance_count(TIG_PERF_TRACE_SAMPLED_OUT, 1);
            return;
        }
    }
    if (trace_count == TRACE_CAPACITY) {
        counters[TIG_PERF_TRACE_DROPPED]++;
        return;
    }
    traces[trace_count++] = (PerformanceTrace){ SDL_GetPerformanceCounter(), event, subject, correlation, value, auxiliary };
}

void tig_performance_count(TigPerformanceCounter counter, Uint64 value)
{
    if (output != NULL && counter >= 0 && counter < TIG_PERF_COUNTER_COUNT) counters[counter] += value;
}

bool tig_performance_summary(TigPerformanceMetric metric, TigPerformanceSummary* summary)
{
    if (metric < 0 || metric >= TIG_PERF_METRIC_COUNT || summary == NULL) return false;
    const PerformanceSamples* bucket = &samples[metric];
    memset(summary, 0, sizeof(*summary));
    summary->count = bucket->count;
    if (bucket->count == 0) return true;
    unsigned int count = bucket->count < SAMPLE_CAPACITY ? (unsigned int)bucket->count : SAMPLE_CAPACITY;
    double sorted[SAMPLE_CAPACITY];
    memcpy(sorted, bucket->values, count * sizeof(double));
    qsort(sorted, count, sizeof(double), compare_samples);
    summary->sampled = count;
    summary->mean_ms = bucket->total / (double)bucket->count;
    summary->max_ms = bucket->maximum;
    summary->p50_ms = sorted[(count * 50 + 99) / 100 - 1];
    summary->p95_ms = sorted[(count * 95 + 99) / 100 - 1];
    summary->p99_ms = sorted[(count * 99 + 99) / 100 - 1];
    return true;
}

Uint64 tig_performance_counter(TigPerformanceCounter counter)
{
    return counter >= 0 && counter < TIG_PERF_COUNTER_COUNT ? counters[counter] : 0;
}

static void write_counter(double elapsed, const char* name, Uint64 value)
{
    fprintf(output, "%llu,%u,%.3f,counter,%s,0,0,0,0,0,0,0,%llu\n",
        (unsigned long long)session, interval, elapsed, name, (unsigned long long)value);
}

void tig_performance_flush(void)
{
    if (output == NULL) return;
    Uint64 flush_start = SDL_GetPerformanceCounter();
    double elapsed = (double)(SDL_GetPerformanceCounter() - window_start) * 1000.0 / (double)frequency;
    for (int i = 0; i < TIG_PERF_METRIC_COUNT; i++) {
        TigPerformanceSummary summary;
        tig_performance_summary((TigPerformanceMetric)i, &summary);
        if (summary.count == 0) continue;
        fprintf(output, "%llu,%u,%.3f,timing,%s,%llu,%u,%.6f,%.6f,%.6f,%.6f,%.6f,0\n",
            (unsigned long long)session, interval, elapsed, metric_names[i],
            (unsigned long long)summary.count, summary.sampled, summary.mean_ms,
            summary.p50_ms, summary.p95_ms, summary.p99_ms, summary.max_ms);
    }
    for (int i = 0; i < TIG_PERF_COUNTER_COUNT; i++) write_counter(elapsed, counter_names[i], counters[i]);
#ifdef TIG_DEBUG_MEMORY
    TigMemoryStatistics memory;
    tig_memory_statistics(&memory);
    write_counter(elapsed, "tig_allocation_requests", memory.allocation_requests - memory_start.allocation_requests);
    write_counter(elapsed, "tig_requested_bytes", memory.requested_bytes - memory_start.requested_bytes);
    write_counter(elapsed, "tig_live_bytes", memory.live_bytes);
    write_counter(elapsed, "tig_live_blocks", memory.live_blocks);
#endif
    if (event_output != NULL) {
        for (unsigned int i = 0; i < trace_count; i++) {
            PerformanceTrace* trace = &traces[i];
            double timestamp_ms = (double)(trace->timestamp - session) * 1000.0 / (double)frequency;
            fprintf(event_output, "%llu,%u,%.6f,%s,%llu,%llu,%lld,%lld\n",
                (unsigned long long)session, interval, timestamp_ms, event_names[trace->event],
                (unsigned long long)trace->subject, (unsigned long long)trace->correlation,
                (long long)trace->value, (long long)trace->auxiliary);
        }
        fflush(event_output);
    }
    fflush(output);
    if (details != NULL) fflush(details);
    interval++;
    reset_window();
    tig_performance_end_detail(TIG_PERF_PROFILE_FLUSH, flush_start, "profiler_csv");
}

void tig_performance_poll(void)
{
    if (output != NULL && (double)(SDL_GetPerformanceCounter() - window_start) / (double)frequency >= 5.0) {
        tig_performance_flush();
    }
}
