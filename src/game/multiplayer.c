#include "game/multiplayer.h"
#include <limits.h>

#include <stdio.h>

#include "game/anim.h"
#include "game/background.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/descriptions.h"
#include "game/effect.h"
#include "game/gamelib.h"
#include "game/item.h"
#include "game/level.h"
#include "game/light_scheme.h"
#include "game/magictech.h"
#include "game/map.h"
#include "game/mes.h"
#include "game/mp_utils.h"
#include "game/obj_private.h"
#include "game/object.h"
#include "game/party.h"
#include "game/player.h"
#include "game/portrait.h"
#include "game/proto.h"
#include "game/rumor.h"
#include "game/skill.h"
#include "game/stat.h"
#include "game/tb.h"
#include "game/tech.h"
#include "game/tf.h"
#include "game/timeevent.h"
#include "game/trap.h"
#include "game/ui.h"

#define NUM_PLAYERS 8

#define BEGIN_SENTINEL 0x4ACEBABE
#define END_SENTINEL 0xBABE01CE

typedef struct MultiplayerObjectIdNode {
    /* 0000 */ ObjectID oid;
    /* 0018 */ struct MultiplayerObjectIdNode* next;
} MultiplayerObjectIdNode;

typedef struct MultiplayerPlayerState {
    /* 0000 */ unsigned int flags;
    /* 0004 */ int reserved_slot_1;
    /* 0008 */ ObjectID character_id;
    /* 0020 */ int reserved_slot_2;
    /* 0024 */ int reserved_slot_3;
    /* 0028 */ int reserved_slot_4;
    /* 002C */ int reserved_slot_5;
    /* 0030 */ int reserved_slot_6;
    /* 0034 */ int reserved_slot_7;
    /* 0038 */ ObjectID reserved_slot_8;
} MultiplayerPlayerState;

// Serializeable.
static_assert(sizeof(MultiplayerPlayerState) == 0x50, "wrong size");

typedef struct MultiplayerDeferredEntry {
    /* 0000 */ int reserved_slot_1;
    /* 0004 */ int reserved_slot_2;
    /* 0008 */ int reserved_slot_3;
    /* 000C */ int reserved_slot_4;
    /* 0010 */ int reserved_slot_5;
    /* 0014 */ int reserved_slot_6;
    /* 0018 */ int reserved_slot_7;
    /* 001C */ int reserved_slot_8;
    /* 0020 */ int reserved_slot_9;
    /* 0024 */ int reserved_slot_10;
    /* 0028 */ int reserved_slot_11;
    /* 002C */ int reserved_slot_12;
    /* 0030 */ int reserved_slot_13;
    /* 0034 */ int reserved_slot_14;
    /* 0038 */ int reserved_slot_15;
    /* 003C */ int reserved_slot_16;
    /* 0040 */ int reserved_slot_17;
    /* 0044 */ int reserved_slot_18;
    /* 0048 */ int reserved_slot_19;
    /* 004C */ int reserved_slot_20;
    /* 0050 */ int reserved_slot_21;
    /* 0054 */ int reserved_slot_22;
    /* 0058 */ int reserved_slot_23;
    /* 005C */ int reserved_slot_24;
    /* 0060 */ int reserved_slot_25;
    /* 0064 */ int reserved_slot_26;
    /* 0068 */ int reserved_slot_27;
    /* 006C */ int reserved_slot_28;
    /* 0070 */ int reserved_slot_29;
    /* 0074 */ int reserved_slot_30;
    /* 0078 */ int reserved_slot_31;
    /* 007C */ int reserved_slot_32;
    /* 0080 */ int reserved_slot_33;
    /* 0084 */ int reserved_slot_34;
    /* 0088 */ int reserved_slot_35;
    /* 008C */ int reserved_slot_36;
    /* 0090 */ int reserved_slot_37;
    /* 0094 */ int reserved_slot_38;
    /* 0098 */ int reserved_slot_39;
    /* 009C */ int reserved_slot_40;
    /* 00A0 */ int reserved_slot_41;
    /* 00A4 */ int reserved_slot_42;
    /* 00A8 */ int reserved_slot_43;
    /* 00AC */ int reserved_slot_44;
    /* 00B0 */ int reserved_slot_45;
    /* 00B4 */ int reserved_slot_46;
    /* 00B8 */ int reserved_slot_47;
    /* 00BC */ int reserved_slot_48;
    /* 00C0 */ int reserved_slot_49;
    /* 00C4 */ int reserved_slot_50;
    /* 00C8 */ int reserved_slot_51;
    /* 00CC */ int reserved_slot_52;
    /* 00D0 */ int reserved_slot_53;
    /* 00D4 */ int reserved_slot_54;
    /* 00D8 */ int reserved_slot_55;
    /* 00DC */ int reserved_slot_56;
    /* 00E0 */ int reserved_slot_57;
    /* 00E4 */ int reserved_slot_58;
    /* 00E8 */ int reserved_slot_59;
    /* 00EC */ int reserved_slot_60;
    /* 00F0 */ int reserved_slot_61;
    /* 00F4 */ int reserved_slot_62;
    /* 00F8 */ int reserved_slot_63;
    /* 00FC */ int reserved_slot_64;
    /* 0100 */ int reserved_slot_65;
    /* 0104 */ int reserved_slot_66;
    /* 0108 */ int reserved_slot_67;
    /* 010C */ int reserved_slot_68;
    /* 0110 */ int reserved_slot_69;
    /* 0114 */ int reserved_slot_70;
    /* 0118 */ int reserved_slot_71;
    /* 011C */ int reserved_slot_72;
    /* 0120 */ int reserved_slot_73;
    /* 0124 */ int reserved_slot_74;
    /* 0128 */ int reserved_slot_75;
    /* 012C */ int reserved_slot_76;
    /* 0130 */ int reserved_slot_77;
    /* 0134 */ int reserved_slot_78;
    /* 0138 */ int reserved_slot_79;
    /* 013C */ int reserved_slot_80;
    /* 0140 */ int reserved_slot_81;
    /* 0144 */ int reserved_slot_82;
    /* 0148 */ int reserved_slot_83;
    /* 014C */ int reserved_slot_84;
    /* 0150 */ int reserved_slot_85;
    /* 0154 */ int reserved_slot_86;
    /* 0158 */ int reserved_slot_87;
    /* 015C */ int reserved_slot_88;
    /* 0160 */ int reserved_slot_89;
    /* 0164 */ int reserved_slot_90;
    /* 0168 */ int reserved_slot_91;
    /* 016C */ int reserved_slot_92;
    /* 0170 */ int reserved_slot_93;
    /* 0174 */ int reserved_slot_94;
    /* 0178 */ int reserved_slot_95;
    /* 017C */ int reserved_slot_96;
    /* 0180 */ int reserved_slot_97;
    /* 0184 */ int reserved_slot_98;
    /* 0188 */ int reserved_slot_99;
    /* 018C */ int reserved_slot_100;
    /* 0190 */ int reserved_slot_101;
    /* 0194 */ int reserved_slot_102;
    /* 0198 */ int reserved_slot_103;
    /* 019C */ int reserved_slot_104;
    /* 01A0 */ int reserved_slot_105;
    /* 01A4 */ int reserved_slot_106;
    /* 01A8 */ struct MultiplayerDeferredEntry* next;
    /* 01AC */ int reserved_slot_107;
} MultiplayerDeferredEntry;

typedef struct MultiplayerObjectLock {
    /* 0000 */ ObjectID object_id;
    /* 0018 */ ObjectID parent_id;
    /* 0030 */ int client_id;
    /* 0034 */ int reference_count;
    /* 0038 */ struct MultiplayerObjectLock* next;
    /* 003C */ int reserved_slot_1;
} MultiplayerObjectLock;

typedef struct MultiplayerLevelSchemeInfo {
    /* 0000 */ ObjectID oid;
    /* 0018 */ char rule[MAX_STRING];
    /* 07E8 */ char name[MAX_STRING];
} MultiplayerLevelSchemeInfo;

typedef struct MultiplayerCharacterMetadata {
    /* 0000 */ ObjectID oid;
    /* 0018 */ int level;
    /* 001C */ int size;
} MultiplayerCharacterMetadata;

typedef struct MultiplayerLockRequestCallbacks {
    /* 0000 */ bool (*success_func)(void*);
    /* 0004 */ void* success_info;
    /* 0008 */ bool (*failure_func)(void*);
    /* 000C */ void* failure_info;
} MultiplayerLockRequestCallbacks;

static void multiplayer_player_state_init(MultiplayerPlayerState* state);
static void multiplayer_start_play(PlayerCreateInfo* player_create_info);
static bool multiplayer_transfer_timeevent_matches_player(TimeEvent* timeevent);
static void multiplayer_handle_message(void* msg);
static void multiplayer_object_location_apply(int64_t obj, int64_t location, int dx, int dy);
static bool multiplayer_player_object_resolve(int player, int64_t* obj_ptr);
static void multiplayer_send_player_list(void);
static bool multiplayer_validate_message(void* msg);
static void multiplayer_packet_67_send(int packet_value);
static bool multiplayer_handle_network_event(int type, int client_id, void* data, int size);
static void multiplayer_disconnect_ui_notify(void);
static void multiplayer_notify_player_lagging(int64_t obj);
static void multiplayer_notify_player_recovered(int64_t obj);
static void multiplayer_player_state_reset(int player);
static void multiplayer_deferred_entry_push(MultiplayerDeferredEntry* entry);
static void multiplayer_object_locks_clear(void);
static bool multiplayer_object_lock_acquire(ObjectID a, ObjectID b, int player);
static void multiplayer_object_lock_entry_add(ObjectID object_id, ObjectID parent_id, int client_id);
static MultiplayerObjectLock* multiplayer_object_lock_find(ObjectID oid);
static void multiplayer_object_lock_release(ObjectID oid);
static void multiplayer_object_lock_entry_remove(ObjectID oid);
static void multiplayer_player_files_send(int player);
static void multiplayer_local_character_files_send(void);
static bool save_char(const char* path, int64_t obj);
static bool load_char(const char* path, int64_t* obj_ptr);
static bool multiplayer_player_blob_exists(int player);
static void multiplayer_level_scheme_set(int64_t obj, const char* rule, const char* name);
static int multiplayer_level_scheme_get(int64_t obj, char* rule, size_t rule_capacity, char* name, size_t name_capacity);
static void multiplayer_hidden_items_reset(void);
static bool multiplayer_item_hide_for_client(int client_id, int64_t item_obj);
static bool multiplayer_hidden_item_release(int client_id);
static void multiplayer_inventory_viewers_reset_legacy(void);
static void multiplayer_active_animations_restart(void);
static void multiplayer_active_spell_events_restore(void);
static void multiplayer_starting_equipment_grant(int64_t obj);
static int multiplayer_autoequip_gold_grant(int64_t obj, mes_file_handle_t mes_file);
static int multiplayer_background_items_grant(int64_t obj);
static int multiplayer_autoequip_skill_items_grant(int64_t obj, mes_file_handle_t mes_file);
static int multiplayer_autoequip_entry_grant_unless_owned(int64_t obj, mes_file_handle_t mes_file, int num);
static int multiplayer_autoequip_weapons_grant(int64_t obj, mes_file_handle_t mes_file);
static void multiplayer_autoequip_armor_grant(int64_t obj, mes_file_handle_t mes_file);
static int multiplayer_owned_prototype_list_count(int64_t obj, char* str);
static int multiplayer_item_list_create(int64_t obj, char* str);
static bool multiplayer_known_schematic_products_grant(int64_t obj);
static void multiplayer_character_state_restore(int64_t obj);
static bool multiplayer_module_guid_suffix_check(const char* module_path, char* normalized_path);

// 0x5B3FD8
static int multiplayer_reserved_setting_value = 10;

// 0x5B3FEC
static tig_button_handle_t multiplayer_disconnect_button_handle = TIG_BUTTON_HANDLE_INVALID;

static const struct {
    const char* name;
    bool (*save_func)(TigFile* stream);
    bool (*load_func)(GameLoadInfo* load_info);
    void (*ping_func)(void);
} multiplayer_map_state_modules[8] = {
    { "Anim", anim_save, anim_load, multiplayer_active_animations_restart },
    { "MagicTech", magictech_post_save, magictech_post_load, multiplayer_active_spell_events_restore },
    { "Trap", NULL, NULL, NULL },
    { "Quest", quest_save, quest_load, NULL },
    { "Rumor", rumor_save, rumor_load, NULL },
    { "Party", party_save, party_load, NULL },
    { "LightScheme", light_scheme_save, light_scheme_load, NULL },
    { "Script", script_save, script_load, NULL },
};

