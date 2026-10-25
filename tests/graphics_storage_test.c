#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <tig/pixel_buffer.h>
#include <tig/bmp.h>
#include <tig/color.h>
#include <tig/file.h>
#include <tig/memory.h>
#include <tig/video.h>
#include <tig/video_upload.h>
#include <tig/scale_region.h>
#include <tig/sprite_cache.h>

static int failures;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s (%s)\n", __LINE__, #value, SDL_GetError()); failures++; } } while (0)

static void sizes(void)
{
    const int bits[] = { 1, 4, 8, 16, 24, 32 };
    for (int b = 0; b < 6; b++) for (int width = 1; width < 100; width++) {
        int pitch = 0;
        size_t bytes = 0;
        uint64_t expected = ((uint64_t)width * bits[b] + 31) / 32 * 4;
        CHECK(tig_pixel_buffer_row_size(width, bits[b], &pitch) && pitch == expected);
        CHECK(tig_pixel_buffer_size(width, 17, bits[b], pitch, &bytes) && bytes == expected * 17);
        CHECK(!tig_pixel_buffer_size(width, 17, bits[b], pitch - 1, &bytes));
        CHECK(tig_pixel_buffer_size(width, 17, bits[b], pitch + 16, &bytes) && bytes == (expected + 16) * 17);
    }
    int pitch;
    size_t bytes;
    CHECK(!tig_pixel_buffer_row_size(INT_MAX, 32, &pitch));
    CHECK(!tig_pixel_buffer_row_size(0, 32, &pitch));
    CHECK(!tig_pixel_buffer_row_size(-1, 32, &pitch));
    CHECK(!tig_pixel_buffer_row_size(1, 0, &pitch));
    CHECK(!tig_pixel_buffer_row_size(1, INT_MAX, &pitch));
    CHECK(!tig_pixel_buffer_size(2, INT_MAX, 32, 8, &bytes));
    CHECK(!tig_pixel_buffer_size(2, 1, 32, -8, &bytes));
    CHECK(!tig_pixel_buffer_size(2, 0, 32, 8, &bytes));
    CHECK(tig_pixel_buffer_size(1, INT_MAX / 4, 32, 4, &bytes) && bytes == (size_t)INT_MAX - 3);
    CHECK(!tig_pixel_buffer_size(1, INT_MAX / 4 + 1, 32, 4, &bytes));
}

static void video_buffers(void)
{
    TigVideoBuffer* buffer = NULL;
    TigVideoBufferCreateInfo create = { .width = 7, .height = 5, .background_color = 0x123456 };
    CHECK(tig_video_buffer_create(&create, &buffer) == TIG_OK && buffer != NULL);
    if (buffer != NULL) {
        CHECK(tig_video_buffer_lock(buffer) == TIG_OK);
        TigVideoBufferData data;
        CHECK(tig_video_buffer_data(buffer, &data) == TIG_OK);
        CHECK(tig_video_buffer_memory_size(buffer) == (size_t)data.pitch * data.height);
        for (int y = 0; y < data.height; y++) for (int x = 0; x < data.width; x++) {
            uint32_t pixel;
            memcpy(&pixel, (unsigned char*)data.pixels + (size_t)y * data.pitch + x * 4, 4);
            CHECK((pixel & 0xffffff) == 0x123456);
        }
        CHECK(tig_video_buffer_unlock(buffer) == TIG_OK);
        CHECK(tig_video_buffer_memory_size(buffer) != 0);
        CHECK(tig_video_buffer_destroy(buffer) == TIG_OK);
    }
    CHECK(tig_video_buffer_memory_size(NULL) == 0);
    const int dimensions[][2] = { { 0, 1 }, { -1, 1 }, { 1, 0 }, { 1, -1 }, { INT_MAX, 2 }, { 65536, 65536 } };
    for (int i = 0; i < 6; i++) {
        create.width = dimensions[i][0]; create.height = dimensions[i][1];
        buffer = (TigVideoBuffer*)(uintptr_t)1;
        CHECK(tig_video_buffer_create(&create, &buffer) == TIG_ERR_INVALID_PARAM && buffer == NULL);
    }
    buffer = (TigVideoBuffer*)(uintptr_t)1;
    CHECK(tig_video_buffer_create(NULL, &buffer) == TIG_ERR_INVALID_PARAM && buffer == NULL);
    CHECK(tig_video_buffer_create(&create, NULL) == TIG_ERR_INVALID_PARAM);
}

static void put32(unsigned char* bytes, uint32_t value)
{
    for (int i = 0; i < 4; i++) bytes[i] = (unsigned char)(value >> (8 * i));
}

