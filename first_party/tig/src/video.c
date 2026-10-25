#include "tig/video.h"
#include "tig/video_upload.h"
#include "tig/scale_region.h"
#include "tig/performance.h"
#include "tig/sprite_cache.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>

#include "tig/art.h"
#include "tig/bmp.h"
#include "tig/color.h"
#include "tig/core.h"
#include "tig/debug.h"
#include "tig/draw.h"
#include "tig/file.h"
#include "tig/memory.h"
#include "tig/pixel_buffer.h"
#include "tig/message.h"
#include "tig/mouse.h"
#include "tig/timer.h"
#include "tig/window.h"

typedef struct TigVideoBuffer {
    TigVideoBufferFlags flags;
    TigRect frame;
    int texture_width;
    int texture_height;
    unsigned int background_color;
    unsigned int color_key;
    SDL_Surface* surface;
    int lock_count;
} TigVideoBuffer;

typedef struct TigVideoState {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    SDL_Surface* surface;
    int fps;
} TigVideoState;

typedef struct TigFadeState {
    bool enabled;
    SDL_Color color;
} TigFadeState;

static bool tig_video_window_create(TigInitInfo* init_info);
static void tig_video_window_destroy(void);
static bool tig_video_surface_color_masks_apply(void);
static int tig_video_screenshot_make_internal(int key);

// 0x5BF3D8
static int tig_video_screenshot_key = -1;

// 0x60F250
static TigVideoState tig_video_state;
static TigVideoUpload tig_video_upload;

typedef struct GpuCursor {
    SDL_Texture* texture;
    TigVideoBuffer* buffer;
    TigRect source;
    TigRect destination;
} GpuCursor;

static TigSpriteCache* tig_video_sprite_cache;
static GpuCursor tig_video_gpu_cursor;
static bool tig_video_gpu_cursor_enabled;

void tig_video_gpu_cursor_clear(void)
{
    SDL_memset(&tig_video_gpu_cursor, 0, sizeof(tig_video_gpu_cursor));
}

bool tig_video_gpu_cursor_is_enabled(void)
{
    return tig_video_gpu_cursor_enabled;
}

void tig_video_gpu_cursor_set(bool enabled)
{
    tig_video_gpu_cursor_clear();
    tig_video_gpu_cursor_enabled = enabled;
    if (!enabled) {
        tig_sprite_cache_destroy(tig_video_sprite_cache);
        tig_video_sprite_cache = NULL;
    }
    tig_window_invalidate_rect(NULL);
}

bool tig_video_gpu_cursor_prepare(TigVideoBuffer* buffer, Uint64 key, Uint64 revision,
    const TigRect* source, const TigRect* destination)
{
    if (!tig_video_gpu_cursor_enabled || !buffer || !source || !destination) return false;
    if (!tig_video_sprite_cache) tig_video_sprite_cache = tig_sprite_cache_create(tig_video_state.renderer, 128, 16 * 1024 * 1024);
    SDL_Texture* texture = tig_sprite_cache_get(tig_video_sprite_cache, key, revision,
        buffer->surface, (buffer->flags & TIG_VIDEO_BUFFER_COLOR_KEY) != 0, buffer->color_key);
    if (!texture) {
        tig_video_gpu_cursor_set(false);
        tig_debug_printf("GPU cursor unavailable, using CPU cursor: %s\n", SDL_GetError());
        return false;
    }
    tig_video_gpu_cursor = (GpuCursor){ texture, buffer, *source, *destination };
    return true;
}

// 0x60FEF8
static float tig_video_gamma;

// 0x61030C
static int tig_video_bpp;

// 0x61031C
static bool tig_video_3d_initialized;

// 0x610320
static bool tig_video_3d_is_hardware;

// 0x610358
static bool tig_video_3d_scene_started;

// 0x610388
static TigRect tig_video_screen_bounds;

// 0x61039C
static bool tig_video_initialized;

// 0x6103A0
static bool tig_video_show_fps;

// 0x6103A4
static int tig_video_legacy_setting;

static TigFadeState tig_fade_state;
static TigColorGrade tig_world_color_grade = { 1, 1, 1, 1 };

// 0x51F330
int tig_video_init(TigInitInfo* init_info)
{
    memset(&tig_video_state, 0, sizeof(tig_video_state));

    if (init_info->width < 800 || init_info->height < 600) {
        return TIG_ERR_GENERIC;
    }

    if (init_info->bpp != 32) {
        return TIG_ERR_GENERIC;
    }

    if (!tig_video_window_create(init_info)) {
        return TIG_ERR_GENERIC;
    }

    if (!tig_video_surface_color_masks_apply()) {
        tig_video_window_destroy();
        return TIG_ERR_GENERIC;
    }

    tig_video_show_fps = (init_info->flags & TIG_INITIALIZE_FPS) != 0;
    tig_video_bpp = init_info->bpp;

    tig_video_screenshot_key = -1;
    tig_video_legacy_setting = 0;

    tig_video_initialized = true;

    return TIG_OK;
}

// 0x51F3F0
void tig_video_exit(void)
{
    tig_color_grade_shutdown();
    tig_video_window_destroy();
    tig_video_initialized = false;
}

int tig_video_window_get(SDL_Window** window_ptr)
{
    if (!tig_video_initialized) {
        return TIG_ERR_NOT_INITIALIZED;
    }

    *window_ptr = tig_video_state.window;

    return TIG_OK;
}

int tig_video_set_vsync_mode(int mode)
{
    if (tig_video_state.renderer == NULL) {
        return TIG_ERR_NOT_INITIALIZED;
    }
    if (!SDL_SetRenderVSync(tig_video_state.renderer, mode)) {
        if (mode == SDL_RENDERER_VSYNC_ADAPTIVE
            && SDL_SetRenderVSync(tig_video_state.renderer, 1)) {
            return TIG_OK;
        }
        tig_debug_printf("Cannot set VSync mode %d: %s\n", mode, SDL_GetError());
        return TIG_ERR_GENERIC;
    }
    return TIG_OK;
}

int tig_video_renderer_get(SDL_Renderer** renderer_ptr)
{
    if (!tig_video_initialized) {
        return TIG_ERR_NOT_INITIALIZED;
    }

    *renderer_ptr = tig_video_state.renderer;

    return TIG_OK;
}

// 0x51F4A0
void tig_video_display_fps(void)
{
    // 0x60F248
    static tig_timestamp_t curr;

    // 0x610380
    static tig_timestamp_t prev;

    // 0x610384
    static unsigned int counter;

    tig_duration_t elapsed;

    if (tig_video_show_fps) {
        ++counter;
        tig_timer_now(&curr);
        if (prev == 0) {
            prev = curr;
            counter = 0;
        }
        elapsed = tig_timer_between(prev, curr);
        if (elapsed >= 1000) {
            tig_video_state.fps = (int)((float)counter * 1000.0f / elapsed + 0.5f);
            prev = curr;
            counter = 0;
        }
    }
}

// 0x51F600
int tig_video_blit(TigVideoBuffer* src_video_buffer, TigRect* src_rect, TigRect* dst_rect)
{
    int rc;
    TigRect clamped_dst_rect;
    SDL_Rect native_src_rect;
    SDL_Rect native_dst_rect;

    if (!tig_video_initialized) {
        return TIG_ERR_NOT_INITIALIZED;
    }

    rc = tig_rect_intersection(dst_rect, &tig_video_screen_bounds, &clamped_dst_rect);
    if (rc != TIG_OK) {
        return rc;
    }

    native_src_rect.x = src_rect->x;
    native_src_rect.y = src_rect->y;
    native_src_rect.w = src_rect->width;
    native_src_rect.h = src_rect->height;

    native_dst_rect.x = clamped_dst_rect.x;
    native_dst_rect.y = clamped_dst_rect.y;
    native_dst_rect.w = clamped_dst_rect.width;
    native_dst_rect.h = clamped_dst_rect.height;

    SDL_BlitSurface(src_video_buffer->surface,
        &native_src_rect,
        tig_video_state.surface,
        &native_dst_rect);
    tig_video_upload_mark(&tig_video_upload, native_dst_rect);

    return TIG_OK;
}

