#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <tig/database.h>
#include <tig/file.h>
#include <tig/file_cache.h>
#include <tig/memory.h>

static int failures;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #value); failures++; } } while (0)

static void fixture(const char* path, int count, int value)
{
    FILE* stream = fopen(path, "wb");
    CHECK(stream != NULL);
    if (stream == NULL) return;
    for (int i = 0; i < count; i++) CHECK(fputc(value, stream) != EOF);
    CHECK(fclose(stream) == 0);
}

static void incomplete_archive(void)
{
    FILE* stream = fopen("incomplete.dat", "wb");
    CHECK(stream != NULL);
    if (stream == NULL) return;
    const unsigned char compressed[] = { 0x78, 0x9c, 0x33, 0x36, 0xa6, 0x0c, 0x00, 0x00, 0x9e, 0xaf, 0x0c, 0xc1 };
    uint32_t prefix[] = { sizeof(compressed) + 4, 1, 9 };
    uint32_t entry[] = { 0, TIG_DATABASE_ENTRY_COMPRESSED, 128, sizeof(compressed), 0 };
    uint32_t footer[] = { SDL_FOURCC(' ', 'T', 'A', 'D'), 9, 49 };
    fwrite(compressed, sizeof(compressed), 1, stream);
    fwrite(prefix, sizeof(prefix), 1, stream);
    fwrite("test.wav", 9, 1, stream);
    fwrite(entry, sizeof(entry), 1, stream);
    fwrite(footer, sizeof(footer), 1, stream);
    fclose(stream);
}

static void references_and_eviction(void)
{
    TigFileCache* cache = tig_file_cache_create(4, 64);
    CHECK(cache != NULL);
    if (cache == NULL) return;
    TigFileCacheEntry* first = tig_file_cache_acquire(cache, "first.wav");
    CHECK(first->data != NULL && first->size == 64);
    if (first->data != NULL) CHECK(((unsigned char*)first->data)[63] == 11);
    CHECK(cache->bytes == 64 && cache->items_count == 1);
    TigFileCacheEntry* again = tig_file_cache_acquire(cache, "FIRST.WAV");
    CHECK(again == first && cache->items[first->index].refcount == 2);
    tig_file_cache_release(cache, again);
    TigFileCacheEntry* second = tig_file_cache_acquire(cache, "second.wav");
    CHECK(second->data != NULL && cache->bytes == 128 && cache->items_count == 2);
    tig_file_cache_flush(cache);
    CHECK(cache->bytes == 128);
    tig_file_cache_release(cache, first);
    TigFileCacheEntry* third = tig_file_cache_acquire(cache, "third.wav");
    CHECK(third->data != NULL && second->data != NULL);
    CHECK(first->data == NULL && cache->bytes == 128 && cache->items_count == 2);
    if (second->data != NULL) CHECK(((unsigned char*)second->data)[0] == 22);
    tig_file_cache_release(cache, second);
    tig_file_cache_release(cache, third);
    tig_file_cache_release(cache, third);
    CHECK(cache->items[third->index].refcount == 0);
    tig_file_cache_flush(cache);
    CHECK(cache->bytes == 0 && cache->items_count == 0);
    tig_file_cache_destroy(cache);
}

static void failed_acquisitions(void)
{
    TigFileCache* cache = tig_file_cache_create(2, 1024);
    CHECK(cache != NULL);
    if (cache == NULL) return;
    TigFileCacheEntry* valid = tig_file_cache_acquire(cache, "first.wav");
    const char* paths[] = { "missing.wav", "empty.wav", "test.wav", "", NULL };
    for (int i = 0; i < SDL_arraysize(paths); i++) {
        TigMemoryStatistics before, after;
        tig_memory_statistics(&before);
        TigFileCacheEntry* failed = tig_file_cache_acquire(cache, paths[i]);
        CHECK(failed != NULL && failed->data == NULL);
        tig_file_cache_release(cache, failed);
        tig_memory_statistics(&after);
        CHECK(before.live_bytes == after.live_bytes && before.live_blocks == after.live_blocks);
        CHECK(cache->bytes == 64 && cache->items_count == 1);
        CHECK(cache->items[valid->index].refcount == 1);
    }
    cache->items[valid->index].refcount = INT_MAX;
    CHECK(tig_file_cache_acquire(cache, "first.wav")->data == NULL);
    CHECK(cache->items[valid->index].refcount == INT_MAX);
    cache->items[valid->index].refcount = 1;
    TigFileCacheEntry foreign = { .index = valid->index };
    tig_file_cache_release(cache, &foreign);
    foreign.index = INT_MAX;
    tig_file_cache_release(cache, &foreign);
    tig_file_cache_release(cache, NULL);
    CHECK(cache->items[valid->index].refcount == 1);
    TigFileCacheEntry* second = tig_file_cache_acquire(cache, "second.wav");
    CHECK(second->data != NULL);
    CHECK(tig_file_cache_acquire(cache, "third.wav")->data == NULL);
    CHECK(cache->bytes == 128 && cache->items_count == 2);
    tig_file_cache_release(cache, valid);
    tig_file_cache_release(cache, second);
    tig_file_cache_destroy(cache);
}

static void wide_accounting(void)
{
    size_t baseline = (size_t)INT_MAX + 64;
    TigFileCache* cache = tig_file_cache_create(2, SIZE_MAX);
    CHECK(cache != NULL && cache->max_size == SIZE_MAX);
    if (cache == NULL) return;
    cache->bytes = baseline;
    TigFileCacheEntry* entry = tig_file_cache_acquire(cache, "first.wav");
    CHECK(entry->data != NULL && cache->bytes == baseline + 64);
    tig_file_cache_release(cache, entry);
    tig_file_cache_flush(cache);
    CHECK(cache->bytes == baseline);
    cache->bytes = SIZE_MAX - 32;
    TigMemoryStatistics before, after;
    tig_memory_statistics(&before);
    CHECK(tig_file_cache_acquire(cache, "first.wav")->data == NULL);
    tig_memory_statistics(&after);
    CHECK(before.live_bytes == after.live_bytes && before.live_blocks == after.live_blocks);
    CHECK(cache->bytes == SIZE_MAX - 32 && cache->items_count == 0);
    cache->bytes = 0;
    tig_file_cache_destroy(cache);
}

int main(void)
{
    CHECK(SDL_Init(0));
    CHECK(tig_memory_init(NULL) == TIG_OK);
    CHECK(tig_file_repository_add("."));
    fixture("first.wav", 64, 11);
    fixture("second.wav", 64, 22);
    fixture("third.wav", 64, 33);
    fixture("empty.wav", 0, 0);
    incomplete_archive();
    CHECK(tig_file_repository_add("incomplete.dat"));
    CHECK(tig_file_cache_create(0, 1024) == NULL);
    CHECK(tig_file_cache_create(-1, 1024) == NULL);
    CHECK(tig_file_cache_acquire(NULL, "first.wav")->data == NULL);
    tig_file_cache_release(NULL, NULL);
    tig_file_cache_flush(NULL);
    tig_file_cache_destroy(NULL);
    references_and_eviction();
    failed_acquisitions();
    wide_accounting();
    tig_file_exit();
    remove("first.wav"); remove("second.wav"); remove("third.wav");
    remove("empty.wav"); remove("incomplete.dat");
    tig_memory_exit();
    SDL_Quit();
    printf("File cache: %d failures\n", failures);
    return failures != 0;
}
