#ifndef ARCANUM_GAME_TERRAIN_ENCODING_H_
#define ARCANUM_GAME_TERRAIN_ENCODING_H_

#include <stdint.h>
#include <SDL3/SDL.h>

// Legacy terrain.tdf layout: from:5, to:5, transition mask:4, variant:2.
static inline uint16_t terrain_encode(int from, int to, int mask, int variant)
{
    return (uint16_t)((SDL_clamp(from, 0, 31) << 11)
        | (SDL_clamp(to, 0, 31) << 6)
        | (SDL_clamp(mask, 0, 15) << 2)
        | SDL_clamp(variant, 0, 3));
}

#endif /* ARCANUM_GAME_TERRAIN_ENCODING_H_ */