// CE: Per-pixel see-through blit for "near-black" source pixels.
// For each pixel in src_rect/dst_rect, if all of R, G, B are
// <= `threshold`, blend src with the underlay VB's pixel at the same
// screen position at `opacity` (255 opaque, 0 fully transparent).
// Other source pixels copy opaque to the screen. Sampling the underlay
// directly (instead of reading from the screen) avoids the top-down
// compositor's stale-dst problem — at the bar's frame the screen is
// stale because the iso below never blit through. underlay_offset_x/y
// give the (x, y) inside the underlay VB that corresponds to screen
// (0, 0); for a fullscreen iso window at frame (0,0,...) these are 0.
//
// Both src VB and the underlay (and screen) are XRGB8888 — bytes in
// memory [B, G, R, X] on little-endian; reads/writes are 32-bit.
int tig_video_blit_near_black_alpha(TigVideoBuffer* src_video_buffer,
    TigRect* src_rect,
    TigRect* dst_rect,
    TigVideoBuffer* underlay_video_buffer,
    int underlay_offset_x,
    int underlay_offset_y,
    uint8_t threshold,
    uint8_t opacity)
{
    if (!tig_video_initialized) {
        return TIG_ERR_NOT_INITIALIZED;
    }
    if (src_video_buffer == NULL || src_video_buffer->surface == NULL
        || tig_video_state.surface == NULL) {
        return TIG_ERR_INVALID_PARAM;
    }

    TigRect clamped_dst;
    int rc = tig_rect_intersection(dst_rect, &tig_video_screen_bounds, &clamped_dst);
    if (rc != TIG_OK) {
        return rc;
    }
    int dx_off = clamped_dst.x - dst_rect->x;
    int dy_off = clamped_dst.y - dst_rect->y;

    SDL_Surface* src = src_video_buffer->surface;
    SDL_Surface* dst = tig_video_state.surface;
    SDL_Surface* under = (underlay_video_buffer != NULL)
        ? underlay_video_buffer->surface
        : NULL;

    if (!SDL_LockSurface(src)) {
        return TIG_ERR_GENERIC;
    }
    if (!SDL_LockSurface(dst)) {
        SDL_UnlockSurface(src);
        return TIG_ERR_GENERIC;
    }
    if (under != NULL && !SDL_LockSurface(under)) {
        SDL_UnlockSurface(dst);
        SDL_UnlockSurface(src);
        return TIG_ERR_GENERIC;
    }

    int src_pitch_px = src->pitch / 4;
    int dst_pitch_px = dst->pitch / 4;
    int under_pitch_px = (under != NULL) ? (under->pitch / 4) : 0;
    uint32_t* src_base = (uint32_t*)src->pixels
        + (src_rect->y + dy_off) * src_pitch_px
        + (src_rect->x + dx_off);
    uint32_t* dst_base = (uint32_t*)dst->pixels
        + clamped_dst.y * dst_pitch_px
        + clamped_dst.x;
    uint32_t* under_base = NULL;
    if (under != NULL) {
        // Underlay-VB row corresponding to clamped_dst.y on screen.
        int uy = clamped_dst.y + underlay_offset_y;
        int ux = clamped_dst.x + underlay_offset_x;
        // Clamp to underlay extents — fall back to screen pixels for
        // any row/col outside the underlay's VB.
        if (uy < 0 || ux < 0 || uy >= under->h || ux >= under->w) {
            under = NULL;
        } else {
            under_base = (uint32_t*)under->pixels + uy * under_pitch_px + ux;
        }
    }

    int w = clamped_dst.width;
    int h = clamped_dst.height;
    // Scale 0..255 -> 0..256 so (s*a + d*(256-a)) >> 8 lands on the
    // standard alpha lerp without an off-by-one at opacity=255.
    int a = opacity + (opacity >> 7);

    for (int y = 0; y < h; y++) {
        uint32_t* sp = src_base + y * src_pitch_px;
        uint32_t* dp = dst_base + y * dst_pitch_px;
        uint32_t* up = (under_base != NULL) ? (under_base + y * under_pitch_px) : NULL;
        for (int x = 0; x < w; x++) {
            uint32_t s = sp[x];
            uint8_t sr = (uint8_t)(s >> 16);
            uint8_t sg = (uint8_t)(s >> 8);
            uint8_t sb = (uint8_t)s;
            if (sr <= threshold && sg <= threshold && sb <= threshold) {
                // "Destination" for the blend = underlay if available,
                // else whatever's on the screen (likely stale).
                uint32_t d = (up != NULL) ? up[x] : dp[x];
                uint8_t dr = (uint8_t)(d >> 16);
                uint8_t dg = (uint8_t)(d >> 8);
                uint8_t db = (uint8_t)d;
                int br = (sr * a + dr * (256 - a)) >> 8;
                int bg = (sg * a + dg * (256 - a)) >> 8;
                int bb = (sb * a + db * (256 - a)) >> 8;
                dp[x] = 0xFF000000u | ((uint32_t)br << 16) | ((uint32_t)bg << 8) | (uint32_t)bb;
            } else {
                dp[x] = s;
            }
        }
    }

    if (under != NULL) {
        SDL_UnlockSurface(under);
    }
    SDL_UnlockSurface(dst);
    SDL_UnlockSurface(src);

    tig_video_upload_mark(&tig_video_upload, (SDL_Rect){ clamped_dst.x, clamped_dst.y, clamped_dst.width, clamped_dst.height });

    return TIG_OK;
}

// 0x51F7C0
int tig_video_fill(const TigRect* rect, tig_color_t color)
{
    int rc;
    TigRect clamped_rect;
    SDL_Rect native_rect;

    if (!tig_video_initialized) {
        return TIG_ERR_NOT_INITIALIZED;
    }

    rc = tig_rect_intersection(rect != NULL ? rect : &tig_video_screen_bounds,
        &tig_video_screen_bounds,
        &clamped_rect);
    if (rc != TIG_OK) {
        return rc;
    }

    native_rect.x = clamped_rect.x;
    native_rect.y = clamped_rect.y;
    native_rect.w = clamped_rect.width;
    native_rect.h = clamped_rect.height;

    if (!SDL_FillSurfaceRect(tig_video_state.surface, &native_rect, color)) {
        return TIG_ERR_GENERIC;
    }

    tig_video_upload_mark(&tig_video_upload, native_rect);

    return TIG_OK;
}

// 0x51F8F0
void tig_video_invalidate_texture(void)
{
    tig_video_gpu_cursor_clear();
    tig_sprite_cache_clear(tig_video_sprite_cache);
    tig_window_invalidate_rect(NULL);
    if (tig_video_state.surface != NULL) {
        tig_video_upload_reset(&tig_video_upload, tig_video_state.surface->w, tig_video_state.surface->h);
    }
}

void tig_video_color_grade_set(TigColorGrade grade)
{
    tig_world_color_grade = grade;
}

bool tig_video_buffer_water_shader(TigVideoBuffer* buffer, SDL_Surface* mask,
    const TigVideoUpload* damage, float time, float strength, float origin_x, float origin_y)
{
    return buffer != NULL && tig_water_shader_apply(buffer->surface, mask, damage, time, strength, origin_x, origin_y);
}

void tig_video_buffer_color_grade(TigVideoBuffer* buffer, const TigVideoUpload* damage)
{
    if (buffer != NULL) tig_color_grade_apply(buffer->surface, damage, &tig_world_color_grade);
}

