#ifndef ARCANUM_UI_BENCHMARK_INPUT_H_
#define ARCANUM_UI_BENCHMARK_INPUT_H_

#include <SDL3/SDL.h>

// Pointer movement outside the window does not invalidate frame measurements.
// Focus changes do: an unfocused application may be paused or throttled.
static inline const char* benchmark_input_interrupt_reason(Uint32 event_type)
{
    switch (event_type) {
    case SDL_EVENT_QUIT: return "interrupted_quit";
    case SDL_EVENT_WINDOW_FOCUS_LOST: return "interrupted_focus_lost";
    default: return NULL;
    }
}

#endif
