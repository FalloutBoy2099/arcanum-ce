#ifndef TIG_CACHE_BUDGET_H_
#define TIG_CACHE_BUDGET_H_

#include <stdint.h>

typedef struct TigCacheBudget {
    uint64_t total_bytes;
    uint64_t art_system_bytes;
    uint64_t art_video_bytes;
    uint64_t prefetch_bytes;
    uint64_t reader_bytes;
    uint64_t file_limit;
    int prefetch_entries;
    int reader_jobs;
} TigCacheBudget;

static inline TigCacheBudget tig_cache_budget_plan(uint64_t total, uint64_t available, unsigned int pointer_bits)
{
    TigCacheBudget plan = { 0 };
    const uint64_t mib = UINT64_C(1024) * 1024;
    if (available > total) available = total;
    uint64_t limit, memory_share;
    if (pointer_bits == 64) { limit = 8192 * mib; memory_share = available / 2; }
    else if (pointer_bits == 32) { limit = 768 * mib; memory_share = available / 4; }
    else return plan;
    plan.total_bytes = total / 4;
    if (plan.total_bytes > memory_share) plan.total_bytes = memory_share;
    if (plan.total_bytes > limit) plan.total_bytes = limit;
    if (plan.total_bytes >= 64 * mib) {
        plan.prefetch_bytes = plan.total_bytes / 16;
        plan.reader_bytes = plan.total_bytes / 64;
        plan.file_limit = plan.reader_bytes / 4;
        uint64_t file_limit = (pointer_bits == 64 ? 16 : 4) * mib;
        if (plan.file_limit > file_limit) plan.file_limit = file_limit;
        plan.prefetch_entries = (int)(plan.prefetch_bytes / (16 * 1024));
        if (plan.prefetch_entries < 64) plan.prefetch_entries = 64;
        if (plan.prefetch_entries > (pointer_bits == 64 ? 8192 : 2048)) plan.prefetch_entries = pointer_bits == 64 ? 8192 : 2048;
        plan.reader_jobs = plan.reader_bytes >= 32 * mib ? 32 : 8;
    }
    uint64_t art_bytes = plan.total_bytes - plan.prefetch_bytes - plan.reader_bytes;
    plan.art_system_bytes = art_bytes / 5 * 3;
    plan.art_video_bytes = art_bytes - plan.art_system_bytes;
    return plan;
}

const TigCacheBudget* tig_cache_budget_get(void);

#endif
