#include "tig/sprite_cache.h"
#include "tig/pixel_buffer.h"

typedef struct SpriteCacheEntry {
    SDL_Texture* texture;
    Uint64 key;
    Uint64 revision;
    Uint64 last_use;
    int width;
    int height;
    SDL_PixelFormat format;
    bool color_key_enabled;
    Uint32 color_key;
    size_t bytes;
} SpriteCacheEntry;

struct TigSpriteCache {
    SDL_Renderer* renderer;
    SpriteCacheEntry* entries;
    int capacity;
    size_t budget;
    Uint64 clock;
    TigSpriteCacheStats stats;
};

TigSpriteCache* tig_sprite_cache_create(SDL_Renderer* renderer, int capacity, size_t budget)
{
    if (!renderer || capacity <= 0 || budget == 0) return NULL;
    TigSpriteCache* cache = SDL_calloc(1, sizeof(*cache));
    if (!cache) return NULL;
    cache->entries = SDL_calloc(capacity, sizeof(*cache->entries));
    if (!cache->entries) { SDL_free(cache); return NULL; }
    cache->renderer = renderer;
    cache->capacity = capacity;
    cache->budget = budget;
    return cache;
}

static void sprite_cache_remove(TigSpriteCache* cache, SpriteCacheEntry* entry)
{
    SDL_DestroyTexture(entry->texture);
    cache->stats.resident_bytes -= entry->bytes;
    cache->stats.resident_entries--;
    SDL_memset(entry, 0, sizeof(*entry));
}

void tig_sprite_cache_clear(TigSpriteCache* cache)
{
    if (!cache) return;
    for (int i = 0; i < cache->capacity; i++) {
        if (cache->entries[i].texture) sprite_cache_remove(cache, &cache->entries[i]);
    }
}

void tig_sprite_cache_destroy(TigSpriteCache* cache)
{
    if (!cache) return;
    tig_sprite_cache_clear(cache);
    SDL_free(cache->entries);
    SDL_free(cache);
}

TigSpriteCacheStats tig_sprite_cache_stats(const TigSpriteCache* cache)
{
    return cache ? cache->stats : (TigSpriteCacheStats){ 0 };
}

SDL_Texture* tig_sprite_cache_get(TigSpriteCache* cache, Uint64 key, Uint64 revision,
    SDL_Surface* surface, bool color_key_enabled, Uint32 color_key)
{
    if (!cache || !surface || surface->w <= 0 || surface->h <= 0
        || SDL_BYTESPERPIXEL(surface->format) != 4) {
        SDL_SetError("Invalid GPU sprite cache or 32-bit image");
        return NULL;
    }
    size_t storage_bytes;
    size_t bytes;
    int pitch;
    if (surface->pixels == NULL
        || !tig_pixel_buffer_size(surface->w, surface->h, 32, surface->pitch, &storage_bytes)
        || !tig_pixel_buffer_row_size(surface->w, 32, &pitch)
        || !tig_pixel_buffer_size(surface->w, surface->h, 32, pitch, &bytes)
        || bytes > cache->budget) {
        SDL_SetError("Sprite image exceeds the GPU cache pixel budget");
        return NULL;
    }
    SpriteCacheEntry* slot = NULL;
    for (int i = 0; i < cache->capacity; i++) {
        SpriteCacheEntry* entry = &cache->entries[i];
        if (!entry->texture) { if (!slot) slot = entry; continue; }
        if (entry->key != key) continue;
        if (entry->revision == revision && entry->width == surface->w && entry->height == surface->h
            && entry->format == surface->format && entry->color_key_enabled == color_key_enabled
            && (!color_key_enabled || entry->color_key == color_key)) {
            entry->last_use = ++cache->clock;
            cache->stats.hits++;
            return entry->texture;
        }
        sprite_cache_remove(cache, entry);
        slot = entry;
        break;
    }
    while (!slot || cache->stats.resident_bytes > cache->budget - bytes) {
        SpriteCacheEntry* oldest = NULL;
        for (int i = 0; i < cache->capacity; i++) {
            SpriteCacheEntry* entry = &cache->entries[i];
            if (entry->texture && (!oldest || entry->last_use < oldest->last_use)) oldest = entry;
        }
        if (!oldest) return NULL;
        sprite_cache_remove(cache, oldest);
        cache->stats.evictions++;
        if (!slot) slot = oldest;
    }
    SDL_Surface* upload = SDL_CreateSurface(surface->w, surface->h, SDL_PIXELFORMAT_ARGB8888);
    if (!upload) return NULL;
    if (!SDL_ConvertPixels(surface->w, surface->h, surface->format, surface->pixels, surface->pitch,
            upload->format, upload->pixels, upload->pitch)) {
        SDL_DestroySurface(upload);
        return NULL;
    }
    if (color_key_enabled) {
        Uint8 r, g, b, a;
        SDL_GetRGBA(color_key, SDL_GetPixelFormatDetails(surface->format), NULL, &r, &g, &b, &a);
        Uint32 key_rgb = ((Uint32)r << 16) | ((Uint32)g << 8) | b;
        for (int y = 0; y < upload->h; y++) {
            Uint32* row = (Uint32*)((Uint8*)upload->pixels + y * upload->pitch);
            for (int x = 0; x < upload->w; x++) {
                row[x] = (row[x] & 0xFFFFFF) | ((row[x] & 0xFFFFFF) == key_rgb ? 0 : 0xFF000000);
            }
        }
        SDL_SetSurfaceColorKey(upload, false, 0);
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(cache->renderer, upload);
    SDL_DestroySurface(upload);
    if (!texture) return NULL;
    if (!SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND)
        || !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST)) {
        SDL_DestroyTexture(texture);
        return NULL;
    }
    *slot = (SpriteCacheEntry){ texture, key, revision, ++cache->clock,
        surface->w, surface->h, surface->format, color_key_enabled, color_key, bytes };
    cache->stats.resident_bytes += bytes;
    cache->stats.resident_entries++;
    cache->stats.uploads++;
    cache->stats.uploaded_bytes += bytes;
    return texture;
}
