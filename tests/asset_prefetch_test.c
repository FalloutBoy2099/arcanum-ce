#include <stdio.h>
#include <string.h>

#include <tig/asset_prefetch.h>
#include <tig/async_asset_reader.h>
#include <tig/file.h>
#include <tig/database.h>
#include <tig/memory.h>

static int failures;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #expr); failures++; } } while (0)

static void write_fixture(const char* path, int value)
{
    FILE* file = fopen(path, "wb");
    CHECK(file != NULL);
    if (file == NULL) return;
    for (int i = 0; i < 64; i++) fputc(value, file);
    fclose(file);
}

static void write_archive(unsigned int flags)
{
    FILE* file = fopen("prefetch-fixture/archive.dat", "wb");
    CHECK(file != NULL);
    if (file == NULL) return;
    const unsigned char compressed[] = { 0x78, 0x9c, 0x33, 0x36, 0xa6, 0x0c, 0x00, 0x00, 0x9e, 0xaf, 0x0c, 0xc1 };
    uint32_t stored_size = (flags & TIG_DATABASE_ENTRY_COMPRESSED) != 0 ? sizeof(compressed) : 64;
    if ((flags & TIG_DATABASE_ENTRY_COMPRESSED) != 0) fwrite(compressed, 1, sizeof(compressed), file);
    else for (int i = 0; i < 64; i++) fputc(51, file);
    uint32_t prefix[] = { stored_size + 4, 1, 9 };
    uint32_t entry[] = { 0, flags, 64, stored_size, 0 };
    uint32_t footer[] = { SDL_FOURCC(' ', 'T', 'A', 'D'), 9, 49 };
    fwrite(prefix, sizeof(prefix), 1, file);
    fwrite("test.art", 9, 1, file);
    fwrite(entry, sizeof(entry), 1, file);
    fwrite(footer, sizeof(footer), 1, file);
    fclose(file);
}

static bool acquire(const char* path, TigAssetData* data)
{
    Uint64 deadline = SDL_GetTicks() + 3000;
    while (SDL_GetTicks() < deadline) {
        if (tig_asset_prefetch_acquire(path, data)) return true;
        SDL_Delay(1);
    }
    return false;
}

static void check_prefetched_equals_native(const char* path, int value)
{
    TigAssetData data = { 0 };
    tig_asset_prefetch_request(path, 100);
    CHECK(acquire(path, &data));
    CHECK(data.size == 64 && data.bytes != NULL);
    unsigned char expected[64];
    TigFile* file = tig_file_fopen(path, "rb");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(tig_file_fread(expected, 1, sizeof(expected), file) == sizeof(expected));
        if (data.bytes != NULL && data.size == sizeof(expected)) {
            CHECK(memcmp(expected, data.bytes, sizeof(expected)) == 0);
            CHECK(expected[0] == value);
        }
        tig_file_fclose(file);
    }
    tig_asset_prefetch_release(&data);
}

