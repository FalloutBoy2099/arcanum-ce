#include "game/combat_auto_exit.h"

#include "game/ai.h"
#include "game/anim.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/gamelib.h"
#include "game/location.h"
#include "game/obj.h"
#include "game/obj_private.h"
#include "game/object.h"
#include "game/object_node.h"
#include "game/player.h"

#define COMBAT_AUTO_EXIT_SCAN_INTERVAL 250
#define COMBAT_AUTO_EXIT_SAFE_DELAY 1500
#define COMBAT_AUTO_EXIT_THREAT_RADIUS 24

static bool combat_auto_exit_has_threat(int64_t member, int64_t pc)
{
    ObjectList nearby;
    int64_t loc = obj_field_int64_get(member, OBJ_F_LOCATION);
    LocRect bounds = {
        LOCATION_GET_X(loc) - COMBAT_AUTO_EXIT_THREAT_RADIUS,
        LOCATION_GET_Y(loc) - COMBAT_AUTO_EXIT_THREAT_RADIUS,
        LOCATION_GET_X(loc) + COMBAT_AUTO_EXIT_THREAT_RADIUS,
        LOCATION_GET_Y(loc) + COMBAT_AUTO_EXIT_THREAT_RADIUS,
    };
    object_list_rect(&bounds, OBJ_TM_NPC, &nearby);
    bool threat = false;
    for (ObjectNode* node = nearby.head; node != NULL; node = node->next) {
        int64_t npc = node->obj;
        if (!obj_handle_is_valid(npc) || !critter_is_active(npc)
            || critter_party_same(pc, npc)
            || (obj_field_int32_get(npc, OBJ_F_FLAGS) & (OF_OFF | OF_DESTROYED)) != 0) continue;
        unsigned int flags = obj_field_int32_get(npc, OBJ_F_CRITTER_FLAGS);
        if ((flags & OCF_SURRENDERED) != 0) continue;
        unsigned int npc_flags = obj_field_int32_get(npc, OBJ_F_NPC_FLAGS);
        int64_t target = obj_field_handle_get(npc, OBJ_F_NPC_COMBAT_FOCUS);
        if (ai_is_fighting(npc) || combat_critter_is_combat_mode_active(npc)
            || (target != OBJ_HANDLE_NULL && obj_handle_is_valid(target) && critter_party_same(pc, target))
            || ((flags & OCF_FLEEING) != 0)
            || ((npc_flags & ONF_KOS) != 0 && (npc_flags & ONF_KOS_OVERRIDE) == 0
                && !critter_faction_same(npc, member))) {
            threat = true;
            break;
        }
    }
    object_list_destroy(&nearby);
    return threat;
}

void combat_auto_exit_tick(tig_timestamp_t timestamp, bool allowed)
{
    static int64_t tracked_pc;
    static tig_timestamp_t last_scan;
    static tig_timestamp_t safe_since;
    static bool tracking;
    static bool safe;

    int64_t pc = player_get_local_pc_obj();
    if (!allowed || settings_get_value(&settings, AUTO_EXIT_COMBAT_KEY) == 0
        || pc == OBJ_HANDLE_NULL || !obj_handle_is_valid(pc) || !critter_is_active(pc)
        || !combat_critter_is_combat_mode_active(pc)) {
        tracking = false;
        safe = false;
        return;
    }
    if (!tracking || tracked_pc != pc) {
        tracking = true;
        tracked_pc = pc;
        last_scan = timestamp;
        safe = false;
        return;
    }
    if ((unsigned int)(timestamp - last_scan) < COMBAT_AUTO_EXIT_SCAN_INTERVAL) return;
    last_scan = timestamp;
    if (combat_turn_based_is_active()
        && (combat_turn_based_whos_turn_get() != pc || combat_turn_based_has_active_opponents(pc))) {
        safe = false;
        return;
    }

    ObjectList party;
    object_list_team(pc, &party);
    bool threatened = false;
    for (ObjectNode* node = party.head; node != NULL; node = node->next) {
        int64_t member = node->obj;
        if (!obj_handle_is_valid(member) || critter_is_dead(member)) continue;
        if (anim_foreground_goal_priority_get(member) >= PRIORITY_3
            || !combat_can_exit_combat_mode(member)
            || combat_auto_exit_has_threat(member, pc)) {
            threatened = true;
            break;
        }
    }
    object_list_destroy(&party);
    if (threatened) {
        safe = false;
        return;
    }
    if (!safe) {
        safe = true;
        safe_since = timestamp;
    } else if ((unsigned int)(timestamp - safe_since) >= COMBAT_AUTO_EXIT_SAFE_DELAY) {
        combat_critter_deactivate_combat_mode(pc);
        tracking = false;
        safe = false;
    }
}
