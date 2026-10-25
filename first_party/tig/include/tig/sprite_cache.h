#ifndef TIG_SPRITE_CACHE_H_
#define TIG_SPRITE_CACHE_H_

#include <SDL3/SDL.h>

typedef struct TigSpriteCache TigSpriteCache;

typedef struct TigSpriteCacheStats {
    Uint64 hits;
    Uint64 uploads;
    Uint64 uploaded_bytes;
    Uint64 evictions;
    size_t resident_bytes;
    int resident_entries;
} TigSpriteCacheStats;

TigSpriteCache* tig_sprite_cache_create(SDL_Renderer* renderer, int capacity, size_t budget);
void tig_sprite_cache_destroy(TigSpriteCache* cache);
void tig_sprite_cache_clear(TigSpriteCache* cache);
SDL_Texture* tig_sprite_cache_get(TigSpriteCache* cache, Uint64 key, Uint64 revision,
    SDL_Surface* surface, bool color_key_enabled, Uint32 color_key);
TigSpriteCacheStats tig_sprite_cache_stats(const TigSpriteCache* cache);

#endif
