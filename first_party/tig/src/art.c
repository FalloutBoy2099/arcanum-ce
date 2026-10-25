#include "tig/art_cache_budget.h"
#include "tig/art_decode.h"
#include "tig/art_frame.h"
#include "tig/asset_prefetch.h"
#include <limits.h>
#include "tig/art.h"
#include "tig/performance.h"

#include <limits.h>
#include <stdio.h>
#include <inttypes.h>

#include "tig/bmp.h"
#include "tig/color.h"
#include "tig/core.h"
#include "tig/debug.h"
#include "tig/file.h"
#include "tig/memory.h"
#include "tig/palette.h"
#include "tig/timer.h"
#include "tig/video.h"

typedef struct ArtInput {
    TigFile* file;
    TigAssetData data;
    size_t position;
} ArtInput;

static bool art_input_open(ArtInput* input, const char* path)
{
    if (tig_asset_prefetch_acquire(path, &input->data)) return true;
    input->file = tig_file_fopen(path, "rb");
    return input->file != NULL;
}

static void art_input_close(ArtInput* input)
{
    if (input->file != NULL) tig_file_fclose(input->file);
    tig_asset_prefetch_release(&input->data);
    input->file = NULL;
}

static int art_input_length(ArtInput* input)
{
    return input->file != NULL ? tig_file_filelength(input->file) : (int)input->data.size;
}

static size_t art_input_read(void* buffer, size_t size, size_t count, ArtInput* input)
{
    if (input->file != NULL) return tig_file_fread(buffer, size, count, input->file);
    if (size == 0 || count == 0 || count > SIZE_MAX / size) return 0;
    size_t requested = size * count;
    size_t remaining = input->position <= input->data.size ? input->data.size - input->position : 0;
    size_t bytes = requested < remaining ? requested : remaining;
    if (bytes != 0) memcpy(buffer, (const unsigned char*)input->data.bytes + input->position, bytes);
    input->position += bytes;
    return bytes / size;
}

static int art_input_seek(ArtInput* input, int offset, int origin)
{
    if (input->file != NULL) return tig_file_fseek(input->file, offset, origin);
    int64_t base = origin == SEEK_SET ? 0 : origin == SEEK_CUR ? (int64_t)input->position : (int64_t)input->data.size;
    int64_t position = base + offset;
    if (position < 0 || (uint64_t)position > input->data.size) return -1;
    input->position = (size_t)position;
    return 0;
}

#define ART_ID_TYPE_SHIFT 28
#define ART_ID_PALETTE_SHIFT 4
#define ART_ID_ROTATION_SHIFT 11

#define ART_ID_MAX_NUM 512
#define ART_ID_NUM_SHIFT 19

#define ART_ID_MAX_FRAME 32
#define ART_ID_FRAME_SHIFT 14

#define TILE_ID_MAX_NUM 64
#define TILE_ID_NUM1_SHIFT 22
#define TILE_ID_NUM2_SHIFT 16
#define TILE_ID_FLIPPABLE1_SHIFT 7
#define TILE_ID_FLIPPABLE2_SHIFT 6
#define FACADE_ID_TILE_NUM_SHIFT 11

#define TILE_ID_MAX_TYPE 2
#define TILE_ID_TYPE_SHIFT 8

#define CRITTER_ID_SHIELD_SHIFT 19
#define CRITTER_ID_ARMOR_SHIFT 20
#define CRITTER_ID_BODY_TYPE_SHIFT 24
#define CRITTER_ID_GENDER_SHIFT 27

#define PORTAL_ID_MAX_TYPE 2
#define PORTAL_ID_TYPE_SHIFT 10

#define WALL_ID_MAX_NUM 256
#define WALL_ID_NUM_SHIFT 20

#define WALL_ID_MAX_P_PIECE 64
#define WALL_ID_P_PIECE_SHIFT 14

#define WALL_ID_MAX_VARIATION 4
#define WALL_ID_VARIATION_SHIFT 8

#define SCENERY_ID_MAX_TYPE 32
#define SCENERY_ID_TYPE_SHIFT 6

#define INTERFACE_ID_MAX_NUM 4096
#define INTERFACE_ID_NUM_SHIFT 16

#define INTERFACE_ID_MAX_FRAME 256
#define INTERFACE_ID_FRAME_SHIFT 8

#define ITEM_ID_MAX_ARMOR_COVERAGE 8
#define ITEM_ID_ARMOR_COVERAGE_SHIFT 14

#define ITEM_ID_MAX_DISPOSITION 4
#define ITEM_ID_DISPOSITION_SHIFT 12

#define ITEM_ID_MAX_SUBTYPE 16
#define ITEM_ID_SUBTYPE_SHIFT 6

#define ITEM_ID_MAX_TYPE 16
#define ITEM_ID_TYPE_SHIFT 0

#define CONTAINER_ID_MAX_TYPE 32
#define CONTAINER_ID_TYPE_SHIFT 6

#define LIGHT_ID_MAX_FRAME 128
#define LIGHT_ID_FRAME_SHIFT 12

#define LIGHT_ID_ROTATION_SHIFT 9

#define ROOF_FADE_SHIFT 12
#define ROOF_FILL_SHIFT 13

// NOTE: For unknown reason facade num is split into two parts. The base number
// is stored in bits 17..24. If bit 27 is set, then base number should be added
// 256.
#define FACADE_ID_MAX_NUM 512
#define FACADE_ID_NUM_HIGH_SHIFT 27
#define FACADE_ID_NUM_LOW_SHIFT 17

#define FACADE_ID_FLIPPABLE_SHIFT 26
#define FACADE_ID_TYPE_SHIFT 25

#define FACADE_ID_MAX_FRAME 1024
#define FACADE_ID_FRAME_SHIFT 1

#define UNIQUE_NPC_ID_MAX_NUM 256
#define UNIQUE_NPC_ID_NUM_SHIFT 20

#define EYE_CANDY_ID_MAX_FRAME 128
#define EYE_CANDY_ID_FRAME_SHIFT 12

#define EYE_CANDY_ID_ROTATION_SHIFT 9
#define EYE_CANDY_ID_TYPE_SHIFT 6

#define MONSTER_ID_ARMOR_SHIFT 20
#define MONSTER_ID_SPECIE_SHIFT 23

#define MAX_PALETTES 4
#define MAX_ROTATIONS 8

// Represents memory size used in art cache.
//
// The type is signed. When used in "available" memory variables the value below
// zero denotes we've used too much memory and should evict something.
typedef int64_t art_size_t;

typedef struct TigArtFileFrameData {
    /* 0000 */ int width;
    /* 0004 */ int height;
    /* 0008 */ int data_size;
    /* 000C */ int hot_x;
    /* 0010 */ int hot_y;
    /* 0014 */ int offset_x;
    /* 0018 */ int offset_y;
} TigArtFileFrameData;

// Serializeable.
static_assert(sizeof(TigArtFileFrameData) == 0x1C, "wrong size");

typedef struct TigArtFileHeader {
    uint32_t flags;
    int32_t fps;
    int32_t bpp;
    uint32_t palette_presence[MAX_PALETTES];
    int32_t action_frame;
    int32_t num_frames;
    uint32_t frame_offsets[MAX_ROTATIONS];
    int32_t data_size[MAX_ROTATIONS];
    uint32_t pixel_offsets[MAX_ROTATIONS];
} TigArtFileHeader;

static_assert(sizeof(TigArtFileHeader) == 0x84, "wrong ART file header size");

typedef struct TigArtHeader {
    /* 0000 */ unsigned int flags;
    /* 0004 */ int fps;
    /* 0008 */ int bpp;
    /* 000C */ TigPalette* palette_tbl[MAX_PALETTES];
    /* 001C */ int action_frame;
    /* 0020 */ int num_frames;
    /* 0024 */ TigArtFileFrameData* frames_tbl[MAX_ROTATIONS];
    /* 0044 */ int data_size[MAX_ROTATIONS];
    /* 0064 */ uint8_t* pixels_tbl[MAX_ROTATIONS];
} TigArtHeader;

#define TIG_ART_CACHE_ENTRY_MODIFIED 0x02

typedef struct TigArtCacheEntry {
    /* 0000 */ unsigned int flags;
    /* 0004 */ char path[TIG_MAX_PATH];
    /* 0108 */ tig_timestamp_t time;
    /* 010C */ unsigned int art_id;
    /* 0110 */ TigArtHeader hdr;
    /* 0194 */ uint8_t dirty[MAX_PALETTES][MAX_ROTATIONS];
    /* 01B4 */ TigVideoBuffer** video_buffers[MAX_PALETTES][MAX_ROTATIONS];
    /* 0234 */ uint8_t** pixels_tbl[MAX_ROTATIONS];
    /* 0254 */ TigPalette* palette_tbl[MAX_PALETTES];
    /* 0264 */ art_size_t system_memory_usage;
    /* 0268 */ art_size_t video_memory_usage;
} TigArtCacheEntry;

static int art_get_video_buffer(int cache_entry_index, tig_art_id_t art_id, TigVideoBuffer** video_buffer_ptr);
static int tig_art_blit_flags_to_video_flags(unsigned int art_blt_flags, unsigned int* vb_blt_flags_ptr);
static int tig_art_frame_blit_software(int cache_entry_index, TigArtBlitInfo* blit_info);
static int art_blit(int cache_entry_index, TigArtBlitInfo* blit_info);
static int tig_art_cache_entry_get_or_load(tig_art_id_t art_id);
static void tig_art_cache_check_fullness(void);
static int tig_art_cache_entry_compare_time(const void* va, const void* vb);
static int tig_art_cache_entry_compare_name(const void* va, const void* vb);
static int tig_art_build_path(unsigned int art_id, char* path, size_t maxlen);
static bool tig_art_cache_find(const char* path, int* index);
static bool tig_art_cache_entry_load(tig_art_id_t art_id, const char* path, int index);
static void tig_art_cache_entry_unload(int cache_entry_index);
static void art_invalidate(int cache_entry_index);
static void tig_art_cache_video_buffers_free(int cache_entry_index);
static int tig_art_file_load(tig_art_id_t art_id, const char* filename, TigArtHeader* hdr, TigPalette** palettes, int load_selected_palette_only, art_size_t* size_ptr);
static int art_header_rotation_count(TigArtHeader* hdr);
static void art_load_failure_cleanup(ArtInput* stream, TigArtHeader* hdr, TigPalette** palette_tbl);
static void art_header_frame_data_free(TigArtHeader* hdr);
static bool art_read_header(TigArtHeader* hdr, bool* palette_presence, ArtInput* stream);

// 0x5BE880
static int tig_art_canonical_tile_transition_masks[16] = {
    0,
    1,
    8,
    3,
    4,
    5,
    6,
    7,
    8,
    3,
    10,
    11,
    6,
    7,
    14,
    15,
};

// 0x5BE8C0
static int tig_art_mirrored_tile_transition_masks[16] = {
    0,
    1,
    2,
    9,
    4,
    5,
    12,
    13,
    2,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
};

// 0x5BE900
static int tig_art_critter_body_type_properties[32] = {
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    1,
    1,
    1,
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
};

// 0x5BE980
static TigArtCritterSize critter_body_type_to_size_tbl[TIG_ART_CRITTER_BODY_TYPE_COUNT] = {
    /*     TIG_ART_CRITTER_BODY_TYPE_HUMAN */ TIG_ART_CRITTER_SIZE_MEDIUM,
    /*     TIG_ART_CRITTER_BODY_TYPE_DWARF */ TIG_ART_CRITTER_SIZE_SMALL,
    /*  TIG_ART_CRITTER_BODY_TYPE_HALFLING */ TIG_ART_CRITTER_SIZE_SMALL,
    /* TIG_ART_CRITTER_BODY_TYPE_HALF_OGRE */ TIG_ART_CRITTER_SIZE_LARGE,
    /*       TIG_ART_CRITTER_BODY_TYPE_ELF */ TIG_ART_CRITTER_SIZE_MEDIUM,
};

// 0x5BE994
static TigArtCritterSize monster_specie_to_size_tbl[TIG_ART_MONSTER_SPECIE_COUNT] = {
    /*            TIG_ART_MONSTER_SPECIE_WOLF */ TIG_ART_CRITTER_SIZE_SMALL,
    /*          TIG_ART_MONSTER_SPECIE_SPIDER */ TIG_ART_CRITTER_SIZE_SMALL,
    /*             TIG_ART_MONSTER_SPECIE_ORC */ TIG_ART_CRITTER_SIZE_MEDIUM,
    /* TIG_ART_MONSTER_SPECIE_EARTH_ELEMENTAL */ TIG_ART_CRITTER_SIZE_LARGE,
    /*    TIG_ART_MONSTER_SPECIE_LESSER_DEMON */ TIG_ART_CRITTER_SIZE_MEDIUM,
    /* TIG_ART_MONSTER_SPECIE_LESSER_SKELETON */ TIG_ART_CRITTER_SIZE_MEDIUM,
    /*   TIG_ART_MONSTER_SPECIE_LESSER_ZOMBIE */ TIG_ART_CRITTER_SIZE_MEDIUM,
    /*    TIG_ART_MONSTER_SPECIE_LESSER_MUMMY */ TIG_ART_CRITTER_SIZE_MEDIUM,
    /*   TIG_ART_MONSTER_SPECIE_GREATER_DEMON */ TIG_ART_CRITTER_SIZE_LARGE,
    /*           TIG_ART_MONSTER_SPECIE_BUNNY */ TIG_ART_CRITTER_SIZE_SMALL,
    /*     TIG_ART_MONSTER_SPECIE_MECH_SPIDER */ TIG_ART_CRITTER_SIZE_SMALL,
    /*       TIG_ART_MONSTER_SPECIE_AUTOMATON */ TIG_ART_CRITTER_SIZE_MEDIUM,
    /*      TIG_ART_MONSTER_SPECIE_LIZARD_MAN */ TIG_ART_CRITTER_SIZE_MEDIUM,
    /*  TIG_ART_MONSTER_SPECIE_PHANTOM_KNIGHT */ TIG_ART_CRITTER_SIZE_MEDIUM,
    /*         TIG_ART_MONSTER_SPECIE_WERERAT */ TIG_ART_CRITTER_SIZE_MEDIUM,
    /*       TIG_ART_MONSTER_SPECIE_SNAKE_MAN */ TIG_ART_CRITTER_SIZE_MEDIUM,
    /*             TIG_ART_MONSTER_SPECIE_APE */ TIG_ART_CRITTER_SIZE_MEDIUM,
    /*            TIG_ART_MONSTER_SPECIE_BEAR */ TIG_ART_CRITTER_SIZE_MEDIUM,
    /*         TIG_ART_MONSTER_SPECIE_CHICKEN */ TIG_ART_CRITTER_SIZE_SMALL,
    /*          TIG_ART_MONSTER_SPECIE_COUGAR */ TIG_ART_CRITTER_SIZE_SMALL,
    /*           TIG_ART_MONSTER_SPECIE_SHEEP */ TIG_ART_CRITTER_SIZE_SMALL,
    /*           TIG_ART_MONSTER_SPECIE_TIGER */ TIG_ART_CRITTER_SIZE_SMALL,
    /*             TIG_ART_MONSTER_SPECIE_PIG */ TIG_ART_CRITTER_SIZE_SMALL,
    /*             TIG_ART_MONSTER_SPECIE_COW */ TIG_ART_CRITTER_SIZE_MEDIUM,
    /*            TIG_ART_MONSTER_SPECIE_WISP */ TIG_ART_CRITTER_SIZE_MEDIUM,
    /*      TIG_ART_MONSTER_SPECIE_MUTANT_PIG */ TIG_ART_CRITTER_SIZE_SMALL,
    /*  TIG_ART_MONSTER_SPECIE_FIRE_ELEMENTAL */ TIG_ART_CRITTER_SIZE_LARGE,
    /* TIG_ART_MONSTER_SPECIE_WATER_ELEMENTAL */ TIG_ART_CRITTER_SIZE_LARGE,
    /*   TIG_ART_MONSTER_SPECIE_AIR_ELEMENTAL */ TIG_ART_CRITTER_SIZE_LARGE,
    /*    TIG_ART_MONSTER_SPECIE_DEATH_KNIGHT */ TIG_ART_CRITTER_SIZE_MEDIUM,
    /*    TIG_ART_MONSTER_SPECIE_SPIDER_QUEEN */ TIG_ART_CRITTER_SIZE_MEDIUM,
    /*        TIG_ART_MONSTER_SPECIE_WEREWOLF */ TIG_ART_CRITTER_SIZE_MEDIUM,
};

// 0x5BEA14
static bool tig_art_mirroring_enabled = true;

// 0x5BEA18
static int tig_art_wall_primary_piece_by_direction[4] = {
    6,
    1,
    3,
    2,
};

// 0x5BEA28
static int tig_art_wall_secondary_piece_by_direction[4] = {
    2,
    4,
    1,
    5,
};

// 0x5BEA38
static float tig_art_cache_video_memory_fullness = 0.3f;

// 0x604710
static TigArtFilePathResolver tig_art_file_path_resolver;

// 0x604714
static int tig_art_last_cache_entry_index;

// 0x604718
static bool tig_art_hardware_accelerated;

// 0x60471C
static int tig_art_cache_entries_length;

// 0x604720
static TigArtCacheEntry* tig_art_cache_entries;

// 0x604724
static art_size_t tig_art_total_system_memory;

// 0x60472C
static int tig_art_cache_entries_capacity;

// 0x604730
static art_size_t tig_art_available_video_memory;

// 0x604744
static TigArtBlitPaletteAdjustCallback tig_art_palette_adjust_callback;

// 0x604738
static int tig_art_bytes_per_pixel;

// 0x60473C
static bool tig_art_initialized;

// 0x604740
static art_size_t tig_art_available_system_memory;

// 0x604748
static art_size_t tig_art_total_video_memory;

// 0x60474C
static int tig_art_bits_per_pixel;

// 0x604750
static TigArtIdResetFunc tig_art_id_reset_func;

// 0x604754

// 0x500590
int tig_art_init(TigInitInfo* init_info)
{
    const TigCacheBudget* cache_budget = tig_cache_budget_get();

    if (tig_art_initialized) {
        return TIG_ERR_ALREADY_INITIALIZED;
    }

    tig_art_cache_entries_capacity = 512;
    tig_art_cache_entries_length = 0;
    tig_art_cache_entries = (TigArtCacheEntry*)MALLOC(sizeof(TigArtCacheEntry) * tig_art_cache_entries_capacity);

    tig_art_total_system_memory = (art_size_t)cache_budget->art_system_bytes;
    tig_art_available_system_memory = tig_art_total_system_memory;

    tig_art_total_video_memory = (art_size_t)cache_budget->art_video_bytes;
    tig_art_available_video_memory = tig_art_total_video_memory;
    tig_debug_printf("Art mem vid avail is %" PRId64 "\n", tig_art_available_video_memory);

    tig_art_bits_per_pixel = init_info->bpp;
    tig_art_bytes_per_pixel = tig_art_bits_per_pixel / 8;
    tig_art_file_path_resolver = init_info->art_file_path_resolver;
    tig_art_id_reset_func = init_info->art_id_reset_func;

    tig_art_initialized = true;

    tig_art_hardware_accelerated = tig_video_3d_check_initialized() == TIG_OK;

    return TIG_OK;
}

// 0x500690
void tig_art_exit(void)
{
    if (tig_art_initialized) {
        tig_art_flush();
        tig_asset_prefetch_enable(false);

        if (tig_art_cache_entries != NULL) {
            FREE(tig_art_cache_entries);
            tig_art_cache_entries = NULL;
            tig_art_cache_entries_length = 0;
            tig_art_cache_entries_capacity = 0;
        }

        tig_art_initialized = false;
    }
}

// 0x5006D0
void tig_art_ping(void)
{
    tig_asset_prefetch_poll();
}

// 0x501DD0
int tig_art_misc_id_create(unsigned int num, unsigned int palette, tig_art_id_t* art_id_ptr)
{
    if (num >= ART_ID_MAX_NUM
        || palette >= MAX_PALETTES) {
        return TIG_ERR_INVALID_PARAM;
    }

    *art_id_ptr = (TIG_ART_TYPE_MISC << ART_ID_TYPE_SHIFT)
        | ((num & (ART_ID_MAX_NUM - 1)) << ART_ID_NUM_SHIFT)
        | ((palette & (MAX_PALETTES - 1)) << ART_ID_PALETTE_SHIFT);

    return TIG_OK;
}

// 0x5021E0
void tig_art_flush(void)
{
    int index;
    tig_asset_prefetch_reset();

    if (!tig_art_initialized) {
        return;
    }

    for (index = 0; index < tig_art_cache_entries_length; index++) {
        tig_art_cache_entry_unload(index);
    }

    tig_art_cache_entries_length = 0;
}

// 0x502220
int tig_art_exists(tig_art_id_t art_id)
{
    char path[TIG_MAX_PATH];
    if (tig_art_build_path(art_id, path, sizeof(path)) != TIG_OK) {
        return TIG_ERR_GENERIC;
    }

    if (!tig_file_exists(path, NULL)) {
        return TIG_ERR_GENERIC;
    }

    return TIG_OK;
}

// 0x502290
bool tig_art_touch_prefetched(tig_art_id_t art_id)
{
    char path[TIG_MAX_PATH];
    int index;
    if (tig_art_build_path(art_id, path, sizeof(path)) != TIG_OK) return false;
    if (tig_art_cache_find(path, &index)) return true;
    TigAssetData data = { 0 };
    if (!tig_asset_prefetch_pin_ready(path, &data)) return false;
    tig_art_cache_check_fullness();
    tig_art_cache_check_fullness();
    bool loaded = tig_art_cache_find(path, &index) || tig_art_cache_entry_load(art_id, path, index);
    if (loaded) {
        tig_art_cache_entries[index].time = tig_ping_timestamp;
        tig_art_cache_entries[index].art_id = art_id;
        tig_art_last_cache_entry_index = index;
        tig_performance_count(TIG_PERF_PREFETCH_DECODED_ART, 1);
    }
    tig_asset_prefetch_release(&data);
    return loaded;
}

void tig_art_prefetch(tig_art_id_t art_id, int priority)
{
    if (!tig_asset_prefetch_is_enabled()) return;
    char path[TIG_MAX_PATH];
    int cache_index;
    if (tig_art_build_path(art_id, path, sizeof(path)) == TIG_OK && !tig_art_cache_find(path, &cache_index))
        tig_asset_prefetch_request(path, priority);
}

int tig_art_touch(tig_art_id_t art_id)
{
    int cache_entry_index;

    cache_entry_index = tig_art_cache_entry_get_or_load(art_id);
    if (cache_entry_index == -1) {
        return TIG_ERR_IO;
    }

    return TIG_OK;
}

