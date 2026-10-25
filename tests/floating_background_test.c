#include <stdio.h>
#include <string.h>

#include "game/floating_background.h"
#include "game/modern_font.h"
#include "game/ui.h"

static int failures;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #value); failures++; } } while (0)
static int opacity = 60;
static int horizontal_margin = 16;
static int vertical_margin = 8;
static tig_color_t background_color;

int modern_font_int_for(const char* key, int fallback, int minimum, int maximum)
{
    if (strcmp(key, FLOATING_MARGIN_HORIZONTAL_KEY) == 0) {
        CHECK(fallback == 16); return SDL_clamp(horizontal_margin, minimum, maximum);
    }
    if (strcmp(key, FLOATING_MARGIN_VERTICAL_KEY) == 0) {
        CHECK(fallback == 8); return SDL_clamp(vertical_margin, minimum, maximum);
    }
    CHECK(strcmp(key, FLOATING_BACKGROUND_OPACITY_KEY) == 0 && fallback == 60);
    return SDL_clamp(opacity, minimum, maximum);
}

tig_color_t modern_font_color_for(const char* key, int red, int green, int blue)
{
    CHECK(strcmp(key, FLOATING_BACKGROUND_COLOR_KEY) == 0 && red == 0 && green == 0 && blue == 0);
    return background_color;
}

static tig_color_t pixel(TigVideoBuffer* buffer, int x, int y)
{
    tig_color_t color = 0;
    CHECK(tig_video_buffer_get_pixel_color(buffer, x, y, &color) == TIG_OK);
    return color;
}

int main(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) return 77;
    CHECK(tig_memory_init(NULL) == TIG_OK);
    CHECK(tig_rect_init(NULL) == TIG_OK);
    CHECK(tig_color_init(NULL) == TIG_OK);
    TigInitInfo init = { 0 };
    init.width = 800; init.height = 600; init.bpp = 32;
    init.flags = TIG_INITIALIZE_WINDOWED | TIG_INITIALIZE_HIDDEN;
    if (tig_video_init(&init) != TIG_OK) {
        tig_color_exit(); tig_rect_exit(); tig_memory_exit(); SDL_Quit(); return 77;
    }
    CHECK(tig_window_init(&init) == TIG_OK);
    TigWindowData data = { 0 };
    data.rect = (TigRect){ 0, 0, 800, 600 };
    data.background_color = tig_color_make(160, 120, 80);
    background_color = tig_color_make(0, 0, 0);
    tig_window_handle_t window;
    CHECK(tig_window_create(&data, &window) == TIG_OK);
    TigVideoBuffer* buffer;
    CHECK(tig_window_vbid_get(window, &buffer) == TIG_OK);
    TigRect text = { 30, 40, 200, 60 };
    TigRect padded = floating_background_pad(text, data.rect);
    CHECK(padded.x == 14 && padded.y == 32 && padded.width == 232 && padded.height == 76);
    horizontal_margin = 0; vertical_margin = 0;
    padded = floating_background_pad(text, data.rect);
    CHECK(memcmp(&padded, &text, sizeof(text)) == 0);
    horizontal_margin = 80; vertical_margin = 80;
    padded = floating_background_pad(text, data.rect);
    CHECK(padded.x == 0 && padded.y == 0 && padded.width == 310 && padded.height == 180);
    TigRect empty = { 0 };
    padded = floating_background_pad(empty, data.rect);
    CHECK(padded.width == 0 && padded.height == 0);
    horizontal_margin = 16; vertical_margin = 8;
    padded = floating_background_pad(text, data.rect);
    floating_background_draw(window, &padded, 255);
    CHECK(pixel(buffer, 14, 32) != data.background_color);
    CHECK(pixel(buffer, 13, 32) == data.background_color);
    tig_window_fill(window, &data.rect, data.background_color);
    for (int percent = 0; percent <= 100; percent += 10) {
        for (int fade = 0; fade <= 255; fade += 51) {
            opacity = percent;
            tig_window_fill(window, &data.rect, data.background_color);
            floating_background_draw(window, &text, (uint8_t)fade);
            int alpha = (percent * fade + 50) / 100;
            tig_color_t expected = alpha == 255 ? background_color
                : alpha == 0 ? data.background_color : tig_color_blend_alpha(background_color, data.background_color, alpha);
            CHECK(pixel(buffer, 30, 40) == expected);
            CHECK(pixel(buffer, 229, 99) == expected);
            CHECK(pixel(buffer, 29, 40) == data.background_color);
            CHECK(pixel(buffer, 230, 99) == data.background_color);
        }
    }
    opacity = 100;
    background_color = tig_color_make(30, 60, 90);
    tig_window_fill(window, &data.rect, data.background_color);
    floating_background_draw(window, &text, 255);
    CHECK(pixel(buffer, 50, 50) == background_color);
    TigRect clipped = { -10, -10, 30, 30 };
    floating_background_draw(window, &clipped, 255);
    CHECK(pixel(buffer, 0, 0) == background_color);
    CHECK(pixel(buffer, 19, 19) == background_color);
    CHECK(pixel(buffer, 20, 20) == data.background_color);
    TigRect invalid = { 0, 0, -1, 30 };
    floating_background_draw(window, &invalid, 255);
    floating_background_draw(window, NULL, 255);
    floating_background_draw(TIG_WINDOW_HANDLE_INVALID, &text, 255);
    tig_window_destroy(window);
    tig_window_exit(); tig_video_exit(); tig_color_exit(); tig_rect_exit(); tig_memory_exit(); SDL_Quit();
    printf("Floating backgrounds: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures != 0;
}
