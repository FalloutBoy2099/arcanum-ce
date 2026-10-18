#ifndef ARCANUM_GAME_DIALOG_H_
#define ARCANUM_GAME_DIALOG_H_

#include "game/context.h"
#include "game/obj.h"
#include "game/object.h"

#define DIALOG_MAX_OPTIONS 10

typedef struct DialogState {
    /* 0000 */ int dlg;
    /* 0008 */ int64_t pc_obj;
    /* 0010 */ ObjectReference pc_reference;
    /* 0038 */ int64_t npc_obj;
    /* 0040 */ ObjectReference npc_reference;
    /* 0068 */ int num;
    /* 006C */ int script_num;
    /* 0070 */ char reply[1000];
    /* 0458 */ int speech_id;
    /* 045C */ int num_options;
    /* 0460 */ char options[DIALOG_MAX_OPTIONS][1000];
    /* 17E8 */ int turn_status;
    /* 17EC */ int response_value;
    /* 17F0 */ int option_response_types[DIALOG_MAX_OPTIONS];
    /* 1804 */ int option_response_values[DIALOG_MAX_OPTIONS];
    /* 1818 */ int option_response_extra_values[DIALOG_MAX_OPTIONS];
    /* 182C */ char* actions[DIALOG_MAX_OPTIONS];
    /* 1840 */ int npc_response_value;
    /* 1844 */ unsigned int seed;
} DialogState;

bool dialog_init(GameInitInfo* init_info);
void dialog_exit(void);
bool dialog_load(const char* path, int* dlg_ptr);
void dialog_unload(int dlg);
void dialog_cache_entry_free(int dlg);
bool dialog_conversation_begin(DialogState* state);
void dialog_option_select(DialogState* state, int index);
void dialog_conversation_end(DialogState* state);
void dialog_copy_npc_greeting_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_farewell_msg(int64_t npc_obj, int64_t pc_obj, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_sell_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_wont_sell_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_normally_wont_sell_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_buy_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_wont_buy_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_wont_buy_stolen_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_normally_wont_buy_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_buy_for_less_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_not_enough_money_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_let_me_handle_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_prepare_npc_reply(DialogState* state, bool a2);
void dialog_copy_npc_wont_follow_msg(int64_t npc_obj, int64_t pc_obj, int reason, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_order_ok_msg(int64_t npc_obj, int64_t pc_obj, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_order_no_msg(int64_t npc_obj, int64_t pc_obj, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_insult_msg(int64_t npc_obj, int64_t pc_obj, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_accidental_attack_msg(int64_t npc_obj, int64_t pc_obj, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_fleeing_msg(int64_t npc_obj, int64_t pc_obj, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_witness_pc_critical_msg(int64_t npc_obj, int64_t pc_obj, int type, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_gamble_msg(int64_t npc_obj, int64_t pc_obj, int type, char* buffer);
void dialog_copy_npc_warning_msg(int64_t npc_obj, int64_t pc_obj, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_newspaper_msg(int64_t npc_obj, int64_t pc_obj, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_warning_follow_msg(int64_t npc_obj, int64_t pc_obj, int reason, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_upset_attacking_msg(int64_t npc_obj, int64_t pc_obj, int reason, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_near_death_msg(int64_t npc_obj, int64_t pc_obj, char* buffer, int* speech_id_ptr);
void dialog_copy_npc_wont_identify_already_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_identify_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_wont_repair_broken_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_wont_repair_undamaged_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_copy_npc_repair_msg(int64_t npc_obj, int64_t pc_obj, char* buffer);
void dialog_check(void);
void dialog_enable_numbers(void);
void dialog_speech_id_decode(int a1, int* a2, int* a3);

#endif /* ARCANUM_GAME_DIALOG_H_ */
