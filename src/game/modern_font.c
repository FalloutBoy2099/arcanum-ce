#include "game/modern_font.h"

#include <stdio.h>
#include <stdlib.h>

#include "game/gamelib.h"
#include "game/settings.h"
#include "tig/color.h"
#include "tig/find_file.h"

static bool modern_font_parse_color(const char* str, int* red, int* green, int* blue);

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

tig_color_t modern_font_color_for(const char* key, int default_red, int default_green, int default_blue)
{
    const char* str;
    int red;
    int green;
    int blue;

    str = settings_get_str_value(&settings, key);
    if (str != NULL && modern_font_parse_color(str, &red, &green, &blue)) {
        return tig_color_make(red, green, blue);
    }

    return tig_color_make(default_red, default_green, default_blue);
}

int modern_font_int_for(const char* key, int default_value, int min_value, int max_value)
{
    const char* str;
    int value;

    str = settings_get_str_value(&settings, key);
    value = str != NULL && str[0] != '\0' ? atoi(str) : default_value;
    if (value < min_value) {
        value = min_value;
    } else if (value > max_value) {
        value = max_value;
    }

    return value;
}

bool modern_font_parse_color(const char* str, int* red, int* green, int* blue)
{
    unsigned int value;
    int r;
    int g;
    int b;

    if (str == NULL) {
        return false;
    }

    if (str[0] == '#') {
        str++;
    } else if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2;
    }

    if (SDL_strlen(str) == 6 && sscanf(str, "%x", &value) == 1) {
        *red = (value >> 16) & 0xFF;
        *green = (value >> 8) & 0xFF;
        *blue = value & 0xFF;
        return true;
    }

    if (sscanf(str, "%d,%d,%d", &r, &g, &b) == 3
        && r >= 0 && r <= 255
        && g >= 0 && g <= 255
        && b >= 0 && b <= 255) {
        *red = r;
        *green = g;
        *blue = b;
        return true;
    }

    return false;
}
