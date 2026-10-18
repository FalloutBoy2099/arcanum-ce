#include "tig/scale_region.h"
#include "game/zoom_damage.h"
#include <stdio.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)

static void test_damage(void)
{
    SDL_Surface* src = SDL_CreateSurface(80, 66, SDL_PIXELFORMAT_XRGB8888);
    SDL_Surface* reference = SDL_CreateSurface(83, 71, SDL_PIXELFORMAT_XRGB8888);
    SDL_Surface* actual = SDL_CreateSurface(83, 71, SDL_PIXELFORMAT_XRGB8888);
    CHECK(src != NULL && reference != NULL && actual != NULL);
    if (src == NULL || reference == NULL || actual == NULL) return;
    TigRect source = { 4, 3, 64, 52 }, destination = { 0, 0, 83, 71 };
    SDL_Rect s = { 4, 3, 64, 52 }, d = { 0, 0, 83, 71 };
    unsigned int random = 493;
    for (int mode = 0; mode < 2; mode++) {
        SDL_ScaleMode filter = mode ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST;
        SDL_FillSurfaceRect(src, NULL, 0x123456);
        SDL_BlitSurfaceScaled(src, &s, actual, &d, filter);
        ZoomDamageState state = { 0 };
        ZoomMapping mapping = { source, destination, src, actual, 100, 200, mode != 0 };
        TigVideoUpload plan;
        zoom_damage_init(&plan, 83, 71);
        zoom_damage_finish(&plan, &state, &mapping);
        CHECK(plan.count == 1 && plan.rects[0].w == 83 && plan.rects[0].h == 71);
        for (int run = 0; run < 120; run++) {
            random = random * 1664525U + 1013904223U;
            SDL_Rect changed = { (int)(random % 88) - 4, (int)((random >> 8) % 74) - 4, 3 + run % 9, 2 + run % 7 };
            SDL_FillSurfaceRect(src, &changed, random);
            CHECK(SDL_BlitSurfaceScaled(src, &s, reference, &d, filter));
            zoom_damage_init(&plan, 83, 71);
            zoom_damage_world(&plan, source, destination, (TigRect){ changed.x, changed.y, changed.w, changed.h });
            zoom_damage_finish(&plan, &state, &mapping);
            for (int i = 0; i < plan.count; i++) CHECK(tig_scale_region(src, s, actual, d, plan.rects[i], filter));
            bool same = true;
            for (int y = 0; y < 71; y++) if (SDL_memcmp((Uint8*)actual->pixels + y * actual->pitch,
                (Uint8*)reference->pixels + y * reference->pitch, 83 * 4) != 0) same = false;
            CHECK(same);
        }
        zoom_damage_init(&plan, 83, 71);
        zoom_damage_finish(&plan, &state, &mapping);
        CHECK(plan.count == 0); // Nothing changed: no scaling work.
        mapping.camera_x++;
        zoom_damage_finish(&plan, &state, &mapping);
        CHECK(plan.count == 1 && plan.rects[0].w == 83);
        zoom_damage_init(&plan, 83, 71);
        mapping.linear = !mapping.linear;
        zoom_damage_finish(&plan, &state, &mapping);
        CHECK(plan.count == 1 && plan.rects[0].h == 71);
        zoom_damage_init(&plan, 83, 71);
        mapping.source_buffer = reference;
        zoom_damage_finish(&plan, &state, &mapping);
        CHECK(plan.count == 1 && plan.rects[0].w == 83);
        zoom_damage_init(&plan, 83, 71);
        zoom_damage_screen(&plan, (TigRect){ 0, 0, 83, 20 });
        zoom_damage_finish(&plan, &state, &mapping);
        CHECK(plan.count == 1 && plan.rects[0].h == 71); // 25% threshold.
    }
    SDL_DestroySurface(actual);
    SDL_DestroySurface(reference);
    SDL_DestroySurface(src);
}

