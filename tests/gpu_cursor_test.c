#include <stdio.h>
#include <stdlib.h>
#include <tig/tig.h>
#include <tig/asset_prefetch.h>
#include <tig/performance.h>

static int failures;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL line %d: %s (%s)\n", __LINE__, #expr, SDL_GetError()); failures++; } } while (0)

static void fixture(void)
{
    CHECK(SDL_CreateDirectory("gpu-cursor-fixture/art"));
    uint32_t header[33] = { 0 };
    header[0] = 1; header[1] = 10; header[2] = 8; header[3] = 1; header[8] = 2; header[17] = 32;
    uint32_t palette[256] = { 0x00FF00, 0xFF0000, 0xFFFFFF };
    int32_t frames[2][7] = { { 4,4,16,1,1,0,0 }, { 4,4,16,1,1,0,0 } };
    uint8_t pixels[32] = { 0 }; pixels[5] = 1; pixels[21] = 2;
    FILE* file = fopen("gpu-cursor-fixture/art/mouse.art", "wb");
    CHECK(file != NULL);
    if (!file) return;
    CHECK(fwrite(header, sizeof(header), 1, file) == 1);
    CHECK(fwrite(palette, sizeof(palette), 1, file) == 1);
    CHECK(fwrite(frames, sizeof(frames), 1, file) == 1);
    CHECK(fwrite(pixels, sizeof(pixels), 1, file) == 1);
    fclose(file);
}

static Uint32 pixel(SDL_Surface* image, int x, int y)
{
    return *(Uint32*)((Uint8*)image->pixels + y * image->pitch + x * 4) & 0xFFFFFF;
}

static SDL_Surface* capture(SDL_Renderer* renderer)
{
    CHECK(tig_video_flip() == TIG_OK);
    SDL_Surface* raw = SDL_RenderReadPixels(renderer, NULL);
    SDL_Surface* image = raw ? SDL_ConvertSurface(raw, SDL_PIXELFORMAT_XRGB8888) : NULL;
    SDL_DestroySurface(raw);
    CHECK(image != NULL);
    return image;
}

static void blue(void)
{
    CHECK(tig_video_fill(NULL, tig_color_make(0, 0, 255)) == TIG_OK);
}

