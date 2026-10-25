#include "game/reaction.h"
#include "game/journal_memory.h"
#include "game/player.h"

#include "game/ai.h"
#include "game/critter.h"
#include "game/effect.h"
#include "game/fate.h"
#include "game/magictech.h"
#include "game/mes.h"
#include "game/obj.h"
#include "game/reputation.h"
#include "game/stat.h"
#include "game/ui.h"

static int reaction_get_with_flags(int64_t npc_obj, int64_t pc_obj, unsigned int flags);
static int reaction_stored_or_base_get(int64_t npc_obj, int64_t pc_obj);
static int reaction_get_base(int64_t obj);
static bool reaction_stored_lookup(int64_t npc_obj, int64_t pc_obj, bool include_current_slot, int* reaction_level_ptr);
static void reaction_stored_remember(int64_t npc_obj, int64_t pc_obj, int value);
static void reaction_stored_slot_set(int64_t npc_obj, int64_t pc_obj, int level, int index);
static int reaction_modifiers_get(int64_t npc_obj, int64_t pc_obj, unsigned int flags);
static int reaction_distance_from_neutral(int reaction_level);

// 0x5FC88C
static mes_file_handle_t reaction_mes_file;

// 0x5FC890
static char* reaction_names[REACTION_COUNT];

// 0x5B684C
static int reaction_racial_modifiers[RACE_COUNT][8] = {
    {
        0,
        0,
        5,
        5,
        0,
        10,
        -5,
        -5,
    },
    {
        -5,
        10,
        -10,
        -5,
        0,
        5,
        -10,
        -15,
    },
    {
        -10,
        -20,
        0,
        0,
        -10,
        10,
        -15,
        -10,
    },
    {
        0,
        0,
        0,
        0,
        0,
        5,
        -5,
        -5,
    },
    {
        0,
        0,
        0,
        0,
        5,
        5,
        -10,
        5,
    },
    {
        0,
        0,
        20,
        15,
        0,
        10,
        -10,
        -10,
    },
    {
        0,
        -20,
        -15,
        -10,
        -10,
        0,
        10,
        0,
    },
    {
        0,
        -20,
        -15,
        -5,
        20,
        10,
        0,
        5,
    },
    {
        -30,
        -30,
        5,
        -20,
        -30,
        -5,
        -30,
        -30,
    },
    {
        -10,
        -30,
        -20,
        -15,
        -20,
        -20,
        0,
        -10,
    },
    {
        -20,
        -30,
        -40,
        -30,
        -20,
        -20,
        10,
        0,
    },
};

// 0x4C0BD0
bool reaction_init(GameInitInfo* init_info)
{
    MesFileEntry mes_file_entry;
    int index;

    (void)init_info;

    if (!mes_load("mes\\reaction.mes", &reaction_mes_file)) {
        return false;
    }

    for (index = 0; index < REACTION_COUNT; index++) {
        mes_file_entry.num = index;
        mes_get_msg(reaction_mes_file, &mes_file_entry);
        reaction_names[index] = mes_file_entry.str;
    }

    return true;
}

// 0x4C0C30
void reaction_exit(void)
{
    mes_unload(reaction_mes_file);
}

