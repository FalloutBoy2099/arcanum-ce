#include <stdio.h>
#include <stdlib.h>
#include <tig/sprite_cache.h>

static int failures;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL line %d: %s (%s)\n", __LINE__, #expr, SDL_GetError()); failures++; } } while (0)

static Uint32 pixel(SDL_Surface* image, int x, int y)
{
    return *(Uint32*)((Uint8*)image->pixels + image->pitch * y + x * 4) & 0xFFFFFF;
}

static SDL_Surface* capture(SDL_Renderer* renderer)
{
    SDL_Surface* raw = SDL_RenderReadPixels(renderer, NULL);
    SDL_Surface* image = raw ? SDL_ConvertSurface(raw, SDL_PIXELFORMAT_XRGB8888) : NULL;
    SDL_DestroySurface(raw);
    CHECK(image != NULL);
    return image;
}

static void background(SDL_Renderer* renderer)
{
    CHECK(SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255));
    CHECK(SDL_RenderClear(renderer));
}

static void verify_pixels(SDL_Renderer* renderer, TigSpriteCache* cache, SDL_Surface* source)
{
    SDL_FillSurfaceRect(source, NULL, 0x00FF00);
    SDL_Rect red = { 1, 1, 2, 2 };
    SDL_FillSurfaceRect(source, &red, 0xFF0000);
    SDL_Texture* texture = tig_sprite_cache_get(cache, 1, 1, source, true, 0x00FF00);
    CHECK(texture != NULL);
    background(renderer);
    SDL_FRect destination = { 1, 1, 4, 4 };
    CHECK(SDL_RenderTexture(renderer, texture, NULL, &destination));
    SDL_Surface* image = capture(renderer);
    if (image) {
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) CHECK(pixel(image, x, y) == (x >= 2 && x < 4 && y >= 2 && y < 4 ? 0xFF0000 : 0x0000FF));
        }
    }
    SDL_DestroySurface(image);
    CHECK(tig_sprite_cache_get(cache, 1, 1, source, true, 0x00FF00) == texture);
    CHECK(tig_sprite_cache_stats(cache).uploads == 1);
    CHECK(tig_sprite_cache_stats(cache).hits == 1);

    SDL_FillSurfaceRect(source, &red, 0xFFFFFF);
    texture = tig_sprite_cache_get(cache, 1, 2, source, true, 0x00FF00);
    background(renderer);
    SDL_FRect cropped_source = { 1, 1, 2, 2 };
    SDL_FRect cropped_destination = { 0, 0, 2, 2 };
    CHECK(SDL_RenderTexture(renderer, texture, &cropped_source, &cropped_destination));
    image = capture(renderer);
    if (image) {
        CHECK(pixel(image, 0, 0) == 0xFFFFFF);
        CHECK(pixel(image, 2, 0) == 0x0000FF);
    }
    SDL_DestroySurface(image);
    CHECK(tig_sprite_cache_stats(cache).uploads == 2);

    texture = tig_sprite_cache_get(cache, 1, 2, source, false, 0);
    background(renderer);
    CHECK(SDL_RenderTexture(renderer, texture, NULL, &destination));
    image = capture(renderer);
    if (image) CHECK(pixel(image, 1, 1) == 0x00FF00);
    SDL_DestroySurface(image);
    CHECK(tig_sprite_cache_stats(cache).uploads == 3);
}

static void verify_lru(SDL_Renderer* renderer, SDL_Surface* source)
{
    TigSpriteCache* cache = tig_sprite_cache_create(renderer, 2, 128);
    CHECK(cache != NULL);
    CHECK(tig_sprite_cache_get(cache, 1, 1, source, false, 0) != NULL);
    CHECK(tig_sprite_cache_get(cache, 2, 1, source, false, 0) != NULL);
    CHECK(tig_sprite_cache_get(cache, 1, 1, source, false, 0) != NULL);
    CHECK(tig_sprite_cache_get(cache, 3, 1, source, false, 0) != NULL);
    TigSpriteCacheStats stats = tig_sprite_cache_stats(cache);
    CHECK(stats.uploads == 3 && stats.hits == 1 && stats.evictions == 1);
    CHECK(stats.resident_entries == 2 && stats.resident_bytes == 128);
    CHECK(tig_sprite_cache_get(cache, 1, 1, source, false, 0) != NULL);
    CHECK(tig_sprite_cache_stats(cache).uploads == 3);
    CHECK(tig_sprite_cache_get(cache, 2, 1, source, false, 0) != NULL);
    CHECK(tig_sprite_cache_stats(cache).uploads == 4);
    SDL_Surface* large = SDL_CreateSurface(8, 8, SDL_PIXELFORMAT_XRGB8888);
    CHECK(tig_sprite_cache_get(cache, 4, 1, large, false, 0) == NULL);
    CHECK(tig_sprite_cache_stats(cache).resident_bytes == 128);
    SDL_DestroySurface(large);
    tig_sprite_cache_clear(cache);
    CHECK(tig_sprite_cache_stats(cache).resident_entries == 0);
    CHECK(tig_sprite_cache_stats(cache).resident_bytes == 0);
    CHECK(tig_sprite_cache_get(cache, 1, 1, source, false, 0) != NULL);
    CHECK(tig_sprite_cache_stats(cache).uploads == 5);
    tig_sprite_cache_destroy(cache);

    cache = tig_sprite_cache_create(renderer, 8, 64);
    CHECK(tig_sprite_cache_get(cache, 1, 1, source, false, 0) != NULL);
    CHECK(tig_sprite_cache_get(cache, 2, 1, source, false, 0) != NULL);
    CHECK(tig_sprite_cache_stats(cache).resident_entries == 1);
    CHECK(tig_sprite_cache_stats(cache).evictions == 1);
    tig_sprite_cache_destroy(cache);
}