int tig_video_flip(void)
{
    Uint64 start = tig_performance_begin();
    Uint64 bytes;
    bool uploaded = tig_video_upload_flush(&tig_video_upload, tig_video_state.texture, tig_video_state.surface, &bytes);
    tig_performance_end(TIG_PERF_TEXTURE_UPLOAD, start);
    tig_performance_count(TIG_PERF_UPLOAD_BYTES, bytes);
    if (!uploaded) return TIG_ERR_GENERIC;

    SDL_RenderClear(tig_video_state.renderer);
    SDL_RenderTexture(tig_video_state.renderer, tig_video_state.texture, NULL, NULL);

    if (tig_video_gpu_cursor.texture) {
        GpuCursor cursor = tig_video_gpu_cursor;
        SDL_FRect source = { (float)cursor.source.x, (float)cursor.source.y,
            (float)cursor.source.width, (float)cursor.source.height };
        SDL_FRect destination = { (float)cursor.destination.x, (float)cursor.destination.y,
            (float)cursor.destination.width, (float)cursor.destination.height };
        if (!SDL_RenderTexture(tig_video_state.renderer, cursor.texture, &source, &destination)) {
            tig_video_gpu_cursor_set(false);
            tig_video_blit(cursor.buffer, &cursor.source, &cursor.destination);
            if (!tig_video_upload_flush(&tig_video_upload, tig_video_state.texture, tig_video_state.surface, NULL)) return TIG_ERR_GENERIC;
            SDL_RenderTexture(tig_video_state.renderer, tig_video_state.texture, NULL, NULL);
        }
    }

    if (tig_fade_state.enabled) {
        SDL_BlendMode blend_mode;
        SDL_GetRenderDrawBlendMode(tig_video_state.renderer, &blend_mode);
        SDL_SetRenderDrawBlendMode(tig_video_state.renderer, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
        SDL_SetRenderDrawColor(tig_video_state.renderer,
            tig_fade_state.color.r,
            tig_fade_state.color.g,
            tig_fade_state.color.b,
            tig_fade_state.color.a);
        SDL_RenderFillRect(tig_video_state.renderer, NULL);
        SDL_SetRenderDrawBlendMode(tig_video_state.renderer, blend_mode);
    }

    if (tig_video_show_fps) {
        SDL_SetRenderDrawColor(tig_video_state.renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
        char text[32];
        SDL_snprintf(text, sizeof(text), "%d FPS", tig_video_state.fps);
        float width = (float)SDL_strlen(text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
        float x = tig_video_state.surface->w - width - 8.0f;
        SDL_FRect background = { x - 4.0f, 4.0f, width + 8.0f, SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 8.0f };
        SDL_SetRenderDrawColor(tig_video_state.renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(tig_video_state.renderer, &background);
        SDL_SetRenderDrawColor(tig_video_state.renderer, 245, 245, 245, SDL_ALPHA_OPAQUE);
        SDL_RenderDebugText(tig_video_state.renderer, x, 8.0f, text);
    }

    start = tig_performance_begin();
    SDL_RenderPresent(tig_video_state.renderer);
    tig_performance_end(TIG_PERF_RENDER_PRESENT, start);

    return TIG_OK;
}

// 0x51F9E0
int tig_video_screenshot_set_settings(TigVideoScreenshotSettings* settings)
{
    int rc;

    if (tig_video_screenshot_key != -1) {
        rc = tig_message_set_key_handler(NULL, tig_video_screenshot_key);
        if (rc != TIG_OK) {
            return rc;
        }
    }

    tig_video_screenshot_key = settings->key;
    tig_video_legacy_setting = settings->legacy_mode;

    if (tig_video_screenshot_key != -1) {
        rc = tig_message_set_key_handler(tig_video_screenshot_make_internal, tig_video_screenshot_key);
        if (rc != TIG_OK) {
            return rc;
        }
    }

    return TIG_OK;
}

// 0x51FA30
int tig_video_screenshot_make(void)
{
    return tig_video_screenshot_make_internal(tig_video_screenshot_key);
}

// 0x51FAA0
int tig_video_get_palette(unsigned int* colors)
{
    if (!tig_video_initialized) {
        return TIG_ERR_NOT_INITIALIZED;
    }

    memset(colors, 0, sizeof(*colors) * 256);

    return TIG_OK;
}

// 0x51FB10
int tig_video_3d_check_initialized(void)
{
    if (!tig_video_initialized) {
        return TIG_ERR_NOT_INITIALIZED;
    }

    if (!tig_video_3d_initialized) {
        return TIG_ERR_GENERIC;
    }

    return TIG_OK;
}

// 0x51FB30
int tig_video_3d_check_hardware(void)
{
    if (!tig_video_initialized) {
        return TIG_ERR_NOT_INITIALIZED;
    }

    if (!tig_video_3d_is_hardware) {
        return TIG_ERR_GENERIC;
    }

    return TIG_OK;
}

// 0x51FB50
int tig_video_3d_begin_scene(void)
{
    if (tig_video_3d_initialized) {
        return TIG_ERR_GENERIC;
    }

    tig_video_3d_scene_started = true;
    return TIG_OK;
}

// 0x51FBA0
int tig_video_3d_end_scene(void)
{
    if (!tig_video_3d_scene_started) {
        return TIG_ERR_GENERIC;
    }

    if (tig_video_3d_initialized) {
        return TIG_ERR_GENERIC;
    }

    tig_video_3d_scene_started = false;
    return TIG_OK;
}

// 0x51FC90
int tig_video_check_gamma_control(void)
{
    return TIG_OK;
}

// NOTE: The original code implements fading using DirectDraw gamma ramp
// adjustments which is not available in SDL3. This implementation achieves
// fading effect by drawing alpha-blended rectangle covering entire window in
// `tig_video_flip`.
//
// This implementation prefers `steps` over `duration` - the decision is based
// on subjective perception of default fade steps (48 steps) vs. default fade
// duration (2 seconds) on a target 60 fps monitor. The steps approach takes
// about 800 ms which is perfectly fine.
//
// 0x51FCA0
int tig_video_fade(tig_color_t color, int steps, float duration, TigFadeFlags flags)
{
    int step;

    (void)duration;

    if ((flags & TIG_FADE_IN) == 0) {
        // Enable faded state.
        tig_fade_state.enabled = true;
        tig_fade_state.color.r = (Uint8)tig_color_get_red(color);
        tig_fade_state.color.g = (Uint8)tig_color_get_green(color);
        tig_fade_state.color.b = (Uint8)tig_color_get_blue(color);
    }

    if ((flags & TIG_FADE_IN) != 0) {
        // Fade in by gradually reducing alpha from 255 (fully opaque) to 0
        // (completely transparent).
        for (step = 0; step < steps; step++) {
            tig_fade_state.color.a = (Uint8)(255 - step * 255 / steps);
            tig_video_flip();
        }
    } else {
        // Fade out by gradually increasing alpha from 0 (completely
        // transparent) to 255 (fully opaque).
        for (step = 0; step < steps; step++) {
            tig_fade_state.color.a = (Uint8)(step * 255 / steps);
            tig_video_flip();
        }
    }

    if ((flags & TIG_FADE_IN) != 0) {
        // Disable faded state.
        tig_fade_state.enabled = false;
    }

    return TIG_OK;
}

// 0x51FFE0
int tig_video_set_gamma(float gamma)
{
    if (gamma == tig_video_gamma) {
        return TIG_OK;
    }

    if (gamma < 0.0 || gamma >= 2.0) {
        return TIG_ERR_INVALID_PARAM;
    }

    tig_video_gamma = gamma;
    tig_video_fade(0, 0, 0.0, 1);

    return TIG_OK;
}

// 0x5200F0
int tig_video_buffer_create(TigVideoBufferCreateInfo* vb_create_info, TigVideoBuffer** video_buffer_ptr)
{
    TigVideoBuffer* video_buffer;
    int texture_width;
    int texture_height;
    int pitch;
    size_t bytes;
    if (video_buffer_ptr == NULL) return TIG_ERR_INVALID_PARAM;
    *video_buffer_ptr = NULL;
    if (vb_create_info == NULL
        || !tig_pixel_buffer_row_size(vb_create_info->width, 32, &pitch)
        || !tig_pixel_buffer_size(vb_create_info->width, vb_create_info->height, 32, pitch, &bytes)) return TIG_ERR_INVALID_PARAM;

    video_buffer = (TigVideoBuffer*)MALLOC(sizeof(*video_buffer));
    memset(video_buffer, 0, sizeof(*video_buffer));

    texture_width = vb_create_info->width;
    texture_height = vb_create_info->height;

    video_buffer->surface = SDL_CreateSurface(texture_width, texture_height, SDL_PIXELFORMAT_XRGB8888);
    if (video_buffer->surface == NULL) {
        FREE(video_buffer);
        return TIG_ERR_OUT_OF_MEMORY;
    }
    if (!tig_pixel_buffer_size(texture_width, texture_height, 32, video_buffer->surface->pitch, &bytes)) {
        SDL_DestroySurface(video_buffer->surface);
        FREE(video_buffer);
        return TIG_ERR_INVALID_PARAM;
    }

    video_buffer->flags |= TIG_VIDEO_BUFFER_SYSTEM_MEMORY;

    if ((vb_create_info->flags & TIG_VIDEO_BUFFER_CREATE_COLOR_KEY) != 0) {
        video_buffer->flags |= TIG_VIDEO_BUFFER_COLOR_KEY;
        tig_video_buffer_set_color_key(video_buffer, vb_create_info->color_key);
    }

    video_buffer->frame.x = 0;
    video_buffer->frame.y = 0;
    video_buffer->frame.width = vb_create_info->width;
    video_buffer->frame.height = vb_create_info->height;
    video_buffer->texture_width = texture_width;
    video_buffer->texture_height = texture_height;
    video_buffer->background_color = vb_create_info->background_color;

    if ((video_buffer->flags & TIG_VIDEO_BUFFER_TEXTURE) == 0) {
        SDL_FillSurfaceRect(video_buffer->surface, NULL, vb_create_info->background_color);
    }

    video_buffer->lock_count = 0;
    *video_buffer_ptr = video_buffer;
    return TIG_OK;
}

// 0x520390
int tig_video_buffer_destroy(TigVideoBuffer* video_buffer)
{
    if (video_buffer == NULL) {
        return TIG_ERR_GENERIC;
    }

    SDL_DestroySurface(video_buffer->surface);
    FREE(video_buffer);

    return TIG_OK;
}

size_t tig_video_buffer_memory_size(const TigVideoBuffer* video_buffer)
{
    size_t bytes;
    if (video_buffer == NULL || video_buffer->surface == NULL
        || !tig_pixel_buffer_size(video_buffer->surface->w, video_buffer->surface->h,
            32, video_buffer->surface->pitch, &bytes)) return 0;
    return bytes;
}

// 0x5203E0
int tig_video_buffer_data(TigVideoBuffer* video_buffer, TigVideoBufferData* video_buffer_data)
{
    if (video_buffer == NULL) {
        return TIG_ERR_GENERIC;
    }

    video_buffer_data->flags = video_buffer->flags;
    video_buffer_data->width = video_buffer->frame.width;
    video_buffer_data->height = video_buffer->frame.height;

    if ((video_buffer->flags & TIG_VIDEO_BUFFER_LOCKED) != 0) {
        video_buffer_data->pitch = video_buffer->surface->pitch;
    } else {
        video_buffer_data->pitch = 0;
    }

    video_buffer_data->background_color = video_buffer->background_color;
    video_buffer_data->color_key = video_buffer->color_key;
    video_buffer_data->bpp = tig_video_bpp;

    if ((video_buffer->flags & TIG_VIDEO_BUFFER_LOCKED) != 0) {
        video_buffer_data->pixels = video_buffer->surface->pixels;
    } else {
        video_buffer_data->pixels = NULL;
    }

    return TIG_OK;
}

// 0x520450
int tig_video_buffer_set_color_key(TigVideoBuffer* video_buffer, int color_key)
{
    if ((video_buffer->flags & TIG_VIDEO_BUFFER_COLOR_KEY) == 0) {
        return TIG_ERR_GENERIC;
    }

    if (!SDL_SetSurfaceColorKey(video_buffer->surface, true, color_key)) {
        return TIG_ERR_GENERIC;
    }

    video_buffer->color_key = color_key;

    return TIG_OK;
}

// 0x5204B0
int tig_video_buffer_lock(TigVideoBuffer* video_buffer)
{
    if (video_buffer->lock_count == 0) {
        if (!SDL_LockSurface(video_buffer->surface)) {
            return TIG_ERR_DIRECTX;
        }

        video_buffer->flags |= TIG_VIDEO_BUFFER_LOCKED;
    }

    video_buffer->lock_count++;

    return TIG_OK;
}

// 0x520500
int tig_video_buffer_unlock(TigVideoBuffer* video_buffer)
{
    if (video_buffer->lock_count == 1) {
        SDL_UnlockSurface(video_buffer->surface);
        video_buffer->flags &= ~TIG_VIDEO_BUFFER_LOCKED;
    }

    video_buffer->lock_count--;

    return TIG_OK;
}

// 0x520540
int tig_video_buffer_outline(TigVideoBuffer* video_buffer, TigRect* rect, tig_color_t color)
{
    TigRect line;
    int rc;

    line.x = rect->x;
    line.y = rect->y;
    line.width = rect->width;
    line.height = 1;
    rc = tig_video_buffer_fill(video_buffer, &line, color);
    if (rc != TIG_OK) {
        return rc;
    }

    line.x = rect->x;
    line.y = rect->y;
    line.width = 1;
    line.height = rect->height;
    rc = tig_video_buffer_fill(video_buffer, &line, color);
    if (rc != TIG_OK) {
        return rc;
    }

    line.x = rect->x + rect->width - 1;
    line.y = rect->y;
    line.width = 1;
    line.height = rect->height;
    rc = tig_video_buffer_fill(video_buffer, &line, color);
    if (rc != TIG_OK) {
        return rc;
    }

    line.x = rect->x;
    line.y = rect->y + rect->height - 1;
    line.width = rect->width;
    line.height = 1;
    rc = tig_video_buffer_fill(video_buffer, &line, color);
    if (rc != TIG_OK) {
        return rc;
    }

    return TIG_OK;
}

// 0x520630
int tig_video_buffer_fill(TigVideoBuffer* video_buffer, TigRect* rect, tig_color_t color)
{
    SDL_Rect native_rect;

    if (rect != NULL) {
        native_rect.x = rect->x;
        native_rect.y = rect->y;
        native_rect.w = rect->width;
        native_rect.h = rect->height;
    }

    if (!SDL_FillSurfaceRect(video_buffer->surface, rect != NULL ? &native_rect : NULL, color)) {
        return TIG_ERR_GENERIC;
    }

    return TIG_OK;
}

// 0x520660
int tig_video_buffer_line(TigVideoBuffer* video_buffer, TigLine* line, TigRect* clip_rect, tig_color_t color)
{
    int pattern = 0;
    bool reversed;
    TigDrawLineModeInfo mode_info;
    TigDrawLineStyleInfo style_info;
    int dx;
    int dy;
    int step_x;
    int step_y;
    int error;
    int error_y;
    int error_x;
    int x1;
    int y1;
    int x2;
    int y2;
    int x;
    int y;

    (void)clip_rect;

    if (tig_video_buffer_lock(video_buffer) != TIG_OK) {
        return TIG_ERR_DIRECTX;
    }

    if (line->y2 <= line->y1) {
        if (line->y2 < line->y1) {
            reversed = true;
        } else {
            reversed = line->x1 >= line->x2;
        }
    } else {
        reversed = false;
    }

    tig_draw_get_line_mode(&mode_info);
    tig_draw_get_line_style(&style_info);

    dx = abs(line->x2 - line->x1);
    dy = abs(line->y2 - line->y1);

    if (reversed) {
        if (dx < dy) {
            error_y = 2 * dx;
            error = 2 * dx - dy;
            error_x = 2 * (dx - dy);
        } else {
            error_y = 2 * dy;
            error = 2 * dy - dx;
            error_x = 2 * (dy - dx);
        }

        x1 = line->x2;
        y1 = line->y2;
        x2 = line->x1;
        y2 = line->y1;

        if (line->x1 < line->x2) {
            step_x = -1;
        } else if (line->x1 > line->x2) {
            step_x = 1;
        } else {
            step_x = 0;
        }
    } else {
        if (dx < dy) {
            error_y = 2 * dx;
            error = 2 * dx - dy;
            error_x = 2 * (dx - dy);
        } else {
            error_y = 2 * dy;
            error = 2 * dy - dx;
            error_x = 2 * (dy - dx);
        }

        x1 = line->x1;
        y1 = line->y1;
        x2 = line->x2;
        y2 = line->y2;

        if (line->x1 < line->x2) {
            step_x = 1;
        } else if (line->x1 > line->x2) {
            step_x = -1;
        } else {
            step_x = 0;
        }
    }

    switch (tig_video_bpp) {
    case 32:
        if (1) {
            uint32_t* dst = (uint32_t*)video_buffer->surface->pixels + (video_buffer->surface->pitch / 4) * y1 + x1;
            *dst = color;

            if (dx < dy) {
                y = y1;
                while (y != y2) {
                    if (error > 0) {
                        error += error_x;
                        y++;
                        dst += video_buffer->surface->pitch / 4 + step_x;
                    } else {
                        error += error_y;
                        y++;
                        dst += video_buffer->surface->pitch / 4;
                    }

                    switch (style_info.style) {
                    case TIG_LINE_STYLE_SOLID:
                        *dst = color;
                        break;
                    case TIG_LINE_STYLE_DOTTED:
                        if ((pattern & 1) != 0) {
                            *dst = color;
                        }
                        ++pattern;
                        break;
                    case TIG_LINE_STYLE_DASHED:
                        if (pattern < 3) {
                            *dst = color;
                        }
                        ++pattern;
                        if (pattern > 5) {
                            pattern = 0;
                        }
                        break;
                    }
                }
            } else {
                if (line->y1 == line->y2) {
                    step_y = step_x;
                } else {
                    step_y = step_x + video_buffer->surface->pitch / 4;
                }

                x = x1;
                while (x != x2) {
                    x += step_x;
                    if (error > 0) {
                        error += error_x;
                        dst += step_y;
                    } else {
                        error += error_y;
                        dst += step_x;
                    }

                    switch (style_info.style) {
                    case TIG_LINE_STYLE_SOLID:
                        *dst = (uint32_t)color;
                        break;
                    case TIG_LINE_STYLE_DOTTED:
                        if ((pattern & 1) != 0) {
                            *dst = (uint32_t)color;
                        }
                        ++pattern;
                        break;
                    case TIG_LINE_STYLE_DASHED:
                        if (pattern < 3) {
                            *dst = (uint32_t)color;
                        }
                        ++pattern;
                        if (pattern > 5) {
                            pattern = 0;
                        }
                        break;
                    }
                }
            }
        }
        break;
    }

    if (tig_video_buffer_unlock(video_buffer) != TIG_OK) {
        return TIG_ERR_GENERIC;
    }

    return TIG_OK;
}

// 0x520FB0
int tig_video_buffer_render_target_check(TigVideoBuffer* video_buffer, unsigned int flags)
{
    TigVideoBufferData data;

    if (flags == 0) {
        return TIG_OK;
    }

    if (!tig_video_3d_initialized) {
        return TIG_ERR_GENERIC;
    }

    if (tig_video_buffer_data(video_buffer, &data) != TIG_OK) {
        return TIG_ERR_GENERIC;
    }

    if ((data.flags & TIG_VIDEO_BUFFER_RENDER_TARGET) == 0) {
        return TIG_ERR_GENERIC;
    }

    return TIG_OK;
}

// 0x521000
int tig_video_buffer_scale_region(TigVideoBufferBlitInfo* blit_info, const TigRect* region)
{
    if (blit_info == NULL || region == NULL || blit_info->src_video_buffer == NULL || blit_info->dst_video_buffer == NULL
        || blit_info->src_rect == NULL || blit_info->dst_rect == NULL
        || (blit_info->flags & ~TIG_VIDEO_BUFFER_BLIT_SCALE_LINEAR) != 0
        || (blit_info->src_video_buffer->flags & TIG_VIDEO_BUFFER_COLOR_KEY) != 0) return TIG_ERR_INVALID_PARAM;
    SDL_Rect source = { blit_info->src_rect->x, blit_info->src_rect->y, blit_info->src_rect->width, blit_info->src_rect->height };
    SDL_Rect destination = { blit_info->dst_rect->x, blit_info->dst_rect->y, blit_info->dst_rect->width, blit_info->dst_rect->height };
    SDL_Rect part = { region->x, region->y, region->width, region->height };
    return tig_scale_region(blit_info->src_video_buffer->surface, source, blit_info->dst_video_buffer->surface,
        destination, part, (blit_info->flags & TIG_VIDEO_BUFFER_BLIT_SCALE_LINEAR) ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST)
        ? TIG_OK : TIG_ERR_GENERIC;
}

static uint8_t tig_video_gradient_channel(float value)
{
    if (value <= 0.0f) return 0;
    if (value >= 255.0f) return 255;
    return (uint8_t)value;
}

static int tig_video_clip_blit_axis(int source_position, int source_extent, int source_limit,
    int destination_position, int destination_extent, int destination_minimum, int destination_limit,
    bool stretched, int* clipped_source_position, int* clipped_source_extent,
    int* clipped_destination_position, int* clipped_destination_extent)
{
    int64_t source_begin = SDL_max((int64_t)source_position, 0);
    int64_t source_end = SDL_min((int64_t)source_position + source_extent, source_limit);
    if (source_begin >= source_end) return TIG_ERR_NO_INTERSECTION;
    int64_t destination_begin = (int64_t)destination_position
        + (source_begin - source_position) * destination_extent / source_extent;
    int64_t destination_size = (int64_t)destination_extent
        - ((int64_t)source_extent - (source_end - source_begin)) * destination_extent / source_extent;
    int64_t destination_end = destination_begin + destination_size;
    if (destination_size <= 0 || destination_begin >= destination_limit || destination_end <= destination_minimum)
        return TIG_ERR_NO_INTERSECTION;
    if (stretched) {
        if (source_end - source_begin > 65535 || destination_size > 65535) return TIG_ERR_INVALID_PARAM;
    } else {
        int64_t visible_begin = SDL_max(destination_begin, destination_minimum);
        int64_t visible_end = SDL_min(destination_end, destination_limit);
        source_begin += visible_begin - destination_begin;
        source_end -= destination_end - visible_end;
        destination_begin = visible_begin;
        destination_size = visible_end - visible_begin;
    }
    if (destination_begin < INT_MIN || destination_begin > INT_MAX) return TIG_ERR_INVALID_PARAM;
    *clipped_source_position = (int)source_begin;
    *clipped_source_extent = (int)(source_end - source_begin);
    *clipped_destination_position = (int)destination_begin;
    *clipped_destination_extent = (int)destination_size;
    return TIG_OK;
}

int tig_video_buffer_blit(TigVideoBufferBlitInfo* blit_info)
{
    bool stretched;
    TigRect blit_src_rect;
    TigRect blit_dst_rect;
    SDL_Rect native_src_rect;
    SDL_Rect native_dst_rect;
    int rc;

    if (blit_info == NULL || blit_info->src_rect == NULL || blit_info->dst_rect == NULL
        || blit_info->src_video_buffer == NULL || blit_info->dst_video_buffer == NULL
        || blit_info->src_video_buffer->surface == NULL || blit_info->dst_video_buffer->surface == NULL) {
        return TIG_ERR_INVALID_PARAM;
    }
    if (blit_info->src_rect->width < 0 || blit_info->src_rect->height < 0
        || blit_info->dst_rect->width < 0 || blit_info->dst_rect->height < 0) {
        return TIG_ERR_INVALID_PARAM;
    }
    if (blit_info->src_rect->width == 0 || blit_info->src_rect->height == 0
        || blit_info->dst_rect->width == 0 || blit_info->dst_rect->height == 0) return TIG_OK;
    if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_BLEND_COLOR_LERP) != 0
        && (blit_info->lerp_rect == NULL || blit_info->lerp_rect->width <= 0 || blit_info->lerp_rect->height <= 0)) {
        return TIG_ERR_INVALID_PARAM;
    }

    if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_BLEND_COLOR_LERP) != 0
        && (blit_info->src_rect->width != blit_info->dst_rect->width
            || blit_info->src_rect->height != blit_info->dst_rect->height)) return TIG_ERR_INVALID_PARAM;

    stretched = blit_info->src_rect->width != blit_info->dst_rect->width
        || blit_info->src_rect->height != blit_info->dst_rect->height;
    if (tig_video_buffer_memory_size(blit_info->src_video_buffer) == 0
        || tig_video_buffer_memory_size(blit_info->dst_video_buffer) == 0
        || blit_info->src_video_buffer->frame.width <= 0 || blit_info->src_video_buffer->frame.height <= 0
        || blit_info->dst_video_buffer->frame.width <= 0 || blit_info->dst_video_buffer->frame.height <= 0)
        return TIG_ERR_INVALID_PARAM;
    SDL_Rect clip;
    if (!SDL_GetSurfaceClipRect(blit_info->dst_video_buffer->surface, &clip)) return TIG_OK;
    int destination_right = (int)SDL_min((int64_t)clip.x + clip.w, blit_info->dst_video_buffer->frame.width);
    int destination_bottom = (int)SDL_min((int64_t)clip.y + clip.h, blit_info->dst_video_buffer->frame.height);
    if (clip.x >= destination_right || clip.y >= destination_bottom) return TIG_OK;
    rc = tig_video_clip_blit_axis(blit_info->src_rect->x, blit_info->src_rect->width,
        SDL_min(blit_info->src_video_buffer->frame.width, blit_info->src_video_buffer->surface->w),
        blit_info->dst_rect->x, blit_info->dst_rect->width, clip.x, destination_right, stretched,
        &blit_src_rect.x, &blit_src_rect.width, &blit_dst_rect.x, &blit_dst_rect.width);
    if (rc == TIG_ERR_NO_INTERSECTION) return TIG_OK;
    if (rc != TIG_OK) return rc;
    rc = tig_video_clip_blit_axis(blit_info->src_rect->y, blit_info->src_rect->height,
        SDL_min(blit_info->src_video_buffer->frame.height, blit_info->src_video_buffer->surface->h),
        blit_info->dst_rect->y, blit_info->dst_rect->height, clip.y, destination_bottom, stretched,
        &blit_src_rect.y, &blit_src_rect.height, &blit_dst_rect.y, &blit_dst_rect.height);
    if (rc == TIG_ERR_NO_INTERSECTION) return TIG_OK;
    if (rc != TIG_OK) return rc;
    if (stretched) {
        size_t scaled_bytes;
        int scaled_pitch;
        int storage_bits = SDL_BYTESPERPIXEL(blit_info->src_video_buffer->surface->format) * 8;
        if (storage_bits == 0) storage_bits = SDL_BITSPERPIXEL(blit_info->src_video_buffer->surface->format);
        if (!tig_pixel_buffer_row_size(blit_dst_rect.width, storage_bits, &scaled_pitch)
            || !tig_pixel_buffer_size(blit_dst_rect.width, blit_dst_rect.height, storage_bits, scaled_pitch, &scaled_bytes))
            return TIG_ERR_INVALID_PARAM;
    }

    native_src_rect.x = blit_src_rect.x;
    native_src_rect.y = blit_src_rect.y;
    native_src_rect.w = blit_src_rect.width;
    native_src_rect.h = blit_src_rect.height;

    native_dst_rect.x = blit_dst_rect.x;
    native_dst_rect.y = blit_dst_rect.y;
    native_dst_rect.w = blit_dst_rect.width;
    native_dst_rect.h = blit_dst_rect.height;

    if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_BLEND_COLOR_LERP) != 0) {
        // 0x5221A2
        // The algorithm is adopted from `TIG_ART_BLT_BLEND_COLOR_LERP` blitter.
        // It is used to blit townmap tiles with partially obscured edges. These
        // tiles are loaded from a bunch of .bmp files, so it bypass art system
        // entirely.
        SDL_Surface* source_surface = blit_info->src_video_buffer->surface;
        SDL_Surface* destination_surface = blit_info->dst_video_buffer->surface;
        if (SDL_BYTESPERPIXEL(source_surface->format) != 4 || SDL_BYTESPERPIXEL(destination_surface->format) != 4
            || source_surface->pixels == NULL || destination_surface->pixels == NULL
            || tig_video_buffer_memory_size(blit_info->src_video_buffer) == 0
            || tig_video_buffer_memory_size(blit_info->dst_video_buffer) == 0
            || blit_src_rect.width != blit_dst_rect.width || blit_src_rect.height != blit_dst_rect.height
            || blit_src_rect.x < 0 || blit_src_rect.y < 0 || blit_dst_rect.x < 0 || blit_dst_rect.y < 0
            || blit_src_rect.width <= 0 || blit_src_rect.height <= 0
            || blit_src_rect.width > source_surface->w || blit_src_rect.height > source_surface->h
            || blit_dst_rect.width > destination_surface->w || blit_dst_rect.height > destination_surface->h
            || blit_src_rect.x > source_surface->w - blit_src_rect.width
            || blit_src_rect.y > source_surface->h - blit_src_rect.height
            || blit_dst_rect.x > destination_surface->w - blit_dst_rect.width
            || blit_dst_rect.y > destination_surface->h - blit_dst_rect.height) return TIG_ERR_INVALID_PARAM;

        int tl_r = tig_color_get_red(blit_info->lerp_colors[0]);
        int tl_g = tig_color_get_green(blit_info->lerp_colors[0]);
        int tl_b = tig_color_get_blue(blit_info->lerp_colors[0]);

        int tr_r = tig_color_get_red(blit_info->lerp_colors[1]);
        int tr_g = tig_color_get_green(blit_info->lerp_colors[1]);
        int tr_b = tig_color_get_blue(blit_info->lerp_colors[1]);

        int br_r = tig_color_get_red(blit_info->lerp_colors[2]);
        int br_g = tig_color_get_green(blit_info->lerp_colors[2]);
        int br_b = tig_color_get_blue(blit_info->lerp_colors[2]);

        int bl_r = tig_color_get_red(blit_info->lerp_colors[3]);
        int bl_g = tig_color_get_green(blit_info->lerp_colors[3]);
        int bl_b = tig_color_get_blue(blit_info->lerp_colors[3]);

        float vert_start_step_r = (float)(bl_r - tl_r) / blit_info->lerp_rect->height;
        float vert_start_r = vert_start_step_r * ((int64_t)blit_src_rect.y - blit_info->lerp_rect->y) + tl_r;
        float vert_end_step_r = (float)(br_r - tr_r) / blit_info->lerp_rect->height;
        float vert_end_r = vert_end_step_r * ((int64_t)blit_src_rect.y - blit_info->lerp_rect->y) + tr_r;

        float vert_start_step_g = (float)(bl_g - tl_g) / blit_info->lerp_rect->height;
        float vert_start_g = vert_start_step_g * ((int64_t)blit_src_rect.y - blit_info->lerp_rect->y) + tl_g;
        float vert_end_step_g = (float)(br_g - tr_g) / blit_info->lerp_rect->height;
        float vert_end_g = vert_end_step_g * ((int64_t)blit_src_rect.y - blit_info->lerp_rect->y) + tr_g;

        float vert_start_step_b = (float)(bl_b - tl_b) / blit_info->lerp_rect->height;
        float vert_start_b = vert_start_step_b * ((int64_t)blit_src_rect.y - blit_info->lerp_rect->y) + tl_b;
        float vert_end_step_b = (float)(br_b - tr_b) / blit_info->lerp_rect->height;
        float vert_end_b = vert_end_step_b * ((int64_t)blit_src_rect.y - blit_info->lerp_rect->y) + tr_b;

        int x;
        int y;

        for (y = 0; y < blit_dst_rect.height; ++y) {
            const uint32_t* src = (const uint32_t*)((const uint8_t*)source_surface->pixels
                + (size_t)source_surface->pitch * (blit_src_rect.y + y) + (size_t)4 * blit_src_rect.x);
            uint32_t* dst = (uint32_t*)((uint8_t*)destination_surface->pixels
                + (size_t)destination_surface->pitch * (blit_dst_rect.y + y) + (size_t)4 * blit_dst_rect.x);
            float hor_step_r = (vert_end_r - vert_start_r) / blit_info->lerp_rect->width;
            float hor_step_g = (vert_end_g - vert_start_g) / blit_info->lerp_rect->width;
            float hor_step_b = (vert_end_b - vert_start_b) / blit_info->lerp_rect->width;

            float r = vert_start_r + hor_step_r * ((int64_t)blit_src_rect.x - blit_info->lerp_rect->x);
            float g = vert_start_g + hor_step_g * ((int64_t)blit_src_rect.x - blit_info->lerp_rect->x);
            float b = vert_start_b + hor_step_b * ((int64_t)blit_src_rect.x - blit_info->lerp_rect->x);

            for (x = 0; x < blit_dst_rect.width; ++x) {
                if ((blit_info->src_video_buffer->flags & TIG_VIDEO_BUFFER_COLOR_KEY) == 0
                    || *src != blit_info->src_video_buffer->color_key) {
                    *dst = tig_color_mul(*src, tig_color_make(tig_video_gradient_channel(r), tig_video_gradient_channel(g), tig_video_gradient_channel(b)));
                }

                r += hor_step_r;
                g += hor_step_g;
                b += hor_step_b;

                src++;
                dst++;
            }

            vert_start_r += vert_start_step_r;
            vert_end_r += vert_end_step_r;

            vert_start_g += vert_start_step_g;
            vert_end_g += vert_end_step_g;

            vert_start_b += vert_start_step_b;
            vert_end_b += vert_end_step_b;

        }

        return TIG_OK;
    }

    if (stretched) {
        SDL_ScaleMode scale_mode = (blit_info->flags & TIG_VIDEO_BUFFER_BLIT_SCALE_LINEAR)
            ? SDL_SCALEMODE_LINEAR
            : SDL_SCALEMODE_NEAREST;

        if (!SDL_BlitSurfaceScaled(blit_info->src_video_buffer->surface, &native_src_rect, blit_info->dst_video_buffer->surface, &native_dst_rect, scale_mode)) {
            return TIG_ERR_GENERIC;
        }
    } else {
        if (!SDL_BlitSurface(blit_info->src_video_buffer->surface, &native_src_rect, blit_info->dst_video_buffer->surface, &native_dst_rect)) {
            return TIG_ERR_GENERIC;
        }
    }

    return TIG_OK;
}

