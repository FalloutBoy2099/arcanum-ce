#include "game/broadcast.h"

#include <stdio.h>

#include "game/ai.h"
#include "game/anim.h"
#include "game/critter.h"
#include "game/dialog.h"
#include "game/highres_config.h"
#include "game/mes.h"
#include "game/party.h"
#include "game/player.h"
#include "game/text_filter.h"
#include "game/ui.h"

typedef struct BroadcastCommand {
    char* str;
    int type;
} BroadcastCommand;

static bool broadcast_execute_follower_order(int64_t pc_obj, int64_t candidate_obj,
    BroadcastCommandType cmd, int64_t loc, int64_t target_obj);
static int broadcast_match_str_to_base_type(const char* str);
static int broadcast_match_str_to_cmd_type(const char* str);
static void broadcast_follower_shortcut_labels_load(void);

// 0x5FDC4C
static char* broadcast_cmd_type_lookup[BROADCAST_CMD_TYPE_COUNT];

// 0x5FDC84
static mes_file_handle_t broadcast_mes_file;

// 0x5FDC88
static BroadcastFloatLineFunc broadcast_float_line_func;

// 0x5FDC8C
static mes_file_handle_t broadcast_multiplayer_mes_file;

// 0x5FDC90
static const char* follower_shortcut_labels[6];

// 0x5FF400
static bool broadcast_initialized;

// 0x5FF404
static BroadcastCommand* broadcast_commands;

// 0x5FF408
static int num_broadcast_commands;

// 0x4C2C10
bool broadcast_init(GameInitInfo* init_info)
{
    mes_file_handle_t mes_file;
    MesFileEntry mes_file_entry;
    int index;

    (void)init_info;

    broadcast_float_line_func = NULL;

    if (!mes_load("mes\\broadcast.mes", &broadcast_mes_file)) {
        tig_debug_printf("broadcast_init: ERROR: couldn't load message file: mes\\broadcast.mes!\n");
        exit(EXIT_FAILURE);
    }

    if (!mes_load("mes\\MultiPlayer.mes", &broadcast_multiplayer_mes_file)) {
        tig_debug_printf("broadcast_init: ERROR: couldn't load message file: %s!\n", "mes\\MultiPlayer.mes");
        exit(EXIT_FAILURE);
    }

    mes_file_entry.num = 100;
    while (mes_search(broadcast_mes_file, &mes_file_entry)) {
        mes_get_msg(broadcast_mes_file, &mes_file_entry);
        broadcast_commands = (BroadcastCommand*)REALLOC(broadcast_commands, sizeof(*broadcast_commands) * (num_broadcast_commands + 1));
        if (broadcast_commands == NULL) {
            tig_debug_printf("broadcast_init: ERROR: realloc failed!\n");
            exit(EXIT_FAILURE);
        }
        broadcast_commands[num_broadcast_commands++].str = mes_file_entry.str;
        mes_file_entry.num++;
    }

    if (!mes_load("Rules\\BroadCastRules.mes", &mes_file)) {
        tig_debug_printf("broadcast_init: ERROR: couldn't load message file: Rules\\BroadCastRules.mes!\n");
        exit(EXIT_FAILURE);
    }

    tig_str_parse_set_separator(',');

    mes_file_entry.num = 0;
    for (index = 0; index < BROADCAST_CMD_TYPE_COUNT; index++) {
        mes_get_msg(mes_file, &mes_file_entry);
        broadcast_cmd_type_lookup[index] = mes_file_entry.str;
        mes_file_entry.num++;
    }

    mes_file_entry.num = 100;
    for (index = 0; index < num_broadcast_commands; index++) {
        mes_get_msg(mes_file, &mes_file_entry);
        broadcast_commands[index].type = broadcast_match_str_to_base_type(mes_file_entry.str);
        mes_file_entry.num++;
    }

    if (!mes_unload(mes_file)) {
        tig_debug_printf("broadcast_init: ERROR: couldn't unload message file: bcRuleMessID!\n");
    }

    broadcast_follower_shortcut_labels_load();

    if (!text_filter_init()) {
        return false;
    }

    broadcast_initialized = true;

    return true;
}

// 0x4C2E20
void broadcast_exit(void)
{
    if (broadcast_commands != NULL) {
        FREE(broadcast_commands);
        broadcast_commands = NULL;
    }

    num_broadcast_commands = 0;

    if (!mes_unload(broadcast_mes_file)) {
        tig_debug_printf("broadcast_exit: ERROR: couldn't unload message file: bcMessID!\n");
    }

    if (!mes_unload(broadcast_multiplayer_mes_file)) {
        tig_debug_printf("broadcast_exit: ERROR: couldn't unload message file: mp_mes_id!\n");
    }

    text_filter_exit();

    broadcast_initialized = false;
}

