#include "ui/status_panel.h"
#include "ui/cache_key.h"
#include <string.h>

static uint64_t panel_style(tig_font_handle_t handle, tig_color_t background, tig_color_t border)
{
    uint64_t key = UI_CACHE_KEY_INIT;
    ui_cache_key_bytes(&key, &handle, sizeof(handle));
    ui_cache_key_bytes(&key, &background, sizeof(background));
    ui_cache_key_bytes(&key, &border, sizeof(border));
    if (handle != TIG_FONT_HANDLE_INVALID) {
        const TigFont* font = (const TigFont*)handle;
#define FIELD(name) ui_cache_key_bytes(&key, &font->name, sizeof(font->name))
        FIELD(flags); FIELD(art_id); FIELD(color); FIELD(underline_color);
        FIELD(strike_through_color); FIELD(scale); FIELD(modern_font);
        FIELD(modern_font_size); FIELD(modern_shadow_color); FIELD(modern_shadow_opacity);
        FIELD(bracket_dim_percent); FIELD(modern_shadow_distance);
        ui_cache_key_string(&key, font->modern_font_path);
#undef FIELD
    }
    return key;
}

bool ui_status_panel_draw(UiStatusPanel* panel, TigRect frame, const char* text,
    tig_font_handle_t font, tig_color_t background, tig_color_t border)
{
    if (panel->window == TIG_WINDOW_HANDLE_INVALID) {
        TigWindowData data = { 0 };
        data.flags = TIG_WINDOW_ALWAYS_ON_TOP;
        data.rect = frame;
        data.background_color = background;
        if (tig_window_create(&data, &panel->window) != TIG_OK) return false;
        panel->valid = false;
    } else {
        TigWindowData previous;
        tig_window_data(panel->window, &previous);
        if (previous.rect.width != frame.width || previous.rect.height != frame.height) {
            ui_status_panel_destroy(panel);
            return ui_status_panel_draw(panel, frame, text, font, background, border);
        }
        if (previous.rect.x != frame.x || previous.rect.y != frame.y) tig_window_move(panel->window, frame.x, frame.y);
    }
    uint64_t style = panel_style(font, background, border);
    bool changed = !panel->valid || panel->style != style || strcmp(panel->text, text) != 0;
    if (changed) {
        TigRect rect = { 0, 0, frame.width, frame.height };
        tig_window_fill(panel->window, &rect, background);
        tig_window_box(panel->window, &rect, border);
        if (text[0] != '\0') {
            TigRect body = { 6, 0, frame.width - 11, frame.height };
            TigFont measure = { 0 };
            tig_font_push(font);
            measure.str = text;
            tig_font_measure(&measure);
            body.y = (body.height - measure.height) / 2;
            body.height = measure.height;
            int rc = tig_window_text_write(panel->window, text, &body);
            tig_font_pop();
            if (rc != TIG_OK) { panel->valid = false; return true; }
        }
        panel->style = style;
        panel->valid = strlen(text) < sizeof(panel->text);
        SDL_strlcpy(panel->text, text, sizeof(panel->text));
    }
    if (tig_window_is_hidden(panel->window)) tig_window_show(panel->window);
    return changed;
}

void ui_status_panel_hide(UiStatusPanel* panel)
{
    if (panel->window != TIG_WINDOW_HANDLE_INVALID && !tig_window_is_hidden(panel->window)) tig_window_hide(panel->window);
}

void ui_status_panel_destroy(UiStatusPanel* panel)
{
    if (panel->window != TIG_WINDOW_HANDLE_INVALID) tig_window_destroy(panel->window);
    memset(panel, 0, sizeof(*panel));
    panel->window = TIG_WINDOW_HANDLE_INVALID;
}