int main(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) return 77;
    fixture();
    TigInitInfo info = { 0 };
    info.flags = TIG_INITIALIZE_WINDOWED | TIG_INITIALIZE_HIDDEN;
    info.width = 800; info.height = 600; info.bpp = 32;
    tig_memory_init(&info);
    CHECK(tig_color_init(&info) == TIG_OK);
    if (tig_video_init(&info) != TIG_OK) {
        SDL_Log("Hidden renderer unavailable: %s", SDL_GetError());
        tig_color_exit(); tig_memory_exit(); SDL_Quit(); return 77;
    }
    CHECK(tig_file_repository_add("gpu-cursor-fixture"));
    CHECK(tig_palette_init(&info) == TIG_OK);
    CHECK(tig_art_init(&info) == TIG_OK);
    CHECK(tig_mouse_init(&info) == TIG_OK);
    tig_mouse_set_active(true);
    SDL_Renderer* renderer = NULL;
    CHECK(tig_video_renderer_get(&renderer) == TIG_OK);
    SDL_Texture* target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_TARGET, 800, 600);
    CHECK(target != NULL);
    CHECK(SDL_SetRenderTarget(renderer, target));
    tig_video_set_vsync_mode(0);

    blue();
    tig_video_gpu_cursor_set(true);
    tig_mouse_display();
    SDL_Surface* gpu = capture(renderer);
    if (gpu) {
        CHECK(pixel(gpu, 400, 300) == 0xFF0000);
        CHECK(pixel(gpu, 399, 299) == 0x0000FF);
    }
    tig_video_gpu_cursor_set(false);
    blue(); tig_mouse_display();
    SDL_Surface* cpu = capture(renderer);
    if (gpu && cpu) {
        for (int y = 0; y < 600; y++) {
            for (int x = 0; x < 800; x++) CHECK(pixel(gpu,x,y) == pixel(cpu,x,y));
        }
    }
    SDL_Surface* native_decode = gpu ? SDL_DuplicateSurface(gpu) : NULL;
    SDL_DestroySurface(gpu); SDL_DestroySurface(cpu);
    tig_art_id_t prepared_cursor_id = tig_mouse_cursor_get_art_id();
    tig_mouse_exit();
    tig_art_flush();
    CHECK(tig_asset_prefetch_enable(true));
    CHECK(!tig_art_touch_prefetched(prepared_cursor_id));
    tig_asset_prefetch_request("art\\mouse.art", 100);
    TigAssetData prepared = { 0 };
    Uint64 deadline = SDL_GetTicks() + 3000;
    bool ready = false;
    while (!(ready = tig_asset_prefetch_acquire("art\\mouse.art", &prepared)) && SDL_GetTicks() < deadline) SDL_Delay(1);
    CHECK(ready);
    tig_asset_prefetch_release(&prepared);
    CHECK(tig_performance_set_enabled(true, "gpu-cursor-prefetch.csv"));
    CHECK(tig_art_touch_prefetched(prepared_cursor_id));
    CHECK(tig_performance_counter(TIG_PERF_PREFETCH_DECODED_ART) == 1);
    CHECK(tig_performance_counter(TIG_PERF_PREFETCH_HITS) == 1);
    CHECK(tig_performance_counter(TIG_PERF_FILE_READ_NATIVE_BYTES) == 0);
    CHECK(tig_art_touch_prefetched(prepared_cursor_id));
    CHECK(tig_performance_counter(TIG_PERF_PREFETCH_DECODED_ART) == 1);
    CHECK(tig_mouse_init(&info) == TIG_OK);
    tig_mouse_set_active(true);
    CHECK(tig_performance_counter(TIG_PERF_PREFETCH_HITS) > 0);
    Uint64 requests = tig_performance_counter(TIG_PERF_PREFETCH_REQUESTS);
    tig_art_prefetch(tig_mouse_cursor_get_art_id(), 100);
    CHECK(tig_performance_counter(TIG_PERF_PREFETCH_REQUESTS) == requests);
    tig_performance_set_enabled(false, NULL);
    tig_video_gpu_cursor_set(true);
    blue(); tig_mouse_display();
    SDL_Surface* memory_decode = capture(renderer);
    if (native_decode && memory_decode) for (int y = 0; y < 600; y++) for (int x = 0; x < 800; x++)
        CHECK(pixel(native_decode, x, y) == pixel(memory_decode, x, y));
    SDL_DestroySurface(native_decode);
    SDL_DestroySurface(memory_decode);

    tig_video_gpu_cursor_set(true);
    blue();
    CHECK(tig_mouse_cursor_set_art_id(tig_art_id_frame_inc(tig_mouse_cursor_get_art_id())) == TIG_OK);
    tig_mouse_display();
    gpu = capture(renderer);
    if (gpu) CHECK(pixel(gpu, 400, 300) == 0xFFFFFF);
    SDL_DestroySurface(gpu);
    tig_mouse_hide();
    gpu = capture(renderer);
    if (gpu) CHECK(pixel(gpu, 400, 300) == 0x0000FF);
    SDL_DestroySurface(gpu);
    tig_mouse_show(); tig_mouse_display();
    gpu = capture(renderer);
    if (gpu) CHECK(pixel(gpu, 400, 300) == 0xFFFFFF);
    SDL_DestroySurface(gpu);

    tig_mouse_set_active(false);
    gpu = capture(renderer);
    if (gpu) CHECK(pixel(gpu, 400, 300) == 0x0000FF);
    SDL_DestroySurface(gpu);
    tig_mouse_set_active(true); tig_mouse_display();
    tig_video_invalidate_texture();
    tig_mouse_display();
    gpu = capture(renderer);
    if (gpu) CHECK(pixel(gpu, 400, 300) == 0xFFFFFF);
    SDL_DestroySurface(gpu);

    tig_art_id_t base;
    CHECK(tig_art_misc_id_create(TIG_ART_SYSTEM_MOUSE, 0, &base) == TIG_OK);
    CHECK(tig_mouse_cursor_overlay(base, 2, 2) == TIG_OK);
    tig_mouse_display();
    gpu = capture(renderer);
    if (gpu) {
        CHECK(pixel(gpu, 400, 300) == 0xFFFFFF);
        CHECK(pixel(gpu, 402, 302) == 0xFF0000);
    }
    SDL_DestroySurface(gpu);
    TigVideoBufferCreateInfo oversized_info = { 0 };
    oversized_info.flags = TIG_VIDEO_BUFFER_CREATE_SYSTEM_MEMORY;
    oversized_info.width = 4097;
    oversized_info.height = 1024;
    TigVideoBuffer* oversized = NULL;
    CHECK(tig_video_buffer_create(&oversized_info, &oversized) == TIG_OK);
    TigRect one_pixel = { 0, 0, 1, 1 };
    CHECK(!tig_video_gpu_cursor_prepare(oversized, 999, 1, &one_pixel, &one_pixel));
    CHECK(!tig_video_gpu_cursor_is_enabled());
    tig_video_buffer_destroy(oversized);
    blue(); tig_mouse_display();
    cpu = capture(renderer);
    if (cpu) {
        CHECK(pixel(cpu, 400, 300) == 0xFFFFFF);
        CHECK(pixel(cpu, 402, 302) == 0xFF0000);
    }
    SDL_DestroySurface(cpu);
    blue(); tig_video_gpu_cursor_set(true); tig_mouse_display();
    tig_mouse_exit();
    gpu = capture(renderer);
    if (gpu) CHECK(pixel(gpu, 400, 300) == 0x0000FF);
    SDL_DestroySurface(gpu);

    SDL_SetRenderTarget(renderer, NULL);
    SDL_DestroyTexture(target);
    tig_art_exit(); tig_palette_exit(); tig_file_exit(); tig_video_exit(); tig_color_exit(); tig_memory_exit();
    remove("gpu-cursor-fixture/art/mouse.art");
    SDL_RemovePath("gpu-cursor-fixture/art"); SDL_RemovePath("gpu-cursor-fixture");
    remove("gpu-cursor-prefetch.csv");
    remove("gpu-cursor-prefetch.csv.details.csv");
    remove("gpu-cursor-prefetch.csv.events.csv");
    SDL_Quit();
    printf("GPU cursor integration: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures != 0;
}
