#include "ui/search_cache.h"

#include <stdlib.h>
#include <string.h>

void ui_search_cache_clear(UiSearchCache* cache)
{
    free(cache->indices);
    free(cache->query);
    *cache = (UiSearchCache){ 0 };
}

bool ui_search_cache_update(UiSearchCache* cache, uint64_t revision, const char* query,
    int source_count, UiSearchPredicate predicate, void* context, bool* rebuilt)
{
    if (rebuilt != NULL) *rebuilt = false;
    if (query == NULL || source_count < 0 || predicate == NULL) {
        cache->valid = false;
        cache->count = 0;
        return false;
    }
    if (cache->valid && cache->revision == revision && cache->source_count == source_count
        && strcmp(cache->query, query) == 0) return true;
    cache->valid = false;
    cache->count = 0;
    if (source_count > cache->capacity) {
        if ((size_t)source_count > SIZE_MAX / sizeof(int)) return false;
        int* indices = realloc(cache->indices, (size_t)source_count * sizeof(int));
        if (indices == NULL) return false;
        cache->indices = indices;
        cache->capacity = source_count;
    }
    size_t length = strlen(query) + 1;
    char* copy = malloc(length);
    if (copy == NULL) return false;
    memcpy(copy, query, length);
    free(cache->query);
    cache->query = copy;
    for (int i = 0; i < source_count; i++) {
        if (predicate(i, context)) cache->indices[cache->count++] = i;
    }
    cache->source_count = source_count;
    cache->revision = revision;
    cache->valid = true;
    if (rebuilt != NULL) *rebuilt = true;
    return true;
}

int ui_search_cache_select(const UiSearchCache* cache, int current_index, int step)
{
    if (!cache->valid || cache->count == 0) return -1;
    int position = -1;
    for (int i = 0; i < cache->count; i++) {
        if (cache->indices[i] == current_index) { position = i; break; }
    }
    int selected = step == 0 || position == -1 ? 0
        : (int)((position + (int64_t)step % cache->count + cache->count) % cache->count);
    return cache->indices[selected];
}