static void bitmap(int bits)
{
    const int width = 5, height = 3;
    int colors = bits <= 8 ? 1 << bits : 0;
    int stride = (width * bits + 31) / 32 * 4;
    unsigned char header[54] = { 'B', 'M' };
    put32(header + 2, 54 + colors * 4 + stride * height);
    put32(header + 10, 54 + colors * 4);
    put32(header + 14, 40);
    put32(header + 18, width); put32(header + 22, height);
    header[26] = 1; header[28] = (unsigned char)bits;
    put32(header + 34, stride * height); put32(header + 46, colors);
    FILE* file = fopen("graphics-fixture.bmp", "wb");
    CHECK(file != NULL);
    if (file == NULL) return;
    fwrite(header, sizeof(header), 1, file);
    for (int i = 0; i < colors; i++) {
        unsigned char color[] = { (unsigned char)(i * 3), (unsigned char)(i * 2), (unsigned char)i, 0 };
        fwrite(color, sizeof(color), 1, file);
    }
    for (int y = 0; y < height; y++) {
        unsigned char row[16] = { 0 };
        for (int x = 0; x < width; x++) {
            unsigned char index = (unsigned char)(x + y);
            if (bits == 4) row[x / 2] |= index << ((x % 2 == 0) ? 4 : 0);
            else if (bits == 8) row[x] = index;
            else { row[x * 3] = index * 3; row[x * 3 + 1] = index * 2; row[x * 3 + 2] = index; }
        }
        fwrite(row, stride, 1, file);
    }
    fclose(file);
    SDL_Surface* reference = SDL_LoadBMP("graphics-fixture.bmp");
    CHECK(reference != NULL);
    TigBmp bmp = { 0 };
    SDL_strlcpy(bmp.name, "graphics-fixture.bmp", sizeof(bmp.name));
    CHECK(tig_bmp_create(&bmp) == TIG_OK);
    if (reference != NULL && bmp.pixels != NULL) {
        CHECK(bmp.width == width && bmp.height == height && bmp.pitch == reference->pitch);
        size_t active = ((size_t)width * bmp.bpp + 7) / 8;
        for (int y = 0; y < height; y++) CHECK(memcmp((unsigned char*)bmp.pixels + (size_t)y * bmp.pitch,
            (unsigned char*)reference->pixels + (size_t)y * reference->pitch, active) == 0);
        for (int i = 0; i < colors; i++) CHECK(bmp.palette[i] == tig_color_make((unsigned char)i, (unsigned char)(i * 2), (unsigned char)(i * 3)));
    }
    SDL_DestroySurface(reference);
    CHECK(tig_bmp_destroy(&bmp) == TIG_OK && bmp.pixels == NULL);
    CHECK(tig_bmp_destroy(&bmp) == TIG_OK);
    remove("graphics-fixture.bmp");
}

static void malformed_surfaces(void)
{
    SDL_Surface* source = SDL_CreateSurface(8, 8, SDL_PIXELFORMAT_XRGB8888);
    SDL_Surface* target = SDL_CreateSurface(8, 8, SDL_PIXELFORMAT_XRGB8888);
    CHECK(source != NULL && target != NULL);
    if (source == NULL || target == NULL) return;
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(target);
    CHECK(renderer != NULL);
    if (renderer == NULL) return;
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, 8, 8);
    TigSpriteCache* cache = tig_sprite_cache_create(renderer, 2, 1024);
    CHECK(texture != NULL && cache != NULL);
    TigVideoUpload upload;
    tig_video_upload_reset(&upload, 8, 8);
    Uint64 bytes;
    int pitch = source->pitch;
    source->pitch = 1;
    CHECK(!tig_video_upload_flush(&upload, texture, source, &bytes) && upload.count == 1 && bytes == 0);
    CHECK(tig_sprite_cache_get(cache, 1, 1, source, false, 0) == NULL);
    CHECK(!tig_scale_region(source, (SDL_Rect){ 0, 0, 8, 8 }, target,
        (SDL_Rect){ 0, 0, 8, 8 }, (SDL_Rect){ 0, 0, 8, 8 }, SDL_SCALEMODE_NEAREST));
    source->pitch = pitch;
    CHECK(tig_video_upload_flush(&upload, texture, source, &bytes) && bytes == 256);
    const SDL_Rect invalid[] = { { -1, 0, 1, 1 }, { INT_MAX, 0, 1, 1 }, { 0, 0, INT_MAX, 1 }, { 0, 0, -1, 1 } };
    for (int i = 0; i < 4; i++) {
        upload.count = 1; upload.rects[0] = invalid[i];
        CHECK(!tig_video_upload_flush(&upload, texture, source, &bytes) && upload.count == 1);
    }
    upload.count = INT_MAX;
    CHECK(!tig_video_upload_flush(&upload, texture, source, &bytes));
    tig_sprite_cache_destroy(cache);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(source);
    SDL_DestroySurface(target);
}