int tig_video_buffer_blit_sdl_surface(SDL_Surface* surface, TigRect* src_rect, TigVideoBuffer* dst_video_buffer, TigRect* dst_rect)
{
    if (surface == NULL || src_rect == NULL || dst_video_buffer == NULL || dst_rect == NULL
        || dst_video_buffer->surface == NULL || surface->pixels == NULL) return TIG_ERR_INVALID_PARAM;
    if (src_rect->width < 0 || src_rect->height < 0) return TIG_ERR_INVALID_PARAM;
    if (src_rect->width == 0 || src_rect->height == 0) return TIG_OK;
    size_t source_bytes;
    int storage_bits = SDL_BYTESPERPIXEL(surface->format) != 0
        ? SDL_BYTESPERPIXEL(surface->format) * 8 : SDL_BITSPERPIXEL(surface->format);
    if (!tig_pixel_buffer_size(surface->w, surface->h, storage_bits, surface->pitch, &source_bytes)
        || SDL_BYTESPERPIXEL(dst_video_buffer->surface->format) != 4
        || tig_video_buffer_memory_size(dst_video_buffer) == 0) return TIG_ERR_INVALID_PARAM;

    int64_t left = SDL_max(INT64_C(0), SDL_max(-(int64_t)src_rect->x, -(int64_t)dst_rect->x));
    int64_t top = SDL_max(INT64_C(0), SDL_max(-(int64_t)src_rect->y, -(int64_t)dst_rect->y));
    int64_t right = SDL_min((int64_t)src_rect->width,
        SDL_min((int64_t)surface->w - src_rect->x, (int64_t)dst_video_buffer->surface->w - dst_rect->x));
    int64_t bottom = SDL_min((int64_t)src_rect->height,
        SDL_min((int64_t)surface->h - src_rect->y, (int64_t)dst_video_buffer->surface->h - dst_rect->y));
    if (left >= right || top >= bottom) return TIG_OK;
    int source_x = (int)((int64_t)src_rect->x + left);
    int source_y = (int)((int64_t)src_rect->y + top);
    int destination_x = (int)((int64_t)dst_rect->x + left);
    int destination_y = (int)((int64_t)dst_rect->y + top);
    int width = (int)(right - left);
    int height = (int)(bottom - top);
    int x;
    int y;

    if (!SDL_LockSurface(surface)) {
        return TIG_ERR_GENERIC;
    }
    if (!SDL_LockSurface(dst_video_buffer->surface)) {
        SDL_UnlockSurface(surface);
        return TIG_ERR_GENERIC;
    }

    for (y = 0; y < height; y++) {
        uint32_t* dst = (uint32_t*)((uint8_t*)dst_video_buffer->surface->pixels
            + (size_t)dst_video_buffer->surface->pitch * (destination_y + y)
            + (size_t)4 * destination_x);

        for (x = 0; x < width; x++) {
            Uint8 r;
            Uint8 g;
            Uint8 b;
            Uint8 a;

            if (!SDL_ReadSurfacePixel(surface, source_x + x, source_y + y, &r, &g, &b, &a)) {
                SDL_UnlockSurface(dst_video_buffer->surface);
                SDL_UnlockSurface(surface);
                return TIG_ERR_GENERIC;
            }

            if ((dst_video_buffer->flags & TIG_VIDEO_BUFFER_COLOR_KEY) != 0
                && dst[x] == dst_video_buffer->color_key) {
                if (a == 255) {
                    dst[x] = tig_color_make(r, g, b);
                } else if (a != 0) {
                    dst[x] = tig_color_make((r * a) / 255, (g * a) / 255, (b * a) / 255);
                }
            } else if (a == 255) {
                dst[x] = tig_color_make(r, g, b);
            } else if (a != 0) {
                dst[x] = tig_color_blend_alpha(tig_color_make(r, g, b), dst[x], a);
            }
        }
    }

    SDL_UnlockSurface(dst_video_buffer->surface);
    SDL_UnlockSurface(surface);

    return TIG_OK;
}

