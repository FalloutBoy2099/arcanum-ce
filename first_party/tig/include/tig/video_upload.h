#ifndef TIG_VIDEO_UPLOAD_H_
#define TIG_VIDEO_UPLOAD_H_

#include <SDL3/SDL.h>

#define TIG_VIDEO_UPLOAD_RECTS 32
typedef struct TigVideoUpload {
    SDL_Rect bounds;
    SDL_Rect rects[TIG_VIDEO_UPLOAD_RECTS];
    int count;
} TigVideoUpload;

// Reset when creating/recreating a texture: its initial contents are undefined.
void tig_video_upload_reset(TigVideoUpload* upload, int width, int height);
void tig_video_upload_mark(TigVideoUpload* upload, SDL_Rect rect);
// Only accepts the engine's 32-bit surface format; retains dirt on failure.
bool tig_video_upload_flush(TigVideoUpload* upload, SDL_Texture* texture, SDL_Surface* surface, Uint64* bytes);

#endif