static void cropped_bitmap(void)
{
    TigVideoBuffer* buffer = NULL;
    TigVideoBufferCreateInfo create = { .width = 13, .height = 9 };
    CHECK(tig_video_buffer_create(&create, &buffer) == TIG_OK);
    if (buffer == NULL) return;
    CHECK(tig_video_buffer_lock(buffer) == TIG_OK);
    TigVideoBufferData data;
    CHECK(tig_video_buffer_data(buffer, &data) == TIG_OK);
    for (int y = 0; y < 9; y++) for (int x = 0; x < 13; x++) {
        Uint32 pixel = ((x + 1) << 16) | ((y + 1) << 8) | (x + y);
        memcpy((Uint8*)data.pixels + (size_t)y * data.pitch + x * 4, &pixel, 4);
    }
    CHECK(tig_video_buffer_unlock(buffer) == TIG_OK);
    TigRect crops[] = { { 3, 2, 7, 5 }, { 12, 8, 1, 1 }, { 0, 0, 13, 9 } };
    TigVideoBufferSaveToBmpInfo save = { .video_buffer = buffer };
    SDL_strlcpy(save.path, "graphics-crop.bmp", sizeof(save.path));
    for (int i = 0; i < 3; i++) {
        save.rect = &crops[i];
        CHECK(tig_video_buffer_save_to_bmp(&save) == TIG_OK);
        SDL_Surface* result = SDL_LoadBMP(save.path);
        CHECK(result != NULL);
        if (result != NULL) {
            CHECK(result->w == crops[i].width && result->h == crops[i].height);
            for (int y = 0; y < result->h; y++) for (int x = 0; x < result->w; x++) {
                Uint8 red, green, blue, alpha;
                CHECK(SDL_ReadSurfacePixel(result, x, y, &red, &green, &blue, &alpha));
                CHECK(red == x + crops[i].x + 1 && green == y + crops[i].y + 1
                    && blue == x + crops[i].x + y + crops[i].y);
            }
        }
        SDL_DestroySurface(result);
    }
    FILE* sentinel = fopen(save.path, "wb");
    CHECK(sentinel != NULL);
    if (sentinel != NULL) { fwrite("keep", 4, 1, sentinel); fclose(sentinel); }
    TigRect invalid[] = { { -1, 0, 1, 1 }, { 0, -1, 1, 1 }, { 0, 0, 0, 1 },
        { INT_MAX, 0, 1, 1 }, { 0, 0, INT_MAX, INT_MAX }, { 12, 8, 2, 1 } };
    for (int i = 0; i < 6; i++) {
        save.rect = &invalid[i];
        CHECK(tig_video_buffer_save_to_bmp(&save) == TIG_ERR_INVALID_PARAM);
    }
    sentinel = fopen(save.path, "rb");
    CHECK(sentinel != NULL);
    if (sentinel != NULL) {
        char contents[5] = { 0 };
        CHECK(fread(contents, 1, 5, sentinel) == 4 && strcmp(contents, "keep") == 0);
        fclose(sentinel);
    }
    CHECK(tig_video_buffer_save_to_bmp(NULL) == TIG_ERR_INVALID_PARAM);
    save.video_buffer = NULL;
    CHECK(tig_video_buffer_save_to_bmp(&save) == TIG_ERR_INVALID_PARAM);
    tig_video_buffer_destroy(buffer);
    remove(save.path);
}



static void cropped_formats(void)
{
    const SDL_PixelFormat formats[] = { SDL_PIXELFORMAT_INDEX1LSB, SDL_PIXELFORMAT_INDEX1MSB,
        SDL_PIXELFORMAT_INDEX4LSB, SDL_PIXELFORMAT_INDEX4MSB, SDL_PIXELFORMAT_INDEX8,
        SDL_PIXELFORMAT_RGB565, SDL_PIXELFORMAT_RGB24, SDL_PIXELFORMAT_XRGB8888, SDL_PIXELFORMAT_ARGB8888 };
    for (int format = 0; format < 9; format++) {
        SDL_Surface* source = SDL_CreateSurface(13, 7, formats[format]);
        CHECK(source != NULL);
        if (source == NULL) continue;
        int bits = SDL_BITSPERPIXEL(source->format);
        if (bits < 8) {
            TigRect crop = { 3, 2, 7, 4 };
            CHECK(tig_bmp_save_surface(source, &crop, "graphics-format-crop.bmp") == TIG_ERR_INVALID_PARAM);
            SDL_DestroySurface(source);
            continue;
        }
        if (SDL_ISPIXELFORMAT_INDEXED(source->format)) {
            SDL_Palette* palette = SDL_CreateSurfacePalette(source);
            CHECK(palette != NULL);
            if (palette == NULL) { SDL_DestroySurface(source); continue; }
            SDL_Color colors[256];
            for (int i = 0; i < palette->ncolors; i++) colors[i] = (SDL_Color){ (Uint8)(i * 7), (Uint8)(i * 11), (Uint8)(i * 13), 255 };
            CHECK(SDL_SetPaletteColors(palette, colors, 0, palette->ncolors));
        }
        for (int y = 0; y < source->h; y++) for (int x = 0; x < source->w; x++) {
            if (SDL_ISPIXELFORMAT_INDEXED(source->format)) {
                *((Uint8*)source->pixels + (size_t)y * source->pitch + x) = (Uint8)(x + y);
            } else {
                CHECK(SDL_WriteSurfacePixel(source, x, y, x * 17, y * 31, (x + y) * 11, 255));
            }
        }
        CHECK(SDL_SetSurfaceColorMod(source, 80, 90, 100));
        CHECK(SDL_SetSurfaceAlphaMod(source, 91));
        CHECK(SDL_SetSurfaceBlendMode(source, SDL_BLENDMODE_BLEND));
        CHECK(SDL_SetSurfaceColorKey(source, true, 0));
        TigRect crop = { 3, 2, 7, 4 };
        CHECK(tig_bmp_save_surface(source, &crop, "graphics-format-crop.bmp") == TIG_OK);
        SDL_Surface* actual = SDL_LoadBMP("graphics-format-crop.bmp");
        CHECK(actual != NULL);
        if (actual != NULL) {
            CHECK(actual->w == 7 && actual->h == 4);
            for (int y = 0; y < 4; y++) for (int x = 0; x < 7; x++) {
                Uint8 r, g, blue, a, expected_r, expected_g, expected_b, expected_a;
                CHECK(SDL_ReadSurfacePixel(source, x + crop.x, y + crop.y, &expected_r, &expected_g, &expected_b, &expected_a));
                CHECK(SDL_ReadSurfacePixel(actual, x, y, &r, &g, &blue, &a));
                CHECK(r == expected_r && g == expected_g && blue == expected_b);
            }
        }
        Uint8 r, g, blue, a;
        SDL_BlendMode blend;
        CHECK(SDL_GetSurfaceColorMod(source, &r, &g, &blue) && r == 80 && g == 90 && blue == 100);
        CHECK(SDL_GetSurfaceAlphaMod(source, &a) && a == 91);
        CHECK(SDL_GetSurfaceBlendMode(source, &blend) && blend == SDL_BLENDMODE_BLEND);
        CHECK(SDL_SurfaceHasColorKey(source));
        SDL_DestroySurface(actual);
        SDL_DestroySurface(source);
    }
    remove("graphics-format-crop.bmp");
}


