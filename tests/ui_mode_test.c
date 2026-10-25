#include <stdio.h>
#include <string.h>

#include "ui/intgame_mode.h"
#include "ui/mainmenu_history.h"

static int failures;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); failures++; } } while (0)

static void enter(IntgameModeState* state, IntgameMode requested, IntgameMode expected)
{
    IntgameModeTransition transition;
    CHECK(intgame_mode_state_begin(state, requested, &transition) == INTGAME_MODE_READY);
    CHECK(transition.to == expected);

    // Close callbacks may recursively request MAIN. They must acknowledge the
    // request without popping a second parent or modifying the active plan.
    IntgameModeState snapshot = *state;
    IntgameModeTransition ignored = { 0 };
    CHECK(intgame_mode_state_begin(state, INTGAME_MODE_MAIN, &ignored) == INTGAME_MODE_REENTRANT);
    CHECK(memcmp(state, &snapshot, sizeof(snapshot)) == 0);
    intgame_mode_state_commit(state, &transition);
    CHECK(intgame_mode_state_current(state) == expected);
    CHECK(intgame_mode_state_begin(state, INTGAME_MODE_LOGBOOK, &ignored) == INTGAME_MODE_REENTRANT);
    CHECK(intgame_mode_state_current(state) == expected);
    intgame_mode_state_end(state);
    CHECK(!state->changing);
}

static void test_parent_screens(void)
{
    IntgameModeState state = { 0 };
    intgame_mode_state_reset(&state);
    enter(&state, INTGAME_MODE_DIALOG, INTGAME_MODE_DIALOG);
    enter(&state, INTGAME_MODE_BARTER, INTGAME_MODE_BARTER);
    enter(&state, INTGAME_MODE_QUANTITY, INTGAME_MODE_QUANTITY);
    enter(&state, INTGAME_MODE_MAIN, INTGAME_MODE_BARTER);
    enter(&state, INTGAME_MODE_MAIN, INTGAME_MODE_DIALOG);
    enter(&state, INTGAME_MODE_MAIN, INTGAME_MODE_MAIN);
    enter(&state, INTGAME_MODE_MAIN, INTGAME_MODE_MAIN);
    CHECK(state.top == 0);

    enter(&state, INTGAME_MODE_INVEN, INTGAME_MODE_INVEN);
    enter(&state, INTGAME_MODE_ITEM, INTGAME_MODE_ITEM);
    enter(&state, INTGAME_MODE_MAIN, INTGAME_MODE_INVEN);
    enter(&state, INTGAME_MODE_SPELL, INTGAME_MODE_SPELL);
    enter(&state, INTGAME_MODE_MAIN, INTGAME_MODE_INVEN);
    enter(&state, INTGAME_MODE_SKILL, INTGAME_MODE_SKILL);
    enter(&state, INTGAME_MODE_MAIN, INTGAME_MODE_INVEN);
    enter(&state, INTGAME_MODE_MAIN, INTGAME_MODE_MAIN);

    // Loading/reset discards nested screens and an in-progress guard.
    IntgameModeTransition transition;
    CHECK(intgame_mode_state_begin(&state, INTGAME_MODE_LOGBOOK, &transition) == INTGAME_MODE_READY);
    intgame_mode_state_reset(&state);
    CHECK(state.top == 0 && !state.changing);
    CHECK(intgame_mode_state_current(&state) == INTGAME_MODE_MAIN);
}

static void test_rejection(void)
{
    IntgameModeState state = { 0 };
    IntgameModeTransition transition;
    for (int i = 1; i < INTGAME_MODE_STACK_CAPACITY; i++) {
        enter(&state, INTGAME_MODE_LOGBOOK, INTGAME_MODE_LOGBOOK);
    }
    IntgameModeState snapshot = state;
    CHECK(intgame_mode_state_begin(&state, INTGAME_MODE_SCHEMATIC, &transition) == INTGAME_MODE_STACK_FULL);
    CHECK(memcmp(&state, &snapshot, sizeof(state)) == 0);
    CHECK(intgame_mode_state_begin(&state, (IntgameMode)-1, &transition) == INTGAME_MODE_INVALID);
    CHECK(intgame_mode_state_begin(&state, INTGAME_MODE_COUNT, &transition) == INTGAME_MODE_INVALID);
    CHECK(memcmp(&state, &snapshot, sizeof(state)) == 0);
    for (int i = 1; i < INTGAME_MODE_STACK_CAPACITY; i++) {
        enter(&state, INTGAME_MODE_MAIN, i == INTGAME_MODE_STACK_CAPACITY - 1 ? INTGAME_MODE_MAIN : INTGAME_MODE_LOGBOOK);
    }
    CHECK(state.top == 0);
    enter(&state, INTGAME_MODE_SCHEMATIC, INTGAME_MODE_SCHEMATIC);
}

