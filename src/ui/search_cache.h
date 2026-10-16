#ifndef ARCANUM_UI_SEARCH_CACHE_H_
#define ARCANUM_UI_SEARCH_CACHE_H_

#include <stdbool.h>
#include <stdint.h>

typedef bool (*UiSearchPredicate)(int index, void* context);

typedef struct UiSearchCache {
    int* indices;
    char* query;
    int count;
    int capacity;
    int source_count;
    uint64_t revision;
    bool valid;
} UiSearchCache;

void ui_search_cache_clear(UiSearchCache* cache);
bool ui_search_cache_update(UiSearchCache* cache, uint64_t revision, const char* query,
    int source_count, UiSearchPredicate predicate, void* context, bool* rebuilt);
int ui_search_cache_select(const UiSearchCache* cache, int current_index, int step);

#endif
