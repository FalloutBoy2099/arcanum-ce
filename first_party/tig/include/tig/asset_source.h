#ifndef TIG_ASSET_SOURCE_H_
#define TIG_ASSET_SOURCE_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct TigAssetSource {
    char path[512];
    int64_t offset;
    uint32_t size;
    uint32_t stored_size;
    bool archive;
    bool compressed;
} TigAssetSource;

#define TIG_ASSET_SOURCE_CAPACITY 64
#define TIG_ASSET_SOURCE_PATH_CAPACITY 512

#endif
