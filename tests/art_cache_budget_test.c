#include <stdio.h>
#include <tig/art_cache_budget.h>
#include <tig/memory.h>

static int failures;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #value); failures++; } } while (0)

int main(void)
{
    const uint64_t gib = UINT64_C(1024) * 1024 * 1024;
    TigCacheBudget large = tig_cache_budget_plan(32 * gib, 16 * gib, 64);
    CHECK(large.total_bytes == 8 * gib);
    CHECK(large.art_system_bytes > 4 * gib && large.art_video_bytes > 2 * gib);
    CHECK(large.prefetch_bytes == gib / 2 && large.reader_bytes == gib / 8);
    CHECK(large.prefetch_entries == 8192 && large.reader_jobs == 32 && large.file_limit == gib / 64);
    TigCacheBudget small = tig_cache_budget_plan(32 * gib, 16 * gib, 32);
    CHECK(small.total_bytes == gib * 3 / 4 && small.file_limit <= 4 * 1024 * 1024);
    CHECK(tig_cache_budget_plan(0, 0, 64).total_bytes == 0);
    CHECK(tig_cache_budget_plan(gib, 0, 64).total_bytes == 0);
    CHECK(tig_cache_budget_plan(gib, gib, 16).total_bytes == 0);
    CHECK(tig_cache_budget_plan(UINT64_MAX, UINT64_MAX, 64).total_bytes == 8 * gib);
    CHECK(tig_cache_budget_plan(UINT64_MAX, UINT64_MAX, 32).total_bytes == gib * 3 / 4);
    for (unsigned int bits = 32; bits <= 64; bits += 32) {
        for (uint64_t ram = gib / 2; ram <= 128 * gib; ram *= 2) {
            uint64_t previous = 0;
            for (int share = 0; share <= 16; share++) {
                uint64_t free_bytes = ram / 16 * share;
                TigCacheBudget plan = tig_cache_budget_plan(ram, free_bytes, bits);
                CHECK(plan.total_bytes >= previous);
                CHECK(plan.total_bytes <= ram / 4 && plan.total_bytes <= free_bytes / (bits == 64 ? 2 : 4));
                CHECK(plan.total_bytes == plan.art_system_bytes + plan.art_video_bytes + plan.prefetch_bytes + plan.reader_bytes);
                CHECK(plan.prefetch_bytes <= gib / 2 && plan.reader_bytes <= gib / 8);
                CHECK(tig_art_cache_budget(ram, free_bytes, bits) == plan.art_system_bytes);
                if (plan.prefetch_entries != 0) {
                    CHECK(plan.file_limit > 0 && plan.file_limit <= plan.reader_bytes / 4);
                    CHECK(plan.prefetch_entries >= 64 && plan.prefetch_entries <= (bits == 64 ? 8192 : 2048));
                }
                previous = plan.total_bytes;
            }
            CHECK(tig_cache_budget_plan(ram, UINT64_MAX, bits).total_bytes == tig_cache_budget_plan(ram, ram, bits).total_bytes);
        }
    }
    const TigCacheBudget* current = tig_cache_budget_get();
    CHECK(current == tig_cache_budget_get());
    CHECK(current->total_bytes == current->art_system_bytes + current->art_video_bytes + current->prefetch_bytes + current->reader_bytes);
    size_t total, available;
    tig_memory_get_system_status(&total, &available);
    CHECK(total > 0 && available <= total);
    printf("ART cache budget: %d failures\n", failures);
    return failures != 0;
}