// 0x5B4070
static int multiplayer_selected_timeevent_parameter = -1;

// 0x5B40D8
static int multiplayer_player_iterator_index = -1;

// 0x5E8838
static char multiplayer_module_source_path[TIG_MAX_PATH];

// 0x5E8940
static TigIdxTable multiplayer_lock_requests;

// 0x5E8AD0
static MultiplayerPlayerState multiplayer_player_states[NUM_PLAYERS];

// 0x5E8D50
static char multiplayer_unused_name_buffer[40];

// 0x5E8D78
static int64_t multiplayer_inventory_viewers[NUM_PLAYERS];

// 0x5E8DB8
static char multiplayer_unused_description_buffer[80];

// 0x5E8E08
static MultiplayerLevelSchemeInfo multiplayer_level_scheme_tbl[NUM_PLAYERS];

// 0x5F0BE8
static mes_file_handle_t multiplayer_mes_file;

// 0x5F0BEC
static char multiplayer_module_guid_path[TIG_MAX_PATH];

// 0x5F0BC8
static MultiplayerCharacterMetadata* multiplayer_character_metadata[NUM_PLAYERS];

// 0x5F0D18
static char multiplayer_unused_path_buffer[128];

// 0x5F0D98
static TigGuid multiplayer_module_guid;

// 0x5F0DE0
static int multiplayer_client_time_lead_seconds;

// 0x5F0DE4
static int64_t* multiplayer_saved_characters;

// 0x5F0DE8
static int multiplayer_saved_character_count;

// 0x5F0DEC
static MultiplayerObjectIdNode* multiplayer_object_id_list;

// 0x5F0DF4
static int multiplayer_level_scheme_next_slot;

// 0x5F0DF8
static MultiplayerDisconnectCallback multiplayer_disconnect_callback;

// 0x5F0DFC
static MultiplayerDeferredEntry* multiplayer_deferred_entries;

// 0x5F0E00
static bool multiplayer_session_active;

// 0x5F0E04
static void (*multiplayer_disconnect_ui_callback)(void);

// 0x5F0E08
static MultiplayerOptionalPlayerCallback multiplayer_optional_player_callback;

// 0x5F0E0C
static int multiplayer_lock_cnt;

// 0x5F0E10
static int multiplayer_ping_suppress_count;

// 0x5F0E14
static bool multiplayer_disconnect_pending;

// 0x5F0E18
static int multiplayer_next_lock_request_id;

// 0x5F0E1C
static MultiplayerObjectLock* multiplayer_object_locks;

// 0x5F0E20
static int64_t multiplayer_dragged_item_objects[NUM_PLAYERS];

// 0x49C670
bool multiplayer_init(GameInitInfo* init_info)
{
    TigFileInfo file_info;
    int index;

    (void)init_info;

    if (tig_file_exists("Players", &file_info)) {
        if ((file_info.attributes & TIG_FILE_ATTRIBUTE_SUBDIR) == 0) {
            tig_debug_printf("MP: init: ERROR: players folder (%s) could not be made (already a file).\n", "Players");
            return false;
        }
    } else {
        tig_file_mkdir_ex("Players");
    }

    multiplayer_disconnect_pending = false;

    if (!tig_net_local_client_set_name("Player")) {
        return false;
    }

    if (!tig_net_local_server_set_max_players(NUM_PLAYERS)) {
        return false;
    }

    if (!tig_net_local_server_set_description("Description")) {
        return false;
    }

    for (index = 0; index < NUM_PLAYERS; index++) {
        multiplayer_player_state_init(&(multiplayer_player_states[index]));
    }

    mes_load("mes\\MultiPlayer.mes", &multiplayer_mes_file);
    tig_idxtable_init(&multiplayer_lock_requests, sizeof(MultiplayerLockRequestCallbacks));
    multiplayer_object_id_list = NULL;
    memset(multiplayer_character_metadata, 0, sizeof(multiplayer_character_metadata));
    multiplayer_hidden_items_reset();
    multiplayer_inventory_viewers_reset_legacy();
    multiplayer_object_locks_clear();

    return true;
}

// 0x49C780
void multiplayer_exit(void)
{
    MultiplayerObjectIdNode* node;
    int index;

    if (multiplayer_session_active) {
        multiplayer_end();
    }

    while (multiplayer_object_id_list != NULL) {
        node = multiplayer_object_id_list;
        multiplayer_object_id_list = multiplayer_object_id_list->next;
        FREE(node);
    }

    for (index = 0; index < NUM_PLAYERS; index++) {
        if (multiplayer_character_metadata[index] != NULL) {
            FREE(multiplayer_character_metadata[index]);
            multiplayer_character_metadata[index] = NULL;
        }
    }

    tig_idxtable_exit(&multiplayer_lock_requests);
    mes_unload(multiplayer_mes_file);

    if (multiplayer_saved_characters) {
        FREE(multiplayer_saved_characters);
    }
}

// 0x49C820
void multiplayer_reset(void)
{
    MultiplayerObjectIdNode* node;
    int index;

    if (!multiplayer_ping_suppress_count) {
        if (multiplayer_session_active) {
            multiplayer_end();
        }
        multiplayer_session_active = false;

        tig_idxtable_exit(&multiplayer_lock_requests);
        tig_idxtable_init(&multiplayer_lock_requests, 16);

        while (multiplayer_object_id_list != NULL) {
            node = multiplayer_object_id_list;
            multiplayer_object_id_list = multiplayer_object_id_list->next;
            FREE(node);
        }

        tig_net_local_server_set_name("Arcanum");
        tig_net_local_client_set_name("Player");
        tig_net_local_server_set_max_players(NUM_PLAYERS);
        tig_net_local_server_set_description("Description");

        for (index = 0; index < NUM_PLAYERS; index++) {
            multiplayer_player_state_init(&(multiplayer_player_states[index]));
        }

        for (index = 0; index < NUM_PLAYERS; index++) {
            if (multiplayer_character_metadata[index] != NULL) {
                FREE(multiplayer_character_metadata[index]);
                multiplayer_character_metadata[index] = NULL;
            }
        }

        if (multiplayer_saved_characters != NULL) {
            multiplayer_saved_characters_release(false);
            multiplayer_saved_characters = NULL;
            multiplayer_saved_character_count = 0;
        }

        multiplayer_hidden_items_reset();
        multiplayer_inventory_viewers_reset_legacy();
        timeevent_clear_all_typed(TIMEEVENT_TYPE_MULTIPLAYER);
    }
}

// 0x49C930
bool multiplayer_save(TigFile* stream)
{
    unsigned int sentinel;
    MultiplayerObjectIdNode* node;
    int cnt;

    if (!tig_net_is_active()) {
        return true;
    }

    sentinel = BEGIN_SENTINEL;
    if (tig_file_fwrite(&sentinel, sizeof(sentinel), 1, stream) != 1) {
        return false;
    }

    cnt = 0;
    node = multiplayer_object_id_list;
    while (node != NULL) {
        cnt++;
        node = node->next;
    }

    if (tig_file_fwrite(&cnt, sizeof(cnt), 1, stream) != 1) {
        return false;
    }

    node = multiplayer_object_id_list;
    while (node != NULL) {
        if (tig_file_fwrite(&(node->oid), sizeof(node->oid), 1, stream) != 1) {
            return false;
        }
        node = node->next;
    }

    if (tig_file_fwrite(multiplayer_player_states, sizeof(*multiplayer_player_states), NUM_PLAYERS, stream) != NUM_PLAYERS) {
        return false;
    }

    sentinel = END_SENTINEL;
    if (tig_file_fwrite(&sentinel, sizeof(sentinel), 1, stream) != 1) {
        return false;
    }

    return true;
}

// 0x49CA20
bool mutliplayer_load(GameLoadInfo* load_info)
{
    unsigned int sentinel;
    MultiplayerObjectIdNode* node;
    int cnt;
    ObjectID oid;

    if (!tig_net_is_active()) {
        return true;
    }

    if (tig_file_fread(&sentinel, sizeof(sentinel), 1, load_info->stream) != 1) {
        return false;
    }

    if (sentinel != BEGIN_SENTINEL) {
        return false;
    }

    if (tig_file_fread(&cnt, sizeof(cnt), 1, load_info->stream) != 1) {
        return false;
    }

    if (!tig_file_count_fits(load_info->stream, cnt, INT_MAX, sizeof(oid))) return false;
    while (cnt != 0) {
        if (tig_file_fread(&oid, sizeof(oid), 1, load_info->stream) != 1) {
            return false;
        }

        node = (MultiplayerObjectIdNode*)MALLOC(sizeof(*node));
        node->oid = oid;
        node->next = multiplayer_object_id_list;
        multiplayer_object_id_list = node;

        cnt--;
    }

    if (tig_file_fread(multiplayer_player_states, sizeof(*multiplayer_player_states), NUM_PLAYERS, load_info->stream) != NUM_PLAYERS) {
        return false;
    }

    if (tig_file_fread(&sentinel, sizeof(sentinel), 1, load_info->stream) != 1) {
        return false;
    }

    if (sentinel != END_SENTINEL) {
        return false;
    }

    return true;
}

// 0x49CB50
bool multiplayer_mod_load(void)
{
    return true;
}

// 0x49CB60
void multiplayer_mod_unload(void)
{
    multiplayer_lock();
    multiplayer_saved_characters_release(true);
    multiplayer_unlock();
}

// 0x49CB80
void multiplayer_player_state_init(MultiplayerPlayerState* state)
{
    memset(state, 0, sizeof(*state));
    state->flags = 0;
    state->reserved_slot_2 = -1;
    state->reserved_slot_3 = 1;
    state->reserved_slot_4 = 0;
    state->reserved_slot_5 = 0;
    state->reserved_slot_6 = 0;
    state->reserved_slot_7 = 0;
    memset(&(state->character_id), 0, sizeof(state->character_id));
    memset(&(state->reserved_slot_8), 0, sizeof(state->reserved_slot_8));
    state->reserved_slot_8.type = OID_TYPE_NULL;
}

// 0x49CBD0
bool multiplayer_start(void)
{
    multiplayer_lock_cnt = 0;
    if (tig_net_start_client() != TIG_OK) {
        return false;
    }

    multiplayer_player_state_reset(0);
    tig_net_on_message(multiplayer_handle_message);
    tig_net_on_message_validation(multiplayer_validate_message);
    tig_net_on_network_event(multiplayer_handle_network_event);
    multiplayer_session_active = false;

    return true;
}

// 0x49CC20
bool multiplayer_end(void)
{
    bool shutdown_status;

    multiplayer_player_state_reset(0);
    tig_net_on_message(NULL);
    shutdown_status = tig_net_shutdown_status_stub();
    multiplayer_session_active = false;
    multiplayer_lock_cnt = 0;
    return !shutdown_status;
}

// 0x49CC50
void multiplayer_server_start(void)
{
    tig_net_start_server();
}

