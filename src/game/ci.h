#ifndef ARCANUM_GAME_CI_H_
#define ARCANUM_GAME_CI_H_

#include "game/context.h"

bool ci_init(GameInitInfo* init_info);
void ci_exit(void);
void ci_counter_increment(void);
void ci_counter_decrement(void);
int ci_counter_get(void);
void ci_reserved_noop(void);
void ci_redraw(void);

#endif /* ARCANUM_GAME_CI_H_ */
