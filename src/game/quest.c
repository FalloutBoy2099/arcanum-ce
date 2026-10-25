#include "game/quest.h"

#include <errno.h>
#include <limits.h>

#include "game/critter.h"
#include "game/mes.h"
#include "game/object.h"
#include "game/party.h"
#include "game/player.h"
#include "game/reaction.h"
#include "game/snd.h"
#include "game/stat.h"
#include "game/ui.h"

#define MAX_QUEST 1000

#define QUEST_BOTCHED_MODIFIER 0x100

#define quest_num_to_idx(num) ((num) - 1000)

typedef struct Quest {
    int experience_level;
    int alignment_adjustment;
    int normal_dialog[QUEST_STATE_COUNT];
    int bad_reaction_dialog[QUEST_STATE_COUNT];
    int dumb_dialog[QUEST_STATE_COUNT];
    char* normal_description;
    char* dumb_description;
} Quest;

typedef enum QuestParseResult {
    QUEST_PARSE_MISSING,
    QUEST_PARSE_INVALID,
    QUEST_PARSE_OK,
} QuestParseResult;

static QuestParseResult quest_parse(const char* path, int start, int end);
static bool quest_access_valid(int num);
static bool quest_state_valid(int state);
static int quest_pc_state_normalize(int state);
static int quest_state_set_internal(int64_t pc_obj, int num, int state, int64_t npc_obj);
static int quest_logbook_entry_compare(const void* va, const void* vb);

/**
 * "gamequestlog.mes"
 *
 * 0x5B6E34
 */
static mes_file_handle_t quest_log_mes_file = MES_FILE_HANDLE_INVALID;

/**
 * "gamequestlogdumb.mes"
 *
 * 0x5B6E38
 */
static mes_file_handle_t quest_log_dumb_mes_file = MES_FILE_HANDLE_INVALID;

/**
 * "xp_quest.mes"
 *
 * 0x5FF414
 */
static mes_file_handle_t quest_xp_mes_file = MES_FILE_HANDLE_INVALID;

/**
 * Array storing the global state of each quest.
 *
 * The g-state uses reduced set of values from `QuestState` enum:
 *  - `QUEST_STATE_ACCEPTED`: this value is the default even if there is no PC
 *    who accepted the quest.
 *  - `QUEST_STATE_COMPLETED`: indicates that some PC have completed the quest.
 *  - `QUEST_STATE_BOTCHED`: indicates the quest can no longer be completed by
 *    by anyone.
 *
 * 0x5FF418
 */
static int* quest_gstate;

/**
 * Array of quests metadata.
 *
 * 0x5FF41C
 */
static Quest* quests;

/**
 * Called when the game is initialized.
 *
 * 0x4C45C0
 */
bool quest_init(GameInitInfo* init_info)
{
    (void)init_info;

    // Allocate memory for global quest states and quests meta.
    quest_gstate = (int*)CALLOC(MAX_QUEST, sizeof(*quest_gstate));
    quests = (Quest*)CALLOC(MAX_QUEST, sizeof(*quests));

    if (quest_gstate == NULL || quests == NULL) {
        FREE(quest_gstate);
        FREE(quests);
        quest_gstate = NULL;
        quests = NULL;
        return false;
    }

    // Load xp rewards data from file (required).
    if (!mes_load("Rules\\xp_quest.mes", &quest_xp_mes_file)) {
        FREE(quest_gstate);
        FREE(quests);
        quest_gstate = NULL;
        quests = NULL;
        return false;
    }

    return true;
}

/**
 * Called when the game is being reset.
 *
 * 0x4C4620
 */
void quest_reset(void)
{
    int index;
    if (quest_gstate == NULL) return;

    for (index = 0; index < MAX_QUEST; index++) {
        quest_gstate[index] = QUEST_STATE_ACCEPTED;
    }
}

/**
 * Called when the game shuts down.
 *
 * 0x4C4640
 */
void quest_exit(void)
{
    mes_unload(quest_xp_mes_file);
    FREE(quest_gstate);
    FREE(quests);
    quests = NULL;
    quest_gstate = NULL;
    quest_xp_mes_file = MES_FILE_HANDLE_INVALID;
}

/**
 * Called when a module is being loaded.
 *
 * 0x4C4670
 */