static void blit_input_validation(void)
{
    TigVideoBuffer* source = NULL;
    TigVideoBuffer* target = NULL;
    TigVideoBufferCreateInfo create = { .width = 4, .height = 3, .background_color = 0x123456 };
    CHECK(tig_video_buffer_create(&create, &source) == TIG_OK);
    create.background_color = 0x654321;
    CHECK(tig_video_buffer_create(&create, &target) == TIG_OK);
    if (source == NULL || target == NULL) {
        if (source != NULL) tig_video_buffer_destroy(source);
        if (target != NULL) tig_video_buffer_destroy(target);
        return;
    }
    TigRect source_rect = { 0, 0, 4, 3 }, target_rect = source_rect;
    TigVideoBufferBlitInfo valid = { .src_video_buffer = source, .dst_video_buffer = target,
        .src_rect = &source_rect, .dst_rect = &target_rect };
    CHECK(tig_video_buffer_blit(NULL) == TIG_ERR_INVALID_PARAM);
    for (int missing = 0; missing < 4; missing++) {
        TigVideoBufferBlitInfo invalid = valid;
        if (missing == 0) invalid.src_video_buffer = NULL;
        if (missing == 1) invalid.dst_video_buffer = NULL;
        if (missing == 2) invalid.src_rect = NULL;
        if (missing == 3) invalid.dst_rect = NULL;
        CHECK(tig_video_buffer_blit(&invalid) == TIG_ERR_INVALID_PARAM);
    }
    int* dimensions[] = { &source_rect.width, &source_rect.height, &target_rect.width, &target_rect.height };
    for (int i = 0; i < 4; i++) {
        int saved = *dimensions[i];
        *dimensions[i] = -1;
        CHECK(tig_video_buffer_blit(&valid) == TIG_ERR_INVALID_PARAM);
        *dimensions[i] = 0;
        CHECK(tig_video_buffer_blit(&valid) == TIG_OK);
        *dimensions[i] = saved;
    }
    TigRect gradient = { 0, 0, 4, 3 };
    valid.flags = TIG_VIDEO_BUFFER_BLIT_BLEND_COLOR_LERP;
    CHECK(tig_video_buffer_blit(&valid) == TIG_ERR_INVALID_PARAM);
    valid.lerp_rect = &gradient;
    gradient.width = 0;
    CHECK(tig_video_buffer_blit(&valid) == TIG_ERR_INVALID_PARAM);
    gradient.width = 4; gradient.height = -1;
    CHECK(tig_video_buffer_blit(&valid) == TIG_ERR_INVALID_PARAM);
    TigVideoBufferData pixels;
    CHECK(tig_video_buffer_lock(target) == TIG_OK);
    CHECK(tig_video_buffer_data(target, &pixels) == TIG_OK);
    for (int y = 0; y < 3; y++) for (int x = 0; x < 4; x++) {
        Uint32 pixel;
        memcpy(&pixel, (Uint8*)pixels.pixels + (size_t)y * pixels.pitch + x * 4, 4);
        CHECK((pixel & 0xffffff) == 0x654321);
    }
    CHECK(tig_video_buffer_unlock(target) == TIG_OK);
    valid.flags = 0;
    CHECK(tig_video_buffer_blit(&valid) == TIG_OK);
    CHECK(tig_video_buffer_lock(target) == TIG_OK);
    CHECK(tig_video_buffer_data(target, &pixels) == TIG_OK);
    for (int y = 0; y < 3; y++) for (int x = 0; x < 4; x++) {
        Uint32 pixel;
        memcpy(&pixel, (Uint8*)pixels.pixels + (size_t)y * pixels.pitch + x * 4, 4);
        CHECK((pixel & 0xffffff) == 0x123456);
    }
    CHECK(tig_video_buffer_unlock(target) == TIG_OK);
    tig_video_buffer_destroy(source);
    tig_video_buffer_destroy(target);
}