// Sets the callback used to adjust blit palettes.
// 0x5022B0
void tig_art_palette_adjust_callback_set(TigArtBlitPaletteAdjustCallback callback)
{
    tig_art_palette_adjust_callback = callback;
}

// Returns the palette adjustment callback.
// 0x5022C0
TigArtBlitPaletteAdjustCallback tig_art_palette_adjust_callback_get(void)
{
    return tig_art_palette_adjust_callback;
}

// Rebuilds adjusted palettes for cached art and invalidates rendered cache entries.
// 0x5022D0
void tig_art_cached_palettes_refresh(void)
{
    int index;
    unsigned int palette;

    for (index = 0; index < tig_art_cache_entries_length; index++) {
        for (palette = 0; palette < MAX_PALETTES; palette++) {
            if (tig_art_cache_entries[index].hdr.palette_tbl[palette] != NULL) {
                tig_art_palette_adjust_or_copy(tig_art_cache_entries[index].art_id,
                    tig_art_cache_entries[index].hdr.palette_tbl[palette],
                    tig_art_cache_entries[index].palette_tbl[palette]);
            }
        }
        art_invalidate(index);
    }
}

// 0x502360
int tig_art_blit(TigArtBlitInfo* blit_info)
{
    TigArtBlitInfo mut_art_blit_info;
    TigVideoBuffer* video_buffer;
    TigVideoBufferData video_buffer_data;
    TigVideoBufferBlitInfo vb_blit_info;
    int rc;
    int cache_entry_index;
    unsigned int type;

    if (blit_info == NULL || blit_info->src_rect == NULL || blit_info->dst_rect == NULL
        || blit_info->dst_video_buffer == NULL) return TIG_ERR_INVALID_PARAM;
    if (blit_info->src_rect->width < 0 || blit_info->src_rect->height < 0
        || blit_info->dst_rect->width < 0 || blit_info->dst_rect->height < 0) return TIG_ERR_INVALID_PARAM;
    if (blit_info->src_rect->width == 0 || blit_info->src_rect->height == 0
        || blit_info->dst_rect->width == 0 || blit_info->dst_rect->height == 0) return TIG_OK;
    if ((blit_info->flags & TIG_ART_BLT_BLEND_COLOR_LERP) != 0
        && (blit_info->blend_colors == NULL || blit_info->blend_rect == NULL
            || blit_info->blend_rect->width <= 0 || blit_info->blend_rect->height <= 0
            || blit_info->src_rect->width != blit_info->dst_rect->width
            || blit_info->src_rect->height != blit_info->dst_rect->height)) return TIG_ERR_INVALID_PARAM;

    mut_art_blit_info = *blit_info;

    cache_entry_index = tig_art_cache_entry_get_or_load(mut_art_blit_info.art_id);
    if (cache_entry_index == -1) {
        return TIG_ERR_IO;
    }

    type = tig_art_type(mut_art_blit_info.art_id);
    if (type == TIG_ART_TYPE_TILE) {
        if (tig_art_tile_id_flippable_get(mut_art_blit_info.art_id)) {
            unsigned int flags = tig_art_id_flags_get(mut_art_blit_info.art_id);
            if ((flags & 0x1) != 0) {
                if ((mut_art_blit_info.flags & TIG_ART_BLT_FLIP_X) != 0) {
                    mut_art_blit_info.flags &= ~TIG_ART_BLT_FLIP_X;
                } else {
                    mut_art_blit_info.flags |= TIG_ART_BLT_FLIP_X;
                }
            }
            mut_art_blit_info.art_id = tig_art_id_flags_set(mut_art_blit_info.art_id, flags & ~0x1);
        }
    } else {
        if (tig_art_mirroring_enabled
            && (type == TIG_ART_TYPE_CRITTER
                || type == TIG_ART_TYPE_MONSTER
                || type == TIG_ART_TYPE_UNIQUE_NPC)) {
            int rotation = tig_art_id_rotation_get(mut_art_blit_info.art_id);
            if (rotation > 0 && rotation < 4) {
                mut_art_blit_info.art_id = tig_art_id_rotation_set(mut_art_blit_info.art_id, MAX_ROTATIONS - rotation);
                if ((mut_art_blit_info.flags & TIG_ART_BLT_FLIP_X) != 0) {
                    mut_art_blit_info.flags &= ~TIG_ART_BLT_FLIP_X;
                } else {
                    mut_art_blit_info.flags |= TIG_ART_BLT_FLIP_X;
                }
            }
        }
    }

    if (tig_art_blit_flags_to_video_flags(mut_art_blit_info.flags, &(vb_blit_info.flags)) == TIG_OK
        && tig_video_buffer_render_target_check(mut_art_blit_info.dst_video_buffer, vb_blit_info.flags) == TIG_OK
        && art_get_video_buffer(cache_entry_index, mut_art_blit_info.art_id, &video_buffer) == TIG_OK) {
        if ((mut_art_blit_info.flags & TIG_ART_BLT_BLEND_COLOR_CONST) != 0) {
            vb_blit_info.lerp_colors[0] = mut_art_blit_info.color;
        } else if ((mut_art_blit_info.flags & TIG_ART_BLT_BLEND_COLOR_LERP) != 0) {
            vb_blit_info.lerp_colors[0] = mut_art_blit_info.blend_colors[0];
            vb_blit_info.lerp_colors[1] = mut_art_blit_info.blend_colors[1];
            vb_blit_info.lerp_colors[2] = mut_art_blit_info.blend_colors[2];
            vb_blit_info.lerp_colors[3] = mut_art_blit_info.blend_colors[3];
            vb_blit_info.lerp_rect = mut_art_blit_info.blend_rect;
        } else if ((mut_art_blit_info.flags & TIG_ART_BLT_BLEND_COLOR_ARRAY) != 0) {
            vb_blit_info.lerp_colors[0] = mut_art_blit_info.blend_colors[0];
            vb_blit_info.lerp_colors[1] = mut_art_blit_info.blend_colors[1];
            vb_blit_info.lerp_colors[2] = mut_art_blit_info.blend_colors[1];
            vb_blit_info.lerp_colors[3] = mut_art_blit_info.blend_colors[0];
            vb_blit_info.lerp_rect = NULL;
        }

        if ((mut_art_blit_info.flags & TIG_ART_BLT_BLEND_ALPHA_CONST) != 0) {
            vb_blit_info.alpha[0] = mut_art_blit_info.alpha[0];
        } else if ((mut_art_blit_info.flags & TIG_ART_BLT_BLEND_ALPHA_LERP_X) != 0) {
            vb_blit_info.alpha[0] = mut_art_blit_info.alpha[0];
            vb_blit_info.alpha[1] = mut_art_blit_info.alpha[0];
            vb_blit_info.alpha[2] = mut_art_blit_info.alpha[1];
            vb_blit_info.alpha[3] = mut_art_blit_info.alpha[1];
        } else if ((mut_art_blit_info.flags & TIG_ART_BLT_BLEND_ALPHA_LERP_Y) != 0) {
            vb_blit_info.alpha[0] = mut_art_blit_info.alpha[0];
            vb_blit_info.alpha[1] = mut_art_blit_info.alpha[0];
            vb_blit_info.alpha[2] = mut_art_blit_info.alpha[3];
            vb_blit_info.alpha[3] = mut_art_blit_info.alpha[3];
        } else if ((mut_art_blit_info.flags & TIG_ART_BLT_BLEND_ALPHA_LERP_BOTH) != 0) {
            vb_blit_info.alpha[0] = mut_art_blit_info.alpha[0];
            vb_blit_info.alpha[1] = mut_art_blit_info.alpha[1];
            vb_blit_info.alpha[2] = mut_art_blit_info.alpha[2];
            vb_blit_info.alpha[3] = mut_art_blit_info.alpha[3];
        }

        vb_blit_info.src_rect = mut_art_blit_info.src_rect;
        vb_blit_info.src_video_buffer = video_buffer;
        vb_blit_info.dst_rect = mut_art_blit_info.dst_rect;
        vb_blit_info.dst_video_buffer = mut_art_blit_info.dst_video_buffer;
        return tig_video_buffer_blit(&vb_blit_info);
    }

    if ((mut_art_blit_info.flags & TIG_ART_BLT_BLEND_ANY) != 0) {
        tig_video_buffer_data(mut_art_blit_info.dst_video_buffer, &video_buffer_data);

        if ((video_buffer_data.flags & TIG_VIDEO_BUFFER_VIDEO_MEMORY) != 0
            && (mut_art_blit_info.flags & TIG_ART_BLT_SCRATCH_VALID) != 0) {
            rc = tig_video_buffer_data(mut_art_blit_info.scratch_video_buffer, &video_buffer_data);
            if (rc != TIG_OK) {
                return rc;
            }

            if ((mut_art_blit_info.flags & TIG_ART_BLT_BLEND_ALPHA_ANY) != 0) {
                if ((video_buffer_data.flags & TIG_VIDEO_BUFFER_SYSTEM_MEMORY) == 0) {
                    tig_debug_printf("Warning: using video memory surface as a scratch buffer.  This will cause slower performance.\n");
                }

                vb_blit_info.flags = 0;
                vb_blit_info.src_rect = mut_art_blit_info.dst_rect;
                vb_blit_info.dst_rect = mut_art_blit_info.dst_rect;
                vb_blit_info.src_video_buffer = mut_art_blit_info.dst_video_buffer;
                vb_blit_info.dst_video_buffer = mut_art_blit_info.scratch_video_buffer;
                rc = tig_video_buffer_blit(&vb_blit_info);
                if (rc != TIG_OK) {
                    return rc;
                }
            } else {
                tig_video_buffer_fill(mut_art_blit_info.scratch_video_buffer,
                    mut_art_blit_info.dst_rect,
                    video_buffer_data.color_key);
            }

            video_buffer = mut_art_blit_info.dst_video_buffer;
            mut_art_blit_info.dst_video_buffer = mut_art_blit_info.scratch_video_buffer;

            rc = art_blit(cache_entry_index, &mut_art_blit_info);
            if (rc != TIG_OK) {
                return rc;
            }

            mut_art_blit_info.dst_video_buffer = video_buffer;

            vb_blit_info.flags = 0;
            vb_blit_info.src_rect = mut_art_blit_info.dst_rect;
            vb_blit_info.src_video_buffer = mut_art_blit_info.scratch_video_buffer;
            vb_blit_info.dst_rect = mut_art_blit_info.dst_rect;
            vb_blit_info.dst_video_buffer = mut_art_blit_info.dst_video_buffer;
            return tig_video_buffer_blit(&vb_blit_info);
        }
    }

    return art_blit(cache_entry_index, &mut_art_blit_info);
}

// 0x502700
int tig_art_type(tig_art_id_t art_id)
{
    return art_id >> ART_ID_TYPE_SHIFT;
}

// 0x502710
unsigned int tig_art_num_get(tig_art_id_t art_id)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_TILE:
    case TIG_ART_TYPE_WALL:
    case TIG_ART_TYPE_CRITTER:
    case TIG_ART_TYPE_MONSTER:
        return 0;
    case TIG_ART_TYPE_UNIQUE_NPC:
        return (art_id >> UNIQUE_NPC_ID_NUM_SHIFT) & (UNIQUE_NPC_ID_MAX_NUM - 1);
    case TIG_ART_TYPE_ITEM:
        return (art_id >> 17) & 0x7FF;
    case TIG_ART_TYPE_INTERFACE:
        return (art_id >> INTERFACE_ID_NUM_SHIFT) & (INTERFACE_ID_MAX_NUM - 1);
    default:
        return (art_id >> ART_ID_NUM_SHIFT) & (ART_ID_MAX_NUM - 1);
    }
}

// 0x502780
tig_art_id_t tig_art_num_set(tig_art_id_t art_id, unsigned int value)
{
    unsigned int max;
    unsigned int mask;
    unsigned int shift;

    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_TILE:
    case TIG_ART_TYPE_WALL:
    case TIG_ART_TYPE_CRITTER:
    case TIG_ART_TYPE_MONSTER:
        return 0;
    case TIG_ART_TYPE_UNIQUE_NPC:
        max = UNIQUE_NPC_ID_MAX_NUM;
        mask = (UNIQUE_NPC_ID_MAX_NUM - 1) << UNIQUE_NPC_ID_NUM_SHIFT;
        shift = UNIQUE_NPC_ID_NUM_SHIFT;
        break;
    case TIG_ART_TYPE_ITEM:
        max = tig_art_item_subtype_limit_get(art_id);
        mask = 0xFFE0000;
        shift = 17;
        break;
    case TIG_ART_TYPE_INTERFACE:
        max = INTERFACE_ID_MAX_NUM;
        mask = (INTERFACE_ID_MAX_NUM - 1) << INTERFACE_ID_NUM_SHIFT;
        shift = INTERFACE_ID_NUM_SHIFT;
        break;
    default:
        max = ART_ID_MAX_NUM;
        mask = (ART_ID_MAX_NUM - 1) << ART_ID_NUM_SHIFT;
        shift = ART_ID_NUM_SHIFT;
        break;
    }

    if (value < max) {
        return (art_id & ~mask) | (value << shift);
    }

    tig_debug_println("Range exceeded in art set.");

    return art_id;
}

// 0x502830
unsigned int tig_art_item_subtype_limit_get(tig_art_id_t art_id)
{
    unsigned int max;
    int type;

    if (tig_art_type(art_id) != TIG_ART_TYPE_ITEM) {
        return 0;
    }

    max = 1000;

    type = tig_art_item_id_type_get(art_id);
    if (type == TIG_ART_ITEM_TYPE_WEAPON
        || type == TIG_ART_ITEM_TYPE_AMMO
        || (type == TIG_ART_ITEM_TYPE_ARMOR
            && tig_art_item_id_armor_coverage_get(art_id) == TIG_ART_ARMOR_COVERAGE_TORSO)) {
        max = 20;
    }

    return max;
}

// 0x502880
int tig_art_id_rotation_get(tig_art_id_t art_id)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_TILE:
    case TIG_ART_TYPE_INTERFACE:
    case TIG_ART_TYPE_MISC:
    case TIG_ART_TYPE_ROOF:
    case TIG_ART_TYPE_ITEM:
    case TIG_ART_TYPE_FACADE:
        return 0;
    case TIG_ART_TYPE_LIGHT:
        return (art_id >> LIGHT_ID_ROTATION_SHIFT) & (MAX_ROTATIONS - 1);
    case TIG_ART_TYPE_EYE_CANDY:
        return (art_id >> EYE_CANDY_ID_ROTATION_SHIFT) & (MAX_ROTATIONS - 1);
    default:
        return (art_id >> ART_ID_ROTATION_SHIFT) & (MAX_ROTATIONS - 1);
    }
}

// 0x5028E0
tig_art_id_t tig_art_id_rotation_inc(tig_art_id_t art_id)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_TILE:
    case TIG_ART_TYPE_INTERFACE:
    case TIG_ART_TYPE_MISC:
    case TIG_ART_TYPE_ROOF:
    case TIG_ART_TYPE_ITEM:
    case TIG_ART_TYPE_FACADE:
        return art_id;
    }

    return tig_art_id_rotation_set(art_id, tig_art_id_rotation_get(art_id) + 1);
}

// 0x502930
tig_art_id_t tig_art_id_rotation_set(tig_art_id_t art_id, int rotation)
{
    int type;
    unsigned int mask;
    int shift;
    int old_rotation;
    int old_cardinal_direction;
    int cardinal_direction;
    int primary_wall_piece;
    int damage_flags;
    int rotated_damage_flags;

    type = tig_art_type(art_id);

    switch (type) {
    case TIG_ART_TYPE_TILE:
    case TIG_ART_TYPE_INTERFACE:
    case TIG_ART_TYPE_ITEM:
    case TIG_ART_TYPE_MISC:
    case TIG_ART_TYPE_ROOF:
    case TIG_ART_TYPE_FACADE:
        return art_id;
    }

    if (tig_art_light_id_has_extended_rotation(art_id)) {
        return art_id;
    }

    switch (type) {
    case TIG_ART_TYPE_EYE_CANDY:
        mask = (MAX_ROTATIONS - 1) << EYE_CANDY_ID_ROTATION_SHIFT;
        shift = EYE_CANDY_ID_ROTATION_SHIFT;
        break;
    case TIG_ART_TYPE_LIGHT:
        mask = (MAX_ROTATIONS - 1) << LIGHT_ID_ROTATION_SHIFT;
        shift = LIGHT_ID_ROTATION_SHIFT;
        break;
    default:
        mask = (MAX_ROTATIONS - 1) << ART_ID_ROTATION_SHIFT;
        shift = ART_ID_ROTATION_SHIFT;
        break;
    }

    if (rotation >= MAX_ROTATIONS) {
        rotation = 0;
    } else if (rotation < 0) {
        rotation = MAX_ROTATIONS - 1;
    }

    old_rotation = tig_art_id_rotation_get(art_id);
    art_id = (art_id & ~mask) | (rotation << shift);

    if (type == TIG_ART_TYPE_WALL) {
        primary_wall_piece = tig_art_wall_id_p_piece_get(art_id);
        old_cardinal_direction = old_rotation / 2;
        cardinal_direction = rotation / 2;

        if (old_cardinal_direction != cardinal_direction) {
            if (tig_art_wall_primary_piece_by_direction[old_cardinal_direction] == primary_wall_piece) {
                primary_wall_piece = tig_art_wall_primary_piece_by_direction[cardinal_direction];
            } else if (tig_art_wall_secondary_piece_by_direction[old_cardinal_direction] == primary_wall_piece) {
                primary_wall_piece = tig_art_wall_secondary_piece_by_direction[cardinal_direction];
            } else if (tig_art_wall_primary_piece_by_direction[(old_cardinal_direction + 2) % 4] == primary_wall_piece) {
                primary_wall_piece = tig_art_wall_primary_piece_by_direction[(cardinal_direction + 2) % 4];
            } else if (tig_art_wall_secondary_piece_by_direction[(old_cardinal_direction + 2) % 4] == primary_wall_piece) {
                primary_wall_piece = tig_art_wall_secondary_piece_by_direction[(cardinal_direction + 2) % 4];
            }

            art_id = tig_art_wall_id_p_piece_set(art_id, primary_wall_piece);
            if (((old_cardinal_direction == 0 || old_cardinal_direction == 3)
                    && (cardinal_direction == 1 || cardinal_direction == 2))
                || ((old_cardinal_direction == 1 || old_cardinal_direction == 2)
                    && (cardinal_direction == 0 || cardinal_direction == 3))) {
                damage_flags = tig_art_id_damaged_get(art_id);
                rotated_damage_flags = 0;
                if ((damage_flags & 0x400) != 0) {
                    rotated_damage_flags |= 0x80;
                }
                if ((damage_flags & 0x80) != 0) {
                    rotated_damage_flags |= 0x400;
                }
                art_id = tig_art_id_damaged_set(art_id, rotated_damage_flags);
            }
        }

        if (cardinal_direction != 1 && cardinal_direction != 3) {
            art_id = tig_art_id_flags_set(art_id, 0);
            art_id = tig_art_id_palette_set(art_id, 0);
        } else {
            art_id = tig_art_id_flags_set(art_id, 1);
            art_id = tig_art_id_palette_set(art_id, 1);
        }
    } else if (type == TIG_ART_TYPE_PORTAL) {
        if (rotation / 2 != 1 && rotation / 2 != 3) {
            art_id = tig_art_id_flags_set(art_id, 0);
            art_id = tig_art_id_palette_set(art_id, 0);
        } else {
            art_id = tig_art_id_flags_set(art_id, 1);
            art_id = tig_art_id_palette_set(art_id, 1);
        }
    }

    return art_id;
}

// 0x502B50
int tig_art_id_frame_get(tig_art_id_t art_id)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_TILE:
    case TIG_ART_TYPE_WALL:
    case TIG_ART_TYPE_ITEM:
        return 0;
    case TIG_ART_TYPE_INTERFACE:
    case TIG_ART_TYPE_MISC:
        return (art_id >> INTERFACE_ID_FRAME_SHIFT) & (INTERFACE_ID_MAX_FRAME - 1);
    case TIG_ART_TYPE_FACADE:
        return (art_id >> FACADE_ID_FRAME_SHIFT) & (FACADE_ID_MAX_FRAME - 1);
    case TIG_ART_TYPE_LIGHT:
        return (art_id >> LIGHT_ID_FRAME_SHIFT) & (LIGHT_ID_MAX_FRAME - 1);
    case TIG_ART_TYPE_EYE_CANDY:
        return (art_id >> EYE_CANDY_ID_FRAME_SHIFT) & (EYE_CANDY_ID_MAX_FRAME - 1);
    default:
        return (art_id >> ART_ID_FRAME_SHIFT) & (ART_ID_MAX_FRAME - 1);
    }
}

// 0x502BC0
tig_art_id_t tig_art_id_frame_inc(tig_art_id_t art_id)
{
    int frame;

    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_TILE:
    case TIG_ART_TYPE_WALL:
    case TIG_ART_TYPE_ITEM:
        return art_id;
    }

    frame = tig_art_id_frame_get(art_id);
    return tig_art_id_frame_set(art_id, frame + 1);
}

// 0x502C00
tig_art_id_t tig_art_id_frame_dec(tig_art_id_t art_id)
{
    int frame;

    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_TILE:
    case TIG_ART_TYPE_WALL:
    case TIG_ART_TYPE_ITEM:
        return art_id;
    }

    frame = tig_art_id_frame_get(art_id);
    if (frame > 0) {
        --frame;
    }

    return tig_art_id_frame_set(art_id, frame);
}

// 0x502C40
tig_art_id_t tig_art_id_frame_set(tig_art_id_t art_id, int value)
{
    TigArtAnimData art_anim_data;
    int type;

    type = tig_art_type(art_id);

    if (type == TIG_ART_TYPE_TILE
        || type == TIG_ART_TYPE_WALL
        || type == TIG_ART_TYPE_ITEM) {
        return art_id;
    }

    if (value > 0) {
        if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
            return art_id;
        }

        if (value >= art_anim_data.num_frames) {
            value = 0;
        }
    } else if (value < 0) {
        if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
            return art_id;
        }

        value = art_anim_data.num_frames - 1;
    }

    switch (type) {
    case TIG_ART_TYPE_INTERFACE:
    case TIG_ART_TYPE_MISC:
        return (art_id & ~((INTERFACE_ID_MAX_FRAME - 1) << INTERFACE_ID_FRAME_SHIFT))
            | (value << INTERFACE_ID_FRAME_SHIFT);
    case TIG_ART_TYPE_FACADE:
        return (art_id & ~((FACADE_ID_MAX_FRAME - 1) << FACADE_ID_FRAME_SHIFT))
            | (value << FACADE_ID_FRAME_SHIFT);
    case TIG_ART_TYPE_LIGHT:
        return (art_id & ~((LIGHT_ID_MAX_FRAME - 1) << LIGHT_ID_FRAME_SHIFT))
            | (value << LIGHT_ID_FRAME_SHIFT);
    case TIG_ART_TYPE_EYE_CANDY:
        return (art_id & ~((EYE_CANDY_ID_MAX_FRAME - 1) << EYE_CANDY_ID_FRAME_SHIFT))
            | (value << EYE_CANDY_ID_FRAME_SHIFT);
    default:
        return (art_id & ~((ART_ID_MAX_FRAME - 1) << ART_ID_FRAME_SHIFT))
            | (value << ART_ID_FRAME_SHIFT);
    }
}

