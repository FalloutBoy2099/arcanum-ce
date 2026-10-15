#include "ui/options_ui.h"

#include "game/anim.h"
#include "game/combat.h"
#include "game/gamelib.h"
#include "game/camera_follow.h"
#include "game/dialog_camera.h"
#include "game/highres_config.h"
#include "game/hrp.h"
#include "game/iso_zoom.h"
#include "ui/intgame.h"
#include "ui/mainmenu_ui.h"
#include "game/location.h"
#include "game/mes.h"
#include "game/name.h"
#include "game/obj.h"
#include "game/player.h"
#include "game/skill.h"
#include "game/snd.h"
#include "game/tf.h"
#include "ui/cyclic_ui.h"
#include "ui/gameuilib.h"

#define MAX_CONTROLS 8
#define MAX_TAB_CONTROLS 32

typedef void (*OptionsUiControlValueGetter)(int* value_ptr, bool* enabled_ptr);
typedef void (*OptionsUiControlValueSetter)(int value);

typedef struct OptionsUiControlInfo {
    /* 0000 */ bool active;
    /* 0004 */ CyclicUiControlType type;
    /* 0008 */ int max_value;
    /* 000C */ const char* debug_name;
    /* 0010 */ const char* mes;
    /* 0014 */ OptionsUiControlValueGetter getter;
    /* 0018 */ OptionsUiControlValueSetter setter;
    /* 001C */ const char* const* text_array;
    /* 0020 */ int text_array_size;
    /* 0024 */ bool force_debug_title;
    int page;
} OptionsUiControlInfo;

typedef struct OptionsUiControlData {
    /* 0000 */ int id;
    /* 0004 */ bool initialized;
} OptionsUiControlData;

static void options_ui_module_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_difficulty_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_difficutly_set(int value);
static void options_ui_violence_filter_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_violence_filter_set(int value);
static void options_ui_combat_mode_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_combat_mode_set(int value);
static void options_ui_auto_attack_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_auto_attack_set(int value);
static void options_ui_auto_switch_weapons_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_auto_switch_weapons_set(int value);
static void options_ui_awlays_run_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_always_run_set(int value);
static void options_ui_follower_skills_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_follower_skills_set(int value);
static void options_ui_brightness_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_brightness_set(int value);
static void options_ui_text_duration_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_text_duration_set(int value);
static void options_ui_floats_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_floats_set(int value);
static void options_ui_float_speed_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_float_speed_set(int value);
static void options_ui_combat_taunts_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_combat_taunts_set(int value);
static void options_ui_main_menu_art_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_main_menu_art_set(int value);
static void options_ui_effects_volume_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_effects_volume_set(int value);
static void options_ui_interface_volume_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_interface_volume_set(int value);
static void options_ui_walking_volume_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_walking_volume_set(int value);
static void options_ui_object_volume_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_object_volume_set(int value);
static void options_ui_spell_volume_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_spell_volume_set(int value);
static void options_ui_critter_volume_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_critter_volume_set(int value);
static void options_ui_voice_volume_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_voice_volume_set(int value);
static void options_ui_music_volume_get(int* value_ptr, bool* enabled_ptr);
static void options_ui_music_volume_set(int value);
static void options_ui_create_page(void);
static void options_ui_destroy_page(void);
static void options_ui_draw_page_heading(void);
static void options_ui_restore_background(TigRect rect);

/**
 * 0x5CCD38
 */
static int options_ui_controls_x[MAX_CONTROLS] = {
    261,
    261,
    261,
    261,
    531,
    531,
    531,
    531,
};

/**
 * 0x5CCD58
 */
static int options_ui_controls_y[MAX_CONTROLS] = {
    122,
    197,
    272,
    347,
    122,
    197,
    272,
    347,
};

static const char* const options_ui_main_menu_art_values[] = { "Noir", "Fondu au noir", "Bois" };
static const char* const options_ui_language_values[] = { "English" };
static const char* const options_ui_hud_values[] = { "Complet", "Moyen", "Mini", "Masqu\351" };
static const char* const options_ui_camera_values[] = { "Minimal", "Dialogue", "PNJ", "Joueur" };
static const char* const options_ui_filter_values[] = { "Pixels nets", "Lisse" };
static const char* options_ui_resolution_values[] = {
    "800x600", "1024x768", "1280x720", "1280x800", "1280x960",
    "1366x768", "1600x900", "1920x1080", "2560x1440", "3840x2160", NULL,
};
static char options_ui_custom_resolution[32];