static void surface_clipping(void)
{
    SDL_Surface* source = SDL_CreateSurface(6, 5, SDL_PIXELFORMAT_ARGB8888);
    TigVideoBuffer* target = NULL;
    TigVideoBufferCreateInfo create = { .width = 8, .height = 7, .background_color = 0x654321 };
    CHECK(source != NULL && tig_video_buffer_create(&create, &target) == TIG_OK);
    if (source == NULL || target == NULL) {
        SDL_DestroySurface(source);
        if (target != NULL) tig_video_buffer_destroy(target);
        return;
    }
    for (int y = 0; y < 5; y++) for (int x = 0; x < 6; x++) {
        CHECK(SDL_WriteSurfacePixel(source, x, y, x + 1, y + 1, x + y, 255));
    }
    const TigRect sources[] = { { -2, -1, 7, 6 }, { 1, 1, 6, 5 }, { INT_MIN, 0, INT_MAX, 2 },
        { INT_MAX, 0, INT_MAX, 2 }, { 0, 0, INT_MAX, INT_MAX }, { 0, 0, 6, 5 }, { 0, 0, 0, 5 } };
    const TigRect destinations[] = { { 1, 2, 7, 6 }, { -2, -2, 6, 5 }, { 0, 0, 1, 1 },
        { 0, 0, 1, 1 }, { 0, 0, 1, 1 }, { INT_MIN, INT_MAX, 6, 5 }, { 0, 0, 6, 5 } };
    for (int test = 0; test < 7; test++) {
        CHECK(tig_video_buffer_fill(target, NULL, 0x654321) == TIG_OK);
        TigRect src = sources[test], dst = destinations[test];
        CHECK(tig_video_buffer_blit_sdl_surface(source, &src, target, &dst) == TIG_OK);
        CHECK(memcmp(&src, &sources[test], sizeof(src)) == 0);
        CHECK(memcmp(&dst, &destinations[test], sizeof(dst)) == 0);
        TigVideoBufferData pixels;
        CHECK(tig_video_buffer_lock(target) == TIG_OK);
        CHECK(tig_video_buffer_data(target, &pixels) == TIG_OK);
        for (int y = 0; y < 7; y++) for (int x = 0; x < 8; x++) {
            int64_t offset_x = (int64_t)x - dst.x, offset_y = (int64_t)y - dst.y;
            int64_t sx = (int64_t)src.x + offset_x, sy = (int64_t)src.y + offset_y;
            bool copied = offset_x >= 0 && offset_x < src.width && offset_y >= 0 && offset_y < src.height
                && sx >= 0 && sx < 6 && sy >= 0 && sy < 5;
            Uint32 expected = copied ? (Uint32)(((sx + 1) << 16) | ((sy + 1) << 8) | (sx + sy)) : 0x654321;
            Uint32 actual;
            memcpy(&actual, (Uint8*)pixels.pixels + (size_t)y * pixels.pitch + x * 4, 4);
            CHECK((actual & 0xffffff) == expected);
        }
        CHECK(tig_video_buffer_unlock(target) == TIG_OK);
    }
    TigRect rect = { 0, 0, 1, 1 };
    CHECK(tig_video_buffer_blit_sdl_surface(NULL, &rect, target, &rect) == TIG_ERR_INVALID_PARAM);
    CHECK(tig_video_buffer_blit_sdl_surface(source, NULL, target, &rect) == TIG_ERR_INVALID_PARAM);
    CHECK(tig_video_buffer_blit_sdl_surface(source, &rect, NULL, &rect) == TIG_ERR_INVALID_PARAM);
    CHECK(tig_video_buffer_blit_sdl_surface(source, &rect, target, NULL) == TIG_ERR_INVALID_PARAM);
    rect.width = -1;
    CHECK(tig_video_buffer_blit_sdl_surface(source, &rect, target, &rect) == TIG_ERR_INVALID_PARAM);
    SDL_DestroySurface(source);
    tig_video_buffer_destroy(target);
}


