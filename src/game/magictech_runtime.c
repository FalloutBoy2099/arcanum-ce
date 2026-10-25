#include "game/magictech_runtime.h"

#include <limits.h>

#include "game/spell.h"
#include "game/stat.h"

static bool magictech_run_metadata_valid(const MagicTechRunInfo* run)
{
    return run != NULL && run->id >= 0 && run->id < MAGICTECH_RUN_CAPACITY
        && run->spell >= 0 && run->spell < MT_SPELL_COUNT
        && run->action >= MAGICTECH_ACTION_BEGIN && run->action < MAGICTECH_ACTION_COUNT;
}

static void magictech_loaded_nodes_release(MagicTechRunInfo* run)
{
    while (run->objlist != NULL) {
        MagicTechObjectNode* node = run->objlist;
        run->objlist = node->next;
        mt_obj_node_destroy(node);
    }
    while (run->summoned_obj != NULL) {
        MagicTechObjectNode* node = run->summoned_obj;
        run->summoned_obj = node->next;
        mt_obj_node_destroy(node);
    }
}

bool magictech_run_info_load(MagicTechRunInfo* run_info, TigFile* stream)
{
    MagicTechRunInfo loaded = { 0 };
    if (run_info == NULL || stream == NULL) return false;
    if (tig_file_fread(&loaded.id, sizeof(loaded.id), 1, stream) != 1
        || tig_file_fread(&loaded.spell, sizeof(loaded.spell), 1, stream) != 1
        || tig_file_fread(&loaded.action, sizeof(loaded.action), 1, stream) != 1
        || !magictech_run_metadata_valid(&loaded)
        || !mt_obj_node_load_detached(&loaded.source_obj, stream)
        || !mt_obj_node_load_detached(&loaded.parent_obj, stream)
        || !mt_obj_node_load_detached(&loaded.target_obj, stream)
        || !mt_obj_node_load_detached(&loaded.attacker_obj, stream)
        || !mt_obj_node_load_list(&loaded.objlist, stream)
        || !mt_obj_node_load_list(&loaded.summoned_obj, stream)
        || tig_file_fread(&loaded.applied_spell_flags, sizeof(loaded.applied_spell_flags), 1, stream) != 1
        || tig_file_fread(&loaded.flags, sizeof(loaded.flags), 1, stream) != 1
        || tig_file_fread(&loaded.trigger, sizeof(loaded.trigger), 1, stream) != 1
        || tig_file_fread(&loaded.resistance_amount, sizeof(loaded.resistance_amount), 1, stream) != 1
        || tig_file_fread(&loaded.event_time, sizeof(loaded.event_time), 1, stream) != 1
        || tig_file_fread(&loaded.remaining_duration_triggers, sizeof(loaded.remaining_duration_triggers), 1, stream) != 1
        || (loaded.flags & MAGICTECH_RUN_ACTIVE) == 0) {
        magictech_loaded_nodes_release(&loaded);
        return false;
    }
    *run_info = loaded;
    return true;
}

bool magictech_runs_load(TigFile* stream, MagicTechRunInfo* destination, int* allocated_count)
{
    int saved_count;
    int count;
    int index = 0;
    int active_count = 0;
    bool valid = true;
    MagicTechRunInfo* loaded;
    if (stream == NULL || destination == NULL || allocated_count == NULL
        || tig_file_fread(&saved_count, sizeof(saved_count), 1, stream) != 1
        || tig_file_fread(&count, sizeof(count), 1, stream) != 1
        || count < 0 || count > MAGICTECH_RUN_CAPACITY) return false;
    loaded = CALLOC(MAGICTECH_RUN_CAPACITY, sizeof(*loaded));
    if (loaded == NULL) return false;
    for (int slot = 0; slot < MAGICTECH_RUN_CAPACITY; slot++) loaded[slot].id = -1;
    while (index < count) {
        int extent;
        int remaining = count - index;
        if (tig_file_fread(&extent, sizeof(extent), 1, stream) != 1
            || extent == 0 || extent > remaining || extent < -remaining) {
            valid = false;
            break;
        }
        if (extent < 0) {
            index -= extent;
            continue;
        }
        for (int offset = 0; offset < extent; offset++, index++) {
            if (!magictech_run_info_load(&loaded[index], stream) || loaded[index].id != index) {
                valid = false;
                break;
            }
            active_count++;
        }
        if (!valid) break;
    }
    if (valid) {
        for (int slot = 0; slot < MAGICTECH_RUN_CAPACITY; slot++) magictech_loaded_nodes_release(&destination[slot]);
        memcpy(destination, loaded, MAGICTECH_RUN_CAPACITY * sizeof(*loaded));
        *allocated_count = active_count;
        if (saved_count != active_count) tig_debug_printf("MagicTech: repaired run count %d to %d.\n", saved_count, active_count);
    } else {
        for (int slot = 0; slot < MAGICTECH_RUN_CAPACITY; slot++) magictech_loaded_nodes_release(&loaded[slot]);
        tig_debug_printf("MagicTech: invalid or incomplete run table.\n");
    }
    FREE(loaded);
    return valid;
}

