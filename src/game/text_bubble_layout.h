#ifndef ARCANUM_GAME_TEXT_BUBBLE_LAYOUT_H_
#define ARCANUM_GAME_TEXT_BUBBLE_LAYOUT_H_

#include <SDL3/SDL.h>

static inline void text_bubble_dimensions(float font_scale, int viewport_width,
    int viewport_height, int* width, int* height)
{
    float scale = SDL_clamp(font_scale, 1.0f, 6.0f);
    int max_width = SDL_max(1, viewport_width * 3 / 4 - 20);
    int max_height = SDL_max(1, viewport_height - 20);
    *width = SDL_min((int)(320.0f * scale + 0.5f), max_width);
    *height = SDL_min((int)(200.0f * scale + 0.5f), max_height);
}

#endif /* ARCANUM_GAME_TEXT_BUBBLE_LAYOUT_H_ */
