#ifndef ARCANUM_UI_MAINMENU_UI_H_
#define ARCANUM_UI_MAINMENU_UI_H_

#include "game/context.h"
#include "game/mes.h"
#include "game/timeevent.h"
#include "ui/types.h"
#include "ui/mainmenu_history.h"

void mainmenu_ui_refresh_background(void);

typedef enum MainMenuFont {
    MM_FONT_FLARE12,
    MM_FONT_ARIAL10,
    MM_FONT_MORPH15,
    MM_FONT_MPICONS,
    MM_FONT_COUNT,
} MainMenuFont;

typedef enum MainMenuColor {
    MM_COLOR_WHITE,
    MM_COLOR_RED,
    MM_COLOR_BLUE,
    MM_COLOR_GOLD,
    MM_COLOR_BROWN,
    MM_COLOR_GREEN,
    MM_COLOR_PURPLE,
    MM_COLOR_LIGHTBLUE,
    MM_COLOR_ORANGE,
    MM_COLOR_GRAY,
    MM_COLOR_COUNT,
} MainMenuColor;

typedef enum MainMenuType {
    MM_TYPE_DEFAULT,
    MM_TYPE_1,
    MM_TYPE_IN_PLAY,
    MM_TYPE_IN_PLAY_LOCKED,
    MM_TYPE_OPTIONS,
    MM_TYPE_IN_PLAY_WITHOUT_LOAD,
} MainMenuType;

extern int mainmenu_reserved_selection;
extern int mainmenu_reserved_object_count;
extern int64_t* mainmenu_reserved_object_handles;

bool mainmenu_ui_init(GameInitInfo* init_info);
void mainmenu_ui_exit(void);
void mainmenu_ui_start(MainMenuType type);
void mainmenu_ui_start_at_window(MainMenuWindowType window_type);
void mainmenu_ui_game_resume(void);
bool mainmenu_ui_handle(void);
bool mainmenu_ui_is_active(void);
TigWindowModalDialogChoice mainmenu_ui_confirm_quit(void);
void mainmenu_ui_reset(void);
void mainmenu_ui_open(void);
void mainmenu_ui_close(bool back);
MainMenuWindowType mainmenu_ui_pop_window_stack(void);
void mainmenu_ui_push_window_stack(MainMenuWindowType window_type);
bool mainmenu_ui_last_save_load(void);
void mainmenu_ui_create_window(void);
void mainmenu_ui_create_window_func(bool should_display);
bool mainmenu_ui_process_callback(TimeEvent* timeevent);
bool mainmenu_ui_return_to_main(tig_button_handle_t button_handle);
void mainmenu_ui_textedit_begin(char* buffer, int size);
char* mainmenu_ui_textedit_buffer_get(void);
void mainmenu_ui_exit_game(void);
void mainmenu_ui_progressbar_init(int max_value);
void mainmenu_ui_progressbar_update(int value);
MainMenuWindowInfo* mainmenu_ui_window_info_get(int index);
MainMenuWindowType mainmenu_ui_window_type_get(void);
void mainmenu_ui_feedback_saving(void);
void mainmenu_ui_feedback_saving_completed(void);
void mainmenu_ui_feedback_cannot_save_in_tb(void);
void mainmenu_ui_feedback_loading(void);
void mainmenu_ui_feedback_loading_completed(void);
tig_window_handle_t mainmenu_ui_window_handle_get(void);
void mainmenu_ui_window_type_set(MainMenuWindowType window_type);
mes_file_handle_t mainmenu_ui_mes_file(void);
tig_font_handle_t mainmenu_ui_font(MainMenuFont font, MainMenuColor color);
void mainmenu_ui_button_labels_refresh(void);
void mainmenu_ui_history_route_seed(int* window_types, int num);
void mainmenu_ui_activity_counter_increment(void);
void mainmenu_ui_activity_counter_decrement(void);
int mainmenu_ui_activity_counter_get(void);
void mainmenu_ui_pregen_probe_reset(void);

#endif /* ARCANUM_UI_MAINMENU_UI_H_ */