int tig_video_buffer_blend_color(TigVideoBuffer* video_buffer, TigRect* rect, tig_color_t color, int alpha)
{
    int rc;
    TigRect frame;
    int x;
    int y;

    if (alpha <= 0) {
        return TIG_OK;
    }
    if (alpha > 255) {
        alpha = 255;
    }

    rc = tig_rect_intersection(rect, &(video_buffer->frame), &frame);
    if (rc != TIG_OK) {
        return rc;
    }

    rc = tig_video_buffer_lock(video_buffer);
    if (rc != TIG_OK) {
        return rc;
    }

    for (y = 0; y < frame.height; y++) {
        uint32_t* dst = (uint32_t*)video_buffer->surface->pixels
            + (video_buffer->surface->pitch / 4) * (y + frame.y)
            + frame.x;

        for (x = 0; x < frame.width; x++) {
            dst[x] = tig_color_blend_alpha(color, dst[x], alpha);
        }
    }

    tig_video_buffer_unlock(video_buffer);

    return TIG_OK;
}

// 0x522F30
int tig_video_buffer_get_pixel_color(TigVideoBuffer* video_buffer, int x, int y, unsigned int* color)
{
    int index;
    int rc;

    if (x < video_buffer->frame.x
        || y < video_buffer->frame.y
        || x >= video_buffer->frame.x + video_buffer->frame.width
        || y >= video_buffer->frame.y + video_buffer->frame.height) {
        return TIG_ERR_INVALID_PARAM;
    }

    rc = tig_video_buffer_lock(video_buffer);
    if (rc != TIG_OK) {
        return rc;
    }

    switch (tig_video_bpp) {
    case 32:
        index = y * video_buffer->surface->pitch + 4 * x;
        *color = *(uint32_t*)((uint8_t*)video_buffer->surface->pixels + index);
        break;
    }

    tig_video_buffer_unlock(video_buffer);

    return TIG_OK;
}

