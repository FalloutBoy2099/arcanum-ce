#include "game/magictech.h"
#include <limits.h>
#include "game/magictech_runtime.h"
#include "game/utility_ai.h"

#include <stdio.h>

#include "game/ai.h"
#include "game/anim.h"
#include "game/anim_private.h"
#include "game/animfx.h"
#include "game/antiteleport.h"
#include "game/area.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/descriptions.h"
#include "game/effect.h"
#include "game/fate.h"
#include "game/gsound.h"
#include "game/item.h"
#include "game/light_scheme.h"
#include "game/map.h"
#include "game/mes.h"
#include "game/mp_utils.h"
#include "game/mt_ai.h"
#include "game/mt_item.h"
#include "game/multiplayer.h"
#include "game/obj_private.h"
#include "game/object.h"
#include "game/path.h"
#include "game/player.h"
#include "game/proto.h"
#include "game/random.h"
#include "game/reaction.h"
#include "game/resistance.h"
#include "game/script.h"
#include "game/sector.h"
#include "game/spell.h"
#include "game/stat.h"
#include "game/target.h"
#include "game/teleport.h"
#include "game/tile.h"
#include "game/tile_script.h"
#include "game/timeevent.h"
#include "game/ui.h"

typedef void (*MagicTechProc)(void);

typedef struct MagicTechSummonTableEntry {
    /* 0000 */ int value;
    /* 0004 */ int basic_prototype;
} MagicTechSummonTableEntry;

typedef struct MagicTechSummonTable {
    /* 0000 */ int cnt;
    /* 0004 */ MagicTechSummonTableEntry* entries;
    /* 0008 */ int type;
} MagicTechSummonTable;

static bool magictech_run_info_save(MagicTechRunInfo* run_info, TigFile* stream);
static bool magictech_spell_headers_load(int unused_context, const char* path, int first_message_id);
static bool magictech_spell_components_load_all(int reserved_rule_set, const char* reserved_rule_path, int message_base);
static void magictech_spell_header_parse(mes_file_handle_t msg_file, MagicTechInfo* info, int num, int magictech);
static void magictech_effect_components_load(mes_file_handle_t msg_file, MagicTechInfo* info, int num, int magictech);
static void magictech_effect_components_free(void);
static bool magictech_casting_cost_consume(int64_t obj, int magictech);
static int magictech_maintenance_slots_capacity(int64_t obj);
static void magictech_spell_flags_set(int64_t obj, unsigned int flags);
static void magictech_process(void);
static void magictech_component_animation_goal_process(void);
static void magictech_component_animation_goal_terminate_process(void);
static void magictech_component_ai_redirect_process(void);
static void magictech_component_cast_process(void);
static void magictech_component_charge_and_branch_process(void);
static void magictech_component_damage_process(void);
static void magictech_component_destroy_process(void);
static void magictech_component_dispel_process(void);
static void magictech_component_dispel_internal(int mt_id, int64_t obj);
static void magictech_component_effect_process(void);
static void magictech_component_eye_candy_process(void);
static void magictech_component_heal_process(void);
static void magictech_component_identify_process(void);
static void magictech_component_interrupt_process(void);
static void magictech_component_object_flag_process(void);
static void magictech_component_movement_process(void);
static void magictech_component_recharge_process(void);
static void magictech_component_summon_process(void);
static void magictech_pick_proto_from_list(ObjectID* oid, int list);
static void magictech_component_terminate_process(void);
static void magictech_component_test_and_branch_process(void);
static void magictech_component_trait_process(void);
static void magictech_polymorph_art_apply(int64_t obj, tig_art_id_t art_id);
static void magictech_component_indexed_trait_process(void);
static void magictech_component_trait_int64_process(void);
static void magictech_component_use_process(void);
static void magictech_component_noop_process(void);
static void magictech_component_unsupported_environment_flag_process(void);
static bool magictech_current_run_costs_and_stacking_check(void);
static bool magictech_maintenance_cost_consume(int64_t obj, int magictech);
static bool magictech_maintenance_cost_consume_with_surcharge(int64_t obj, int magictech, int surcharge_percent);
static bool magictech_other_instance_on_target_find(int mt_id, int spell, int64_t obj, int* other_mt_id_ptr);
static void magictech_conflicting_spell_flags_cancel(MagicTechRunInfo* run_info);
static void magictech_current_target_context_init(void);
static bool magictech_source_antimagic_check(void);
static bool magictech_current_target_resistance_resolve(void);
static void magictech_component_target_prepare(void);
static void magictech_aggressive_target_notify(void);
static void magictech_aggressive_target_notify_once(int64_t attacker_obj, int64_t target_obj);
static void magictech_current_run_advance_or_finish(void);
static bool magictech_run_maintenance_capacity_check(MagicTechRunInfo* run_info);
static bool magictech_target_is_in_range(int64_t source_loc, int64_t target_loc, int64_t target_obj, int spell);
static void magictech_maintenance_timeevent_prepare(TimeEvent* timeevent, int mt_id, int period_seconds, DateTime* datetime);
static bool magictech_maintenance_schedule_and_show(TimeEvent* timeevent, DateTime* datetime);
static bool magictech_timeevent_add_unless_pending(TimeEvent* timeevent, DateTime* delay, DateTime* at);
static int magictech_matching_spell_flag_instances_count(int64_t obj, int fld, unsigned int flag_mask, bool include_base_flag);
static bool magictech_unlock_difficulty_check(int64_t caster_obj, int64_t power_source_obj, int64_t locked_obj);
static void magictech_duration_adjust_for_aptitude_and_mastery(MagicTechRunInfo* run_info, DateTime* datetime);
static void magictech_move_object_along_path(int64_t obj, int64_t target_loc);
static void magictech_summoned_object_add(MagicTechRunInfo* run_info, int64_t obj);
static bool magictech_target_antimagic_and_reflection_resolve(TargetContext* target_ctx, MagicTechRunInfo* run_info);
static void magictech_run_slots_reset(void);
static void magictech_id_new_lock(MagicTechRunInfo** lock_ptr);
static void magictech_id_free_lock(int mt_id);
static void magictech_run_object_lists_release(MagicTechRunInfo* run_info);
static void magictech_run_slot_clear(MagicTechRunInfo* run_info);
static void magictech_invocation_start(MagicTechInvocation* mt_invocation);
static bool magictech_disallowed_flags_check(int64_t caster_obj, int64_t target_obj, MagicTechInfo* magictech);
static void magictech_preload_art(MagicTechRunInfo* run_info);
static void magictech_run_callback_action_process(int mt_id, int action);
static bool magictech_maintenance_timeevent_matches_current(TimeEvent* timeevent);
static void magictech_run_end_immediate(int mt_id);
static void magictech_source_detach(int mt_id);
static void magictech_spell_info_defaults_init(MagicTechInfo* info, int magictech);
static void magictech_build_aoe_info(MagicTechInfo* info, char* str);
static void magictech_spell_costs_and_duration_parse(MagicTechInfo* info, char* str);
static void magictech_spell_animation_parse(MagicTechInfo* info, char* str);
static void magictech_spell_flags_parse(MagicTechInfo* info, char* str);
static void magictech_build_ai_info(MagicTechInfo* info, char* str);
static void magictech_build_effect_info(MagicTechInfo* info, char* str);
static bool magictech_find_first(int64_t obj, int* mt_id_ptr);
static bool magictech_find_next(int64_t obj, int* mt_id_ptr);
static bool magictech_spell_instance_on_target_find(int64_t obj, int spell, int* index_ptr);
static void magictech_maintenance_action_process(int mt_id);
static bool magictech_timeevent_matches_selected_action(TimeEvent* timeevent);
static bool magictech_recharge_timeevent_schedule(int64_t item_obj, int mana_cost, bool add);
static bool magictech_recharge_timeevent_add(TimeEvent* timeevent);
static void magictech_run_info_free(MagicTechRunInfo* run_info);
static void magictech_debug_object_print(int64_t obj, const char* msg);

// 0x596140
static uint64_t magictech_target_flag_values[] = {
    TGT_NONE,
    TGT_SELF,
    TGT_SOURCE,
    TGT_OBJECT,
    TGT_OBJ_SELF | TGT_OBJECT,
    TGT_OBJ_RADIUS,
    TGT_OBJ_T_PC | TGT_OBJECT,
    TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_OBJ_ST_CRITTER_ANIMAL | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_OBJ_ST_CRITTER_DEAD | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_OBJ_ST_CRITTER_UNDEAD | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_OBJ_ST_CRITTER_DEMON | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_OBJ_ST_CRITTER_MECHANICAL | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_OBJ_ST_CRITTER_GOOD | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_OBJ_ST_CRITTER_EVIL | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_OBJ_ST_CRITTER_UNREVIVIFIABLE | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_OBJ_ST_CRITTER_UNRESURRECTABLE | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_OBJ_T_PORTAL | TGT_OBJECT,
    TGT_OBJ_T_CONTAINER | TGT_OBJECT,
    TGT_OBJ_ST_OPENABLE_LOCKED | TGT_OBJECT,
    TGT_OBJ_T_WALL | TGT_OBJECT,
    TGT_OBJ_DAMAGED | TGT_OBJECT,
    TGT_OBJ_DAMAGED_POISONED | TGT_OBJECT,
    TGT_OBJ_POISONED | TGT_OBJECT,
    TGT_OBJ_M_STONE | TGT_OBJECT,
    TGT_OBJ_M_FLESH | TGT_OBJECT,
    TGT_OBJ_INVEN | TGT_OBJECT,
    TGT_OBJ_WEIGHT_BELOW_5 | TGT_OBJECT,
    TGT_OBJ_IN_WALL | TGT_OBJECT,
    TGT_OBJ_NO_SELF | TGT_OBJECT,
    TGT_OBJ_NO_T_PC | TGT_OBJECT,
    TGT_OBJ_NO_ST_CRITTER_ANIMAL | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_OBJ_NO_ST_CRITTER_DEAD | TGT_OBJECT, // FIXME: Does not include `TGT_OBJ_T_CRITTER`.
    TGT_OBJ_NO_ST_CRITTER_UNDEAD | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_OBJ_NO_ST_CRITTER_DEMON | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_OBJ_NO_ST_CRITTER_MECHANICAL | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_OBJ_NO_ST_CRITTER_GOOD | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_OBJ_NO_ST_CRITTER_EVIL | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_OBJ_NO_ST_CRITTER_UNREVIVIFIABLE | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_OBJ_NO_ST_CRITTER_UNRESURRECTABLE | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_OBJ_NO_ST_OPENABLE_LOCKED | TGT_OBJECT,
    TGT_OBJ_NO_ST_MAGICALLY_HELD | TGT_OBJECT,
    TGT_OBJ_NO_T_WALL | TGT_OBJECT,
    TGT_OBJ_NO_DAMAGED | TGT_OBJECT,
    TGT_OBJ_NO_M_STONE | TGT_OBJECT,
    TGT_OBJ_NO_INVEN | TGT_OBJECT,
    TGT_OBJ_NO_INVULNERABLE | TGT_OBJECT,
    TGT_SUMMONED | TGT_OBJECT,
    TGT_TILE,
    TGT_TILE_SELF,
    TGT_TILE_PATHABLE_TO | TGT_TILE,
    TGT_TILE_EMPTY | TGT_TILE,
    TGT_TILE_EMPTY_IMMOBILES | TGT_TILE,
    TGT_TILE_NO_EMPTY | TGT_TILE,
    TGT_TILE_RADIUS | TGT_TILE,
    TGT_TILE_RADIUS_WALL | TGT_TILE,
    TGT_TILE_OFFSCREEN | TGT_TILE,
    TGT_TILE_INDOOR_OR_OUTDOOR_MATCH | TGT_TILE,
    TGT_CONE,
    TGT_ALL_PARTY_CRITTERS | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_PARTY_CRITTER | TGT_OBJ_T_CRITTER | TGT_OBJECT,
    TGT_NON_PARTY_CRITTERS | TGT_OBJECT,
    TGT_PARENT_OBJ | TGT_OBJECT,
    TGT_ATTACKER_OBJ | TGT_OBJECT,
    TGT_LIST,
};

// 0x5B0BA0
static int magictech_selected_timeevent_run_id = -1;

// 0x5B0BA4
static int magictech_cur_id = -1;

// 0x4513FD
static MagicTechProc magictech_procs[] = {
    magictech_component_noop_process,
    magictech_component_animation_goal_process,
    magictech_component_animation_goal_terminate_process,
    magictech_component_ai_redirect_process,
    magictech_component_cast_process,
    magictech_component_charge_and_branch_process,
    magictech_component_damage_process,
    magictech_component_destroy_process,
    magictech_component_dispel_process,
    magictech_component_effect_process,
    magictech_component_unsupported_environment_flag_process,
    magictech_component_eye_candy_process,
    magictech_component_heal_process,
    magictech_component_identify_process,
    magictech_component_interrupt_process,
    magictech_component_movement_process,
    magictech_component_object_flag_process,
    magictech_component_recharge_process,
    magictech_component_summon_process,
    magictech_component_terminate_process,
    magictech_component_test_and_branch_process,
    magictech_component_trait_process,
    magictech_component_indexed_trait_process,
    magictech_component_trait_int64_process,
    magictech_component_use_process,
};

// 0x5B0C0C
static const char* magictech_action_names[] = {
    "[Begin]",
    "[Maintain]",
    "[End]",
    "[Callback]",
    "[EndCallback]",
};

// 0x5B0C20
static const char* magictech_info_flag_names[] = {
    "neutral",
    "friendly",
    "aggressive",
    "may_be_aggressive",
};

// 0x5B0C30
static MagicTechFlags magictech_info_flag_values[] = {
    0,
    MAGICTECH_FRIENDLY,
    MAGICTECH_AGGRESSIVE,
    MAGICTECH_FRIENDLY | MAGICTECH_AGGRESSIVE,
};

// 0x5B0C40
static const char* magictech_animation_goal_names[] = {
    "anim_goal_animate",
    "anim_goal_knockback",
    "anim_goal_follow",
    "anim_goal_floating",
    "anim_goal_destroy_obj",
    "anim_goal_knock_down",
};

// 0x5B0C58
static int magictech_animation_goal_types[6] = {
    0,
    43,
    24,
    44,
    32,
    62,
};

// 0x5B0C70
static const char* magictech_damage_type_names[] = {
    "Dmg_Normal",
    "Dmg_Poison",
    "Dmg_Electrical",
    "Dmg_Fire",
    "Dmg_Magic",
    "Dmg_Acid",
};

// 0x5B0C88
static const char* magictech_add_remove_names[] = {
    "Remove",
    "Add",
};

// 0x5B0C90
static const char* magictech_flag_state_names[] = {
    "FLAG_OFF",
    "FLAG_ON",
};

// 0x5B0C98
static const char* magictech_comparison_operator_names[] = {
    "Equal",
    "GreaterThan",
    "GreaterThanOrEqual",
    "LessThan",
    "LessThanOrEqual",
};

// 0x5B0CAC
static const char* magictech_trait_field_names[] = {
    "art_num",
    "Teleport_Tile",
    "obj_f_critter_fleeing_from",
    "HP_Damage",
};

// 0x5B0CBC
static int magictech_trait_fields[] = {
    OBJ_F_CURRENT_AID,
    OBJ_F_CRITTER_TELEPORT_DEST,
    OBJ_F_CRITTER_FLEEING_FROM,
    OBJ_F_HP_DAMAGE,
};

// 0x5B0CCC
static const char* magictech_indexed_trait_field_names[] = {
    "obj_f_critter_stat_base_idx",
    "obj_f_resistance_idx",
};

// 0x5B0CD4
static int magictech_indexed_trait_fields[] = {
    OBJ_F_CRITTER_STAT_BASE_IDX,
    OBJ_F_RESISTANCE_IDX,
};

// 0x5B0CDC
static const char** magictech_indexed_trait_value_names[] = {
    stat_lookup_keys_tbl,
    resistance_lookup_keys_tbl,
};

// 0x5B0CE4
static int magictech_indexed_trait_value_counts[] = {
    STAT_COUNT,
    RESISTANCE_TYPE_COUNT,
};

// 0x5B0CF8
static const char* magictech_trait_int64_source_names[] = {
    "Target_Tile",
    "Caster_Obj",
    "Caster_Owner_Obj",
};

// 0x5B0D04
static const char* magictech_movement_destination_names[] = {
    "Target_Tile",
    "Random_Tile_In_Radius_Self",
    "Teleport_Tile",
    "Teleport_Area_Portal",
};

// 0x5B0D14
static const char* magictech_damage_flag_names[] = {
    "None",
    "Full",
    "Resurrect",
    "Reanimate",
    "Death",
    "Stunned",
    "Scars",
    "Scaled",
    "Weap_Dropped",
    "Crippling",
};

// 0x5B0D3C
static unsigned int magictech_damage_flag_values[] = {
    0,
    COMBAT_DAMAGE_FULL,
    COMBAT_DAMAGE_RESURRECT,
    COMBAT_DAMAGE_REANIMATE | COMBAT_DAMAGE_RESURRECT,
    COMBAT_DAMAGE_DEATH,
    COMBAT_DAMAGE_STUN,
    COMBAT_DAMAGE_SCAR,
    COMBAT_DAMAGE_SCALE,
    COMBAT_DAMAGE_DROP_WEAPON,
    COMBAT_DAMAGE_BLIND | COMBAT_DAMAGE_CRIPPLE_ARM | COMBAT_DAMAGE_CRIPPLE_LEG,
};

// 0x5B0D64
static struct {
    const char* aoe;
    const char* aoe_sf;
    const char* aoe_no_sf;
    const char* radius;
    const char* count;
} magictech_action_parameter_labels[] = {
    {
        "[Begin]AoE:",
        "[Begin]AoE_SF:",
        "[Begin]AoE_NO_SF:",
        "[Begin]Radius:",
        "[Begin]Count:",
    },
    {
        "[Maintain]AoE:",
        "[Maintain]AoE_SF:",
        "[Maintain]AoE_NO_SF:",
        "[Maintain]Radius:",
        "[Maintain]Count:",
    },
    {
        "[End]AoE:",
        "[End]AoE_SF:",
        "[End]AoE_NO_SF:",
        "[End]Radius:",
        "[End]Count:",
    },
    {
        "[Callback]AoE:",
        "[Callback]AoE_SF:",
        "[Callback]AoE_NO_SF:",
        "[Callback]Radius:",
        "[Callback]Count:",
    },
    {
        "[EndCallback]AoE:",
        "[EndCallback]AoE_SF:",
        "[EndCallback]AoE_NO_SF:",
        "[EndCallback]Radius:",
        "[EndCallback]Count:",
    },
};

// 0x5B0DC8
static int magictech_eye_candy_sound_effects[] = {
    0,
    4,
    5,
    6,
    3,
    7,
};

// 0x5B0DE0
static int magictech_intelligence_requirements_by_tier[] = {
    6,
    9,
    12,
    15,
    18,
};

// 0x5B0DF8
static MagicTechSummonTableEntry magictech_summon_wild_animal_entries[] = {
    { 20, BP_WOLF },
    { 1, BP_VORPAL_BUNNY },
    { 19, BP_FOREST_APE },
    { 20, BP_GRIZZLY_BEAR },
    { 20, BP_COUGAR },
    { 20, BP_TIGER },
};

// 0x5B0E28
static MagicTechSummonTableEntry magictech_summon_farm_animal_entries[] = {
    { 20, BP_BUNNY },
    { 20, BP_SHEEP },
    { 20, BP_CHICKEN },
    { 20, BP_SHEEP },
    { 20, BP_PIG },
    { 20, BP_COW },
};

// 0x5B0E58
static MagicTechSummonTableEntry magictech_summon_wolf_entries[] = {
    { 99, BP_PATRIARCH_WOLF },
    { 1, BP_VORPAL_BUNNY },
};

// 0x5B0E68
static MagicTechSummonTableEntry magictech_summon_familiar_entries[] = {
    { 25, BP_FAMILIAR_UNDERLING },
    { 50, BP_FAMILIAR },
    { 75, BP_FAMILIAR_SLASHER },
    { 1000, BP_FAMILIAR_BLOOD_CLAW },
};

// 0x5B0E88
static MagicTechSummonTableEntry magictech_summon_scaled_animal_entries[] = {
    { 20, BP_WOLF },
    { 40, BP_COUGAR },
    { 55, BP_TIGER },
    { 70, BP_GRIZZLY_BEAR },
    { 95, BP_FOREST_APE },
    { 1000, BP_VORPAL_BUNNY },
};

// 0x5B0EB8
static MagicTechSummonTableEntry magictech_summon_undead_entries[] = {
    { 20, BP_DECAYED_SOLDIER },
    { 60, BP_RAGGED_FIGHTER },
    { 85, BP_SKELETON_WARRIOR },
    { 1000, BP_UNDEAD_CHAMPION },
};

// 0x5B0ED8
static MagicTechSummonTable magictech_summon_tables[6] = {
    { SDL_arraysize(magictech_summon_wild_animal_entries), magictech_summon_wild_animal_entries, 0 },
    { SDL_arraysize(magictech_summon_farm_animal_entries), magictech_summon_farm_animal_entries, 0 },
    { SDL_arraysize(magictech_summon_wolf_entries), magictech_summon_wolf_entries, 0 },
    { SDL_arraysize(magictech_summon_familiar_entries), magictech_summon_familiar_entries, 1 },
    { SDL_arraysize(magictech_summon_scaled_animal_entries), magictech_summon_scaled_animal_entries, 1 },
    { SDL_arraysize(magictech_summon_undead_entries), magictech_summon_undead_entries, 1 },
};

// 0x5B0F20
static int magictech_selected_timeevent_index = -1;

// 0x5BBD70
const char* magictech_target_flag_names[] = {
    "Tgt_None",
    "Tgt_Self",
    "Tgt_Source",
    "Tgt_Object",
    "Tgt_Obj_Self",
    "Tgt_Obj_Radius",
    "Tgt_Obj_T_PC",
    "Tgt_Obj_T_Critter",
    "Tgt_Obj_ST_Critter_Animal",
    "Tgt_Obj_ST_Critter_Dead",
    "Tgt_Obj_ST_Critter_Undead",
    "Tgt_Obj_ST_Critter_Demon",
    "Tgt_Obj_ST_Critter_Mechanical",
    "Tgt_Obj_ST_Critter_Good",
    "Tgt_Obj_ST_Critter_Evil",
    "Tgt_Obj_ST_Critter_Unrevivifiable",
    "Tgt_Obj_ST_Critter_Unresurrectable",
    "Tgt_Obj_T_Portal",
    "Tgt_Obj_T_Container",
    "Tgt_Obj_ST_Openable_Locked",
    "Tgt_Obj_T_Wall",
    "Tgt_Obj_Damaged",
    "Tgt_Obj_Damaged_Poisoned",
    "Tgt_Obj_Poisoned",
    "Tgt_Obj_M_Stone",
    "Tgt_Obj_M_Flesh",
    "Tgt_Obj_Inven",
    "Tgt_Obj_Weight_Below_5",
    "Tgt_Obj_In_Wall",
    "Tgt_Obj_No_Self",
    "Tgt_Obj_No_T_PC",
    "Tgt_Obj_No_ST_Critter_Animal",
    "Tgt_Obj_No_ST_Critter_Dead",
    "Tgt_Obj_No_ST_Critter_Undead",
    "Tgt_Obj_No_ST_Critter_Demon",
    "Tgt_Obj_No_ST_Critter_Mechanical",
    "Tgt_Obj_No_ST_Critter_Good",
    "Tgt_Obj_No_ST_Critter_Evil",
    "Tgt_Obj_No_ST_Critter_Unrevivifiable",
    "Tgt_Obj_No_ST_Critter_Unresurrectable",
    "Tgt_Obj_No_ST_Openable_Locked",
    "Tgt_Obj_No_ST_Magically_Held",
    "Tgt_Obj_No_T_Wall",
    "Tgt_Obj_No_Damaged",
    "Tgt_Obj_No_M_Stone",
    "Tgt_Obj_No_Inven",
    "Tgt_Obj_No_Invulnerable",
    "Tgt_Summoned",
    "Tgt_Tile",
    "Tgt_Tile_Self",
    "Tgt_Tile_Pathable_To",
    "Tgt_Tile_Empty",
    "Tgt_Tile_Empty_Immobiles",
    "Tgt_Tile_No_Empty",
    "Tgt_Tile_Radius",
    "Tgt_Tile_Radius_Wall",
    "Tgt_Tile_Offscreen",
    "Tgt_Tile_Indoor_Or_Outdoor_Match",
    "Tgt_Cone",
    "Tgt_All_Party_Critters",
    "Tgt_Party_Critter",
    "Tgt_Non_Party_Critters",
    "Tgt_Parent_Obj",
    "Tgt_Attacker_Obj",
    "Tgt_List",
};

// 0x5E3510
static bool magictech_editor;

// 0x5E3518
static TargetList magictech_current_target_list;

// 0x5E6D20
static mes_file_handle_t magictech_spell_rules_mes_file;

// 0x5E6D24
static IsoInvalidateRectFunc* magictech_invalidate_rect_callback;

// 0x5E6D28
static TargetContext magictech_current_target_context;

// 0x5E6D90
static int magictech_branch_destination;

// 0x5E7568
static AnimFxList spell_eye_candies;

// 0x5E7594
static mes_file_handle_t magictech_spell_mes_file;

// 0x5E7598
static MagicTechInfo* magictech_cur_spell_info;

// 0x5E759C
static MagicTechComponentList* magictech_cur_component_list;

// 0x5E75A0
static int magictech_current_resistance_amount;

// 0x5E75A8
static bool magictech_cur_is_fate_maximized;

// 0x5E75AC
static int magictech_cur_target_obj_type;

// 0x5E75B0
static int64_t magictech_original_source_obj;

// 0x5E75B8
static int64_t magictech_source_swap_obj;

// 0x5E75C0
static AnimID magictech_component_animation_id;

// 0x5E75CC
static int magictech_waiting_for_effect_callback;

// 0x5E75D0
static unsigned int magictech_damage_target_object_flags;

// 0x5E75D4
static int magictech_effect_callback_pending;

// 0x5E75D8
static int magictech_effect_end_callback_pending;

// 0x5E75DC
static int magictech_current_run_had_effect;

// 0x5E75E0
static int64_t magictech_last_aggression_notified_obj;

// 0x5E75E4
static int magictech_unused_state;

// 0x5E75E8
static int magictech_initial_processing_action;

// 0x5E75F0
static MagicTechRunInfo* magictech_cur_run_info;

// 0x5E75F4
static mes_file_handle_t magictech_mes_file;

// 0x5E75F8
static const char** magictech_component_names;

// 0x5E75FC
static bool magictech_cheat_mode;

// 0x5E7600
static int magictech_ignore_intelligence_requirements;

// 0x5E7604
static bool magictech_clearing_events;

// 0x5E7608
static int magictech_recharge_timeevent_mana_cost;

// 0x5E760C
static int magictech_cast_event_start_seconds;

// 0x5E7610
static int64_t magictech_recharge_timeevent_item_obj;

// 0x5E7618
static bool magictech_initialized;

// 0x5E761C
static MagicTechComponentInfo* magictech_cur_component;

// 0x5E7620
static MagicTechResistance* magictech_cur_resistance;

// 0x5E7624
static bool magictech_branch_pending;

// 0x5E7628
static int magictech_spell_being_parsed;

// 0x5E762C
static int magictech_last_checked_cost;

// 0x5E7630
static bool magictech_effect_callback_processing;

// 0x5E7634
static int magictech_selected_timeevent_subtype;

// 0x6876D8
MagicTechInfo* magictech_spells;

// 0x6876DC
static int magictech_allocated_run_count;

// 0x6876E0
MagicTechRunInfo* magictech_run_info;

// 0x44EF50
bool magictech_init(GameInitInfo* init_info)
{
    MesFileEntry mes_file_entry;
    int index;

    magictech_invalidate_rect_callback = init_info->invalidate_rect_func;
    magictech_editor = init_info->editor;
    magictech_component_names = (const char**)CALLOC(25, sizeof(const char*));
    magictech_run_info = (MagicTechRunInfo*)CALLOC(512, sizeof(MagicTechRunInfo));
    if (magictech_component_names == NULL || magictech_run_info == NULL) {
        FREE(magictech_component_names);
        FREE(magictech_run_info);
        magictech_run_info = NULL;
        return false;
    }
    magictech_run_slots_reset();

    if (!mes_load("Rules\\magictech.mes", &magictech_mes_file)) {
        FREE(magictech_component_names);
        FREE(magictech_run_info);
        magictech_run_info = NULL;
        return false;
    }

    if (!mes_load("mes\\spell.mes", &magictech_spell_mes_file)) {
        FREE(magictech_component_names);
        FREE(magictech_run_info);
        magictech_run_info = NULL;
        return false;
    }

    mes_file_entry.num = 10;
    for (index = 0; index < 25; index++) {
        if (!mes_search(magictech_mes_file, &mes_file_entry)) {
            FREE(magictech_component_names);
            FREE(magictech_run_info);
            magictech_run_info = NULL;
            return false;
        }

        magictech_component_names[index] = mes_file_entry.str;
        mes_file_entry.num++;
    }

    if (!magictech_editor) {
        if (!animfx_list_init(&spell_eye_candies)) {
            FREE(magictech_component_names);
            FREE(magictech_run_info);
            magictech_run_info = NULL;
            return false;
        }

        spell_eye_candies.path = "Rules\\SpellEyeCandy.mes";
        spell_eye_candies.capacity = 1344;
        spell_eye_candies.num_fields = 6;
        spell_eye_candies.step = 10;
        spell_eye_candies.num_sound_effects = 6;
        spell_eye_candies.sound_effects = magictech_eye_candy_sound_effects;
        if (!animfx_list_load(&spell_eye_candies)) {
            FREE(magictech_component_names);
            FREE(magictech_run_info);
            magictech_run_info = NULL;
            return false;
        }
    }

    magictech_spells = (MagicTechInfo*)MALLOC(sizeof(MagicTechInfo) * MT_SPELL_COUNT);

    if (!magictech_spell_headers_load(0, "Rules\\spelllist.mes", 1000)) {
        FREE(magictech_component_names);
        FREE(magictech_run_info);
        magictech_run_info = NULL;
        return false;
    }

    magictech_initialized = true;
    magictech_allocated_run_count = 0;

    return true;
}

// 0x44F150
void magictech_reset(void)
{
    magictech_cheat_mode = false;
}

// 0x44F160
bool magictech_post_init(GameInitInfo* init_info)
{
    (void)init_info;

    if (!magictech_spell_components_load_all(0, "Rules\\spelllist.mes", 1000)) {
        FREE(magictech_component_names);
        FREE(magictech_run_info);
        magictech_run_info = NULL;
        magictech_initialized = false;
        return false;
    }

    return true;
}

// 0x44F1B0
void magictech_exit(void)
{
    if (magictech_initialized) {
        if (!magictech_editor) {
            if (spell_eye_candies.num_effects) {
                animfx_list_exit(&spell_eye_candies);
            }
        }

        magictech_effect_components_free();

        mes_unload(magictech_spell_mes_file);
        mes_unload(magictech_mes_file);

        if (magictech_spells != NULL) {
            FREE(magictech_spells);
            magictech_spells = NULL;
        }

        if (magictech_component_names != NULL) {
            FREE(magictech_component_names);
        }

        if (magictech_run_info != NULL) {
            FREE(magictech_run_info);
            magictech_run_info = NULL;
        }

        magictech_initialized = false;
    }
}

