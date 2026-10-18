#include "game/world_composition.h"
#include <tig/scale_region.h>
#include <stdio.h>

struct TigVideoBuffer {
    SDL_Surface* surface;
};

static int failures;
static bool reject_region;
static int full_blits;

#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #value); failures++; } } while (0)

int tig_video_buffer_scale_region(TigVideoBufferBlitInfo* info, const TigRect* region)
{
    if (reject_region) return TIG_ERR_GENERIC;
    SDL_Rect source = { info->src_rect->x, info->src_rect->y, info->src_rect->width, info->src_rect->height };
    SDL_Rect destination = { info->dst_rect->x, info->dst_rect->y, info->dst_rect->width, info->dst_rect->height };
    SDL_Rect clipped = { region->x, region->y, region->width, region->height };
    return tig_scale_region(info->src_video_buffer->surface, source, info->dst_video_buffer->surface,
        destination, clipped, info->flags ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST) ? TIG_OK : TIG_ERR_GENERIC;
}

int tig_video_buffer_blit(TigVideoBufferBlitInfo* info)
{
    full_blits++;
    SDL_Rect source = { info->src_rect->x, info->src_rect->y, info->src_rect->width, info->src_rect->height };
    SDL_Rect destination = { info->dst_rect->x, info->dst_rect->y, info->dst_rect->width, info->dst_rect->height };
    if (source.w == destination.w && source.h == destination.h) {
        return SDL_BlitSurface(info->src_video_buffer->surface, &source,
            info->dst_video_buffer->surface, &destination) ? TIG_OK : TIG_ERR_GENERIC;
    }
    return SDL_BlitSurfaceScaled(info->src_video_buffer->surface, &source, info->dst_video_buffer->surface,
        &destination, info->flags ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST) ? TIG_OK : TIG_ERR_GENERIC;
}

void tig_video_buffer_color_grade(TigVideoBuffer* buffer, const TigVideoUpload* damage)
{
    (void)buffer;
    (void)damage;
}

static Uint32 pixel(SDL_Surface* surface, int x, int y)
{
    return *(Uint32*)((Uint8*)surface->pixels + y * surface->pitch + x * sizeof(Uint32));
}

static void test_world_and_overlay(void)
{
    TigVideoBuffer world = { SDL_CreateSurface(64, 48, SDL_PIXELFORMAT_XRGB8888) };
    TigVideoBuffer output = { SDL_CreateSurface(32, 24, SDL_PIXELFORMAT_XRGB8888) };
    CHECK(world.surface && output.surface);
    if (!world.surface || !output.surface) return;
    SDL_FillSurfaceRect(world.surface, NULL, 0x123456);
    TigRect viewport = { 0, 0, 32, 24 };
    TigVideoUpload damage;
    tig_video_upload_reset(&damage, 32, 24);
    CHECK(world_composition_present(&world, viewport, &output, viewport, &damage, false) == TIG_OK);
    SDL_Rect text = { 3, 4, 7, 5 };
    SDL_FillSurfaceRect(output.surface, &text, 0xFFFFFF);
    CHECK(pixel(world.surface, 4, 5) == 0x123456);
    CHECK(pixel(output.surface, 4, 5) == 0xFFFFFF);
    SDL_Rect change = { 20, 18, 2, 2 };
    SDL_FillSurfaceRect(world.surface, &change, 0x654321);
    memset(&damage, 0, sizeof(damage));
    damage.bounds = (SDL_Rect){ 0, 0, 32, 24 };
    tig_video_upload_mark(&damage, change);
    CHECK(world_composition_present(&world, viewport, &output, viewport, &damage, false) == TIG_OK);
    CHECK(pixel(output.surface, 4, 5) == 0xFFFFFF);
    CHECK(pixel(output.surface, 20, 18) == 0x654321);
    tig_video_upload_mark(&damage, text);
    CHECK(world_composition_present(&world, viewport, &output, viewport, &damage, false) == TIG_OK);
    CHECK(pixel(output.surface, 4, 5) == 0x123456);
    reject_region = true;
    CHECK(world_composition_present(&world, viewport, &output, viewport, &damage, false) == TIG_OK);
    CHECK(damage.count == 1 && damage.rects[0].w == 32 && damage.rects[0].h == 24);
    reject_region = false;
    int before = full_blits;
    damage.count = 0;
    CHECK(world_composition_present(&world, viewport, &output, viewport, &damage, false) == TIG_OK);
    CHECK(before == full_blits);
    CHECK(world_composition_present(&world, viewport, &world, viewport, &damage, false) == TIG_ERR_INVALID_PARAM);
    SDL_DestroySurface(output.surface);
    SDL_DestroySurface(world.surface);
}

