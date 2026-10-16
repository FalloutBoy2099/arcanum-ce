#ifndef ARCANUM_UI_LOGBOOK_UI_H_
#define ARCANUM_UI_LOGBOOK_UI_H_

#include "game/context.h"

bool logbook_ui_init(GameInitInfo* init_info);
void logbook_ui_exit(void);
void logbook_ui_reset(void);
void logbook_ui_open(int64_t obj);
void logbook_ui_close(void);
bool logbook_ui_is_created(void);
void logbook_ui_check(void);

typedef enum LogbookView {
    LOGBOOK_VIEW_RUMORS,
    LOGBOOK_VIEW_CONVERSATIONS,
    LOGBOOK_VIEW_QUEST_INDEX,
} LogbookView;
bool logbook_ui_show_view(LogbookView view);
void logbook_ui_turn_page_left(void);
void logbook_ui_turn_page_right(void);
void logbook_ui_search_bar_visible(bool visible);
// Borrowed font: owned by the journal, shared by both search toolbars.
tig_font_handle_t logbook_ui_toolbar_font(void);

#endif /* ARCANUM_UI_LOGBOOK_UI_H_ */
