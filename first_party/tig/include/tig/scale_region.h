#ifndef TIG_SCALE_REGION_H_
#define TIG_SCALE_REGION_H_

#include <SDL3/SDL.h>

// Opaque XRGB8888 only, no blending/modulation. Keep the full source/destination
// transform while updating a destination subregion. False requests SDL fallback.
bool tig_scale_region(SDL_Surface* src, SDL_Rect source, SDL_Surface* dst,
    SDL_Rect destination, SDL_Rect region, SDL_ScaleMode mode);

#endif