// 0x44F250
bool magictech_post_save(TigFile* stream)
{
    int cnt;
    int index;
    int start;
    int extent;

    if (stream == NULL) {
        return false;
    }

    if (tig_file_fwrite(&magictech_allocated_run_count, sizeof(magictech_allocated_run_count), 1, stream) != 1) {
        return false;
    }

    cnt = 512;
    if (tig_file_fwrite(&cnt, sizeof(cnt), 1, stream) != 1) {
        return false;
    }

    index = 0;
    while (index < cnt) {
        start = index;
        while (index < cnt && (magictech_run_info[index].flags & MAGICTECH_RUN_ACTIVE) != 0) {
            index++;
        }

        extent = index - start;
        if (extent > 0) {
            if (tig_file_fwrite(&extent, sizeof(extent), 1, stream) != 1) {
                return false;
            }

            while (start < index) {
                if (!magictech_run_info_save(&(magictech_run_info[start]), stream)) {
                    return false;
                }
                start++;
            }
        }

        while (index < cnt && (magictech_run_info[index].flags & MAGICTECH_RUN_ACTIVE) == 0) {
            index++;
        }

        extent = index - start;
        if (extent > 0) {
            extent = -extent;
            if (tig_file_fwrite(&extent, sizeof(extent), 1, stream) != 1) {
                return false;
            }
        }
    }

    return true;
}

// 0x44F3C0
bool magictech_run_info_save(MagicTechRunInfo* run_info, TigFile* stream)
{
    if (stream == NULL) return false;
    if (tig_file_fwrite(&(run_info->id), sizeof(run_info->id), 1, stream) != 1) return false;
    if (tig_file_fwrite(&(run_info->spell), sizeof(run_info->spell), 1, stream) != 1) return false;
    if (tig_file_fwrite(&(run_info->action), sizeof(run_info->action), 1, stream) != 1) return false;
    if (!mt_obj_node_save_detached(&(run_info->source_obj), stream)) return false;
    if (!mt_obj_node_save_detached(&(run_info->parent_obj), stream)) return false;
    if (!mt_obj_node_save_detached(&(run_info->target_obj), stream)) return false;
    if (!mt_obj_node_save_detached(&(run_info->attacker_obj), stream)) return false;
    if (!mt_obj_node_save_list(&(run_info->objlist), stream)) return false;
    if (!mt_obj_node_save_list(&(run_info->summoned_obj), stream)) return false;
    if (tig_file_fwrite(&(run_info->applied_spell_flags), sizeof(run_info->applied_spell_flags), 1, stream) != 1) return false;
    if (tig_file_fwrite(&(run_info->flags), sizeof(run_info->flags), 1, stream) != 1) return false;
    if (tig_file_fwrite(&(run_info->trigger), sizeof(run_info->trigger), 1, stream) != 1) return false;
    if (tig_file_fwrite(&(run_info->resistance_amount), sizeof(run_info->resistance_amount), 1, stream) != 1) return false;
    if (tig_file_fwrite(&(run_info->event_time), sizeof(run_info->event_time), 1, stream) != 1) return false;
    if (tig_file_fwrite(&(run_info->remaining_duration_triggers), sizeof(run_info->remaining_duration_triggers), 1, stream) != 1) return false;

    return true;
}

// 0x44F540
bool magictech_post_load(GameLoadInfo* load_info)
{
    return load_info != NULL
        && magictech_runs_load(load_info->stream, magictech_run_info, &magictech_allocated_run_count);
}

// 0x44F7A0
void magictech_break_nodes_to_map(const char* map)
{
    char path[TIG_MAX_PATH];
    TigFile* stream;
    bool append;
    int cnt;
    int idx;
    MagicTechObjectNode* node;

    snprintf(path, sizeof(path), "save\\current\\maps\\%s\\MT.dat", map);

    if (tig_file_exists(path, NULL)) {
        append = true;
        stream = tig_file_fopen(path, "r+b");
    } else {
        append = false;
        stream = tig_file_fopen(path, "wb");
    }

    if (stream == NULL) {
        // FIXME: Message is misleading, informs about TimeEvent, not magictech.
        tig_debug_printf("MagicTech: magictech_save_nodes_to_map: ERROR: Couldn't create TimeEvent data file for map!\n");
        return;
    }

    cnt = 0;
    if (!append) {
        if (tig_file_fwrite(&cnt, sizeof(cnt), 1, stream) != 1) {
            tig_debug_printf("MagicTech: magictech_save_nodes_to_map: ERROR: Writing Header to data file for map!\n");
            tig_file_fclose(stream);
            return;
        }
    } else {
        if (tig_file_fseek(stream, 0, SEEK_SET) != 0) {
            tig_debug_printf("MagicTech: magictech_save_nodes_to_map: ERROR: Seeking to start of data file for map!\n");
            tig_file_fclose(stream);
            return;
        }

        if (tig_file_fread(&cnt, sizeof(cnt), 1, stream) != 1) {
            tig_debug_printf("MagicTech: magictech_save_nodes_to_map: ERROR: Reading Header to data file for map!\n");
            tig_file_fclose(stream);
            return;
        }

        if (tig_file_fseek(stream, 0, SEEK_END) != 0) {
            tig_debug_printf("MagicTech: magictech_save_nodes_to_map: ERROR: Seeking to end of data file for map!\n");
            tig_file_fclose(stream);
            return;
        }
    }

    for (idx = 0; idx < 512; idx++) {
        if ((magictech_run_info[idx].flags & MAGICTECH_RUN_ACTIVE) != 0) {
            if (magictech_run_info[idx].source_obj.obj != OBJ_HANDLE_NULL
                && !teleport_is_teleporting_obj(magictech_run_info[idx].source_obj.obj)) {
                magictech_run_end_immediate(idx);
                continue;
            }

            if (magictech_run_info[idx].parent_obj.obj != magictech_run_info[idx].source_obj.obj
                && magictech_run_info[idx].parent_obj.obj != OBJ_HANDLE_NULL
                && !teleport_is_teleporting_obj(magictech_run_info[idx].parent_obj.obj)) {
                magictech_run_end_immediate(idx);
                continue;
            }

            if (magictech_run_info[idx].target_obj.obj != OBJ_HANDLE_NULL
                && !teleport_is_teleporting_obj(magictech_run_info[idx].target_obj.obj)) {
                magictech_run_end_immediate(idx);
                continue;
            }

            node = magictech_run_info[idx].objlist;
            while (node != NULL) {
                if (node->obj != OBJ_HANDLE_NULL
                    && !teleport_is_teleporting_obj(node->obj)) {
                    magictech_run_end_immediate(idx);
                    break;
                }
                node = node->next;
            }
            if (node != NULL) {
                continue;
            }

            node = magictech_run_info[idx].summoned_obj;
            while (node != NULL) {
                if (node->obj != OBJ_HANDLE_NULL
                    && !teleport_is_teleporting_obj(node->obj)) {
                    magictech_run_end_immediate(idx);
                    break;
                }
                node = node->next;
            }
            if (node != NULL) {
                continue;
            }

            if (!magictech_run_info_save(&(magictech_run_info[idx]), stream)) {
                break;
            }

            magictech_id_free_lock(idx);
            cnt++;
        }
    }

    if (idx < 512) {
        tig_debug_printf("MagicTech: magictech_save_nodes_to_map: ERROR: Failed to save out nodes!\n");
        tig_file_fclose(stream);
        return;
    }

    if (tig_file_fseek(stream, 0, SEEK_SET) != 0
        || tig_file_fwrite(&cnt, sizeof(cnt), 1, stream) != 1) {
        tig_debug_printf("MagicTech: magictech_save_nodes_to_map: ERROR: Writing Header to data file for map!\n");
        tig_file_fclose(stream);
        return;
    }

    tig_file_fclose(stream);
}

// 0x44FA70
void magictech_save_nodes_to_map(const char* map)
{
    char path[TIG_MAX_PATH];
    TigFile* stream;
    bool append;
    int cnt;
    int idx;

    snprintf(path, sizeof(path), "save\\current\\maps\\%s\\MT.dat", map);

    if (tig_file_exists(path, NULL)) {
        append = true;
        stream = tig_file_fopen(path, "r+b");
    } else {
        append = false;
        stream = tig_file_fopen(path, "wb");
    }

    if (stream == NULL) {
        // FIXME: Message is misleading, informs about TimeEvent, not magictech.
        tig_debug_printf("MagicTech: magictech_save_nodes_to_map: ERROR: Couldn't create TimeEvent data file for map!\n");
        return;
    }

    cnt = 0;
    if (!append) {
        if (tig_file_fwrite(&cnt, sizeof(cnt), 1, stream) != 1) {
            tig_debug_printf("MagicTech: magictech_save_nodes_to_map: ERROR: Writing Header to data file for map!\n");
            tig_file_fclose(stream);
            return;
        }
    } else {
        if (tig_file_fseek(stream, 0, SEEK_SET) != 0) {
            tig_debug_printf("MagicTech: magictech_save_nodes_to_map: ERROR: Seeking to start of data file for map!\n");
            tig_file_fclose(stream);
            return;
        }

        if (tig_file_fread(&cnt, sizeof(cnt), 1, stream) != 1) {
            tig_debug_printf("MagicTech: magictech_save_nodes_to_map: ERROR: Reading Header to data file for map!\n");
            tig_file_fclose(stream);
            return;
        }

        if (tig_file_fseek(stream, 0, SEEK_END) != 0) {
            tig_debug_printf("MagicTech: magictech_save_nodes_to_map: ERROR: Seeking to end of data file for map!\n");
            tig_file_fclose(stream);
            return;
        }
    }

    for (idx = 0; idx < 512; idx++) {
        if ((magictech_run_info[idx].flags & MAGICTECH_RUN_ACTIVE) != 0) {
            if (!magictech_run_info_save(&(magictech_run_info[idx]), stream)) {
                break;
            }
            cnt++;
        }
    }

    if (idx < 512) {
        tig_debug_printf("MagicTech: magictech_save_nodes_to_map: ERROR: Failed to save out nodes!\n");
        tig_file_fclose(stream);
        return;
    }

    if (tig_file_fseek(stream, 0, SEEK_SET) != 0
        || tig_file_fwrite(&cnt, sizeof(cnt), 1, stream) != 1) {
        tig_debug_printf("MagicTech: magictech_save_nodes_to_map: ERROR: Writing Header to data file for map!\n");
        tig_file_fclose(stream);
        return;
    }

    tig_file_fclose(stream);
}

// 0x44FC30
void magictech_load_nodes_from_map(const char* map)
{
    char path[TIG_MAX_PATH];
    int loaded_ids[MAGICTECH_RUN_CAPACITY];
    int loaded_count;
    if (map == NULL || magictech_run_info == NULL) return;
    snprintf(path, sizeof(path), "save\\current\\maps\\%s\\MT.dat", map);
    if (!tig_file_exists(path, NULL)) return;
    TigFile* stream = tig_file_fopen(path, "rb");
    if (stream == NULL) return;
    bool loaded = magictech_map_runs_load(stream, magictech_run_info, &magictech_allocated_run_count, loaded_ids, &loaded_count);
    tig_file_fclose(stream);
    if (!loaded) {
        tig_debug_printf("MagicTech: invalid or incomplete map runs in %s.\n", path);
        return;
    }
    for (int index = 0; index < loaded_count; index++) magictech_run_timeevent_restart(loaded_ids[index]);
    tig_file_remove(path);
}

// 0x44FDC0
void magictech_get_msg(MesFileEntry* mes_file_entry)
{
    mes_get_msg(magictech_spell_mes_file, mes_file_entry);
}

// 0x44FDE0
char* magictech_spell_name(int num)
{
    MesFileEntry mes_file_entry;

    if (num >= 0 && num < MT_SPELL_COUNT) {
        mes_file_entry.num = num;
        mes_get_msg(magictech_spell_mes_file, &mes_file_entry);
        return mes_file_entry.str;
    } else {
        return NULL;
    }
}

// 0x44FE20
void magictech_cheat_mode_on(void)
{
    magictech_cheat_mode = true;
}

// 0x44FE30
bool magictech_spell_headers_load(int unused_context, const char* path, int first_message_id)
{
    int magictech = 0;
    int action;

    (void)unused_context;

    if (!mes_load(path, &magictech_spell_rules_mes_file)) {
        return false;
    }

    for (magictech = 0; magictech < MT_SPELL_COUNT; magictech++) {
        for (action = 0; action < MAGICTECH_ACTION_COUNT; action++) {
            magictech_spells[magictech].components[action].cnt = 0;
            magictech_spells[magictech].components[action].entries = NULL;
        }
    }

    for (magictech = 0; magictech < MAGICTECH_PLAYER_SPELL_COUNT; magictech++) {
        magictech_spell_header_parse(magictech_spell_rules_mes_file, &(magictech_spells[magictech]), first_message_id, magictech);
    }

    for (; magictech < MAGICTECH_FIRST_TECH_ENTRY; magictech++) {
        magictech_spell_header_parse(magictech_spell_rules_mes_file, &(magictech_spells[magictech]), first_message_id, magictech);
        magictech_spells[magictech].iq = 0;
    }

    for (; magictech < MT_SPELL_COUNT; magictech++) {
        magictech_spell_header_parse(magictech_spell_rules_mes_file, &(magictech_spells[magictech]), first_message_id + 2000, magictech);
        magictech_spells[magictech].flags |= MAGICTECH_IS_TECH;
        magictech_spells[magictech].iq = 0;
    }

    // NOTE: Meaningless, never executed.
    for (; magictech < MT_SPELL_COUNT; magictech++) {
        magictech_spell_header_parse(magictech_spell_rules_mes_file, &(magictech_spells[magictech]), first_message_id, magictech);
        magictech_spells[magictech].iq = 0;
    }

    return true;
}

// 0x44FFA0
bool magictech_spell_components_load_all(int reserved_rule_set, const char* reserved_rule_path, int message_base)
{
    int magictech = 0;

    (void)reserved_rule_set;
    (void)reserved_rule_path;

    while (magictech < MAGICTECH_PLAYER_SPELL_COUNT) {
        magictech_effect_components_load(magictech_spell_rules_mes_file, &(magictech_spells[magictech]), message_base, magictech);
        magictech++;
    }

    while (magictech < MAGICTECH_FIRST_TECH_ENTRY) {
        magictech_effect_components_load(magictech_spell_rules_mes_file, &(magictech_spells[magictech]), message_base, magictech);
        magictech_spells[magictech].iq = 0;
        magictech++;
    }

    while (magictech < MT_SPELL_COUNT) {
        magictech_effect_components_load(magictech_spell_rules_mes_file, &(magictech_spells[magictech]), message_base + 2000, magictech);
        magictech_spells[magictech].flags |= MAGICTECH_IS_TECH;
        magictech_spells[magictech].iq = 0;
        magictech++;
    }

    mes_unload(magictech_spell_rules_mes_file);

    return true;
}

// 0x450090
void magictech_spell_header_parse(mes_file_handle_t msg_file, MagicTechInfo* info, int num, int magictech)
{
    MesFileEntry mes_file_entry;

    magictech_spell_being_parsed = magictech;

    magictech_spell_info_defaults_init(info, magictech);

    mes_file_entry.num = magictech;
    mes_get_msg(magictech_spell_mes_file, &mes_file_entry);
    info->name = mes_file_entry.str;

    mes_file_entry.num = num + 50 * magictech;
    mes_get_msg(msg_file, &mes_file_entry);
    magictech_build_aoe_info(info, mes_file_entry.str);

    mes_file_entry.num++;
    mes_get_msg(msg_file, &mes_file_entry);
    magictech_spell_costs_and_duration_parse(info, mes_file_entry.str);

    mes_file_entry.num++;
    if (mes_search(msg_file, &mes_file_entry)) {
        mes_get_msg(msg_file, &mes_file_entry);
        magictech_spell_animation_parse(info, mes_file_entry.str);
    }

    mes_file_entry.num++;
    if (mes_search(msg_file, &mes_file_entry)) {
        mes_get_msg(msg_file, &mes_file_entry);
        magictech_spell_flags_parse(info, mes_file_entry.str);
    }

    memset(&(info->ai), 0xFF, sizeof(info->ai));
    info->defensive2 = 0;

    mes_file_entry.num++;
    if (mes_search(msg_file, &mes_file_entry)) {
        mes_get_msg(msg_file, &mes_file_entry);
        magictech_build_ai_info(info, mes_file_entry.str);
    }
}

// 0x4501D0
void magictech_effect_components_load(mes_file_handle_t msg_file, MagicTechInfo* info, int num, int magictech)
{
    MesFileEntry mes_file_entry;
    int index;

    mes_file_entry.num = num + 50 * magictech + 5;
    for (index = 0; index < 44; index++) {
        if (!mes_search(msg_file, &mes_file_entry)) {
            return;
        }

        mes_get_msg(msg_file, &mes_file_entry);
        magictech_build_effect_info(info, mes_file_entry.str);

        mes_file_entry.num++;
    }
}

// 0x450240
void magictech_effect_components_free(void)
{
    int index;
    int action;

    for (index = 0; index < MT_SPELL_COUNT; index++) {
        for (action = 0; action < MAGICTECH_ACTION_COUNT; action++) {
            if (magictech_spells[index].components[action].entries != NULL) {
                FREE(magictech_spells[index].components[action].entries);
            }
        }
    }
}

// 0x450280
int magictech_get_range(int magictech)
{
    if (magictech_initialized) {
        return magictech_spells[magictech].range;
    } else {
        return 1;
    }
}

// 0x4502B0
int magictech_target_radius_get(int magictech)
{
    if (magictech_initialized) {
        return magictech_spells[magictech].target_params[0].radius;
    } else {
        return 0;
    }
}

// 0x4502E0
int magictech_min_intelligence(int magictech)
{
    (void)magictech;

    return 5;
}

// 0x4502F0
int magictech_min_willpower(int magictech)
{
    if (magictech_initialized) {
        return magictech_spells[magictech].iq;
    }

    if (magictech >= 0 && magictech < MAGICTECH_PLAYER_SPELL_COUNT) {
        return magictech_intelligence_requirements_by_tier[magictech % 5];
    }

    return magictech_intelligence_requirements_by_tier[1];
}

// 0x450340
int magictech_get_cost(int magictech)
{
    if (magictech_initialized) {
        return magictech_spells[magictech].cost;
    } else {
        return 1;
    }
}

// 0x450370
bool magictech_is_aggressive(int magictech)
{
    if (magictech_initialized) {
        return (magictech_spells[magictech].flags & MAGICTECH_AGGRESSIVE) != 0;
    } else {
        return false;
    }
}

// 0x4503A0
bool magictech_has_maintenance(int magictech)
{
    if (magictech_initialized) {
        return magictech_spells[magictech].maintenance.period > 0;
    } else {
        return false;
    }
}

// 0x4503E0
MagicTechMaintenanceInfo* magictech_get_maintenance(int magictech)
{
    return &(magictech_spells[magictech].maintenance);
}

// 0x450400
MagicTechDurationInfo* magictech_get_duration(int magictech)
{
    return &(magictech_spells[magictech].duration);
}

// 0x450420
bool magictech_cost_check_or_consume(int64_t obj, int cost, bool consume_cost, int magictech)
{
    MagicTechInfo* info;
    bool charges_from_cells = false;
    int obj_type;
    int64_t parent_obj;

    if (tig_net_is_active() && !multiplayer_is_locked()) {
        Packet76 pkt;

        if (!tig_net_is_host()) {
            return true;
        }

        pkt.type = 76;
        pkt.oid = obj_get_id(obj);
        pkt.cost = cost;
        pkt.consume_cost = consume_cost;
        pkt.magictech = magictech;
        tig_net_send_app_all(&pkt, sizeof(pkt));
    }

    if (magictech != 10000) {
        info = &(magictech_spells[magictech]);
        if (magictech >= 0
            && magictech < MT_SPELL_COUNT
            && info != NULL
            && (info->flags & 0x20) != 0) {
            charges_from_cells = true;
        }
    } else {
        info = NULL;
    }

    if (cost <= 0) {
        return true;
    }

    magictech_last_checked_cost = cost;

    if (obj == OBJ_HANDLE_NULL) {
        return true;
    }

    obj_type = obj_field_int32_get(obj, OBJ_F_TYPE);

    if (charges_from_cells) {
        if (obj_type_is_item(obj_type)) {
            parent_obj = obj_field_handle_get(obj, OBJ_F_ITEM_PARENT);
            cost = 2 * cost - item_adjust_magic(obj, parent_obj, cost);
        } else {
            parent_obj = obj;
        }

        if (item_ammo_quantity_get(parent_obj, TIG_ART_AMMO_TYPE_CHARGE) < cost) {
            return false;
        }

        if (!item_ammo_transfer(parent_obj, OBJ_HANDLE_NULL, cost, TIG_ART_AMMO_TYPE_CHARGE, OBJ_HANDLE_NULL)) {
            return false;
        }

        return true;
    }

    if (obj_type_is_critter(obj_type)
        && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & OCF_FATIGUE_LIMITING) != 0) {
        cost /= 2;
        if (cost == 0) {
            cost = 1;
        }
    }

    if (obj_type_is_critter(obj_type)) {
        int inventory_location;
        int64_t item_obj;
        int mana_store;
        int new_mana_store;
        int item_mana_cost;

        for (inventory_location = FIRST_WEAR_INV_LOC; inventory_location <= LAST_WEAR_INV_LOC; inventory_location++) {
            item_obj = item_wield_get(obj, inventory_location);
            if (item_obj != OBJ_HANDLE_NULL) {
                mana_store = obj_field_int32_get(item_obj, OBJ_F_ITEM_MANA_STORE);
                if (mana_store != 0) {
                    if (mana_store >= cost) {
                        item_mana_cost = cost;
                        new_mana_store = mana_store - cost;
                        cost = 0;
                    } else {
                        item_mana_cost = mana_store;
                        new_mana_store = 0;
                        cost -= mana_store;
                    }

                    if (consume_cost) {
                        obj_field_int32_set(item_obj, OBJ_F_ITEM_MANA_STORE, new_mana_store);
                        magictech_recharge_timeevent_schedule(item_obj, item_mana_cost, true);
                        ui_counters_refresh();

                        if (new_mana_store == 0) {
                            mt_ai_notify_item_exhausted(obj, item_obj);
                        }
                    }
                }
            }
        }

        if (cost != 0) {
            if (critter_fatigue_current(obj) - cost <= -15) {
                return false;
            }

            if (consume_cost) {
                int fatigue_dam = critter_fatigue_damage_get(obj);
                if (critter_fatigue_damage_set(obj, fatigue_dam + cost) == 0) {
                    return false;
                }

                if (info != NULL
                    && (info->maintenance.period != 0 || info->duration.period == 0)
                    && critter_fatigue_current(obj) < 0) {
                    return false;
                }
            }
        }

        return true;
    }

    if (obj_type_is_item(obj_type)) {
        int mana_store;
        unsigned int item_flags;

        mana_store = obj_field_int32_get(obj, OBJ_F_ITEM_SPELL_MANA_STORE);
        if (mana_store == 0) {
            return false;
        }
        if (mana_store > 0) {
            item_flags = obj_field_int32_get(obj, OBJ_F_ITEM_FLAGS);
            item_flags |= OIF_IS_MAGICAL;
            obj_field_int32_set(obj, OBJ_F_ITEM_FLAGS, item_flags);

            if (consume_cost) {
                obj_field_int32_set(obj, OBJ_F_ITEM_SPELL_MANA_STORE, mana_store - 1);
                ui_counters_refresh();
            }
        }

        return true;
    }

    return true;
}

// 0x4507B0
void magictech_casting_cost_consume_callback(int64_t obj, int magictech)
{
    magictech_casting_cost_consume(obj, magictech);
}

// 0x4507D0
bool magictech_casting_cost_consume(int64_t obj, int magictech)
{
    if (obj == OBJ_HANDLE_NULL) return true;
    int cost = magictech_fatigue_cost(obj, magictech, MAGICTECH_ACTION_BEGIN, magictech_cheat_mode);
    return cost >= 0 && magictech_cost_check_or_consume(obj, cost, true, magictech);
}

// 0x4508A0
bool magictech_can_charge_spell_fatigue(int64_t obj, int magictech)
{
    if (obj == OBJ_HANDLE_NULL) return false;
    int cost = magictech_fatigue_cost(obj, magictech, MAGICTECH_ACTION_BEGIN, magictech_cheat_mode);
    return cost >= 0 && magictech_cost_check_or_consume(obj, cost, false, magictech);
}

// 0x450940
bool magictech_run_cost_check(int mt_id)
{
    return magictech_run_cost_check_for_mode(mt_id, magictech_cheat_mode);
}

int magictech_active_maintenance_cost_estimate(int64_t obj, int horizon_seconds)
{
    int index;
    int total = 0;
    DateTime now = game_time_get();
    if (!magictech_initialized || magictech_run_info == NULL || horizon_seconds <= 0) return 0;
    for (index = 0; index < 512; index++) {
        MagicTechRunInfo* run = &magictech_run_info[index];
        int period;
        int cost;
        int64_t first_payment;
        if ((run->flags & MAGICTECH_RUN_ACTIVE) == 0 || (run->flags & MAGICTECH_RUN_FREE) != 0
            || run->source_obj.obj != obj || run->action != MAGICTECH_ACTION_MAINTAIN
            || run->spell < 0 || run->spell >= MT_SPELL_COUNT) continue;
        cost = spell_maintain_cost(run->spell, obj, &period);
        cost = utility_ai_fatigue_payment(cost,
            (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & OCF_FATIGUE_LIMITING) != 0);
        first_payment = ((int64_t)run->event_time.days - now.days) * 86400
            + (int64_t)run->event_time.milliseconds / 1000 - now.milliseconds / 1000;
        total += utility_ai_maintenance_cost(cost, (int64_t)period * 8, first_payment, horizon_seconds);
        if (total >= 1000000) return 1000000;
    }
    return total;
}

// 0x450A50
int64_t magictech_inventory_owner_or_self(int64_t obj)
{
    int type;

    if (obj == OBJ_HANDLE_NULL) {
        return OBJ_HANDLE_NULL;
    }

    type = obj_field_int32_get(obj, OBJ_F_TYPE);
    if (type != OBJ_TYPE_PC
        && type != OBJ_TYPE_NPC
        && type >= OBJ_TYPE_WEAPON
        && type <= OBJ_TYPE_GENERIC
        && (obj_field_int32_get(obj, OBJ_F_FLAGS) & OF_INVENTORY) != 0) {
        return obj_field_handle_get(obj, OBJ_F_ITEM_PARENT);
    }

    return obj;
}

// 0x450AC0
int magictech_maintained_spell_count_stub(int64_t obj)
{
    int index;

    if (obj == OBJ_HANDLE_NULL) {
        return 0;
    }

    if (!obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))) {
        return 0;
    }

    for (index = 0; index < 512; index++) {
        // NOTE: Unclear, likely no-op.
    }

    return 0;
}

// 0x450B40
int magictech_maintenance_slots_available(int64_t obj)
{
    if (obj == OBJ_HANDLE_NULL) {
        return 0;
    }

    if (!obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))) {
        return 0;
    }

    return magictech_maintenance_slots_capacity(obj) - magictech_maintained_spell_count_stub(obj);
}

// 0x450B90
int magictech_maintenance_slots_capacity(int64_t obj)
{
    return stat_level_get(obj, STAT_INTELLIGENCE) / 4;
}

// 0x450C10
void magictech_spell_flags_set(int64_t obj, unsigned int flags)
{
    obj_field_int32_set(obj, OBJ_F_SPELL_FLAGS, obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) | flags);
}

// 0x450C40
void magictech_effect_summon(MagicTechSummonInfo* summon_info)
{
    int64_t proto_obj;
    int64_t obj;

    proto_obj = obj_pool_perm_lookup(summon_info->prototype_id);

    if (tig_net_is_active()) {
        if (tig_net_is_host()) {
            PacketSummon pkt;

            if (!object_create(proto_obj, summon_info->loc, &obj)) {
                tig_debug_printf("magictech_effect_summon: Error: object create failed!\n");
                exit(EXIT_FAILURE);
            }

            pkt.type = 73;
            pkt.summon_info = *summon_info;
            pkt.summon_info.summoned_object_id = obj_get_id(obj);
            object_reference_init(pkt.summon_info.summoner.obj, &(pkt.summon_info.summoner.reference));
            object_reference_init(pkt.summon_info.target.obj, &(pkt.summon_info.target.reference));
            summon_info->source_swap_obj = magictech_source_swap_obj;
            mp_object_id_get_nullable(magictech_source_swap_obj, &(pkt.summon_info.source_swap_id));
            tig_net_send_app_all(&pkt, sizeof(pkt));

            if (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_SCENERY) {
                anim_object_interrupt_by_priority(obj, 5, false, false);
                object_scenery_animation_refresh(obj);
            }
        } else {
            if (!multiplayer_is_locked()) {
                tig_debug_println("MP: MagicTech: magictech_effect_summon called without MP set correctly, tell smoret@troikagames.com");
                return;
            }

            object_create_ex(proto_obj, summon_info->loc, summon_info->summoned_object_id, &obj);
        }
    } else {
        if (!object_create(proto_obj, summon_info->loc, &obj)) {
            tig_debug_printf("magictech_effect_summon: Error: object create failed!\n");
            exit(EXIT_FAILURE);
        }
    }

    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    *summon_info->summoned_obj_ptr = obj;

    magictech_spell_flags_set(obj, OSF_SUMMONED);

    if ((obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_NPC)) {
        tig_art_id_t art_id;
        int64_t summoner_loc;
        int rot;

        // CE: Summoned monsters can have specific palette which may differ
        // from the prototype. This palette must be considered the base art ID
        // so that temporary changes (during transformations and polymorping)
        // can be reversible.
        art_id = obj_field_int32_get(obj, OBJ_F_AID);
        art_id = tig_art_id_palette_set(art_id, summon_info->palette);
        obj_field_int32_set(obj, OBJ_F_AID, art_id);

        summoner_loc = obj_field_int64_get(summon_info->summoner.obj, OBJ_F_LOCATION);
        rot = location_rot(summon_info->loc, summoner_loc);

        art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
        art_id = tig_art_id_rotation_set(art_id, rot);
        art_id = tig_art_id_palette_set(art_id, summon_info->palette);
        object_set_current_aid(obj, art_id);

        critter_set_concealed(obj, true);
        object_flags_set(obj, OF_DONTDRAW);

        obj_field_int32_set(obj,
            OBJ_F_CRITTER_FLAGS2,
            obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2) | OCF2_NO_PICKPOCKET);

        obj_field_int32_set(obj, OBJ_F_NPC_EXPERIENCE_WORTH, 0);
        obj_field_int32_set(obj, OBJ_F_NPC_EXPERIENCE_POOL, 0);

        if (summon_info->prototype_list) {
            obj_field_int32_set(obj, OBJ_F_NPC_FACTION, 0);
        } else {
            if ((!tig_net_is_active()
                    || tig_net_is_host())
                && magictech_cur_run_info->parent_obj.obj != OBJ_HANDLE_NULL) {
                stat_base_set(obj,
                    STAT_ALIGNMENT,
                    stat_level_get(magictech_cur_run_info->parent_obj.obj, STAT_ALIGNMENT));
            }

            if (summon_info->summoner.obj != OBJ_HANDLE_NULL
                && obj_field_int32_get(summon_info->summoner.obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
                obj_field_int32_set(obj,
                    OBJ_F_NPC_FACTION,
                    obj_field_int32_get(summon_info->summoner.obj, OBJ_F_NPC_FACTION));
            }
        }
    } else {
        anim_root_goal_parent_set_legacy(obj, summon_info->summoner.obj);
    }

    item_inventory_mark_summoned(obj);
}