#define BOOLEAN_OPTION(name, key, invert) \
    static void name##_get(int* value, bool* enabled) { \
        *value = (settings_get_value(&settings, key) != 0) ^ (invert); *enabled = true; \
    } \
    static void name##_set(int value) { settings_set_value(&settings, key, (value != 0) ^ (invert)); }

BOOLEAN_OPTION(options_ui_autosave_travel, AUTOSAVE_TRAVEL_KEY, 0)
BOOLEAN_OPTION(options_ui_autosave_sleep, AUTOSAVE_SLEEP_KEY, 0)
BOOLEAN_OPTION(options_ui_combat_music, NO_COMBAT_MUSIC_KEY, 1)
BOOLEAN_OPTION(options_ui_vsync, VSYNC_MODE_KEY, 0)
BOOLEAN_OPTION(options_ui_borders, SCROLL_USING_BORDERS_KEY, 0)
BOOLEAN_OPTION(options_ui_follow, CAMERA_FOLLOWS_PLAYER_KEY, 0)
BOOLEAN_OPTION(options_ui_zoom, ISO_ZOOM_ENABLED_KEY, 0)
BOOLEAN_OPTION(options_ui_modern_fonts, MODERN_FONTS_KEY, 0)
BOOLEAN_OPTION(options_ui_numbers, DIALOGUE_OPTION_NUMBERS_KEY, 0)
BOOLEAN_OPTION(options_ui_dialogue_zoom, DIALOGUE_CAMERA_ZOOM_TO_FIT_KEY, 0)

static void options_ui_language_get(int* value, bool* enabled)
{
    *value = 0;
    *enabled = false;
}

static void options_ui_hud_get(int* value, bool* enabled)
{
    *value = SDL_clamp(settings_get_value(&settings, HUD_STAGE_KEY), 0, 3);
    *enabled = true;
}

static void options_ui_hud_set(int value)
{
    settings_set_value(&settings, HUD_STAGE_KEY, value);
}

static void options_ui_camera_get(int* value, bool* enabled)
{
    *value = SDL_clamp(settings_get_value(&settings, DIALOGUE_CAMERA_MODE_KEY), 0, 3);
    *enabled = true;
}

static void options_ui_camera_set(int value)
{
    settings_set_value(&settings, DIALOGUE_CAMERA_MODE_KEY, value);
}

static void options_ui_translucent_get(int* value, bool* enabled)
{
    *value = settings_get_value(&settings, TRANSLUCENT_BLACK_UI_KEY) != 0;
    *enabled = true;
}

static void options_ui_translucent_set(int value)
{
    settings_set_value(&settings, TRANSLUCENT_BLACK_UI_KEY, value != 0);
    tig_window_modal_translucent_black_set(value != 0, 8, 128);
}

static void options_ui_filter_get(int* value, bool* enabled)
{
    const char* filter = settings_get_str_value(&settings, ZOOM_FILTER_KEY);
    *value = filter == NULL || SDL_strcasecmp(filter, "nearest") != 0;
    *enabled = true;
}

static void options_ui_filter_set(int value)
{
    settings_set_str_value(&settings, ZOOM_FILTER_KEY, value ? "linear" : "nearest");
    gamelib_invalidate_rect(NULL);
}

static void options_ui_resolution_get(int* value, bool* enabled)
{
    const char* saved = settings_get_str_value(&settings, "resolution");
    *value = 0;
    *enabled = true;
    if (saved == NULL || saved[0] == '\0') {
        SDL_snprintf(options_ui_custom_resolution, sizeof(options_ui_custom_resolution),
            "%dx%d", hrp_iso_window_width_get(), hrp_iso_window_height_get());
        saved = options_ui_custom_resolution;
    }
    for (int i = 0; i < SDL_arraysize(options_ui_resolution_values); i++) {
        if (options_ui_resolution_values[i] != NULL && SDL_strcmp(saved, options_ui_resolution_values[i]) == 0) {
            *value = i;
            return;
        }
    }
    *value = SDL_arraysize(options_ui_resolution_values) - 1;
}

static void options_ui_resolution_set(int value)
{
    if (value >= 0 && value < SDL_arraysize(options_ui_resolution_values)
        && options_ui_resolution_values[value] != NULL) {
        settings_set_str_value(&settings, "resolution", options_ui_resolution_values[value]);
    }
}

/**
 * Defines controls metadata.
 *
 * 0x5CCD78
 */
