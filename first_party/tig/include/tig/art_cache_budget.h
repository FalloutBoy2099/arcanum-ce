#ifndef TIG_ART_CACHE_BUDGET_H_
#define TIG_ART_CACHE_BUDGET_H_

#include "tig/cache_budget.h"

static inline int64_t tig_art_cache_budget(uint64_t total_bytes, uint64_t available_bytes, unsigned int pointer_bits)
{
    return (int64_t)tig_cache_budget_plan(total_bytes, available_bytes, pointer_bits).art_system_bytes;
}

#endif