// 0x451070
void magictech_run_process(MagicTechRunInfo* run_info)
{
    if (magictech_cur_id != -1 && magictech_cur_id != run_info->id) {
        tig_debug_printf("\n\nMagicTech: ERROR: Process function is NOT Re-Entrant, Spell: %d (%s)!\n",
            run_info->spell,
            magictech_get_name(run_info->spell));
        return;
    }

    magictech_cur_run_info = run_info;
    magictech_cur_target_obj_type = -1;
    magictech_waiting_for_effect_callback = 0;
    magictech_effect_callback_pending = 0;
    magictech_effect_end_callback_pending = 0;
    magictech_current_run_had_effect = 0;
    magictech_last_aggression_notified_obj = 0;
    magictech_initial_processing_action = run_info->action;
    magictech_process();
}

// 0x4510F0
void magictech_process(void)
{
    int idx;
    int comp;

    if (magictech_cur_run_info->id == -1) {
        return;
    }

    if (tig_net_is_active() && !tig_net_is_host()) {
        return;
    }

    magictech_cur_spell_info = &(magictech_spells[magictech_cur_run_info->spell]);
    magictech_cur_resistance = &(magictech_cur_spell_info->resistance);
    magictech_cur_component_list = &(magictech_cur_spell_info->components[magictech_cur_run_info->action]);
    magictech_cur_run_info->flags |= MAGICTECH_RUN_PROCESSING_STARTED;
    magictech_cur_id = magictech_cur_run_info->id;

    if (magictech_cur_run_info->action == MAGICTECH_ACTION_BEGIN
        && !magictech_disallowed_flags_check(magictech_cur_run_info->parent_obj.obj, magictech_cur_run_info->target_obj.obj, magictech_cur_spell_info)) {
        magictech_cur_id = -1;
        if (magictech_cur_run_info->action < MAGICTECH_ACTION_END) {
            magictech_interrupt_delayed(magictech_cur_run_info->id);
        }
        return;
    }

    if (!magictech_current_run_costs_and_stacking_check()) {
        magictech_cur_id = -1;
        if (magictech_cur_run_info->action < MAGICTECH_ACTION_END) {
            magictech_interrupt_delayed(magictech_cur_run_info->id);
        }
        return;
    }

    magictech_current_target_context_init();

    if (!magictech_source_antimagic_check()) {
        magictech_cur_id = -1;
        if (magictech_cur_run_info->action < MAGICTECH_ACTION_END) {
            magictech_interrupt_delayed(magictech_cur_run_info->id);
        }
        return;
    }

    if (magictech_cur_component_list->cnt > 0) {
        magictech_current_target_context.params = &(magictech_cur_spell_info->target_params[magictech_cur_run_info->action]);
        target_context_build_list(&magictech_current_target_context);

        if (magictech_cur_run_info->source_obj.obj != OBJ_HANDLE_NULL
            && obj_field_int32_get(magictech_cur_run_info->source_obj.obj, OBJ_F_TYPE) == OBJ_TYPE_PC
            && fate_resolve(magictech_cur_run_info->source_obj.obj, FATE_SPELL_AT_MAXIMUM)) {
            magictech_cur_run_info->source_obj.aptitude = 100;
            magictech_cur_run_info->parent_obj.aptitude = 100;
            magictech_cur_is_fate_maximized = true;
        } else {
            magictech_cur_is_fate_maximized = false;
        }

        if (magictech_current_target_list.cnt == 0) {
            magictech_current_run_had_effect = 1;
        }

        for (idx = 0; idx < magictech_current_target_list.cnt; idx++) {
            magictech_current_target_context.target_obj = magictech_current_target_list.entries[idx].obj;
            magictech_current_target_context.target_loc = magictech_current_target_list.entries[idx].loc;
            if (magictech_current_target_context.target_obj != OBJ_HANDLE_NULL) {
                magictech_cur_target_obj_type = obj_field_int32_get(magictech_current_target_context.target_obj, OBJ_F_TYPE);
            }

            if (magictech_target_antimagic_and_reflection_resolve(&magictech_current_target_context, magictech_cur_run_info)) {
                if (magictech_current_target_resistance_resolve()) {
                    for (comp = 0; comp < magictech_cur_component_list->cnt; comp++) {
                        magictech_cur_component = &(magictech_cur_component_list->entries[comp]);
                        magictech_current_target_context.params = &(magictech_cur_component->aoe);

                        if (target_context_evaluate(&magictech_current_target_context)) {
                            magictech_component_target_prepare();

                            if (magictech_cur_spell_info->item_triggers == 0
                                || magictech_cur_component->item_triggers == 0
                                || ((magictech_cur_run_info->trigger & magictech_cur_component->item_triggers) != 0
                                    && ((magictech_cur_component->item_triggers & 0x100) == 0
                                        || (magictech_cur_run_info->trigger & 0x100) != 0))) {
                                magictech_current_run_had_effect = 1;
                                magictech_aggressive_target_notify();
                                magictech_procs[magictech_cur_component->type]();

                                if (magictech_branch_pending) {
                                    comp = magictech_branch_destination;
                                    magictech_branch_pending = false;
                                }
                            }
                        }
                    }

                    if (magictech_cur_run_info->source_obj.obj != magictech_original_source_obj) {
                        magictech_source_swap_obj = magictech_cur_run_info->source_obj.obj;
                        magictech_cur_run_info->source_obj.obj = magictech_current_target_context.target_obj;
                        magictech_current_target_context.target_obj = magictech_source_swap_obj;
                        if (magictech_source_swap_obj != OBJ_HANDLE_NULL) {
                            magictech_cur_target_obj_type = obj_field_int32_get(magictech_source_swap_obj, OBJ_F_TYPE);
                        }
                    }
                } else {
                    magictech_resurrection_fx_notify(magictech_cur_run_info->parent_obj.obj, magictech_current_target_context.target_obj);
                }
            }
        }
    } else {
        magictech_current_run_had_effect = 1;
    }

    magictech_cur_id = -1;
    magictech_current_run_advance_or_finish();
}

// 0x4514E0
void magictech_component_animation_goal_process(void)
{
    int64_t loc;
    int64_t new_loc;
    AnimGoalData goal_data;
    tig_art_id_t art_id;
    int rot;

    switch (magictech_cur_component->data.agoal.goal) {
    case AG_KNOCKBACK:
        if (magictech_cur_run_info->parent_obj.obj != OBJ_HANDLE_NULL) {
            loc = obj_field_int64_get(magictech_cur_run_info->parent_obj.obj, OBJ_F_LOCATION);
        } else {
            loc = magictech_current_target_context.source_loc;
        }

        if (loc != 0) {
            new_loc = obj_field_int64_get(magictech_current_target_context.target_obj, OBJ_F_LOCATION);
            if (loc == new_loc) {
                art_id = obj_field_int32_get(magictech_current_target_context.target_obj, OBJ_F_CURRENT_AID);
                rot = (tig_art_id_rotation_get(art_id) + 4) % 8;
            } else {
                rot = location_rot(loc, new_loc);
            }

            anim_goal_knockback(magictech_current_target_context.target_obj, rot, 4, magictech_cur_run_info->parent_obj.obj);
        }
        break;
    case AG_FOLLOW:
        if (magictech_cur_run_info->parent_obj.obj != OBJ_HANDLE_NULL) {
            critter_follow(magictech_source_swap_obj, magictech_cur_run_info->parent_obj.obj, true);
        }
        break;
    case AG_KNOCK_DOWN:
        anim_goal_knockdown(magictech_current_target_context.target_obj);
        break;
    default:
        if (magictech_cur_run_info->parent_obj.obj != OBJ_HANDLE_NULL
            && anim_goal_data_init_without_interrupt(&goal_data, magictech_current_target_context.target_obj, magictech_cur_component->data.agoal.goal)) {
            goal_data.params[AGDATA_TARGET_OBJ].obj = magictech_cur_run_info->parent_obj.obj;
            if ((magictech_cur_component->data.agoal.subgoal & 1) != 0) {
                // __FILE__: "C:\Troika\Code\Game\GameLibX\MagicTech.c"
                // __LINE__: 3277
                anim_subgoal_add(magictech_component_animation_id, &goal_data, __FILE__, __LINE__);
            } else {
                anim_goal_add(&goal_data, &magictech_component_animation_id);
            }
        }
        break;
    }
}

// 0x4516D0
void magictech_component_animation_goal_terminate_process(void)
{
    if (magictech_current_target_context.target_obj != OBJ_HANDLE_NULL) {
        anim_interrupt_all_goals_of_type(magictech_current_target_context.target_obj, magictech_cur_component->data.agoal_terminate.goal, -1);
    }
}

// 0x451700
void magictech_component_ai_redirect_process(void)
{
    AiRedirect ai_redirect;

    ai_redirect_init(&ai_redirect, magictech_current_target_context.target_obj, magictech_current_target_context.target_obj);
    ai_redirect.critter_flags = magictech_cur_component->data.ai_redirect.critter_flags;
    ai_redirect.min_iq = magictech_cur_component->data.ai_redirect.min_iq;
    ai_redirect_perform(&ai_redirect);
}

// 0x451740
void magictech_component_cast_process(void)
{
    MagicTechInvocation mt_invocation;

    magictech_invocation_init(&mt_invocation, OBJ_HANDLE_NULL, magictech_cur_component->data.cast.spell);
    object_handle_reference_init(magictech_current_target_context.target_obj, &(mt_invocation.target_obj));
    object_handle_reference_init(magictech_cur_run_info->parent_obj.obj, &(mt_invocation.parent_obj));
    mt_invocation.flags |= MAGICTECH_INVOCATION_FRIENDLY;
    mt_invocation.target_loc = magictech_current_target_context.target_loc;

    if (mt_invocation.parent_obj.obj != OBJ_HANDLE_NULL) {
        mt_invocation.loc = obj_field_int64_get(mt_invocation.parent_obj.obj, OBJ_F_LOCATION);
    }

    if (mt_invocation.target_loc == 0) {
        if (mt_invocation.target_obj.obj == OBJ_HANDLE_NULL) {
            return;
        }

        mt_invocation.target_loc = obj_field_int64_get(mt_invocation.target_obj.obj, OBJ_F_LOCATION);
    }

    if (mt_invocation.target_obj.obj != OBJ_HANDLE_NULL || mt_invocation.target_loc != 0) {
        magictech_invocation_run(&mt_invocation);
    }
}

// 0x451850
void magictech_component_charge_and_branch_process(void)
{
    if (magictech_cur_run_info->source_obj.obj != OBJ_HANDLE_NULL
        && !magictech_cost_check_or_consume(magictech_cur_run_info->source_obj.obj, magictech_cur_component->data.charge_branch.cost, true, magictech_cur_run_info->spell)) {
        if (magictech_cur_component->data.charge_branch.branch != -1) {
            magictech_branch_pending = true;
            magictech_branch_destination = SDL_min(magictech_cur_component->data.charge_branch.branch, magictech_cur_component_list->cnt);
        } else {
            magictech_branch_pending = true;
            magictech_branch_destination = magictech_cur_component_list->cnt;
        }
    }
}

// 0x4518D0
void magictech_component_damage_process(void)
{
    CombatContext combat;
    int dam_min;
    int dam_max;

    if (magictech_current_target_context.target_obj == OBJ_HANDLE_NULL) {
        return;
    }

    combat_context_init(magictech_cur_run_info->parent_obj.obj, magictech_current_target_context.target_obj, &combat);

    combat.dam_flags |= magictech_cur_component->data.damage.damage_flags;

    magictech_aggressive_target_notify_once(magictech_cur_run_info->parent_obj.obj, magictech_current_target_context.target_obj);

    if (magictech_cur_run_info->attacker_obj.obj != OBJ_HANDLE_NULL) {
        combat.credited_attacker_obj = magictech_cur_run_info->attacker_obj.obj;
    }

    dam_min = magictech_cur_component->data.damage.damage_min;
    dam_max = magictech_cur_component->data.damage.damage_max;

    if ((combat.dam_flags & COMBAT_DAMAGE_SCALE) != 0) {
        if (obj_type_is_critter(magictech_cur_run_info->parent_obj.type)) {
            int aptitude = magictech_cur_run_info->parent_obj.aptitude;
            if (aptitude > 0) {
                dam_max = dam_min + aptitude * (dam_max - dam_min) / 100;
                dam_min = dam_max;
            } else {
                dam_max = dam_min;
            }
        }

        combat.dam_flags &= ~COMBAT_DAMAGE_SCALE;
    }

    if ((combat.dam_flags & COMBAT_DAMAGE_FULL) != 0) {
        if (magictech_cur_component->data.damage.damage_type == DAMAGE_TYPE_FATIGUE) {
            combat.dam[DAMAGE_TYPE_FATIGUE] = critter_fatigue_current(magictech_current_target_context.target_obj) + 10;
            combat.dam_flags &= ~COMBAT_DAMAGE_FULL;
        }
    } else {
        if (magictech_cur_component->data.damage.damage_type < DAMAGE_TYPE_COUNT) {
            combat.dam[magictech_cur_component->data.damage.damage_type] = random_between(dam_min, dam_max);
        } else {
            combat.dam[DAMAGE_TYPE_NORMAL] = random_between(dam_min, dam_max);
        }
    }

    combat.flags |= 0x200 | 0x80;

    if (magictech_cur_component->data.damage.damage_type < DAMAGE_TYPE_COUNT) {
        if (magictech_cur_run_info->resistance_amount != 0) {
            if ((combat.dam_flags & COMBAT_DAMAGE_DEATH) == 0) {
                int resisted = magictech_cur_run_info->resistance_amount * combat.dam[magictech_cur_component->data.damage.damage_type] / 100;
                if (resisted == 0) {
                    resisted = 1;
                }
                combat.dam[magictech_cur_component->data.damage.damage_type] -= resisted;
                combat_dmg(&combat);
            }
        } else {
            combat_dmg(&combat);
        }
    } else if (magictech_cur_component->data.damage.damage_type == DAMAGE_TYPE_COUNT) {
        combat_acid_dmg(&combat);
    }

    magictech_damage_target_object_flags = obj_field_int32_get(combat.target_obj, OBJ_F_FLAGS);

    if ((magictech_damage_target_object_flags & (OF_DESTROYED | OF_OFF)) == 0
        && (combat.dam_flags & COMBAT_DAMAGE_HAVE_DAMAGE) != 0) {
        anim_play_blood_splotch_fx(combat.target_obj, BLOOD_SPLOTCH_TYPE_NORMAL, magictech_cur_component->data.damage.damage_type, &combat);
    }
}

// 0x451AF0
void magictech_component_destroy_process(void)
{
    unsigned int spell_flags;

    if (magictech_current_target_context.target_obj == OBJ_HANDLE_NULL) {
        return;
    }

    spell_flags = obj_field_int32_get(magictech_current_target_context.target_obj, OBJ_F_SPELL_FLAGS);
    if (!magictech_cur_run_info->resistance_amount || (spell_flags & OSF_SUMMONED) != 0) {
        object_destroy(magictech_current_target_context.target_obj);

        if (tig_net_is_active()
            && tig_net_is_host()) {
            PacketObjectDestroy pkt;

            pkt.type = 72;
            pkt.oid = obj_get_id(magictech_current_target_context.target_obj);
            tig_net_send_app_all(&pkt, sizeof(pkt));
        }
    }
}

// 0x451B90
void magictech_component_dispel_process(void)
{
    magictech_component_dispel(magictech_current_target_context.target_obj, magictech_cur_run_info->id);
}

// 0x451BB0
void magictech_component_dispel(int64_t obj, int mt_id)
{
    if (!multiplayer_is_locked()) {
        Packet74 pkt;

        if (!tig_net_is_host()) {
            return;
        }

        pkt.type = 74;
        pkt.subtype = 1;
        pkt.mt_id = mt_id;
        if (obj != OBJ_HANDLE_NULL) {
            pkt.oid = obj_get_id(obj);
        } else {
            pkt.oid.type = OID_TYPE_NULL;
        }
        tig_net_send_app_all(&pkt, sizeof(pkt));
    }

    if (obj != OBJ_HANDLE_NULL) {
        magictech_component_dispel_internal(mt_id, obj);
    }
}

// 0x451C40
void magictech_component_dispel_internal(int mt_id, int64_t obj)
{
    int obj_type;
    int index;
    MagicTechRunInfo* run_info;
    MagicTechObjectNode* node;
    unsigned int flags;

    if (mt_id == -1) {
        return;
    }

    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    obj_type = obj_field_int32_get(obj, OBJ_F_TYPE);

    for (index = 0; index < 512; index++) {
        run_info = &(magictech_run_info[index]);
        if (run_info->source_obj.obj == obj
            && run_info->id != mt_id
            && (magictech_spells[run_info->spell].flags & MAGICTECH_IS_TECH) == 0) {
            run_info->flags |= MAGICTECH_RUN_DISPELLED;
            magictech_interrupt_delayed(run_info->id);
        }
    }

    for (index = 0; index < 512; index++) {
        run_info = &(magictech_run_info[index]);
        if ((run_info->flags & MAGICTECH_RUN_ACTIVE) != 0) {
            if (run_info->target_obj.obj == obj
                && index != mt_id
                && (magictech_spells[run_info->spell].flags & MAGICTECH_IS_TECH) == 0) {
                run_info->flags |= MAGICTECH_RUN_DISPELLED;
                magictech_interrupt_delayed(run_info->id);
            }

            node = run_info->summoned_obj;
            while (node != NULL) {
                if (node->obj == obj
                    && index != mt_id
                    && (magictech_spells[run_info->spell].flags & MAGICTECH_IS_TECH) == 0) {
                    run_info->flags |= MAGICTECH_RUN_DISPELLED;
                    magictech_interrupt_delayed(run_info->id);
                }
                node = node->next;
            }

            node = run_info->objlist;
            while (node != NULL) {
                if (node->obj == obj
                    && index != mt_id
                    && (magictech_spells[run_info->spell].flags & MAGICTECH_IS_TECH) == 0) {
                    run_info->flags |= MAGICTECH_RUN_DISPELLED;
                    magictech_interrupt_delayed(run_info->id);
                }
                node = node->next;
            }
        }
    }

    switch (obj_type) {
    case OBJ_TYPE_PORTAL:
        flags = obj_field_int32_get(obj, OBJ_F_PORTAL_FLAGS);
        if ((flags & OPF_MAGICALLY_HELD) != 0) {
            flags &= ~OPF_MAGICALLY_HELD;
            obj_field_int32_set(obj, OBJ_F_PORTAL_FLAGS, flags);
        }
        break;
    case OBJ_TYPE_CONTAINER:
        flags = obj_field_int32_get(obj, OBJ_F_CONTAINER_FLAGS);
        if ((flags & OCOF_MAGICALLY_HELD) != 0) {
            flags &= ~OCOF_MAGICALLY_HELD;
            obj_field_int32_set(obj, OBJ_F_CONTAINER_FLAGS, flags);
        }
        break;
    case OBJ_TYPE_PC:
    case OBJ_TYPE_NPC:
        flags = obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS);
        if ((flags & OSF_STONED) != 0) {
            flags &= ~OSF_STONED;
            obj_field_int32_set(obj, OBJ_F_SPELL_FLAGS, flags);
        }
        if ((flags & OSF_SUMMONED) != 0) {
            object_destroy(obj);
        }

        flags = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2);
        if ((flags & OCF2_PERMA_POLYMORPH) != 0) {
            magictech_polymorph_art_apply(obj, obj_field_int32_get(obj, OBJ_F_CURRENT_AID));

            flags &= ~OCF2_PERMA_POLYMORPH;
            obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS2, flags);
            effect_remove_one_typed(obj, EFFECT_POLYMORPH);
        }
        break;
    }
}

// 0x451F20
void magictech_component_effect_process(void)
{
    int scale;
    int cnt;

    if (magictech_current_target_context.target_obj == OBJ_HANDLE_NULL) {
        return;
    }

    scale = magictech_cur_component->data.effect.scaled;
    cnt = magictech_cur_component->data.effect.count;

    if (scale > 0) {
        if (obj_type_is_critter(magictech_cur_run_info->parent_obj.type)) {
            cnt += scale * magictech_cur_run_info->parent_obj.aptitude / 100;
        }
    }

    if (magictech_cur_component->data.effect.add_remove == 0) {
        while (cnt > 0) {
            effect_remove_one_typed(magictech_current_target_context.target_obj, magictech_cur_component->data.effect.num);
            cnt--;
        }
    } else {
        while (cnt > 0) {
            effect_add(magictech_current_target_context.target_obj,
                magictech_cur_component->data.effect.num,
                magictech_cur_component->data.effect.cause);
            cnt--;
        }
    }
}

// 0x451FE0
void magictech_component_eye_candy_process(void)
{
    int mt_id;

    if (magictech_current_target_context.target_obj == OBJ_HANDLE_NULL) {
        return;
    }

    if (magictech_cur_component->data.eye_candy.add_remove == 0) {
        if (!tig_net_is_active()
            || tig_net_is_host()) {
            mt_id = (magictech_cur_component->data.eye_candy.flags & 0x100) == 0
                ? magictech_cur_run_info->id
                : -1;
            animfx_remove(&spell_eye_candies,
                magictech_current_target_context.target_obj,
                magictech_cur_component->data.eye_candy.num + 6 * magictech_cur_run_info->spell,
                mt_id);

            if (tig_net_is_active()) {
                PacketMagicTechEyeCandy pkt;

                pkt.type = 77;
                pkt.subtype = 0;
                pkt.oid = obj_get_id(magictech_current_target_context.target_obj);
                pkt.fx_id = magictech_cur_component->data.eye_candy.num + 6 * magictech_cur_run_info->spell;
                pkt.mt_id = mt_id;
                tig_net_send_app_all(&pkt, sizeof(pkt));
            }
        }
    } else {
        AnimFxNode node;

        animfx_node_init(&spell_eye_candies,
            &node,
            magictech_current_target_context.target_obj,
            magictech_cur_run_info->id,
            magictech_cur_component->data.eye_candy.num + 6 * magictech_cur_run_info->spell);

        if (magictech_current_target_context.target_obj != OBJ_HANDLE_NULL) {
            node.rotation = tig_art_id_rotation_get(obj_field_int32_get(magictech_current_target_context.target_obj, OBJ_F_CURRENT_AID));
        }

        node.animate = true;
        node.flags = magictech_cur_component->data.eye_candy.flags;
        node.parent_obj = magictech_cur_run_info->parent_obj.obj;

        if ((magictech_cur_run_info->flags & MAGICTECH_RUN_END_IMMEDIATE) == 0) {
            if (animfx_add(&node)) {
                if ((node.flags & (ANIMFX_PLAY_CALLBACK | ANIMFX_PLAY_END_CALLBACK)) != 0) {
                    magictech_waiting_for_effect_callback = 1;
                }
                return;
            }
        }

        if ((node.flags & (ANIMFX_PLAY_CALLBACK | ANIMFX_PLAY_END_CALLBACK)) != 0) {
            magictech_effect_callback_pending = 1;
            if ((node.flags & ANIMFX_PLAY_END_CALLBACK) != 0) {
                magictech_effect_end_callback_pending = 1;
            }
        }
    }
}

// 0x4521A0
void magictech_component_heal_process(void)
{
    CombatContext combat;
    int heal_min;
    int heal_max;

    if (magictech_current_target_context.target_obj != OBJ_HANDLE_NULL) {
        combat_context_init(magictech_cur_run_info->parent_obj.obj, magictech_current_target_context.target_obj, &combat);
        combat.dam_flags |= magictech_cur_component->data.heal.damage_flags;
        if ((combat.dam_flags & COMBAT_DAMAGE_FULL) == 0) {
            heal_min = magictech_cur_component->data.heal.damage_min;
            heal_max = magictech_cur_component->data.heal.damage_max;
            if ((combat.dam_flags & COMBAT_DAMAGE_SCALE) != 0
                && obj_type_is_critter(magictech_cur_run_info->parent_obj.type)) {
                if (magictech_cur_run_info->parent_obj.aptitude > 0) {
                    heal_max = heal_min + magictech_cur_run_info->parent_obj.aptitude * (heal_max - heal_min) / 100;
                    heal_min = heal_max;
                } else {
                    heal_max = heal_min;
                }
                combat.dam_flags &= ~COMBAT_DAMAGE_SCALE;
            }

            if (magictech_cur_component->data.heal.damage_type == DAMAGE_TYPE_POISON) {
                combat.dam[DAMAGE_TYPE_POISON] = random_between(heal_min, heal_max);
            } else if (magictech_cur_component->data.heal.damage_type == DAMAGE_TYPE_FATIGUE) {
                combat.dam[DAMAGE_TYPE_FATIGUE] = random_between(heal_min, heal_max);
            } else {
                combat.dam[DAMAGE_TYPE_NORMAL] = random_between(heal_min, heal_max);
            }
        }

        combat_heal(&combat);
    }
}

// 0x4522A0
void magictech_component_identify_process(void)
{
    if (magictech_current_target_context.target_obj != OBJ_HANDLE_NULL) {
        if (obj_type_is_critter(magictech_cur_target_obj_type)) {
            mp_ui_character_sheet_request(magictech_cur_run_info->parent_obj.obj, magictech_current_target_context.target_obj);
        } else if (magictech_cur_target_obj_type == OBJ_TYPE_CONTAINER) {
            mp_ui_show_inven_identify(magictech_cur_run_info->parent_obj.obj, magictech_current_target_context.target_obj);
        } else {
            mp_ui_object_examine_request(magictech_cur_run_info->parent_obj.obj, magictech_current_target_context.target_obj);
        }
    }
}

// 0x452300
void magictech_component_interrupt_process(void)
{
    MagicTechInvocation mt_invocation;

    magictech_invocation_init(&mt_invocation, OBJ_HANDLE_NULL, magictech_cur_component->data.interrupt.magictech);
    object_handle_reference_init(magictech_current_target_context.target_obj, &(mt_invocation.target_obj));
    mt_invocation.target_loc = magictech_current_target_context.target_loc;
    if (mt_invocation.target_obj.obj != OBJ_HANDLE_NULL || mt_invocation.target_loc != OBJ_HANDLE_NULL) {
        magictech_matching_invocation_interrupt(&mt_invocation);
    }
}

// 0x452380
void magictech_component_object_flag_process(void)
{
    if (magictech_cur_component->data.obj_flag.state == 1) {
        magictech_cur_run_info->applied_spell_flags |= magictech_cur_component->data.obj_flag.value;
    } else {
        magictech_cur_run_info->applied_spell_flags &= ~magictech_cur_component->data.obj_flag.value;
    }

    magictech_component_obj_flag(magictech_current_target_context.target_obj,
        magictech_current_target_context.self_obj,
        magictech_cur_component->data.obj_flag.flags_fld,
        magictech_cur_component->data.obj_flag.value,
        magictech_cur_component->data.obj_flag.state,
        magictech_cur_run_info->parent_obj.obj,
        magictech_cur_run_info->source_obj.obj);

    if (tig_net_is_active()
        && tig_net_is_host()) {
        PacketMagicTechObjFlag pkt;

        mp_object_id_get_nullable(magictech_current_target_context.target_obj, &(pkt.target_id));
        mp_object_id_get_nullable(magictech_current_target_context.self_obj, &(pkt.self_oid));
        pkt.fld = magictech_cur_component->data.obj_flag.flags_fld;
        pkt.value = magictech_cur_component->data.obj_flag.value;
        pkt.state = magictech_cur_component->data.obj_flag.state;
        mp_object_id_get_nullable(magictech_cur_run_info->parent_obj.obj, &(pkt.parent_oid));
        mp_object_id_get_nullable(magictech_cur_run_info->source_obj.obj, &(pkt.source_oid));

        tig_net_send_app_all(&pkt, sizeof(pkt));
    }
}

// 0x4524C0
void magictech_component_movement_process(void)
{
    int64_t loc = 0;

    if (magictech_current_target_context.target_obj != OBJ_HANDLE_NULL
        && !magictech_cur_run_info->resistance_amount) {
        switch (magictech_cur_component->data.movement.move_location) {
        case 0:
            loc = magictech_cur_run_info->target_obj.loc;
            break;
        case 1:
            target_find_displacement_loc(magictech_cur_run_info->target_obj.obj, magictech_cur_component->data.movement.tile_radius, &loc);
            break;
        case 2:
            if (antiteleport_check(magictech_cur_run_info->parent_obj.obj, 0)) {
                if (tig_net_is_active()) {
                    TeleportData teleport_data;

                    map_starting_loc_get(&loc);

                    teleport_data.loc = loc;
                    teleport_data.map = map_current_map();
                    teleport_data.flags = TELEPORT_RENDER_LOCK | TELEPORT_SKIP_FOLLOWERS;
                    teleport_data.obj = magictech_current_target_context.target_obj;
                    teleport_do(&teleport_data);
                    return;
                }

                loc = 0;
                ui_wmap_select(magictech_cur_run_info->source_obj.obj, magictech_cur_run_info->spell);
            } else {
                MesFileEntry mes_file_entry;

                mes_file_entry.num = 10000; // "This place seems to block your attempt to teleport."
                mes_get_msg(magictech_spell_mes_file, &mes_file_entry);
                ui_display_warning(mes_file_entry.str);
            }
            break;
        case 3:
            magictech_exit_to_world_area(magictech_current_target_context.target_obj);
            break;
        }

        if (loc != 0) {
            magictech_move_object_along_path(magictech_current_target_context.target_obj, loc);
        }
    }
}

// 0x452650
void magictech_exit_to_world_area(int64_t obj)
{
    int area = AREA_UNKNOWN;
    int cur_map;
    int start_map;
    int64_t loc;
    MesFileEntry mes_file_entry;
    TeleportData teleport_data;

    cur_map = map_current_map();
    start_map = map_by_type(MAP_TYPE_START_MAP);

    if (cur_map == start_map) {
        area = area_get_nearest_area_in_range(obj_field_int64_get(obj, OBJ_F_LOCATION), true);
    } else {
        if (!tig_net_is_active()) {
            map_get_area(cur_map, &area);
        }
    }

    if (area > 0 && antiteleport_check(magictech_current_target_context.target_obj, 0)) {
        loc = area_get_location(area);

        if (tig_net_is_active()) {
            sector_flush(0);

            teleport_data.loc = loc;
            teleport_data.map = map_current_map();
            teleport_data.flags = TELEPORT_SKIP_FOLLOWERS | TELEPORT_RENDER_LOCK;
            teleport_data.obj = obj;
            teleport_do(&teleport_data);
        } else {
            sector_flush(0);

            teleport_data.loc = loc;
            teleport_data.map = map_by_type(MAP_TYPE_START_MAP);
            teleport_data.flags = TELEPORT_SKIP_FOLLOWERS | TELEPORT_RENDER_LOCK;
            teleport_data.obj = obj;
            teleport_do(&teleport_data);
        }
    } else {
        if (player_is_local_pc_obj(obj)) {
            mes_file_entry.num = 10000; // "This place seems to block your attempt to teleport."
            mes_get_msg(magictech_spell_mes_file, &mes_file_entry);
            ui_display_warning(mes_file_entry.str);
        }
    }
}

// 0x452800
void magictech_component_recharge_process(void)
{
    magictech_component_recharge(magictech_current_target_context.target_obj,
        magictech_cur_component->data.recharge.num,
        magictech_cur_component->data.recharge.max);
}

