#include "ui/skill_ui.h"
#include "game/activity_time.h"

#include "game/anim.h"
#include "game/critter.h"
#include "game/dialog.h"
#include "game/mes.h"
#include "game/obj.h"
#include "game/player.h"
#include "game/script.h"
#include "game/skill.h"
#include "game/stat.h"
#include "game/target.h"
#include "game/tf.h"
#include "ui/charedit_ui.h"
#include "ui/dialog_ui.h"
#include "ui/intgame.h"
#include "ui/inven_ui.h"
#include "ui/types.h"

#define FOUR 4

static bool skill_ui_pickpocket_is_forbidden(int64_t obj);
static void skill_ui_best_follower_select(SkillInvocation* skill_invocation);
static void skill_ui_invocation_execute(SkillInvocation* skill_invocation);
static bool skill_ui_open_steal_or_loot(int64_t a1, int64_t a2);
static bool skill_ui_result_feedback_primary(int64_t source_obj, int64_t target_obj, bool success);
static bool skill_ui_steal_item(int64_t source_obj, int64_t target_obj, int64_t item_obj, bool success);
static bool skill_ui_plant_item(int64_t source_obj, int64_t target_obj, int64_t item_obj, bool success);
static bool skill_ui_result_feedback_secondary(int64_t source_obj, int64_t target_obj, bool success);
static bool skill_ui_disarm_trap(int64_t source_obj, int64_t target_obj, bool success);
static bool skill_ui_repair(int64_t source_obj, int64_t target_obj, bool success);
static bool skill_ui_no_repair(int64_t source_obj, int64_t target_obj, bool success);
static bool skill_ui_lock(int64_t source_obj, int64_t target_obj, bool success);
static bool skill_ui_no_lock(int64_t source_obj);

// 0x5CB220
static int skill_ui_button_art_numbers[FOUR] = {
    279,
    280,
    282,
    278,
};

// 0x5CB230
static int skill_ui_icon_art_numbers[FOUR] = {
    279,
    280,
    282,
    278,
};

// 0x5CB240
static int skill_ui_action_skills[FOUR] = {
    SKILL_PROWLING,
    SKILL_PICK_POCKET,
    SKILL_DISARM_TRAPS,
    SKILL_REPAIR,
};

// 0x5CB250
static uint64_t skill_ui_action_target_flags[FOUR] = {
    TGT_SELF,
    TGT_OBJ_NO_ST_CRITTER_ANIMAL | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_TILE | TGT_OBJECT,
    TGT_OBJ_INVEN | TGT_OBJECT,
};

// 0x5CB270
static int skill_ui_selected_action = -1;

// 0x683478
static char* skill_ui_action_names[FOUR];

// 0x683488
static mes_file_handle_t skill_ui_mes_file;

// 0x683490
static int64_t skill_ui_source_obj;

// 0x579E80
bool skill_ui_init(GameInitInfo* init_info)
{
    int index;
    SkillCallbacks callbacks;
    MesFileEntry mes_file_entry;

    (void)init_info;

    callbacks.field_0 = skill_ui_result_feedback_primary;
    callbacks.steal_item_func = skill_ui_steal_item;
    callbacks.plant_item_output = skill_ui_plant_item;
    callbacks.field_C = skill_ui_result_feedback_secondary;
    callbacks.disarm_trap_func = skill_ui_disarm_trap;
    callbacks.repair_func = skill_ui_repair;
    callbacks.no_repair_func = skill_ui_no_repair;
    callbacks.lock_func = skill_ui_lock;
    callbacks.no_lock_func = skill_ui_no_lock;
    skill_set_callbacks(&callbacks);

    if (!mes_load("mes\\skill_ui.mes", &skill_ui_mes_file)) {
        return false;
    }

    for (index = 0; index < FOUR; index++) {
        mes_file_entry.num = 100 + index;
        mes_get_msg(skill_ui_mes_file, &mes_file_entry);
        skill_ui_action_names[index] = mes_file_entry.str;
    }

    return true;
}

// 0x579F30
void skill_ui_reset(void)
{
    skill_ui_source_obj = OBJ_HANDLE_NULL;
}

// 0x579F40
void skill_ui_exit(void)
{
    mes_unload(skill_ui_mes_file);
}

// 0x579F50
int skill_ui_action_button_art_num_get(int index)
{
    return index >= 0 && index < FOUR ? skill_ui_button_art_numbers[index] : skill_ui_button_art_numbers[0];
}

// 0x579F70
int skill_ui_action_icon_art_num_get(int index)
{
    return index >= 0 && index < FOUR ? skill_ui_icon_art_numbers[index] : skill_ui_icon_art_numbers[0];
}

