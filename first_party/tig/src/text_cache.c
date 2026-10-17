#include "tig/text_cache.h"

static bool same_key(const TigTextCacheKey* a, const TigTextCacheKey* b)
{
    return a->font == b->font && a->flags == b->flags && a->width == b->width
        && a->dim_percent == b->dim_percent && a->color.r == b->color.r
        && a->color.g == b->color.g && a->color.b == b->color.b && a->color.a == b->color.a;
}

static void erase(TigTextCache* cache, TigTextCacheEntry* entry)
{
    SDL_assert(entry->users == 0);
    cache->bytes -= entry->bytes;
    SDL_DestroySurface(entry->surface);
    SDL_free(entry->text);
    SDL_memset(entry, 0, sizeof(*entry));
}

TigTextLease tig_text_cache_get(TigTextCache* cache, const TigTextCacheKey* key, const char* text)
{
    for (int i = 0; i < TIG_TEXT_CACHE_CAPACITY; i++) {
        TigTextCacheEntry* entry = &cache->entries[i];
        if (entry->surface != NULL && same_key(&entry->key, key) && SDL_strcmp(entry->text, text) == 0) {
            entry->age = ++cache->clock;
            entry->users++;
            return (TigTextLease){ entry->surface, entry };
        }
    }
    return (TigTextLease){ 0 };
}

TigTextLease tig_text_cache_put(TigTextCache* cache, const TigTextCacheKey* key, const char* text, SDL_Surface* surface)
{
    TigTextLease lease = { surface, NULL };
    if (surface == NULL) return lease;
    size_t bytes = (size_t)surface->pitch * surface->h + SDL_strlen(text) + 1 + sizeof(TigTextCacheEntry) + sizeof(SDL_Surface);
    if (bytes > TIG_TEXT_CACHE_BYTES) return lease;
    TigTextCacheEntry* slot = NULL;
    for (;;) {
        TigTextCacheEntry* oldest = NULL;
        for (int i = 0; i < TIG_TEXT_CACHE_CAPACITY; i++) {
            TigTextCacheEntry* entry = &cache->entries[i];
            if (entry->surface == NULL) slot = entry;
            else if (entry->users == 0 && (oldest == NULL || entry->age < oldest->age)) oldest = entry;
        }
        if (slot != NULL && cache->bytes + bytes <= TIG_TEXT_CACHE_BYTES) break;
        if (oldest == NULL) return lease;
        erase(cache, oldest);
        slot = oldest;
    }
    char* copy = SDL_strdup(text);
    if (copy == NULL) return lease;
    *slot = (TigTextCacheEntry){ *key, copy, surface, bytes, ++cache->clock, 1 };
    cache->bytes += bytes;
    lease.entry = slot;
    return lease;
}

void tig_text_cache_release(TigTextLease lease)
{
    if (lease.entry != NULL) {
        SDL_assert(lease.entry->users > 0);
        lease.entry->users--;
    } else if (lease.surface != NULL) SDL_DestroySurface(lease.surface);
}

void tig_text_cache_clear(TigTextCache* cache, const void* font)
{
    for (int i = 0; i < TIG_TEXT_CACHE_CAPACITY; i++) {
        TigTextCacheEntry* entry = &cache->entries[i];
        if (entry->surface != NULL && (font == NULL || entry->key.font == font)) erase(cache, entry);
    }
}