static void test_legacy_sequences(void)
{
    // Exhaust all three-request combinations against the original push/pop
    // rules. Include reserved modes: existing numeric enum values stay valid.
    for (int a = 0; a < INTGAME_MODE_COUNT; a++) {
        for (int b = 0; b < INTGAME_MODE_COUNT; b++) {
            for (int c = 0; c < INTGAME_MODE_COUNT; c++) {
                IntgameModeState state = { 0 };
                IntgameMode legacy[INTGAME_MODE_STACK_CAPACITY] = { INTGAME_MODE_MAIN };
                int top = 0;
                IntgameMode requests[] = { a, b, c, INTGAME_MODE_MAIN, INTGAME_MODE_MAIN, INTGAME_MODE_MAIN };
                for (int i = 0; i < sizeof(requests) / sizeof(requests[0]); i++) {
                    IntgameMode previous = legacy[top];
                    IntgameMode next = requests[i];
                    bool pushing = next != INTGAME_MODE_MAIN;
                    bool popping = !pushing && top > 0;
                    if (pushing) top++;
                    else if (popping) next = legacy[--top];
                    legacy[top] = next;
                    IntgameModeTransition transition;
                    CHECK(intgame_mode_state_begin(&state, requests[i], &transition) == INTGAME_MODE_READY);
                    CHECK(transition.from == previous && transition.to == next);
                    CHECK(transition.pushing == pushing && transition.popping == popping);
                    intgame_mode_state_commit(&state, &transition);
                    intgame_mode_state_end(&state);
                    CHECK(state.top == top);
                    CHECK(memcmp(state.stack, legacy, sizeof(legacy)) == 0);
                }
            }
        }
    }
}

static void test_menu_history(void)
{
    MainMenuHistory history = { 0 };
    CHECK(mainmenu_history_back(&history) == MM_WINDOW_RESERVED_PRIMARY && history.count == 0);
    CHECK(mainmenu_history_push(&history, MM_WINDOW_MAINMENU_IN_PLAY));
    CHECK(mainmenu_history_push(&history, MM_WINDOW_OPTIONS));
    CHECK(mainmenu_history_back(&history) == MM_WINDOW_MAINMENU_IN_PLAY);
    CHECK(history.count == 0);
    // Reopening the parent records it again, exactly as mainmenu_ui_open does.
    CHECK(mainmenu_history_push(&history, MM_WINDOW_MAINMENU_IN_PLAY));
    CHECK(mainmenu_history_back(&history) == MM_WINDOW_RESERVED_PRIMARY && history.count == 0);
    CHECK(mainmenu_history_push(&history, MM_WINDOW_OPTIONS));
    CHECK(mainmenu_history_back(&history) == MM_WINDOW_RESERVED_PRIMARY); // Direct O shortcut.

    for (int count = 0; count <= MAINMENU_HISTORY_CAPACITY; count++) {
        mainmenu_history_reset(&history);
        for (int i = 0; i < count; i++) CHECK(mainmenu_history_push(&history, MM_WINDOW_LOAD_GAME));
        mainmenu_history_discard_current(&history); // Intro/Credits redirect.
        CHECK(history.count == (count > 0 ? count - 1 : 0));
    }
    mainmenu_history_reset(&history);
    for (int i = 0; i < MAINMENU_HISTORY_CAPACITY; i++) CHECK(mainmenu_history_push(&history, MM_WINDOW_OPTIONS));
    MainMenuHistory snapshot = history;
    CHECK(!mainmenu_history_push(&history, MM_WINDOW_LOAD_GAME));
    CHECK(!mainmenu_history_push(&history, (MainMenuWindowType)-1));
    CHECK(!mainmenu_history_push(&history, MM_WINDOW_COUNT));
    CHECK(memcmp(&history, &snapshot, sizeof(history)) == 0);

    int route[] = { MM_WINDOW_SINGLE_PLAYER, MM_WINDOW_LOAD_GAME, MM_WINDOW_OPTIONS };
    CHECK(mainmenu_history_seed(&history, route, 3));
    CHECK(history.count == 4);
    CHECK(mainmenu_history_back(&history) == MM_WINDOW_LOAD_GAME && history.count == 2);
    snapshot = history;
    CHECK(!mainmenu_history_seed(&history, route, -1));
    CHECK(!mainmenu_history_seed(&history, route, MAINMENU_HISTORY_CAPACITY));
    CHECK(!mainmenu_history_seed(&history, NULL, 1));
    int invalid[] = { MM_WINDOW_LOAD_GAME, MM_WINDOW_COUNT };
    CHECK(!mainmenu_history_seed(&history, invalid, 2));
    CHECK(memcmp(&history, &snapshot, sizeof(history)) == 0);
    CHECK(mainmenu_history_seed(&history, NULL, 0) && history.count == 1);
    CHECK(mainmenu_history_back(&history) == MM_WINDOW_RESERVED_PRIMARY && history.count == 0);

    for (int a = 0; a < MM_WINDOW_COUNT; a++) {
        for (int b = 0; b < MM_WINDOW_COUNT; b++) {
            mainmenu_history_reset(&history);
            CHECK(mainmenu_history_push(&history, (MainMenuWindowType)a));
            CHECK(mainmenu_history_push(&history, (MainMenuWindowType)b));
            CHECK(mainmenu_history_back(&history) == (MainMenuWindowType)a);
            CHECK(history.count == 0);
        }
    }
}

int main(void)
{
    test_parent_screens();
    test_rejection();
    test_legacy_sequences();
    test_menu_history();
    printf("UI mode transitions: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures != 0;
}
