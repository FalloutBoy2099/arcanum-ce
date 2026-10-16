#include "tig/controller.h"

#include <math.h>
#include "tig/debug.h"
#include "tig/kb.h"
#include "tig/mouse.h"
#include "tig/video.h"

static TigControllerConfig controller_config;
static TigControllerOutput controller_output;
static SDL_Gamepad* controller;
static bool controller_initialized;
static bool controller_needs_neutral = true;
static Uint64 controller_last_tick;
static Uint64 controller_next_scan;
static float controller_remainder_x;
static float controller_remainder_y;

static void controller_target(TigControllerOutput* output, int target)
{
    if (target == TIG_CONTROLLER_MOUSE_LEFT) output->mouse[0] = true;
    else if (target == TIG_CONTROLLER_MOUSE_RIGHT) output->mouse[1] = true;
    else if (target > SDL_SCANCODE_UNKNOWN && target < SDL_SCANCODE_COUNT) output->keys[target] = true;
}

static float controller_axis(int value, int deadzone)
{
    int magnitude = SDL_abs(value);
    if (magnitude <= deadzone) return 0.0f;
    float normalized = SDL_min(1.0f, (float)(magnitude - deadzone) / (32767 - deadzone));
    return value < 0 ? -normalized : normalized;
}

void tig_controller_build_output(const TigControllerConfig* config,
    const TigControllerSnapshot* snapshot, TigControllerOutput* output)
{
    *output = (TigControllerOutput){ 0 };
    if (!config->enabled) return;
    int layer = snapshot->buttons[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER] ? 1 : 0;
    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
        if (snapshot->buttons[i]) controller_target(output, config->buttons[layer][i]);
    }
    for (int i = 0; i < 2; i++) {
        if (snapshot->axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER + i] > SDL_clamp(config->trigger_threshold, 0, 32766)) {
            controller_target(output, config->triggers[i]);
        }
    }
    int camera_axis = config->swap_sticks ? SDL_GAMEPAD_AXIS_RIGHTX : SDL_GAMEPAD_AXIS_LEFTX;
    int cursor_axis = config->swap_sticks ? SDL_GAMEPAD_AXIS_LEFTX : SDL_GAMEPAD_AXIS_RIGHTX;
    int deadzone = SDL_clamp(config->camera_deadzone, 0, 30000);
    if (snapshot->axes[camera_axis] < -deadzone) controller_target(output, config->camera[0]);
    if (snapshot->axes[camera_axis] > deadzone) controller_target(output, config->camera[1]);
    if (snapshot->axes[camera_axis + 1] < -deadzone) controller_target(output, config->camera[2]);
    if (snapshot->axes[camera_axis + 1] > deadzone) controller_target(output, config->camera[3]);
    deadzone = SDL_clamp(config->cursor_deadzone, 0, 30000);
    float x = controller_axis(snapshot->axes[cursor_axis], deadzone);
    float y = controller_axis(snapshot->axes[cursor_axis + 1], deadzone);
    float length = sqrtf(x * x + y * y);
    // A smooth response near the center and bounded diagonal speed.
    if (length > 0.0f) {
        float scale = SDL_min(length, 1.0f) * SDL_min(length, 1.0f) / length;
        float speed = (float)SDL_clamp(config->cursor_speed, 50, 3000);
        if (layer) speed *= SDL_clamp(config->precision_percent, 5, 100) / 100.0f;
        output->cursor_x = x * scale * speed;
        output->cursor_y = y * scale * speed;
    }
}

bool tig_controller_mouse_held(int button)
{
    if (button == TIG_MOUSE_BUTTON_LEFT) return controller_output.mouse[0];
    if (button == TIG_MOUSE_BUTTON_RIGHT) return controller_output.mouse[1];
    return false;
}

void tig_controller_frame(const TigControllerConfig* config,
    const TigControllerSnapshot* snapshot, bool allowed, bool* needs_neutral,
    TigControllerOutput* output)
{
    *output = (TigControllerOutput){ 0 };
    if (!config->enabled || snapshot == NULL || !allowed) {
        *needs_neutral = true;
        return;
    }
    if (*needs_neutral) {
        for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) if (snapshot->buttons[i]) return;
        int deadzone = SDL_min(SDL_clamp(config->cursor_deadzone, 0, 30000), SDL_clamp(config->camera_deadzone, 0, 30000));
        for (int i = 0; i < 4; i++) if (SDL_abs((int)snapshot->axes[i]) > deadzone) return;
        for (int i = 4; i < SDL_GAMEPAD_AXIS_COUNT; i++) if (snapshot->axes[i] > SDL_clamp(config->trigger_threshold, 0, 32766)) return;
        *needs_neutral = false;
    }
    tig_controller_build_output(config, snapshot, output);
}