// 0x502D30
tig_art_id_t tig_art_id_flags_set(tig_art_id_t art_id, int value)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_TILE:
    case TIG_ART_TYPE_WALL:
    case TIG_ART_TYPE_PORTAL:
    case TIG_ART_TYPE_ROOF:
        if (value >= 16) {
            tig_debug_println("Range exceeded in art set.");
            value = 0;
        }
        return (art_id & ~0xF) | value;
    default:
        return art_id;
    }
}

// 0x502D80
int tig_art_id_palette_get(tig_art_id_t art_id)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_LIGHT:
    case TIG_ART_TYPE_FACADE:
        return 0;
    default:
        return (art_id >> ART_ID_PALETTE_SHIFT) & (MAX_PALETTES - 1);
    }
}

// 0x502DB0
tig_art_id_t tig_art_id_palette_set(tig_art_id_t art_id, int value)
{
    if (value >= MAX_PALETTES) {
        tig_debug_println("Range exceeded in art set.");
        value = 0;
    }

    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_LIGHT:
    case TIG_ART_TYPE_FACADE:
        return art_id;
    default:
        return (art_id & ~((MAX_PALETTES - 1) << ART_ID_PALETTE_SHIFT))
            | (value << ART_ID_PALETTE_SHIFT);
    }
}

// Returns a TIG status indicating whether the requested original palette exists.
// 0x502E00
int tig_art_palette_check(tig_art_id_t art_id)
{
    int cache_entry_index;
    int palette;

    cache_entry_index = tig_art_cache_entry_get_or_load(art_id);
    if (cache_entry_index == -1) {
        return TIG_ERR_IO;
    }

    palette = tig_art_id_palette_get(art_id);
    if (tig_art_cache_entries[cache_entry_index].hdr.palette_tbl[palette] == 0) {
        return TIG_ERR_GENERIC;
    }

    return TIG_OK;
}

// Reads the original palette color at a frame pixel, accounting for mirroring.
// 0x502E50
int tig_art_pixel_color_get(tig_art_id_t art_id, int x, int y, unsigned int* color_ptr)
{
    TigArtCacheEntry* cache_entry;
    int cache_entry_index;
    int rotation;
    int frame;
    int type;
    int palette;
    uint8_t byte;

    cache_entry_index = tig_art_cache_entry_get_or_load(art_id);
    if (cache_entry_index == -1) {
        return TIG_ERR_IO;
    }

    cache_entry = &(tig_art_cache_entries[cache_entry_index]);

    rotation = tig_art_id_rotation_get(art_id);
    frame = tig_art_id_frame_get(art_id);

    if (tig_art_mirroring_enabled) {
        type = tig_art_type(art_id);
        if ((type == TIG_ART_TYPE_CRITTER
                || type == TIG_ART_TYPE_MONSTER
                || type == TIG_ART_TYPE_UNIQUE_NPC)
            && rotation > 0
            && rotation < 4) {
            rotation = MAX_ROTATIONS - rotation;
            x = cache_entry->hdr.frames_tbl[rotation][frame].width - x - 1;
        }
    }

    byte = cache_entry->pixels_tbl[rotation][frame][y * cache_entry->hdr.frames_tbl[rotation][frame].width + x];

    palette = tig_art_id_palette_get(art_id);
    if (cache_entry->hdr.palette_tbl[palette] == NULL) {
        palette = 0;
    }

    *color_ptr = cache_entry->hdr.palette_tbl[palette]->colors[byte];

    return TIG_OK;
}

// Returns TIG_OK for pixel indices at least two; lower indices fail the hit test.
// 0x502FD0
int tig_art_pixel_hit_test(tig_art_id_t art_id, int x, int y)
{
    int cache_entry_index;
    int rotation;
    int frame;
    int type;
    int index;

    cache_entry_index = tig_art_cache_entry_get_or_load(art_id);
    if (cache_entry_index == -1) {
        return TIG_ERR_IO;
    }

    rotation = tig_art_id_rotation_get(art_id);
    frame = tig_art_id_frame_get(art_id);

    if (tig_art_mirroring_enabled) {
        type = tig_art_type(art_id);
        if ((type == TIG_ART_TYPE_CRITTER
                || type == TIG_ART_TYPE_MONSTER
                || type == TIG_ART_TYPE_UNIQUE_NPC)
            && rotation > 0
            && rotation < 4) {
            rotation = MAX_ROTATIONS - rotation;
            x = tig_art_cache_entries[cache_entry_index].hdr.frames_tbl[rotation][frame].width - x - 1;
        }
    }

    index = y * tig_art_cache_entries[cache_entry_index].hdr.frames_tbl[rotation][frame].width + x;
    if (tig_art_cache_entries[cache_entry_index].pixels_tbl[rotation][frame][index] < 2) {
        return TIG_ERR_GENERIC;
    }

    return TIG_OK;
}

// 0x5030B0
int tig_art_anim_data(tig_art_id_t art_id, TigArtAnimData* data)
{
    int cache_entry_index;
    TigArtCacheEntry* cache_entry;
    int palette;

    cache_entry_index = tig_art_cache_entry_get_or_load(art_id);
    if (cache_entry_index == -1) {
        return TIG_ERR_GENERIC;
    }

    cache_entry = &(tig_art_cache_entries[cache_entry_index]);

    data->flags = cache_entry->hdr.flags;
    data->fps = cache_entry->hdr.fps;
    data->bpp = tig_art_bits_per_pixel;
    data->num_frames = cache_entry->hdr.num_frames;
    data->action_frame = cache_entry->hdr.action_frame;

    palette = tig_art_id_palette_get(art_id);

    data->palette1 = cache_entry->palette_tbl[palette];
    if (data->palette1 == NULL) {
        data->palette1 = cache_entry->palette_tbl[0];
    }

    data->palette2 = cache_entry->hdr.palette_tbl[palette];
    if (data->palette2 == 0) {
        data->palette2 = cache_entry->hdr.palette_tbl[0];
    }

    data->color_key = data->palette1->colors[0];

    return TIG_OK;
}

// 0x5031C0
int tig_art_frame_data(tig_art_id_t art_id, TigArtFrameData* data)
{
    int cache_entry_index;
    TigArtCacheEntry* cache_entry;
    int rotation;
    int frame;
    bool mirrored;
    int type;

    cache_entry_index = tig_art_cache_entry_get_or_load(art_id);
    if (cache_entry_index == -1) {
        return TIG_ERR_GENERIC;
    }

    rotation = tig_art_id_rotation_get(art_id);
    frame = tig_art_id_frame_get(art_id);
    type = tig_art_type(art_id);

    mirrored = false;
    if (tig_art_mirroring_enabled
        && (type == TIG_ART_TYPE_CRITTER
            || type == TIG_ART_TYPE_MONSTER
            || type == TIG_ART_TYPE_UNIQUE_NPC)
        && rotation > 0
        && rotation < 4) {
        mirrored = true;
        rotation = MAX_ROTATIONS - rotation;
    }

    cache_entry = &(tig_art_cache_entries[cache_entry_index]);

    data->width = cache_entry->hdr.frames_tbl[rotation][frame].width;
    data->height = cache_entry->hdr.frames_tbl[rotation][frame].height;
    data->hot_x = cache_entry->hdr.frames_tbl[rotation][frame].hot_x;
    data->hot_y = cache_entry->hdr.frames_tbl[rotation][frame].hot_y;
    data->offset_x = cache_entry->hdr.frames_tbl[rotation][frame].offset_x;
    data->offset_y = cache_entry->hdr.frames_tbl[rotation][frame].offset_y;

    if ((type == TIG_ART_TYPE_WALL || type == TIG_ART_TYPE_PORTAL)
        && (rotation < 2 || rotation > 5)) {
        data->hot_x -= 40;
        data->hot_y += 20;
    }

    if (mirrored) {
        data->hot_x = data->width - data->hot_x - 1;
        data->offset_x = -data->offset_x;
    }

    if ((tig_art_id_flags_get(art_id) & 0x1) != 0) {
        if (type == TIG_ART_TYPE_ROOF) {
            data->hot_x = 0;
            data->offset_x = 0;
        } else {
            data->hot_x = cache_entry->hdr.frames_tbl[rotation][frame].width - data->hot_x - 2;
            data->offset_x = -data->offset_x;
        }
    }

    return TIG_OK;
}

// 0x503340
int tig_art_frame_pixels_copy_legacy(tig_art_id_t art_id, uint8_t* dst, int pitch)
{
    int cache_entry_index;
    int rotation;
    int frame_index;
    uint8_t* src;
    int width;
    int height;

    cache_entry_index = tig_art_cache_entry_get_or_load(art_id);
    if (cache_entry_index == -1) {
        return TIG_ERR_IO;
    }

    rotation = tig_art_id_rotation_get(art_id);
    frame_index = tig_art_id_frame_get(art_id);
    src = tig_art_cache_entries[cache_entry_index].pixels_tbl[rotation][frame_index];
    width = tig_art_cache_entries[cache_entry_index].hdr.frames_tbl[rotation][frame_index].width;
    height = tig_art_cache_entries[cache_entry_index].hdr.frames_tbl[rotation][frame_index].height;

    while (height > 0) {
        memcpy(dst, src, width);
        dst += pitch;
        height--;
    }

    return TIG_OK;
}

// 0x5033E0
int tig_art_frames_for_displacement_get(tig_art_id_t art_id, int distance_x, int distance_y)
{
    int cache_entry_index;
    int rotation;
    int frame;
    int type;
    int delta;
    int frame_count;
    int accumulated_x;
    int accumulated_y;
    int frame_offset_x;
    int frame_offset_y;

    cache_entry_index = tig_art_cache_entry_get_or_load(art_id);
    if (cache_entry_index == -1) {
        return 0;
    }

    rotation = tig_art_id_rotation_get(art_id);
    frame = tig_art_id_frame_get(art_id);

    // NOTE: Initialize to keep compiler happy.
    delta = 0;

    if (tig_art_mirroring_enabled) {
        delta = 1;

        type = tig_art_type(art_id);
        if ((type == TIG_ART_TYPE_CRITTER
                || type == TIG_ART_TYPE_MONSTER
                || type == TIG_ART_TYPE_UNIQUE_NPC)
            && rotation > 0
            && rotation < 4) {
            delta = -1;
            rotation = MAX_ROTATIONS - rotation;
        }
    }

    frame_count = 0;

    if (distance_x < 0) {
        distance_x = -distance_x;
    }

    if (distance_y < 0) {
        distance_y = -distance_y;
    }

    accumulated_x = 0;
    accumulated_y = 0;
    while ((accumulated_x < distance_x || accumulated_y < distance_y) && frame_count < 100) {
        frame++;
        if (frame == tig_art_cache_entries[cache_entry_index].hdr.num_frames) {
            frame = 0;
        }

        frame_offset_x = tig_art_cache_entries[cache_entry_index].hdr.frames_tbl[rotation][frame].offset_x;
        frame_offset_y = tig_art_cache_entries[cache_entry_index].hdr.frames_tbl[rotation][frame].offset_y;

        if (tig_art_mirroring_enabled) {
            frame_offset_x *= delta;
            frame_offset_y *= delta;
        }

        if (frame_offset_x >= 0) {
            accumulated_x += frame_offset_x;
        } else {
            accumulated_x -= frame_offset_x;
        }

        if (frame_offset_y >= 0) {
            accumulated_y += frame_offset_y;
        } else {
            accumulated_y -= frame_offset_y;
        }

        frame_count++;
    }

    return frame_count;
}

// 0x503510
int tig_art_size(tig_art_id_t art_id, int* width_ptr, int* height_ptr)
{
    TigArtCacheEntry* art;
    TigArtFileFrameData* frm;
    int cache_entry_index;
    int type;
    int rotation_offset;
    int num_rotations;
    int rotation;
    int frame;
    int width = 0;
    int height = 0;

    cache_entry_index = tig_art_cache_entry_get_or_load(art_id);
    if (cache_entry_index == -1) {
        return TIG_ERR_GENERIC;
    }

    art = &(tig_art_cache_entries[cache_entry_index]);
    type = tig_art_type(art_id);

    if ((art->hdr.flags & TIG_ART_SINGLE_ROTATION) != 0) {
        rotation_offset = 0;
        num_rotations = 1;
    } else if (tig_art_mirroring_enabled
        && (type == TIG_ART_TYPE_CRITTER
            || type == TIG_ART_TYPE_MONSTER
            || type == TIG_ART_TYPE_UNIQUE_NPC)) {
        rotation_offset = 4;
        num_rotations = 5;
    } else {
        rotation_offset = 0;
        num_rotations = MAX_ROTATIONS;
    }

    for (frame = 0; frame < art->hdr.num_frames; ++frame) {
        for (rotation = 0; rotation < num_rotations; ++rotation) {
            frm = &(art->hdr.frames_tbl[(rotation + rotation_offset) % MAX_ROTATIONS][frame]);
            if (width < frm->width) {
                width = frm->width;
            }
            if (height < frm->height) {
                height = frm->height;
            }
        }
    }

    *width_ptr = width;
    *height_ptr = height;

    return TIG_OK;
}

// 0x503600
int tig_art_tile_id_create(unsigned int num1, unsigned int num2, unsigned int transition_mask, unsigned int variant, unsigned int type, unsigned int flippable1, unsigned int flippable2, unsigned int palette, tig_art_id_t* art_id_ptr)
{
    if (num1 >= TILE_ID_MAX_NUM
        || num2 >= TILE_ID_MAX_NUM
        || transition_mask >= 0x10
        || variant >= 0x10
        || type >= TILE_ID_MAX_TYPE
        || flippable1 >= 2
        || flippable2 >= 2
        || palette >= 4) {
        return TIG_ERR_INVALID_PARAM;
    }

    *art_id_ptr = (TIG_ART_TYPE_TILE << ART_ID_TYPE_SHIFT)
        | ((num1 & (TILE_ID_MAX_NUM - 1)) << TILE_ID_NUM1_SHIFT)
        | ((num2 & (TILE_ID_MAX_NUM - 1)) << TILE_ID_NUM2_SHIFT)
        | ((type & (TILE_ID_MAX_TYPE - 1)) << TILE_ID_TYPE_SHIFT)
        | ((flippable1 & 1) << TILE_ID_FLIPPABLE1_SHIFT)
        | ((flippable2 & 1) << TILE_ID_FLIPPABLE2_SHIFT)
        | ((palette & 3) << 4);

    *art_id_ptr = tig_art_tile_id_transition_mask_set(*art_id_ptr, transition_mask);
    *art_id_ptr = tig_art_tile_id_variant_set(*art_id_ptr, variant);

    return TIG_OK;
}

// 0x5036B0
int tig_art_tile_id_num1_get(tig_art_id_t art_id)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_TILE:
        return (art_id >> TILE_ID_NUM1_SHIFT) & (TILE_ID_MAX_NUM - 1);
    case TIG_ART_TYPE_FACADE:
        return (art_id >> FACADE_ID_TILE_NUM_SHIFT) & (TILE_ID_MAX_NUM - 1);
    default:
        return 0;
    }
}

// 0x5036E0
int tig_art_tile_id_num2_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_TILE) {
        return 0;
    }

    return (art_id >> TILE_ID_NUM2_SHIFT) & (TILE_ID_MAX_NUM - 1);
}

// 0x503700
int tig_art_tile_id_transition_mask_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_TILE) {
        return 0;
    }

    int transition_mask = (art_id >> 12) & 0xF;
    if ((tig_art_id_flags_get(art_id) & 1) != 0) {
        transition_mask = tig_art_mirrored_tile_transition_masks[transition_mask];
    }
    return transition_mask;
}

// 0x503740
tig_art_id_t tig_art_tile_id_transition_mask_set(tig_art_id_t art_id, int value)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_TILE) {
        return art_id;
    }

    if (value >= 16) {
        tig_debug_println("Range exceeded in art set.");
        value = 0;
    }

    if (value != tig_art_canonical_tile_transition_masks[value]) {
        if (tig_art_tile_id_flippable_get(art_id)) {
            art_id = tig_art_id_flags_set(art_id, tig_art_id_flags_get(art_id) | 1);
            value = tig_art_canonical_tile_transition_masks[value];
        }
    }

    art_id = (art_id & ~0xF000) | (value << 12);

    return art_id;
}

// Reads the tile variant including the mirrored variant bank when applicable.
// 0x5037B0
int tig_art_tile_id_variant_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_TILE) {
        return 0;
    }

    int variant = (art_id >> 9) & 7;
    int transition_mask = tig_art_tile_id_transition_mask_get(art_id);
    if (tig_art_mirrored_tile_transition_masks[transition_mask] == tig_art_canonical_tile_transition_masks[transition_mask]) {
        if ((tig_art_id_flags_get(art_id) & 1) != 0) {
            variant += 8;
        }
    }
    return variant;
}

// Writes the tile variant and its associated mirroring flag.
// 0x503800
tig_art_id_t tig_art_tile_id_variant_set(tig_art_id_t art_id, int value)
{
    int transition_mask;

    if (tig_art_type(art_id) != TIG_ART_TYPE_TILE) {
        return art_id;
    }

    if (value >= 16) {
        tig_debug_println("Range exceeded in art set.");
        value = 0;
    }

    transition_mask = tig_art_tile_id_transition_mask_get(art_id);
    if (value < 8) {
        if (tig_art_mirrored_tile_transition_masks[transition_mask] == tig_art_canonical_tile_transition_masks[transition_mask]) {
            art_id = tig_art_id_flags_set(art_id, tig_art_id_flags_get(art_id) & ~1);
        }
        art_id = (art_id & ~0xE00) | (value << 9);
    } else {
        if (tig_art_mirrored_tile_transition_masks[transition_mask] == tig_art_canonical_tile_transition_masks[transition_mask]) {
            if (tig_art_tile_id_flippable_get(art_id)) {
                art_id = tig_art_id_flags_set(art_id, tig_art_id_flags_get(art_id) | 1);
            }
        }
        art_id = (art_id & ~0xE00) | ((value - 8) << 9);
    }

    return art_id;
}

// 0x5038C0
int tig_art_tile_id_type_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) == TIG_ART_TYPE_TILE) {
        return (art_id >> TILE_ID_TYPE_SHIFT) & (TILE_ID_MAX_TYPE - 1);
    }

    if (tig_art_type(art_id) == TIG_ART_TYPE_FACADE) {
        return (art_id >> FACADE_ID_TYPE_SHIFT) & (TILE_ID_MAX_TYPE - 1);
    }

    return 0;
}

// 0x503900
int tig_art_tile_id_flippable_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) == TIG_ART_TYPE_FACADE) {
        return (art_id >> FACADE_ID_FLIPPABLE_SHIFT) & 1;
    }

    if (tig_art_type(art_id) == TIG_ART_TYPE_TILE) {
        return tig_art_tile_id_flippable1_get(art_id) != 0
            && tig_art_tile_id_flippable2_get(art_id) != 0;
    }

    return 0;
}

// 0x503950
int tig_art_tile_id_flippable1_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) == TIG_ART_TYPE_TILE) {
        return (art_id >> TILE_ID_FLIPPABLE1_SHIFT) & 1;
    }

    if (tig_art_type(art_id) == TIG_ART_TYPE_FACADE) {
        return tig_art_tile_id_flippable_get(art_id);
    }

    return 0;
}

// 0x503990
int tig_art_tile_id_flippable2_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) == TIG_ART_TYPE_TILE) {
        return (art_id >> TILE_ID_FLIPPABLE2_SHIFT) & 1;
    }

    if (tig_art_type(art_id) == TIG_ART_TYPE_FACADE) {
        return tig_art_tile_id_flippable_get(art_id);
    }

    return 0;
}

// 0x5039D0
int tig_art_wall_id_create(unsigned int num, int p_piece, int variation, int rotation, unsigned int palette, int damaged, tig_art_id_t* art_id_ptr)
{
    if (num >= WALL_ID_MAX_NUM
        || p_piece >= 46 // FIXME: Likely error, should be WALL_ID_MAX_P_PIECE(64).
        || variation >= WALL_ID_MAX_VARIATION
        || rotation >= MAX_ROTATIONS
        || palette >= MAX_PALETTES
        || (damaged & ~0x480) != 0) {
        return TIG_ERR_INVALID_PARAM;
    }

    *art_id_ptr = (TIG_ART_TYPE_WALL << ART_ID_TYPE_SHIFT)
        | ((num & (WALL_ID_MAX_NUM - 1)) << WALL_ID_NUM_SHIFT)
        | ((p_piece & (WALL_ID_MAX_P_PIECE - 1)) << WALL_ID_P_PIECE_SHIFT)
        | ((rotation & (MAX_ROTATIONS - 1)) << ART_ID_ROTATION_SHIFT)
        | ((variation & (WALL_ID_MAX_VARIATION - 1)) << WALL_ID_VARIATION_SHIFT)
        | ((palette & (MAX_PALETTES - 1)) << ART_ID_PALETTE_SHIFT)
        | damaged;

    *art_id_ptr = tig_art_id_rotation_set(*art_id_ptr, rotation);

    return TIG_OK;
}

// 0x503A60
int tig_art_wall_id_p_piece_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_WALL) {
        return 0;
    }

    return (art_id >> 14) & 0x3F;
}

// 0x503A90
tig_art_id_t tig_art_wall_id_p_piece_set(tig_art_id_t art_id, int value)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_WALL) {
        return art_id;
    }

    if (value >= WALL_ID_MAX_P_PIECE) {
        tig_debug_println("Range exceeded in art set.");
        value = 0;
    }

    return (art_id & ~((WALL_ID_MAX_P_PIECE - 1) << WALL_ID_P_PIECE_SHIFT))
        | (value << WALL_ID_P_PIECE_SHIFT);
}

// 0x503AD0
int tig_art_wall_id_num_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_WALL) {
        return 0;
    }

    return (art_id >> WALL_ID_NUM_SHIFT) & (WALL_ID_MAX_NUM - 1);
}

// 0x503B00
int tig_art_wall_id_variation_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_WALL) {
        return 0;
    }

    return (art_id >> WALL_ID_VARIATION_SHIFT) & (WALL_ID_MAX_VARIATION - 1);
}