bool quest_mod_load(void)
{
    int index;
    int state;
    MesFileEntry mes_file_entry;
    int num;
    if (quests == NULL || quest_gstate == NULL) return false;

    // Initialize quests meta.
    for (index = 0; index < MAX_QUEST; index++) {
        for (state = 0; state < QUEST_STATE_COUNT; state++) {
            quests[index].normal_dialog[state] = -1;
            quests[index].bad_reaction_dialog[state] = -1;
            quests[index].dumb_dialog[state] = -1;
        }

        quests[index].experience_level = 1;
        quests[index].alignment_adjustment = 0;
        quests[index].normal_description = NULL;
        quests[index].dumb_description = NULL;
    }

    // Parse quests meta from file, which is optional. If this file exists
    // proceed to loading logbook descriptions.
    quest_reset();
    QuestParseResult parsed = quest_parse("rules\\gamequest.mes", 1000, 1999);
    if (parsed == QUEST_PARSE_INVALID) return false;
    if (parsed == QUEST_PARSE_OK) {
        // Load quest logbook descriptions (for characters with normal
        // intelligence).
        if (mes_load("mes\\gamequestlog.mes", &quest_log_mes_file)) {
            for (num = 1000; num < 2000; num++) {
                mes_file_entry.num = num;
                if (mes_search(quest_log_mes_file, &mes_file_entry)) {
                    quests[quest_num_to_idx(num)].normal_description = mes_file_entry.str;
                }
            }
        }

        // Load quest logbook descriptions (for characters with low
        // intelligence).
        if (mes_load("mes\\gamequestlogdumb.mes", &quest_log_dumb_mes_file)) {
            for (num = 1000; num < 2000; num++) {
                mes_file_entry.num = num;
                if (mes_search(quest_log_dumb_mes_file, &mes_file_entry)) {
                    quests[quest_num_to_idx(num)].dumb_description = mes_file_entry.str;
                }
            }
        }

    }

    return true;
}

/**
 * Called when a module is being unloaded.
 *
 * 0x4C47C0
 */
void quest_mod_unload(void)
{
    if (quest_log_mes_file != MES_FILE_HANDLE_INVALID) {
        mes_unload(quest_log_mes_file);
        quest_log_mes_file = MES_FILE_HANDLE_INVALID;
    }

    if (quest_log_dumb_mes_file != MES_FILE_HANDLE_INVALID) {
        mes_unload(quest_log_dumb_mes_file);
        quest_log_dumb_mes_file = MES_FILE_HANDLE_INVALID;
    }
    if (quests != NULL) {
        for (int index = 0; index < MAX_QUEST; index++) {
            quests[index].normal_description = NULL;
            quests[index].dumb_description = NULL;
        }
    }
}

/**
 * Called when the game is being loaded.
 *
 * 0x4C4800
 */
bool quest_load(GameLoadInfo* load_info)
{
    int loaded[MAX_QUEST];
    if (load_info == NULL || load_info->stream == NULL || quest_gstate == NULL
        || tig_file_fread(loaded, sizeof(loaded), 1, load_info->stream) != 1) {
        return false;
    }
    for (int index = 0; index < MAX_QUEST; index++) if (!quest_state_valid(loaded[index])) return false;
    memcpy(quest_gstate, loaded, sizeof(loaded));
    return true;
}

/**
 * Called when the game is being saved.
 *
 * 0x4C4830
 */
bool quest_save(TigFile* stream)
{
    if (stream == NULL || quest_gstate == NULL) return false;
    if (tig_file_fwrite(quest_gstate, sizeof(*quest_gstate) * MAX_QUEST, 1, stream) != 1) {
        return false;
    }

    return true;
}

static bool quest_access_valid(int num)
{
    if (num >= 1000 && num < 1000 + MAX_QUEST && quests != NULL && quest_gstate != NULL) return true;
    tig_debug_printf("Quest: invalid or unavailable quest %d.\n", num);
    return false;
}

static bool quest_state_valid(int state)
{
    return state >= QUEST_STATE_UNKNOWN && state < QUEST_STATE_COUNT;
}

static int quest_pc_state_normalize(int state)
{
    int base = state & ~QUEST_BOTCHED_MODIFIER;
    if (!quest_state_valid(base)) return QUEST_STATE_UNKNOWN;
    return (state & QUEST_BOTCHED_MODIFIER) != 0 ? QUEST_STATE_BOTCHED : base;
}

