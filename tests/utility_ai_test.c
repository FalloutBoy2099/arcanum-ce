#include <stdio.h>
#include <string.h>
#include "game/utility_ai.h"

static int failures;
static int validations;
static int rejected_spell;
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #condition); failures++; } } while (0)

static bool validate(const UtilityAiCandidate* candidate, void* context)
{
    (void)context;
    validations++;
    return candidate->spell != rejected_spell;
}

static bool reject_all(const UtilityAiCandidate* candidate, void* context)
{
    (void)candidate; (void)context;
    validations++;
    return false;
}

static bool validate_target(const UtilityAiCandidate* candidate, void* context)
{
    validations++;
    return candidate->target != *(int64_t*)context;
}

int main(void)
{
    UtilityAiContext context = { 100, 50, 50, 4 };
    UtilityAiCandidate candidates[UTILITY_AI_CANDIDATE_LIMIT + 1] = { 0 };
    UtilityAiCandidate snapshot[UTILITY_AI_CANDIDATE_LIMIT + 1];
    int index;
    int score;
    UtilityAiSupport support = { 85, true, true, true };
    candidates[0] = (UtilityAiCandidate){ .action = UTILITY_AI_OFFENSIVE, .spell = 1,
        .target_health_percent = 100, .base_score = 100, .initial_cost = 5, .distance = 4 };
    candidates[1] = (UtilityAiCandidate){ .action = UTILITY_AI_HEAL, .spell = 2,
        .target_health_percent = 100, .initial_cost = 5 };
    CHECK(utility_ai_choose(&context, candidates, 2, NULL, NULL, &score) == 0);
    CHECK(score > UTILITY_AI_ATTACK_SCORE);
    candidates[1].target_health_percent = 20;
    CHECK(utility_ai_choose(&context, candidates, 2, NULL, NULL, &score) == 1);
    rejected_spell = 2;
    validations = 0;
    CHECK(utility_ai_choose(&context, candidates, 2, validate, NULL, &score) == 0);
    CHECK(validations == 2);
    context.fatigue = 18;
    candidates[0].initial_cost = 12;
    CHECK(utility_ai_score(&context, &candidates[0]) == 0);
    context.fatigue = 25;
    candidates[0].initial_cost = 5;
    candidates[0].maintenance_cost = 20;
    CHECK(utility_ai_score(&context, &candidates[0]) == 0);
    context.fatigue = 12;
    candidates[1].initial_cost = 10;
    CHECK(utility_ai_score(&context, &candidates[1]) > UTILITY_AI_ATTACK_SCORE);
    candidates[1].initial_cost = 11;
    CHECK(utility_ai_score(&context, &candidates[1]) == 0);
    candidates[2] = (UtilityAiCandidate){ .action = UTILITY_AI_RECOVER_FATIGUE, .spell = -1,
        .item = 77, .target_fatigue_percent = 16 };
    context.fatigue = 8;
    CHECK(utility_ai_choose(&context, candidates, 3, NULL, NULL, &score) == 2);
    candidates[2].target_fatigue_percent = 100;
    CHECK(utility_ai_choose(&context, candidates, 3, NULL, NULL, &score) == -1);
    CHECK(score == UTILITY_AI_ATTACK_SCORE);
    context.fatigue = 50;
    candidates[0].maintenance_cost = 0;
    for (index = 0; index < UTILITY_AI_CANDIDATE_LIMIT + 1; index++) {
        candidates[index] = candidates[0];
        candidates[index].spell = index + 1;
    }
    memcpy(snapshot, candidates, sizeof(snapshot));
    validations = 0;
    CHECK(utility_ai_choose(&context, candidates, UTILITY_AI_CANDIDATE_LIMIT + 1, reject_all, NULL, &score) == -1);
    CHECK(validations == UTILITY_AI_VALIDATION_LIMIT);
    for (index = 0; index < 100; index++) CHECK(utility_ai_choose(&context, candidates, 4, NULL, NULL, &score) == 0);
    CHECK(memcmp(snapshot, candidates, sizeof(snapshot)) == 0);
    CHECK(utility_ai_choose(&context, candidates, 0, NULL, NULL, &score) == -1);
    candidates[0].distance = 1000;
    CHECK(utility_ai_score(&context, &candidates[0]) == 0);
    context.max_fatigue = 0;
    CHECK(utility_ai_score(&context, &candidates[1]) == 0);
    context = (UtilityAiContext){ .health_percent = 100, .fatigue = 25, .max_fatigue = 50, .threat_distance = 4 };
    candidates[0] = (UtilityAiCandidate){ .action = UTILITY_AI_OFFENSIVE, .spell = 1,
        .target_health_percent = 100, .base_score = 100, .initial_cost = 5 };
    CHECK(utility_ai_score(&context, &candidates[0]) > UTILITY_AI_ATTACK_SCORE);
    context.committed_fatigue = 12;
    CHECK(utility_ai_score(&context, &candidates[0]) == 0);
    candidates[1] = (UtilityAiCandidate){ .action = UTILITY_AI_RECOVER_FATIGUE, .spell = -1,
        .target_fatigue_percent = 20, .consumable = true };
    CHECK(utility_ai_choose(&context, candidates, 2, NULL, NULL, &score) == 1);
    context.committed_fatigue = 0;
    candidates[1] = (UtilityAiCandidate){ .action = UTILITY_AI_CURE_POISON, .spell = 2,
        .target_health_percent = 90, .target_poison_level = 300 };
    CHECK(utility_ai_choose(&context, candidates, 2, NULL, NULL, &score) == 1);
    candidates[1].target_poison_level = 0;
    CHECK(utility_ai_choose(&context, candidates, 2, NULL, NULL, &score) == 0);
    candidates[1] = (UtilityAiCandidate){ .action = UTILITY_AI_RESURRECT, .spell = 3, .target_dead = true };
    CHECK(utility_ai_choose(&context, candidates, 2, NULL, NULL, &score) == 1);
    candidates[1].target_dead = false;
    CHECK(utility_ai_score(&context, &candidates[1]) == 0);
    candidates[1].action = UTILITY_AI_HEAL;
    candidates[1].target_dead = true;
    CHECK(utility_ai_score(&context, &candidates[1]) == 0);
    CHECK(utility_ai_ally_priority(100, 100, 0, false, 0, &support) == 0);
    CHECK(utility_ai_ally_priority(10, 100, 0, false, 4, &support) > utility_ai_ally_priority(60, 100, 0, false, 1, &support));
    CHECK(utility_ai_ally_priority(100, 10, 0, false, 2, &support) > 0);
    CHECK(utility_ai_ally_priority(100, 100, 300, false, 2, &support) > 0);
    CHECK(utility_ai_ally_priority(0, 0, 0, true, 3, &support) > 0);
    CHECK(utility_ai_ally_priority(10, 10, 300, false, 13, &support) == 0);
    CHECK(utility_ai_maintenance_cost(2, 8, 8, 80) == 20);
    CHECK(utility_ai_maintenance_cost(2, 24, 24, 80) == 6);
    CHECK(utility_ai_maintenance_cost(2, 80, 80, 80) == 2);
    CHECK(utility_ai_maintenance_cost(2, 80, 81, 80) == 0);
    CHECK(utility_ai_maintenance_cost(2, 80, 0, 80) == 4);
    CHECK(utility_ai_maintenance_cost(2, 0, 0, 80) == 0);
    CHECK(utility_ai_maintenance_cost(2147483647, 1, 0, 2147483647) == 1000000);
    context = (UtilityAiContext){ .health_percent = 100, .fatigue = 50, .max_fatigue = 50, .threat_distance = 1 };
    candidates[0] = (UtilityAiCandidate){ .action = UTILITY_AI_RETREAT, .spell = -3 };
    CHECK(utility_ai_score(&context, &candidates[0]) == 0);
    context.fatigue = 10;
    CHECK(utility_ai_score(&context, &candidates[0]) > UTILITY_AI_ATTACK_SCORE);
    context.fatigue = 11;
    CHECK(utility_ai_score(&context, &candidates[0]) == 0);
    context.health_percent = 25;
    CHECK(utility_ai_score(&context, &candidates[0]) > UTILITY_AI_ATTACK_SCORE);
    context.threat_distance = 3;
    CHECK(utility_ai_score(&context, &candidates[0]) == 0);
    context.threat_distance = 1;
    candidates[1] = (UtilityAiCandidate){ .action = UTILITY_AI_HEAL, .target_health_percent = 25 };
    CHECK(utility_ai_choose(&context, candidates, 2, NULL, NULL, &score) == 1);
    candidates[1] = (UtilityAiCandidate){ .action = UTILITY_AI_RECOVER_FATIGUE, .target_fatigue_percent = 20 };
    CHECK(utility_ai_choose(&context, candidates, 2, NULL, NULL, &score) == 1);
    {
        int current[] = { 1, 3 };
        int destination[] = { 2, 3 };
        CHECK(utility_ai_retreat_position_score(current, destination, 2, 4) > 0);
        CHECK(utility_ai_retreat_position_score(current, destination, 2, 9) == 0);
        CHECK(utility_ai_retreat_position_score(current, destination, 0, 0) == 0);
        CHECK(utility_ai_retreat_position_score(current, destination, UTILITY_AI_RETREAT_THREAT_LIMIT + 1, 0) == 0);
        destination[1] = 2;
        CHECK(utility_ai_retreat_position_score(current, destination, 2, 0) == 0);
        destination[0] = 1;
        destination[1] = 3;
        CHECK(utility_ai_retreat_position_score(current, destination, 2, 0) == 0);
        current[0] = -1;
        CHECK(utility_ai_retreat_position_score(current, destination, 2, 0) == 0);
    }
    CHECK(utility_ai_fatigue_payment(8, false) == 8);
    CHECK(utility_ai_fatigue_payment(8, true) == 4);
    CHECK(utility_ai_fatigue_payment(7, true) == 3);
    CHECK(utility_ai_fatigue_payment(1, true) == 1);
    CHECK(utility_ai_fatigue_payment(0, false) == 0);
    CHECK(utility_ai_fatigue_payment(0, true) == 1);
    CHECK(utility_ai_fatigue_payment(-1, true) == 0);
    context = (UtilityAiContext){ .health_percent = 100, .fatigue = 20, .max_fatigue = 100,
        .threat_distance = 2, .committed_fatigue = 10 };
    candidates[0] = (UtilityAiCandidate){ .action = UTILITY_AI_RELEASE_MAINTENANCE, .released_maintenance_cost = 10 };
    CHECK(utility_ai_score(&context, &candidates[0]) > UTILITY_AI_ATTACK_SCORE);
    candidates[0].released_maintenance_cost = 0;
    CHECK(utility_ai_score(&context, &candidates[0]) == 0);
    candidates[0].released_maintenance_cost = 10;
    context.fatigue = 21;
    CHECK(utility_ai_score(&context, &candidates[0]) == 0);
    context.fatigue = 20;
    context.committed_fatigue = 0;
    CHECK(utility_ai_score(&context, &candidates[0]) == 0);
    context.committed_fatigue = 10;
    candidates[1] = (UtilityAiCandidate){ .action = UTILITY_AI_RECOVER_FATIGUE, .target_fatigue_percent = 20 };
    CHECK(utility_ai_choose(&context, candidates, 2, NULL, NULL, &score) == 1);
    candidates[1] = (UtilityAiCandidate){ .action = UTILITY_AI_HEAL, .target_health_percent = 20 };
    CHECK(utility_ai_choose(&context, candidates, 2, NULL, NULL, &score) == 1);
    candidates[0] = (UtilityAiCandidate){ .action = UTILITY_AI_OFFENSIVE, .maintenance_cost = 1 };
    context.committed_fatigue = 0;
    context.fatigue = 35;
    CHECK(utility_ai_score(&context, &candidates[0]) == 0);
    context.fatigue = 36;
    CHECK(utility_ai_score(&context, &candidates[0]) > UTILITY_AI_ATTACK_SCORE);
    context.fatigue = 35;
    candidates[0].maintenance_cost = 0;
    CHECK(utility_ai_score(&context, &candidates[0]) > UTILITY_AI_ATTACK_SCORE);
    support = (UtilityAiSupport){ 85, false, false, false };
    CHECK(utility_ai_ally_priority(0, 0, 0, true, 1, &support) == 0);
    CHECK(utility_ai_ally_priority(100, 0, 300, false, 1, &support) == 0);
    CHECK(utility_ai_ally_priority(70, 0, 300, false, 1, &support) > 0);
    support.healing_health_limit = 41;
    CHECK(utility_ai_ally_priority(41, 100, 0, false, 0, &support) == 0);
    CHECK(utility_ai_ally_priority(40, 100, 0, false, 0, &support) > 0);
    support.healing_health_limit = 66;
    CHECK(utility_ai_ally_priority(66, 100, 0, false, 0, &support) == 0);
    CHECK(utility_ai_ally_priority(65, 100, 0, false, 0, &support) > 0);
    support = (UtilityAiSupport){ 0, false, true, false };
    CHECK(utility_ai_ally_priority(10, 10, 0, false, 0, &support) == 0);
    CHECK(utility_ai_ally_priority(10, 10, 1, false, 0, &support) > 0);
    context = (UtilityAiContext){ 100, 50, 50, 4 };
    for (index = 0; index < 10; index++) {
        candidates[index] = (UtilityAiCandidate){ .action = index % 2 ? UTILITY_AI_HEAL : UTILITY_AI_RECOVER_FATIGUE,
            .spell = 2, .item = 77, .target = 44, .target_health_percent = 10, .target_fatigue_percent = 0 };
    }
    candidates[10] = (UtilityAiCandidate){ .action = UTILITY_AI_OFFENSIVE, .spell = 1, .target = 88 };
    rejected_spell = 2;
    validations = 0;
    CHECK(utility_ai_choose(&context, candidates, 11, validate, NULL, &score) == 10);
    CHECK(validations == 2);
    {
        int64_t rejected_target = 44;
        candidates[1] = candidates[0];
        candidates[1].target = 45;
        validations = 0;
        CHECK(utility_ai_choose(&context, candidates, 2, validate_target, &rejected_target, &score) == 1);
        CHECK(validations == 2);
    }
    candidates[1] = candidates[0];
    candidates[1].item = 78;
    validations = 0;
    CHECK(utility_ai_choose(&context, candidates, 2, reject_all, NULL, &score) == -1);
    CHECK(validations == 2);
    context.health_percent = 20;
    context.threat_distance = 1;
    candidates[0] = (UtilityAiCandidate){ .action = UTILITY_AI_RETREAT, .spell = -3, .destination = 100 };
    candidates[1] = candidates[0];
    candidates[1].destination = 101;
    validations = 0;
    CHECK(utility_ai_choose(&context, candidates, 2, reject_all, NULL, &score) == -1);
    CHECK(validations == 2);
    context.fatigue = 10;
    context.committed_fatigue = 10;
    candidates[0] = (UtilityAiCandidate){ .action = UTILITY_AI_RELEASE_MAINTENANCE, .spell = 2,
        .maintenance_id = 1, .released_maintenance_cost = 10 };
    candidates[1] = candidates[0];
    candidates[1].maintenance_id = 2;
    validations = 0;
    CHECK(utility_ai_choose(&context, candidates, 2, reject_all, NULL, &score) == -1);
    CHECK(validations == 2);
    return failures != 0;
}
