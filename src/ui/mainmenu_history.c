#include "ui/mainmenu_history.h"

static bool window_valid(int window)
{
    return window >= MM_WINDOW_RESERVED_PRIMARY && window < MM_WINDOW_COUNT;
}

void mainmenu_history_reset(MainMenuHistory* history)
{
    history->count = 0;
}

bool mainmenu_history_push(MainMenuHistory* history, MainMenuWindowType window)
{
    if (!window_valid(window) || history->count >= MAINMENU_HISTORY_CAPACITY) return false;
    history->entries[history->count++] = window;
    return true;
}

MainMenuWindowType mainmenu_history_back(MainMenuHistory* history)
{
    if (history->count > 0 && --history->count > 0) {
        return history->entries[--history->count];
    }
    return MM_WINDOW_RESERVED_PRIMARY;
}

void mainmenu_history_discard_current(MainMenuHistory* history)
{
    if (history->count > 0) history->count--;
}

bool mainmenu_history_seed(MainMenuHistory* history, const int* windows, int count)
{
    if (count < 0 || count >= MAINMENU_HISTORY_CAPACITY || (count > 0 && windows == 0)) return false;
    for (int i = 0; i < count; i++) {
        if (!window_valid(windows[i])) return false;
    }
    for (int i = 0; i < count; i++) history->entries[i + 1] = windows[i];
    history->count = count + 1;
    return true;
}