// 0x503B30
tig_art_id_t tig_art_wall_id_variation_set(tig_art_id_t art_id, int value)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_WALL) {
        return art_id;
    }

    if (value >= 5) {
        tig_debug_println("Range exceeded in art set.");
        return art_id;
    }

    return (art_id & ~((WALL_ID_MAX_VARIATION - 1) << WALL_ID_VARIATION_SHIFT))
        | (value << WALL_ID_VARIATION_SHIFT);
}

// 0x503B70
int tig_art_id_damaged_get(tig_art_id_t art_id)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_WALL:
        return art_id & 0x480;
    case TIG_ART_TYPE_PORTAL:
        return art_id & 0x200;
    case TIG_ART_TYPE_ITEM:
        return art_id & 0x800;
    default:
        return 0;
    }
}

// 0x503BB0
tig_art_id_t tig_art_id_damaged_set(tig_art_id_t art_id, int value)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_WALL:
        return (art_id & ~0x480) | value;
    case TIG_ART_TYPE_PORTAL:
        return (art_id & ~0x200) | value;
    case TIG_ART_TYPE_ITEM:
        return (art_id & ~0x800) | value;
    default:
        return art_id;
    }
}

// 0x503C00
int tig_art_critter_id_create(unsigned int gender, int body_type, int armor, unsigned int shield, unsigned int frame, int rotation, int anim, int weapon, unsigned int palette, tig_art_id_t* art_id_ptr)
{
    if (gender >= 2
        || body_type >= 8
        || armor >= 16
        || shield >= 2
        || frame >= 0x20
        || rotation >= MAX_ROTATIONS
        || anim >= 32
        || weapon >= 16
        || palette >= MAX_PALETTES) {
        return TIG_ERR_INVALID_PARAM;
    }

    *art_id_ptr = (TIG_ART_TYPE_CRITTER << ART_ID_TYPE_SHIFT)
        | ((gender & 1) << CRITTER_ID_GENDER_SHIFT)
        | ((body_type & 7) << CRITTER_ID_BODY_TYPE_SHIFT)
        | ((armor & 0xF) << CRITTER_ID_ARMOR_SHIFT)
        | ((shield & 1) << CRITTER_ID_SHIELD_SHIFT)
        | ((frame & 0x1F) << ART_ID_FRAME_SHIFT)
        | ((rotation & (MAX_ROTATIONS - 1)) << ART_ID_ROTATION_SHIFT)
        | ((anim & 0x1F) << 6)
        | ((palette & (MAX_PALETTES - 1)) << ART_ID_PALETTE_SHIFT)
        | (weapon & 0xF);

    return TIG_OK;
}

// 0x503CD0
int tig_art_monster_id_create(int specie, int armor, unsigned int shield, unsigned int frame, int rotation, int anim, int weapon, unsigned int palette, tig_art_id_t* art_id_ptr)
{
    if (specie >= TIG_ART_MONSTER_SPECIE_COUNT
        || armor >= 8
        || shield >= 2
        || frame >= 0x20
        || rotation >= MAX_ROTATIONS
        || anim >= 32
        || weapon >= 16
        || palette >= MAX_PALETTES) {
        return TIG_ERR_INVALID_PARAM;
    }

    *art_id_ptr = (TIG_ART_TYPE_MONSTER << ART_ID_TYPE_SHIFT)
        | ((specie & (TIG_ART_MONSTER_SPECIE_COUNT - 1)) << MONSTER_ID_SPECIE_SHIFT)
        | ((armor & 7) << MONSTER_ID_ARMOR_SHIFT)
        | ((shield & 1) << CRITTER_ID_SHIELD_SHIFT)
        | ((frame & 0x1F) << ART_ID_FRAME_SHIFT)
        | ((rotation & (MAX_ROTATIONS - 1)) << ART_ID_ROTATION_SHIFT)
        | ((anim & 0x1F) << 6)
        | ((palette & (MAX_PALETTES - 1)) << ART_ID_PALETTE_SHIFT)
        | (weapon & 0xF);

    return TIG_OK;
}

// 0x503D80
int tig_art_unique_npc_id_create(int num, unsigned int shield, unsigned int frame, int rotation, int anim, int weapon, unsigned int palette, tig_art_id_t* art_id_ptr)
{
    if (num >= UNIQUE_NPC_ID_MAX_NUM
        || shield >= 2
        || frame >= ART_ID_MAX_FRAME
        || rotation >= MAX_ROTATIONS
        || anim >= 32
        || weapon >= 16
        || palette >= MAX_PALETTES) {
        return TIG_ERR_INVALID_PARAM;
    }

    *art_id_ptr = (TIG_ART_TYPE_UNIQUE_NPC << ART_ID_TYPE_SHIFT)
        | ((num & (UNIQUE_NPC_ID_MAX_NUM - 1)) << UNIQUE_NPC_ID_NUM_SHIFT)
        | ((shield & 1) << CRITTER_ID_SHIELD_SHIFT)
        | ((frame & (ART_ID_MAX_FRAME - 1)) << ART_ID_FRAME_SHIFT)
        | ((rotation & (MAX_ROTATIONS - 1)) << ART_ID_ROTATION_SHIFT)
        | ((anim & 0x1F) << 6)
        | ((palette & (MAX_PALETTES - 1)) << ART_ID_PALETTE_SHIFT)
        | (weapon & 0xF);

    return TIG_OK;
}

// 0x503E20
int tig_art_id_anim_get(tig_art_id_t art_id)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_CRITTER:
    case TIG_ART_TYPE_MONSTER:
    case TIG_ART_TYPE_UNIQUE_NPC:
        return (art_id >> 6) & 0x1F;
    default:
        return 0;
    }
}

// 0x503E50
tig_art_id_t tig_art_id_anim_set(tig_art_id_t art_id, int value)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_CRITTER:
    case TIG_ART_TYPE_MONSTER:
    case TIG_ART_TYPE_UNIQUE_NPC:
        if (value >= 26) {
            tig_debug_println("Range exceeded in art set.");
            value = 0;
        }
        return (art_id & ~0x7C0) | (value << 6);
    default:
        return art_id;
    }
}

// 0x503EA0
int tig_art_critter_id_body_type_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) == TIG_ART_TYPE_CRITTER) {
        return (art_id >> CRITTER_ID_BODY_TYPE_SHIFT) & 7;
    }

    return 0;
}

// 0x503ED0
tig_art_id_t tig_art_critter_id_body_type_set(tig_art_id_t art_id, int value)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_CRITTER:
    case TIG_ART_TYPE_MONSTER:
    case TIG_ART_TYPE_UNIQUE_NPC:
        if (value >= TIG_ART_CRITTER_BODY_TYPE_COUNT) {
            tig_debug_println("Range exceeded in art set.");
            value = 0;
        }
        return (art_id & ~0x7000000) | (value << CRITTER_ID_BODY_TYPE_SHIFT);
    default:
        return art_id;
    }
}

// 0x503F20
int tig_art_monster_id_specie_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) == TIG_ART_TYPE_MONSTER) {
        return (art_id >> MONSTER_ID_SPECIE_SHIFT) & (TIG_ART_MONSTER_SPECIE_COUNT - 1);
    }

    return 0;
}

// 0x503F50
int tig_art_critter_body_type_property_get(int body_type)
{
    return tig_art_critter_body_type_properties[body_type];
}

// 0x503F60
int tig_art_critter_id_size_get(tig_art_id_t art_id)
{
    int idx;

    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_CRITTER:
        // FIXME: This is wrong, art number for critters is always 0, probably
        // should call `tig_art_critter_id_body_type_get` to obtain correct
        // index into `critter_body_type_to_size_tbl` lookup table. Need some
        // tests to be sure combat animation won't break.
        idx = tig_art_num_get(art_id);
        return critter_body_type_to_size_tbl[idx];
    case TIG_ART_TYPE_MONSTER:
        idx = tig_art_monster_id_specie_get(art_id);
        return monster_specie_to_size_tbl[idx];
    default:
        return TIG_ART_CRITTER_SIZE_MEDIUM;
    }
}

// 0x503FB0
int tig_art_critter_id_gender_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) == TIG_ART_TYPE_CRITTER) {
        return (art_id >> CRITTER_ID_GENDER_SHIFT) & 1;
    } else {
        return 0;
    }
}

// 0x503FE0
tig_art_id_t tig_art_critter_id_gender_set(tig_art_id_t art_id, int value)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_CRITTER:
    case TIG_ART_TYPE_MONSTER:
    case TIG_ART_TYPE_UNIQUE_NPC:
        if (value >= TIG_ART_CRITTER_GENDER_COUNT) {
            tig_debug_println("Range exceeded in art set.");
            value = 0;
        }
        return (art_id & ~0x8000000) | (value << CRITTER_ID_GENDER_SHIFT);
    default:
        return art_id;
    }
}

// 0x504030
int tig_art_critter_id_armor_get(tig_art_id_t art_id)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_CRITTER:
        return (art_id >> CRITTER_ID_ARMOR_SHIFT) & 0xF;
    case TIG_ART_TYPE_MONSTER:
        return (art_id >> MONSTER_ID_ARMOR_SHIFT) & 7;
    default:
        return 0;
    }
}

// 0x504060
tig_art_id_t tig_art_critter_id_armor_set(tig_art_id_t art_id, int value)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_CRITTER:
        if (value >= TIG_ART_ARMOR_TYPE_COUNT) {
            tig_debug_println("Range exceeded in art set.");
            value = 0;
        }
        return (art_id & ~0xF00000) | (value << CRITTER_ID_ARMOR_SHIFT);
    case TIG_ART_TYPE_MONSTER:
        if (value >= 8) {
            tig_debug_println("Range exceeded in art set.");
            value = 0;
        }
        return (art_id & ~0x700000) | (value << MONSTER_ID_ARMOR_SHIFT);
    default:
        return art_id;
    }
}

// 0x5040D0
int tig_art_critter_id_weapon_get(tig_art_id_t art_id)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_CRITTER:
    case TIG_ART_TYPE_MONSTER:
    case TIG_ART_TYPE_UNIQUE_NPC:
        return art_id & 0xF;
    default:
        return 0;
    }
}

// 0x504100
tig_art_id_t tig_art_critter_id_weapon_set(tig_art_id_t art_id, int value)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_CRITTER:
    case TIG_ART_TYPE_MONSTER:
    case TIG_ART_TYPE_UNIQUE_NPC:
        if (value >= 15) {
            tig_debug_println("Range exceeded in art set.");
            value = 0;
        }
        return (art_id & ~0xF) | value;
    default:
        return art_id;
    }
}

// 0x504150
int tig_art_critter_id_shield_get(tig_art_id_t art_id)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_CRITTER:
    case TIG_ART_TYPE_MONSTER:
    case TIG_ART_TYPE_UNIQUE_NPC:
        return (art_id >> CRITTER_ID_SHIELD_SHIFT) & 1;
    default:
        return 0;
    }
}

// 0x504180
tig_art_id_t tig_art_critter_id_shield_set(tig_art_id_t art_id, int value)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_CRITTER:
    case TIG_ART_TYPE_MONSTER:
    case TIG_ART_TYPE_UNIQUE_NPC:
        if (value >= 2) {
            tig_debug_println("Range exceeded in art set.");
            value = 0;
        }
        return (art_id & ~0x80000) | (value << CRITTER_ID_SHIELD_SHIFT);
    default:
        return art_id;
    }
}

// 0x5041D0
int tig_art_portal_id_create(unsigned int num, int type, int damage_flags, unsigned int frame, int rotation, unsigned int palette, tig_art_id_t* art_id_ptr)
{
    if (num >= ART_ID_MAX_NUM
        || type >= 2
        || frame >= ART_ID_MAX_FRAME
        || rotation >= MAX_ROTATIONS
        || palette >= MAX_PALETTES
        || (damage_flags & ~0x200u) != 0) {
        return TIG_ERR_INVALID_PARAM;
    }

    *art_id_ptr = (TIG_ART_TYPE_PORTAL << ART_ID_TYPE_SHIFT)
        | ((num & (ART_ID_MAX_NUM - 1)) << ART_ID_NUM_SHIFT)
        | ((frame & (ART_ID_MAX_FRAME - 1)) << ART_ID_FRAME_SHIFT)
        | ((rotation & (MAX_ROTATIONS - 1)) << ART_ID_ROTATION_SHIFT)
        | ((type & (PORTAL_ID_MAX_TYPE - 1)) << PORTAL_ID_TYPE_SHIFT)
        | ((palette & (MAX_PALETTES - 1)) << ART_ID_PALETTE_SHIFT)
        | damage_flags;

    return TIG_OK;
}

// 0x504260
int tig_art_portal_id_type_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) == TIG_ART_TYPE_PORTAL) {
        return (art_id >> PORTAL_ID_TYPE_SHIFT) & (PORTAL_ID_MAX_TYPE - 1);
    } else {
        return 0;
    }
}

// 0x504290
int tig_art_scenery_id_create(unsigned int num, int type, unsigned int frame, int rotation, unsigned int palette, tig_art_id_t* art_id_ptr)
{
    if (num >= ART_ID_MAX_NUM
        || type >= SCENERY_ID_MAX_TYPE
        || frame >= ART_ID_MAX_FRAME
        || rotation >= MAX_ROTATIONS
        || palette >= MAX_PALETTES) {
        return TIG_ERR_INVALID_PARAM;
    }

    *art_id_ptr = (TIG_ART_TYPE_SCENERY << ART_ID_TYPE_SHIFT)
        | ((num & (ART_ID_MAX_NUM - 1)) << ART_ID_NUM_SHIFT)
        | ((frame & (ART_ID_MAX_FRAME - 1)) << ART_ID_FRAME_SHIFT)
        | ((rotation & (MAX_ROTATIONS - 1)) << ART_ID_ROTATION_SHIFT)
        | ((type & (SCENERY_ID_MAX_TYPE - 1)) << SCENERY_ID_TYPE_SHIFT)
        | ((palette & (MAX_PALETTES - 1)) << ART_ID_PALETTE_SHIFT);

    return TIG_OK;
}

// 0x504300
int tig_art_scenery_id_type_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_SCENERY) {
        return 0;
    }

    return (art_id >> SCENERY_ID_TYPE_SHIFT) & (SCENERY_ID_MAX_TYPE - 1);
}

// 0x504330
int tig_art_interface_id_create(unsigned int num, unsigned int frame, unsigned char special_flag, unsigned int palette, tig_art_id_t* art_id_ptr)
{
    if (num >= INTERFACE_ID_MAX_NUM
        || frame >= INTERFACE_ID_MAX_FRAME
        || palette >= MAX_PALETTES) {
        return TIG_ERR_INVALID_PARAM;
    }

    *art_id_ptr = (TIG_ART_TYPE_INTERFACE << ART_ID_TYPE_SHIFT)
        | ((num & (INTERFACE_ID_MAX_NUM - 1)) << INTERFACE_ID_NUM_SHIFT)
        | ((frame & (INTERFACE_ID_MAX_FRAME - 1)) << INTERFACE_ID_FRAME_SHIFT)
        | ((special_flag & 1) << 7)
        | ((palette & (MAX_PALETTES - 1)) << ART_ID_PALETTE_SHIFT);

    return TIG_OK;
}

// 0x504390
int tig_art_interface_special_flag_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) == TIG_ART_TYPE_INTERFACE) {
        return (art_id >> 7) & 1;
    } else {
        return 0;
    }
}

// 0x5043C0
int tig_art_item_id_create(int num, int disposition, int damaged, int destroyed, int subtype, int type, int armor_coverage, unsigned int palette, tig_art_id_t* art_id_ptr)
{
    // NOTE: Unsigned compare
    if ((unsigned int)num >= 0x3E8
        || disposition >= ITEM_ID_MAX_DISPOSITION
        || (damaged & ~0x800) != 0
        || (destroyed & ~0x400) != 0
        || subtype >= ITEM_ID_MAX_SUBTYPE
        || type >= ITEM_ID_MAX_TYPE
        || armor_coverage >= ITEM_ID_MAX_ARMOR_COVERAGE
        || palette >= MAX_PALETTES) {
        return TIG_ERR_INVALID_PARAM;
    }

    *art_id_ptr = (TIG_ART_TYPE_ITEM << ART_ID_TYPE_SHIFT)
        | ((num & 0x7FF) << 17)
        | ((armor_coverage & (ITEM_ID_MAX_ARMOR_COVERAGE - 1)) << ITEM_ID_ARMOR_COVERAGE_SHIFT)
        | ((disposition & (ITEM_ID_MAX_DISPOSITION - 1)) << ITEM_ID_DISPOSITION_SHIFT)
        | ((subtype & (ITEM_ID_MAX_SUBTYPE - 1)) << ITEM_ID_SUBTYPE_SHIFT)
        | ((palette & (MAX_PALETTES - 1)) << ART_ID_PALETTE_SHIFT)
        | ((type & (ITEM_ID_MAX_TYPE - 1)) << ITEM_ID_TYPE_SHIFT)
        | damaged
        | destroyed;

    // NOTE: Signed compare.
    if ((int)num >= (int)tig_art_item_subtype_limit_get(*art_id_ptr)) {
        return TIG_ERR_INVALID_PARAM;
    }

    return TIG_OK;
}

// 0x504490
int tig_art_item_id_disposition_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_ITEM) {
        return 0;
    }

    return (art_id >> ITEM_ID_DISPOSITION_SHIFT) & (ITEM_ID_MAX_DISPOSITION - 1);
}

// 0x5044C0
tig_art_id_t tig_art_item_id_disposition_set(tig_art_id_t art_id, int value)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_ITEM) {
        return 0;
    }

    if (value >= ITEM_ID_MAX_DISPOSITION) {
        value = 0;
    }

    return (art_id & ~((ITEM_ID_MAX_DISPOSITION - 1) << ITEM_ID_DISPOSITION_SHIFT))
        | (value << ITEM_ID_DISPOSITION_SHIFT);
}

// 0x5044F0
int tig_art_item_id_subtype_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_ITEM) {
        return 0;
    }

    return (art_id >> ITEM_ID_SUBTYPE_SHIFT) & (ITEM_ID_MAX_SUBTYPE - 1);
}

// 0x504520
tig_art_id_t tig_art_item_id_subtype_set(tig_art_id_t art_id, int value)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_ITEM) {
        return art_id;
    }

    if (value >= ITEM_ID_MAX_SUBTYPE) {
        return art_id;
    }

    return (art_id & ~((ITEM_ID_MAX_SUBTYPE - 1) << ITEM_ID_SUBTYPE_SHIFT))
        | (value << ITEM_ID_SUBTYPE_SHIFT);
}

// 0x504550
int tig_art_item_id_type_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_ITEM) {
        return 0;
    }

    return (art_id >> ITEM_ID_TYPE_SHIFT) & (ITEM_ID_MAX_TYPE - 1);
}

// 0x504570
int tig_art_item_id_armor_coverage_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_ITEM) {
        return 0;
    }

    return (art_id >> ITEM_ID_ARMOR_COVERAGE_SHIFT) & (ITEM_ID_MAX_ARMOR_COVERAGE - 1);
}

// 0x5045A0
int tig_art_item_id_destroyed_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_ITEM) {
        return 0;
    }

    return art_id & 0x400;
}

// 0x5045C0
tig_art_id_t tig_art_item_id_destroyed_set(tig_art_id_t art_id, int value)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_ITEM) {
        return art_id;
    }

    if (value) {
        art_id |= 0x400;
    } else {
        art_id &= ~0x400;
    }

    return art_id;
}

// 0x5045F0
int tig_art_container_id_create(unsigned int num, int type, unsigned int frame, int rotation, unsigned int palette, tig_art_id_t* art_id_ptr)
{
    if (num >= ART_ID_MAX_NUM
        || type >= CONTAINER_ID_MAX_TYPE
        || frame >= ART_ID_MAX_FRAME
        || rotation >= MAX_ROTATIONS
        || palette >= 4) {
        return TIG_ERR_INVALID_PARAM;
    }

    *art_id_ptr = (TIG_ART_TYPE_CONTAINER << ART_ID_TYPE_SHIFT)
        | ((num & (ART_ID_MAX_NUM - 1)) << ART_ID_NUM_SHIFT)
        | ((frame & (ART_ID_MAX_FRAME - 1)) << ART_ID_FRAME_SHIFT)
        | ((rotation & (MAX_ROTATIONS - 1)) << ART_ID_ROTATION_SHIFT)
        | ((type & (CONTAINER_ID_MAX_TYPE - 1)) << CONTAINER_ID_TYPE_SHIFT)
        | ((palette & 3) << 4);

    return TIG_OK;
}

// 0x504660
int tig_art_container_id_type_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_CONTAINER) {
        return 0;
    }

    return (art_id >> CONTAINER_ID_TYPE_SHIFT) & (CONTAINER_ID_MAX_TYPE - 1);
}

// 0x504690
int tig_art_light_id_create(unsigned int num, unsigned int frame, unsigned int rotation, int extended_rotation, tig_art_id_t* art_id_ptr)
{
    if (num >= ART_ID_MAX_NUM
        || frame >= LIGHT_ID_MAX_FRAME
        || (extended_rotation ? rotation >= 32 : rotation >= 8)) {
        return TIG_ERR_INVALID_PARAM;
    }

    *art_id_ptr = (TIG_ART_TYPE_LIGHT << ART_ID_TYPE_SHIFT)
        | ((num & (ART_ID_MAX_NUM - 1)) << ART_ID_NUM_SHIFT)
        | ((frame & (LIGHT_ID_MAX_FRAME - 1)) << LIGHT_ID_FRAME_SHIFT)
        | ((rotation & (MAX_ROTATIONS - 1)) << LIGHT_ID_ROTATION_SHIFT)
        | ((rotation & 0x1F) << 4)
        | (extended_rotation & 1);

    return TIG_OK;
}

// 0x504700
int tig_art_light_id_rotation_get(tig_art_id_t art_id)
{
    if (tig_art_light_id_has_extended_rotation(art_id) != 0) {
        return (art_id >> 4) & 0x1F;
    } else {
        return tig_art_id_rotation_get(art_id);
    }
}

// 0x504730
tig_art_id_t tig_art_light_id_rotation_set(tig_art_id_t art_id, int rotation)
{
    if (!tig_art_light_id_has_extended_rotation(art_id)) {
        return tig_art_id_rotation_set(art_id, rotation);
    }

    if (rotation >= 32) {
        rotation = 0;
    } else if (rotation < 0) {
        rotation = 31;
    }

    return (art_id & ~0xFF0)
        | ((rotation % 8) << 9)
        | (rotation << 4);
}