static bool quest_definition_parse(const char* text, Quest* quest)
{
    int values[2 + 3 * QUEST_STATE_COUNT];
    const char* cursor = text;
    if (cursor == NULL) return false;
    for (int index = 0; index < (int)SDL_arraysize(values); index++) {
        char* end;
        long value;
        while (SDL_isspace((unsigned char)*cursor)) cursor++;
        errno = 0;
        value = strtol(cursor, &end, 10);
        if (end == cursor || errno == ERANGE || value < INT_MIN || value > INT_MAX
            || (*end != '\0' && !SDL_isspace((unsigned char)*end))) return false;
        if ((index == 0 && value < 0) || (index >= 2 && value < -1)) return false;
        values[index] = (int)value;
        cursor = end;
    }
    while (SDL_isspace((unsigned char)*cursor)) cursor++;
    if (*cursor != '\0') return false;
    quest->experience_level = values[0];
    quest->alignment_adjustment = values[1];
    for (int state = 0; state < QUEST_STATE_COUNT; state++) {
        quest->normal_dialog[state] = values[2 + state];
        quest->bad_reaction_dialog[state] = values[2 + QUEST_STATE_COUNT + state];
        quest->dumb_dialog[state] = values[2 + 2 * QUEST_STATE_COUNT + state];
    }
    return true;
}

/**
 * Parses quest info from a message file from the specified range.
 *
 * 0x4C4860
 */
static QuestParseResult quest_parse(const char* path, int start, int end)
{
    mes_file_handle_t mes_file;
    Quest* parsed;
    bool valid = true;
    if (start < 1000 || end >= 1000 + MAX_QUEST || start > end || quests == NULL) return QUEST_PARSE_INVALID;
    if (!mes_load(path, &mes_file)) return QUEST_PARSE_MISSING;
    parsed = MALLOC(MAX_QUEST * sizeof(*parsed));
    if (parsed == NULL) { mes_unload(mes_file); return QUEST_PARSE_INVALID; }
    memcpy(parsed, quests, MAX_QUEST * sizeof(*parsed));
    for (int index = 0; index < mes_entries_count(mes_file); index++) {
        MesFileEntry entry;
        if (!mes_get_entry(mes_file, index, &entry)) { valid = false; break; }
        if (entry.num < start || entry.num > end) {
            tig_debug_printf("Quest: ignoring auxiliary catalog entry %d outside %d-%d.\n", entry.num, start, end);
            continue;
        }
        if (!quest_definition_parse(entry.str, &parsed[quest_num_to_idx(entry.num)])) {
            tig_debug_printf("Quest: invalid definition in %s, entry %d; expected 23 integers and a valid quest number.\n", path, entry.num);
            valid = false;
            break;
        }
    }
    if (valid) memcpy(quests, parsed, MAX_QUEST * sizeof(*quests));
    FREE(parsed);
    mes_unload(mes_file);
    return valid ? QUEST_PARSE_OK : QUEST_PARSE_INVALID;
}

/**
 * Retrieves the dialog line for a quest.
 *
 * 0x4C4C00
 */
int quest_dialog_line(int64_t pc_obj, int64_t npc_obj, int num)
{
    int state;

    // Verify that the object is a PC.
    if (!quest_access_valid(num) || pc_obj == OBJ_HANDLE_NULL || obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return 0;
    }

    state = quest_state_get(pc_obj, num);

    // Check if the PC has low intelligence.
    if (stat_level_get(pc_obj, STAT_INTELLIGENCE) <= LOW_INTELLIGENCE) {
        return quests[quest_num_to_idx(num)].dumb_dialog[state];
    }

    // Check if the PC has bad reaction.
    if (reaction_get(npc_obj, pc_obj) < 20) {
        return quests[quest_num_to_idx(num)].bad_reaction_dialog[state];
    }

    // Normal option.
    return quests[quest_num_to_idx(num)].normal_dialog[state];
}

/**
 * Retrieves the current state of a quest for a player character.
 *
 * 0x4C4CB0
 */
int quest_state_get(int64_t pc_obj, int num)
{
    PcQuestState pc_quest_state = { 0 };
    int state;
    if (!quest_access_valid(num)) return QUEST_STATE_UNKNOWN;

    // Check if obj is not really a PC. In this case return global quest state.
    if (pc_obj == OBJ_HANDLE_NULL || obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return quest_global_state_get(num);
    }

    // Retrieve the PC's quest state.
    obj_arrayfield_pc_quest_get(pc_obj, OBJ_F_PC_QUEST_IDX, num, &pc_quest_state);

    state = quest_pc_state_normalize(pc_quest_state.state);

    return state;
}

