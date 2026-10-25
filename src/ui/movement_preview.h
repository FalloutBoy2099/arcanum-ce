#ifndef ARCANUM_UI_MOVEMENT_PREVIEW_H_
#define ARCANUM_UI_MOVEMENT_PREVIEW_H_

#include "game/context.h"

void movement_preview_update(tig_window_handle_t window, int64_t character, int64_t destination);
void movement_preview_draw(GameDrawInfo* draw_info);
void movement_preview_clear(void);
void movement_preview_destroy(void);

#endif
