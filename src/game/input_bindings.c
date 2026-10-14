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
    uint32_t mouse_mask;
    TigMessageMouseEvent mouse_up;
    TigMessageMouseEvent mouse_down;
} InputBindingPart;

typedef struct InputBindingDef {
    InputAction action;
    const char* key;
    const char* default_value;
} InputBindingDef;

static bool input_binding_parse_part(const char* str, InputBindingPart* part);
static SDL_Scancode input_binding_scancode_from_name(const char* str);
static void input_binding_trim(char* str);
static bool input_binding_part_matches_keyboard(const InputBindingPart* part, const TigMessage* msg);
static bool input_binding_part_matches_mouse(const InputBindingPart* part, const TigMessage* msg);
static uint32_t input_binding_mouse_mask_from_event(TigMessageMouseEvent event);

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
    { INPUT_ACTION_ZOOM_IN, "zoom_in", "=|Keypad +" },
    { INPUT_ACTION_ZOOM_OUT, "zoom_out", "-|Keypad -" },
    { INPUT_ACTION_ZOOM_RESET, "zoom_reset", "Keypad 0" },
    { INPUT_ACTION_CYCLE_HUD, "cycle_hud", "TAB" },
};

static InputBindingPart input_bindings[INPUT_ACTION_COUNT][INPUT_BINDING_MAX_PARTS];
static uint32_t input_binding_mouse_state;

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

