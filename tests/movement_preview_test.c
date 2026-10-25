#include <stdio.h>
#include <string.h>

#include "ui/movement_preview.h"
#include "game/anim.h"
#include "game/combat.h"
#include "game/gamelib.h"
#include "game/iso_zoom.h"
#include "game/location.h"
#include "game/modern_font.h"
#include "game/ui.h"

static int failures;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #value); failures++; } } while (0)

static int search_count, invalidations, opacity = 70;
static bool blocked, turn_based;
static float zoom = 1.0f;
static int64_t character_location;
static int offset_x;

int64_t obj_field_int64_get(int64_t obj, int field) { (void)obj; (void)field; return character_location; }
int obj_field_int32_get(int64_t obj, int field) { (void)obj; return field == OBJ_F_OFFSET_X ? offset_x : 0; }
bool combat_critter_is_combat_mode_active(int64_t obj) { (void)obj; return turn_based; }
bool combat_turn_based_is_active(void) { return turn_based; }
int combat_action_points_get(void) { return 2; }
bool get_always_run(int64_t obj) { (void)obj; return true; }
float iso_zoom_current(void) { return zoom; }
int modern_font_int_for(const char* key, int fallback, int minimum, int maximum)
{ (void)key; (void)fallback; (void)minimum; (void)maximum; return opacity; }
tig_color_t modern_font_color_for(const char* key, int red, int green, int blue)
{ (void)key; return tig_color_make(red, green, blue); }
void gamelib_invalidate_rect(TigRect* rect) { CHECK(rect->width > 0 && rect->height > 0); invalidations++; }
void location_xy(int64_t loc, int64_t* x, int64_t* y)
{ *x = 100 + 40 * (LOCATION_GET_Y(loc) - LOCATION_GET_X(loc) - 1); *y = 50 + 20 * (LOCATION_GET_Y(loc) + LOCATION_GET_X(loc)); }
bool location_in_dir(int64_t loc, int direction, int64_t* next)
{ CHECK(direction == 3); *next = location_make(LOCATION_GET_X(loc), LOCATION_GET_Y(loc) + 1); return true; }
int anim_path_preview(int64_t obj, int64_t destination, int8_t* rotations, int capacity)
{
    (void)obj;
    search_count++;
    if (blocked) return 0;
    int steps = (int)(LOCATION_GET_Y(destination) - LOCATION_GET_Y(character_location));
    CHECK(steps > 0 && steps <= capacity);
    memset(rotations, 3, steps);
    return steps;
}

static tig_color_t pixel(TigVideoBuffer* buffer, int x, int y)
{
    tig_color_t color = 0;
    CHECK(tig_video_buffer_get_pixel_color(buffer, x, y, &color) == TIG_OK);
    return color;
}

