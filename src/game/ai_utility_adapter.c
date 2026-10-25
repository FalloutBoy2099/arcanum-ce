#include "game/ai_utility_adapter.h"

#include <tig/performance.h>

#include "game/anim.h"
#include "game/anim_private.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/gamelib.h"
#include "game/item.h"
#include "game/magictech.h"
#include "game/map.h"
#include "game/mt_ai.h"
#include "game/mt_item.h"
#include "game/object_node.h"
#include "game/path.h"
#include "game/spell.h"
#include "game/stat.h"
#include "game/utility_ai.h"

typedef struct AiRetreatCooldown {
    int64_t actor;
    int map;
    tig_timestamp_t started;
    int delay;
    int64_t destination;
} AiRetreatCooldown;

static AiRetreatCooldown ai_retreat_cooldowns[64];

typedef struct AiUtilityResources {
    MagicTechAiActionList actions[MAGICTECH_AI_ACTION_COUNT];
    MagicTechAiActionListEntry entries[MAGICTECH_AI_ACTION_COUNT][MT_AI_UTILITY_RESOURCE_LIMIT];
    unsigned int loaded_actions;
    const UtilityAiContext* context;
} AiUtilityResources;

void ai_utility_reset(void)
{
    memset(ai_retreat_cooldowns, 0, sizeof(ai_retreat_cooldowns));
}

