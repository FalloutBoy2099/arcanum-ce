#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "ui/cache_key.h"
#include "ui/search_cache.h"
#include "ui/text_pagination.h"

static int failures;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); failures++; } } while (0)

typedef struct SearchFixture { int calls; bool visible[8]; } SearchFixture;

static bool visible(int index, void* context)
{
    SearchFixture* fixture = context;
    fixture->calls++;
    return fixture->visible[index];
}

typedef struct PageFixture {
    int heights[8];
    int calls;
    int first_calls;
    int right_calls;
    int dry_calls;
} PageFixture;

static int page_entry(int entry, int page, int remaining, bool dry, bool first, void* context)
{
    PageFixture* fixture = context;
    fixture->calls++;
    if (first) fixture->first_calls++;
    if (page == 1) fixture->right_calls++;
    if (dry) fixture->dry_calls++;
    int height = fixture->heights[entry];
    if (height > remaining) return first ? remaining : 0;
    return height;
}

static void test_filtered_pages(void)
{
    int pages[] = { 10, 10 };
    PageFixture fixture = { { -1, 6, -1, 6, -1, 6, -1, 6 } };
    CHECK(ui_text_paginate_spread(0, 7, pages, page_entry, &fixture) == 4);
    CHECK(fixture.right_calls > 0 && fixture.dry_calls == 0);
    // The next spread starts at the first entry that did not fit, even when
    // filtered entries intervene. Every matching entry remains reachable.
    CHECK(ui_text_paginate_spread(5, 7, pages, page_entry, &fixture) == 7);
    fixture = (PageFixture){ { -1, -1, -1, -1, -1, -1, -1, -1 } };
    CHECK(ui_text_paginate_spread(0, 7, pages, page_entry, &fixture) == 7);
    CHECK(fixture.calls == 8 && fixture.first_calls == 8 && fixture.right_calls == 0);
    fixture = (PageFixture){ { -1, 100, -1, -1, -1, -1, -1, -1 } };
    CHECK(ui_text_paginate_spread(0, 7, pages, page_entry, &fixture) == 7);
    CHECK(fixture.right_calls == 0); // First visible entry retains truncation.
    fixture = (PageFixture){ { 6, 6, 6, 6, 6, 6, 6, 6 } };
    CHECK(ui_text_paginate_spread(7, 0, pages, page_entry, &fixture) == 6);
    CHECK(fixture.dry_calls == fixture.calls);
}

int main(void)
{
    test_filtered_pages();
    uint64_t left = UI_CACHE_KEY_INIT, right = UI_CACHE_KEY_INIT;
    ui_cache_key_string(&left, "ab"); ui_cache_key_string(&left, "c");
    ui_cache_key_string(&right, "a"); ui_cache_key_string(&right, "bc");
    CHECK(left != right);
    right = UI_CACHE_KEY_INIT;
    ui_cache_key_string(&right, "ab"); ui_cache_key_string(&right, "c");
    CHECK(left == right);
    unsigned int knowledge = 1;
    ui_cache_key_bytes(&right, &knowledge, sizeof(knowledge));
    CHECK(left != right);

    UiSearchCache cache = { 0 };
    SearchFixture fixture = { 0, { false, true, false, true, false, true, false, false } };
    bool rebuilt = false;
    CHECK(ui_search_cache_select(&cache, 1, 1) == -1);
    CHECK(ui_search_cache_update(&cache, 100, "tool", 8, visible, &fixture, &rebuilt));
    CHECK(rebuilt && fixture.calls == 8 && cache.count == 3);
    CHECK(cache.indices[0] == 1 && cache.indices[1] == 3 && cache.indices[2] == 5);
    int* allocation = cache.indices;
    CHECK(ui_search_cache_update(&cache, 100, "tool", 8, visible, &fixture, &rebuilt));
    CHECK(!rebuilt && fixture.calls == 8 && cache.indices == allocation);
    CHECK(ui_search_cache_select(&cache, 3, 1) == 5);
    CHECK(ui_search_cache_select(&cache, 5, 1) == 1);
    CHECK(ui_search_cache_select(&cache, 1, -1) == 5);
    CHECK(ui_search_cache_select(&cache, 5, 0) == 1);
    CHECK(ui_search_cache_select(&cache, 99, -1) == 1);
    CHECK(ui_search_cache_select(&cache, 1, INT_MIN) >= 0);
    CHECK(ui_search_cache_select(&cache, 1, INT_MAX) >= 0);
    CHECK(ui_search_cache_update(&cache, 100, "another query", 8, visible, &fixture, &rebuilt));
    CHECK(rebuilt && fixture.calls == 16 && cache.indices == allocation);
    fixture.visible[3] = false;
    CHECK(ui_search_cache_update(&cache, 101, cache.query, 8, visible, &fixture, &rebuilt));
    CHECK(rebuilt && fixture.calls == 24 && cache.count == 2);
    CHECK(ui_search_cache_select(&cache, 1, 1) == 5);
    CHECK(ui_search_cache_update(&cache, 101, cache.query, 2, visible, &fixture, &rebuilt));
    CHECK(rebuilt && fixture.calls == 26 && cache.count == 1 && cache.capacity == 8);
    CHECK(ui_search_cache_update(&cache, 101, "empty", 0, visible, &fixture, &rebuilt));
    CHECK(rebuilt && cache.count == 0 && ui_search_cache_select(&cache, 0, 1) == -1);
    CHECK(ui_search_cache_update(&cache, 101, "empty", 0, visible, &fixture, &rebuilt));
    CHECK(!rebuilt && fixture.calls == 26);
    CHECK(!ui_search_cache_update(&cache, 101, NULL, 8, visible, &fixture, &rebuilt));
    CHECK(!cache.valid && cache.count == 0);
    CHECK(!ui_search_cache_update(&cache, 101, "x", -1, visible, &fixture, &rebuilt));
    CHECK(!ui_search_cache_update(&cache, 101, "x", 8, NULL, &fixture, &rebuilt));
    ui_search_cache_clear(&cache);
    CHECK(!cache.valid && cache.indices == NULL && cache.query == NULL && cache.capacity == 0);
    ui_search_cache_clear(&cache);
    CHECK(ui_search_cache_update(&cache, 101, "reload", 8, visible, &fixture, NULL));
    CHECK(cache.count == 2);
    ui_search_cache_clear(&cache);
    printf("UI cache regressions: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures != 0;
}
