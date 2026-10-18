#include "game/wallcheck.h"

#include "game/object.h"
#include "game/player.h"
#include "game/roof.h"
#include "game/sector.h"

typedef struct WallVisibilitySample {
    /* 0000 */ unsigned int flags;
    /* 0004 */ int offset_x;
    /* 0008 */ int offset_y;
} WallVisibilitySample;

typedef struct FadedRoofReference {
    /* 0000 */ int64_t location;
    /* 0008 */ int reference_count;
    /* 000C */ int field_C;
} FadedRoofReference;

typedef struct WallTransparencyEntry {
    /* 0000 */ unsigned int flags;
    /* 0008 */ int64_t sector_id;
    /* 0010 */ int64_t obj;
    /* 0018 */ int64_t roof_location;
} WallTransparencyEntry;

typedef struct WallVisibilityProfile {
    /* 0000 */ int flags;
    /* 0004 */ int primary_sample_index;
    /* 0008 */ int secondary_sample_index;
    /* 000C */ int primary_sample_flags;
    /* 0010 */ int secondary_sample_flags;
    /* 0014 */ int scan_direction;
    /* 0018 */ int suppressed_profiles[10];
    /* 0040 */ int suppressed_profiles_by_quadrant[4][5];
} WallVisibilityProfile;

static void wallcheck_object_transparency_track(int64_t obj, int64_t sector_id, int type);
static bool wallcheck_object_index_find(int64_t obj, int* index_ptr);
static void wallcheck_roof_fade_acquire(int64_t location);
static bool wallcheck_roof_index_find(int64_t location, int* index_ptr);
static void wallcheck_pending_transparency_apply(void);
static void wallcheck_roof_faded_clear(int64_t location);

// 0x5A3E90
static bool wallcheck_enabled = true;

// 0x5A3E94
static uint8_t wallcheck_viewer_quadrant_masks[4][4] = {
    { 3, 1, 1, 9 },
    { 2, 3, 9, 8 },
    { 2, 6, 12, 8 },
    { 6, 4, 4, 12 },
};

// 0x5A3EA4
static char wallcheck_quadrant_masks[4] = {
    1,
    2,
    4,
    8,
};

// 0x5A3EA8
static WallVisibilitySample wallcheck_visibility_samples[27] = {
    { 0, 8, -8 },
    { 0, 4, -8 },
    { 0, 8, -4 },
    { 0, 4, -4 },
    { 0, 0, -4 },
    { 0, 12, 0 },
    { 0, 8, 0 },
    { 0, 4, 0 },
    { 0, 0, 0 },
    { 0, -4, 0 },
    { 0, 16, 4 },
    { 0, 12, 4 },
    { 0, 8, 4 },
    { 0, 4, 4 },
    { 0, 0, 4 },
    { 0, -4, 4 },
    { 0, -8, 4 },
    { 0, 12, 8 },
    { 0, 8, 8 },
    { 0, 4, 8 },
    { 0, 0, 8 },
    { 0, -4, 8 },
    { 0, -8, 8 },
    { 0, 8, 12 },
    { 0, 4, 12 },
    { 0, 0, 12 },
    { 0, 4, 16 },
};