// 0x579F90
int skill_ui_selected_action_get(void)
{
    return skill_ui_selected_action;
}

// 0x579FA0
void skill_ui_action_activate(int64_t obj, int type)
{
    int64_t pc_obj;

    pc_obj = player_get_local_pc_obj();
    if (pc_obj != obj) {
        if (critter_is_dead(pc_obj)
            || critter_is_unconscious(pc_obj)) {
            return;
        }

        if (critter_is_dead(obj)
            || critter_is_unconscious(obj)) {
            return;
        }
    }

    if (intgame_mode_get() == INTGAME_MODE_SKILL) {
        skill_ui_cancel();
    }

    skill_ui_source_obj = obj;
    skill_ui_selected_action = type;

    if ((skill_ui_action_target_flags[type] & 0x2000000) != 0
        || intgame_mode_set(INTGAME_MODE_MAIN)) {
        skill_ui_preprocess(obj, type);
    }
}

// 0x57A0A0
void skill_ui_preprocess(int64_t obj, int type)
{
    bool is_pc;
    uint64_t tgt;
    SkillInvocation skill_invocation;

    is_pc = player_is_local_pc_obj(obj);
    skill_ui_source_obj = obj;
    skill_ui_selected_action = type;

    tgt = skill_ui_action_target_flags[type];
    skill_invocation_init(&skill_invocation);

    switch (type) {
    case 0:
        critter_set_concealed(obj, !critter_is_concealed(obj));
        break;
    case 1:
    case 2:
        break;
    case 3:
        if (!is_pc) {
            return;
        }

        if (!inven_ui_is_created()) {
            inven_ui_open(obj, OBJ_HANDLE_NULL, INVEN_UI_MODE_INVENTORY);
        }
        break;
    default:
        tig_debug_printf("skill_ui_preprocess: ERROR: type out of range!\n");
        return;
    }

    if (is_pc && tgt != TGT_NONE && tgt != TGT_SELF) {
        if (intgame_mode_set(INTGAME_MODE_SKILL)) {
            target_flags_set(tgt);
        }
    }
}

// 0x57A1A0
void skill_ui_cancel(void)
{
    intgame_mode_set(INTGAME_MODE_MAIN);
    skill_ui_source_obj = OBJ_HANDLE_NULL;
}

// 0x57A1F0
void skill_ui_apply(TargetDescriptor* td)
{
    int64_t pc_obj;

    pc_obj = player_get_local_pc_obj();

    if ((pc_obj == skill_ui_source_obj || !critter_is_unconscious(pc_obj))
        && !critter_is_unconscious(skill_ui_source_obj)) {
        skill_ui_activate(td, skill_ui_source_obj, skill_ui_selected_action);
    }
}

// 0x57A320
void skill_ui_activate(TargetDescriptor* td, int64_t obj, int a3)
{
    bool is_pc;
    int skill;
    unsigned int spell_flags;
    MesFileEntry mes_file_entry;
    SkillInvocation skill_invocation;

    if (critter_is_dead(obj)) {
        return;
    }

    is_pc = player_is_local_pc_obj(obj);
    skill = skill_ui_target_skill_resolve(a3, td);

    switch (skill) {
    case SKILL_PICK_POCKET:
        if (!td->is_loc
            && is_pc
            && obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_PC) {
            skill_ui_cancel();

            spell_flags = obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS);

            if ((obj_field_int32_get(td->obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) == 0) {
                if (skill_ui_pickpocket_is_forbidden(td->obj)) {
                    if (player_is_local_pc_obj(obj)) {
                        mes_file_entry.num = 525; // "You cannot pickpocket this creature."
                        mes_get_msg(skill_ui_mes_file, &mes_file_entry);
                        tf_add(obj, TF_TYPE_WHITE, mes_file_entry.str);
                    }
                } else if (critter_is_dead(td->obj)) {
                    if (object_script_execute(obj, td->obj, obj, SAP_USE, 0)
                        && (spell_flags & OSF_POLYMORPHED) == 0
                        && !anim_foreground_goal_find(td->obj, 0)) {
                        ui_show_inven_loot(obj, td->obj);
                    }
                } else {
                    skill_ui_open_steal_or_loot(obj, td->obj);
                }
            }
        }
        return;
    case SKILL_REPAIR:
        if (!td->is_loc) {
            skill_invocation_init(&skill_invocation);
            object_handle_reference_init(obj, &(skill_invocation.source));
            object_handle_reference_init(td->obj, &(skill_invocation.target));
            object_handle_reference_init(OBJ_HANDLE_NULL, &(skill_invocation.item));
            skill_invocation.flags = 0;
            skill_invocation.skill = SKILL_REPAIR;
            skill_ui_invocation_execute(&skill_invocation);
        }
        return;
    case SKILL_DISARM_TRAPS:
        if (!td->is_loc && combat_consume_action_points(obj, 4)) {
            skill_invocation_init(&skill_invocation);
            object_handle_reference_init(obj, &(skill_invocation.source));
            object_handle_reference_init(td->obj, &(skill_invocation.target));
            object_handle_reference_init(OBJ_HANDLE_NULL, &(skill_invocation.item));
            skill_invocation.flags = SKILL_INVOCATION_0x04;
            skill_invocation.skill = SKILL_DISARM_TRAPS;
            skill_ui_best_follower_select(&skill_invocation);
            skill_ui_invocation_execute(&skill_invocation);
        }
        return;
    default:
        if (!td->is_loc) {
            skill_use(obj, skill, td->obj, 0);
        }
        return;
    }
}