static OptionsUiControlInfo options_ui_tab_controls_meta[OPTIONS_UI_TAB_COUNT][MAX_TAB_CONTROLS] = {
    {
        { true, CYCLIC_UI_CONTROL_TEXT_ARRAY, 0, "Module", "", options_ui_module_get, NULL, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_TEXT_ARRAY, 0, "Langue", "", options_ui_language_get, NULL, options_ui_language_values, SDL_arraysize(options_ui_language_values), true, 0 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "Difficult\351", "mes\\OptionsDifficulty.mes", options_ui_difficulty_get, options_ui_difficutly_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "Filtre anti-violence", "mes\\OptionsOffOn.mes", options_ui_violence_filter_get, options_ui_violence_filter_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "Mode de combat par d\351faut", "mes\\OptionsCombatMode.mes", options_ui_combat_mode_get, options_ui_combat_mode_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "Attaque automatique", "mes\\OptionsOffOn.mes", options_ui_auto_attack_get, options_ui_auto_attack_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "R\351armement automatique", "mes\\OptionsOffOn.mes", options_ui_auto_switch_weapons_get, options_ui_auto_switch_weapons_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "Courir constamment", "mes\\OptionsOffOn.mes", options_ui_awlays_run_get, options_ui_always_run_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "Comp\351tence des compagnons", "mes\\OptionsOffOn.mes", options_ui_follower_skills_get, options_ui_follower_skills_set, NULL, 0, true, 1 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "Sauvegarde apr\350s voyage", "mes\\OptionsOffOn.mes", options_ui_autosave_travel_get, options_ui_autosave_travel_set, NULL, 0, true, 1 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "Sauvegarde apr\350s sommeil", "mes\\OptionsOffOn.mes", options_ui_autosave_sleep_get, options_ui_autosave_sleep_set, NULL, 0, true, 1 },
    },
    {
        { true, CYCLIC_UI_CONTROL_TEXT_ARRAY, 0, "R\351solution", "", options_ui_resolution_get, options_ui_resolution_set, options_ui_resolution_values, SDL_arraysize(options_ui_resolution_values), true, 0 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "Synchronisation verticale", "mes\\OptionsOffOn.mes", options_ui_vsync_get, options_ui_vsync_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_NUMERIC_BAR, 9, "Luminosit\351", "", options_ui_brightness_get, options_ui_brightness_set, NULL, 0, true, 0 },
        { 0 }, // Intentionally empty fourth slot.
        { true, CYCLIC_UI_CONTROL_NUMERIC_BAR, 0, "Dur\351e d'affichage du texte", "", options_ui_text_duration_get, options_ui_text_duration_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "Messages flottants", "mes\\OptionsFloats.mes", options_ui_floats_get, options_ui_floats_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_NUMERIC_BAR, 0, "Vitesse des messages", "", options_ui_float_speed_get, options_ui_float_speed_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "Commentaires", "mes\\OptionsOffOn.mes", options_ui_combat_taunts_get, options_ui_combat_taunts_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "D\351filement bordures", "mes\\OptionsOffOn.mes", options_ui_borders_get, options_ui_borders_set, NULL, 0, true, 1 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "Cam\351ra suit le joueur", "mes\\OptionsOffOn.mes", options_ui_follow_get, options_ui_follow_set, NULL, 0, true, 1 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "Utilisation du zoom", "mes\\OptionsOffOn.mes", options_ui_zoom_get, options_ui_zoom_set, NULL, 0, true, 1 },
        { true, CYCLIC_UI_CONTROL_TEXT_ARRAY, 0, "Filtre du zoom", "", options_ui_filter_get, options_ui_filter_set, options_ui_filter_values, SDL_arraysize(options_ui_filter_values), true, 1 },
    },
    {
        { true, CYCLIC_UI_CONTROL_NUMERIC_BAR, 0, "Effets", "", options_ui_effects_volume_get, options_ui_effects_volume_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_NUMERIC_BAR, 0, "Interface", "", options_ui_interface_volume_get, options_ui_interface_volume_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_NUMERIC_BAR, 0, "Pas", "", options_ui_walking_volume_get, options_ui_walking_volume_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_NUMERIC_BAR, 0, "Objets", "", options_ui_object_volume_get, options_ui_object_volume_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_NUMERIC_BAR, 0, "Sorts", "", options_ui_spell_volume_get, options_ui_spell_volume_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_NUMERIC_BAR, 0, "Cr\351atures", "", options_ui_critter_volume_get, options_ui_critter_volume_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_NUMERIC_BAR, 0, "Voix", "", options_ui_voice_volume_get, options_ui_voice_volume_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_NUMERIC_BAR, 0, "Musique", "", options_ui_music_volume_get, options_ui_music_volume_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "Musique des combats", "mes\\OptionsOffOn.mes", options_ui_combat_music_get, options_ui_combat_music_set, NULL, 0, true, 1 },
    },
    {
        { true, CYCLIC_UI_CONTROL_TEXT_ARRAY, 0, "Fond menu", "", options_ui_main_menu_art_get, options_ui_main_menu_art_set, options_ui_main_menu_art_values, SDL_arraysize(options_ui_main_menu_art_values), true, 0 },
        { true, CYCLIC_UI_CONTROL_TEXT_ARRAY, 0, "Type d'interface", "", options_ui_hud_get, options_ui_hud_set, options_ui_hud_values, SDL_arraysize(options_ui_hud_values), true, 0 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "Transparence fen\352tre", "mes\\OptionsOffOn.mes", options_ui_translucent_get, options_ui_translucent_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "Police moderne", "mes\\OptionsOffOn.mes", options_ui_modern_fonts_get, options_ui_modern_fonts_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "Num\351roter les dialogues", "mes\\OptionsOffOn.mes", options_ui_numbers_get, options_ui_numbers_set, NULL, 0, true, 0 },
        { true, CYCLIC_UI_CONTROL_TEXT_ARRAY, 0, "Param\351trage cam\351ra", "", options_ui_camera_get, options_ui_camera_set, options_ui_camera_values, SDL_arraysize(options_ui_camera_values), true, 0 },
        { true, CYCLIC_UI_CONTROL_MESSAGE_FILE, 0, "Cam\351ra zoom progressif", "mes\\OptionsOffOn.mes", options_ui_dialogue_zoom_get, options_ui_dialogue_zoom_set, NULL, 0, true, 0 },
    }
};

