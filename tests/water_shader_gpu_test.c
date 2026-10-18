#include <tig/color_grade.h>
#include <stdio.h>

static Uint32 pixel(SDL_Surface* surface, int x, int y)
{
    return *(Uint32*)((Uint8*)surface->pixels + y * surface->pitch + x * 4) & 0xFFFFFF;
}

static void fill(SDL_Surface* frame)
{
    for (int y = 0; y < frame->h; y++) {
        Uint32* row = (Uint32*)((Uint8*)frame->pixels + y * frame->pitch);
        for (int x = 0; x < frame->w; x++) row[x] = ((50 + x * 2) << 16) | ((60 + y * 3) << 8) | 50;
    }
}

int main(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) return 77;
    SDL_Surface* base = SDL_CreateSurface(64, 48, SDL_PIXELFORMAT_XRGB8888);
    SDL_Surface* frame = SDL_CreateSurface(64, 48, SDL_PIXELFORMAT_XRGB8888);
    SDL_Surface* first = SDL_CreateSurface(64, 48, SDL_PIXELFORMAT_XRGB8888);
    SDL_Surface* mask = SDL_CreateSurface(64, 48, SDL_PIXELFORMAT_INDEX8);
    if (!base || !frame || !first || !mask) return 1;
    fill(base);
    SDL_FillSurfaceRect(mask, NULL, 0);
    SDL_Rect water = { 16, 8, 32, 32 };
    SDL_FillSurfaceRect(mask, &water, 255);
    SDL_BlitSurface(base, NULL, frame, NULL);
    TigVideoUpload damage;
    tig_video_upload_reset(&damage, 64, 48);
    if (!tig_water_shader_apply(frame, mask, &damage, 0, 0.8f, 0, 0)) {
        fprintf(stderr, "Water GPU shader unavailable: %s\n", SDL_GetError());
        tig_color_grade_shutdown();
        SDL_DestroySurface(base);
        SDL_DestroySurface(frame);
        SDL_DestroySurface(first);
        SDL_DestroySurface(mask);
        SDL_Quit();
        return 77;
    }
    int changed = 0;
    int outside = 0;
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 64; x++) {
            bool different = pixel(base, x, y) != pixel(frame, x, y);
            if (x >= 16 && x < 48 && y >= 8 && y < 40) changed += different;
            else outside += different;
        }
    }
    bool valid = changed > 100 && outside == 0;
    SDL_BlitSurface(frame, NULL, first, NULL);
    SDL_BlitSurface(base, NULL, frame, NULL);
    valid = tig_water_shader_apply(frame, mask, &damage, 2, 0.8f, 0, 0) && valid;
    int animated = 0;
    for (int y = 8; y < 40; y++) {
        for (int x = 16; x < 48; x++) animated += pixel(first, x, y) != pixel(frame, x, y);
    }
    valid = animated > 100 && valid;
    SDL_BlitSurface(base, NULL, frame, NULL);
    valid = tig_water_shader_apply(frame, mask, &damage, 2, 0, 0, 0) && valid;
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 64; x++) valid = pixel(base, x, y) == pixel(frame, x, y) && valid;
    }
    damage.count = 1;
    damage.rects[0] = (SDL_Rect){ 22, 16, 16, 16 };
    valid = tig_water_shader_apply(frame, mask, &damage, 0, 0.8f, 0, 0) && valid;
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 64; x++) {
            if (x < 22 || x >= 38 || y < 16 || y >= 32) valid = pixel(base, x, y) == pixel(frame, x, y) && valid;
        }
    }
    printf("Water shader: %d changed water pixels, %d changed dry pixels, %d animated pixels: %s\n",
        changed, outside, animated, valid ? "PASS" : "FAIL");
    SDL_DestroySurface(frame);
    SDL_DestroySurface(mask);
    frame = SDL_CreateSurface(1280, 720, SDL_PIXELFORMAT_XRGB8888);
    mask = SDL_CreateSurface(1280, 720, SDL_PIXELFORMAT_INDEX8);
    if (!frame || !mask) return 1;
    SDL_FillSurfaceRect(mask, NULL, 255);
    tig_video_upload_reset(&damage, 1280, 720);
    Uint64 start = SDL_GetPerformanceCounter();
    for (int i = 0; i < 10; i++) {
        SDL_FillSurfaceRect(frame, NULL, 0x507050);
        valid = tig_water_shader_apply(frame, mask, &damage, i * 0.05f, 0.45f, 0, 0) && valid;
    }
    printf("1280x720 water shader including upload/readback: %.2f ms/frame\n",
        (SDL_GetPerformanceCounter() - start) * 1000.0 / SDL_GetPerformanceFrequency() / 10);
    tig_color_grade_shutdown();
    SDL_DestroySurface(base);
    SDL_DestroySurface(frame);
    SDL_DestroySurface(first);
    SDL_DestroySurface(mask);
    SDL_Quit();
    return valid ? 0 : 1;
}
