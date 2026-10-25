#include "game/terrain.h"

#include <stdio.h>

#include "game/mes.h"
#include "game/random.h"
#include "game/tile.h"
#include "game/terrain_encoding.h"
#include "game/terrain_storage.h"

#define FIRST_TERRAIN_COLOR_ID 100
#define FIRST_TERRAIN_TRANSITION_ID 200

#define TERRAIN_TYPE_MAX 32

typedef struct TerrainHeader {
    /* 0000 */ float version;
    /* 0004 */ unsigned int flags;
    /* 0008 */ int64_t width;
    /* 0010 */ int64_t height;
    /* 0018 */ int base_terrain_type;
    /* 001C */ int padding_1C;
} TerrainHeader;

// Serializeable.
static_assert(sizeof(TerrainHeader) == 0x20, "wrong size");

static void terrain_editor_bitmap_render_stub(int bitmap_x, int bitmap_y, TigVideoBuffer* vb, TigRect* rect);
static void terrain_sector_tile_set(int64_t sec, uint16_t tid, void (*callback)(uint64_t sec));
static const char* terrain_base_name(int terrain_type);
static int terrain_match_base_name(const char* base_name);
static void terrain_color(int terrain_type, int* red, int* green, int* blue);
static void terrain_encode_to_pointer(int primary_type, int secondary_type, int transition_mask, int variant, uint16_t* value);
static int terrain_transition_variant_randomize(int primary_type, int secondary_type, int transition_mask);
static int64_t terrain_variant_sector_id(int variant);
static bool terrain_init_types(void);
static bool terrain_types_and_variants_scan(void);
static bool terrain_init_transitions(void);
static bool terrain_parse_transition(char* str, int* from_terrain_type, int* to_terrain_type);
static bool terrain_transition_first_step_find(int from, int to, bool* transitions, int* next_type_ptr);
static bool terrain_transition_path_search(int* path, int path_depth, int destination_type, bool* transitions);
static uint16_t terrain_transition_base_tile_get(int from, int to);
static bool terrain_transition_uses_endpoint(int from, int to);
static int terrain_compressed_row_cache_load(int64_t row_y);
static void terrain_compressed_row_decode(void* dst, uint16_t* src);
static bool terrain_row_inflate(void* dst, void* src, int size);
static uint16_t* terrain_compressed_row_pointer_get(int64_t row_y);
static bool terrain_compressed_rows_read(TigFile* stream);
static bool terrain_compressed_rows_write(TigFile* stream);

// 0x5B9968
static int64_t terrain_transition_variant_sector_ids[] = {
    -1,
    1,
    (2 << 26) | 1,
    (1 << 26) | 1,
    (2 << 26) | 3,
    0,
    (2 << 26) | 2,
    -1,
    3,
    2,
    -1,
    -1,
    (1 << 26) | 3,
    -1,
    -1,
    -1,
};

// 0x5B99E8
static int terrain_unused_direction_masks[] = {
    1,
    3,
    2,
    6,
    4,
    12,
    8,
    9,
};

// 0x603748
static int terrain_variant_counts[TERRAIN_TYPE_MAX];

// 0x6037C8
static char terrain_save_path[TIG_MAX_PATH];

// 0x6038CC
static int terrain_row_cache_next_slot;

// 0x6038D0
static int16_t* terrain_row_cache[4];

// 0x6038E0
static bool terrain_editor;

// 0x6038E4
static char terrain_base_path[TIG_MAX_PATH];

// 0x6039E8
static int* terrain_transition_next_steps;

// 0x6039EC
static uint16_t* terrain_tile_data;

// 0x6039F0
static TerrainHeader terrain_header;

// 0x603A10
static bool terrain_modified;

// 0x603A14
static int terrain_type_count;

// 0x603A18
static int terrain_tile_data_size;

// 0x603A1C
static mes_file_handle_t terrain_mes_file;

// 0x603A20
static int terrain_type_blocked_flags[TERRAIN_TYPE_MAX];

// 0x603AA0
static int64_t terrain_row_cache_y_coordinates[4];

