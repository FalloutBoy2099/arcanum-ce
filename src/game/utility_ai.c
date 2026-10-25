#include "game/utility_ai.h"

static int clamp_percent(int value)
{
    return value < 0 ? 0 : value > 100 ? 100 : value;
}

int utility_ai_score(const UtilityAiContext* context, const UtilityAiCandidate* candidate)
{
    int health = clamp_percent(context->health_percent);
    int target_health = clamp_percent(candidate->target_health_percent);
    int target_fatigue = clamp_percent(candidate->target_fatigue_percent);
    int quality = clamp_percent(candidate->base_score);
    int64_t cost = (int64_t)candidate->initial_cost + candidate->maintenance_cost;
    int64_t reserve = context->max_fatigue / 5;
    int score;

    if (context->max_fatigue <= 0 || candidate->initial_cost < 0 || candidate->maintenance_cost < 0
        || candidate->distance >= 1000) return 0;
    if (candidate->target_dead && candidate->action != UTILITY_AI_RESURRECT
        && candidate->action != UTILITY_AI_RELEASE_MAINTENANCE) return 0;
    if (reserve < 5) reserve = 5;
    if (candidate->action == UTILITY_AI_HEAL && target_health <= 25) reserve = 2;
    if (cost > 0 && (int64_t)context->fatigue - cost - context->committed_fatigue < reserve) return 0;

    switch (candidate->action) {
    case UTILITY_AI_OFFENSIVE:
        if (candidate->maintenance_cost > 0
            && (int64_t)context->fatigue * 100 <= (int64_t)context->max_fatigue * 35) return 0;
        score = 55 + quality / 5 + (100 - target_health) / 10;
        if (context->threat_distance > 2) score += 8;
        break;
    case UTILITY_AI_DEFENSIVE:
        score = 15 + (100 - health) + quality / 5;
        if (context->threat_distance <= 2) score += 15;
        break;
    case UTILITY_AI_SUMMON:
        score = 40 + quality / 5;
        if (context->threat_distance > 3) score += 10;
        break;
    case UTILITY_AI_HEAL:
        if (target_health >= 85) return 0;
        score = 70 + (100 - target_health) + quality / 5;
        if (target_health <= 25) score += 35;
        break;
    case UTILITY_AI_RECOVER_FATIGUE:
        if (target_fatigue >= 50) return 0;
        score = 80 + (100 - target_fatigue) + quality / 5;
        break;
    case UTILITY_AI_CURE_POISON:
        if (candidate->target_poison_level <= 0) return 0;
        score = 70 + clamp_percent(candidate->target_poison_level / 3) + quality / 5;
        if (target_health < 50) score += 15;
        break;
    case UTILITY_AI_RESURRECT:
        if (!candidate->target_dead) return 0;
        score = (context->threat_distance <= 2 ? 55 : 95) + quality / 5;
        if (health < 40) score -= 25;
        break;
    case UTILITY_AI_RETREAT:
        if (context->threat_distance > 2 || context->threat_distance < 0) return 0;
        if (health <= 30) {
            score = 95 + (30 - health);
        } else if ((int64_t)context->fatigue * 5 <= context->max_fatigue) {
            score = 85;
        } else {
            return 0;
        }
        break;
    case UTILITY_AI_RELEASE_MAINTENANCE:
        if ((int64_t)context->fatigue * 5 > context->max_fatigue
            || candidate->released_maintenance_cost <= 0
            || (int64_t)context->fatigue - context->committed_fatigue >= reserve) return 0;
        score = 110 + (candidate->released_maintenance_cost > 20 ? 20 : candidate->released_maintenance_cost);
        break;
    default:
        return 0;
    }
    score -= (int)(cost * 80 / context->max_fatigue);
    if (candidate->consumable) score -= 10;
    if (candidate->distance > 1 && candidate->distance < 1000) score -= candidate->distance / 2;
    return score > 0 ? score : 0;
}

