#ifndef ARCANUM_UI_MAINMENU_HISTORY_H_
#define ARCANUM_UI_MAINMENU_HISTORY_H_

#include <stdbool.h>

typedef enum MainMenuWindowType {
    MM_WINDOW_RESERVED_PRIMARY,
    MM_WINDOW_RESERVED_SECONDARY,
    MM_WINDOW_MAINMENU,
    MM_WINDOW_MAINMENU_IN_PLAY,
    MM_WINDOW_MAINMENU_IN_PLAY_LOCKED,
    MM_WINDOW_SINGLE_PLAYER,
    MM_WINDOW_OPTIONS,
    MM_WINDOW_LOAD_GAME,
    MM_WINDOW_SAVE_GAME,
    MM_WINDOW_LAST_SAVE_GAME,
    MM_WINDOW_INTRO,
    MM_WINDOW_PICK_NEW_OR_PREGEN,
    MM_WINDOW_NEW_CHAR,
    MM_WINDOW_PREGEN_CHAR,
    MM_WINDOW_CHAREDIT,
    MM_WINDOW_SHOP,
    MM_WINDOW_CREDITS,
    MM_WINDOW_MAINMENU_WITHOUT_LOAD,
    MM_WINDOW_COUNT,
} MainMenuWindowType;

#define MAINMENU_HISTORY_CAPACITY 50

typedef struct MainMenuHistory {
    MainMenuWindowType entries[MAINMENU_HISTORY_CAPACITY];
    int count;
} MainMenuHistory;

void mainmenu_history_reset(MainMenuHistory* history);
bool mainmenu_history_push(MainMenuHistory* history, MainMenuWindowType window);
// Discard current and take parent out of the history: opening the returned
// screen records it again. MM_WINDOW_0 means return to the game/no parent.
MainMenuWindowType mainmenu_history_back(MainMenuHistory* history);
void mainmenu_history_discard_current(MainMenuHistory* history);
// Preserve the legacy reserved entry at index zero when installing a route.
bool mainmenu_history_seed(MainMenuHistory* history, const int* windows, int count);

#endif