// 0x4E7B00
bool terrain_init(GameInitInfo* init_info)
{
    if (!mes_load("terrain\\terrain.mes", &terrain_mes_file)) {
        return false;
    }

    if (!terrain_init_types()) {
        return false;
    }

    if (!terrain_types_and_variants_scan()) {
        return false;
    }

    if (!terrain_init_transitions()) {
        return false;
    }

    terrain_editor = init_info->editor;

    return true;
}

// 0x4E7B50
void terrain_reset(void)
{
}

// 0x4E7B60
void terrain_exit(void)
{
    if (terrain_transition_next_steps != NULL) {
        FREE(terrain_transition_next_steps);
        terrain_transition_next_steps = NULL;
    }

    mes_unload(terrain_mes_file);
}

// 0x4E7B90
bool terrain_map_new(MapNewInfo* new_map_info)
{
    int index;

    terrain_map_close();

    terrain_header.version = 1.2f;
    terrain_header.flags = 0;
    terrain_header.width = new_map_info->width;
    terrain_header.height = new_map_info->height;
    terrain_header.base_terrain_type = new_map_info->base_terrain_type;

    if (!terrain_storage_size(terrain_header.width, terrain_header.height, &terrain_tile_data_size)) {
        terrain_map_close();
        return false;
    }
    terrain_tile_data = (uint16_t*)MALLOC(terrain_tile_data_size);
    if (terrain_tile_data == NULL) {
        terrain_map_close();
        return false;
    }

    for (index = 0; index < terrain_header.height * terrain_header.width; index++) {
        terrain_tile_data[index] = terrain_transition_variant_randomize(new_map_info->base_terrain_type, new_map_info->base_terrain_type, 15);
    }

    snprintf(terrain_base_path, sizeof(terrain_base_path), "%s\\terrain.tdf", new_map_info->base_path);

    if (terrain_editor) {
        snprintf(terrain_save_path, sizeof(terrain_save_path), "%s\\terrain.tdf", new_map_info->base_path);
    }

    terrain_modified = true;

    return terrain_flush();
}

// 0x4E7CB0
bool terrain_open(const char* base_path, const char* save_path)
{
    TigFile* stream;
    bool header_loaded = false;
    int expanded_size;
    int file_size;

    terrain_map_close();

    snprintf(terrain_base_path, sizeof(terrain_base_path), "%s\\terrain.tdf", base_path);

    if (terrain_editor) {
        snprintf(terrain_save_path, sizeof(terrain_save_path), "%s\\terrain.tdf", save_path);
    }

    stream = tig_file_fopen(terrain_save_path, "rb");
    if (stream == NULL) {
        stream = tig_file_fopen(terrain_base_path, "rb");
        if (stream == NULL) {
            tig_debug_printf("Terrain file doesn't exist [%s]\n", terrain_base_path);
            return false;
        }
    }

    if (tig_file_fread(&terrain_header, sizeof(terrain_header), 1, stream) != 1) {
        tig_debug_println("Unable to read terrain header");
        tig_file_fclose(stream);
        terrain_map_close();
        return false;
    }

    if (terrain_header.version != 1.2f) {
        tig_debug_println("Wrong terrain header version");
        tig_file_fclose(stream);
        terrain_map_close();
        return false;
    }

    file_size = tig_file_filelength(stream);
    if (!terrain_storage_size(terrain_header.width, terrain_header.height, &expanded_size)
        || file_size < (int)sizeof(terrain_header)
        || ((terrain_header.flags & 1) == 0 && file_size - (int)sizeof(terrain_header) < expanded_size)) {
        tig_file_fclose(stream);
        terrain_map_close();
        return false;
    }
    terrain_tile_data_size = file_size - (int)sizeof(terrain_header);

    if ((terrain_header.flags & 0x1) != 0) {
        if (terrain_editor) {
            header_loaded = true;
            terrain_tile_data_size = expanded_size;
        } else {
            int index;

            for (index = 0; index < 4; index++) {
                terrain_row_cache[index] = (int16_t*)MALLOC(sizeof(int16_t) * (size_t)terrain_header.width);
                if (terrain_row_cache[index] == NULL) {
                    tig_file_fclose(stream);
                    terrain_map_close();
                    return false;
                }
                terrain_row_cache_y_coordinates[index] = -1;
            }

            terrain_row_cache_next_slot = 0;
        }
    }

    terrain_tile_data = (uint16_t*)MALLOC(terrain_tile_data_size);
    if (terrain_tile_data == NULL) {
        tig_file_fclose(stream);
        terrain_map_close();
        return false;
    }

    if (header_loaded) {
        if (!terrain_compressed_rows_read(stream)) {
            tig_debug_println("Unable to decompress terrain data");
            tig_file_fclose(stream);
            terrain_map_close();
            return false;
        }

        terrain_header.flags &= ~0x1;
    } else {
        if (tig_file_fread(terrain_tile_data, terrain_tile_data_size, 1, stream) != 1) {
            tig_debug_println("Unable to read terrain data");
            tig_file_fclose(stream);
            terrain_map_close();
            return false;
        }
    }

    tig_file_fclose(stream);
    if ((terrain_header.flags & 1) != 0
        && !terrain_storage_rows_valid(terrain_tile_data, terrain_tile_data_size, terrain_header.height)) {
        terrain_map_close();
        return false;
    }

    return true;
}