// 0x49CC70
bool multiplayer_host_game_load(const char* module_name, const char* save_name)
{
    const char* suffixes[3] = {
        ".mpc",
        ".bmp",
        "_b.bmp",
    };
    mes_file_handle_t mes_file;
    TigWindowModalDialogInfo modal_dialog_info;
    MesFileEntry mes_file_entry;
    char path[TIG_MAX_PATH];
    char str[512];
    int map;
    char* map_name;
    PlayerCreateInfo player_create_info;
    int idx;
    char pc_file_base_name[40];
    char src[TIG_MAX_PATH];
    char dst[TIG_MAX_PATH];

    timeevent_pause_reset();
    tig_file_empty_directory(".\\data\\temp");

    modal_dialog_info.type = TIG_WINDOW_MODAL_DIALOG_TYPE_OK;
    modal_dialog_info.x = 237;
    modal_dialog_info.y = 232;
    modal_dialog_info.process = NULL;
    modal_dialog_info.redraw = gamelib_redraw;

    mes_load("mes\\MultiPlayer.mes", &mes_file);

    snprintf(path, sizeof(path), "%s\\%s.dat", ".\\Modules", module_name);
    if (tig_file_exists(path, NULL)) {
        tig_file_repository_add(path);
        if (tig_file_repository_guid(path, &multiplayer_module_guid)) {
            if (multiplayer_module_guid_suffix_check(path, multiplayer_module_source_path)) {
                strncpy(multiplayer_module_guid_path, path, sizeof(multiplayer_module_guid_path));
            } else {
                strncpy(multiplayer_module_source_path, path, sizeof(multiplayer_module_source_path));
                snprintf(multiplayer_module_guid_path, sizeof(multiplayer_module_guid_path),
                    "%s\\%s-{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}.dat",
                    ".\\Modules",
                    module_name,
                    (multiplayer_module_guid.data[0] << 24) | (multiplayer_module_guid.data[1] << 16) | (multiplayer_module_guid.data[2] << 8) | multiplayer_module_guid.data[3],
                    (multiplayer_module_guid.data[4] << 8) | multiplayer_module_guid.data[5],
                    (multiplayer_module_guid.data[6] << 8) | multiplayer_module_guid.data[7],
                    multiplayer_module_guid.data[8],
                    multiplayer_module_guid.data[9],
                    multiplayer_module_guid.data[10],
                    multiplayer_module_guid.data[11],
                    multiplayer_module_guid.data[12],
                    multiplayer_module_guid.data[13],
                    multiplayer_module_guid.data[14],
                    multiplayer_module_guid.data[15]);
            }
        }
        tig_file_repository_remove(path);
    } else {
        mes_file_entry.num = 900;
        mes_get_msg(mes_file, &mes_file_entry);
        snprintf(str, sizeof(str), mes_file_entry.str, module_name);
        modal_dialog_info.text = str;
        tig_window_modal_dialog(&modal_dialog_info, NULL);
    }

    if (!gamelib_mod_load(module_name) || !ui_gameuilib_mod_load()) {
        if (!gamelib_mod_load(gamelib_default_module_name_get()) || !ui_gameuilib_mod_load()) {
            tig_debug_printf("Multiplayer: Could not load module %s, or default module %s.\n",
                module_name,
                gamelib_default_module_name_get());
            exit(EXIT_SUCCESS); // FIXME: Should be `EXIT_FAILURE`.
        }

        mes_file_entry.num = 901;
        mes_get_msg(mes_file, &mes_file_entry);
        snprintf(str, sizeof(str), mes_file_entry.str, module_name);
        modal_dialog_info.text = str;
        tig_window_modal_dialog(&modal_dialog_info, NULL);
        mes_unload(mes_file);
        return false;
    }

    tig_net_local_server_set_description(module_name);
    player_local_pc_destroy();

    if (save_name != NULL) {
        if (gamelib_load(save_name)) {
            gamelib_current_mode_name_set(module_name);
        }
    } else {
        map = map_by_type(MAP_TYPE_START_MAP);
        if (map == 0) {
            mes_file_entry.num = 902;
            mes_get_msg(mes_file, &mes_file_entry);
            modal_dialog_info.text = mes_file_entry.str;
            tig_window_modal_dialog(&modal_dialog_info, NULL);
            mes_unload(mes_file);
            return false;
        }

        if (!map_get_name(map, &map_name)) {
            mes_unload(mes_file);
            return false;
        }

        multiplayer_map_open_by_name(map_name);
        tig_net_legacy_play_begin_stub();

        multiplayer_start_play(&player_create_info);
        tig_net_legacy_play_end_stub();
        location_origin_set(obj_field_int64_get(player_create_info.obj, OBJ_F_LOCATION));

        objid_id_to_str(pc_file_base_name, obj_get_id(player_create_info.obj));
        snprintf(dst, sizeof(dst), "%s\\Players", ".\\data\\temp");
        if (!tig_file_is_directory(dst)) {
            tig_file_mkdir(dst);
        }

        for (idx = 0; idx < 3; idx++) {
            snprintf(path, sizeof(path), "%s%s", pc_file_base_name, suffixes[idx]);
            snprintf(src, sizeof(src), "Players\\%s", path);
            snprintf(dst, sizeof(dst), "%s\\Players\\%s", ".\\data\\temp", path);
            tig_file_copy(src, dst);
        }
    }

    tig_net_legacy_session_ready_stub();
    multiplayer_player_preferences_apply();
    tig_net_legacy_preferences_applied_stub();
    mes_unload(mes_file);

    return true;
}

// 0x49D100
void multiplayer_start_play(PlayerCreateInfo* player_create_info)
{
    int64_t loc;
    int poison;
    mes_file_handle_t mes_file;
    TigWindowModalDialogInfo modal_dialog_info;
    MesFileEntry mes_file_entry;
    DateTime datetime;
    TimeEvent timeevent;

    player_create_info_init(player_create_info);
    map_starting_loc_get(&loc);
    player_create_info->loc = location_make(1, 1);
    mes_load("mes\\MultiPlayer.mes", &mes_file);

    modal_dialog_info.type = TIG_WINDOW_MODAL_DIALOG_TYPE_OK;
    modal_dialog_info.x = 237;
    modal_dialog_info.y = 232;
    modal_dialog_info.process = NULL;
    modal_dialog_info.redraw = gamelib_redraw;

    if (multiplayer_player_blob_exists(0)) {
        if (!object_deserialize_at_location(multiplayer_player_blob_data_get(0), multiplayer_player_blob_data_size_get(0), &(player_create_info->obj), player_create_info->loc)) {
            exit(EXIT_FAILURE);
        }

        multiplayer_player_states[0].character_id = obj_get_id(player_create_info->obj);
        player_local_pc_set(player_create_info->obj);
        critter_fatigue_damage_set(player_create_info->obj, 0);
        object_hp_damage_set(player_create_info->obj, 0);
        object_flags_unset(player_create_info->obj, OF_OFF);

        poison = stat_base_get(player_create_info->obj, STAT_POISON_LEVEL);
        if (poison != 0) {
            stat_base_set(player_create_info->obj, STAT_POISON_LEVEL, poison);
        }
    } else {
        mes_file_entry.num = 903;
        mes_get_msg(mes_file, &mes_file_entry);

        modal_dialog_info.text = mes_file_entry.str;
        tig_window_modal_dialog(&modal_dialog_info, NULL);

        player_create_info->obj = OBJ_HANDLE_NULL;
        player_create_info->flags = PLAYER_CREATE_INFO_LOC;
        if (!player_obj_create_player(player_create_info)) {
            tig_debug_printf("MP: multiplayer_start_play could not create_player");
            exit(EXIT_FAILURE);
        }

        multiplayer_player_states[0].character_id = player_create_info->oid;
        obj_field_string_set(player_create_info->obj, OBJ_F_PC_PLAYER_NAME, tig_net_client_info_get_name(0));
    }

    multiplayer_character_state_restore(player_create_info->obj);
    multiplayer_starting_equipment_grant(player_create_info->obj);
    mp_object_move_to_location(player_create_info->obj, loc, 0, 0, false);

    datetime.days = 0;
    datetime.milliseconds = 0;
    timeevent.type = TIMEEVENT_TYPE_TELEPORTED;
    timeevent.params[0].object_value = player_create_info->obj;
    timeevent_add_delay(&timeevent, &datetime);

    mes_unload(mes_file);
}

// 0x49D320
bool multiplayer_timeevent_process(TimeEvent* timeevent)
{
    DateTime datetime;
    Packet64 pkt;
    char* map_name;
    int player;
    char str[40];
    char path[TIG_MAX_PATH];

    switch (timeevent->params[0].integer_value) {
    case 2:
        if (tig_net_xfer_count(timeevent->params[1].integer_value)) {
            datetime_init(&datetime, 50);
            timeevent_add_delay(timeevent, &datetime);
            return true;
        }

        multiplayer_selected_timeevent_parameter = timeevent->params[1].integer_value;
        timeevent_clear_all_ex(TIMEEVENT_TYPE_MULTIPLAYER, multiplayer_transfer_timeevent_matches_player);
        multiplayer_selected_timeevent_parameter = -1;

        pkt.type = 64;
        pkt.map = map_current_map();
        map_get_name(map_current_map(), &map_name);
        strcpy(pkt.name, map_name);
        pkt.player = timeevent->params[1].integer_value;
        pkt.reserved_slot_3 = 0;
        tig_net_send_app_all(&pkt, sizeof(pkt));
        return true;
    case 4:
        if (tig_net_xfer_count(timeevent->params[1].integer_value)) {
            datetime_init(&datetime, 50);
            timeevent_add_delay(timeevent, &datetime);
            return true;
        }

        for (player = 1; player < 8; player++) {
            if (tig_net_client_is_active(player)
                && !tig_net_client_is_waiting(player)
                && !tig_net_client_is_loading(player)
                && player != timeevent->params[1].integer_value) {
                multiplayer_player_files_send(player);
            }
        }

        return true;
    case 3:
        objid_id_to_str(str, obj_get_id(player_get_local_pc_obj()));
        snprintf(path, sizeof(path), "Players\\%s.mpc", str);
        anim_object_interrupt_by_priority(player_get_local_pc_obj(), PRIORITY_HIGHEST, false, true);

        if (ui_multiplayer_confirmation_1902()) {
            save_char(path, player_get_local_pc_obj());
        }

        multiplayer_disconnect_pending = 0;
        if (multiplayer_disconnect_callback != NULL) {
            multiplayer_disconnect_callback(multiplayer_disconnect_button_handle);
        }

        multiplayer_reset();
        multiplayer_disconnect_callback = NULL;

        return true;
    case 5:
        tig_net_legacy_timed_event_stub(timeevent->params[1].integer_value);
        return true;
    default:
        return true;
    }
}

// 0x49D570
bool multiplayer_transfer_timeevent_matches_player(TimeEvent* timeevent)
{
    return timeevent->params[0].integer_value == 2
        && timeevent->params[1].integer_value == multiplayer_selected_timeevent_parameter;
}

// 0x49D590
bool multiplayer_map_open_by_name(const char* name)
{
    char path[TIG_MAX_PATH];
    char save_path[TIG_MAX_PATH];

    snprintf(path, sizeof(path), ".\\%s.dat", name);
    if (tig_file_exists(path, NULL)) {
        tig_file_repository_add(path);
    }

    tig_net_legacy_play_begin_stub();
    map_flush(0);

    if (!obj_validate_system(1)) {
        tig_debug_println("Object system validate failed pre-load in multiplayer_map_open_by_name.");
        tig_message_post_quit(0);
    }

    snprintf(path, sizeof(path), "maps\\%s", name);
    snprintf(save_path, sizeof(save_path), "%s\\maps\\%s", "Save\\Current", name);
    tig_debug_printf("MP: Loading Map: %s\n", path);
    if (!map_open(path, save_path, 1)) {
        return false;
    }

    tig_net_legacy_play_end_stub();
    ui_targeting_reset();

    if (!obj_validate_system(1)) {
        tig_debug_println("Object system validate failed post-load in multiplayer_map_open_by_name.");
        tig_message_post_quit(0);
    }

    return true;
}

// 0x49D690
void multiplayer_handle_message(void* msg)
{
    // TODO: Incomplete.
}

// 0x4A1F30
void multiplayer_object_location_apply(int64_t obj, int64_t location, int dx, int dy)
{
    if (location != 0) {
        object_move_to_location(obj, location, dx, dy);
    }
}

// 0x4A1F60
bool multiplayer_player_object_resolve(int player, int64_t* obj_ptr)
{
    if (player >= 0 && player < NUM_PLAYERS) {
        *obj_ptr = obj_pool_perm_lookup(multiplayer_player_states[player].character_id);
        return *obj_ptr != OBJ_HANDLE_NULL;
    }

    *obj_ptr = OBJ_HANDLE_NULL;
    return false;
}

// 0x4A1FC0
void multiplayer_send_player_list(void)
{
    PacketGamePlayerList pkt;
    int cnt;
    int index;

    if (tig_net_is_host()) {
        pkt.type = 2;

        cnt = tig_net_local_server_get_max_players();
        for (index = 0; index < cnt; index++) {
            pkt.oids[index] = multiplayer_player_states[index].character_id;
        }

        tig_net_send_app_all(&pkt, sizeof(pkt));
    }
}

// 0x4A2020
bool multiplayer_validate_message(void* msg)
{
    int type;

    type = *(int*)msg;
    return type > 0 && type < 131;
}

// 0x4A2040
void multiplayer_packet_67_send(int packet_value)
{
    Packet67 pkt;

    pkt.type = 67;
    pkt.value = packet_value;
    tig_net_send_app_all(&pkt, sizeof(pkt));
}

// 0x4A2070
bool multiplayer_handle_network_event(int type, int client_id, void* data, int size)
{
    // TODO: Incomplete.
}

// 0x4A2A30
void multiplayer_disconnect_ui_notify(void)
{
    if (multiplayer_disconnect_ui_callback != NULL) {
        multiplayer_disconnect_ui_callback();
    } else {
        ui_mainmenu_request();
    }
}

// 0x4A2A40
void multiplayer_notify_player_lagging(int64_t obj)
{
    PacketNotifyPlayerLagging pkt;

    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    anim_lag_icon_add(obj);

    if (tig_net_is_host()) {
        pkt.type = 68;
        object_handle_reference_init(obj, &(pkt.object_reference));
        tig_net_send_app_all(&pkt, sizeof(pkt));
    }
}

