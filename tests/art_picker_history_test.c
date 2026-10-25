#include "game/art_picker_history.h"
#include <stdio.h>

static int failures;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #expr); failures++; } } while (0)

int main(void)
{
    ArtPickerHistory history = { 0 };
    uint32_t selected = UINT32_MAX;
    CHECK(!art_picker_history_pop(&history, &selected) && selected == UINT32_MAX);
    for (uint32_t id = 0; id < 3000; id++) CHECK(art_picker_history_push(&history, id));
    CHECK(history.count == 3000);
    for (uint32_t id = 3000; id > 0; id--) {
        CHECK(art_picker_history_pop(&history, &selected));
        CHECK(selected == id - 1);
    }
    CHECK(!art_picker_history_pop(&history, &selected));
    CHECK(art_picker_history_push(&history, UINT32_MAX));
    CHECK(art_picker_history_pop(&history, &selected) && selected == UINT32_MAX);
    CHECK(art_picker_history_push(&history, 7));
    CHECK(art_picker_history_push(&history, 9));
    CHECK(art_picker_history_pop(&history, &selected) && selected == 9);
    CHECK(art_picker_history_push(&history, 8));
    CHECK(art_picker_history_pop(&history, &selected) && selected == 8);
    CHECK(art_picker_history_pop(&history, &selected) && selected == 7);
    art_picker_history_clear(&history);
    CHECK(history.entries == NULL && history.count == 0 && history.capacity == 0);
    CHECK(art_picker_history_push(&history, 42));
    CHECK(art_picker_history_pop(&history, &selected) && selected == 42);
    art_picker_history_clear(&history);
    art_picker_history_clear(&history);
    CHECK(!art_picker_history_push(NULL, 0));
    CHECK(!art_picker_history_pop(NULL, &selected));
    CHECK(!art_picker_history_pop(&history, NULL));
    art_picker_history_clear(NULL);
    return failures ? 1 : 0;
}