// 0x4E7E80
void terrain_map_close(void)
{
    int index;

    for (index = 0; index < 4; index++) {
        if (terrain_row_cache[index] != NULL) {
            FREE(terrain_row_cache[index]);
            terrain_row_cache[index] = NULL;
        }
        terrain_row_cache_y_coordinates[index] = -1;
    }

    if (terrain_tile_data != NULL) {
        FREE(terrain_tile_data);
        terrain_tile_data = NULL;
    }

    memset(&terrain_header, 0, sizeof(terrain_header));
    terrain_base_path[0] = '\0';
    terrain_save_path[0] = '\0';
    terrain_modified = false;
}

// 0x4E7EF0
void terrain_reopen_current(void)
{
    char drive[COMPAT_MAX_DRIVE];
    char dir[COMPAT_MAX_DIR];
    char path1[TIG_MAX_PATH];
    char path2[TIG_MAX_PATH];

    compat_splitpath(terrain_base_path, drive, dir, NULL, NULL);
    snprintf(path1, sizeof(path1), "%s%s", drive, dir);
    compat_splitpath(terrain_save_path, drive, dir, NULL, NULL);
    snprintf(path2, sizeof(path2), "%s%s", drive, dir);
    terrain_map_close();
    terrain_open(path1, path2);
}

// 0x4E7F90
bool terrain_editor_bitmap_export(void)
{
    TigVideoBufferCreateInfo vb_create_info;
    TigVideoBuffer* vb;
    TigRect rect;
    TigVideoBufferSaveToBmpInfo vb_to_bmp_info;
    char drive[COMPAT_MAX_DRIVE];
    char dir[COMPAT_MAX_DIR];
    char fname[COMPAT_MAX_FNAME];

    if (!terrain_editor) {
        return false;
    }

    if ((terrain_header.flags & 0x1) != 0) {
        return false;
    }

    vb_create_info.flags = TIG_VIDEO_BUFFER_CREATE_SYSTEM_MEMORY;
    vb_create_info.width = (int)terrain_header.width; // TODO: Check casts.
    vb_create_info.height = (int)terrain_header.height;
    vb_create_info.background_color = 0;
    if (tig_video_buffer_create(&vb_create_info, &vb) != TIG_OK) {
        return false;
    }

    rect.x = 0;
    rect.y = 0;
    rect.width = (int)terrain_header.width; // TODO: Check casts.
    rect.height = (int)terrain_header.height;
    terrain_editor_bitmap_render_stub(0, 0, vb, &rect);

    vb_to_bmp_info.flags = 0;
    vb_to_bmp_info.video_buffer = vb;
    compat_splitpath(terrain_base_path, drive, dir, fname, NULL);
    // NOTE: Original is slightly different and looks wrong.
    compat_makepath(vb_to_bmp_info.path, drive, dir, fname, "bmp");
    vb_to_bmp_info.rect = NULL;
    if (tig_video_buffer_save_to_bmp(&vb_to_bmp_info) != TIG_OK) {
        // FIXME: Leaking `vb`.
        return false;
    }

    tig_video_buffer_destroy(vb);

    return true;
}

