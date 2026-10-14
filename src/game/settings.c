#include "game/settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/obj_private.h"

static SettingsEntry* settings_find(Settings* settings, const char* key);
static const char* settings_normalize_key(const char* key);
static const char* settings_normalize_section_key(const char* section, const char* key);
static const char* settings_output_key(const char* key);
static bool settings_key_in_save_order(const char* key, const char* const* const* save_order);
static void settings_save_ordered(FILE* stream, Settings* settings, const char* const* const* save_order, const char* const* section_names);
static void settings_trim(char* str);

static const char* const arcanum_ini_global_save_order[] = {
    "language",
    "show_version",
    NULL,
};

static const char* const arcanum_ini_game_save_order[] = {
    "difficulty",
    "violence_filter",
    "turn_based",
    "fast_turn_based",
    "auto_attack",
    "auto_switch",
    "always_run",
    "follower_skills",
    "followers_manually_level_up",
    NULL,
};

static const char* const arcanum_ini_video_save_order[] = {
    "brightness",
    "text_duration",
    "text_floaters",
    "float_speed",
    "combat_taunts",
    "dialog_font",
    "logbook_font",
    "main_menu_art",
    "menu_borders",
    "object_lighting",
    "shadows",
    NULL,
};

static const char* const arcanum_ini_audio_save_order[] = {
    "effects_volume",
    "interface_volume",
    "walking_volume",
    "object_volume",
    "spell_volume",
    "critter_volume",
    "voice_volume",
    "music_volume",
    "no_combat_music",
    NULL,
};

static const char* const arcanum_ini_ui_save_order[] = {
    "dialog_options_min",
    "dialog_options_max",
    NULL,
};

static const char* const arcanum_ini_bindings_save_order[] = {
    "main_menu",
    "options",
    "quick_save",
    "quick_load",
    "screenshot",
    "toggle_compact_ui",
    "binding_always_run",
    "center_camera",
    "",
    "hotkey1",
    "hotkey2",
    "hotkey3",
    "hotkey4",
    "hotkey5",
    "hotkey6",
    "hotkey7",
    "hotkey8",
    "hotkey9",
    "hotkey0",
    "",
    "character",
    "inventory",
    "logbook",
    "map",
    "fate",
    "skills",
    "spells",
    "schematics",
    "sleep",
    "chat",
    "repeat_action",
    "combat_mode",
    "toggle_combat",
    "end_turn",
    "",
    "follower_walk",
    "follower_attack",
    "follower_stay_close",
    "follower_spread_out",
    "follower_stop",
    "",
    "scroll_up",
    "scroll_down",
    "scroll_left",
    "scroll_right",
    NULL,
};

static const char* const arcanum_ini_section_names[] = {
    "global",
    "game",
    "video",
    "audio",
    "ui",
    "bindings",
    NULL,
};

static const char* const* const arcanum_ini_save_order[] = {
    arcanum_ini_global_save_order,
    arcanum_ini_game_save_order,
    arcanum_ini_video_save_order,
    arcanum_ini_audio_save_order,
    arcanum_ini_ui_save_order,
    arcanum_ini_bindings_save_order,
    NULL,
};

static const char* const arcanum_cfg_save_order[] = {
    "splash",
    NULL,
};

typedef struct SettingsKeyAlias {
    const char* legacy;
    const char* current;
} SettingsKeyAlias;

static const SettingsKeyAlias settings_key_aliases[] = {
    { "violence filter", "violence_filter" },
    { "turn-based", "turn_based" },
    { "fast turn-based", "fast_turn_based" },
    { "auto attack", "auto_attack" },
    { "auto switch", "auto_switch" },
    { "always run", "always_run" },
    { "follower skills", "follower_skills" },
    { "followers manually level up", "followers_manually_level_up" },
    { "text duration", "text_duration" },
    { "text floaters", "text_floaters" },
    { "float speed", "float_speed" },
    { "combat taunts", "combat_taunts" },
    { "object lighting", "object_lighting" },
    { "effects volume", "effects_volume" },
    { "interface volume", "interface_volume" },
    { "voice volume", "voice_volume" },
    { "music volume", "music_volume" },
    { "show version", "show_version" },
    { NULL, NULL },
};

/**
 * Initializes a settings structure with a file path.
 *
 * 0x438AE0
 */
void settings_init(Settings* settings, const char* path)
{
    settings->path = path;
    settings->entries = NULL;
    settings->flags = 0;
}

/**
 * Frees all resources associated with a settings structure.
 *
 * 0x438B00
 */
void settings_exit(Settings* settings)
{
    SettingsEntry* curr;
    SettingsEntry* next;

    curr = settings->entries;
    while (curr != NULL) {
        next = curr->next;
        FREE(curr->key);
        FREE(curr->value);
        FREE(curr);
        curr = next;
    }

    settings->path = NULL;
    settings->entries = NULL;
}

/**
 * Loads settings from the associated file.
 *
 * File path is specified at creation time with `settings_init` and should
 * not be changed.
 *
 * 0x438B50
 */