// 0x4C2EA0
void broadcast_set_float_line_func(BroadcastFloatLineFunc func)
{
    broadcast_float_line_func = func;
}

// 0x4C2EB0
int broadcast_match_str_to_base_type(const char* str)
{
    int index;

    for (index = 0; index < BROADCAST_CMD_TYPE_COUNT; index++) {
        if (SDL_strcasecmp(str, broadcast_cmd_type_lookup[index]) == 0) {
            return index;
        }
    }

    tig_debug_printf("broadcast_match_str_to_base_type: ERROR: couldn't map str to type!\n");
    return 0;
}

// 0x4C2F00
void broadcast_msg(int64_t obj, Broadcast* bcast)
{
    broadcast_msg_client(obj, bcast);
}

/**
 * Execute a resolved order without parsing its translated display label.
 * Used by both text broadcasts and the follower portrait menu.
 */
static bool broadcast_execute_follower_order(int64_t pc_obj, int64_t candidate_obj,
    BroadcastCommandType cmd, int64_t loc, int64_t target_obj)
{
    char str[MAX_STRING];
    int speech_id;
    unsigned int spell_flags = obj_field_int32_get(candidate_obj, OBJ_F_SPELL_FLAGS);

    if ((cmd == BROADCAST_CMD_TYPE_WAIT && (spell_flags & OSF_MIND_CONTROLLED) != 0)
        || (cmd == BROADCAST_CMD_TYPE_WALK_HERE && loc == 0)
        || (cmd == BROADCAST_CMD_TYPE_ATTACK
            && (target_obj == OBJ_HANDLE_NULL
                || (!combat_critter_is_combat_mode_active(pc_obj)
                    && critter_pc_leader_get(target_obj) == candidate_obj)))) {
        return false;
    }

    switch (cmd) {
    case BROADCAST_CMD_TYPE_WAIT:
    case BROADCAST_CMD_TYPE_STAY_CLOSE:
    case BROADCAST_CMD_TYPE_SPREAD_OUT:
    case BROADCAST_CMD_TYPE_BACK_OFF:
    case BROADCAST_CMD_TYPE_WALK_HERE:
        break;
    case BROADCAST_CMD_TYPE_MOVE: {
        bool moved = anim_goal_please_move(pc_obj, candidate_obj);
        if (broadcast_float_line_func != NULL) {
            if (moved) {
                dialog_copy_npc_order_ok_msg(candidate_obj, pc_obj, str, &speech_id);
            } else {
                dialog_copy_npc_order_no_msg(candidate_obj, pc_obj, str, &speech_id);
            }
            broadcast_float_line_func(candidate_obj, pc_obj, str, speech_id);
        }
        return moved;
    }
    case BROADCAST_CMD_TYPE_ATTACK:
        ai_group_attacks_enable(candidate_obj);
        ai_attack(candidate_obj, target_obj, LOUDNESS_LOUD, 0x4);
        return true;
    default:
        return false;
    }

    if (broadcast_float_line_func != NULL) {
        dialog_copy_npc_order_ok_msg(candidate_obj, pc_obj, str, &speech_id);
        broadcast_float_line_func(candidate_obj, pc_obj, str, speech_id);
    }
    switch (cmd) {
    case BROADCAST_CMD_TYPE_WAIT:
        anim_object_interrupt_by_priority(candidate_obj, PRIORITY_3, false, true);
        ai_npc_wait(candidate_obj);
        break;
    case BROADCAST_CMD_TYPE_STAY_CLOSE:
        critter_stay_close(candidate_obj);
        break;
    case BROADCAST_CMD_TYPE_SPREAD_OUT:
        critter_spread_out(candidate_obj);
        break;
    case BROADCAST_CMD_TYPE_BACK_OFF:
        ai_stop_attacking(candidate_obj);
        break;
    case BROADCAST_CMD_TYPE_WALK_HERE:
        return anim_goal_run_to_tile(candidate_obj, loc);
    default:
        return false;
    }
    return true;
}