// 0x4E80C0
void terrain_editor_bitmap_render_stub(int bitmap_x, int bitmap_y, TigVideoBuffer* vb, TigRect* rect)
{
    // TODO: Incomplete.
}

// 0x4E84C0
void terrain_sector_path(int64_t sector_id, char* path)
{
    uint16_t tid;
    int base;
    int secondary_type;
    int transition_mask;
    int variant;
    int64_t variant_sector_id;
    const char* to;
    const char* from;

    tid = terrain_sector_tile_get(sector_id);
    base = terrain_primary_type_get(tid);

    if (base < 0 || base >= terrain_type_count) {
        tig_debug_printf("Error: Invalid base in terrain_sector_path\n  tid: %d  base: %d\n", tid, base);
    }

    secondary_type = terrain_secondary_type_get(tid);
    transition_mask = terrain_transition_mask_get(tid);
    if (base == secondary_type || transition_mask == 0) {
        variant = terrain_variant_get(tid);
        variant_sector_id = terrain_variant_sector_id(variant);

        to = terrain_base_name(base);
        if (to == NULL) {
            tig_debug_printf("Error: terrain_base_name failed in terrain_sector_path\n  tid: %d  base: $d\n", tid);
        }

        strcpy(path, "terrain\\");
        strcat(path, to);
    } else {
        variant_sector_id = terrain_transition_variant_sector_ids[transition_mask];
        if (variant_sector_id != -1) {
            from = terrain_base_name(base);
            to = terrain_base_name(secondary_type);
        } else {
            from = terrain_base_name(secondary_type);
            to = terrain_base_name(base);
            variant_sector_id = terrain_transition_variant_sector_ids[15 - transition_mask];
        }

        strcpy(path, "terrain\\");
        strcat(path, from);
        strcat(path, " to ");
        strcat(path, to);
    }

    strcat(path, "\\");
    SDL_lltoa(variant_sector_id, &(path[strlen(path)]), 10);
    strcat(path, ".sec");
}

// 0x4E86F0
void terrain_fill(Sector* sector)
{
    tig_art_id_t art_id;
    int index;

    tig_art_tile_id_create(7, 7, 15, 0, 0, 0, 0, 0, &art_id);

    for (index = 0; index < 4096; index++) {
        sector->tiles.art_ids[index] = tile_transition_art_id_create_random(art_id, 7, 0, 15);
    }
}

// 0x4E8740
bool terrain_flush(void)
{
    TigFile* stream;
    TerrainHeader hdr;

    if (!terrain_editor) {
        return false;
    }

    if (terrain_modified) {
        stream = tig_file_fopen(terrain_save_path, "wb");
        if (stream == NULL) {
            return false;
        }

        hdr = terrain_header;
        hdr.flags = terrain_header.flags | 0x1;
        if (tig_file_fwrite(&hdr, sizeof(hdr), 1, stream) != 1) {
            // FIXME: Leaks `stream`.
            return false;
        }

        if (!terrain_compressed_rows_write(stream)) {
            // FIXME: Leaks `stream`.
            return false;
        }

        terrain_modified = false;
        tig_file_fclose(stream);
    }

    return true;
}

// 0x4E87F0
uint16_t terrain_sector_tile_get(int64_t sec)
{
    int idx;
    int64_t x;
    int64_t y;

    x = SECTOR_X(sec);
    y = SECTOR_Y(sec);
    if (x < 0
        || x >= terrain_header.width
        || y < 0
        || y >= terrain_header.height) {
        return -1;
    }

    if ((terrain_header.flags & 0x1) == 0) {
        return terrain_tile_data[x + y * terrain_header.width];
    }

    for (idx = 0; idx < 4; idx++) {
        if (terrain_row_cache_y_coordinates[idx] == y) {
            break;
        }
    }

    if (idx == 4) {
        idx = terrain_compressed_row_cache_load(y);
    }

    return terrain_row_cache[idx][x];
}

// 0x4E88C0
void terrain_sector_tile_set(int64_t sec, uint16_t tid, void (*callback)(uint64_t sec))
{
    int x;
    int y;

    x = (int)SECTOR_X(sec);
    y = (int)SECTOR_Y(sec);

    terrain_tile_data[terrain_header.width * y + x] = tid;

    if (callback != NULL) {
        callback(sec);
    }

    terrain_modified = true;
}

