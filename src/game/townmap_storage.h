#ifndef ARCANUM_TOWNMAP_STORAGE_H_
#define ARCANUM_TOWNMAP_STORAGE_H_
#include "game/townmap.h"
#include <limits.h>
#include <math.h>

static inline bool townmap_storage_valid(const TownMapInfo* info)
{
    if (info == NULL || info->version != 2 || info->width <= 0 || info->height <= 0
        || info->num_hor_tiles <= 0 || info->num_vert_tiles <= 0
        || info->num_hor_tiles > info->width || info->num_vert_tiles > info->height
        || info->num_hor_tiles > INT_MAX / info->num_vert_tiles
        || !isfinite(info->scale) || info->scale <= 0) return false;
    double scaled_width = (double)info->width * info->scale;
    double scaled_height = (double)info->height * info->scale;
    return scaled_width <= INT_MAX && scaled_height <= INT_MAX
        && (double)(info->width / info->num_hor_tiles) * info->scale >= 1
        && (double)(info->height / info->num_vert_tiles) * info->scale >= 1;
}
#endif