void input_bindings_handle_message(const TigMessage* msg)
{
    uint32_t mask;

    if (msg->type != TIG_MESSAGE_MOUSE) {
        return;
    }

    mask = input_binding_mouse_mask_from_event(msg->data.mouse.event);
    if (mask == 0) {
        return;
    }

    switch (msg->data.mouse.event) {
    case TIG_MESSAGE_MOUSE_MIDDLE_BUTTON_DOWN:
    case TIG_MESSAGE_MOUSE_X1_BUTTON_DOWN:
    case TIG_MESSAGE_MOUSE_X2_BUTTON_DOWN:
        input_binding_mouse_state |= mask;
        break;
    case TIG_MESSAGE_MOUSE_MIDDLE_BUTTON_UP:
    case TIG_MESSAGE_MOUSE_X1_BUTTON_UP:
    case TIG_MESSAGE_MOUSE_X2_BUTTON_UP:
        input_binding_mouse_state &= ~mask;
        break;
    default:
        break;
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
    uint32_t mouse_state;

    if (action < 0 || action >= INPUT_ACTION_COUNT) {
        return false;
    }

    mouse_state = SDL_GetMouseState(NULL, NULL) | input_binding_mouse_state;

    for (index = 0; index < INPUT_BINDING_MAX_PARTS; index++) {
        part = &(input_bindings[action][index]);
        if (part->type == INPUT_BINDING_PART_KEY && tig_kb_is_key_pressed(part->scancode)) {
            return true;
        }
        if (part->type == INPUT_BINDING_PART_MOUSE && (mouse_state & part->mouse_mask) != 0) {
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
        part->mouse_mask = SDL_BUTTON_MMASK;
        part->mouse_down = TIG_MESSAGE_MOUSE_MIDDLE_BUTTON_DOWN;
        part->mouse_up = TIG_MESSAGE_MOUSE_MIDDLE_BUTTON_UP;
        return true;
    }

    if (SDL_strcasecmp(str, "PREVIOUS_MOUSE") == 0
        || SDL_strcasecmp(str, "BACK_MOUSE") == 0
        || SDL_strcasecmp(str, "MOUSE4") == 0
        || SDL_strcasecmp(str, "X1_MOUSE") == 0) {
        part->type = INPUT_BINDING_PART_MOUSE;
        part->mouse_mask = SDL_BUTTON_X1MASK;
        part->mouse_down = TIG_MESSAGE_MOUSE_X1_BUTTON_DOWN;
        part->mouse_up = TIG_MESSAGE_MOUSE_X1_BUTTON_UP;
        return true;
    }

    if (SDL_strcasecmp(str, "NEXT_MOUSE") == 0
        || SDL_strcasecmp(str, "FORWARD_MOUSE") == 0
        || SDL_strcasecmp(str, "MOUSE5") == 0
        || SDL_strcasecmp(str, "X2_MOUSE") == 0) {
        part->type = INPUT_BINDING_PART_MOUSE;
        part->mouse_mask = SDL_BUTTON_X2MASK;
        part->mouse_down = TIG_MESSAGE_MOUSE_X2_BUTTON_DOWN;
        part->mouse_up = TIG_MESSAGE_MOUSE_X2_BUTTON_UP;
        return true;
    }

    scancode = input_binding_scancode_from_name(str);
    if (scancode != SDL_SCANCODE_UNKNOWN) {
        part->type = INPUT_BINDING_PART_KEY;
        part->scancode = scancode;
        return true;
    }

    return false;
}

SDL_Scancode input_binding_scancode_from_name(const char* str)
{
    char upper[64];
    size_t len;

    len = SDL_strlen(str);
    if (len == 1) {
        int ch = SDL_toupper(str[0]);
        if (ch >= 'A' && ch <= 'Z') {
            return (SDL_Scancode)(SDL_SCANCODE_A + (ch - 'A'));
        }

        if (ch >= '1' && ch <= '9') {
            return (SDL_Scancode)(SDL_SCANCODE_1 + (ch - '1'));
        }

        if (ch == '0') {
            return SDL_SCANCODE_0;
        }
    }

    SDL_strlcpy(upper, str, sizeof(upper));
    for (size_t index = 0; upper[index] != '\0'; index++) {
        upper[index] = (char)SDL_toupper(upper[index]);
    }

    if (SDL_strcasecmp(upper, "ESC") == 0) return SDL_SCANCODE_ESCAPE;
    if (SDL_strcasecmp(upper, "ESCAPE") == 0) return SDL_SCANCODE_ESCAPE;
    if (SDL_strcasecmp(upper, "ENTER") == 0) return SDL_SCANCODE_RETURN;
    if (SDL_strcasecmp(upper, "RETURN") == 0) return SDL_SCANCODE_RETURN;
    if (SDL_strcasecmp(upper, "SPACE") == 0) return SDL_SCANCODE_SPACE;
    if (SDL_strcasecmp(upper, "TAB") == 0) return SDL_SCANCODE_TAB;
    if (SDL_strcasecmp(upper, "BACKSPACE") == 0) return SDL_SCANCODE_BACKSPACE;
    if (SDL_strcasecmp(upper, "DELETE") == 0) return SDL_SCANCODE_DELETE;
    if (SDL_strcasecmp(upper, "INSERT") == 0) return SDL_SCANCODE_INSERT;
    if (SDL_strcasecmp(upper, "HOME") == 0) return SDL_SCANCODE_HOME;
    if (SDL_strcasecmp(upper, "END") == 0) return SDL_SCANCODE_END;
    if (SDL_strcasecmp(upper, "PAGEUP") == 0) return SDL_SCANCODE_PAGEUP;
    if (SDL_strcasecmp(upper, "PAGEDOWN") == 0) return SDL_SCANCODE_PAGEDOWN;
    if (SDL_strcasecmp(upper, "UP") == 0) return SDL_SCANCODE_UP;
    if (SDL_strcasecmp(upper, "DOWN") == 0) return SDL_SCANCODE_DOWN;
    if (SDL_strcasecmp(upper, "LEFT") == 0) return SDL_SCANCODE_LEFT;
    if (SDL_strcasecmp(upper, "RIGHT") == 0) return SDL_SCANCODE_RIGHT;
    if (SDL_strcasecmp(upper, "NUMLOCK") == 0) return SDL_SCANCODE_NUMLOCKCLEAR;
    if (SDL_strcasecmp(upper, "NUMLOCKCLEAR") == 0) return SDL_SCANCODE_NUMLOCKCLEAR;

    if (upper[0] == 'F') {
        int fn = SDL_atoi(upper + 1);
        if (fn >= 1 && fn <= 12) {
            return (SDL_Scancode)(SDL_SCANCODE_F1 + (fn - 1));
        }
    }

    return SDL_GetScancodeFromName(str);
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
        && msg->data.mouse.event == part->mouse_down
        && !msg->data.mouse.repeat;
}

uint32_t input_binding_mouse_mask_from_event(TigMessageMouseEvent event)
{
    switch (event) {
    case TIG_MESSAGE_MOUSE_MIDDLE_BUTTON_DOWN:
    case TIG_MESSAGE_MOUSE_MIDDLE_BUTTON_UP:
        return SDL_BUTTON_MMASK;
    case TIG_MESSAGE_MOUSE_X1_BUTTON_DOWN:
    case TIG_MESSAGE_MOUSE_X1_BUTTON_UP:
        return SDL_BUTTON_X1MASK;
    case TIG_MESSAGE_MOUSE_X2_BUTTON_DOWN:
    case TIG_MESSAGE_MOUSE_X2_BUTTON_UP:
        return SDL_BUTTON_X2MASK;
    default:
        return 0;
    }
}
