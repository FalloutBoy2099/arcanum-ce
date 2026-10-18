#ifndef ARCANUM_GAME_LI_H_
#define ARCANUM_GAME_LI_H_

#include "game/context.h"

bool li_init(GameInitInfo* init_info);
void li_exit(void);
void li_resize(GameResizeInfo* resize_info);
void li_visibility_suppress_push(void);
void li_visibility_suppress_pop(void);
int li_visibility_suppress_count(void);
void li_update(void);
void li_redraw(void);

#endif /* ARCANUM_GAME_LI_H_ */
