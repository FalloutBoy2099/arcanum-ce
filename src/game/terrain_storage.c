#include "game/terrain_storage.h"
#include <tig/file.h>
#include <tig/memory.h>

bool terrain_storage_rows_write(TigFile* stream, const uint16_t* data, int64_t width, int64_t height)
{
    int total_size;
    if (stream == NULL || data == NULL || !terrain_storage_size(width, height, &total_size)) return false;
    uLong row_size = (uLong)width * sizeof(*data);
    uLong bound = compressBound(row_size);
    unsigned char* encoded = MALLOC(bound);
    if (encoded == NULL) return false;
    bool success = true;
    for (int64_t row = 0; row < height; row++) {
        uLongf produced = bound;
        if (compress2(encoded, &produced, (const Bytef*)(data + (size_t)row * (size_t)width), row_size, Z_BEST_COMPRESSION) != Z_OK
            || produced > INT_MAX) {
            success = false;
            break;
        }
        int stored_size = (int)produced;
        if (tig_file_fwrite(&stored_size, sizeof(stored_size), 1, stream) != 1
            || tig_file_fwrite(encoded, 1, produced, stream) != produced) {
            success = false;
            break;
        }
    }
    FREE(encoded);
    return success;
}
