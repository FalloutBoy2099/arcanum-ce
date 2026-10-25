#include "tig/video_upload.h"
#include "tig/pixel_buffer.h"

void tig_video_upload_reset(TigVideoUpload* upload, int width, int height)
{
    upload->bounds = (SDL_Rect){ 0, 0, width, height };
    upload->rects[0] = upload->bounds;
    upload->count = 1;
}

void tig_video_upload_mark(TigVideoUpload* upload, SDL_Rect rect)
{
    SDL_Rect clipped;
    if (!SDL_GetRectIntersection(&rect, &upload->bounds, &clipped)) return;
    rect = clipped;
    for (int i = 0; i < upload->count;) {
        SDL_Rect* previous = &upload->rects[i];
        // Merge touching rectangles as well, bounding the number of SDL calls.
        if (rect.x <= previous->x + previous->w && previous->x <= rect.x + rect.w
            && rect.y <= previous->y + previous->h && previous->y <= rect.y + rect.h) {
            SDL_GetRectUnion(&rect, previous, &clipped);
            rect = clipped;
            upload->rects[i] = upload->rects[--upload->count];
            i = 0; // A union may now touch an earlier rectangle.
        } else i++;
    }
    Uint64 area = (Uint64)rect.w * rect.h;
    for (int i = 0; i < upload->count; i++) area += (Uint64)upload->rects[i].w * upload->rects[i].h;
    if (upload->count == TIG_VIDEO_UPLOAD_RECTS
        || area * 5 >= (Uint64)upload->bounds.w * upload->bounds.h * 3) {
        upload->rects[0] = upload->bounds;
        upload->count = 1;
    } else upload->rects[upload->count++] = rect;
}

bool tig_video_upload_flush(TigVideoUpload* upload, SDL_Texture* texture, SDL_Surface* surface, Uint64* bytes)
{
    if (bytes != NULL) *bytes = 0;
    size_t surface_bytes;
    if (upload == NULL || surface == NULL || surface->pixels == NULL
        || upload->count < 0 || upload->count > TIG_VIDEO_UPLOAD_RECTS
        || !tig_pixel_buffer_size(surface->w, surface->h, 32, surface->pitch, &surface_bytes)
        || SDL_BYTESPERPIXEL(surface->format) != 4
        || surface->w != upload->bounds.w || surface->h != upload->bounds.h) return false;
    for (int i = 0; i < upload->count; i++) {
        const SDL_Rect* rect = &upload->rects[i];
        if (rect->x < 0 || rect->y < 0 || rect->w <= 0 || rect->h <= 0
            || rect->w > surface->w || rect->h > surface->h
            || rect->x > surface->w - rect->w || rect->y > surface->h - rect->h) return false;
    }
    for (int i = 0; i < upload->count; i++) {
        const SDL_Rect* rect = &upload->rects[i];
        const Uint8* pixels = (const Uint8*)surface->pixels + (size_t)rect->y * surface->pitch + rect->x * 4;
        if (!SDL_UpdateTexture(texture, rect, pixels, surface->pitch)) return false;
        if (bytes != NULL) *bytes += (Uint64)rect->w * rect->h * 4;
    }
    upload->count = 0;
    return true;
}