/**
 * Sets the quest state for a player character.
 *
 * 0x4C4D20
 */
int quest_state_set(int64_t pc_obj, int num, int state, int64_t npc_obj)
{
    int old_state;
    if (!quest_access_valid(num) || !quest_state_valid(state) || pc_obj == OBJ_HANDLE_NULL
        || obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) return QUEST_STATE_UNKNOWN;

    // Obtain the current quest state.
    old_state = quest_state_get(pc_obj, num);

    // Prevent changes if the quest is already completed or botched.
    if (old_state == QUEST_STATE_COMPLETED
        || old_state == QUEST_STATE_OTHER_COMPLETED
        || old_state == QUEST_STATE_BOTCHED) {
        return old_state;
    }

    // Prevent setting a state lower than the current one.
    if (old_state >= state) {
        return old_state;
    }

    // Award experience points if the quest is completed.
    if (state == QUEST_STATE_COMPLETED) {
        critter_give_xp(pc_obj, quest_get_xp(quests[quest_num_to_idx(num)].experience_level));
    }

    return quest_state_set_internal(pc_obj, num, state, npc_obj);
}

/**
 * Internal function to set a quest state and handle side effects.
 *
 * 0x4C4E60
 */
int quest_state_set_internal(int64_t pc_obj, int num, int state, int64_t npc_obj)
{
    int old_state;
    PcQuestState pc_quest_state = { 0 };
    int reaction;
    if (!quest_access_valid(num) || !quest_state_valid(state) || pc_obj == OBJ_HANDLE_NULL) return QUEST_STATE_UNKNOWN;

    // Verify the object is a player character.
    if (obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return state;
    }

    old_state = quest_state_get(pc_obj, num);

    if (quest_gstate[quest_num_to_idx(num)] == QUEST_STATE_ACCEPTED) {
        // Update global quest state based on current state.
        if (state == QUEST_STATE_COMPLETED || state == QUEST_STATE_OTHER_COMPLETED) {
            quest_gstate[quest_num_to_idx(num)] = QUEST_STATE_COMPLETED;
        } else if (state == QUEST_STATE_BOTCHED) {
            quest_gstate[quest_num_to_idx(num)] = QUEST_STATE_BOTCHED;
        }
    } else {
        // Adjust state if the global state is already completed or botched.
        if (quest_gstate[quest_num_to_idx(num)] == QUEST_STATE_COMPLETED) {
            state = QUEST_STATE_OTHER_COMPLETED;
        } else {
            state = QUEST_STATE_BOTCHED;
        }
    }

    // Update the player's quest state.
    obj_arrayfield_pc_quest_get(pc_obj, OBJ_F_PC_QUEST_IDX, num, &pc_quest_state);
    if (state == QUEST_STATE_BOTCHED) {
        // Keep current quest state, but mark as it botched.
        pc_quest_state.state = old_state | QUEST_BOTCHED_MODIFIER;
    } else {
        pc_quest_state.state = state;
    }

    pc_quest_state.datetime = game_time_get();
    obj_arrayfield_pc_quest_set(pc_obj, OBJ_F_PC_QUEST_IDX, num, &pc_quest_state);

    // Apply alignment adjustment.
    if (state == QUEST_STATE_COMPLETED) {
        int64_t alignment = (int64_t)stat_base_get(pc_obj, STAT_ALIGNMENT) + quests[quest_num_to_idx(num)].alignment_adjustment;
        stat_base_set(pc_obj,
            STAT_ALIGNMENT,
            alignment > INT_MAX ? INT_MAX : alignment < INT_MIN ? INT_MIN : (int)alignment);

        // Play quest completed sound.
        tig_sound_quick_play(SND_INTERFACE_QUESTCOMPLETE);
    }

    // Adjust NPC reaction based on quest state.
    if (npc_obj != OBJ_HANDLE_NULL) {
        if (state == QUEST_STATE_ACCEPTED) {
            // Quest accepted - immediately set reaction to be at least neutral.
            reaction = reaction_get(npc_obj, pc_obj);
            if (reaction < 41) {
                reaction_adj(npc_obj, pc_obj, 41 - reaction);
            }
        } else if (state == QUEST_STATE_COMPLETED) {
            // Quest completed - slightly improve NPC reaction.
            reaction_adj(npc_obj, pc_obj, 10);
        }
    }

    // Highlight logbook button if this is a local PC.
    if (player_is_local_pc_obj(pc_obj)) {
        if ((pc_quest_state.state & ~QUEST_BOTCHED_MODIFIER) != QUEST_STATE_UNKNOWN) {
            ui_toggle_primary_button(UI_PRIMARY_BUTTON_LOGBOOK, true);
        }
    }

    return state;
}