// 0x504790
int tig_art_light_id_has_extended_rotation(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) == TIG_ART_TYPE_LIGHT) {
        return art_id & 1;
    } else {
        return 0;
    }
}

// 0x5047B0
int tig_art_roof_id_create(unsigned int num, int piece, unsigned int fill, unsigned int fade, tig_art_id_t* art_id_ptr)
{
    int flipped;
    int palette;

    if (num >= ART_ID_MAX_NUM
        || piece >= 13
        || fill > 1
        || fade > 1) {
        return TIG_ERR_INVALID_PARAM;
    }

    if (piece >= 9) {
        piece -= 9;
        flipped = 1;
        palette = 1;
    } else {
        flipped = 0;
        palette = 0;
    }

    *art_id_ptr = (TIG_ART_TYPE_ROOF << ART_ID_TYPE_SHIFT)
        | ((num & (ART_ID_MAX_NUM - 1)) << ART_ID_NUM_SHIFT)
        | ((piece & 0x1F) << 14)
        | ((fill & 1) << ROOF_FILL_SHIFT)
        | ((fade & 1) << ROOF_FADE_SHIFT)
        | ((palette & (MAX_PALETTES - 1)) << ART_ID_PALETTE_SHIFT);
    *art_id_ptr = tig_art_id_flags_set(*art_id_ptr, flipped);

    return TIG_OK;
}

// 0x504840
int tig_art_roof_id_piece_get(tig_art_id_t art_id)
{
    int piece;

    if (tig_art_type(art_id) != TIG_ART_TYPE_ROOF) {
        return -1;
    }

    piece = tig_art_id_frame_get(art_id);
    if ((tig_art_id_flags_get(art_id) & 1) != 0) {
        piece += 9;
    }

    return piece;
}

// 0x504880
tig_art_id_t tig_art_roof_id_piece_set(tig_art_id_t art_id, int frame)
{
    int flipped = 0;
    int palette = 0;

    if (tig_art_type(art_id) != TIG_ART_TYPE_ROOF) {
        return art_id;
    }

    if (frame >= 9) {
        frame = frame - 9;
        flipped = 1;
        palette = 1;
    }

    art_id = tig_art_id_frame_set(art_id, frame);
    art_id = tig_art_id_flags_set(art_id, flipped);
    art_id = tig_art_id_palette_set(art_id, palette);

    return art_id;
}

// 0x5048D0
unsigned int tig_art_roof_id_fill_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_ROOF) {
        return 0;
    }

    return (art_id >> ROOF_FILL_SHIFT) & 1;
}

// 0x504900
tig_art_id_t tig_art_roof_id_fill_set(tig_art_id_t art_id, unsigned int value)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_ROOF) {
        return art_id;
    }

    if (value > 1) {
        tig_debug_println("Range exceeded in art set.");
        value = 0;
    }

    art_id &= ~(1 << ROOF_FILL_SHIFT);
    art_id |= value << ROOF_FILL_SHIFT;

    return art_id;
}

// 0x504940
unsigned int tig_art_roof_id_fade_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_ROOF) {
        return 0;
    }

    return (art_id >> ROOF_FADE_SHIFT) & 1;
}

// 0x504970
tig_art_id_t tig_art_roof_id_fade_set(tig_art_id_t art_id, unsigned int value)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_ROOF) {
        return art_id;
    }

    if (value > 1) {
        tig_debug_println("Range exceeded in art set.");
        value = 0;
    }

    art_id &= ~(1 << ROOF_FADE_SHIFT);
    art_id |= value << ROOF_FADE_SHIFT;

    return art_id;
}

// 0x5049B0
int tig_art_facade_id_create(unsigned int num, unsigned int tile_num, unsigned int type, unsigned int flippable, unsigned int frame, unsigned int walkable, tig_art_id_t* art_id_ptr)
{
    if (num >= FACADE_ID_MAX_NUM
        || tile_num >= TILE_ID_MAX_NUM
        || type >= TILE_ID_MAX_TYPE
        || flippable >= 2
        || frame >= FACADE_ID_MAX_FRAME
        || walkable >= 2) {
        return TIG_ERR_INVALID_PARAM;
    }

    *art_id_ptr = (TIG_ART_TYPE_FACADE << ART_ID_TYPE_SHIFT)
        | ((num < 256 ? 0 : 1) << FACADE_ID_NUM_HIGH_SHIFT)
        | ((flippable & 1) << FACADE_ID_FLIPPABLE_SHIFT)
        | ((type & (TILE_ID_MAX_TYPE - 1)) << FACADE_ID_TYPE_SHIFT)
        | ((num & 0xFF) << FACADE_ID_NUM_LOW_SHIFT)
        | ((tile_num & (TILE_ID_MAX_NUM - 1)) << FACADE_ID_TILE_NUM_SHIFT)
        | ((frame & (FACADE_ID_MAX_FRAME - 1)) << FACADE_ID_FRAME_SHIFT)
        | (walkable & 1);

    return TIG_OK;
}

// 0x504A60
int tig_art_facade_id_num_get(tig_art_id_t art_id)
{
    int num;

    if (tig_art_type(art_id) != TIG_ART_TYPE_FACADE) {
        return 0;
    }

    num = (art_id >> FACADE_ID_NUM_LOW_SHIFT) & 0xFF;
    if ((art_id & (1 << FACADE_ID_NUM_HIGH_SHIFT)) != 0) {
        num += 256;
    }

    return num;
}

// 0x504A90
int tig_art_facade_id_frame_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_FACADE) {
        return 0;
    }

    return (art_id >> FACADE_ID_FRAME_SHIFT) & (FACADE_ID_MAX_FRAME - 1);
}

// 0x504AC0
int tig_art_facade_id_walkable_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_FACADE) {
        return 0;
    }

    return art_id & 1;
}

// 0x504AE0
int tig_art_eye_candy_id_create(unsigned int num, unsigned int frame, int rotation, int translucency, int type, unsigned int palette, int scale, tig_art_id_t* art_id)
{
    if (num >= ART_ID_MAX_NUM
        || frame >= EYE_CANDY_ID_MAX_FRAME
        || rotation >= MAX_ROTATIONS
        || translucency >= 2
        || type >= 4 // FIXME: Wrong, should be TIG_ART_EYE_CANDY_TYPE_COUNT(3).
        || palette >= MAX_PALETTES
        || scale < 0
        || scale > 7) {
        return TIG_ERR_INVALID_PARAM;
    }

    *art_id = (TIG_ART_TYPE_EYE_CANDY << ART_ID_TYPE_SHIFT)
        | ((num & (ART_ID_MAX_NUM - 1)) << ART_ID_NUM_SHIFT)
        | ((frame & (EYE_CANDY_ID_MAX_FRAME - 1)) << EYE_CANDY_ID_FRAME_SHIFT)
        | ((rotation & (MAX_ROTATIONS - 1)) << EYE_CANDY_ID_ROTATION_SHIFT)
        | ((translucency & 1) << 8)
        | ((type & TIG_ART_EYE_CANDY_TYPE_COUNT) << 6)
        | ((palette & (MAX_PALETTES - 1)) << ART_ID_PALETTE_SHIFT)
        | ((scale & 7) << 1);

    return TIG_OK;
}

// 0x504B90
int tig_art_eye_candy_id_type_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_EYE_CANDY) {
        return 0;
    }

    return (art_id >> EYE_CANDY_ID_TYPE_SHIFT) & TIG_ART_EYE_CANDY_TYPE_COUNT;
}

// 0x504BC0
tig_art_id_t tig_art_eye_candy_id_type_set(tig_art_id_t art_id, int value)
{
    if (tig_art_type(art_id) != TIG_ART_TYPE_EYE_CANDY) {
        return art_id;
    }

    if (value >= TIG_ART_EYE_CANDY_TYPE_COUNT) {
        tig_debug_println("Range exceeded in art set.");
        value = 0;
    }

    // NOTE: Rare case - value is additionally ANDed with max value.
    return (art_id & ~(TIG_ART_EYE_CANDY_TYPE_COUNT << EYE_CANDY_ID_TYPE_SHIFT))
        | ((value & TIG_ART_EYE_CANDY_TYPE_COUNT) << EYE_CANDY_ID_TYPE_SHIFT);
}

// 0x504C00
int tig_art_eye_candy_id_translucency_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) == TIG_ART_TYPE_EYE_CANDY) {
        return (art_id >> 8) & 1;
    } else {
        return 0;
    }
}

// 0x504C30
tig_art_id_t tig_art_eye_candy_id_translucency_set(tig_art_id_t art_id, int value)
{
    if (tig_art_type(art_id) == TIG_ART_TYPE_EYE_CANDY) {
        return (art_id & ~0x100) | ((value & 1) << 8);
    } else {
        return art_id;
    }
}

// 0x504C60
int tig_art_eye_candy_id_scale_get(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) == TIG_ART_TYPE_EYE_CANDY) {
        return (art_id >> 1) & 7;
    } else {
        // NOTE: Rare case - default is not 0.
        return 4;
    }
}

// 0x504C90
tig_art_id_t tig_art_eye_candy_id_scale_set(tig_art_id_t art_id, int value)
{
    if (tig_art_type(art_id) == TIG_ART_TYPE_EYE_CANDY) {
        return (art_id & ~0xE) | ((value & 7) << 1);
    } else {
        return art_id;
    }
}

// 0x504FD0
int tig_art_id_flags_get(tig_art_id_t art_id)
{
    switch (tig_art_type(art_id)) {
    case TIG_ART_TYPE_TILE:
    case TIG_ART_TYPE_WALL:
    case TIG_ART_TYPE_PORTAL:
    case TIG_ART_TYPE_ROOF:
        return art_id & 0xF;
    default:
        return 0;
    }
}

// 0x505000
void tig_art_palette_adjust_or_copy(tig_art_id_t art_id, TigPalette* src_palette, TigPalette* dst_palette)
{
    TigPaletteModifyInfo modify_info;

    if (tig_art_palette_adjust_callback != NULL && tig_art_palette_adjust_callback(art_id, &modify_info)) {
        modify_info.src_palette = src_palette;
        modify_info.dst_palette = dst_palette;
        tig_palette_modify(&modify_info);
    } else {
        tig_palette_copy(dst_palette, src_palette);
    }
}

// 0x505060
static int art_get_video_buffer_impl(int cache_entry_index, tig_art_id_t art_id, TigVideoBuffer** video_buffer_ptr);
int art_get_video_buffer(int cache_entry_index, tig_art_id_t art_id, TigVideoBuffer** video_buffer_ptr)
{
    if (video_buffer_ptr == NULL) return TIG_ERR_INVALID_PARAM;
    *video_buffer_ptr = NULL;
    if (cache_entry_index < 0 || cache_entry_index >= tig_art_cache_entries_length) return TIG_ERR_INVALID_PARAM;

    Uint64 start = tig_performance_begin();
    char resource[TIG_MAX_PATH];
    if (start != 0) SDL_strlcpy(resource, tig_art_cache_entries[cache_entry_index].path, sizeof(resource));
    int rc = art_get_video_buffer_impl(cache_entry_index, art_id, video_buffer_ptr);
    tig_performance_end_detail(TIG_PERF_ART_PREPARE, start, start != 0 ? resource : NULL);
    return rc;
}

static void tig_art_video_batch_free(TigVideoBuffer** buffers, int count)
{
    if (buffers == NULL) return;
    for (int frame = 0; frame < count; frame++) {
        if (buffers[frame] != NULL) tig_video_buffer_destroy(buffers[frame]);
    }
    FREE(buffers);
}

static int art_get_video_buffer_impl(int cache_entry_index, tig_art_id_t art_id, TigVideoBuffer** video_buffer_ptr)
{
    TigArtCacheEntry* art = &tig_art_cache_entries[cache_entry_index];
    int type = tig_art_type(art_id);
    if ((type == TIG_ART_TYPE_ITEM && tig_art_item_id_disposition_get(art_id) != 0)
        || (type == TIG_ART_TYPE_INTERFACE && tig_art_interface_special_flag_get(art_id) == 0)
        || type == TIG_ART_TYPE_MISC
        || (type == TIG_ART_TYPE_LIGHT && (!tig_art_hardware_accelerated || !tig_art_light_id_has_extended_rotation(art_id))))
        return TIG_ERR_GENERIC;
    if (type == TIG_ART_TYPE_TILE) art_id = tig_art_id_flags_set(art_id, tig_art_id_flags_get(art_id) & ~1);
    bool roof = type == TIG_ART_TYPE_ROOF;
    int rotation = roof ? 0 : tig_art_id_rotation_get(art_id);
    if ((type == TIG_ART_TYPE_WALL || type == TIG_ART_TYPE_PORTAL) && (rotation & 1) != 0) {
        art_id = tig_art_id_rotation_set(art_id, rotation - 1);
        rotation = tig_art_id_rotation_get(art_id);
    }
    int palette = roof ? 0 : tig_art_id_palette_get(art_id);
    if (art->palette_tbl[palette] == NULL) {
        palette = 0;
        art_id = tig_art_id_palette_set(art_id, 0);
    }
    int frame_count = roof ? 13 : art->hdr.num_frames;
    int requested_frame = roof ? tig_art_roof_id_piece_get(art_id) : tig_art_id_frame_get(art_id);
    if (rotation < 0 || rotation >= MAX_ROTATIONS || art->hdr.frames_tbl[rotation] == NULL
        || art->hdr.num_frames <= 0 || (roof && (art->hdr.num_frames < 9 || art->hdr.num_frames > 13))
        || requested_frame < 0 || requested_frame >= frame_count
        || (uint64_t)frame_count * sizeof(TigVideoBuffer*) > INT_MAX) return TIG_ERR_INVALID_PARAM;
    TigVideoBuffer** buffers = art->video_buffers[palette][rotation];
    bool creating = buffers == NULL;
    art_size_t video_bytes = 0;
    if (creating) {
        buffers = (TigVideoBuffer**)CALLOC(frame_count, sizeof(*buffers));
        if (buffers == NULL) return TIG_ERR_OUT_OF_MEMORY;
        TigVideoBufferCreateInfo create = { 0 };
        create.color_key = tig_color_make(0, 255, 0);
        create.background_color = create.color_key;
        if (tig_art_hardware_accelerated) {
            create.flags = TIG_VIDEO_BUFFER_CREATE_TEXTURE;
            create.color_key = 0;
        } else create.flags = TIG_VIDEO_BUFFER_CREATE_COLOR_KEY | TIG_VIDEO_BUFFER_CREATE_VIDEO_MEMORY;
        for (int frame = 0; frame < frame_count; frame++) {
            int source_frame = roof && frame >= 9 ? frame - 9 : frame;
            create.width = art->hdr.frames_tbl[rotation][source_frame].width;
            create.height = art->hdr.frames_tbl[rotation][source_frame].height;
            int rc = tig_video_buffer_create(&create, &buffers[frame]);
            if (rc != TIG_OK) {
                tig_art_video_batch_free(buffers, frame_count);
                return rc;
            }
            video_bytes += (art_size_t)tig_video_buffer_memory_size(buffers[frame]);
        }
    }
    if (creating || art->dirty[palette][rotation] != 0) {
        for (int frame = 0; frame < frame_count; frame++) {
            int source_frame = roof && frame >= 9 ? frame - 9 : frame;
            TigRect rect = { 0, 0, art->hdr.frames_tbl[rotation][source_frame].width,
                art->hdr.frames_tbl[rotation][source_frame].height };
            TigArtBlitInfo blit = { 0 };
            blit.art_id = roof ? tig_art_roof_id_piece_set(art_id, frame) : tig_art_id_frame_set(art_id, frame);
            blit.src_rect = &rect;
            blit.dst_rect = &rect;
            blit.dst_video_buffer = buffers[frame];
            int rc = tig_art_hardware_accelerated ? tig_art_frame_blit_software(cache_entry_index, &blit) : art_blit(cache_entry_index, &blit);
            if (rc != TIG_OK) {
                if (creating) tig_art_video_batch_free(buffers, frame_count);
                return rc;
            }
        }
        art->dirty[palette][rotation] = 0;
    }
    if (creating) {
        art_size_t table_bytes = (art_size_t)frame_count * sizeof(*buffers);
        art->video_buffers[palette][rotation] = buffers;
        art->system_memory_usage += table_bytes;
        art->video_memory_usage += video_bytes;
        tig_art_available_system_memory -= table_bytes;
        tig_art_available_video_memory -= video_bytes;
    }
    *video_buffer_ptr = buffers[requested_frame];
    return TIG_OK;
}

// 0x505940
int tig_art_blit_flags_to_video_flags(unsigned int art_blt_flags, unsigned int* vb_blt_flags_ptr)
{
    if ((art_blt_flags & 0x1802C) != 0) {
        return TIG_ERR_INVALID_PARAM;
    }

    *vb_blt_flags_ptr = 0;

    if ((art_blt_flags & TIG_ART_BLT_FLIP_X) != 0) {
        *vb_blt_flags_ptr |= TIG_VIDEO_BUFFER_BLIT_FLIP_X;
    }

    if ((art_blt_flags & TIG_ART_BLT_FLIP_Y) != 0) {
        *vb_blt_flags_ptr |= TIG_VIDEO_BUFFER_BLIT_FLIP_Y;
    }

    if ((art_blt_flags & TIG_ART_BLT_BLEND_ADD) != 0) {
        *vb_blt_flags_ptr |= TIG_VIDEO_BUFFER_BLIT_BLEND_ADD;
    }

    if ((art_blt_flags & TIG_ART_BLT_BLEND_MUL) != 0) {
        *vb_blt_flags_ptr |= TIG_VIDEO_BUFFER_BLIT_BLEND_MUL;
    }

    if ((art_blt_flags & TIG_ART_BLT_BLEND_ALPHA_AVG) != 0) {
        *vb_blt_flags_ptr |= TIG_VIDEO_BUFFER_BLIT_BLEND_ALPHA_AVG;
    }

    if ((art_blt_flags & TIG_ART_BLT_BLEND_ALPHA_CONST) != 0) {
        *vb_blt_flags_ptr |= TIG_VIDEO_BUFFER_BLIT_BLEND_ALPHA_CONST;
    }

    if ((art_blt_flags & TIG_ART_BLT_BLEND_ALPHA_SRC) != 0) {
        *vb_blt_flags_ptr |= TIG_VIDEO_BUFFER_BLIT_BLEND_ALPHA_SRC;
    }

    if ((art_blt_flags & TIG_ART_BLT_BLEND_ALPHA_LERP_X) != 0) {
        *vb_blt_flags_ptr |= TIG_VIDEO_BUFFER_BLIT_BLEND_ALPHA_LERP;
    }

    if ((art_blt_flags & TIG_ART_BLT_BLEND_ALPHA_LERP_Y) != 0) {
        *vb_blt_flags_ptr |= TIG_VIDEO_BUFFER_BLIT_BLEND_ALPHA_LERP;
    }

    if ((art_blt_flags & TIG_ART_BLT_BLEND_ALPHA_LERP_BOTH) != 0) {
        *vb_blt_flags_ptr |= TIG_VIDEO_BUFFER_BLIT_BLEND_ALPHA_LERP;
    }

    if ((art_blt_flags & TIG_ART_BLT_BLEND_COLOR_CONST) != 0) {
        *vb_blt_flags_ptr |= TIG_VIDEO_BUFFER_BLIT_BLEND_COLOR_CONST;
    }

    if ((art_blt_flags & TIG_ART_BLT_BLEND_COLOR_ARRAY) != 0) {
        *vb_blt_flags_ptr |= TIG_VIDEO_BUFFER_BLIT_BLEND_COLOR_LERP;
    }

    if ((art_blt_flags & TIG_ART_BLT_BLEND_COLOR_LERP) != 0) {
        *vb_blt_flags_ptr |= TIG_VIDEO_BUFFER_BLIT_BLEND_COLOR_LERP;
    }

    return TIG_OK;
}

// 0x5059F0
int tig_art_frame_blit_software(int cache_entry_index, TigArtBlitInfo* blit_info)
{
    if (blit_info == NULL || blit_info->dst_video_buffer == NULL || cache_entry_index < 0
        || cache_entry_index >= tig_art_cache_entries_length || tig_art_bits_per_pixel != 32
        || tig_video_buffer_memory_size(blit_info->dst_video_buffer) == 0) return TIG_ERR_INVALID_PARAM;
    TigArtCacheEntry* art = &tig_art_cache_entries[cache_entry_index];
    int rotation = tig_art_id_rotation_get(blit_info->art_id), frame = tig_art_id_frame_get(blit_info->art_id);
    if (frame < 0 || frame >= art->hdr.num_frames || rotation < 0 || rotation >= MAX_ROTATIONS
        || art->pixels_tbl[rotation] == NULL || art->hdr.frames_tbl[rotation] == NULL) return TIG_ERR_INVALID_PARAM;
    TigArtFileFrameData* image = &art->hdr.frames_tbl[rotation][frame];
    if (image->width <= 0 || image->height <= 0 || (uint64_t)image->width * image->height > INT_MAX)
        return TIG_ERR_INVALID_PARAM;
    int palette_index = tig_art_id_palette_get(blit_info->art_id);
    TigPalette* palette = art->hdr.palette_tbl[palette_index];
    if (palette == NULL) palette = art->hdr.palette_tbl[0];
    if (palette == NULL) return TIG_ERR_INVALID_PARAM;
    int rc = tig_video_buffer_lock(blit_info->dst_video_buffer);
    if (rc != TIG_OK) return rc;
    TigVideoBufferData destination;
    rc = tig_video_buffer_data(blit_info->dst_video_buffer, &destination);
    if (rc == TIG_OK && !tig_art_frame_convert(art->pixels_tbl[rotation][frame], (size_t)image->width * image->height,
            image->width, image->height, palette->colors, (tig_art_id_flags_get(blit_info->art_id) & 1) != 0,
            destination.pixels, destination.width, destination.height, destination.pitch)) rc = TIG_ERR_INVALID_PARAM;
    tig_video_buffer_unlock(blit_info->dst_video_buffer);
    return rc;
}

// 0x505EB0
static int art_blit_impl(int cache_entry_index, TigArtBlitInfo* blit_info);
int art_blit(int cache_entry_index, TigArtBlitInfo* blit_info)
{
    Uint64 start = tig_performance_begin();
    char resource[TIG_MAX_PATH];
    if (start != 0) SDL_strlcpy(resource, tig_art_cache_entries[cache_entry_index].path, sizeof(resource));
    int rc = art_blit_impl(cache_entry_index, blit_info);
    tig_performance_end_detail(TIG_PERF_ART_BLIT, start, start != 0 ? resource : NULL);
    return rc;
}

static uint8_t tig_art_gradient_channel(float value)
{
    if (value <= 0.0f) return 0;
    if (value >= 255.0f) return 255;
    return (uint8_t)value;
}

