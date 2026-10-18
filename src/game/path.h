#ifndef ARCANUM_GAME_PATH_H_
#define ARCANUM_GAME_PATH_H_

#include "game/context.h"

typedef unsigned int PathFlags;

#define PATH_FLAG_STOP_BEFORE_DESTINATION 0x0001u
#define PATH_FLAG_PORTALS_BLOCK 0x0002u
#define PATH_FLAG_WINDOWS_BLOCK 0x0004u
#define PATH_FLAG_IGNORE_TILE_BLOCKING 0x0008u
#define PATH_FLAG_IGNORE_CRITTERS 0x0010u
#define PATH_FLAG_DIRECT_ONLY 0x0020u
#define PATH_FLAG_IGNORE_BLOCKERS 0x0040u
#define PATH_FLAG_STRAIGHT_PIXEL_OFFSETS 0x0080u
#define PATH_FLAG_STRAIGHT_TILE_STEPS 0x0100u
#define PATH_FLAG_AVOID_SPOTTED_TRAPS 0x0200u
#define PATH_FLAG_AVOID_FIRE 0x0400u
#define PATH_FLAG_NEARBY_DESTINATION_SEARCH 0x0800u
#define PATH_FLAG_LONG_RANGE_FALLBACK 0x1000u

typedef struct PathCreateInfo {
    /* 0000 */ int64_t obj;
    /* 0008 */ int64_t from;
    /* 0010 */ int64_t to;
    /* 0018 */ int max_rotations;
    /* 001C */ int8_t* rotations;
    /* 0020 */ PathFlags flags;
    /* 0024 */ int range_or_partial_path_length;
} PathCreateInfo;

typedef struct WmapPathInfo {
    /* 0000 */ int64_t from;
    /* 0008 */ int64_t to;
    /* 0010 */ int max_rotations;
    /* 0014 */ int8_t* rotations;
    /* 0018 */ int partial_path_length;
} WmapPathInfo;

int path_make(PathCreateInfo* path_create_info);
unsigned int path_translate_flags(PathFlags flags);
int path_make_straight(int64_t from, int64_t to, int8_t* rotations);
int wmap_path_make(WmapPathInfo* path_info);
bool path_set_limit(int value);
bool path_set_time_limit(int value);

#endif /* ARCANUM_GAME_PATH_H_ */
