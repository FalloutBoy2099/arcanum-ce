#ifndef TIG_TEXT_CACHE_H_
#define TIG_TEXT_CACHE_H_

#include <SDL3/SDL.h>

#define TIG_TEXT_CACHE_CAPACITY 128
#define TIG_TEXT_CACHE_BYTES (8 * 1024 * 1024)

typedef struct TigTextCacheKey {
    const void* font;
    Uint32 flags;
    int width;
    SDL_Color color;
    int dim_percent;
} TigTextCacheKey;

typedef struct TigTextCacheEntry {
    TigTextCacheKey key;
    char* text;
    SDL_Surface* surface;
    size_t bytes;
    Uint64 age;
    unsigned int users;
} TigTextCacheEntry;

typedef struct TigTextCache {
    TigTextCacheEntry entries[TIG_TEXT_CACHE_CAPACITY];
    size_t bytes;
    Uint64 clock;
} TigTextCache;

typedef struct TigTextLease {
    SDL_Surface* surface;
    TigTextCacheEntry* entry;
} TigTextLease;

// Leases pin surfaces while the caller draws another cached layer (e.g. shadow).
TigTextLease tig_text_cache_get(TigTextCache* cache, const TigTextCacheKey* key, const char* text);
// Takes ownership of surface; oversized entries remain valid until release.
TigTextLease tig_text_cache_put(TigTextCache* cache, const TigTextCacheKey* key, const char* text, SDL_Surface* surface);
void tig_text_cache_release(TigTextLease lease);
// Call outside rendering, before closing the font. NULL clears all fonts.
void tig_text_cache_clear(TigTextCache* cache, const void* font);

#endif