// 0x4E8B20
const char* terrain_base_name(int terrain_type)
{
    MesFileEntry mes_file_entry;

    if (terrain_type >= 0 && terrain_type < terrain_type_count) {
        mes_file_entry.num = terrain_type;

        if (mes_search(terrain_mes_file, &mes_file_entry)) {
            return mes_file_entry.str;
        }
    }

    return NULL;
}

// 0x4E8B60
int terrain_match_base_name(const char* base_name)
{
    MesFileEntry mes_file_entry;

    mes_file_entry.num = 0;
    if (mes_search(terrain_mes_file, &mes_file_entry)) {
        do {
            if (SDL_strcasecmp(base_name, mes_file_entry.str) == 0) {
                return mes_file_entry.num;
            }
        } while (mes_find_next(terrain_mes_file, &mes_file_entry));
    }

    return -1;
}

// 0x4E8BE0
void terrain_color(int terrain_type, int* red, int* green, int* blue)
{
    MesFileEntry mes_file_entry;
    char buffer[MAX_STRING];
    char* tok;

    *red = 255;
    *green = 0;
    *blue = 0;

    if (terrain_type >= 0 && terrain_type < terrain_type_count) {
        mes_file_entry.num = terrain_type + FIRST_TERRAIN_COLOR_ID;
        if (mes_search(terrain_mes_file, &mes_file_entry)) {
            strcpy(buffer, mes_file_entry.str);

            tok = strtok(buffer, ",");
            if (tok != NULL) {
                *red = atoi(tok);

                tok = strtok(NULL, ",");
                if (tok != NULL) {
                    *green = atoi(tok);

                    tok = strtok(NULL, ",");
                    if (tok != NULL) {
                        *blue = atoi(tok);
                    }
                }
            }
        }
    }
}

// 0x4E8CE0
void terrain_encode_to_pointer(int primary_type, int secondary_type, int transition_mask, int variant, uint16_t* value)
{
    *value = terrain_encode(primary_type, secondary_type, transition_mask, variant);
}

// 0x4E8D60
int terrain_transition_variant_randomize(int primary_type, int secondary_type, int transition_mask)
{
    uint16_t value;
    int variant = 0;

    // A full or empty transition mask selects a single terrain type.
    if (transition_mask == 0) {
        primary_type = secondary_type;
    } else if (transition_mask == 15) {
        secondary_type = primary_type;
    }
    if (primary_type == secondary_type) {
        transition_mask = 0;
        if (primary_type >= 0 && primary_type < TERRAIN_TYPE_MAX && terrain_variant_counts[primary_type] > 1) {
            variant = random_between(0, terrain_variant_counts[primary_type] - 1);
        }
    }
    terrain_encode_to_pointer(primary_type, secondary_type, transition_mask, variant, &value);
    return value;
}

// 0x4E8DC0
int terrain_primary_type_get(uint16_t tile_data)
{
    return (tile_data >> 11) & 0x1F;
}

// 0x4E8DD0
int terrain_secondary_type_get(uint16_t tile_data)
{
    return (tile_data >> 6) & 0x1F;
}

// 0x4E8DE0
int terrain_transition_mask_get(uint16_t tile_data)
{
    return (tile_data >> 2) & 0xF;
}

// 0x4E8DF0
int terrain_variant_get(uint16_t tile_data)
{
    return tile_data & 3;
}

// 0x4E8E00
bool terrain_is_blocked(int64_t sec)
{
    uint16_t tile_data;

    tile_data = terrain_sector_tile_get(sec);
    if (tile_data == 0xFFFF) {
        return true;
    }

    if ((terrain_type_blocked_flags[terrain_primary_type_get(tile_data)] & 1) != 0) {
        return true;
    }

    if ((terrain_type_blocked_flags[terrain_secondary_type_get(tile_data)] & 1) != 0) {
        return true;
    }

    return false;
}

// 0x4E8E60
int64_t terrain_variant_sector_id(int variant)
{
    // TODO: Check.
    int64_t variant_index = variant;
    return (variant_index % 2) | ((variant_index / 2) << 26);
}