static int tig_art_blit_gradient(int cache_entry_index, TigArtBlitInfo* blit_info)
{
    TigArtCacheEntry* art = &tig_art_cache_entries[cache_entry_index];
    int rotation = tig_art_id_rotation_get(blit_info->art_id);
    int frame = tig_art_id_frame_get(blit_info->art_id);
    if (frame < 0 || frame >= art->hdr.num_frames || rotation < 0 || rotation >= MAX_ROTATIONS
        || art->hdr.frames_tbl[rotation] == NULL || art->pixels_tbl[rotation] == NULL)
        return TIG_ERR_INVALID_PARAM;
    TigArtFileFrameData* image = &art->hdr.frames_tbl[rotation][frame];
    const uint8_t* source = art->pixels_tbl[rotation][frame];
    if (image->width == 0 || image->height == 0) return TIG_OK;
    if (source == NULL || image->width < 0 || image->height < 0
        || (uint64_t)image->width * image->height > INT_MAX) return TIG_ERR_INVALID_PARAM;
    int palette_index = tig_art_id_palette_get(blit_info->art_id);
    TigPalette* palette;
    if ((blit_info->flags & TIG_ART_BLT_PALETTE_OVERRIDE) != 0) {
        palette = blit_info->palette;
    } else {
        TigPalette** palettes = (blit_info->flags & TIG_ART_BLT_PALETTE_ORIGINAL) != 0
            ? art->hdr.palette_tbl : art->palette_tbl;
        palette = palettes[palette_index] != NULL ? palettes[palette_index] : palettes[0];
    }
    if (palette == NULL || tig_art_bits_per_pixel != 32
        || tig_video_buffer_memory_size(blit_info->dst_video_buffer) == 0) return TIG_ERR_INVALID_PARAM;
    TigVideoBufferData destination;
    int rc = tig_video_buffer_lock(blit_info->dst_video_buffer);
    if (rc != TIG_OK) return rc;
    rc = tig_video_buffer_data(blit_info->dst_video_buffer, &destination);
    if (rc != TIG_OK) {
        tig_video_buffer_unlock(blit_info->dst_video_buffer);
        return rc;
    }
    int64_t left = SDL_max(0, SDL_max(-(int64_t)blit_info->src_rect->x, -(int64_t)blit_info->dst_rect->x));
    int64_t top = SDL_max(0, SDL_max(-(int64_t)blit_info->src_rect->y, -(int64_t)blit_info->dst_rect->y));
    int64_t right = SDL_min(blit_info->src_rect->width,
        SDL_min((int64_t)image->width - blit_info->src_rect->x, (int64_t)destination.width - blit_info->dst_rect->x));
    int64_t bottom = SDL_min(blit_info->src_rect->height,
        SDL_min((int64_t)image->height - blit_info->src_rect->y, (int64_t)destination.height - blit_info->dst_rect->y));
    if (left < right && top < bottom) {
        int source_x = (int)((int64_t)blit_info->src_rect->x + left);
        int source_y = (int)((int64_t)blit_info->src_rect->y + top);
        int destination_x = (int)((int64_t)blit_info->dst_rect->x + left);
        int destination_y = (int)((int64_t)blit_info->dst_rect->y + top);
        int width = (int)(right - left), height = (int)(bottom - top);
        unsigned int flip = blit_info->flags & (TIG_ART_BLT_FLIP_X | TIG_ART_BLT_FLIP_Y);
        if ((tig_art_id_flags_get(blit_info->art_id) & 1) != 0) flip ^= TIG_ART_BLT_FLIP_X;
        float start[3], end[3], start_step[3], end_step[3];
        const int channels[4][3] = {
            { tig_color_get_red(blit_info->blend_colors[0]), tig_color_get_green(blit_info->blend_colors[0]), tig_color_get_blue(blit_info->blend_colors[0]) },
            { tig_color_get_red(blit_info->blend_colors[1]), tig_color_get_green(blit_info->blend_colors[1]), tig_color_get_blue(blit_info->blend_colors[1]) },
            { tig_color_get_red(blit_info->blend_colors[2]), tig_color_get_green(blit_info->blend_colors[2]), tig_color_get_blue(blit_info->blend_colors[2]) },
            { tig_color_get_red(blit_info->blend_colors[3]), tig_color_get_green(blit_info->blend_colors[3]), tig_color_get_blue(blit_info->blend_colors[3]) }
        };
        for (int channel = 0; channel < 3; channel++) {
            start_step[channel] = (float)(channels[3][channel] - channels[0][channel]) / blit_info->blend_rect->height;
            end_step[channel] = (float)(channels[2][channel] - channels[1][channel]) / blit_info->blend_rect->height;
            start[channel] = start_step[channel] * ((int64_t)source_y - blit_info->blend_rect->y) + channels[0][channel];
            end[channel] = end_step[channel] * ((int64_t)source_y - blit_info->blend_rect->y) + channels[1][channel];
        }
        for (int y = 0; y < height; y++) {
            int sample_y = (flip & TIG_ART_BLT_FLIP_Y) != 0 ? image->height - 1 - source_y - y : source_y + y;
            const uint8_t* row = source + (size_t)sample_y * image->width;
            uint32_t* target = (uint32_t*)((uint8_t*)destination.pixels + (size_t)(destination_y + y) * destination.pitch) + destination_x;
            float value[3], step[3];
            for (int channel = 0; channel < 3; channel++) {
                step[channel] = (end[channel] - start[channel]) / blit_info->blend_rect->width;
                value[channel] = start[channel] + step[channel] * ((int64_t)source_x - blit_info->blend_rect->x);
            }
            for (int x = 0; x < width; x++) {
                int sample_x = (flip & TIG_ART_BLT_FLIP_X) != 0 ? image->width - 1 - source_x - x : source_x + x;
                uint8_t index = row[sample_x];
                if (index != 0) target[x] = tig_color_mul(palette->colors[index],
                    tig_color_make(tig_art_gradient_channel(value[0]), tig_art_gradient_channel(value[1]), tig_art_gradient_channel(value[2])));
                for (int channel = 0; channel < 3; channel++) value[channel] += step[channel];
            }
            for (int channel = 0; channel < 3; channel++) {
                start[channel] += start_step[channel];
                end[channel] += end_step[channel];
            }
        }
    }
    tig_video_buffer_unlock(blit_info->dst_video_buffer);
    return TIG_OK;
}

static bool tig_art_clip_copy(const TigRect* source, const TigRect* destination,
    int source_width, int source_height, int destination_width, int destination_height,
    TigRect* clipped_source, TigRect* clipped_destination)
{
    int64_t left = SDL_max(0, SDL_max(-(int64_t)source->x, -(int64_t)destination->x));
    int64_t top = SDL_max(0, SDL_max(-(int64_t)source->y, -(int64_t)destination->y));
    int64_t right = SDL_min(source->width,
        SDL_min((int64_t)source_width - source->x, (int64_t)destination_width - destination->x));
    int64_t bottom = SDL_min(source->height,
        SDL_min((int64_t)source_height - source->y, (int64_t)destination_height - destination->y));
    if (left >= right || top >= bottom) return false;
    *clipped_source = (TigRect){ (int)((int64_t)source->x + left), (int)((int64_t)source->y + top),
        (int)(right - left), (int)(bottom - top) };
    *clipped_destination = (TigRect){ (int)((int64_t)destination->x + left), (int)((int64_t)destination->y + top),
        (int)(right - left), (int)(bottom - top) };
    return true;
}

static void tig_art_copy_row(const uint8_t* source, const TigVideoBufferData* destination,
    const TigRect* source_rect, const TigRect* destination_rect, int image_width, int image_height,
    unsigned int flip, int row, uint8_t** source_pixels, uint8_t** destination_pixels)
{
    int source_y = (flip & TIG_ART_BLT_FLIP_Y) != 0 ? image_height - 1 - source_rect->y - row : source_rect->y + row;
    int source_x = (flip & TIG_ART_BLT_FLIP_X) != 0 ? image_width - 1 - source_rect->x : source_rect->x;
    *source_pixels = (uint8_t*)source + (size_t)source_y * image_width + source_x;
    *destination_pixels = (uint8_t*)destination->pixels + (size_t)(destination_rect->y + row) * destination->pitch
        + (size_t)destination_rect->x * tig_art_bytes_per_pixel;
}

typedef struct TigArtScaleAxis {
    uint64_t sample;
    uint64_t remainder;
    uint64_t step;
    uint64_t remainder_step;
    uint64_t denominator;
} TigArtScaleAxis;

static TigArtScaleAxis tig_art_scale_axis(int source_extent, int destination_extent, int64_t offset)
{
    uint64_t denominator = (uint64_t)destination_extent * 2;
    uint64_t numerator = (uint64_t)offset * source_extent * 2 + destination_extent - 1;
    TigArtScaleAxis axis = { numerator / denominator, numerator % denominator,
        (uint64_t)source_extent / destination_extent, (uint64_t)(source_extent % destination_extent) * 2, denominator };
    return axis;
}

static void tig_art_scale_axis_advance(TigArtScaleAxis* axis)
{
    axis->sample += axis->step;
    axis->remainder += axis->remainder_step;
    if (axis->remainder >= axis->denominator) {
        axis->remainder -= axis->denominator;
        axis->sample++;
    }
}

static unsigned int tig_art_scaled_blend_mode(unsigned int flags)
{
    const unsigned int modes[] = { TIG_ART_BLT_BLEND_ADD, TIG_ART_BLT_BLEND_SUB, TIG_ART_BLT_BLEND_MUL,
        TIG_ART_BLT_BLEND_ALPHA_AVG, TIG_ART_BLT_BLEND_ALPHA_CONST, TIG_ART_BLT_BLEND_ALPHA_SRC };
    for (int index = 0; index < sizeof(modes) / sizeof(modes[0]); index++) {
        if ((flags & modes[index]) != 0) return modes[index];
    }
    if ((flags & TIG_ART_BLT_BLEND_ALPHA_LERP_ANY) != 0) return flags & TIG_ART_BLT_BLEND_ALPHA_LERP_ANY;
    if ((flags & TIG_ART_BLT_BLEND_ALPHA_STIPPLE_S) != 0) return TIG_ART_BLT_BLEND_ALPHA_STIPPLE_S;
    if ((flags & TIG_ART_BLT_BLEND_ALPHA_STIPPLE_D) != 0) return TIG_ART_BLT_BLEND_ALPHA_STIPPLE_D;
    return 0;
}

static void tig_art_scaled_pixel(const TigArtBlitInfo* info, unsigned int blend, uint32_t base,
    int source_x, int source_y, int image_width, int image_height, int destination_x, int destination_y, uint32_t* target)
{
    bool constant = (info->flags & TIG_ART_BLT_BLEND_COLOR_CONST) != 0;
    bool array = !constant && (info->flags & TIG_ART_BLT_BLEND_COLOR_ARRAY) != 0;
    uint32_t tint = constant ? info->color : array ? info->blend_colors[source_x] : 0;
    uint32_t color = constant || array ? tig_color_mul(base, tint) : base;
    switch (blend) {
    case TIG_ART_BLT_BLEND_ADD:
        *target = tig_color_add(*target, color);
        break;
    case TIG_ART_BLT_BLEND_SUB:
        *target = tig_color_sub(color, *target);
        break;
    case TIG_ART_BLT_BLEND_MUL:
        *target = tig_color_mul(color, *target);
        break;
    case TIG_ART_BLT_BLEND_ALPHA_AVG:
        *target = tig_color_blend_alpha(color, *target, tig_color_rgb_to_grayscale(color));
        break;
    case TIG_ART_BLT_BLEND_ALPHA_CONST:
        *target = tig_color_blend_alpha(color, *target, info->alpha[0]);
        break;
    case TIG_ART_BLT_BLEND_ALPHA_SRC:
        *target = tig_color_blend_alpha(constant || array ? tint : base, *target, tig_color_alpha(base));
        break;
    case TIG_ART_BLT_BLEND_ALPHA_STIPPLE_S:
        if (((source_x ^ source_y) & 1) != 0) *target = color;
        break;
    case TIG_ART_BLT_BLEND_ALPHA_STIPPLE_D:
        if (((destination_x ^ destination_y) & 1) != 0) *target = color;
        break;
    case 0:
        *target = color;
        break;
    default: {
        float left = info->alpha[0], right = info->alpha[1];
        if (blend != TIG_ART_BLT_BLEND_ALPHA_LERP_X) {
            left += (float)((int)info->alpha[3] - info->alpha[0]) / image_height * source_y;
            right += (float)((int)info->alpha[2] - info->alpha[1]) / image_height * source_y;
        }
        float alpha = blend == TIG_ART_BLT_BLEND_ALPHA_LERP_Y ? left : left + (right - left) / image_width * source_x;
        *target = tig_color_blend_alpha(color, *target, tig_art_gradient_channel(alpha));
        break;
    }
    }
}

static int tig_art_blit_scaled(int cache_entry_index, TigArtBlitInfo* info)
{
    TigArtCacheEntry* art = &tig_art_cache_entries[cache_entry_index];
    int rotation = tig_art_id_rotation_get(info->art_id), frame = tig_art_id_frame_get(info->art_id);
    if (frame < 0 || frame >= art->hdr.num_frames || rotation < 0 || rotation >= MAX_ROTATIONS
        || art->hdr.frames_tbl[rotation] == NULL || art->pixels_tbl[rotation] == NULL) return TIG_ERR_INVALID_PARAM;
    TigArtFileFrameData* image = &art->hdr.frames_tbl[rotation][frame];
    const uint8_t* source = art->pixels_tbl[rotation][frame];
    if (image->width == 0 || image->height == 0) return TIG_OK;
    if (source == NULL || image->width < 0 || image->height < 0
        || (uint64_t)image->width * image->height > INT_MAX || tig_art_bits_per_pixel != 32
        || tig_video_buffer_memory_size(info->dst_video_buffer) == 0) return TIG_ERR_INVALID_PARAM;
    int palette_index = tig_art_id_palette_get(info->art_id);
    TigPalette* palette;
    if ((info->flags & TIG_ART_BLT_PALETTE_OVERRIDE) != 0) palette = info->palette;
    else {
        TigPalette** palettes = (info->flags & TIG_ART_BLT_PALETTE_ORIGINAL) != 0 ? art->hdr.palette_tbl : art->palette_tbl;
        palette = palettes[palette_index] != NULL ? palettes[palette_index] : palettes[0];
    }
    if (palette == NULL || ((info->flags & TIG_ART_BLT_BLEND_COLOR_ARRAY) != 0 && info->blend_colors == NULL))
        return TIG_ERR_INVALID_PARAM;
    int rc = tig_video_buffer_lock(info->dst_video_buffer);
    if (rc != TIG_OK) return rc;
    TigVideoBufferData destination;
    rc = tig_video_buffer_data(info->dst_video_buffer, &destination);
    if (rc != TIG_OK) {
        tig_video_buffer_unlock(info->dst_video_buffer);
        return rc;
    }
    int64_t left = SDL_max((int64_t)info->dst_rect->x, 0);
    int64_t top = SDL_max((int64_t)info->dst_rect->y, 0);
    int64_t right = SDL_min((int64_t)info->dst_rect->x + info->dst_rect->width, destination.width);
    int64_t bottom = SDL_min((int64_t)info->dst_rect->y + info->dst_rect->height, destination.height);
    if (left < right && top < bottom) {
        unsigned int flip = info->flags & (TIG_ART_BLT_FLIP_X | TIG_ART_BLT_FLIP_Y);
        if ((tig_art_id_flags_get(info->art_id) & 1) != 0) flip ^= TIG_ART_BLT_FLIP_X;
        unsigned int blend = tig_art_scaled_blend_mode(info->flags);
        TigArtScaleAxis first_x = tig_art_scale_axis(info->src_rect->width, info->dst_rect->width, left - info->dst_rect->x);
        TigArtScaleAxis axis_y = tig_art_scale_axis(info->src_rect->height, info->dst_rect->height, top - info->dst_rect->y);
        for (int y = (int)top; y < bottom; y++) {
            int64_t source_y = (int64_t)info->src_rect->y + (int64_t)SDL_min(axis_y.sample, (uint64_t)info->src_rect->height - 1);
            if (source_y >= 0 && source_y < image->height) {
                int sample_y = (flip & TIG_ART_BLT_FLIP_Y) != 0 ? image->height - 1 - (int)source_y : (int)source_y;
                const uint8_t* row = source + (size_t)sample_y * image->width;
                uint32_t* target = (uint32_t*)((uint8_t*)destination.pixels + (size_t)y * destination.pitch);
                TigArtScaleAxis axis_x = first_x;
                for (int x = (int)left; x < right; x++) {
                    int64_t source_x = (int64_t)info->src_rect->x + (int64_t)SDL_min(axis_x.sample, (uint64_t)info->src_rect->width - 1);
                    if (source_x >= 0 && source_x < image->width) {
                        int sample_x = (flip & TIG_ART_BLT_FLIP_X) != 0 ? image->width - 1 - (int)source_x : (int)source_x;
                        uint8_t index = row[sample_x];
                        if (index != 0) tig_art_scaled_pixel(info, blend, palette->colors[index], (int)source_x, (int)source_y,
                            image->width, image->height, x, y, &target[x]);
                    }
                    tig_art_scale_axis_advance(&axis_x);
                }
            }
            tig_art_scale_axis_advance(&axis_y);
        }
    }
    tig_video_buffer_unlock(info->dst_video_buffer);
    return TIG_OK;
}