/**
 * Flag indicating if the options UI is displayed in-game.
 *
 * This flag controls availability of the "Module" control.
 *
 * 0x687260
 */
static bool options_ui_in_play;

/**
 * List of available game modules.
 *
 * 0x687268
 */
static GameModuleList options_ui_modlist;

/**
 * "options_ui.mes"
 *
 * 0x687274
 */
static mes_file_handle_t options_ui_mes_file;

/**
 * Runtime data for controls.
 *
 * 0x687278
 */
static OptionsUiControlData options_ui_controls[MAX_CONTROLS];

/**
 * Flag indicating whether the options UI has been started.
 *
 * 0x6872B8
 */
static bool options_ui_started;

/**
 * Flag indicating if the module list is initialized.
 *
 * 0x6872BC
 */
static bool options_ui_modlist_initialized;

static OptionsUiTab options_ui_tab;
static tig_window_handle_t options_ui_window;
static int options_ui_page;
static int options_ui_page_count;
static tig_font_handle_t options_ui_page_font;
static tig_button_handle_t options_ui_page_buttons[2] = {
    TIG_BUTTON_HANDLE_INVALID,
    TIG_BUTTON_HANDLE_INVALID,
};

/**
 * Opens the options UI at a specified tab.
 *
 * NOTE: This function and entire module is a little bit strange, it's
 * implementation does not match other UI modules. This function does not create
 * its own window to display options, instead the window handle is passed by the
 * caller. The caller is responsible for drawing the background. This function
 * only deals with controls.
 *
 * 0x589280
 */