// 0x452830
void magictech_component_summon_process(void)
{
    MagicTechSummonInfo summon_info;

    summon_info.summoner.obj = magictech_cur_run_info->parent_obj.obj;
    summon_info.target.obj = magictech_cur_run_info->target_obj.obj;
    summon_info.loc = magictech_current_target_context.target_loc;
    summon_info.summoned_obj_ptr = &magictech_source_swap_obj;
    summon_info.palette = magictech_cur_component->data.summon.palette;
    summon_info.prototype_list = magictech_cur_component->data.summon.list;

    if (magictech_cur_component->data.summon.oid.type != OID_TYPE_NULL) {
        summon_info.prototype_id = magictech_cur_component->data.summon.oid;
    } else {
        magictech_pick_proto_from_list(&(summon_info.prototype_id), magictech_cur_component->data.summon.list);
    }

    magictech_effect_summon(&summon_info);

    magictech_current_target_context.summoned_obj = magictech_source_swap_obj;
    magictech_summoned_object_add(magictech_cur_run_info, magictech_source_swap_obj);
}

// 0x452900
void magictech_pick_proto_from_list(ObjectID* oid, int list)
{
    int rnd;
    int idx;

    switch (magictech_summon_tables[list].type) {
    case 0:
        rnd = random_between(0, 100);
        for (idx = 0; idx < magictech_summon_tables[list].cnt; idx++) {
            if (rnd < magictech_summon_tables[list].entries[idx].value) {
                break;
            }

            rnd -= magictech_summon_tables[list].entries[idx].value;
        }
        if (idx >= magictech_summon_tables[list].cnt) {
            tig_debug_printf("MagicTech: Error: magictech_pick_proto_from_list: went off end of list!\n");
            idx = 0;
        }
        break;
    case 1:
        for (idx = 0; idx < magictech_summon_tables[list].cnt; idx++) {
            if (magictech_cur_run_info->parent_obj.aptitude <= magictech_summon_tables[list].entries[idx].value) {
                break;
            }
        }
        break;
    }

    *oid = obj_get_id(proto_handle_get(magictech_summon_tables[list].entries[idx].basic_prototype));
}

// 0x4529D0
void magictech_component_terminate_process(void)
{
    magictech_branch_pending = 1;
    magictech_branch_destination = magictech_cur_component_list->cnt;
}

// 0x4529F0
void magictech_component_test_and_branch_process(void)
{
    int value;

    if (magictech_current_target_context.target_obj != OBJ_HANDLE_NULL && obj_type_is_critter(magictech_cur_target_obj_type)) {
        value = obj_field_int32_get(magictech_current_target_context.target_obj, magictech_cur_component->data.test_in_branch.object_field);
        switch (magictech_cur_component->data.test_in_branch.comparison_operator) {
        case 0:
            if (value == magictech_cur_component->data.test_in_branch.test_value) {
                return;
            }
            break;
        case 1:
            if (value > magictech_cur_component->data.test_in_branch.test_value) {
                return;
            }
            break;
        case 2:
            if (value >= magictech_cur_component->data.test_in_branch.test_value) {
                return;
            }
            break;
        case 3:
            if (value < magictech_cur_component->data.test_in_branch.test_value) {
                return;
            }
            break;
        case 4:
            if (value <= magictech_cur_component->data.test_in_branch.test_value) {
                return;
            }
            break;
        default:
            tig_debug_printf("magictech_component_test_and_branch_process: Error: test type out of range!\n");
            break;
        }

        magictech_branch_pending = true;
        if (magictech_cur_component->data.test_in_branch.branch_destination != -1) {
            magictech_branch_destination = magictech_cur_component->data.test_in_branch.branch_destination;
            if (magictech_branch_destination >= magictech_cur_component_list->cnt) {
                magictech_branch_destination = magictech_cur_component_list->cnt;
            }
        } else {
            magictech_branch_destination = magictech_cur_component_list->cnt;
        }
    }
}

// 0x452AD0
void magictech_component_trait_process(void)
{
    magictech_component_trait(magictech_current_target_context.target_obj, &(magictech_cur_component->data.trait), magictech_cur_target_obj_type);
}

// 0x452B00
void magictech_component_trait(int64_t obj, MagicTechComponentTrait* trait, int obj_type)
{
    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    if (!multiplayer_is_locked()) {
        Packet74 pkt;

        pkt.type = 74;
        pkt.subtype = 0;
        pkt.oid = obj_get_id(obj);
        pkt.trait = *trait;
        pkt.obj_type = obj_type;
        tig_net_send_app_all(&pkt, sizeof(pkt));
    }

    if (trait->fld == OBJ_F_CURRENT_AID && obj_type_is_critter(obj_type)) {
        tig_art_id_t art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);

        if (trait->value != -1) {
            int inventory_location;
            int weapon;
            int anim;
            int rot;

            for (inventory_location = FIRST_WEAR_INV_LOC; inventory_location <= LAST_WEAR_INV_LOC; inventory_location++) {
                item_unequip_slot(obj, inventory_location);
                if (!item_unequip_slot(obj, inventory_location)) {
                    int64_t item_obj = item_wield_get(obj, inventory_location);
                    if (item_obj != OBJ_HANDLE_NULL) {
                        if (!item_drop(item_obj)) {
                            tig_debug_printf("MagicTech: magictech_process: MTComponentTrait: ERROR: Item_Drop Failed!\n");
                        }
                    }
                }
            }

            weapon = tig_art_critter_id_weapon_get(art_id);
            anim = tig_art_id_anim_get(art_id);
            rot = tig_art_id_rotation_get(art_id);

            if (tig_art_monster_id_create(trait->value, 0, 0, 0, rot, anim, weapon, trait->palette, &art_id) != TIG_OK) {
                tig_debug_printf("MagicTech: magictech_process: MTComponentTrait: ERROR: Monster Art Create Failed!\n");
                exit(EXIT_FAILURE);
            }

            object_set_current_aid(obj, art_id);

            if (trait->value >= TIG_ART_MONSTER_SPECIE_FIRE_ELEMENTAL
                && trait->value <= TIG_ART_MONSTER_SPECIE_AIR_ELEMENTAL) {
                obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS2, OCF2_AUTO_ANIMATES);
                obj_field_int32_set(obj, OBJ_F_BLIT_FLAGS, TIG_ART_BLT_BLEND_ADD);
            }
        } else {
            magictech_polymorph_art_apply(obj, art_id);
        }
    }
}

// 0x452CD0
void magictech_polymorph_art_apply(int64_t obj, tig_art_id_t art_id)
{
    int specie;
    unsigned int flags;
    int rotation;
    int anim;
    tig_art_id_t current_art_id;

    if (tig_art_type(art_id) == TIG_ART_TYPE_MONSTER) {
        specie = tig_art_monster_id_specie_get(art_id);
        if (specie >= TIG_ART_MONSTER_SPECIE_FIRE_ELEMENTAL
            && specie <= TIG_ART_MONSTER_SPECIE_AIR_ELEMENTAL) {
            flags = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2);
            flags &= ~OCF2_AUTO_ANIMATES;
            obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS2, flags);

            flags = obj_field_int32_get(obj, OBJ_F_BLIT_FLAGS);
            flags &= ~TIG_ART_BLT_BLEND_ADD;
            obj_field_int32_set(obj, OBJ_F_BLIT_FLAGS, flags);
        }
    }

    rotation = tig_art_id_rotation_get(art_id);
    anim = tig_art_id_anim_get(art_id);

    current_art_id = obj_field_int32_get(obj, OBJ_F_AID);
    current_art_id = tig_art_id_rotation_set(current_art_id, rotation);
    current_art_id = tig_art_id_anim_set(current_art_id, anim);
    current_art_id = tig_art_id_frame_set(current_art_id, 0);
    object_set_current_aid(obj, current_art_id);

    item_wield_best_all(obj, OBJ_HANDLE_NULL);
}

// 0x452D80
void magictech_component_indexed_trait_process(void)
{
    int value;

    if (magictech_current_target_context.target_obj != OBJ_HANDLE_NULL && obj_type_is_critter(magictech_cur_target_obj_type)) {
        if (magictech_cur_component->data.trait_idx.object_field == OBJ_F_CRITTER_STAT_BASE_IDX) {
            value = stat_base_get(magictech_current_target_context.target_obj,
                magictech_cur_component->data.trait_idx.index);
        } else {
            value = obj_arrayfield_int32_get(magictech_current_target_context.target_obj,
                magictech_cur_component->data.trait_idx.object_field,
                magictech_cur_component->data.trait_idx.index);
        }

        if (magictech_cur_component->data.trait_idx.indexed_trait_mode == 0) {
            value = value * magictech_cur_component->data.trait_idx.scale_numerator / magictech_cur_component->data.trait_idx.scale_denominator + magictech_cur_component->data.trait_idx.addend;
            if (magictech_cur_component->data.trait_idx.object_field == OBJ_F_CRITTER_STAT_BASE_IDX) {
                stat_base_set(magictech_current_target_context.target_obj,
                    magictech_cur_component->data.trait_idx.index,
                    value);
            } else {
                obj_arrayfield_int32_set(magictech_current_target_context.target_obj,
                    magictech_cur_component->data.trait_idx.object_field,
                    magictech_cur_component->data.trait_idx.index, value);
            }
        }
    }
}

// 0x452E40
void magictech_component_trait_int64_process(void)
{
    switch (magictech_cur_component->data.trait64.value_source) {
    case 0:
        obj_field_int64_set(magictech_current_target_context.target_obj, magictech_cur_component->data.trait64.object_field, magictech_current_target_context.target_loc);
        obj_field_int32_set(magictech_current_target_context.target_obj, OBJ_F_CRITTER_TELEPORT_MAP, map_current_map());
        break;
    case 1:
    case 2:
        magictech_source_swap_obj = magictech_cur_run_info->parent_obj.obj;
        obj_field_handle_set(magictech_current_target_context.target_obj, magictech_cur_component->data.trait64.object_field, magictech_source_swap_obj);
        break;
    }
}

// 0x452ED0
void magictech_component_use_process(void)
{
    if (magictech_current_target_context.target_obj != OBJ_HANDLE_NULL) {
        object_script_execute(magictech_current_target_context.target_obj,
            magictech_current_target_context.target_obj,
            magictech_current_target_context.target_obj,
            SAP_USE,
            0);
    }
}

// 0x452F00
void magictech_component_noop_process(void)
{
}

// 0x452F10
void magictech_component_unsupported_environment_flag_process(void)
{
    tig_debug_printf("MagicTech: Process: Component: Error:  Invalid Component Type!\n");
}

// 0x452F20
bool magictech_current_run_costs_and_stacking_check(void)
{
    bool requirements_failed;
    bool consume_energy;
    MesFileEntry mes_file_entry;

    requirements_failed = false;
    consume_energy = (magictech_cur_run_info->flags & MAGICTECH_RUN_FREE) == 0;

    if (magictech_cur_run_info->action == MAGICTECH_ACTION_BEGIN) {
        if (obj_type_is_critter(magictech_cur_run_info->source_obj.type)) {
            if (critter_is_dead(magictech_cur_run_info->source_obj.obj)) {
                return false;
            }

            if (critter_is_unconscious(magictech_cur_run_info->source_obj.obj)) {
                return false;
            }
        }

        if (obj_type_is_critter(magictech_cur_run_info->parent_obj.type)
            && magictech_is_magic(magictech_cur_run_info->spell)) {
            if (stat_level_get(magictech_cur_run_info->parent_obj.obj, STAT_INTELLIGENCE) < magictech_min_intelligence(magictech_cur_run_info->spell)) {
                if (obj_type_is_critter(magictech_cur_run_info->source_obj.type)) {
                    requirements_failed = true;
                    consume_energy = false;
                    mes_file_entry.num = 602; // "You lose your concentration."
                }
            }

            if (stat_level_get(magictech_cur_run_info->parent_obj.obj, STAT_WILLPOWER) < magictech_min_willpower(magictech_cur_run_info->spell)) {
                if (obj_type_is_critter(magictech_cur_run_info->source_obj.type)) {
                    requirements_failed = true;
                    consume_energy = false;
                    mes_file_entry.num = 602; // "You lose your concentration."
                }
            }

            if (consume_energy) {
                if ((magictech_cur_spell_info->flags & 0x100) == 0
                    && !magictech_casting_cost_consume(magictech_cur_run_info->source_obj.obj, magictech_cur_run_info->spell)) {
                    requirements_failed = true;
                    mes_file_entry.num = 600; // "Not enough Energy."
                }
            }

            if (requirements_failed) {
                if (player_is_local_pc_obj(magictech_cur_run_info->parent_obj.obj)) {
                    mes_get_msg(magictech_spell_mes_file, &mes_file_entry);
                    ui_display_warning(mes_file_entry.str);
                }

                return false;
            }
        }

        magictech_source_swap_obj = magictech_cur_run_info->target_obj.obj;

        if (magictech_source_swap_obj == OBJ_HANDLE_NULL) {
            magictech_source_swap_obj = magictech_cur_run_info->source_obj.obj;
        }

        if (magictech_source_swap_obj != OBJ_HANDLE_NULL) {
            if (magictech_cur_spell_info->no_stack) {
                int other_mt_id;
                if (magictech_other_instance_on_target_find(magictech_cur_run_info->id, magictech_cur_run_info->spell, magictech_source_swap_obj, &other_mt_id)
                    && magictech_cur_run_info->id != other_mt_id) {
                    return false;
                }
            }
        }

        magictech_conflicting_spell_flags_cancel(magictech_cur_run_info);
        return true;
    }

    if (magictech_cur_run_info->action == MAGICTECH_ACTION_MAINTAIN) {
        if (obj_type_is_critter(magictech_cur_run_info->parent_obj.type)) {
            if (stat_level_get(magictech_cur_run_info->parent_obj.obj, STAT_INTELLIGENCE) < magictech_min_intelligence(magictech_cur_run_info->spell)) {
                if (obj_type_is_critter(magictech_cur_run_info->source_obj.type)) {
                    requirements_failed = true;
                    consume_energy = false;
                    mes_file_entry.num = 602; // "You lose your concentration."
                }
            }

            if (stat_level_get(magictech_cur_run_info->parent_obj.obj, STAT_WILLPOWER) < magictech_min_willpower(magictech_cur_run_info->spell)) {
                if (obj_type_is_critter(magictech_cur_run_info->source_obj.type)) {
                    requirements_failed = true;
                    consume_energy = false;
                    mes_file_entry.num = 602; // "You lose your concentration."
                }
            }

            if (consume_energy) {
                if (!magictech_maintenance_cost_consume(magictech_cur_run_info->source_obj.obj, magictech_cur_run_info->spell)) {
                    requirements_failed = true;
                    mes_file_entry.num = 601; // "Maintain terminated."
                }
            }

            if (magictech_cur_run_info->resistance_amount != 0) {
                if (!magictech_maintenance_cost_consume_with_surcharge(magictech_cur_run_info->source_obj.obj, magictech_cur_run_info->spell, magictech_cur_run_info->resistance_amount)) {
                    requirements_failed = true;
                    mes_file_entry.num = 601; // "Maintain terminated."
                }
            }

            if (requirements_failed) {
                if (player_is_local_pc_obj(magictech_cur_run_info->parent_obj.obj)) {
                    mes_get_msg(magictech_spell_mes_file, &mes_file_entry);
                    ui_display_warning(mes_file_entry.str);
                    ui_spell_maintain_end(magictech_cur_run_info->id);
                }

                magictech_cur_run_info->action = MAGICTECH_ACTION_END;

                magictech_current_target_list.cnt = 0;
                magictech_run_process(magictech_cur_run_info);

                return false;
            }
        }

        return true;
    }

    return true;
}

// 0x4532F0
bool magictech_maintenance_cost_consume(int64_t obj, int magictech)
{
    if (obj == OBJ_HANDLE_NULL) return true;
    int cost = magictech_fatigue_cost(obj, magictech, MAGICTECH_ACTION_MAINTAIN, false);
    return cost >= 0 && magictech_cost_check_or_consume(obj, cost, true, magictech);
}

// 0x453370
bool magictech_maintenance_cost_consume_with_surcharge(int64_t obj, int magictech, int surcharge_percent)
{
    int cost;
    int surcharge;

    if (obj == OBJ_HANDLE_NULL) {
        return true;
    }

    cost = magictech_get_maintenance(magictech)->cost;
    surcharge = surcharge_percent * cost / 100;
    if (surcharge == 0) {
        surcharge = 1;
    }
    cost += surcharge;

    if (obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))
        && stat_level_get(obj, STAT_RACE) == RACE_DWARF) {
        cost *= 2;
    }

    return magictech_cost_check_or_consume(obj, cost, true, magictech);
}

// 0x453410
bool magictech_other_instance_on_target_find(int mt_id, int spell, int64_t obj, int* other_mt_id_ptr)
{
    int idx;

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    for (idx = 0; idx < 512; idx++) {
        if ((magictech_run_info[idx].flags & MAGICTECH_RUN_ACTIVE) != 0) {
            if (magictech_run_info[idx].target_obj.obj == obj
                && magictech_run_info[idx].spell == spell
                && magictech_run_info[idx].id != mt_id) {
                if (other_mt_id_ptr != NULL) {
                    *other_mt_id_ptr = idx;
                }
                return true;
            } else if (magictech_run_info[idx].target_obj.obj == OBJ_HANDLE_NULL
                && (magictech_spells[magictech_run_info[idx].spell].target_params[MAGICTECH_ACTION_BEGIN].tgt & TGT_SELF) != 0
                && magictech_run_info[idx].source_obj.obj == obj
                && magictech_run_info[idx].spell == spell
                && magictech_run_info[idx].id != mt_id) {
                if (other_mt_id_ptr != NULL) {
                    *other_mt_id_ptr = idx;
                }
                return true;
            }
        }
    }

    return false;
}

// 0x4534E0
void magictech_conflicting_spell_flags_cancel(MagicTechRunInfo* run_info)
{
    int index;
    MagicTechInfo* info;
    MagicTechRunInfo* other_run_info;

    if ((run_info->flags & MAGICTECH_RUN_ACTIVE) == 0) {
        return;
    }

    info = &(magictech_spells[run_info->spell]);
    if (info->cancels_sf != 0) {
        for (index = 0; index < 512; index++) {
            other_run_info = &(magictech_run_info[index]);
            if ((other_run_info->flags & MAGICTECH_RUN_ACTIVE) != 0
                && other_run_info->source_obj.obj == run_info->source_obj.obj
                && (magictech_spells[other_run_info->spell].cancels_sf & info->cancels_sf) != 0
                && other_run_info->id != run_info->id) {
                magictech_interrupt(other_run_info->id);
            }
        }
    }

    if (info->cancels_envsf != 0 && magictech_check_env_sf(info->cancels_envsf)) {
        for (index = 0; index < 512; index++) {
            other_run_info = &(magictech_run_info[index]);
            if ((other_run_info->flags & MAGICTECH_RUN_ACTIVE) != 0
                && other_run_info->source_obj.obj == run_info->source_obj.obj
                && (magictech_spells[other_run_info->spell].cancels_envsf & info->cancels_envsf) != 0
                && other_run_info->id != run_info->id) {
                magictech_interrupt(other_run_info->id);
            }
        }
    }
}

// 0x453630
void magictech_current_target_context_init(void)
{
    target_context_init(&magictech_current_target_context, 0, magictech_cur_run_info->source_obj.obj);
    magictech_current_target_context.targets = &magictech_current_target_list;
    magictech_current_target_context.obj_list = &magictech_cur_run_info->objlist;
    magictech_current_target_context.summoned_obj_list = &magictech_cur_run_info->summoned_obj;
    magictech_current_target_context.orig_target_obj = magictech_cur_run_info->target_obj.obj;
    magictech_current_target_context.orig_target_loc = magictech_cur_run_info->target_obj.loc;
    magictech_current_target_context.attacker_obj = magictech_cur_run_info->attacker_obj.obj;
    magictech_current_target_context.summoned_obj = OBJ_HANDLE_NULL;
    magictech_current_target_context.self_obj = magictech_cur_run_info->parent_obj.obj;
    magictech_original_source_obj = magictech_cur_run_info->source_obj.obj;

    if (magictech_cur_run_info->source_obj.obj == OBJ_HANDLE_NULL) {
        magictech_current_target_context.source_loc = magictech_cur_run_info->source_obj.loc;
    }
}

// 0x453710
bool magictech_source_antimagic_check(void)
{
    MesFileEntry mes_file_entry;

    if (magictech_cur_run_info->source_obj.obj == OBJ_HANDLE_NULL) {
        return true;
    }

    if (magictech_cur_run_info->action >= MAGICTECH_ACTION_END) {
        return true;
    }

    magictech_current_target_context.source_spell_flags = obj_field_int32_get(magictech_cur_run_info->source_obj.obj, OBJ_F_SPELL_FLAGS);
    if ((magictech_current_target_context.source_spell_flags & OSF_ANTI_MAGIC_SHELL) == 0
        || (magictech_cur_run_info->applied_spell_flags & 0x800) != 0) {
        return true;
    }

    if (player_is_local_pc_obj(magictech_cur_run_info->parent_obj.obj)) {
        mes_file_entry.num = 602; // "You lose your concentration."
        mes_get_msg(magictech_spell_mes_file, &mes_file_entry);
        ui_display_warning(mes_file_entry.str);
        ui_spell_maintain_end(magictech_cur_run_info->id);
    }

    return false;
}

// 0x4537B0
bool magictech_current_target_resistance_resolve(void)
{
    int accumulated_resistance_penalty = 0;

    if (magictech_cur_run_info->action != MAGICTECH_ACTION_BEGIN) {
        return true;
    }

    if ((magictech_cur_spell_info->flags & MAGICTECH_IS_TECH) != 0) {
        return true;
    }

    if ((magictech_cur_run_info->flags & MAGICTECH_RUN_UNRESISTABLE) != 0) {
        return true;
    }

    magictech_current_resistance_amount = 0;
    magictech_cur_run_info->resistance_amount = 0;

    if (magictech_current_target_context.target_obj == OBJ_HANDLE_NULL) {
        return true;
    }

    if (magictech_cur_run_info->spell == SPELL_DISINTEGRATE
        && obj_type_is_critter(obj_field_int32_get(magictech_current_target_context.target_obj, OBJ_F_TYPE))
        && (obj_field_int32_get(magictech_current_target_context.target_obj, OBJ_F_CRITTER_FLAGS2) & OCF2_NO_DISINTEGRATE) != 0) {
        return false;
    }

    if (magictech_cur_run_info->parent_obj.obj != magictech_current_target_context.target_obj) {
        if (obj_field_int32_get(magictech_current_target_context.target_obj, OBJ_F_TYPE) == OBJ_TYPE_PC
            && fate_resolve(magictech_current_target_context.target_obj, FATE_SAVE_AGAINST_MAGICK)) {
            magictech_current_resistance_amount = 100;
            magictech_cur_run_info->resistance_amount = 10;
        } else {
            // CE: The original code does not account for magic resistance
            // adjustments from equipped items.
            int resistance = object_get_resistance(magictech_current_target_context.target_obj, RESISTANCE_TYPE_MAGIC, false);
            if (!magictech_cur_is_fate_maximized) {
                int aptitude = stat_level_get(magictech_current_target_context.target_obj, STAT_MAGICK_TECH_APTITUDE);
                if (aptitude < 0) {
                    resistance = 100 - (100 - resistance) * (aptitude + 100) / 100;
                }
            }

            int chance = random_between(1, 100);
            if (chance < resistance) {
                magictech_current_resistance_amount = resistance - chance;
                magictech_cur_run_info->resistance_amount = resistance - chance;
            }

            if (resistance > 0
                && (magictech_cur_spell_info->flags & MAGICTECH_HAVE_DAMAGE) == 0
                && magictech_cur_spell_info->maintenance.period <= 0) {
                if (magictech_cur_resistance->stat != -1) {
                    accumulated_resistance_penalty = magictech_cur_run_info->resistance_amount / 10;
                } else {
                    if (chance < resistance) {
                        return false;
                    }
                }
            }
        }
    }

    if (magictech_current_target_context.target_obj == OBJ_HANDLE_NULL
        || magictech_cur_run_info->parent_obj.obj == magictech_current_target_context.target_obj
        || magictech_cur_resistance->stat == -1
        || !obj_type_is_critter(magictech_cur_target_obj_type)) {
        return true;
    }

    if (magictech_cur_resistance->stat == STAT_WILLPOWER
        && stat_is_extraordinary(magictech_current_target_context.target_obj, STAT_WILLPOWER)) {
        if ((magictech_cur_spell_info->flags & MAGICTECH_AGGRESSIVE) != 0
            && magictech_cur_run_info->action != MAGICTECH_ACTION_END) {
            magictech_aggressive_target_notify_once(magictech_cur_run_info->parent_obj.obj, magictech_current_target_context.target_obj);
        }
        return false;
    }

    int resistance_difficulty = magictech_cur_resistance->value + stat_level_get(magictech_current_target_context.target_obj, magictech_cur_resistance->stat) - accumulated_resistance_penalty;
    if (resistance_difficulty > 0) {
        int resistance_roll = random_between(1, 20);
        if (resistance_roll <= resistance_difficulty) {
            magictech_cur_run_info->resistance_amount += resistance_difficulty - resistance_roll;
            magictech_current_resistance_amount = magictech_cur_run_info->resistance_amount;

            if ((magictech_cur_spell_info->flags & MAGICTECH_HAVE_DAMAGE) != 0) {
                if (magictech_cur_run_info->resistance_amount < 50) {
                    magictech_current_resistance_amount = 50;
                    magictech_cur_run_info->resistance_amount = 50;
                }
            } else {
                if (magictech_cur_spell_info->maintenance.period == 0) {
                    return false;
                }
            }
        }
    }

    return true;
}

/**
 * Determines the failure chance of casting a given spell.
 *
 * 0x453B20
 */
int magictech_cast_spell_fail_chance(int64_t attacker_obj, int64_t target_obj, int spell)
{
    MagicTechInfo* info;
    int obj_type;
    int resistance = 0;
    int saving_throw_bonus = 0;

    if (target_obj == OBJ_HANDLE_NULL) {
        return 0;
    }

    obj_type = obj_field_int32_get(target_obj, OBJ_F_TYPE);
    info = &(magictech_spells[spell]);

    // Apply resistance mechanics only to spells, not tech abilities.
    if ((info->flags & MAGICTECH_IS_TECH) == 0) {
        // Self-cast spells should never fail.
        if (attacker_obj != target_obj) {
            // Retrieve the target's base magic resistance, which is also the
            // base chance for a spell to fail.

            // CE: The original code does not account for magic resistance
            // adjustments from equipped items.
            resistance = object_get_resistance(target_obj, RESISTANCE_TYPE_MAGIC, false);

            if (!magictech_cur_is_fate_maximized) {
                // Adjust resistance based on the target's tech aptitude: the
                // more technology-aligned the target, the larger the bonus. For
                // example, with a base resistance of 50%: a target with 20%
                // tech aptitude would have 60% resistance, one with 80%
                // aptitude would have 90% resistance, and a target with 100%
                // tech aptitude would have 100% resistance.
                int aptitude = stat_level_get(target_obj, STAT_MAGICK_TECH_APTITUDE);
                if (aptitude < 0) {
                    resistance = 100 - (100 - resistance) * (aptitude + 100) / 100;
                }
            }

            // "If the spell does not do damage, but has a "saving throw"
            // instead, the target will enjoy a +1 bonus on the save for each
            // 10% of resistance it possesses..." (from the manual).
            if (resistance > 0
                && (info->flags & MAGICTECH_HAVE_DAMAGE) == 0
                && info->maintenance.period <= 0) {
                if (info->resistance.stat != -1) {
                    saving_throw_bonus = resistance / 10;
                } else {
                    // The spell does not define a saving throw, meaning
                    // resistance alone determines the outcome. Resistance above
                    // 40% is considered too powerful and grants complete
                    // immunity to the spell.
                    if (resistance > 40) {
                        return 100;
                    }
                }
            }
        }

        if (attacker_obj != target_obj) {
            if (info->resistance.stat != -1
                && obj_type_is_critter(obj_type)) {
                // Extraordinary willpower grants complete immunity to any spell
                // resisted by willpower.
                if (info->resistance.stat == STAT_WILLPOWER
                    && stat_is_extraordinary(target_obj, STAT_WILLPOWER)) {
                    return 100;
                }

                // Calculate the effective difficulty. Remember the resistance
                // value is almost always negative (see `spelllist.mes`), that
                // is acts a penalty to the specified stat.
                int difficulty = stat_level_get(target_obj, info->resistance.stat) + info->resistance.value - saving_throw_bonus;
                if (difficulty > 0 && random_between(1, 20) <= difficulty) {
                    // The target rolled a successful saving throw.
                    if ((info->flags & MAGICTECH_HAVE_DAMAGE) == 0) {
                        // Non-damaging spells with no ongoing effects have
                        // 100% chance to fail.
                        if (info->maintenance.period == 0) {
                            resistance = 100;
                        }
                    } else {
                        // Damaging spells have at least 50% chance to fail.
                        if (resistance < 50) {
                            resistance = 50;
                        }
                    }
                }
            }
        }
    }

    return resistance;
}

/**
 * Determines the failure chance of using a given item.
 *
 * 0x453CC0
 */
int magictech_use_item_fail_chance(int64_t attacker_obj, int64_t item_obj, int64_t target_obj)
{
    int spell_mana_store;
    int item_flags;
    int spell;

    if (target_obj == OBJ_HANDLE_NULL
        || item_obj == OBJ_HANDLE_NULL) {
        return 0;
    }

    spell_mana_store = obj_field_int32_get(item_obj, OBJ_F_ITEM_SPELL_MANA_STORE);
    item_flags = obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS);

    if (spell_mana_store == 0
        && (item_flags & OIF_IS_MAGICAL) == 0) {
        return 0;
    }

    spell = obj_field_int32_get(item_obj, OBJ_F_ITEM_SPELL_1);
    if (spell == -1) {
        return 0;
    }

    return magictech_cast_spell_fail_chance(attacker_obj, target_obj, spell);
}

// 0x453D40
void magictech_component_target_prepare(void)
{
    if (magictech_cur_component->apply_aoe.tgt == TGT_NONE) {
        return;
    }

    if ((magictech_cur_component->apply_aoe.tgt & TGT_SELF) != 0) {
        magictech_current_target_context.target_obj = magictech_cur_run_info->parent_obj.obj;
        if (magictech_current_target_context.target_obj != OBJ_HANDLE_NULL) {
            magictech_cur_target_obj_type = obj_field_int32_get(magictech_current_target_context.target_obj, OBJ_F_TYPE);
        }
    }

    if ((magictech_cur_component->apply_aoe.tgt & TGT_SOURCE) != 0) {
        magictech_current_target_context.target_obj = magictech_cur_run_info->source_obj.obj;
        if (magictech_current_target_context.target_obj != OBJ_HANDLE_NULL) {
            magictech_cur_target_obj_type = obj_field_int32_get(magictech_current_target_context.target_obj, OBJ_F_TYPE);
        }
    }

    if ((magictech_cur_component->apply_aoe.tgt & TGT_SUMMONED) != 0) {
        magictech_current_target_context.target_obj = magictech_current_target_context.summoned_obj;
        if (magictech_current_target_context.target_obj != OBJ_HANDLE_NULL) {
            magictech_cur_target_obj_type = obj_field_int32_get(magictech_current_target_context.target_obj, OBJ_F_TYPE);
        }
    }

    if ((magictech_cur_component->apply_aoe.tgt & TGT_TILE) != 0) {
        if ((magictech_cur_component->apply_aoe.tgt & TGT_TILE_OFFSCREEN) != 0) {
            if (magictech_current_target_context.target_obj != OBJ_HANDLE_NULL) {
                magictech_current_target_context.target_loc = obj_field_int64_get(magictech_current_target_context.target_obj, OBJ_F_LOCATION);
            }
        } else {
            if (magictech_cur_run_info->target_obj.loc != 0) {
                magictech_current_target_context.target_loc = magictech_cur_run_info->target_obj.loc;
            } else {
                magictech_current_target_context.target_loc = obj_field_int64_get(magictech_cur_run_info->target_obj.obj, OBJ_F_LOCATION);
            }
        }
    }
}

