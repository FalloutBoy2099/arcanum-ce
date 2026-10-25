#ifndef ARCANUM_GAME_COMBAT_H_
#define ARCANUM_GAME_COMBAT_H_

#include "game/context.h"
#include "game/damage_type.h"
#include "game/timeevent.h"

typedef void (*CombatModeChangeNotify)(int active);
typedef void (*CombatUiCreate)(void);
typedef void (*CombatUiDestroy)(void);
typedef void (*CombatActionPointsDraw)(int preview_action_point_cost);
typedef void (*CombatObjectNotifyStub)(int64_t obj);

typedef struct CombatCallbacks {
    CombatModeChangeNotify mode_change_notify;
    CombatUiCreate create;
    CombatUiDestroy destroy;
    CombatActionPointsDraw action_points_draw;
    CombatObjectNotifyStub object_notify_stub;
} CombatCallbacks;

typedef enum Loudness {
    LOUDNESS_SILENT,
    LOUDNESS_NORMAL,
    LOUDNESS_LOUD,
    LOUDNESS_COUNT,
} Loudness;

typedef enum HitLocation {
    HIT_LOC_TORSO,
    HIT_LOC_HEAD,
    HIT_LOC_ARM,
    HIT_LOC_LEG,
    HIT_LOC_COUNT,
} HitLocation;

// clang-format off
#define COMBAT_ATTACK_AIM                  0x00000001
#define COMBAT_ATTACK_HIT                  0x00000002
#define COMBAT_ATTACK_CRITICAL             0x00000004
#define COMBAT_ATTACK_RANGED               0x00000200
#define COMBAT_ATTACK_WEAPON_WEAR          0x00010000
#define COMBAT_ATTACK_TRAP                 0x00080000
// clang-format on

// clang-format off
#define COMBAT_DAMAGE_FULL                0x00000001
#define COMBAT_DAMAGE_RESURRECT           0x00000002
#define COMBAT_DAMAGE_DEATH               0x00000004
#define COMBAT_DAMAGE_BONUS_DAM_50        0x00000008
#define COMBAT_DAMAGE_BONUS_DAM_100       0x00000010
#define COMBAT_DAMAGE_BONUS_DAM_200       0x00000020
#define COMBAT_DAMAGE_STUN                0x00000040
#define COMBAT_DAMAGE_KNOCKDOWN           0x00000080
#define COMBAT_DAMAGE_KNOCKOUT            0x00000100
#define COMBAT_DAMAGE_BLIND               0x00000200
#define COMBAT_DAMAGE_CRIPPLE_ARM         0x00000400
#define COMBAT_DAMAGE_CRIPPLE_LEG         0x00000800
#define COMBAT_DAMAGE_SCAR                0x00001000
#define COMBAT_DAMAGE_DROP_WEAPON         0x00002000
#define COMBAT_DAMAGE_DAMAGE_WEAPON       0x00004000
#define COMBAT_DAMAGE_DESTROY_WEAPON      0x00008000
#define COMBAT_DAMAGE_EXPLODE_WEAPON      0x00010000
#define COMBAT_DAMAGE_LOST_AMMO           0x00020000
#define COMBAT_DAMAGE_DESTROY_AMMO        0x00040000
#define COMBAT_DAMAGE_DROP_HELMET         0x00080000
#define COMBAT_DAMAGE_DAMAGE_ARMOR        0x00100000
#define COMBAT_DAMAGE_HAVE_DAMAGE         0x00200000
#define COMBAT_DAMAGE_REANIMATE           0x00400000
#define COMBAT_DAMAGE_SCALE               0x00800000
#define COMBAT_DAMAGE_IGNORE_RESISTANCE   0x01000000
// clang-format on

typedef struct CombatContext {
    /* 0000 */ unsigned int flags;
    /* 0004 */ int padding_4;
    /* 0008 */ int64_t attacker_obj;
    /* 0010 */ int64_t weapon_obj;
    /* 0018 */ int skill;
    /* 001C */ int padding_1C;
    /* 0020 */ int64_t target_obj;
    /* 0028 */ int64_t original_target_obj;
    /* 0030 */ int64_t credited_attacker_obj;
    /* 0038 */ int64_t target_loc;
    /* 0040 */ int hit_loc;
    /* 0044 */ int dam[DAMAGE_TYPE_COUNT];
    /* 0058 */ unsigned int dam_flags;
    /* 005C */ int total_dam;
    /* 0060 */ int game_difficulty;
    /* 0064 */ int knockout_fatigue_damage;
} CombatContext;

