#ifndef ARCANUM_TERRAIN_STORAGE_H_
#define ARCANUM_TERRAIN_STORAGE_H_

#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <zlib.h>

static inline bool terrain_storage_size(int64_t width, int64_t height, int* bytes)
{
    if (width <= 0 || height <= 0 || width > INT_MAX / 2
        || height > (INT_MAX / 2) / width) return false;
    *bytes = (int)(width * height * 2);
    return true;
}

static inline bool terrain_storage_rows_valid(const void* data, size_t size, int64_t height)
{
    const unsigned char* cursor = data;
    if (data == NULL || height <= 0 || (uint64_t)height > size / 5) return false;
    for (int64_t row = 0; row < height; row++) {
        uint32_t length;
        if (size < sizeof(length)) return false;
        memcpy(&length, cursor, sizeof(length));
        cursor += sizeof(length);
        size -= sizeof(length);
        if (length == 0 || length > INT_MAX || length > size) return false;
        cursor += length;
        size -= length;
    }
    return true;
}

static inline bool terrain_storage_inflate_row(void* destination, size_t output_size, const void* source, size_t input_size)
{
    if (destination == NULL || source == NULL || output_size == 0 || output_size > INT_MAX
        || input_size == 0 || input_size > UINT_MAX) return false;
    uLongf produced = (uLongf)output_size;
    return uncompress(destination, &produced, source, (uLong)input_size) == Z_OK && produced == output_size;
}

struct TigFile;
bool terrain_storage_rows_write(struct TigFile* stream, const uint16_t* data, int64_t width, int64_t height);

#endif
