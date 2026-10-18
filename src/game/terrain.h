#ifndef ARCANUM_GAME_TERRAIN_H_
#define ARCANUM_GAME_TERRAIN_H_

#include "game/context.h"
#include "game/sector.h"

typedef enum TerrainType {
    TERRAIN_TYPE_GREEN_GRASSLANDS,
    TERRAIN_TYPE_SWAMPS,
    TERRAIN_TYPE_WATER,
    TERRAIN_TYPE_PLAINS,
    TERRAIN_TYPE_FOREST,
    TERRAIN_TYPE_DESERT_ISLAND,
    TERRAIN_TYPE_VOID_PLAINS,
    TERRAIN_TYPE_BROAD_LEAF_FOREST,
    TERRAIN_TYPE_DESERT,
    TERRAIN_TYPE_MOUNTAINS,
    TERRAIN_TYPE_ELVEN_FOREST,
    TERRAIN_TYPE_DEFORESTED,
    TERRAIN_TYPE_SNOW_PLAINS,
    TERRAIN_TYPE_TROPICAL_JUNGLE,
    TERRAIN_TYPE_VOID_MOUNTAINS,
    TERRAIN_TYPE_SCORCHED_EARTH,
    TERRAIN_TYPE_DESERT_MOUNTAINS,
    TERRAIN_TYPE_SNOWY_MOUNTAINS,
    TERRAIN_TYPE_TROPICAL_MOUNTAINS,
    TERRAIN_TYPE_COUNT,
} TerrainType;

#define TERRAIN_TYPE_IS_VALID(t) ((t) >= 0 && (t) < TERRAIN_TYPE_COUNT)

bool terrain_init(GameInitInfo* init_info);
void terrain_reset(void);
void terrain_exit(void);
bool terrain_map_new(MapNewInfo* new_map_info);
bool terrain_open(const char* base_path, const char* save_path);
void terrain_map_close(void);
void terrain_sector_path(int64_t sector_id, char* path);
int terrain_primary_type_get(uint16_t tile_data);
int terrain_secondary_type_get(uint16_t tile_data);
int terrain_transition_mask_get(uint16_t tile_data);
int terrain_variant_get(uint16_t tile_data);
bool terrain_is_blocked(int64_t sec);
void terrain_fill(Sector* sector);
bool terrain_flush(void);
uint16_t terrain_sector_tile_get(int64_t sec);

#endif /* ARCANUM_GAME_TERRAIN_H_ */
