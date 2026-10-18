#include <tig/color_grade.h>
#include <stdio.h>
#include <math.h>

static Uint32 pixel(SDL_Surface* surface, int x, int y)
{
    return *(Uint32*)((Uint8*)surface->pixels + y * surface->pitch + x * 4);
}

int main(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) return 77;
    SDL_Surface* frame = SDL_CreateSurface(32, 24, SDL_PIXELFORMAT_XRGB8888);
    if (!frame) return 1;
    SDL_FillSurfaceRect(frame, NULL, 0x808080);
    TigVideoUpload damage = { 0 };
    damage.bounds = (SDL_Rect){ 0, 0, 32, 24 };
    damage.count = 1;
    damage.rects[0] = (SDL_Rect){ 4, 5, 8, 6 };
    TigColorGrade grade = { 0.5f, 0.75f, 1, 1 };
    if (!tig_color_grade_apply(frame, &damage, &grade)) {
        fprintf(stderr, "GPU shader unavailable: %s\n", SDL_GetError());
        tig_color_grade_shutdown();
        SDL_DestroySurface(frame);
        SDL_Quit();
        return 77;
    }
    Uint32 graded = pixel(frame, 5, 6);
    bool valid = abs((int)((graded >> 16) & 255) - 64) <= 2
        && abs((int)((graded >> 8) & 255) - 96) <= 2
        && abs((int)(graded & 255) - 128) <= 2
        && (pixel(frame, 0, 0) & 0xFFFFFF) == 0x808080;
    printf("GPU graded pixel: %06X, unchanged overlay pixel: %06X\n", graded & 0xFFFFFF, pixel(frame, 0, 0) & 0xFFFFFF);
    SDL_DestroySurface(frame);
    frame = SDL_CreateSurface(1280, 720, SDL_PIXELFORMAT_XRGB8888);
    if (!frame) return 1;
    tig_video_upload_reset(&damage, 1280, 720);
    Uint64 start = SDL_GetPerformanceCounter();
    for (int index = 0; index < 20; index++) {
        SDL_FillSurfaceRect(frame, NULL, 0x808080);
        valid = tig_color_grade_apply(frame, &damage, &grade) && valid;
    }
    printf("1280x720 GPU grading including upload/readback: %.2f ms/frame\n",
        (SDL_GetPerformanceCounter() - start) * 1000.0 / SDL_GetPerformanceFrequency() / 20);
    tig_color_grade_shutdown();
    SDL_DestroySurface(frame);
    SDL_Quit();
    return valid ? 0 : 1;
}
