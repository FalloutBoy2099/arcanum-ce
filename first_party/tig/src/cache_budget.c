#include "tig/cache_budget.h"

#include <inttypes.h>
#include <limits.h>
#include "tig/memory.h"
#include "tig/debug.h"

const TigCacheBudget* tig_cache_budget_get(void)
{
    static bool initialized;
    static TigCacheBudget plan;
    if (!initialized) {
        size_t total, available;
        tig_memory_get_system_status(&total, &available);
        plan = tig_cache_budget_plan(total, available, sizeof(void*) * CHAR_BIT);
        initialized = true;
        tig_debug_printf("Cache budgets: bits=%u RAM=%" PRIu64 " available=%" PRIu64 " total=%" PRIu64
            " ART-system=%" PRIu64 " ART-buffers=%" PRIu64 " prefetch=%" PRIu64 " reader=%" PRIu64 " bytes.\n",
            (unsigned int)(sizeof(void*) * CHAR_BIT), (uint64_t)total, (uint64_t)available, plan.total_bytes,
            plan.art_system_bytes, plan.art_video_bytes, plan.prefetch_bytes, plan.reader_bytes);
    }
    return &plan;
}
