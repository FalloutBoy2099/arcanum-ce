#include "game/floating_background.h"

#include "game/modern_font.h"
#include "game/ui.h"

int floating_margin_horizontal(void)
{
    return modern_font_int_for(FLOATING_MARGIN_HORIZONTAL_KEY, 16, 0, 80);
}

int floating_margin_vertical(void)
{
    return modern_font_int_for(FLOATING_MARGIN_VERTICAL_KEY, 8, 0, 80);
}

TigRect floating_background_pad(TigRect text, TigRect buffer_bounds)
{
    if (text.width <= 0 || text.height <= 0) return (TigRect){ 0 };
    int horizontal = floating_margin_horizontal();
    int vertical = floating_margin_vertical();
    TigRect padded = { text.x - horizontal, text.y - vertical,
        text.width + horizontal * 2, text.height + vertical * 2 };
    if (tig_rect_intersection(&padded, &buffer_bounds, &padded) != TIG_OK) return (TigRect){ 0 };
    return padded;
}

void floating_background_draw(tig_window_handle_t window, TigRect* rect, uint8_t text_opacity)
{
    if (window == TIG_WINDOW_HANDLE_INVALID || rect == NULL || rect->width <= 0 || rect->height <= 0) return;
    int percent = modern_font_int_for(FLOATING_BACKGROUND_OPACITY_KEY, 60, 0, 100);
    int alpha = (percent * text_opacity + 50) / 100;
    if (alpha == 0) return;
    TigVideoBuffer* buffer;
    if (tig_window_vbid_get(window, &buffer) != TIG_OK) return;
    tig_color_t color = modern_font_color_for(FLOATING_BACKGROUND_COLOR_KEY, 0, 0, 0);
    if (alpha == 255) {
        tig_video_buffer_fill(buffer, rect, color);
    } else {
        tig_video_buffer_blend_color(buffer, rect, color, alpha);
    }
}