static void verify_animation(SDL_Renderer* renderer, SDL_Surface* source)
{
    TigSpriteCache* cache = tig_sprite_cache_create(renderer, 16, 1024);
    for (int pass = 0; pass < 10; pass++) {
        for (int frame = 0; frame < 16; frame++) {
            SDL_FillSurfaceRect(source, NULL, 0x101010 * frame);
            CHECK(tig_sprite_cache_get(cache, frame, frame, source, false, 0) != NULL);
        }
    }
    TigSpriteCacheStats stats = tig_sprite_cache_stats(cache);
    CHECK(stats.uploads == 16 && stats.hits == 144 && stats.evictions == 0);
    CHECK(stats.uploaded_bytes == 1024 && stats.resident_bytes == 1024);
    printf("Animation cache: %llu uploads, %llu hits, %llu transferred bytes\n",
        (unsigned long long)stats.uploads, (unsigned long long)stats.hits, (unsigned long long)stats.uploaded_bytes);
    tig_sprite_cache_destroy(cache);
}

static void verify_composition(SDL_Renderer* renderer)
{
    TigSpriteCache* cache = tig_sprite_cache_create(renderer, 8, 4096);
    SDL_Surface* source = SDL_CreateSurface(4, 4, SDL_PIXELFORMAT_ARGB8888);
    SDL_FillSurfaceRect(source, NULL, 0xFFFF0000);
    SDL_Texture* red = tig_sprite_cache_get(cache, 1, 1, source, false, 0);
    SDL_FillSurfaceRect(source, NULL, 0xFF00FF00);
    SDL_Texture* green = tig_sprite_cache_get(cache, 2, 1, source, false, 0);
    SDL_FRect first = { 1, 1, 4, 4 };
    SDL_FRect second = { 2, 2, 4, 4 };
    background(renderer);
    CHECK(SDL_RenderTexture(renderer, red, NULL, &first));
    CHECK(SDL_RenderTexture(renderer, green, NULL, &second));
    SDL_Surface* image = capture(renderer);
    if (image) {
        CHECK(pixel(image, 1, 1) == 0xFF0000);
        CHECK(pixel(image, 2, 2) == 0x00FF00);
        CHECK(pixel(image, 0, 0) == 0x0000FF);
    }
    SDL_DestroySurface(image);
    background(renderer);
    CHECK(SDL_RenderTexture(renderer, green, NULL, &second));
    CHECK(SDL_RenderTexture(renderer, red, NULL, &first));
    image = capture(renderer);
    if (image) CHECK(pixel(image, 2, 2) == 0xFF0000);
    SDL_DestroySurface(image);

    SDL_FillSurfaceRect(source, NULL, 0x80FF0000);
    SDL_Texture* half = tig_sprite_cache_get(cache, 3, 1, source, false, 0);
    background(renderer);
    CHECK(SDL_RenderTexture(renderer, half, NULL, &first));
    image = capture(renderer);
    if (image) {
        Uint32 mixed = pixel(image, 2, 2);
        CHECK(abs((int)((mixed >> 16) & 255) - 128) <= 1);
        CHECK(abs((int)(mixed & 255) - 127) <= 1);
        CHECK(((mixed >> 8) & 255) == 0);
    }
    SDL_DestroySurface(image);

    SDL_FillSurfaceRect(source, NULL, 0xFFFF0000);
    SDL_Rect right = { 2, 0, 2, 4 };
    SDL_FillSurfaceRect(source, &right, 0xFF00FF00);
    SDL_Texture* split = tig_sprite_cache_get(cache, 4, 1, source, false, 0);
    background(renderer);
    SDL_FRect negative = { -1, -1, 4, 4 };
    CHECK(SDL_RenderTextureRotated(renderer, split, NULL, &negative, 0, NULL, SDL_FLIP_HORIZONTAL));
    image = capture(renderer);
    if (image) {
        CHECK(pixel(image, 0, 0) == 0x00FF00);
        CHECK(pixel(image, 1, 0) == 0xFF0000);
        CHECK(pixel(image, 3, 0) == 0x0000FF);
    }
    SDL_DestroySurface(image);
    SDL_DestroySurface(source);
    tig_sprite_cache_destroy(cache);
}

int main(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) return 77;
    SDL_GPUDevice* device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL, false, "direct3d12");
    SDL_Renderer* renderer = device ? SDL_CreateGPURenderer(device, NULL) : NULL;
    if (!renderer) {
        SDL_Log("GPU unavailable: %s", SDL_GetError());
        SDL_DestroyGPUDevice(device);
        SDL_Quit();
        return 77;
    }
    SDL_Texture* target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_TARGET, 8, 8);
    SDL_Surface* source = SDL_CreateSurface(4, 4, SDL_PIXELFORMAT_XRGB8888);
    if (!target || !source) return 1;
    CHECK(SDL_SetRenderTarget(renderer, target));
    TigSpriteCache* cache = tig_sprite_cache_create(renderer, 16, 4096);
    verify_pixels(renderer, cache, source);
    tig_sprite_cache_destroy(cache);
    verify_lru(renderer, source);
    verify_animation(renderer, source);
    verify_composition(renderer);
    SDL_SetRenderTarget(renderer, NULL);
    SDL_DestroySurface(source);
    SDL_DestroyTexture(target);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyGPUDevice(device);
    SDL_Quit();
    printf("GPU sprite cache: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures != 0;
}