void settings_load(Settings* settings)
{
    settings_load_legacy(settings, settings->path);
}

void settings_load_legacy(Settings* settings, const char* path)
{
    bool exists;
    TigFindFileData find_file_data;
    FILE* stream;
    char buffer[256];
    char* sep;
    char section[64];

    section[0] = '\0';

    // Check if the settings file exists.
    exists = tig_find_first_file(path, &find_file_data);
    tig_find_close(&find_file_data); // FIX: Memory leak.

    if (!exists) {
        return;
    }

    stream = fopen(path, "rt");
    if (stream == NULL) {
        return;
    }

    while (fgets(buffer, sizeof(buffer), stream) != NULL) {
        settings_trim(buffer);
        if (buffer[0] == '[') {
            char* section_end = strchr(buffer, ']');
            if (section_end != NULL) {
                *section_end = '\0';
                SDL_strlcpy(section, buffer + 1, sizeof(section));
                settings_trim(section);
            }
            continue;
        }

        if (buffer[0] == '\0'
            || buffer[0] == '#'
            || buffer[0] == ';') {
            continue;
        }

        // Find the key-value separator.
        sep = strchr(buffer, '=');
        if (sep != NULL) {
            // Split the string into key and value.
            *sep++ = '\0';

            // Trim whitespace from both key and value.
            settings_trim(sep);

            // Only store non-empty key-value pairs.
            if (sep[0] != '\0' && buffer[0] != '\0') {
                settings_set_str_value(settings, settings_normalize_section_key(section, buffer), sep);
            }
        }
    }

    fclose(stream);

    // Loading existing files must not force an immediate rewrite.
    settings->flags &= ~SETTINGS_CHANGED;
}

/**
 * Saves settings to the associated file.
 *
 * 0x438C20
 */
void settings_save(Settings* settings)
{
    FILE* stream;
    SettingsEntry* curr;

    // Ensure there is something worth saving.
    if (settings->entries == NULL) {
        return;
    }

    // Only save settings have been changed.
    if ((settings->flags & SETTINGS_CHANGED) == 0) {
        return;
    }

    stream = fopen(settings->path, "wt");
    if (stream == NULL) {
        // Something's wrong, this should not normally happen.
        return;
    }

    if (SDL_strcasecmp(settings->path, "arcanum.ini") == 0) {
        settings_save_ordered(stream, settings, arcanum_ini_save_order, arcanum_ini_section_names);
    } else if (SDL_strcasecmp(settings->path, "arcanum.cfg") == 0) {
        const char* const* const cfg_order[] = { arcanum_cfg_save_order, NULL };
        const char* const cfg_sections[] = { NULL };
        settings_save_ordered(stream, settings, cfg_order, cfg_sections);
    } else {
        curr = settings->entries;
        while (curr != NULL) {
            fprintf(stream, "%s=%s\n", curr->key, curr->value);
            curr = curr->next;
        }
    }

    fclose(stream);

    // FIX: Settings synchronized to disk, unset `SETTINGS_CHANGED`.
    settings->flags &= ~SETTINGS_CHANGED;
}

/**
 * Registers a new setting with a given key and default value.
 *
 * An optional callback `value_changed_func` can be specified as part of
 * setting. This callback will be triggered whenever the value associated with
 * a given key is changed. It will not be fired upon registration. Care should
 * be taken when changing the settings from within this callback to avoid
 * creating infinite loops.
 *
 * 0x438C80
 */
void settings_register(Settings* settings, const char* key, const char* default_value, SettingsValueChangedFunc value_changed_func)
{
    SettingsEntry* entry;
    const char* normalized_key;

    normalized_key = settings_normalize_key(key);
    entry = settings_find(settings, normalized_key);
    if (entry != NULL) {
        entry->value_changed_func = value_changed_func;
    } else {
        entry = (SettingsEntry*)MALLOC(sizeof(*entry));
        entry->key = STRDUP(normalized_key);
        entry->value = STRDUP(default_value);
        entry->value_changed_func = value_changed_func;
        entry->next = settings->entries;
        settings->entries = entry;
    }
}

/**
 * Sets an integer value for a setting.
 *
 * 0x438CE0
 */
void settings_set_value(Settings* settings, const char* key, int value)
{
    char buffer[48];

    SDL_itoa(value, buffer, 10);
    settings_set_str_value(settings, key, buffer);
}

/**
 * Retrieves an integer value for a setting.
 *
 * Returns `0` if the key is not found.
 *
 * 0x438D10
 */
int settings_get_value(Settings* settings, const char* key)
{
    const char* str;

    str = settings_get_str_value(settings, key);
    if (str != NULL) {
        return atoi(str);
    } else {
        return 0;
    }
}

/**
 * Sets an ObjectID value for a setting.
 *
 * 0x438D40
 */
void settings_set_obj_value(Settings* settings, const char* key, ObjectID oid)
{
    char str[40];

    objid_id_to_str(str, oid);
    settings_set_str_value(settings, key, str);
}

/**
 * Retrieves an ObjectID value for a setting.
 *
 * Returns an ObjectID with type `OID_TYPE_NULL` if the key is not found.
 *
 * 0x438DA0
 */