// 0x57A5E0
bool skill_ui_pickpocket_is_forbidden(int64_t obj)
{
    return (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2) & OCF2_NO_PICKPOCKET) != 0
        || (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & OCF_MECHANICAL) != 0;
}

// 0x57A620
void skill_ui_best_follower_select(SkillInvocation* skill_invocation)
{
    int64_t obj;
    char str[1000];

    if (!tig_kb_get_modifier(SDL_KMOD_CTRL)) {
        obj = skill_invocation->source.obj;
        skill_pick_best_follower(skill_invocation);
        if (obj != skill_invocation->source.obj) {
            dialog_copy_npc_let_me_handle_msg(skill_invocation->source.obj, obj, str);
            dialog_ui_float_line(skill_invocation->source.obj, obj, str, 0);
        }
    }
}

// 0x57A6A0
int skill_ui_action_skill_get(int index)
{
    return skill_ui_action_skills[index];
}

// 0x57A6B0
void skill_ui_invocation_execute(SkillInvocation* skill_invocation)
{
    skill_invocation_run(skill_invocation);
}

// 0x57A6C0
int skill_ui_target_skill_resolve(int a1, TargetDescriptor* td)
{
    int v1;
    int complexity;

    v1 = skill_ui_action_skills[a1];
    if (v1 == -1) {
        complexity = obj_field_int32_get(td->obj, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY);
        if (a1 == 3 && complexity >= 20) {
            v1 = SKILL_REPAIR;
        }
    }

    return v1;
}

// 0x57A700
char* skill_ui_action_name_get(int index)
{
    return skill_ui_action_names[index];
}

// 0x57A710
bool skill_ui_open_steal_or_loot(int64_t pc_obj, int64_t target_obj)
{
    int mode;

    if (pc_obj != target_obj) {
        mode = obj_type_is_critter(obj_field_int32_get(target_obj, OBJ_F_TYPE))
            ? INVEN_UI_MODE_STEAL
            : INVEN_UI_MODE_LOOT;
        inven_ui_open(pc_obj, target_obj, mode);
    }

    return true;
}

// 0x57A770
bool skill_ui_result_feedback_primary(int64_t source_obj, int64_t target_obj, bool success)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    int tf_type;

    (void)target_obj;

    if (success) {
        mes_file_entry.num = 500; // "You succeed."
        tf_type = TF_TYPE_WHITE;
    } else {
        mes_file_entry.num = 501; // "You fail."
        tf_type = TF_TYPE_RED;
    }

    mes_get_msg(skill_ui_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;
    intgame_message_window_display_msg(&ui_message);

    tf_add(source_obj, tf_type, mes_file_entry.str);

    return true;
}

// 0x57A7F0
bool skill_ui_steal_item(int64_t source_obj, int64_t target_obj, int64_t item_obj, bool success)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    int tf_type;

    (void)target_obj;
    (void)item_obj;

    if (success) {
        mes_file_entry.num = 500; // "You succeed."
        tf_type = TF_TYPE_WHITE;
    } else {
        mes_file_entry.num = 501; // "You fail."
        tf_type = TF_TYPE_RED;
    }

    mes_get_msg(skill_ui_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;

    if (player_is_local_pc_obj(source_obj)) {
        intgame_message_window_display_msg(&ui_message);
    }

    tf_add(source_obj, tf_type, mes_file_entry.str);

    return true;
}

