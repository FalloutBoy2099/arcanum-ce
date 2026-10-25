#ifndef TIG_PIXEL_BUFFER_H_
#define TIG_PIXEL_BUFFER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

static inline bool tig_pixel_buffer_row_size(int width, int bits_per_pixel, int* pitch)
{
    if (width <= 0 || (bits_per_pixel != 1 && bits_per_pixel != 4 && bits_per_pixel != 8
        && bits_per_pixel != 16 && bits_per_pixel != 24 && bits_per_pixel != 32)) return false;
    uint64_t row = ((uint64_t)width * bits_per_pixel + 7) / 8;
    row = (row + 3) & ~UINT64_C(3);
    if (row > INT_MAX) return false;
    *pitch = (int)row;
    return true;
}

static inline bool tig_pixel_buffer_size(int width, int height, int bits_per_pixel, int pitch, size_t* bytes)
{
    int minimum_pitch;
    if (height <= 0 || !tig_pixel_buffer_row_size(width, bits_per_pixel, &minimum_pitch)
        || pitch < minimum_pitch) return false;
    uint64_t total = (uint64_t)pitch * height;
    if (total > INT_MAX || total > SIZE_MAX) return false;
    *bytes = (size_t)total;
    return true;
}

#endif