// 0x4E8EA0
bool terrain_init_types(void)
{
    MesFileEntry mes_file_entry;
    int index;
    char* pch;

    for (index = 0; index < TERRAIN_TYPE_MAX; index++) {
        mes_file_entry.num = index;
        if (!mes_search(terrain_mes_file, &mes_file_entry)) {
            break;
        }

        pch = strstr(mes_file_entry.str, "/b");
        if (pch != NULL) {
            terrain_type_blocked_flags[index] = 1;

            do {
                pch--;
            } while (SDL_isspace(*pch));

            pch[1] = '\0';
        }
    }

    return true;
}

// 0x4E8F40
bool terrain_types_and_variants_scan(void)
{
    MesFileEntry msg;
    char path[TIG_MAX_PATH];
    int index;
    int k;

    for (index = 0; index < TERRAIN_TYPE_MAX; index++) {
        msg.num = index;
        if (!mes_search(terrain_mes_file, &msg)) {
            break;
        }

        for (k = 0; k <= 3; k++) {
            strcpy(path, "terrain\\");
            strcat(path, msg.str);
            strcat(path, "\\");
            SDL_lltoa(terrain_variant_sector_id(k), path + strlen(path), 10);
            strcat(path, ".sec");

            if (!tig_file_exists(path, NULL)) {
                break;
            }

            terrain_variant_counts[index]++;
        }

        if (k == 0 && index == 0) {
            return false;
        }
    }

    if (index == 0) {
        return false;
    }

    terrain_type_count = index;

    return true;
}

// 0x4E90B0
bool terrain_init_transitions(void)
{
    MesFileEntry mes_file_entry;
    int transition_count;
    bool* transitions;
    int index;
    int from;
    int to;
    int first_step_type;

    mes_file_entry.num = FIRST_TERRAIN_TRANSITION_ID;
    if (!mes_search(terrain_mes_file, &mes_file_entry)) {
        return false;
    }

    transition_count = terrain_type_count * terrain_type_count;
    transitions = (bool*)MALLOC(sizeof(*transitions) * transition_count);
    for (index = 0; index < transition_count; index++) {
        transitions[index] = false;
    }

    do {
        if (!terrain_parse_transition(mes_file_entry.str, &from, &to)) {
            FREE(transitions);
            return false;
        }

        transitions[to + from * terrain_type_count] = true;
        transitions[from + to * terrain_type_count] = true;
    } while (mes_find_next(terrain_mes_file, &mes_file_entry));

    terrain_transition_next_steps = (int*)MALLOC(sizeof(*terrain_transition_next_steps) * transition_count);

    for (from = 0; from < terrain_type_count; from++) {
        for (to = 0; to < terrain_type_count; to++) {
            if (!terrain_transition_first_step_find(from, to, transitions, &first_step_type)) {
                FREE(transitions);
                return false;
            }

            terrain_transition_next_steps[to + from * terrain_type_count] = first_step_type;
        }
    }

    FREE(transitions);
    return true;
}

// 0x4E9240
bool terrain_parse_transition(char* str, int* from_terrain_type, int* to_terrain_type)
{
    char* pch;
    int from;
    int to;

    pch = strstr(str, " to ");
    if (pch == NULL) {
        return false;
    }

    *pch = '\0';

    from = terrain_match_base_name(str);
    *pch = ' ';
    if (from == -1) {
        return false;
    }

    to = terrain_match_base_name(pch + 4);
    if (to == -1) {
        return false;
    }

    *from_terrain_type = from;
    *to_terrain_type = to;

    return true;
}

// 0x4E92B0
bool terrain_transition_first_step_find(int from, int to, bool* transitions, int* next_type_ptr)
{
    int* path;
    bool result;

    if (from == to) {
        *next_type_ptr = from;
        return true;
    }

    path = (int*)MALLOC(sizeof(*path) * terrain_type_count);
    path[0] = from;

    result = terrain_transition_path_search(path, 0, to, transitions);

    *next_type_ptr = path[1];
    FREE(path);

    return result;
}

