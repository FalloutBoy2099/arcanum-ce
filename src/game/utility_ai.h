#ifndef ARCANUM_GAME_UTILITY_AI_H_
#define ARCANUM_GAME_UTILITY_AI_H_

#include <stdbool.h>
#include <stdint.h>

#define UTILITY_AI_CANDIDATE_LIMIT 128
#define UTILITY_AI_VALIDATION_LIMIT 8
#define UTILITY_AI_ATTACK_SCORE 35
#define UTILITY_AI_RETREAT_THREAT_LIMIT 8

typedef enum UtilityAiAction {
    UTILITY_AI_OFFENSIVE,
    UTILITY_AI_DEFENSIVE,
    UTILITY_AI_SUMMON,
    UTILITY_AI_HEAL,
    UTILITY_AI_RECOVER_FATIGUE,
    UTILITY_AI_CURE_POISON,
    UTILITY_AI_RESURRECT,
    UTILITY_AI_RETREAT,
    UTILITY_AI_RELEASE_MAINTENANCE,
} UtilityAiAction;

typedef struct UtilityAiContext {
    int health_percent;
    int fatigue;
    int max_fatigue;
    int threat_distance;
    int committed_fatigue;
} UtilityAiContext;

typedef struct UtilityAiSupport {
    int healing_health_limit;
    bool recover_fatigue;
    bool cure_poison;
    bool resurrect;
} UtilityAiSupport;

typedef struct UtilityAiCandidate {
    UtilityAiAction action;
    int spell;
    int64_t item;
    int64_t target;
    int target_health_percent;
    int target_fatigue_percent;
    int distance;
    int base_score;
    int initial_cost;
    int maintenance_cost;
    int target_poison_level;
    bool target_dead;
    bool consumable;
    int64_t destination;
    int maintenance_id;
    int released_maintenance_cost;
} UtilityAiCandidate;

typedef bool (*UtilityAiValidate)(const UtilityAiCandidate* candidate, void* context);

int utility_ai_score(const UtilityAiContext* context, const UtilityAiCandidate* candidate);
int utility_ai_retreat_position_score(const int* current_distances, const int* destination_distances,
    int count, int leader_distance);
int utility_ai_ally_priority(int health_percent, int fatigue_percent, int poison_level, bool dead,
    int distance, const UtilityAiSupport* support);
int utility_ai_maintenance_cost(int cost, int64_t period_seconds, int64_t first_payment_seconds, int horizon_seconds);
int utility_ai_fatigue_payment(int cost, bool fatigue_limiting);
int utility_ai_choose(const UtilityAiContext* context, const UtilityAiCandidate* candidates,
    int count, UtilityAiValidate validate, void* validation_context, int* score);

#endif
