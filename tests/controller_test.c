#include <math.h>
#include <stdio.h>
#include <tig/controller.h>
#include <tig/kb.h>
#include <tig/memory.h>
#include <tig/message.h>

static int failures;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); failures++; } } while (0)

static TigControllerConfig config(void)
{
    TigControllerConfig c = { 0 };
    c.enabled = true;
    c.cursor_speed = 1200;
    c.cursor_deadzone = 8000;
    c.camera_deadzone = 12000;
    c.precision_percent = 45;
    c.trigger_threshold = 4500;
    c.buttons[0][SDL_GAMEPAD_BUTTON_SOUTH] = TIG_CONTROLLER_MOUSE_LEFT;
    c.buttons[0][SDL_GAMEPAD_BUTTON_NORTH] = SDL_SCANCODE_R;
    c.buttons[1][SDL_GAMEPAD_BUTTON_NORTH] = SDL_SCANCODE_F7;
    c.triggers[0] = TIG_CONTROLLER_MOUSE_RIGHT;
    c.triggers[1] = TIG_CONTROLLER_MOUSE_LEFT;
    c.camera[0] = SDL_SCANCODE_LEFT;
    c.camera[1] = SDL_SCANCODE_RIGHT;
    c.camera[2] = SDL_SCANCODE_UP;
    c.camera[3] = SDL_SCANCODE_DOWN;
    return c;
}

static void test_controls(void)
{
    TigControllerConfig c = config();
    TigControllerSnapshot s = { 0 };
    TigControllerOutput out;
    tig_controller_build_output(&c, &s, &out);
    CHECK(out.cursor_x == 0 && out.cursor_y == 0 && !out.mouse[0]);
    s.axes[SDL_GAMEPAD_AXIS_RIGHTX] = 8000;
    s.axes[SDL_GAMEPAD_AXIS_LEFTY] = -12000;
    tig_controller_build_output(&c, &s, &out);
    CHECK(out.cursor_x == 0 && !out.keys[SDL_SCANCODE_UP]);
    s.axes[SDL_GAMEPAD_AXIS_RIGHTX] = 32767;
    s.axes[SDL_GAMEPAD_AXIS_LEFTY] = -32768;
    tig_controller_build_output(&c, &s, &out);
    CHECK(fabsf(out.cursor_x - 1200) < 0.1f && out.keys[SDL_SCANCODE_UP]);
    s.axes[SDL_GAMEPAD_AXIS_RIGHTY] = 32767;
    tig_controller_build_output(&c, &s, &out);
    CHECK(fabsf(sqrtf(out.cursor_x * out.cursor_x + out.cursor_y * out.cursor_y) - 1200) < 0.1f);
    s.buttons[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER] = true;
    s.buttons[SDL_GAMEPAD_BUTTON_NORTH] = true;
    tig_controller_build_output(&c, &s, &out);
    CHECK(out.keys[SDL_SCANCODE_F7] && !out.keys[SDL_SCANCODE_R]);
    CHECK(fabsf(sqrtf(out.cursor_x * out.cursor_x + out.cursor_y * out.cursor_y) - 540) < 0.1f);
    s = (TigControllerSnapshot){ 0 };
    s.buttons[SDL_GAMEPAD_BUTTON_SOUTH] = true;
    s.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = 32767;
    tig_controller_build_output(&c, &s, &out);
    CHECK(out.mouse[0]);
    s.buttons[SDL_GAMEPAD_BUTTON_SOUTH] = false;
    tig_controller_build_output(&c, &s, &out);
    CHECK(out.mouse[0]); // Releasing A must not release the held trigger.
    s.axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] = 4500;
    tig_controller_build_output(&c, &s, &out);
    CHECK(!out.mouse[0]);
    s.axes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER] = 4501;
    tig_controller_build_output(&c, &s, &out);
    CHECK(out.mouse[1]);
    s = (TigControllerSnapshot){ 0 };
    c.swap_sticks = true;
    s.axes[SDL_GAMEPAD_AXIS_LEFTX] = -32768;
    s.axes[SDL_GAMEPAD_AXIS_RIGHTY] = 32767;
    tig_controller_build_output(&c, &s, &out);
    CHECK(fabsf(out.cursor_x + 1200) < 0.1f && out.keys[SDL_SCANCODE_DOWN]);
    c.enabled = false;
    tig_controller_build_output(&c, &s, &out);
    CHECK(out.cursor_x == 0 && !out.keys[SDL_SCANCODE_DOWN]);
    c = config();
    c.cursor_deadzone = 999999;
    c.cursor_speed = -1;
    c.precision_percent = 1000;
    s = (TigControllerSnapshot){ 0 };
    s.axes[SDL_GAMEPAD_AXIS_RIGHTX] = 32767;
    tig_controller_build_output(&c, &s, &out);
    CHECK(fabsf(out.cursor_x - 50) < 0.1f);
}

