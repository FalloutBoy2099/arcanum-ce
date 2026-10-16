#include "tig/kb.h"

#include "tig/core.h"
#include "tig/message.h"

// 0x62B1A0
static bool tig_kb_initialized;
static bool tig_kb_controller_keys[SDL_SCANCODE_COUNT];

// 0x52B2F0
int tig_kb_init(TigInitInfo* init_info)
{
    (void)init_info;

    if (tig_kb_initialized) {
        return TIG_ERR_ALREADY_INITIALIZED;
    }

    tig_kb_initialized = true;

    return TIG_OK;
}

// 0x52B320
void tig_kb_exit(void)
{
    if (tig_kb_initialized) {
        tig_kb_initialized = false;
    }
}

// 0x52B340
bool tig_kb_is_key_pressed(SDL_Scancode scancode)
{
    return scancode > SDL_SCANCODE_UNKNOWN && scancode < SDL_SCANCODE_COUNT
        && (SDL_GetKeyboardState(NULL)[scancode] || tig_kb_controller_keys[scancode]);
}

// 0x52B350
bool tig_kb_get_modifier(SDL_Keymod keymod)
{
    return (SDL_GetModState() & keymod) != 0;
}

void tig_kb_set_key(SDL_Keycode key, SDL_Scancode scancode, bool down)
{
    TigMessage message;

    if (!down && scancode > SDL_SCANCODE_UNKNOWN && scancode < SDL_SCANCODE_COUNT
        && tig_kb_controller_keys[scancode]) return;

    message.type = TIG_MESSAGE_KEYBOARD;
    message.timestamp = tig_ping_timestamp;
    message.data.keyboard.key = key;
    message.data.keyboard.scancode = scancode;
    message.data.keyboard.pressed = down;
    tig_message_enqueue(&message);
}

void tig_kb_set_controller_key(SDL_Scancode scancode, bool down)
{
    if (scancode <= SDL_SCANCODE_UNKNOWN || scancode >= SDL_SCANCODE_COUNT
        || tig_kb_controller_keys[scancode] == down) return;
    tig_kb_controller_keys[scancode] = down;
    if (!SDL_GetKeyboardState(NULL)[scancode]) {
        tig_kb_set_key(SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false), scancode, down);
    }
}
