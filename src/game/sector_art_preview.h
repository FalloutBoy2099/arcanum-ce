#ifndef ARCANUM_SECTOR_ART_PREVIEW_H_
#define ARCANUM_SECTOR_ART_PREVIEW_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SECTOR_ART_PREVIEW_CAPACITY 4352

bool sector_art_preview_read(const void* bytes, size_t size, uint32_t* art_ids, size_t* count);

#endif