// NOTE: Exactly the same implementation as above.
//
// 0x57A8C0
bool skill_ui_plant_item(int64_t source_obj, int64_t target_obj, int64_t item_obj, bool success)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    int tf_type;

    (void)target_obj;
    (void)item_obj;

    if (success) {
        mes_file_entry.num = 500; // "You succeed."
        tf_type = TF_TYPE_WHITE;
    } else {
        mes_file_entry.num = 501; // "You fail."
        tf_type = TF_TYPE_RED;
    }

    mes_get_msg(skill_ui_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;

    if (player_is_local_pc_obj(source_obj)) {
        intgame_message_window_display_msg(&ui_message);
    }

    tf_add(source_obj, tf_type, mes_file_entry.str);

    return true;
}

// 0x57A990
bool skill_ui_result_feedback_secondary(int64_t source_obj, int64_t target_obj, bool success)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    int tf_type;

    (void)target_obj;

    if (success) {
        mes_file_entry.num = 500; // "You succeed."
        tf_type = TF_TYPE_WHITE;
    } else {
        mes_file_entry.num = 501; // "You fail."
        tf_type = TF_TYPE_RED;
    }

    mes_get_msg(skill_ui_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;
    intgame_message_window_display_msg(&ui_message);

    tf_add(source_obj, tf_type, mes_file_entry.str);

    return true;
}

// 0x57AA10
bool skill_ui_disarm_trap(int64_t source_obj, int64_t target_obj, bool success)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    int tf_type;

    (void)target_obj;

    if (success) {
        mes_file_entry.num = 560; // "Trap successfully disarmed."
        tf_type = TF_TYPE_WHITE;
    } else {
        mes_file_entry.num = 561; // "Trap disarm failed."
        tf_type = TF_TYPE_RED;
    }

    mes_get_msg(skill_ui_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;
    intgame_message_window_display_msg(&ui_message);

    tf_add(source_obj, tf_type, mes_file_entry.str);

    return true;
}

// 0x57AA90
bool skill_ui_repair(int64_t source_obj, int64_t target_obj, bool success)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    int tf_type;

    (void)target_obj;

    if (success) {
        mes_file_entry.num = 500; // "You succeed."
        tf_type = TF_TYPE_WHITE;
    } else {
        mes_file_entry.num = 501; // "You fail."
        tf_type = TF_TYPE_RED;
    }

    mes_get_msg(skill_ui_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;
    intgame_message_window_display_msg(&ui_message);

    tf_add(source_obj, tf_type, mes_file_entry.str);

    return true;
}

// 0x57AB10
bool skill_ui_no_repair(int64_t source_obj, int64_t target_obj, bool success)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;

    (void)target_obj;
    (void)success;

    mes_file_entry.num = 505; // "That is not in need of repair."
    mes_get_msg(skill_ui_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;
    intgame_message_window_display_msg(&ui_message);

    tf_add(source_obj, TF_TYPE_RED, mes_file_entry.str);

    return true;
}

// 0x57AB70
bool skill_ui_lock(int64_t source_obj, int64_t target_obj, bool success)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    int tf_type;

    (void)target_obj;

    if (success) {
        mes_file_entry.num = 550; // "Lock successfully picked."
        tf_type = TF_TYPE_WHITE;
    } else {
        mes_file_entry.num = 551; // "Lock picking failed."
        tf_type = TF_TYPE_RED;
    }

    mes_get_msg(skill_ui_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;
    intgame_message_window_display_msg(&ui_message);

    tf_add(source_obj, tf_type, mes_file_entry.str);

    return true;
}

// 0x57ABF0
bool skill_ui_no_lock(int64_t source_obj)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;

    mes_file_entry.num = 512; // "That has no lock."
    mes_get_msg(skill_ui_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;
    intgame_message_window_display_msg(&ui_message);

    tf_add(source_obj, TF_TYPE_RED, mes_file_entry.str);

    return true;
}

// 0x57AC50
void skill_ui_adjust_skill(int64_t obj, int skill, int action)
{
    switch (action) {
    case 1:
        skill_ui_inc_skill(obj, skill);
        break;
    case -1:
        skill_ui_dec_skill(obj, skill);
        break;
    }
}

// 0x57AC90
void skill_ui_set_training(int64_t obj, int skill, int training)
{
    if (IS_TECH_SKILL(skill)) {
        basic_skill_training_set(obj, GET_TECH_SKILL(skill), training);
    } else {
        tech_skill_training_set(obj, GET_BASIC_SKILL(skill), training);
    }
}