void options_ui_start(OptionsUiTab tab, tig_window_handle_t window_handle, bool in_play)
{
    int index;
    TigButtonData button_data = { 0 };
    TigFont font_desc = { 0 };

    // Close the previous options tab if it is still visible.
    if (options_ui_started) {
        options_ui_close();
    }

    const char* saved_resolution = settings_get_str_value(&settings, "resolution");
    if (saved_resolution != NULL && saved_resolution[0] != '\0') {
        SDL_strlcpy(options_ui_custom_resolution, saved_resolution, sizeof(options_ui_custom_resolution));
    } else {
        SDL_snprintf(options_ui_custom_resolution, sizeof(options_ui_custom_resolution),
            "%dx%d", hrp_iso_window_width_get(), hrp_iso_window_height_get());
    }
    options_ui_resolution_values[SDL_arraysize(options_ui_resolution_values) - 1] = options_ui_custom_resolution;

    options_ui_in_play = in_play;
    options_ui_tab = tab;
    options_ui_window = window_handle;
    options_ui_page = 0;
    options_ui_page_count = 1;
    for (index = 0; index < MAX_TAB_CONTROLS; index++) {
        OptionsUiControlInfo* meta = &options_ui_tab_controls_meta[tab][index];
        if (meta->active && meta->page >= options_ui_page_count) {
            options_ui_page_count = meta->page + 1;
        }
    }

    // Load control titles.
    mes_load("mes\\options_ui.mes", &options_ui_mes_file);

    if (tab == OPTIONS_UI_TAB_GAME && !in_play) {
        gamelib_modlist_create(&options_ui_modlist, 0);
        options_ui_modlist_initialized = true;
    }

    tig_art_interface_id_create(27, 0, 0, 0, &font_desc.art_id);
    font_desc.flags = TIG_FONT_CENTERED;
    font_desc.color = tig_color_make(255, 255, 255);
    tig_font_create(&font_desc, &options_ui_page_font);

    button_data.flags = TIG_BUTTON_MOMENTARY;
    button_data.window_handle = window_handle;
    button_data.y = 59;
    button_data.mouse_down_snd_id = SND_INTERFACE_BUTTON_MEDIUM;
    button_data.mouse_up_snd_id = SND_INTERFACE_BUTTON_MEDIUM_RELEASE;
    button_data.mouse_enter_snd_id = -1;
    button_data.mouse_exit_snd_id = -1;
    if (options_ui_page_count > 1) {
        for (index = 0; index < 2; index++) {
            TigArtAnimData anim_data;
            tig_art_interface_id_create(index == 0 ? NAME_OPTIONS_MENU_LEFT_BUTTON : NAME_OPTIONS_MENU_RIGHT_BUTTON,
                0, 0, 0, &button_data.art_id);
            if (tig_art_anim_data(button_data.art_id, &anim_data) == TIG_OK
                && anim_data.num_frames >= 3) {
                button_data.x = index == 0 ? 286 : 648;
                button_data.y = 62;
            } else {
                button_data.x = index == 0 ? 283 : 653;
                button_data.y = 59;
                tig_art_interface_id_create(index == 0 ? 272 : 273, 0, 0, 0, &button_data.art_id);
            }
            tig_button_create(&button_data, &options_ui_page_buttons[index]);
        }
    }

    options_ui_create_page();

    // Keep the PC lens centered when opening options during play.
    if (options_ui_in_play) {
        location_origin_set(obj_field_int64_get(player_get_local_pc_obj(), OBJ_F_LOCATION));
    }
    options_ui_started = true;
}

static void options_ui_create_page(void)
{
    int index;
    int slot = 0;
    CyclicUiControlInfo control_info;
    OptionsUiControlInfo* meta;
    MesFileEntry mes_file_entry;
    int value;

    for (index = 0; index < MAX_TAB_CONTROLS; index++) {
        meta = &(options_ui_tab_controls_meta[options_ui_tab][index]);
        if (!meta->active || meta->page != options_ui_page) {
            continue;
        }

        slot = index % MAX_CONTROLS;

        // Initialize cyclic UI control.
        cyclic_ui_control_init(&control_info);

        // Special case - the "Module" control must be turned off when options
        // are opened in the middle of the game.
        if (options_ui_tab == OPTIONS_UI_TAB_GAME && index == 0) {
            if (options_ui_in_play) {
                continue;
            }

            // Use the module list retained while navigating within this tab.
            control_info.text_array = (const char* const*)options_ui_modlist.paths;
            control_info.text_array_size = options_ui_modlist.count;
        }

        if (meta->type == CYCLIC_UI_CONTROL_TEXT_ARRAY && meta->text_array != NULL) {
            control_info.text_array = meta->text_array;
            control_info.text_array_size = meta->text_array_size;
            if (meta->text_array == options_ui_resolution_values) {
                for (int i = 0; i < meta->text_array_size - 1; i++) {
                    if (SDL_strcmp(options_ui_custom_resolution, options_ui_resolution_values[i]) == 0) {
                        control_info.text_array_size--;
                        break;
                    }
                }
            }
        }

        // Set up control.
        control_info.type = meta->type;
        control_info.window_handle = options_ui_window;
        control_info.max_value = meta->max_value;

        // Load control title from the message file.
        mes_file_entry.num = index + 100 * (options_ui_tab + 1);
        control_info.title = !meta->force_debug_title && options_ui_tab != OPTIONS_UI_TAB_AUDIO && mes_search(options_ui_mes_file, &mes_file_entry)
            ? mes_file_entry.str
            : meta->debug_name;

        // Finish control set up.
        control_info.mes_file_path = meta->mes;
        control_info.value_changed_callback = meta->setter;
        control_info.x = options_ui_controls_x[slot];
        control_info.y = options_ui_controls_y[slot];

        // Query initial value.
        if (meta->getter != NULL) {
            meta->getter(&value, &(control_info.enabled));
        } else {
            value = 0;
            control_info.enabled = false;
        }

        // Create the control and set its initial value.
        if (cyclic_ui_control_create(&control_info, &(options_ui_controls[slot].id))) {
            options_ui_controls[slot].initialized = true;
            cyclic_ui_control_set(options_ui_controls[slot].id, value);
        } else {
            options_ui_controls[slot].initialized = false;
        }
    }
    options_ui_draw_page_heading();
}