// 0x4A2A90
void multiplayer_notify_player_recovered(int64_t obj)
{
    PacketNotifyPlayerRecovered pkt;

    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    anim_lag_icon_remove(obj);

    if (tig_net_is_host()) {
        pkt.type = 69;
        object_handle_reference_init(obj, &(pkt.object_reference));
        tig_net_send_app_all(&pkt, sizeof(pkt));
    }
}

// 0x4A2AE0
void multiplayer_player_state_reset(int player)
{
    multiplayer_player_state_init(&multiplayer_player_states[player]);
}

// 0x4A2B00
void multiplayer_player_callback_set(MultiplayerOptionalPlayerCallback func)
{
    multiplayer_optional_player_callback = func;
}

// 0x4A2B10
int multiplayer_find_slot_from_obj(int64_t obj)
{
    int player;
    int64_t player_obj;

    for (player = 0; player < NUM_PLAYERS; player++) {
        if (tig_net_client_is_active(player)
            && multiplayer_player_object_resolve(player, &player_obj)
            && player_obj == obj) {
            return player;
        }
    }

    return -1;
}

// 0x4A2B60
int64_t multiplayer_active_player_object_get(int player)
{
    int64_t obj;

    if (tig_net_client_is_active(player)
        && multiplayer_player_object_resolve(player, &obj)) {
        return obj;
    }

    return OBJ_HANDLE_NULL;
}

// 0x4A2BA0
bool multiplayer_is_locked(void)
{
    return tig_net_is_active() ? multiplayer_lock_cnt > 0 : true;
}

// 0x4A2BC0
void multiplayer_lock(void)
{
    multiplayer_lock_cnt++;
}

// 0x4A2BD0
void multiplayer_unlock(void)
{
    if (multiplayer_lock_cnt > 0) {
        multiplayer_lock_cnt--;
    }
}

// 0x4A2BE0
int64_t multiplayer_player_find_first(void)
{
    int64_t obj;

    multiplayer_player_iterator_index = -1;

    if (!tig_net_is_active()) {
        return player_get_local_pc_obj();
    }

    for (multiplayer_player_iterator_index = 0; multiplayer_player_iterator_index < NUM_PLAYERS; multiplayer_player_iterator_index++) {
        if (tig_net_client_is_active(multiplayer_player_iterator_index)
            && multiplayer_player_object_resolve(multiplayer_player_iterator_index, &obj)) {
            multiplayer_player_iterator_index++;
            return obj;
        }
    }

    return OBJ_HANDLE_NULL;
}

// 0x4A2C60
int64_t multiplayer_player_find_next(void)
{
    int64_t obj;

    if (!tig_net_is_active()) {
        return OBJ_HANDLE_NULL;
    }

    for (; multiplayer_player_iterator_index < NUM_PLAYERS; multiplayer_player_iterator_index++) {
        if (tig_net_client_is_active(multiplayer_player_iterator_index)
            && multiplayer_player_object_resolve(multiplayer_player_iterator_index, &obj)) {
            multiplayer_player_iterator_index++;
            return obj;
        }
    }

    return OBJ_HANDLE_NULL;
}

// 0x4A2CD0
void multiplayer_deferred_entry_push(MultiplayerDeferredEntry* entry)
{
    MultiplayerDeferredEntry* node;

    node = (MultiplayerDeferredEntry*)MALLOC(sizeof(*node));
    memcpy(node, entry, sizeof(*node));
    node->next = multiplayer_deferred_entries;
    multiplayer_deferred_entries = node;
}

// 0x4A2D00
void multiplayer_deferred_queue_hook_stub(void)
{
    // TODO: Incomplete.
}

// 0x4A2E90
void multiplayer_object_locks_clear(void)
{
    MultiplayerObjectLock* node;

    while (multiplayer_object_locks != NULL) {
        node = multiplayer_object_locks;
        multiplayer_object_locks = multiplayer_object_locks->next;
        FREE(node);
    }
}

// 0x4A2EC0
bool multiplayer_object_lock_acquire(ObjectID item_oid, ObjectID parent_oid, int player)
{
    int64_t item_obj;
    int64_t parent_obj;
    int64_t tmp_parent_obj;
    MultiplayerObjectLock* lock;

    item_obj = obj_pool_perm_lookup(item_oid);
    mp_object_id_resolve_nullable(parent_oid, &parent_obj);

    if (obj_type_is_item(obj_field_int32_get(item_obj, OBJ_F_TYPE))) {
        if (item_parent(item_obj, &tmp_parent_obj)
            && tmp_parent_obj != parent_obj) {
            return false;
        }

        if ((obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS) & OIF_NO_DISPLAY) != 0) {
            if (multiplayer_active_player_object_get(player) != parent_obj) {
                return false;
            }
        }
    }

    if ((obj_field_int32_get(item_obj, OBJ_F_FLAGS) & OF_MULTIPLAYER_LOCK) != 0) {
        lock = multiplayer_object_lock_find(item_oid);
        if (lock == NULL) {
            return false;
        }

        if (lock->client_id != player) {
            return false;
        }

        lock->reference_count++;
        return true;
    } else {
        object_flags_set(item_obj, OF_MULTIPLAYER_LOCK);
        multiplayer_object_lock_entry_add(item_oid, parent_oid, player);
        return true;
    }
}

// 0x4A3030
void multiplayer_object_lock_entry_add(ObjectID object_id, ObjectID parent_id, int client_id)
{
    MultiplayerObjectLock* node;

    node = (MultiplayerObjectLock*)MALLOC(sizeof(*node));
    node->object_id = object_id;
    node->parent_id = parent_id;
    node->client_id = client_id;
    node->reference_count = 1;
    node->next = multiplayer_object_locks;
    multiplayer_object_locks = node;
}

// 0x4A3080
MultiplayerObjectLock* multiplayer_object_lock_find(ObjectID oid)
{
    MultiplayerObjectLock* node;

    node = multiplayer_object_locks;
    while (node != NULL) {
        if (objid_is_equal(node->object_id, oid)) {
            return node;
        }
        node = node->next;
    }

    return NULL;
}

// 0x4A30D0
void multiplayer_object_lock_release(ObjectID oid)
{
    int64_t obj;
    MultiplayerObjectLock* node;

    mp_object_id_resolve_nullable(oid, &obj);
    if ((obj_field_int32_get(obj, OBJ_F_FLAGS) & OF_MULTIPLAYER_LOCK) != 0) {
        node = multiplayer_object_lock_find(oid);
        if (node != NULL) {
            node->reference_count--;
            if (node->reference_count == 0) {
                object_flags_unset(obj, OF_MULTIPLAYER_LOCK);
                multiplayer_object_lock_entry_remove(oid);
            }
        }
    }
}

// 0x4A3170
void multiplayer_object_lock_entry_remove(ObjectID oid)
{
    MultiplayerObjectLock* node;
    MultiplayerObjectLock** parent_ptr;

    if (multiplayer_object_locks != NULL) {
        // NOTE: Probably can be joined into loop.
        if (objid_is_equal(multiplayer_object_locks->object_id, oid)) {
            node = multiplayer_object_locks;
            multiplayer_object_locks = multiplayer_object_locks->next;
            FREE(node);
        } else {
            node = multiplayer_object_locks;
            parent_ptr = &(multiplayer_object_locks->next);
            while (node->next != NULL) {
                if (objid_is_equal(node->object_id, oid)) {
                    *parent_ptr = node->next;
                    FREE(node);
                    break;
                }
                node = node->next;
                parent_ptr = &((*parent_ptr)->next);
            }
        }
    }
}

// 0x4A3230
void multiplayer_object_lock_request(ObjectID oid, bool (*success_func)(void*), void* success_info, bool (*failure_func)(void*), void* failure_info)
{
    if (tig_net_is_active()) {
        int64_t item_obj;
        int64_t parent_obj;
        ObjectID parent_oid;

        mp_object_id_resolve_nullable(oid, &item_obj);
        item_parent(item_obj, &parent_obj);
        mp_object_id_get_nullable(parent_obj, &parent_oid);
        if (tig_net_is_host()) {
            if (multiplayer_object_lock_acquire(oid, parent_oid, 0)) {
                if (success_func != NULL) {
                    success_func(success_info);
                }

                multiplayer_object_lock_release(oid);
            } else {
                if (failure_func != NULL) {
                    failure_func(failure_info);
                }
            }
        } else {
            MultiplayerLockRequestCallbacks entry;
            Packet80 pkt;

            entry.success_func = success_func;
            entry.success_info = success_info;
            entry.failure_func = failure_func;
            entry.failure_info = failure_info;
            tig_idxtable_set(&multiplayer_lock_requests, multiplayer_next_lock_request_id, &entry);

            pkt.type = 80;
            pkt.item_oid = oid;
            pkt.parent_oid = parent_oid;
            pkt.idx = multiplayer_next_lock_request_id;
            pkt.request_mode = 1;
            tig_net_send_app_all(&pkt, sizeof(pkt));

            multiplayer_next_lock_request_id++;
        }
    } else {
        if (success_func != NULL) {
            success_func(success_info);
        }
    }
}

// 0x4A33F0
void multiplayer_map_state_transfer(int client_id, int transfer_event_value)
{
    int player;
    int64_t obj;
    char* map_name;
    char path[TIG_MAX_PATH];
    TigFile* stream;
    int func_idx;
    TigFileList file_list;
    unsigned int file_idx;
    char src_path[TIG_MAX_PATH];
    char dst_path[TIG_MAX_PATH];
    TimeEvent timeevent;
    DateTime datetime;

    tig_net_legacy_play_begin_stub();
    map_flush(0);

    for (player = 0; player < 8; player++) {
        if (tig_net_client_is_active(player)
            && multiplayer_player_states[player].character_id.type != OID_TYPE_NULL) {
            mp_object_id_resolve_nullable(multiplayer_player_states[player].character_id, &obj);
            map_precache_sectors(obj_field_int64_get(obj, OBJ_F_LOCATION));
        }
    }

    tig_net_legacy_play_end_stub();

    map_get_name(map_current_map(), &map_name);
    snprintf(path, sizeof(path), "save\\current\\maps\\%s\\xferdata.mp", map_name);

    stream = tig_file_fopen(path, "wb");
    if (stream != NULL) {
        for (func_idx = 0; func_idx < 8; func_idx++) {
            if (multiplayer_map_state_modules[func_idx].save_func != NULL) {
                multiplayer_map_state_modules[func_idx].save_func(stream);
            }
        }
        tig_file_fclose(stream);

        snprintf(path, sizeof(path), "%s\\maps\\%s\\*.*", "Save\\Current", map_name);
        tig_file_list_create(&file_list, path);

        for (file_idx = 0; file_idx < file_list.count; file_idx++) {
            if (strcmp(file_list.entries[file_idx].path, ".") != 0
                && strcmp(file_list.entries[file_idx].path, "..") != 0) {
                snprintf(src_path, sizeof(src_path),
                    "%s\\maps\\%s\\%s",
                    "Save\\Current",
                    map_name,
                    file_list.entries[file_idx].path);
                snprintf(dst_path, sizeof(dst_path),
                    "%s\\maps\\%s\\%s",
                    ".\\data\\temp",
                    map_name,
                    file_list.entries[file_idx].path);
                tig_net_xfer_send_as(src_path, dst_path, client_id, NULL);
            }
        }

        tig_file_list_destroy(&file_list);
        multiplayer_player_files_send(client_id);

        timeevent.type = TIMEEVENT_TYPE_MULTIPLAYER;
        timeevent.params[0].integer_value = 2;
        timeevent.params[1].integer_value = client_id;
        timeevent.params[2].integer_value = transfer_event_value;
        datetime_init(&datetime, 50);
        timeevent_add_delay(&timeevent, &datetime);
    }
}

// 0x4A3660
void multiplayer_player_files_send(int player)
{
    char pattern[TIG_MAX_PATH];
    char path[TIG_MAX_PATH];
    TigFileList file_list;
    unsigned int index;

    snprintf(pattern, sizeof(pattern), "%s\\Players\\*.*", ".\\data\\temp");
    tig_file_list_create(&file_list, pattern);

    for (index = 0; index < file_list.count; index++) {
        if (strcmp(file_list.entries[index].path, ".") != 0
            && strcmp(file_list.entries[index].path, "..") != 0) {
            snprintf(path, sizeof(path),
                "%s\\Players\\%s",
                ".\\data\\temp",
                file_list.entries[index].path);
            tig_net_xfer_send(path, player, NULL);
        }
    }

    tig_file_list_destroy(&file_list);
}

