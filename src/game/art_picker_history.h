#ifndef ARCANUM_GAME_ART_PICKER_HISTORY_H_
#define ARCANUM_GAME_ART_PICKER_HISTORY_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ArtPickerHistory {
    uint32_t* entries;
    size_t count;
    size_t capacity;
} ArtPickerHistory;

bool art_picker_history_push(ArtPickerHistory* history, uint32_t art_id);
bool art_picker_history_pop(ArtPickerHistory* history, uint32_t* art_id);
void art_picker_history_clear(ArtPickerHistory* history);

#endif
