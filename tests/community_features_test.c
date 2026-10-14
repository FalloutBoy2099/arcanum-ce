#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/gamelib.h"
#include "game/highres_config.h"
#include "game/input_bindings.h"
#include "game/iso_zoom.h"
#include "game/obj_id.h"

Settings settings;
static int invalidations;
static int failures;

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); failures++; } } while (0)

void gamelib_invalidate_rect(TigRect* rect)
{
    (void)rect;
    invalidations++;
}

// Object-valued settings are outside the scope of these tests.
void objid_id_to_str(char* buffer, ObjectID oid)
{
    (void)oid;
    buffer[0] = '\0';
}

bool objid_id_from_str(ObjectID* oid, const char* str)
{
    (void)str;
    memset(oid, 0, sizeof(*oid));
    return false;
}

static void test_ini_round_trip(void)
{
    FILE* stream = fopen("arcanum.ini", "wt");
    CHECK(stream != NULL);
    if (stream == NULL) return;
    fputs("[video]\nscroll_using_borders=0\ncamera_follows_player=1\nvsync_mode=2\nzoom_min=0.5\nzoom_max=1.75\nzoom_enabled=1\nzoom_filter=nearest\n"
          "[ui]\nhud_stage=2\ndialogue_camera_mode=2\n"
          "dialogue_camera_zoom_to_fit=1\ndialogue_camera_tween_back=1\ntranslucent_black_ui=1\n"
          "dialogue_option_numbers=1\ndialogue_emote_dim=1\ndialogue_emote_dim_choices=1\ndialogue_emote_dim_percent=60\n"
          "[game]\nautosave_travel=0\nautosave_sleep=1\n[bindings]\nzoom_reset=Keypad 0\ncycle_hud=TAB\n", stream);
    fclose(stream);
    settings_init(&settings, "arcanum.ini");
    settings_load(&settings);
    CHECK(settings_get_value(&settings, VSYNC_MODE_KEY) == 2);
    CHECK(settings_get_value(&settings, "camera follows player") == 1);
    CHECK(settings_get_value(&settings, AUTOSAVE_TRAVEL_KEY) == 0);
    CHECK(settings_get_value(&settings, AUTOSAVE_SLEEP_KEY) == 1);
    CHECK(strcmp(settings_get_str_value(&settings, ZOOM_FILTER_KEY), "nearest") == 0);
    settings_set_value(&settings, VSYNC_MODE_KEY, 1);
    settings_save(&settings);
    CHECK((settings.flags & SETTINGS_CHANGED) == 0);
    stream = fopen("arcanum.ini", "rt");
    CHECK(stream != NULL);
    if (stream != NULL) {
        char line[256];
        while (fgets(line, sizeof(line), stream) != NULL) {
            CHECK(line[0] != ';' && line[0] != '#');
        }
        fclose(stream);
    }
    settings_exit(&settings);
    settings_init(&settings, "arcanum.ini");
    settings_load(&settings);
    CHECK(settings_get_value(&settings, VSYNC_MODE_KEY) == 1);
    CHECK(settings_get_value(&settings, SCROLL_USING_BORDERS_KEY) == 0);
    CHECK(settings_get_value(&settings, "hud_stage") == 2);
    CHECK(settings_get_value(&settings, "dialogue_camera_tween_back") == 1);
    CHECK(settings_get_value(&settings, "dialogue_emote_dim_percent") == 60);
    CHECK(strcmp(settings_get_str_value(&settings, "zoom_reset"), "Keypad 0") == 0);
}

static void test_startup_resolution(void)
{
    CHECK(SDL_CreateDirectory("HighRes"));
    FILE* stream = fopen("HighRes/config.ini", "wt");
    CHECK(stream != NULL);
    if (stream == NULL) return;
    fputs("Width=1280\nHeight=720\n", stream);
    fclose(stream);
    const char* values[] = { "1920x1080", "0x0", "640x480", "99999x99999", "1920x1080junk" };
    for (int i = 0; i < SDL_arraysize(values); i++) {
        stream = fopen("arcanum.ini", "wt");
        CHECK(stream != NULL);
        if (stream == NULL) return;
        fprintf(stream, "[video]\nresolution=%s\n", values[i]);
        fclose(stream);
        highres_config_load();
        CHECK(highres_config_get()->width == (i == 0 ? 1920 : 1280));
        CHECK(highres_config_get()->height == (i == 0 ? 1080 : 720));
    }
    stream = fopen("arcanum.ini", "wt");
    CHECK(stream != NULL);
    if (stream != NULL) {
        fputs("[ui]\nresolution=1920x1080\n", stream);
        fclose(stream);
    }
    highres_config_load();
    CHECK(highres_config_get()->width == 1280);
    CHECK(highres_config_get()->height == 720);
}

static void test_bindings(void)
{
    TigMessage message = { 0 };
    message.type = TIG_MESSAGE_KEYBOARD;
    input_bindings_init();
    message.data.keyboard.scancode = SDL_SCANCODE_EQUALS;
    CHECK(input_binding_matches_keyboard(INPUT_ACTION_ZOOM_IN, &message));
    message.data.keyboard.scancode = SDL_SCANCODE_KP_PLUS;
    CHECK(input_binding_matches_keyboard(INPUT_ACTION_ZOOM_IN, &message));
    message.data.keyboard.scancode = SDL_SCANCODE_KP_MINUS;
    CHECK(input_binding_matches_keyboard(INPUT_ACTION_ZOOM_OUT, &message));
    message.data.keyboard.scancode = SDL_SCANCODE_KP_0;
    CHECK(input_binding_matches_keyboard(INPUT_ACTION_ZOOM_RESET, &message));
    settings_set_str_value(&settings, "cycle_hud", "F11");
    input_bindings_init();
    message.data.keyboard.scancode = SDL_SCANCODE_TAB;
    CHECK(!input_binding_matches_keyboard(INPUT_ACTION_CYCLE_HUD, &message));
    message.data.keyboard.scancode = SDL_SCANCODE_F11;
    CHECK(input_binding_matches_keyboard(INPUT_ACTION_CYCLE_HUD, &message));
}