// Serializeable.
static_assert(sizeof(CombatContext) == 0x68, "wrong size");

bool combat_init(GameInitInfo* init_info);
void combat_exit(void);
void combat_reset(void);
bool combat_save(TigFile* stream);
bool combat_load(GameLoadInfo* load_info);
void combat_context_init(int64_t attacker_obj, int64_t target_obj, CombatContext* combat);
int64_t combat_critter_weapon(int64_t critter_obj);
void combat_projectile_finish_from_context(int64_t projectile_obj, int64_t attacker_obj, CombatContext* combat);
bool combat_projectile_step_resolve(int64_t attacker_obj, int64_t target_obj, int64_t target_loc, int64_t projectile_obj, int range, int64_t cur_loc, int64_t original_target_obj);
int combat_attack_resolve(CombatContext* combat);
void combat_weapon_wear_attack(int64_t attacker_obj, int64_t target_obj, int hit_loc);
void combat_throw(int64_t attacker_obj, int64_t weapon_obj, int64_t target_obj, int64_t target_loc, int hit_loc);
bool combat_critter_is_combat_mode_active(int64_t obj);
bool combat_can_exit_combat_mode(int64_t obj);
void combat_critter_deactivate_combat_mode(int64_t obj);
void combat_critter_activate_combat_mode(int64_t obj);
void combat_refresh_fidget_if_allowed(int64_t obj);
void combat_dmg(CombatContext* combat);
void combat_acid_dmg(CombatContext* combat);
void combat_heal(CombatContext* combat);
int combat_hit_loc_penalty(int hit_loc);
int combat_projectile_rot(int64_t from_loc, int64_t to_loc);
tig_art_id_t combat_projectile_art_id_rotation_set(tig_art_id_t aid, int projectile_rot);
bool combat_set_callbacks(CombatCallbacks* callbacks);
bool combat_is_turn_based(void);
bool combat_turn_based_mode_change(bool turn_based);
void combat_turn_based_toggle(void);
bool combat_turn_based_is_active(void);
bool combat_turn_based_has_active_opponents(int64_t pc);
int64_t combat_turn_based_whos_turn_get(void);
void combat_turn_based_whos_turn_set(int64_t obj);
void combat_npc_turn_process(int64_t obj);
bool combat_tb_timeevent_process(TimeEvent* timeevent);
void combat_turn_based_next_subturn(void);
int combat_action_points_get(void);
bool combat_check_action_points(int64_t obj, int required_action_points);
bool combat_check_move_to(int64_t source_obj, int64_t target_loc);
bool combat_check_attack(int64_t source_obj, int64_t target_obj);
bool combat_check_use_obj(int64_t source_obj, int64_t target_obj);
bool combat_check_pick_item(int64_t source_obj, int64_t target_obj);
bool combat_check_cast_spell(int64_t source_obj);
bool combat_check_use_skill(int64_t source_obj);
int combat_required_action_points_get(void);
int combat_attack_cost(int64_t obj);
int combat_move_cost(int64_t source_obj, int64_t target_loc, bool adjacent);
int combat_hit_chance_preview(int64_t source_obj, int64_t target_obj, int hit_loc);
void combat_turn_based_end_critter_turn(int64_t obj);
bool combat_consume_action_points(int64_t obj, int action_points);
void combat_turn_based_add_critter(int64_t obj);
bool combat_current_npc_turn_can_end(int64_t obj);
int combat_turn_based_turn_get(void);
void combat_recalc_reaction(int64_t obj);
bool combat_set_blinded(int64_t obj);
bool combat_auto_attack_get(int64_t obj);
void combat_auto_attack_set(bool value);
bool combat_taunts_get(void);
void combat_taunts_set(bool value);
bool combat_auto_switch_weapons_get(int64_t obj);
void combat_auto_switch_weapons_set(bool value);

#endif /* ARCANUM_GAME_COMBAT_H_ */
