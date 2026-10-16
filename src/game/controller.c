#include "game/controller.h"

#include <stdio.h>
#include <string.h>
#include "game/gamelib.h"
#include "game/input_bindings.h"

typedef struct ControllerBinding {
    const char* action;
    const char* buttons;
} ControllerBinding;

static const ControllerBinding controller_bindings[] = {
    { "mouse_left", "a|rt" },
    { "mouse_right", "b|lt" },
    { "toggle_combat", "x" },
    { "combat_mode", "y" },
    { "end_turn", "rb" },
    { "map", "back|dpad_left" },
    { "main_menu", "start" },
    { "inventory", "dpad_up" },
    { "character", "dpad_right" },
    { "logbook", "dpad_down" },
    { "center_camera", "left_stick" },
    { "zoom_reset", "right_stick" },
    { "quick_save", "lb+y" },
    { "sleep", "lb+rb" },
    { "spells", "lb+back" },
    { "schematics", "lb+start" },
    { "zoom_in", "lb+x" },
    { "zoom_out", "lb+b" },
};

static const char* controller_button_names[] = {
    "a", "b", "x", "y", "back", "guide", "start", "left_stick", "right_stick",
    "lb", "rb", "dpad_up", "dpad_down", "dpad_left", "dpad_right",
};

void controller_settings_register(Settings* prefs)
{
    settings_register(prefs, "controller_enabled", "0", NULL);
    settings_register(prefs, "controller_cursor_speed", "1200", NULL);
    settings_register(prefs, "controller_cursor_deadzone", "8000", NULL);
    settings_register(prefs, "controller_camera_deadzone", "12000", NULL);
    settings_register(prefs, "controller_precision_percent", "45", NULL);
    settings_register(prefs, "controller_trigger_threshold", "4500", NULL);
    settings_register(prefs, "controller_swap_sticks", "0", NULL);
    for (int i = 0; i < SDL_arraysize(controller_bindings); i++) {
        char key[80];
        snprintf(key, sizeof(key), "controller_%s", controller_bindings[i].action);
        settings_register(prefs, key, controller_bindings[i].buttons, NULL);
    }
}

static void controller_assign(TigControllerConfig* config, const char* token, int target,
    int layer, bool assigned[2][SDL_GAMEPAD_BUTTON_COUNT], bool triggers_assigned[2])
{
    if (SDL_strcasecmp(token, "lt") == 0 || SDL_strcasecmp(token, "rt") == 0) {
        int trigger = SDL_strcasecmp(token, "rt") == 0 ? 1 : 0;
        if (layer != 0 || triggers_assigned[trigger]) {
            tig_debug_printf("Controller: conflicting or unsupported trigger binding %s\n", token);
            return;
        }
        config->triggers[trigger] = target;
        triggers_assigned[trigger] = true;
        return;
    }
    for (int button = 0; button < SDL_arraysize(controller_button_names); button++) {
        if (SDL_strcasecmp(token, controller_button_names[button]) != 0) continue;
        if (button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER || button == SDL_GAMEPAD_BUTTON_GUIDE
            || assigned[layer][button]) {
            tig_debug_printf("Controller: reserved or conflicting button binding %s\n", token);
            return;
        }
        config->buttons[layer][button] = target;
        assigned[layer][button] = true;
        return;
    }
    tig_debug_printf("Controller: unknown button %s\n", token);
}

void controller_config_build(Settings* prefs, TigControllerConfig* config)
{
    *config = (TigControllerConfig){ 0 };
    config->enabled = settings_get_value(prefs, "controller_enabled") != 0;
    config->swap_sticks = settings_get_value(prefs, "controller_swap_sticks") != 0;
    config->cursor_speed = settings_get_value(prefs, "controller_cursor_speed");
    config->cursor_deadzone = settings_get_value(prefs, "controller_cursor_deadzone");
    config->camera_deadzone = settings_get_value(prefs, "controller_camera_deadzone");
    config->precision_percent = settings_get_value(prefs, "controller_precision_percent");
    config->trigger_threshold = settings_get_value(prefs, "controller_trigger_threshold");
    bool assigned[2][SDL_GAMEPAD_BUTTON_COUNT] = { 0 };
    bool triggers_assigned[2] = { 0 };
    // Resolve normal mappings first, then inherit them into the LB layer.
    // Explicit LB mappings can override inherited controls without conflicts.
    for (int layer = 0; layer < 2; layer++) {
        if (layer == 1) memcpy(config->buttons[1], config->buttons[0], sizeof(config->buttons[0]));
        for (int i = 0; i < SDL_arraysize(controller_bindings); i++) {
            const char* action = controller_bindings[i].action;
            int target = SDL_strcasecmp(action, "mouse_left") == 0 ? TIG_CONTROLLER_MOUSE_LEFT
                : SDL_strcasecmp(action, "mouse_right") == 0 ? TIG_CONTROLLER_MOUSE_RIGHT
                : input_binding_named_scancode(action);
            char key[80];
            snprintf(key, sizeof(key), "controller_%s", action);
            const char* value = settings_get_str_value(prefs, key);
            if (value == NULL) continue;
            while (*value != '\0') {
                const char* end = strchr(value, '|');
                size_t length = end != NULL ? (size_t)(end - value) : strlen(value);
                char token[64];
                if (length < sizeof(token)) {
                    memcpy(token, value, length);
                    token[length] = '\0';
                    char* start = token;
                    while (SDL_isspace(*start)) start++;
                    char* tail = start + strlen(start);
                    while (tail > start && SDL_isspace(tail[-1])) *--tail = '\0';
                    bool shifted = SDL_strncasecmp(start, "lb+", 3) == 0;
                    if ((shifted ? 1 : 0) == layer && *start != '\0' && SDL_strcasecmp(start, "none") != 0) {
                        controller_assign(config, shifted ? start + 3 : start, target, layer, assigned, triggers_assigned);
                    }
                } else tig_debug_printf("Controller: button name too long for %s\n", action);
                if (end == NULL) break;
                value = end + 1;
            }
        }
    }
    config->camera[0] = input_binding_named_scancode("scroll_left");
    config->camera[1] = input_binding_named_scancode("scroll_right");
    config->camera[2] = input_binding_named_scancode("scroll_up");
    config->camera[3] = input_binding_named_scancode("scroll_down");
}

void controller_init(void)
{
    controller_settings_register(&settings);
    TigControllerConfig config;
    controller_config_build(&settings, &config);
    tig_controller_configure(&config);
}