ObjectID settings_get_obj_value(Settings* settings, const char* key)
{
    const char* str;
    ObjectID oid;

    str = settings_get_str_value(settings, key);
    if (str != NULL) {
        objid_id_from_str(&oid, str);
    } else {
        oid.type = OID_TYPE_NULL;
    }

    return oid;
}

/**
 * Sets a string value for a setting.
 *
 * 0x438DF0
 */
void settings_set_str_value(Settings* settings, const char* key, const char* value)
{
    SettingsEntry* entry;
    const char* normalized_key;

    // Mark settings as changed to trigger saving.
    settings->flags |= SETTINGS_CHANGED;
    normalized_key = settings_normalize_key(key);

    // Check if the key already exists.
    entry = settings_find(settings, normalized_key);
    if (entry == NULL) {
        // Add a new entry if the key does not exist.
        settings_register(settings, normalized_key, value, NULL);
    } else {
        // Update the existing entry's value.
        if (strlen(value) > strlen(entry->value)) {
            // The new string is longer than existing one - free existing value
            // and make a copy of the new string.
            FREE(entry->value);
            entry->value = STRDUP(value);
        } else {
            // Value fits into already existing buffer.
            memcpy(entry->value, value, strlen(value) + 1);
        }

        // Trigger the callback.
        if (entry->value_changed_func != NULL) {
            entry->value_changed_func();
        }
    }
}

/**
 * Retrieves a string value for a setting.
 *
 * Returns `NULL` if the key is not found.
 *
 * 0x438E90
 */
const char* settings_get_str_value(Settings* settings, const char* key)
{
    SettingsEntry* entry;

    entry = settings_find(settings, settings_normalize_key(key));
    if (entry != NULL) {
        return entry->value;
    } else {
        return NULL;
    }
}

/**
 * Internal helper function to find a settings entry by key.
 *
 * 0x438EB0
 */
SettingsEntry* settings_find(Settings* settings, const char* key)
{
    SettingsEntry* curr;

    curr = settings->entries;
    while (curr != NULL) {
        if (SDL_strcasecmp(curr->key, key) == 0) {
            return curr;
        }
        curr = curr->next;
    }

    return NULL;
}

const char* settings_normalize_key(const char* key)
{
    int index;

    for (index = 0; settings_key_aliases[index].legacy != NULL; index++) {
        if (SDL_strcasecmp(key, settings_key_aliases[index].legacy) == 0) {
            return settings_key_aliases[index].current;
        }
    }

    return key;
}

const char* settings_normalize_section_key(const char* section, const char* key)
{
    if (SDL_strcasecmp(section, "bindings") == 0
        && SDL_strcasecmp(key, "always_run") == 0) {
        return "binding_always_run";
    }

    return settings_normalize_key(key);
}

const char* settings_output_key(const char* key)
{
    if (SDL_strcasecmp(key, "binding_always_run") == 0) {
        return "always_run";
    }

    return key;
}

bool settings_key_in_save_order(const char* key, const char* const* const* save_order)
{
    int index;
    int section;

    for (section = 0; save_order[section] != NULL; section++) {
        for (index = 0; save_order[section][index] != NULL; index++) {
            if (save_order[section][index][0] != '\0'
                && SDL_strcasecmp(key, save_order[section][index]) == 0) {
                return true;
            }
        }
    }

    return false;
}

void settings_save_ordered(FILE* stream, Settings* settings, const char* const* const* save_order, const char* const* section_names)
{
    SettingsEntry* entry;
    bool wrote;
    bool pending_blank;
    int index;
    int section;

    for (section = 0; save_order[section] != NULL; section++) {
        wrote = false;
        pending_blank = false;

        if (section_names[section] != NULL) {
            fprintf(stream, "[%s]\n", section_names[section]);
        }

        for (index = 0; save_order[section][index] != NULL; index++) {
            if (save_order[section][index][0] == '\0') {
                if (wrote) {
                    pending_blank = true;
                }
                continue;
            }

            entry = settings_find(settings, save_order[section][index]);
            if (entry == NULL) {
                continue;
            }

            if (pending_blank) {
                fprintf(stream, "\n");
                pending_blank = false;
            }

            fprintf(stream, "%s=%s\n", settings_output_key(entry->key), entry->value);
            wrote = true;
        }

        if (save_order[section + 1] != NULL) {
            fprintf(stream, "\n");
        }
    }
}

/**
 * Internal helper function to trim leading and trailing whitespace from a
 * string (in place).
 *
 * 0x438EF0
 */
void settings_trim(char* str)
{
    char* curr;
    size_t len;

    // Skip leading whitespace.
    curr = str;
    while (SDL_isspace(*curr)) {
        curr++;
    }

    // Move the trimmed string to the start.
    len = strlen(curr);
    memmove(str, curr, len + 1); // FIX: Instead of `memcpy`.

    // Remove trailing whitespace.
    while (len > 0 && SDL_isspace(str[len - 1])) {
        str[--len] = '\0';
    }
}
