#include "tig/art_decode.h"
#include <string.h>

bool tig_art_decode_rle(const uint8_t* encoded, size_t encoded_size, uint8_t* pixels, size_t pixel_size)
{
    if ((encoded_size != 0 && encoded == NULL) || (pixel_size != 0 && pixels == NULL)) return false;
    size_t input = 0, output = 0;
    while (input < encoded_size) {
        unsigned int control = encoded[input++];
        size_t count = control & 0x7F;
        if (count > pixel_size - output) return false;
        if ((control & 0x80) != 0) {
            if (count > encoded_size - input) return false;
            if (count != 0) memcpy(pixels + output, encoded + input, count);
            input += count;
        } else {
            if (input == encoded_size) return false;
            uint8_t color = encoded[input++];
            if (count != 0) memset(pixels + output, color, count);
        }
        output += count;
    }
    return output == pixel_size;
}
