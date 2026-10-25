#ifndef ARCANUM_GAME_FLOATING_BACKGROUND_H_
#define ARCANUM_GAME_FLOATING_BACKGROUND_H_

#include "game/context.h"

int floating_margin_horizontal(void);
int floating_margin_vertical(void);
TigRect floating_background_pad(TigRect text, TigRect buffer_bounds);
void floating_background_draw(tig_window_handle_t window, TigRect* rect, uint8_t text_opacity);

#endif
