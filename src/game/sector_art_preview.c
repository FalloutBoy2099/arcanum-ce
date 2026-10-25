#include "game/sector_art_preview.h"

#include <stdlib.h>
#include <string.h>

static uint32_t preview_uint32(const unsigned char* bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8)
        | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static int preview_compare_art(const void* a, const void* b)
{
    uint32_t left = *(const uint32_t*)a;
    uint32_t right = *(const uint32_t*)b;
    return (left > right) - (left < right);
}

bool sector_art_preview_read(const void* bytes, size_t size, uint32_t* art_ids, size_t* count)
{
    if (count == NULL) return false;
    *count = 0;
    if (bytes == NULL || art_ids == NULL || size < 4) return false;
    const unsigned char* input = bytes;
    uint32_t lights = preview_uint32(input);
    if (lights > (size - 4) / 48) return false;
    size_t offset = 4 + (size_t)lights * 48;
    if (size - offset < 4096 * 4 + 4) return false;
    uint32_t empty_roofs = preview_uint32(input + offset + 4096 * 4);
    if (empty_roofs > 1) return false;
    size_t total = empty_roofs ? 4096 : SECTOR_ART_PREVIEW_CAPACITY;
    if (!empty_roofs && size - offset < 4096 * 4 + 4 + 256 * 4) return false;
    for (size_t i = 0; i < 4096; i++) art_ids[i] = preview_uint32(input + offset + i * 4);
    for (size_t i = 4096; i < total; i++) art_ids[i] = preview_uint32(input + offset + 4 + i * 4);
    qsort(art_ids, total, sizeof(*art_ids), preview_compare_art);
    for (size_t i = 0; i < total; i++) {
        if (art_ids[i] != UINT32_MAX && (*count == 0 || art_ids[i] != art_ids[*count - 1]))
            art_ids[(*count)++] = art_ids[i];
    }
    return true;
}