static void options_ui_restore_background(TigRect rect)
{
    TigArtBlitInfo blit_info = { 0 };
    TigRect src_rect = rect;

    // The options background starts below the 41-pixel menu top bar.
    src_rect.y -= 41;
    tig_art_interface_id_create(669, 0, 0, 0, &blit_info.art_id);
    blit_info.src_rect = &src_rect;
    blit_info.dst_rect = &rect;
    tig_window_blit_art(options_ui_window, &blit_info);
}

static void options_ui_draw_page_heading(void)
{
    TigRect rect = { 360, 63, 260, 39 };
    char title[32];

    options_ui_restore_background(rect);
    if (options_ui_page_count <= 1) {
        return;
    }
    SDL_snprintf(title, sizeof(title), "Page %d / %d", options_ui_page + 1, options_ui_page_count);
    tig_font_push(options_ui_page_font);
    tig_window_text_write(options_ui_window, title, &rect);
    tig_font_pop();
}

static void options_ui_destroy_page(void)
{
    int index;

    for (index = 0; index < MAX_CONTROLS; index++) {
        if (options_ui_controls[index].initialized) {
            cyclic_ui_control_destroy(options_ui_controls[index].id, false);
            options_ui_controls[index].initialized = false;
        }
    }
}

/**
 * Loads the selected game module.
 *
 * 0x589430
 */
bool options_ui_load_module(void)
{
    unsigned int selected;

    if (!options_ui_started
        || !options_ui_modlist_initialized
        || options_ui_in_play
        || options_ui_page != 0
        || !options_ui_controls[0].initialized) {
        return true;
    }

    selected = cyclic_ui_control_get(options_ui_controls[0].id);
    if (selected == options_ui_modlist.selected) {
        return true;
    }

    if (!gamelib_mod_load(options_ui_modlist.paths[selected])
        || !gameuilib_mod_load()) {
        tig_debug_printf("Can't load module %s\n", options_ui_modlist.paths[selected]);
        return false;
    }

    options_ui_modlist.selected = selected;
    return true;
}

/**
 * Closes the options UI.
 *
 * 0x5894C0
 */
void options_ui_close(void)
{
    int index;

    if (!options_ui_started) {
        return;
    }

    options_ui_destroy_page();
    for (index = 0; index < 2; index++) {
        if (options_ui_page_buttons[index] != TIG_BUTTON_HANDLE_INVALID) {
            tig_button_destroy(options_ui_page_buttons[index]);
            options_ui_page_buttons[index] = TIG_BUTTON_HANDLE_INVALID;
        }
    }
    options_ui_restore_background((TigRect) { 261, 52, 483, 60 });
    tig_font_destroy(options_ui_page_font);

    // Destroy module list.
    if (options_ui_modlist_initialized) {
        gamelib_modlist_destroy(&options_ui_modlist);
        options_ui_modlist_initialized = false;
    }

    mes_unload(options_ui_mes_file);

    options_ui_started = false;
}

/**
 * Handles button press event for the options UI.
 *
 * This function delegates handling to the cyclic UI system. Returns `true`
 * if the specified button is one of left/right buttons (in this case the
 * event should be considered as processed), `false` otherwise.
 *
 * 0x589530
 */
bool options_ui_handle_button_pressed(tig_button_handle_t button_handle)
{
    int direction;
    int page;

    if (!options_ui_started || button_handle == TIG_BUTTON_HANDLE_INVALID) {
        return false;
    }
    for (direction = 0; direction < 2; direction++) {
        if (button_handle == options_ui_page_buttons[direction]) {
            page = options_ui_page + (direction == 0 ? -1 : 1);
            if (page >= 0 && page < options_ui_page_count && options_ui_load_module()) {
                options_ui_destroy_page();
                options_ui_page = page;
                options_ui_create_page();
            }
            return true;
        }
    }

    // Delegate button handling to the cyclic UI system.
    return cyclic_ui_handle_button_pressed(button_handle);
}

/**
 * Called to retrieve the initial value and state of "Module".
 *
 * 0x589540
 */
void options_ui_module_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = options_ui_modlist.selected;
    *enabled_ptr = !options_ui_in_play;
}

/**
 * Called to retrieve the initial value and state of "Difficulty".
 *
 * 0x589560
 */
void options_ui_difficulty_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = settings_get_value(&settings, DIFFICULTY_KEY);
    *enabled_ptr = true;
}

/**
 * Called when the value of "Difficulty" is changed.
 *
 * 0x589580
 */
void options_ui_difficutly_set(int value)
{
    settings_set_value(&settings, DIFFICULTY_KEY, value);
}

