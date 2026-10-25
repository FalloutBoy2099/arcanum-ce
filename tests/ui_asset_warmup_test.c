#include <stdio.h>
#include <tig/art.h>
#include <tig/asset_prefetch.h>
#include <tig/performance.h>

#include "ui/asset_warmup.h"

static int failures;
static bool enabled = true;
static bool ready;
static unsigned int requests;
static unsigned int attempts;
static tig_art_id_t attempted[512];
static int maximum_priority;
static unsigned int startup_attempts;
static int startup_failed_id = -1;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #value); failures++; } } while (0)

bool tig_asset_prefetch_is_enabled(void) { return enabled; }
Uint64 tig_performance_begin(void) { return 0; }
void tig_performance_end_detail(TigPerformanceMetric metric, Uint64 start, const char* detail)
{
    (void)metric; (void)start; (void)detail;
}

int tig_art_interface_id_create(unsigned int number, unsigned int frame, unsigned char palette, unsigned int rotation, tig_art_id_t* id)
{
    (void)frame; (void)palette; (void)rotation;
    *id = number;
    return TIG_OK;
}

void tig_art_prefetch(tig_art_id_t id, int priority)
{
    (void)id;
    requests++;
    if (priority > maximum_priority) maximum_priority = priority;
}

int tig_art_touch(tig_art_id_t id)
{
    startup_attempts++;
    return id == (tig_art_id_t)startup_failed_id ? TIG_ERR_IO : TIG_OK;
}

bool tig_art_touch_prefetched(tig_art_id_t id)
{
    if (attempts < 512) attempted[attempts] = id;
    attempts++;
    return ready;
}

int main(void)
{
    ui_asset_warmup_clear();
    ui_asset_warmup_tick(true);
    CHECK(requests == 0 && attempts == 0);
    ui_asset_warmup_register(-1);
    for (int i = 0; i < 6; i++) ui_asset_warmup_register(i);
    ui_asset_warmup_register(0);
    ui_asset_warmup_tick(false);
    enabled = false;
    ui_asset_warmup_tick(true);
    CHECK(requests == 0 && attempts == 0);
    enabled = true;
    for (int i = 0; i < 12; i++) {
        unsigned int before = requests;
        ui_asset_warmup_tick(true);
        CHECK(requests - before == 4);
        CHECK(attempts == (unsigned int)i + 1);
        CHECK(attempted[i] == (unsigned int)i % 6);
    }
    CHECK(maximum_priority == 8);
    ui_asset_warmup_clear();
    ui_asset_warmup_register(42);
    unsigned int before = requests;
    ui_asset_warmup_tick(true);
    CHECK(requests - before == 1 && attempted[12] == 42);
    ui_asset_warmup_clear();
    for (int i = 0; i < UI_ASSET_WARMUP_CAPACITY + 10; i++) ui_asset_warmup_register(i);
    unsigned int first = attempts;
    for (int i = 0; i <= UI_ASSET_WARMUP_CAPACITY; i++) ui_asset_warmup_tick(true);
    CHECK(attempted[first + UI_ASSET_WARMUP_CAPACITY] == 0);
    ui_asset_warmup_clear();
    ui_asset_warmup_register(9);
    ready = true;
    first = attempts;
    ui_asset_warmup_tick(true);
    CHECK(attempts == first + 1);
    ui_asset_warmup_tick(true);
    CHECK(attempts == first + 1);
    ui_asset_warmup_rewind();
    ui_asset_warmup_tick(true);
    CHECK(attempts == first + 2);
    ui_asset_warmup_clear();
    CHECK(ui_asset_warmup_startup() == 0 && startup_attempts == 0);
    ui_asset_warmup_register(7);
    ui_asset_warmup_register(8);
    ui_asset_warmup_register(9);
    ui_asset_warmup_register(7);
    enabled = false;
    CHECK(ui_asset_warmup_startup() == 0 && startup_attempts == 0);
    enabled = true;
    startup_failed_id = 8;
    CHECK(ui_asset_warmup_startup() == 2 && startup_attempts == 3);
    first = attempts;
    ui_asset_warmup_tick(true);
    CHECK(attempts == first + 1 && attempted[first] == 8);
    CHECK(ui_asset_warmup_startup() == 0 && startup_attempts == 3);
    ui_asset_warmup_rewind();
    CHECK(ui_asset_warmup_startup() == 2 && startup_attempts == 6);
    startup_failed_id = -1;
    CHECK(ui_asset_warmup_startup() == 1 && startup_attempts == 7);
    ui_asset_warmup_clear();
    for (int index = 0; index < UI_ASSET_WARMUP_CAPACITY + 10; index++) ui_asset_warmup_register(index);
    CHECK(ui_asset_warmup_startup() == UI_ASSET_WARMUP_CAPACITY);
    CHECK(startup_attempts == 7 + UI_ASSET_WARMUP_CAPACITY);
    printf("UI asset warmup: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures != 0;
}