// 0x4E9310
bool terrain_transition_path_search(int* path, int path_depth, int destination_type, bool* transitions)
{
    int candidate_type;
    int adjacency_row_offset;
    int visited_index;

    adjacency_row_offset = 0;
    for (candidate_type = 0; candidate_type < terrain_type_count; candidate_type++) {
        if (transitions[adjacency_row_offset + path[path_depth]]) {
            for (visited_index = 0; visited_index <= path_depth; visited_index++) {
                if (candidate_type == path[visited_index]) {
                    break;
                }
            }

            if (visited_index > path_depth) {
                path[path_depth + 1] = candidate_type;
                if (candidate_type == destination_type) {
                    return true;
                }

                if (terrain_transition_path_search(path, path_depth + 1, destination_type, transitions)) {
                    return true;
                }
            }
        }
        adjacency_row_offset += terrain_type_count;
    }

    return false;
}

// 0x4E93A0
uint16_t terrain_transition_base_tile_get(int from, int to)
{
    uint16_t value;
    int next_type;

    next_type = terrain_transition_next_steps[to + from * terrain_type_count];
    terrain_encode_to_pointer(next_type, next_type, 15, 0, &value);

    return value;
}

// 0x4E93E0
bool terrain_transition_uses_endpoint(int from, int to)
{
    int terrain_type;

    terrain_type = terrain_primary_type_get(terrain_transition_base_tile_get(from, to));
    return terrain_type == from || terrain_type == to;
}

// 0x4E9410
int terrain_compressed_row_cache_load(int64_t row_y)
{
    int slot;

    slot = terrain_row_cache_next_slot;
    terrain_compressed_row_decode(terrain_row_cache[slot], terrain_compressed_row_pointer_get(row_y));
    terrain_row_cache_y_coordinates[slot] = row_y;
    terrain_row_cache_next_slot = (slot + 1) % 4;

    return slot;
}

// 0x4E9470
void terrain_compressed_row_decode(void* dst, uint16_t* src)
{
    uint32_t size;
    memcpy(&size, src, sizeof(size));
    if (!terrain_row_inflate(dst, (uint8_t*)src + sizeof(size), (int)size)) {
        memset(dst, 0, (size_t)terrain_header.width * sizeof(uint16_t));
        tig_debug_println("Invalid compressed terrain row.");
    }
}

// 0x4E9490
bool terrain_row_inflate(void* dst, void* src, int size)
{
    return size > 0 && terrain_storage_inflate_row(dst, (size_t)terrain_header.width * sizeof(uint16_t), src, (size_t)size);
}

// 0x4E9540
uint16_t* terrain_compressed_row_pointer_get(int64_t row_y)
{
    int64_t row_index;
    uint16_t* row_data = terrain_tile_data;

    for (row_index = 0; row_index < row_y; row_index++) {
        uint32_t size;
        memcpy(&size, row_data, sizeof(size));
        row_data = (uint16_t*)((uint8_t*)row_data + size + sizeof(size));
    }
    return row_data;
}

// 0x4E9580
bool terrain_compressed_rows_read(TigFile* stream)
{
    int64_t y;
    int size;
    int tmp_size = 0;
    void* tmp_buf = NULL;
    uint16_t* dst = terrain_tile_data;

    for (y = 0; y < terrain_header.height; y++) {
        if (tig_file_fread(&size, sizeof(size), 1, stream) != 1
            || size <= 0 || size > tig_file_filelength(stream) - tig_file_ftell(stream)) {
            if (tmp_size != 0) {
                FREE(tmp_buf);
            }
            return false;
        }

        if (size > tmp_size) {
            void* resized = REALLOC(tmp_buf, size);
            if (resized == NULL) {
                FREE(tmp_buf);
                return false;
            }
            tmp_buf = resized;
            tmp_size = size;
        }

        if (tig_file_fread(tmp_buf, size, 1, stream) != 1) {
            if (tmp_size != 0) {
                FREE(tmp_buf);
            }
            return false;
        }

        if (!terrain_row_inflate(dst, tmp_buf, size)) {
            FREE(tmp_buf);
            return false;
        }

        dst += terrain_header.width;
    }

    if (tmp_size != 0) {
        FREE(tmp_buf);
    }

    return true;
}

// 0x4E9680
bool terrain_compressed_rows_write(TigFile* stream)
{
    return terrain_storage_rows_write(stream, terrain_tile_data, terrain_header.width, terrain_header.height);
}
