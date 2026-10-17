#include "tig/text_cache.h"
#include <stdio.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)

int main(void)
{
    TigTextCache cache = { 0 };
    int owner, other;
    TigTextCacheKey key = { &owner, 0, 100, { 255, 0, 0, 255 }, 100 };
    CHECK(tig_text_cache_get(&cache, &key, "one").surface == NULL);
    TigTextLease pinned = tig_text_cache_put(&cache, &key, "one", SDL_CreateSurface(16, 16, SDL_PIXELFORMAT_ARGB8888));
    CHECK(pinned.surface != NULL && pinned.entry != NULL);
    for (int i = 0; i < 200; i++) {
        char text[32]; SDL_snprintf(text, sizeof(text), "%d", i);
        TigTextLease item = tig_text_cache_put(&cache, &key, text, SDL_CreateSurface(512, 512, SDL_PIXELFORMAT_ARGB8888));
        CHECK(item.surface != NULL);
        tig_text_cache_release(item);
        CHECK(cache.bytes <= TIG_TEXT_CACHE_BYTES);
    }
    TigTextLease hit = tig_text_cache_get(&cache, &key, "one");
    CHECK(hit.surface == pinned.surface);
    tig_text_cache_release(hit);
    TigTextCacheKey changed = key;
    changed.width++; CHECK(tig_text_cache_get(&cache, &changed, "one").surface == NULL);
    changed = key; changed.flags++; CHECK(tig_text_cache_get(&cache, &changed, "one").surface == NULL);
    changed = key; changed.color.a--; CHECK(tig_text_cache_get(&cache, &changed, "one").surface == NULL);
    changed = key; changed.dim_percent--; CHECK(tig_text_cache_get(&cache, &changed, "one").surface == NULL);
    changed = key; changed.font = &other; CHECK(tig_text_cache_get(&cache, &changed, "one").surface == NULL);
    tig_text_cache_release(pinned);
    tig_text_cache_clear(&cache, &owner);
    CHECK(cache.bytes == 0 && tig_text_cache_get(&cache, &key, "one").surface == NULL);
    TigTextLease huge = tig_text_cache_put(&cache, &key, "huge", SDL_CreateSurface(2048, 1200, SDL_PIXELFORMAT_ARGB8888));
    CHECK(huge.surface != NULL && huge.entry == NULL && cache.bytes == 0);
    tig_text_cache_release(huge);
    TigTextLease all[TIG_TEXT_CACHE_CAPACITY];
    for (int i = 0; i < TIG_TEXT_CACHE_CAPACITY; i++) {
        char text[32]; SDL_snprintf(text, sizeof(text), "%d", i);
        all[i] = tig_text_cache_put(&cache, &key, text, SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_ARGB8888));
        CHECK(all[i].entry != NULL);
    }
    TigTextLease overflow = tig_text_cache_put(&cache, &key, "overflow", SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_ARGB8888));
    CHECK(overflow.surface != NULL && overflow.entry == NULL);
    tig_text_cache_release(overflow);
    for (int i = 0; i < TIG_TEXT_CACHE_CAPACITY; i++) tig_text_cache_release(all[i]);
    tig_text_cache_clear(&cache, NULL);
    CHECK(cache.bytes == 0);
    printf("Text cache: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
