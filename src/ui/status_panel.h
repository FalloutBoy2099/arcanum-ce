#ifndef ARCANUM_UI_STATUS_PANEL_H_
#define ARCANUM_UI_STATUS_PANEL_H_

#include <tig/font.h>
#include <tig/window.h>

typedef struct UiStatusPanel {
    tig_window_handle_t window;
    bool valid;
    uint64_t style;
    char text[128];
} UiStatusPanel;

// Returns true only if pixels were redrawn. Position/visibility update independently.
bool ui_status_panel_draw(UiStatusPanel* panel, TigRect frame, const char* text,
    tig_font_handle_t font, tig_color_t background, tig_color_t border);
void ui_status_panel_hide(UiStatusPanel* panel);
void ui_status_panel_destroy(UiStatusPanel* panel);

#endif
