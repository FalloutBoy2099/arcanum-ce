#ifndef ARCANUM_UI_INTGAME_MODE_H_
#define ARCANUM_UI_INTGAME_MODE_H_

#include <stdbool.h>

typedef enum IntgameMode {
    INTGAME_MODE_MAIN,
    INTGAME_MODE_SPELL,
    INTGAME_MODE_SKILL,
    INTGAME_MODE_DIALOG,
    INTGAME_MODE_BARTER,
    INTGAME_MODE_WMAP,
    INTGAME_MODE_SLEEP,
    INTGAME_MODE_LOGBOOK,
    INTGAME_MODE_INVEN,
    INTGAME_MODE_CHAREDIT,
    INTGAME_MODE_LOOT,
    INTGAME_MODE_STEAL,
    INTGAME_MODE_RESERVED_12,
    INTGAME_MODE_QUANTITY,
    INTGAME_MODE_SCHEMATIC,
    INTGAME_MODE_WRITTEN,
    INTGAME_MODE_ITEM,
    INTGAME_MODE_RESERVED_17,
    INTGAME_MODE_FOLLOWER,
    INTGAME_MODE_NPC_IDENTIFY,
    INTGAME_MODE_NPC_REPAIR,
    INTGAME_MODE_COUNT,
} IntgameMode;

#define INTGAME_MODE_STACK_CAPACITY 11

typedef struct IntgameModeState {
    IntgameMode stack[INTGAME_MODE_STACK_CAPACITY];
    int top; // Index of the active entry, including MAIN at index zero.
    bool changing;
} IntgameModeState;

typedef struct IntgameModeTransition {
    IntgameMode from;
    IntgameMode to;
    bool pushing;
    bool popping;
} IntgameModeTransition;

typedef enum IntgameModeResult {
    INTGAME_MODE_READY,
    INTGAME_MODE_REENTRANT,
    INTGAME_MODE_INVALID,
    INTGAME_MODE_STACK_FULL,
} IntgameModeResult;

void intgame_mode_state_reset(IntgameModeState* state);
IntgameMode intgame_mode_state_current(const IntgameModeState* state);
// MAIN means return one level, not discard the entire stack. Recursive close
// callbacks are acknowledged without changing the in-progress transition.
IntgameModeResult intgame_mode_state_begin(IntgameModeState* state, IntgameMode requested,
    IntgameModeTransition* transition);
// Publish after screen callbacks, keeping recursive transitions blocked during
// cursor refresh. End releases that guard once refresh has finished.
void intgame_mode_state_commit(IntgameModeState* state, const IntgameModeTransition* transition);
void intgame_mode_state_end(IntgameModeState* state);

#endif