// 0x523120
int tig_video_buffer_tint(TigVideoBuffer* video_buffer, TigRect* rect, tig_color_t tint_color, TigVideoBufferTintMode mode)
{
    int rc;
    TigRect frame;
    int x;
    int y;

    if (mode >= TIG_VIDEO_BUFFER_TINT_MODE_COUNT) {
        return TIG_ERR_INVALID_PARAM;
    }

    if (tint_color == tig_color_make(0, 0, 0)
        && mode != TIG_VIDEO_BUFFER_TINT_MODE_GRAYSCALE) {
        return TIG_OK;
    }

    rc = tig_rect_intersection(rect, &(video_buffer->frame), &frame);
    if (rc != TIG_OK) {
        return rc;
    }

    rc = tig_video_buffer_lock(video_buffer);
    if (rc != TIG_OK) {
        return rc;
    }

    for (y = 0; y < frame.height; ++y) {
        switch (tig_video_bpp) {
        case 32:
            if (1) {
                uint32_t* dst = (uint32_t*)video_buffer->surface->pixels + (video_buffer->surface->pitch / 4) * (y + frame.y) + frame.x;
                uint32_t src_color;

                switch (mode) {
                case TIG_VIDEO_BUFFER_TINT_MODE_ADD:
                    for (x = 0; x < frame.width; ++x) {
                        src_color = *dst;
                        *dst++ = tig_color_add(tint_color, src_color);
                    }
                    break;
                case TIG_VIDEO_BUFFER_TINT_MODE_SUB:
                    for (x = 0; x < frame.width; ++x) {
                        src_color = *dst;
                        *dst++ = tig_color_sub(tint_color, src_color);
                    }
                    break;
                case TIG_VIDEO_BUFFER_TINT_MODE_MUL:
                    for (x = 0; x < frame.width; ++x) {
                        src_color = *dst;
                        *dst++ = tig_color_mul(tint_color, src_color);
                    }
                    break;
                case TIG_VIDEO_BUFFER_TINT_MODE_GRAYSCALE:
                    for (x = 0; x < frame.width; ++x) {
                        src_color = *dst;
                        *dst++ = tig_color_rgb_to_grayscale(src_color);
                    }
                    break;
                default:
                    // Should be unreachable.
                    abort();
                }
            }
            break;
        }
    }

    tig_video_buffer_unlock(video_buffer);
    return TIG_OK;
}