// 0x4A3780
void multiplayer_local_character_files_send(void)
{
    char prefix[40];
    char name[TIG_MAX_PATH];
    char src[TIG_MAX_PATH];
    char dst[TIG_MAX_PATH];
    int player;
    const char* exts[] = {
        ".mpc",
        ".bmp",
        "_b.bmp",
    };
    int index;

    player = tig_net_local_client_id_get();
    objid_id_to_str(prefix, multiplayer_player_states[player].character_id);

    for (index = 0; index < sizeof(exts) / sizeof(exts[0]); index++) {
        snprintf(name, sizeof(name), "%s%s", prefix, exts[index]);
        snprintf(src, sizeof(src), "Players\\%s", name);
        snprintf(dst, sizeof(dst), "%s\\Players", ".\\data\\temp");
        if (!tig_file_is_directory(dst)) {
            tig_file_mkdir(dst);
        }
        snprintf(dst, sizeof(dst), "%s\\Players\\%s", ".\\data\\temp", name);
        tig_file_copy(src, dst);
        tig_net_xfer_send_as(dst, dst, 0, NULL);
    }
}

// 0x4A3890
void multiplayer_character_transfer_hook_stub(void)
{
}

// 0x4A38A0
int multiplayer_client_time_lead_seconds_get(void)
{
    return multiplayer_client_time_lead_seconds;
}

// 0x4A38B0
bool multiplayer_disconnect_with_character_save(bool (*func)(tig_button_handle_t), tig_button_handle_t button_handle)
{
    if (tig_net_is_host()) {
        char oidstr[40];
        char path[TIG_MAX_PATH];

        objid_id_to_str(oidstr, obj_get_id(player_get_local_pc_obj()));
        snprintf(path, sizeof(path), "Players\\%s.mpc", oidstr);
        if (ui_multiplayer_confirmation_1902()) {
            save_char(path, player_get_local_pc_obj());
        }

        if (func != NULL) {
            func(button_handle);
        }

        tig_net_reset_connection();

        return true;
    } else {
        Packet46 pkt;
        TimeEvent timeevent;
        DateTime datetime;

        pkt.type = tig_net_local_client_id_get();
        pkt.player = tig_net_local_client_id_get();
        multiplayer_disconnect_callback_set(func, button_handle);
        tig_net_send_app_all(&pkt, sizeof(pkt));

        timeevent.type = TIMEEVENT_TYPE_MULTIPLAYER;
        timeevent.params[0].integer_value = 3;
        datetime_init(&datetime, 5000);
        timeevent_add_delay(&timeevent, &datetime);

        return true;
    }
}

// 0x4A39D0
void multiplayer_disconnect_callback_set(MultiplayerDisconnectCallback func, tig_button_handle_t button_handle)
{
    multiplayer_disconnect_callback = func;
    multiplayer_disconnect_button_handle = button_handle;
}

// 0x4A39F0
bool save_char(const char* path, int64_t obj)
{
    int scheme;
    const char* scheme_rule;
    const char* scheme_name;
    char oidstr[40];
    char src_path[TIG_MAX_PATH];
    char dst_path[TIG_MAX_PATH];
    unsigned int flags;
    TigFile* stream;
    uint8_t* data;
    int size;

    multiplayer_lock();
    obj_get_id(obj);
    item_inventory_drop_all(obj, true);

    if ((obj_field_int32_get(obj, OBJ_F_FLAGS) & OF_TEXT) != 0) {
        tb_remove(obj);
    }

    if ((obj_field_int32_get(obj, OBJ_F_FLAGS) & OF_TEXT_FLOATER) != 0) {
        tf_remove(obj);
    }

    magictech_object_spells_end(obj);
    anim_object_interrupt_by_priority(obj, PRIORITY_HIGHEST, false, true);

    scheme = auto_level_scheme_get(obj);
    if (scheme != 0) {
        scheme_rule = auto_level_scheme_rule(scheme);
        scheme_name = auto_level_scheme_name(scheme);
    } else {
        scheme_rule = NULL;
        scheme_name = NULL;
    }

    if (scheme_rule == NULL) {
        scheme_rule = "";
    }

    if (scheme_name == NULL) {
        scheme_name = "";
    }

    objid_id_to_str(oidstr, obj_get_id(obj));

    portrait_path(portrait_get(obj), src_path, sizeof(src_path), 32);
    if (src_path[0] != '\0') {
        snprintf(dst_path, sizeof(dst_path), "Players\\%s.bmp", oidstr);
        tig_file_copy(src_path, dst_path);
    }

    portrait_path(portrait_get(obj), src_path, sizeof(src_path), 128);
    if (src_path[0] != '\0') {
        snprintf(dst_path, sizeof(dst_path), "Players\\%s_b.bmp", oidstr);
        tig_file_copy(src_path, dst_path);
    }

    flags = obj_field_int32_get(obj, OBJ_F_PC_FLAGS);
    flags |= OPCF_USE_ALT_DATA;
    obj_field_int32_set(obj, OPCF_USE_ALT_DATA, flags);

    multiplayer_character_state_restore(obj);

    object_serialize_to_memory(&data, &size, obj);
    if (data == NULL || size <= 0) {
        multiplayer_unlock();
        return false;
    }

    stream = tig_file_fopen(path, "wb");
    if (stream == NULL) {
        FREE(data); // FIX: Leak.
        multiplayer_unlock();
        return false;
    }

    do {
        if (tig_file_fwrite(&size, sizeof(size), 1, stream) != 1) {
            break;
        }

        if (tig_file_fwrite(data, size, 1, stream) != 1) {
            break;
        }

        size = (int)strlen(scheme_rule) + 1;
        if (tig_file_fwrite(&size, sizeof(size), 1, stream) != 1) {
            break;
        }

        if (tig_file_fwrite(scheme_rule, sizeof(*scheme_rule), 1, stream) != 1) {
            break;
        }

        size = (int)strlen(scheme_name) + 1;
        if (tig_file_fwrite(&size, sizeof(size), 1, stream) != 1) {
            break;
        }

        if (tig_file_fwrite(scheme_name, sizeof(*scheme_rule), 1, stream) != 1) {
            break;
        }

        multiplayer_level_scheme_set(obj, scheme_rule, scheme_name);
        tig_file_fclose(stream);
        FREE(data);
        multiplayer_unlock();

        return true;
    } while (0);

    tig_file_fclose(stream);

    FREE(data);
    multiplayer_unlock();

    return false;
}

// 0x4A3D00
bool multiplayer_saved_characters_release(bool destroy_objects)
{
    int idx;

    if (destroy_objects) {
        if (multiplayer_saved_characters != NULL) {
            for (idx = 0; idx < multiplayer_saved_character_count; idx++) {
                if (multiplayer_saved_characters[idx] != OBJ_HANDLE_NULL) {
                    object_destroy(multiplayer_saved_characters[idx]);
                }
            }
        }
    }

    if (multiplayer_saved_characters != NULL) {
        FREE(multiplayer_saved_characters);
    }

    multiplayer_saved_characters = NULL;
    multiplayer_saved_character_count = 0;

    return true;
}

// 0x4A3D70
bool multiplayer_saved_characters_load(int64_t** objs_ptr, int* cnt_ptr)
{
    char path[TIG_MAX_PATH];
    TigFileList file_list;
    unsigned int idx;
    int cnt = 0;
    char* name;

    snprintf(path, sizeof(path), "Players\\%s.mpc", "G_*");
    tig_file_list_create(&file_list, path);

    if (multiplayer_saved_characters != NULL) {
        multiplayer_saved_characters_release(true);
    }

    if (file_list.count != 0) {
        multiplayer_saved_characters = (int64_t*)CALLOC(sizeof(*multiplayer_saved_characters), file_list.count);
        *objs_ptr = (int64_t*)CALLOC(sizeof(int64_t), file_list.count);

        for (idx = 0; idx < file_list.count; idx++) {
            snprintf(path, sizeof(path), "Players\\%s", file_list.entries[idx].path);
            if (load_char(path, &((*objs_ptr)[cnt]))) {
                obj_field_string_get((*objs_ptr)[cnt], OBJ_F_PC_PLAYER_NAME, &name);
                FREE(name);
                multiplayer_saved_characters[cnt] = (*objs_ptr)[cnt];
                cnt++;
            } else {
                (*objs_ptr)[cnt] = OBJ_HANDLE_NULL;
                multiplayer_saved_characters[cnt] = OBJ_HANDLE_NULL;
            }
        }

        *cnt_ptr = cnt;
        multiplayer_saved_character_count = cnt;
    } else {
        multiplayer_saved_characters = (int64_t*)CALLOC(sizeof(*multiplayer_saved_characters), 1);
        *multiplayer_saved_characters = OBJ_HANDLE_NULL;
        multiplayer_saved_character_count = 0;

        *objs_ptr = (int64_t*)CALLOC(8, 1);
        *(*objs_ptr) = OBJ_HANDLE_NULL;
        *cnt_ptr = 0;
    }

    tig_file_list_destroy(&file_list);

    return true;
}

// 0x4A3F40
bool load_char(const char* path, int64_t* obj_ptr)
{
    TigFile* stream;
    int size;
    uint8_t* data;

    stream = tig_file_fopen(path, "rb");
    if (stream == NULL) {
        *obj_ptr = OBJ_HANDLE_NULL;
        return false;
    }

    if (tig_file_fread(&size, sizeof(size), 1, stream) != 1
        || size <= 0 || size > tig_file_filelength(stream) - tig_file_ftell(stream)) {
        tig_file_fclose(stream);
        *obj_ptr = OBJ_HANDLE_NULL;
        return false;
    }

    data = MALLOC(size);
    if (data == NULL) {
        tig_file_fclose(stream);
        *obj_ptr = OBJ_HANDLE_NULL;
        return false;
    }

    if (tig_file_fread(data, 1, size, stream) != size) {
        FREE(data);
        tig_file_fclose(stream);
        *obj_ptr = OBJ_HANDLE_NULL;
        return false;
    }

    tig_file_fclose(stream);

    if (!object_deserialize_at_location(data, size, obj_ptr, location_make(1, 1))) {
        FREE(data);
        *obj_ptr = OBJ_HANDLE_NULL;
        return false;
    }

    if (obj_field_int32_get(*obj_ptr, OBJ_F_CRITTER_INVENTORY_NUM) != 0) {
        tig_debug_printf("MP: load_char failure, object has an inventory!\n");
    }

    if (obj_arrayfield_length_get(*obj_ptr, OBJ_F_CRITTER_INVENTORY_LIST_IDX) != 0) {
        tig_debug_printf("MP: load_char failure, object has items in inventory!\n");
    }

    if (obj_arrayfield_length_get(*obj_ptr, OBJ_F_CRITTER_FOLLOWER_IDX) != 0) {
        tig_debug_printf("MP: load_char failure, object has followers!\n");
    }

    FREE(data);

    return true;
}

// 0x4A40D0
bool multiplayer_player_blob_exists(int player)
{
    return multiplayer_character_metadata[player] != NULL;
}

// 0x4A40F0
void multiplayer_player_blob_set(int player, ObjectID oid, int level, void* data, int size)
{
    if (player < 0 || player >= NUM_PLAYERS || size <= 0 || data == NULL
        || (size_t)size > SIZE_MAX - sizeof(MultiplayerCharacterMetadata) - 8) return;
    MultiplayerCharacterMetadata* loaded = MALLOC(sizeof(*loaded) + 8 + (size_t)size);
    if (loaded == NULL) return;
    loaded->oid = oid;
    loaded->level = level;
    loaded->size = size;
    memcpy(loaded + 1, data, size);
    FREE(multiplayer_character_metadata[player]);
    multiplayer_character_metadata[player] = loaded;
}

// 0x4A4180
void* multiplayer_player_blob_data_get(int player)
{
    if (multiplayer_player_blob_exists(player)) {
        return (void*)(multiplayer_character_metadata[player] + 1);
    } else {
        return NULL;
    }
}

// 0x4A41B0
int multiplayer_player_blob_data_size_get(int player)
{
    if (multiplayer_player_blob_exists(player)) {
        return multiplayer_character_metadata[player]->size;
    } else {
        return 0;
    }
}

// 0x4A41E0
ObjectID multiplayer_player_blob_object_id_get(int player)
{
    ObjectID oid;

    if (multiplayer_player_blob_exists(player)) {
        oid = multiplayer_character_metadata[player]->oid;
    } else {
        oid.type = OID_TYPE_NULL;
    }

    return oid;
}

// 0x4A4230
void* multiplayer_player_blob_record_get(int player)
{
    return multiplayer_character_metadata[player];
}

