#include "game/terrain_storage.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <tig/database.h>
#include <tig/file_position.h>
#include <tig/memory.h>
#include <tig/file.h>
#include <tig/file_cache.h>
#include <zlib.h>
#include <tig/async_asset_reader.h>
#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#include <io.h>
#endif

static int failures;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #value); failures++; } } while (0)

static void fixture(uint32_t start, bool compressed, bool absolute, int damage)
{
    FILE* file = fopen("storage.dat", "wb");
    CHECK(file != NULL);
    if (file == NULL) return;
#ifdef _WIN32
    DWORD returned;
    CHECK(DeviceIoControl((HANDLE)_get_osfhandle(_fileno(file)), FSCTL_SET_SPARSE,
        NULL, 0, NULL, 0, &returned, NULL));
#endif
    CHECK(tig_stdio_seek(file, start, SEEK_SET) == 0);
    const unsigned char packed[] = { 0x78, 0x9c, 0x33, 0x36, 0xa6, 0x0c, 0x00, 0x00, 0x9e, 0xaf, 0x0c, 0xc1 };
    uint32_t stored = compressed ? sizeof(packed) : 64;
    if (compressed) fwrite(packed, sizeof(packed), 1, file);
    else for (int i = 0; i < 64; i++) fputc(51, file);
    uint32_t prefix[] = { absolute ? 0x80000000U : stored + 4, damage == 1 ? UINT32_MAX : 1, damage == 2 ? UINT32_MAX : 9 };
    uint32_t entry[] = { 0, compressed ? TIG_DATABASE_ENTRY_COMPRESSED : TIG_DATABASE_ENTRY_PLAIN, 64, stored, absolute ? start : 0 };
    if (damage == 4) entry[2] = UINT32_MAX;
    if (damage == 5) entry[4] = UINT32_MAX;
    uint32_t footer[] = { SDL_FOURCC(' ', 'T', 'A', 'D'), damage == 6 ? UINT32_MAX : 9, 49 };
    fwrite(prefix, sizeof(prefix), 1, file);
    fwrite(damage == 3 ? "test.artX" : "test.art", 9, 1, file);
    fwrite(entry, sizeof(entry), 1, file);
    fwrite(footer, sizeof(footer), 1, file);
    fclose(file);
}

