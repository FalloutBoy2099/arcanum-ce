#include "game/art_picker_history.h"

#include <SDL3/SDL.h>

bool art_picker_history_push(ArtPickerHistory* history, uint32_t art_id)
{
    if (history == NULL) return false;
    if (history->count == history->capacity) {
        if (history->capacity > SIZE_MAX / 2 / sizeof(*history->entries)) return false;
        size_t capacity = history->capacity != 0 ? history->capacity * 2 : 32;
        uint32_t* entries = SDL_realloc(history->entries, capacity * sizeof(*entries));
        if (entries == NULL) return false;
        history->entries = entries;
        history->capacity = capacity;
    }
    history->entries[history->count++] = art_id;
    return true;
}

bool art_picker_history_pop(ArtPickerHistory* history, uint32_t* art_id)
{
    if (history == NULL || art_id == NULL || history->count == 0) return false;
    *art_id = history->entries[--history->count];
    return true;
}

void art_picker_history_clear(ArtPickerHistory* history)
{
    if (history == NULL) return;
    SDL_free(history->entries);
    *history = (ArtPickerHistory){ 0 };
}
