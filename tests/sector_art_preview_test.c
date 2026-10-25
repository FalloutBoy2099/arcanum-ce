#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/sector_art_preview.h"

static int failures;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #value); failures++; } } while (0)

static void put_uint32(unsigned char* bytes, uint32_t value)
{
    for (int i = 0; i < 4; i++) bytes[i] = (unsigned char)(value >> (i * 8));
}

int main(void)
{
    size_t size = 4 + 48 + 4096 * 4 + 4 + 256 * 4;
    unsigned char* data = calloc(1, size);
    uint32_t ids[SECTOR_ART_PREVIEW_CAPACITY];
    size_t count = 99;
    CHECK(data != NULL);
    if (data == NULL) return 1;
    put_uint32(data, 1);
    for (int i = 0; i < 4096; i++) put_uint32(data + 52 + i * 4, (uint32_t)(i % 5) + 100);
    for (int i = 0; i < 256; i++) put_uint32(data + 52 + 4096 * 4 + 4 + i * 4, UINT32_MAX);
    put_uint32(data + 52 + 4096 * 4 + 4, 999);
    CHECK(sector_art_preview_read(data, size, ids, &count));
    CHECK(count == 6 && ids[0] == 100 && ids[4] == 104 && ids[5] == 999);
    for (size_t length = 0; length < size; length++) {
        CHECK(!sector_art_preview_read(data, length, ids, &count));
        CHECK(count == 0);
    }
    put_uint32(data + 52 + 4096 * 4, 1);
    CHECK(sector_art_preview_read(data, size - 256 * 4, ids, &count));
    CHECK(count == 5);
    put_uint32(data + 52 + 4096 * 4, 2);
    CHECK(!sector_art_preview_read(data, size, ids, &count));
    put_uint32(data, UINT32_MAX);
    CHECK(!sector_art_preview_read(data, size, ids, &count));
    CHECK(!sector_art_preview_read(NULL, size, ids, &count));
    CHECK(!sector_art_preview_read(data, size, NULL, &count));
    CHECK(!sector_art_preview_read(data, size, ids, NULL));
    free(data);
    printf("Sector art preview: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures != 0;
}