// 0x4A4240
int multiplayer_player_blob_record_size_get(int player)
{
    if (multiplayer_player_blob_exists(player)) {
        // See 0x4A40F0.
        return multiplayer_character_metadata[player]->size + sizeof(MultiplayerCharacterMetadata) + 8;
    } else {
        return 0;
    }
}

// 0x4A4270
void multiplayer_ping_suppress_push(void)
{
    multiplayer_ping_suppress_count++;
}

// 0x4A4280
void multiplayer_ping_suppress_pop(void)
{
    multiplayer_ping_suppress_count--;
}

// 0x4A4320
bool multiplayer_local_character_save(void)
{
    char str[40];
    char path[TIG_MAX_PATH];

    objid_id_to_str(str, obj_get_id(player_get_local_pc_obj()));
    snprintf(path, sizeof(path), "Players\\%s.mpc", str);
    anim_object_interrupt_by_priority(player_get_local_pc_obj(), 6, false, true);
    return save_char(path, player_get_local_pc_obj());
}

// 0x4A43B0
void multiplayer_level_scheme_set(int64_t obj, const char* rule, const char* name)
{
    int index;

    index = multiplayer_level_scheme_get(obj, NULL, 0, NULL, 0);
    if (index == -1) {
        index = multiplayer_level_scheme_next_slot;
        multiplayer_level_scheme_next_slot = (multiplayer_level_scheme_next_slot + 1) % NUM_PLAYERS;
        multiplayer_level_scheme_tbl[index].oid = obj_get_id(obj);
        multiplayer_level_scheme_tbl[index].rule[0] = '\0';
        multiplayer_level_scheme_tbl[index].name[0] = '\0';
    }

    if (rule != NULL) {
        if (rule[0] != '\0') {
            SDL_strlcpy(multiplayer_level_scheme_tbl[index].rule, rule, sizeof(multiplayer_level_scheme_tbl[index].rule));
        } else {
            multiplayer_level_scheme_tbl[index].rule[0] = '\0';
        }
    }

    if (name != NULL) {
        if (name[0] != '\0') {
            SDL_strlcpy(multiplayer_level_scheme_tbl[index].name, name, sizeof(multiplayer_level_scheme_tbl[index].name));
        } else {
            multiplayer_level_scheme_tbl[index].name[0] = '\0';
        }
    }
}

// 0x4A44C0
int multiplayer_level_scheme_get(int64_t obj, char* rule, size_t rule_capacity, char* name, size_t name_capacity)
{
    ObjectID oid;
    int index;

    oid = obj_get_id(obj);
    for (index = 0; index < NUM_PLAYERS; index++) {
        if (objid_is_equal(oid, multiplayer_level_scheme_tbl[index].oid)) {
            if (rule != NULL) {
                SDL_strlcpy(rule, multiplayer_level_scheme_tbl[index].rule, rule_capacity);
            }
            if (name != NULL) {
                SDL_strlcpy(name, multiplayer_level_scheme_tbl[index].name, name_capacity);
            }
            return index;
        }
    }

    return -1;
}

// 0x4A45B0
bool multiplayer_notify_level_scheme_changed(int64_t obj)
{
    int size = 0;
    void* data = NULL;
    int scheme;
    const char* rule;
    const char* name;
    char str[40];
    char path[TIG_MAX_PATH];
    TigFile* stream;

    scheme = auto_level_scheme_get(obj);
    if (scheme == 0) {
        rule = auto_level_scheme_rule(scheme);
        name = auto_level_scheme_name(scheme);
    } else {
        rule = "";
        name = "";
    }

    if (rule == NULL) {
        rule = "";
    }

    if (name == NULL) {
        name = "";
    }

    multiplayer_level_scheme_set(obj, rule, name);
    objid_id_to_str(str, obj_get_id(obj));
    snprintf(path, sizeof(path), "Players\\%s.mpc", str);

    if (tig_file_exists(path, NULL)) {
        stream = tig_file_fopen(path, "rb");
        if (stream != NULL) {
            bool valid = tig_file_fread(&size, sizeof(size), 1, stream) == 1
                && size > 0 && tig_file_count_fits(stream, size, INT_MAX, 1);
            if (valid) {
                data = MALLOC(size);
                valid = data != NULL && tig_file_fread(data, 1, size, stream) == (size_t)size;
            }
            tig_file_fclose(stream);
            if (!valid) {
                FREE(data);
                return false;
            }
        } else {
            return false;
        }

        stream = tig_file_fopen(path, "wb");
        if (stream != NULL) {
            if (tig_file_fwrite(&size, sizeof(size), 1, stream) == 1
                && tig_file_fwrite(data, size, 1, stream) == 1) {
                size = (int)strlen(rule) + 1;
                if (tig_file_fwrite(&size, sizeof(size), 1, stream) == 1) {
                    tig_file_fwrite(rule, sizeof(*rule), size, stream);

                    size = (int)strlen(name) + 1;
                    if (tig_file_fwrite(&size, sizeof(size), 1, stream) == 1) {
                        tig_file_fwrite(name, sizeof(*name), size, stream);
                    }
                }
            }
            tig_file_fclose(stream);
        }

        FREE(data);
        mp_player_rule_packet_send(obj, name, rule);
    }

    return true;
}

// 0x4A47D0
bool multiplayer_level_scheme_rule(int64_t obj, char* str, size_t capacity)
{
    if (str == NULL || capacity == 0) return false;
    str[0] = '\0';
    TigFile* stream;
    char oidstr[40];
    char path[TIG_MAX_PATH];
    int pos;
    int size;

    if (multiplayer_level_scheme_get(obj, str, capacity, NULL, 0) != -1) {
        return true;
    }

    objid_id_to_str(oidstr, obj_get_id(obj));
    snprintf(path, sizeof(path), "Players\\%s.mpc", oidstr);
    if (tig_file_exists(path, NULL)) {
        stream = tig_file_fopen(path, "rb");
        if (stream != NULL) {
            do {
                if (tig_file_fread(&pos, sizeof(pos), 1, stream) != 1) {
                    break;
                }

                if (!tig_file_count_fits(stream, pos, INT_MAX, 1) || tig_file_fseek(stream, pos, SEEK_CUR) != 0) {
                    break;
                }

                if (tig_file_fread(&size, sizeof(size), 1, stream) != 1) {
                    break;
                }

                if (size <= 0 || (size_t)size > capacity
                    || tig_file_fread(str, 1, size, stream) != (size_t)size || str[size - 1] != '\0') {
                    break;
                }

                multiplayer_level_scheme_set(obj, str, NULL);
                tig_file_fclose(stream);
                return true;
            } while (0);

            tig_file_fclose(stream);
        }
    }

    snprintf(path, sizeof(path), ".\\data\\temp\\Players\\%s.mpc", oidstr);
    if (tig_file_exists(path, NULL)) {
        stream = tig_file_fopen(path, "rb");
        if (stream != NULL) {
            do {
                if (tig_file_fread(&pos, sizeof(pos), 1, stream) != 1) {
                    break;
                }

                if (!tig_file_count_fits(stream, pos, INT_MAX, 1) || tig_file_fseek(stream, pos, SEEK_CUR) != 0) {
                    break;
                }

                if (tig_file_fread(&size, sizeof(size), 1, stream) != 1) {
                    break;
                }

                if (size <= 0 || (size_t)size > capacity
                    || tig_file_fread(str, 1, size, stream) != (size_t)size || str[size - 1] != '\0') {
                    break;
                }

                multiplayer_level_scheme_set(obj, str, NULL);
                tig_file_fclose(stream);
                return true;
            } while (0);

            tig_file_fclose(stream);
        }
    }

    *str = '\0';
    return false;
}

// 0x4A49E0
bool multiplayer_level_scheme_name(int64_t obj, char* str, size_t capacity)
{
    if (str == NULL || capacity == 0) return false;
    str[0] = '\0';
    TigFile* stream;
    char oidstr[40];
    char path[TIG_MAX_PATH];
    int pos;
    int size;

    if (multiplayer_level_scheme_get(obj, NULL, 0, str, capacity) != -1) {
        return true;
    }

    objid_id_to_str(oidstr, obj_get_id(obj));
    snprintf(path, sizeof(path), "Players\\%s.mpc", oidstr);
    if (tig_file_exists(path, NULL)) {
        stream = tig_file_fopen(path, "rb");
        if (stream != NULL) {
            do {
                if (tig_file_fread(&pos, sizeof(pos), 1, stream) != 1) {
                    break;
                }

                if (!tig_file_count_fits(stream, pos, INT_MAX, 1) || tig_file_fseek(stream, pos, SEEK_CUR) != 0) {
                    break;
                }

                if (tig_file_fread(&size, sizeof(size), 1, stream) != 1) {
                    break;
                }

                if (!tig_file_count_fits(stream, size, INT_MAX, 1) || tig_file_fseek(stream, size, SEEK_CUR) != 0) {
                    break;
                }

                if (tig_file_fread(&size, sizeof(size), 1, stream) != 1) {
                    break;
                }

                if (size <= 0 || (size_t)size > capacity
                    || tig_file_fread(str, 1, size, stream) != (size_t)size || str[size - 1] != '\0') {
                    break;
                }

                multiplayer_level_scheme_set(obj, NULL, str);
                tig_file_fclose(stream);
                return true;
            } while (0);

            tig_file_fclose(stream);
        }
    }

    snprintf(path, sizeof(path), ".\\data\\temp\\Players\\%s.mpc", oidstr);
    if (tig_file_exists(path, NULL)) {
        stream = tig_file_fopen(path, "rb");
        if (stream != NULL) {
            do {
                if (tig_file_fread(&pos, sizeof(pos), 1, stream) != 1) {
                    break;
                }

                if (!tig_file_count_fits(stream, pos, INT_MAX, 1) || tig_file_fseek(stream, pos, SEEK_CUR) != 0) {
                    break;
                }

                if (tig_file_fread(&size, sizeof(size), 1, stream) != 1) {
                    break;
                }

                if (!tig_file_count_fits(stream, size, INT_MAX, 1) || tig_file_fseek(stream, size, SEEK_CUR) != 0) {
                    break;
                }

                if (tig_file_fread(&size, sizeof(size), 1, stream) != 1) {
                    break;
                }

                if (size <= 0 || (size_t)size > capacity
                    || tig_file_fread(str, 1, size, stream) != (size_t)size || str[size - 1] != '\0') {
                    break;
                }

                multiplayer_level_scheme_set(obj, NULL, str);
                tig_file_fclose(stream);
                return true;
            } while (0);

            tig_file_fclose(stream);
        }
    }

    *str = '\0';
    return false;
}

// 0x4A4C40
bool multiplayer_portrait_path(int64_t obj, int size, char* path)
{
    char str[40];

    objid_id_to_str(str, obj_get_id(obj));

    snprintf(path,
        TIG_MAX_PATH,
        "Players\\%s%s.bmp",
        str,
        size >= 128 ? "_b" : "");
    if (tig_file_exists(path, NULL)) {
        return true;
    }

    snprintf(path,
        TIG_MAX_PATH,
        "%s\\Players\\%s%s.bmp",
        ".\\data\\temp",
        str,
        size >= 128 ? "_b" : "");
    if (tig_file_exists(path, NULL)) {
        return true;
    }

    snprintf(path,
        TIG_MAX_PATH,
        "%s\\Players\\%s%s.bmp",
        "Save\\Current",
        str,
        size >= 128 ? "_b" : "");
    if (tig_file_exists(path, NULL)) {
        return true;
    }

    path[0] = '\0';
    return false;
}

// 0x4A50D0
bool multiplayer_drag_item_hide(int64_t pc_obj, int64_t item_obj)
{
    unsigned int flags;

    if (tig_net_is_active()) {
        int client_id;
        Packet93 pkt;
        int64_t parent_obj;

        if (pc_obj == player_get_local_pc_obj()) {
            client_id = multiplayer_find_slot_from_obj(pc_obj);
            if (client_id != -1) {
                if (multiplayer_item_hide_for_client(client_id, item_obj)) {
                    pkt.type = 93;
                    pkt.client_id = client_id;
                    pkt.active = 0;
                    pkt.oid = obj_get_id(item_obj);
                    tig_net_send_app_all(&pkt, sizeof(pkt));
                }
            }
        }

        ui_update_inven(item_obj);

        if (item_parent(item_obj, &parent_obj)) {
            ui_update_inven(parent_obj);
        }

        return true;
    }

    flags = obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS);
    flags |= OIF_NO_DISPLAY;
    obj_field_int32_set(item_obj, OBJ_F_ITEM_FLAGS, flags);

    return true;
}

