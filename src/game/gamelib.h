#ifndef ARCANUM_GAME_GAMELIB_H_
#define ARCANUM_GAME_GAMELIB_H_

#include "game/context.h"
#include "game/settings.h"
#include "game/timeevent.h"

#define DIFFICULTY_KEY "difficulty"
#define VIOLENCE_FILTER_KEY "violence_filter"
#define TURN_BASED_KEY "turn_based"
#define FAST_TURN_BASED_KEY "fast_turn_based"
#define AUTO_ATTACK_KEY "auto_attack"
#define AUTO_SWITCH_WEAPON_KEY "auto_switch"
#define ALWAYS_RUN_KEY "always_run"
#define FOLLOWER_SKILLS_KEY "follower_skills"
#define FOLLOWERS_MANUALLY_LEVEL_UP_KEY "followers_manually_level_up"

#define BRIGHTNESS_KEY "brightness"
#define TEXT_DURATION_KEY "text_duration"
#define TEXT_FLOATERS_KEY "text_floaters"
#define FLOAT_SPEED_KEY "float_speed"
#define COMBAT_TAUNTS_KEY "combat_taunts"
#define DIALOG_FONT_KEY "dialog_font"
#define LOGBOOK_FONT_KEY "logbook_font"
#define MAIN_MENU_ART_KEY "main_menu_art"
#define MENU_BORDERS_KEY "menu_borders"
#define DIALOG_OPTIONS_MIN_KEY "dialog_options_min"
#define DIALOG_OPTIONS_MAX_KEY "dialog_options_max"

#define EFFECTS_VOLUME_KEY "effects_volume"
#define INTERFACE_VOLUME_KEY "interface_volume"
#define WALKING_VOLUME_KEY "walking_volume"
#define OBJECT_VOLUME_KEY "object_volume"
#define SPELL_VOLUME_KEY "spell_volume"
#define CRITTER_VOLUME_KEY "critter_volume"
#define VOICE_VOLUME_KEY "voice_volume"
#define MUSIC_VOLUME_KEY "music_volume"
#define NO_COMBAT_MUSIC_KEY "no_combat_music"

#define SPLASH_KEY "splash"
#define SHOW_VERSION_KEY "show_version"
#define OBJECT_LIGHTING_KEY "object_lighting"
#define SHADOWS_KEY "shadows"

typedef bool (*GameExtraSaveFunc)(void);
typedef bool (*GameExtraLoadFunc)(void);

typedef enum GameDifficulty {
    GAME_DIFFICULTY_EASY,
    GAME_DIFFICULTY_NORMAL,
    GAME_DIFFICULTY_HARD,
} GameDifficulty;

typedef struct GameModuleList {
    unsigned int count;
    unsigned int selected;
    char** paths;
} GameModuleList;

typedef struct GameSaveList {
    char* module;
    unsigned int count;
    char** names;
} GameSaveList;

typedef enum GameSaveListOrder {
    GAME_SAVE_LIST_ORDER_DATE,
    GAME_SAVE_LIST_ORDER_NAME,
} GameSaveListOrder;

typedef struct GameSaveInfo {
    /* 0000 */ int version;
    /* 0004 */ char name[256];
    /* 0104 */ char module_name[256];
    /* 0204 */ int pc_portrait;
    /* 0208 */ int pc_level;
    /* 020C */ int field_20C;
    /* 0210 */ int64_t pc_location;
    /* 0218 */ char description[256];
    /* 0318 */ TigVideoBuffer* thumbnail_video_buffer;
    /* 031C */ int field_31C;
    /* 0320 */ DateTime datetime;
    /* 0328 */ char pc_name[24];
    /* 0340 */ int map;
    /* 0344 */ int field_344;
    /* 0348 */ int field_348;
    /* 034C */ int field_34C;
    /* 0350 */ int field_350;
    /* 0354 */ int field_354;
    /* 0358 */ int field_358;
    /* 035C */ int story_state;
} GameSaveInfo;

extern unsigned int gamelib_ping_time;
extern Settings settings;
extern TigVideoBuffer* gamelib_scratch_video_buffer;

bool gamelib_init(GameInitInfo* init_info);
void gamelib_reset(void);
void gamelib_exit(void);
void gamelib_ping(void);
void gamelib_resize(GameResizeInfo* resize_info);
void gamelib_default_module_name_set(const char* name);
const char* gamelib_default_module_name_get(void);
void gamelib_modlist_create(GameModuleList* module_list, int type);
void gamelib_modlist_destroy(GameModuleList* module_list);
bool gamelib_mod_load(const char* path);
bool gamelib_mod_guid_get(TigGuid* guid_ptr);
void gamelib_mod_unload(void);
bool gamelib_in_reset(void);
int gamelib_cheat_level_get(void);
void gamelib_cheat_level_set(int level);
void gamelib_invalidate_rect(TigRect* rect);
bool gamelib_draw(void);
void gamelib_renderlock_acquire(void);
void gamelib_renderlock_release(void);
void gamelib_clear_screen(void);
const char* gamelib_current_module_name_get(void);
void gamelib_current_mode_name_set(const char* name);
bool gamelib_save(const char* name, const char* description);
bool gamelib_load(const char* name);
bool gamelib_delete(const char* name);
const char* gamelib_last_save_name(void);
bool gamelib_in_save(void);
bool gamelib_in_load(void);
void gamelib_set_extra_save_func(GameExtraSaveFunc func);
void gamelib_set_extra_load_func(GameExtraLoadFunc func);
void gamelib_savelist_create(GameSaveList* save_list);
void gamelib_savelist_create_module(const char* module, GameSaveList* save_list);
void gamelib_savelist_destroy(GameSaveList* save_list);
void gamelib_savelist_sort(GameSaveList* save_list, GameSaveListOrder order, bool a3);
bool gamelib_saveinfo_init(const char* name, const char* description, GameSaveInfo* save_info);
void gamelib_saveinfo_exit(GameSaveInfo* save_info);
bool gamelib_saveinfo_save(GameSaveInfo* save_info);
bool gamelib_saveinfo_load(const char* name, GameSaveInfo* save_info);
void gamelib_thumbnail_size_set(int width, int height);
int gamelib_game_difficulty_get(void);
void gamelib_redraw(void);
bool gamelib_copy_version(char* long_version, char* short_version, char* locale);
void gamelib_patch_lvl_set(const char* patch_lvl);
const char* gamelib_get_locale(void);

#endif /* ARCANUM_GAME_GAMELIB_H_ */
