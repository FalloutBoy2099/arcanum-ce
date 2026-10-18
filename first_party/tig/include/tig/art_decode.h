#ifndef TIG_ART_DECODE_H_
#define TIG_ART_DECODE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Decode one complete ART frame. Both the encoded stream and output must be
// consumed exactly; malformed packets never write beyond the output buffer.
bool tig_art_decode_rle(const uint8_t* encoded, size_t encoded_size, uint8_t* pixels, size_t pixel_size);

#ifdef __cplusplus
}
#endif

#endif
