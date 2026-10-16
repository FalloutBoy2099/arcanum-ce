#ifndef ARCANUM_UI_SEARCH_BAR_H_
#define ARCANUM_UI_SEARCH_BAR_H_

#include <tig/tig.h>

bool ui_search_bar_open(tig_window_handle_t* handle, tig_window_handle_t parent,
    TigRect relative_rect, TigWindowMessageFilterFunc filter);
void ui_search_bar_close(tig_window_handle_t* handle);
void ui_search_bar_cell(tig_window_handle_t handle, TigRect rect, tig_font_handle_t font,
    const char* text, bool selected);
bool ui_search_bar_contains(const TigRect* rect, int x, int y);
// Recreate an unowned panel buffer, keeping its top-left fixed. Contents and
// buttons are intentionally not retained; use only before locking a panel.
bool ui_panel_window_height(tig_window_handle_t* handle, int height);
void ui_panel_background_draw(tig_window_handle_t handle, int art_num, int x, int y);

#endif
