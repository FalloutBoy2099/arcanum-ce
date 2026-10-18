#include "tig/color_grade.h"

#ifdef SDL_PLATFORM_WINDOWS
#include "color_grade_dxil.h"
#endif

static SDL_GPUDevice* color_grade_device;
static SDL_Renderer* color_grade_renderer;
static SDL_GPUShader* color_grade_shader;
static SDL_GPURenderState* color_grade_render_state;
typedef struct WorldShaderBuffers {
    SDL_Texture* input;
    SDL_Texture* output;
    SDL_Surface* staging;
    int width;
    int height;
    SDL_PixelFormat format;
} WorldShaderBuffers;

static WorldShaderBuffers world_shader_buffers[2];
static bool color_grade_attempted;
static bool color_grade_failed;

typedef struct WorldShaderUniforms {
    TigColorGrade grade;
    float water_time;
    float water_strength;
    float origin_x;
    float origin_y;
} WorldShaderUniforms;

static bool world_shader_apply(SDL_Surface* frame, SDL_Surface* mask,
    const TigVideoUpload* damage, const WorldShaderUniforms* uniforms);

void tig_color_grade_shutdown(void)
{
    for (int i = 0; i < 2; i++) {
        SDL_DestroyTexture(world_shader_buffers[i].input);
        SDL_DestroyTexture(world_shader_buffers[i].output);
        SDL_DestroySurface(world_shader_buffers[i].staging);
    }
    SDL_memset(world_shader_buffers, 0, sizeof(world_shader_buffers));
    if (color_grade_render_state) SDL_DestroyGPURenderState(color_grade_render_state);
    if (color_grade_shader) SDL_ReleaseGPUShader(color_grade_device, color_grade_shader);
    SDL_DestroyRenderer(color_grade_renderer);
    if (color_grade_device) SDL_DestroyGPUDevice(color_grade_device);
    color_grade_device = NULL;
    color_grade_renderer = NULL;
    color_grade_shader = NULL;
    color_grade_render_state = NULL;
    color_grade_attempted = false;
    color_grade_failed = false;
}

static bool color_grade_initialize(void)
{
    if (color_grade_attempted) return color_grade_render_state != NULL && !color_grade_failed;
    color_grade_attempted = true;
#ifdef SDL_PLATFORM_WINDOWS
    color_grade_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL, false, "direct3d12");
    if (color_grade_device) color_grade_renderer = SDL_CreateGPURenderer(color_grade_device, NULL);
    if (color_grade_renderer) {
        SDL_GPUShaderCreateInfo shader = { 0 };
        shader.code = color_grade_dxil;
        shader.code_size = sizeof(color_grade_dxil);
        shader.entrypoint = "main";
        shader.format = SDL_GPU_SHADERFORMAT_DXIL;
        shader.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        shader.num_samplers = 1;
        shader.num_uniform_buffers = 1;
        color_grade_shader = SDL_CreateGPUShader(color_grade_device, &shader);
    }
    if (color_grade_shader) {
        SDL_GPURenderStateCreateInfo state = { 0 };
        state.fragment_shader = color_grade_shader;
        color_grade_render_state = SDL_CreateGPURenderState(color_grade_renderer, &state);
    }
#endif
    if (color_grade_render_state == NULL) {
        SDL_Log("Day/night shader unavailable, using original rendering: %s", SDL_GetError());
        color_grade_failed = true;
        return false;
    }
    SDL_Surface* probe = SDL_CreateSurface(2, 2, SDL_PIXELFORMAT_XRGB8888);
    TigVideoUpload probe_damage;
    tig_video_upload_reset(&probe_damage, 2, 2);
    TigColorGrade probe_grade = { 0.5f, 0.75f, 1.0f, 1.0f };
    bool valid = probe != NULL && SDL_FillSurfaceRect(probe, NULL, 0x808080)
        && tig_color_grade_apply(probe, &probe_damage, &probe_grade);
    if (valid) {
        Uint32 pixel = *(Uint32*)probe->pixels;
        valid = abs((int)((pixel >> 16) & 255) - 64) <= 2
            && abs((int)((pixel >> 8) & 255) - 96) <= 2
            && abs((int)(pixel & 255) - 128) <= 2;
    }
    SDL_DestroySurface(probe);
    if (!valid) {
        color_grade_failed = true;
        SDL_Log("Day/night shader self-test failed, using original rendering");
        return false;
    }
    SDL_Log("Day/night shader initialized on %s", SDL_GetGPUDeviceDriver(color_grade_device));
    return true;
}

