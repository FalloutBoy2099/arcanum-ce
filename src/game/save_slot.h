#ifndef ARCANUM_GAME_SAVE_SLOT_H_
#define ARCANUM_GAME_SAVE_SLOT_H_

#include <SDL3/SDL.h>

// Save filenames begin with an eight-byte internal slot ID, followed by a
// localized description. Only the stable ID identifies the quicksave.
static inline bool save_slot_is_quicksave(const char* name)
{
    return name != NULL && SDL_strncasecmp(name, "SlotAuto", 8) == 0;
}

static inline int save_slot_compare_priority(const char* a, const char* b)
{
    bool a_quick = save_slot_is_quicksave(a);
    bool b_quick = save_slot_is_quicksave(b);
    return a_quick == b_quick ? 0 : (a_quick ? -1 : 1);
}

#endif /* ARCANUM_GAME_SAVE_SLOT_H_ */
