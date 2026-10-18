#ifndef ARCANUM_GAME_MULTIPLAYER_H_
#define ARCANUM_GAME_MULTIPLAYER_H_

#include "game/context.h"
#include "game/obj.h"
#include "game/timeevent.h"

#define MULTIPLAYER_ALWAYS_RUN 0x0100u
#define MULTIPLAYER_AUTO_ATTACK 0x0200u
#define MULTIPLAYER_AUTO_SWITCH_WEAPONS 0x0400u
#define MULTIPLAYER_FOLLOWER_SKILLS 0x0800u

typedef void (*MultiplayerOptionalPlayerCallback)(void);
typedef bool (*MultiplayerDisconnectCallback)(tig_button_handle_t button_handle);

bool multiplayer_init(GameInitInfo* init_info);
void multiplayer_exit(void);
void multiplayer_reset(void);
bool multiplayer_save(TigFile* stream);
bool mutliplayer_load(GameLoadInfo* load_info);
bool multiplayer_mod_load(void);
void multiplayer_mod_unload(void);
bool multiplayer_start(void);
bool multiplayer_end(void);
void multiplayer_server_start(void);
bool multiplayer_host_game_load(const char* a1, const char* a2);
bool multiplayer_timeevent_process(TimeEvent* timeevent);
bool multiplayer_map_open_by_name(const char* name);
void multiplayer_player_callback_set(MultiplayerOptionalPlayerCallback func);
int multiplayer_find_slot_from_obj(int64_t obj);
int64_t multiplayer_active_player_object_get(int player);
bool multiplayer_is_locked(void);
void multiplayer_lock(void);
void multiplayer_unlock(void);
int64_t multiplayer_player_find_first(void);
int64_t multiplayer_player_find_next(void);
void multiplayer_object_lock_request(ObjectID oid, bool (*success_func)(void*), void* success_info, bool (*failure_func)(void*), void* failure_info);
void multiplayer_character_transfer_hook_stub(void);
int multiplayer_client_time_lead_seconds_get(void);
bool multiplayer_disconnect_with_character_save(bool (*func)(tig_button_handle_t), tig_button_handle_t button_handle);
void multiplayer_disconnect_callback_set(MultiplayerDisconnectCallback func, tig_button_handle_t button_handle);
bool multiplayer_saved_characters_release(bool a1);
bool multiplayer_saved_characters_load(int64_t** objs_ptr, int* cnt_ptr);
void multiplayer_player_blob_set(int player, ObjectID oid, int level, void* data, int size);
void* multiplayer_player_blob_data_get(int player);
int multiplayer_player_blob_data_size_get(int player);
ObjectID multiplayer_player_blob_object_id_get(int player);
void* multiplayer_player_blob_record_get(int player);
int multiplayer_player_blob_record_size_get(int player);
void multiplayer_ping_suppress_push(void);
void multiplayer_ping_suppress_pop(void);
bool multiplayer_local_character_save(void);
bool multiplayer_notify_level_scheme_changed(int64_t obj);
bool multiplayer_level_scheme_rule(int64_t obj, char* str);
bool multiplayer_level_scheme_name(int64_t obj, char* str);
bool multiplayer_portrait_path(int64_t obj, int size, char* path);
bool multiplayer_drag_item_hide(int64_t pc_obj, int64_t item_obj);
bool multiplayer_drag_item_show(int64_t pc_obj, int64_t item_obj);
int multiplayer_inventory_viewer_count(int64_t inventory_owner_obj);
void multiplayer_ping(tig_timestamp_t timestamp);
void multiplayer_inventory_viewer_set(int64_t viewer_obj, int64_t inventory_owner_obj);
void multiplayer_flags_set(int client_id, unsigned int flags);
void multiplayer_flags_unset(int client_id, unsigned int flags);
unsigned int multiplayer_flags_get(int client_id);
int multiplayer_reserved_setting_get(void);
void multiplayer_reserved_setting_set(int value);
void multiplayer_player_preferences_apply(void);
bool multiplayer_damage_is_disallowed(int64_t a1, int64_t a2, int64_t a3, int64_t a4);
bool multiplayer_character_save_confirm(int64_t pc_obj);

#endif /* ARCANUM_GAME_MULTIPLAYER_H_ */