bool broadcast_order_follower(int64_t pc_obj, int64_t follower_obj,
    BroadcastCommandType command, int64_t loc, int64_t target_obj, const char* label)
{
    if (pc_obj == OBJ_HANDLE_NULL || follower_obj == OBJ_HANDLE_NULL
        || critter_is_dead(pc_obj) || critter_is_unconscious(pc_obj)
        || critter_is_dead(follower_obj) || critter_is_unconscious(follower_obj)
        || critter_pc_leader_get(follower_obj) != pc_obj
        || location_dist(obj_field_int64_get(pc_obj, OBJ_F_LOCATION),
               obj_field_int64_get(follower_obj, OBJ_F_LOCATION)) > highres_config_get()->broadcast_limit) {
        return false;
    }

    if (broadcast_float_line_func != NULL && label != NULL) {
        char name[MAX_STRING];
        char message[MAX_STRING];
        object_examine(follower_obj, pc_obj, name);
        snprintf(message, sizeof(message), "%s %s", name, label);
        broadcast_float_line_func(pc_obj, OBJ_HANDLE_NULL, message, -1);
    }

    bool accepted = broadcast_execute_follower_order(pc_obj, follower_obj, command, loc, target_obj);
    ai_process(follower_obj);
    return accepted;
}

// 0x4C31A0
void broadcast_msg_client(int64_t pc_obj, Broadcast* bcast)
{
    char* bcast_str;
    size_t pos;
    int64_t loc;
    int idx;
    int iter;
    int64_t* followers;
    int num_followers;
    unsigned int spell_flags;
    char str[1000];
    int speech_id;
    BroadcastCommandType cmd;
    int64_t target_obj;
    int64_t candidate_obj;
    ObjectList pcs;
    ObjectNode* node;
    bool done;
    bool found = false;

    text_filter_process(pc_obj, bcast->str, &bcast_str);

    if (broadcast_float_line_func != NULL) {
        broadcast_float_line_func(pc_obj, OBJ_HANDLE_NULL, bcast_str, -1);
    }

    loc = obj_field_int64_get(pc_obj, OBJ_F_LOCATION);

    if (obj_field_int32_get(pc_obj, OBJ_F_TYPE) == OBJ_TYPE_PC) {
        num_followers = obj_arrayfield_length_get(pc_obj, OBJ_F_CRITTER_FOLLOWER_IDX);
        followers = (int64_t*)MALLOC(sizeof(*followers) * num_followers);
        for (idx = 0; idx < num_followers; idx++) {
            followers[idx] = obj_arrayfield_handle_get(pc_obj, OBJ_F_CRITTER_FOLLOWER_IDX, idx);
        }

        idx = 0;
        done = false;
        for (iter = 0; iter < 200; iter++) {
            if (done) {
                break;
            }

            found = false;
            while (idx < num_followers) {
                if (found) {
                    break;
                }

                candidate_obj = followers[idx];
                if (candidate_obj != OBJ_HANDLE_NULL) {
                    int64_t candidate_loc = obj_field_int64_get(candidate_obj, OBJ_F_LOCATION);
                    if (location_dist(loc, candidate_loc) <= highres_config_get()->broadcast_limit) {
                        char candidate_follower_name[MAX_STRING];

                        object_examine(candidate_obj, pc_obj, candidate_follower_name);
                        pos = strlen(candidate_follower_name);
                        if (bcast_str[0] == '#') {
                            found = true;
                            pos = 1;
                        } else if (SDL_strncasecmp(bcast_str, candidate_follower_name, pos) == 0) {
                            found = true;
                            done = true;
                        }
                    }
                }

                idx++;
            }

            if (idx == num_followers) {
                done = true;
            }

            if (found) {
                cmd = broadcast_match_str_to_cmd_type(&(bcast_str[pos]));
                spell_flags = obj_field_int32_get(candidate_obj, OBJ_F_SPELL_FLAGS);
                switch (cmd) {
                case BROADCAST_CMD_TYPE_NONE:
                    break;
                case BROADCAST_CMD_TYPE_LEAVE:
                    if ((spell_flags & OSF_MIND_CONTROLLED) == 0) {
                        if (broadcast_float_line_func != NULL) {
                            dialog_copy_npc_farewell_msg(candidate_obj, pc_obj, str, &speech_id);
                            broadcast_float_line_func(candidate_obj, pc_obj, str, speech_id);
                        }
                        critter_disband(candidate_obj, true);
                    }
                    break;
                case BROADCAST_CMD_TYPE_WAIT:
                    broadcast_execute_follower_order(pc_obj, candidate_obj, cmd,
                        cmd == BROADCAST_CMD_TYPE_WALK_HERE ? bcast->loc : 0,
                        cmd == BROADCAST_CMD_TYPE_ATTACK ? object_hover_obj_get() : OBJ_HANDLE_NULL);
                    break;
                case BROADCAST_CMD_TYPE_FOLLOW: {
                    bool force = (obj_field_int32_get(candidate_obj, OBJ_F_NPC_FLAGS) & ONF_FORCED_FOLLOWER) != 0;
                    AiFollowResult reason = ai_check_follow(candidate_obj, pc_obj, force);
                    if (broadcast_float_line_func != NULL) {
                        if (reason == AI_FOLLOW_OK) {
                            dialog_copy_npc_order_ok_msg(candidate_obj, pc_obj, str, &speech_id);
                        } else {
                            dialog_copy_npc_order_no_msg(candidate_obj, pc_obj, str, &speech_id);
                        }
                        broadcast_float_line_func(candidate_obj, pc_obj, str, speech_id);
                    }
                    if (reason == AI_FOLLOW_OK) {
                        ai_npc_unwait(candidate_obj, force);
                    }
                    break;
                }
                case BROADCAST_CMD_TYPE_MOVE:
                    if (anim_goal_please_move(pc_obj, candidate_obj)) {
                        if (broadcast_float_line_func != NULL) {
                            dialog_copy_npc_order_ok_msg(candidate_obj, pc_obj, str, &speech_id);
                            broadcast_float_line_func(candidate_obj, pc_obj, str, speech_id);
                        }
                    } else {
                        if (broadcast_float_line_func != NULL) {
                            dialog_copy_npc_order_no_msg(candidate_obj, pc_obj, str, &speech_id);
                            broadcast_float_line_func(candidate_obj, pc_obj, str, speech_id);
                        }
                    }
                    break;
                case BROADCAST_CMD_TYPE_STAY_CLOSE:
                    broadcast_execute_follower_order(pc_obj, candidate_obj, cmd,
                        cmd == BROADCAST_CMD_TYPE_WALK_HERE ? bcast->loc : 0,
                        cmd == BROADCAST_CMD_TYPE_ATTACK ? object_hover_obj_get() : OBJ_HANDLE_NULL);
                    break;
                case BROADCAST_CMD_TYPE_SPREAD_OUT:
                    broadcast_execute_follower_order(pc_obj, candidate_obj, cmd,
                        cmd == BROADCAST_CMD_TYPE_WALK_HERE ? bcast->loc : 0,
                        cmd == BROADCAST_CMD_TYPE_ATTACK ? object_hover_obj_get() : OBJ_HANDLE_NULL);
                    break;
                case BROADCAST_CMD_TYPE_CURSE:
                    if ((spell_flags & OSF_MIND_CONTROLLED) == 0) {
                        if (critter_disband(candidate_obj, false)) {
                            if (broadcast_float_line_func != NULL) {
                                dialog_copy_npc_insult_msg(candidate_obj, pc_obj, str, &speech_id);
                                broadcast_float_line_func(candidate_obj, pc_obj, str, speech_id);
                            }
                            ai_attack(pc_obj, candidate_obj, LOUDNESS_NORMAL, 0);
                        }
                    }
                    break;
                case BROADCAST_CMD_TYPE_JOIN:
                case BROADCAST_CMD_TYPE_DISBAND:
                    found = false;
                    break;
                case BROADCAST_CMD_TYPE_ATTACK:
                    broadcast_execute_follower_order(pc_obj, candidate_obj, cmd,
                        cmd == BROADCAST_CMD_TYPE_WALK_HERE ? bcast->loc : 0,
                        cmd == BROADCAST_CMD_TYPE_ATTACK ? object_hover_obj_get() : OBJ_HANDLE_NULL);
                    break;
                case BROADCAST_CMD_TYPE_WALK_HERE:
                    broadcast_execute_follower_order(pc_obj, candidate_obj, cmd,
                        cmd == BROADCAST_CMD_TYPE_WALK_HERE ? bcast->loc : 0,
                        cmd == BROADCAST_CMD_TYPE_ATTACK ? object_hover_obj_get() : OBJ_HANDLE_NULL);
                    break;
                case BROADCAST_CMD_TYPE_BACK_OFF:
                    broadcast_execute_follower_order(pc_obj, candidate_obj, cmd,
                        cmd == BROADCAST_CMD_TYPE_WALK_HERE ? bcast->loc : 0,
                        cmd == BROADCAST_CMD_TYPE_ATTACK ? object_hover_obj_get() : OBJ_HANDLE_NULL);
                    break;
                default:
                    tig_debug_printf("broadcast_msg_client: ERROR: type out of range!\n");
                    break;
                }

                ai_process(candidate_obj);
            }
        }

        if (found) {
            FREE(followers);
            FREE(bcast_str);
            return;
        }

        done = false;
        object_list_vicinity(pc_obj, OBJ_TM_PC, &pcs);

        node = pcs.head;
        if (node != NULL) {
            for (iter = 0; iter < 200; iter++) {
                if (done) {
                    break;
                }

                while (node != NULL) {
                    char* candidate_pc_name;

                    if (found) {
                        break;
                    }

                    candidate_obj = node->obj;
                    obj_field_string_get(candidate_obj, OBJ_F_PC_PLAYER_NAME, &candidate_pc_name);
                    pos = strlen(candidate_pc_name);
                    if (*bcast_str == '#') {
                        found = true;
                        pos = 1;
                    } else if (SDL_strncasecmp(bcast_str, candidate_pc_name, pos) == 0) {
                        found = true;
                        done = true;
                    }

                    FREE(candidate_pc_name);
                    node = node->next;
                }

                if (node == NULL) {
                    done = true;
                }

                if (found) {
                    cmd = broadcast_match_str_to_cmd_type(&(bcast_str[pos]));
                    switch (cmd) {
                    case BROADCAST_CMD_TYPE_NONE:
                    case BROADCAST_CMD_TYPE_LEAVE:
                    case BROADCAST_CMD_TYPE_WAIT:
                    case BROADCAST_CMD_TYPE_FOLLOW:
                    case BROADCAST_CMD_TYPE_MOVE:
                    case BROADCAST_CMD_TYPE_STAY_CLOSE:
                    case BROADCAST_CMD_TYPE_SPREAD_OUT:
                    case BROADCAST_CMD_TYPE_CURSE:
                    case BROADCAST_CMD_TYPE_BACK_OFF:
                        found = false;
                        break;
                    case BROADCAST_CMD_TYPE_JOIN:
                        party_add(pc_obj, candidate_obj, NULL);
                        found = false;
                        break;
                    case BROADCAST_CMD_TYPE_DISBAND:
                        if (pc_obj != candidate_obj
                            || !party_remove(pc_obj)) {
                            found = false;
                        }
                        break;
                    default:
                        tig_debug_printf("broadcast_msg_client: ERROR: type out of range!\n");
                        found = false;
                        break;
                    }
                }
            }
        }

        object_list_destroy(&pcs);
        FREE(followers);
    }

    if (!found) {
        cmd = broadcast_match_str_to_cmd_type(bcast_str);
        if (cmd == BROADCAST_CMD_TYPE_FOLLOW
            || cmd == BROADCAST_CMD_TYPE_FOLLOW_OTHER) {
            target_obj = object_hover_obj_get();
            if (target_obj != OBJ_HANDLE_NULL) {
                anim_goal_follow_obj(pc_obj, target_obj);
            }
        }
    }

    FREE(bcast_str);
}