// 0x453EE0
void magictech_aggressive_target_notify(void)
{
    if ((magictech_cur_spell_info->flags & MAGICTECH_AGGRESSIVE) != 0
        && magictech_current_target_context.target_obj != OBJ_HANDLE_NULL
        && magictech_cur_run_info->action != MAGICTECH_ACTION_END) {
        magictech_aggressive_target_notify_once(magictech_cur_run_info->parent_obj.obj, magictech_current_target_context.target_obj);
    }
}

// 0x453F20
void magictech_aggressive_target_notify_once(int64_t attacker_obj, int64_t target_obj)
{
    if (magictech_last_aggression_notified_obj != target_obj) {
        magictech_last_aggression_notified_obj = target_obj;
        ai_attack(attacker_obj, target_obj, 1, 0);

        if (magictech_cur_run_info->attacker_obj.obj != OBJ_HANDLE_NULL
            && attacker_obj != magictech_cur_run_info->attacker_obj.obj) {
            ai_attack(magictech_cur_run_info->attacker_obj.obj, target_obj, LOUDNESS_NORMAL, 0);
        }
    }
}

// TODO: Lots of jumps, rewrite without goto.
//
// 0x453FA0
void magictech_current_run_advance_or_finish(void)
{
    bool should_end_run;
    bool continue_processing;
    MagicTechMaintenanceInfo* maintenance;
    MagicTechDurationInfo* duration;
    MesFileEntry mes_file_entry;
    DateTime datetime;
    TimeEvent timeevent;

    should_end_run = false;
    continue_processing = true;

    if (magictech_effect_callback_pending) {
        if (!magictech_effect_callback_processing) {
            magictech_effect_callback_processing = true;
            if (magictech_effect_end_callback_pending) {
                magictech_run_callback_action_process(magictech_cur_run_info->id, MAGICTECH_ACTION_END_CALLBACK);
            } else {
                magictech_run_callback_action_process(magictech_cur_run_info->id, MAGICTECH_ACTION_CALLBACK);
            }
            magictech_effect_callback_processing = false;
            return;
        }
    } else {
        if (magictech_cur_run_info->action == MAGICTECH_ACTION_END) {
            goto LABEL_25;
        }

        if (magictech_cur_run_info->action >= MAGICTECH_ACTION_END && magictech_cur_run_info->action != MAGICTECH_ACTION_CALLBACK) {
        LABEL_69:
            if (magictech_cur_spell_info->components[MAGICTECH_ACTION_CALLBACK].cnt == 0
                && magictech_cur_spell_info->components[MAGICTECH_ACTION_END_CALLBACK].cnt == 0) {
                should_end_run = true;
            }

            if (magictech_cur_run_info->action == MAGICTECH_ACTION_CALLBACK
                && magictech_cur_spell_info->components[MAGICTECH_ACTION_END].cnt == 0) {
                should_end_run = true;
            }

            if (magictech_cur_run_info->action == MAGICTECH_ACTION_END_CALLBACK) {
                magictech_cur_run_info->action = MAGICTECH_ACTION_END;
                should_end_run = false;
            }

            if ((magictech_cur_run_info->action != MAGICTECH_ACTION_BEGIN
                    || magictech_cur_spell_info->components[MAGICTECH_ACTION_CALLBACK].cnt == 0
                    || !magictech_waiting_for_effect_callback)
                && should_end_run) {
                magictech_cur_run_info->action = MAGICTECH_ACTION_END;
                magictech_run_process(magictech_cur_run_info);
                return;
            }

            if (!continue_processing) {
                return;
            }

            goto LABEL_25;
        }

        maintenance = magictech_get_maintenance(magictech_cur_run_info->spell);
        if ((magictech_cur_run_info->flags & MAGICTECH_RUN_UNRESISTABLE) != 0) {
            should_end_run = true;
            goto LABEL_69;
        }

        if (maintenance->period > 0) {
            if (magictech_current_run_had_effect) {
                if (magictech_cur_run_info->source_obj.obj != OBJ_HANDLE_NULL) {
                    if (!magictech_run_maintenance_capacity_check(magictech_cur_run_info)) {
                        if (player_is_pc_obj(magictech_cur_run_info->parent_obj.obj)) {
                            mes_file_entry.num = 601; // "Maintain terminated."
                            mes_get_msg(magictech_spell_mes_file, &mes_file_entry);
                            ui_display_warning(mes_file_entry.str);
                        }
                        magictech_cur_run_info->action = MAGICTECH_ACTION_END;
                        magictech_run_process(magictech_cur_run_info);
                        return;
                    }

                    magictech_maintenance_timeevent_prepare(&timeevent, magictech_cur_run_info->id, maintenance->period, &datetime);

                    if (magictech_maintenance_schedule_and_show(&timeevent, &datetime)) {
                    LABEL_25:
                        if (magictech_cur_run_info->action != MAGICTECH_ACTION_END) {
                            return;
                        }

                        if (magictech_waiting_for_effect_callback
                            && magictech_cur_spell_info->components[MAGICTECH_ACTION_END_CALLBACK].cnt == 0) {
                            magictech_waiting_for_effect_callback = false;
                        }

                        if (magictech_cur_run_info->action != MAGICTECH_ACTION_END
                            || magictech_waiting_for_effect_callback
                            || magictech_cur_run_info->id == -1) {
                            return;
                        }

                        goto END;
                    }

                    goto LABEL_69;
                }

                if (magictech_cur_run_info->remaining_duration_triggers == 0) {
                    magictech_maintenance_timeevent_prepare(&timeevent, magictech_cur_run_info->id, maintenance->period, &datetime);
                    continue_processing = false;
                    if (magictech_cur_spell_info->duration_trigger_count != 0) {
                        magictech_cur_run_info->remaining_duration_triggers = magictech_cur_spell_info->duration_trigger_count;
                    } else {
                        magictech_cur_run_info->remaining_duration_triggers = 8;
                    }

                LABEL_22:
                    if (magictech_maintenance_schedule_and_show(&timeevent, &datetime)) {
                        if (!continue_processing) {
                            return;
                        }

                        goto LABEL_25;
                    }

                    goto LABEL_69;
                }

                magictech_cur_run_info->remaining_duration_triggers--;
                if (magictech_cur_run_info->remaining_duration_triggers > 0) {
                    magictech_maintenance_timeevent_prepare(&timeevent, magictech_cur_run_info->id, maintenance->period, &datetime);
                    goto LABEL_22;
                }
            }
            should_end_run = true;
        } else {
            duration = magictech_get_duration(magictech_cur_run_info->spell);
            if (magictech_current_run_had_effect && (duration->period != 0 || duration->stat != -1)) {
                timeevent.type = TIMEEVENT_TYPE_MAGICTECH;
                timeevent.params[0].integer_value = magictech_cur_run_info->id;
                timeevent.params[2].integer_value = 1;

                if (magictech_cur_run_info->remaining_duration_triggers > 0) {
                    magictech_cur_run_info->remaining_duration_triggers--;
                } else {
                    magictech_cur_run_info->action = MAGICTECH_ACTION_END;
                    magictech_cur_run_info->flags |= MAGICTECH_RUN_UNRESISTABLE;
                }

                datetime_init(&datetime, duration->period);

                if (duration->stat > -1) {
                    if (magictech_cur_run_info->parent_obj.type != -1) {
                        if (obj_type_is_critter(magictech_cur_run_info->parent_obj.type)) {
                            int level = duration->level - stat_level_get(magictech_cur_run_info->parent_obj.obj, duration->stat);
                            if (level < 0) {
                                level = 0;
                            }
                            datetime.milliseconds += level * duration->modifier;
                            should_end_run = false;
                        }
                    } else {
                        if (magictech_cur_run_info->target_obj.obj != OBJ_HANDLE_NULL
                            && obj_type_is_critter(magictech_cur_run_info->target_obj.type)) {
                            int level = duration->level - stat_level_get(magictech_cur_run_info->target_obj.obj, duration->stat);
                            if (level < 0) {
                                level = 0;
                            }
                            datetime.milliseconds += level * duration->modifier;
                        }

                        should_end_run = false;
                    }
                }

                if (datetime.milliseconds == 0) {
                    datetime.milliseconds = 1;
                }

                datetime.milliseconds *= 1000;

                magictech_duration_adjust_for_aptitude_and_mastery(magictech_cur_run_info, &datetime);

                datetime.milliseconds *= 8;
                if (magictech_timeevent_add_unless_pending(&timeevent, &datetime, &magictech_cur_run_info->event_time)) {
                    return;
                }

                continue_processing = true;
            } else {
                should_end_run = true;
            }
        }

        goto LABEL_69;
    }

END:

    if (player_is_pc_obj(magictech_cur_run_info->parent_obj.obj)) {
        ui_spell_maintain_end(magictech_cur_run_info->id);
    }

    if (tig_net_is_active() && tig_net_is_host()) {
        PacketPlayerSpellMaintainEnd pkt;

        pkt.type = 61;
        pkt.mt_id = magictech_cur_run_info->id;
        pkt.player = multiplayer_find_slot_from_obj(magictech_cur_run_info->parent_obj.obj);

        if (pkt.player != -1) {
            tig_net_send_app_all(&pkt, sizeof(pkt));
        } else {
            tig_debug_println("MP: MagicTech: could not find player for object in SpellMaintainEnd but am handling...");
        }
    }

    magictech_id_free_lock(magictech_cur_run_info->id);

    if (player_is_pc_obj(magictech_cur_run_info->target_obj.obj)) {
        ui_character_panels_refresh();
    }
}

// 0x4545E0
bool magictech_run_maintenance_capacity_check(MagicTechRunInfo* run_info)
{
    int idx;
    int cnt = 0;

    if (run_info->source_obj.obj == OBJ_HANDLE_NULL) {
        return true;
    }

    // FIXME: Meaningless.
    if (run_info->parent_obj.obj != OBJ_HANDLE_NULL) {
        magictech_target_is_in_range(obj_field_int64_get(run_info->parent_obj.obj, OBJ_F_LOCATION),
            run_info->target_obj.loc,
            run_info->target_obj.obj,
            run_info->spell);
    }

    if (!obj_type_is_critter(obj_field_int32_get(run_info->parent_obj.obj, OBJ_F_TYPE))) {
        return false;
    }

    for (idx = 0; idx < 512; idx++) {
        if ((magictech_run_info[idx].flags & MAGICTECH_RUN_ACTIVE) != 0
            && (magictech_spells[magictech_run_info[idx].spell].flags & MAGICTECH_IS_TECH) == 0
            && magictech_run_info[idx].parent_obj.obj == run_info->parent_obj.obj
            && (magictech_run_info[idx].flags & MAGICTECH_RUN_PROCESSING_STARTED) != 0
            && magictech_spells[magictech_run_info[idx].spell].maintenance.period > 0) {
            cnt++;
        }
    }

    if (magictech_maintenance_slots_capacity(run_info->parent_obj.obj) < cnt) {
        return false;
    }

    return true;
}

// 0x454700
bool magictech_target_is_in_range(int64_t source_loc, int64_t target_loc, int64_t target_obj, int spell)
{
    if (source_loc == 0) {
        return true;
    }

    if (target_obj != OBJ_HANDLE_NULL) {
        if ((obj_field_int32_get(target_obj, OBJ_F_FLAGS) & OF_INVENTORY) != 0) {
            return true;
        }

        target_loc = obj_field_int64_get(target_obj, OBJ_F_LOCATION);
    }

    if (target_loc == 0) {
        return false;
    }

    return location_dist(source_loc, target_loc) <= magictech_get_range(spell);
}

// 0x454790
void magictech_maintenance_timeevent_prepare(TimeEvent* timeevent, int mt_id, int period_seconds, DateTime* datetime)
{
    timeevent->type = TIMEEVENT_TYPE_MAGICTECH;
    timeevent->params[0].integer_value = mt_id;
    timeevent->params[2].integer_value = 1;
    magictech_cur_run_info->action = MAGICTECH_ACTION_MAINTAIN;
    datetime_init(datetime, 1000 * period_seconds);
    magictech_duration_adjust_for_aptitude_and_mastery(magictech_cur_run_info, datetime);
    datetime->milliseconds *= 8;
}

// 0x4547F0
bool magictech_maintenance_schedule_and_show(TimeEvent* timeevent, DateTime* datetime)
{
    int player;

    if (!magictech_timeevent_add_unless_pending(timeevent, datetime, &(magictech_cur_run_info->event_time))) {
        return false;
    }

    if (player_is_local_pc_obj(magictech_cur_run_info->parent_obj.obj)) {
        if ((magictech_spells[magictech_cur_run_info->spell].flags & MAGICTECH_IS_TECH) == 0
            && !ui_spell_maintain_add(magictech_cur_run_info->id)) {
            magictech_cur_run_info->action = MAGICTECH_ACTION_END;
        }
    } else {
        player = multiplayer_find_slot_from_obj(magictech_cur_run_info->parent_obj.obj);
        if (player != -1) {
            PacketPlayerSpellMaintainAdd pkt;

            pkt.type = 60;
            pkt.mt_id = magictech_cur_run_info->id;
            pkt.player = player;

            tig_net_send_app_all(&pkt, sizeof(pkt));
        }
    }

    return true;
}

// 0x4548D0
bool magictech_timeevent_add_unless_pending(TimeEvent* timeevent, DateTime* delay, DateTime* at)
{
    bool already_pending;

    magictech_selected_timeevent_run_id = timeevent->params[0].integer_value;
    already_pending = timeevent_any(TIMEEVENT_TYPE_MAGICTECH, magictech_maintenance_timeevent_matches_current);
    magictech_selected_timeevent_run_id = -1;

    if (already_pending) {
        return true;
    }

    return timeevent_add_delay_at(timeevent, delay, at);
}

// 0x454920
bool magictech_component_recharge(int64_t obj, int num, int max)
{
    int spell_mana_store;

    if (!multiplayer_is_locked()) {
        if (!tig_net_is_host()) {
            return false;
        }

        Packet74 pkt;
        pkt.type = 74;
        pkt.subtype = 2;
        if (obj != OBJ_HANDLE_NULL) {
            pkt.oid = obj_get_id(obj);
        } else {
            pkt.oid.type = OID_TYPE_NULL;
        }
        tig_net_send_app_all(&pkt, sizeof(pkt));
    }

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (!obj_type_is_item(obj_field_int32_get(obj, OBJ_F_TYPE))) {
        return false;
    }

    spell_mana_store = obj_field_int32_get(obj, OBJ_F_ITEM_SPELL_MANA_STORE);
    if (spell_mana_store < max) {
        spell_mana_store += num;
        if (spell_mana_store > max) {
            spell_mana_store = max;
        }
        obj_field_int32_set(obj, OBJ_F_ITEM_SPELL_MANA_STORE, spell_mana_store);
        ui_counters_refresh();
    }

    return true;
}

// 0x454A10
void magictech_component_obj_flag(int64_t obj, int64_t source_obj, int fld, int flag_mask, int operation, int64_t caster_obj, int64_t power_source_obj)
{
    int obj_type;
    unsigned int flags;
    bool lock_changed = false;
    bool change_allowed = true;
    CombatContext combat;

    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    obj_type = obj_field_int32_get(obj, OBJ_F_TYPE);
    flags = obj_field_int32_get(obj, fld);

    if (operation == 1) {
        if (magictech_matching_spell_flag_instances_count(obj, fld, flag_mask, true) > 0) {
            return;
        }

        flags |= flag_mask;

        switch (fld) {
        case OBJ_F_SPELL_FLAGS:
            if ((flag_mask & (OSF_DETECTING_INVISIBLE | OSF_DETECTING_TRAPS | OSF_DETECTING_ALIGNMENT | OSF_DETECTING_MAGIC)) != 0) {
                if (player_is_local_pc_obj(obj)) {
                    magictech_invalidate_rect_callback(NULL);
                }
            } else if ((flag_mask & OSF_INVISIBLE) != 0) {
                if (!player_is_local_pc_obj(obj)) {
                    object_flags_set(obj, OF_INVISIBLE);
                }
                object_add_flags(obj, OF_TRANSLUCENT);
            } else if ((flag_mask & OSF_SHRUNK) != 0) {
                object_add_flags(obj, OF_SHRUNK);
            } else if ((flag_mask & OSF_WATER_WALKING) != 0) {
                object_flags_set(obj, OF_WATER_WALKING);
            } else if ((flag_mask & OSF_STONED) != 0) {
                object_flags_set(obj, OF_STONED);
            } else if ((flag_mask & OSF_ENTANGLED) != 0) {
                anim_interrupt_all_goals_of_type(obj, AG_MOVE_TO_TILE, -1);
                anim_interrupt_all_goals_of_type(obj, AG_ATTEMPT_MOVE_NEAR, -1);
            } else if ((flag_mask & OSF_MIND_CONTROLLED) != 0) {
                if (caster_obj != OBJ_HANDLE_NULL) {
                    if (obj_type == OBJ_TYPE_NPC) {
                        if (player_is_pc_obj(caster_obj)
                            && (obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_SUMMONED) == 0) {
                            reaction_adj(obj, caster_obj, -50);
                        }

                        obj_field_int32_set(obj, OBJ_F_SPELL_FLAGS, flags);

                        if (!critter_follow(obj, caster_obj, true)) {
                            tig_debug_printf("magictech_component_obj_flag: Error: critter_follow failed!\n");
                        }

                        ai_set_no_flee(obj);
                    }
                    return;
                }
            } else if ((flag_mask & OSF_CHARMED) != 0) {
                if (obj_type == OBJ_TYPE_NPC) {
                    if ((obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & OCF_ANIMAL) != 0) {
                        ai_hostility_end_against_group(obj, caster_obj);
                    } else if (player_is_pc_obj(caster_obj)) {
                        // CE: Bypass effects to make this bonus reversible.
                        reaction_adj_ex(obj, caster_obj, 30, false);
                    }

                    obj_field_int32_set(obj,
                        OBJ_F_NPC_FLAGS,
                        obj_field_int32_get(obj, OBJ_F_NPC_FLAGS) | ONF_KOS_OVERRIDE);
                }
            }
            break;
        case OBJ_F_CRITTER_FLAGS:
            if ((flag_mask & OCF_UNDEAD) != 0) {
                object_flags_set(obj, OF_ANIMATED_DEAD);
            } else if ((flag_mask & OCF_FLEEING) != 0) {
                ai_flee(obj, source_obj);
                return;
            } else if ((flag_mask & OCF_BLINDED) != 0) {
                combat_set_blinded(obj);
                return;
            } else if ((flag_mask & OCF_PARALYZED) != 0) {
                anim_interrupt_all_goals_of_type(obj, AG_MOVE_TO_TILE, -1);
                anim_interrupt_all_goals_of_type(obj, AG_ATTEMPT_MOVE_NEAR, -1);
            }
            break;
        case OBJ_F_CRITTER_FLAGS2:
            break;
        case OBJ_F_FLAGS:
            if ((flag_mask & OF_DONTDRAW) != 0) {
                object_flags_set(obj, OF_DONTDRAW);

                if (obj_type_is_critter(obj_type)) {
                    obj_field_int32_set(obj,
                        OBJ_F_CRITTER_FLAGS,
                        obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) | OCF_PARALYZED);
                }

                return;
            }
            break;
        }
    } else if (operation == 0) {
        if (magictech_matching_spell_flag_instances_count(obj, fld, flag_mask, false) > 1) {
            return;
        }

        flags &= ~flag_mask;

        switch (fld) {
        case OBJ_F_SPELL_FLAGS:
            if ((flag_mask & (OSF_DETECTING_INVISIBLE | OSF_DETECTING_TRAPS | OSF_DETECTING_ALIGNMENT | OSF_DETECTING_MAGIC)) != 0) {
                if (player_is_local_pc_obj(obj)) {
                    magictech_invalidate_rect_callback(NULL);
                }
            } else if ((flag_mask & OSF_INVISIBLE) != 0) {
                if (!player_is_local_pc_obj(obj)) {
                    object_flags_unset(obj, OF_INVISIBLE);
                }
                object_remove_flags(obj, OF_TRANSLUCENT);
            } else if ((flag_mask & OSF_SHRUNK) != 0) {
                object_remove_flags(obj, OF_SHRUNK);
            } else if ((flag_mask & OSF_WATER_WALKING) != 0) {
                object_flags_unset(obj, OF_WATER_WALKING);
                if (tile_is_blocking(obj_field_int64_get(obj, OBJ_F_LOCATION), false)) {
                    combat_context_init(OBJ_HANDLE_NULL, obj, &combat);
                    combat.dam_flags |= COMBAT_DAMAGE_FULL;
                    combat_dmg(&combat);

                    // FIXME: Meaningless.
                    obj_field_int32_get(combat.target_obj, OBJ_F_FLAGS);
                }
            } else if ((flag_mask & OSF_STONED) != 0) {
                object_flags_unset(obj, OF_STONED);
            } else if ((flag_mask & OSF_MIND_CONTROLLED) != 0) {
                if (obj_type == OBJ_TYPE_NPC) {
                    obj_field_int32_set(obj, OBJ_F_SPELL_FLAGS, flags);
                    if (!critter_disband(obj, true)) {
                        tig_debug_printf("magictech_component_obj_flag: Error: critter_disband failed!\n");
                    }
                }
            } else if ((flag_mask & OSF_CHARMED) != 0) {
                if (obj_type == OBJ_TYPE_NPC) {
                    // CE: Reversing charm bonus bypasses effects.
                    reaction_adj_ex(obj, caster_obj, -30, false);
                }
            }
            break;
        case OBJ_F_CRITTER_FLAGS:
            if ((flag_mask & OCF_UNDEAD) != 0) {
                object_flags_unset(obj, OF_ANIMATED_DEAD);
            } else if ((flag_mask & OCF_FLEEING) != 0) {
                ai_stop_fleeing(obj);
                return;
            }
            break;
        case OBJ_F_CRITTER_FLAGS2:
            break;
        case OBJ_F_FLAGS:
            if ((flag_mask & OF_DONTDRAW) != 0) {
                object_flags_unset(obj, OF_DONTDRAW);

                if (obj_type_is_critter(obj_type)) {
                    obj_field_int32_set(obj,
                        OBJ_F_CRITTER_FLAGS,
                        obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS) & ~OCF_PARALYZED);
                }

                if ((obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2) & OCF2_AUTO_ANIMATES) != 0) {
                    anim_goal_unconceal(obj);
                }

                return;
            }
            break;
        case OBJ_F_PORTAL_FLAGS:
            if ((flag_mask & OPF_LOCKED) != 0) {
                if (!magictech_unlock_difficulty_check(caster_obj, power_source_obj, obj)) {
                    change_allowed = false;
                    magictech_resurrection_fx_notify(caster_obj, obj);
                }

                ai_notify_portal_container_guards(caster_obj, obj, false, LOUDNESS_LOUD);
                lock_changed = true;

                if (!change_allowed) {
                    ui_hovered_object_refresh(obj);
                    return;
                }
            }
            break;
        case OBJ_F_CONTAINER_FLAGS:
            if ((flag_mask & OCOF_LOCKED) != 0) {
                if (!magictech_unlock_difficulty_check(caster_obj, power_source_obj, obj)) {
                    change_allowed = false;
                    magictech_resurrection_fx_notify(caster_obj, obj);
                }

                ai_notify_portal_container_guards(caster_obj, obj, false, LOUDNESS_LOUD);
                lock_changed = true;

                if (!change_allowed) {
                    ui_hovered_object_refresh(obj);
                    return;
                }
            }
            break;
        }
    }

    obj_field_int32_set(obj, fld, flags);

    if (lock_changed) {
        ui_hovered_object_refresh(obj);
    }
}

// 0x455100
int magictech_matching_spell_flag_instances_count(int64_t obj, int fld, unsigned int flag_mask, bool include_base_flag)
{
    unsigned int flags;
    int mt_id;
    MagicTechRunInfo* run_info;
    int cnt = 0;

    flags = obj_field_int32_get(obj, fld);

    if (include_base_flag) {
        if ((flags & flag_mask) == 0) {
            return 0;
        }
    }

    if (magictech_find_first(obj, &mt_id)) {
        do {
            if (magictech_id_to_run_info(mt_id, &run_info)
                && fld == OBJ_F_SPELL_FLAGS
                && (magictech_spells[run_info->spell].applied_spell_flags & flag_mask) != 0) {
                cnt++;
            }
        } while (magictech_find_next(obj, &mt_id));
    }

    return cnt;
}

// 0x4551C0
bool magictech_unlock_difficulty_check(int64_t caster_obj, int64_t power_source_obj, int64_t locked_obj)
{
    int lock_difficulty;

    if (caster_obj != OBJ_HANDLE_NULL
        && obj_type_is_critter(obj_field_int32_get(caster_obj, OBJ_F_TYPE))
        && !magictech_cur_is_fate_maximized) {
        switch (obj_field_int32_get(locked_obj, OBJ_F_TYPE)) {
        case OBJ_TYPE_PORTAL:
            lock_difficulty = obj_field_int32_get(locked_obj, OBJ_F_PORTAL_LOCK_DIFFICULTY);
            break;
        case OBJ_TYPE_CONTAINER:
            lock_difficulty = obj_field_int32_get(locked_obj, OBJ_F_CONTAINER_LOCK_DIFFICULTY);
            break;
        default:
            return false;
        }

        if (lock_difficulty >= 0 && magictech_cur_run_info->parent_obj.aptitude < lock_difficulty) {
            return false;
        }
    }

    return true;
}

// 0x455250
void magictech_duration_adjust_for_aptitude_and_mastery(MagicTechRunInfo* run_info, DateTime* datetime)
{
    unsigned int millis;
    int source_aptitude;
    int target_aptitude;
    int delta;

    if (run_info->parent_obj.obj == OBJ_HANDLE_NULL) {
        return;
    }

    if (!obj_type_is_critter(run_info->parent_obj.type)) {
        return;
    }

    millis = datetime->milliseconds;

    if (run_info->spell >= 0
        && run_info->spell < MAGICTECH_PLAYER_SPELL_COUNT
        && spell_mastery_get(run_info->parent_obj.obj) == COLLEGE_FROM_SPELL(run_info->spell)) {
        millis *= 2;
    }

    if (run_info->target_obj.obj != OBJ_HANDLE_NULL
        && obj_type_is_critter(run_info->target_obj.type)) {
        source_aptitude = stat_level_get(run_info->parent_obj.obj, STAT_MAGICK_TECH_APTITUDE);
        target_aptitude = stat_level_get(run_info->target_obj.obj, STAT_MAGICK_TECH_APTITUDE);

        if (millis != 0) {
            if (source_aptitude > target_aptitude) {
                delta = source_aptitude - target_aptitude;
            } else {
                delta = target_aptitude - source_aptitude;
            }

            if (delta < 0) {
                millis = 100 * millis / (delta + 100);
            }
        }
    }

    datetime->milliseconds = millis;
}

// 0x455350
void magictech_move_object_along_path(int64_t obj, int64_t target_loc)
{
    int64_t source_loc;
    AnimPath path;
    PathCreateInfo path_create_info;

    source_loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    path.rotation_capacity = sizeof(path.rotations) / sizeof(path.rotations[0]);
    anim_path_init(&path);

    path_create_info.to = target_loc;
    path_create_info.from = source_loc;
    path_create_info.obj = obj;
    path_create_info.max_rotations = sizeof(path.rotations) / sizeof(path.rotations[0]);
    path_create_info.rotations = path.rotations;
    path_create_info.flags = PATH_FLAG_IGNORE_CRITTERS;
    path.max = path_make(&path_create_info);

    if (path.max != 0) {
        for (path.curr = 0; path.curr < path.max; path.curr++) {
            if (!location_in_dir(source_loc, path.rotations[path.curr], &source_loc)) {
                break;
            }

            tile_script_exec(source_loc, obj);
        }

        anim_object_interrupt_by_priority(obj, 5, false, true);
        object_move_to_location(obj, source_loc, 0, 0);

        if (player_is_local_pc_obj(obj)) {
            location_origin_set(source_loc);
        }
    }

    anim_path_destroy(&path);
}

// 0x4554B0
void magictech_summoned_object_add(MagicTechRunInfo* run_info, int64_t obj)
{
    MagicTechObjectNode* node;

    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    node = run_info->summoned_obj;

    run_info->summoned_obj = mt_obj_node_create();
    run_info->summoned_obj->next = node;
    run_info->summoned_obj->obj = obj;
    run_info->summoned_obj->type = obj_field_int32_get(obj, OBJ_F_TYPE);
    if (obj_type_is_critter(run_info->summoned_obj->type)) {
        run_info->summoned_obj->aptitude = stat_level_get(obj, STAT_MAGICK_TECH_APTITUDE);
    }

    object_reference_init(obj, &(run_info->summoned_obj->reference));
}

// 0x455550
bool magictech_target_antimagic_and_reflection_resolve(TargetContext* target_ctx, MagicTechRunInfo* run_info)
{
    MesFileEntry mes_file_entry;
    int64_t parent_obj;

    target_ctx->target_spell_flags = 0;

    // FIX: Move the self-targeting spell check below, after handling the
    // effects of the "Dweomer Shield". This prevents the execution of end
    // actions for spells that didn't even have a chance to begin (the result of
    // interrupting a spell during the initial processing).
    if (target_ctx->target_obj == OBJ_HANDLE_NULL) {
        return true;
    }

    target_ctx->target_spell_flags = obj_field_int32_get(target_ctx->target_obj, OBJ_F_SPELL_FLAGS);

    if ((target_ctx->target_spell_flags & OSF_ANTI_MAGIC_SHELL) != 0
        && (run_info->applied_spell_flags & 0x800) == 0) {
        // FIX: Gracefully complete spells when dispelled by the "Dweomer
        // Shield".
        if ((run_info->flags & MAGICTECH_RUN_DISPELLED) != 0
            && run_info->action >= MAGICTECH_ACTION_END) {
            return true;
        }

        if (player_is_local_pc_obj(run_info->parent_obj.obj)) {
            mes_file_entry.num = 603; // "The effect is nullified."
            mes_get_msg(magictech_spell_mes_file, &mes_file_entry);
            ui_display_warning(mes_file_entry.str);
            ui_spell_maintain_end(run_info->id);
        }
        return false;
    }

    if (target_ctx->target_obj == magictech_cur_run_info->parent_obj.obj) {
        return true;
    }

    if (((target_ctx->target_spell_flags & OSF_FULL_REFLECTION) == 0
            && (run_info->flags & MAGICTECH_RUN_REFLECTED) == 0)
        || (magictech_cur_spell_info->flags & MAGICTECH_NO_REFLECT) != 0) {
        return true;
    }

    // FIX: Gracefully complete spells that were cast before the "Reflection
    // Shield" was applied.
    if ((run_info->flags & MAGICTECH_RUN_REFLECTED) == 0
        && run_info->action >= MAGICTECH_ACTION_END) {
        return true;
    }

    run_info->flags |= MAGICTECH_RUN_REFLECTED;

    if ((run_info->applied_spell_flags & 0x2000) == 0
        && magictech_spell_flag_caster_find(target_ctx->target_obj, OSF_FULL_REFLECTION, &parent_obj)) {
        magictech_cost_check_or_consume(parent_obj, magictech_last_checked_cost, true, 10000);
    }

    if (target_ctx->target_obj == run_info->source_obj.obj) {
        return true;
    }

    if ((run_info->applied_spell_flags & 0x2000) != 0) {
        return true;
    }

    if ((target_ctx->source_spell_flags & 0x2000) == 0) {
        int64_t source_obj = run_info->source_obj.obj;
        run_info->source_obj.obj = target_ctx->target_obj;
        target_ctx->target_obj = source_obj;
        return true;
    }

    if (player_is_local_pc_obj(run_info->parent_obj.obj)) {
        mes_file_entry.num = 602; // "You lose your concentration."
        mes_get_msg(magictech_spell_mes_file, &mes_file_entry);
        ui_display_warning(mes_file_entry.str);
        ui_spell_maintain_end(run_info->id);
    }

    return false;
}