int ai_utility_mode(Ai* ai)
{
    int mode = settings_get_value(&settings, "utility_ai");
    if (mode < 1 || mode > 3 || obj_field_int32_get(ai->obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) return 0;
    if (mode != 3 && critter_pc_leader_get(ai->obj) == OBJ_HANDLE_NULL) return 0;
    return mode;
}

static int ai_utility_percent(int value, int maximum)
{
    if (maximum <= 0 || value <= 0) return 0;
    return value >= maximum ? 100 : (int)((int64_t)value * 100 / maximum);
}

static bool ai_utility_maintenance_releasable(Ai* ai, const UtilityAiCandidate* candidate)
{
    MagicTechRunInfo* run;
    MagicTechInfo* info;
    int action;
    int location;
    if (magictech_run_info == NULL || magictech_spells == NULL
        || candidate->maintenance_id < 0 || candidate->maintenance_id >= 512
        || candidate->spell < 0 || candidate->spell >= MAGICTECH_PLAYER_SPELL_COUNT) return false;
    run = &magictech_run_info[candidate->maintenance_id];
    if (run->id != candidate->maintenance_id || run->spell != candidate->spell
        || (run->flags & (MAGICTECH_RUN_ACTIVE | MAGICTECH_RUN_UTILITY_AI))
            != (MAGICTECH_RUN_ACTIVE | MAGICTECH_RUN_UTILITY_AI)
        || (run->flags & (MAGICTECH_RUN_FREE | MAGICTECH_RUN_END_IMMEDIATE | MAGICTECH_RUN_DISPELLED)) != 0
        || run->action != MAGICTECH_ACTION_MAINTAIN || run->source_obj.obj != ai->obj
        || run->parent_obj.obj != ai->obj || run->target_obj.obj != ai->danger_source
        || run->target_obj.obj == ai->obj || run->target_obj.obj == OBJ_HANDLE_NULL
        || run->summoned_obj != NULL || run->resistance_amount != 0) return false;
    if ((int64_t)critter_fatigue_current(ai->obj) * 5 > critter_fatigue_max(ai->obj)) return false;
    info = &magictech_spells[run->spell];
    if ((info->flags & MAGICTECH_IS_TECH) != 0 || info->maintenance.cost <= 0
        || info->maintenance.period <= 0 || info->ai.values[MAGICTECH_AI_ACTION_OFFENSIVE] <= 0) return false;
    for (action = 0; action < MAGICTECH_AI_ACTION_COUNT; action++) {
        if (action != MAGICTECH_AI_ACTION_OFFENSIVE && info->ai.values[action] > 0) return false;
    }
    for (location = FIRST_WEAR_INV_LOC; location <= LAST_WEAR_INV_LOC; location++) {
        int64_t item = item_wield_get(ai->obj, location);
        if (item != OBJ_HANDLE_NULL && obj_field_int32_get(item, OBJ_F_ITEM_MANA_STORE) != 0) return false;
    }
    return true;
}

static void ai_utility_maintenance_candidates_add(Ai* ai, const UtilityAiContext* context,
    UtilityAiCandidate* candidates, int* count)
{
    DateTime now;
    int index;
    int accepted = 0;
    if (context->max_fatigue <= 0 || (int64_t)context->fatigue * 5 > context->max_fatigue
        || context->committed_fatigue <= 0 || magictech_run_info == NULL) return;
    now = game_time_get();
    for (index = 0; index < 512 && accepted < 8 && *count < UTILITY_AI_CANDIDATE_LIMIT; index++) {
        MagicTechRunInfo* run = &magictech_run_info[index];
        if ((run->flags & (MAGICTECH_RUN_ACTIVE | MAGICTECH_RUN_UTILITY_AI))
            != (MAGICTECH_RUN_ACTIVE | MAGICTECH_RUN_UTILITY_AI)) continue;
        UtilityAiCandidate candidate = { .action = UTILITY_AI_RELEASE_MAINTENANCE,
            .spell = run->spell, .target = ai->danger_source, .maintenance_id = index };
        int period;
        int cost;
        int64_t first_payment;
        if (!ai_utility_maintenance_releasable(ai, &candidate)) continue;
        cost = spell_maintain_cost(run->spell, ai->obj, &period);
        cost = utility_ai_fatigue_payment(cost,
            (obj_field_int32_get(ai->obj, OBJ_F_CRITTER_FLAGS) & OCF_FATIGUE_LIMITING) != 0);
        first_payment = ((int64_t)run->event_time.days - now.days) * 86400
            + (int64_t)run->event_time.milliseconds / 1000 - now.milliseconds / 1000;
        candidate.released_maintenance_cost = utility_ai_maintenance_cost(cost,
            (int64_t)period * 8, first_payment, 80);
        if (utility_ai_score(context, &candidate) <= UTILITY_AI_ATTACK_SCORE) continue;
        candidates[(*count)++] = candidate;
        accepted++;
    }
}

static bool ai_utility_candidate_valid(const UtilityAiCandidate* candidate, void* context)
{
    Ai* ai = (Ai*)context;
    int64_t owner;
    if (candidate->action == UTILITY_AI_RETREAT) return true;
    if (candidate->action == UTILITY_AI_RELEASE_MAINTENANCE) return ai_utility_maintenance_releasable(ai, candidate);
    if (candidate->target == OBJ_HANDLE_NULL) return false;
    if (critter_is_dead(candidate->target) != (candidate->action == UTILITY_AI_RESURRECT)) return false;
    if (candidate->item != OBJ_HANDLE_NULL
        && (!item_parent(candidate->item, &owner) || owner != ai->obj)) return false;
    if (candidate->spell < 0) {
        return candidate->item != OBJ_HANDLE_NULL
            && item_check_use_box_flag(ai->obj, candidate->item) == ITEM_CANNOT_OK;
    }
    return !magictech_is_under_influence_of(candidate->target, candidate->spell)
        && ai_magictech_action_check(ai->obj, candidate->item, candidate->target, candidate->spell) == 0;
}

static const MagicTechAiActionList* ai_utility_resources_get(Ai* ai, AiUtilityResources* resources, int action)
{
    unsigned int bit = 1u << action;
    if ((resources->loaded_actions & bit) == 0) {
        resources->actions[action].entries = resources->entries[action];
        resources->actions[action].cnt = mt_ai_utility_actions_collect(ai->obj, action,
            resources->entries[action], MT_AI_UTILITY_RESOURCE_LIMIT);
        resources->loaded_actions |= bit;
    }
    return &resources->actions[action];
}

static UtilityAiSupport ai_utility_support_get(Ai* ai, AiUtilityResources* resources,
    int health, int fatigue, int poison, bool dead)
{
    UtilityAiSupport support = { 0 };
    if (dead) {
        support.resurrect = ai_utility_resources_get(ai, resources, MAGICTECH_AI_ACTION_RESURRECT)->cnt > 0;
        return support;
    }
    if (health < 85) {
        if (ai_utility_resources_get(ai, resources, MAGICTECH_AI_ACTION_HEAL_LIGHT)->cnt > 0) support.healing_health_limit = 85;
        else if (health <= 65 && ai_utility_resources_get(ai, resources, MAGICTECH_AI_ACTION_HEAL_MEDIUM)->cnt > 0) support.healing_health_limit = 66;
        else if (health <= 40 && ai_utility_resources_get(ai, resources, MAGICTECH_AI_ACTION_HEAL_HEAVY)->cnt > 0) support.healing_health_limit = 41;
    }
    if (fatigue < 50) support.recover_fatigue = ai_utility_resources_get(ai, resources, MAGICTECH_AI_ACTION_FATIGUE_RECOVER)->cnt > 0;
    if (poison > 0) support.cure_poison = ai_utility_resources_get(ai, resources, MAGICTECH_AI_ACTION_CURE_POISON)->cnt > 0;
    return support;
}

static void ai_utility_candidates_add(Ai* ai, AiUtilityResources* resources, UtilityAiCandidate* candidates, int* count,
    int action, UtilityAiAction utility_action, int64_t target, int limit)
{
    const MagicTechAiActionList* list;
    UtilityAiCandidate best[UTILITY_AI_VALIDATION_LIMIT];
    int scores[UTILITY_AI_VALIDATION_LIMIT];
    int index;
    int accepted = 0;
    if (target == OBJ_HANDLE_NULL) return;
    if (critter_is_dead(target) != (utility_action == UTILITY_AI_RESURRECT)) return;
    if (limit > UTILITY_AI_VALIDATION_LIMIT) limit = UTILITY_AI_VALIDATION_LIMIT;
    if (limit <= 0) return;
    list = ai_utility_resources_get(ai, resources, action);
    for (index = 0; index < list->cnt && index < MT_AI_UTILITY_RESOURCE_LIMIT; index++) {
        const MagicTechAiActionListEntry* entry = &list->entries[index];
        UtilityAiCandidate candidate = { 0 };
        int duplicate;
        int period = 0;
        int maintenance = 0;
        int64_t distance;
        int score;
        int insert;
        int move;
        if (entry->spell < -1 || entry->spell >= MT_SPELL_COUNT) continue;
        if (entry->spell == -1 && entry->item_obj == OBJ_HANDLE_NULL) continue;
        candidate.action = utility_action;
        candidate.spell = entry->spell;
        candidate.item = entry->item_obj;
        candidate.target = target;
        if (utility_action == UTILITY_AI_DEFENSIVE && entry->spell >= 0
            && mt_ai_secondary_defense_score_get(entry->spell)) candidate.target = ai->danger_source;
        for (duplicate = 0; duplicate < *count; duplicate++) {
            if (candidates[duplicate].spell == candidate.spell && candidates[duplicate].item == candidate.item
                && candidates[duplicate].target == candidate.target && candidates[duplicate].action == candidate.action) break;
        }
        if (duplicate < *count) continue;
        candidate.target_health_percent = ai_utility_percent(object_hp_current(candidate.target), object_hp_max(candidate.target));
        candidate.target_fatigue_percent = ai_utility_percent(critter_fatigue_current(candidate.target), critter_fatigue_max(candidate.target));
        candidate.target_dead = critter_is_dead(candidate.target);
        candidate.target_poison_level = stat_level_get(candidate.target, STAT_POISON_LEVEL);
        candidate.consumable = candidate.item != OBJ_HANDLE_NULL
            && (obj_field_int32_get(candidate.item, OBJ_F_TYPE) == OBJ_TYPE_FOOD
                || obj_field_int32_get(candidate.item, OBJ_F_TYPE) == OBJ_TYPE_SCROLL);
        if (utility_action == UTILITY_AI_HEAL
            && ((action == MAGICTECH_AI_ACTION_HEAL_MEDIUM && candidate.target_health_percent > 65)
                || (action == MAGICTECH_AI_ACTION_HEAL_HEAVY && candidate.target_health_percent > 40))) continue;
        distance = object_dist(ai->obj, candidate.target);
        candidate.distance = distance > 1000 ? 1000 : distance < 0 ? 0 : (int)distance;
        candidate.base_score = entry->spell >= 0 ? magictech_spells[entry->spell].ai.values[action] : 0;
        if (entry->item_obj == OBJ_HANDLE_NULL && entry->spell >= 0) {
            bool fatigue_limiting = (obj_field_int32_get(ai->obj, OBJ_F_CRITTER_FLAGS) & OCF_FATIGUE_LIMITING) != 0;
            candidate.initial_cost = utility_ai_fatigue_payment(spell_cast_cost(entry->spell, ai->obj), fatigue_limiting);
            maintenance = spell_maintain_cost(entry->spell, ai->obj, &period);
            maintenance = utility_ai_fatigue_payment(maintenance, fatigue_limiting);
            if (period > 0 && maintenance > 0) {
                candidate.maintenance_cost = utility_ai_maintenance_cost(maintenance,
                    (int64_t)period * 8, (int64_t)period * 8, 80);
            }
        }
        score = utility_ai_score(resources->context, &candidate);
        if (score <= UTILITY_AI_ATTACK_SCORE) continue;
        for (insert = 0; insert < accepted; insert++) if (score > scores[insert]) break;
        if (insert >= limit) continue;
        if (accepted < limit) accepted++;
        for (move = accepted - 1; move > insert; move--) {
            best[move] = best[move - 1];
            scores[move] = scores[move - 1];
        }
        best[insert] = candidate;
        scores[insert] = score;
    }
    for (index = 0; index < accepted && *count < UTILITY_AI_CANDIDATE_LIMIT; index++) candidates[(*count)++] = best[index];
}

static void ai_utility_ally_consider(Ai* ai, AiUtilityResources* resources,
    int64_t target, int64_t* allies, int* priorities)
{
    int64_t distance;
    int priority;
    int index;
    int insert;
    int health;
    int fatigue;
    int poison;
    bool dead;
    UtilityAiSupport support;
    if (target == OBJ_HANDLE_NULL || !obj_type_is_critter(obj_field_int32_get(target, OBJ_F_TYPE))) return;
    for (index = 0; index < 4; index++) if (allies[index] == target) return;
    distance = object_dist(ai->obj, target);
    if (distance < 0 || distance > 12) return;
    dead = critter_is_dead(target);
    health = ai_utility_percent(object_hp_current(target), object_hp_max(target));
    fatigue = ai_utility_percent(critter_fatigue_current(target), critter_fatigue_max(target));
    poison = stat_level_get(target, STAT_POISON_LEVEL);
    support = ai_utility_support_get(ai, resources, health, fatigue, poison, dead);
    priority = utility_ai_ally_priority(health, fatigue, poison, dead, (int)distance, &support);
    if (priority <= 0) return;
    for (insert = 0; insert < 4; insert++) if (priority > priorities[insert]) break;
    if (insert == 4) return;
    for (index = 3; index > insert; index--) {
        priorities[index] = priorities[index - 1];
        allies[index] = allies[index - 1];
    }
    priorities[insert] = priority;
    allies[insert] = target;
}

static void ai_utility_support_candidates_add(Ai* ai, AiUtilityResources* resources,
    UtilityAiCandidate* candidates, int* count, int64_t target)
{
    int action;
    if (target == OBJ_HANDLE_NULL) return;
    if (critter_is_dead(target)) {
        ai_utility_candidates_add(ai, resources, candidates, count, MAGICTECH_AI_ACTION_RESURRECT, UTILITY_AI_RESURRECT, target, 2);
        return;
    }
    if (ai_utility_percent(critter_fatigue_current(target), critter_fatigue_max(target)) < 50) {
        ai_utility_candidates_add(ai, resources, candidates, count, MAGICTECH_AI_ACTION_FATIGUE_RECOVER, UTILITY_AI_RECOVER_FATIGUE, target, 2);
    }
    if (ai_utility_percent(object_hp_current(target), object_hp_max(target)) < 85) {
        for (action = MAGICTECH_AI_ACTION_HEAL_LIGHT; action <= MAGICTECH_AI_ACTION_HEAL_HEAVY; action++) {
            ai_utility_candidates_add(ai, resources, candidates, count, action, UTILITY_AI_HEAL, target, 2);
        }
    }
    if (stat_level_get(target, STAT_POISON_LEVEL) > 0) {
        ai_utility_candidates_add(ai, resources, candidates, count, MAGICTECH_AI_ACTION_CURE_POISON, UTILITY_AI_CURE_POISON, target, 2);
    }
}

static AiRetreatCooldown* ai_utility_retreat_cooldown(int64_t actor)
{
    AiRetreatCooldown* available = NULL;
    int map = map_current_map();
    int index;
    for (index = 0; index < 64; index++) {
        AiRetreatCooldown* entry = &ai_retreat_cooldowns[index];
        bool expired = entry->actor == OBJ_HANDLE_NULL || entry->map != map
            || tig_timer_elapsed(entry->started) >= entry->delay;
        if (entry->actor == actor && entry->map == map) return expired ? entry : NULL;
        if (expired && available == NULL) available = entry;
    }
    return available;
}

static bool ai_utility_retreat_step_valid(Ai* ai, int64_t destination)
{
    PathCreateInfo path = { 0 };
    int8_t rotation;
    if (combat_turn_based_is_active()
        || (obj_field_int32_get(ai->obj, OBJ_F_NPC_FLAGS) & (ONF_AI_WAIT_HERE | ONF_BACKING_OFF)) != 0
        || (obj_field_int32_get(ai->obj, OBJ_F_CRITTER_FLAGS) & OCF_NO_FLEE) != 0) return false;
    path.obj = ai->obj;
    path.from = obj_field_int64_get(ai->obj, OBJ_F_LOCATION);
    path.to = destination;
    path.max_rotations = 1;
    path.rotations = &rotation;
    path.flags = PATH_FLAG_DIRECT_ONLY | PATH_FLAG_PORTALS_BLOCK | PATH_FLAG_WINDOWS_BLOCK | PATH_FLAG_AVOID_FIRE;
    return path_make(&path) == 1;
}

static void ai_utility_retreat_candidate_add(Ai* ai, AiUtilityResources* resources, const UtilityAiContext* context,
    UtilityAiCandidate* candidates, int* count)
{
    UtilityAiCandidate candidate = { .action = UTILITY_AI_RETREAT, .spell = -3, .target = ai->danger_source };
    AiRetreatCooldown* cooldown;
    AiParams params;
    ObjectList objects;
    ObjectNode* node;
    int64_t threats[UTILITY_AI_RETREAT_THREAT_LIMIT];
    int current_distances[UTILITY_AI_RETREAT_THREAT_LIMIT];
    int destination_distances[UTILITY_AI_RETREAT_THREAT_LIMIT];
    int64_t origin = obj_field_int64_get(ai->obj, OBJ_F_LOCATION);
    int64_t leader = critter_pc_leader_get(ai->obj);
    int threat_count = 1;
    int inspected = 0;
    int best_score = 0;
    int direction;
    int index;
    bool overflow = false;
    if (*count >= UTILITY_AI_CANDIDATE_LIMIT || combat_turn_based_is_active()
        || !obj_type_is_critter(obj_field_int32_get(ai->danger_source, OBJ_F_TYPE))
        || (obj_field_int32_get(ai->obj, OBJ_F_NPC_FLAGS) & (ONF_AI_WAIT_HERE | ONF_BACKING_OFF)) != 0
        || (obj_field_int32_get(ai->obj, OBJ_F_CRITTER_FLAGS) & OCF_NO_FLEE) != 0
        || utility_ai_score(context, &candidate) == 0) return;
    ai_copy_params(ai->obj, &params);
    if (context->health_percent > 30 && params.minimum_combat_distance <= 1) {
        if (ai_utility_resources_get(ai, resources, MAGICTECH_AI_ACTION_OFFENSIVE)->cnt == 0) return;
    }
    cooldown = ai_utility_retreat_cooldown(ai->obj);
    if (cooldown == NULL) return;
    cooldown->actor = ai->obj;
    cooldown->map = map_current_map();
    cooldown->delay = 1000;
    cooldown->destination = 0;
    tig_timer_now(&cooldown->started);
    if (leader == OBJ_HANDLE_NULL) leader = ai->leader_obj;
    threats[0] = obj_field_int64_get(ai->danger_source, OBJ_F_LOCATION);
    ai_objects_in_radius(ai->obj, 6, &objects, OBJ_TM_NPC);
    for (node = objects.head; node != NULL; node = node->next) {
        int danger;
        int64_t target;
        if (++inspected > 64) { overflow = true; break; }
        if (node->obj == ai->obj || node->obj == ai->danger_source || critter_is_dead(node->obj)
            || critter_is_unconscious(node->obj)) continue;
        ai_danger_source(node->obj, &danger, &target);
        if (danger != AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS || target == OBJ_HANDLE_NULL) continue;
        if (target != ai->obj && (leader == OBJ_HANDLE_NULL
                || (target != leader && critter_pc_leader_get(target) != leader))) continue;
        if (ai_can_see(ai->obj, node->obj) != 0) continue;
        if (threat_count == UTILITY_AI_RETREAT_THREAT_LIMIT) { overflow = true; break; }
        threats[threat_count++] = obj_field_int64_get(node->obj, OBJ_F_LOCATION);
    }
    object_list_destroy(&objects);
    if (overflow) return;
    for (index = 0; index < threat_count; index++) current_distances[index] = (int)location_dist(origin, threats[index]);
    for (direction = 0; direction < 8; direction++) {
        int64_t destination;
        int position_score;
        int leader_distance;
        if (!location_in_dir(origin, direction, &destination)) continue;
        leader_distance = leader == OBJ_HANDLE_NULL ? 0
            : (int)location_dist(destination, obj_field_int64_get(leader, OBJ_F_LOCATION));
        for (index = 0; index < threat_count; index++) destination_distances[index] = (int)location_dist(destination, threats[index]);
        position_score = utility_ai_retreat_position_score(current_distances, destination_distances, threat_count, leader_distance);
        if (position_score <= best_score || !ai_utility_retreat_step_valid(ai, destination)) continue;
        best_score = position_score;
        candidate.destination = destination;
    }
    if (best_score > 0) candidates[(*count)++] = candidate;
}

bool ai_utility_spell_select(Ai* ai, bool apply)
{
    UtilityAiContext context;
    AiUtilityResources resources = { 0 };
    UtilityAiCandidate candidates[UTILITY_AI_CANDIDATE_LIMIT];
    int count = 0;
    int choice;
    int score;
    int index;
    int64_t allies[4] = { 0 };
    int priorities[4] = { 0 };
    int64_t leader = critter_pc_leader_get(ai->obj);
    int64_t distance;
    const UtilityAiCandidate* selected;
    if (ai->danger_source == OBJ_HANDLE_NULL || critter_is_dead(ai->danger_source)) return false;
    context.health_percent = ai_utility_percent(object_hp_current(ai->obj), object_hp_max(ai->obj));
    context.fatigue = critter_fatigue_current(ai->obj);
    context.max_fatigue = critter_fatigue_max(ai->obj);
    context.committed_fatigue = magictech_active_maintenance_cost_estimate(ai->obj, 80);
    distance = object_dist(ai->obj, ai->danger_source);
    context.threat_distance = distance > 1000 ? 1000 : distance < 0 ? 0 : (int)distance;
    resources.context = &context;
    if (leader == OBJ_HANDLE_NULL) leader = ai->leader_obj;
    if (leader == OBJ_HANDLE_NULL) leader = ai->obj;
    ai_utility_ally_consider(ai, &resources, ai->obj, allies, priorities);
    ai_utility_ally_consider(ai, &resources, leader, allies, priorities);
    for (index = 0; index < obj_arrayfield_length_get(leader, OBJ_F_CRITTER_FOLLOWER_IDX) && index < 32; index++) {
        ai_utility_ally_consider(ai, &resources, obj_arrayfield_handle_get(leader, OBJ_F_CRITTER_FOLLOWER_IDX, index), allies, priorities);
    }
    ai_utility_candidates_add(ai, &resources, candidates, &count, MAGICTECH_AI_ACTION_FATIGUE_RECOVER, UTILITY_AI_RECOVER_FATIGUE, ai->obj, 2);
    for (index = 0; index < 4; index++) ai_utility_support_candidates_add(ai, &resources, candidates, &count, allies[index]);
    ai_utility_candidates_add(ai, &resources, candidates, &count, MAGICTECH_AI_ACTION_DEFENSIVE, UTILITY_AI_DEFENSIVE, ai->obj, 8);
    ai_utility_candidates_add(ai, &resources, candidates, &count, MAGICTECH_AI_ACTION_OFFENSIVE, UTILITY_AI_OFFENSIVE, ai->danger_source, 8);
    if (critter_num_followers(ai->obj, false) == 0) {
        ai_utility_candidates_add(ai, &resources, candidates, &count, MAGICTECH_AI_ACTION_SUMMON, UTILITY_AI_SUMMON, ai->obj, 8);
    }
    ai_utility_retreat_candidate_add(ai, &resources, &context, candidates, &count);
    ai_utility_maintenance_candidates_add(ai, &context, candidates, &count);
    choice = utility_ai_choose(&context, candidates, count, apply ? ai_utility_candidate_valid : NULL, ai, &score);
    tig_performance_trace(TIG_PERF_EVENT_UTILITY_AI, (Uint64)ai->obj,
        choice < 0 ? (Uint64)ai->danger_source : (Uint64)candidates[choice].target,
        choice < 0 ? -2 : candidates[choice].action == UTILITY_AI_RELEASE_MAINTENANCE ? -4 : candidates[choice].spell,
        apply ? score : -score);
    tig_performance_trace(TIG_PERF_EVENT_UTILITY_AI_DETAIL, (Uint64)ai->obj,
        choice < 0 ? (Uint64)ai->danger_source : (Uint64)candidates[choice].target,
        choice < 0 ? 0 : candidates[choice].action + 1,
        (int64_t)context.fatigue - context.committed_fatigue
            - (choice < 0 ? 0 : (int64_t)candidates[choice].initial_cost + candidates[choice].maintenance_cost)
            + (choice < 0 ? 0 : candidates[choice].released_maintenance_cost));
    if (!apply || choice < 0) return false;
    selected = &candidates[choice];
    ai->utility_selected = true;
    if (selected->action == UTILITY_AI_RELEASE_MAINTENANCE) {
        ai->maintenance_id = selected->maintenance_id;
        ai->spell = selected->spell;
        ai->action_type = AI_ACTION_TYPE_RELEASE_MAINTENANCE;
        return true;
    }
    if (selected->action == UTILITY_AI_RETREAT) {
        ai->move_destination = selected->destination;
        ai->action_type = AI_ACTION_TYPE_RETREAT;
        return true;
    }
    ai->spell = selected->spell;
    ai->item_obj = selected->item;
    ai->danger_source = selected->target;
    ai->action_type = selected->item != OBJ_HANDLE_NULL
        && (selected->spell == -1 || mt_item_valid_ai_action(selected->item))
        ? AI_ACTION_TYPE_USE_ITEM : AI_ACTION_TYPE_CAST_SPELL;
    return true;
}

bool ai_utility_retreat_pending(Ai* ai)
{
    for (int index = 0; index < 64; index++) {
        AiRetreatCooldown* entry = &ai_retreat_cooldowns[index];
        if (entry->actor == ai->obj && entry->map == map_current_map()
            && entry->destination != 0 && tig_timer_elapsed(entry->started) < 2000
            && obj_field_int64_get(ai->obj, OBJ_F_LOCATION) != entry->destination
            && anim_is_current_goal_type(ai->obj, AG_MOVE_TO_TILE, NULL)) return true;
    }
    return false;
}

bool ai_utility_action_perform(Ai* ai)
{
    if (ai->action_type == AI_ACTION_TYPE_RELEASE_MAINTENANCE) {
        UtilityAiCandidate candidate = { .spell = ai->spell, .maintenance_id = ai->maintenance_id };
        if (!ai_utility_maintenance_releasable(ai, &candidate)) return false;
        magictech_run_info[ai->maintenance_id].flags &= ~MAGICTECH_RUN_UTILITY_AI;
        magictech_interrupt_delayed(ai->maintenance_id);
        return true;
    }
    if (ai->action_type == AI_ACTION_TYPE_RETREAT
        && ai_utility_retreat_step_valid(ai, ai->move_destination)
        && anim_goal_move_to_tile(ai->obj, ai->move_destination)) {
        for (int index = 0; index < 64; index++) {
            if (ai_retreat_cooldowns[index].actor == ai->obj
                && ai_retreat_cooldowns[index].map == map_current_map()) {
                tig_timer_now(&ai_retreat_cooldowns[index].started);
                ai_retreat_cooldowns[index].delay = 5000;
                ai_retreat_cooldowns[index].destination = ai->move_destination;
                break;
            }
        }
        return true;
    }
    return false;
}