// 0x4A51C0
bool multiplayer_drag_item_show(int64_t pc_obj, int64_t item_obj)
{
    unsigned int flags;

    if (tig_net_is_active()) {
        int client_id;
        Packet93 pkt;

        if (pc_obj != player_get_local_pc_obj()) {
            return false;
        }

        client_id = multiplayer_find_slot_from_obj(pc_obj);
        if (client_id == -1) {
            return false;
        }

        if (!multiplayer_hidden_item_release(client_id)) {
            return false;
        }

        pkt.type = 93;
        pkt.client_id = client_id;
        pkt.active = 1;
        pkt.oid = obj_get_id(item_obj);
        tig_net_send_app_all(&pkt, sizeof(pkt));
        return true;
    }

    flags = obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS);
    flags &= ~OIF_NO_DISPLAY;
    obj_field_int32_set(item_obj, OBJ_F_ITEM_FLAGS, flags);

    return true;
}

// 0x4A5290
void multiplayer_hidden_items_reset(void)
{
    multiplayer_dragged_item_objects[0] = OBJ_HANDLE_NULL;
    memmove(&(multiplayer_dragged_item_objects[1]),
        &(multiplayer_dragged_item_objects[0]),
        sizeof(multiplayer_dragged_item_objects) - sizeof(multiplayer_dragged_item_objects[0]));
}

// 0x4A52C0
bool multiplayer_item_hide_for_client(int client_id, int64_t item_obj)
{
    unsigned int flags;

    if (multiplayer_dragged_item_objects[client_id] != OBJ_HANDLE_NULL) {
        return false;
    }

    flags = obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS);
    flags |= OIF_NO_DISPLAY;
    obj_field_int32_set(item_obj, OBJ_F_ITEM_FLAGS, flags);

    multiplayer_dragged_item_objects[client_id] = item_obj;

    return true;
}

// 0x4A5320
bool multiplayer_hidden_item_release(int client_id)
{
    unsigned int flags;

    if (multiplayer_dragged_item_objects[client_id] == OBJ_HANDLE_NULL) {
        return false;
    }

    flags = obj_field_int32_get(multiplayer_dragged_item_objects[client_id], OBJ_F_ITEM_FLAGS);
    flags &= ~OIF_NO_DISPLAY;
    obj_field_int32_set(multiplayer_dragged_item_objects[client_id], OBJ_F_ITEM_FLAGS, flags);

    multiplayer_dragged_item_objects[client_id] = OBJ_HANDLE_NULL;

    return true;
}

// 0x4A5380
void multiplayer_inventory_viewers_reset_legacy(void)
{
    multiplayer_inventory_viewers[0] = OBJ_HANDLE_NULL;
    memmove(&(multiplayer_inventory_viewers[1]),
        &(multiplayer_inventory_viewers[0]),
        sizeof(multiplayer_inventory_viewers) - sizeof(multiplayer_inventory_viewers[0]));
}

// 0x4A53B0
void multiplayer_inventory_viewer_set(int64_t viewer_obj, int64_t inventory_owner_obj)
{
    int client_id;

    if (!multiplayer_is_locked()) {
        Packet97 pkt;

        pkt.type = 97;
        pkt.viewer_id = obj_get_id(viewer_obj);
        if (inventory_owner_obj != OBJ_HANDLE_NULL) {
            pkt.inventory_owner_id = obj_get_id(inventory_owner_obj);
        } else {
            pkt.inventory_owner_id.type = OID_TYPE_NULL;
        }
        tig_net_send_app_all(&pkt, sizeof(pkt));
    }

    client_id = multiplayer_find_slot_from_obj(viewer_obj);
    if (client_id != -1) {
        multiplayer_inventory_viewers[client_id] = inventory_owner_obj;
    }
}

// 0x4A5460
int multiplayer_inventory_viewer_count(int64_t inventory_owner_obj)
{
    int cnt = 0;
    int index;

    for (index = 0; index < NUM_PLAYERS; index++) {
        if (multiplayer_inventory_viewers[index] == inventory_owner_obj) {
            cnt++;
        }
    }

    return cnt;
}

// 0x4A5490
void multiplayer_ping(tig_timestamp_t timestamp)
{
    (void)timestamp;
}

// 0x4A54A0
void multiplayer_active_animations_restart(void)
{
    int index;
    char str[ANIM_ID_STR_SIZE];

    for (index = 0; index < 216; index++) {
        if ((anim_run_info[index].flags & 0x1) != 0) {
            if (!anim_goal_restart(&(anim_run_info[index].id))) {
                // FIXME: Meaningless.
                anim_id_to_str(&(anim_run_info[index].id), str);
            }
        }
    }
}

// 0x4A54E0
void multiplayer_active_spell_events_restore(void)
{
    int index;

    for (index = 0; index < 512; index++) {
        if ((magictech_run_info[index].flags & MAGICTECH_RUN_ACTIVE) != 0) {
            magictech_run_timeevent_restart(index);
        }
    }
}

// 0x4A5510
void multiplayer_flags_set(int client_id, unsigned int flags)
{
    PacketMultiplayerFlagsChange pkt;

    multiplayer_player_states[client_id].flags |= flags & 0xFF00;

    pkt.type = 98;
    pkt.client_id = client_id;
    pkt.flags = multiplayer_player_states[client_id].flags & 0xFF00;
    tig_net_send_app_all(&pkt, sizeof(pkt));
}

// 0x4A5570
void multiplayer_flags_unset(int client_id, unsigned int flags)
{
    PacketMultiplayerFlagsChange pkt;

    multiplayer_player_states[client_id].flags &= ~(flags & 0xFF00);

    pkt.type = 98;
    pkt.client_id = client_id;
    pkt.flags = multiplayer_player_states[client_id].flags & 0xFF00;
    tig_net_send_app_all(&pkt, sizeof(pkt));
}

// 0x4A55D0
unsigned int multiplayer_flags_get(int client_id)
{
    return multiplayer_player_states[client_id].flags & 0xFF00;
}

// 0x4A55F0
int multiplayer_reserved_setting_get(void)
{
    return multiplayer_reserved_setting_value;
}

// 0x4A5600
void multiplayer_reserved_setting_set(int value)
{
    multiplayer_reserved_setting_value = value;
}

// 0x4A5610
void multiplayer_player_preferences_apply(void)
{
    set_always_run(settings_get_value(&settings, ALWAYS_RUN_KEY));
    combat_auto_attack_set(settings_get_value(&settings, AUTO_ATTACK_KEY));
    combat_auto_switch_weapons_set(settings_get_value(&settings, AUTO_SWITCH_WEAPON_KEY));
    set_follower_skills(settings_get_value(&settings, FOLLOWER_SKILLS_KEY));
}

// 0x4A5670
void multiplayer_starting_equipment_grant(int64_t obj)
{
    mes_file_handle_t mes_file;

    if (mes_load("Rules\\AutoEquip.mes", &mes_file)) {
        if ((tig_net_local_server_get_options() & TIG_NET_SERVER_AUTO_EQUIP) == 0) {
            tig_str_parse_set_separator(' ');
            multiplayer_autoequip_gold_grant(obj, mes_file);
            multiplayer_background_items_grant(obj);
            multiplayer_known_schematic_products_grant(obj);
            multiplayer_autoequip_armor_grant(obj, mes_file);
            multiplayer_autoequip_weapons_grant(obj, mes_file);
            multiplayer_autoequip_skill_items_grant(obj, mes_file);
            item_wield_best_all(obj, OBJ_HANDLE_NULL);
        }
    }
}

// 0x4A5710
int multiplayer_autoequip_gold_grant(int64_t obj, mes_file_handle_t mes_file)
{
    int64_t location;
    int level;
    MesFileEntry mes_file_entry;
    int money_amt;
    int64_t money_obj;

    location = obj_field_int64_get(obj, OBJ_F_LOCATION);
    level = stat_level_get(obj, STAT_LEVEL);

    mes_file_entry.num = (level + 9) / 10;
    if (!mes_search(mes_file, &mes_file_entry)) {
        return 0;
    }

    mes_get_msg(mes_file, &mes_file_entry);
    money_amt = atoi(mes_file_entry.str);
    money_amt = background_adjust_money(money_amt, background_get(obj));
    if (money_amt > 0) {
        mp_object_create(BP_GOLD, location, &money_obj);
        obj_field_int32_set(money_obj, OBJ_F_GOLD_QUANTITY, money_amt);
        item_transfer(money_obj, obj);
    }
    return money_amt;
}

// 0x4A57F0
int multiplayer_background_items_grant(int64_t obj)
{
    char buffer[2000];

    background_get_items(buffer, sizeof(buffer), background_get(obj));

    return multiplayer_item_list_create(obj, buffer);
}

// 0x4A5840
int multiplayer_autoequip_skill_items_grant(int64_t obj, mes_file_handle_t mes_file)
{
    int granted_count = 0;
    int level;
    int value;

    level = stat_level_get(obj, STAT_LEVEL);

    value = tech_skill_level(obj, TECH_SKILL_PICK_LOCKS);
    if (value > 0) {
        granted_count += multiplayer_autoequip_entry_grant_unless_owned(obj, mes_file, (value + level + 29) / 30 + 1100);
    }

    value = basic_skill_level(obj, BASIC_SKILL_HEAL);
    if (value > 0) {
        granted_count += multiplayer_autoequip_entry_grant_unless_owned(obj, mes_file, (value + level + 29) / 30 + 1200);
    }

    value = basic_skill_level(obj, BASIC_SKILL_BACKSTAB);
    if (value > 0) {
        granted_count += multiplayer_autoequip_entry_grant_unless_owned(obj, mes_file, (value + level + 29) / 30 + 1300);
    }

    return granted_count;
}

// 0x4A5920
int multiplayer_autoequip_entry_grant_unless_owned(int64_t obj, mes_file_handle_t mes_file, int num)
{
    MesFileEntry mes_file_entry1;
    MesFileEntry mes_file_entry2;

    mes_file_entry1.num = num;
    mes_file_entry2.num = num + 10;

    if (mes_search(mes_file, &mes_file_entry2)) {
        mes_get_msg(mes_file, &mes_file_entry2);
        if (multiplayer_owned_prototype_list_count(obj, mes_file_entry2.str)) {
            return 0;
        }
    }

    if (mes_search(mes_file, &mes_file_entry1)) {
        mes_get_msg(mes_file, &mes_file_entry1);
        return multiplayer_item_list_create(obj, mes_file_entry1.str);
    }

    return 0;
}

// 0x4A59F0
int multiplayer_autoequip_weapons_grant(int64_t obj, mes_file_handle_t mes_file)
{
    int granted_count = 0;
    int level;
    int value;
    MesFileEntry mes_file_entry;

    level = stat_level_get(obj, STAT_LEVEL);

    // Melee Weapons
    value = basic_skill_level(obj, BASIC_SKILL_MELEE);
    if (value > 0) {
        mes_file_entry.num = 600;
        if (mes_search(mes_file, &mes_file_entry)) {
            mes_get_msg(mes_file, &mes_file_entry);
            granted_count += multiplayer_item_list_create(obj, mes_file_entry.str);
        }
        granted_count += multiplayer_autoequip_entry_grant_unless_owned(obj, mes_file, (value + level + 9) / 10 + 600);
    }

    // Bows
    value = basic_skill_level(obj, BASIC_SKILL_BOW);
    if (value > 0) {
        mes_file_entry.num = 700;
        if (mes_search(mes_file, &mes_file_entry)) {
            mes_get_msg(mes_file, &mes_file_entry);
            granted_count += multiplayer_item_list_create(obj, mes_file_entry.str);
        }
        granted_count += multiplayer_autoequip_entry_grant_unless_owned(obj, mes_file, (value + level + 9) / 10 + 700);
    }

    // Throwing
    value = basic_skill_level(obj, BASIC_SKILL_THROWING);
    if (value > 0) {
        mes_file_entry.num = 800;
        if (mes_search(mes_file, &mes_file_entry)) {
            mes_get_msg(mes_file, &mes_file_entry);
            granted_count += multiplayer_item_list_create(obj, mes_file_entry.str);
        }
        granted_count += multiplayer_autoequip_entry_grant_unless_owned(obj, mes_file, (value + level + 9) / 10 + 800);
    }

    // Firearms
    value = tech_skill_level(obj, TECH_SKILL_FIREARMS);
    if (value > 0) {
        mes_file_entry.num = 900;
        if (mes_search(mes_file, &mes_file_entry)) {
            mes_get_msg(mes_file, &mes_file_entry);
            granted_count += multiplayer_item_list_create(obj, mes_file_entry.str);
        }
        granted_count += multiplayer_autoequip_entry_grant_unless_owned(obj, mes_file, (value + level + 9) / 10 + 900);
    }

    if (granted_count != 0) {
        return granted_count;
    }

    // Fail safe weapons (if you didn't get any of the above).
    if (level <= 20) {
        // {6029} Dagger
        mes_file_entry.num = 1001;
    } else {
        if (stat_level_get(obj, STAT_MAGICK_POINTS) >= 75) {
            // {6163} Mage's Staff
            mes_file_entry.num = 1002;
        } else if (stat_level_get(obj, STAT_MAGICK_POINTS) >= 20) {
            // {6054} Magic Dagger
            mes_file_entry.num = 1003;
        } else if (stat_level_get(obj, STAT_TECH_POINTS) >= 75) {
            // {6053} Mechanical Dagger
            mes_file_entry.num = 1004;
        } else {
            // {6051} Quality Dagger
            mes_file_entry.num = 1005;
        }
    }

    mes_get_msg(mes_file, &mes_file_entry);

    return multiplayer_item_list_create(obj, mes_file_entry.str);
}