/**
 * Called to retrieve the initial value and state of "Violence Filter".
 *
 * 0x5895A0
 */
void options_ui_violence_filter_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = settings_get_value(&settings, VIOLENCE_FILTER_KEY);
    *enabled_ptr = true;
}

/**
 * Called when the value of "Violence Filter" is changed.
 *
 * 0x5895D0
 */
void options_ui_violence_filter_set(int value)
{
    settings_set_value(&settings, VIOLENCE_FILTER_KEY, value ? 1 : 0);
}

/**
 * Called to retrieve the initial value and state of "Combat Mode".
 *
 * 0x589610
 */
void options_ui_combat_mode_get(int* value_ptr, bool* enabled_ptr)
{
    if (combat_is_turn_based()) {
        *value_ptr = settings_get_value(&settings, FAST_TURN_BASED_KEY) ? 2 : 1;
    } else {
        *value_ptr = 0;
    }
    *enabled_ptr = true;
}

/**
 * Called when the value of "Combat Mode" is changed.
 *
 * 0x5896A0
 */
void options_ui_combat_mode_set(int value)
{
    switch (value) {
    case 0:
        settings_set_value(&settings, TURN_BASED_KEY, 0);
        settings_set_value(&settings, FAST_TURN_BASED_KEY, 0);
        break;
    case 1:
        settings_set_value(&settings, TURN_BASED_KEY, 1);
        settings_set_value(&settings, FAST_TURN_BASED_KEY, 0);
        break;
    case 2:
        settings_set_value(&settings, TURN_BASED_KEY, 1);
        settings_set_value(&settings, FAST_TURN_BASED_KEY, 1);
        break;
    }
}

/**
 * Called to retrieve the initial value and state of "Auto Attack".
 *
 * 0x589710
 */
void options_ui_auto_attack_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = combat_auto_attack_get(player_get_local_pc_obj());
    *enabled_ptr = true;
}

/**
 * Called when the value of "Auto Attack" is changed.
 *
 * 0x589740
 */
void options_ui_auto_attack_set(int value)
{
    combat_auto_attack_set(value);
}

/**
 * Called to retrieve the initial value and state of "Auto Switch Weapons".
 *
 * 0x589750
 */
void options_ui_auto_switch_weapons_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = combat_auto_switch_weapons_get(player_get_local_pc_obj());
    *enabled_ptr = true;
}

/**
 * Called when the value of "Auto Switch Weapons" is changed.
 *
 * 0x589780
 */
void options_ui_auto_switch_weapons_set(int value)
{
    combat_auto_switch_weapons_set(value);
}

/**
 * Called to retrieve the initial value and state of "Always Run".
 *
 * 0x589790
 */
void options_ui_awlays_run_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = get_always_run(player_get_local_pc_obj());
    *enabled_ptr = true;
}

/**
 * Called when the value of "Always Run" is changed.
 *
 * 0x5897C0
 */
void options_ui_always_run_set(int value)
{
    set_always_run(value);
}

/**
 * Called to retrieve the initial value and state of "Follower Skills".
 *
 * 0x5897D0
 */
void options_ui_follower_skills_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = get_follower_skills(player_get_local_pc_obj());
    *enabled_ptr = true;
}

/**
 * Called when the value of "Follower Skills" is changed.
 *
 * 0x589800
 */
void options_ui_follower_skills_set(int value)
{
    set_follower_skills(value);
}

/**
 * Called to retrieve the initial value and state of "Brightness".
 *
 * 0x589810
 */
void options_ui_brightness_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = settings_get_value(&settings, BRIGHTNESS_KEY);
    *enabled_ptr = tig_video_check_gamma_control() == TIG_OK;
}

/**
 * Called when the value of "Brightness" is changed.
 *
 * 0x589840
 */
void options_ui_brightness_set(int value)
{
    settings_set_value(&settings, BRIGHTNESS_KEY, value);
}

/**
 * Called to retrieve the initial value and state of "Text Duration".
 *
 * 0x589860
 */
void options_ui_text_duration_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = settings_get_value(&settings, TEXT_DURATION_KEY);
    *enabled_ptr = true;
}

/**
 * Called when the value of "Text Duration" is changed.
 *
 * 0x589880
 */
void options_ui_text_duration_set(int value)
{
    settings_set_value(&settings, TEXT_DURATION_KEY, value);
}

/**
 * Called to retrieve the initial value and state of "Floats".
 *
 * 0x5898A0
 */
void options_ui_floats_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = tf_level_get();
    *enabled_ptr = true;
}

/**
 * Called when the value of "Floats" is changed.
 *
 * 0x5898C0
 */