/**
 * Attempts to unbotch a quest.
 *
 * 0x4C5070
 */
int quest_unbotch(int64_t obj, int num)
{
    int state;
    PcQuestState pc_quest_state = { 0 };
    if (!quest_access_valid(num)) return QUEST_STATE_UNKNOWN;

    // Check if the quest is botched globally.
    state = quest_global_state_get(num);
    if (state != QUEST_STATE_BOTCHED) {
        return state;
    }

    // Verify the object is a player character.
    if (obj == OBJ_HANDLE_NULL || obj_field_int32_get(obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return state;
    }

    obj_arrayfield_pc_quest_get(obj, OBJ_F_PC_QUEST_IDX, num, &pc_quest_state);
    if (!quest_state_valid(pc_quest_state.state & ~QUEST_BOTCHED_MODIFIER)) return QUEST_STATE_UNKNOWN;

    // Reset the global quest state ("accepted" is the default state).
    quest_gstate[quest_num_to_idx(num)] = QUEST_STATE_ACCEPTED;

    // Remove botched modifier from the player's quest state, effectively
    // returning quest to it's previous state.
    pc_quest_state.state &= ~QUEST_BOTCHED_MODIFIER;
    pc_quest_state.datetime = game_time_get();
    obj_arrayfield_pc_quest_set(obj, OBJ_F_PC_QUEST_IDX, num, &pc_quest_state);

    if (player_is_local_pc_obj(obj)) {
        if (pc_quest_state.state == QUEST_STATE_COMPLETED) {
            // Play quest completed sound.
            tig_sound_quick_play(SND_INTERFACE_QUESTCOMPLETE);
        }

        if (pc_quest_state.state != QUEST_STATE_UNKNOWN) {
            ui_toggle_primary_button(UI_PRIMARY_BUTTON_LOGBOOK, true);
        }
    }

    return pc_quest_state.state;
}

/**
 * Retrieves the global state of a quest.
 *
 * 0x4C51A0
 */
int quest_global_state_get(int num)
{
    if (!quest_access_valid(num)) return QUEST_STATE_UNKNOWN;
    return quest_gstate[quest_num_to_idx(num)];
}

/**
 * Sets the global state of a quest.
 *
 * 0x4C51C0
 */
int quest_global_state_set(int num, int state)
{
    int old_state;
    if (!quest_access_valid(num) || !quest_state_valid(state)) return QUEST_STATE_UNKNOWN;

    old_state = quest_gstate[quest_num_to_idx(num)];

    // Prevent changes if already completed or botched.
    if (old_state == QUEST_STATE_COMPLETED || old_state == QUEST_STATE_BOTCHED) {
        return old_state;
    }

    if (state == QUEST_STATE_COMPLETED || state == QUEST_STATE_BOTCHED) {
        quest_gstate[quest_num_to_idx(num)] = state;
    } else {
        state = QUEST_STATE_ACCEPTED;
    }

    return state;
}

/**
 * Retrieves the quest description to a buffer based on player intelligence.
 *
 * 0x4C5250
 */
void quest_copy_description(int64_t obj, int num, char* buffer, size_t maxlen)
{
    if (buffer == NULL || maxlen == 0) return;
    buffer[0] = '\0';
    if (!quest_access_valid(num) || obj == OBJ_HANDLE_NULL) return;
    if (quests[quest_num_to_idx(num)].dumb_description != NULL
        && stat_level_get(obj, STAT_INTELLIGENCE) <= LOW_INTELLIGENCE) {
        SDL_strlcpy(buffer, quests[quest_num_to_idx(num)].dumb_description, maxlen);
    } else if (quests[quest_num_to_idx(num)].normal_description != NULL) {
        SDL_strlcpy(buffer, quests[quest_num_to_idx(num)].normal_description, maxlen);
    }
}

/**
 * Populates quest logbook entries array with quests known to a player
 * character. The resulting array is sorted by game time (earliest first).
 *
 * Returns the number of entries written to `logbook_entries`.
 *
 * 0x4C52E0
 */
int quest_get_logbook_data(int64_t obj, QuestLogbookEntry* logbook_entries)
{
    int index;
    PcQuestState pc_quests[2000] = { 0 };
    int cnt;

    // Verify the object is a player character.
    if (logbook_entries == NULL || obj == OBJ_HANDLE_NULL || quests == NULL
        || obj_field_int32_get(obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return 0;
    }

    // Copy all PC quest states.
    obj_arrayfield_pc_quest_copy_to_flat(obj, OBJ_F_PC_QUEST_IDX, 1999, pc_quests);

    // Collect active quests (non-unknown states).
    cnt = 0;
    for (index = 1000; index < 2000; index++) {
        if (quest_pc_state_normalize(pc_quests[index].state) != QUEST_STATE_UNKNOWN) {
            logbook_entries[cnt].num = index;
            logbook_entries[cnt].datetime = pc_quests[index].datetime;
            if ((pc_quests[index].state & QUEST_BOTCHED_MODIFIER) != 0) {
                logbook_entries[cnt].state = QUEST_STATE_BOTCHED;
            } else {
                logbook_entries[cnt].state = quest_pc_state_normalize(pc_quests[index].state);
            }
            cnt++;
        }
    }

    // Sort entries by timestamp.
    qsort(logbook_entries, cnt, sizeof(*logbook_entries), quest_logbook_entry_compare);

    return cnt;
}

/**
 * Compares two quest logbook entries by timestamp.
 *
 * 0x4C53A0
 */
int quest_logbook_entry_compare(const void* va, const void* vb)
{
    const QuestLogbookEntry* a = (const QuestLogbookEntry*)va;
    const QuestLogbookEntry* b = (const QuestLogbookEntry*)vb;

    return datetime_compare(&(a->datetime), &(b->datetime));
}

/**
 * Retrieves the experience points for a quest level.
 *
 * 0x4C53C0
 */
int quest_get_xp(int xp_level)
{
    MesFileEntry mes_file_entry;

    mes_file_entry.num = xp_level;
    if (xp_level < 0 || quest_xp_mes_file == MES_FILE_HANDLE_INVALID
        || !mes_search(quest_xp_mes_file, &mes_file_entry) || mes_file_entry.str == NULL) return 0;
    char* end;
    errno = 0;
    long value = strtol(mes_file_entry.str, &end, 10);
    if (end == mes_file_entry.str || errno == ERANGE || value < 0 || value > INT_MAX) return 0;
    while (SDL_isspace((unsigned char)*end)) end++;
    return *end == '\0' ? (int)value : 0;
}

/**
 * Copies accepted quests from one player to another.
 *
 * 0x4C5400
 */
bool quest_copy_accepted(int64_t src_obj, int64_t dst_obj)
{
    PcQuestState pc_quests[2000] = { 0 };
    int index;
    int dst_state;
    int src_state;

    // Ensure both objects are player characters.
    if (src_obj == OBJ_HANDLE_NULL || dst_obj == OBJ_HANDLE_NULL || quests == NULL
        || obj_field_int32_get(src_obj, OBJ_F_TYPE) != OBJ_TYPE_PC
        || obj_field_int32_get(dst_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return false;
    }

    // Copy source player's quests.
    obj_arrayfield_pc_quest_copy_to_flat(src_obj, OBJ_F_PC_QUEST_IDX, 1999, pc_quests);

    for (index = 1000; index < 2000; index++) {
        dst_state = quest_state_get(dst_obj, index);

        // Skip if quest is completed or botched.
        if (dst_state == QUEST_STATE_COMPLETED
            || dst_state == QUEST_STATE_OTHER_COMPLETED
            || dst_state == QUEST_STATE_BOTCHED) {
            continue;
        }

        src_state = pc_quests[index].state & ~QUEST_BOTCHED_MODIFIER;

        // Copy over if dst doesn't know about the quest, or heard something
        // about it (`QUEST_STATE_UNKNOWN` and `QUEST_STATE_MENTIONED`).
        if (dst_state < src_state && src_state == QUEST_STATE_ACCEPTED) {
            quest_state_set_internal(dst_obj, index, QUEST_STATE_ACCEPTED, OBJ_HANDLE_NULL);

            // Mark as botched if needed.
            if ((pc_quests[index].state & QUEST_BOTCHED_MODIFIER) != 0) {
                quest_state_set_internal(dst_obj, index, QUEST_STATE_BOTCHED, OBJ_HANDLE_NULL);
            }
        }
    }

    return true;
}
