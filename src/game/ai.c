#include "game/ai.h"
#include <tig/performance.h>

#include "game/anim.h"
#include "game/anim_private.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/gamelib.h"
#include "game/ai_utility_adapter.h"
#include "game/descriptions.h"
#include "game/dialog.h"
#include "game/highres_config.h"
#include "game/item.h"
#include "game/magictech.h"
#include "game/map.h"
#include "game/monstergen.h"
#include "game/mt_ai.h"
#include "game/mt_item.h"
#include "game/obj.h"
#include "game/object.h"
#include "game/object_node.h"
#include "game/path.h"
#include "game/player.h"
#include "game/proto.h"
#include "game/random.h"
#include "game/reaction.h"
#include "game/script.h"
#include "game/sector.h"
#include "game/sfx.h"
#include "game/skill.h"
#include "game/spell.h"
#include "game/stat.h"
#include "game/teleport.h"
#include "game/tile.h"
#include "game/timeevent.h"
#include "game/ui.h"

#define CLOCKWORK_DECOY 6719

typedef struct AiMagicTechCandidates {
    /* 0000 */ unsigned int flags;
    /* 0004 */ MagicTechAiActionListEntry* entries;
    /* 0008 */ int cnt;
    /* 0010 */ int64_t obj;
} AiMagicTechCandidates;

static bool ai_context_prepare_for_processing(Ai* ai);
static void ai_context_init(Ai* ai, int64_t obj);
static bool ai_heal(Ai* ai);
static bool ai_heal_func(Ai* ai, int64_t obj, bool allow_noncritical_healing);
static bool ai_look_for_item(Ai* ai);
static bool ai_look_for_item_func(int64_t obj, unsigned int flags);
static void ai_danger_state_update(Ai* ai);
static void ai_redirect_func(int64_t source_obj, int64_t target_obj);
static void ai_followers_notify_attack(int64_t leader_obj, int64_t target_obj, int notify_upset, int passive_notify_only);
static void ai_follower_attack_react(int64_t source_obj, int64_t leader_obj, int64_t target_obj, int notify_upset, int passive_notify_only, int lock_target);
static void ai_nearby_attack_witnesses_notify(int64_t victim_obj, int64_t attacker_obj, int loudness);
static void ai_attack_witness_process(int64_t witness_obj, int64_t attacker_obj, int64_t victim_obj, int loudness);
static void ai_forget_combat_target(int64_t obj, int64_t target_obj);
static void ai_engage_or_flee_from_target(int64_t critter_obj, int64_t threat_obj);
static bool ai_npc_wait_here_timeevent_check(TimeEvent* timeevent);
static int ai_danger_state_set(int64_t source_obj, int danger_type, int64_t target_obj, int* sound_id_ptr);
static bool ai_flee_spell_select(Ai* ai);
static bool ai_flee_distance_reached_or_mind_controlled(int64_t fleeing_obj, int64_t threat_obj);
static int64_t ai_choose_target(int64_t attacker_obj, int64_t candidate1_obj, int64_t candidate2_obj);
static void ai_select_flee_or_combat_danger(int64_t source_obj, int64_t target_obj);
static bool ai_should_flee(int64_t source_obj, int64_t target_obj);
static int64_t ai_find_target(int64_t critter_obj);
static bool ai_combat_target_is_valid(int64_t source_obj, int64_t target_obj);
static void ai_combat_spell_action_prepare(Ai* ai);
static bool ai_combat_spell_select(Ai* ai);
static int ai_combat_spell_attempt_chance(Ai* ai);
static bool ai_is_stronger(int64_t attacker_obj, int64_t target_obj);
static bool ai_magictech_candidate_select(Ai* ai, AiMagicTechCandidates* candidates);
static void ai_action_perform(Ai* ai);
static void ai_action_perform_cast(Ai* ai);
static void ai_action_perform_item(Ai* ai);
static void ai_action_perform_skill(Ai* ai);
static void ai_action_perform_non_combat(Ai* ai);
static void ai_action_perform_fleeing(Ai* ai);
static void ai_action_perform_surrender(Ai* ai);
static void ai_action_perform_combat(Ai* ai);
static void ai_action_perform_baking_off(Ai* ai);
static bool ai_use_grenade(Ai* ai, int64_t distance);
static bool ai_waypoints_process(int64_t obj, bool teleport_to_waypoint);
static bool ai_is_day(void);
static bool ai_standpoints_process(int64_t obj, bool teleport_to_standpoint);
static bool ai_get_standpoint(int64_t obj, int64_t* standpoint_ptr);
static void ai_wake_up(int64_t npc_obj);
static int64_t ai_find_nearest_bed(int64_t obj);
static void ai_move_to(int64_t obj, int64_t loc, int range);
static bool ai_is_alive_near_player_outside_turn_based(int64_t obj);
static bool ai_distant_follower_relocate(int64_t obj);
static int ai_timeevent_delay(int64_t obj);
static int ai_distance_to_nearest_player(int64_t obj);
static bool ai_timeevent_check(TimeEvent* timeevent);
static void ai_schedule_delay_milliseconds(int64_t obj, int millis);
static void ai_schedule_delay(int64_t obj, DateTime* datetime);
static int ai_check_leader(int64_t npc_obj, int64_t pc_obj);
static int ai_check_upset_attacking(int64_t source_obj, int64_t target_obj, int64_t leader_obj);
static void ai_calc_party_size_and_level_internal(int64_t obj, int* cnt_ptr, int* lvl_ptr);
static int ai_check_protect(int64_t source_obj, int64_t target_obj);
static int64_t ai_dead_ally_attacker_find(int64_t observer_obj, int64_t ally_obj);
static bool ai_is_indoor_to_outdoor_transition(int64_t portal_obj, int dir);
static int ai_perception_distance(int value);
static int ai_straight_path_cost_to_object(int64_t source_obj, int64_t target_obj);
static bool ai_check_decoy(int64_t source_obj, int64_t target_obj);
static void ai_add_team_to_hostility_list(int64_t source_obj, int64_t target_obj);
static void ai_shitlist_add(int64_t npc_obj, int64_t shit_obj);
static void ai_shitlist_remove(int64_t npc_obj, int64_t shit_obj);
static int64_t ai_shitlist_get(int64_t obj);

#define concealed_to_loudness(concealed) ((concealed) ? LOUDNESS_SILENT : LOUDNESS_NORMAL)

// 0x5B5088
static DateTime ai_wait_durations[6] = {
    { .days = 0, .milliseconds = 0 },
    { .milliseconds = 900000 },
    { .milliseconds = 7200000 },
    { .days = 1 },
    { .days = 7 },
    { .days = 30 },
};

// 0x5B50C0
static int ai_sound_radius_by_loudness[LOUDNESS_COUNT] = {
    2,
    8,
    15,
};

// 0x5B50CC
static bool ai_npc_fighting_enabled = true;

// NOTE: Original code likely defined it as `int[150][17]`.
//
// 0x5F5CB0
static AiParams ai_parameter_profiles[150];

// 0x5F8488
static AiFloatLineFunc ai_float_line_func;

// 0x5F848C
static AiEndDialogCallback ai_end_dialog_callback;

// 0x5F8498
static bool in_find_target;

// 0x5F8490
static int64_t ai_npc_wait_here_test_obj;

// 0x5F84A0
static int64_t ai_test_obj;

// 0x4A8320
bool ai_init(GameInitInfo* init_info)
{
    mes_file_handle_t mes_file;
    MesFileEntry mes_file_entry;
    int param;
    int idx;
    char* str;
    char* tok;

    (void)init_info;

    ai_end_dialog_callback = NULL;
    ai_float_line_func = NULL;
    ai_utility_reset();

    if (!mes_load("rules\\ai_params.mes", &mes_file)) {
        return false;
    }

    mes_file_entry.num = 0;
    if (mes_search(mes_file, &mes_file_entry)) {
        idx = mes_file_entry.num;
        do {
            str = mes_file_entry.str;
            for (param = 0; param < AI_PARAMS_MAX; param++) {
                tok = strtok(str, " \t\n");
                if (tok == NULL) {
                    break;
                }

                ai_parameter_profiles[idx].values[param] = atoi(tok);

                str = NULL;
            }
            idx++;
        } while (idx < 100 && mes_find_next(mes_file, &mes_file_entry));
    }

    mes_unload(mes_file);

    return true;
}

// 0x4A83F0
void ai_exit(void)
{
}

// 0x4A8400
bool ai_mod_load(void)
{
    mes_file_handle_t mes_file;
    MesFileEntry mes_file_entry;
    int param;
    int idx;
    char* str;
    char* tok;

    if (mes_load("rules\\gameai.mes", &mes_file)) {
        mes_file_entry.num = 100;
        if (mes_search(mes_file, &mes_file_entry)) {
            idx = mes_file_entry.num;
            do {
                str = mes_file_entry.str;
                for (param = 0; param < AI_PARAMS_MAX; param++) {
                    tok = strtok(str, " \t\n");
                    if (tok == NULL) {
                        break;
                    }

                    ai_parameter_profiles[idx].values[param] = atoi(tok);

                    str = NULL;
                }
                idx++;
            } while (idx < 150 && mes_find_next(mes_file, &mes_file_entry));
        }

        mes_unload(mes_file);
    }

    return true;
}

// 0x4A84C0
void ai_mod_unload(void)
{
    ai_utility_reset();
}

// 0x4A84D0
void ai_set_callbacks(AiEndDialogCallback end_dialog_callback, AiFloatLineFunc float_line_func)
{
    ai_end_dialog_callback = end_dialog_callback;
    ai_float_line_func = float_line_func;
}

// 0x4A84F0
static void ai_process_measured(int64_t obj);

void ai_process(int64_t obj)
{
    Uint64 start = tig_performance_begin();
    ai_process_measured(obj);
    tig_performance_end(TIG_PERF_AI_DECISION, start);
}

static void ai_process_measured(int64_t obj)
{
    Ai ai;

    if (map_is_clearing_objects()) {
        return;
    }

    ai_context_init(&ai, obj);

    if (!ai_context_prepare_for_processing(&ai)) {
        return;
    }

    if (ai_npc_fighting_enabled && ai_utility_mode(&ai) >= 2
        && ai.danger_type == AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS && ai_utility_retreat_pending(&ai)) return;

    if (ai_npc_fighting_enabled && ai_utility_mode(&ai) >= 2
        && ai.danger_type == AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS) {
        ai_danger_state_update(&ai);
    } else if (!ai_heal(&ai) && !ai_look_for_item(&ai)) {
        ai_danger_state_update(&ai);
    }

    tig_performance_trace(TIG_PERF_EVENT_AI_ACTION, (Uint64)obj, (Uint64)ai.danger_source, ai.action_type, ai.spell);
    ai_action_perform(&ai);
}