static void test_focus_and_disconnect(void)
{
    TigControllerConfig c = config();
    TigControllerSnapshot s = { 0 };
    TigControllerOutput out;
    bool neutral = true;
    s.buttons[SDL_GAMEPAD_BUTTON_SOUTH] = true;
    tig_controller_frame(&c, &s, true, &neutral, &out);
    CHECK(neutral && !out.mouse[0]);
    s.buttons[SDL_GAMEPAD_BUTTON_SOUTH] = false;
    tig_controller_frame(&c, &s, true, &neutral, &out);
    CHECK(!neutral);
    s.buttons[SDL_GAMEPAD_BUTTON_SOUTH] = true;
    s.axes[SDL_GAMEPAD_AXIS_LEFTX] = 32767;
    tig_controller_frame(&c, &s, true, &neutral, &out);
    CHECK(out.mouse[0] && out.keys[SDL_SCANCODE_RIGHT]);
    tig_controller_frame(&c, &s, false, &neutral, &out);
    CHECK(neutral && !out.mouse[0] && !out.keys[SDL_SCANCODE_RIGHT]);
    tig_controller_frame(&c, &s, true, &neutral, &out);
    CHECK(neutral && !out.mouse[0]);
    s = (TigControllerSnapshot){ 0 };
    tig_controller_frame(&c, &s, true, &neutral, &out);
    CHECK(!neutral);
    tig_controller_frame(&c, NULL, true, &neutral, &out);
    CHECK(neutral && !out.mouse[0]);
}

static void test_key_holds(void)
{
    CHECK(tig_memory_init(NULL) == TIG_OK);
    CHECK(tig_message_init(NULL) == TIG_OK);
    CHECK(tig_kb_init(NULL) == TIG_OK);
    tig_kb_set_controller_key(SDL_SCANCODE_LEFT, true);
    CHECK(tig_kb_is_key_pressed(SDL_SCANCODE_LEFT));
    TigMessage message;
    CHECK(tig_message_dequeue(&message) == TIG_OK);
    CHECK(message.type == TIG_MESSAGE_KEYBOARD && message.data.keyboard.pressed
        && message.data.keyboard.scancode == SDL_SCANCODE_LEFT);
    tig_kb_set_controller_key(SDL_SCANCODE_LEFT, true);
    tig_kb_set_key(SDLK_LEFT, SDL_SCANCODE_LEFT, false);
    CHECK(tig_message_dequeue(&message) != TIG_OK);
    tig_kb_set_controller_key(SDL_SCANCODE_LEFT, false);
    CHECK(!tig_kb_is_key_pressed(SDL_SCANCODE_LEFT));
    CHECK(tig_message_dequeue(&message) == TIG_OK && !message.data.keyboard.pressed);
    tig_kb_exit();
    tig_message_exit();
    tig_memory_exit();
}

static void test_virtual_gamepad(void)
{
    CHECK(SDL_InitSubSystem(SDL_INIT_GAMEPAD));
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
    desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    desc.name = "CE test Xbox-style controller";
    SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
    CHECK(id != 0);
    SDL_Gamepad* gamepad = SDL_OpenGamepad(id);
    CHECK(gamepad != NULL);
    if (gamepad != NULL) {
        SDL_Joystick* joystick = SDL_GetGamepadJoystick(gamepad);
        CHECK(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true));
        CHECK(SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHTX, 32767));
        SDL_UpdateGamepads();
        CHECK(SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH));
        CHECK(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX) == 32767);
        CHECK(SDL_DetachVirtualJoystick(id));
        SDL_UpdateGamepads();
        CHECK(!SDL_GamepadConnected(gamepad));
        SDL_CloseGamepad(gamepad);
    }
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
}

int main(void)
{
    CHECK(SDL_Init(0));
    test_controls();
    test_focus_and_disconnect();
    test_key_holds();
    test_virtual_gamepad();
    SDL_Quit();
    return failures != 0;
}