static void read_archive(uint32_t start, bool compressed, bool absolute)
{
    fixture(start, compressed, absolute, 0);
    TigDatabase* database = tig_database_open("storage.dat");
    CHECK(database != NULL);
    if (database == NULL) { remove("storage.dat"); return; }
    TigDatabaseEntry* entry = NULL;
    CHECK(tig_database_get_entry(database, "test.art", &entry));
    CHECK(entry != NULL && entry->offset == start);
    TigDatabaseFileHandle* file = tig_database_fopen(database, "test.art", "rb");
    CHECK(file != NULL);
    if (file != NULL) {
        unsigned char bytes[64] = { 0 };
        CHECK(tig_database_fread(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
        for (int i = 0; i < 64; i++) CHECK(bytes[i] == 51);
        CHECK(tig_database_fseek(file, -32, SEEK_END) == 0);
        CHECK(tig_database_fgetc(file) == 51);
        CHECK(tig_database_fseek(file, 0, SEEK_SET) == 0);
        CHECK(tig_database_fgetc(file) == 51);
        CHECK(tig_database_fseek(file, INT_MAX, SEEK_CUR) != 0);
        CHECK(tig_database_fclose(file) == 0);
    }
    if (entry != NULL) {
        TigAssetSource source = { .archive = true, .compressed = compressed,
            .offset = entry->offset, .size = entry->size, .stored_size = entry->compressed_size };
        char* current = SDL_GetCurrentDirectory();
        SDL_snprintf(source.path, sizeof(source.path), "%sstorage.dat", current);
        SDL_free(current);
        AsyncAssetReader* reader = async_asset_reader_create(1, 64, 64);
        CHECK(reader != NULL);
        if (reader != NULL) {
            CHECK(async_asset_reader_request_sources(reader, &source, 1, 0, 1, 0));
            AsyncAssetResult result = { 0 };
            bool ready = false;
            Uint64 deadline = SDL_GetTicks() + 3000;
            while (!(ready = async_asset_reader_take(reader, 0, 1, &result)) && SDL_GetTicks() < deadline) SDL_Delay(1);
            CHECK(ready && result.success && result.size == 64);
            if (result.success) for (int i = 0; i < 64; i++) CHECK(((unsigned char*)result.bytes)[i] == 51);
            async_asset_result_release(&result);
            async_asset_reader_destroy(reader);
        }
    }
    CHECK(tig_database_close(database));
    CHECK(remove("storage.dat") == 0);
}

static void check_real_archive(const char* path)
{
    TigDatabase* database = tig_database_open(path);
    CHECK(database != NULL);
    if (database == NULL) return;
    unsigned int read_count = 0;
    unsigned int terrain_count = 0;
    for (unsigned int i = 0; i < database->entries_count; i++) {
        TigDatabaseEntry* entry = &database->entries[i];
        if ((entry->flags & TIG_DATABASE_ENTRY_DIRECTORY) != 0) continue;
        TigDatabaseFileHandle* file = tig_database_fopen_entry(database, entry, "rb");
        CHECK(file != NULL);
        if (file == NULL) continue;
        if (strstr(entry->path, "terrain.tdf") != NULL && entry->size >= 32) {
            unsigned char* data = SDL_malloc(entry->size);
            CHECK(data != NULL);
            if (data != NULL) {
                CHECK(tig_database_fread(data, 1, entry->size, file) == entry->size);
                int64_t width, height;
                uint32_t flags;
                int expanded_size;
                memcpy(&flags, data + 4, 4);
                memcpy(&width, data + 8, 8);
                memcpy(&height, data + 16, 8);
                CHECK(terrain_storage_size(width, height, &expanded_size));
                if ((flags & 1) != 0) CHECK(terrain_storage_rows_valid(data + 32, entry->size - 32, height));
                else CHECK((uint64_t)width * height * 2 <= entry->size - 32);
                SDL_free(data);
                terrain_count++;
                CHECK(tig_database_fseek(file, 0, SEEK_SET) == 0);
            }
        }
        unsigned char buffer[16384];
        unsigned int remaining = entry->size;
        while (remaining != 0) {
            size_t count = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
            size_t actual = tig_database_fread(buffer, 1, count, file);
            CHECK(actual == count);
            if (actual != count) break;
            remaining -= (unsigned int)count;
        }
        CHECK(remaining == 0);
        tig_database_fclose(file);
        read_count++;
    }
    printf("Archive %s: %u complete files read, %u terrain files validated\n", path, read_count, terrain_count);
    CHECK(tig_database_close(database));
}

static void stream_fixture(const unsigned char* bytes, size_t length, bool compressed, int advertised_adjustment, bool bad_checksum)
{
    unsigned char packed[512];
    uLongf packed_length = sizeof(packed);
    if (compressed) {
        CHECK(compress2(packed, &packed_length, bytes, (uLong)length, Z_BEST_SPEED) == Z_OK);
        if (bad_checksum) packed[packed_length - 1] ^= 0xff;
    }
    FILE* file = fopen("stream.dat", "wb");
    CHECK(file != NULL);
    if (file == NULL) return;
    uint32_t stored = compressed ? (uint32_t)packed_length : (uint32_t)length;
    CHECK(fwrite(compressed ? packed : bytes, 1, stored, file) == stored);
    uint32_t prefix[] = { stored + 4, 1, 9 };
    uint32_t entry[] = { 0, compressed ? TIG_DATABASE_ENTRY_COMPRESSED : TIG_DATABASE_ENTRY_PLAIN,
        (uint32_t)((int)length + advertised_adjustment), stored, 0 };
    uint32_t footer[] = { SDL_FOURCC(' ', 'T', 'A', 'D'), 9, 49 };
    fwrite(prefix, sizeof(prefix), 1, file);
    fwrite("test.bin", 9, 1, file);
    fwrite(entry, sizeof(entry), 1, file);
    fwrite(footer, sizeof(footer), 1, file);
    fclose(file);
}

static void stream_boundaries(bool compressed)
{
    unsigned char expected[64];
    for (int i = 0; i < 64; i++) expected[i] = (unsigned char)(i + 1);
    stream_fixture(expected, sizeof(expected), compressed, 0, false);
    TigDatabase* database = tig_database_open("stream.dat");
    CHECK(database != NULL);
    if (database == NULL) return;
    TigDatabaseFileHandle* file = tig_database_fopen(database, "test.bin", "rb");
    CHECK(file != NULL);
    if (file != NULL) {
        unsigned char data[80];
        memset(data, 0xaa, sizeof(data));
        CHECK(tig_database_fread(data, 0, SIZE_MAX, file) == 0);
        CHECK(tig_database_fread(data, SIZE_MAX, 0, file) == 0);
        CHECK(tig_database_ftell(file) == 0 && !tig_database_ferror(file) && !tig_database_feof(file));
        CHECK(tig_database_fread(data, SIZE_MAX / 2 + 1, 2, file) == 0);
        CHECK(tig_database_ferror(file) && tig_database_ftell(file) == 0 && data[0] == 0xaa);
        tig_database_rewind(file);
        CHECK(tig_database_ungetc(99, file) == -1);
        CHECK(tig_database_ftell(file) == 0);
        CHECK(tig_database_fread(data, 1, 5, file) == 5);
        CHECK(memcmp(data, expected, 5) == 0);
        CHECK(tig_database_ungetc(99, file) == 99);
        CHECK(tig_database_ungetc(100, file) == -1);
        CHECK(tig_database_ftell(file) == 4);
        CHECK(tig_database_fseek(file, 0, SEEK_CUR) == 0);
        CHECK(tig_database_fgetc(file) == 5);
        CHECK(tig_database_fread(data, 1, 59, file) == 59);
        CHECK(memcmp(data, expected + 5, 59) == 0);
        CHECK(tig_database_ftell(file) == 64 && !tig_database_ferror(file) && !tig_database_feof(file));
        CHECK(tig_database_fread(data, 1, 1, file) == 0);
        CHECK(tig_database_feof(file) && !tig_database_ferror(file));
        CHECK(tig_database_ungetc(77, file) == 77);
        CHECK(!tig_database_feof(file));
        CHECK(tig_database_fread(data, 1, 3, file) == 1 && data[0] == 77);
        CHECK(tig_database_feof(file) && !tig_database_ferror(file) && tig_database_ftell(file) == 64);
        CHECK(tig_database_fseek(file, 0, SEEK_SET) == 0);
        memset(data, 0xaa, sizeof(data));
        CHECK(tig_database_fread(data, 3, 22, file) == 21);
        CHECK(memcmp(data, expected, 64) == 0 && data[64] == 0xaa);
        CHECK(tig_database_ftell(file) == 64 && tig_database_feof(file) && !tig_database_ferror(file));
        CHECK(tig_database_fread(NULL, 1, 1, file) == 0);
        CHECK(tig_database_ferror(file));
        tig_database_fclose(file);
    }
    CHECK(tig_database_close(database));
    remove("stream.dat");
}

static void text_streams(bool compressed)
{
    const unsigned char text[] = "one\r\ntwo\rX\nlast";
    stream_fixture(text, sizeof(text) - 1, compressed, 0, false);
    TigDatabase* database = tig_database_open("stream.dat");
    CHECK(database != NULL);
    if (database == NULL) return;
    TigDatabaseFileHandle* file = tig_database_fopen(database, "test.bin", "rt");
    CHECK(file != NULL);
    if (file != NULL) {
        char line[32];
        CHECK(tig_database_fgets(line, 0, file) == NULL);
        CHECK(tig_database_fgets(NULL, 4, file) == NULL);
        CHECK(tig_database_fgets(line, 1, file) == line && line[0] == '\0');
        CHECK(tig_database_ftell(file) == 0);
        CHECK(tig_database_fgets(line, sizeof(line), file) == line && strcmp(line, "one\n") == 0);
        CHECK(tig_database_fgets(line, sizeof(line), file) == line && strcmp(line, "two\rX\n") == 0);
        CHECK(tig_database_fgets(line, sizeof(line), file) == line && strcmp(line, "last") == 0);
        CHECK(tig_database_feof(file) && !tig_database_ferror(file));
        CHECK(tig_database_fgets(line, sizeof(line), file) == NULL);
        tig_database_fclose(file);
    }
    CHECK(tig_database_close(database));
    remove("stream.dat");
}

static void interrupted_plain_read(void)
{
    unsigned char expected[64];
    for (int i = 0; i < 64; i++) expected[i] = (unsigned char)i;
    stream_fixture(expected, sizeof(expected), false, 0, false);
    TigDatabase* database = tig_database_open("stream.dat");
    CHECK(database != NULL);
    if (database == NULL) return;
    TigDatabaseFileHandle* file = tig_database_fopen(database, "test.bin", "rb");
    CHECK(file != NULL);
    if (file != NULL) {
        FILE* writer = fopen("stream.dat", "wb");
        CHECK(writer != NULL);
        if (writer != NULL) {
            CHECK(fwrite(expected, 1, 32, writer) == 32);
            fclose(writer);
            unsigned char data[80];
            memset(data, 0xaa, sizeof(data));
            CHECK(tig_database_fread(data, 1, 64, file) == 32);
            CHECK(memcmp(data, expected, 32) == 0 && data[32] == 0xaa);
            CHECK(tig_database_ftell(file) == 32 && tig_database_ferror(file));
            CHECK(tig_database_fread(data, 1, 1, file) == 0 && tig_database_ftell(file) == 32);
        }
        tig_database_fclose(file);
    }
    CHECK(tig_database_close(database));
    remove("stream.dat");
}

static void compressed_failures(void)
{
    unsigned char expected[64];
    memset(expected, 51, sizeof(expected));
    for (int damaged_checksum = 0; damaged_checksum < 2; damaged_checksum++) {
        stream_fixture(expected, sizeof(expected), true, damaged_checksum ? 0 : 16, damaged_checksum != 0);
        TigDatabase* database = tig_database_open("stream.dat");
        CHECK(database != NULL);
        if (database == NULL) continue;
        TigDatabaseFileHandle* file = tig_database_fopen(database, "test.bin", "rb");
        CHECK(file != NULL);
        if (file != NULL) {
            unsigned char data[80];
            memset(data, 0xaa, sizeof(data));
            CHECK(tig_database_fread(data, 1, sizeof(data), file) == 64);
            CHECK(memcmp(data, expected, sizeof(expected)) == 0 && data[64] == 0xaa);
            CHECK(tig_database_ftell(file) == 64 && tig_database_ferror(file));
            CHECK(tig_database_fread(data, 1, 1, file) == 0);
            if (damaged_checksum) {
                tig_database_rewind(file);
                CHECK(tig_database_fseek(file, 64, SEEK_SET) != 0);
                CHECK(tig_database_ferror(file));
            }
            tig_database_fclose(file);
        }
        CHECK(tig_database_close(database));
        CHECK(tig_file_repository_add("stream.dat"));
        TigFileCache* cache = tig_file_cache_create(2, 1024);
        CHECK(cache != NULL);
        if (cache != NULL) {
            TigFileCacheEntry* item = tig_file_cache_acquire(cache, "test.bin");
            CHECK(item->data == NULL && cache->bytes == 0 && cache->items_count == 0);
            tig_file_cache_destroy(cache);
        }
        CHECK(tig_file_repository_remove("stream.dat"));
        remove("stream.dat");
    }
}

int main(int argc, char** argv)
{
    CHECK(SDL_Init(0));
    CHECK(tig_memory_init(NULL) == TIG_OK);
    stream_boundaries(false); stream_boundaries(true);
    text_streams(false); text_streams(true);
    interrupted_plain_read();
    compressed_failures();
    for (int packed = 0; packed < 2; packed++) {
        read_archive(0, packed != 0, false);
        read_archive((uint32_t)INT_MAX + 4096U, packed != 0, false);
        read_archive((uint32_t)INT_MAX + 4096U, packed != 0, true);
    }
    for (int damage = 1; damage <= 6; damage++) {
        fixture(0, false, false, damage);
        TigDatabase* database = tig_database_open("storage.dat");
        CHECK(database == NULL);
        if (database != NULL) tig_database_close(database);
        remove("storage.dat");
    }
    for (int i = 1; i < argc; i++) check_real_archive(argv[i]);
    tig_file_exit();
    tig_memory_exit();
    SDL_Quit();
    printf("Database storage: %d failures\n", failures);
    return failures != 0;
}