static void test_zoom(void)
{
    iso_zoom_init();
    for (int i = 0; i < 30; i++) iso_zoom_step_out();
    CHECK(fabsf(iso_zoom_target() - 0.5f) < 0.001f);
    for (int i = 0; i < 100; i++) iso_zoom_ping();
    CHECK(!iso_zoom_is_animating());
    CHECK(fabsf(iso_zoom_current() - 0.5f) < 0.001f);
    CHECK(invalidations > 0);
    for (int i = 0; i < 30; i++) iso_zoom_step_in();
    CHECK(fabsf(iso_zoom_target() - 1.75f) < 0.001f);
    settings_set_value(&settings, ISO_ZOOM_ENABLED_KEY, 0);
    CHECK(!iso_zoom_is_available());
    iso_zoom_step_out();
    iso_zoom_set_target(0.5f);
    CHECK(iso_zoom_target() == 1.0f);
    settings_set_value(&settings, ISO_ZOOM_ENABLED_KEY, 1);
    iso_zoom_set_available(false);
    iso_zoom_step_in();
    CHECK(iso_zoom_current() == 1.0f);
    iso_zoom_set_available(true);
    settings_set_str_value(&settings, ISO_ZOOM_MIN_KEY, "0.1");
    iso_zoom_set_target(0.1f);
    CHECK(iso_zoom_target() >= 0.5f);
    settings_set_str_value(&settings, ISO_ZOOM_MAX_KEY, "nan");
    iso_zoom_step_in();
    CHECK(isfinite(iso_zoom_target()));
    settings_set_str_value(&settings, ISO_ZOOM_MAX_KEY, "99");
    for (int i = 0; i < 50; i++) iso_zoom_step_in();
    CHECK(iso_zoom_target() <= 3.0f);
    iso_zoom_reset();
    CHECK(iso_zoom_current() == 1.0f);
}

static void test_modern_brackets(const char* path, int width)
{
    if (path == NULL || path[0] == '\0') return;
    TigVideoBufferCreateInfo info = { 0 };
    info.flags = TIG_VIDEO_BUFFER_CREATE_SYSTEM_MEMORY;
    info.width = width;
    info.height = 160;
    TigVideoBuffer* normal = NULL;
    TigVideoBuffer* dimmed = NULL;
    CHECK(tig_video_buffer_create(&info, &normal) == TIG_OK);
    CHECK(tig_video_buffer_create(&info, &dimmed) == TIG_OK);
    if (normal == NULL || dimmed == NULL) return;
    TigFont font = { 0 };
    font.flags = TIG_FONT_MODERN | TIG_FONT_DIM_BRACKETS;
    font.modern_font_path = path;
    font.modern_font_size = 20;
    font.color = tig_color_make(255, 255, 255);
    TigRect rect = { 0, 0, width, 160 };
    TigRect dirty;
    tig_font_handle_t handle;
    font.bracket_dim_percent = 100;
    tig_font_create(&font, &handle);
    tig_font_push(handle);
    CHECK(tig_font_write(normal, "Hello [action with words] world", &rect, &dirty) == TIG_OK);
    tig_font_pop();
    tig_font_destroy(handle);
    font.bracket_dim_percent = 60;
    tig_font_create(&font, &handle);
    tig_font_push(handle);
    CHECK(tig_font_write(dimmed, "Hello [action with words] world", &rect, &dirty) == TIG_OK);
    tig_font_pop();
    tig_font_destroy(handle);
    int darker = 0;
    int unchanged_ink = 0;
    TigVideoBufferData normal_data, dimmed_data;
    CHECK(tig_video_buffer_lock(normal) == TIG_OK);
    CHECK(tig_video_buffer_lock(dimmed) == TIG_OK);
    CHECK(tig_video_buffer_data(normal, &normal_data) == TIG_OK);
    CHECK(tig_video_buffer_data(dimmed, &dimmed_data) == TIG_OK);
    for (int y = 0; y < info.height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned int before = *(Uint32*)((Uint8*)normal_data.pixels + y * normal_data.pitch + x * 4);
            unsigned int after = *(Uint32*)((Uint8*)dimmed_data.pixels + y * dimmed_data.pitch + x * 4);
            if ((before & 0x00FFFFFFu) > (after & 0x00FFFFFFu)) darker++;
            if ((before & 0x00FFFFFFu) != 0 && before == after) unchanged_ink++;
        }
    }
    tig_video_buffer_unlock(normal);
    tig_video_buffer_unlock(dimmed);
    CHECK(darker > 0);
    CHECK(unchanged_ink > 0);
    tig_video_buffer_destroy(normal);
    tig_video_buffer_destroy(dimmed);
}

int main(int argc, char** argv)
{
    CHECK(SDL_Init(0));
    tig_memory_init(NULL);
    tig_color_init(NULL);
    tig_color_set_rgba_settings(0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    test_ini_round_trip();
    test_startup_resolution();
    test_bindings();
    test_zoom();
    if (argc > 1) {
        test_modern_brackets(argv[1], 400);
        test_modern_brackets(argv[1], 90);
    }
    settings_exit(&settings);
    tig_color_exit();
    tig_memory_exit();
    SDL_Quit();
    printf("Community feature regressions: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures != 0;
}
