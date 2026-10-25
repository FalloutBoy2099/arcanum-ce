#include "game/zoom_damage.h"

void zoom_damage_init(TigVideoUpload* plan, int width, int height)
{
    memset(plan, 0, sizeof(*plan));
    plan->bounds = (SDL_Rect){ 0, 0, width, height };
}

void zoom_damage_screen(TigVideoUpload* plan, TigRect dirty)
{
    if (plan == NULL || plan->bounds.x != 0 || plan->bounds.y != 0
        || plan->bounds.w <= 0 || plan->bounds.h <= 0 || dirty.width <= 0 || dirty.height <= 0) return;
    if (plan->count < 0 || plan->count > TIG_VIDEO_UPLOAD_RECTS) {
        tig_video_upload_reset(plan, plan->bounds.w, plan->bounds.h);
        return;
    }
    int64_t left = SDL_max((int64_t)dirty.x, 0);
    int64_t top = SDL_max((int64_t)dirty.y, 0);
    int64_t right = SDL_min((int64_t)dirty.x + dirty.width, plan->bounds.w);
    int64_t bottom = SDL_min((int64_t)dirty.y + dirty.height, plan->bounds.h);
    if (left < right && top < bottom) {
        tig_video_upload_mark(plan, (SDL_Rect){ (int)left, (int)top, (int)(right - left), (int)(bottom - top) });
    }
}

void zoom_damage_world(TigVideoUpload* plan, TigRect source, TigRect destination, TigRect dirty)
{
    if (plan == NULL || plan->bounds.x != 0 || plan->bounds.y != 0
        || plan->bounds.w <= 0 || plan->bounds.h <= 0
        || source.width <= 0 || source.height <= 0
        || destination.width <= 0 || destination.height <= 0
        || dirty.width <= 0 || dirty.height <= 0) return;
    int64_t left = SDL_max((int64_t)dirty.x - 1, (int64_t)source.x);
    int64_t top = SDL_max((int64_t)dirty.y - 1, (int64_t)source.y);
    int64_t right = SDL_min((int64_t)dirty.x + dirty.width + 1, (int64_t)source.x + source.width);
    int64_t bottom = SDL_min((int64_t)dirty.y + dirty.height + 1, (int64_t)source.y + source.height);
    if (left >= right || top >= bottom) return;
    left = (int64_t)destination.x + (left - source.x) * destination.width / source.width - 2;
    top = (int64_t)destination.y + (top - source.y) * destination.height / source.height - 2;
    right = (int64_t)destination.x + ((right - source.x) * destination.width + source.width - 1) / source.width + 2;
    bottom = (int64_t)destination.y + ((bottom - source.y) * destination.height + source.height - 1) / source.height + 2;
    left = SDL_max(left, 0);
    top = SDL_max(top, 0);
    right = SDL_min(right, plan->bounds.w);
    bottom = SDL_min(bottom, plan->bounds.h);
    if (left < right && top < bottom) {
        zoom_damage_screen(plan, (TigRect){ (int)left, (int)top, (int)(right - left), (int)(bottom - top) });
    }
}

void zoom_damage_finish(TigVideoUpload* plan, ZoomDamageState* state, const ZoomMapping* mapping)
{
    if (plan == NULL || state == NULL || mapping == NULL) return;
    const ZoomMapping* old = &state->mapping;
    bool changed = !state->valid || memcmp(&old->source, &mapping->source, sizeof(TigRect)) != 0
        || memcmp(&old->destination, &mapping->destination, sizeof(TigRect)) != 0
        || old->source_buffer != mapping->source_buffer || old->destination_buffer != mapping->destination_buffer
        || old->camera_x != mapping->camera_x || old->camera_y != mapping->camera_y || old->linear != mapping->linear;
    Uint64 threshold = plan->bounds.w > 0 && plan->bounds.h > 0
        ? ((Uint64)plan->bounds.w * plan->bounds.h + 3) / 4 : 0;
    Uint64 area = 0;
    bool invalid = plan->count < 0 || plan->count > TIG_VIDEO_UPLOAD_RECTS;
    if (!invalid) {
        for (int i = 0; i < plan->count; i++) {
            const SDL_Rect* rect = &plan->rects[i];
            if (rect->x < 0 || rect->y < 0 || rect->w <= 0 || rect->h <= 0
                || rect->w > plan->bounds.w || rect->h > plan->bounds.h
                || rect->x > plan->bounds.w - rect->w || rect->y > plan->bounds.h - rect->h) {
                invalid = true;
                break;
            }
            Uint64 rectangle_area = (Uint64)rect->w * rect->h;
            if (rectangle_area >= threshold - area) {
                area = threshold;
                break;
            }
            area += rectangle_area;
        }
    }
    if (changed || invalid || area >= threshold) {
        tig_video_upload_reset(plan, plan->bounds.w, plan->bounds.h);
    }
    state->mapping = *mapping;
    state->valid = true;
}