int utility_ai_retreat_position_score(const int* current_distances, const int* destination_distances,
    int count, int leader_distance)
{
    int current_nearest = 1000;
    int destination_nearest = 1000;
    int index;
    if (count <= 0 || count > UTILITY_AI_RETREAT_THREAT_LIMIT || leader_distance < 0 || leader_distance > 8) return 0;
    for (index = 0; index < count; index++) {
        if (current_distances[index] < 0 || destination_distances[index] >= 1000
            || destination_distances[index] < current_distances[index]) return 0;
        if (current_distances[index] < current_nearest) current_nearest = current_distances[index];
        if (destination_distances[index] < destination_nearest) destination_nearest = destination_distances[index];
    }
    if (destination_nearest <= current_nearest) return 0;
    return (destination_nearest - current_nearest) * 16 + 8 - leader_distance;
}

int utility_ai_ally_priority(int health_percent, int fatigue_percent, int poison_level, bool dead,
    int distance, const UtilityAiSupport* support)
{
    int health = clamp_percent(health_percent);
    int fatigue = clamp_percent(fatigue_percent);
    int priority = 0;
    if (distance < 0 || distance > 12) return 0;
    if (dead) return support->resurrect ? 60 - distance : 0;
    if (health < support->healing_health_limit) priority = 100 - health + (health <= 30 ? 50 : 0);
    if (support->recover_fatigue && fatigue < 50 && 100 - fatigue > priority) priority = 100 - fatigue;
    if (support->cure_poison && poison_level > 0 && 70 + clamp_percent(poison_level / 3) > priority) {
        priority = 70 + clamp_percent(poison_level / 3);
    }
    return priority > 0 ? priority + 12 - distance : 0;
}

int utility_ai_maintenance_cost(int cost, int64_t period_seconds, int64_t first_payment_seconds, int horizon_seconds)
{
    int64_t payments;
    int64_t total;
    if (cost <= 0 || period_seconds <= 0 || horizon_seconds <= 0) return 0;
    if (first_payment_seconds < 0) first_payment_seconds = 0;
    if (first_payment_seconds > horizon_seconds) return 0;
    payments = 1 + (horizon_seconds - first_payment_seconds) / period_seconds;
    total = payments * cost;
    return total > 1000000 ? 1000000 : (int)total;
}

int utility_ai_fatigue_payment(int cost, bool fatigue_limiting)
{
    if (cost < 0) return 0;
    if (!fatigue_limiting) return cost;
    return cost / 2 > 0 ? cost / 2 : 1;
}

static bool utility_ai_same_execution(const UtilityAiCandidate* first, const UtilityAiCandidate* second)
{
    if (first->action == UTILITY_AI_RETREAT || second->action == UTILITY_AI_RETREAT) {
        return first->action == second->action && first->destination == second->destination;
    }
    if (first->action == UTILITY_AI_RELEASE_MAINTENANCE || second->action == UTILITY_AI_RELEASE_MAINTENANCE) {
        return first->action == second->action && first->maintenance_id == second->maintenance_id
            && first->spell == second->spell && first->target == second->target;
    }
    return first->spell == second->spell && first->item == second->item && first->target == second->target;
}

int utility_ai_choose(const UtilityAiContext* context, const UtilityAiCandidate* candidates,
    int count, UtilityAiValidate validate, void* validation_context, int* score)
{
    int scores[UTILITY_AI_CANDIDATE_LIMIT];
    int index;
    int attempt;
    if (score != 0) *score = UTILITY_AI_ATTACK_SCORE;
    if (count < 0) return -1;
    if (count > UTILITY_AI_CANDIDATE_LIMIT) count = UTILITY_AI_CANDIDATE_LIMIT;
    for (index = 0; index < count; index++) scores[index] = utility_ai_score(context, &candidates[index]);
    for (attempt = 0; attempt < UTILITY_AI_VALIDATION_LIMIT; attempt++) {
        int best = -1;
        int best_score = UTILITY_AI_ATTACK_SCORE;
        for (index = 0; index < count; index++) {
            if (scores[index] > best_score) {
                best = index;
                best_score = scores[index];
            }
        }
        if (best < 0) return -1;
        if (validate == 0 || validate(&candidates[best], validation_context)) {
            if (score != 0) *score = best_score;
            return best;
        }
        for (index = 0; index < count; index++) {
            if (utility_ai_same_execution(&candidates[best], &candidates[index])) scores[index] = 0;
        }
    }
    return -1;
}
