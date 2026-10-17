#include "tig/video_upload.h"
#include <stdio.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)

static void compare(SDL_Renderer* renderer, SDL_Texture* texture, SDL_Surface* source)
{
    CHECK(SDL_RenderTexture(renderer, texture, NULL, NULL));
    SDL_Surface* actual = SDL_RenderReadPixels(renderer, NULL);
    CHECK(actual != NULL);
    if (actual == NULL) return;
    SDL_Surface* converted = SDL_ConvertSurface(actual, source->format);
    CHECK(converted != NULL);
    if (converted != NULL) {
        for (int y = 0; y < source->h; y++) {
            for (int x = 0; x < source->w; x++) {
                Uint32 expected = *(Uint32*)((Uint8*)source->pixels + y * source->pitch + x * 4);
                Uint32 received = *(Uint32*)((Uint8*)converted->pixels + y * converted->pitch + x * 4);
                CHECK((expected & 0xFFFFFF) == (received & 0xFFFFFF));
            }
        }
        SDL_DestroySurface(converted);
    }
    SDL_DestroySurface(actual);
}

int main(void)
{
    CHECK(SDL_Init(0));
    SDL_Surface* target = SDL_CreateSurface(100, 80, SDL_PIXELFORMAT_XRGB8888);
    SDL_Surface* source = SDL_CreateSurface(100, 80, SDL_PIXELFORMAT_XRGB8888);
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(target);
    CHECK(target != NULL && source != NULL && renderer != NULL);
    if (target == NULL || source == NULL || renderer == NULL) return 1;
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, 100, 80);
    CHECK(texture != NULL);
    TigVideoUpload upload;
    tig_video_upload_reset(&upload, 100, 80);
    SDL_FillSurfaceRect(source, NULL, 0x123456);
    Uint64 bytes;
    CHECK(tig_video_upload_flush(&upload, texture, source, &bytes));
    CHECK(bytes == 100 * 80 * 4 && upload.count == 0);
    compare(renderer, texture, source);
    CHECK(tig_video_upload_flush(&upload, texture, source, &bytes));
    CHECK(bytes == 0);
    unsigned int random = 917;
    for (int run = 0; run < 100; run++) {
        for (int i = 0; i < 10; i++) {
            random = random * 1664525U + 1013904223U;
            SDL_Rect rect = { (int)(random % 120) - 10, (int)((random >> 8) % 100) - 10, 13, 11 };
            SDL_FillSurfaceRect(source, &rect, random & 0xFFFFFF);
            tig_video_upload_mark(&upload, rect);
        }
        CHECK(tig_video_upload_flush(&upload, texture, source, &bytes));
        compare(renderer, texture, source);
    }
    tig_video_upload_mark(&upload, (SDL_Rect){ -5, -5, 10, 10 });
    CHECK(upload.count == 1 && upload.rects[0].x == 0 && upload.rects[0].w == 5);
    // A failed upload must retain all regions for the next frame.
    CHECK(!tig_video_upload_flush(&upload, NULL, source, &bytes));
    CHECK(upload.count == 1);
    CHECK(tig_video_upload_flush(&upload, texture, source, &bytes));
    CHECK(bytes == 5 * 5 * 4);
    for (int i = 0; i < 33; i++) tig_video_upload_mark(&upload, (SDL_Rect){ (i % 10) * 9, (i / 10) * 9, 1, 1 });
    CHECK(upload.count == 1 && upload.rects[0].w == 100 && upload.rects[0].h == 80);
    CHECK(tig_video_upload_flush(&upload, texture, source, &bytes));
    tig_video_upload_mark(&upload, (SDL_Rect){ 0, 0, 100, 60 });
    CHECK(upload.count == 1 && upload.rects[0].h == 80);
    CHECK(tig_video_upload_flush(&upload, texture, source, &bytes));
    tig_video_upload_reset(&upload, 100, 80);
    CHECK(tig_video_upload_flush(&upload, texture, source, &bytes));
    CHECK(bytes == 100 * 80 * 4);
    compare(renderer, texture, source);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(source);
    SDL_DestroySurface(target);
    SDL_Quit();
    printf("Partial texture uploads: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