static int art_blit_impl(int cache_entry_index, TigArtBlitInfo* blit_info)
{
    if ((blit_info->flags & TIG_ART_BLT_BLEND_COLOR_LERP) != 0
        && (blit_info->flags & (TIG_ART_BLT_BLEND_COLOR_CONST | TIG_ART_BLT_BLEND_COLOR_ARRAY)) == 0)
        return tig_art_blit_gradient(cache_entry_index, blit_info);

    if (blit_info->src_rect->width != blit_info->dst_rect->width
        || blit_info->src_rect->height != blit_info->dst_rect->height)
        return tig_art_blit_scaled(cache_entry_index, blit_info);

    TigVideoBufferData video_buffer_data;
    TigArtCacheEntry* art;
    TigArtFileFrameData* frm;
    TigPalette* plt;
    TigRect src_rect;
    TigRect dst_rect;
    int rc;
    int rotation;
    int frame;
    int palette;
    uint8_t* src_pixels;
    const uint8_t* source_base;
    int src_step;
    int width;
    int height;
    uint8_t* dst_pixels;
    int x;
    int y;
    float start_alpha_vertical_step;
    float end_alpha_vertical_step;
    float start_alpha_x;
    float end_alpha_x;
    float alpha_range;
    float start_alpha;
    float end_alpha;
    float current_alpha;
    float alpha_horizontal_step;
    uint32_t* mask;
    unsigned int flip;
    int src_checkerboard_cur_x;
    int src_checkerboard_cur_y;
    int dst_checkerboard_cur_x;
    int dst_checkerboard_cur_y;

    if (tig_video_buffer_memory_size(blit_info->dst_video_buffer) == 0) return TIG_ERR_INVALID_PARAM;

    rc = tig_video_buffer_lock(blit_info->dst_video_buffer);
    if (rc != TIG_OK) {
        return rc;
    }

    rc = tig_video_buffer_data(blit_info->dst_video_buffer, &video_buffer_data);
    if (rc != TIG_OK) {
        tig_video_buffer_unlock(blit_info->dst_video_buffer);
        return rc;
    }

    rotation = tig_art_id_rotation_get(blit_info->art_id);
    frame = tig_art_id_frame_get(blit_info->art_id);

    art = &(tig_art_cache_entries[cache_entry_index]);
    if (frame < 0 || frame >= art->hdr.num_frames || rotation < 0 || rotation >= MAX_ROTATIONS
        || art->hdr.frames_tbl[rotation] == NULL || art->pixels_tbl[rotation] == NULL) {
        tig_video_buffer_unlock(blit_info->dst_video_buffer);
        return TIG_ERR_INVALID_PARAM;
    }
    frm = &(art->hdr.frames_tbl[rotation][frame]);

    src_pixels = art->pixels_tbl[rotation][frame];
    width = frm->width;
    height = frm->height;
    source_base = src_pixels;
    if (width == 0 || height == 0) {
        tig_video_buffer_unlock(blit_info->dst_video_buffer);
        return TIG_OK;
    }
    if (width < 0 || height < 0 || source_base == NULL || (uint64_t)width * height > INT_MAX) {
        tig_video_buffer_unlock(blit_info->dst_video_buffer);
        return TIG_ERR_INVALID_PARAM;
    }


    if (!tig_art_clip_copy(blit_info->src_rect, blit_info->dst_rect, width, height,
            video_buffer_data.width, video_buffer_data.height, &src_rect, &dst_rect)) {
        tig_video_buffer_unlock(blit_info->dst_video_buffer);
        return TIG_OK;
    }

    if ((blit_info->flags & TIG_ART_BLT_PALETTE_OVERRIDE) != 0) {
        plt = blit_info->palette;
    } else if ((blit_info->flags & TIG_ART_BLT_PALETTE_ORIGINAL) != 0) {
        palette = tig_art_id_palette_get(blit_info->art_id);
        plt = art->hdr.palette_tbl[palette];
        if (plt == NULL) {
            plt = art->hdr.palette_tbl[0];
        }
    } else {
        palette = tig_art_id_palette_get(blit_info->art_id);
        plt = art->palette_tbl[palette];
        if (plt == NULL) {
            plt = art->palette_tbl[0];
        }
    }

    if (plt == NULL || ((blit_info->flags & TIG_ART_BLT_BLEND_COLOR_ARRAY) != 0 && blit_info->blend_colors == NULL)) {
        tig_video_buffer_unlock(blit_info->dst_video_buffer);
        return TIG_ERR_INVALID_PARAM;
    }

    src_checkerboard_cur_x = src_rect.x;
    src_checkerboard_cur_y = src_rect.y;
    dst_checkerboard_cur_x = dst_rect.x;
    dst_checkerboard_cur_y = dst_rect.y;

    flip = blit_info->flags & (TIG_ART_BLT_FLIP_X | TIG_ART_BLT_FLIP_Y);
    if ((tig_art_id_flags_get(blit_info->art_id) & 1) != 0) {
        if ((flip & TIG_ART_BLT_FLIP_X) != 0) {
            flip &= ~TIG_ART_BLT_FLIP_X;
        } else {
            flip |= TIG_ART_BLT_FLIP_X;
        }
    }

    src_step = (flip & TIG_ART_BLT_FLIP_X) != 0 ? -1 : 1;

    {
        if ((blit_info->flags & TIG_ART_BLT_BLEND_COLOR_CONST) != 0) {
            // 0x5084E9
            if ((blit_info->flags & TIG_ART_BLT_BLEND_ADD) != 0) {
                // 0x5084F5
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                uint32_t color = tig_color_mul(plt->colors[*src_pixels], blit_info->color);
                                *(uint32_t*)dst_pixels = tig_color_add(*(uint32_t*)dst_pixels, color);
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;
                        }
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_SUB) != 0) {
                // 0x508A1C
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                *(uint32_t*)dst_pixels = tig_color_sub(tig_color_mul(plt->colors[*src_pixels], blit_info->color),
                                    *(uint32_t*)dst_pixels);
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;
                        }
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_MUL) != 0) {
                // 0x508FB6
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                uint32_t color = tig_color_mul(plt->colors[*src_pixels], blit_info->color);
                                *(uint32_t*)dst_pixels = tig_color_mul(color, *(uint32_t*)dst_pixels);
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;
                        }
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_AVG) != 0) {
                // 0x50964F
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                uint32_t color = tig_color_mul(plt->colors[*src_pixels], blit_info->color);
                                *(uint32_t*)dst_pixels = tig_color_blend_alpha(color,
                                    *(uint32_t*)dst_pixels,
                                    tig_color_rgb_to_grayscale(color));
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;
                        }
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_CONST) != 0) {
                // 0x509CD6
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                *(uint32_t*)dst_pixels = tig_color_blend_alpha(tig_color_mul(plt->colors[*src_pixels], blit_info->color),
                                    *(uint32_t*)dst_pixels,
                                    blit_info->alpha[0]);
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;
                        }
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_SRC) != 0) {
                // 0x50ABD1
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                *(uint32_t*)dst_pixels = tig_color_blend_alpha(blit_info->color,
                                    *(uint32_t*)dst_pixels,
                                    tig_color_alpha(plt->colors[*src_pixels]));
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;
                        }
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_LERP_ANY) != 0) {
                // 0x50AF93
                switch (blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_LERP_ANY) {
                case TIG_ART_BLT_BLEND_ALPHA_LERP_X:
                    start_alpha_vertical_step = 0.0;
                    end_alpha_vertical_step = 0.0;
                    start_alpha_x = (float)src_rect.x;
                    end_alpha_x = (float)(width - src_rect.width - src_rect.x);
                    alpha_range = ((float)blit_info->alpha[1] - (float)blit_info->alpha[0]) / width;
                    start_alpha = (float)blit_info->alpha[0] + start_alpha_x * alpha_range;
                    end_alpha = (float)blit_info->alpha[1] - end_alpha_x * alpha_range;
                    break;
                case TIG_ART_BLT_BLEND_ALPHA_LERP_Y:
                    start_alpha_vertical_step = ((float)blit_info->alpha[3] - (float)blit_info->alpha[0]) / height;
                    end_alpha_vertical_step = start_alpha_vertical_step;
                    start_alpha = src_rect.y * start_alpha_vertical_step + (float)blit_info->alpha[0];
                    end_alpha = start_alpha;
                    break;
                default:
                    start_alpha_vertical_step = ((float)blit_info->alpha[3] - (float)blit_info->alpha[0]) / height;
                    end_alpha_vertical_step = ((float)(uint8_t)blit_info->alpha[2] - (float)blit_info->alpha[1]) / height;
                    start_alpha_x = (float)src_rect.x;
                    end_alpha_x = (float)(width - src_rect.width - src_rect.x);
                    alpha_range = ((src_rect.y * end_alpha_vertical_step + (float)blit_info->alpha[1]) - (src_rect.y * start_alpha_vertical_step + (float)blit_info->alpha[0])) / width;
                    start_alpha = (src_rect.y * start_alpha_vertical_step + (float)blit_info->alpha[0]) + start_alpha_x * alpha_range;
                    end_alpha = (src_rect.y * end_alpha_vertical_step + (float)blit_info->alpha[1]) - end_alpha_x * alpha_range;
                    break;
                }

                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        current_alpha = start_alpha;
                        alpha_horizontal_step = (end_alpha - start_alpha) / src_rect.width;

                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                *(uint32_t*)dst_pixels = tig_color_blend_alpha(tig_color_mul(plt->colors[*src_pixels], blit_info->color),
                                    *(uint32_t*)dst_pixels,
                                    (int)current_alpha);
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;

                            current_alpha += alpha_horizontal_step;
                        }

                        start_alpha += start_alpha_vertical_step;
                        end_alpha += end_alpha_vertical_step;
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_STIPPLE_S) != 0) {
                // 0x50A2A6
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        for (x = 0; x < dst_rect.width; x++) {
                            if (((src_checkerboard_cur_x ^ src_checkerboard_cur_y) & 1) != 0) {
                                if (*src_pixels != 0) {
                                    *(uint32_t*)dst_pixels = tig_color_mul(plt->colors[*src_pixels], blit_info->color);
                                }
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;

                            src_checkerboard_cur_x++;
                        }

                        src_checkerboard_cur_x = src_rect.x;
                        src_checkerboard_cur_y++;
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_STIPPLE_D) != 0) {
                // 0x50A739
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        for (x = 0; x < dst_rect.width; x++) {
                            if (((dst_checkerboard_cur_x ^ dst_checkerboard_cur_y) & 1) != 0) {
                                if (*src_pixels != 0) {
                                    *(uint32_t*)dst_pixels = tig_color_mul(plt->colors[*src_pixels], blit_info->color);
                                }
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;

                            dst_checkerboard_cur_x++;
                        }

                        dst_checkerboard_cur_x = dst_rect.x;
                        dst_checkerboard_cur_y++;
                    }
                    break;
                }
            } else {
                // 0x509647
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                *(uint32_t*)dst_pixels = tig_color_mul(plt->colors[*src_pixels], blit_info->color);
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;
                        }
                    }
                    break;
                }
            }
        } else if ((blit_info->flags & TIG_ART_BLT_BLEND_COLOR_ARRAY) != 0) {
            // 0x50BAEF
            if ((blit_info->flags & TIG_ART_BLT_BLEND_ADD) != 0) {
                // 0x50BAFB
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        mask = &(blit_info->blend_colors[src_rect.x]);

                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                uint32_t color = tig_color_mul(plt->colors[*src_pixels], *mask);
                                *(uint32_t*)dst_pixels = tig_color_add(*(uint32_t*)dst_pixels, color);
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;

                            mask++;
                        }
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_SUB) != 0) {
                // 0x50C06F
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        mask = &(blit_info->blend_colors[src_rect.x]);

                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                *(uint32_t*)dst_pixels = tig_color_sub(tig_color_mul(plt->colors[*src_pixels], *mask),
                                    *(uint32_t*)dst_pixels);
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;

                            mask++;
                        }
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_MUL) != 0) {
                // 0x50C646
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        mask = &(blit_info->blend_colors[src_rect.x]);

                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                uint32_t color = tig_color_mul(plt->colors[*src_pixels], *mask);
                                *(uint32_t*)dst_pixels = tig_color_mul(color, *(uint32_t*)dst_pixels);
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;

                            mask++;
                        }
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_AVG) != 0) {
                // 0x50CD23
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        mask = &(blit_info->blend_colors[src_rect.x]);

                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                uint32_t color = tig_color_mul(plt->colors[*src_pixels], *mask);
                                *(uint32_t*)dst_pixels = tig_color_blend_alpha(color,
                                    *(uint32_t*)dst_pixels,
                                    tig_color_rgb_to_grayscale(color));
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;

                            mask++;
                        }
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_CONST) != 0) {
                // 0x50D3F9
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        mask = &(blit_info->blend_colors[src_rect.x]);

                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                uint32_t color = tig_color_mul(plt->colors[*src_pixels], *mask);
                                *(uint32_t*)dst_pixels = tig_color_blend_alpha(color,
                                    *(uint32_t*)dst_pixels,
                                    blit_info->alpha[0]);
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;

                            mask++;
                        }
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_SRC) != 0) {
                // 0x50E3C4
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        mask = &(blit_info->blend_colors[src_rect.x]);

                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                // NOTE: This is somewhat different from
                                // surrounding code which uses seemingly inlined
                                // `tig_color_mul`.
                                *(uint32_t*)dst_pixels = tig_color_blend_alpha(*mask,
                                    *(uint32_t*)dst_pixels,
                                    tig_color_alpha(plt->colors[*src_pixels]));
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;

                            mask++;
                        }
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_LERP_ANY) != 0) {
                // 0x50E7CB
                switch (blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_LERP_ANY) {
                case TIG_ART_BLT_BLEND_ALPHA_LERP_X:
                    start_alpha_vertical_step = 0.0;
                    end_alpha_vertical_step = 0.0;
                    start_alpha_x = (float)src_rect.x;
                    end_alpha_x = (float)(width - src_rect.width - src_rect.x);
                    alpha_range = ((float)blit_info->alpha[1] - (float)blit_info->alpha[0]) / width;
                    start_alpha = (float)blit_info->alpha[0] + start_alpha_x * alpha_range;
                    end_alpha = (float)blit_info->alpha[1] - end_alpha_x * alpha_range;
                    break;
                case TIG_ART_BLT_BLEND_ALPHA_LERP_Y:
                    start_alpha_vertical_step = ((float)blit_info->alpha[3] - (float)blit_info->alpha[0]) / height;
                    end_alpha_vertical_step = start_alpha_vertical_step;
                    start_alpha = src_rect.y * start_alpha_vertical_step + (float)blit_info->alpha[0];
                    end_alpha = start_alpha;
                    break;
                default:
                    start_alpha_vertical_step = ((float)blit_info->alpha[3] - (float)blit_info->alpha[0]) / height;
                    end_alpha_vertical_step = ((float)(uint8_t)blit_info->alpha[2] - (float)blit_info->alpha[1]) / height;
                    start_alpha_x = (float)src_rect.x;
                    end_alpha_x = (float)(width - src_rect.width - src_rect.x);
                    alpha_range = ((src_rect.y * end_alpha_vertical_step + (float)blit_info->alpha[1]) - (src_rect.y * start_alpha_vertical_step + (float)blit_info->alpha[0])) / width;
                    start_alpha = (src_rect.y * start_alpha_vertical_step + (float)blit_info->alpha[0]) + start_alpha_x * alpha_range;
                    end_alpha = (src_rect.y * end_alpha_vertical_step + (float)blit_info->alpha[1]) - end_alpha_x * alpha_range;
                    break;
                }

                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        current_alpha = start_alpha;
                        alpha_horizontal_step = (end_alpha - start_alpha) / src_rect.width;
                        mask = &(blit_info->blend_colors[src_rect.x]);

                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                uint32_t color = tig_color_mul(plt->colors[*src_pixels], *mask);
                                *(uint32_t*)dst_pixels = tig_color_blend_alpha(color,
                                    *(uint32_t*)dst_pixels,
                                    (int)current_alpha);
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;

                            current_alpha += alpha_horizontal_step;
                            mask++;
                        }

                        start_alpha += start_alpha_vertical_step;
                        end_alpha += end_alpha_vertical_step;
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_STIPPLE_S) != 0) {
                // 0x50DA0F
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        mask = &(blit_info->blend_colors[src_rect.x]);

                        for (x = 0; x < dst_rect.width; x++) {
                            if (((src_checkerboard_cur_x ^ src_checkerboard_cur_y) & 1) != 0) {
                                if (*src_pixels != 0) {
                                    *(uint32_t*)dst_pixels = tig_color_mul(plt->colors[*src_pixels], *mask);
                                }
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;

                            src_checkerboard_cur_x++;
                            mask++;
                        }

                        src_checkerboard_cur_x = src_rect.x;
                        src_checkerboard_cur_y++;
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_STIPPLE_D) != 0) {
                // 0x50DEE7
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        mask = &(blit_info->blend_colors[src_rect.x]);

                        for (x = 0; x < dst_rect.width; x++) {
                            if (((dst_checkerboard_cur_x ^ dst_checkerboard_cur_y) & 1) != 0) {
                                if (*src_pixels != 0) {
                                    *(uint32_t*)dst_pixels = tig_color_mul(plt->colors[*src_pixels], *mask);
                                }
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;

                            dst_checkerboard_cur_x++;
                            mask++;
                        }

                        dst_checkerboard_cur_x = dst_rect.x;
                        dst_checkerboard_cur_y++;
                    }
                    break;
                }
            } else {
                // 0x50CD1B
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        mask = &(blit_info->blend_colors[src_rect.x]);

                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                *(uint32_t*)dst_pixels = tig_color_mul(plt->colors[*src_pixels], *mask);
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;

                            mask++;
                        }
                    }
                    break;
                }
            }
        } else {
            // 0x50655B
            if ((blit_info->flags & TIG_ART_BLT_BLEND_ADD) != 0) {
                // 0x506566
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                *(uint32_t*)dst_pixels = tig_color_add(plt->colors[*src_pixels],
                                    *(uint32_t*)dst_pixels);
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;
                        }
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_SUB) != 0) {
                // 0x506838
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                *(uint32_t*)dst_pixels = tig_color_sub(plt->colors[*src_pixels],
                                    *(uint32_t*)dst_pixels);
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;
                        }
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_MUL) != 0) {
                // 0x506B87
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                *(uint32_t*)dst_pixels = tig_color_mul(plt->colors[*src_pixels],
                                    *(uint32_t*)dst_pixels);
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;
                        }
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_AVG) != 0) {
                // 0x506FD0
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                *(uint32_t*)dst_pixels = tig_color_blend_alpha(plt->colors[*src_pixels],
                                    *(uint32_t*)dst_pixels,
                                    tig_color_rgb_to_grayscale(plt->colors[*src_pixels]));
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;
                        }
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_CONST) != 0) {
                // 0x50742A
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                *(uint32_t*)dst_pixels = tig_color_blend_alpha(plt->colors[*src_pixels],
                                    *(uint32_t*)dst_pixels,
                                    blit_info->alpha[0]);
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;
                        }
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_SRC) != 0) {
                // 0x507B0F
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                *(uint32_t*)dst_pixels = tig_color_blend_alpha(plt->colors[*src_pixels],
                                    *(uint32_t*)dst_pixels,
                                    tig_color_alpha(plt->colors[*src_pixels]));
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;
                        }
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_LERP_ANY) != 0) {
                // 0x507ECB
                switch (blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_LERP_ANY) {
                case TIG_ART_BLT_BLEND_ALPHA_LERP_X:
                    start_alpha_vertical_step = 0.0;
                    end_alpha_vertical_step = 0.0;
                    start_alpha_x = (float)src_rect.x;
                    end_alpha_x = (float)(width - src_rect.width - src_rect.x);
                    alpha_range = ((float)blit_info->alpha[1] - (float)blit_info->alpha[0]) / width;
                    start_alpha = (float)blit_info->alpha[0] + start_alpha_x * alpha_range;
                    end_alpha = (float)blit_info->alpha[1] - end_alpha_x * alpha_range;
                    break;
                case TIG_ART_BLT_BLEND_ALPHA_LERP_Y:
                    start_alpha_vertical_step = ((float)blit_info->alpha[3] - (float)blit_info->alpha[0]) / height;
                    end_alpha_vertical_step = start_alpha_vertical_step;
                    start_alpha = src_rect.y * start_alpha_vertical_step + (float)blit_info->alpha[0];
                    end_alpha = start_alpha;
                    break;
                default:
                    start_alpha_vertical_step = ((float)blit_info->alpha[3] - (float)blit_info->alpha[0]) / height;
                    end_alpha_vertical_step = ((float)(uint8_t)blit_info->alpha[2] - (float)blit_info->alpha[1]) / height;
                    start_alpha_x = (float)src_rect.x;
                    end_alpha_x = (float)(width - src_rect.width - src_rect.x);
                    alpha_range = ((src_rect.y * end_alpha_vertical_step + (float)blit_info->alpha[1]) - (src_rect.y * start_alpha_vertical_step + (float)blit_info->alpha[0])) / width;
                    start_alpha = (src_rect.y * start_alpha_vertical_step + (float)blit_info->alpha[0]) + start_alpha_x * alpha_range;
                    end_alpha = (src_rect.y * end_alpha_vertical_step + (float)blit_info->alpha[1]) - end_alpha_x * alpha_range;
                    break;
                }

                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        current_alpha = start_alpha;
                        alpha_horizontal_step = (end_alpha - start_alpha) / src_rect.width;

                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                *(uint32_t*)dst_pixels = tig_color_blend_alpha(plt->colors[*src_pixels],
                                    *(uint32_t*)dst_pixels,
                                    (int)current_alpha);
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;

                            current_alpha += alpha_horizontal_step;
                        }

                        start_alpha += start_alpha_vertical_step;
                        end_alpha += end_alpha_vertical_step;
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_STIPPLE_S) != 0) {
                // 0x50779F
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        for (x = 0; x < dst_rect.width; x++) {
                            if (((src_checkerboard_cur_x ^ src_checkerboard_cur_y) & 1) != 0) {
                                if (*src_pixels != 0) {
                                    *(uint32_t*)dst_pixels = plt->colors[*src_pixels];
                                }
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;

                            src_checkerboard_cur_x++;
                        }

                        src_checkerboard_cur_x = src_rect.x;
                        src_checkerboard_cur_y++;
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_ART_BLT_BLEND_ALPHA_STIPPLE_D) != 0) {
                // 0x507955
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        for (x = 0; x < dst_rect.width; x++) {
                            if (((dst_checkerboard_cur_x ^ dst_checkerboard_cur_y) & 1) != 0) {
                                if (*src_pixels != 0) {
                                    *(uint32_t*)dst_pixels = plt->colors[*src_pixels];
                                }
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;

                            dst_checkerboard_cur_x++;
                        }

                        dst_checkerboard_cur_x = dst_rect.x;
                        dst_checkerboard_cur_y++;
                    }
                    break;
                }
            } else {
                // 0x506FC8
                switch (tig_art_bits_per_pixel) {
                case 32:
                    for (y = 0; y < dst_rect.height; y++) {
                        tig_art_copy_row(source_base, &video_buffer_data, &src_rect, &dst_rect,
                            width, height, flip, y, &src_pixels, &dst_pixels);
                        for (x = 0; x < dst_rect.width; x++) {
                            if (*src_pixels != 0) {
                                *(uint32_t*)dst_pixels = plt->colors[*src_pixels];
                            }
                            if (x + 1 < dst_rect.width) src_pixels += src_step;
                            dst_pixels += 4;
                        }
                    }
                    break;
                }
            }
        }
    }

    tig_video_buffer_unlock(blit_info->dst_video_buffer);

    return TIG_OK;
}

// 0x51AA90
int tig_art_cache_entry_get_or_load(tig_art_id_t art_id)
{
    char path[TIG_MAX_PATH];
    int cache_entry_index;

    if (tig_art_build_path(art_id, path, sizeof(path)) != TIG_OK) {
        return -1;
    }

    if (tig_art_last_cache_entry_index < tig_art_cache_entries_length
        && strcmp(path, tig_art_cache_entries[tig_art_last_cache_entry_index].path) == 0) {
        tig_performance_count(TIG_PERF_ART_HITS, 1);
        tig_art_cache_entries[tig_art_last_cache_entry_index].time = tig_ping_timestamp;
        return tig_art_last_cache_entry_index;
    }

    // Called twice to check both system and video memory.
    tig_art_cache_check_fullness();
    tig_art_cache_check_fullness();

    if (!tig_art_cache_find(path, &cache_entry_index)) {
        tig_performance_count(TIG_PERF_ART_MISSES, 1);
        if (!tig_art_cache_entry_load(art_id, path, cache_entry_index)) {
            tig_debug_printf("ART LOAD FAILURE!!! Trying to load %s\n", path);

            if (!tig_art_cache_entry_load(art_id, "art\\badart.art", cache_entry_index)) {
                tig_debug_printf("ART LOAD FAILURE!!! Trying to load badart.art\n");
                return -1;
            }
        }
    } else {
        tig_performance_count(TIG_PERF_ART_HITS, 1);
    }

    tig_art_cache_entries[cache_entry_index].time = tig_ping_timestamp;
    tig_art_cache_entries[cache_entry_index].art_id = art_id;
    tig_art_last_cache_entry_index = cache_entry_index;

    return cache_entry_index;
}

// 0x51AC00
void tig_art_cache_set_video_memory_fullness(int fullness)
{
    tig_art_cache_video_memory_fullness = (float)fullness / 100.0f;
}

// 0x51AC20
void tig_art_cache_check_fullness(void)
{
    // Flag that denotes if we should check video memory (true) or system memory
    // (false). This flag alternates on every call.
    //
    // 0x604728
    static bool vid_vs_sys;

    art_size_t acc = 0;
    art_size_t tgt;
    int index;
    int cnt;
    Uint64 eviction_start;

    if (vid_vs_sys) {
        // NOTE: Signed compare.
        if (tig_art_available_video_memory > 0) {
            vid_vs_sys = !vid_vs_sys;
            return;
        }

        tig_debug_printf("Art cache full (vid), making some room");
        eviction_start = tig_performance_begin();

        // Calculate target size we'd like to evict.
        tgt = (art_size_t)((double)tig_art_total_video_memory * tig_art_cache_video_memory_fullness);

        // Sort cache entries by last access time, earliest first.
        qsort(tig_art_cache_entries,
            tig_art_cache_entries_length,
            sizeof(TigArtCacheEntry),
            tig_art_cache_entry_compare_time);

        // Loop thru cache entries until we reach eviction target.
        for (index = 0; index < tig_art_cache_entries_length; ++index) {
            acc += tig_art_cache_entries[index].video_memory_usage;
            if (acc >= tgt) {
                break;
            }
        }
    } else {
        // NOTE: Signed compare.
        if (tig_art_available_system_memory > 0) {
            vid_vs_sys = !vid_vs_sys;
            return;
        }

        tig_debug_printf("Art cache full (sys), making some room");
        eviction_start = tig_performance_begin();

        // Calculate target size we'd like to evict (30% of total system
        // memory).
        tgt = (art_size_t)((double)tig_art_total_system_memory * 0.3f);

        // Sort cache entries by last access time, earliest first.
        qsort(tig_art_cache_entries,
            tig_art_cache_entries_length,
            sizeof(TigArtCacheEntry),
            tig_art_cache_entry_compare_time);

        // Loop thru cache entries until we reach eviction target.
        for (index = 0; index < tig_art_cache_entries_length; ++index) {
            acc += tig_art_cache_entries[index].system_memory_usage;
            if (acc >= tgt) {
                break;
            }
        }
    }

    // NOTE: Signed compare.
    if (acc < tgt) {
        // We haven't reached eviction target, flush everything.
        tig_performance_count(TIG_PERF_ART_EVICTIONS, tig_art_cache_entries_length);
        tig_art_flush();
        tig_performance_end(TIG_PERF_ART_EVICTION, eviction_start);
        tig_debug_printf("...\n");
        return;
    }

    cnt = index + 1;
    tig_performance_count(TIG_PERF_ART_EVICTIONS, cnt);

    // Free cache entries.
    for (; index >= 0; index--) {
        tig_art_cache_entry_unload(index);
    }

    tig_art_cache_entries_length -= cnt;

    // Move remaining cache entries on top.
    memmove(tig_art_cache_entries,
        &(tig_art_cache_entries[cnt]),
        sizeof(TigArtCacheEntry) * (tig_art_cache_entries_length));

    // Sort back cache entries by name (as `tig_art_cache_find` uses binary
    // search by name).
    qsort(tig_art_cache_entries,
        tig_art_cache_entries_length,
        sizeof(TigArtCacheEntry),
        tig_art_cache_entry_compare_name);

    tig_debug_printf("...\n");
    vid_vs_sys = !vid_vs_sys;
    tig_performance_end(TIG_PERF_ART_EVICTION, eviction_start);
}

