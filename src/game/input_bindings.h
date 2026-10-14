#ifndef ARCANUM_GAME_INPUT_BINDINGS_H_
#define ARCANUM_GAME_INPUT_BINDINGS_H_

#include "game/context.h"

typedef enum InputAction {
    INPUT_ACTION_MAIN_MENU,
    INPUT_ACTION_OPTIONS,
    INPUT_ACTION_QUICK_SAVE,
    INPUT_ACTION_QUICK_LOAD,
    INPUT_ACTION_SCREENSHOT,
    INPUT_ACTION_TOGGLE_COMPACT_UI,
    INPUT_ACTION_ALWAYS_RUN,
    INPUT_ACTION_CENTER_CAMERA,
    INPUT_ACTION_HOTKEY_1,
    INPUT_ACTION_HOTKEY_2,
    INPUT_ACTION_HOTKEY_3,
    INPUT_ACTION_HOTKEY_4,
    INPUT_ACTION_HOTKEY_5,
    INPUT_ACTION_HOTKEY_6,
    INPUT_ACTION_HOTKEY_7,
    INPUT_ACTION_HOTKEY_8,
    INPUT_ACTION_HOTKEY_9,
    INPUT_ACTION_HOTKEY_0,
    INPUT_ACTION_CHARACTER,
    INPUT_ACTION_INVENTORY,
    INPUT_ACTION_LOGBOOK,
    INPUT_ACTION_MAP,
    INPUT_ACTION_FATE,
    INPUT_ACTION_SKILLS,
    INPUT_ACTION_SPELLS,
    INPUT_ACTION_SCHEMATICS,
    INPUT_ACTION_SLEEP,
    INPUT_ACTION_CHAT,
    INPUT_ACTION_REPEAT_ACTION,
    INPUT_ACTION_COMBAT_MODE,
    INPUT_ACTION_TOGGLE_COMBAT,
    INPUT_ACTION_END_TURN,
    INPUT_ACTION_FOLLOWER_WALK,
    INPUT_ACTION_FOLLOWER_ATTACK,
    INPUT_ACTION_FOLLOWER_STAY_CLOSE,
    INPUT_ACTION_FOLLOWER_SPREAD_OUT,
    INPUT_ACTION_FOLLOWER_STOP,
    INPUT_ACTION_SCROLL_UP,
    INPUT_ACTION_SCROLL_DOWN,
    INPUT_ACTION_SCROLL_LEFT,
    INPUT_ACTION_SCROLL_RIGHT,
    INPUT_ACTION_COUNT,
} InputAction;

void input_bindings_init(void);
void input_bindings_handle_message(const TigMessage* msg);
bool input_binding_matches_keyboard(InputAction action, const TigMessage* msg);
bool input_binding_matches_mouse(InputAction action, const TigMessage* msg);
bool input_binding_is_pressed(InputAction action);
SDL_Scancode input_binding_primary_scancode(InputAction action, SDL_Scancode fallback);
int input_binding_hotkey_index(const TigMessage* msg);
int input_binding_follower_index(const TigMessage* msg);

#endif /* ARCANUM_GAME_INPUT_BINDINGS_H_ */
