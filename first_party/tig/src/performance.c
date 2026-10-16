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
};
static const char* counter_names[TIG_PERF_COUNTER_COUNT] = {
    "dirty_pixels_requested", "path_expansions", "path_scanned_cells",
    "search_cache_builds", "search_cache_hits", "history_cache_hits", "index_cache_hits",
};
static PerformanceSamples samples[TIG_PERF_METRIC_COUNT];
static Uint64 counters[TIG_PERF_COUNTER_COUNT];
static FILE* output;
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

void tig_performance_record(TigPerformanceMetric metric, double milliseconds)
{
    if (output == NULL || metric < 0 || metric >= TIG_PERF_METRIC_COUNT
        || !isfinite(milliseconds) || milliseconds < 0.0) return;
    PerformanceSamples* bucket = &samples[metric];
    bucket->values[bucket->count % SAMPLE_CAPACITY] = milliseconds;
    bucket->count++;
    bucket->total += milliseconds;
    if (milliseconds > bucket->maximum) bucket->maximum = milliseconds;
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
    fflush(output);
    interval++;
    reset_window();
}

void tig_performance_poll(void)
{
    if (output != NULL && (double)(SDL_GetPerformanceCounter() - window_start) / (double)frequency >= 5.0) {
        tig_performance_flush();
    }
}
