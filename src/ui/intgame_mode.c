#include "ui/intgame_mode.h"

void intgame_mode_state_reset(IntgameModeState* state)
{
    state->top = 0;
    state->stack[0] = INTGAME_MODE_MAIN;
    state->changing = false;
}

IntgameMode intgame_mode_state_current(const IntgameModeState* state)
{
    return state->stack[state->top];
}

IntgameModeResult intgame_mode_state_begin(IntgameModeState* state, IntgameMode requested,
    IntgameModeTransition* transition)
{
    if (requested < INTGAME_MODE_MAIN || requested >= INTGAME_MODE_COUNT) return INTGAME_MODE_INVALID;
    if (state->changing) return INTGAME_MODE_REENTRANT;
    if (requested != INTGAME_MODE_MAIN && state->top == INTGAME_MODE_STACK_CAPACITY - 1) {
        return INTGAME_MODE_STACK_FULL;
    }

    *transition = (IntgameModeTransition){ state->stack[state->top], requested, false, false };
    state->changing = true;
    if (requested != INTGAME_MODE_MAIN) {
        state->top++;
        transition->pushing = true;
    } else if (state->top > 0) {
        transition->to = state->stack[--state->top];
        transition->popping = true;
    }
    return INTGAME_MODE_READY;
}

void intgame_mode_state_commit(IntgameModeState* state, const IntgameModeTransition* transition)
{
    state->stack[state->top] = transition->to;
}

void intgame_mode_state_end(IntgameModeState* state)
{
    state->changing = false;
}