bool magictech_map_runs_load(TigFile* stream, MagicTechRunInfo* destination, int* allocated_count, int* loaded_ids, int* loaded_count)
{
    int count;
    int free_count = 0;
    bool occupied[MAGICTECH_RUN_CAPACITY];
    MagicTechRunInfo* loaded;
    if (loaded_count != NULL) *loaded_count = 0;
    if (stream == NULL || destination == NULL || allocated_count == NULL || loaded_ids == NULL || loaded_count == NULL
        || tig_file_fread(&count, sizeof(count), 1, stream) != 1
        || count < 0 || count > MAGICTECH_RUN_CAPACITY) return false;
    for (int slot = 0; slot < MAGICTECH_RUN_CAPACITY; slot++) {
        occupied[slot] = destination[slot].id != -1 || (destination[slot].flags & MAGICTECH_RUN_ACTIVE) != 0;
        if (!occupied[slot]) free_count++;
    }
    if (count > free_count) return false;
    loaded = CALLOC(MAGICTECH_RUN_CAPACITY, sizeof(*loaded));
    if (loaded == NULL) return false;
    for (int index = 0; index < count; index++) {
        if (!magictech_run_info_load(&loaded[index], stream)) {
            for (int slot = 0; slot <= index; slot++) magictech_loaded_nodes_release(&loaded[slot]);
            FREE(loaded);
            return false;
        }
    }
    for (int index = 0; index < count; index++) {
        int id = loaded[index].id;
        if (occupied[id]) {
            for (id = 0; id < MAGICTECH_RUN_CAPACITY && occupied[id]; id++) {}
        }
        occupied[id] = true;
        loaded[index].id = id;
        destination[id] = loaded[index];
        loaded_ids[index] = id;
    }
    *loaded_count = count;
    *allocated_count = MAGICTECH_RUN_CAPACITY - free_count + count;
    FREE(loaded);
    return true;
}

bool magictech_id_to_run_info(int mt_id, MagicTechRunInfo** run_info_ptr)
{
    if (run_info_ptr == NULL) return false;
    *run_info_ptr = NULL;
    if (magictech_run_info == NULL || mt_id < 0 || mt_id >= MAGICTECH_RUN_CAPACITY) return false;
    MagicTechRunInfo* run = &magictech_run_info[mt_id];
    if (!magictech_run_metadata_valid(run) || run->id != mt_id
        || (run->flags & MAGICTECH_RUN_ACTIVE) == 0
        || !magictech_run_object_handles_recover(run)) return false;
    *run_info_ptr = run;
    return true;
}

int magictech_fatigue_cost(int64_t obj, int spell, MagicTechAction action, bool cheat_mode)
{
    int64_t cost;
    if (magictech_spells == NULL || spell < 0 || spell >= MT_SPELL_COUNT
        || action < MAGICTECH_ACTION_BEGIN || action >= MAGICTECH_ACTION_COUNT) return -1;
    cost = action == MAGICTECH_ACTION_BEGIN ? magictech_spells[spell].cost : magictech_spells[spell].maintenance.cost;
    if (cost < 0) return -1;
    if (action == MAGICTECH_ACTION_BEGIN && spell < MAGICTECH_PLAYER_SPELL_COUNT
        && spell_mastery_get(obj) == COLLEGE_FROM_SPELL(spell)) {
        cost /= 2;
        if ((cost & 1) != 0) cost++;
    }
    if (cheat_mode && action == MAGICTECH_ACTION_BEGIN) cost = 1;
    int type = obj_field_int32_get(obj, OBJ_F_TYPE);
    if ((type == OBJ_TYPE_PC || type == OBJ_TYPE_NPC) && stat_level_get(obj, STAT_RACE) == RACE_DWARF) cost *= 2;
    return cost > INT_MAX ? -1 : (int)cost;
}

bool magictech_run_cost_check_for_mode(int mt_id, bool cheat_mode)
{
    MagicTechRunInfo* run;
    if (!magictech_id_to_run_info(mt_id, &run)) return false;
    if ((run->flags & MAGICTECH_RUN_FREE) != 0 || run->source_obj.obj == OBJ_HANDLE_NULL) return true;
    int cost = magictech_fatigue_cost(run->source_obj.obj, run->spell, run->action, cheat_mode);
    return cost >= 0 && magictech_cost_check_or_consume(run->source_obj.obj, cost, false, run->spell);
}
