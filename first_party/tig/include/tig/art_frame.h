#ifndef TIG_ART_FRAME_H_
#define TIG_ART_FRAME_H_

#include <string.h>
#include "tig/pixel_buffer.h"

static inline bool tig_art_frame_convert(const uint8_t* source, size_t source_size, int width, int height,
    const uint32_t* palette, bool mirrored, void* destination, int destination_width, int destination_height, int pitch)
{
    size_t destination_size;
    if (source == NULL || palette == NULL || destination == NULL || width <= 0 || height <= 0
        || (uint64_t)width * height > source_size || width > destination_width || height > destination_height
        || !tig_pixel_buffer_size(destination_width, destination_height, 32, pitch, &destination_size)) return false;
    for (int y = 0; y < height; y++) {
        const uint8_t* row = source + (size_t)y * width;
        uint8_t* target = (uint8_t*)destination + (size_t)y * pitch;
        for (int x = 0; x < width; x++) {
            uint8_t index = row[mirrored ? width - 1 - x : x];
            uint32_t color = index != 0 ? palette[index] : 0;
            memcpy(target + (size_t)x * 4, &color, sizeof(color));
        }
    }
    return true;
}

#endif
