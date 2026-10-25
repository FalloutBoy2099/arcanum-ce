#ifndef ARCANUM_GAME_AI_PRIVATE_H_
#define ARCANUM_GAME_AI_PRIVATE_H_

#include "game/ai.h"

#define AI_PARAMS_MAX 17

typedef union AiParams {
    struct {
        /* 0000 */ int flee_hit_points_percent; // Percentage of NPC hit points below which NPC will flee.
        /* 0004 */ int flee_party_size_threshold; // Number of people besides PC beyond which NPC will flee.
        /* 0008 */ int flee_level_difference; // Number of levels above NPC beyond which NPC will flee.
        /* 000C */ int leader_hit_points_prevent_flee_percent; // Percentage of PC hit points below which NPC will never flee.
        /* 0010 */ int flee_distance_tiles; // How far to flee, in tiles.
        /* 0014 */ int minimum_follow_reaction; // The reaction level at which the NPC will not follow the PC.
        /* 0018 */ int maximum_good_alignment_difference; // How far PC align is above NPC align before NPC wont follow.
        /* 001C */ int maximum_evil_alignment_difference; // How far PC align is below NPC align before NPC wont follow.
        /* 0020 */ int maximum_follower_level_difference; // How many levels the NPC can be above the PC and still join.
        /* 0024 */ int friendly_hit_reaction_penalty; // How much a non-accidental hit will lower a follower's reaction.
        /* 0028 */ int attack_reaction_threshold; // NPC will attack if his reaction is below this.
        /* 002C */ int attack_alignment_difference; // How different alignments can be before non-follower NPC attacks.
        /* 0030 */ int protected_good_alignment_threshold; // Alignment of target at (or above) which the good-aligned follower NPC will not attack.
        /* 0034 */ int defensive_spell_chance; // Chance of throwing defensive spell (as opposed to offensive).
        /* 0038 */ int healing_spell_chance; // Chance of throwing a healing spell in combat.
        /* 003C */ int minimum_combat_distance; // Minimum distance in combat.
        /* 0040 */ int can_open_portals; // The NPC can open portals if this is nonzero and cannot if it is zero.
    };
    int values[AI_PARAMS_MAX];
} AiParams;

typedef struct Ai {
    /* 0000 */ int64_t obj;
    /* 0008 */ int64_t danger_source;
    /* 0010 */ int danger_type;
    /* 0014 */ int action_type;
    /* 0018 */ int spell;
    /* 001C */ int skill;
    /* 0020 */ int64_t item_obj;
    /* 0028 */ int64_t leader_obj;
    /* 0030 */ int sound_id;
    int64_t move_destination;
    int maintenance_id;
    bool utility_selected;
} Ai;

void ai_copy_params(int64_t obj, AiParams* params);
void ai_danger_source(int64_t obj, int* type_ptr, int64_t* danger_source_ptr);
int ai_magictech_action_check(int64_t attacker_obj, int64_t item_obj, int64_t target_obj, int spell);

#endif
