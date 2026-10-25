#include <stdio.h>
#include "game/terrain_storage.h"
#include <tig/file.h>
#include <tig/memory.h>

static int failures;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #value); failures++; } } while (0)

static void compressed_roundtrip(void)
{
    uint16_t source[3][4096];
    uint16_t decoded[4096];
    for (int row = 0; row < 3; row++) for (int x = 0; x < 4096; x++) source[row][x] = (uint16_t)(x * 17113 + row * 3907);
    CHECK(SDL_Init(0));
    tig_memory_init(NULL);
    CHECK(tig_file_repository_add("."));
    TigFile* file = tig_file_fopen("terrain-rows.bin", "wb");
    CHECK(terrain_storage_rows_write(file, &source[0][0], 4096, 3));
    tig_file_fclose(file);
    file = tig_file_fopen("terrain-rows.bin", "rb");
    for (int row = 0; row < 3; row++) {
        int size;
        CHECK(tig_file_fread(&size, sizeof(size), 1, file) == 1);
        CHECK(size > 0 && size <= 2 * sizeof(decoded));
        if (size <= 0 || size > 2 * sizeof(decoded)) break;
        unsigned char encoded[2 * sizeof(decoded)];
        CHECK(tig_file_fread(encoded, 1, size, file) == (size_t)size);
        CHECK(terrain_storage_inflate_row(decoded, sizeof(decoded), encoded, size));
        CHECK(memcmp(source[row], decoded, sizeof(decoded)) == 0);
        for (int prefix = 0; prefix < size; prefix++) CHECK(!terrain_storage_inflate_row(decoded, sizeof(decoded), encoded, prefix));
        CHECK(!terrain_storage_inflate_row(decoded, sizeof(decoded) - 1, encoded, size));
        CHECK(!terrain_storage_inflate_row(decoded, sizeof(decoded) + 1, encoded, size));
        encoded[size - 1] ^= 1;
        CHECK(!terrain_storage_inflate_row(decoded, sizeof(decoded), encoded, size));
    }
    CHECK(tig_file_ftell(file) == tig_file_filelength(file));
    tig_file_fclose(file);
    CHECK(!terrain_storage_rows_write(NULL, &source[0][0], 4096, 3));
    CHECK(!terrain_storage_rows_write(NULL, &source[0][0], INT64_MAX, 3));
    remove("terrain-rows.bin");
    tig_file_exit();
    tig_memory_exit();
    SDL_Quit();
}

int main(void)
{
    compressed_roundtrip();
    int bytes = 0;
    CHECK(terrain_storage_size(4096, 4096, &bytes) && bytes == 33554432);
    CHECK(terrain_storage_size(1, INT_MAX / 2, &bytes) && bytes == INT_MAX - 1);
    CHECK(!terrain_storage_size(INT64_MAX, INT64_MAX, &bytes));
    CHECK(!terrain_storage_size(65536, 65536, &bytes));
    CHECK(!terrain_storage_size(-1, 10, &bytes));
    CHECK(!terrain_storage_size(10, 0, &bytes));
    unsigned char rows[] = { 1, 0, 0, 0, 17, 2, 0, 0, 0, 18, 19 };
    CHECK(terrain_storage_rows_valid(rows, sizeof(rows), 2));
    for (size_t size = 0; size < sizeof(rows); size++) CHECK(!terrain_storage_rows_valid(rows, size, 2));
    CHECK(!terrain_storage_rows_valid(rows, sizeof(rows), INT64_MAX));
    rows[5] = 255;
    CHECK(!terrain_storage_rows_valid(rows, sizeof(rows), 2));
    rows[5] = 0;
    CHECK(!terrain_storage_rows_valid(rows, sizeof(rows), 2));
    printf("Terrain storage: %d failures\n", failures);
    return failures != 0;
}