// 0x455710
void magictech_run_slots_reset(void)
{
    int index;
    MagicTechRunInfo* run_info;

    for (index = 0; index < 512; index++) {
        run_info = &(magictech_run_info[index]);
        run_info->source_obj.obj = OBJ_HANDLE_NULL;
        run_info->id = -1;
        run_info->flags = 0;
    }
}

// 0x455740
void magictech_id_new_lock(MagicTechRunInfo** run_info_ptr)
{
    int index;

    for (index = 0; index < 512; index++) {
        if (magictech_run_info[index].id == -1) {
            magictech_run_info[index].id = index;
            magictech_run_info[index].flags = MAGICTECH_RUN_ACTIVE;
            magictech_run_info[index].action = MAGICTECH_ACTION_BEGIN;
            *run_info_ptr = &(magictech_run_info[index]);
            magictech_allocated_run_count++;
            return;
        }
    }

    tig_debug_printf("magictech_id_new_lock: Error: ran out of id's!\n");
    exit(EXIT_FAILURE);
}

// 0x455820
bool magictech_run_object_handles_recover(MagicTechRunInfo* run_info)
{
    bool success = true;
    MagicTechObjectNode* node;

    if (!object_handle_recover_if_stale(&(run_info->source_obj.obj), &(run_info->source_obj.reference))) {
        success = false;
    }

    if (!object_handle_recover_if_stale(&(run_info->parent_obj.obj), &(run_info->parent_obj.reference))) {
        success = false;
    }

    node = run_info->objlist;
    while (node != NULL) {
        if (!object_handle_recover_if_stale(&(node->obj), &(node->reference))) {
            success = false;
        }
        node = node->next;
    }

    node = run_info->summoned_obj;
    while (node != NULL) {
        if (!object_handle_recover_if_stale(&(node->obj), &(node->reference))) {
            success = false;
        }
        node = node->next;
    }

    return success;
}

// 0x4558D0
void magictech_id_free_lock(int mt_id)
{
    MagicTechRunInfo* run_info;
    Packet54 pkt;

    run_info = &(magictech_run_info[mt_id]);
    if (run_info->id != -1) {
        magictech_selected_timeevent_run_id = run_info->id;
        timeevent_clear_one_ex(TIMEEVENT_TYPE_MAGICTECH, magictech_maintenance_timeevent_matches_current);
        magictech_selected_timeevent_run_id = -1;

        magictech_run_object_lists_release(run_info);
        magictech_allocated_run_count--;

        if (tig_net_is_active()
            && tig_net_is_host()) {
            pkt.type = 54;
            pkt.magictech_id = mt_id;
            tig_net_send_app_all(&pkt, sizeof(pkt));
        }
    }
}

// 0x455960
void magictech_run_object_lists_release(MagicTechRunInfo* run_info)
{
    MagicTechObjectNode* node;
    MagicTechObjectNode* next;

    if (run_info->id != -1) {
        run_info->id = -1;
        run_info->source_obj.obj = OBJ_HANDLE_NULL;
        run_info->parent_obj.obj = OBJ_HANDLE_NULL;

        node = run_info->objlist;
        while (node != NULL) {
            next = node->next;
            mt_obj_node_destroy(node);
            node = next;
        }
        run_info->objlist = NULL;

        node = run_info->summoned_obj;
        while (node != NULL) {
            next = node->next;
            mt_obj_node_destroy(node);
            node = next;
        }
        run_info->summoned_obj = NULL;

        run_info->flags = 0;
    }
}

// 0x4559E0
void magictech_run_slot_clear(MagicTechRunInfo* run_info)
{
    if (run_info->id != -1) {
        run_info->id = -1;
        run_info->source_obj.obj = OBJ_HANDLE_NULL;
        run_info->parent_obj.obj = OBJ_HANDLE_NULL;
        run_info->objlist = NULL;
        run_info->summoned_obj = NULL;
        run_info->flags = 0;
    }
}

// 0x455A20
void magictech_invocation_init(MagicTechInvocation* mt_invocation, int64_t obj, int spell)
{
    mt_invocation->spell = spell;
    object_handle_reference_init(obj, &(mt_invocation->source_obj));
    if (obj != OBJ_HANDLE_NULL) {
        mt_invocation->loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    } else {
        mt_invocation->loc = 0;
    }
    object_handle_reference_init(OBJ_HANDLE_NULL, &(mt_invocation->target_obj));
    mt_invocation->target_loc = 0;
    object_handle_reference_init(magictech_inventory_owner_or_self(obj), &(mt_invocation->parent_obj));
    object_handle_reference_init(OBJ_HANDLE_NULL, &(mt_invocation->attacker_obj));
    mt_invocation->trigger = 0;
    mt_invocation->flags = 0;
}

// 0x455AC0
void magictech_invocation_run(MagicTechInvocation* mt_invocation)
{
    if (magictech_editor) {
        return;
    }

    if (tig_net_is_active()) {
        if (tig_net_is_host()) {
            PacketPlayerCastSpell pkt;

            pkt.type = 58;
            pkt.player = multiplayer_find_slot_from_obj(mt_invocation->source_obj.obj);
            object_handle_reference_init(mt_invocation->source_obj.obj, &(mt_invocation->source_obj));
            object_handle_reference_init(mt_invocation->parent_obj.obj, &(mt_invocation->parent_obj));
            object_handle_reference_init(mt_invocation->target_obj.obj, &(mt_invocation->target_obj));
            object_handle_reference_init(mt_invocation->attacker_obj.obj, &(mt_invocation->attacker_obj));
            pkt.invocation = *mt_invocation;
            tig_net_send_app_all(&pkt, sizeof(pkt));
            magictech_invocation_start(mt_invocation);
        } else {
            PacketPlayerRequestCastSpell pkt;

            pkt.type = 57;
            pkt.player = multiplayer_find_slot_from_obj(mt_invocation->source_obj.obj);
            object_handle_reference_init(mt_invocation->source_obj.obj, &(mt_invocation->source_obj));
            object_handle_reference_init(mt_invocation->parent_obj.obj, &(mt_invocation->parent_obj));
            object_handle_reference_init(mt_invocation->target_obj.obj, &(mt_invocation->target_obj));
            object_handle_reference_init(mt_invocation->attacker_obj.obj, &(mt_invocation->attacker_obj));
            pkt.invocation = *mt_invocation;
            tig_net_send_app_all(&pkt, sizeof(pkt));
        }
    } else {
        magictech_invocation_start(mt_invocation);
    }
}

// 0x455C30
void magictech_invocation_start(MagicTechInvocation* mt_invocation)
{
    MagicTechInfo* info;
    MagicTechRunInfo* run_info;
    TimeEvent timeevent;
    DateTime datetime;

    if (magictech_editor) {
        return;
    }

    if (!magictech_invocation_check(mt_invocation)) {
        return;
    }

    info = &(magictech_spells[mt_invocation->spell]);
    magictech_id_new_lock(&run_info);

    run_info->source_obj.obj = mt_invocation->source_obj.obj;
    object_reference_init(run_info->source_obj.obj, &(run_info->source_obj.reference));
    if (run_info->source_obj.obj != OBJ_HANDLE_NULL) {
        run_info->source_obj.type = obj_field_int32_get(run_info->source_obj.obj, OBJ_F_TYPE);
        if (obj_type_is_critter(run_info->source_obj.type)) {
            run_info->source_obj.aptitude = stat_level_get(run_info->source_obj.obj, STAT_MAGICK_TECH_APTITUDE);
        }
    } else {
        run_info->source_obj.type = -1;
    }
    run_info->source_obj.loc = mt_invocation->loc;

    run_info->parent_obj.obj = mt_invocation->parent_obj.obj;
    object_reference_init(run_info->parent_obj.obj, &(run_info->parent_obj.reference));
    if (run_info->parent_obj.obj != OBJ_HANDLE_NULL) {
        run_info->parent_obj.type = obj_field_int32_get(run_info->parent_obj.obj, OBJ_F_TYPE);
        if (obj_type_is_critter(run_info->parent_obj.type)) {
            run_info->parent_obj.aptitude = stat_level_get(run_info->parent_obj.obj, STAT_MAGICK_TECH_APTITUDE);
        }
    } else {
        run_info->parent_obj.type = -1;
    }
    run_info->parent_obj.loc = mt_invocation->loc;

    run_info->target_obj.obj = mt_invocation->target_obj.obj;
    object_reference_init(run_info->target_obj.obj, &(run_info->target_obj.reference));
    if (run_info->target_obj.obj != OBJ_HANDLE_NULL) {
        run_info->target_obj.type = obj_field_int32_get(run_info->target_obj.obj, OBJ_F_TYPE);
        if (obj_type_is_critter(run_info->target_obj.type)) {
            run_info->target_obj.aptitude = stat_level_get(run_info->target_obj.obj, STAT_MAGICK_TECH_APTITUDE);
        }
    } else {
        run_info->target_obj.type = -1;
    }
    run_info->target_obj.loc = mt_invocation->target_loc;

    run_info->attacker_obj.obj = mt_invocation->attacker_obj.obj;
    object_reference_init(run_info->attacker_obj.obj, &(run_info->attacker_obj.reference));
    if (run_info->attacker_obj.obj != OBJ_HANDLE_NULL) {
        run_info->attacker_obj.type = obj_field_int32_get(run_info->attacker_obj.obj, OBJ_F_TYPE);
        if (obj_type_is_critter(run_info->attacker_obj.type)) {
            run_info->attacker_obj.aptitude = stat_level_get(run_info->attacker_obj.obj, STAT_MAGICK_TECH_APTITUDE);
        }
    } else {
        run_info->attacker_obj.type = -1;
    }

    run_info->spell = mt_invocation->spell;
    run_info->action = MAGICTECH_ACTION_BEGIN;
    run_info->objlist = NULL;
    run_info->summoned_obj = NULL;

    if ((mt_invocation->flags & MAGICTECH_INVOCATION_FREE) != 0) {
        run_info->flags |= MAGICTECH_RUN_FREE;
    }

    if ((mt_invocation->flags & MAGICTECH_INVOCATION_UNRESISTABLE) != 0) {
        run_info->flags |= MAGICTECH_RUN_UNRESISTABLE;
    }

    if ((mt_invocation->flags & MAGICTECH_INVOCATION_UTILITY_AI) != 0) {
        run_info->flags |= MAGICTECH_RUN_UTILITY_AI;
    }

    run_info->applied_spell_flags = 0;
    run_info->trigger = mt_invocation->trigger;
    run_info->resistance_amount = 0;
    run_info->remaining_duration_triggers = info->duration_trigger_count;

    if ((info->flags & MAGICTECH_NO_RESIST) != 0) {
        run_info->flags |= MAGICTECH_RUN_UNRESISTABLE;
    }

    if (run_info->source_obj.loc != 0
        && run_info->parent_obj.obj != OBJ_HANDLE_NULL) {
        tig_art_id_t art_id;
        int64_t loc;
        int rot;

        art_id = obj_field_int32_get(run_info->parent_obj.obj, OBJ_F_CURRENT_AID);
        art_id = tig_art_id_frame_set(art_id, 0);

        loc = run_info->target_obj.obj != OBJ_HANDLE_NULL
            ? obj_field_int64_get(run_info->target_obj.obj, OBJ_F_LOCATION)
            : run_info->target_obj.loc;
        rot = location_rot(run_info->source_obj.loc, loc);
        tig_art_id_rotation_set(art_id, rot);
    }

    magictech_preload_art(run_info);

    if (run_info->parent_obj.obj != OBJ_HANDLE_NULL) {
        int anim;
        AnimGoalData goal_data;
        AnimID anim_id;

        anim = info->casting_anim != -1
            ? AG_THROW_SPELL_W_CAST_ANIM
            : AG_THROW_SPELL;

        if ((mt_invocation->flags & MAGICTECH_INVOCATION_FRIENDLY) != 0) {
            if (anim_goal_data_init_without_interrupt(&goal_data, run_info->parent_obj.obj, AG_THROW_SPELL_FRIENDLY)) {
                goal_data.params[AGDATA_SPELL_DATA].data = run_info->id;
                goal_data.params[AGDATA_TARGET_OBJ].obj = run_info->target_obj.obj;
                goal_data.params[AGDATA_TARGET_TILE].obj = run_info->target_obj.loc;
                goal_data.params[AGDATA_ANIM_ID].data = TIG_ART_ID_INVALID;

                if (tig_net_is_active() && !tig_net_is_host()) {
                    return;
                }

                if (anim_goal_add(&goal_data, &anim_id)) {
                    turn_on_flags(anim_id, 0x200000, 0);
                    return;
                }
            }
        } else if (anim_is_current_goal_type(run_info->parent_obj.obj, AG_THROW_SPELL, &anim_id)) {
            if (num_goal_subslots_in_use(&anim_id) < 4
                && !combat_turn_based_is_active()
                && anim_goal_data_init_without_interrupt(&goal_data, run_info->parent_obj.obj, anim)) {
                goal_data.params[AGDATA_SPELL_DATA].data = run_info->id;
                goal_data.params[AGDATA_TARGET_OBJ].obj = run_info->target_obj.obj;
                goal_data.params[AGDATA_TARGET_TILE].obj = run_info->target_obj.loc;
                goal_data.params[AGDATA_ANIM_ID].data = TIG_ART_ID_INVALID;

                if (info->casting_anim != -1) {
                    tig_art_id_t art_id = obj_field_int32_get(run_info->parent_obj.obj, OBJ_F_CURRENT_AID);
                    art_id = tig_art_id_frame_set(art_id, 0);
                    art_id = tig_art_id_anim_set(art_id, info->casting_anim);
                    goal_data.params[AGDATA_ANIM_ID].data = art_id;
                }

                if (tig_net_is_active() && !tig_net_is_host()) {
                    return;
                }

                // __FILE__: "C:\Troika\Code\Game\GameLibX\MagicTech.c"
                // __LINE__: 6893
                if (anim_subgoal_add(anim_id, &goal_data, __FILE__, __LINE__)) {
                    return;
                }
            }
        } else if (anim_goal_data_init_with_interrupt(&goal_data, run_info->parent_obj.obj, anim)) {
            goal_data.params[AGDATA_SPELL_DATA].data = run_info->id;
            goal_data.params[AGDATA_TARGET_OBJ].obj = run_info->target_obj.obj;
            goal_data.params[AGDATA_TARGET_TILE].obj = run_info->target_obj.loc;
            goal_data.params[AGDATA_ANIM_ID].data = TIG_ART_ID_INVALID;

            if (info->casting_anim != -1) {
                tig_art_id_t art_id = obj_field_int32_get(run_info->parent_obj.obj, OBJ_F_CURRENT_AID);
                art_id = tig_art_id_frame_set(art_id, 0);
                art_id = tig_art_id_anim_set(art_id, info->casting_anim);
                goal_data.params[AGDATA_ANIM_ID].data = art_id;
            }

            if (tig_net_is_active() && !tig_net_is_host()) {
                return;
            }

            if (anim_goal_add(&goal_data, &anim_id)) {
                if (info->casting_anim != -1) {
                    if (anim_goal_data_init_without_interrupt(&goal_data, run_info->parent_obj.obj, AG_THROW_SPELL_W_CAST_ANIM_2NDARY)) {
                        goal_data.params[AGDATA_SPELL_DATA].data = run_info->id;
                        goal_data.params[AGDATA_TARGET_OBJ].obj = run_info->target_obj.obj;
                        goal_data.params[AGDATA_TARGET_TILE].obj = run_info->target_obj.loc;
                        goal_data.params[AGDATA_ANIM_ID].data = TIG_ART_ID_INVALID;
                        anim_goal_add(&goal_data, &anim_id);
                    }
                }
                return;
            }
        }
    } else {
        if (magictech_projectile_art_id_get(run_info->id) == -1) {
            magictech_secondary_casting_fx_play(run_info->id);
        }

        timeevent.type = TIMEEVENT_TYPE_MAGICTECH;
        timeevent.params[0].integer_value = run_info->id;
        timeevent.params[2].integer_value = 3;
        if (combat_turn_based_is_active()) {
            datetime = game_time_get();
            if (timeevent_add_base(&timeevent, &datetime)) {
                return;
            }
        } else {
            datetime_init(&datetime, 2);
            if (timeevent_add_delay(&timeevent, &datetime)) {
                return;
            }
        }
    }

    magictech_id_free_lock(run_info->id);
}

// 0x456430
bool magictech_disallowed_flags_check(int64_t caster_obj, int64_t target_obj, MagicTechInfo* magictech)
{
    if (caster_obj == OBJ_HANDLE_NULL) {
        return true;
    }

    if ((obj_field_int32_get(caster_obj, OBJ_F_SPELL_FLAGS) & magictech->disallowed_sf) != 0) {
        return false;
    }

    if ((obj_field_int32_get(target_obj, OBJ_F_SPELL_FLAGS) & magictech->disallowed_tsf) != 0) {
        return false;
    }

    if (obj_type_is_critter(obj_field_int32_get(target_obj, OBJ_F_TYPE))
        && (obj_field_int32_get(target_obj, OBJ_F_CRITTER_FLAGS) & magictech->disallowed_tcf) != 0) {
        return false;
    }

    return true;
}

// 0x4564E0
bool magictech_invocation_check(MagicTechInvocation* mt_invocation)
{
    MagicTechInfo* info;
    MesFileEntry mes_file_entry;

    info = &(magictech_spells[mt_invocation->spell]);

    if (mt_invocation->parent_obj.obj != OBJ_HANDLE_NULL
        && combat_turn_based_is_active()
        && combat_turn_based_whos_turn_get() != mt_invocation->parent_obj.obj
        && obj_type_is_critter(obj_field_int32_get(mt_invocation->parent_obj.obj, OBJ_F_TYPE))) {
        return false;
    }

    if (mt_invocation->spell < 0 || mt_invocation->spell >= MT_SPELL_COUNT) {
        tig_debug_printf("MagicTech: Activate: ERROR: NOT A SPELL!\n");
        return false;
    }

    if (mt_invocation->parent_obj.obj != OBJ_HANDLE_NULL) {
        int64_t parent_loc = obj_field_int64_get(mt_invocation->parent_obj.obj, OBJ_F_LOCATION);
        if (!magictech_target_is_in_range(parent_loc, mt_invocation->target_loc, mt_invocation->target_obj.obj, mt_invocation->spell)) {
            return false;
        }
    }

    if (!magictech_check_los(mt_invocation)) {
        return false;
    }

    if (!magictech_disallowed_flags_check(mt_invocation->parent_obj.obj, mt_invocation->target_obj.obj, info)) {
        return false;
    }

    if (mt_invocation->parent_obj.obj != OBJ_HANDLE_NULL
        && (obj_field_int32_get(mt_invocation->parent_obj.obj, OBJ_F_SPELL_FLAGS) & OSF_BONDS_OF_MAGIC) != 0
        && mt_invocation->parent_obj.obj == mt_invocation->source_obj.obj) {
        return false;
    }

    if (!player_is_pc_obj(mt_invocation->parent_obj.obj)) {
        TargetParams target_params;
        TargetContext target_ctx;
        TargetList targets;
        int idx;
        uint64_t* tgt;

        target_params_init(&target_params);
        magictech_target_flags_pointer_get(mt_invocation->spell, &tgt);
        target_params.tgt = *tgt;
        target_params.radius = 2;

        target_context_init(&target_ctx, NULL, mt_invocation->source_obj.obj);
        target_ctx.orig_target_loc = mt_invocation->target_loc;
        target_ctx.target_loc = mt_invocation->target_loc;
        target_ctx.source_loc = mt_invocation->loc;
        target_ctx.orig_target_obj = mt_invocation->target_obj.obj;
        target_ctx.target_obj = mt_invocation->target_obj.obj;
        target_ctx.summoned_obj = OBJ_HANDLE_NULL;
        target_ctx.params = &target_params;

        if (!target_context_evaluate(&target_ctx)) {
            if (mt_invocation->target_obj.obj == OBJ_HANDLE_NULL) {
                return false;
            }

            if ((target_params.tgt & TGT_TILE) == 0) {
                return false;
            }

            target_ctx.orig_target_loc = obj_field_int64_get(mt_invocation->target_obj.obj, OBJ_F_LOCATION);
            target_ctx.target_loc = obj_field_int64_get(mt_invocation->target_obj.obj, OBJ_F_LOCATION);
            target_ctx.targets = &targets;
            target_params.tgt |= TGT_TILE_RADIUS | TGT_TILE_INDOOR_OR_OUTDOOR_MATCH;
            target_context_build_list(&target_ctx);

            for (idx = 0; idx < target_ctx.targets->cnt; idx++) {
                if (target_ctx.targets->entries[idx].loc != 0) {
                    target_ctx.target_loc = target_ctx.targets->entries[idx].loc;
                    if (target_context_evaluate(&target_ctx)) {
                        break;
                    }
                }
            }

            if (idx >= target_ctx.targets->cnt) {
                return false;
            }

            mt_invocation->target_loc = target_ctx.target_loc;
            mt_invocation->target_obj.obj = OBJ_HANDLE_NULL;
            mt_invocation->target_obj.reference.objid.type = OID_TYPE_NULL;
        }
    }

    if ((info->flags & MAGICTECH_IS_TECH) == 0) {
        int source_aptitude = mt_invocation->parent_obj.obj != OBJ_HANDLE_NULL
            ? stat_level_get(mt_invocation->parent_obj.obj, STAT_MAGICK_TECH_APTITUDE)
            : 0;
        int target_aptitude = mt_invocation->target_obj.obj != OBJ_HANDLE_NULL
            ? stat_level_get(mt_invocation->target_obj.obj, STAT_MAGICK_TECH_APTITUDE)
            : 0;

        if (mt_invocation->source_obj.obj != OBJ_HANDLE_NULL
            && source_aptitude < 0
            && mt_invocation->parent_obj.obj != mt_invocation->source_obj.obj
            && item_effective_power(mt_invocation->source_obj.obj, mt_invocation->parent_obj.obj) <= 0) {
            magictech_resurrection_fx_notify(mt_invocation->parent_obj.obj, mt_invocation->target_obj.obj);
            return false;
        }

        if (target_aptitude < 0) {
            if (source_aptitude >= 0) {
                target_aptitude = target_aptitude * (100 - source_aptitude) / 100;
            }
            if (random_between(1, 100) <= target_aptitude) {
                magictech_resurrection_fx_notify(mt_invocation->parent_obj.obj, mt_invocation->target_obj.obj);
                return false;
            }
        }
    }

    if (mt_invocation->parent_obj.obj != OBJ_HANDLE_NULL
        && obj_type_is_critter(obj_field_int32_get(mt_invocation->parent_obj.obj, OBJ_F_TYPE))
        && mt_invocation->parent_obj.obj == mt_invocation->source_obj.obj
        && stat_level_get(mt_invocation->parent_obj.obj, STAT_INTELLIGENCE) < magictech_min_intelligence(mt_invocation->spell)) {
        return false;
    }

    if (mt_invocation->parent_obj.obj != OBJ_HANDLE_NULL
        && mt_invocation->spell == SPELL_TELEPORTATION
        && !antiteleport_check(mt_invocation->parent_obj.obj, 0)) {
        if (player_is_local_pc_obj(mt_invocation->parent_obj.obj)) {
            mes_file_entry.num = 10000; // "This place seems to block your attempt to teleport."
            mes_get_msg(magictech_spell_mes_file, &mes_file_entry);
            ui_display_warning(mes_file_entry.str);
        }
        return false;
    }

    return true;
}

// 0x456A10
bool magictech_item_effective_power_check(int64_t caster_obj, int64_t parent_obj, int64_t item_obj)
{
    int aptitude;

    if (caster_obj == OBJ_HANDLE_NULL) {
        return true;
    }

    if (obj_field_int32_get(item_obj, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY) <= 0) {
        return true;
    }

    aptitude = stat_level_get(caster_obj, STAT_MAGICK_TECH_APTITUDE);

    // FIXME: Useless.
    if (parent_obj != OBJ_HANDLE_NULL) {
        stat_level_get(parent_obj, STAT_MAGICK_TECH_APTITUDE);
    }

    if (aptitude > 0) {
        return true;
    }

    if (item_effective_power(item_obj, caster_obj) > 0) {
        return true;
    }

    return false;
}

// 0x456A90
bool magictech_run_target_revalidate(int mt_id)
{
    MagicTechRunInfo* run_info;
    uint64_t* tgt_ptr;
    uint64_t prev_tgt;
    TargetDescriptor td;
    TargetDescriptorEvaluation evaluation;
    bool rc;

    if (tig_net_is_active()
        && !tig_net_is_host()) {
        return true;
    }

    if (!magictech_id_to_run_info(mt_id, &run_info)) {
        return false;
    }

    magictech_target_flags_pointer_get(run_info->spell, &tgt_ptr);

    if (*tgt_ptr == TGT_NONE
        || ((*tgt_ptr & TGT_SELF) != 0
            && (*tgt_ptr & TGT_TILE) == 0)
        || *tgt_ptr == TGT_OBJ_RADIUS) {
        return true;
    }

    prev_tgt = target_flags_get();
    target_flags_set(*tgt_ptr);
    if (run_info->target_obj.obj != OBJ_HANDLE_NULL) {
        target_descriptor_set_obj(&td, run_info->target_obj.obj);
    } else {
        target_descriptor_set_loc(&td, run_info->target_obj.loc);
    }

    evaluation.source_obj = run_info->source_obj.obj;
    evaluation.self_obj = run_info->parent_obj.obj;
    evaluation.descriptor = &td;
    rc = target_descriptor_evaluate_current(&evaluation);
    target_flags_set(prev_tgt);

    return rc;
}

// 0x456BC0
bool magictech_check_los(MagicTechInvocation* mt_invocation)
{
    int64_t loc;
    int64_t blocking_obj;

    if (mt_invocation->parent_obj.obj == mt_invocation->target_obj.obj || mt_invocation->parent_obj.obj == OBJ_HANDLE_NULL) {
        return true;
    }

    loc = mt_invocation->target_loc;
    if (loc == 0) {
        if (mt_invocation->target_obj.obj != OBJ_HANDLE_NULL) {
            if (obj_type_is_item(obj_field_int32_get(mt_invocation->target_obj.obj, OBJ_F_TYPE))
                && item_parent(mt_invocation->target_obj.obj, NULL)) {
                return true;
            }

            loc = obj_field_int64_get(mt_invocation->target_obj.obj, OBJ_F_LOCATION);
        }
    }

    if (loc != 0) {
        if (ai_straight_path_cost_to_location(mt_invocation->parent_obj.obj, loc, &blocking_obj) >= 100) {
            return false;
        }

        if (blocking_obj != OBJ_HANDLE_NULL
            && (obj_field_int32_get(blocking_obj, OBJ_F_FLAGS) & OF_SHOOT_THROUGH) == 0
            && blocking_obj != mt_invocation->target_obj.obj) {
            return false;
        }
    }

    return true;
}

// 0x456CD0
void magictech_preload_art(MagicTechRunInfo* run_info)
{
    int type;
    AnimFxNode node;

    for (type = 0; type < MAGICTECH_EYE_CANDY_TYPE_COUNT; type++) {
        animfx_node_init(&spell_eye_candies,
            &node,
            run_info->parent_obj.obj,
            run_info->id,
            6 * run_info->spell + type);
        animfx_preload_art(&node);
    }
}

// 0x456D20
bool magictech_projectile_effect_describe(int mt_id, tig_art_id_t* art_id_ptr, tig_art_id_t* light_art_id_ptr, tig_color_t* light_color_ptr, int* foreground_overlay_index, int* background_overlay_index, int* light_overlay_index, int* effect_list_index)
{
    MagicTechRunInfo* run_info;
    AnimFxNode node;

    if (!magictech_id_to_run_info(mt_id, &run_info)) {
        return false;
    }

    animfx_node_init(&spell_eye_candies,
        &node,
        run_info->parent_obj.obj,
        run_info->id,
        6 * run_info->spell);

    if (node.obj != OBJ_HANDLE_NULL) {
        node.rotation = tig_art_id_rotation_get(obj_field_int32_get(node.obj, OBJ_F_CURRENT_AID));
    }

    node.art_id_ptr = art_id_ptr;
    node.light_art_id_ptr = light_art_id_ptr;
    node.light_color_ptr = light_color_ptr;

    if (!animfx_add(&node)) {
        return false;
    }

    *foreground_overlay_index = node.overlay_fore_index;
    *background_overlay_index = node.overlay_back_index;
    *light_overlay_index = node.overlay_light_index;
    *effect_list_index = animfx_list_find(&spell_eye_candies);

    return true;
}

// 0x456E00
void magictech_secondary_casting_fx_play(int mt_id)
{
    MagicTechRunInfo* run_info;
    AnimFxNode node;

    if (magictech_id_to_run_info(mt_id, &run_info)) {
        animfx_node_init(&spell_eye_candies,
            &node,
            run_info->parent_obj.obj,
            run_info->id,
            6 * run_info->spell + MAGICTECH_EYE_CANDY_TYPE_SECONDARY_CASTING);
        node.animate = true;
        animfx_add(&node);
    }
}

// 0x456E60
void magictech_fx_add(int64_t obj, int fx)
{
    AnimFxNode node;

    animfx_node_init(&spell_eye_candies, &node, obj, -1, fx % 10 + 6 * (fx / 10));
    node.animate = true;
    animfx_add(&node);
}

// 0x456EC0
void magictech_fx_remove(int64_t obj, int fx)
{
    if (tig_net_is_active() && !tig_net_is_host()) {
        return;
    }

    animfx_remove(&spell_eye_candies, obj, fx % 10 + 6 * (fx / 10), -1);

    if (tig_net_is_active()) {
        PacketMagicTechEyeCandy pkt;

        pkt.type = 77;
        pkt.subtype = 0;
        pkt.oid = obj_get_id(obj);
        pkt.fx_id = fx % 10 + 6 * (fx / 10);
        pkt.mt_id = -1;
        tig_net_send_app_all(&pkt, sizeof(pkt));
    }
}

// 0x456F70
void magictech_begin_action_process(int mt_id)
{
    MagicTechRunInfo* run_info;

    if (magictech_id_to_run_info(mt_id, &run_info)) {
        run_info->action = MAGICTECH_ACTION_BEGIN;
        magictech_run_process(run_info);
    }
}

// 0x456FA0
void magictech_casting_complete_and_release(int mt_id, unsigned int flags)
{
    if (mt_id != -1
        && (!tig_net_is_active()
            || tig_net_is_host())) {
        if ((flags & 0x1) != 0) {
            magictech_casting_cost_consume(magictech_run_info[mt_id].source_obj.obj,
                magictech_run_info[mt_id].spell);
        }
        magictech_id_free_lock(mt_id);
    }
}

// 0x457000
void magictech_action_process(int mt_id, int action)
{
    MagicTechRunInfo* run_info;

    if (magictech_id_to_run_info(mt_id, &run_info)) {
        run_info->action = action;
        magictech_run_process(run_info);
    }
}