int main(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 77; }
    CHECK(tig_memory_init(NULL) == TIG_OK);
    CHECK(tig_rect_init(NULL) == TIG_OK);
    CHECK(tig_color_init(NULL) == TIG_OK);
    CHECK(tig_color_set_rgba_settings(0xff0000, 0xff00, 0xff, 0xff000000) == TIG_OK);
    TigInitInfo init = { 0 };
    init.width = 800; init.height = 600; init.bpp = 32;
    init.flags = TIG_INITIALIZE_WINDOWED | TIG_INITIALIZE_HIDDEN;
    if (tig_video_init(&init) != TIG_OK) {
        tig_color_exit(); tig_rect_exit(); tig_memory_exit(); SDL_Quit(); return 77;
    }
    CHECK(tig_window_init(&init) == TIG_OK);
    TigWindowData data = { 0 };
    data.rect = (TigRect){ 0, 0, 640, 480 };
    data.background_color = tig_color_make(20, 30, 40);
    tig_window_handle_t window;
    CHECK(tig_window_create(&data, &window) == TIG_OK);
    TigVideoBuffer* buffer;
    CHECK(tig_window_vbid_get(window, &buffer) == TIG_OK);
    TigRectListNode node = { { 0, 0, 640, 480 }, NULL };
    TigRectListNode* head = &node;
    GameDrawInfo draw = { 0 };
    draw.rects = &head;
    int64_t destination = location_make(0, 4);
    movement_preview_update(window, 1, destination);
    CHECK(search_count == 1 && invalidations == 1);
    movement_preview_draw(&draw);
    tig_color_t green = tig_color_make(144, 216, 160);
    CHECK(pixel(buffer, 120, 80) == tig_color_blend_alpha(green, data.background_color, 70 * 255 / 100));
    CHECK(pixel(buffer, 10, 10) == data.background_color);
    movement_preview_update(window, 1, destination);
    CHECK(search_count == 1 && invalidations == 1);
    movement_preview_clear();
    CHECK(invalidations == 2);
    tig_window_fill(window, &data.rect, data.background_color);
    movement_preview_draw(&draw);
    CHECK(pixel(buffer, 120, 80) == data.background_color);

    movement_preview_update(window, 1, destination);
    CHECK(search_count == 1);
    turn_based = true;
    SDL_Delay(210);
    movement_preview_update(window, 1, destination);
    CHECK(search_count == 2);
    tig_window_fill(window, &data.rect, data.background_color);
    movement_preview_draw(&draw);
    CHECK(pixel(buffer, 120, 80) == tig_color_blend_alpha(green, data.background_color, 70 * 255 / 100));
    tig_color_t amber = tig_color_make(216, 176, 96);
    CHECK(pixel(buffer, 200, 120) == tig_color_blend_alpha(amber, data.background_color, 70 * 255 / 100));

    node.rect = (TigRect){ 0, 0, 130, 480 };
    tig_window_fill(window, &data.rect, data.background_color);
    movement_preview_draw(&draw);
    CHECK(pixel(buffer, 120, 80) != data.background_color);
    CHECK(pixel(buffer, 200, 120) == data.background_color);
    node.rect = data.rect;

    movement_preview_destroy();
    blocked = true;
    movement_preview_update(window, 1, destination);
    tig_window_fill(window, &data.rect, data.background_color);
    movement_preview_draw(&draw);
    tig_color_t red = tig_color_make(240, 128, 128);
    CHECK(pixel(buffer, 260, 145) == tig_color_blend_alpha(red, data.background_color, 70 * 255 / 100));
    CHECK(pixel(buffer, 120, 80) == data.background_color);

    movement_preview_destroy();
    blocked = false; turn_based = false; zoom = 0.5f;
    movement_preview_update(window, 1, destination);
    tig_window_fill(window, &data.rect, data.background_color);
    movement_preview_draw(&draw);
    CHECK(pixel(buffer, 220, 160) == tig_color_blend_alpha(green, data.background_color, 70 * 255 / 100));
    CHECK(pixel(buffer, 120, 80) == data.background_color);
    opacity = 0;
    movement_preview_update(window, 1, destination);
    tig_window_fill(window, &data.rect, data.background_color);
    movement_preview_draw(&draw);
    CHECK(pixel(buffer, 220, 160) == data.background_color);
    movement_preview_destroy();
    opacity = 100; zoom = 1.0f;
    int searches_before = search_count;
    movement_preview_update(window, 1, destination);
    CHECK(search_count == searches_before + 1);
    character_location = location_make(0, 1);
    movement_preview_update(window, 1, destination);
    CHECK(search_count == searches_before + 1);
    tig_window_fill(window, &data.rect, data.background_color);
    movement_preview_draw(&draw);
    CHECK(pixel(buffer, 120, 80) == data.background_color);
    CHECK(pixel(buffer, 160, 100) == green);
    tig_window_handle_t at_position;
    CHECK(tig_window_get_at_position(160, 100, &at_position) == TIG_OK && at_position == window);
    movement_preview_destroy();
    character_location = 0;
    destination = location_make(0, 200);
    movement_preview_update(window, 1, destination);
    tig_window_fill(window, &data.rect, data.background_color);
    movement_preview_draw(&draw);
    CHECK(pixel(buffer, 620, 330) == green);
    CHECK(pixel(buffer, 10, 10) == data.background_color);
    movement_preview_destroy();
    tig_window_destroy(window);
    tig_window_exit();
    tig_video_exit();
    tig_color_exit();
    tig_rect_exit();
    tig_memory_exit();
    SDL_Quit();
    printf("Movement preview: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures != 0;
}