static void gradient_copy_bounds(void)
{
    TigVideoBuffer* source = NULL;
    TigVideoBuffer* target = NULL;
    TigVideoBufferCreateInfo create = { .width = 4, .height = 4, .background_color = 0xffffff };
    CHECK(tig_video_buffer_create(&create, &source) == TIG_OK);
    create.background_color = 0x654321;
    CHECK(tig_video_buffer_create(&create, &target) == TIG_OK);
    if (source == NULL || target == NULL) {
        if (source != NULL) tig_video_buffer_destroy(source);
        if (target != NULL) tig_video_buffer_destroy(target);
        return;
    }
    TigRect src = { 0, 0, 1, 1 }, dst = { 0, 0, 4, 4 }, gradient = { 0, 0, 4, 4 };
    TigVideoBufferBlitInfo info = { .flags = TIG_VIDEO_BUFFER_BLIT_BLEND_COLOR_LERP,
        .src_video_buffer = source, .dst_video_buffer = target, .src_rect = &src, .dst_rect = &dst,
        .lerp_rect = &gradient, .lerp_colors = { 0, 0xc0c0c0, 0xc0c0c0, 0 } };
    CHECK(tig_video_buffer_blit(&info) == TIG_ERR_INVALID_PARAM);
    src.width = 4;
    CHECK(tig_video_buffer_blit(&info) == TIG_ERR_INVALID_PARAM);
    TigVideoBufferData pixels;
    CHECK(tig_video_buffer_lock(target) == TIG_OK);
    CHECK(tig_video_buffer_data(target, &pixels) == TIG_OK);
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
        Uint32 value;
        memcpy(&value, (Uint8*)pixels.pixels + (size_t)y * pixels.pitch + x * 4, 4);
        CHECK((value & 0xffffff) == 0x654321);
    }
    CHECK(tig_video_buffer_unlock(target) == TIG_OK);
    src.height = 4;
    dst.x = -1; dst.y = -1;
    CHECK(tig_video_buffer_blit(&info) == TIG_OK);
    CHECK(tig_video_buffer_lock(target) == TIG_OK);
    CHECK(tig_video_buffer_data(target, &pixels) == TIG_OK);
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
        Uint32 value;
        memcpy(&value, (Uint8*)pixels.pixels + (size_t)y * pixels.pitch + x * 4, 4);
        Uint32 expected = 0x654321;
        if (x < 3 && y < 3) {
            Uint8 intensity = (Uint8)((x + 1) * 48);
            expected = tig_color_mul(0xffffff, tig_color_make(intensity, intensity, intensity));
        }
        CHECK((value & 0xffffff) == expected);
    }
    CHECK(tig_video_buffer_unlock(target) == TIG_OK);
    tig_video_buffer_destroy(source);
    tig_video_buffer_destroy(target);
}

static void gradient_extreme_coordinates(void)
{
    TigVideoBuffer* source = NULL;
    TigVideoBuffer* target = NULL;
    TigVideoBufferCreateInfo create = { .width = 4, .height = 4, .background_color = 0xffffff };
    CHECK(tig_video_buffer_create(&create, &source) == TIG_OK);
    CHECK(tig_video_buffer_create(&create, &target) == TIG_OK);
    if (source == NULL || target == NULL) {
        if (source != NULL) tig_video_buffer_destroy(source);
        if (target != NULL) tig_video_buffer_destroy(target);
        return;
    }
    TigRect rect = { 0, 0, 4, 4 };
    const TigRect gradients[] = {
        { INT_MIN, 0, 1, 4 }, { INT_MAX, 0, 1, 4 },
        { 0, INT_MIN, 4, 1 }, { 0, INT_MAX, 4, 1 },
        { -1, 0, 1, 4 }, { 1, 0, 1, 4 }
    };
    for (int test = 0; test < 6; test++) {
        TigRect gradient = gradients[test];
        TigVideoBufferBlitInfo info = { .flags = TIG_VIDEO_BUFFER_BLIT_BLEND_COLOR_LERP,
            .src_video_buffer = source, .dst_video_buffer = target, .src_rect = &rect, .dst_rect = &rect,
            .lerp_rect = &gradient, .lerp_colors = { 0, 0xffffff, 0xffffff, 0 } };
        if (test == 2 || test == 3) {
            info.lerp_colors[1] = 0;
            info.lerp_colors[3] = 0xffffff;
        }
        CHECK(tig_video_buffer_fill(target, NULL, 0x654321) == TIG_OK);
        CHECK(tig_video_buffer_blit(&info) == TIG_OK);
        CHECK(memcmp(&gradient, &gradients[test], sizeof(gradient)) == 0);
        TigVideoBufferData pixels;
        CHECK(tig_video_buffer_lock(target) == TIG_OK);
        CHECK(tig_video_buffer_data(target, &pixels) == TIG_OK);
        for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
            Uint32 value;
            memcpy(&value, (Uint8*)pixels.pixels + (size_t)y * pixels.pitch + x * 4, 4);
            bool bright = test == 0 || test == 2 || test == 4 || (test == 5 && x >= 2);
            Uint32 expected = tig_color_mul(0xffffff, bright ? 0xffffff : 0);
            CHECK((value & 0xffffff) == expected);
        }
        CHECK(tig_video_buffer_unlock(target) == TIG_OK);
    }
    tig_video_buffer_destroy(source);
    tig_video_buffer_destroy(target);
}