// 0x51ADE0
int tig_art_cache_entry_compare_time(const void* va, const void* vb)
{
    const TigArtCacheEntry* a = (const TigArtCacheEntry*)va;
    const TigArtCacheEntry* b = (const TigArtCacheEntry*)vb;
    return a->time - b->time;
}

// 0x51AE00
int tig_art_cache_entry_compare_name(const void* va, const void* vb)
{
    const TigArtCacheEntry* a = (const TigArtCacheEntry*)va;
    const TigArtCacheEntry* b = (const TigArtCacheEntry*)vb;
    return strcmp(a->path, b->path);
}

// 0x51AE50
int tig_art_build_path(unsigned int art_id, char* path, size_t maxlen)
{
    if (tig_art_type(art_id) == TIG_ART_TYPE_MISC) {
        switch (tig_art_num_get(art_id)) {
        case TIG_ART_SYSTEM_MOUSE:
            strcpy(path, "art\\mouse.art");
            return TIG_OK;
        case TIG_ART_SYSTEM_BUTTON:
            strcpy(path, "art\\button.art");
            return TIG_OK;
        case TIG_ART_SYSTEM_UP:
            strcpy(path, "art\\up.art");
            return TIG_OK;
        case TIG_ART_SYSTEM_DOWN:
            strcpy(path, "art\\down.art");
            return TIG_OK;
        case TIG_ART_SYSTEM_CANCEL:
            strcpy(path, "art\\cancel.art");
            return TIG_OK;
        case TIG_ART_SYSTEM_LENS:
            strcpy(path, "art\\lens.art");
            return TIG_OK;
        case TIG_ART_SYSTEM_X:
            strcpy(path, "art\\x.art");
            return TIG_OK;
        case TIG_ART_SYSTEM_PLUS:
            strcpy(path, "art\\plus.art");
            return TIG_OK;
        case TIG_ART_SYSTEM_MINUS:
            strcpy(path, "art\\minus.art");
            return TIG_OK;
        case TIG_ART_SYSTEM_BLANK:
            strcpy(path, "art\\blank.art");
            return TIG_OK;
        case TIG_ART_SYSTEM_FONT:
            strcpy(path, "art\\morph15font.art");
            return TIG_OK;
        }
    } else {
        if (tig_art_file_path_resolver != NULL) {
            return tig_art_file_path_resolver(art_id, path, maxlen);
        }
    }

    return TIG_ERR_GENERIC;
}

// 0x51B0A0
tig_art_id_t tig_art_id_reset(tig_art_id_t art_id)
{
    if (tig_art_type(art_id) == TIG_ART_TYPE_MISC) {
        art_id = tig_art_id_palette_set(art_id, 0);
        art_id = tig_art_id_frame_set(art_id, 0);
        return art_id;
    }

    if (tig_art_id_reset_func != NULL) {
        return tig_art_id_reset_func(art_id);
    }

    return art_id;
}

// 0x51B0E0
bool tig_art_cache_find(const char* path, int* index)
{
    int l = 0;
    int r = tig_art_cache_entries_length - 1;

    while (l <= r) {
        int mid = (l + r) / 2;
        int cmp = strcmp(tig_art_cache_entries[mid].path, path);
        if (cmp == 0) {
            *index = mid;
            return true;
        }

        if (cmp > 0) {
            r = mid - 1;
        } else {
            l = mid + 1;
        }
    }

    *index = l;
    return false;
}

// 0x51B170
static bool tig_art_cache_entry_load_impl(tig_art_id_t art_id, const char* path, int cache_entry_index);
bool tig_art_cache_entry_load(tig_art_id_t art_id, const char* path, int cache_entry_index)
{
    Uint64 start = tig_performance_begin();
    bool loaded = tig_art_cache_entry_load_impl(art_id, path, cache_entry_index);
    tig_performance_end_detail(TIG_PERF_ART_LOAD, start, path);
    return loaded;
}

static bool tig_art_cache_entry_load_impl(tig_art_id_t art_id, const char* path, int cache_entry_index)
{
    TigArtCacheEntry* art;
    int rc;
    art_size_t size;
    int type;
    int start;
    int num_rotations;
    int rotation;
    int index;
    int frame;
    int offset;

    if (tig_art_cache_entries_length == tig_art_cache_entries_capacity - 1) {
        tig_art_cache_entries_capacity += 32;
        tig_art_cache_entries = (TigArtCacheEntry*)REALLOC(tig_art_cache_entries,
            sizeof(TigArtCacheEntry) * tig_art_cache_entries_capacity);
    }

    memmove(&(tig_art_cache_entries[cache_entry_index + 1]),
        &(tig_art_cache_entries[cache_entry_index]),
        sizeof(TigArtCacheEntry) * (tig_art_cache_entries_length - cache_entry_index));

    art = &(tig_art_cache_entries[cache_entry_index]);

    memset(art, 0, sizeof(TigArtCacheEntry));
    strcpy(art->path, path);

    rc = tig_art_file_load(art_id,
        path,
        &(art->hdr),
        art->palette_tbl,
        0,
        &size);
    if (rc != TIG_OK) {
        memmove(&(tig_art_cache_entries[cache_entry_index]),
            &(tig_art_cache_entries[cache_entry_index + 1]),
            sizeof(TigArtCacheEntry) * (tig_art_cache_entries_length - cache_entry_index));
        return false;
    }

    art->system_memory_usage += size;

    type = tig_art_type(art_id);
    if ((art->hdr.flags & TIG_ART_SINGLE_ROTATION) != 0) {
        start = 0;
        num_rotations = 1;
    } else if (tig_art_mirroring_enabled
        && (type == TIG_ART_TYPE_CRITTER
            || type == TIG_ART_TYPE_MONSTER
            || type == TIG_ART_TYPE_UNIQUE_NPC)) {
        start = 4;
        num_rotations = 5;
    } else {
        start = 0;
        num_rotations = MAX_ROTATIONS;
    }

    for (index = 0; index < num_rotations; index++) {
        rotation = (index + start) % MAX_ROTATIONS;
        art->pixels_tbl[rotation] = (uint8_t**)MALLOC(sizeof(uint8_t*) * art->hdr.num_frames);
        art->system_memory_usage += sizeof(uint8_t*) * art->hdr.num_frames;

        offset = 0;
        for (frame = 0; frame < art->hdr.num_frames; ++frame) {
            art->pixels_tbl[rotation][frame] = art->hdr.pixels_tbl[rotation] + offset;
            offset += art->hdr.frames_tbl[rotation][frame].width * art->hdr.frames_tbl[rotation][frame].height;
        }
    }

    if (MAX_ROTATIONS - num_rotations > 0) {
        rotation = num_rotations + start;
        for (index = MAX_ROTATIONS - num_rotations; index > 0; --index) {
            art->pixels_tbl[rotation % MAX_ROTATIONS] = art->pixels_tbl[0];
            rotation++;
        }
    }

    tig_art_cache_entries_length++;
    tig_art_available_system_memory -= art->system_memory_usage;

    return true;
}

// 0x51B490
void tig_art_cache_entry_unload(int cache_entry_index)
{
    TigArtCacheEntry* cache_entry;
    int type;
    int rotation_start;
    int num_rotations;
    int rotation;
    int palette;
    int idx;

    cache_entry = &(tig_art_cache_entries[cache_entry_index]);

    tig_art_available_system_memory += cache_entry->system_memory_usage;
    tig_art_available_video_memory += cache_entry->video_memory_usage;

    tig_art_cache_video_buffers_free(cache_entry_index);

    type = tig_art_type(cache_entry->art_id);

    if ((cache_entry->hdr.flags & TIG_ART_SINGLE_ROTATION) != 0) {
        rotation_start = 0;
        num_rotations = 1;
    } else if (tig_art_mirroring_enabled
        && (type == TIG_ART_TYPE_CRITTER
            || type == TIG_ART_TYPE_MONSTER
            || type == TIG_ART_TYPE_UNIQUE_NPC)) {
        rotation_start = 4;
        num_rotations = 5;
    } else {
        rotation_start = 0;
        num_rotations = MAX_ROTATIONS;
    }

    for (idx = 0; idx < num_rotations; ++idx) {
        rotation = (idx + rotation_start) % MAX_ROTATIONS;
        FREE(cache_entry->pixels_tbl[rotation]);
        FREE(cache_entry->hdr.pixels_tbl[rotation]);
        FREE(cache_entry->hdr.frames_tbl[rotation]);
    }

    for (palette = 0; palette < MAX_PALETTES; ++palette) {
        if (cache_entry->hdr.palette_tbl[palette] != NULL) {
            tig_palette_destroy(cache_entry->hdr.palette_tbl[palette]);
        }

        if (cache_entry->palette_tbl[palette] != NULL) {
            tig_palette_destroy(cache_entry->palette_tbl[palette]);
        }
    }
}

void tig_art_cache_statistics(TigArtCacheStatistics* statistics)
{
    if (statistics == NULL) return;
    memset(statistics, 0, sizeof(*statistics));
    if (!tig_art_initialized) return;
    statistics->entries = tig_art_cache_entries_length;
    for (int index = 0; index < tig_art_cache_entries_length; index++) {
        TigArtCacheEntry* art = &tig_art_cache_entries[index];
        statistics->system_bytes += art->system_memory_usage;
        statistics->video_bytes += art->video_memory_usage;
        int frames = tig_art_type(art->art_id) == TIG_ART_TYPE_ROOF ? 13 : art->hdr.num_frames;
        for (int palette = 0; palette < MAX_PALETTES; palette++) for (int rotation = 0; rotation < MAX_ROTATIONS; rotation++) {
            TigVideoBuffer** buffers = art->video_buffers[palette][rotation];
            if (buffers == NULL) continue;
            statistics->video_tables++;
            for (int frame = 0; frame < frames; frame++) {
                if (buffers[frame] != NULL) statistics->video_buffers++;
            }
        }
    }
}

// 0x51B610
void art_invalidate(int cache_entry_index)
{
    int palette;
    int rotation;

    // Mark all palette and rotation variants as dirty so that the subsequent
    // call to `art_get_video_buffer` updates appropriate video buffer.
    for (palette = 0; palette < MAX_PALETTES; palette++) {
        for (rotation = 0; rotation < MAX_ROTATIONS; rotation++) {
            tig_art_cache_entries[cache_entry_index].dirty[palette][rotation] = 1;
        }
    }
}

// 0x51B650
void tig_art_cache_video_buffers_free(int cache_entry_index)
{
    TigArtCacheEntry* art;
    int num_frames;
    int palette;
    int rotation;
    int frame;

    art = &(tig_art_cache_entries[cache_entry_index]);

    if (tig_art_type(art->art_id) == TIG_ART_TYPE_ROOF) {
        num_frames = 13;
    } else {
        num_frames = art->hdr.num_frames;
    }

    for (palette = 0; palette < MAX_PALETTES; palette++) {
        for (rotation = 0; rotation < MAX_ROTATIONS; rotation++) {
            if (art->video_buffers[palette][rotation] != NULL) {
                for (frame = 0; frame < num_frames; frame++) {
                    tig_video_buffer_destroy(art->video_buffers[palette][rotation][frame]);
                }

                FREE(art->video_buffers[palette][rotation]);
                art->video_buffers[palette][rotation] = NULL;
            }
        }
    }
}

// 0x51B710
int tig_art_file_load(tig_art_id_t art_id, const char* filename, TigArtHeader* hdr, TigPalette** palette_tbl, int load_selected_palette_only, art_size_t* size_ptr)
{
    ArtInput input = { 0 };
    ArtInput* stream = &input;
    int rotation;
    int palette;
    bool palette_presence[MAX_PALETTES];
    uint32_t temp_palette_entries[256];
    art_size_t size_tbl[MAX_ROTATIONS];
    int index;
    int frame;
    int current_palette_index;
    void* current_palette;
    int num_rotations;
    uint8_t* encoded = NULL;
    size_t encoded_capacity = 0;

    // NOTE: Keep compiler happy.
    current_palette_index = 0;
    current_palette = NULL;

    *size_ptr = 0;

    for (rotation = 0; rotation < MAX_ROTATIONS; rotation++) {
        hdr->frames_tbl[rotation] = NULL;
        hdr->pixels_tbl[rotation] = NULL;
    }

    Uint64 open_start = tig_performance_begin();
    bool opened = art_input_open(stream, filename);
    tig_performance_end_detail(TIG_PERF_ART_FILE_OPEN, open_start, filename);
    if (!opened) {
        return TIG_ERR_GENERIC;
    }

    Uint64 metadata_start = tig_performance_begin();
    if (!art_read_header(hdr, palette_presence, stream)) {
        art_input_close(stream);
        return TIG_ERR_GENERIC;
    }

    // Only 8-bpp palette-indexed ART files are supported.
    if (hdr->bpp != 8) {
        art_input_close(stream);
        return TIG_ERR_GENERIC;
    }

    if (load_selected_palette_only) {
        current_palette_index = tig_art_id_palette_get(art_id);
        if (palette_presence[current_palette_index]) {
            art_input_close(stream);
            return TIG_ERR_GENERIC;
        }
        current_palette = palette_tbl[0];
    }

    for (palette = 0; palette < MAX_PALETTES; palette++) {
        hdr->palette_tbl[palette] = NULL;
        palette_tbl[palette] = NULL;
    }

    for (palette = 0; palette < MAX_PALETTES; palette++) {
        if (palette_presence[palette]) {
            if (art_input_read(temp_palette_entries, sizeof(uint32_t), 256, stream) != 256) {
                art_load_failure_cleanup(stream, hdr, palette_tbl);
                return TIG_ERR_GENERIC;
            }

            if (load_selected_palette_only) {
                if (palette == current_palette_index) {
                    for (index = 0; index < 256; index++) {
                        ((uint32_t*)current_palette)[index] = temp_palette_entries[index];
                    }

                    art_input_close(stream);
                    return TIG_OK;
                }
            } else {
                hdr->palette_tbl[palette] = tig_palette_create();
                palette_tbl[palette] = tig_palette_create();
                *size_ptr += (art_size_t)tig_palette_system_memory_size() * 2;

                for (index = 0; index < 256; index++) {
                    hdr->palette_tbl[palette]->colors[index] = tig_color_index_of(temp_palette_entries[index]);
                }
            }

            tig_art_palette_adjust_or_copy(art_id, hdr->palette_tbl[palette], palette_tbl[palette]);
        }
    }

    num_rotations = art_header_rotation_count(hdr);
    int file_size = art_input_length(stream);
    if (file_size < 0 || hdr->num_frames <= 0
        || (uint64_t)hdr->num_frames * num_rotations * sizeof(TigArtFileFrameData) > (uint64_t)file_size) {
        art_load_failure_cleanup(stream, hdr, palette_tbl);
        return TIG_ERR_GENERIC;
    }

    for (rotation = 0; rotation < num_rotations; rotation++) {
        hdr->frames_tbl[rotation] = (TigArtFileFrameData*)MALLOC(sizeof(TigArtFileFrameData) * hdr->num_frames);
        *size_ptr += sizeof(TigArtFileFrameData) * hdr->num_frames;

        if (art_input_read(hdr->frames_tbl[rotation], sizeof(TigArtFileFrameData), hdr->num_frames, stream) != (size_t)hdr->num_frames) {
            art_load_failure_cleanup(stream, hdr, palette_tbl);
            return TIG_ERR_GENERIC;
        }
    }

    tig_performance_end_detail(TIG_PERF_ART_METADATA, metadata_start, filename);
    Uint64 pixels_start = tig_performance_begin();

    // Bound all output sizes and reuse one encoded-frame buffer for this load.
    // Small per-packet reads are especially costly through compressed DAT streams.
    for (rotation = 0; rotation < num_rotations; rotation++) {
        int64_t total = 0;
        for (frame = 0; frame < hdr->num_frames; frame++) {
            const TigArtFileFrameData* data = &hdr->frames_tbl[rotation][frame];
            int64_t pixels = (int64_t)data->width * data->height;
            if (data->width < 0 || data->height < 0 || pixels > INT_MAX
                || data->data_size < 0 || data->data_size > file_size || total + pixels > INT_MAX) {
                art_load_failure_cleanup(stream, hdr, palette_tbl);
                return TIG_ERR_GENERIC;
            }
            total += pixels;
            if (data->data_size != pixels && (size_t)data->data_size > encoded_capacity)
                encoded_capacity = data->data_size;
        }
    }
    if (encoded_capacity != 0) encoded = MALLOC(encoded_capacity);

    for (index = 0; index < num_rotations; index++) {
        uint8_t* bytes;
        art_size_t total_size;

        // Calculate total size of pixels data.
        total_size = 0;
        for (frame = 0; frame < hdr->num_frames; ++frame) {
            total_size += hdr->frames_tbl[index][frame].width * hdr->frames_tbl[index][frame].height;
        }

        // Allocate appropriate memory.
        hdr->pixels_tbl[index] = (uint8_t*)MALLOC(total_size);

        // Store size for later use.
        size_tbl[index] = total_size;
        *size_ptr += total_size;

        // Read raw pixel data for each frame.
        bytes = hdr->pixels_tbl[index];
        for (frame = 0; frame < hdr->num_frames; ++frame) {
            if (hdr->frames_tbl[index][frame].data_size == hdr->frames_tbl[index][frame].width * hdr->frames_tbl[index][frame].height) {
                // Pixels are not compressed, read everything in one go.
                if (art_input_read(bytes, 1, hdr->frames_tbl[index][frame].data_size, stream) != (size_t)hdr->frames_tbl[index][frame].data_size) {
                    FREE(encoded);
                    art_load_failure_cleanup(stream, hdr, palette_tbl);
                    return TIG_ERR_GENERIC;
                }
                bytes += hdr->frames_tbl[index][frame].data_size;
            } else if (hdr->frames_tbl[index][frame].data_size > 0) {
                // One stream read per compressed frame; decode packets in RAM.
                size_t compressed_size = hdr->frames_tbl[index][frame].data_size;
                size_t pixel_size = (size_t)hdr->frames_tbl[index][frame].width * hdr->frames_tbl[index][frame].height;
                if (art_input_read(encoded, 1, compressed_size, stream) != compressed_size
                    || !tig_art_decode_rle(encoded, compressed_size, bytes, pixel_size)) {
                    FREE(encoded);
                    art_load_failure_cleanup(stream, hdr, palette_tbl);
                    return TIG_ERR_GENERIC;
                }
                bytes += pixel_size;
            }
        }
    }

    FREE(encoded);
    tig_performance_end_detail(TIG_PERF_ART_PIXELS, pixels_start, filename);

    while (index < MAX_ROTATIONS) {
        hdr->frames_tbl[index] = hdr->frames_tbl[0];
        hdr->pixels_tbl[index] = hdr->pixels_tbl[0];
        index++;
    }

    if (num_rotations > 1) {
        int type = tig_art_type(art_id);
        if (tig_art_mirroring_enabled) {
            if (type == TIG_ART_TYPE_CRITTER
                || type == TIG_ART_TYPE_MONSTER
                || type == TIG_ART_TYPE_UNIQUE_NPC) {
                for (index = 0; index < 3; index++) {
                    *size_ptr -= size_tbl[1 + index];

                    FREE(hdr->pixels_tbl[1 + index]);
                    hdr->pixels_tbl[1 + index] = hdr->pixels_tbl[0];

                    FREE(hdr->frames_tbl[1 + index]);
                    hdr->frames_tbl[1 + index] = hdr->frames_tbl[0];
                }
            }
        }
    }

    art_input_close(stream);
    return TIG_OK;
}

// Returns one or eight stored rotations according to the header flag.
// 0x51BE30
int art_header_rotation_count(TigArtHeader* hdr)
{
    return (hdr->flags & TIG_ART_SINGLE_ROTATION) != 0 ? 1 : MAX_ROTATIONS;
}

// Closes the stream and frees frame, pixel and palette allocations after loading failure.
// 0x51BE50
void art_load_failure_cleanup(ArtInput* stream, TigArtHeader* hdr, TigPalette** palette_tbl)
{
    int palette;

    if (stream != NULL) {
        art_input_close(stream);
    }

    art_header_frame_data_free(hdr);

    for (palette = 0; palette < MAX_PALETTES; ++palette) {
        if (hdr->palette_tbl[palette] != NULL) {
            tig_palette_destroy(hdr->palette_tbl[palette]);
        }

        if (palette_tbl[palette] != NULL) {
            tig_palette_destroy(palette_tbl[palette]);
        }
    }
}

// Frees per-rotation frame and pixel arrays without clearing their pointers.
// 0x51BF20
void art_header_frame_data_free(TigArtHeader* hdr)
{
    int num_rotations;
    int rotation;

    num_rotations = art_header_rotation_count(hdr);
    for (rotation = 0; rotation < num_rotations; rotation++) {
        if (hdr->pixels_tbl[rotation] != NULL) {
            FREE(hdr->pixels_tbl[rotation]);
        }
        if (hdr->frames_tbl[rotation] != NULL) {
            FREE(hdr->frames_tbl[rotation]);
        }
    }
}

bool art_read_header(TigArtHeader* hdr, bool* palette_presence, ArtInput* stream)
{
    TigArtFileHeader file_header;
    if (art_input_read(&file_header, sizeof(file_header), 1, stream) != 1) return false;

    hdr->flags = file_header.flags;
    hdr->fps = file_header.fps;
    hdr->bpp = file_header.bpp;
    hdr->action_frame = file_header.action_frame;
    hdr->num_frames = file_header.num_frames;

    // Read palette table, non-zero value indicates presence of palette entries.
    for (int index = 0; index < MAX_PALETTES; index++) {
        palette_presence[index] = file_header.palette_presence[index] != 0;
        hdr->palette_tbl[index] = NULL;
    }
    for (int rotation = 0; rotation < MAX_ROTATIONS; rotation++) {
        hdr->data_size[rotation] = file_header.data_size[rotation];
    }
    return true;
}