// 0x4A5CA0
void multiplayer_autoequip_armor_grant(int64_t obj, mes_file_handle_t mes_file)
{
    int64_t loc;
    int race;
    int wearable_armor_size;
    int level;
    int strength;
    int score;
    int num;

    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    race = stat_level_get(obj, STAT_RACE);
    wearable_armor_size = item_armor_size(race);

    switch (wearable_armor_size) {
    case OARF_SIZE_SMALL:
        num = stat_level_get(obj, STAT_MAGICK_POINTS) < 50 ? 400 : 200;
        break;
    case OARF_SIZE_MEDIUM:
        num = stat_level_get(obj, STAT_MAGICK_POINTS) < 50 ? 300 : 100;
        break;
    case OARF_SIZE_LARGE:
        num = 500;
        break;
    default:
        // Should be unreachable.
        assert(0);
    }

    level = stat_level_get(obj, STAT_LEVEL);
    strength = stat_level_get(obj, STAT_STRENGTH);
    score = strength + level - 21;
    if (score < 1) {
        num += 1;
    } else if (score > 50) {
        num += 5;
    } else {
        num += (strength + level - 12) / 10;
    }

    multiplayer_autoequip_entry_grant_unless_owned(obj, mes_file, num);
}

// 0x4A5D80
int multiplayer_owned_prototype_list_count(int64_t obj, char* str)
{
    int cnt = 0;
    int basic_proto;

    while (str != NULL && *str != '\0') {
        while (SDL_isspace(*str)) {
            str++;
        }

        if (*str == '\0') {
            break;
        }

        tig_str_parse_value(&str, &basic_proto);
        if (item_find_matching_prototype_filtered(obj, proto_handle_get(basic_proto), 0)) {
            cnt++;
        }
    }

    return cnt;
}

// 0x4A5E10
int multiplayer_item_list_create(int64_t obj, char* str)
{
    int64_t loc;
    int cnt = 0;
    int basic_proto;
    int64_t item_obj;

    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    while (str != NULL && *str != '\0') {
        while (SDL_isspace(*str)) {
            str++;
        }

        if (*str == '\0') {
            break;
        }

        tig_str_parse_value(&str, &basic_proto);
        mp_object_create(basic_proto, loc, &item_obj);
        item_transfer(item_obj, obj);
        cnt++;
    }

    return cnt;
}

// 0x4A5EE0
bool multiplayer_known_schematic_products_grant(int64_t obj)
{
    int64_t loc;
    int race;
    int wearable_armor_size;
    int tech;
    int degree;
    int schematic;
    SchematicInfo schematic_info;
    int idx;
    int prod_basic_proto;
    int64_t prod_proto_obj;
    int size;
    int qty;
    int64_t item_obj;

    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    race = stat_level_get(obj, STAT_RACE);
    wearable_armor_size = item_armor_size(race);

    for (tech = 0; tech < TECH_COUNT; tech++) {
        degree = tech_degree_get(obj, tech);
        if (degree > 0) {
            schematic = tech_schematic_get(tech, degree);
            ui_schematic_info_get(schematic, &schematic_info);

            for (idx = 0; idx < 3; idx++) {
                prod_basic_proto = schematic_info.prod[idx];
                prod_proto_obj = proto_handle_get(schematic_info.prod[idx]);
                if (obj_field_int32_get(prod_proto_obj, OBJ_F_TYPE) != OBJ_TYPE_ARMOR) {
                    break;
                }
                size = obj_field_int32_get(prod_proto_obj, OBJ_F_ARMOR_FLAGS) & (OARF_SIZE_SMALL | OARF_SIZE_MEDIUM | OARF_SIZE_LARGE);
                if (size == 0 || size == wearable_armor_size) {
                    break;
                }
            }

            if (idx != 3) {
                for (qty = 0; qty < schematic_info.qty; qty++) {
                    mp_object_create(prod_basic_proto, loc, &item_obj);
                    item_transfer(item_obj, obj);
                }
            }
        }
    }

    return true;
}

// 0x4A6010
void multiplayer_character_state_restore(int64_t obj)
{
    unsigned int flags;
    int poison;
    int index;

    object_hp_damage_set(obj, 0);
    critter_fatigue_damage_set(obj, 0);
    object_flags_unset(obj, OF_OFF);
    object_flags_unset(obj, OF_FLAT);

    flags = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS);
    flags &= ~0xFC03D0FF;
    obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS, flags);

    flags = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2);
    flags &= ~0x3FFFFF;
    obj_field_int32_set(obj, OBJ_F_CRITTER_FLAGS2, flags);

    if (combat_critter_is_combat_mode_active(obj)) {
        combat_critter_deactivate_combat_mode(obj);
    }

    poison = stat_base_get(obj, STAT_POISON_LEVEL);
    if (poison != 0) {
        stat_base_set(obj, STAT_POISON_LEVEL, poison);
    }

    for (index = 0; index < 10; index++) {
        if (index < 0 || index > 2) {
            effect_remove_one_caused_by(obj, index);
        }
    }

    obj_field_reset(obj, OBJ_F_PC_QUEST_IDX);
    obj_field_reset(obj, OBJ_F_PC_RUMOR_IDX);
    obj_field_reset(obj, OBJ_F_PC_BLESSING_IDX);
    obj_field_reset(obj, OBJ_F_PC_BLESSING_TS_IDX);
    obj_field_reset(obj, OBJ_F_PC_CURSE_IDX);
    obj_field_reset(obj, OBJ_F_PC_CURSE_TS_IDX);
    obj_field_reset(obj, OBJ_F_PC_REPUTATION_IDX);
    obj_field_reset(obj, OBJ_F_PC_REPUTATION_TS_IDX);
    obj_field_reset(obj, OBJ_F_CRITTER_FOLLOWER_IDX);
    obj_field_reset(obj, OBJ_F_PC_GLOBAL_FLAGS);
    obj_field_reset(obj, OBJ_F_PC_GLOBAL_VARIABLES);
    obj_field_reset(obj, OBJ_F_SPELL_FLAGS);

    for (index = 0; index < 7; index++) {
        object_overlay_set(obj, OBJ_F_OVERLAY_FORE, index, TIG_ART_ID_INVALID);
        object_overlay_set(obj, OBJ_F_OVERLAY_BACK, index, TIG_ART_ID_INVALID);
    }
}

// 0x4A6190
bool multiplayer_damage_is_disallowed(int64_t attacker_obj, int64_t target_obj, int64_t target_leader_obj, int64_t attacker_leader_obj)
{
    if ((tig_net_local_server_get_options() & TIG_NET_SERVER_PLAYER_KILLING) == 0
        && attacker_obj != target_obj) {
        if (attacker_obj != OBJ_HANDLE_NULL
            && obj_field_int32_get(attacker_obj, OBJ_F_TYPE) == OBJ_TYPE_PC
            && target_obj != OBJ_HANDLE_NULL) {
            if (obj_field_int32_get(target_obj, OBJ_F_TYPE) == OBJ_TYPE_PC) {
                return true;
            }

            if (obj_field_int32_get(target_leader_obj, OBJ_F_TYPE) == OBJ_TYPE_PC) {
                return true;
            }
        }

        if (attacker_leader_obj != OBJ_HANDLE_NULL
            && obj_field_int32_get(attacker_leader_obj, OBJ_F_TYPE) == OBJ_TYPE_PC
            && target_obj != OBJ_HANDLE_NULL) {
            if (obj_field_int32_get(target_obj, OBJ_F_TYPE) == OBJ_TYPE_PC) {
                return true;
            }

            if (obj_field_int32_get(target_leader_obj, OBJ_F_TYPE) == OBJ_TYPE_PC) {
                return true;
            }
        }
    }

    if ((tig_net_local_server_get_options() & TIG_NET_SERVER_FRIENDLY_FIRE) == 0
        && attacker_obj != target_obj) {
        if (attacker_obj != OBJ_HANDLE_NULL
            && (obj_field_int32_get(attacker_obj, OBJ_F_TYPE) == OBJ_TYPE_PC
                || obj_field_int32_get(attacker_obj, OBJ_F_TYPE) == OBJ_TYPE_NPC)) {
            if (target_obj != OBJ_HANDLE_NULL
                && (obj_field_int32_get(target_obj, OBJ_F_TYPE) == OBJ_TYPE_PC
                    || obj_field_int32_get(target_obj, OBJ_F_TYPE) == OBJ_TYPE_NPC)
                && critter_party_same(attacker_obj, target_obj)) {
                return true;
            }

            if (target_leader_obj != OBJ_HANDLE_NULL
                && (obj_field_int32_get(target_leader_obj, OBJ_F_TYPE) == OBJ_TYPE_PC
                    || obj_field_int32_get(target_leader_obj, OBJ_F_TYPE) == OBJ_TYPE_NPC)
                && critter_party_same(attacker_obj, target_leader_obj)) {
                return true;
            }
        }

        if (attacker_leader_obj != OBJ_HANDLE_NULL
            && (obj_field_int32_get(attacker_leader_obj, OBJ_F_TYPE) == OBJ_TYPE_PC
                || obj_field_int32_get(attacker_leader_obj, OBJ_F_TYPE) == OBJ_TYPE_NPC)) {
            if (target_obj != OBJ_HANDLE_NULL
                && (obj_field_int32_get(target_obj, OBJ_F_TYPE) == OBJ_TYPE_PC
                    || obj_field_int32_get(target_obj, OBJ_F_TYPE) == OBJ_TYPE_NPC)
                && critter_party_same(attacker_obj, target_obj)) {
                return true;
            }

            if (target_leader_obj != OBJ_HANDLE_NULL
                && (obj_field_int32_get(target_leader_obj, OBJ_F_TYPE) == OBJ_TYPE_PC
                    || obj_field_int32_get(target_leader_obj, OBJ_F_TYPE) == OBJ_TYPE_NPC)
                && critter_party_same(attacker_obj, target_leader_obj)) {
                return true;
            }
        }
    }

    return false;
}

// 0x4A6470
bool multiplayer_character_save_confirm(int64_t pc_obj)
{
    char oidstr[40];
    char path[TIG_MAX_PATH];
    char str[256];

    objid_id_to_str(oidstr, obj_get_id(pc_obj));
    snprintf(path, sizeof(path), "Players\\%s.mpc", oidstr);
    anim_object_interrupt_by_priority(player_get_local_pc_obj(), PRIORITY_HIGHEST, false, true);

    if (ui_multiplayer_confirmation_1903()) {
        if (tig_file_exists(path, NULL)) {
            object_examine(pc_obj, pc_obj, str);
            if (!ui_multiplayer_named_confirmation(str, oidstr)) {
                return false;
            }
        }

        if (!save_char(path, pc_obj)) {
            return false;
        }
    }

    return true;
}

// 0x4A6560
bool multiplayer_module_guid_suffix_check(const char* module_path, char* normalized_path)
{
    static const struct {
        char ch;
        int pos;
    } meta[7] = {
        { '-', 0 },
        { '{', 1 },
        { '-', 10 },
        { '-', 15 },
        { '-', 20 },
        { '-', 25 },
        { '}', 38 },
    };

    char path[TIG_MAX_PATH];
    char dir[COMPAT_MAX_DIR];
    char fname[COMPAT_MAX_FNAME];
    char ext[COMPAT_MAX_EXT];
    int pos;
    int idx;

    strncpy(path, module_path, sizeof(path));
    compat_splitpath(path, NULL, dir, fname, ext);

    pos = (int)strlen(fname);
    if (pos > 39) {
        for (idx = 0; idx < 7; idx++) {
            if (fname[pos - 39 + meta[idx].pos] != meta[idx].ch) {
                return false;
            }
        }

        if (normalized_path != NULL) {
            fname[pos] = '\0';
            compat_makepath(normalized_path, NULL, dir, fname, ext);
        }

        return true;
    }

    return false;
}
