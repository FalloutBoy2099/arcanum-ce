#include "game/input_bindings.h"

#include "game/gamelib.h"

#define INPUT_BINDING_MAX_PARTS 2

typedef enum InputBindingPartType {
    INPUT_BINDING_PART_NONE,
    INPUT_BINDING_PART_KEY,
    INPUT_BINDING_PART_MOUSE,
} InputBindingPartType;

typedef struct InputBindingPart {
    InputBindingPartType type;
    SDL_Scancode scancode;
    TigMessageMouseEvent mouse_up;
    TigMessageMouseEvent mouse_down;
} InputBindingPart;

typedef struct InputBindingDef {
    InputAction action;
    const char* key;
    const char* default_value;
} InputBindingDef;

static bool input_binding_parse_part(const char* str, InputBindingPart* part);
static void input_binding_trim(char* str);
static bool input_binding_part_matches_keyboard(const InputBindingPart* part, const TigMessage* msg);
static bool input_binding_part_matches_mouse(const InputBindingPart* part, const TigMessage* msg);

static const InputBindingDef input_binding_defs[] = {
    { INPUT_ACTION_MAIN_MENU, "main_menu", "ESCAPE" },
    { INPUT_ACTION_OPTIONS, "options", "O" },
    { INPUT_ACTION_QUICK_SAVE, "quick_save", "F7" },
    { INPUT_ACTION_QUICK_LOAD, "quick_load", "F8" },
    { INPUT_ACTION_SCREENSHOT, "screenshot", "F12" },
    { INPUT_ACTION_TOGGLE_COMPACT_UI, "toggle_compact_ui", "F10" },
    { INPUT_ACTION_ALWAYS_RUN, "binding_always_run", "NUMLOCK" },
    { INPUT_ACTION_CENTER_CAMERA, "center_camera", "HOME|MIDDLE_MOUSE" },
    { INPUT_ACTION_HOTKEY_1, "hotkey1", "1" },
    { INPUT_ACTION_HOTKEY_2, "hotkey2", "2" },
    { INPUT_ACTION_HOTKEY_3, "hotkey3", "3" },
    { INPUT_ACTION_HOTKEY_4, "hotkey4", "4" },
    { INPUT_ACTION_HOTKEY_5, "hotkey5", "5" },
    { INPUT_ACTION_HOTKEY_6, "hotkey6", "6" },
    { INPUT_ACTION_HOTKEY_7, "hotkey7", "7" },
    { INPUT_ACTION_HOTKEY_8, "hotkey8", "8" },
    { INPUT_ACTION_HOTKEY_9, "hotkey9", "9" },
    { INPUT_ACTION_HOTKEY_0, "hotkey0", "0" },
    { INPUT_ACTION_CHARACTER, "character", "C" },
    { INPUT_ACTION_INVENTORY, "inventory", "I" },
    { INPUT_ACTION_LOGBOOK, "logbook", "L" },
    { INPUT_ACTION_MAP, "map", "W" },
    { INPUT_ACTION_FATE, "fate", "F" },
    { INPUT_ACTION_SKILLS, "skills", "K" },
    { INPUT_ACTION_SPELLS, "spells", "M" },
    { INPUT_ACTION_SCHEMATICS, "schematics", "T" },
    { INPUT_ACTION_SLEEP, "sleep", "S" },
    { INPUT_ACTION_CHAT, "chat", "RETURN" },
    { INPUT_ACTION_REPEAT_ACTION, "repeat_action", "A" },
    { INPUT_ACTION_COMBAT_MODE, "combat_mode", "R" },
    { INPUT_ACTION_TOGGLE_COMBAT, "toggle_combat", "SPACE|PREVIOUS_MOUSE" },
    { INPUT_ACTION_END_TURN, "end_turn", "E|NEXT_MOUSE" },
    { INPUT_ACTION_FOLLOWER_WALK, "follower_walk", "F1" },
    { INPUT_ACTION_FOLLOWER_ATTACK, "follower_attack", "F2" },
    { INPUT_ACTION_FOLLOWER_STAY_CLOSE, "follower_stay_close", "F3" },
    { INPUT_ACTION_FOLLOWER_SPREAD_OUT, "follower_spread_out", "F4" },
    { INPUT_ACTION_FOLLOWER_STOP, "follower_stop", "F5" },
    { INPUT_ACTION_SCROLL_UP, "scroll_up", "UP" },
    { INPUT_ACTION_SCROLL_DOWN, "scroll_down", "DOWN" },
    { INPUT_ACTION_SCROLL_LEFT, "scroll_left", "LEFT" },
    { INPUT_ACTION_SCROLL_RIGHT, "scroll_right", "RIGHT" },
};

static InputBindingPart input_bindings[INPUT_ACTION_COUNT][INPUT_BINDING_MAX_PARTS];

void input_bindings_init(void)
{
    int index;
    int part_index;
    const char* value;
    char buffer[128];
    char* part;
    char* next;

    for (index = 0; index < SDL_arraysize(input_binding_defs); index++) {
        settings_register(&settings, input_binding_defs[index].key, input_binding_defs[index].default_value, NULL);

        for (part_index = 0; part_index < INPUT_BINDING_MAX_PARTS; part_index++) {
            input_bindings[input_binding_defs[index].action][part_index].type = INPUT_BINDING_PART_NONE;
        }

        value = settings_get_str_value(&settings, input_binding_defs[index].key);
        if (value == NULL) {
            continue;
        }

        SDL_strlcpy(buffer, value, sizeof(buffer));
        part = buffer;
        for (part_index = 0; part_index < INPUT_BINDING_MAX_PARTS && part != NULL; part_index++) {
            next = strchr(part, '|');
            if (next != NULL) {
                *next++ = '\0';
            }

            input_binding_trim(part);
            input_binding_parse_part(part, &(input_bindings[input_binding_defs[index].action][part_index]));
            part = next;
        }
    }
}

