#include "ui/search_bar.h"

bool ui_panel_window_height(tig_window_handle_t* handle, int height)
{
    TigWindowData data;
    if (height <= 0 || tig_window_data(*handle, &data) != TIG_OK) return false;
    if (data.rect.height == height) return true;
    data.rect.height = height;
    tig_window_handle_t replacement;
    if (tig_window_create(&data, &replacement) != TIG_OK) return false;
    tig_window_destroy(*handle);
    *handle = replacement;
    return true;
}

void ui_panel_background_draw(tig_window_handle_t handle, int art_num, int x, int y)
{
    TigArtBlitInfo blit = { 0 };
    TigArtFrameData frame;
    tig_art_interface_id_create(art_num, 0, 0, 0, &blit.art_id);
    if (tig_art_frame_data(blit.art_id, &frame) != TIG_OK) return;
    TigRect source = { 0, 0, frame.width, frame.height };
    TigRect destination = { x, y, frame.width, frame.height };
    blit.src_rect = &source;
    blit.dst_rect = &destination;
    tig_window_blit_art(handle, &blit);
}

bool ui_search_bar_open(tig_window_handle_t* handle, tig_window_handle_t parent,
    TigRect relative_rect, TigWindowMessageFilterFunc filter)
{
    TigWindowData data;
    if (tig_window_data(parent, &data) != TIG_OK) return false;
    relative_rect.x += data.rect.x;
    relative_rect.y += data.rect.y;
    if (*handle == TIG_WINDOW_HANDLE_INVALID) {
        TigWindowData toolbar = { 0 };
        toolbar.flags = TIG_WINDOW_MESSAGE_FILTER;
        toolbar.rect = relative_rect;
        toolbar.background_color = tig_color_make(105, 76, 35);
        toolbar.message_filter = filter;
        if (tig_window_create(&toolbar, handle) != TIG_OK) return false;
    } else if (tig_window_data(*handle, &data) == TIG_OK
        && (data.rect.x != relative_rect.x || data.rect.y != relative_rect.y)) {
        tig_window_move(*handle, relative_rect.x, relative_rect.y);
    }
    tig_window_show(*handle);
    return true;
}

void ui_search_bar_close(tig_window_handle_t* handle)
{
    if (*handle != TIG_WINDOW_HANDLE_INVALID) tig_window_destroy(*handle);
    *handle = TIG_WINDOW_HANDLE_INVALID;
}

void ui_search_bar_cell(tig_window_handle_t handle, TigRect rect, tig_font_handle_t font,
    const char* text, bool selected)
{
    tig_window_fill(handle, &rect, tig_color_make(105, 76, 35));
    rect.x++; rect.y++; rect.width -= 2; rect.height -= 2;
    tig_window_fill(handle, &rect, selected ? tig_color_make(180, 155, 105) : tig_color_make(210, 190, 145));
    rect.x += 3; rect.y += 2; rect.width -= 6; rect.height -= 4;
    tig_font_push(font);
    tig_window_text_write(handle, text, &rect);
    tig_font_pop();
}

bool ui_search_bar_contains(const TigRect* rect, int x, int y)
{
    return x >= rect->x && x < rect->x + rect->width && y >= rect->y && y < rect->y + rect->height;
}