// 0x4C3B40
int broadcast_match_str_to_cmd_type(const char* str)
{
    int index;

    while (*str == ' ') {
        str++;
    }

    for (index = 0; index < num_broadcast_commands; index++) {
        if (SDL_strcasecmp(str, broadcast_commands[index].str) == 0) {
            return broadcast_commands[index].type;
        }
    }

    return BROADCAST_CMD_TYPE_NONE;
}

// 0x4C3B90
void broadcast_follower_shortcut_labels_load(void)
{
    int index;
    MesFileEntry mes_file_entry;

    for (index = 0; index < 6; index++) {
        mes_file_entry.num = 500 + index;
        mes_get_msg(broadcast_mes_file, &mes_file_entry);
        follower_shortcut_labels[index] = mes_file_entry.str;
    }
}

// 0x4C3BE0
void broadcast_follower_shortcut_execute(unsigned int a1, int64_t loc)
{
    BroadcastCommandType command = broadcast_follower_shortcut_command(a1);
    int64_t pc_obj = player_get_local_pc_obj();
    int64_t target_obj = object_hover_obj_get();
    ObjectList followers;
    ObjectNode* node;

    if (command == BROADCAST_CMD_TYPE_NONE || pc_obj == OBJ_HANDLE_NULL) {
        return;
    }

    // These strings are feedback only. Never parse translated shortcut labels.
    if (broadcast_float_line_func != NULL) {
        broadcast_float_line_func(pc_obj, OBJ_HANDLE_NULL, follower_shortcut_labels[a1], -1);
    }
    object_list_followers(pc_obj, &followers);
    for (node = followers.head; node != NULL; node = node->next) {
        broadcast_order_follower(pc_obj, node->obj, command, loc, target_obj, NULL);
    }
    object_list_destroy(&followers);
}