static void general_blit_clipping(void)
{
    const TigRect sources[] = {
        { 0, 0, 6, 5 }, { -2, -1, 8, 7 }, { 2, 1, 7, 6 }, { 1, 1, 3, 3 },
        { 0, 0, 6, 5 }, { 1, 1, 4, 3 }, { -1, -2, 9, 8 }, { 8, 8, 2, 2 }
    };
    const TigRect destinations[] = {
        { -3, -2, 12, 10 }, { -2, -1, 12, 10 }, { 2, 3, 3, 2 }, { -1, 2, 7, 5 },
        { 5, 5, 12, 10 }, { -2, -1, 2, 2 }, { -3, -2, 4, 3 }, { 0, 0, 4, 4 }
    };
    for (int keyed = 0; keyed < 2; keyed++) {
        TigVideoBuffer* source = NULL;
        TigVideoBuffer* target = NULL;
        TigVideoBufferCreateInfo create = { .width = 6, .height = 5,
            .flags = keyed ? TIG_VIDEO_BUFFER_CREATE_COLOR_KEY : 0, .color_key = 0x112233 };
        CHECK(tig_video_buffer_create(&create, &source) == TIG_OK);
        create.width = 8; create.height = 7; create.flags = 0;
        CHECK(tig_video_buffer_create(&create, &target) == TIG_OK);
        SDL_Surface* reference_source = SDL_CreateSurface(6, 5, SDL_PIXELFORMAT_XRGB8888);
        SDL_Surface* reference_target = SDL_CreateSurface(8, 7, SDL_PIXELFORMAT_XRGB8888);
        if (source == NULL || target == NULL || reference_source == NULL || reference_target == NULL) {
            CHECK(false);
        } else {
            TigVideoBufferData pixels;
            CHECK(tig_video_buffer_lock(source) == TIG_OK);
            CHECK(tig_video_buffer_data(source, &pixels) == TIG_OK);
            for (int y = 0; y < 5; y++) for (int x = 0; x < 6; x++) {
                Uint32 value = x == 2 && y == 2 ? 0x112233 : (x * 37 << 16) | (y * 53 << 8) | (x * 23 + y * 7);
                memcpy((Uint8*)pixels.pixels + (size_t)y * pixels.pitch + x * 4, &value, 4);
                memcpy((Uint8*)reference_source->pixels + (size_t)y * reference_source->pitch + x * 4, &value, 4);
            }
            CHECK(tig_video_buffer_unlock(source) == TIG_OK);
            if (keyed) CHECK(SDL_SetSurfaceColorKey(reference_source, true, 0x112233));
            for (int linear = 0; linear < 2; linear++) for (int test = 0; test < 8; test++) {
                TigRect src = sources[test], dst = destinations[test];
                TigVideoBufferBlitInfo info = { .src_video_buffer = source, .dst_video_buffer = target,
                    .src_rect = &src, .dst_rect = &dst, .flags = linear ? TIG_VIDEO_BUFFER_BLIT_SCALE_LINEAR : 0 };
                SDL_Rect reference_src = { src.x, src.y, src.width, src.height };
                SDL_Rect reference_dst = { dst.x, dst.y, dst.width, dst.height };
                CHECK(tig_video_buffer_fill(target, NULL, 0x654321) == TIG_OK);
                CHECK(SDL_FillSurfaceRect(reference_target, NULL, 0x654321));
                CHECK(SDL_BlitSurfaceScaled(reference_source, &reference_src, reference_target, &reference_dst,
                    linear ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST));
                CHECK(tig_video_buffer_blit(&info) == TIG_OK);
                CHECK(memcmp(&src, &sources[test], sizeof(src)) == 0);
                CHECK(memcmp(&dst, &destinations[test], sizeof(dst)) == 0);
                CHECK(tig_video_buffer_lock(target) == TIG_OK);
                CHECK(tig_video_buffer_data(target, &pixels) == TIG_OK);
                bool same = true;
                for (int y = 0; y < 7; y++) for (int x = 0; x < 8; x++) {
                    Uint32 actual, expected;
                    memcpy(&actual, (Uint8*)pixels.pixels + (size_t)y * pixels.pitch + x * 4, 4);
                    memcpy(&expected, (Uint8*)reference_target->pixels + (size_t)y * reference_target->pitch + x * 4, 4);
                    if ((actual & 0xffffff) != (expected & 0xffffff)) same = false;
                }
                if (!same) fprintf(stderr, "Blit mismatch: keyed=%d linear=%d case=%d\n", keyed, linear, test);
                CHECK(same);
                CHECK(tig_video_buffer_unlock(target) == TIG_OK);
            }
            const TigRect extreme_sources[] = {
                { INT_MIN, 0, INT_MAX, 5 }, { INT_MAX, 0, INT_MAX, 5 },
                { -1, -1, INT_MAX, INT_MAX }, { -INT_MAX + 3, 0, INT_MAX, 5 }, { 0, 0, 6, 5 }
            };
            const TigRect extreme_destinations[] = {
                { 0, 0, INT_MAX, 5 }, { 0, 0, INT_MAX, 5 },
                { -2, -2, INT_MAX, INT_MAX }, { -INT_MAX + 4, 0, INT_MAX, 5 }, { INT_MAX, INT_MIN, 6, 5 }
            };
            for (int test = 0; test < 5; test++) {
                TigRect src = extreme_sources[test], dst = extreme_destinations[test];
                TigVideoBufferBlitInfo info = { .src_video_buffer = source, .dst_video_buffer = target,
                    .src_rect = &src, .dst_rect = &dst };
                CHECK(tig_video_buffer_fill(target, NULL, 0x654321) == TIG_OK);
                CHECK(tig_video_buffer_blit(&info) == TIG_OK);
                CHECK(memcmp(&src, &extreme_sources[test], sizeof(src)) == 0);
                CHECK(memcmp(&dst, &extreme_destinations[test], sizeof(dst)) == 0);
                CHECK(tig_video_buffer_lock(target) == TIG_OK);
                CHECK(tig_video_buffer_data(target, &pixels) == TIG_OK);
                for (int y = 0; y < 7; y++) for (int x = 0; x < 8; x++) {
                    int64_t offset_x = (int64_t)x - dst.x, offset_y = (int64_t)y - dst.y;
                    int64_t sx = (int64_t)src.x + offset_x, sy = (int64_t)src.y + offset_y;
                    Uint32 expected = 0x654321, actual;
                    if (offset_x >= 0 && offset_y >= 0 && offset_x < src.width && offset_y < src.height
                        && sx >= 0 && sx < 6 && sy >= 0 && sy < 5) {
                        memcpy(&expected, (Uint8*)reference_source->pixels + (size_t)sy * reference_source->pitch + (size_t)sx * 4, 4);
                        if (keyed && expected == 0x112233) expected = 0x654321;
                    }
                    memcpy(&actual, (Uint8*)pixels.pixels + (size_t)y * pixels.pitch + x * 4, 4);
                    CHECK((actual & 0xffffff) == (expected & 0xffffff));
                }
                CHECK(tig_video_buffer_unlock(target) == TIG_OK);
            }
            TigRect src = { -INT_MAX + 3, 0, INT_MAX, 5 }, dst = { -INT_MAX + 4, 0, INT_MAX, 10 };
            TigVideoBufferBlitInfo info = { .src_video_buffer = source, .dst_video_buffer = target,
                .src_rect = &src, .dst_rect = &dst };
            SDL_Rect reference_src = { 0, 0, 3, 5 }, reference_dst = { 1, 0, 3, 10 };
            CHECK(tig_video_buffer_fill(target, NULL, 0x654321) == TIG_OK);
            CHECK(SDL_FillSurfaceRect(reference_target, NULL, 0x654321));
            CHECK(SDL_BlitSurfaceScaled(reference_source, &reference_src, reference_target, &reference_dst, SDL_SCALEMODE_NEAREST));
            CHECK(tig_video_buffer_blit(&info) == TIG_OK);
            CHECK(tig_video_buffer_lock(target) == TIG_OK);
            CHECK(tig_video_buffer_data(target, &pixels) == TIG_OK);
            for (int y = 0; y < 7; y++) for (int x = 0; x < 8; x++) {
                Uint32 actual, expected;
                memcpy(&actual, (Uint8*)pixels.pixels + (size_t)y * pixels.pitch + x * 4, 4);
                memcpy(&expected, (Uint8*)reference_target->pixels + (size_t)y * reference_target->pitch + x * 4, 4);
                CHECK((actual & 0xffffff) == (expected & 0xffffff));
            }
            CHECK(tig_video_buffer_unlock(target) == TIG_OK);
            src = (TigRect){ 0, 0, 6, 5 }; dst = (TigRect){ 0, 0, INT_MAX, 10 };
            CHECK(tig_video_buffer_blit(&info) == TIG_ERR_INVALID_PARAM);
            dst = (TigRect){ 0, 0, 65535, 65535 };
            CHECK(tig_video_buffer_blit(&info) == TIG_ERR_INVALID_PARAM);
        }
        SDL_DestroySurface(reference_source);
        SDL_DestroySurface(reference_target);
        if (source != NULL) tig_video_buffer_destroy(source);
        if (target != NULL) tig_video_buffer_destroy(target);
    }
}

int main(void)
{
    CHECK(SDL_Init(0));
    CHECK(tig_memory_init(NULL) == TIG_OK);
    CHECK(tig_file_repository_add("."));
    CHECK(tig_color_init(NULL) == TIG_OK);
    CHECK(tig_color_set_rgba_settings(0xff0000, 0xff00, 0xff, 0) == TIG_OK);
    general_blit_clipping();
    gradient_copy_bounds();
    gradient_extreme_coordinates();
    surface_clipping();
    blit_input_validation();
    sizes(); video_buffers(); cropped_bitmap(); cropped_formats();
    bitmap(4); bitmap(8); bitmap(24);
    malformed_surfaces();
    CHECK(tig_bmp_create(NULL) == TIG_ERR_INVALID_PARAM);
    CHECK(tig_bmp_destroy(NULL) == TIG_ERR_INVALID_PARAM);
    tig_color_exit(); tig_file_exit(); tig_memory_exit(); SDL_Quit();
    printf("Graphics storage: %d failures\n", failures);
    return failures != 0;
}