// 0x57ACD0
void skill_ui_inc_skill(int64_t obj, int skill)
{
    bool is_pc;
    int base;
    int level;
    int cost;
    int points;

    is_pc = player_is_local_pc_obj(obj);
    if (IS_TECH_SKILL(skill)) {
        skill = GET_TECH_SKILL(skill);
        base = tech_skill_points_get(obj, skill);
        level = tech_skill_level(obj, skill);
        cost = tech_skill_cost_inc(obj, skill);
        points = stat_level_get(obj, STAT_UNSPENT_POINTS);
        if (cost > points) {
            if (is_pc && charedit_is_created()) {
                charedit_error_not_enough_character_points();
            }
            return;
        }

        if (tech_skill_points_set(obj, skill, base + cost) != base + cost) {
            if (level == 20) {
                if (is_pc && charedit_is_created()) {
                    charedit_error_skill_at_max();
                }
            } else {
                if (is_pc && charedit_is_created()) {
                    charedit_error_not_enough_stat(tech_skill_stat(skill));
                }
            }
            return;
        }

        stat_base_set(obj, STAT_UNSPENT_POINTS, points - cost);
        activity_time_record(obj, ACTIVITY_TIME_SKILL_POINTS, cost);
        if (charedit_is_created()) {
            charedit_refresh();
            intgame_message_window_clear();
        }
    } else {
        skill = GET_BASIC_SKILL(skill);
        base = basic_skill_points_get(obj, skill);
        level = basic_skill_level(obj, skill);
        cost = basic_skill_cost_inc(obj, skill);
        points = stat_level_get(obj, STAT_UNSPENT_POINTS);
        if (cost > points) {
            if (is_pc && charedit_is_created()) {
                charedit_error_not_enough_character_points();
            }
            return;
        }

        if (basic_skill_points_set(obj, skill, base + cost) != base + cost) {
            if (level == 20) {
                if (is_pc && charedit_is_created()) {
                    charedit_error_skill_at_max();
                }
            } else {
                if (is_pc && charedit_is_created()) {
                    charedit_error_not_enough_stat(basic_skill_stat(skill));
                }
            }
            return;
        }

        stat_base_set(obj, STAT_UNSPENT_POINTS, points - cost);
        activity_time_record(obj, ACTIVITY_TIME_SKILL_POINTS, cost);
        if (charedit_is_created()) {
            charedit_refresh();
            intgame_message_window_clear();
        }
    }
}

// 0x57AEB0
void skill_ui_dec_skill(int64_t obj, int skill)
{
    bool is_pc;
    int base;
    int level;
    int cost;
    int points;

    is_pc = player_is_local_pc_obj(obj);
    if (IS_TECH_SKILL(skill)) {
        skill = GET_TECH_SKILL(skill);
        base = tech_skill_points_get(obj, skill);
        level = tech_skill_level(obj, skill); // FIXME: Unused.
        cost = tech_skill_cost_dec(obj, skill);
        points = stat_level_get(obj, STAT_UNSPENT_POINTS);

        if (tech_skill_points_set(obj, skill, base - cost) != base - cost) {
            if (tech_skill_points_get(obj, skill) != 0) {
                if (is_pc && charedit_is_created()) {
                    charedit_error_skill_at_min();
                }
            } else {
                if (is_pc && charedit_is_created()) {
                    charedit_error_skill_is_zero();
                }
            }
            return;
        }

        stat_base_set(obj, STAT_UNSPENT_POINTS, points + cost);
        activity_time_record(obj, ACTIVITY_TIME_SKILL_POINTS, -cost);
        if (charedit_is_created()) {
            charedit_refresh();
            intgame_message_window_clear();
        }
    } else {
        skill = GET_BASIC_SKILL(skill);
        base = basic_skill_points_get(obj, skill);
        level = basic_skill_level(obj, skill);
        cost = basic_skill_cost_dec(obj, skill);
        points = stat_level_get(obj, STAT_UNSPENT_POINTS);

        // NOTE: This code is different from tech skills code path above.
        while (level == basic_skill_level(obj, skill)) {
            if (basic_skill_points_set(obj, skill, base - cost) != base - cost) {
                if (basic_skill_points_get(obj, skill) != 0) {
                    // FIXME: No check for pc/charedit.
                    charedit_error_skill_at_min();
                } else {
                    // FIXME: No check for pc/charedit.
                    charedit_error_skill_is_zero();
                }
                return;
            }

            points = stat_base_set(obj, STAT_UNSPENT_POINTS, points + cost);
            activity_time_record(obj, ACTIVITY_TIME_SKILL_POINTS, -cost);
            if (charedit_is_created()) {
                charedit_refresh();
                intgame_message_window_clear();
            }
        }
    }
}