// 0x457030
void magictech_run_callback_action_process(int mt_id, int action)
{
    MagicTechRunInfo* run_info;

    if (magictech_id_to_run_info(mt_id, &run_info)) {
        run_info->action = action;
        magictech_run_callback_process(run_info);
    }
}

// 0x457060
void magictech_run_callback_process(MagicTechRunInfo* run_info)
{
    if (magictech_cur_id != -1 && magictech_cur_id != run_info->id) {
        tig_debug_printf("\n\nMagicTech: ERROR: Process function is NOT Re-Entrant, Spell: %d (%s)!\n",
            run_info->spell,
            magictech_get_name(run_info->spell));
        return;
    }

    magictech_cur_run_info = run_info;
    magictech_cur_target_obj_type = -1;
    magictech_waiting_for_effect_callback = 0;
    magictech_effect_callback_pending = 1;
    magictech_current_run_had_effect = 0;
    magictech_last_aggression_notified_obj = 0;
    magictech_initial_processing_action = run_info->action;
    magictech_process();
}

// 0x4570E0
bool magictech_maintenance_timeevent_matches_current(TimeEvent* timeevent)
{
    return timeevent->params[0].integer_value == magictech_selected_timeevent_run_id
        && timeevent->params[2].integer_value == 1;
}

// 0x457100
void magictech_run_slots_reset_callback(void)
{
    magictech_run_slots_reset();
}

// 0x457110
void magictech_interrupt(int mt_id)
{
    MagicTechRunInfo* run_info;
    MagicTechInfo* info;

    if (!magictech_id_to_run_info(mt_id, &run_info)) {
        return;
    }

    info = &(magictech_spells[run_info->spell]);

    if (run_info->action == MAGICTECH_ACTION_MAINTAIN) {
        if (info->components[MAGICTECH_ACTION_END_CALLBACK].cnt > 0) {
            run_info->action = MAGICTECH_ACTION_END;
        } else if (info->components[MAGICTECH_ACTION_CALLBACK].cnt > 0) {
            run_info->action = MAGICTECH_ACTION_CALLBACK;
        } else {
            run_info->action = MAGICTECH_ACTION_END;
        }
    } else {
        if (info->components[MAGICTECH_ACTION_END_CALLBACK].cnt > 0) {
            run_info->action = MAGICTECH_ACTION_END_CALLBACK;
        } else if (info->components[MAGICTECH_ACTION_CALLBACK].cnt > 0) {
            run_info->action = MAGICTECH_ACTION_CALLBACK;
        } else {
            run_info->action = MAGICTECH_ACTION_END;
        }
    }

    magictech_current_target_list.cnt = 0;
    if (!magictech_clearing_events) {
        magictech_clearing_events = true;
        magictech_selected_timeevent_run_id = mt_id;
        timeevent_clear_one_ex(TIMEEVENT_TYPE_MAGICTECH, magictech_maintenance_timeevent_matches_current);
        magictech_clearing_events = false;
    }

    magictech_run_process(run_info);
}

// 0x4571E0
void magictech_interrupt_delayed(int mt_id)
{
    MagicTechRunInfo* run_info;
    TimeEvent timeevent;
    DateTime datetime;

    ui_spell_maintain_end(mt_id);

    if (magictech_id_to_run_info(mt_id, &run_info)
        && run_info->action == MAGICTECH_ACTION_BEGIN
        && (run_info->flags & MAGICTECH_RUN_PROCESSING_STARTED) == 0) {
        magictech_casting_complete_and_release(mt_id, 1);
        return;
    }

    timeevent.type = TIMEEVENT_TYPE_MAGICTECH;
    timeevent.params[0].integer_value = mt_id;
    timeevent.params[2].integer_value = 2;
    datetime_init(&datetime, 1);

    if (!timeevent_add_delay(&timeevent, &datetime)) {
        tig_debug_printf("magictech_interrupt_delayed: Error: failed to queue timeevent!\n");
    }
}

// 0x457270
void magictech_run_end_immediate(int mt_id)
{
    MagicTechRunInfo* run_info;

    if (magictech_id_to_run_info(mt_id, &run_info)) {
        run_info->action = MAGICTECH_ACTION_END;
        magictech_current_target_list.cnt = 0;
        if (!magictech_clearing_events) {
            magictech_clearing_events = true;
            magictech_selected_timeevent_run_id = mt_id;
            timeevent_clear_one_ex(TIMEEVENT_TYPE_MAGICTECH, magictech_maintenance_timeevent_matches_current);
            magictech_clearing_events = false;
        }
        run_info->flags |= MAGICTECH_RUN_END_IMMEDIATE;
        magictech_run_process(run_info);
    }
}

// 0x4573D0
void magictech_matching_invocation_interrupt(MagicTechInvocation* mt_invocation)
{
    int idx;
    MagicTechRunInfo* run_info;

    if (mt_invocation->source_obj.obj) {
        for (idx = 0; idx < 512; idx++) {
            if (magictech_id_to_run_info(idx, &run_info)
                && run_info->parent_obj.obj == mt_invocation->parent_obj.obj
                && run_info->target_obj.obj == mt_invocation->target_obj.obj
                && run_info->spell == mt_invocation->spell) {
                magictech_interrupt(run_info->id);
                break;
            }
        }
    }
}

// 0x457450
void magictech_demaintain_spells(int64_t obj)
{
    int index;
    MagicTechInfo* info;

    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    for (index = 0; index < 512; index++) {
        if (magictech_run_info[index].source_obj.obj == obj) {
            info = &(magictech_spells[magictech_run_info[index].spell]);
            if ((info->flags & MAGICTECH_IS_TECH) == 0
                && (info->item_triggers == 0 || info->maintenance.period > 0)) {
                magictech_interrupt_delayed(magictech_run_info[index].id);
            }
        }
    }
}

// 0x4574D0
void magictech_object_sources_detach(int64_t obj)
{
    int index;

    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    for (index = 0; index < 512; index++) {
        if (magictech_run_info[index].parent_obj.obj == obj
            || magictech_run_info[index].source_obj.obj == obj) {
            magictech_source_detach(magictech_run_info[index].id);
        }
    }
}

// 0x457530
void magictech_source_detach(int mt_id)
{
    MagicTechRunInfo* run_info;

    if (magictech_id_to_run_info(mt_id, &run_info)) {
        run_info->source_obj.obj = OBJ_HANDLE_NULL;
        object_reference_init(OBJ_HANDLE_NULL, &(run_info->source_obj.reference));
        run_info->source_obj.type = -1;
    }
}

// 0x457580
void magictech_spell_info_defaults_init(MagicTechInfo* info, int magictech)
{
    int index;

    if (magictech_ignore_intelligence_requirements != 0) {
        info->iq = 0;
    } else {
        info->iq = magictech_intelligence_requirements_by_tier[magictech % 5];
    }

    info->cost = 0;
    info->flags = 0;
    info->item_triggers = 0;
    info->maintenance.cost = 0;
    info->maintenance.period = 0;
    info->duration.period = 0;
    info->duration.stat = -1;
    info->duration.level = 0;
    info->duration.modifier = 1;
    info->duration_trigger_count = 0;
    info->range = 99;
    info->resistance.stat = -1;
    info->resistance.value = 0;
    info->missile = -1;
    info->casting_anim = -1;
    info->no_stack = 0;
    info->applied_spell_flags = 0;
    info->cancels_sf = 0;
    info->disallowed_sf = 0;
    info->disallowed_tsf = 0;
    info->disallowed_tcf = 0;
    info->cancels_envsf = 0;

    for (index = 0; index < MAGICTECH_ACTION_COUNT; index++) {
        info->pairs[index].caster = -1;
        info->pairs[index].target = -1;

        info->target_params[index].tgt = TGT_NONE;
        info->target_params[index].spell_flags = 0;
        info->target_params[index].no_spell_flags = 0;
        info->target_params[index].radius = 0;
        info->target_params[index].count = -1;

        info->components[index].cnt = 0;
        info->components[index].entries = NULL;
    }
}

// 0x457650
void magictech_build_aoe_info(MagicTechInfo* info, char* str)
{
    uint64_t tgt;
    int action;
    unsigned int flags;
    int value;

    tig_str_parse_set_separator(',');

    if (!tig_str_parse_named_flag_list_64(&str, "AoE:", magictech_target_flag_names, magictech_target_flag_values, 65, &tgt)) {
        tig_debug_printf("magictech_build_aoe_info: Error: failed match target AoE strings!\n");
    }

    for (action = 0; action < MAGICTECH_ACTION_COUNT; action++) {
        if (magictech_spell_being_parsed == 2
            || magictech_spell_being_parsed == 9
            || magictech_spell_being_parsed == 1) {
            info->target_params[action].tgt = tgt;
        } else {
            info->target_params[action].tgt = tgt;
            info->target_params[action].tgt |= TGT_OBJ_NO_INVULNERABLE;
        }
    }

    if (tig_str_parse_named_flag_list_direct(&str, "AoE_SF:", obj_flags_tbl[OFS_SPELL_FLAGS], obj_flags_size_tbl[OFS_SPELL_FLAGS], &flags)) {
        for (action = 0; action < MAGICTECH_ACTION_COUNT; action++) {
            info->target_params[action].spell_flags = flags;
        }
    } else {
        for (action = 0; action < MAGICTECH_ACTION_COUNT; action++) {
            info->target_params[action].spell_flags = 0;
        }
    }

    if (tig_str_parse_named_flag_list_direct(&str, "AoE_NO_SF:", obj_flags_tbl[OFS_SPELL_FLAGS], obj_flags_size_tbl[OFS_SPELL_FLAGS], &flags)) {
        for (action = 0; action < MAGICTECH_ACTION_COUNT; action++) {
            info->target_params[action].no_spell_flags = flags;
        }
    } else {
        for (action = 0; action < MAGICTECH_ACTION_COUNT; action++) {
            info->target_params[action].no_spell_flags = 0;
        }
    }

    if (tig_str_parse_named_value(&str, "Radius:", &value)) {
        for (action = 0; action < MAGICTECH_ACTION_COUNT; action++) {
            info->target_params[action].radius = value;
        }
    }

    if (tig_str_parse_named_value(&str, "Count:", &value)) {
        for (action = 0; action < MAGICTECH_ACTION_COUNT; action++) {
            info->target_params[action].count = value;
        }
    }

    for (action = 0; action < MAGICTECH_ACTION_COUNT; action++) {
        if (tig_str_parse_named_flag_list_64(&str, magictech_action_parameter_labels[action].aoe, magictech_target_flag_names, magictech_target_flag_values, 65, &tgt)) {
            info->target_params[action].tgt |= tgt;
        }

        if (tig_str_parse_named_flag_list_direct(&str, magictech_action_parameter_labels[action].aoe_sf, obj_flags_tbl[OFS_SPELL_FLAGS], obj_flags_size_tbl[OFS_SPELL_FLAGS], &flags)) {
            info->target_params[action].spell_flags |= flags;
        }

        if (tig_str_parse_named_flag_list_direct(&str, magictech_action_parameter_labels[action].aoe_no_sf, obj_flags_tbl[OFS_SPELL_FLAGS], obj_flags_size_tbl[OFS_SPELL_FLAGS], &flags)) {
            info->target_params[action].no_spell_flags |= flags;
        }

        if (tig_str_parse_named_value(&str, magictech_action_parameter_labels[action].radius, &value)) {
            info->target_params[action].radius = value;
        }

        if (tig_str_parse_named_value(&str, magictech_action_parameter_labels[action].count, &value)) {
            info->target_params[action].count = value;
        }
    }
}

// 0x4578F0
void magictech_spell_costs_and_duration_parse(MagicTechInfo* info, char* str)
{
    char* curr = str;
    int value1;
    int value2;

    tig_str_parse_set_separator(',');

    if (tig_str_parse_named_value(&curr, "IQ:", &value1)) {
        info->iq = value1;
    }

    if (tig_str_parse_named_value(&curr, "Cost:", &value1)) {
        info->cost = value1;
    }

    if (tig_str_parse_named_complex_value(&curr, "Maintain:", '@', &value1, &value2)) {
        info->maintenance.cost = value1;
        info->maintenance.period = value2;
    }

    if (tig_str_parse_named_complex_value(&curr, "Duration:", '@', &value1, &value2)) {
        info->duration.period = value1;
        info->duration.stat = value2;

        if (info->duration.stat != -1) {
            if (tig_str_parse_named_complex_value(&curr, "DurationStatInfo:", '@', &value1, &value2)) {
                info->duration.level = value1;
                info->duration.modifier = value2;
            }
        }
    }

    if (tig_str_parse_named_value(&curr, "DurationTriggerCount:", &value1)) {
        info->duration_trigger_count = value1;
    }

    if (tig_str_parse_named_complex_str_value(&curr, "Resist:", '@', stat_lookup_keys_tbl, STAT_COUNT, &value1, &value2)) {
        info->resistance.stat = value1;
        info->resistance.value = value2;
    }

    if (tig_str_parse_named_value(&curr, "Range:", &value1)) {
        info->range = value1;
    }

    if (tig_str_parse_named_value(&curr, "ChargesFromCells:", &value1)) {
        if (value1 != 0) {
            info->flags |= 0x20;
        }
    }

    if (tig_str_parse_named_value(&curr, "ChargeBeginCost:", &value1)) {
        if (value1 == 0) {
            info->flags |= 0x0100;
        }
    }

    if (tig_str_match_named_str_to_list(&curr, "Info:", magictech_info_flag_names, 4, &value1)) {
        info->flags |= magictech_info_flag_values[value1];
    }

    if (!tig_str_parse_named_value(&curr, "Disabled:", &value1) || value1 == 0) {
        info->flags |= MAGICTECH_IS_ENABLED;
    }
}

// 0x457B20
void magictech_spell_animation_parse(MagicTechInfo* info, char* str)
{
    char* curr = str;
    int value;

    tig_str_parse_set_separator(',');

    if (tig_str_parse_named_value(&curr, "CastingAnim:", &value)) {
        info->casting_anim = value;
    }

    if (tig_str_parse_named_value(&curr, "Missile:", &value)) {
        info->missile = value;
    }

    if (tig_str_parse_named_value(&curr, "[Begin]Caster:", &value)) {
        info->pairs[MAGICTECH_ACTION_BEGIN].caster = value;
    }

    if (tig_str_parse_named_value(&curr, "[Begin]Target:", &value)) {
        info->pairs[MAGICTECH_ACTION_BEGIN].target = value;
    }

    if (tig_str_parse_named_value(&curr, "[Maintain]Caster:", &value)) {
        info->pairs[MAGICTECH_ACTION_MAINTAIN].caster = value;
    }

    if (tig_str_parse_named_value(&curr, "[Maintain]Target:", &value)) {
        info->pairs[MAGICTECH_ACTION_MAINTAIN].target = value;
    }

    if (tig_str_parse_named_value(&curr, "[End]Caster:", &value)) {
        info->pairs[MAGICTECH_ACTION_END].caster = value;
    }

    if (tig_str_parse_named_value(&curr, "[End]Target:", &value)) {
        info->pairs[MAGICTECH_ACTION_END].target = value;
    }

    if (tig_str_parse_named_value(&curr, "[Callback]Caster:", &value)) {
        info->pairs[MAGICTECH_ACTION_CALLBACK].caster = value;
    }

    if (tig_str_parse_named_value(&curr, "[Callback]Target:", &value)) {
        info->pairs[MAGICTECH_ACTION_CALLBACK].target = value;
    }

    if (tig_str_parse_named_value(&curr, "[EndCallback]Caster:", &value)) {
        info->pairs[MAGICTECH_ACTION_END_CALLBACK].caster = value;
    }

    if (tig_str_parse_named_value(&curr, "[EndCallback]Target:", &value)) {
        info->pairs[MAGICTECH_ACTION_END_CALLBACK].target = value;
    }

    if (tig_str_parse_named_value(&curr, "Is_Tech:", &value)) {
        if (value != 0) {
            info->flags |= MAGICTECH_IS_TECH;
        }
    }
}

// 0x457D00
void magictech_spell_flags_parse(MagicTechInfo* info, char* str)
{
    char* curr = str;
    unsigned int flags;
    int value;

    tig_str_parse_set_separator(',');

    if (tig_str_parse_named_value(&curr, "No_Stack:", &value)) {
        info->no_stack = value;
    }

    if (tig_str_parse_named_flag_list_direct(&curr, "Cancels_SF:", obj_flags_tbl[OFS_SPELL_FLAGS], obj_flags_size_tbl[OFS_SPELL_FLAGS], &flags)) {
        info->cancels_sf = flags;
    }

    if (tig_str_parse_named_flag_list_direct(&curr, "Cancels_EnvSF:", obj_flags_tbl[OFS_SPELL_FLAGS], obj_flags_size_tbl[OFS_SPELL_FLAGS], &flags)) {
        info->cancels_envsf = flags;
    }

    if (tig_str_parse_named_flag_list_direct(&curr, "Disallowed_SF:", obj_flags_tbl[OFS_SPELL_FLAGS], obj_flags_size_tbl[OFS_SPELL_FLAGS], &flags)) {
        info->disallowed_sf = flags;
    }

    if (tig_str_parse_named_flag_list_direct(&curr, "Disallowed_TSF:", obj_flags_tbl[OFS_SPELL_FLAGS], obj_flags_size_tbl[OFS_SPELL_FLAGS], &flags)) {
        info->disallowed_tsf = flags;
    }

    if (tig_str_parse_named_flag_list_direct(&curr, "Disallowed_TCF:", obj_flags_tbl[OFS_CRITTER_FLAGS], obj_flags_size_tbl[OFS_CRITTER_FLAGS], &flags)) {
        info->disallowed_tcf = flags;
    }

    if (tig_str_parse_named_flag_list(&curr, "ItemTriggers:", mt_item_trig_keys, mt_item_trig_values, MT_ITEM_TRIG_COUNT, &flags)) {
        info->item_triggers = flags;
    }
}

// 0x457E70
void magictech_build_ai_info(MagicTechInfo* info, char* str)
{
    char* curr = str;
    int value1;
    int value2;

    tig_str_parse_set_separator(',');

    if (tig_str_parse_named_value(&curr, "AI_Flee:", &value1)) {
        info->ai.flee = value1;
    }

    if (tig_str_parse_named_value(&curr, "AI_Summon:", &value1)) {
        info->ai.summon = value1;
    }

    if (tig_str_parse_named_complex_value(&curr, "AI_Defensive:", '@', &value1, &value2)) {
        info->ai.defensive1 = value1;
        info->defensive2 = value2;
    }

    if (tig_str_parse_named_value(&curr, "AI_Offensive:", &value1)) {
        info->ai.offensive = value1;
    }

    if (tig_str_parse_named_value(&curr, "AI_HealingLight:", &value1)) {
        info->ai.healing_light = value1;
    }

    if (tig_str_parse_named_value(&curr, "AI_HealingMedium:", &value1)) {
        info->ai.healing_medium = value1;
    }

    if (tig_str_parse_named_value(&curr, "AI_HealingHeavy:", &value1)) {
        info->ai.healing_heavy = value1;
    }

    if (tig_str_parse_named_value(&curr, "AI_CurePoison:", &value1)) {
        info->ai.cure_poison = value1;
    }

    if (tig_str_parse_named_value(&curr, "AI_FatigueRecover:", &value1)) {
        info->ai.fatigue_recover = value1;
    }

    if (tig_str_parse_named_value(&curr, "AI_Resurrect:", &value1)) {
        info->ai.resurrect = value1;
    }

    if (tig_str_parse_named_value(&curr, "No_Resist:", &value1)) {
        info->flags |= MAGICTECH_NO_RESIST;
    }

    if (tig_str_parse_named_value(&curr, "No_Reflect:", &value1)) {
        info->flags |= MAGICTECH_NO_REFLECT;
    }
}

// 0x458060
void magictech_build_effect_info(MagicTechInfo* info, char* str)
{
    int action;
    MagicTechComponentList* component_list;
    MagicTechComponentInfo* component_info;
    uint64_t tgt;
    unsigned int flags;
    int value;

    tig_str_parse_set_separator(',');

    tig_str_match_str_to_list(&str, magictech_action_names, MAGICTECH_ACTION_COUNT, &action);

    component_list = &(info->components[action]);
    if (component_list->cnt < 0 || component_list->cnt == INT_MAX
        || (size_t)component_list->cnt + 1 > SIZE_MAX / sizeof(*component_list->entries)) {
        tig_debug_printf("magictech_build_effect_info: component count overflow.\n");
        exit(EXIT_FAILURE);
    }
    if (component_list->entries != NULL) {
        component_list->entries = REALLOC(component_list->entries, sizeof(*component_list->entries) * ((size_t)component_list->cnt + 1));
        if (component_list->entries == NULL) {
            tig_debug_printf("magictech_build_effect_info: Error: failed realloc component list!\n");
            exit(EXIT_FAILURE);
        }
    } else {
        component_list->entries = MALLOC(sizeof(*component_list->entries));
        if (component_list->entries == NULL) {
            tig_debug_printf("magictech_build_effect_info: component allocation failed.\n");
            exit(EXIT_FAILURE);
        }
    }

    component_info = &(component_list->entries[component_list->cnt++]);
    component_info->aoe.tgt = TGT_NONE;
    component_info->aoe.spell_flags = 0;
    component_info->aoe.no_spell_flags = 0;
    component_info->aoe.radius = 0;
    component_info->aoe.count = -1;

    if (tig_str_parse_named_flag_list_64(&str, "AoE:", magictech_target_flag_names, magictech_target_flag_values, 65, &tgt)) {
        component_info->aoe.tgt = tgt;
    }

    if (tig_str_parse_named_flag_list_direct(&str, "AoE_SF:", obj_flags_tbl[OFS_SPELL_FLAGS], obj_flags_size_tbl[OFS_SPELL_FLAGS], &flags)) {
        component_info->aoe.spell_flags |= flags;
    }

    if (tig_str_parse_named_flag_list_direct(&str, "AoE_NO_SF:", obj_flags_tbl[OFS_SPELL_FLAGS], obj_flags_size_tbl[OFS_SPELL_FLAGS], &flags)) {
        component_info->aoe.no_spell_flags |= flags;
    }

    if (tig_str_parse_named_value(&str, "Radius:", &value)) {
        component_info->aoe.radius = value;
    } else {
        component_info->aoe.radius = info->target_params[action].radius;
    }

    if (tig_str_parse_named_value(&str, "Count:", &value)) {
        component_info->aoe.count = value;
    }

    component_info->apply_aoe.tgt = 0;
    component_info->apply_aoe.spell_flags = 0;
    component_info->apply_aoe.no_spell_flags = 0;
    component_info->apply_aoe.radius = 0;
    component_info->apply_aoe.count = -1;

    tig_str_parse_named_flag_list_64(&str, "Apply_AoE:", magictech_target_flag_names, magictech_target_flag_values, 65, &(component_info->apply_aoe.tgt));

    if (tig_str_parse_named_flag_list(&str, "ItemTriggers:", mt_item_trig_keys, mt_item_trig_values, 26, &flags)) {
        component_info->item_triggers = flags;
    } else {
        component_info->item_triggers = 0;
    }

    if (tig_str_match_named_str_to_list(&str, "Type:", magictech_component_names, 25, &(component_info->type))) {
        switch (component_info->type) {
        case MAGICTECH_COMPONENT_ANIMATION_GOAL:
            tig_str_match_str_to_list(&str, magictech_animation_goal_names, 6, &value);
            component_info->data.agoal.goal = magictech_animation_goal_types[value];
            if (tig_str_parse_named_value(&str, "SubGoal:", &value)) {
                component_info->data.agoal.subgoal = value;
            } else {
                component_info->data.agoal.subgoal = 0;
            }
            break;
        case MAGICTECH_COMPONENT_ANIMATION_TERMINATE:
            tig_str_match_str_to_list(&str, magictech_animation_goal_names, 6, &value);
            component_info->data.agoal_terminate.goal = magictech_animation_goal_types[value];
            break;
        case MAGICTECH_COMPONENT_AI_REDIRECT:
            tig_str_match_str_to_list(&str, obj_flags_ocf, 32, &value);
            component_info->data.ai_redirect.critter_flags = 1 << value;
            if (tig_str_parse_named_value(&str, "MinIQ:", &value)) {
                component_info->data.ai_redirect.min_iq = value;
            } else {
                component_info->data.ai_redirect.min_iq = -1;
            }
            break;
        case MAGICTECH_COMPONENT_CAST:
            if (tig_str_parse_named_value(&str, "Spell:", &value)) {
                component_info->data.cast.spell = value;
            } else {
                component_info->data.cast.spell = 10000;
                tig_debug_printf("MagicTech: ERROR: Cast Component has no spell!\n");
            }
            break;
        case MAGICTECH_COMPONENT_CHARGE_AND_BRANCH:
            component_info->data.charge_branch.branch = -1;
            component_info->data.charge_branch.cost = 0;
            tig_str_parse_named_value(&str, "Cost:", &(component_info->data.charge_branch.cost));
            if (tig_str_parse_named_value(&str, "Branch:", &value)) {
                component_info->data.charge_branch.branch = value;
            }
            break;
        case MAGICTECH_COMPONENT_DAMAGE:
            tig_str_match_named_str_to_list(&str, "DmgType:", magictech_damage_type_names, 6, &(component_info->data.damage.damage_type));
            tig_str_parse_named_range(&str, "Dmg:", &(component_info->data.damage.damage_min), &(component_info->data.damage.damage_max));
            component_info->data.damage.damage_flags = 0;
            tig_str_parse_named_flag_list(&str, "Dmg_Flags:", magictech_damage_flag_names, magictech_damage_flag_values, 10, &(component_info->data.damage.damage_flags));
            info->flags |= MAGICTECH_HAVE_DAMAGE;
            break;
        case MAGICTECH_COMPONENT_EFFECT:
            tig_str_parse_value(&str, &(component_info->data.effect.num));
            tig_str_match_str_to_list(&str, magictech_add_remove_names, 2, &(component_info->data.effect.add_remove));
            if (tig_str_parse_named_value(&str, "Count:", &value)) {
                component_info->data.effect.count = value;
            } else {
                component_info->data.effect.count = 1;
            }

            if (tig_str_match_named_str_to_list(&str, "Cause:", effect_cause_lookup_tbl, 10, &value)) {
                component_info->data.effect.cause = value;
            } else {
                component_info->data.effect.cause = 6;
            }

            if (tig_str_parse_named_value(&str, "Scaled:", &value)) {
                component_info->data.effect.scaled = value;
            } else {
                component_info->data.effect.scaled = 0;
            }
            break;
        case MAGICTECH_COMPONENT_ENVIRONMENT_FLAG:
            tig_str_match_str_to_list(&str, obj_flags_tbl[OFS_SPELL_FLAGS], obj_flags_size_tbl[OFS_SPELL_FLAGS], &value);
            component_info->data.env_flags.flags = 1 << value;
            tig_str_match_str_to_list(&str, magictech_flag_state_names, 2, &(component_info->data.env_flags.state));
            break;
        case MAGICTECH_COMPONENT_VISUAL_EFFECT:
            tig_str_parse_value(&str, &(component_info->data.eye_candy.num));
            tig_str_match_str_to_list(&str, magictech_add_remove_names, 2, &(component_info->data.eye_candy.add_remove));
            tig_str_parse_named_flag_list(&str,
                "Play:",
                animfx_play_flags_lookup_tbl_keys,
                animfx_play_flags_lookup_tbl_values,
                ANIMFX_PLAY_COUNT,
                &(component_info->data.eye_candy.flags));
            break;
        case MAGICTECH_COMPONENT_HEAL:
            tig_str_match_named_str_to_list(&str, "DmgType:", magictech_damage_type_names, 6, &(component_info->data.heal.damage_type));
            tig_str_parse_named_range(&str, "Dmg:", &(component_info->data.heal.damage_min), &(component_info->data.heal.damage_max));
            component_info->data.heal.damage_flags = 0;
            tig_str_parse_named_flag_list(&str, "Dmg_Flags:", magictech_damage_flag_names, magictech_damage_flag_values, 10, &(component_info->data.heal.damage_flags));
            break;
        case MAGICTECH_COMPONENT_INTERRUPT:
            if (tig_str_parse_named_value(&str, "MagicTech:", &value)) {
                component_info->data.interrupt.magictech = value;
            } else {
                component_info->data.interrupt.magictech = 10000;
                tig_debug_printf("MagicTech: ERROR: Interrupt Component has no spell!\n");
            }
            break;
        case MAGICTECH_COMPONENT_MOVEMENT:
            if (tig_str_match_named_str_to_list(&str, "Move_Location:", magictech_movement_destination_names, 4, &(component_info->data.movement.move_location))
                && component_info->data.movement.move_location == 1) {
                tig_str_parse_named_value(&str, "Tile_Radius:", &(component_info->data.movement.tile_radius));
            }
            break;
        case MAGICTECH_COMPONENT_OBJECT_FLAG:
            tig_str_match_str_to_list(&str, obj_flags_fields_lookup_tbl_keys, OFS_COUNT, &value);
            component_info->data.obj_flag.flags_fld = obj_flags_fields_lookup_tbl_values[value];
            tig_str_match_str_to_list(&str, obj_flags_tbl[value], obj_flags_size_tbl[value], &value);
            component_info->data.obj_flag.value = 1u << value;
            tig_str_match_str_to_list(&str, magictech_flag_state_names, 2, &(component_info->data.obj_flag.state));
            if (component_info->data.obj_flag.flags_fld == OBJ_F_SPELL_FLAGS
                && component_info->data.obj_flag.state) {
                info->applied_spell_flags |= component_info->data.obj_flag.value;
            }
            break;
        case MAGICTECH_COMPONENT_RECHARGE:
            tig_str_parse_value(&str, &(component_info->data.recharge.num));
            if (!tig_str_parse_named_value(&str, "Max:", &(component_info->data.recharge.max))) {
                component_info->data.recharge.max = 9999;
            }
            break;
        case MAGICTECH_COMPONENT_SUMMON:
            if (!tig_str_parse_named_value(&str, "Proto:", &value)) {
                tig_debug_printf("magictech_build_effect_info: MTComponentSummon: Error: failed to find proto #!\n");
            }

            if (value > 0) {
                component_info->data.summon.oid = obj_get_id(proto_handle_get(value));
            } else {
                component_info->data.summon.oid.type = OID_TYPE_NULL;
            }

            if (tig_str_parse_named_value(&str, "Clear_Faction:", &value)) {
                component_info->data.summon.clear_faction = value;
            } else {
                component_info->data.summon.clear_faction = 0;
            }

            if (tig_str_parse_named_value(&str, "Palette:", &value)) {
                component_info->data.summon.palette = value;
            } else {
                component_info->data.summon.palette = 0;
            }

            if (tig_str_parse_named_value(&str, "List:", &value)) {
                component_info->data.summon.list = value;
            } else {
                component_info->data.summon.list = -1;
            }
            break;
        case MAGICTECH_COMPONENT_TEST_AND_BRANCH:
            component_info->data.test_in_branch.test_value = 0;
            component_info->data.test_in_branch.branch_destination = -1;
            tig_str_match_str_to_list(&str, magictech_trait_field_names, 4, &value);
            component_info->data.test_in_branch.object_field = magictech_trait_fields[value];
            tig_str_match_str_to_list(&str, magictech_comparison_operator_names, 5, &value);
            component_info->data.test_in_branch.comparison_operator = value;
            if (tig_str_parse_named_value(&str, "TestVal:", &value)) {
                component_info->data.test_in_branch.test_value = value;
            }
            if (tig_str_parse_named_value(&str, "Branch:", &value)) {
                component_info->data.test_in_branch.branch_destination = value;
            }
            break;
        case MAGICTECH_COMPONENT_TRAIT:
            tig_str_match_str_to_list(&str, magictech_trait_field_names, 4, &value);
            component_info->data.trait.fld = magictech_trait_fields[value];
            component_info->data.trait.reserved_slot_1 = 0;
            tig_str_parse_value(&str, &value);
            component_info->data.trait.value = value;
            component_info->data.trait.reserved_slot_3 = 1;
            component_info->data.trait.reserved_slot_2 = 1;
            tig_str_parse_named_value(&str, "Palette:", &(component_info->data.trait.palette));
            break;
        case MAGICTECH_COMPONENT_INDEXED_TRAIT:
            tig_str_match_str_to_list(&str, magictech_indexed_trait_field_names, 2, &value);
            component_info->data.trait_idx.object_field = magictech_indexed_trait_fields[value];
            tig_str_match_str_to_list(&str, magictech_indexed_trait_value_names[value], magictech_indexed_trait_value_counts[value], &value);
            component_info->data.trait_idx.index = value;
            component_info->data.trait_idx.indexed_trait_mode = 0;
            tig_str_parse_value(&str, &value);
            component_info->data.trait_idx.addend = value;
            component_info->data.trait_idx.scale_denominator = 1;
            component_info->data.trait_idx.scale_numerator = 1;
            break;
        case MAGICTECH_COMPONENT_TRAIT_INT64:
            tig_str_match_str_to_list(&str, magictech_trait_field_names, 4, &value);
            component_info->data.trait64.object_field = magictech_trait_fields[value];
            tig_str_match_str_to_list(&str, magictech_trait_int64_source_names, 3, &value);
            component_info->data.trait64.value_source = value;
            break;
        }
    }
}