// 0x4A8570
bool ai_context_prepare_for_processing(Ai* ai)
{
    int64_t pc_obj;
    int64_t conversation_pc;
    unsigned int critter_flags;
    unsigned int npc_flags;
    int64_t leader_obj;

    if (critter_is_dead(ai->obj)) {
        return false;
    }

    if (!critter_is_sleeping(ai->obj)
        && (obj_field_int32_get(ai->obj, OBJ_F_FLAGS) & (OF_OFF | OF_DONTDRAW)) != 0) {
        return false;
    }

    pc_obj = player_get_local_pc_obj();

    if ((obj_field_int32_get(pc_obj, OBJ_F_FLAGS) & OF_OFF) != 0) {
        return false;
    }

    if ((obj_field_int32_get(ai->obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
        return false;
    }

    critter_flags = obj_field_int32_get(ai->obj, OBJ_F_CRITTER_FLAGS);

    conversation_pc = reaction_conversation_pc_get(ai->obj);
    if (conversation_pc) {
        if (pc_obj == conversation_pc
            && ai_end_dialog_callback != NULL
            && ai->danger_type != AI_DANGER_SOURCE_TYPE_NONE
            && ai->danger_type != AI_DANGER_SOURCE_TYPE_SURRENDER) {
            ai_end_dialog_callback(pc_obj, 0);
        }
        return false;
    }

    if (combat_critter_is_combat_mode_active(ai->obj)) {
        if (!combat_turn_based_is_active()) {
            object_script_execute(ai->danger_source, ai->obj, OBJ_HANDLE_NULL, SAP_END_COMBAT, 0);
        }
        object_script_execute(ai->danger_source, ai->obj, OBJ_HANDLE_NULL, SAP_START_COMBAT, 0);
    }

    if (combat_turn_based_is_active() && combat_turn_based_whos_turn_get() != ai->obj) {
        return false;
    }

    if (anim_foreground_goal_priority_get(ai->obj) > 2) {
        return false;
    }

    if ((critter_flags & OCF_BLINDED) != 0
        && random_between(1, 160) <= 1) {
        critter_flags &= ~OCF_BLINDED;
        obj_field_int32_set(ai->obj, OBJ_F_CRITTER_FLAGS, critter_flags);
    }

    if ((critter_flags & OCF_CRIPPLED) != 0
        && random_between(1, 8000) <= 1) {
        critter_flags &= ~OCF_CRIPPLED;
        obj_field_int32_set(ai->obj, OBJ_F_CRITTER_FLAGS, critter_flags);
    }

    if ((critter_flags & OCF_STUNNED) != 0) {
        if (!anim_is_current_goal_type(ai->obj, AG_ANIMATE_STUNNED, NULL)) {
            critter_flags &= ~OCF_STUNNED;
            obj_field_int32_set(ai->obj, OBJ_F_CRITTER_FLAGS, critter_flags);
            return false;
        }
    }

    if (critter_is_unconscious(ai->obj)) {
        return false;
    }

    npc_flags = obj_field_int32_get(ai->obj, OBJ_F_NPC_FLAGS);
    if ((npc_flags & ONF_GENERATOR) != 0) {
        return false;
    }

    if ((npc_flags & ONF_CHECK_WIELD) != 0) {
        item_wield_best_all(ai->obj, ai->danger_source);

        npc_flags = obj_field_int32_get(ai->obj, OBJ_F_NPC_FLAGS);
        npc_flags &= ~ONF_CHECK_WIELD;
        obj_field_int32_set(ai->obj, OBJ_F_NPC_FLAGS, npc_flags);
    } else if ((npc_flags & ONF_CHECK_WEAPON) != 0) {
        item_wield_best(ai->obj, ITEM_INV_LOC_WEAPON, ai->danger_source);

        npc_flags = obj_field_int32_get(ai->obj, OBJ_F_NPC_FLAGS);
        npc_flags &= ~ONF_CHECK_WEAPON;
        obj_field_int32_set(ai->obj, OBJ_F_NPC_FLAGS, npc_flags);
    }

    if ((npc_flags & ONF_DEMAINTAIN_SPELLS) != 0) {
        leader_obj = critter_leader_get(ai->obj);
        if (leader_obj == OBJ_HANDLE_NULL
            || critter_is_dead(leader_obj)
            || !combat_critter_is_combat_mode_active(leader_obj)) {
            npc_flags &= ~ONF_DEMAINTAIN_SPELLS;
            obj_field_int32_set(ai->obj, OBJ_F_NPC_FLAGS, npc_flags);
            magictech_demaintain_spells(ai->obj);
        }
    }

    return true;
}

// 0x4A88D0
void ai_context_init(Ai* ai, int64_t obj)
{
    ai->obj = obj;
    ai->danger_source = OBJ_HANDLE_NULL;
    ai->danger_type = AI_DANGER_SOURCE_TYPE_NONE;
    ai->action_type = AI_ACTION_TYPE_UNKNOWN;
    ai->spell = 10000;
    ai->skill = -1;
    ai->item_obj = OBJ_HANDLE_NULL;
    ai->leader_obj = critter_leader_get(obj);
    ai_danger_source(obj, &(ai->danger_type), &(ai->danger_source));
    ai->sound_id = -1;
    ai->move_destination = 0;
    ai->maintenance_id = -1;
    ai->utility_selected = false;
}

// 0x4A8940
bool ai_heal(Ai* ai)
{
    bool force;
    AiParams ai_params;
    int64_t leader_obj;
    ObjectList objects;
    ObjectNode* node;

    if (critter_is_sleeping(ai->obj)) {
        return false;
    }

    if (ai->danger_type == AI_DANGER_SOURCE_TYPE_FLEE) {
        return false;
    }

    force = true;

    if (combat_critter_is_combat_mode_active(ai->obj)) {
        ai_copy_params(ai->obj, &ai_params);
        if (random_between(1, 100) > ai_params.healing_spell_chance) {
            force = false;
        }
    }

    if (ai_heal_func(ai, ai->obj, force)) {
        return true;
    }

    leader_obj = critter_pc_leader_get(ai->obj);
    if (leader_obj == OBJ_HANDLE_NULL) {
        leader_obj = ai->leader_obj;
    }

    if (leader_obj != OBJ_HANDLE_NULL) {
        if (ai_heal_func(ai, leader_obj, force)) {
            return true;
        }
    } else {
        leader_obj = ai->obj;
    }

    object_list_all_followers(leader_obj, &objects);
    node = objects.head;
    while (node != NULL) {
        if (node->obj != ai->obj
            && ai_heal_func(ai, node->obj, force)) {
            break;
        }
        node = node->next;
    }
    object_list_destroy(&objects);

    if (node == NULL) {
        return false;
    }

    return true;
}

// 0x4A8AA0
bool ai_heal_func(Ai* ai, int64_t obj, bool allow_noncritical_healing)
{
    AiMagicTechCandidates spell_candidates;
    MagicTechAi magictech_ai;
    int hp_ratio;
    int64_t item_obj;

    if ((obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & ONF_NO_ATTACK) != 0) {
        return false;
    }

    if (critter_is_dead(obj)) {
        mt_ai_create(&magictech_ai, ai->obj, MAGICTECH_AI_ACTION_RESURRECT);
        spell_candidates.flags = 0x8;
        spell_candidates.entries = magictech_ai.actions[MAGICTECH_AI_ACTION_RESURRECT].entries;
        spell_candidates.cnt = magictech_ai.actions[MAGICTECH_AI_ACTION_RESURRECT].cnt;
        spell_candidates.obj = obj;
        if (ai_magictech_candidate_select(ai, &spell_candidates)) {
            mt_ai_destroy(&magictech_ai);
            return true;
        } else {
            mt_ai_destroy(&magictech_ai);
            return false;
        }
    }

    hp_ratio = ai_object_hp_ratio(obj);
    if (hp_ratio > 30 && !allow_noncritical_healing) {
        return false;
    }

    if (ai->danger_type != AI_DANGER_SOURCE_TYPE_NONE) {
        if (ai_critter_fatigue_ratio(ai->obj) < 20) {
            mt_ai_create(&magictech_ai, ai->obj, MAGICTECH_AI_ACTION_FATIGUE_RECOVER);
            spell_candidates.flags = 0x8;
            spell_candidates.entries = magictech_ai.actions[MAGICTECH_AI_ACTION_FATIGUE_RECOVER].entries;
            spell_candidates.cnt = magictech_ai.actions[MAGICTECH_AI_ACTION_FATIGUE_RECOVER].cnt;
            spell_candidates.obj = obj;
            if (ai_magictech_candidate_select(ai, &spell_candidates)) {
                mt_ai_destroy(&magictech_ai);
                return true;
            }

            mt_ai_destroy(&magictech_ai);
        }
    } else {
        if (hp_ratio < 90) {
            item_obj = skill_supplementary_item(ai->obj, BASIC_SKILL_HEAL);
            if (ai_check_use_skill(ai->obj, obj, item_obj, BASIC_SKILL_HEAL) == AI_USE_SKILL_OK) {
                ai->danger_source = obj;
                ai->item_obj = item_obj;
                ai->action_type = AI_ACTION_TYPE_USE_SKILL;
                ai->skill = BASIC_SKILL_HEAL;
                return true;
            }
        }
    }

    if (hp_ratio < 40) {
        mt_ai_create(&magictech_ai, ai->obj, MAGICTECH_AI_ACTION_HEAL_HEAVY);
        spell_candidates.flags = 0x8;
        spell_candidates.entries = magictech_ai.actions[MAGICTECH_AI_ACTION_HEAL_HEAVY].entries;
        spell_candidates.cnt = magictech_ai.actions[MAGICTECH_AI_ACTION_HEAL_HEAVY].cnt;
        spell_candidates.obj = obj;
        if (ai_magictech_candidate_select(ai, &spell_candidates)) {
            mt_ai_destroy(&magictech_ai);
            return true;
        }

        mt_ai_destroy(&magictech_ai);
    }

    if (hp_ratio < 55) {
        mt_ai_create(&magictech_ai, ai->obj, MAGICTECH_AI_ACTION_HEAL_MEDIUM);
        spell_candidates.flags = 0x8;
        spell_candidates.entries = magictech_ai.actions[MAGICTECH_AI_ACTION_HEAL_MEDIUM].entries;
        spell_candidates.cnt = magictech_ai.actions[MAGICTECH_AI_ACTION_HEAL_MEDIUM].cnt;
        spell_candidates.obj = obj;
        if (ai_magictech_candidate_select(ai, &spell_candidates)) {
            mt_ai_destroy(&magictech_ai);
            return true;
        }

        mt_ai_destroy(&magictech_ai);
    }

    if (stat_level_get(obj, STAT_POISON_LEVEL) > 0) {
        mt_ai_create(&magictech_ai, ai->obj, MAGICTECH_AI_ACTION_CURE_POISON);
        spell_candidates.flags = 0x8;
        spell_candidates.entries = magictech_ai.actions[MAGICTECH_AI_ACTION_CURE_POISON].entries;
        spell_candidates.cnt = magictech_ai.actions[MAGICTECH_AI_ACTION_CURE_POISON].cnt;
        spell_candidates.obj = obj;
        if (ai_magictech_candidate_select(ai, &spell_candidates)) {
            mt_ai_destroy(&magictech_ai);
            return true;
        }

        mt_ai_destroy(&magictech_ai);
    }

    if (hp_ratio < 70
        || (ai->danger_type == AI_DANGER_SOURCE_TYPE_NONE
            && hp_ratio < 90)) {
        mt_ai_create(&magictech_ai, ai->obj, MAGICTECH_AI_ACTION_HEAL_LIGHT);
        spell_candidates.flags = 0x8;
        spell_candidates.entries = magictech_ai.actions[MAGICTECH_AI_ACTION_HEAL_LIGHT].entries;
        spell_candidates.cnt = magictech_ai.actions[MAGICTECH_AI_ACTION_HEAL_LIGHT].cnt;
        spell_candidates.obj = obj;
        if (ai_magictech_candidate_select(ai, &spell_candidates)) {
            mt_ai_destroy(&magictech_ai);
            return true;
        }

        mt_ai_destroy(&magictech_ai);
    }

    return false;
}

// 0x4A8E70
bool ai_look_for_item(Ai* ai)
{
    unsigned int npc_flags;

    if (critter_is_sleeping(ai->obj)) {
        return false;
    }

    if ((obj_field_int32_get(ai->obj, OBJ_F_CRITTER_FLAGS) & OCF_BLINDED) != 0) {
        return false;
    }

    if (ui_inventory_target_object_get() != OBJ_HANDLE_NULL) {
        return false;
    }

    npc_flags = obj_field_int32_get(ai->obj, OBJ_F_NPC_FLAGS);

    if ((npc_flags & ONF_LOOK_FOR_AMMO) != 0) {
        npc_flags &= ~ONF_LOOK_FOR_AMMO;
        obj_field_int32_set(ai->obj, OBJ_F_NPC_FLAGS, npc_flags);

        if (ai_look_for_item_func(ai->obj, OBJ_TM_AMMO)) {
            return true;
        }
    }

    if ((npc_flags & ONF_LOOK_FOR_WEAPON) != 0) {
        npc_flags &= ~ONF_LOOK_FOR_WEAPON;
        obj_field_int32_set(ai->obj, OBJ_F_NPC_FLAGS, npc_flags);

        if (ai_look_for_item_func(ai->obj, OBJ_TM_WEAPON)) {
            return true;
        }
    }

    if ((npc_flags & ONF_LOOK_FOR_ARMOR) != 0) {
        npc_flags &= ~ONF_LOOK_FOR_ARMOR;
        obj_field_int32_set(ai->obj, OBJ_F_NPC_FLAGS, npc_flags);

        if (ai_look_for_item_func(ai->obj, OBJ_TM_ARMOR)) {
            return true;
        }
    }

    return false;
}

// 0x4A8F90
bool ai_look_for_item_func(int64_t obj, unsigned int flags)
{
    int64_t nearest_obj = OBJ_HANDLE_NULL;
    int64_t nearest_dist = 0;
    int64_t dist;
    bool found = false;
    int radius;
    ObjectList objects;
    ObjectNode* node;
    int item_type;
    int64_t item_obj;

    if (critter_is_monstrous(obj)) {
        return false;
    }

    radius = ai_perception_distance(stat_level_get(obj, STAT_PERCEPTION) - 5);
    ai_objects_in_radius(obj, radius, &objects, flags);
    node = objects.head;
    while (node != NULL) {
        if ((obj_field_int32_get(node->obj, OBJ_F_ITEM_FLAGS) & (OIF_NO_DISPLAY | OIF_NO_NPC_PICKUP)) == 0
            && !item_has_zero_worth(node->obj)
            && item_check_insert(node->obj, obj, NULL) == ITEM_CANNOT_OK) {
            dist = object_dist(obj, node->obj);
            if (nearest_obj == OBJ_HANDLE_NULL
                || nearest_dist < dist) {
                nearest_obj = node->obj;
                nearest_dist = dist;
            }
        }
        node = node->next;
    }

    if (nearest_obj != OBJ_HANDLE_NULL
        && anim_goal_pickup_item(obj, nearest_obj)) {
        found = true;
    }
    object_list_destroy(&objects);

    if (found) {
        return true;
    }

    if (flags != OBJ_TM_ITEM) {
        switch (flags) {
        case OBJ_TM_WEAPON:
            item_type = OBJ_TYPE_WEAPON;
            break;
        case OBJ_TM_AMMO:
            item_type = OBJ_TYPE_AMMO;
            break;
        case OBJ_TM_ARMOR:
            item_type = OBJ_TYPE_ARMOR;
            break;
        case OBJ_TM_GOLD:
            item_type = OBJ_TYPE_GOLD;
            break;
        case OBJ_TM_FOOD:
            item_type = OBJ_TYPE_FOOD;
            break;
        case OBJ_TM_SCROLL:
            item_type = OBJ_TYPE_SCROLL;
            break;
        case OBJ_TM_KEY:
            item_type = OBJ_TYPE_KEY;
            break;
        case OBJ_TM_KEY_RING:
            item_type = OBJ_TYPE_KEY_RING;
            break;
        case OBJ_TM_WRITTEN:
            item_type = OBJ_TYPE_WRITTEN;
            break;
        case OBJ_TM_GENERIC:
            item_type = OBJ_TYPE_GENERIC;
            break;
        default:
            return false;
        }
    } else {
        item_type = -1;
    }

    ai_objects_in_radius(obj, radius, &objects, OBJ_TM_CONTAINER);
    node = objects.head;
    while (node != NULL) {
        if (proto_number_from_instance(node->obj) == BP_JUNK_PILE) {
            item_obj = item_type != -1
                ? item_find_first_of_type(node->obj, item_type)
                : item_find_first(node->obj);
            if (item_obj != OBJ_HANDLE_NULL
                && (obj_field_int32_get(item_obj, OBJ_F_FLAGS) & OF_OFF) == 0
                && (obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS) & (OIF_NO_DISPLAY | OIF_NO_NPC_PICKUP)) == 0
                && !item_has_zero_worth(item_obj)
                && item_check_insert(item_obj, obj, NULL) == ITEM_CANNOT_OK
                && anim_goal_pickup_item(obj, item_obj)) {
                found = true;
                break;
            }
        }
        node = node->next;
    }
    object_list_destroy(&objects);

    return found;
}

// 0x4A92D0
void ai_danger_state_update(Ai* ai)
{
    int64_t danger_source_obj;

    if (critter_is_sleeping(ai->obj)) {
        return;
    }

    switch (ai->danger_type) {
    case 0:
        danger_source_obj = ai_find_target(ai->obj);
        if (danger_source_obj != OBJ_HANDLE_NULL) {
            ai->danger_source = danger_source_obj;
            ai_combat_spell_action_prepare(ai);
            ai->danger_type = ai_danger_state_set(ai->obj,
                AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS,
                danger_source_obj,
                &(ai->sound_id));
        }
        break;
    case 1:
        danger_source_obj = ai_combat_target_get_or_find(ai->obj);
        if (danger_source_obj != OBJ_HANDLE_NULL) {
            ai->danger_source = danger_source_obj;
            ai_combat_spell_action_prepare(ai);
            ai->danger_type = ai_danger_state_set(ai->obj,
                AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS,
                danger_source_obj,
                &(ai->sound_id));
            break;
        }

        obj_field_obj_get(ai->obj, OBJ_F_NPC_WHO_HIT_ME_LAST, &danger_source_obj);
        ai->danger_source = danger_source_obj;
        if (ai_combat_target_is_valid(ai->obj, ai->danger_source)) {
            ai_combat_spell_action_prepare(ai);
            ai->danger_type = ai_danger_state_set(ai->obj,
                AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS,
                danger_source_obj,
                &(ai->sound_id));
            break;
        }

        danger_source_obj = ai_shitlist_get(ai->obj);
        ai->danger_source = danger_source_obj;
        if (ai->danger_source != OBJ_HANDLE_NULL) {
            ai_combat_spell_action_prepare(ai);
            ai->danger_type = ai_danger_state_set(ai->obj,
                AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS,
                danger_source_obj,
                &(ai->sound_id));
            break;
        }

        ai->danger_type = ai_danger_state_set(ai->obj,
            AI_DANGER_SOURCE_TYPE_NONE,
            OBJ_HANDLE_NULL,
            &(ai->sound_id));
        break;
    case 2:
        ai_flee_spell_select(ai);
        break;
    case 3:
        if (ai_object_hp_ratio(ai->obj) >= 80 && random_between(1, 500) == 1) {
            ai->danger_type = ai_danger_state_set(ai->obj,
                AI_DANGER_SOURCE_TYPE_NONE,
                OBJ_HANDLE_NULL,
                &(ai->sound_id));
        }
        break;
    }
}

// 0x4A94C0
void ai_redirect_func(int64_t source_obj, int64_t target_obj)
{
    if (obj_field_int32_get(source_obj, OBJ_F_TYPE) == OBJ_TYPE_NPC && source_obj != target_obj) {
        ai_attack(source_obj, target_obj, LOUDNESS_SILENT, 0);
        obj_field_handle_set(target_obj, OBJ_F_NPC_COMBAT_FOCUS, source_obj);
        obj_field_handle_set(target_obj, OBJ_F_NPC_WHO_HIT_ME_LAST, source_obj);
    }
}

// 0x4A9530
void ai_redirect_init(AiRedirect* ai_redirect, int64_t source_obj, int64_t target_obj)
{
    ai_redirect->source_obj = source_obj;
    ai_redirect->target_obj = target_obj;
    ai_redirect->min_iq = -1;
    ai_redirect->critter_flags = 0;
}

// 0x4A9560
void ai_redirect_perform(AiRedirect* ai_redirect)
{
    ObjectList objects;
    ObjectNode* node;

    if (ai_redirect->source_obj == OBJ_HANDLE_NULL
        || ai_redirect->target_obj == OBJ_HANDLE_NULL) {
        return;
    }

    object_list_vicinity(ai_redirect->target_obj, OBJ_TM_PC | OBJ_TM_NPC, &objects);
    node = objects.head;
    while (node != NULL) {
        if (!critter_is_dead(node->obj)
            && node->obj != ai_redirect->source_obj
            && node->obj != ai_redirect->target_obj
            && (ai_redirect->min_iq <= 0
                || stat_level_get(ai_redirect->min_iq, STAT_INTELLIGENCE) <= ai_redirect->min_iq)
            && (ai_redirect->critter_flags == 0
                || (obj_field_int32_get(node->obj, OBJ_F_CRITTER_FLAGS) & ai_redirect->critter_flags) != 0)) {
            ai_redirect_func(ai_redirect->source_obj, node->obj);
        }
        node = node->next;
    }
    object_list_destroy(&objects);
}

// 0x4A9650
void ai_attack(int64_t source_obj, int64_t target_obj, int loudness, unsigned int flags)
{
    int target_obj_type;
    int source_obj_type;
    int64_t leader_obj;
    int64_t target_group_leader;
    unsigned int npc_flags;
    AiParams ai_params;
    int rc;
    char str[1000];
    int speech_id;
    int current_reaction;

    if (source_obj == OBJ_HANDLE_NULL
        || target_obj == OBJ_HANDLE_NULL) {
        return;
    }

    target_obj_type = obj_field_int32_get(target_obj, OBJ_F_TYPE);
    if (obj_type_is_critter(target_obj_type)) {
        if ((obj_field_int32_get(target_obj, OBJ_F_CRITTER_FLAGS2) & OCF2_NIGH_INVULNERABLE) != 0
            || critter_is_dead(target_obj)) {
            return;
        }

        if (critter_is_sleeping(target_obj)) {
            ai_wake_up(target_obj);
        }
    }

    if ((obj_field_int32_get(target_obj, OBJ_F_FLAGS) & (OF_OFF | OF_DONTDRAW | OF_INVULNERABLE)) != 0) {
        return;
    }

    if (obj_field_int32_get(target_obj, OBJ_F_NAME) == 6719) {
        return;
    }

    if (target_obj_type == OBJ_TYPE_WALL) {
        return;
    }

    if (source_obj == target_obj) {
        return;
    }

    source_obj_type = obj_field_int32_get(source_obj, OBJ_F_TYPE);
    if (obj_type_is_critter(source_obj_type)) {
        if ((flags & 0x04) != 0) {
            leader_obj = critter_leader_get(source_obj);
            if (leader_obj != OBJ_HANDLE_NULL) {
                ai_follower_attack_react(source_obj, leader_obj, target_obj, 1, 0, 1);
            }
            return;
        }

        if ((flags & 0x02) == 0) {
            ai_followers_notify_attack(source_obj, target_obj, 1, (flags & 0x01) != 0);
        }

        if (obj_type_is_critter(target_obj_type)) {
            target_group_leader = critter_pc_leader_get(target_obj);
            if (target_group_leader == OBJ_HANDLE_NULL) {
                target_group_leader = target_obj;
            }

            if (critter_is_concealed(target_group_leader)) {
                critter_set_concealed(target_group_leader, false);
            }

            if (target_obj_type == OBJ_TYPE_NPC) {
                npc_flags = obj_field_int32_get(target_obj, OBJ_F_NPC_FLAGS);
                npc_flags &= ~ONF_KOS_OVERRIDE;
                obj_field_int32_set(target_obj, OBJ_F_NPC_FLAGS, npc_flags);
            }

            if ((flags & 0x02) == 0) {
                if (target_obj_type == OBJ_TYPE_PC) {
                    ai_auto_attack_response(target_obj, source_obj);
                }

                ai_followers_notify_attack(target_obj, source_obj, 0, (flags & 0x01) != 0);

                if ((flags & 0x01) == 0) {
                    ai_nearby_attack_witnesses_notify(target_obj, source_obj, loudness);
                }
            }

            if (target_obj_type == OBJ_TYPE_NPC) {
                anim_goal_rotate(target_obj, object_rot(target_obj, source_obj));

                leader_obj = critter_leader_get(target_obj);

                if ((flags & 0x01) == 0
                    || !critter_faction_same(source_obj, target_obj)) {
                    obj_field_handle_set(target_obj, OBJ_F_NPC_WHO_HIT_ME_LAST, source_obj);
                }

                ai_add_team_to_hostility_list(target_obj, source_obj);

                if (source_obj_type == OBJ_TYPE_NPC) {
                    ai_add_team_to_hostility_list(source_obj, target_obj);
                } else if (source_obj_type == OBJ_TYPE_PC) {
                    ai_copy_params(target_obj, &ai_params);
                    if (leader_obj == source_obj) {
                        if ((flags & 0x01) == 0) {
                            reaction_adj(target_obj, source_obj, ai_params.friendly_hit_reaction_penalty);
                        }

                        rc = ai_check_follow(target_obj, source_obj, true);
                        if (rc != AI_FOLLOW_OK
                            && critter_disband(target_obj, false)) {
                            npc_flags = obj_field_int32_get(target_obj, OBJ_F_NPC_FLAGS);
                            npc_flags |= ONF_JILTED;
                            obj_field_int32_set(target_obj, OBJ_F_NPC_FLAGS, npc_flags);

                            if (critter_is_active(target_obj)) {
                                if (ai_float_line_func != NULL) {
                                    dialog_copy_npc_wont_follow_msg(target_obj, source_obj, rc, str, &speech_id);
                                    ai_float_line_func(target_obj, source_obj, str, speech_id);
                                }
                            }
                        } else {
                            if (random_between(1, 3) == 1
                                && critter_is_active(target_obj)) {
                                if (ai_float_line_func != NULL) {
                                    dialog_copy_npc_accidental_attack_msg(target_obj, source_obj, str, &speech_id);
                                    ai_float_line_func(target_obj, source_obj, str, speech_id);
                                }
                            }
                        }
                    } else {
                        if ((flags & 0x01) != 0) {
                            reaction_adj(target_obj, source_obj, -10);
                        } else {
                            current_reaction = reaction_get_with_limited_penalty(target_obj, source_obj);
                            if (current_reaction > ai_params.attack_reaction_threshold) {
                                reaction_adj(target_obj, source_obj, ai_params.attack_reaction_threshold - current_reaction);
                            }
                        }
                    }
                }

                if ((flags & 0x01) == 0
                    || !critter_faction_same(source_obj, target_obj)) {
                    ai_engage_or_flee_from_target(target_obj, source_obj);
                }
            }
        }
    } else if (target_obj_type == OBJ_TYPE_NPC) {
        ai_danger_state_set(target_obj,
            AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS,
            source_obj,
            NULL);
    }
}

// 0x4A9AD0
void ai_auto_attack_response(int64_t attacker_obj, int64_t target_obj)
{
    if (attacker_obj == target_obj) {
        return;
    }

    if (ai_end_dialog_callback != NULL) {
        ai_end_dialog_callback(attacker_obj, 0);
    }

    if (!combat_critter_is_combat_mode_active(attacker_obj)) {
        combat_critter_activate_combat_mode(attacker_obj);
        if (combat_turn_based_is_active()) {
            combat_turn_based_whos_turn_set(target_obj);
        }
    }

    if (!anim_foreground_goal_find(attacker_obj, NULL) || anim_is_idle(attacker_obj)) {
        if (combat_auto_attack_get(attacker_obj)) {
            if (!anim_is_current_goal_type(attacker_obj, AG_ATTEMPT_ATTACK, NULL)) {
                anim_goal_attack(attacker_obj, target_obj);
            }
        }
    }
}

// 0x4A9B80
void ai_followers_notify_attack(int64_t leader_obj, int64_t target_obj, int notify_upset, int passive_notify_only)
{
    ObjectList followers;
    ObjectNode* node;

    if (leader_obj != target_obj) {
        object_list_followers(leader_obj, &followers);
        node = followers.head;
        while (node != NULL) {
            ai_follower_attack_react(node->obj, leader_obj, target_obj, notify_upset, passive_notify_only, 0);
            node = node->next;
        }
        object_list_destroy(&followers);
    }
}

// 0x4A9C00
void ai_follower_attack_react(int64_t source_obj, int64_t leader_obj, int64_t target_obj, int notify_upset, int passive_notify_only, int lock_target)
{
    int target_obj_type;
    int rc;
    char str[1000];
    int speech_id;

    if (leader_obj == target_obj) {
        return;
    }

    target_obj_type = obj_field_int32_get(target_obj, OBJ_F_TYPE);
    if (obj_type_is_critter(target_obj_type)) {
        if (source_obj == target_obj || critter_is_dead(source_obj)) {
            return;
        }

        rc = ai_check_upset_attacking(source_obj, target_obj, leader_obj);
        if (rc != 0) {
            if (notify_upset
                && !combat_critter_is_combat_mode_active(target_obj)
                && !passive_notify_only
                && ai_float_line_func != NULL) {
                if (critter_is_active(source_obj)) {
                    dialog_copy_npc_upset_attacking_msg(source_obj, leader_obj, rc, str, &speech_id);
                    ai_float_line_func(source_obj, leader_obj, str, speech_id);
                }

                reaction_adj(source_obj, leader_obj, -5);
            }
            return;
        }

        if (passive_notify_only) {
            return;
        }

        if (lock_target) {
            ai_target_lock(source_obj, target_obj);
        } else {
            ai_engage_or_flee_from_target(source_obj, target_obj);
        }
    } else {
        if (passive_notify_only) {
            return;
        }

        if (lock_target) {
            ai_target_lock(source_obj, target_obj);
        } else {
            ai_danger_state_set(source_obj,
                AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS,
                target_obj,
                NULL);
        }
    }
}

// 0x4A9E10
void ai_nearby_attack_witnesses_notify(int64_t victim_obj, int64_t attacker_obj, int loudness)
{
    int radius;
    int dist;
    ObjectList objects;
    ObjectNode* node;

    radius = ai_sound_radius_by_loudness[loudness];
    dist = (int)object_dist(attacker_obj, victim_obj);
    if (dist < 2 * ai_sound_radius_by_loudness[LOUDNESS_LOUD]) {
        ai_objects_in_radius(attacker_obj, radius, &objects, OBJ_TM_NPC);
        node = objects.head;
        while (node != NULL) {
            ai_attack_witness_process(node->obj, attacker_obj, victim_obj, loudness);
            node = node->next;
        }
        object_list_destroy(&objects);
    }
    if (dist > 1) {
        ai_objects_in_radius(victim_obj, radius, &objects, OBJ_TM_NPC);
        node = objects.head;
        while (node != NULL) {
            ai_attack_witness_process(node->obj, attacker_obj, victim_obj, loudness);
            node = node->next;
        }
        object_list_destroy(&objects);
    }
}

// 0x4A9F10
void ai_attack_witness_process(int64_t witness_obj, int64_t attacker_obj, int64_t victim_obj, int loudness)
{
    int64_t leader_obj;
    int danger_type;

    if ((obj_field_int32_get(witness_obj, OBJ_F_NPC_FLAGS) & ONF_AI_WAIT_HERE) != 0) {
        leader_obj = OBJ_HANDLE_NULL;
    } else {
        leader_obj = critter_leader_get(witness_obj);
    }

    if (witness_obj != victim_obj && witness_obj != attacker_obj && leader_obj != attacker_obj && leader_obj != victim_obj) {
        if (critter_is_sleeping(witness_obj)) {
            ai_wake_up(witness_obj);
        }

        if (ai_check_protect(witness_obj, victim_obj) != AI_PROTECT_NO) {
            if (ai_can_see(witness_obj, victim_obj) == 0 || ai_can_hear(witness_obj, victim_obj, loudness) == 0) {
                ai_engage_or_flee_from_target(witness_obj, attacker_obj);
            }
        } else if (ai_check_protect(witness_obj, attacker_obj) != AI_PROTECT_NO) {
            if (ai_can_see(witness_obj, victim_obj) == 0 || ai_can_hear(witness_obj, victim_obj, loudness) == 0) {
                ai_engage_or_flee_from_target(witness_obj, victim_obj);
            }
        } else if (critter_social_class_get(witness_obj) != SOCIAL_CLASS_GUARD
            && (obj_field_int32_get(witness_obj, OBJ_F_CRITTER_FLAGS) & OCF_NO_FLEE) == 0) {
            ai_danger_source(witness_obj, &danger_type, NULL);
            if (danger_type == AI_DANGER_SOURCE_TYPE_NONE
                && (ai_can_see(witness_obj, victim_obj) == 0 || ai_can_hear(witness_obj, victim_obj, loudness) == 0)) {
                ai_danger_state_set(witness_obj,
                    AI_DANGER_SOURCE_TYPE_FLEE,
                    attacker_obj,
                    NULL);
            }
        }
    }
}

// 0x4AA0D0
void ai_notify_explosion_dynamite(int64_t pc_obj)
{
    ObjectList npcs;
    ObjectNode* node;

    ai_objects_in_radius(pc_obj, 10, &npcs, OBJ_TM_NPC);

    node = npcs.head;
    while (node != NULL) {
        if (!critter_is_dead(node->obj)
            && (obj_field_int32_get(node->obj, OBJ_F_SPELL_FLAGS) & OSF_MIND_CONTROLLED) == 0
            && critter_pc_leader_get(node->obj) != pc_obj
            && (ai_can_see(node->obj, pc_obj) == 0
                || ai_can_hear(node->obj, pc_obj, LOUDNESS_SILENT) == 0)) {
            ai_attack(pc_obj, node->obj, LOUDNESS_SILENT, 0);
        }
        node = node->next;
    }

    object_list_destroy(&npcs);
}

// 0x4AA1B0
void ai_notify_killed(int64_t victim_obj, int64_t killer_obj)
{
    ai_stop_fleeing(victim_obj);
    ai_npc_unwait(victim_obj, true);

    if (killer_obj != OBJ_HANDLE_NULL
        && obj_field_int32_get(killer_obj, OBJ_F_TYPE) == OBJ_TYPE_PC) {
        ObjectList npcs;
        ObjectNode* node;
        int danger_type;
        int64_t danger_obj;
        unsigned int flags;

        ai_objects_in_radius(killer_obj, 3, &npcs, OBJ_TM_NPC);

        node = npcs.head;
        while (node != NULL) {
            if (!critter_is_dead(node->obj)
                && (obj_field_int32_get(node->obj, OBJ_F_CRITTER_FLAGS) & OCF_NO_FLEE) == 0
                && critter_faction_same(victim_obj, node->obj)) {
                ai_danger_source(node->obj, &danger_type, &danger_obj);
                if (danger_type == AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS
                    && danger_obj == killer_obj) {
                    flags = obj_field_int32_get(node->obj, OBJ_F_NPC_FLAGS);
                    flags |= ONF_BACKING_OFF;
                    obj_field_int32_set(node->obj, OBJ_F_NPC_FLAGS, flags);
                }
            }
            node = node->next;
        }

        object_list_destroy(&npcs);
    }
}

// 0x4AA300
void ai_hostility_end_against_group(int64_t critter_obj, int64_t target_obj)
{
    int64_t target_group_leader;
    int obj_type;
    int reaction;

    target_group_leader = critter_pc_leader_get(target_obj);
    if (target_group_leader == OBJ_HANDLE_NULL) {
        target_group_leader = target_obj;
    }

    obj_field_handle_set(critter_obj, OBJ_F_NPC_COMBAT_FOCUS, OBJ_HANDLE_NULL);
    obj_field_handle_set(critter_obj, OBJ_F_NPC_WHO_HIT_ME_LAST, OBJ_HANDLE_NULL);
    ai_shitlist_remove(critter_obj, target_group_leader);

    obj_type = obj_field_int32_get(target_group_leader, OBJ_F_TYPE);
    if (obj_type == OBJ_TYPE_PC) {
        reaction = reaction_get(critter_obj, target_group_leader);
        if (reaction < 50) {
            reaction_adj(critter_obj, target_group_leader, 50 - reaction);
        }
    }

    ai_danger_state_set(critter_obj, AI_DANGER_SOURCE_TYPE_NONE, OBJ_HANDLE_NULL, NULL);
    anim_attack_interrupt_for_target_or_leader(critter_obj, target_group_leader);

    if (obj_type_is_critter(obj_type)) {
        ObjectList objects;
        ObjectNode* node;

        object_list_all_followers(target_group_leader, &objects);
        node = objects.head;
        while (node != NULL) {
            ai_forget_combat_target(node->obj, critter_obj);
            ai_shitlist_remove(critter_obj, node->obj);
            anim_attack_interrupt_for_target(node->obj, critter_obj);
            node = node->next;
        }
        object_list_destroy(&objects);
    }
}

// 0x4AA420
void ai_forget_combat_target(int64_t obj, int64_t target_obj)
{
    if (obj_field_handle_get(obj, OBJ_F_NPC_COMBAT_FOCUS) == target_obj) {
        obj_field_handle_set(obj, OBJ_F_NPC_COMBAT_FOCUS, OBJ_HANDLE_NULL);
    }
    if (obj_field_handle_get(obj, OBJ_F_NPC_WHO_HIT_ME_LAST) == target_obj) {
        obj_field_handle_set(obj, OBJ_F_NPC_WHO_HIT_ME_LAST, OBJ_HANDLE_NULL);
    }
    ai_shitlist_remove(obj, target_obj);
}

// 0x4AA4A0
void ai_stop_attacking(int64_t obj)
{
    int type;

    type = obj_field_int32_get(obj, OBJ_F_TYPE);
    if (type == OBJ_TYPE_NPC) {
        int64_t combat_focus_obj;
        ObjectNpcFlags npc_flags;

        combat_focus_obj = obj_field_handle_get(obj, OBJ_F_NPC_COMBAT_FOCUS);
        if (combat_focus_obj != OBJ_HANDLE_NULL) {
            ai_hostility_end_against_group(obj, combat_focus_obj);
            anim_object_interrupt_by_priority(obj, 3, 0, 0);
            combat_critter_deactivate_combat_mode(obj);
        }
        if (critter_pc_leader_get(obj) != OBJ_HANDLE_NULL) {
            npc_flags = obj_field_int32_get(obj, OBJ_F_NPC_FLAGS);
            npc_flags |= ONF_NO_ATTACK;
            obj_field_int32_set(obj, OBJ_F_NPC_FLAGS, npc_flags);
        }
        ai_target_unlock(obj);
    } else if (type == OBJ_TYPE_PC) {
        ObjectList objects;
        ObjectNode* node;

        object_list_all_followers(obj, &objects);

        node = objects.head;
        while (node != NULL) {
            ai_stop_attacking(node->obj);
            node = node->next;
        }

        object_list_destroy(&objects);
    }
}

// 0x4AA580
void ai_group_attacks_enable(int64_t obj)
{
    int type;
    unsigned int flags;
    ObjectList objects;
    ObjectNode* node;

    type = obj_field_int32_get(obj, OBJ_F_TYPE);
    switch (type) {
    case OBJ_TYPE_NPC:
        if (critter_pc_leader_get(obj) != OBJ_HANDLE_NULL) {
            flags = obj_field_int32_get(obj, OBJ_F_NPC_FLAGS);
            flags &= ~ONF_NO_ATTACK;
            obj_field_int32_set(obj, OBJ_F_NPC_FLAGS, flags);
        }
        break;
    case OBJ_TYPE_PC:
        object_list_all_followers(obj, &objects);

        node = objects.head;
        while (node != NULL) {
            ai_group_attacks_enable(node->obj);
            node = node->next;
        }

        object_list_destroy(&objects);
        break;
    }
}

// 0x4AA620
void ai_engage_or_flee_from_target(int64_t critter_obj, int64_t threat_obj)
{
    int danger_type;
    int64_t danger_source_obj;
    char str[1000];
    int speech_id;

    if (critter_is_dead(critter_obj)) {
        return;
    }

    ai_danger_source(critter_obj, &danger_type, &danger_source_obj);

    switch (danger_type) {
    case AI_DANGER_SOURCE_TYPE_NONE:
        ai_select_flee_or_combat_danger(critter_obj, threat_obj);
        break;
    case AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS:
        if (danger_source_obj == threat_obj
            || ai_choose_target(critter_obj, danger_source_obj, threat_obj) == threat_obj) {
            ai_select_flee_or_combat_danger(critter_obj, threat_obj);
        }
        break;
    case AI_DANGER_SOURCE_TYPE_FLEE:
        if (danger_source_obj != threat_obj
            && ai_flee_distance_reached_or_mind_controlled(critter_obj, danger_source_obj)) {
            ai_select_flee_or_combat_danger(critter_obj, threat_obj);
        }
        break;
    case AI_DANGER_SOURCE_TYPE_SURRENDER:
        if (danger_source_obj == threat_obj
            || ai_choose_target(critter_obj, danger_source_obj, threat_obj) == threat_obj) {
            ai_select_flee_or_combat_danger(critter_obj, threat_obj);
        } else {
            if (critter_is_active(critter_obj)) {
                dialog_copy_npc_fleeing_msg(critter_obj, threat_obj, str, &speech_id);
                ai_float_line_func(critter_obj, threat_obj, str, speech_id);
            }
            anim_goal_flee(critter_obj, threat_obj);
        }
        break;
    }

    combat_turn_based_add_critter(critter_obj);
}

// 0x4AA7A0
void ai_npc_wait(int64_t obj)
{
    int wait;
    int64_t leader_obj;
    TimeEvent timeevent = { 0 };
    DateTime datetime;
    unsigned int npc_flags;

    if (obj_field_int32_get(obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
        return;
    }

    if ((obj_field_int32_get(obj, OBJ_F_NPC_FLAGS) & ONF_AI_WAIT_HERE) != 0) {
        return;
    }

    wait = obj_field_int32_get(obj, OBJ_F_NPC_WAIT);
    leader_obj = critter_leader_get(obj);
    if (leader_obj == OBJ_HANDLE_NULL) {
        return;
    }

    // NOTE: The original code (0x4AA818) skips the "become jilted" timer when the leader's
    // charisma IS extraordinary - such followers wait indefinitely.
    if (!stat_is_extraordinary(leader_obj, STAT_CHARISMA)
        && (obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_MIND_CONTROLLED) == 0) {
        if (basic_skill_training_get(leader_obj, BASIC_SKILL_PERSUATION) != TRAINING_NONE) {
            wait++;
        }

        if (wait >= 0 && wait < SDL_arraysize(ai_wait_durations)) {
            datetime = ai_wait_durations[wait];
            timeevent.type = TIMEEVENT_TYPE_NPC_WAIT_HERE;
            timeevent.params[0].object_value = obj;
            timeevent_add_delay(&timeevent, &datetime);
        }
    }
    critter_disband(obj, true);
    critter_leader_set(obj, leader_obj);

    npc_flags = obj_field_int32_get(obj, OBJ_F_NPC_FLAGS);
    npc_flags |= ONF_AI_WAIT_HERE;
    obj_field_int32_set(obj, OBJ_F_NPC_FLAGS, npc_flags);
}

// 0x4AA8C0
void ai_npc_unwait(int64_t obj, bool force)
{
    int64_t leader_obj;
    unsigned int flags;

    if (obj_field_int32_get(obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
        return;
    }

    leader_obj = critter_leader_get(obj);
    if (leader_obj == OBJ_HANDLE_NULL) {
        return;
    }

    if ((obj_field_int32_get(obj, OBJ_F_NPC_FLAGS) & ONF_AI_WAIT_HERE) == 0) {
        return;
    }

    if (!force) {
        if (ai_check_follow(obj, leader_obj, false) != AI_FOLLOW_OK) {
            return;
        }
    }

    flags = obj_field_int32_get(obj, OBJ_F_NPC_FLAGS);
    flags &= ~ONF_AI_WAIT_HERE;
    obj_field_int32_set(obj, OBJ_F_NPC_FLAGS, flags);

    critter_leader_set(obj, OBJ_HANDLE_NULL);

    ai_npc_wait_here_test_obj = obj;
    timeevent_clear_one_ex(TIMEEVENT_TYPE_NPC_WAIT_HERE, ai_npc_wait_here_timeevent_check);

    critter_follow(obj, leader_obj, force);
}

// 0x4AA990
bool ai_npc_wait_here_timeevent_process(TimeEvent* timeevent)
{
    int64_t obj;
    int64_t leader_obj;
    unsigned int flags;
    int max_charisma;
    int charisma;

    obj = timeevent->params[0].object_value;
    leader_obj = critter_leader_get(obj);
    if (leader_obj != OBJ_HANDLE_NULL) {
        flags = obj_field_int32_get(obj, OBJ_F_NPC_FLAGS);
        flags &= ~ONF_AI_WAIT_HERE;
        flags |= ONF_JILTED;
        obj_field_int32_set(obj, OBJ_F_NPC_FLAGS, flags);

        max_charisma = stat_level_max(leader_obj, STAT_CHARISMA);
        charisma = stat_level_get(leader_obj, STAT_CHARISMA);
        reaction_adj(obj, leader_obj, 2 * (charisma - max_charisma));
        critter_leader_set(obj, OBJ_HANDLE_NULL);
    }

    return true;
}

// 0x4AAA30
bool ai_npc_wait_here_timeevent_check(TimeEvent* timeevent)
{
    return timeevent->params[0].object_value == ai_npc_wait_here_test_obj;
}

// 0x4AAA60
void ai_copy_params(int64_t obj, AiParams* params)
{
    int index;

    index = obj_field_int32_get(obj, OBJ_F_NPC_AI_DATA);

    // NOTE: Original code is different. It treats `params` as an array of 17
    // integers and copies them one by one in the loop.
    *params = ai_parameter_profiles[index];
}

// 0x4AAAA0
bool ai_is_fighting(int64_t obj)
{
    int type;

    ai_danger_source(obj, &type, NULL);

    return type == AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS;
}

// 0x4AAB00
void ai_danger_source(int64_t obj, int* type_ptr, int64_t* danger_source_ptr)
{
    if ((obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & OCF_FLEEING) != 0) {
        if (danger_source_ptr != NULL) {
            obj_field_obj_get(obj, OBJ_F_CRITTER_FLEEING_FROM, danger_source_ptr);
        }
        *type_ptr = AI_DANGER_SOURCE_TYPE_FLEE;
    } else if ((obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & OCF_SURRENDERED) != 0) {
        if (danger_source_ptr != NULL) {
            obj_field_obj_get(obj, OBJ_F_CRITTER_FLEEING_FROM, danger_source_ptr);
        }
        *type_ptr = AI_DANGER_SOURCE_TYPE_SURRENDER;
    } else if ((obj_field_int32_get(obj, OBJ_F_NPC_FLAGS) & ONF_FIGHTING) != 0) {
        if (danger_source_ptr != NULL) {
            obj_field_obj_get(obj, OBJ_F_NPC_COMBAT_FOCUS, danger_source_ptr);
        }
        *type_ptr = AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS;
    } else {
        if (danger_source_ptr != NULL) {
            *danger_source_ptr = OBJ_HANDLE_NULL;
        }
        *type_ptr = AI_DANGER_SOURCE_TYPE_NONE;
    }
}

// 0x4AABE0
int ai_danger_state_set(int64_t source_obj, int danger_type, int64_t target_obj, int* sound_id_ptr)
{
    unsigned int critter_flags;
    int64_t leader_obj;
    unsigned int npc_flags;
    int current_reaction;
    AiParams ai_params;
    char str[1000];
    int speech_id;

    if ((obj_field_int32_get(source_obj, OBJ_F_FLAGS) & OF_INVULNERABLE) != 0
        || (obj_field_int32_get(source_obj, OBJ_F_CRITTER_FLAGS2) & OCF2_NIGH_INVULNERABLE) != 0
        || obj_field_int32_get(source_obj, OBJ_F_NAME) == 6719) {
        danger_type = AI_DANGER_SOURCE_TYPE_NONE;
    }

    critter_flags = obj_field_int32_get(source_obj, OBJ_F_CRITTER_FLAGS);

    if (danger_type == AI_DANGER_SOURCE_TYPE_FLEE) {
        if ((critter_flags & OCF_NO_FLEE) != 0) {
            danger_type = AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS;
        }
    }

    if (danger_type == AI_DANGER_SOURCE_TYPE_FLEE
        || danger_type == AI_DANGER_SOURCE_TYPE_SURRENDER) {
        leader_obj = critter_leader_get(source_obj);
        if (leader_obj != OBJ_HANDLE_NULL
            && stat_is_extraordinary(leader_obj, STAT_CHARISMA)) {
            danger_type = AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS;

            if (!ai_npc_fighting_enabled
                || (obj_field_int32_get(source_obj, OBJ_F_NPC_FLAGS) & ONF_NO_ATTACK) != 0) {
                danger_type = AI_DANGER_SOURCE_TYPE_NONE;
            }

            critter_flags &= ~(OCF_FLEEING | OCF_SURRENDERED);
            obj_field_int32_set(source_obj, OBJ_F_CRITTER_FLAGS, critter_flags);
        }
    }

    if (danger_type == AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS) {
        if (!ai_npc_fighting_enabled
            || (obj_field_int32_get(source_obj, OBJ_F_NPC_FLAGS) & ONF_NO_ATTACK) != 0) {
            danger_type = AI_DANGER_SOURCE_TYPE_NONE;
        }
    }

    if (danger_type == AI_DANGER_SOURCE_TYPE_FLEE
        || danger_type == AI_DANGER_SOURCE_TYPE_SURRENDER) {
        if (ai_float_line_func != NULL
            && (critter_flags & (OCF_FLEEING | OCF_SURRENDERED)) == 0) {
            if (critter_is_active(source_obj)) {
                dialog_copy_npc_fleeing_msg(source_obj, target_obj, str, &speech_id);
                ai_float_line_func(source_obj, target_obj, str, speech_id);
            }
        }

        critter_flags &= ~(OCF_FLEEING | OCF_SURRENDERED);

        if (danger_type == AI_DANGER_SOURCE_TYPE_FLEE) {
            critter_flags |= OCF_FLEEING;
        } else {
            critter_flags |= OCF_SURRENDERED;
            combat_critter_deactivate_combat_mode(source_obj);
        }

        obj_field_handle_set(source_obj, OBJ_F_CRITTER_FLEEING_FROM, target_obj);
        obj_field_int32_set(source_obj, OBJ_F_CRITTER_FLAGS, critter_flags);
    } else {
        critter_flags &= ~(OCF_FLEEING | OCF_SURRENDERED);
        obj_field_int32_set(source_obj, OBJ_F_CRITTER_FLAGS, critter_flags);
    }

    npc_flags = obj_field_int32_get(source_obj, OBJ_F_NPC_FLAGS);
    if (danger_type == AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS) {
        if ((obj_field_int32_get(source_obj, OBJ_F_CRITTER_FLAGS2) & OCF2_TARGET_LOCK) == 0) {
            if ((npc_flags & ONF_FIGHTING) == 0) {
                npc_flags |= ONF_FIGHTING | ONF_CHECK_WIELD | ONF_CHECK_GRENADE;
                obj_field_int32_set(source_obj, OBJ_F_NPC_FLAGS, npc_flags);
                object_script_execute(target_obj, source_obj, OBJ_HANDLE_NULL, SAP_ENTER_COMBAT, 0);

                if (sound_id_ptr != NULL) {
                    *sound_id_ptr = sfx_critter_sound(source_obj, CRITTER_SOUND_ALERTED);
                }
            }

            if (obj_field_int32_get(target_obj, OBJ_F_TYPE) == OBJ_TYPE_PC) {
                current_reaction = reaction_get_with_limited_penalty(source_obj, target_obj);
                ai_copy_params(source_obj, &ai_params);
                if (current_reaction > ai_params.attack_reaction_threshold) {
                    reaction_adj(source_obj, target_obj, ai_params.attack_reaction_threshold - current_reaction);
                }
            }

            obj_field_handle_set(source_obj, OBJ_F_NPC_COMBAT_FOCUS, target_obj);
        }
    } else {
        if ((npc_flags & ONF_BACKING_OFF) != 0) {
            npc_flags &= ~ONF_BACKING_OFF;
            obj_field_int32_set(source_obj, OBJ_F_NPC_FLAGS, npc_flags);
        }

        if ((npc_flags & ONF_FIGHTING) != 0) {
            npc_flags &= ~ONF_FIGHTING;
            obj_field_int32_set(source_obj, OBJ_F_NPC_FLAGS, npc_flags);

            if (danger_type != AI_DANGER_SOURCE_TYPE_FLEE) {
                npc_flags |= ONF_DEMAINTAIN_SPELLS;
                obj_field_int32_set(source_obj, OBJ_F_NPC_FLAGS, npc_flags);
            }

            object_script_execute(target_obj, source_obj, OBJ_HANDLE_NULL, SAP_EXIT_COMBAT, 0);
        }
    }

    return danger_type;
}

// 0x4AAF50
bool ai_flee_spell_select(Ai* ai)
{
    unsigned int critter_flags;
    AiMagicTechCandidates spell_candidates;
    MagicTechAi magictech_ai;

    critter_flags = obj_field_int32_get(ai->obj, OBJ_F_CRITTER_FLAGS);
    if ((critter_flags & OCF_SPELL_FLEE) != 0) {
        obj_field_int32_set(ai->obj, OBJ_F_CRITTER_FLAGS, critter_flags & ~OCF_SPELL_FLEE);
        return false;
    }

    mt_ai_create(&magictech_ai, ai->obj, MAGICTECH_AI_ACTION_FLEE);
    spell_candidates.flags = 0x1;
    spell_candidates.entries = magictech_ai.actions[MAGICTECH_AI_ACTION_FLEE].entries;
    spell_candidates.cnt = magictech_ai.actions[MAGICTECH_AI_ACTION_FLEE].cnt;
    if (!ai_magictech_candidate_select(ai, &spell_candidates)) {
        mt_ai_destroy(&magictech_ai);
        return false;
    }

    critter_flags |= OCF_SPELL_FLEE;
    obj_field_int32_set(ai->obj, OBJ_F_CRITTER_FLAGS, critter_flags);
    mt_ai_destroy(&magictech_ai);

    return true;
}

// 0x4AB030
bool ai_flee_distance_reached_or_mind_controlled(int64_t fleeing_obj, int64_t threat_obj)
{
    AiParams params;

    if ((obj_field_int32_get(fleeing_obj, OBJ_F_SPELL_FLAGS) & OSF_MIND_CONTROLLED) != 0) {
        return true;
    }

    ai_copy_params(fleeing_obj, &params);

    return object_dist(fleeing_obj, threat_obj) > params.flee_distance_tiles;
}

// 0x4AB0B0
int64_t ai_choose_target(int64_t attacker_obj, int64_t candidate1_obj, int64_t candidate2_obj)
{
    int64_t dist2;
    int64_t dist1;
    int score2;
    int score1;

    if (candidate1_obj == OBJ_HANDLE_NULL) {
        return candidate2_obj;
    }

    if (candidate2_obj == OBJ_HANDLE_NULL) {
        return candidate1_obj;
    }

    if (!obj_type_is_critter(obj_field_int32_get(candidate2_obj, OBJ_F_TYPE))) {
        return candidate1_obj;
    }

    if (!obj_type_is_critter(obj_field_int32_get(candidate1_obj, OBJ_F_TYPE))) {
        return candidate2_obj;
    }

    if (critter_is_unconscious(candidate2_obj)) {
        return candidate1_obj;
    }

    if (critter_is_unconscious(candidate1_obj)) {
        return candidate2_obj;
    }

    dist2 = object_dist(attacker_obj, candidate2_obj);
    if (dist2 > 20) {
        return candidate1_obj;
    }

    dist1 = object_dist(attacker_obj, candidate1_obj);
    if (dist1 > 20) {
        return candidate2_obj;
    }

    if (ai_check_decoy(attacker_obj, candidate1_obj)) {
        return candidate1_obj;
    }

    if (ai_check_decoy(attacker_obj, candidate2_obj)) {
        return candidate2_obj;
    }

    if (random_between(1, 100) > 50) {
        score1 = -((int)dist1);
        score2 = -((int)dist2);
    } else {
        score1 = stat_level_get(candidate1_obj, STAT_LEVEL) - (int)dist1;
        score2 = stat_level_get(candidate2_obj, STAT_LEVEL) - (int)dist2;
    }

    if (score2 > score1) {
        return candidate2_obj;
    } else {
        return candidate1_obj;
    }
}

// 0x4AB2A0
void ai_select_flee_or_combat_danger(int64_t source_obj, int64_t target_obj)
{
    if (ai_should_flee(source_obj, target_obj)) {
        ai_danger_state_set(source_obj, AI_DANGER_SOURCE_TYPE_FLEE, target_obj, 0);
    } else {
        ai_danger_state_set(source_obj, AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS, target_obj, 0);
    }
}

// 0x4AB2F0
bool ai_should_flee(int64_t source_obj, int64_t target_obj)
{
    AiParams ai_params;
    int target_party_cnt;
    int target_party_lvl;
    int source_party_cnt;
    int source_party_lvl;

    if ((obj_field_int32_get(source_obj, OBJ_F_CRITTER_FLAGS) & OCF_NO_FLEE) != 0) {
        return false;
    }

    if ((obj_field_int32_get(source_obj, OBJ_F_SPELL_FLAGS) & OSF_MIND_CONTROLLED) != 0) {
        return false;
    }

    ai_copy_params(source_obj, &ai_params);

    if (ai_object_hp_ratio(target_obj) < ai_params.leader_hit_points_prevent_flee_percent) {
        return false;
    }

    if (ai_object_hp_ratio(source_obj) <= ai_params.flee_hit_points_percent) {
        return true;
    }

    ai_calc_party_size_and_level(target_obj, &target_party_cnt, &target_party_lvl);
    ai_calc_party_size_and_level(source_obj, &source_party_cnt, &source_party_lvl);

    if (target_party_cnt - source_party_cnt >= ai_params.flee_party_size_threshold) {
        return true;
    }

    if (target_party_lvl - source_party_lvl >= ai_params.flee_level_difference) {
        return true;
    }

    return false;
}

// 0x4AB400
int ai_object_hp_ratio(int64_t obj)
{
    return 100 * object_hp_current(obj) / object_hp_max(obj);
}

// 0x4AB430
int ai_critter_fatigue_ratio(int64_t obj)
{
    return 100 * critter_fatigue_current(obj) / critter_fatigue_max(obj);
}

// 0x4AB460
int64_t ai_find_target(int64_t critter_obj)
{
    int radius;
    ObjectList objects;
    ObjectNode* node;
    int cnt;
    int idx;
    int64_t handles[50];
    int64_t ranges[50];
    int64_t leader_obj;
    int candidate_danger_type;
    int64_t candidate_obj;
    bool concealed;
    int obj_type;
    int64_t danger_source_obj = OBJ_HANDLE_NULL;
    int64_t selected_target = OBJ_HANDLE_NULL;

    if (in_find_target) {
        return OBJ_HANDLE_NULL;
    }

    if ((obj_field_int32_get(critter_obj, OBJ_F_FLAGS) & OF_INVULNERABLE) != 0) {
        return OBJ_HANDLE_NULL;
    }

    if ((obj_field_int32_get(critter_obj, OBJ_F_CRITTER_FLAGS2) & OCF2_NIGH_INVULNERABLE) != 0) {
        return OBJ_HANDLE_NULL;
    }

    if (obj_field_int32_get(critter_obj, OBJ_F_NAME) == 6719) {
        return OBJ_HANDLE_NULL;
    }

    in_find_target = true;

    radius = ai_perception_distance(stat_level_get(critter_obj, STAT_PERCEPTION));

    cnt = 0;
    ai_objects_in_radius(critter_obj, radius, &objects, OBJ_TM_CRITTER);
    node = objects.head;
    while (node != NULL) {
        if (cnt >= 50) {
            break;
        }

        handles[cnt++] = node->obj;
        node = node->next;
    }
    if (cnt != 0) {
        leader_obj = critter_leader_get(critter_obj);
        if (cnt > 1) {
            for (idx = 0; idx < cnt; idx++) {
                if (ai_check_decoy(critter_obj, handles[idx])) {
                    ranges[idx] = 0;
                } else {
                    ranges[idx] = object_dist(critter_obj, handles[idx]);
                }

                if (critter_is_unconscious(handles[idx])) {
                    ranges[idx] += 1000;
                }
            }

            // NOTE: Original code is different, but looks like it sorts both
            // range and handle arrays by range using bubble sort algorithm.
            for (int i = 0; i < cnt - 1; i++) {
                for (int j = 0; j < cnt - i - 1; j++) {
                    if (ranges[j] > ranges[j + 1]) {
                        int64_t tmp_range = ranges[j];
                        ranges[j] = ranges[j + 1];
                        ranges[j + 1] = tmp_range;

                        int64_t tmp_handle = handles[j];
                        handles[j] = handles[j + 1];
                        handles[j + 1] = tmp_handle;
                    }
                }
            }
        }

        for (idx = 0; idx < cnt; idx++) {
            concealed = critter_is_concealed(handles[idx]);
            if (ai_can_hear(critter_obj, handles[idx], concealed_to_loudness(concealed)) == 0
                || ai_can_see(critter_obj, handles[idx]) == 0) {
                obj_type = obj_field_int32_get(handles[idx], OBJ_F_TYPE);
                if (obj_type == OBJ_TYPE_PC
                    && critter_is_concealed(handles[idx])) {
                    selected_target = handles[idx];
                }

                if (ai_check_kos(critter_obj, handles[idx]) != AI_KOS_NO) {
                    danger_source_obj = handles[idx];
                    break;
                }

                if (obj_type == OBJ_TYPE_NPC) {
                    ai_danger_source(handles[idx], &candidate_danger_type, &candidate_obj);
                    if (ai_combat_target_is_valid(critter_obj, candidate_obj)
                        && (candidate_danger_type == AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS
                            || candidate_danger_type == AI_DANGER_SOURCE_TYPE_FLEE
                            || candidate_danger_type == AI_DANGER_SOURCE_TYPE_SURRENDER)) {
                        if (ai_check_protect(critter_obj, handles[idx]) != AI_PROTECT_NO
                            || (critter_social_class_get(critter_obj) == SOCIAL_CLASS_GUARD
                                && critter_is_monstrous(candidate_obj)
                                && critter_leader_get(candidate_obj) == OBJ_HANDLE_NULL)) {
                            if (ai_check_upset_attacking(critter_obj, candidate_obj, leader_obj) == AI_UPSET_ATTACKING_NONE) {
                                concealed = critter_is_concealed(candidate_obj);
                                if (ai_can_hear(critter_obj, candidate_obj, concealed_to_loudness(concealed)) == 0
                                    || ai_can_see(critter_obj, candidate_obj) == 0) {
                                    danger_source_obj = candidate_obj;
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            candidate_obj = ai_dead_ally_attacker_find(critter_obj, handles[idx]);
            if (candidate_obj != OBJ_HANDLE_NULL) {
                concealed = critter_is_concealed(candidate_obj);
                if (ai_can_hear(critter_obj, candidate_obj, concealed_to_loudness(concealed)) == 0
                    || ai_can_see(critter_obj, candidate_obj) == 0) {
                    danger_source_obj = candidate_obj;
                    break;
                }
            }
        }
    }
    object_list_destroy(&objects);

    if (danger_source_obj == OBJ_HANDLE_NULL) {
        if (selected_target != OBJ_HANDLE_NULL) {
            anim_goal_rotate(critter_obj, object_rot(critter_obj, selected_target));
        }
    }

    in_find_target = false;

    return danger_source_obj;
}

// 0x4AB990
bool ai_combat_target_is_valid(int64_t source_obj, int64_t target_obj)
{
    int64_t source_leader_obj;
    int64_t target_leader_obj;
    int target_obj_type;
    int64_t preferred_target;

    if (target_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(target_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF | OF_DONTDRAW | OF_INVULNERABLE)) != 0) {
        return false;
    }

    if (target_obj == source_obj) {
        return false;
    }

    source_leader_obj = critter_leader_get(source_obj);
    target_obj_type = obj_field_int32_get(target_obj, OBJ_F_TYPE);

    if (obj_type_is_critter(target_obj_type)) {
        if (critter_is_dead(target_obj)) {
            return false;
        }

        if (critter_is_unconscious(target_obj)) {
            if ((obj_field_int32_get(source_obj, OBJ_F_CRITTER_FLAGS) & OCF_NON_LETHAL_COMBAT) != 0) {
                return false;
            }

            preferred_target = ai_find_target(source_obj);
            if (preferred_target != OBJ_HANDLE_NULL
                && preferred_target != target_obj) {
                return false;
            }
        }

        if (target_obj == source_leader_obj) {
            return false;
        }

        target_leader_obj = critter_leader_get(target_obj);
        if (target_leader_obj != OBJ_HANDLE_NULL
            && source_leader_obj == target_leader_obj) {
            return false;
        }

        if (source_obj == target_leader_obj) {
            return false;
        }

        if ((obj_field_int32_get(target_obj, OBJ_F_SPELL_FLAGS) & OSF_STONED) != 0) {
            return false;
        }

        if (!magictech_invisible_target_can_be_seen(source_obj, target_obj)) {
            return false;
        }

        if (stat_level_get(source_obj, STAT_INTELLIGENCE) > LOW_INTELLIGENCE
            && obj_field_int32_get(target_obj, OBJ_F_NAME) == 6719) {
            return false;
        }
    } else {
        if (target_obj_type == OBJ_TYPE_WALL) {
            return false;
        }

        if (object_is_busted(target_obj)) {
            return false;
        }
    }

    if (object_dist(source_obj, target_obj) > 20) {
        return false;
    }

    if (source_leader_obj != OBJ_HANDLE_NULL
        && object_dist(source_leader_obj, target_obj) > 20) {
        return false;
    }

    if (obj_type_is_critter(target_obj_type)
        && critter_is_concealed(target_obj)
        && basic_skill_training_get(target_obj, BASIC_SKILL_PROWLING) >= TRAINING_MASTER
        && ai_can_see(source_obj, target_obj) != 0) {
        return false;
    }

    return true;
}

// 0x4ABBC0
int64_t ai_combat_target_get_or_find(int64_t obj)
{
    int64_t combat_focus_obj;

    obj_field_obj_get(obj, OBJ_F_NPC_COMBAT_FOCUS, &combat_focus_obj);
    if (ai_combat_target_is_valid(obj, combat_focus_obj)) {
        return combat_focus_obj;
    }

    ai_target_unlock(obj);

    return ai_find_target(obj);
}

// 0x4ABC20
void ai_combat_spell_action_prepare(Ai* ai)
{
    if ((obj_field_int32_get(ai->obj, OBJ_F_FLAGS) & OF_INVULNERABLE) != 0
        || (obj_field_int32_get(ai->obj, OBJ_F_CRITTER_FLAGS2) & OCF2_NIGH_INVULNERABLE) != 0
        || !ai_combat_spell_select(ai)) {
        ai->action_type = AI_ACTION_TYPE_UNKNOWN;
    }
}

// 0x4ABC70
bool ai_combat_spell_select(Ai* ai)
{
    MagicTechAi magictech_ai;
    AiMagicTechCandidates spell_candidates;
    AiParams ai_params;

    if (!ai_npc_fighting_enabled) {
        return false;
    }

    {
        int mode = ai_utility_mode(ai);
        if (mode > 0) {
            bool selected = ai_utility_spell_select(ai, mode >= 2);
            if (mode >= 2) return selected;
        }
    }

    if (random_between(1, 100) > ai_combat_spell_attempt_chance(ai)) {
        return false;
    }

    if (critter_num_followers(ai->obj, false) == 0) {
        mt_ai_create(&magictech_ai, ai->obj, MAGICTECH_AI_ACTION_SUMMON);
        spell_candidates.flags = 0x1;
        spell_candidates.entries = magictech_ai.actions[MAGICTECH_AI_ACTION_SUMMON].entries;
        spell_candidates.cnt = magictech_ai.actions[MAGICTECH_AI_ACTION_SUMMON].cnt;
        if (ai_magictech_candidate_select(ai, &spell_candidates)) {
            mt_ai_destroy(&magictech_ai);
            return true;
        }

        mt_ai_destroy(&magictech_ai);
    }

    ai_copy_params(ai->obj, &ai_params);

    if (ai_params.defensive_spell_chance < random_between(1, 100)) {
        mt_ai_create(&magictech_ai, ai->obj, MAGICTECH_AI_ACTION_DEFENSIVE);
        spell_candidates.flags = 0x4;
        spell_candidates.entries = magictech_ai.actions[MAGICTECH_AI_ACTION_DEFENSIVE].entries;
        spell_candidates.cnt = magictech_ai.actions[MAGICTECH_AI_ACTION_DEFENSIVE].cnt;
        if (ai_magictech_candidate_select(ai, &spell_candidates)) {
            mt_ai_destroy(&magictech_ai);
            return true;
        }

        mt_ai_destroy(&magictech_ai);
    }

    mt_ai_create(&magictech_ai, ai->obj, MAGICTECH_AI_ACTION_OFFENSIVE);
    spell_candidates.flags = 0x2;
    spell_candidates.entries = magictech_ai.actions[MAGICTECH_AI_ACTION_OFFENSIVE].entries;
    spell_candidates.cnt = magictech_ai.actions[MAGICTECH_AI_ACTION_OFFENSIVE].cnt;
    if (ai_magictech_candidate_select(ai, &spell_candidates)) {
        mt_ai_destroy(&magictech_ai);
        return true;
    }

    mt_ai_destroy(&magictech_ai);
    return false;
}

// 0x4ABE20
int ai_combat_spell_attempt_chance(Ai* ai)
{
    AiParams ai_params;

    if (ai->leader_obj != OBJ_HANDLE_NULL
        && ai_object_hp_ratio(ai->obj) > 50
        && ai_object_hp_ratio(ai->leader_obj) > 50
        && !ai_is_stronger(ai->obj, ai->danger_source)) {
        return 0;
    }

    ai_copy_params(ai->obj, &ai_params);

    return ai_params.minimum_combat_distance > 1 ? 50 : 25;
}

// 0x4ABEB0
bool ai_is_stronger(int64_t attacker_obj, int64_t target_obj)
{
    int attacker_level;
    int target_level;

    target_level = stat_level_get(target_obj, STAT_LEVEL);
    attacker_level = stat_level_get(attacker_obj, STAT_LEVEL);

    if (target_level < 20 && target_level >= attacker_level + 10) {
        return true;
    } else if (target_level < 40 && target_level >= attacker_level + 5) {
        return true;
    } else if (target_level >= attacker_level) {
        return true;
    }

    return false;
}

// 0x4ABF10
bool ai_magictech_candidate_select(Ai* ai, AiMagicTechCandidates* candidates)
{
    int64_t obj;
    int base;
    int idx;
    MagicTechAiActionListEntry* entry;

    if (candidates->cnt <= 0) {
        return false;
    }

    if ((candidates->flags & 0x1) != 0) {
        obj = ai->obj;
    } else if ((candidates->flags & 0x2) != 0) {
        obj = ai->danger_source;
    } else if ((candidates->flags & 0x8) != 0) {
        obj = candidates->obj;
    }

    base = random_between(1, 100) >= 80 ? random_between(0, candidates->cnt - 1) : 0;

    for (idx = 0; idx < candidates->cnt; idx++) {
        entry = &(candidates->entries[(base + idx) % candidates->cnt]);
        if ((candidates->flags & 0x4) != 0) {
            if (mt_ai_secondary_defense_score_get(entry->spell)) {
                obj = ai->danger_source;
            } else {
                obj = ai->obj;
            }
        }

        if (entry->spell == -1) {
            item_use_on_obj(ai->obj, entry->item_obj, obj);
            return true;
        }

        if (!magictech_is_under_influence_of(obj, entry->spell)
            && ai_magictech_action_check(ai->obj, entry->item_obj, obj, entry->spell) == 0) {
            if (entry->item_obj != OBJ_HANDLE_NULL
                && mt_item_valid_ai_action(entry->item_obj)) {
                ai->action_type = AI_ACTION_TYPE_USE_ITEM;
                ai->spell = entry->spell;
                ai->item_obj = entry->item_obj;
                ai->danger_source = obj;
            } else {
                ai->action_type = AI_ACTION_TYPE_CAST_SPELL;
                ai->spell = entry->spell;
                ai->danger_source = obj;
                ai->item_obj = entry->item_obj;
            }
            return true;
        }
    }

    return false;
}

// 0x4AC180
void ai_action_perform(Ai* ai)
{
    if (!combat_turn_based_is_active() || combat_turn_based_whos_turn_get() == ai->obj) {
        if ((obj_field_int32_get(ai->obj, OBJ_F_NPC_FLAGS) & ONF_BACKING_OFF) != 0) {
            ai_action_perform_baking_off(ai);
            return;
        }

        switch (ai->action_type) {
        case AI_ACTION_TYPE_RELEASE_MAINTENANCE:
        case AI_ACTION_TYPE_RETREAT:
            if (!ai_utility_action_perform(ai)) ai_action_perform_combat(ai);
            break;
        case AI_ACTION_TYPE_CAST_SPELL:
            ai_action_perform_cast(ai);
            break;
        case AI_ACTION_TYPE_USE_ITEM:
            ai_action_perform_item(ai);
            break;
        case AI_ACTION_TYPE_USE_SKILL:
            ai_action_perform_skill(ai);
            break;
        default:
            switch (ai->danger_type) {
            case 0:
                ai_action_perform_non_combat(ai);
                break;
            case 1:
                ai_action_perform_combat(ai);
                break;
            case 2:
                ai_action_perform_fleeing(ai);
                break;
            case 3:
                ai_action_perform_surrender(ai);
                break;
            }
        }
    }
}

// 0x4AC250
void ai_action_perform_cast(Ai* ai)
{
    MagicTechInvocation mt_invocation;

    if (ai->item_obj != OBJ_HANDLE_NULL) {
        magictech_invocation_init(&mt_invocation, ai->item_obj, ai->spell);
    } else {
        magictech_invocation_init(&mt_invocation, ai->obj, ai->spell);
    }

    object_handle_reference_init(ai->danger_source, &(mt_invocation.target_obj));

    if (ai->utility_selected && ai->item_obj == OBJ_HANDLE_NULL) {
        mt_invocation.flags |= MAGICTECH_INVOCATION_UTILITY_AI;
    }

    if (magictech_invocation_check(&mt_invocation)) {
        magictech_invocation_run(&mt_invocation);

        if (ai->item_obj != OBJ_HANDLE_NULL) {
            magictech_object_sources_detach(ai->item_obj);

            switch (obj_field_int32_get(ai->item_obj, OBJ_F_TYPE)) {
            case OBJ_TYPE_FOOD:
            case OBJ_TYPE_SCROLL:
                object_destroy(ai->item_obj);
                break;
            }
        }
    }
}

// 0x4AC320
void ai_action_perform_item(Ai* ai)
{
    anim_goal_use_item_on_obj(ai->obj,
        ai->danger_source,
        ai->item_obj,
        0);
}

// 0x4AC350
void ai_action_perform_skill(Ai* ai)
{
    anim_goal_use_skill_on(ai->obj,
        ai->danger_source,
        ai->item_obj,
        ai->skill,
        0);
}

// 0x4AC380
void ai_action_perform_non_combat(Ai* ai)
{
    unsigned int npc_flags;
    int rc;
    char str[1000];
    int speech_id;

    combat_critter_deactivate_combat_mode(ai->obj);

    if (ai->leader_obj != OBJ_HANDLE_NULL) {
        npc_flags = obj_field_int32_get(ai->obj, OBJ_F_NPC_FLAGS);
        if ((npc_flags & ONF_AI_WAIT_HERE) != 0) {
            return;
        }

        anim_goal_follow_obj(ai->obj, ai->leader_obj);

        if (!anim_foreground_goal_find(ai->obj, NULL)) {
            rc = ai_check_follow(ai->obj, ai->leader_obj, true);
            if (rc != AI_FOLLOW_OK && critter_disband(ai->obj, false)) {
                npc_flags = obj_field_int32_get(ai->obj, OBJ_F_NPC_FLAGS);
                npc_flags |= ONF_JILTED;
                obj_field_int32_set(ai->obj, OBJ_F_NPC_FLAGS, npc_flags);

                if (ai_float_line_func != NULL && critter_is_active(ai->obj)) {
                    dialog_copy_npc_warning_follow_msg(ai->obj, ai->leader_obj, rc, str, &speech_id);
                    ai_float_line_func(ai->obj, ai->leader_obj, str, speech_id);
                }
            } else if ((npc_flags & ONF_CHECK_LEADER) != 0) {
                npc_flags = obj_field_int32_get(ai->obj, OBJ_F_NPC_FLAGS);
                npc_flags &= ~ONF_CHECK_LEADER;
                obj_field_int32_set(ai->obj, OBJ_F_NPC_FLAGS, npc_flags);

                rc = ai_check_leader(ai->obj, ai->leader_obj);
                if (rc != AI_FOLLOW_OK) {
                    if (ai_float_line_func != NULL && critter_is_active(ai->obj)) {
                        dialog_copy_npc_warning_follow_msg(ai->obj, ai->leader_obj, rc, str, &speech_id);
                        ai_float_line_func(ai->obj, ai->leader_obj, str, speech_id);
                    }
                }
            }
        }
    } else if (!ai_waypoints_process(ai->obj, false) && !ai_standpoints_process(ai->obj, false)) {
        anim_object_auto_fidget(ai->obj);
        anim_overlapping_critters_spread_out(ai->obj);
    }

    if (!critter_is_sleeping(ai->obj)) {
        if (ui_inventory_target_object_get() == OBJ_HANDLE_NULL && random_between(1, 100) == 1) {
            ai_look_for_item_func(ai->obj, OBJ_TM_ITEM);
        }
    }
}

// 0x4AC620
void ai_action_perform_fleeing(Ai* ai)
{
    ai_danger_state_set(ai->obj, AI_DANGER_SOURCE_TYPE_SURRENDER, ai->danger_source, 0);
    anim_goal_flee(ai->obj, ai->danger_source);
}

// 0x4AC660
void ai_action_perform_surrender(Ai* ai)
{
    int64_t fleeing_from_obj;

    obj_field_obj_get(ai->obj, OBJ_F_CRITTER_FLEEING_FROM, &fleeing_from_obj);

    if (fleeing_from_obj == OBJ_HANDLE_NULL
        || critter_is_dead(fleeing_from_obj)
        || critter_is_unconscious(fleeing_from_obj)) {
        ai->danger_type = ai_danger_state_set(ai->obj, 0, OBJ_HANDLE_NULL, 0);
    }
}

// 0x4AC6E0
void ai_action_perform_combat(Ai* ai)
{
    AiParams params;
    ObjectNpcFlags npc_flags;
    int64_t distance;

    ai_copy_params(ai->obj, &params);
    npc_flags = obj_field_int32_get(ai->obj, OBJ_F_NPC_FLAGS);
    distance = object_dist(ai->obj, ai->danger_source);
    if (params.minimum_combat_distance > 1 && distance < params.minimum_combat_distance && random_between(1, 20) == 1) {
        npc_flags |= ONF_BACKING_OFF;
        npc_flags |= ONF_CHECK_GRENADE;
        obj_field_int32_set(ai->obj, OBJ_F_NPC_FLAGS, npc_flags);
    } else if (!ai_use_grenade(ai, distance)) {
        anim_goal_attack_ex(ai->obj, ai->danger_source, ai->sound_id);
    }
}

// 0x4AC7B0
void ai_action_perform_baking_off(Ai* ai)
{
    AiParams ai_params;
    unsigned int npc_flags;
    int combat_min_dist;
    PathCreateInfo path_create_info;
    int8_t rotations[100];

    ai_copy_params(ai->obj, &ai_params);

    npc_flags = obj_field_int32_get(ai->obj, OBJ_F_NPC_FLAGS);
    if ((npc_flags & ONF_BACKING_OFF) != 0) {
        combat_min_dist = ai_params.minimum_combat_distance;
        if (combat_min_dist <= 1) {
            combat_min_dist = 5;
        }

        if (object_dist(ai->obj, ai->danger_source) < combat_min_dist) {
            path_create_info.obj = ai->obj;
            path_create_info.from = obj_field_int64_get(ai->obj, OBJ_F_LOCATION);
            path_create_info.to = obj_field_int64_get(ai->danger_source, OBJ_F_LOCATION);
            path_create_info.max_rotations = 100;
            path_create_info.rotations = rotations;
            path_create_info.flags = PATH_FLAG_NEARBY_DESTINATION_SEARCH;
            path_create_info.range_or_partial_path_length = combat_min_dist;
            if (path_make(&path_create_info)) {
                anim_goal_run_to_tile(ai->obj, path_create_info.to);
            } else {
                obj_field_int32_set(ai->obj, OBJ_F_NPC_FLAGS, npc_flags & ~(ONF_BACKING_OFF));
            }
        } else {
            obj_field_int32_set(ai->obj, OBJ_F_NPC_FLAGS, npc_flags & ~(ONF_BACKING_OFF));
        }
    }
}

// 0x4AC910
bool ai_use_grenade(Ai* ai, int64_t distance)
{
    ObjectNpcFlags npc_flags;
    int64_t grenade_obj;
    int radius;
    int64_t loc;
    int64_t block_obj;
    ObjectList objects;
    ObjectNode* node;

    npc_flags = obj_field_int32_get(ai->obj, OBJ_F_NPC_FLAGS);
    if ((npc_flags & ONF_CHECK_GRENADE) == 0) {
        return false;
    }

    if ((npc_flags & ONF_BACKING_OFF) != 0) {
        return false;
    }

    npc_flags &= ~ONF_CHECK_GRENADE;
    obj_field_int32_set(ai->obj, OBJ_F_NPC_FLAGS, npc_flags);

    if (basic_skill_points_get(ai->obj, BASIC_SKILL_THROWING) == 0) {
        return false;
    }

    grenade_obj = item_find_first_generic(ai->obj, OGF_IS_GRENADE);
    if (grenade_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    radius = item_weapon_aoe_radius(grenade_obj);
    if (distance <= radius) {
        return false;
    }

    loc = obj_field_int64_get(ai->danger_source, OBJ_F_LOCATION);
    ai_straight_path_cost_to_location(ai->obj, loc, &block_obj);
    if (block_obj != 0) {
        return false;
    }

    ai_objects_in_radius(ai->danger_source, radius, &objects, OBJ_TM_NPC);
    node = objects.head;
    while (node != NULL) {
        if (!critter_is_dead(node->obj)
            && ai_check_protect(ai->obj, node->obj) != AI_PROTECT_NO) {
            break;
        }
        node = node->next;
    }
    object_list_destroy(&objects);

    if (node != NULL) {
        return false;
    }

    loc = obj_field_int64_get(ai->danger_source, OBJ_F_LOCATION);
    anim_goal_throw_item(ai->obj, grenade_obj, loc);

    return true;
}

// 0x4ACBB0
bool ai_waypoints_process(int64_t obj, bool teleport_to_waypoint)
{
    bool is_sleeping;
    int map;
    int next_map;
    unsigned int npc_flags;
    int64_t loc;
    int64_t next_loc;
    int wp;
    TeleportData teleport_data;

    is_sleeping = critter_is_sleeping(obj);
    if (!is_sleeping
        && (obj_field_int32_get(obj, OBJ_F_FLAGS) & (OF_OFF | OF_DONTDRAW)) != 0) {
        return false;
    }

    if (obj_arrayfield_length_get(obj, OBJ_F_NPC_WAYPOINTS_IDX) == 0) {
        return false;
    }

    map = map_current_map();
    next_map = critter_teleport_map_get(obj);
    if (next_map != map && !teleport_to_waypoint) {
        return false;
    }

    npc_flags = obj_field_int32_get(obj, OBJ_F_NPC_FLAGS);
    if (ai_is_day()) {
        if ((npc_flags & ONF_WAYPOINTS_DAY) == 0) {
            return false;
        }
    } else {
        if ((npc_flags & ONF_WAYPOINTS_NIGHT) == 0) {
            return false;
        }
    }

    if (is_sleeping) {
        ai_wake_up(obj);
        return true;
    }

    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    wp = obj_field_int32_get(obj, OBJ_F_NPC_WAYPOINT_CURRENT);
    next_loc = obj_arrayfield_int64_get(obj, OBJ_F_NPC_WAYPOINTS_IDX, wp);
    if (location_dist(loc, next_loc) <= 3) {
        wp++;
        if (wp == obj_arrayfield_length_get(obj, OBJ_F_NPC_WAYPOINTS_IDX)) {
            wp = 0;
        }
        obj_field_int32_set(obj, OBJ_F_NPC_WAYPOINT_CURRENT, wp);
        next_loc = obj_arrayfield_int64_get(obj, OBJ_F_NPC_WAYPOINTS_IDX, wp);
    }

    if (teleport_to_waypoint) {
        if (next_map == map) {
            anim_object_interrupt_by_priority(obj, 4, false, true);
            object_move_to_location(obj, next_loc, 0, 0);
        } else {
            teleport_data.flags = 0;
            teleport_data.map = next_map;
            teleport_data.loc = next_loc;
            teleport_data.obj = obj;
            teleport_do(&teleport_data);
        }
    } else {
        ai_move_to(obj, next_loc, 3);
    }

    return true;
}

// 0x4ACD90
bool ai_is_day(void)
{
    int hour;

    hour = datetime_current_hour();
    return hour >= 6 && hour < 21;
}

// 0x4ACDB0
bool ai_standpoints_process(int64_t obj, bool teleport_to_standpoint)
{
    bool is_sleeping;
    int64_t standpoint_loc;
    int map;
    int next_map;
    int hour;
    int64_t loc;
    unsigned int npc_flags;
    int64_t dist;
    int wandering_range;
    int64_t bed_obj;
    TeleportData teleport_data;

    is_sleeping = critter_is_sleeping(obj);
    if (!is_sleeping
        && (obj_field_int32_get(obj, OBJ_F_FLAGS) & (OF_OFF | OF_DONTDRAW)) != 0) {
        return false;
    }

    if (!ai_get_standpoint(obj, &standpoint_loc)) {
        return false;
    }

    next_map = critter_teleport_map_get(obj);
    map = map_current_map();
    if (next_map == map) {
        if (!teleport_to_standpoint) {
            hour = datetime_current_hour();
            if ((hour == 6 || hour == 21)
                && random_between(1, 1000) != 1) {
                return false;
            }
        }
    } else {
        if (!teleport_to_standpoint) {
            return false;
        }
    }

    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    npc_flags = obj_field_int32_get(obj, OBJ_F_NPC_FLAGS);
    dist = location_dist(standpoint_loc, loc);

    wandering_range = (npc_flags & (ONF_WANDERS_IN_DARK | ONF_WANDERS)) != 0 ? 4 : 1;
    if (dist <= wandering_range) {
        if ((npc_flags & ONF_WAYPOINTS_BED) == 0) {
            npc_flags |= ONF_WAYPOINTS_BED;
            obj_field_int32_set(obj, OBJ_F_NPC_FLAGS, npc_flags);

            bed_obj = ai_find_nearest_bed(standpoint_loc);
            if (bed_obj != OBJ_HANDLE_NULL) {
                critter_enter_bed(obj, bed_obj);
            }
        }

        if (!is_sleeping) {
            if ((npc_flags & ONF_WANDERS) != 0) {
                anim_goal_wander(obj, standpoint_loc, 4);
                return true;
            }

            if ((npc_flags & ONF_WANDERS_IN_DARK) != 0) {
                anim_goal_wander_seek_darkness(obj, standpoint_loc, 4);
                return true;
            }

            return false;
        }

        return true;
    }

    if (is_sleeping) {
        ai_wake_up(obj);
        return true;
    }

    if ((npc_flags & ONF_WAYPOINTS_BED) != 0) {
        npc_flags &= ~ONF_WAYPOINTS_BED;
        obj_field_int32_set(obj, OBJ_F_NPC_FLAGS, npc_flags);
    }

    if (teleport_to_standpoint) {
        if (next_map == map) {
            anim_object_interrupt_by_priority(obj, 4, 0, 1);
            object_move_to_location(obj, standpoint_loc, 0, 0);
        } else {
            teleport_data.flags = 0;
            teleport_data.map = next_map;
            teleport_data.loc = standpoint_loc;
            teleport_data.obj = obj;
            teleport_do(&teleport_data);
        }
    } else {
        ai_move_to(obj, standpoint_loc, 1);
    }

    return true;
}

// 0x4AD060
bool ai_get_standpoint(int64_t obj, int64_t* standpoint_ptr)
{
    if (critter_leader_get(obj) != OBJ_HANDLE_NULL) {
        return false;
    }

    if (ai_is_day()) {
        *standpoint_ptr = obj_field_int64_get(obj, OBJ_F_NPC_STANDPOINT_DAY);
    } else {
        *standpoint_ptr = obj_field_int64_get(obj, OBJ_F_NPC_STANDPOINT_NIGHT);
    }

    return true;
}

// 0x4AD0B0
void ai_wake_up(int64_t npc_obj)
{
    int64_t loc;
    int64_t bed_obj;

    loc = obj_field_int64_get(npc_obj, OBJ_F_NPC_STANDPOINT_NIGHT);
    bed_obj = ai_find_nearest_bed(loc);
    if (bed_obj != OBJ_HANDLE_NULL
        && obj_field_handle_get(bed_obj, OBJ_F_SCENERY_WHOS_IN_ME) == npc_obj) {
        critter_leave_bed(npc_obj, bed_obj);
        return;
    }

    loc = obj_field_int64_get(npc_obj, OBJ_F_NPC_STANDPOINT_DAY);
    bed_obj = ai_find_nearest_bed(loc);
    if (bed_obj != OBJ_HANDLE_NULL
        && obj_field_handle_get(bed_obj, OBJ_F_SCENERY_WHOS_IN_ME) == npc_obj) {
        critter_leave_bed(npc_obj, bed_obj);
        return;
    }
}

// 0x4AD140
int64_t ai_find_nearest_bed(int64_t loc)
{
    int64_t bed_obj = OBJ_HANDLE_NULL;
    ObjectList objects;
    ObjectNode* node;
    tig_art_id_t art_id;

    object_list_location(loc, OBJ_TM_SCENERY, &objects);

    node = objects.head;
    while (node != NULL) {
        art_id = obj_field_int32_get(node->obj, OBJ_F_CURRENT_AID);
        if (tig_art_scenery_id_type_get(art_id) == TIG_ART_SCENERY_TYPE_BEDS) {
            bed_obj = node->obj;
            break;
        }

        node = node->next;
    }

    object_list_destroy(&objects);

    return bed_obj;
}

// 0x4AD1B0
void ai_move_to(int64_t obj, int64_t loc, int range)
{
    if (range != 0) {
        anim_goal_move_near_tile(obj, loc, range);
    } else {
        anim_goal_move_to_tile(obj, loc);
    }
}

// 0x4AD200
bool ai_timeevent_process(TimeEvent* timeevent)
{
    int64_t obj;
    unsigned int npc_flags;
    DateTime datetime;
    int64_t loc;
    int64_t sector_id;
    bool skip;
    bool sector_is_loaded;
    unsigned int millis;

    obj = timeevent->params[0].object_value;
    if (obj == OBJ_HANDLE_NULL) {
        return true;
    }

    skip = false;
    sector_is_loaded = false;
    if (!critter_is_dead(obj) && timeevent->params[1].integer_value != 0) {
        npc_flags = obj_field_int32_get(obj, OBJ_F_NPC_FLAGS);
        npc_flags |= ONF_CHECK_WIELD;
        obj_field_int32_set(obj, OBJ_F_NPC_FLAGS, npc_flags);

        if (critter_leader_get(obj) == OBJ_HANDLE_NULL
            && !combat_critter_is_combat_mode_active(obj)
            && !ai_waypoints_process(obj, true)) {
            ai_standpoints_process(obj, true);
        }

        if (!object_script_execute(obj, obj, OBJ_HANDLE_NULL, SAP_FIRST_HEARTBEAT, 0)) {
            skip = true;
        }
    }

    if (!skip && ai_is_alive_near_player_outside_turn_based(obj)) {
        if (object_script_execute(obj, obj, OBJ_HANDLE_NULL, SAP_HEARTBEAT, 0)) {
            if ((obj_field_int32_get(obj, OBJ_F_NPC_FLAGS) & ONF_GENERATOR) != 0) {
                if (monstergen_process(obj, &datetime)) {
                    ai_schedule_delay(obj, &datetime);
                    return true;
                }

                if (!combat_turn_based_is_active()
                    && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & OCF_ENCOUNTER) != 0) {
                    object_destroy(obj);
                }

                return true;
            }
            ai_process(obj);
        }
    } else {
        if (!combat_turn_based_is_active() && !critter_is_dead(obj)) {
            anim_object_interrupt_by_priority(obj, 4, false, true);
        }

        loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
        sector_id = sector_id_from_loc(loc);
        if (sector_loaded(sector_id)) {
            sector_is_loaded = true;
        }

        if (!ai_distant_follower_relocate(obj) && !sector_is_loaded) {
            if (!combat_turn_based_is_active()
                && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & OCF_ENCOUNTER) != 0) {
                object_destroy(obj);
            }

            return true;
        }
    }

    millis = ai_timeevent_delay(obj);
    if (timeevent->params[1].integer_value != 0) {
        millis += random_between(0, 5000);
    }
    ai_schedule_delay_milliseconds(obj, millis);
    return true;
}

// 0x4AD420
bool ai_is_alive_near_player_outside_turn_based(int64_t obj)
{
    if (critter_is_dead(obj)) {
        return false;
    }

    if (combat_turn_based_is_active()) {
        return false;
    }

    if (object_dist(player_get_local_pc_obj(), obj) <= highres_config_get()->preload_limit) {
        return true;
    }

    return false;
}

// 0x4AD4D0
bool ai_distant_follower_relocate(int64_t obj)
{
    int64_t pc_leader_obj;

    if (critter_is_dead(obj)) {
        return false;
    }

    if (combat_turn_based_is_active()) {
        return false;
    }

    pc_leader_obj = critter_pc_leader_get(obj);
    if (pc_leader_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (object_dist(pc_leader_obj, obj) <= highres_config_get()->preload_limit) {
        return false;
    }

    if ((obj_field_int32_get(obj, OBJ_F_NPC_FLAGS) & ONF_AI_WAIT_HERE) != 0) {
        return false;
    }

    if ((obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & OCF_PARALYZED) != 0) {
        return false;
    }

    if (!critter_is_active(obj)) {
        return false;
    }

    anim_object_interrupt_by_priority(obj, 4, false, true);
    object_move_to_location(obj, obj_field_int64_get(pc_leader_obj, OBJ_F_LOCATION), 0, 0);

    return true;
}

// 0x4AD5D0
int ai_timeevent_delay(int64_t obj)
{
    return 4750 * ai_distance_to_nearest_player(obj) / highres_config_get()->preload_limit + 250;
}

// 0x4AD610
int ai_distance_to_nearest_player(int64_t obj)
{
    int64_t nearest_dist;

    nearest_dist = object_dist(player_get_local_pc_obj(), obj);

    return nearest_dist >= 0 && nearest_dist <= highres_config_get()->preload_limit
        ? (int)nearest_dist
        : highres_config_get()->preload_limit;
}

// 0x4AD6B0
bool ai_timeevent_check(TimeEvent* timeevent)
{
    return timeevent->params[0].object_value == ai_test_obj;
}

// 0x4AD6E0
void ai_schedule_default_delay(int64_t obj)
{
    ai_schedule_delay_milliseconds(obj, ai_timeevent_delay(obj));
}

// 0x4AD700
void ai_schedule_delay_milliseconds(int64_t obj, int millis)
{
    DateTime datetime;

    datetime_init(&datetime, millis);
    ai_schedule_delay(obj, &datetime);
}

// 0x4AD730
void ai_schedule_delay(int64_t obj, DateTime* datetime)
{
    TimeEvent timeevent;

    ai_timeevent_clear(obj);

    timeevent.type = TIMEEVENT_TYPE_AI;
    timeevent.params[0].object_value = obj;
    timeevent.params[1].integer_value = 0;
    timeevent_add_delay(&timeevent, datetime);
}

// 0x4AD790
void ai_schedule_immediate(int64_t obj, int event_processing_flags)
{
    TimeEvent timeevent;

    ai_timeevent_clear(obj);

    timeevent.type = TIMEEVENT_TYPE_AI;
    timeevent.params[0].object_value = obj;
    timeevent.params[1].integer_value = event_processing_flags;
    timeevent_add_immediate(&timeevent);
}

// 0x4AD7D0
void ai_timeevent_clear(int64_t obj)
{
    ai_test_obj = obj;
    timeevent_clear_one_ex(TIMEEVENT_TYPE_AI, ai_timeevent_check);
}

// 0x4AD800
int ai_can_speak(int64_t npc_obj, int64_t pc_obj, bool ignore_combat_and_animation)
{
    ObjectCritterFlags critter_flags;
    ObjectSpellFlags spell_flags;
    int danger_type;
    int64_t reaction_pc_obj;

    if ((obj_field_int32_get(npc_obj, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF)) != 0) {
        return AI_SPEAK_CANNOT;
    }

    if (obj_field_int32_get(npc_obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
        critter_flags = obj_field_int32_get(npc_obj, OBJ_F_CRITTER_FLAGS);
        if ((critter_flags & (OCF_MUTE | OCF_PARALYZED | OCF_STUNNED)) != 0) {
            return AI_SPEAK_CANNOT;
        }
        if ((critter_flags & OCF_SLEEPING) != 0) {
            return AI_SPEAK_SLEEP_UNCONSCIOUS;
        }

        if (magictech_is_under_influence_of(npc_obj, SPELL_CREATE_UNDEAD)) {
            return AI_SPEAK_CANNOT;
        }

        spell_flags = obj_field_int32_get(npc_obj, OBJ_F_SPELL_FLAGS);
        if ((spell_flags & OSF_STONED) != 0) {
            return AI_SPEAK_CANNOT;
        }

        if (critter_is_dead(npc_obj)) {
            return (spell_flags & OSF_SPOKEN_WITH_DEAD) == 0
                ? AI_SPEAK_DEAD
                : AI_SPEAK_OK;
        }

        if (critter_is_unconscious(npc_obj)) {
            return AI_SPEAK_SLEEP_UNCONSCIOUS;
        }

        if (!ignore_combat_and_animation) {
            ai_danger_source(npc_obj, &danger_type, NULL);

            if (danger_type == AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS
                || danger_type == AI_DANGER_SOURCE_TYPE_FLEE) {
                return AI_SPEAK_COMBAT;
            }

            if (!anim_object_interrupt_by_priority(npc_obj, 2, false, false)) {
                return AI_SPEAK_ANIM;
            }
        }

        reaction_pc_obj = reaction_conversation_pc_get(npc_obj);
        if (reaction_pc_obj != OBJ_HANDLE_NULL
            && reaction_pc_obj != pc_obj) {
            return AI_SPEAK_REACTION;
        }
    }

    return AI_SPEAK_OK;
}

// 0x4AD950
int ai_check_follow(int64_t npc_obj, int64_t pc_obj, bool ignore_charisma_limits)
{
    int64_t mind_controlled_by_obj;
    int64_t leader_obj;
    AiParams params;
    int npc_alignment;
    int pc_alignment;
    int pc_max_followers;

    if ((obj_field_int32_get(npc_obj, OBJ_F_SPELL_FLAGS) & OSF_MIND_CONTROLLED) != 0) {
        if (magictech_spell_flag_caster_find(npc_obj, OSF_MIND_CONTROLLED, &mind_controlled_by_obj)) {
            if (mind_controlled_by_obj == pc_obj) {
                return AI_FOLLOW_OK;
            }
        } else {
            if (critter_pc_leader_get(npc_obj) != OBJ_HANDLE_NULL) {
                return AI_FOLLOW_OK;
            }
        }
    }

    if (basic_skill_training_get(pc_obj, BASIC_SKILL_PERSUATION) >= TRAINING_MASTER) {
        return AI_FOLLOW_OK;
    }

    leader_obj = critter_leader_get(npc_obj);
    if (leader_obj != OBJ_HANDLE_NULL && leader_obj != pc_obj) {
        if (stat_level_get(pc_obj, STAT_CHARISMA) <= stat_level_get(leader_obj, STAT_CHARISMA)) {
            return AI_FOLLOW_ALREADY_IN_GROUP;
        }
    }

    if ((obj_field_int32_get(npc_obj, OBJ_F_NPC_FLAGS) & ONF_FORCED_FOLLOWER) != 0) {
        return AI_FOLLOW_OK;
    }

    ai_copy_params(npc_obj, &params);

    if (reaction_get(npc_obj, pc_obj) <= params.minimum_follow_reaction) {
        return AI_FOLLOW_DISLIKE;
    }

    npc_alignment = stat_level_get(npc_obj, STAT_ALIGNMENT);
    pc_alignment = stat_level_get(pc_obj, STAT_ALIGNMENT);
    if (pc_alignment > npc_alignment) {
        if (pc_alignment - npc_alignment > params.maximum_good_alignment_difference) {
            return AI_FOLLOW_TOO_GOOD;
        }
    } else {
        if (npc_alignment - pc_alignment > params.maximum_evil_alignment_difference) {
            return AI_FOLLOW_TOO_BAD;
        }
    }

    if (stat_level_get(npc_obj, STAT_LEVEL) >= stat_level_get(pc_obj, STAT_LEVEL) + params.maximum_follower_level_difference) {
        return AI_FOLLOW_LOW_LEVEL;
    }

    if (!ignore_charisma_limits) {
        pc_max_followers = stat_level_get(pc_obj, STAT_MAX_FOLLOWERS);
        if (pc_max_followers == 0) {
            return AI_FOLLOW_NOT_ALLOWED;
        }
        if (critter_num_followers(pc_obj, true) >= pc_max_followers) {
            return AI_FOLLOW_LIMIT_REACHED;
        }
    }

    return AI_FOLLOW_OK;
}

// 0x4ADB50
int ai_check_leader(int64_t npc_obj, int64_t pc_obj)
{
    unsigned int critter_flags2;
    int64_t mind_controlled_by_obj;
    AiParams params;
    int npc_alignment;
    int pc_alignment;

    critter_flags2 = obj_field_int32_get(npc_obj, OBJ_F_CRITTER_FLAGS2);
    obj_field_int32_set(npc_obj, OBJ_F_CRITTER_FLAGS2, critter_flags2 & ~(OCF2_CHECK_ALIGN_BAD | OCF2_CHECK_ALIGN_GOOD | OCF2_CHECK_REACTION_BAD));

    if ((obj_field_int32_get(npc_obj, OBJ_F_SPELL_FLAGS) & OSF_MIND_CONTROLLED) != 0) {
        if (magictech_spell_flag_caster_find(npc_obj, OSF_MIND_CONTROLLED, &mind_controlled_by_obj)) {
            if (mind_controlled_by_obj == pc_obj) {
                return AI_FOLLOW_OK;
            }
        } else {
            if (critter_pc_leader_get(npc_obj) != OBJ_HANDLE_NULL) {
                return AI_FOLLOW_OK;
            }
        }
    }

    if (basic_skill_training_get(pc_obj, BASIC_SKILL_PERSUATION) >= TRAINING_MASTER) {
        return AI_FOLLOW_OK;
    }

    if ((obj_field_int32_get(npc_obj, OBJ_F_NPC_FLAGS) & ONF_FORCED_FOLLOWER) != 0) {
        return AI_FOLLOW_OK;
    }

    ai_copy_params(npc_obj, &params);

    if ((critter_flags2 & OCF2_CHECK_REACTION_BAD) != 0
        && reaction_get(npc_obj, pc_obj) <= params.minimum_follow_reaction + 20) {
        return AI_FOLLOW_DISLIKE;
    }

    npc_alignment = stat_level_get(npc_obj, STAT_ALIGNMENT);
    pc_alignment = stat_level_get(pc_obj, STAT_ALIGNMENT);
    if (pc_alignment > npc_alignment) {
        if ((critter_flags2 & OCF2_CHECK_ALIGN_GOOD) != 0
            && pc_alignment - npc_alignment > params.maximum_good_alignment_difference - 100) {
            return AI_FOLLOW_TOO_GOOD;
        }
    } else {
        if ((critter_flags2 & OCF2_CHECK_ALIGN_BAD) != 0
            && npc_alignment - pc_alignment > params.maximum_evil_alignment_difference - 100) {
            return AI_FOLLOW_TOO_BAD;
        }
    }

    return AI_FOLLOW_OK;
}

// 0x4ADCC0
int ai_check_upset_attacking(int64_t source_obj, int64_t target_obj, int64_t leader_obj)
{
    int64_t mind_controlled_by_obj;
    AiParams params;

    if ((obj_field_int32_get(source_obj, OBJ_F_SPELL_FLAGS) & OSF_MIND_CONTROLLED) != 0) {
        if (magictech_spell_flag_caster_find(source_obj, OSF_MIND_CONTROLLED, &mind_controlled_by_obj)) {
            if (mind_controlled_by_obj == leader_obj) {
                return AI_UPSET_ATTACKING_NONE;
            }
        } else {
            if (critter_pc_leader_get(source_obj) != OBJ_HANDLE_NULL) {
                return AI_UPSET_ATTACKING_NONE;
            }
        }
    }

    if (!obj_type_is_critter(obj_field_int32_get(target_obj, OBJ_F_TYPE))) {
        return AI_UPSET_ATTACKING_NONE;
    }

    if (critter_leader_get(target_obj) == leader_obj) {
        return AI_UPSET_ATTACKING_PARTY;
    }

    if (leader_obj != OBJ_HANDLE_NULL) {
        if (stat_level_get(source_obj, STAT_ALIGNMENT) > 0) {
            ai_copy_params(source_obj, &params);
            if (stat_level_get(target_obj, STAT_ALIGNMENT) >= params.protected_good_alignment_threshold) {
                return AI_UPSET_ATTACKING_GOOD;
            }
        }
    }

    if (critter_origin_same(source_obj, target_obj)) {
        return AI_UPSET_ATTACKING_ORIGIN;
    }

    if (critter_faction_same(source_obj, target_obj)) {
        return AI_UPSET_ATTACKING_FACTION;
    }

    return AI_UPSET_ATTACKING_NONE;
}

// 0x4ADE00
int ai_straight_path_cost_to_location(int64_t source_obj, int64_t target_loc, int64_t* block_obj_ptr)
{
    int64_t source_loc;
    int64_t source_loc_x;
    int64_t source_loc_y;
    int cnt;
    int8_t offsets[200];
    unsigned int flags;
    int offset_x;
    int offset_y;
    int idx;
    int64_t new_loc;
    int rot;
    int64_t block_obj;
    int block_obj_type;
    int cost = 0;

    if (block_obj_ptr != NULL) {
        *block_obj_ptr = OBJ_HANDLE_NULL;
    }

    source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
    if (source_loc == target_loc) {
        return 0;
    }

    cnt = path_make_straight(source_loc, target_loc, offsets);
    if (cnt == 0) {
        return 100;
    }

    flags = OBJ_TRAVERSAL_IGNORE_CRITTERS | OBJ_TRAVERSAL_PROJECTILE;
    offset_x = obj_field_int32_get(source_obj, OBJ_F_OFFSET_X);
    offset_y = obj_field_int32_get(source_obj, OBJ_F_OFFSET_Y);
    location_xy(source_loc, &source_loc_x, &source_loc_y);

    for (idx = 0; idx < cnt; idx += 2) {
        if (source_loc == target_loc) {
            break;
        }

        offset_x += offsets[idx];
        offset_y += offsets[idx + 1];
        location_at(source_loc_x + offset_x + 40, source_loc_y + offset_y + 20, &new_loc);
        if (new_loc != source_loc) {
            rot = location_rot(source_loc, new_loc);
            if (new_loc == target_loc) {
                flags |= OBJ_TRAVERSAL_SKIP_OBJECTS;
            }

            cost += object_calc_traversal_cost(OBJ_HANDLE_NULL, source_loc, rot, flags, &block_obj, &block_obj_type, NULL);

            if (block_obj != OBJ_HANDLE_NULL
                && block_obj_ptr != NULL) {
                *block_obj_ptr = block_obj;
                return cost;
            }

            source_loc = new_loc;
        }
    }

    return cost;
}

// 0x4ADFF0
void ai_switch_weapon(int64_t obj)
{
    ObjectNpcFlags npc_flags;

    npc_flags = obj_field_int32_get(obj, OBJ_F_NPC_FLAGS);
    npc_flags |= ONF_CHECK_WEAPON;
    npc_flags |= ONF_LOOK_FOR_WEAPON;
    npc_flags |= ONF_LOOK_FOR_AMMO;
    obj_field_int32_set(obj, OBJ_F_NPC_FLAGS, npc_flags);
}

// 0x4AE020
void ai_calc_party_size_and_level(int64_t obj, int* cnt_ptr, int* lvl_ptr)
{
    int64_t leader_obj;

    if (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_PC) {
        ai_calc_party_size_and_level_internal(obj, cnt_ptr, lvl_ptr);
        return;
    }

    leader_obj = critter_leader_get(obj);
    if (leader_obj != OBJ_HANDLE_NULL) {
        ai_calc_party_size_and_level_internal(leader_obj, cnt_ptr, lvl_ptr);
        return;
    }

    *cnt_ptr = 1;
    *lvl_ptr = stat_level_get(obj, STAT_LEVEL);
}

// 0x4AE0A0
void ai_calc_party_size_and_level_internal(int64_t obj, int* cnt_ptr, int* lvl_ptr)
{
    ObjectList objects;
    ObjectNode* node;

    *cnt_ptr = 1;
    *lvl_ptr = stat_level_get(obj, STAT_LEVEL);

    object_list_all_followers(obj, &objects);
    node = objects.head;
    while (node != NULL) {
        *cnt_ptr += 1;
        *lvl_ptr += stat_level_get(obj, STAT_LEVEL);
        node = node->next;
    }

    object_list_destroy(&objects);
}

// 0x4AE120
int ai_check_kos(int64_t source_obj, int64_t target_obj)
{
    int64_t pc_leader_obj;
    int obj_type;
    int danger_source_type;
    int64_t danger_source_obj;
    unsigned int npc_flags;
    unsigned int critter_flags;
    AiParams ai_params;

    if (ai_combat_target_is_valid(source_obj, target_obj)) {
        pc_leader_obj = critter_pc_leader_get(source_obj);
        obj_type = obj_field_int32_get(target_obj, OBJ_F_TYPE);

        if (object_script_execute(target_obj, source_obj, OBJ_HANDLE_NULL, SAP_WILL_KOS, 0) != 0) {
            ai_danger_source(source_obj, &danger_source_type, &danger_source_obj);

            if (danger_source_type != AI_DANGER_SOURCE_TYPE_SURRENDER || danger_source_obj != target_obj) {
                if (pc_leader_obj == OBJ_HANDLE_NULL) {
                    npc_flags = obj_field_int32_get(source_obj, OBJ_F_NPC_FLAGS);
                    if ((npc_flags & ONF_KOS) != 0) {
                        critter_flags = obj_field_int32_get(source_obj, OBJ_F_CRITTER_FLAGS);
                        if ((obj_field_int32_get(target_obj, OBJ_F_SPELL_FLAGS) & OSF_ENSHROUDED) != 0
                            && (critter_flags & OCF_UNDEAD) != 0) {
                            return AI_KOS_NO;
                        }

                        if ((obj_field_int32_get(target_obj, OBJ_F_CRITTER_FLAGS) & OCF_ANIMAL_ENSHROUD) != 0
                            && (critter_flags & OCF_ANIMAL) != 0) {
                            return AI_KOS_NO;
                        }

                        if ((npc_flags & ONF_KOS_OVERRIDE) == 0
                            && !critter_faction_same(source_obj, target_obj)) {
                            return AI_KOS_FACTION;
                        }
                    }

                    ai_copy_params(source_obj, &ai_params);

                    if (obj_type == OBJ_TYPE_PC
                        && reaction_get_with_limited_penalty(source_obj, target_obj) <= ai_params.attack_reaction_threshold) {
                        return AI_KOS_REACTION;
                    }

                    if (abs(stat_level_get(source_obj, STAT_ALIGNMENT) - stat_level_get(target_obj, STAT_ALIGNMENT)) >= ai_params.attack_alignment_difference) {
                        return AI_KOS_ALIGNMENT;
                    }

                    if (ai_check_decoy(source_obj, target_obj)) {
                        return AI_KOS_DECOY;
                    }
                }

                if (obj_type == OBJ_TYPE_NPC) {
                    ai_danger_source(target_obj, &danger_source_type, &danger_source_obj);

                    if (danger_source_type == AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS
                        && danger_source_obj != OBJ_HANDLE_NULL
                        && (ai_check_protect(source_obj, danger_source_obj) != AI_PROTECT_NO
                            || (critter_social_class_get(source_obj) == SOCIAL_CLASS_GUARD
                                && critter_is_monstrous(target_obj)
                                && critter_leader_get(target_obj) == OBJ_HANDLE_NULL))
                        && ai_check_upset_attacking(source_obj, target_obj, critter_leader_get(source_obj)) == AI_UPSET_ATTACKING_NONE) {
                        return AI_KOS_GUARD;
                    }
                }
            }
        }
    }

    return AI_KOS_NO;
}

// 0x4AE3A0
int ai_check_protect(int64_t source_obj, int64_t target_obj)
{
    int64_t leader_obj;

    if (source_obj == target_obj) {
        return AI_PROTECT_SELF;
    }

    leader_obj = critter_leader_get(source_obj);
    if (leader_obj != OBJ_HANDLE_NULL && leader_obj == target_obj) {
        return AI_PROTECT_GROUP;
    }

    if ((obj_field_int32_get(source_obj, OBJ_F_SPELL_FLAGS) & OSF_MIND_CONTROLLED) == 0) {
        if (critter_faction_same(source_obj, target_obj)) {
            return AI_PROTECT_FACTION;
        }

        if (critter_social_class_get(source_obj) == SOCIAL_CLASS_GUARD
            && critter_origin_same(source_obj, target_obj)) {
            return AI_PROTECT_ORIGIN;
        }
    }

    return AI_PROTECT_NO;
}

// 0x4AE450
int64_t ai_dead_ally_attacker_find(int64_t observer_obj, int64_t ally_obj)
{
    int64_t combat_focus_obj;

    if (critter_is_dead(ally_obj)
        && obj_field_int32_get(ally_obj, OBJ_F_TYPE) == OBJ_TYPE_NPC
        && obj_field_obj_get(ally_obj, OBJ_F_NPC_COMBAT_FOCUS, &combat_focus_obj)
        && combat_focus_obj != OBJ_HANDLE_NULL
        && ai_combat_target_is_valid(observer_obj, combat_focus_obj)
        && ai_check_protect(observer_obj, ally_obj) != AI_PROTECT_NO) {
        return combat_focus_obj;
    }

    return OBJ_HANDLE_NULL;
}

// 0x4AE4E0
void ai_objects_in_radius(int64_t obj, int radius, ObjectList* objects, unsigned int flags)
{
    int64_t loc;
    LocRect loc_rect;

    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    loc_rect.x1 = LOCATION_GET_X(loc) - radius;
    loc_rect.y1 = LOCATION_GET_Y(loc) - radius;
    loc_rect.x2 = LOCATION_GET_X(loc) + radius;
    loc_rect.y2 = LOCATION_GET_Y(loc) + radius;
    object_list_rect(&loc_rect, flags, objects);
}

// 0x4AE570
int ai_check_use_skill(int64_t source_obj, int64_t target_obj, int64_t item_obj, int skill)
{
    switch (skill) {
    case SKILL_HEAL:
        if (item_obj == OBJ_HANDLE_NULL
            || obj_field_int32_get(item_obj, OBJ_F_TYPE) != OBJ_TYPE_GENERIC
            || (obj_field_int32_get(item_obj, OBJ_F_GENERIC_FLAGS) & OGF_IS_HEALING_ITEM) == 0) {
            return AI_USE_SKILL_BAD_SOURCE;
        }

        if (target_obj == OBJ_HANDLE_NULL
            || !obj_type_is_critter(obj_field_int32_get(target_obj, OBJ_F_TYPE))
            || (obj_field_int32_get(target_obj, OBJ_F_CRITTER_FLAGS) & (OCF_UNDEAD | OCF_MECHANICAL)) != 0) {
            return AI_USE_SKILL_BAD_TARGET;
        }

        return AI_USE_SKILL_OK;
    case SKILL_REPAIR:
        if (target_obj == OBJ_HANDLE_NULL
            || !obj_type_is_item(obj_field_int32_get(target_obj, OBJ_F_TYPE))) {
            return AI_USE_SKILL_BAD_TARGET;
        }

        return AI_USE_SKILL_OK;
    case SKILL_DISARM_TRAPS:
        if (item_obj != OBJ_HANDLE_NULL) {
            if (obj_field_int32_get(item_obj, OBJ_F_TYPE) != OBJ_TYPE_GENERIC
                || (obj_field_int32_get(item_obj, OBJ_F_GENERIC_FLAGS) & OGF_IS_LOCKPICK) == 0) {
                return AI_USE_SKILL_BAD_SOURCE;
            }
        } else {
            if (source_obj == OBJ_HANDLE_NULL
                || critter_pc_leader_get(source_obj) == OBJ_HANDLE_NULL) {
                return AI_USE_SKILL_BAD_SOURCE;
            }
        }

        if (target_obj == OBJ_HANDLE_NULL
            || !object_is_lockable(target_obj)) {
            return AI_USE_SKILL_BAD_TARGET;
        }

        return AI_USE_SKILL_OK;
    }

    return AI_USE_SKILL_OK;
}

// 0x4AE720
int ai_magictech_action_check(int64_t attacker_obj, int64_t item_obj, int64_t target_obj, int spell)
{
    int attacker_obj_type = obj_field_int32_get(attacker_obj, OBJ_F_TYPE);

    if (item_obj != OBJ_HANDLE_NULL) {
        if (attacker_obj_type == OBJ_TYPE_NPC
            && magictech_has_maintenance(spell)
            && !magictech_maintenance_slots_available(attacker_obj)) {
            return 6;
        }

        if (obj_field_int32_get(item_obj, OBJ_F_ITEM_SPELL_MANA_STORE) == 0) {
            return 2;
        }

        if (target_obj != OBJ_HANDLE_NULL
            && (COLLEGE_FROM_SPELL(spell) != COLLEGE_NECROMANTIC_WHITE
                || (ai_object_hp_ratio(target_obj) > 30
                    && ai_critter_fatigue_ratio(attacker_obj) < 80))
            && magictech_use_item_fail_chance(attacker_obj, item_obj, target_obj) >= 38) {
            return 8;
        }
    } else {
        if (!spell_is_known(attacker_obj, spell)) {
            return 1;
        }

        if (attacker_obj_type == OBJ_TYPE_NPC) {
            if (critter_pc_leader_get(attacker_obj) == OBJ_HANDLE_NULL
                && ai_critter_fatigue_ratio(attacker_obj) < 20) {
                return 2;
            }

            if (magictech_has_maintenance(spell) && !magictech_maintenance_slots_available(attacker_obj)) {
                return 6;
            }
        }

        if (spell_cast_cost(spell, attacker_obj) >= critter_fatigue_current(attacker_obj)) {
            return 2;
        }

        if (spell_min_intelligence(spell) > stat_level_get(attacker_obj, STAT_INTELLIGENCE)) {
            return 3;
        }

        if (spell_min_willpower(spell) > stat_level_get(attacker_obj, STAT_WILLPOWER)) {
            return 7;
        }

        if ((obj_field_int32_get(attacker_obj, OBJ_F_NPC_FLAGS) & ONF_CAST_HIGHEST) != 0
            && spell_college_level_get(attacker_obj, COLLEGE_FROM_SPELL(spell)) + 5 * COLLEGE_FROM_SPELL(spell) - 1 != spell) {
            return 4;
        }

        if (target_obj != OBJ_HANDLE_NULL
            && (COLLEGE_FROM_SPELL(spell) != COLLEGE_NECROMANTIC_WHITE
                || ai_object_hp_ratio(target_obj) > 30)
            && magictech_cast_spell_fail_chance(attacker_obj, target_obj, spell) >= 38) {
            return 8;
        }
    }

    if (attacker_obj != target_obj && target_obj != OBJ_HANDLE_NULL) {
        int64_t block_obj;
        int64_t target_obj_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);

        if (ai_straight_path_cost_to_location(attacker_obj, target_obj_loc, &block_obj) >= 100
            || (block_obj != OBJ_HANDLE_NULL && block_obj != target_obj)) {
            return 5;
        }
    }

    return 0;
}

// 0x4AE9E0
void ai_npc_witness_pc_critical(int64_t pc_obj, int type)
{
    int cnt;
    int rnd;
    int64_t follower_obj;
    char str[1000];
    int speech_id;

    if (random_between(1, 2) == 1) {
        return;
    }

    if (ai_float_line_func == NULL) {
        return;
    }

    cnt = critter_num_followers(pc_obj, false);
    if (cnt == 0) {
        return;
    }

    rnd = cnt > 1 ? random_between(0, cnt - 1) : 0;
    follower_obj = obj_arrayfield_handle_get(pc_obj, OBJ_F_CRITTER_FOLLOWER_IDX, rnd);
    if (!critter_is_active(follower_obj)) {
        return;
    }

    dialog_copy_npc_witness_pc_critical_msg(follower_obj, pc_obj, type, str, &speech_id);
    ai_float_line_func(follower_obj, pc_obj, str, speech_id);
}

// 0x4AEAB0
void ai_npc_near_death(int64_t npc_obj, int64_t pc_obj)
{
    char str[1000];
    int speech_id;

    dialog_copy_npc_near_death_msg(npc_obj, pc_obj, str, &speech_id);
    ai_float_line_func(npc_obj, pc_obj, str, speech_id);
}

// 0x4AEB10
bool ai_critter_can_open_portals(int64_t obj)
{
    AiParams params;

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    switch (obj_field_int32_get(obj, OBJ_F_TYPE)) {
    case OBJ_TYPE_PC:
        return true;
    case OBJ_TYPE_NPC:
        ai_copy_params(obj, &params);
        return params.can_open_portals;
    }

    return false;
}

// 0x4AEB70
int ai_attempt_open_portal(int64_t obj, int64_t portal_obj, int dir)
{
    unsigned int portal_flags;
    int key_id;

    if (object_is_busted(portal_obj)) {
        return AI_ATTEMPT_OPEN_PORTAL_OK;
    }

    if (!obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))) {
        return AI_ATTEMPT_OPEN_PORTAL_NOT_CRITTER;
    }

    if (!object_script_execute(obj, portal_obj, OBJ_HANDLE_NULL, SAP_UNLOCK, 0)) {
        return AI_ATTEMPT_OPEN_PORTAL_NOT_ALLOWED;
    }

    if (obj_field_int32_get(portal_obj, OBJ_F_TYPE) == OBJ_TYPE_PORTAL) {
        portal_flags = obj_field_int32_get(portal_obj, OBJ_F_PORTAL_FLAGS);
        if ((portal_flags & OPF_JAMMED) != 0) {
            return AI_ATTEMPT_OPEN_PORTAL_JAMMED;
        }
        if ((portal_flags & OPF_MAGICALLY_HELD) != 0) {
            return AI_ATTEMPT_OPEN_PORTAL_MAGICALLY_HELD;
        }
        if ((portal_flags & OPF_ALWAYS_LOCKED) == 0) {
            if (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
                if (critter_pc_leader_get(obj) == OBJ_HANDLE_NULL
                    || ai_is_indoor_to_outdoor_transition(portal_obj, dir)) {
                    return AI_ATTEMPT_OPEN_PORTAL_OK;
                }
            } else if (ai_is_indoor_to_outdoor_transition(portal_obj, dir)) {
                return AI_ATTEMPT_OPEN_PORTAL_OK;
            }
        }

        if (object_locked_get(portal_obj)) {
            key_id = obj_field_int32_get(portal_obj, OBJ_F_PORTAL_KEY_ID);
            if (!item_group_has_matching_key(obj, key_id)) {
                return AI_ATTEMPT_OPEN_PORTAL_LOCKED;
            }
        }
    }

    return AI_ATTEMPT_OPEN_PORTAL_OK;
}

// 0x4AECA0
bool ai_is_indoor_to_outdoor_transition(int64_t portal_obj, int dir)
{
    int64_t loc;
    tig_art_id_t aid;
    int rot;
    int64_t new_loc;
    int64_t tmp_loc;

    loc = obj_field_int64_get(portal_obj, OBJ_F_LOCATION);
    aid = obj_field_int32_get(portal_obj, OBJ_F_CURRENT_AID);
    rot = tig_art_id_rotation_get(aid);
    if (!location_in_dir(loc, rot, &new_loc)) {
        return false;
    }

    if ((dir + 4) % 8 == rot
        || (dir + 5) % 8 == rot
        || (dir + 3) % 8 == rot) {
        tmp_loc = new_loc;
        loc = new_loc;
        new_loc = tmp_loc;
    }

    aid = tile_art_id_at(loc);
    if (tig_art_tile_id_type_get(aid) != TIG_ART_TILE_TYPE_INDOOR) {
        return false;
    }

    aid = tile_art_id_at(new_loc);
    if (tig_art_tile_id_type_get(aid) == TIG_ART_TILE_TYPE_INDOOR) {
        return false;
    }

    return true;
}

// 0x4AED80
int ai_attempt_open_container(int64_t obj, int64_t container_obj)
{
    unsigned int container_flags;
    int key_id;

    if (proto_number_from_instance(container_obj) == BP_JUNK_PILE) {
        return AI_ATTEMPT_OPEN_CONTAINER_OK;
    }

    if (object_is_busted(container_obj)) {
        return AI_ATTEMPT_OPEN_CONTAINER_OK;
    }

    if (!object_script_execute(obj, container_obj, OBJ_HANDLE_NULL, SAP_UNLOCK, 0)) {
        return AI_ATTEMPT_OPEN_CONTAINER_NOT_ALLOWED;
    }

    if (obj_field_int32_get(container_obj, OBJ_F_TYPE) == OBJ_TYPE_CONTAINER) {
        container_flags = obj_field_int32_get(container_obj, OBJ_F_CONTAINER_FLAGS);
        if ((container_flags & OCOF_JAMMED) != 0) {
            return AI_ATTEMPT_OPEN_CONTAINER_JAMMED;
        }
        if ((container_flags & OCOF_MAGICALLY_HELD) != 0) {
            return AI_ATTEMPT_OPEN_CONTAINER_MAGICALLY_HELD;
        }

        if (object_locked_get(container_obj)) {
            key_id = obj_field_int32_get(container_obj, OBJ_F_CONTAINER_KEY_ID);
            if (!item_group_has_matching_key(obj, key_id)) {
                return AI_ATTEMPT_OPEN_CONTAINER_LOCKED;
            }
        }
    }

    return AI_ATTEMPT_OPEN_CONTAINER_OK;
}

// 0x4AEE50
void ai_notify_portal_container_guards(int64_t critter_obj, int64_t target_obj, bool apply_reaction_penalty, int loudness)
{
    int64_t pc_obj;
    int notify_npc;
    ObjectList objects;
    ObjectNode* node;
    AiParams ai_params;
    char str[1000];
    int speech_id;

    if (critter_obj == OBJ_HANDLE_NULL) {
        return;
    }

    switch (obj_field_int32_get(critter_obj, OBJ_F_TYPE)) {
    case OBJ_TYPE_PC:
        pc_obj = critter_obj;
        break;
    case OBJ_TYPE_NPC:
        pc_obj = critter_pc_leader_get(critter_obj);
        break;
    default:
        pc_obj = OBJ_HANDLE_NULL;
        break;
    }

    if (pc_obj == OBJ_HANDLE_NULL) {
        return;
    }

    switch (obj_field_int32_get(target_obj, OBJ_F_TYPE)) {
    case OBJ_TYPE_PORTAL:
        notify_npc = obj_field_int32_get(target_obj, OBJ_F_PORTAL_NOTIFY_NPC);
        break;
    case OBJ_TYPE_CONTAINER:
        notify_npc = obj_field_int32_get(target_obj, OBJ_F_CONTAINER_NOTIFY_NPC);
        break;
    case OBJ_TYPE_SCENERY:
        notify_npc = -1;
        break;
    default:
        notify_npc = 0;
        break;
    }

    if (notify_npc == 0) {
        return;
    }

    ai_objects_in_radius(target_obj, 10, &objects, OBJ_TM_NPC);
    node = objects.head;
    while (node != NULL) {
        if ((obj_field_int32_get(node->obj, OBJ_F_NAME) == notify_npc
                || critter_social_class_get(node->obj) == SOCIAL_CLASS_GUARD)
            && !critter_is_dead(node->obj)
            && (obj_field_int32_get(node->obj, OBJ_F_SPELL_FLAGS) & OSF_MIND_CONTROLLED) == 0
            && (ai_can_see(node->obj, critter_obj) == 0
                || ai_can_hear(node->obj, critter_obj, loudness) == 0)) {
            if (object_script_execute(critter_obj, node->obj, target_obj, SAP_CATCHING_THIEF_PC, 0) == 1) {
                if (apply_reaction_penalty && !critter_is_sleeping(node->obj)) {
                    reaction_adj(node->obj, pc_obj, -20);
                    ai_copy_params(node->obj, &ai_params);
                    if (reaction_get(node->obj, pc_obj) <= ai_params.attack_reaction_threshold) {
                        ai_attack(critter_obj, node->obj, loudness, 0);
                    }
                    if (critter_is_active(node->obj)) {
                        if (ai_float_line_func != NULL && critter_is_active(node->obj)) {
                            dialog_copy_npc_warning_msg(node->obj, critter_obj, str, &speech_id);
                            ai_float_line_func(node->obj, critter_obj, str, speech_id);
                        }
                    }
                } else {
                    ai_attack(critter_obj, node->obj, loudness, 0);
                }
            }
        }
        node = node->next;
    }
    object_list_destroy(&objects);
}

// 0x4AF130
void ai_flee(int64_t obj, int64_t danger_obj)
{
    if (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
        ai_danger_state_set(obj, AI_DANGER_SOURCE_TYPE_FLEE, danger_obj, 0);
    }
}

// 0x4AF170
void ai_stop_fleeing(int64_t obj)
{
    int danger_type;

    if (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
        ai_danger_source(obj, &danger_type, NULL);
        if (danger_type == AI_DANGER_SOURCE_TYPE_FLEE) {
            ai_danger_state_set(obj, AI_DANGER_SOURCE_TYPE_NONE, OBJ_HANDLE_NULL, 0);
        }
        anim_interrupt_all_goals_of_type(obj, AG_FLEE, -1);
    }
}

// 0x4AF1D0
void ai_set_no_flee(int64_t obj)
{
    ObjectCritterFlags critter_flags;

    ai_stop_fleeing(obj);

    critter_flags = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS);
    critter_flags |= OCF_NO_FLEE;
    obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS, critter_flags);
}

// 0x4AF210
bool ai_surrendered(int64_t obj, int64_t* danger_source_ptr)
{
    int danger_type;

    ai_danger_source(obj, &danger_type, danger_source_ptr);

    return danger_type == AI_DANGER_SOURCE_TYPE_SURRENDER;
}

// 0x4AF240
int ai_perception_distance(int value)
{
    if (value < 1) {
        value = 1;
    } else if (value > 10) {
        value = 10;
    }
    return value;
}

// 0x4AF260
int ai_can_see(int64_t source_obj, int64_t target_obj)
{
    int perception;
    int64_t dist;
    int extra_dist;
    int64_t target_loc;
    int64_t block_obj;

    if ((obj_field_int32_get(source_obj, OBJ_F_CRITTER_FLAGS) & (OCF_SLEEPING | OCF_BLINDED | OCF_STUNNED)) != 0) {
        return 1000;
    }

    if (critter_is_unconscious(source_obj)) {
        return 1000;
    }

    if (((obj_field_int32_get(target_obj, OBJ_F_FLAGS) & OF_INVISIBLE) != 0
            || (obj_field_int32_get(target_obj, OBJ_F_SPELL_FLAGS) & OSF_INVISIBLE) != 0)
        && !stat_is_extraordinary(source_obj, STAT_PERCEPTION)
        && (obj_field_int32_get(source_obj, OBJ_F_SPELL_FLAGS) & OSF_DETECTING_INVISIBLE) == 0) {
        return 1000;
    }

    if (!critter_is_facing_to(source_obj, target_obj)) {
        return 1000;
    }

    perception = stat_level_get(source_obj, STAT_PERCEPTION);

    if (critter_is_concealed(target_obj)) {
        SkillInvocation skill_invocation;
        int prowling;
        int diff;

        prowling = basic_skill_effectiveness(target_obj, BASIC_SKILL_PROWLING, source_obj);
        skill_invocation_init(&skill_invocation);
        object_handle_reference_init(target_obj, &(skill_invocation.source));
        object_handle_reference_init(source_obj, &(skill_invocation.target));
        skill_invocation.flags |= SKILL_INVOCATION_CHECK_SEEING;
        skill_invocation.skill = BASIC_SKILL_PROWLING;

        diff = prowling - skill_invocation_difficulty(&skill_invocation);
        if (diff < 0) {
            diff = 0;
        } else if (diff > 100) {
            diff = 100;
        }

        perception += perception * diff / -100;
    }

    perception = ai_perception_distance(perception);

    dist = object_dist(source_obj, target_obj);
    if (dist > 1000) {
        return 1000;
    }

    extra_dist = (int)dist - perception;
    if (extra_dist < 0) {
        extra_dist = 0;
    }

    target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    ai_straight_path_cost_to_location(source_obj, target_loc, &block_obj);
    if (block_obj != OBJ_HANDLE_NULL) {
        extra_dist++;
    }

    return extra_dist;
}

// 0x4AF470
int ai_can_hear(int64_t source_obj, int64_t target_obj, int loudness)
{
    unsigned int critter_flags;
    int64_t dist;
    int perception;
    int hear_dist;

    critter_flags = obj_field_int32_get(source_obj, OBJ_F_CRITTER_FLAGS);
    if ((critter_flags & OCF_STUNNED) != 0) {
        return 1000;
    }

    if (critter_is_unconscious(source_obj)) {
        return 1000;
    }

    dist = object_dist(source_obj, target_obj);
    if (dist > 1000) {
        return 1000;
    }

    perception = stat_level_get(source_obj, STAT_PERCEPTION);
    if ((critter_flags & OCF_SLEEPING) != 0) {
        perception /= 2;
    }

    if ((obj_field_int32_get(target_obj, OBJ_F_CRITTER_FLAGS) & OCF_MOVING_SILENTLY) != 0) {
        SkillInvocation skill_invocation;
        int prowling;
        int diff;

        prowling = basic_skill_effectiveness(target_obj, BASIC_SKILL_PROWLING, source_obj);
        skill_invocation_init(&skill_invocation);
        object_handle_reference_init(target_obj, &(skill_invocation.source));
        object_handle_reference_init(source_obj, &(skill_invocation.target));
        skill_invocation.flags |= SKILL_INVOCATION_CHECK_HEARING;
        skill_invocation.skill = BASIC_SKILL_PROWLING;

        diff = prowling - skill_invocation_difficulty(&skill_invocation);
        if (diff < 0) {
            diff = 0;
        } else if (diff > 100) {
            diff = 100;
        }

        perception += perception * diff / -100;
    }

    hear_dist = (ai_sound_radius_by_loudness[loudness] - ai_sound_radius_by_loudness[LOUDNESS_SILENT] + ai_perception_distance(perception - 4)) / 2 - ai_straight_path_cost_to_object(source_obj, target_obj);
    if ((int)dist > hear_dist) {
        return (int)dist - hear_dist;
    }

    return 0;
}

// 0x4AF640
int ai_straight_path_cost_to_object(int64_t source_obj, int64_t target_obj)
{
    int cost = 0;
    int64_t source_loc;
    int64_t target_loc;
    int cnt;
    int8_t offsets[200];
    unsigned int flags;
    int offset_x;
    int offset_y;
    int64_t source_loc_x;
    int64_t source_loc_y;
    int idx;
    int64_t new_loc;
    int rot;
    int64_t block_obj;
    int block_obj_type;

    source_loc = obj_field_int64_get(source_obj, OBJ_F_LOCATION);
    target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    if (source_loc == target_loc) {
        return 0;
    }

    cnt = path_make_straight(source_loc, target_loc, offsets);
    if (cnt == 0) {
        return 100;
    }

    flags = OBJ_TRAVERSAL_SOUND;

    offset_x = obj_field_int32_get(source_obj, OBJ_F_OFFSET_X);
    offset_y = obj_field_int32_get(source_obj, OBJ_F_OFFSET_Y);
    location_xy(source_loc, &source_loc_x, &source_loc_y);

    for (idx = 0; idx < cnt; idx += 2) {
        if (source_loc == target_loc) {
            break;
        }

        offset_x += offsets[idx];
        offset_y += offsets[idx + 1];
        location_at(source_loc_x + offset_x + 40, source_loc_y + offset_y + 20, &new_loc);
        if (new_loc != source_loc) {
            rot = location_rot(source_loc, new_loc);
            if (new_loc == target_loc) {
                flags |= OBJ_TRAVERSAL_SKIP_OBJECTS;
            }

            cost += object_calc_traversal_cost(OBJ_HANDLE_NULL,
                source_loc,
                rot,
                flags,
                &block_obj,
                &block_obj_type,
                NULL);
            source_loc = new_loc;
        }
    }

    return cost;
}

// 0x4AF800
bool ai_check_decoy(int64_t source_obj, int64_t target_obj)
{
    return !critter_is_dead(target_obj)
        && stat_level_get(source_obj, STAT_INTELLIGENCE) < 5
        && obj_field_int32_get(target_obj, OBJ_F_NAME) == CLOCKWORK_DECOY;
}

// 0x4AF860
void ai_npc_fighting_toggle(void)
{
    UiMessage ui_message;

    ai_npc_fighting_enabled = !ai_npc_fighting_enabled;

    ui_message.type = UI_MSG_TYPE_EXCLAMATION;
    if (ai_npc_fighting_enabled) {
        ui_message.str = "NPC fighting is ON";
    } else {
        ui_message.str = "NPC fighting is OFF";
    }
    ui_display_msg(&ui_message);
}

// 0x4AF8C0
void ai_add_team_to_hostility_list(int64_t source_obj, int64_t target_obj)
{
    int64_t team_leader;
    ObjectList objects;
    ObjectNode* node;

    team_leader = critter_pc_leader_get(target_obj);
    if (team_leader == OBJ_HANDLE_NULL) {
        team_leader = target_obj;
    }

    object_list_team(team_leader, &objects);

    node = objects.head;
    while (node != NULL) {
        ai_shitlist_add(source_obj, node->obj);
        node = node->next;
    }

    object_list_destroy(&objects);
}

// 0x4AF930
void ai_shitlist_add(int64_t npc_obj, int64_t shit_obj)
{
    int idx;
    int64_t obj;

    if (obj_field_int32_get(shit_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        obj = critter_pc_leader_get(shit_obj);
    } else {
        obj = shit_obj;
    }

    if ((obj == OBJ_HANDLE_NULL || obj != critter_pc_leader_get(npc_obj))
        && !ai_shitlist_has(npc_obj, shit_obj)) {
        idx = obj_arrayfield_length_get(npc_obj, OBJ_F_NPC_SHIT_LIST_IDX);
        obj_arrayfield_obj_set(npc_obj, OBJ_F_NPC_SHIT_LIST_IDX, idx, shit_obj);
        combat_recalc_reaction(npc_obj);
    }
}

// 0x4AF9D0
void ai_shitlist_remove(int64_t npc_obj, int64_t shit_obj)
{
    int cnt;
    int idx;
    int64_t obj;

    cnt = obj_arrayfield_length_get(npc_obj, OBJ_F_NPC_SHIT_LIST_IDX);
    for (idx = 0; idx < cnt; idx++) {
        obj_arrayfield_obj_get(npc_obj, OBJ_F_NPC_SHIT_LIST_IDX, idx, &obj);
        if (obj == shit_obj || obj == OBJ_HANDLE_NULL) {
            if (idx < cnt - 1) {
                obj_arrayfield_obj_get(npc_obj, OBJ_F_NPC_SHIT_LIST_IDX, cnt - 1, &obj);
                obj_arrayfield_obj_set(npc_obj, OBJ_F_NPC_SHIT_LIST_IDX, idx, obj);
            }
            obj_arrayfield_length_set(npc_obj, OBJ_F_NPC_SHIT_LIST_IDX, cnt - 1);
            cnt--;
            idx--;
        }
    }
}

// 0x4AFA90
int64_t ai_shitlist_get(int64_t obj)
{
    int cnt;
    int start;
    int index;
    int64_t shit_obj;

    ai_shitlist_remove(obj, OBJ_HANDLE_NULL);

    cnt = obj_arrayfield_length_get(obj, OBJ_F_NPC_SHIT_LIST_IDX);
    start = random_between(0, cnt - 1);

    for (index = 0; index < cnt; index++) {
        obj_arrayfield_obj_get(obj, OBJ_F_NPC_SHIT_LIST_IDX, (start + index) % cnt, &shit_obj);
        if (ai_combat_target_is_valid(obj, shit_obj)) {
            return shit_obj;
        }
    }

    return OBJ_HANDLE_NULL;
}

// 0x4AFB30
bool ai_shitlist_has(int64_t npc_obj, int64_t shit_obj)
{
    int cnt;
    int idx;
    int64_t obj;

    if (npc_obj != OBJ_HANDLE_NULL
        && obj_field_int32_get(npc_obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
        cnt = obj_arrayfield_length_get(npc_obj, OBJ_F_NPC_SHIT_LIST_IDX);
        for (idx = 0; idx < cnt; idx++) {
            obj_arrayfield_obj_get(npc_obj, OBJ_F_NPC_SHIT_LIST_IDX, idx, &obj);
            if (obj == shit_obj) {
                return true;
            }
        }
    }

    return false;
}

// 0x4AFBB0
int ai_max_dialog_distance(int64_t obj)
{
    return player_is_pc_obj(obj) ? 5 : 10;
}

// 0x4AFBD0
void ai_target_lock(int64_t obj, int64_t tgt)
{
    int danger_type;
    int64_t danger_source;
    unsigned int flags;

    if (obj != OBJ_HANDLE_NULL
        && tgt != OBJ_HANDLE_NULL
        && obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
        ai_target_unlock(obj);
        ai_danger_state_set(obj, AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS, tgt, 0);
        ai_danger_source(obj, &danger_type, &danger_source);
        if (danger_type == AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS
            && danger_source == tgt) {
            flags = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2);
            obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS2, flags | OCF2_TARGET_LOCK);
        }
    }
}

// 0x4AFC70
void ai_target_unlock(int64_t obj)
{
    unsigned int flags;

    if (obj != OBJ_HANDLE_NULL
        && obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
        flags = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2);
        if ((flags & OCF2_TARGET_LOCK) != 0) {
            obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS2, flags & ~OCF2_TARGET_LOCK);
        }
    }
}