bool input_binding_matches_keyboard(InputAction action, const TigMessage* msg)
{
    int index;

    if (action < 0 || action >= INPUT_ACTION_COUNT || msg->type != TIG_MESSAGE_KEYBOARD) {
        return false;
    }

    for (index = 0; index < INPUT_BINDING_MAX_PARTS; index++) {
        if (input_binding_part_matches_keyboard(&(input_bindings[action][index]), msg)) {
            return true;
        }
    }

    return false;
}

bool input_binding_matches_mouse(InputAction action, const TigMessage* msg)
{
    int index;

    if (action < 0 || action >= INPUT_ACTION_COUNT || msg->type != TIG_MESSAGE_MOUSE) {
        return false;
    }

    for (index = 0; index < INPUT_BINDING_MAX_PARTS; index++) {
        if (input_binding_part_matches_mouse(&(input_bindings[action][index]), msg)) {
            return true;
        }
    }

    return false;
}

bool input_binding_is_pressed(InputAction action)
{
    int index;
    InputBindingPart* part;

    if (action < 0 || action >= INPUT_ACTION_COUNT) {
        return false;
    }

    for (index = 0; index < INPUT_BINDING_MAX_PARTS; index++) {
        part = &(input_bindings[action][index]);
        if (part->type == INPUT_BINDING_PART_KEY && tig_kb_is_key_pressed(part->scancode)) {
            return true;
        }
    }

    return false;
}

SDL_Scancode input_binding_primary_scancode(InputAction action, SDL_Scancode fallback)
{
    int index;
    InputBindingPart* part;

    if (action < 0 || action >= INPUT_ACTION_COUNT) {
        return fallback;
    }

    for (index = 0; index < INPUT_BINDING_MAX_PARTS; index++) {
        part = &(input_bindings[action][index]);
        if (part->type == INPUT_BINDING_PART_KEY) {
            return part->scancode;
        }
    }

    return fallback;
}

int input_binding_hotkey_index(const TigMessage* msg)
{
    int index;

    for (index = 0; index < 10; index++) {
        if (input_binding_matches_keyboard((InputAction)(INPUT_ACTION_HOTKEY_1 + index), msg)) {
            return index;
        }
    }

    return -1;
}

int input_binding_follower_index(const TigMessage* msg)
{
    int index;

    for (index = 0; index < 5; index++) {
        if (input_binding_matches_keyboard((InputAction)(INPUT_ACTION_FOLLOWER_WALK + index), msg)) {
            return index;
        }
    }

    return -1;
}

bool input_binding_parse_part(const char* str, InputBindingPart* part)
{
    SDL_Scancode scancode;

    part->type = INPUT_BINDING_PART_NONE;

    if (SDL_strcasecmp(str, "MIDDLE_MOUSE") == 0) {
        part->type = INPUT_BINDING_PART_MOUSE;
        part->mouse_down = TIG_MESSAGE_MOUSE_MIDDLE_BUTTON_DOWN;
        part->mouse_up = TIG_MESSAGE_MOUSE_MIDDLE_BUTTON_UP;
        return true;
    }

    if (SDL_strcasecmp(str, "PREVIOUS_MOUSE") == 0) {
        part->type = INPUT_BINDING_PART_MOUSE;
        part->mouse_down = TIG_MESSAGE_MOUSE_X1_BUTTON_DOWN;
        part->mouse_up = TIG_MESSAGE_MOUSE_X1_BUTTON_UP;
        return true;
    }

    if (SDL_strcasecmp(str, "NEXT_MOUSE") == 0) {
        part->type = INPUT_BINDING_PART_MOUSE;
        part->mouse_down = TIG_MESSAGE_MOUSE_X2_BUTTON_DOWN;
        part->mouse_up = TIG_MESSAGE_MOUSE_X2_BUTTON_UP;
        return true;
    }

    scancode = SDL_GetScancodeFromName(str);
    if (scancode == SDL_SCANCODE_UNKNOWN && SDL_strlen(str) == 1) {
        char upper[2];
        upper[0] = (char)SDL_toupper(str[0]);
        upper[1] = '\0';
        scancode = SDL_GetScancodeFromName(upper);
    }

    if (scancode != SDL_SCANCODE_UNKNOWN) {
        part->type = INPUT_BINDING_PART_KEY;
        part->scancode = scancode;
        return true;
    }

    return false;
}

void input_binding_trim(char* str)
{
    char* curr;
    size_t len;

    curr = str;
    while (SDL_isspace(*curr)) {
        curr++;
    }

    len = strlen(curr);
    memmove(str, curr, len + 1);

    while (len > 0 && SDL_isspace(str[len - 1])) {
        str[--len] = '\0';
    }
}

bool input_binding_part_matches_keyboard(const InputBindingPart* part, const TigMessage* msg)
{
    return part->type == INPUT_BINDING_PART_KEY
        && msg->data.keyboard.scancode == part->scancode;
}

bool input_binding_part_matches_mouse(const InputBindingPart* part, const TigMessage* msg)
{
    return part->type == INPUT_BINDING_PART_MOUSE
        && msg->data.mouse.event == part->mouse_up;
}