void options_ui_floats_set(int value)
{
    if (tf_level_get() != value) {
        tf_level_set(value);
    }
}

/**
 * Called to retrieve the initial value and state of "Float Speed".
 *
 * 0x5898E0
 */
void options_ui_float_speed_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = settings_get_value(&settings, FLOAT_SPEED_KEY);
    *enabled_ptr = true;
}

/**
 * Called when the value of "Float Speed" is changed.
 *
 * 0x589900
 */
void options_ui_float_speed_set(int value)
{
    settings_set_value(&settings, FLOAT_SPEED_KEY, value);
}

/**
 * Called to retrieve the initial value and state of "Combat Taunts".
 *
 * 0x589920
 */
void options_ui_combat_taunts_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = settings_get_value(&settings, COMBAT_TAUNTS_KEY);
    *enabled_ptr = true;
}

/**
 * Called when the value of "Combat Taunts" is changed.
 *
 * 0x589940
 */
void options_ui_combat_taunts_set(int value)
{
    settings_set_value(&settings, COMBAT_TAUNTS_KEY, value);
}

void options_ui_main_menu_art_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = settings_get_value(&settings, MAIN_MENU_ART_KEY);
    if (*value_ptr < 0) {
        *value_ptr = 0;
    } else if (*value_ptr > 2) {
        *value_ptr = 2;
    }
    *enabled_ptr = true;
}

void options_ui_main_menu_art_set(int value)
{
    if (value < 0) {
        value = 0;
    } else if (value > 2) {
        value = 2;
    }
    settings_set_value(&settings, MAIN_MENU_ART_KEY, value);
    mainmenu_ui_refresh_background();
}

/**
 * Called to retrieve the initial value and state of "Effects".
 *
 * 0x589960
 */
void options_ui_effects_volume_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = settings_get_value(&settings, EFFECTS_VOLUME_KEY);
    *enabled_ptr = true;
}

/**
 * Called when the value of "Effects" is changed.
 *
 * 0x589980
 */
void options_ui_effects_volume_set(int value)
{
    settings_set_value(&settings, EFFECTS_VOLUME_KEY, value);
}

/**
 * Called to retrieve the initial value and state of "Interface".
 */
void options_ui_interface_volume_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = settings_get_value(&settings, INTERFACE_VOLUME_KEY);
    *enabled_ptr = true;
}

/**
 * Called when the value of "Interface" is changed.
 */
void options_ui_interface_volume_set(int value)
{
    settings_set_value(&settings, INTERFACE_VOLUME_KEY, value);
}

void options_ui_walking_volume_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = settings_get_value(&settings, WALKING_VOLUME_KEY);
    *enabled_ptr = true;
}

void options_ui_walking_volume_set(int value)
{
    settings_set_value(&settings, WALKING_VOLUME_KEY, value);
}

void options_ui_object_volume_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = settings_get_value(&settings, OBJECT_VOLUME_KEY);
    *enabled_ptr = true;
}

void options_ui_object_volume_set(int value)
{
    settings_set_value(&settings, OBJECT_VOLUME_KEY, value);
}

void options_ui_spell_volume_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = settings_get_value(&settings, SPELL_VOLUME_KEY);
    *enabled_ptr = true;
}

void options_ui_spell_volume_set(int value)
{
    settings_set_value(&settings, SPELL_VOLUME_KEY, value);
}

void options_ui_critter_volume_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = settings_get_value(&settings, CRITTER_VOLUME_KEY);
    *enabled_ptr = true;
}

void options_ui_critter_volume_set(int value)
{
    settings_set_value(&settings, CRITTER_VOLUME_KEY, value);
}

/**
 * Called to retrieve the initial value and state of "Voice".
 *
 * 0x5899A0
 */
void options_ui_voice_volume_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = settings_get_value(&settings, VOICE_VOLUME_KEY);
    *enabled_ptr = true;
}

/**
 * Called when the value of "Voice" is changed.
 *
 * 0x5899C0
 */
void options_ui_voice_volume_set(int value)
{
    settings_set_value(&settings, VOICE_VOLUME_KEY, value);
}

/**
 * Called to retrieve the initial value and state of "Music".
 *
 * 0x5899E0
 */
void options_ui_music_volume_get(int* value_ptr, bool* enabled_ptr)
{
    *value_ptr = settings_get_value(&settings, MUSIC_VOLUME_KEY);
    *enabled_ptr = true;
}

/**
 * Called when the value of "Music" is changed.
 *
 * 0x589A00
 */
void options_ui_music_volume_set(int value)
{
    settings_set_value(&settings, MUSIC_VOLUME_KEY, value);
}
