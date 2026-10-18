#include "game/zoom_damage.h"

void zoom_damage_init(TigVideoUpload* plan, int width, int height)
{
    memset(plan, 0, sizeof(*plan));
    plan->bounds = (SDL_Rect){ 0, 0, width, height };
}

void zoom_damage_screen(TigVideoUpload* plan, TigRect dirty)
{
    tig_video_upload_mark(plan, (SDL_Rect){ dirty.x, dirty.y, dirty.width, dirty.height });
}

void zoom_damage_world(TigVideoUpload* plan, TigRect source, TigRect destination, TigRect dirty)
{
    if (source.width <= 0 || source.height <= 0) return;
    SDL_Rect src = { source.x, source.y, source.width, source.height };
    SDL_Rect changed = { dirty.x, dirty.y, dirty.width, dirty.height };
    // Linear interpolation may sample a neighboring source pixel. Expand before
    // clipping, then round outward in destination coordinates with extra margin.
    changed.x--; changed.y--; changed.w += 2; changed.h += 2;
    if (!SDL_GetRectIntersection(&changed, &src, &changed)) return;
    int left = (int)((int64_t)(changed.x - source.x) * destination.width / source.width) - 2;
    int top = (int)((int64_t)(changed.y - source.y) * destination.height / source.height) - 2;
    int right = (int)(((int64_t)(changed.x + changed.w - source.x) * destination.width + source.width - 1) / source.width) + 2;
    int bottom = (int)(((int64_t)(changed.y + changed.h - source.y) * destination.height + source.height - 1) / source.height) + 2;
    zoom_damage_screen(plan, (TigRect){ destination.x + left, destination.y + top, right - left, bottom - top });
}

void zoom_damage_finish(TigVideoUpload* plan, ZoomDamageState* state, const ZoomMapping* mapping)
{
    const ZoomMapping* old = &state->mapping;
    bool changed = !state->valid || memcmp(&old->source, &mapping->source, sizeof(TigRect)) != 0
        || memcmp(&old->destination, &mapping->destination, sizeof(TigRect)) != 0
        || old->source_buffer != mapping->source_buffer || old->destination_buffer != mapping->destination_buffer
        || old->camera_x != mapping->camera_x || old->camera_y != mapping->camera_y || old->linear != mapping->linear;
    Uint64 area = 0;
    for (int i = 0; i < plan->count; i++) area += (Uint64)plan->rects[i].w * plan->rects[i].h;
    if (changed || area * 4 >= (Uint64)plan->bounds.w * plan->bounds.h) {
        tig_video_upload_reset(plan, plan->bounds.w, plan->bounds.h);
    }
    state->mapping = *mapping;
    state->valid = true;
}