int main(void)
{
    CHECK(SDL_Init(0));
    test_damage();
    unsigned int random = 12345;
    for (int test = 0; test < 160; test++) {
        int sw = 2 + test * 17 % 131, sh = 2 + test * 13 % 97;
        int dw = 2 + test * 11 % 151, dh = 2 + test * 7 % 111;
        SDL_Surface* src = SDL_CreateSurface(sw + 12, sh + 10, SDL_PIXELFORMAT_XRGB8888);
        SDL_Surface* reference = SDL_CreateSurface(dw + 10, dh + 8, SDL_PIXELFORMAT_XRGB8888);
        SDL_Surface* actual = SDL_CreateSurface(dw + 10, dh + 8, SDL_PIXELFORMAT_XRGB8888);
        CHECK(src != NULL && reference != NULL && actual != NULL);
        if (src == NULL || reference == NULL || actual == NULL) return 1;
        for (int y = 0; y < src->h; y++) for (int x = 0; x < src->w; x++) {
            random = random * 1664525U + 1013904223U;
            *(Uint32*)((Uint8*)src->pixels + y * src->pitch + x * 4) = random;
        }
        SDL_Rect source = { 5, 3, sw, sh }, destination = { 3, 2, dw, dh };
        for (int mode = 0; mode < 2; mode++) {
            SDL_ScaleMode filter = mode ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST;
            SDL_FillSurfaceRect(reference, NULL, 0x12345678);
            SDL_FillSurfaceRect(actual, NULL, 0x12345678);
            CHECK(SDL_BlitSurfaceScaled(src, &source, reference, &destination, filter));
            // Partition destination with unaligned edges; every subregion must
            // use the full transformation, not rescale a cropped source.
            for (int y = destination.y; y < destination.y + dh; y += 11) {
                for (int x = destination.x; x < destination.x + dw; x += 7) {
                    SDL_Rect region = { x, y, SDL_min(7, destination.x + dw - x), SDL_min(11, destination.y + dh - y) };
                    CHECK(tig_scale_region(src, source, actual, destination, region, filter));
                }
            }
            bool same = true;
            for (int y = 0; y < actual->h; y++) {
                if (SDL_memcmp((Uint8*)actual->pixels + y * actual->pitch,
                    (Uint8*)reference->pixels + y * reference->pitch, actual->w * 4) != 0) same = false;
            }
            CHECK(same);
            SDL_Rect clip = { 4, 3, SDL_max(1, dw / 2), SDL_max(1, dh / 2) };
            SDL_SetSurfaceClipRect(actual, &clip);
            SDL_Rect outside = { -40, -30, 1, 1 };
            CHECK(tig_scale_region(src, source, actual, destination, outside, filter));
            SDL_SetSurfaceClipRect(actual, NULL);
        }
        SDL_DestroySurface(actual);
        SDL_DestroySurface(reference);
        SDL_DestroySurface(src);
    }
    SDL_Surface* special = SDL_CreateSurface(8, 8, SDL_PIXELFORMAT_XRGB8888);
    SDL_Surface* target = SDL_CreateSurface(8, 8, SDL_PIXELFORMAT_XRGB8888);
    SDL_Rect whole = { 0, 0, 8, 8 };
    SDL_SetSurfaceColorMod(special, 100, 255, 255);
    CHECK(!tig_scale_region(special, whole, target, whole, whole, SDL_SCALEMODE_LINEAR));
    SDL_SetSurfaceColorMod(special, 255, 255, 255);
    SDL_SetSurfaceBlendMode(special, SDL_BLENDMODE_BLEND);
    CHECK(!tig_scale_region(special, whole, target, whole, whole, SDL_SCALEMODE_LINEAR));
    SDL_DestroySurface(target);
    SDL_DestroySurface(special);
    SDL_Quit();
    printf("Partial scaling: 320 pixel-exact transformations; %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
