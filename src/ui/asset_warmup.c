#include "ui/asset_warmup.h"

#include <tig/art.h>
#include <tig/asset_prefetch.h>
#include <tig/performance.h>

static int art_numbers[UI_ASSET_WARMUP_CAPACITY];
static bool prepared[UI_ASSET_WARMUP_CAPACITY];
static int art_count;
static int art_cursor;

void ui_asset_warmup_clear(void)
{
    art_count = 0;
    art_cursor = 0;
}

void ui_asset_warmup_rewind(void)
{
    art_cursor = 0;
    for (int i = 0; i < art_count; i++) prepared[i] = false;
}

void ui_asset_warmup_register(int art_number)
{
    if (art_number < 0) return;
    for (int i = 0; i < art_count; i++) if (art_numbers[i] == art_number) return;
    if (art_count < UI_ASSET_WARMUP_CAPACITY) {
        prepared[art_count] = false;
        art_numbers[art_count++] = art_number;
    }
}

int ui_asset_warmup_startup(void)
{
    if (!tig_asset_prefetch_is_enabled()) return 0;
    Uint64 start = tig_performance_begin();
    int loaded = 0;
    for (int index = 0; index < art_count; index++) {
        if (prepared[index]) continue;
        tig_art_id_t art_id;
        if (tig_art_interface_id_create(art_numbers[index], 0, 0, 0, &art_id) == TIG_OK
            && tig_art_touch(art_id) == TIG_OK) {
            prepared[index] = true;
            loaded++;
        }
    }
    tig_performance_end_detail(TIG_PERF_UI_ASSET_WARMUP, start, "startup_interface_images");
    return loaded;
}

void ui_asset_warmup_tick(bool allowed)
{
    if (!allowed || art_count == 0 || !tig_asset_prefetch_is_enabled()) return;
    int next = -1;
    for (int offset = 0; offset < art_count; offset++) {
        int candidate = (art_cursor + offset) % art_count;
        if (!prepared[candidate]) { next = candidate; break; }
    }
    if (next == -1) return;
    art_cursor = next;
    Uint64 start = tig_performance_begin();
    int requested = 0;
    for (int offset = 0; offset < art_count && requested < 4; offset++) {
        int candidate = (art_cursor + offset) % art_count;
        if (prepared[candidate]) continue;
        tig_art_id_t art_id;
        if (tig_art_interface_id_create(art_numbers[candidate], 0, 0, 0, &art_id) == TIG_OK)
            tig_art_prefetch(art_id, 8 - requested);
        requested++;
    }
    tig_art_id_t art_id;
    if (tig_art_interface_id_create(art_numbers[art_cursor], 0, 0, 0, &art_id) == TIG_OK)
        prepared[art_cursor] = tig_art_touch_prefetched(art_id);
    art_cursor = (art_cursor + 1) % art_count;
    tig_performance_end_detail(TIG_PERF_UI_ASSET_WARMUP, start, "interface_images");
}