static WorldShaderBuffers* color_grade_resize(SDL_Surface* frame, bool water)
{
    WorldShaderBuffers* buffers = &world_shader_buffers[water ? 1 : 0];
    if (frame->w == buffers->width && frame->h == buffers->height && frame->format == buffers->format) return buffers;
    SDL_SetRenderTarget(color_grade_renderer, NULL);
    SDL_DestroyTexture(buffers->input);
    SDL_DestroyTexture(buffers->output);
    SDL_DestroySurface(buffers->staging);
    SDL_memset(buffers, 0, sizeof(*buffers));
    buffers->staging = SDL_CreateSurface(frame->w, frame->h, SDL_PIXELFORMAT_ARGB8888);
    buffers->input = SDL_CreateTexture(color_grade_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, frame->w, frame->h);
    buffers->output = SDL_CreateTexture(color_grade_renderer, frame->format, SDL_TEXTUREACCESS_TARGET, frame->w, frame->h);
    if (!buffers->input || !buffers->output || !buffers->staging) return NULL;
    SDL_SetTextureBlendMode(buffers->input, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(buffers->input, water ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
    buffers->width = frame->w;
    buffers->height = frame->h;
    buffers->format = frame->format;
    return buffers;
}

bool tig_color_grade_apply(SDL_Surface* frame, const TigVideoUpload* damage, const TigColorGrade* grade)
{
    if (!frame || !damage || !grade || SDL_BYTESPERPIXEL(frame->format) != 4) return false;
    if (damage->count == 0 || (grade->red == 1.0f && grade->green == 1.0f && grade->blue == 1.0f && grade->saturation == 1.0f)) return true;
    WorldShaderUniforms uniforms = { *grade, 0, 0, 0, 0 };
    return world_shader_apply(frame, NULL, damage, &uniforms);
}

bool tig_water_shader_apply(SDL_Surface* frame, SDL_Surface* mask, const TigVideoUpload* damage,
    float time, float strength, float origin_x, float origin_y)
{
    if (!frame || !mask || !damage || mask->format != SDL_PIXELFORMAT_INDEX8
        || frame->w != mask->w || frame->h != mask->h || SDL_BYTESPERPIXEL(frame->format) != 4) return false;
    if (strength <= 0 || damage->count == 0) return true;
    SDL_Rect bounds = damage->rects[0];
    for (int i = 1; i < damage->count; i++) SDL_GetRectUnion(&bounds, &damage->rects[i], &bounds);
    SDL_Rect frame_bounds = { 0, 0, frame->w, frame->h };
    if (!SDL_GetRectIntersection(&bounds, &frame_bounds, &bounds)) return true;
    SDL_Surface* region = SDL_CreateSurfaceFrom(bounds.w, bounds.h, frame->format,
        (Uint8*)frame->pixels + bounds.y * frame->pitch + bounds.x * 4, frame->pitch);
    SDL_Surface* region_mask = SDL_CreateSurfaceFrom(bounds.w, bounds.h, mask->format,
        (Uint8*)mask->pixels + bounds.y * mask->pitch + bounds.x, mask->pitch);
    TigVideoUpload regions = { 0 };
    regions.bounds = (SDL_Rect){ 0, 0, bounds.w, bounds.h };
    for (int i = 0; i < damage->count; i++) {
        SDL_Rect rect;
        if (SDL_GetRectIntersection(&damage->rects[i], &bounds, &rect)) {
            rect.x -= bounds.x;
            rect.y -= bounds.y;
            regions.rects[regions.count++] = rect;
        }
    }
    WorldShaderUniforms uniforms = { { 1, 1, 1, 1 }, time, SDL_clamp(strength, 0.0f, 1.0f), origin_x - bounds.x, origin_y - bounds.y };
    bool success = region && region_mask && world_shader_apply(region, region_mask, &regions, &uniforms);
    SDL_DestroySurface(region);
    SDL_DestroySurface(region_mask);
    return success;
}

static bool world_shader_apply(SDL_Surface* frame, SDL_Surface* mask,
    const TigVideoUpload* damage, const WorldShaderUniforms* uniforms)
{
    if (!color_grade_initialize()) return false;
    WorldShaderBuffers* buffers = color_grade_resize(frame, mask != NULL);
    if (!buffers) return false;
    if (!SDL_ConvertPixels(frame->w, frame->h, frame->format, frame->pixels, frame->pitch,
            buffers->staging->format, buffers->staging->pixels, buffers->staging->pitch)) return false;
    if (mask) {
        for (int y = 0; y < frame->h; y++) {
            Uint32* row = (Uint32*)((Uint8*)buffers->staging->pixels + y * buffers->staging->pitch);
            Uint8* mask_row = (Uint8*)mask->pixels + y * mask->pitch;
            for (int x = 0; x < frame->w; x++) row[x] = (row[x] & 0xFFFFFF) | ((Uint32)mask_row[x] << 24);
        }
    }

    SDL_Rect viewport = { 0, 0, frame->w, frame->h };
    SDL_FRect destination = { 0, 0, (float)frame->w, (float)frame->h };
    bool success = SDL_UpdateTexture(buffers->input, NULL, buffers->staging->pixels, buffers->staging->pitch)
        && SDL_SetRenderTarget(color_grade_renderer, buffers->output)
        && SDL_SetRenderViewport(color_grade_renderer, &viewport)
        && SDL_SetRenderDrawColor(color_grade_renderer, 0, 0, 0, 255)
        && SDL_RenderClear(color_grade_renderer)
        && SDL_SetGPURenderStateFragmentUniforms(color_grade_render_state, 0, uniforms, sizeof(*uniforms))
        && SDL_SetGPURenderState(color_grade_renderer, color_grade_render_state)
        && SDL_RenderTexture(color_grade_renderer, buffers->input, NULL, &destination);
    SDL_Rect bounds = damage->rects[0];
    for (int part = 1; part < damage->count; part++) SDL_GetRectUnion(&bounds, &damage->rects[part], &bounds);
    SDL_Surface* readback = success ? SDL_RenderReadPixels(color_grade_renderer, &bounds) : NULL;
    SDL_Surface* converted = readback ? SDL_ConvertSurface(readback, frame->format) : NULL;
    if (converted) {
        for (int part = 0; part < damage->count; part++) {
            const SDL_Rect* region = &damage->rects[part];
            for (int y = 0; y < region->h; y++) {
                SDL_memcpy((Uint8*)frame->pixels + (region->y + y) * frame->pitch + region->x * 4,
                    (Uint8*)converted->pixels + (region->y - bounds.y + y) * converted->pitch + (region->x - bounds.x) * 4,
                    region->w * 4);
            }
        }
    }
    success = converted != NULL;
    SDL_DestroySurface(converted);
    SDL_DestroySurface(readback);
    SDL_SetGPURenderState(color_grade_renderer, NULL);
    SDL_RenderPresent(color_grade_renderer);
    if (!success) {
        SDL_Log("Day/night shader failed, using original rendering: %s", SDL_GetError());
        color_grade_failed = true;
    }
    return success;
}
