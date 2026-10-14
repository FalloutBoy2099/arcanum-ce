#include "game/modern_font.h"

#include "game/gamelib.h"
#include "game/settings.h"
#include "tig/find_file.h"

static const char* const modern_font_fallback_paths[] = {
    "fonts\\modern.ttf",
    "fonts\\dialog.ttf",
    "fonts\\logbook.ttf",
    "C:\\Windows\\Fonts\\GARA.TTF",
    "C:\\Windows\\Fonts\\georgia.ttf",
    "C:\\Windows\\Fonts\\times.ttf",
    NULL,
};

bool modern_font_enabled(void)
{
    return settings_get_value(&settings, MODERN_FONTS_KEY) != 0;
}

const char* modern_font_path(void)
{
    return modern_font_path_for(MODERN_FONT_PATH_KEY);
}

const char* modern_font_path_for(const char* key)
{
    TigFindFileData find_file_data;
    const char* path;
    int index;

    path = settings_get_str_value(&settings, key);
    if (path == NULL || path[0] == '\0') {
        path = settings_get_str_value(&settings, MODERN_FONT_PATH_KEY);
    }
    if (path != NULL && path[0] != '\0') {
        if (tig_find_first_file(path, &find_file_data)) {
            tig_find_close(&find_file_data);
            return path;
        }
    }

    for (index = 0; modern_font_fallback_paths[index] != NULL; index++) {
        if (tig_find_first_file(modern_font_fallback_paths[index], &find_file_data)) {
            tig_find_close(&find_file_data);
            return modern_font_fallback_paths[index];
        }
    }

    return NULL;
}

float modern_font_size(void)
{
    return modern_font_size_for(MODERN_FONT_SIZE_KEY);
}

float modern_font_size_for(const char* key)
{
    int value;

    value = settings_get_value(&settings, key);
    if (value == 0) {
        value = settings_get_value(&settings, MODERN_FONT_SIZE_KEY);
    }
    if (value < 6) {
        value = 6;
    } else if (value > 72) {
        value = 72;
    }

    return (float)value;
}