// 0x523930
int tig_video_buffer_save_to_bmp(TigVideoBufferSaveToBmpInfo* save_info)
{
    int rc;
    TigRect rect;

    if (save_info == NULL || save_info->video_buffer == NULL
        || save_info->video_buffer->surface == NULL) return TIG_ERR_INVALID_PARAM;

    if (save_info->rect != NULL) {
        rect = *save_info->rect;
    } else {
        rect.x = 0;
        rect.y = 0;
        rect.width = save_info->video_buffer->surface->w;
        rect.height = save_info->video_buffer->surface->h;
    }

    rc = tig_bmp_save_surface(save_info->video_buffer->surface,
        &rect,
        save_info->path);

    return rc;
}

// 0x5239D0
int tig_video_buffer_load_from_bmp(const char* filename, TigVideoBuffer** video_buffer_ptr, unsigned int flags)
{
    SDL_IOStream* io;
    SDL_Surface* surface;
    SDL_Rect blit_rect;
    TigVideoBufferCreateInfo vb_create_info;
    int rc;

    io = tig_file_io_open(filename, "rb");
    if (io == NULL) {
        return TIG_ERR_IO;
    }

    surface = SDL_LoadBMP_IO(io, true);
    if (surface == NULL) {
        return TIG_ERR_IO;
    }

    if ((flags & 0x1) != 0) {
        vb_create_info.flags = TIG_VIDEO_BUFFER_CREATE_SYSTEM_MEMORY;
        vb_create_info.width = surface->w;
        vb_create_info.height = surface->h;
        vb_create_info.color_key = 0;
        vb_create_info.background_color = 0;

        rc = tig_video_buffer_create(&vb_create_info, video_buffer_ptr);
        if (rc != TIG_OK) {
            SDL_DestroySurface(surface);

            return rc;
        }
    }

    if ((*video_buffer_ptr)->surface->w < surface->w || (*video_buffer_ptr)->surface->h < surface->h) {
        if ((flags & 0x1) != 0) {
            tig_video_buffer_destroy(*video_buffer_ptr);
        }

        SDL_DestroySurface(surface);

        return TIG_ERR_INVALID_PARAM;
    }

    blit_rect.x = 0;
    blit_rect.y = 0;
    blit_rect.w = surface->w;
    blit_rect.h = surface->h;

    if (!SDL_BlitSurface(surface, &blit_rect, (*video_buffer_ptr)->surface, &blit_rect)) {
        if ((flags & 0x1) != 0) {
            tig_video_buffer_destroy(*video_buffer_ptr);
        }

        SDL_DestroySurface(surface);

        return TIG_ERR_GENERIC;
    }

    SDL_DestroySurface(surface);

    return TIG_OK;
}

