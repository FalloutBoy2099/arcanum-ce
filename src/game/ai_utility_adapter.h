#ifndef ARCANUM_GAME_AI_UTILITY_ADAPTER_H_
#define ARCANUM_GAME_AI_UTILITY_ADAPTER_H_

#include "game/ai_private.h"

void ai_utility_reset(void);
int ai_utility_mode(Ai* ai);
bool ai_utility_spell_select(Ai* ai, bool apply);
bool ai_utility_retreat_pending(Ai* ai);
bool ai_utility_action_perform(Ai* ai);

#endif