static void test_scroll(void)
{
    TigVideoBuffer world = { SDL_CreateSurface(64, 48, SDL_PIXELFORMAT_XRGB8888) };
    CHECK(world.surface != NULL);
    if (!world.surface) return;
    for (int dy = -3; dy <= 3; dy += 3) {
        for (int dx = -5; dx <= 5; dx += 5) {
            for (int y = 0; y < 48; y++) {
                for (int x = 0; x < 64; x++) {
                    *(Uint32*)((Uint8*)world.surface->pixels + y * world.surface->pitch + x * 4) = y * 64 + x;
                }
            }
            CHECK(world_composition_scroll(&world, (TigRect){ 0, 0, 32, 24 }, dx, dy) == TIG_OK);
            for (int y = 0; y < 48; y++) {
                for (int x = 0; x < 64; x++) {
                    bool copied = x < 32 && y < 24 && x - dx >= 0 && x - dx < 32 && y - dy >= 0 && y - dy < 24;
                    Uint32 expected = copied ? (y - dy) * 64 + x - dx : y * 64 + x;
                    CHECK(pixel(world.surface, x, y) == expected);
                }
            }
        }
    }
    CHECK(world_composition_scroll(&world, (TigRect){ 0, 0, 32, 24 }, 32, 0) == TIG_ERR_INVALID_PARAM);
    SDL_DestroySurface(world.surface);
}

static void test_scaled_world(void)
{
    TigVideoBuffer world = { SDL_CreateSurface(64, 48, SDL_PIXELFORMAT_XRGB8888) };
    TigVideoBuffer output = { SDL_CreateSurface(32, 24, SDL_PIXELFORMAT_XRGB8888) };
    SDL_Surface* reference = SDL_CreateSurface(32, 24, SDL_PIXELFORMAT_XRGB8888);
    CHECK(world.surface && output.surface && reference);
    if (!world.surface || !output.surface || !reference) return;
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 64; x++) {
            *(Uint32*)((Uint8*)world.surface->pixels + y * world.surface->pitch + x * 4) = (x * 3 << 16) | (y * 5 << 8);
        }
    }
    for (int linear = 0; linear < 2; linear++) {
        for (int size = 0; size < 3; size++) {
            int width = size == 0 ? 16 : size == 1 ? 32 : 64;
            int height = size == 0 ? 12 : size == 1 ? 24 : 48;
            TigRect source = { (64 - width) / 2, (48 - height) / 2, width, height };
            TigRect destination = { 0, 0, 32, 24 };
            SDL_Rect native_source = { source.x, source.y, width, height };
            SDL_Rect native_destination = { 0, 0, 32, 24 };
            CHECK(SDL_BlitSurfaceScaled(world.surface, &native_source, reference, &native_destination,
                linear ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST));
            TigVideoUpload damage;
            tig_video_upload_reset(&damage, 32, 24);
            CHECK(world_composition_present(&world, source, &output, destination, &damage, linear != 0) == TIG_OK);
            for (int y = 0; y < 24; y++) {
                for (int x = 0; x < 32; x++) CHECK(pixel(output.surface, x, y) == pixel(reference, x, y));
            }
            SDL_Rect overlay = { 4, 5, 6, 4 };
            SDL_FillSurfaceRect(output.surface, &overlay, 0xFFFFFF);
            memset(&damage, 0, sizeof(damage));
            damage.bounds = native_destination;
            tig_video_upload_mark(&damage, overlay);
            CHECK(world_composition_present(&world, source, &output, destination, &damage, linear != 0) == TIG_OK);
            for (int y = 0; y < 24; y++) {
                for (int x = 0; x < 32; x++) CHECK(pixel(output.surface, x, y) == pixel(reference, x, y));
            }
        }
    }
    SDL_DestroySurface(reference);
    SDL_DestroySurface(output.surface);
    SDL_DestroySurface(world.surface);
}

int main(void)
{
    CHECK(SDL_Init(0));
    test_world_and_overlay();
    test_scroll();
    test_scaled_world();
    SDL_Quit();
    return failures ? 1 : 0;
}