// 0x524080
bool tig_video_window_create(TigInitInfo* init_info)
{
    int pitch;
    size_t bytes;
    if (init_info == NULL || !tig_pixel_buffer_row_size(init_info->width, 32, &pitch)
        || !tig_pixel_buffer_size(init_info->width, init_info->height, 32, pitch, &bytes)) return false;
    const char* name = (init_info->flags & TIG_INITIALIZE_SET_WINDOW_NAME) != 0
        ? init_info->window_name
        : "TIG";

    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if ((init_info->flags & TIG_INITIALIZE_HIDDEN) != 0) flags |= SDL_WINDOW_HIDDEN;
    if ((init_info->flags & TIG_INITIALIZE_WINDOWED) == 0) {
        flags |= SDL_WINDOW_FULLSCREEN;
    } else {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

    double physical_width = init_info->width * (double)scale;
    double physical_height = init_info->height * (double)scale;
    if (!isfinite(physical_width) || !isfinite(physical_height)
        || physical_width < 1 || physical_height < 1 || physical_width > INT_MAX || physical_height > INT_MAX) return false;
    int window_width = (int)physical_width;
    int window_height = (int)physical_height;

#if SDL_PLATFORM_ANDROID || SDL_PLATFORM_IOS
    SDL_Rect display_bounds;
    SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &display_bounds);

    // Calculate the view size in logical pixels. Android reports display bounds
    // in physical pixels and provides the appropriate scale, while in iOS
    // display bounds are already in logical pixels with a scale of 1.0.
    init_info->width = (int)((float)display_bounds.w / scale + 0.5f);
    init_info->height = (int)((float)display_bounds.h / scale + 0.5f);

    const int min_height = 600;
    if (init_info->height < min_height) {
        // The logical resolution is too small to accommodate the base graphic.
        // This likely means we're on a mobile phone, where the height is
        // usually between 350 and 450 logical pixels.
        float width_scale = (float)min_height / (float)init_info->height;
        init_info->width = (int)((float)init_info->width * width_scale + 0.5f);
        init_info->height = min_height;
    }

    window_width = display_bounds.w;
    window_height = display_bounds.h;
    flags |= SDL_WINDOW_FULLSCREEN;
#endif

// We're using streaming texture which is extremely slow in Metal.
#if SDL_PLATFORM_MACOS
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl,metal");
#elif SDL_PLATFORM_IOS
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2,metal");
#endif

    if (!tig_pixel_buffer_row_size(init_info->width, 32, &pitch)
        || !tig_pixel_buffer_size(init_info->width, init_info->height, 32, pitch, &bytes)) return false;
    SDL_Window* window;
    SDL_Renderer* renderer;
    if (!SDL_CreateWindowAndRenderer(name, window_width, window_height, flags, &window, &renderer)) {
        return false;
    }

    SDL_PropertiesID renderer_props = SDL_GetRendererProperties(renderer);
    const char* driver_name = SDL_GetStringProperty(renderer_props, SDL_PROP_RENDERER_NAME_STRING, "");
    tig_debug_printf("TIG Video: Using '%s' driver\n", driver_name);

    if ((init_info->flags & TIG_INITIALIZE_POSITIONED) != 0) {
        SDL_SetWindowPosition(window, init_info->x, init_info->y);
    }

    if (!SDL_SetRenderVSync(renderer, 1)) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        return false;
    }

    if (!SDL_SetRenderLogicalPresentation(renderer, init_info->width, init_info->height, SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        return false;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, init_info->width, init_info->height);
    if (texture == NULL) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        return false;
    }

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);

    SDL_PropertiesID texture_props = SDL_GetTextureProperties(texture);
    SDL_PixelFormat format = (SDL_PixelFormat)SDL_GetNumberProperty(texture_props, SDL_PROP_TEXTURE_FORMAT_NUMBER, 0);

    SDL_Surface* surface = SDL_CreateSurface(init_info->width, init_info->height, format);
    if (surface == NULL) {
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        return false;
    }

    if (!tig_pixel_buffer_size(surface->w, surface->h, 32, surface->pitch, &bytes)) {
        SDL_DestroySurface(surface);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        return false;
    }
    tig_video_state.window = window;
    tig_video_state.renderer = renderer;
    tig_video_state.texture = texture;
    tig_video_state.surface = surface;
    tig_video_upload_reset(&tig_video_upload, surface->w, surface->h);

    tig_video_screen_bounds.x = 0;
    tig_video_screen_bounds.y = 0;
    tig_video_screen_bounds.width = init_info->width;
    tig_video_screen_bounds.height = init_info->height;

    return true;
}

// 0x5242F0
void tig_video_window_destroy(void)
{
    tig_video_gpu_cursor_clear();
    if (tig_video_sprite_cache) {
        TigSpriteCacheStats stats = tig_sprite_cache_stats(tig_video_sprite_cache);
        tig_debug_printf("GPU sprite cache: %llu hits, %llu uploads, %llu bytes, %llu evictions\n",
            (unsigned long long)stats.hits, (unsigned long long)stats.uploads,
            (unsigned long long)stats.uploaded_bytes, (unsigned long long)stats.evictions);
    }
    tig_sprite_cache_destroy(tig_video_sprite_cache);
    tig_video_sprite_cache = NULL;
    tig_video_gpu_cursor_enabled = false;
    if (tig_video_state.surface != NULL) {
        SDL_DestroySurface(tig_video_state.surface);
        tig_video_state.surface = NULL;
    }

    if (tig_video_state.texture != NULL) {
        SDL_DestroyTexture(tig_video_state.texture);
        tig_video_state.texture = NULL;
    }

    if (tig_video_state.renderer != NULL) {
        SDL_DestroyRenderer(tig_video_state.renderer);
        tig_video_state.renderer = NULL;
    }

    if (tig_video_state.window != NULL) {
        SDL_DestroyWindow(tig_video_state.window);
        tig_video_state.window = NULL;
    }
}

// 0x524830
bool tig_video_surface_color_masks_apply(void)
{
    int bpp;
    Uint32 r;
    Uint32 g;
    Uint32 b;
    Uint32 a;

    if (!SDL_GetMasksForPixelFormat(tig_video_state.surface->format, &bpp, &r, &g, &b, &a)) {
        return false;
    }

    if (tig_color_set_rgba_settings(r, g, b, a) != TIG_OK) {
        return false;
    }

    return true;
}

// 0x525250
int tig_video_screenshot_make_internal(int key)
{
    int rc;
    int index;
    char path[TIG_MAX_PATH];
    TigRect rect;

    if (tig_video_screenshot_key != key) {
        return TIG_ERR_GENERIC;
    }

    for (index = 0; index < INT_MAX; index++) {
        SDL_snprintf(path, sizeof(path), "screen%04d.bmp", index);
        if (!tig_file_exists(path, NULL)) {
            break;
        }
    }

    if (index == INT_MAX) {
        return TIG_ERR_IO;
    }

    rect.x = 0;
    rect.y = 0;
    rect.width = tig_video_state.surface->w;
    rect.height = tig_video_state.surface->h;

    SDL_Surface* snapshot = NULL;
    if (tig_video_gpu_cursor.texture && tig_video_gpu_cursor.buffer) {
        snapshot = SDL_ConvertSurface(tig_video_state.surface, tig_video_state.surface->format);
        if (!snapshot) return TIG_ERR_GENERIC;
        SDL_Rect source = { tig_video_gpu_cursor.source.x, tig_video_gpu_cursor.source.y,
            tig_video_gpu_cursor.source.width, tig_video_gpu_cursor.source.height };
        SDL_Rect destination = { tig_video_gpu_cursor.destination.x, tig_video_gpu_cursor.destination.y,
            tig_video_gpu_cursor.destination.width, tig_video_gpu_cursor.destination.height };
        if (!SDL_BlitSurface(tig_video_gpu_cursor.buffer->surface, &source, snapshot, &destination)) {
            SDL_DestroySurface(snapshot);
            return TIG_ERR_GENERIC;
        }
    }
    rc = tig_bmp_save_surface(snapshot ? snapshot : tig_video_state.surface, &rect, path);
    SDL_DestroySurface(snapshot);

    return rc;
}