// 0x5A3FF0
static WallVisibilityProfile wallcheck_visibility_profiles[34] = {
    {
        1,
        7,
        13,
        2,
        1,
        3,
        { 4, 6, 10, 12, 14, 20, 26, 28, 30, 32 },
        {
            { 11, 31, 33, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { 18, 22, -1, -1, -1 },
            { 11, 18, 22, 31, 33 },
        },
    },
    {
        1,
        13,
        14,
        32,
        16,
        1,
        { 5, 7, 11, 13, 15, 21, 27, 29, 31, 33 },
        {
            { -1, -1, -1, -1, -1 },
            { 10, 30, 32, -1, -1 },
            { 10, 19, 23, 30, 32 },
            { 19, 23, -1, -1, -1 },
        },
    },
    {
        0,
        7,
        8,
        0,
        0,
        1,
        { 8, 17, 18, 22, 24, -1, -1, -1, -1, -1 },
        {
            { 26, -1, -1, -1, -1 },
            { 26, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        0,
        8,
        14,
        0,
        0,
        3,
        { 9, 16, 19, 23, 25, -1, -1, -1, -1, -1 },
        {
            { 27, -1, -1, -1, -1 },
            { 27, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        12,
        13,
        32,
        16,
        1,
        { 10, 30, 32, -1, -1, -1, -1, -1, -1, -1 },
        {
            { 11, 31, 33, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { 11, 31, 33, -1, -1 },
        },
    },
    {
        1,
        13,
        19,
        2,
        1,
        3,
        { 31, 33, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { 10, 11, 30, 32, -1 },
            { 10, 11, 30, 32, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        6,
        12,
        2,
        1,
        3,
        { 14, 20, 28, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        19,
        20,
        32,
        16,
        1,
        { 15, 21, 29, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        3,
        7,
        2,
        1,
        3,
        { 18, 22, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { 26, -1, -1, -1, -1 },
            { 26, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        14,
        15,
        32,
        16,
        1,
        { 19, 23, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { 27, -1, -1, -1, -1 },
            { 27, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        12,
        18,
        2,
        1,
        3,
        { 30, 32, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        18,
        19,
        32,
        16,
        1,
        { 31, 33, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        0,
        6,
        7,
        0,
        0,
        1,
        { 26, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { 22, -1, -1, -1, -1 },
            { 22, -1, -1, -1, -1 },
        },
    },
    {
        0,
        14,
        20,
        0,
        0,
        3,
        { 27, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { 23, -1, -1, -1, -1 },
            { 23, -1, -1, -1, -1 },
        },
    },
    {
        1,
        5,
        11,
        2,
        1,
        3,
        { 28, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        24,
        25,
        32,
        16,
        1,
        { 29, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        0,
        9,
        15,
        0,
        0,
        3,
        { 25, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        0,
        3,
        4,
        0,
        0,
        1,
        { 24, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        0,
        2,
        3,
        0,
        0,
        1,
        { 22, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        0,
        15,
        21,
        0,
        0,
        3,
        { 23, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        11,
        12,
        32,
        16,
        1,
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { 30, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { 30, -1, -1, -1, -1 },
        },
    },
    {
        1,
        19,
        24,
        2,
        1,
        3,
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { 31, -1, -1, -1, -1 },
            { 31, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        0,
        2,
        2,
        1,
        3,
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        21,
        22,
        32,
        16,
        1,
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        1,
        3,
        2,
        1,
        3,
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        15,
        16,
        32,
        16,
        1,
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        2,
        6,
        2,
        1,
        3,
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        20,
        21,
        32,
        16,
        1,
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        10,
        11,
        32,
        16,
        1,
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        24,
        26,
        2,
        1,
        3,
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        11,
        17,
        2,
        1,
        3,
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        23,
        24,
        32,
        16,
        1,
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        17,
        18,
        32,
        16,
        1,
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
    {
        1,
        18,
        23,
        2,
        1,
        3,
        { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
        {
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 },
        },
    },
};

// 0x5A5310
static int wallcheck_sample_direction_x_offsets[8] = {
    0,
    -1,
    0,
    0,
    0,
    0,
    0,
    0,
};

// 0x5A5330
static int wallcheck_sample_direction_y_offsets[8] = {
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
};

// 0x5E0A00
static IsoInvalidateRectFunc* wallcheck_invalidate_rect_callback;

// 0x5E0A08
static int64_t wallcheck_viewer_location;

// 0x5E0A10
static FadedRoofReference wallcheck_faded_roofs[64];

// 0x5E0E10
static int wallcheck_previous_quadrant_mask;

// 0x5E0E18
static int64_t wallcheck_previous_sample_location;

// 0x5E0E20
static WallTransparencyEntry wallcheck_transparency_entries[256];

// 0x5E2E24
static int wallcheck_transparency_entry_count;

// 0x5E2E28
static int wallcheck_faded_roof_count;

// 0x5DE6E8
static bool wallcheck_dirty;

// 0x5DE6F0
static ObjectList wallcheck_sample_wall_lists[34][3];

// 0x437D90
bool wallcheck_init(GameInitInfo* init_info)
{
    wallcheck_transparency_entry_count = 0;
    wallcheck_faded_roof_count = 0;
    wallcheck_invalidate_rect_callback = init_info->invalidate_rect_func;

    return true;
}

// 0x437DB0
void wallcheck_reset(void)
{
    int index;

    if (wallcheck_transparency_entry_count > 0) {
        for (index = 0; index < wallcheck_transparency_entry_count; index++) {
            wallcheck_transparency_entries[index].flags &= ~0x2;
            wallcheck_transparency_entries[index].flags |= 0x1;
        }

        wallcheck_pending_transparency_apply();
    }

    wallcheck_previous_sample_location = 0;
    wallcheck_previous_quadrant_mask = 0;
    wallcheck_dirty = false;
}

// 0x437E00
void wallcheck_exit(void)
{
    wallcheck_reset();
    wallcheck_invalidate_rect_callback = 0;
}

// 0x437E10
void wallcheck_ping(tig_timestamp_t timestamp)
{
    (void)timestamp;

    if (wallcheck_dirty) {
        roof_recalc(wallcheck_viewer_location);
        wallcheck_recalc(wallcheck_viewer_location);
        wallcheck_dirty = false;
    }
}

// 0x437E50
void wallcheck_recalc(int64_t loc)
{
    int v1;
    int v2;
    ObjectList walls;
    ObjectNode* node;
    tig_art_id_t art_id;
    int rot;
    int64_t x;
    int64_t y;
    int64_t tmp_x;
    int64_t tmp_y;
    unsigned int v3;

    if (!wallcheck_enabled) {
        return;
    }

    x = LOCATION_GET_X(loc);
    y = LOCATION_GET_Y(loc);
    v1 = (int)((LOCATION_GET_X(loc) - 2) % 4);
    v2 = (int)((LOCATION_GET_Y(loc) - 2) % 4);

    if (v1 == 0) {
        object_list_location(loc, OBJ_TM_WALL, &walls);
        node = walls.head;
        while (node != NULL) {
            art_id = obj_field_int32_get(node->obj, OBJ_F_CURRENT_AID);
            rot = tig_art_id_rotation_get(art_id);
            if (rot == 4 || rot == 5) {
                v1 = 3;
                x--;
            } else if (rot == 2 || rot == 3) {
                v2 = 3;
                y--;
            }
            node = node->next;
        }
        object_list_destroy(&walls);
    } else if (v2 == 0) {
        object_list_location(loc, OBJ_TM_WALL, &walls);
        node = walls.head;
        while (node != NULL) {
            art_id = obj_field_int32_get(node->obj, OBJ_F_CURRENT_AID);
            rot = tig_art_id_rotation_get(art_id);
            if (rot == 2 || rot == 3) {
                y--;
                v2 = 3;
            }
            node = node->next;
        }
        object_list_destroy(&walls);
    }

    x -= v1;
    y -= v2;
    v3 = wallcheck_viewer_quadrant_masks[v2][v1];

    if (wallcheck_previous_sample_location == LOCATION_MAKE(x, y)
        && (wallcheck_previous_quadrant_mask & v3) != 0) {
        return;
    }

    wallcheck_previous_sample_location = LOCATION_MAKE(x, y);
    wallcheck_previous_quadrant_mask = v3;

    for (int i = 0; i < wallcheck_transparency_entry_count; i++) {
        wallcheck_transparency_entries[i].flags &= ~0x02;
        wallcheck_transparency_entries[i].flags |= 0x01;
    }

    for (int i = 0; i < 34; i++) {
        wallcheck_visibility_profiles[i].flags &= 0xFFFF;
    }

    for (int i = 0; i < 27; i++) {
        wallcheck_visibility_samples[i].flags = 0;
    }

    for (int i = 0; i < 22; i++) {
        if ((wallcheck_visibility_profiles[i].flags & 0x10000) == 0) {
            tmp_x = x
                + wallcheck_sample_direction_x_offsets[wallcheck_visibility_profiles[i].scan_direction]
                + wallcheck_visibility_samples[wallcheck_visibility_profiles[i].primary_sample_index].offset_x;
            tmp_y = y
                + wallcheck_sample_direction_y_offsets[wallcheck_visibility_profiles[i].scan_direction]
                + wallcheck_visibility_samples[wallcheck_visibility_profiles[i].primary_sample_index].offset_y;

            for (int j = 0; j < 3; j++) {
                object_list_location(LOCATION_MAKE(tmp_x, tmp_y), OBJ_TM_WALL, &(wallcheck_sample_wall_lists[i][j]));
                if (wallcheck_sample_wall_lists[i][j].head == NULL) {
                    wallcheck_visibility_profiles[i].flags |= 0x20000;
                    if ((wallcheck_visibility_profiles[i].flags & 0x01) == 0) {
                        break;
                    }
                }

                tmp_x += wallcheck_sample_direction_x_offsets[wallcheck_visibility_profiles[i].scan_direction];
                tmp_y += wallcheck_sample_direction_y_offsets[wallcheck_visibility_profiles[i].scan_direction];
            }

            if ((wallcheck_visibility_profiles[i].flags & 0x20000) == 0) {
                for (int j = 0; j < 10; j++) {
                    if (wallcheck_visibility_profiles[i].suppressed_profiles[j] == -1) {
                        break;
                    }
                    wallcheck_visibility_profiles[wallcheck_visibility_profiles[i].suppressed_profiles[j]].flags |= 0x10000;
                }

                for (int j = 0; j < 4; j++) {
                    if ((wallcheck_quadrant_masks[j] & wallcheck_previous_quadrant_mask) != 0) {
                        for (int k = 0; k < 5; k++) {
                            if (wallcheck_visibility_profiles[i].suppressed_profiles_by_quadrant[j][k] == -1) {
                                break;
                            }

                            wallcheck_visibility_profiles[wallcheck_visibility_profiles[i].suppressed_profiles_by_quadrant[j][k]].flags |= 0x10000;
                        }
                    }
                }
            }
        }
    }

    for (int i = 22; i < 34; i++) {
        if ((wallcheck_visibility_profiles[i].flags & 0x10000) == 0) {
            tmp_x = x
                + wallcheck_sample_direction_x_offsets[wallcheck_visibility_profiles[i].scan_direction]
                + wallcheck_visibility_samples[wallcheck_visibility_profiles[i].primary_sample_index].offset_x;
            tmp_y = y
                + wallcheck_sample_direction_y_offsets[wallcheck_visibility_profiles[i].scan_direction]
                + wallcheck_visibility_samples[wallcheck_visibility_profiles[i].primary_sample_index].offset_y;

            for (int j = 0; j < 3; j++) {
                object_list_location(LOCATION_MAKE(tmp_x, tmp_y), OBJ_TM_WALL, &(wallcheck_sample_wall_lists[i][j]));
                tmp_x += wallcheck_sample_direction_x_offsets[wallcheck_visibility_profiles[i].scan_direction];
                tmp_y += wallcheck_sample_direction_y_offsets[wallcheck_visibility_profiles[i].scan_direction];
            }
        }
    }

    for (int i = 0; i < 34; i++) {
        if ((wallcheck_visibility_profiles[i].flags & 0x10000) == 0
            && (wallcheck_visibility_profiles[i].flags & 0x01) != 0) {
            bool v1 = false;
            for (int j = 0; j < 3; j++) {
                if (wallcheck_sample_wall_lists[i][j].head != NULL) {
                    v1 = true;
                    wallcheck_object_transparency_track(wallcheck_sample_wall_lists[i][j].head->obj,
                        wallcheck_sample_wall_lists[i][j].sectors[0],
                        0);
                }
            }
            if (v1) {
                wallcheck_visibility_samples[wallcheck_visibility_profiles[i].primary_sample_index].flags |= wallcheck_visibility_profiles[i].primary_sample_flags;
                wallcheck_visibility_samples[wallcheck_visibility_profiles[i].secondary_sample_index].flags |= wallcheck_visibility_profiles[i].secondary_sample_flags;
            }
        }
    }

    for (int i = 0; i < 34; i++) {
        for (int j = 0; j < 3; j++) {
            if (wallcheck_sample_wall_lists[i][j].num_sectors > 0) {
                object_list_destroy(&(wallcheck_sample_wall_lists[i][j]));
                wallcheck_sample_wall_lists[i][j].num_sectors = 0;
            }
        }

        wallcheck_visibility_profiles[i].flags &= 0xFFFF;
    }

    for (int i = 0; i < 27; i++) {
        if (wallcheck_visibility_samples[i].flags != 0) {
            object_list_location(LOCATION_MAKE(x + wallcheck_visibility_samples[i].offset_x, y + wallcheck_visibility_samples[i].offset_y), OBJ_TM_WALL, &walls);
            node = walls.head;
            while (node != NULL) {
                art_id = obj_field_int32_get(node->obj, OBJ_F_CURRENT_AID);
                rot = tig_art_id_rotation_get(art_id);
                if ((rot & 1) != 0) {
                    rot--;
                }
                if (rot == 0 || rot == 4) {
                    if ((wallcheck_visibility_samples[i].flags & 0x03) != 0) {
                        wallcheck_object_transparency_track(node->obj, walls.sectors[0], wallcheck_visibility_samples[i].flags & 0x03);
                    }
                } else {
                    if ((wallcheck_visibility_samples[i].flags & 0x30) != 0) {
                        wallcheck_object_transparency_track(node->obj, walls.sectors[0], wallcheck_visibility_samples[i].flags & 0x30);
                    }
                }
                node = node->next;
            }
            object_list_destroy(&walls);
            wallcheck_visibility_samples[i].flags = 0;
        }
    }

    wallcheck_pending_transparency_apply();
}

// 0x4384F0
void wallcheck_flush(void)
{
    wallcheck_reset();
}

// 0x438500
void wallcheck_set_enabled(bool enabled)
{
    if (wallcheck_enabled != enabled) {
        wallcheck_enabled = enabled;
    }
}

// 0x438520
bool wallcheck_is_enabled(void)
{
    return wallcheck_enabled;
}

// 0x438530
void wallcheck_player_location_notify(int64_t obj)
{
    if (player_is_local_pc_obj(obj)) {
        wallcheck_viewer_location = obj_field_int64_get(obj, OBJ_F_LOCATION);
        wallcheck_dirty = true;
    }
}

// 0x438570
void wallcheck_object_transparency_track(int64_t obj, int64_t sector_id, int type)
{
    int index;
    Sector* sector;

    if (!wallcheck_object_index_find(obj, &index)) {
        memmove(&(wallcheck_transparency_entries[index + 1]),
            &(wallcheck_transparency_entries[index]),
            sizeof(*wallcheck_transparency_entries) * (wallcheck_transparency_entry_count - index));
        wallcheck_transparency_entries[index].flags = 0;
        wallcheck_transparency_entries[index].sector_id = sector_id;
        wallcheck_transparency_entries[index].obj = obj;
        wallcheck_transparency_entries[index].roof_location = roof_normalize_loc(obj_field_int64_get(obj, OBJ_F_LOCATION));

        sector_lock(sector_id, &sector);
        wallcheck_roof_fade_acquire(wallcheck_transparency_entries[index].roof_location);
        wallcheck_transparency_entry_count++;
    }

    if (type == 1 || type == 16) {
        wallcheck_transparency_entries[index].flags = 0x04 | 0x02;
    } else if (type == 2 || type == 32) {
        wallcheck_transparency_entries[index].flags = 0x08 | 0x02;
    } else {
        wallcheck_transparency_entries[index].flags = 0x08 | 0x04 | 0x02;
    }
}

// 0x4386B0
bool wallcheck_object_index_find(int64_t obj, int* index_ptr)
{
    int l;
    int r;
    int m;
    int64_t curr;

    l = 0;
    r = wallcheck_transparency_entry_count - 1;
    while (l <= r) {
        m = (l + r) / 2;
        curr = wallcheck_transparency_entries[m].obj;

        if (curr < obj) {
            l = m + 1;
        } else if (curr > obj) {
            r = m - 1;
        } else {
            *index_ptr = m;
            return true;
        }
    }

    *index_ptr = l;
    return false;
}

// 0x438720
void wallcheck_roof_fade_acquire(int64_t a1)
{
    int index;

    if (!wallcheck_roof_index_find(a1, &index)) {
        memmove(&(wallcheck_faded_roofs[index + 1]),
            &(wallcheck_faded_roofs[index]),
            sizeof(*wallcheck_faded_roofs) * (wallcheck_faded_roof_count - index));
        wallcheck_faded_roofs[index].location = a1;
        wallcheck_faded_roofs[index].reference_count = 0;
        wallcheck_faded_roof_count++;
    }

    if (++wallcheck_faded_roofs[index].reference_count == 1) {
        roof_fade_on(a1);
    }
}

// 0x4387C0
bool wallcheck_roof_index_find(int64_t a1, int* index_ptr)
{
    int l;
    int r;
    int m;
    int64_t curr;

    l = 0;
    r = wallcheck_faded_roof_count - 1;
    while (l <= r) {
        m = (l + r) / 2;
        curr = wallcheck_faded_roofs[m].location;

        if (curr < a1) {
            l = m + 1;
        } else if (curr > a1) {
            r = m - 1;
        } else {
            *index_ptr = m;
            return true;
        }
    }

    *index_ptr = l;
    return false;
}

// 0x438830
void wallcheck_pending_transparency_apply(void)
{
    int idx;
    unsigned int wall_flags;
    unsigned int render_flags;
    TigRect obj_rect;
    unsigned int new_wall_flags;

    for (idx = 0; idx < wallcheck_transparency_entry_count; idx++) {
        if ((wallcheck_transparency_entries[idx].flags & 0x01) != 0) {
            wall_flags = obj_field_int32_get(wallcheck_transparency_entries[idx].obj, OBJ_F_WALL_FLAGS);
            if ((wall_flags & 0x01) == 0) {
                wall_flags &= ~(0x04 | 0x02);
                obj_field_int32_set(wallcheck_transparency_entries[idx].obj, OBJ_F_WALL_FLAGS, wall_flags);

                render_flags = obj_field_int32_get(wallcheck_transparency_entries[idx].obj, OBJ_F_RENDER_FLAGS);
                render_flags &= ~ORF_01000000;
                obj_field_int32_set(wallcheck_transparency_entries[idx].obj, OBJ_F_RENDER_FLAGS, render_flags);

                object_get_rect(wallcheck_transparency_entries[idx].obj, 0, &obj_rect);
                wallcheck_invalidate_rect_callback(&obj_rect);
            }

            wallcheck_roof_faded_clear(wallcheck_transparency_entries[idx].roof_location);

            sector_unlock(wallcheck_transparency_entries[idx].sector_id);

            memmove(&(wallcheck_transparency_entries[idx]),
                &(wallcheck_transparency_entries[idx + 1]),
                sizeof(wallcheck_transparency_entries[0]) * (wallcheck_transparency_entry_count - idx - 1));
            wallcheck_transparency_entry_count--;
            idx--;
        } else if ((wallcheck_transparency_entries[idx].flags & 0x02) != 0) {
            wall_flags = obj_field_int32_get(wallcheck_transparency_entries[idx].obj, OBJ_F_WALL_FLAGS);
            if ((wall_flags & 0x01) == 0) {
                wall_flags &= ~(0x04 | 0x02);

                if (((wallcheck_transparency_entries[idx].flags) & (0x08 | 0x04)) == (0x08 | 0x04)) {
                    new_wall_flags = 0x04 | 0x02;
                } else if ((wallcheck_transparency_entries[idx].flags & (0x08 | 0x04)) == 0x04) {
                    new_wall_flags = 0x02;
                } else {
                    new_wall_flags = 0x04;
                }

                if (new_wall_flags != wall_flags) {
                    obj_field_int32_set(wallcheck_transparency_entries[idx].obj, OBJ_F_WALL_FLAGS, wall_flags | new_wall_flags);

                    render_flags = obj_field_int32_get(wallcheck_transparency_entries[idx].obj, OBJ_F_RENDER_FLAGS);
                    render_flags &= ~ORF_01000000;
                    obj_field_int32_set(wallcheck_transparency_entries[idx].obj, OBJ_F_RENDER_FLAGS, render_flags);

                    object_get_rect(wallcheck_transparency_entries[idx].obj, 0, &obj_rect);
                    wallcheck_invalidate_rect_callback(&obj_rect);
                }
            }

            wallcheck_transparency_entries[idx].flags &= ~0x02;
        }
    }
}

// 0x438A50
void wallcheck_roof_faded_clear(int64_t location)
{
    int index;

    if (!wallcheck_roof_index_find(location, &index)) {
        tig_debug_printf("Warning: unable to find roof to clear in wallcheck_roof_faded_clear()\n");
        return;
    }

    if (wallcheck_faded_roofs[index].reference_count-- == 1) {
        roof_fade_off(location);
        memmove(&(wallcheck_faded_roofs[index]),
            &(wallcheck_faded_roofs[index + 1]),
            sizeof(*wallcheck_faded_roofs) * (wallcheck_faded_roof_count - index - 1));
        wallcheck_faded_roof_count--;
    }
}