// 0x458A80
bool magictech_check_env_sf(unsigned int flags)
{
    int64_t pc_obj;

    pc_obj = player_get_local_pc_obj();
    if (pc_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if ((obj_field_int32_get(pc_obj, OBJ_F_SPELL_FLAGS) & flags) == flags) {
        return true;
    }

    if (flags == OSF_DETECTING_INVISIBLE
        && stat_is_extraordinary(pc_obj, STAT_PERCEPTION)) {
        return true;
    }

    return false;
}

// 0x458AE0
tig_art_id_t magictech_run_spell_icon_get(int mt_id)
{
    MagicTechRunInfo* run_info;
    tig_art_id_t art_id;

    if (!magictech_id_to_run_info(mt_id, &run_info)) {
        return TIG_ART_ID_INVALID;
    }

    if (tig_art_interface_id_create(spell_icon(run_info->spell), 0, 0, 0, &art_id) != TIG_OK) {
        return TIG_ART_ID_INVALID;
    }

    return art_id;
}

// 0x458B60
const char* magictech_get_name(int magictech)
{
    return spell_name(magictech);
}

// 0x458B70
tig_art_id_t magictech_projectile_art_id_get(int mt_id)
{
    MagicTechRunInfo* run_info;
    AnimFxListEntry* effect_entry;
    tig_art_id_t eye_candy_art_id = TIG_ART_ID_INVALID;
    tig_art_id_t obj_art_id;
    int rotation;

    if (magictech_id_to_run_info(mt_id, &run_info)
        && animfx_id_get(&spell_eye_candies, run_info->spell * 6 + MAGICTECH_EYE_CANDY_TYPE_PROJECTILE, &effect_entry)
        && run_info->parent_obj.obj != OBJ_HANDLE_NULL) {
        eye_candy_art_id = effect_entry->eye_candy_art_id;
        if (eye_candy_art_id != TIG_ART_ID_INVALID) {
            obj_art_id = obj_field_int32_get(run_info->parent_obj.obj, OBJ_F_CURRENT_AID);
            rotation = tig_art_id_rotation_get(obj_art_id);
            eye_candy_art_id = tig_art_id_rotation_set(eye_candy_art_id, rotation);
        }
    }

    return eye_candy_art_id;
}

// 0x458C00
void magictech_projectile_light_and_sound_apply_legacy(int spell, int64_t obj)
{
    MagicTechRunInfo* run_info;
    AnimFxListEntry* fx_info;

    if (obj == OBJ_HANDLE_NULL
        && magictech_id_to_run_info(spell, &run_info)
        && animfx_id_get(&spell_eye_candies, run_info->spell * 6 + MAGICTECH_EYE_CANDY_TYPE_PROJECTILE, &fx_info)) {
        if (tig_art_exists(fx_info->light_art_id) == TIG_OK) {
            object_set_light(obj, 0x20, fx_info->light_art_id, fx_info->light_color);
        }

        if (fx_info->sound != -1) {
            gsound_play_sfx_on_obj(fx_info->sound, 1, obj);
        }
    }
}

// 0x458CA0
int magictech_projectile_speed_get(int mt_id)
{
    MagicTechRunInfo* run_info;
    AnimFxListEntry* fx_info;

    if (magictech_id_to_run_info(mt_id, &run_info)
        && animfx_id_get(&spell_eye_candies, run_info->spell * 6 + MAGICTECH_EYE_CANDY_TYPE_PROJECTILE, &fx_info)) {
        return fx_info->projectile_speed;
    } else {
        return 0;
    }
}

// 0x458CF0
bool magictech_find_first(int64_t obj, int* mt_id_ptr)
{
    int idx;
    MagicTechObjectNode* node;

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    for (idx = 0; idx < 512; idx++) {
        if ((magictech_run_info[idx].flags & MAGICTECH_RUN_ACTIVE) != 0) {
            if (magictech_run_info[idx].target_obj.obj == obj) {
                if (mt_id_ptr != NULL) {
                    *mt_id_ptr = idx;
                }
                return true;
            }

            node = magictech_run_info[idx].summoned_obj;
            while (node != NULL) {
                if (node->obj == obj) {
                    if (mt_id_ptr != NULL) {
                        *mt_id_ptr = idx;
                    }
                    return true;
                }
                node = node->next;
            }

            node = magictech_run_info[idx].objlist;
            while (node != NULL) {
                if (node->obj == obj) {
                    if (mt_id_ptr != NULL) {
                        *mt_id_ptr = idx;
                    }
                    return true;
                }
                node = node->next;
            }
        }
    }

    return false;
}

// 0x458D90
bool magictech_find_next(int64_t obj, int* mt_id_ptr)
{
    int idx;
    MagicTechObjectNode* node;

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    for (idx = *mt_id_ptr + 1; idx < 512; idx++) {
        if ((magictech_run_info[idx].flags & MAGICTECH_RUN_ACTIVE) != 0) {
            if (magictech_run_info[idx].target_obj.obj == obj) {
                if (mt_id_ptr != NULL) {
                    *mt_id_ptr = idx;
                }
                return true;
            }

            node = magictech_run_info[idx].summoned_obj;
            while (node != NULL) {
                if (node->obj == obj) {
                    if (mt_id_ptr != NULL) {
                        *mt_id_ptr = idx;
                    }
                    return true;
                }
                node = node->next;
            }

            node = magictech_run_info[idx].objlist;
            while (node != NULL) {
                if (node->obj == obj) {
                    if (mt_id_ptr != NULL) {
                        *mt_id_ptr = idx;
                    }
                    return true;
                }
                node = node->next;
            }
        }
    }

    return false;
}

// 0x459040
bool magictech_spell_flag_caster_find(int64_t obj, unsigned int flags, int64_t* parent_obj_ptr)
{
    int idx;
    MagicTechObjectNode* node;

    if (parent_obj_ptr == NULL) {
        return false;
    }

    if (obj == OBJ_HANDLE_NULL) {
        *parent_obj_ptr = OBJ_HANDLE_NULL;
        return false;
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & flags) == 0) {
        *parent_obj_ptr = OBJ_HANDLE_NULL;
        return false;
    }

    for (idx = 0; idx < 512; idx++) {
        if (magictech_run_info[idx].source_obj.obj != OBJ_HANDLE_NULL) {
            if (magictech_run_info[idx].target_obj.obj == obj
                && (magictech_run_info[idx].applied_spell_flags & flags) == flags) {
                *parent_obj_ptr = magictech_run_info[idx].parent_obj.obj;
                return true;
            }

            node = magictech_run_info[idx].summoned_obj;
            while (node != NULL) {
                if (node->obj == obj
                    && (magictech_run_info[idx].applied_spell_flags & flags) == flags) {
                    *parent_obj_ptr = magictech_run_info[idx].parent_obj.obj;
                    return true;
                }
                node = node->next;
            }

            node = magictech_run_info[idx].objlist;
            while (node != NULL) {
                if (node->obj == obj
                    && (magictech_run_info[idx].applied_spell_flags & flags) == flags) {
                    *parent_obj_ptr = magictech_run_info[idx].parent_obj.obj;
                    return true;
                }
                node = node->next;
            }
        }
    }

    *parent_obj_ptr = OBJ_HANDLE_NULL;
    return false;
}

// 0x459170
bool magictech_spell_flag_instance_find(int64_t obj, unsigned int flags, int* index_ptr)
{
    int idx;
    MagicTechObjectNode* node;

    if (index_ptr == NULL) {
        return false;
    }

    if (obj == OBJ_HANDLE_NULL) {
        *index_ptr = -1;
        return false;
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & flags) == 0) {
        *index_ptr = -1;
        return false;
    }

    for (idx = 0; idx < 512; idx++) {
        if ((magictech_run_info[idx].flags & MAGICTECH_RUN_ACTIVE) != 0) {
            if (magictech_run_info[idx].target_obj.obj == obj
                && (magictech_run_info[idx].applied_spell_flags & flags) == flags) {
                *index_ptr = idx;
                return true;
            }

            node = magictech_run_info[idx].summoned_obj;
            while (node != NULL) {
                if (node->obj == obj
                    && (magictech_run_info[idx].applied_spell_flags & flags) == flags) {
                    *index_ptr = idx;
                    return true;
                }
                node = node->next;
            }

            node = magictech_run_info[idx].objlist;
            while (node != NULL) {
                if (node->obj == obj
                    && (magictech_run_info[idx].applied_spell_flags & flags) == flags) {
                    *index_ptr = idx;
                    return true;
                }
                node = node->next;
            }
        }
    }

    *index_ptr = -1;
    return false;
}

// 0x459290
bool magictech_spell_instance_on_target_find(int64_t obj, int spell, int* index_ptr)
{
    int idx;
    MagicTechObjectNode* node;

    if (index_ptr == NULL) {
        return false;
    }

    if (obj == OBJ_HANDLE_NULL) {
        *index_ptr = -1;
        return false;
    }

    for (idx = 0; idx < 512; idx++) {
        if ((magictech_run_info[idx].flags & MAGICTECH_RUN_ACTIVE) != 0) {
            if (magictech_run_info[idx].target_obj.obj == obj
                && magictech_run_info[idx].spell == spell) {
                *index_ptr = idx;
                return true;
            }

            node = magictech_run_info[idx].summoned_obj;
            while (node != NULL) {
                if (node->obj == obj
                    && magictech_run_info[idx].spell == spell) {
                    *index_ptr = idx;
                    return true;
                }
                node = node->next;
            }

            node = magictech_run_info[idx].objlist;
            while (node != NULL) {
                if (node->obj == obj
                    && magictech_run_info[idx].spell == spell) {
                    *index_ptr = idx;
                    return true;
                }
                node = node->next;
            }
        }
    }

    *index_ptr = -1;
    return false;
}

// 0x459380
bool magictech_is_under_influence_of(int64_t obj, int magictech)
{
    int mt_id;
    MagicTechRunInfo* run_info;

    if (magictech_find_first(obj, &mt_id)) {
        do {
            if (magictech_id_to_run_info(mt_id, &run_info) && run_info->spell == magictech) {
                return true;
            }
        } while (magictech_find_next(obj, &mt_id));
    }

    return false;
}

// 0x4593F0
bool magictech_stop_spell(int64_t obj, int magictech)
{
    int mt_id;

    if (!magictech_spell_instance_on_target_find(obj, magictech, &mt_id)) {
        return false;
    }

    magictech_interrupt(mt_id);

    return true;
}

// 0x459430
bool magictech_timeevent_process(TimeEvent* timeevent)
{
    MagicTechRunInfo* run_info;

    switch (timeevent->params[2].integer_value) {
    case 1:
        magictech_maintenance_action_process(timeevent->params[0].integer_value);
        break;
    case 2:
        magictech_interrupt(timeevent->params[0].integer_value);
        break;
    case 3:
        if (magictech_id_to_run_info(timeevent->params[0].integer_value, &run_info)) {
            magictech_run_process(run_info);
        }
        break;
    }

    return true;
}

// 0x459490
void magictech_maintenance_action_process(int mt_id)
{
    MagicTechRunInfo* run_info;

    if (magictech_id_to_run_info(mt_id, &run_info)) {
        if (run_info->action == MAGICTECH_ACTION_BEGIN) {
            run_info->action = MAGICTECH_ACTION_MAINTAIN;
        }
        magictech_run_process(run_info);
    }
}

// 0x4594D0
bool magictech_timeevent_matches_selected_action(TimeEvent* timeevent)
{
    return timeevent->params[0].integer_value == magictech_selected_timeevent_index
        && timeevent->params[2].integer_value == magictech_selected_timeevent_subtype;
}

// 0x459500
bool magictech_run_timeevent_restart(int index)
{
    MagicTechRunInfo* run_info;
    TimeEvent timeevent;

    if (index == -1
        || !magictech_id_to_run_info(index, &run_info)
        || (run_info->flags & MAGICTECH_RUN_ACTIVE) == 0) {
        return false;
    }

    magictech_selected_timeevent_index = index;
    magictech_selected_timeevent_subtype = 1;
    timeevent_clear_one_ex(TIMEEVENT_TYPE_MAGICTECH, magictech_timeevent_matches_selected_action);

    timeevent.type = TIMEEVENT_TYPE_MAGICTECH;
    timeevent.params[0].integer_value = run_info->id;
    timeevent.params[2].integer_value = 1;
    return timeevent_add_base(&timeevent, &(run_info->event_time));
}

// 0x459590
bool magictech_recharge_timeevent_schedule(int64_t item_obj, int mana_cost, bool add)
{
    DateTime datetime;
    TimeEvent timeevent;

    magictech_recharge_timeevent_mana_cost = mana_cost;
    magictech_recharge_timeevent_item_obj = item_obj;
    magictech_cast_event_start_seconds = game_time_seconds_get();

    if (add) {
        if (timeevent_any(TIMEEVENT_TYPE_RECHARGE_MAGIC_ITEM, magictech_recharge_timeevent_add)) {
            return true;
        }
    }

    timeevent.type = TIMEEVENT_TYPE_RECHARGE_MAGIC_ITEM;
    timeevent.params[0].object_value = item_obj;
    timeevent.params[1].integer_value = magictech_cast_event_start_seconds;
    timeevent.params[2].integer_value = magictech_recharge_timeevent_mana_cost;
    datetime_init(&datetime, 60000);
    datetime.milliseconds *= 8;
    return timeevent_add_delay(&timeevent, &datetime);
}

// 0x459640
bool magictech_recharge_timeevent_add(TimeEvent* timeevent)
{
    if (timeevent->params[0].object_value != magictech_recharge_timeevent_item_obj) {
        return false;
    }

    timeevent->params[2].integer_value += magictech_recharge_timeevent_mana_cost;

    return true;
}

// 0x459680
bool magictech_recharge_timeevent_process(TimeEvent* timeevent)
{
    int64_t obj;
    int start_time;
    int remaining_mana;
    int elapsed_intervals;
    int mana_store;
    int recovered_mana;
    int64_t parent_obj;
    int diff;

    obj = timeevent->params[0].object_value;
    start_time = timeevent->params[1].integer_value;
    remaining_mana = timeevent->params[2].integer_value;

    if (obj == OBJ_HANDLE_NULL) {
        return false;
    }

    elapsed_intervals = game_time_seconds_elapsed_since(start_time) / 120;
    if (elapsed_intervals < 1) {
        elapsed_intervals = 1;
    }

    mana_store = obj_field_int32_get(obj, OBJ_F_ITEM_MANA_STORE);
    recovered_mana = 2 * elapsed_intervals;
    if (recovered_mana > remaining_mana) {
        recovered_mana = remaining_mana;
    }

    diff = remaining_mana - recovered_mana;
    obj_field_int32_set(obj, OBJ_F_ITEM_MANA_STORE, mana_store + recovered_mana);

    parent_obj = obj_field_handle_get(obj, OBJ_F_ITEM_PARENT);
    if (parent_obj != OBJ_HANDLE_NULL && player_is_local_pc_obj(parent_obj)) {
        ui_counters_refresh();
    }

    if (diff > 0) {
        magictech_recharge_timeevent_schedule(obj, diff, false);
    }

    return true;
}

// 0x459740
void magictech_object_removal_notify(int64_t obj)
{
    int idx;
    MagicTechObjectNode* node;

    if (!magictech_initialized) {
        return;
    }

    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    if (light_scheme_is_changing()) {
        return;
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_SUMMONED) != 0) {
        if (!map_is_clearing_objects()) {
            item_inventory_drop_or_destroy_summoned(obj, true);
        }

        for (idx = 0; idx < 512; idx++) {
            if ((magictech_run_info[idx].flags & MAGICTECH_RUN_ACTIVE) != 0
                && idx != magictech_cur_id) {
                node = magictech_run_info[idx].summoned_obj;
                while (node != NULL) {
                    if (node->obj == obj) {
                        magictech_interrupt_delayed(magictech_run_info[idx].id);
                        break;
                    }
                    node = node->next;
                }
            }
        }
    }

    for (idx = 0; idx < 512; idx++) {
        if (idx != magictech_cur_id) {
            if (magictech_run_info[idx].parent_obj.obj != OBJ_HANDLE_NULL
                && magictech_run_info[idx].parent_obj.obj == obj) {
                magictech_interrupt_delayed(magictech_run_info[idx].id);
            } else if (magictech_run_info[idx].source_obj.obj != OBJ_HANDLE_NULL
                && magictech_run_info[idx].source_obj.obj == obj) {
                magictech_source_detach(magictech_run_info[idx].id);
            }
        }
    }

    for (idx = 0; idx < 512; idx++) {
        if ((magictech_run_info[idx].flags & MAGICTECH_RUN_ACTIVE) != 0
            && idx != magictech_cur_id
            && magictech_run_info[idx].target_obj.obj == obj) {
            magictech_interrupt_delayed(magictech_run_info[idx].id);
        }
    }
}

// 0x4598D0
void magictech_object_spells_end(int64_t obj)
{
    int idx;
    MagicTechObjectNode* node;

    if (!magictech_initialized) {
        return;
    }

    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    if ((obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS) & OSF_SUMMONED) != 0) {
        item_inventory_drop_all(obj, true);

        for (idx = 0; idx < 512; idx++) {
            if ((magictech_run_info[idx].flags & MAGICTECH_RUN_ACTIVE) != 0
                && idx != magictech_cur_id) {
                node = magictech_run_info[idx].summoned_obj;
                while (node != NULL) {
                    if (node->obj == obj) {
                        magictech_run_end_immediate(magictech_run_info[idx].id);
                        break;
                    }
                    node = node->next;
                }
            }
        }
    }

    for (idx = 0; idx < 512; idx++) {
        if (idx != magictech_cur_id) {
            if (magictech_run_info[idx].parent_obj.obj == obj) {
                magictech_run_end_immediate(magictech_run_info[idx].id);
            }
        }
    }

    for (idx = 0; idx < 512; idx++) {
        if ((magictech_run_info[idx].flags & MAGICTECH_RUN_ACTIVE) != 0
            && idx != magictech_cur_id
            && magictech_run_info[idx].target_obj.obj == obj) {
            magictech_run_end_immediate(magictech_run_info[idx].id);
        }
    }
}

// 0x459A20
void magictech_object_callback_stub(int64_t obj)
{
}

// 0x459A30
void magictech_anim_play_hit_fx(int64_t obj, CombatContext* combat)
{
    unsigned int spell_flags;
    AnimFxNode node;
    int magictech;

    spell_flags = obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS);

    if ((spell_flags & OSF_SHIELDED) != 0) {
        if (magictech_spell_flag_instance_find(obj, OSF_SHIELDED, &magictech)) {
            animfx_node_init(&spell_eye_candies,
                &node,
                obj,
                magictech_run_info[magictech].id,
                6 * magictech_run_info[magictech].spell + MAGICTECH_EYE_CANDY_TYPE_DAMAGE);
            node.animate = true;
            animfx_add(&node);
        } else {
            tig_debug_printf("MagicTech: magictech_anim_play_hit_fx: Failed to match spell from flags!\n");
        }
    }

    if ((spell_flags & OSF_MAGNETIC_INVERSION) != 0) {
        if (magictech_spell_flag_instance_find(obj, OSF_MAGNETIC_INVERSION, &magictech)) {
            animfx_node_init(&spell_eye_candies,
                &node,
                obj,
                magictech_run_info[magictech].id,
                6 * magictech_run_info[magictech].spell + MAGICTECH_EYE_CANDY_TYPE_DAMAGE);
            node.animate = true;
            animfx_add(&node);
        } else {
            tig_debug_printf("MagicTech: magictech_anim_play_hit_fx: Failed to match spell from flags!\n");
            animfx_node_init(&spell_eye_candies, &node, obj, -1, 1097);
            node.animate = true;
            animfx_add(&node);
        }
    }

    if ((spell_flags & OSF_ENTANGLED) != 0) {
        if (magictech_spell_flag_instance_find(obj, OSF_ENTANGLED, &magictech)) {
            if (!magictech_target_resistance_check(obj, magictech)) {
                magictech_resurrection_fx_notify(combat->attacker_obj, obj);
                magictech_interrupt_delayed(magictech);
            }
        }
    }

    if (proto_number_from_instance(obj) == BP_KERGHAN_3 && !combat->total_dam) {
        animfx_node_init(&spell_eye_candies, &node, obj, -1, 1259);
        node.animate = true;
        animfx_add(&node);
    }
}

// 0x459C10
bool magictech_target_resistance_check(int64_t obj, int mt_id)
{
    MagicTechRunInfo* run_info;
    MagicTechInfo* info;
    MagicTechMaintenanceInfo* maintenance;
    int resistance;
    int roll;
    int resistance_margin;
    int save_penalty = 0;
    int save_difficulty;

    if (!magictech_id_to_run_info(mt_id, &run_info)) {
        return true;
    }

    info = &(magictech_spells[run_info->spell]);
    maintenance = magictech_get_maintenance(run_info->spell);
    if ((info->flags & MAGICTECH_IS_TECH) != 0
        || (run_info->flags & MAGICTECH_RUN_UNRESISTABLE) != 0
        || obj == OBJ_HANDLE_NULL) {
        return true;
    }

    if (run_info->spell == SPELL_DISINTEGRATE
        && obj_type_is_critter(obj_field_int32_get(obj, OBJ_F_TYPE))
        && (obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2) & OCF2_NO_DISINTEGRATE) != 0) {
        return false;
    }

    if (run_info->parent_obj.obj != obj
        && (obj_field_int32_get(obj, OBJ_F_TYPE) != OBJ_TYPE_PC
            || !fate_resolve(obj, FATE_SAVE_AGAINST_MAGICK))) {
        // CE: The original code does not account for magic resistance
        // adjustments from equipped items.
        resistance = object_get_resistance(obj, RESISTANCE_TYPE_MAGIC, false);
        if (!magictech_cur_is_fate_maximized) {
            int aptitude = stat_level_get(obj, STAT_MAGICK_TECH_APTITUDE);
            if (aptitude < 0) {
                resistance = 100 - (aptitude + 100) * (100 - resistance) / 100;
            }
        }

        roll = random_between(1, 100);
        resistance_margin = roll < resistance ? resistance - roll : 0;
        if (resistance > 0 && (info->flags & 0x80) == 0 && maintenance->period <= 0) {
            if (info->resistance.stat != -1) {
                save_penalty = resistance_margin / 10;
            } else {
                if (roll < resistance) {
                    return 0;
                }
            }
        }
    }

    if (run_info->parent_obj.obj == obj) {
        return true;
    }

    if (info->resistance.stat != -1
        && obj_type_is_critter(magictech_cur_target_obj_type)) {
        if (info->resistance.stat == STAT_WILLPOWER
            && stat_is_extraordinary(obj, STAT_WILLPOWER)) {
            if ((magictech_cur_spell_info->flags & MAGICTECH_AGGRESSIVE) != 0
                && run_info->action != MAGICTECH_ACTION_END) {
                magictech_aggressive_target_notify_once(magictech_cur_run_info->parent_obj.obj, obj);
            }
            return false;
        } else {
            save_difficulty = info->resistance.value + stat_level_get(obj, info->resistance.stat) - save_penalty;
            return save_difficulty <= 0 || random_between(1, 20) > save_difficulty;
        }
    }

    return true;
}

// 0x459EA0
void magictech_tempus_fugit_and_familiar_flags_clear(int64_t obj)
{
    unsigned int flags;
    int64_t leader_obj;

    if (obj != OBJ_HANDLE_NULL) {
        flags = obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS);
        if ((flags & OSF_TEMPUS_FUGIT) != 0) {
            flags &= ~OSF_TEMPUS_FUGIT;
        }
        obj_field_int32_set(obj, OBJ_F_SPELL_FLAGS, flags);

        if ((obj_field_int32_get(obj, OBJ_F_NPC_FLAGS) & ONF_FAMILIAR) != 0) {
            leader_obj = critter_leader_get(obj);
            if (leader_obj != OBJ_HANDLE_NULL) {
                flags = obj_field_int32_get(leader_obj, OBJ_F_SPELL_FLAGS);
                flags &= ~OSF_FAMILIAR;
                obj_field_int32_set(leader_obj, OBJ_F_SPELL_FLAGS, flags);
            }
        }
    }
}

// 0x459F20
bool magictech_target_flags_pointer_get(int magictech, uint64_t** target_flags_ptr)
{
    // TODO: Unclear.
    *target_flags_ptr = &(magictech_spells[magictech].target_params[0].tgt);
    return true;
}

// 0x459F50
void magictech_ai_reset(void)
{
    mt_ai_reset();
}

// 0x459F60
bool magictech_is_enabled(int magictech)
{
    return (magictech_spells[magictech].flags & MAGICTECH_IS_ENABLED) != 0;
}

// 0x459F90
bool magictech_is_magic(int magictech)
{
    return (magictech_spells[magictech].flags & MAGICTECH_IS_TECH) == 0;
}

// 0x459FC0
bool magictech_is_tech(int magictech)
{
    return (magictech_spells[magictech].flags & MAGICTECH_IS_TECH) != 0;
}

// 0x459FF0
bool magictech_run_has_valid_spell(int mt_id)
{
    MagicTechRunInfo* run_info;

    if (!magictech_id_to_run_info(mt_id, &run_info)) {
        return false;
    }

    return run_info->spell >= 0 && run_info->spell < MT_SPELL_COUNT;
}

// 0x45A030
bool magictech_has_item_triggers(int magictech)
{
    return magictech_spells[magictech].item_triggers != 0;
}

// 0x45A060
int magictech_get_aptitude_adj(int64_t sector_id)
{
    int aptitude_adj;
    Sector* sector;

    aptitude_adj = 0;
    if (sector_lock(sector_id, &sector)) {
        aptitude_adj = sector->aptitude_adj;
        sector_unlock(sector_id);
    }

    return aptitude_adj;
}

// 0x45A480
void magictech_run_info_free(MagicTechRunInfo* run_info)
{
    MagicTechObjectNode* node;

    while (run_info->objlist != NULL) {
        node = run_info->objlist;
        run_info->objlist = node->next;
        mt_obj_node_destroy(node);
    }

    while (run_info->summoned_obj != NULL) {
        node = run_info->summoned_obj;
        run_info->summoned_obj = node->next;
        mt_obj_node_destroy(node);
    }

    FREE(run_info);
}

// 0x45A4F0
void magictech_eye_candy_remove(int64_t obj, int fx_id, int mt_id)
{
    animfx_remove(&spell_eye_candies, obj, fx_id, mt_id);
}

// 0x45A520
void magictech_resurrection_fx_notify(int64_t unused_source_obj, int64_t resurrected_obj)
{
    (void)unused_source_obj;

    magictech_fx_add(resurrected_obj, MAGICTECH_FX_RESURRECT);
}

// 0x45A540
void magictech_error_unressurectable(int64_t obj)
{
    MesFileEntry mes_file_entry;

    mes_file_entry.num = 605; // "This life cannot be replenished."
    magictech_get_msg(&mes_file_entry);
    ui_display_warning(mes_file_entry.str);

    magictech_fx_add(obj, MAGICTECH_FX_RESURRECT);
}

// 0x45A580
bool magictech_invisible_target_can_be_seen(int64_t observer_obj, int64_t target_obj)
{
    if ((obj_field_int32_get(target_obj, OBJ_F_SPELL_FLAGS) & OSF_INVISIBLE) == 0
        || (obj_field_int32_get(observer_obj, OBJ_F_SPELL_FLAGS) & OSF_DETECTING_INVISIBLE) != 0) {
        return true;
    }

    if ((obj_field_int32_get(target_obj, OBJ_F_FLAGS) & OF_INVISIBLE) != 0) {
        return false;
    }

    if ((obj_field_int32_get(target_obj, OBJ_F_TYPE) != OBJ_TYPE_PC)) {
        return true;
    }

    if (anim_is_current_goal_type(target_obj, AG_ATTEMPT_ATTACK, NULL)) {
        return true;
    }

    if (anim_is_current_goal_type(target_obj, AG_ATTEMPT_SPELL, NULL)) {
        return true;
    }

    if (combat_critter_is_combat_mode_active(target_obj)) {
        return true;
    }

    return false;
}

// 0x45A620
void magictech_debug_lists(void)
{
    int index;
    MagicTechRunInfo* run_info;
    MagicTechObjectNode* node;

    tig_debug_printf("\n\nMagicTech DEBUG Lists:\n");
    tig_debug_printf("----------------------\n\n");

    for (index = 0; index < 512; index++) {
        run_info = &(magictech_run_info[index]);
        if ((run_info->flags & MAGICTECH_RUN_ACTIVE) != 0) {
            tig_debug_printf("mtID: [%d], Spell: %s(%d)\n",
                index,
                magictech_spell_name(run_info->spell),
                run_info->spell);
            tig_debug_printf("\tAction: %s\n", magictech_action_names[run_info->action]);
            magictech_debug_object_print(run_info->parent_obj.obj, "Parent");
            magictech_debug_object_print(run_info->source_obj.obj, "Source");
            magictech_debug_object_print(run_info->target_obj.obj, "Target");
            node = run_info->summoned_obj;
            while (node != NULL) {
                magictech_debug_object_print(node->obj, "SummonedObj");
                node = node->next;
            }
            node = run_info->objlist;
            while (node != NULL) {
                magictech_debug_object_print(node->obj, "ObjList");
                node = node->next;
            }
            tig_debug_printf("\n");
        }
    }

    tig_debug_printf("Local PC Has %d Maintained Spells\n",
        magictech_maintained_spell_count_stub(player_get_local_pc_obj()));
    tig_debug_printf("\n\n");
}

// 0x45A760
void magictech_debug_object_print(int64_t obj, const char* msg)
{
    // 0x5E6D94
    static char buffer[2000];

    if (obj != OBJ_HANDLE_NULL) {
        object_examine(obj, obj, buffer);
        tig_debug_printf("\t%s: %s(%I64d), Loc: [%I64d x %I64d]\n",
            msg,
            buffer,
            obj,
            obj_field_int64_get(obj, OBJ_F_LOCATION),
            obj_field_int64_get(obj, OBJ_F_LOCATION));
    }
}
