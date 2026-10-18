#include "ui/benchmark_widget.h"
#include "ui/search_bar.h"

static tig_window_handle_t button = TIG_WINDOW_HANDLE_INVALID;
static tig_window_handle_t notification = TIG_WINDOW_HANDLE_INVALID;
static TigRect frame;
static TigRect notice_frame;
static tig_font_handle_t text_font;
static bool shown, pressed, hovered, requested, dirty, notice_dismissed;
static char button_label[80];
static char notice_text[512];

static void hide(tig_window_handle_t window)
{
    if (window != TIG_WINDOW_HANDLE_INVALID && !tig_window_is_hidden(window)) tig_window_hide(window);
}

static bool place(tig_window_handle_t* window, TigRect rect)
{
    if (*window != TIG_WINDOW_HANDLE_INVALID) {
        TigWindowData data;
        if (tig_window_data(*window, &data) == TIG_OK) {
            if (data.rect.width != rect.width || data.rect.height != rect.height) {
                tig_window_destroy(*window);
                *window = TIG_WINDOW_HANDLE_INVALID;
            } else if (data.rect.x != rect.x || data.rect.y != rect.y) tig_window_move(*window, rect.x, rect.y);
        }
    }
    if (*window == TIG_WINDOW_HANDLE_INVALID) {
        TigWindowData data = { 0 };
        data.rect = rect;
        data.flags = TIG_WINDOW_ALWAYS_ON_TOP | TIG_WINDOW_MESSAGE_FILTER;
        data.background_color = tig_color_make(15, 12, 8);
        data.message_filter = benchmark_widget_mouse;
        if (tig_window_create(&data, window) != TIG_OK) return false;
        dirty = true;
    }
    if (tig_window_is_hidden(*window)) tig_window_show(*window);
    return true;
}

bool benchmark_widget_mouse(TigMessage* message)
{
    if (!shown || message->type != TIG_MESSAGE_MOUSE) return false;
    int x = message->data.mouse.x, y = message->data.mouse.y;
    bool inside = ui_search_bar_contains(&frame, x, y);
    if (hovered != inside) { hovered = inside; dirty = true; }
    if (message->data.mouse.event == TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN) {
        if (!message->data.mouse.repeat) { pressed = inside; dirty = true; }
    } else if (message->data.mouse.event == TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP) {
        if (pressed && inside) requested = true;
        pressed = false;
        dirty = true;
        if (!notice_dismissed && notice_text[0] != '\0' && ui_search_bar_contains(&notice_frame, x, y)) {
            notice_dismissed = true;
            hide(notification);
            return true;
        }
    } else if (!inside && pressed) { pressed = false; dirty = true; }
    return inside || (!notice_dismissed && notice_text[0] != '\0' && ui_search_bar_contains(&notice_frame, x, y));
}

bool benchmark_widget_take_request(void)
{
    bool result = requested;
    requested = false;
    return result;
}

void benchmark_widget_update(TigRect screen, bool visible, tig_font_handle_t font,
    const char* label, const char* notice)
{
    if (strcmp(button_label, label) != 0 || text_font != font) {
        SDL_strlcpy(button_label, label, sizeof(button_label));
        dirty = true;
    }
    if (strcmp(notice_text, notice) != 0) {
        SDL_strlcpy(notice_text, notice, sizeof(notice_text));
        notice_dismissed = false;
        dirty = true;
    }
    text_font = font;
    shown = visible;
    if (!visible) { hide(button); hide(notification); pressed = hovered = requested = false; dirty = true; return; }
    frame = (TigRect){ screen.x + SDL_max(8, screen.width - 132), screen.y + 64, 124, 26 };
    notice_frame = (TigRect){ screen.x + SDL_max(8, screen.width - 392), screen.y + 96, SDL_min(384, screen.width - 16), 84 };
    // Keep the press until its queued release/drag event. tig_mouse_get_state
    // exposes cursor visibility flags, not the held-button state.
    if (!place(&button, frame)) return;
    if (notice_text[0] == '\0' || notice_dismissed) hide(notification);
    else if (!place(&notification, notice_frame)) return;
    if (!dirty) return;
    TigFont* font_data = (TigFont*)text_font;
    tig_color_t saved_color = font_data->color;
    font_data->color = tig_color_make(15, 12, 8);
    ui_search_bar_cell(button, (TigRect){ 0, 0, frame.width, frame.height }, text_font, button_label, pressed || hovered);
    font_data->color = saved_color;
    if (notice_text[0] != '\0' && !notice_dismissed) {
        TigRect rect = { 0, 0, notice_frame.width, notice_frame.height };
        tig_window_fill(notification, &rect, tig_color_make(15, 12, 8));
        tig_window_box(notification, &rect, tig_color_make(160, 125, 60));
        rect = (TigRect){ 8, 6, notice_frame.width - 16, notice_frame.height - 12 };
        tig_font_push(text_font);
        tig_window_text_write(notification, notice_text, &rect);
        tig_font_pop();
    }
    dirty = false;
}

void benchmark_widget_close(void)
{
    if (button != TIG_WINDOW_HANDLE_INVALID) tig_window_destroy(button);
    if (notification != TIG_WINDOW_HANDLE_INVALID) tig_window_destroy(notification);
    button = notification = TIG_WINDOW_HANDLE_INVALID;
    shown = pressed = hovered = requested = notice_dismissed = false;
    button_label[0] = notice_text[0] = '\0';
    text_font = TIG_FONT_HANDLE_INVALID;
    dirty = true;
}
