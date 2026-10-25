#include "game/world_composition.h"
#include <limits.h>

int world_composition_present(TigVideoBuffer* world, TigRect source, TigVideoBuffer* output,
    TigRect destination, TigVideoUpload* damage, bool linear)
{
    if (world == NULL || output == NULL || world == output || damage == NULL
        || source.width <= 0 || source.height <= 0
        || destination.width <= 0 || destination.height <= 0
        || damage->count < 0 || damage->count > TIG_VIDEO_UPLOAD_RECTS) return TIG_ERR_INVALID_PARAM;

    for (int part = 0; part < damage->count; part++) {
        const SDL_Rect* rect = &damage->rects[part];
        if (rect->x < 0 || rect->y < 0 || rect->w <= 0 || rect->h <= 0
            || rect->w > damage->bounds.w || rect->h > damage->bounds.h
            || rect->x > damage->bounds.w - rect->w || rect->y > damage->bounds.h - rect->h) return TIG_ERR_INVALID_PARAM;
    }

    TigVideoBufferBlitInfo blit = { 0 };
    blit.src_video_buffer = world;
    blit.src_rect = &source;
    blit.dst_video_buffer = output;
    blit.dst_rect = &destination;
    blit.flags = linear ? TIG_VIDEO_BUFFER_BLIT_SCALE_LINEAR : 0;
    bool full_copy = damage->count == 1
        && damage->rects[0].x == destination.x && damage->rects[0].y == destination.y
        && damage->rects[0].w == destination.width && damage->rects[0].h == destination.height;

    if (!full_copy) {
        for (int part = 0; part < damage->count; part++) {
            SDL_Rect* region = &damage->rects[part];
            TigRect clipped = { region->x, region->y, region->w, region->h };
            if (tig_video_buffer_scale_region(&blit, &clipped) != TIG_OK) {
                full_copy = true;
                break;
            }
        }
    }

    if (!full_copy) {
        tig_video_buffer_color_grade(output, damage);
        return TIG_OK;
    }
    int result = tig_video_buffer_blit(&blit);
    if (result == TIG_OK) {
        tig_video_upload_reset(damage, damage->bounds.w, damage->bounds.h);
        tig_video_buffer_color_grade(output, damage);
    }
    return result;
}

int world_composition_scroll(TigVideoBuffer* world, TigRect bounds, int dx, int dy)
{
    if (world == NULL || bounds.width <= 0 || bounds.height <= 0) return TIG_ERR_INVALID_PARAM;
    if (bounds.x < 0 || bounds.y < 0
        || bounds.x > INT_MAX - bounds.width || bounds.y > INT_MAX - bounds.height) return TIG_ERR_INVALID_PARAM;
    if (dx <= -bounds.width || dx >= bounds.width || dy <= -bounds.height || dy >= bounds.height) {
        return TIG_ERR_INVALID_PARAM;
    }
    if (dx == 0 && dy == 0) return TIG_OK;

    TigRect source = bounds;
    TigRect destination = bounds;
    source.x += dx < 0 ? -dx : 0;
    source.y += dy < 0 ? -dy : 0;
    destination.x += dx > 0 ? dx : 0;
    destination.y += dy > 0 ? dy : 0;
    source.width -= dx < 0 ? -dx : dx;
    source.height -= dy < 0 ? -dy : dy;
    destination.width = source.width;
    destination.height = source.height;
    TigVideoBufferBlitInfo blit = { 0 };
    blit.src_video_buffer = world;
    blit.src_rect = &source;
    blit.dst_video_buffer = world;
    blit.dst_rect = &destination;
    return tig_video_buffer_blit(&blit);
}
