#ifndef TIG_CONTROLLER_H_
#define TIG_CONTROLLER_H_

#include "tig/types.h"

// Negative targets are mouse buttons; zero is unassigned, positive values
// are SDL scancodes. Mappings are resolved by the game, not by TIG.
#define TIG_CONTROLLER_MOUSE_LEFT (-1)
#define TIG_CONTROLLER_MOUSE_RIGHT (-2)

typedef struct TigControllerConfig {
    bool enabled;
    bool swap_sticks;
    int cursor_speed;
    int cursor_deadzone;
    int camera_deadzone;
    int precision_percent;
    int trigger_threshold;
    int buttons[2][SDL_GAMEPAD_BUTTON_COUNT];
    int triggers[2];
    int camera[4]; // left, right, up, down
} TigControllerConfig;

typedef struct TigControllerSnapshot {
    bool buttons[SDL_GAMEPAD_BUTTON_COUNT];
    Sint16 axes[SDL_GAMEPAD_AXIS_COUNT];
} TigControllerSnapshot;

typedef struct TigControllerOutput {
    bool keys[SDL_SCANCODE_COUNT];
    bool mouse[2];
    float cursor_x;
    float cursor_y;
} TigControllerOutput;

void tig_controller_build_output(const TigControllerConfig* config,
    const TigControllerSnapshot* snapshot, TigControllerOutput* output);
// Suppress held inputs after connecting or returning to the game until the
// controller has been released. Also used for benchmark input isolation.
void tig_controller_frame(const TigControllerConfig* config,
    const TigControllerSnapshot* snapshot, bool allowed, bool* needs_neutral,
    TigControllerOutput* output);
void tig_controller_configure(const TigControllerConfig* config);
void tig_controller_ping(bool blocked);
void tig_controller_devices_changed(void);
void tig_controller_exit(void);
bool tig_controller_mouse_held(int button);

#endif