int main(void)
{
    CHECK(SDL_Init(0));
    CHECK(tig_memory_init(NULL) == TIG_OK);
    CHECK(SDL_CreateDirectory("prefetch-fixture/base"));
    CHECK(SDL_CreateDirectory("prefetch-fixture/override"));
    write_fixture("prefetch-fixture/base/test.art", 17);
    write_fixture("prefetch-fixture/override/test.art", 34);
    CHECK(tig_file_repository_add("prefetch-fixture/base"));
    TigAssetPrefetchConfig config = { 32 * 1024 * 1024, 16 * 1024 * 1024, 4 * 1024 * 1024, 128, 16 };
    CHECK(tig_asset_prefetch_configure(&config));
    CHECK(tig_asset_prefetch_enable(true));
    CHECK(!tig_asset_prefetch_configure(NULL));
    tig_asset_prefetch_request("test.art", 10);
    TigAssetData base = { 0 };
    CHECK(acquire("test.art", &base));
    CHECK(base.size == 64 && base.bytes != NULL);
    if (base.bytes != NULL) CHECK(((const unsigned char*)base.bytes)[0] == 17);
    CHECK(!tig_asset_prefetch_enable(false));
    CHECK(tig_file_repository_add("prefetch-fixture/override"));
    TigAssetData unavailable = { 0 };
    CHECK(!tig_asset_prefetch_acquire("test.art", &unavailable));
    tig_asset_prefetch_request("test.art", 100);
    TigAssetData override = { 0 };
    CHECK(acquire("test.art", &override));
    if (override.bytes != NULL) CHECK(((const unsigned char*)override.bytes)[0] == 34);
    if (base.bytes != NULL) CHECK(((const unsigned char*)base.bytes)[0] == 17);
    TigFile* file = tig_file_fopen("test.art", "rb");
    CHECK(file != NULL);
    unsigned char expected[64];
    if (file != NULL) {
        CHECK(tig_file_fread(expected, 1, sizeof(expected), file) == sizeof(expected));
        if (override.bytes != NULL) CHECK(memcmp(expected, override.bytes, sizeof(expected)) == 0);
        tig_file_fclose(file);
    }
    tig_asset_prefetch_release(&base);
    tig_asset_prefetch_release(&override);
    for (int i = 0; i < 300; i++) {
        char path[64];
        SDL_snprintf(path, sizeof(path), "missing-%d.art", i);
        tig_asset_prefetch_request(path, i % 5);
    }
    tig_asset_prefetch_reset();
    CHECK(!tig_asset_prefetch_acquire("test.art", &unavailable));
    tig_asset_prefetch_request("test.art", 100);
    CHECK(acquire("test.art", &override));
    if (override.bytes != NULL) CHECK(((const unsigned char*)override.bytes)[0] == 34);
    tig_asset_prefetch_release(&override);
    for (int i = 0; i < 17; i++) {
        char directory[64];
        SDL_snprintf(directory, sizeof(directory), "prefetch-fixture/extra-%02d", i);
        CHECK(SDL_CreateDirectory(directory));
        CHECK(tig_file_repository_add(directory));
    }
    char many_sources[TIG_ASSET_SOURCE_CAPACITY][TIG_ASSET_SOURCE_PATH_CAPACITY];
    int source_count = tig_file_prefetch_sources("test.art", many_sources);
    printf("Many repositories: %d candidates, expected 19\n", source_count);
    CHECK(source_count == 19);
    if (source_count > 0) {
        tig_asset_prefetch_request("test.art", 100);
        CHECK(acquire("test.art", &override));
        if (override.bytes != NULL) CHECK(((const unsigned char*)override.bytes)[0] == 34);
        tig_asset_prefetch_release(&override);
    }
    for (int i = 0; i < 17; i++) {
        char directory[64];
        SDL_snprintf(directory, sizeof(directory), "prefetch-fixture/extra-%02d", i);
        CHECK(tig_file_repository_remove(directory));
        CHECK(SDL_RemovePath(directory));
    }
    tig_asset_prefetch_reset();
    FILE* large_file = fopen("prefetch-fixture/base/large.art", "wb");
    CHECK(large_file != NULL);
    void* block = SDL_malloc(4 * 1024 * 1024);
    CHECK(block != NULL);
    if (large_file != NULL && block != NULL) {
        memset(block, 85, 4 * 1024 * 1024);
        CHECK(fwrite(block, 4 * 1024 * 1024, 1, large_file) == 1);
    }
    if (large_file != NULL) fclose(large_file);
    SDL_free(block);
    char aliases[9][TIG_MAX_PATH];
    char* current = SDL_GetCurrentDirectory();
    CHECK(current != NULL);
    TigAssetData pinned = { 0 };
    if (current != NULL) {
        for (int i = 0; i < 9; i++) {
            SDL_snprintf(aliases[i], sizeof(aliases[i]), "%sprefetch-fixture/base/", current);
            for (int j = 0; j <= i; j++) SDL_strlcat(aliases[i], "./", sizeof(aliases[i]));
            SDL_strlcat(aliases[i], "large.art", sizeof(aliases[i]));
            tig_asset_prefetch_request(aliases[i], 100);
            TigAssetData data = { 0 };
            CHECK(acquire(aliases[i], &data));
            CHECK(data.size == 4 * 1024 * 1024);
            if (data.bytes != NULL) CHECK(((const unsigned char*)data.bytes)[data.size - 1] == 85);
            if (i == 0) pinned = data;
            else tig_asset_prefetch_release(&data);
        }
        TigAssetPrefetchStatistics statistics;
        tig_asset_prefetch_statistics(&statistics);
        CHECK(statistics.config.entries == config.entries && statistics.config.cache_bytes == config.cache_bytes);
        CHECK(statistics.resident_bytes + statistics.metadata_bytes <= config.cache_bytes);
        CHECK(statistics.evictions > 0 && statistics.pinned_entries == 1);
        CHECK(!tig_asset_prefetch_acquire(aliases[1], &unavailable));
        if (pinned.bytes != NULL) CHECK(((const unsigned char*)pinned.bytes)[0] == 85);
        tig_asset_prefetch_reset();
        if (pinned.bytes != NULL) CHECK(((const unsigned char*)pinned.bytes)[0] == 85);
        tig_asset_prefetch_release(&pinned);
        SDL_free(current);
    }
    write_archive(TIG_DATABASE_ENTRY_PLAIN);
    CHECK(tig_file_repository_add("prefetch-fixture/archive.dat"));
    char sources[TIG_ASSET_SOURCE_CAPACITY][TIG_ASSET_SOURCE_PATH_CAPACITY];
    CHECK(tig_file_prefetch_sources("test.art", sources) == 0);
    check_prefetched_equals_native("test.art", 51);
    TigFile* archived = tig_file_fopen("test.art", "rb");
    CHECK(archived != NULL);
    if (archived != NULL) { CHECK(tig_file_fgetc(archived) == 51); tig_file_fclose(archived); }
    TigAssetSource snapshot[TIG_ASSET_SOURCE_CAPACITY];
    CHECK(tig_file_snapshot_sources("test.art", snapshot) == 1);
    CHECK(snapshot[0].archive && !snapshot[0].compressed && snapshot[0].size == 64 && snapshot[0].offset == 0);
    AsyncAssetReader* snapshot_reader = async_asset_reader_create(1, 128, 128);
    CHECK(snapshot_reader != NULL);
    CHECK(async_asset_reader_request_sources(snapshot_reader, snapshot, 1, 0, 1, 0));
    AsyncAssetResult snapshot_result = { 0 };
    Uint64 deadline = SDL_GetTicks() + 3000;
    bool ready = false;
    while (!ready && SDL_GetTicks() < deadline) {
        ready = async_asset_reader_take(snapshot_reader, 0, 1, &snapshot_result);
        if (!ready) SDL_Delay(1);
    }
    CHECK(ready && snapshot_result.success && snapshot_result.size == 64);
    if (snapshot_result.bytes != NULL) CHECK(((unsigned char*)snapshot_result.bytes)[0] == 51);
    async_asset_result_release(&snapshot_result);
    async_asset_reader_destroy(snapshot_reader);
    CHECK(tig_file_repository_remove("prefetch-fixture/override"));
    CHECK(tig_file_repository_add("prefetch-fixture/override"));
    CHECK(tig_file_snapshot_sources("test.art", snapshot) == 1);
    CHECK(snapshot[0].archive);
    check_prefetched_equals_native("test.art", 51);
    CHECK(tig_file_repository_remove("prefetch-fixture/archive.dat"));
    write_archive(TIG_DATABASE_ENTRY_PLAIN | TIG_DATABASE_ENTRY_OVERRIDE_UNCHECKED);
    CHECK(tig_file_repository_add("prefetch-fixture/archive.dat"));
    CHECK(tig_file_repository_remove("prefetch-fixture/override"));
    CHECK(tig_file_repository_add("prefetch-fixture/override"));
    CHECK(tig_file_snapshot_sources("test.art", snapshot) == 2);
    CHECK(!snapshot[0].archive && snapshot[1].archive);
    check_prefetched_equals_native("test.art", 34);
    archived = tig_file_fopen("test.art", "rb");
    CHECK(archived != NULL);
    if (archived != NULL) { CHECK(tig_file_fgetc(archived) == 34); tig_file_fclose(archived); }
    CHECK(tig_file_repository_remove("prefetch-fixture/override"));
    check_prefetched_equals_native("test.art", 51);
    CHECK(tig_file_repository_remove("prefetch-fixture/archive.dat"));
    for (int field = 0; field < 2; field++) {
        write_archive(TIG_DATABASE_ENTRY_COMPRESSED);
        FILE* oversized = fopen("prefetch-fixture/archive.dat", "r+b");
        CHECK(oversized != NULL);
        if (oversized != NULL) {
            uint32_t too_large = 4 * 1024 * 1024 + 1;
            CHECK(fseek(oversized, field == 0 ? -24 : -20, SEEK_END) == 0);
            CHECK(fwrite(&too_large, sizeof(too_large), 1, oversized) == 1);
            fclose(oversized);
        }
        if (field == 1) {
            CHECK(!tig_file_repository_add("prefetch-fixture/archive.dat"));
            continue;
        }
        CHECK(tig_file_repository_add("prefetch-fixture/archive.dat"));
        tig_asset_prefetch_request("test.art", 1000);
        tig_asset_prefetch_request("large.art", 1);
        TigAssetData large = { 0 };
        CHECK(acquire("large.art", &large));
        CHECK(large.size == 4 * 1024 * 1024);
        CHECK(!tig_asset_prefetch_acquire("test.art", &unavailable));
        tig_asset_prefetch_release(&large);
        CHECK(tig_file_repository_remove("prefetch-fixture/archive.dat"));
    }
    write_archive(TIG_DATABASE_ENTRY_COMPRESSED);
    CHECK(tig_file_repository_add("prefetch-fixture/archive.dat"));
    check_prefetched_equals_native("test.art", 51);
    CHECK(tig_file_snapshot_sources("test.art", snapshot) == 1);
    CHECK(snapshot[0].archive && snapshot[0].compressed && snapshot[0].stored_size == 12);
    tig_asset_prefetch_request("test.art", 1);
    CHECK(acquire("test.art", &pinned));
    CHECK(tig_file_repository_remove("prefetch-fixture/archive.dat"));
    CHECK(!tig_asset_prefetch_acquire("test.art", &unavailable));
    check_prefetched_equals_native("test.art", 17);
    if (pinned.bytes != NULL) CHECK(((const unsigned char*)pinned.bytes)[0] == 51);
    tig_asset_prefetch_release(&pinned);
    write_archive(TIG_DATABASE_ENTRY_COMPRESSED | TIG_DATABASE_ENTRY_OVERRIDE_UNCHECKED);
    CHECK(tig_file_repository_add("prefetch-fixture/archive.dat"));
    CHECK(tig_file_repository_add("prefetch-fixture/override"));
    check_prefetched_equals_native("test.art", 34);
    tig_file_ignore("test.art", TIG_FILE_IGNORE_DATABASE);
    CHECK(!tig_asset_prefetch_acquire("test.art", &unavailable));
    check_prefetched_equals_native("test.art", 34);
    CHECK(tig_asset_prefetch_enable(false));
    FILE* retention = fopen("prefetch-fixture/base/retention.art", "wb");
    CHECK(retention != NULL);
    if (retention != NULL) {
        unsigned char chunk[65536]; memset(chunk, 91, sizeof(chunk));
        for (int index = 0; index < 8; index++) CHECK(fwrite(chunk, sizeof(chunk), 1, retention) == 1);
        fclose(retention);
        char paths[8][TIG_MAX_PATH];
        char* retention_directory = SDL_GetCurrentDirectory();
        CHECK(retention_directory != NULL);
        int hits[2] = { 0 };
        for (int profile = 0; profile < 2; profile++) {
            TigAssetPrefetchConfig retention_config = { (profile == 0 ? 2 : 8) * 1024 * 1024,
                4 * 1024 * 1024, 512 * 1024, 128, 4 };
            CHECK(tig_asset_prefetch_configure(&retention_config));
            CHECK(tig_asset_prefetch_enable(true));
            for (int index = 0; index < 8; index++) {
                SDL_snprintf(paths[index], sizeof(paths[index]), "%sprefetch-fixture/base/", retention_directory != NULL ? retention_directory : "");
                for (int prefix = 0; prefix <= index; prefix++) SDL_strlcat(paths[index], "./", sizeof(paths[index]));
                SDL_strlcat(paths[index], "retention.art", sizeof(paths[index]));
                tig_asset_prefetch_request(paths[index], 100);
                TigAssetData data = { 0 };
                CHECK(acquire(paths[index], &data));
                CHECK(data.size == 512 * 1024);
                if (data.bytes != NULL) CHECK(((const unsigned char*)data.bytes)[data.size - 1] == 91);
                tig_asset_prefetch_release(&data);
            }
            for (int index = 0; index < 8; index++) {
                TigAssetData data = { 0 };
                if (tig_asset_prefetch_acquire(paths[index], &data)) { hits[profile]++; tig_asset_prefetch_release(&data); }
            }
            TigAssetPrefetchStatistics statistics; tig_asset_prefetch_statistics(&statistics);
            CHECK(statistics.resident_bytes + statistics.metadata_bytes <= retention_config.cache_bytes);
            if (profile == 0) CHECK(statistics.evictions > 0);
            else CHECK(statistics.evictions == 0);
            CHECK(tig_asset_prefetch_enable(false));
        }
        CHECK(hits[0] < hits[1] && hits[1] == 8);
        printf("Retention workload: %d/8 hits with 2 MiB, %d/8 with 8 MiB\n", hits[0], hits[1]);
        SDL_free(retention_directory);
        remove("prefetch-fixture/base/retention.art");
    }
    CHECK(tig_asset_prefetch_configure(NULL));
    CHECK(tig_asset_prefetch_enable(true));
    TigAssetPrefetchStatistics automatic;
    tig_asset_prefetch_statistics(&automatic);
    CHECK(automatic.config.entries > 0 && automatic.metadata_bytes < automatic.config.cache_bytes);
    tig_asset_prefetch_request("test.art", 1);
    TigAssetData automatic_data = { 0 };
    CHECK(acquire("test.art", &automatic_data));
    CHECK(automatic_data.size == 64);
    tig_asset_prefetch_release(&automatic_data);
    if (automatic.config.file_limit >= 5 * 1024 * 1024) {
        FILE* extended = fopen("prefetch-fixture/base/extended.art", "wb");
        CHECK(extended != NULL);
        if (extended != NULL) {
            unsigned char chunk[65536]; memset(chunk, 123, sizeof(chunk));
            for (int index = 0; index < 80; index++) CHECK(fwrite(chunk, sizeof(chunk), 1, extended) == 1);
            fclose(extended);
            tig_asset_prefetch_request("extended.art", 1);
            TigAssetData extended_data = { 0 };
            CHECK(acquire("extended.art", &extended_data));
            CHECK(extended_data.size == 5 * 1024 * 1024);
            if (extended_data.bytes != NULL) CHECK(((const unsigned char*)extended_data.bytes)[extended_data.size - 1] == 123);
            tig_asset_prefetch_release(&extended_data);
            remove("prefetch-fixture/base/extended.art");
        }
    }

    CHECK(tig_asset_prefetch_enable(false));
    config.entries = 0; CHECK(!tig_asset_prefetch_configure(&config)); config.entries = 128;
    config.file_limit = SIZE_MAX; CHECK(!tig_asset_prefetch_configure(&config));
    tig_file_exit();
    tig_memory_exit();
    remove("prefetch-fixture/archive.dat");
    remove("prefetch-fixture/base/large.art");
    remove("prefetch-fixture/base/test.art");
    remove("prefetch-fixture/override/test.art");
    SDL_RemovePath("prefetch-fixture/base");
    SDL_RemovePath("prefetch-fixture/override");
    SDL_RemovePath("prefetch-fixture");
    SDL_Quit();
    printf("Native asset prefetch: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures != 0;
}