static void controller_apply(const TigControllerOutput* next)
{
    TigControllerOutput previous = controller_output;
    controller_output = *next;
    for (int i = 1; i < SDL_SCANCODE_COUNT; i++) {
        if (previous.keys[i] != next->keys[i]) tig_kb_set_controller_key((SDL_Scancode)i, next->keys[i]);
    }
    Uint32 physical = SDL_GetMouseState(NULL, NULL);
    for (int i = 0; i < 2; i++) {
        Uint32 mask = i == 0 ? SDL_BUTTON_LMASK : SDL_BUTTON_RMASK;
        int button = i == 0 ? TIG_MOUSE_BUTTON_LEFT : TIG_MOUSE_BUTTON_RIGHT;
        if (previous.mouse[i] != next->mouse[i] && !(physical & mask)) tig_mouse_set_button(button, next->mouse[i]);
    }
}

void tig_controller_devices_changed(void)
{
    controller_next_scan = 0;
}

void tig_controller_configure(const TigControllerConfig* config)
{
    controller_apply(&(TigControllerOutput){ 0 });
    controller_config = *config;
    controller_needs_neutral = true;
    controller_last_tick = 0;
    controller_next_scan = 0;
    if (config->enabled && !controller_initialized) {
        controller_initialized = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        if (!controller_initialized) tig_debug_printf("Controller initialization failed: %s\n", SDL_GetError());
    }
}

void tig_controller_ping(bool blocked)
{
    Uint64 now = SDL_GetTicks();
    float elapsed = controller_last_tick != 0 ? SDL_min(now - controller_last_tick, 50) / 1000.0f : 0.0f;
    controller_last_tick = now;
    if (controller != NULL && !SDL_GamepadConnected(controller)) {
        controller_apply(&(TigControllerOutput){ 0 });
        SDL_CloseGamepad(controller);
        controller = NULL;
        controller_needs_neutral = true;
        controller_next_scan = 0;
    }
    if (controller_config.enabled && controller_initialized && controller == NULL && now >= controller_next_scan) {
        controller_next_scan = now + 1000;
        int count = 0;
        SDL_JoystickID* ids = SDL_GetGamepads(&count);
        for (int i = 0; i < count && controller == NULL; i++) controller = SDL_OpenGamepad(ids[i]);
        SDL_free(ids);
        if (controller != NULL) tig_debug_printf("Controller connected: %s\n", SDL_GetGamepadName(controller));
    }
    SDL_Window* window = NULL;
    bool focused = tig_video_window_get(&window) == TIG_OK && (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;
    if (!controller_config.enabled || controller == NULL || blocked || !focused) {
        TigControllerOutput released;
        tig_controller_frame(&controller_config, NULL, false, &controller_needs_neutral, &released);
        controller_apply(&released);
        controller_remainder_x = controller_remainder_y = 0.0f;
        return;
    }
    TigControllerSnapshot snapshot = { 0 };
    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) snapshot.buttons[i] = SDL_GetGamepadButton(controller, (SDL_GamepadButton)i);
    for (int i = 0; i < SDL_GAMEPAD_AXIS_COUNT; i++) snapshot.axes[i] = SDL_GetGamepadAxis(controller, (SDL_GamepadAxis)i);
    TigControllerOutput next;
    tig_controller_frame(&controller_config, &snapshot, true, &controller_needs_neutral, &next);
    // Move first so a simultaneous click lands at the new cursor position.
    controller_remainder_x += next.cursor_x * elapsed;
    controller_remainder_y += next.cursor_y * elapsed;
    int dx = (int)controller_remainder_x, dy = (int)controller_remainder_y;
    controller_remainder_x -= dx;
    controller_remainder_y -= dy;
    if (dx != 0 || dy != 0) {
        TigMouseState mouse;
        SDL_Renderer* renderer;
        int width, height;
        if (tig_mouse_get_state(&mouse) == TIG_OK && tig_video_renderer_get(&renderer) == TIG_OK
            && SDL_GetRenderLogicalPresentation(renderer, &width, &height, NULL)) {
            int x = SDL_clamp(mouse.x + dx, 0, width - 1);
            int y = SDL_clamp(mouse.y + dy, 0, height - 1);
            tig_mouse_set_position(x, y);
            float window_x, window_y;
            if (SDL_RenderCoordinatesToWindow(renderer, (float)x, (float)y, &window_x, &window_y)) SDL_WarpMouseInWindow(window, window_x, window_y);
        }
    }
    controller_apply(&next);
}

void tig_controller_exit(void)
{
    controller_apply(&(TigControllerOutput){ 0 });
    if (controller != NULL) SDL_CloseGamepad(controller);
    controller = NULL;
    if (controller_initialized) SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    controller_initialized = false;
    controller_config = (TigControllerConfig){ 0 };
}