// 0x4C0C40
bool reaction_met_before(int64_t npc_obj, int64_t pc_obj)
{
    int stored_reaction;

    if (npc_obj == pc_obj) {
        return true;
    }

    // Make sure `pc_obj` is actually PC.
    if (obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return false;
    }

    // Make sure `npc_obj` is actually NPC.
    if (obj_field_int32_get(npc_obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
        return false;
    }

    return reaction_stored_lookup(npc_obj, pc_obj, false, &stored_reaction);
}

// 0x4C0CC0
int reaction_get(int64_t npc_obj, int64_t pc_obj)
{
    return reaction_get_with_flags(npc_obj, pc_obj, 0);
}

// 0x4C0CE0
int reaction_get_with_limited_penalty(int64_t npc_obj, int64_t pc_obj)
{
    return reaction_get_with_flags(npc_obj, pc_obj, 1);
}

// Computes reaction including hostility, modifiers, effects and mind control.
// 0x4C0D00
int reaction_get_with_flags(int64_t npc_obj, int64_t pc_obj, unsigned int flags)
{
    int value;
    int64_t mind_controlled_by_obj;

    if (ai_shitlist_has(npc_obj, pc_obj)) {
        return 0;
    }

    // Make sure `pc_obj` is actually PC.
    if (obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return 50;
    }

    // Make sure `npc_obj` is actually NPC.
    if (obj_field_int32_get(npc_obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
        return 50;
    }

    value = reaction_modifiers_get(npc_obj, pc_obj, flags) + reaction_stored_or_base_get(npc_obj, pc_obj);
    value = effect_adjust_reaction(npc_obj, value);

    if (value < 50
        && magictech_spell_flag_caster_find(npc_obj, OSF_MIND_CONTROLLED, &mind_controlled_by_obj)
        && mind_controlled_by_obj == pc_obj) {
        value = 50;
    }

    return value;
}

// 0x4C0DE0
void reaction_adj(int64_t npc_obj, int64_t pc_obj, int value)
{
    reaction_adj_ex(npc_obj, pc_obj, value, true);
}

// CE: There is a bug when temporary reaction adjustments must be reversible,
// notably with the Charm spell. The bug stems from applying separate modifiers
// for negative and positive adjustments that are not symmetric. For example, a
// PC gnome has `badreactionadj +10`. When Charm is applied it grants +30 bonus
// to reaction. When it wears off the net reduction becomes -20 (-30 from Charm
// and +10 from being a gnome), thus granting permanent increase in reaction.
// The fix is to add a parameter that controls whether a reaction adjustment
// should be affected by effects or not.
void reaction_adj_ex(int64_t npc_obj, int64_t pc_obj, int value, bool apply_effects)
{
    int64_t mind_controlled_by_obj;
    int base;
    int adjusted_value;
    unsigned int flags;

    if (value == 0) {
        return;
    }

    // Make sure `pc_obj` is actually PC.
    if (obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return;
    }

    // Make sure `npc_obj` is actually NPC.
    if (obj_field_int32_get(npc_obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
        return;
    }

    if (magictech_spell_flag_caster_find(npc_obj, OSF_MIND_CONTROLLED, &mind_controlled_by_obj)
        && mind_controlled_by_obj == pc_obj) {
        return;
    }

    if (value < 0
        && stat_is_extraordinary(pc_obj, STAT_CHARISMA)
        && critter_pc_leader_get(npc_obj) == pc_obj) {
        return;
    }

    base = reaction_stored_or_base_get(npc_obj, pc_obj);
    if (apply_effects) {
        adjusted_value = effect_adjust_good_bad_reaction(pc_obj, value);
    } else {
        adjusted_value = value;
    }
    reaction_stored_remember(npc_obj, pc_obj, base + adjusted_value);

    if (value < 0 && critter_leader_get(npc_obj) == pc_obj) {
        flags = obj_field_int32_get(npc_obj, OBJ_F_NPC_FLAGS);
        flags |= ONF_CHECK_LEADER;
        obj_field_int32_set(npc_obj, OBJ_F_NPC_FLAGS, flags);

        flags = obj_field_int32_get(npc_obj, OBJ_F_CRITTER_FLAGS2);
        flags |= OCF2_CHECK_REACTION_BAD;
        obj_field_int32_set(npc_obj, OBJ_F_CRITTER_FLAGS2, flags);
    }
}

// 0x4C0F50
void reaction_forget(int64_t npc_obj, int64_t pc_obj)
{
    int index;

    for (index = 0; index < 10; index++) {
        if (index != 0
            && obj_arrayfield_handle_get(npc_obj, OBJ_F_NPC_REACTION_PC_IDX, index) == pc_obj) {
            obj_arrayfield_obj_set(npc_obj, OBJ_F_NPC_REACTION_PC_IDX, index, OBJ_HANDLE_NULL);
            break;
        }
    }

    combat_recalc_reaction(npc_obj);
}

// 0x4C0FC0
int reaction_translate(int value)
{
    if (value <= 0) return REACTION_HATRED;
    if (value <= 20) return REACTION_DISLIKE;
    if (value <= 40) return REACTION_SUSPICIOUS;
    if (value <= 60) return REACTION_NEUTRAL;
    if (value <= 80) return REACTION_COURTEOUS;
    if (value <= 100) return REACTION_AMIABLE;
    return REACTION_LOVE;
}

// 0x4C1010
const char* reaction_get_name(int reaction)
{
    return reaction_names[reaction];
}

// Initializes reaction slot zero for a conversation, applying the good reaction fate override.
// 0x4C1020
void reaction_begin_conversation(int64_t npc_obj, int64_t pc_obj)
{
    int reaction_level;

    if (obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return;
    }

    if (obj_field_int32_get(npc_obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
        return;
    }

    reaction_level = reaction_stored_or_base_get(npc_obj, pc_obj);
    if (reaction_level < 100 && fate_resolve(pc_obj, FATE_FORCE_GOOD_REACTION)) {
        reaction_level = 100;
    }

    reaction_stored_slot_set(npc_obj, pc_obj, reaction_level, 0);
}

// Clears the conversation owner and remembers the reaction for that PC.
// 0x4C10A0
void reaction_end_conversation(int64_t npc_obj, int64_t pc_obj)
{
    int reaction_level;

    if (obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return;
    }

    if (obj_field_int32_get(npc_obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
        return;
    }

    reaction_level = reaction_stored_or_base_get(npc_obj, pc_obj);
    reaction_stored_slot_set(npc_obj, 0, reaction_level, 0);
    reaction_stored_remember(npc_obj, pc_obj, reaction_level);
}

// Reads the PC handle in reaction slot zero, or null for a non-NPC.
// 0x4C1110
int64_t reaction_conversation_pc_get(int64_t npc)
{
    if (obj_field_int32_get(npc, OBJ_F_TYPE) != OBJ_TYPE_NPC) {
        return 0;
    }

    return obj_arrayfield_handle_get(npc, OBJ_F_NPC_REACTION_PC_IDX, 0);
}

// TODO: Figure out the math.
//
// Scales a price according to the NPC reaction toward the PC.
// 0x4C1150
int reaction_adjust_price(int64_t npc_obj, int64_t pc_obj, int base_price)
{
    int reaction_level;
    int price_percent;

    reaction_level = reaction_get(npc_obj, pc_obj);
    if (reaction_level > 100) {
        reaction_level = 100;
    } else {
        if (reaction_level < 0) {
            price_percent = 200;
            return base_price * price_percent / 100;
        }

        if (reaction_level < 50) {
            price_percent = 2 * (100 - reaction_level);
            return base_price * price_percent / 100;
        }
    }

    price_percent = 120 - 2 * reaction_level / 5;
    return base_price * price_percent / 100;
}

// Adjusts reaction based on the signed surplus value of a trade.
// 0x4C11D0
void reaction_adjust_for_trade_surplus(int64_t npc_obj, int64_t pc_obj, int trade_surplus)
{
    int reaction_delta;

    if (trade_surplus < 0) {
        reaction_delta = trade_surplus / 100;
        if (reaction_delta == 0) {
            reaction_delta = -2;
        } else if (reaction_delta < -30) {
            reaction_delta = -30;
        }
    } else if (trade_surplus > 50
        && reaction_translate(reaction_get(npc_obj, pc_obj)) < REACTION_AMIABLE) {
        if (trade_surplus < 200) {
            reaction_delta = trade_surplus / 40;
        } else if (trade_surplus < 1000) {
            reaction_delta = (trade_surplus - 200) / 100 + 5;
        } else {
            reaction_delta = 15;
        }
    } else {
        reaction_delta = 0;
    }

    if (reaction_delta != 0) {
        reaction_adj(npc_obj, pc_obj, reaction_delta);
    }
}

// Returns stored reaction for the PC, falling back to the NPC base reaction.
// 0x4C1290
int reaction_stored_or_base_get(int64_t npc_obj, int64_t pc_obj)
{
    int stored_reaction;

    if (reaction_stored_lookup(npc_obj, pc_obj, 1, &stored_reaction)) {
        return stored_reaction;
    }

    return reaction_get_base(npc_obj);
}

// 0x4C12D0
int reaction_get_base(int64_t obj)
{
    return obj_field_int32_get(obj, OBJ_F_NPC_REACTION_BASE);
}

// Searches ten reaction slots, optionally including the conversation slot.
// 0x4C12F0
bool reaction_stored_lookup(int64_t npc_obj, int64_t pc_obj, bool include_current_slot, int* reaction_level_ptr)
{
    int index;

    for (index = 0; index < 10; index++) {
        if ((index != 0 || include_current_slot)
            && pc_obj == obj_arrayfield_handle_get(npc_obj, OBJ_F_NPC_REACTION_PC_IDX, index)) {
            *reaction_level_ptr = obj_arrayfield_uint32_get(npc_obj, OBJ_F_NPC_REACTION_LEVEL_IDX, index);
            return true;
        }
    }

    return false;
}

// Updates or chooses a reaction memory slot according to existing replacement rules.
// 0x4C1360
void reaction_stored_remember(int64_t npc_obj, int64_t pc_obj, int value)
{
    int index;
    int candidate;
    int64_t obj;
    int new_reaction_distance;
    int reaction_level;
    int stored_reaction_distance;
    int candidate_reaction_distance;

    // NOTE: Silence compiler warning.
    candidate_reaction_distance = -1;

    candidate = -1;
    for (index = 0; index < 10; index++) {
        obj = obj_arrayfield_handle_get(npc_obj, OBJ_F_NPC_REACTION_PC_IDX, index);
        if (obj == pc_obj) {
            reaction_stored_slot_set(npc_obj, pc_obj, value, index);
            return;
        }

        if (obj == OBJ_HANDLE_NULL
            && index != 0
            && candidate == -1) {
            candidate = index;
        }
    }

    if (candidate != -1) {
        reaction_stored_slot_set(npc_obj, pc_obj, value, candidate);
        return;
    }

    // FIXME: Unused.
    datetime_current_second();

    new_reaction_distance = reaction_distance_from_neutral(value);
    for (index = 0; index < 10; index++) {
        if (index != 0) {
            reaction_level = obj_arrayfield_uint32_get(npc_obj, OBJ_F_NPC_REACTION_LEVEL_IDX, index);

            // FIXME: Unused.
            obj_arrayfield_uint32_get(npc_obj, OBJ_F_NPC_REACTION_TIME_IDX, index);

            stored_reaction_distance = reaction_distance_from_neutral(reaction_level);
            if (stored_reaction_distance < new_reaction_distance && (candidate == -1 || stored_reaction_distance < candidate_reaction_distance)) {
                candidate = index;
                candidate_reaction_distance = stored_reaction_distance;
            }
        }
    }

    if (candidate != -1) {
        reaction_stored_slot_set(npc_obj, pc_obj, value, candidate);
        return;
    }
}

// Writes a reaction slot and time, then refreshes journal memory, UI and combat reaction.
// 0x4C1490
void reaction_stored_slot_set(int64_t npc_obj, int64_t pc_obj, int level, int index)
{
    obj_arrayfield_obj_set(npc_obj, OBJ_F_NPC_REACTION_PC_IDX, index, pc_obj);
    obj_arrayfield_uint32_set(npc_obj, OBJ_F_NPC_REACTION_LEVEL_IDX, index, level);
    obj_arrayfield_uint32_set(npc_obj, OBJ_F_NPC_REACTION_TIME_IDX, index, datetime_current_second());
    if (index != 0 && pc_obj != OBJ_HANDLE_NULL && pc_obj == player_get_local_pc_obj()) {
        journal_memory_remember(obj_field_int32_get(npc_obj, OBJ_F_DESCRIPTION));
    }
    ui_refresh_health_bar(npc_obj);
    combat_recalc_reaction(npc_obj);
}

// Calculates race, reaction-stat and reputation modifiers with aloof/monster rules.
// 0x4C1500
int reaction_modifiers_get(int64_t npc_obj, int64_t pc_obj, unsigned int flags)
{
    int total_modifier = 0;
    int modifier;
    int npc_race;
    int pc_race;

    if ((obj_field_int32_get(npc_obj, OBJ_F_NPC_FLAGS) & ONF_ALOOF) == 0
        && !critter_is_monstrous(npc_obj)) {
        modifier = stat_level_get(pc_obj, STAT_REACTION_MODIFIER);
        npc_race = stat_level_get(npc_obj, STAT_RACE);
        pc_race = stat_level_get(pc_obj, STAT_RACE);

        total_modifier = reaction_racial_modifiers[npc_race][pc_race] + modifier;
        if ((flags & 0x1) != 0) {
            if (total_modifier < -49) {
                total_modifier = -49;
            }
        }
    }

    total_modifier += reputation_reaction_adj(pc_obj, npc_obj);

    return total_modifier;
}

// Returns the absolute distance from neutral reaction fifty.
// 0x4C15A0
int reaction_distance_from_neutral(int reaction_level)
{
    // FIXME: Unused.
    datetime_current_second();

    if (reaction_level > 50) {
        return reaction_level - 50;
    } else {
        return 50 - reaction_level;
    }
}
