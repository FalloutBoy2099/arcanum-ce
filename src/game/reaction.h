#ifndef ARCANUM_GAME_REACTION_H_
#define ARCANUM_GAME_REACTION_H_

#include "game/context.h"

typedef enum Reaction {
    REACTION_LOVE,
    REACTION_AMIABLE,
    REACTION_COURTEOUS,
    REACTION_NEUTRAL,
    REACTION_SUSPICIOUS,
    REACTION_DISLIKE,
    REACTION_HATRED,
    REACTION_COUNT,
} Reaction;

bool reaction_init(GameInitInfo* init_info);
void reaction_exit(void);
bool reaction_met_before(int64_t npc_obj, int64_t pc_obj);
int reaction_get(int64_t npc_obj, int64_t pc_obj);
int reaction_get_with_limited_penalty(int64_t npc_obj, int64_t pc_obj);
void reaction_adj(int64_t npc_obj, int64_t pc_obj, int value);
void reaction_adj_ex(int64_t npc_obj, int64_t pc_obj, int value, bool apply_effects);
void reaction_forget(int64_t npc_obj, int64_t pc_obj);
int reaction_translate(int value);
const char* reaction_get_name(int reaction);
void reaction_begin_conversation(int64_t pc, int64_t npc);
void reaction_end_conversation(int64_t pc, int64_t npc);
int64_t reaction_conversation_pc_get(int64_t npc);
int reaction_adjust_price(int64_t a1, int64_t a2, int a3);
void reaction_adjust_for_trade_surplus(int64_t a1, int64_t a2, int a3);

#endif /* ARCANUM_GAME_REACTION_H_ */
