#include "ui/asset_warmup.h"
#include "ui/wmap_ui.h"

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#include "game/ai.h"
#include "game/anim.h"
#include "game/antiteleport.h"
#include "game/area.h"
#include "game/critter.h"
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/hrp.h"
#include "game/input_bindings.h"
#include "game/location.h"
#include "game/magictech.h"
#include "game/map.h"
#include "game/map_marker.h"
#include "game/language.h"
#include "game/mes.h"
#include "game/name.h"
#include "game/obj.h"
#include "game/path.h"
#include "game/movement_route.h"
#include "game/player.h"
#include "game/sector.h"
#include "game/teleport.h"
#include "game/townmap.h"
#include "game/ui.h"
#include "ui/anim_ui.h"
#include "ui/intgame.h"
#include "ui/logbook_ui.h"
#include "ui/map_marker_input.h"
#include "ui/search_bar.h"
#include "ui/textedit_ui.h"
#include "ui/types.h"

typedef enum WmapUiMode {
    WMAP_UI_MODE_WORLD,
    WMAP_UI_MODE_CONTINENT,
    WMAP_UI_MODE_TOWN,
    WMAP_UI_MODE_COUNT,
} WmapUiMode;

typedef enum WmapNoteType {
    WMAP_NOTE_TYPE_NOTE,
    WMAP_NOTE_TYPE_SKULL,
    WMAP_NOTE_TYPE_CHEST,
    WMAP_NOTE_TYPE_QUEST,
    WMAP_NOTE_TYPE_CROSS,
    WMAP_NOTE_TYPE_LOC,
    WMAP_NOTE_TYPE_WAYPOINT,
    WMAP_NOTE_TYPE_NEW_LOC,
    WMAP_NOTE_TYPE_COUNT,
} WmapNoteType;

typedef enum WmapRouteType {
    WMAP_ROUTE_TYPE_WORLD,
    WMAP_ROUTE_TYPE_TOWN,
    WMAP_ROUTE_TYPE_COUNT,
} WmapRouteType;

typedef enum WmapUiState {
    /**
     * The UI is stationary.
     */
    WMAP_UI_STATE_NORMAL,

    /**
     * NOTE: Not implemented in the original code. Looks like the idea was to
     * allow moving/updating individual waypoints on the path.
     */
    WMAP_UI_STATE_WAYPOINT_DRAG,

    /**
     * Moving along the route.
     */
    WMAP_UI_STATE_MOVING,

    /**
     * NOTE: Not implemented in the original code. The idea was to allow
     * creating/editing custom map notes.
     */
    WMAP_UI_STATE_EDITING,

    /**
     * Navigating the map by pressing and holding directional pad in the center
     * of the map.
     */
    WMAP_UI_STATE_NAVIGATING,
} WmapUiState;

typedef struct WmapCoords {
    int x;
    int y;
} WmapCoords;

typedef struct WmapNoteTypeInfo {
    // NOTE: Rare case - during initialization the value at offset 0 is art num,
    // which is replaced with interface art id (see `wmap_ui_init`).
    union {
        int num;
        tig_art_id_t art_id;
    };
    /* 0004 */ int width;
    /* 0008 */ int height;
    /* 000C */ tig_color_t color;
    /* 0010 */ tig_font_handle_t font;
    /* 0014 */ bool enabled;
} WmapNoteTypeInfo;

typedef struct WmapNote {
    /* 0000 */ int id;
    /* 0004 */ unsigned int flags;
    /* 0008 */ WmapCoords coords;
    /* 0010 */ int64_t location;
    /* 0018 */ TigRect bounds;
    /* 0028 */ WmapNoteType type;
    /* 002C */ char str[250];
    /* 0128 */ TigVideoBuffer* video_buffer;
} WmapNote;

typedef struct WmapTile {
    /* 0000 */ unsigned int flags;
    /* 0004 */ TigVideoBuffer* video_buffer;
    /* 0008 */ TigRect rect;
    /* 0018 */ void* reserved_data;
    /* 001C */ int reserved_data_size;
} WmapTile;

typedef struct WmapInfo {
    /* 0000 */ unsigned int flags;
    /* 0004 */ TigRect rect;
    /* 0014 */ TigRect interactive_rect;
    /* 0024 */ int viewport_half_width;
    /* 0028 */ int viewport_half_height;
    /* 002C */ int center_x_max;
    /* 0030 */ int center_y_max;
    /* 0034 */ int scroll_x;
    /* 0038 */ int scroll_y;
    /* 003C */ WmapCoords player_coordinates;
    /* 0044 */ bool navigatable;
    /* 0048 */ void (*refresh)(void);
    /* 004C */ void (*refresh_rect)(TigRect* rect);
    /* 0050 */ void (*reserved_slot_1)(int direction, int, int, int);
    /* 0054 */ int compass_art_number;
    /* 0058 */ int map_width;
    /* 005C */ int map_height;
    /* 0060 */ int scroll_x_max;
    /* 0064 */ int scroll_y_max;
    /* 0068 */ char tile_directory[TIG_MAX_PATH];
    /* 016C */ int tile_width;
    /* 0170 */ int tile_height;
    /* 0174 */ int num_tiles;
    /* 0178 */ int num_hor_tiles;
    /* 017C */ int num_vert_tiles;
    /* 0180 */ WmapTile* tiles;
    /* 0184 */ int num_loaded_tiles;
    /* 0188 */ WmapNote* notes;
    /* 018C */ int reserved_slot_2;
    /* 0190 */ int* num_notes;
    /* 0194 */ int next_note_id;
    /* 0198 */ int hovered_note_id;
    /* 019C */ int reserved_slot_3;
    /* 01A0 */ int reserved_slot_4;
    /* 01A4 */ char str[260];
    /* 02A8 */ TigRect overview_image_rect;
    /* 02B8 */ TigVideoBuffer* video_buffer;
    /* 02BC */ void (*screen_to_map_coordinates)(int x, int y, WmapCoords* coords);
} WmapInfo;

typedef struct WmapRouteWaypoint {
    /* 0000 */ int reserved_flags;
    /* 0004 */ int travel_value;
    /* 0008 */ WmapCoords coords;
    /* 0010 */ int64_t loc;
    /* 0018 */ int start;
    /* 001C */ int steps;
} WmapRouteWaypoint;

typedef struct WmapRoute {
    /* 0000 */ WmapRouteWaypoint waypoints[MOVEMENT_ROUTE_CAPACITY];
    /* 03C0 */ int length;
    /* 03C4 */ int default_travel_value;
    /* 03C8 */ int waypoint_edit_enabled;
    /* 03CC */ int reserved_slot_1;
} WmapRoute;

// 0x5C9160
static WmapNoteTypeInfo wmap_note_type_info[WMAP_NOTE_TYPE_COUNT] = {
    /*     WMAP_NOTE_TYPE_NOTE */ { .num = 142, .enabled = true },
    /*    WMAP_NOTE_TYPE_SKULL */ { .num = 144, .enabled = true },
    /*    WMAP_NOTE_TYPE_CHEST */ { .num = 141, .enabled = true },
    /*    WMAP_NOTE_TYPE_QUEST */ { .num = 143, .enabled = true },
    /*    WMAP_NOTE_TYPE_CROSS */ { .num = 204, .enabled = true },
    /*      WMAP_NOTE_TYPE_LOC */ { .num = 205, .enabled = true },
    /* WMAP_NOTE_TYPE_WAYPOINT */ { .num = 206, .enabled = true },
    /*  WMAP_NOTE_TYPE_NEW_LOC */ { .num = 814, .enabled = true },
};

static void wmap_ui_town_tiles_free(void);
static void wmap_ui_town_notes_save(void);
static void wmap_ui_destroy(void);
static bool wmap_ui_town_note_save(WmapNote* note, TigFile* stream);
static bool wmap_ui_rect_write(TigRect* rect, TigFile* stream);
static bool wmap_ui_town_note_load(WmapNote* note, TigFile* stream);
static bool wmap_ui_rect_read(TigRect* rect, TigFile* stream);
static void wmap_ui_open_internal(void);
static bool wmap_load_worldmap_info(void);
static void wmap_ui_info_loaded_hook_stub(void);
static void wmap_ui_travel_context_refresh(void);
static bool wmap_ui_create(void);
static void wmap_ui_position_from_location_set(int64_t location);
static void wmap_ui_location_to_coords(int64_t location, WmapCoords* coords);
static void wmap_ui_current_position_set(int x, int y);
static bool is_default_wmap(void);
static bool wmap_ui_state_set(WmapUiState state);
static void wmap_ui_coords_to_location(WmapCoords* coords, int64_t* loc_ptr);
static bool wmap_ui_teleport(int64_t loc);
static bool wmap_ui_message_filter(TigMessage* msg);
static bool wmap_ui_destination_coords_check(WmapCoords* coords);
static bool wmap_ui_destination_location_check_stub(int64_t loc);
static void wmap_ui_area_note_add(int id);
static void wmap_ui_draw_coords(WmapCoords* coords);
static void wmap_ui_draw_travel_time(void);
static void wmap_ui_navigate(int x, int y);
static void wmap_ui_scroll_at_edges(int x, int y);
static void wmap_ui_mode_set(WmapUiMode mode);
static bool wmap_load_townmap_info(void);
static void wmap_ui_tile_render_state_init(WmapTile* tile);
static bool wmap_ui_tile_art_lock(int tile);
static void wmap_ui_tile_art_unlock(int tile);
static void wmap_ui_tile_art_unload(int tile);
static bool wmap_ui_mode_tile_art_lock(WmapUiMode mode, int tile);
static bool wmap_ui_tile_art_load(const char* path, TigVideoBuffer** video_buffer_ptr, TigRect* rect);
static bool wmap_ui_mode_tile_art_unlock(WmapUiMode mode, int tile);
static void wmap_ui_mode_tile_art_unload(WmapUiMode mode, int tile);
static void wmap_ui_current_rect_refresh(void);
static void wmap_ui_scroll_rect_refresh(int direction, int reserved_horizontal, int reserved_vertical, int scroll_distance);
static void wmap_ui_viewport_scroll(int direction, int reserved_horizontal, int reserved_vertical, int scroll_distance);
static void wmap_ui_scroll_origin_set(WmapCoords* coords, bool refresh);
static void wmap_ui_handle_scroll(void);
static void wmap_ui_scroll_with_kb(int direction);
static void wmap_ui_scroll_internal(int direction, int scale);
static void wmap_ui_mouse_to_map_coords(int x, int y, WmapCoords* coords);
static void wmap_ui_overview_to_map_coords(int x, int y, WmapCoords* coords);
static void wmap_ui_screen_to_scrolled_coords(int x, int y, WmapCoords* coords);
static bool wmap_ui_note_insert_or_update(WmapNote* note);
static void wmap_ui_note_bounds_update(WmapNote* note);
static void wmap_ui_note_changed_hook_stub(int note_index, int change_flags);
static WmapNote* wmap_ui_note_find_by_id(int id);
static bool find_note_by_coords(WmapCoords* coords, int* id_ptr);
static bool find_note_by_coords_mode(WmapCoords* coords, int* id_ptr, WmapUiMode mode);
static bool wmap_ui_coords_snap_to_player(WmapCoords* coords, int64_t* location_ptr);
static void wmap_ui_new_note_edit_begin(WmapCoords* coords);
static void wmap_ui_note_edit_begin(WmapNote* note);
static void wmap_ui_note_edit_end(bool clear_displayed_text);
static void wmap_ui_textedit_on_enter(TextEdit* textedit);
static bool wmap_note_add(WmapNote* note);
static bool wmap_note_add_mode(WmapNote* note, WmapUiMode mode);
static bool wmap_note_remove(WmapNote* note);
static void wmap_ui_note_removed_hook_stub(int id, WmapUiMode mode);
static void wmap_ui_note_buffer_free(WmapNote* note);
static void wmap_ui_textedit_on_change(TextEdit* textedit);
static void wmap_ui_note_edit_by_id(int id);
static bool wmap_ui_note_has_nonspace_text(const char* str);
static bool wmap_ui_route_destination_add(WmapCoords* coords);
static bool wmap_ui_route_waypoint_hit_test(WmapCoords* coords, int* idx_ptr);
static void wmap_ui_waypoint_drag_hook_stub(int waypoint_index, WmapCoords* coords);
static void wmap_ui_route_waypoint_remove(int waypoint_index);
static void wmap_ui_waypoint_travel_value_set(int waypoint_index, int travel_value, bool refresh);
static void wmap_ui_selected_waypoint_snap(void);
static void wmap_ui_waypoint_snap_to_known_area(WmapRouteWaypoint* wp);
static void wmap_ui_travel_time_refresh(void);
static void wmap_ui_selected_waypoint_value_hook_stub(int waypoint_value);
static bool wmap_ui_travel_arrival_apply(int64_t loc);
static void wmap_ui_nearby_area_discover(int64_t pc_obj, int64_t loc);
static void wmap_ui_destination_to_nearest_area(WmapCoords* coords, int64_t* loc_ptr);
static int64_t wmap_ui_direct_travel_hours_estimate(WmapCoords* from_coords, WmapCoords* to_coords, DateTime* datetime);
static void wmap_ui_town_notes_load(void);
static void wmap_ui_town_notes_clear(void);
static int wmap_ui_compass_arrow_frame_calc(WmapCoords* a, WmapCoords* b);
static void wmap_ui_compass_arrow_frame_set(int frame);
static int wmap_ui_compass_arrow_frame_get(void);
static bool wmap_ui_player_prepare_for_travel(void);
static void wmap_ui_world_route_step(WmapCoords* coords);
static void wmap_ui_overview_draw(void);
static void wmap_ui_overview_position_project_legacy(int x, int y, WmapCoords* coords);
static void wmap_world_refresh_rect(TigRect* rect);
static bool wmap_ui_note_check_stub(WmapNote* note);
static void wmap_ui_note_blit(WmapNote* note, TigRect* viewport_rect, TigRect* clip_rect);
static void wmap_note_vbid_lock(WmapNote* note);
static void wmap_ui_tile_buffer_draw(TigVideoBuffer* video_buffer, TigRect* rect);
static void wmap_town_refresh_rect(TigRect* rect);
static void wmap_ui_route_draw(WmapInfo* map_info, TigRect* viewport_rect, TigRect* clip_rect);
static void wmap_ui_note_icon_draw(int type, WmapCoords* coords, TigRect* viewport_rect, TigRect* clip_rect, WmapInfo* wmap_info, bool completed);
static bool wmap_marker_input(TigMessage* msg);
static void wmap_marker_close(void);
static void wmap_marker_draw(void);
static void wmap_marker_text_changed(TextEdit* edit);
static void wmap_marker_text_enter(TextEdit* edit);
static void wmap_marker_commit(void);
static void wmap_ui_scroll_by(int dx, int dy);
static void wmap_ui_scroll_from_mouse_wheel(int dx, int dy);

// 0x5C9220
static int wmap_ui_spell = -1;

// 0x5C9A68
static TigRect wmap_ui_canvas_frame = { 150, 52, 501, 365 };

// 0x5C9A78
static TigRect wmap_ui_compass_rect = { 662, 248, 130, 130 };

// 0x5C9A88
static TigRect wmap_ui_navigation_dead_zone = { 391, 403, 10, 12 };

// 0x5C9A98
static TigRect wmap_ui_lat_long_frame[] = {
    { 707, 200, 60, 18 },
    { 707, 232, 60, 18 },
};

// 0x5C9AB8
static TigRect wmap_ui_pc_lens_frame = { 25, 65, 89, 89 };

// 0x5C9AC8
static TigRect wmap_ui_note_bounds = { 0, 0, 200, 50 };

// 0x5C9AD8
static int wmap_ui_selected_waypoint = -1;

// 0x5C9B00
static tig_window_handle_t wmap_ui_window = TIG_WINDOW_HANDLE_INVALID;

// 0x5C9B08
static TigRect wmap_ui_window_frame = { 0, 41, 800, 400 };

// 0x5C9B18
static int wmap_unused_selection = -1;

// 0x5C9B20
static TextEdit wmap_ui_textedit = {
    0,
    "",
    50,
    wmap_ui_textedit_on_enter,
    wmap_ui_textedit_on_change,
    NULL,
};

// 0x5C9B38
static TigRect wmap_ui_nav_cvr_frame = { 294, 0, 0, 0 };

// 0x5C9B48
static tig_art_id_t wmap_ui_nav_cvr_art_id = TIG_ART_ID_INVALID;

// 0x5C9B50
static UiButtonInfo wmap_ui_zoom_button_info[2] = {
    { 20, 206, 811, TIG_BUTTON_HANDLE_INVALID },
    { 19, 308, 812, TIG_BUTTON_HANDLE_INVALID },
};

// 0x5C9B70
static UiButtonInfo wmap_ui_navigate_button_info = { 375, 388, 139, TIG_BUTTON_HANDLE_INVALID };

// 0x5C9B80
static UiButtonInfo wmap_ui_travel_button_info = { 698, 353, 813, TIG_BUTTON_HANDLE_INVALID };

// 0x5C9B90
static TigButtonFlags wmap_ui_travel_button_flags = TIG_BUTTON_MOMENTARY;

// 0x5C9B94
static int wmap_ui_scroll_speed_x = 5;

// 0x5C9B98
static int wmap_ui_scroll_speed_y = 5;

static bool wmap_ui_pan_active;
static bool wmap_ui_pan_moved;
static int wmap_ui_pan_x;
static int wmap_ui_pan_y;

static tig_window_handle_t wmap_marker_window = TIG_WINDOW_HANDLE_INVALID;
static int wmap_marker_swallow_event = -1;
static bool wmap_marker_swallow_enter;
static bool wmap_marker_text_focused;
static WmapUiMode wmap_marker_mode;
static WmapNote wmap_marker_draft;
static char wmap_marker_description[MAP_MARKER_TEXT_SIZE];
static TextEdit wmap_marker_edit = { 0, wmap_marker_description, sizeof(wmap_marker_description), wmap_marker_text_enter, wmap_marker_text_changed, NULL };
// 0x64E030
static tig_color_t wmap_unused_blue_color;

// 0x64E034
static tig_color_t wmap_ui_selected_waypoint_color;

// 0x64E03C
static tig_color_t wmap_ui_color_key;

// 0x64E048
static WmapRoute wmap_ui_routes[WMAP_ROUTE_TYPE_COUNT];

// 0x64E7E8
static WmapCoords wmap_ui_mouse_coordinates;

// 0x64E7F0
static mes_file_handle_t wmap_ui_worldmap_mes_file;

// 0x64E7F4
static TigVideoBuffer* wmap_ui_coordinate_text_buffer;

// 0x64E7F8
static TownMapInfo wmap_ui_tmi;

// 0x64E828
static int8_t wmap_ui_world_path_rotations[5000];

// 0x64FBB0
static TigRect wmap_ui_compass_base_bounds;

// 0x64FBC8
static PcLens wmap_ui_pc_lens;

// 0x64FBD8
static WmapNote wmap_ui_world_notes[200];

// 0x65E968
static int wmap_ui_current_path_step;

// 0x65E970
static tig_color_t wmap_unused_green_color;

// 0x65E974
static tig_color_t wmap_ui_waypoint_color;

// 0x65E978
static WmapNote wmap_ui_town_notes[200];

// 0x66D6F8
static int wmap_ui_sector_x_origin;

// 0x66D6FC
static mes_file_handle_t wmap_ui_worldmap_info_mes_file;

// 0x66D708
static TigRect wmap_ui_coordinate_text_rect;

// 0x66D718
static WmapNote wmap_ui_new_note;

// 0x66D848
static tig_sound_handle_t wmap_ui_music_handle;

// 0x66D850
static int64_t wmap_ui_known_area_search_radius;

// 0x66D858
static int wmap_ui_offset_x;

// 0x66D85C
static int wmap_ui_offset_y;

// 0x66D860
static bool wmap_ui_initialized;

// 0x66D864
static bool wmap_ui_created;

// 0x66D868
static WmapUiMode wmap_ui_mode;

// 0x66D86C
static int wmap_ui_num_world_notes;

// 0x66D870
static int wmap_ui_num_town_notes;

// 0x66D874
static int wmap_ui_townmap;

// 0x66D878
static int wmap_ui_town_notes_count;

// 0x66D87C
static int wmap_ui_world_map_id;

// 0x66D880
static int wmap_ui_world_travel_available;

// 0x66D888
static int64_t wmap_ui_obj;

// 0x66D890
static int wmap_note_type_icon_max_width;

// 0x66D894
static int wmap_note_type_icon_max_height;

// 0x66D898
static bool wmap_ui_textedit_focused;

// 0x66D89C
static WmapNoteType wmap_ui_selected_note_icon;

// 0x66D8A0
static int wmap_ui_note_width;

// 0x66D8A4
static int wmap_ui_note_height;

// 0x66D8A8
static bool wmap_ui_navigating;

// 0x66D8AC
static int wmap_ui_state;

// Keep the displayed estimate stable while the accelerated journey advances.
static bool wmap_ui_travel_time_valid;
static int wmap_ui_travel_time_hours;
static tig_timestamp_t wmap_ui_travel_time_updated;


// 0x66D8B0
static int wmap_ui_compass_arrow_num_frames;

// 0x66D8B4
static int wmap_ui_nav_cvr_width;

// 0x66D8B8
static int wmap_ui_nav_cvr_height;

// 0x66D8BC
static char wmap_ui_module_name[256];

// 0x66D9BC
static bool wmap_ui_info_loaded;

// 0x66D9C0
static const char* wmap_ui_action;

// 0x66D9C4
static bool wmap_ui_encounter;

// 0x66D9C8
static bool wmap_ui_selecting;

// 0x66D9CC
static int wmap_legacy_initialization_seen;

// 0x66D9D0
static tig_timestamp_t wmap_ui_last_scroll_time;

// 0x66D9D4
static WmapNote* wmap_ui_edited_note;

// NOTE: Rare case - this array is put behind uninitialized stuff because it
// obtains references to many internal state variables.
//
// 0x5C9228
static WmapInfo wmap_ui_mode_info[WMAP_UI_MODE_COUNT] = {
    {
        6,
        { 150, 52, 501, 365 },
        { 150, 52, 501, 365 },
        0,
        0,
        0,
        0,
        0,
        0,
        { 0, 0 },
        1,
        wmap_ui_current_rect_refresh,
        wmap_world_refresh_rect,
        wmap_ui_scroll_rect_refresh,
        -1,
        0,
        0,
        0,
        0,
        { 0 },
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        wmap_ui_world_notes,
        200,
        &wmap_ui_num_world_notes,
        200,
        -1,
        0,
        0,
        { 0 },
        { 0 },
        NULL,
        wmap_ui_mouse_to_map_coords,
    },
    {
        0,
        { 218, 52, 365, 365 },
        { 218, 52, 365, 365 },
        0,
        0,
        0,
        0,
        0,
        0,
        { 0, 0 },
        0,
        wmap_ui_overview_draw,
        NULL,
        NULL,
        -1,
        0,
        0,
        0,
        0,
        { 0 },
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        wmap_ui_world_notes,
        200,
        &wmap_ui_num_world_notes,
        200,
        -1,
        0,
        0,
        { 0 },
        { 0 },
        NULL,
        wmap_ui_overview_to_map_coords,
    },
    {
        4,
        { 150, 52, 501, 365 },
        { 150, 52, 501, 365 },
        0,
        0,
        0,
        0,
        0,
        0,
        { 0, 0 },
        1,
        wmap_ui_current_rect_refresh,
        wmap_town_refresh_rect,
        wmap_ui_viewport_scroll,
        -1,
        0,
        0,
        0,
        0,
        { 0 },
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        wmap_ui_town_notes,
        200,
        &wmap_ui_num_town_notes,
        200,
        -1,
        0,
        0,
        { 0 },
        { 0 },
        NULL,
        wmap_ui_screen_to_scrolled_coords,
    }
};

// 0x55F8D0
bool wmap_ui_init(GameInitInfo* init_info)
{

    ui_asset_warmup_register(138);
    if (settings_get_value(&settings, WORLDMAP_TOOLBAR_KEY)) ui_asset_warmup_register(NAME_MAP_MAIN_XL);
    const int interface_art_numbers[] = { 196, 197, 198, 217 };
    for (size_t index = 0; index < SDL_arraysize(interface_art_numbers); index++) ui_asset_warmup_register(interface_art_numbers[index]);

    MesFileEntry mes_file_entry;
    TigArtFrameData art_frame_data;
    TigArtAnimData art_anim_data;
    tig_art_id_t art_id;
    int index;

    (void)init_info;

    if (!mes_load("mes\\WorldMap.mes", &wmap_ui_worldmap_mes_file)) {
        tig_debug_printf("wmap_ui_init: ERROR: failed to load msg file: mes\\WorldMap.mes!");
        exit(EXIT_FAILURE);
    }

    mes_file_entry.num = 499; // "Action"
    if (mes_search(wmap_ui_worldmap_mes_file, &mes_file_entry)) {
        mes_get_msg(wmap_ui_worldmap_mes_file, &mes_file_entry);
        wmap_ui_action = mes_file_entry.str;
    }

    wmap_ui_mode = WMAP_UI_MODE_WORLD;
    wmap_ui_state = WMAP_UI_STATE_NORMAL;

    if (tig_art_interface_id_create(217, 0, 0, 0, &wmap_ui_nav_cvr_art_id) != TIG_OK
        || tig_art_frame_data(wmap_ui_nav_cvr_art_id, &art_frame_data) != TIG_OK) {
        tig_debug_printf("WMapUI: ERROR: Can't build button cover art info!\n");
        return false;
    }

    wmap_ui_nav_cvr_width = art_frame_data.width;
    wmap_ui_nav_cvr_height = art_frame_data.height;
    wmap_unused_green_color = tig_color_make(50, 160, 50);
    wmap_ui_waypoint_color = tig_color_make(160, 50, 50);
    wmap_unused_blue_color = tig_color_make(50, 50, 235);
    wmap_ui_selected_waypoint_color = tig_color_make(50, 50, 110);

    for (index = 0; index < WMAP_UI_MODE_COUNT; index++) {
        if (wmap_ui_mode_info[index].rect.y > wmap_ui_window_frame.y) {
            wmap_ui_mode_info[index].rect.y -= wmap_ui_window_frame.y;
        }
    }

    wmap_ui_canvas_frame.y -= wmap_ui_window_frame.y;
    wmap_ui_lat_long_frame[0].y -= wmap_ui_window_frame.y;
    wmap_ui_lat_long_frame[1].y -= wmap_ui_window_frame.y;
    wmap_ui_compass_rect.y -= wmap_ui_window_frame.y;
    wmap_ui_pc_lens_frame.y -= wmap_ui_window_frame.y;
    wmap_ui_color_key = tig_color_make(0, 0, 255);

    for (index = 0; index < WMAP_NOTE_TYPE_COUNT; index++) {
        wmap_note_type_info[index].enabled = true;
        tig_art_interface_id_create(wmap_note_type_info[index].num, 0, 0, 0, &art_id);
        wmap_note_type_info[index].art_id = art_id;

        if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
            return false;
        }

        if (tig_art_frame_data(art_id, &art_frame_data) == TIG_OK) {
            wmap_note_type_info[index].width = art_frame_data.width;
            wmap_note_type_info[index].height = art_frame_data.height;

            if (art_frame_data.width > wmap_note_type_icon_max_width) {
                wmap_note_type_icon_max_width = art_frame_data.width;
            }

            if (art_frame_data.height > wmap_note_type_icon_max_height) {
                wmap_note_type_icon_max_height = art_frame_data.height;
            }
        }
    }

    wmap_note_type_info[WMAP_NOTE_TYPE_NOTE].color = tig_color_make(255, 255, 255);
    wmap_note_type_info[WMAP_NOTE_TYPE_SKULL].color = tig_color_make(255, 0, 0);
    wmap_note_type_info[WMAP_NOTE_TYPE_CHEST].color = tig_color_make(255, 255, 0);
    wmap_note_type_info[WMAP_NOTE_TYPE_QUEST].color = tig_color_make(0, 255, 0);
    wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].color = tig_color_make(150, 150, 150);
    wmap_note_type_info[WMAP_NOTE_TYPE_LOC].color = tig_color_make(250, 250, 250);

    mes_file_entry.num = 655;
    if (mes_search(wmap_ui_worldmap_mes_file, &mes_file_entry)) {
        char* str;
        int r, g, b;

        mes_get_msg(wmap_ui_worldmap_mes_file, &mes_file_entry);
        tig_str_parse_set_separator(',');
        str = mes_file_entry.str;
        tig_str_parse_value(&str, &r);
        tig_str_parse_value(&str, &g);
        tig_str_parse_value(&str, &b);
        wmap_note_type_info[WMAP_NOTE_TYPE_LOC].color = tig_color_make(r, g, b);
    }

    wmap_note_type_info[WMAP_NOTE_TYPE_WAYPOINT].color = tig_color_make(150, 150, 150);

    wmap_ui_coordinate_text_rect.x = 0;
    wmap_ui_coordinate_text_rect.y = wmap_ui_note_bounds.y;
    wmap_ui_coordinate_text_rect.width = wmap_ui_note_bounds.width + wmap_note_type_icon_max_width + 3;
    wmap_ui_coordinate_text_rect.height = wmap_ui_note_bounds.height;

    wmap_ui_note_bounds.x = wmap_note_type_icon_max_width + 3;

    if (tig_art_interface_id_create(217, 0, 0, 0, &art_id) != TIG_OK
        || tig_art_frame_data(art_id, &art_frame_data) != TIG_OK) {
        return false;
    }

    wmap_ui_nav_cvr_frame.y = 382 - wmap_ui_window_frame.y;
    wmap_ui_nav_cvr_frame.width = art_frame_data.width;
    wmap_ui_nav_cvr_frame.height = art_frame_data.height;

    if (tig_art_interface_id_create(196, 0, 0, 0, &art_id) != TIG_OK
        || tig_art_frame_data(art_id, &art_frame_data) != TIG_OK) {
        return false;
    }

    wmap_ui_compass_base_bounds.x = 0;
    wmap_ui_compass_base_bounds.y = 0;
    wmap_ui_compass_base_bounds.width = art_frame_data.width;
    wmap_ui_compass_base_bounds.height = art_frame_data.height;

    if (tig_art_interface_id_create(197, 0, 0, 0, &art_id) != TIG_OK
        || tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        return false;
    }

    wmap_ui_compass_arrow_num_frames = art_anim_data.num_frames;

    for (index = 0; index < WMAP_ROUTE_TYPE_COUNT; index++) {
        wmap_ui_routes[index].length = 0;
        wmap_ui_routes[index].default_travel_value = 1;
        wmap_ui_routes[index].waypoint_edit_enabled = 1;
    }

    wmap_ui_initialized = true;
    return true;
}

// 0x55FF80
void wmap_ui_exit(void)
{
    int mode;
    int tile;
    wmap_marker_close();

    wmap_ui_mode = WMAP_UI_MODE_WORLD;
    wmap_ui_state = WMAP_UI_STATE_NORMAL;

    for (mode = 0; mode < WMAP_UI_MODE_COUNT; mode++) {
        if (wmap_ui_mode_info[mode].num_notes != NULL) {
            for (int note = 0; note < *wmap_ui_mode_info[mode].num_notes; note++) wmap_ui_note_buffer_free(&wmap_ui_mode_info[mode].notes[note]);
        }
        if (wmap_ui_mode_info[mode].tiles != NULL) {
            for (tile = 0; tile < wmap_ui_mode_info[mode].num_tiles; tile++) {
                if (wmap_ui_mode_info[mode].tiles[tile].reserved_data != NULL) {
                    FREE(wmap_ui_mode_info[mode].tiles[tile].reserved_data);
                }
            }

            FREE(wmap_ui_mode_info[mode].tiles);
            wmap_ui_mode_info[mode].tiles = NULL;

            wmap_ui_mode_info[mode].num_tiles = 0;
        }
    }

    wmap_ui_destroy();
    wmap_ui_town_tiles_free();
    mes_unload(wmap_ui_worldmap_mes_file);
    wmap_ui_initialized = false;
}

// 0x560010
void wmap_ui_town_tiles_free(void)
{
    wmap_ui_town_notes_save();
    wmap_ui_mode_info[WMAP_UI_MODE_TOWN].num_tiles = 0;
    if (wmap_ui_mode_info[WMAP_UI_MODE_TOWN].tiles != NULL) {
        FREE(wmap_ui_mode_info[WMAP_UI_MODE_TOWN].tiles);
        wmap_ui_mode_info[WMAP_UI_MODE_TOWN].tiles = NULL;
    }
}

// 0x560040
void wmap_ui_town_notes_save(void)
{
    int index;
    bool success = true;
    char path[TIG_MAX_PATH];
    TigFile* stream;

    if (wmap_ui_town_notes_count == 0) {
        return;
    }

    if (wmap_ui_town_notes_count >= townmap_count()) {
        wmap_ui_town_notes_count = 0;
        return;
    }

    snprintf(path, sizeof(path),
        "%s\\%s.tmn",
        "Save\\Current",
        townmap_name(wmap_ui_town_notes_count));

    stream = tig_file_fopen(path, "wb");
    if (stream == NULL) {
        tig_debug_printf("wmap_ui_town_notes_save(): Error creating file %s\n", path);
        return;
    }

    if (tig_file_fwrite(&wmap_ui_num_town_notes, sizeof(wmap_ui_num_town_notes), 1, stream) == 1) {
        if (tig_file_fwrite(&(wmap_ui_mode_info[WMAP_UI_MODE_TOWN].next_note_id), sizeof(wmap_ui_mode_info[WMAP_UI_MODE_TOWN].next_note_id), 1, stream) == 1) {
            for (index = 0; index < wmap_ui_num_town_notes; index++) {
                if (!wmap_ui_town_note_save(&(wmap_ui_town_notes[index]), stream)) {
                    success = false;
                    break;
                }
            }
        } else {
            // FIXME: Should set error?
        }
    } else {
        success = false;
    }

    tig_file_fclose(stream);

    if (!success) {
        tig_debug_printf("wmap_ui_town_notes_save(): Error writing data!\n");
    }
}

// 0x560150
void wmap_ui_destroy(void)
{
    int mode;
    int tile;

    for (mode = 0; mode < WMAP_UI_MODE_COUNT; mode++) {
        wmap_ui_mode = mode;

        if ((wmap_ui_mode_info[mode].flags & 0x1) != 0) {
            if (tig_video_buffer_destroy(wmap_ui_mode_info[mode].video_buffer) == TIG_OK) {
                wmap_ui_mode_info[mode].flags &= ~0x1;
            } else {
                tig_debug_printf("WMapUI: Destroy: ERROR: Video Buffer Destroy FAILED!\n");
            }
        }

        for (tile = 0; tile < wmap_ui_mode_info[mode].num_tiles; tile++) {
            wmap_ui_tile_art_unload(tile);
        }
    }

    wmap_ui_mode = WMAP_UI_MODE_WORLD;
    wmap_ui_world_map_id = 0;
    wmap_ui_module_name[0] = '\0';

    if (wmap_ui_info_loaded) {
        mes_unload(wmap_ui_worldmap_info_mes_file);
    }

    wmap_ui_info_loaded = false;
}

// 0x560200
void wmap_ui_reset(void)
{
    int index;

    if (wmap_ui_initialized) {
        wmap_ui_close();

        for (index = 0; index < WMAP_UI_MODE_COUNT; index++) {
            if (wmap_ui_mode_info[index].num_notes != NULL) {
                for (int note = 0; note < *wmap_ui_mode_info[index].num_notes; note++) {
                    wmap_ui_note_buffer_free(&wmap_ui_mode_info[index].notes[note]);
                }
                *wmap_ui_mode_info[index].num_notes = 0;
            }
            wmap_ui_mode_info[index].hovered_note_id = -1;
        }

        for (index = 0; index < WMAP_ROUTE_TYPE_COUNT; index++) {
            wmap_ui_routes[index].length = 0;
            wmap_ui_routes[index].default_travel_value = 1;
            wmap_ui_routes[index].waypoint_edit_enabled = 1;
        }

        wmap_ui_selected_waypoint = -1;
        wmap_ui_mode = WMAP_UI_MODE_WORLD;
        wmap_ui_state = WMAP_UI_STATE_NORMAL;
        wmap_ui_current_path_step = 0;
        wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].length = 0;
        wmap_ui_encounter = false;
    }
}

// 0x560280
bool wmap_ui_save(TigFile* stream)
{
    int mode;
    int note;
    int num_notes;
    int cnt;

    if (stream == NULL) {
        return false;
    }

    for (mode = 0; mode < WMAP_UI_MODE_COUNT; mode++) {
        if ((wmap_ui_mode_info[mode].flags & 0x2) != 0) {
            if (wmap_ui_mode_info[mode].num_notes != NULL) {
                num_notes = *wmap_ui_mode_info[mode].num_notes;
            } else {
                num_notes = 0;
            }

            cnt = num_notes;
            for (note = 0; note < num_notes; note++) {
                if ((wmap_ui_mode_info[mode].notes[note].flags & 0x2) != 0) {
                    cnt--;
                }
            }

            if (tig_file_fwrite(&cnt, sizeof(cnt), 1, stream) != 1) {
                return false;
            }

            for (note = 0; note < num_notes; note++) {
                if ((wmap_ui_mode_info[mode].notes[note].flags & 0x2) == 0) {
                    if (!wmap_ui_town_note_save(&(wmap_ui_mode_info[mode].notes[note]), stream)) {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

// 0x560350
bool wmap_ui_town_note_save(WmapNote* note, TigFile* stream)
{
    MapMarkerRecord record = { note->id, note->flags, note->coords.x, note->coords.y,
        { note->bounds.x, note->bounds.y, note->bounds.width, note->bounds.height }, note->type };
    memcpy(record.text, note->str, sizeof(record.text));
    return map_marker_record_save(&record, stream);
}

// 0x560440
bool wmap_ui_rect_write(TigRect* rect, TigFile* stream)
{
    if (tig_file_fwrite(&(rect->x), sizeof(rect->x), 1, stream) != 1) return false;
    if (tig_file_fwrite(&(rect->y), sizeof(rect->y), 1, stream) != 1) return false;
    if (tig_file_fwrite(&(rect->width), sizeof(rect->width), 1, stream) != 1) return false;
    if (tig_file_fwrite(&(rect->height), sizeof(rect->height), 1, stream) != 1) return false;

    return true;
}

// 0x5604B0
bool wmap_ui_load(GameLoadInfo* load_info)
{
    int mode;
    int cnt;
    int note;
    int64_t loc;
    int64_t sector_id;

    if (load_info->stream == NULL) {
        return false;
    }

    for (mode = 0; mode < WMAP_UI_MODE_COUNT; mode++) {
        if ((wmap_ui_mode_info[mode].flags & 0x2) != 0) {
            tig_debug_printf("WMapUI: Reading Saved Notes.\n");

            if (tig_file_fread(&cnt, sizeof(cnt), 1, load_info->stream) != 1) {
                return false;
            }
            if (cnt < 0 || cnt > SDL_arraysize(wmap_ui_world_notes)) return false;

            tig_debug_printf("WMapUI: Reading Saved Notes: %d.\n", cnt);

            if (wmap_ui_mode_info[mode].num_notes == NULL) {
                tig_debug_printf("WMapUI: Load: ERROR: Note Data Doesn't Match!\n");
                exit(EXIT_FAILURE);
            }

            *wmap_ui_mode_info[mode].num_notes = cnt;

            for (note = 0; note < cnt; note++) {
                tig_debug_printf("WMapUI: Reading Individual Note: %d.\n", note);

                if (!wmap_ui_town_note_load(&(wmap_ui_mode_info[mode].notes[note]), load_info->stream)) {
                    return false;
                }
            }
        }
    }

    loc = obj_field_int64_get(player_get_local_pc_obj(), OBJ_F_LOCATION);
    sector_id = sector_id_from_loc(loc);
    wmap_ui_notify_sector_changed(player_get_local_pc_obj(), sector_id);

    return true;
}

// 0x5605C0
bool wmap_ui_town_note_load(WmapNote* note, TigFile* stream)
{
    MapMarkerRecord record;
    if (!map_marker_record_load(&record, stream)) return false;
    note->id = record.id;
    note->flags = record.flags;
    note->coords = (WmapCoords){ record.x, record.y };
    note->bounds = (TigRect){ record.bounds[0], record.bounds[1], record.bounds[2], record.bounds[3] };
    note->type = record.type;
    memcpy(note->str, record.text, sizeof(note->str));
    note->video_buffer = NULL;
    note->location = 0;

    return true;
}

// 0x5606B0
bool wmap_ui_rect_read(TigRect* rect, TigFile* stream)
{
    if (tig_file_fread(&(rect->x), sizeof(rect->x), 1, stream) != 1) return false;
    if (tig_file_fread(&(rect->y), sizeof(rect->y), 1, stream) != 1) return false;
    if (tig_file_fread(&(rect->width), sizeof(rect->width), 1, stream) != 1) return false;
    if (tig_file_fread(&(rect->height), sizeof(rect->height), 1, stream) != 1) return false;

    return true;
}

// 0x560720
void wmap_ui_encounter_start(void)
{
    wmap_ui_encounter = true;
    anim_ui_event_add_delay(ANIM_UI_EVENT_TYPE_END_RANDOM_ENCOUNTER, 0, 4000);
}

// 0x560740
bool wmap_ui_is_encounter(void)
{
    return wmap_ui_encounter;
}

// 0x560750
void wmap_ui_encounter_end(void)
{
    wmap_ui_encounter = false;
}

// 0x560760
void wmap_ui_open(void)
{
    wmap_ui_spell = -1;
    wmap_ui_selecting = false;
    wmap_ui_obj = OBJ_HANDLE_NULL;
    wmap_ui_open_internal();
}

// 0x560790
void wmap_ui_select(int64_t obj, int spell)
{
    wmap_ui_obj = obj;
    wmap_ui_current_path_step = 0;
    wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].length = 0;
    wmap_ui_selecting = true;
    wmap_ui_world_travel_available = 0;
    wmap_ui_spell = spell;
    wmap_ui_open_internal();
}

// 0x5607E0
void wmap_ui_open_internal(void)
{
    int64_t pc_obj;
    int64_t loc;
    int64_t sector_id;
    MesFileEntry mes_file_entry;
    UiMessage ui_message;

    if (wmap_ui_created) {
        wmap_ui_close();
        return;
    }

    pc_obj = player_get_local_pc_obj();

    if (pc_obj == OBJ_HANDLE_NULL
        || critter_is_dead(pc_obj)
        || critter_is_unconscious(pc_obj)) {
        return;
    }

    if (wmap_ui_is_encounter()) {
        mes_file_entry.num = 602; // "You cannot access the World Map during an encounter."
        mes_get_msg(wmap_ui_worldmap_mes_file, &mes_file_entry);

        ui_message.type = UI_MSG_TYPE_FEEDBACK;
        ui_message.str = mes_file_entry.str;
        intgame_message_window_display_msg(&ui_message);

        return;
    }

    loc = obj_field_int64_get(pc_obj, OBJ_F_LOCATION);
    sector_id = sector_id_from_loc(loc);
    if (sector_is_blocked(sector_id)) {
        mes_file_entry.num = 605; // "The World Map is not available."
        mes_get_msg(wmap_ui_worldmap_mes_file, &mes_file_entry);

        ui_message.type = UI_MSG_TYPE_FEEDBACK;
        ui_message.str = mes_file_entry.str;
        intgame_message_window_display_msg(&ui_message);

        return;
    }

    if (combat_critter_is_combat_mode_active(pc_obj)) {
        if (!combat_can_exit_combat_mode(pc_obj)) {
            mes_file_entry.num = 600; // "You cannot access the World Map during combat."
            mes_get_msg(wmap_ui_worldmap_mes_file, &mes_file_entry);

            ui_message.type = UI_MSG_TYPE_FEEDBACK;
            ui_message.str = mes_file_entry.str;
            intgame_message_window_display_msg(&ui_message);

            return;
        }
        combat_critter_deactivate_combat_mode(pc_obj);
    }

    if (!wmap_load_worldmap_info()) {
        if (wmap_ui_selecting) {
            magictech_exit_to_world_area(pc_obj);
            return;
        }

        mes_file_entry.num = 605; // "The World Map is not available."
        mes_get_msg(wmap_ui_worldmap_mes_file, &mes_file_entry);

        ui_message.type = UI_MSG_TYPE_FEEDBACK;
        ui_message.str = mes_file_entry.str;
        intgame_message_window_display_msg(&ui_message);

        return;
    }

    if (wmap_ui_selecting) {
        wmap_ui_townmap = TOWNMAP_NONE;
    } else {
        wmap_ui_travel_context_refresh();
    }

    if (wmap_ui_created) {
        wmap_ui_close();
        return;
    }

    if (!intgame_mode_set(INTGAME_MODE_MAIN)) {
        return;
    }

    if (!intgame_mode_set(INTGAME_MODE_WMAP)) {
        return;
    }

    anim_object_interrupt_by_priority(player_get_local_pc_obj(), 4, false, true);

    if (!wmap_ui_create()) {
        wmap_ui_close();
        intgame_mode_set(INTGAME_MODE_MAIN);

        if (wmap_ui_selecting) {
            magictech_exit_to_world_area(pc_obj);
            return;
        }

        mes_file_entry.num = 605; // "The World Map is not available."
        mes_get_msg(wmap_ui_worldmap_mes_file, &mes_file_entry);

        ui_message.type = UI_MSG_TYPE_FEEDBACK;
        ui_message.str = mes_file_entry.str;
        intgame_message_window_display_msg(&ui_message);

        return;
    }

    if (wmap_ui_townmap != TOWNMAP_NONE) {
        wmap_ui_mode_set(WMAP_UI_MODE_TOWN);
    } else if (tig_net_is_active()) {
        wmap_ui_close();
    }

    wmap_ui_mode_info[wmap_ui_mode].refresh();

    ui_toggle_primary_button(UI_PRIMARY_BUTTON_WORLDMAP, false);
}

// 0x560AA0
bool wmap_load_worldmap_info(void)
{
    const char* name;
    char path[TIG_MAX_PATH];
    MesFileEntry mes_file_entry;
    char* str;
    int worldmap;
    int idx;
    WmapTile* tile;
    WmapInfo* wmap_info;
    int map_keyed_to;

    name = gamelib_current_module_name_get();
    if (*name == '\0') {
        return false;
    }

    if (strcmp(wmap_ui_module_name, name) == 0) {
        return false;
    }

    strcpy(wmap_ui_module_name, name);

    tig_str_parse_set_separator(',');

    snprintf(path, sizeof(path), "WorldMap\\WorldMap.mes");
    if (mes_load(path, &wmap_ui_worldmap_info_mes_file)) {
        mes_file_entry.num = 20;
        mes_get_msg(wmap_ui_worldmap_info_mes_file, &mes_file_entry);
        str = mes_file_entry.str;
        tig_str_parse_value(&str, &wmap_ui_offset_x);
        tig_str_parse_value(&str, &wmap_ui_offset_y);

        mes_file_entry.num = 50;
        if (map_get_worldmap(map_current_map(), &worldmap) && worldmap != -1) {
            mes_file_entry.num += worldmap;
        }
        mes_get_msg(wmap_ui_worldmap_info_mes_file, &mes_file_entry);
        str = mes_file_entry.str;
        tig_str_parse_value(&str, &wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_hor_tiles);
        tig_str_parse_value(&str, &wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_vert_tiles);

        if (wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tiles == NULL) {
            wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_tiles = wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_hor_tiles * wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_vert_tiles;
            wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tiles = (WmapTile*)CALLOC(wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_tiles, sizeof(WmapTile));

            // FIXME: Meaningless, calloc already zeroes it out.
            for (idx = 0; idx < wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_tiles; idx++) {
                wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tiles[idx].flags = 0;
            }
        }

        tig_str_parse_str_value(&str, wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tile_directory, sizeof(wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tile_directory));

        if (!wmap_ui_tile_art_lock(0)) {
            tig_debug_printf("wmap_load_worldmap_info: ERROR: wmap_ui_mode_tile_art_lock failed!\n");
            exit(EXIT_FAILURE);
        }

        wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tile_width = wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tiles->rect.width;
        wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tile_height = wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tiles->rect.height;
        wmap_ui_mode_info[WMAP_UI_MODE_WORLD].map_width = wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tile_width * wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_hor_tiles;
        wmap_ui_mode_info[WMAP_UI_MODE_WORLD].map_height = wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tile_height * wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_vert_tiles;
        wmap_ui_tile_art_unlock(0);
        wmap_ui_tile_art_unload(0);

        tig_str_parse_named_str_value(&str, "ZoomedName:", wmap_ui_mode_info[WMAP_UI_MODE_WORLD].str, sizeof(wmap_ui_mode_info[WMAP_UI_MODE_WORLD].str));

        wmap_ui_world_map_id = 0;
        if (tig_str_parse_named_value(&str, "MapKeyedTo:", &map_keyed_to)) {
            wmap_ui_world_map_id = map_keyed_to;
        }

        for (idx = 0; idx < wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_tiles; idx++) {
            tile = &(wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tiles[idx]);
            if ((tile->flags & 0x02) == 0) {
                tile->flags = 0x02;
                tile->rect.width = wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tile_width;
                tile->rect.height = wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tile_height;
                tile->reserved_data = NULL;
                tile->reserved_data_size = 0;
            }
        }
    } else {
        wmap_ui_offset_x = 0;
        wmap_ui_offset_y = 0;

        wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_hor_tiles = 8;
        wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_vert_tiles = 8;
        wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tile_width = 250;
        wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tile_height = 250;
        wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tile_directory[0] = '\0';
        wmap_ui_mode_info[WMAP_UI_MODE_WORLD].map_width = wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tile_width * wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_hor_tiles;
        wmap_ui_mode_info[WMAP_UI_MODE_WORLD].map_height = wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tile_height * wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_vert_tiles;
        wmap_ui_mode_info[WMAP_UI_MODE_WORLD].str[0] = '\0';
    }

    for (idx = 0; idx < WMAP_UI_MODE_COUNT; idx++) {
        wmap_info = &(wmap_ui_mode_info[idx]);
        if (wmap_info->rect.width > 0) {
            wmap_info->viewport_half_width = wmap_info->rect.width / 2;
            wmap_info->viewport_half_height = wmap_info->rect.height / 2;
            wmap_info->center_x_max = wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_hor_tiles * wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tile_width - wmap_info->viewport_half_width;
            wmap_info->center_y_max = wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_vert_tiles * wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tile_height - wmap_info->viewport_half_height;
        }

        if (wmap_info->num_notes != NULL && idx != 2) {
            *wmap_info->num_notes = 0;
        }

        wmap_info->hovered_note_id = -1;
    }

    wmap_ui_selected_waypoint = -1;
    wmap_ui_note_width = wmap_note_type_info[WMAP_NOTE_TYPE_LOC].width;
    wmap_ui_note_height = wmap_note_type_info[WMAP_NOTE_TYPE_LOC].height;
    wmap_ui_known_area_search_radius = 320;
    wmap_ui_info_loaded_hook_stub();

    wmap_ui_info_loaded = true;

    return true;
}

// 0x560EE0
void wmap_ui_info_loaded_hook_stub(void)
{
}

// 0x560EF0
void wmap_ui_travel_context_refresh(void)
{
    wmap_ui_world_travel_available = 0;
    wmap_ui_townmap = townmap_get(sector_id_from_loc(obj_field_int64_get(player_get_local_pc_obj(), OBJ_F_LOCATION)));
    if (map_current_map() == wmap_ui_world_map_id) {
        if (wmap_ui_townmap == TOWNMAP_NONE) {
            wmap_ui_world_travel_available = 1;
        }
    }
}

// 0x560F40
void wmap_ui_close(void)
{
    int64_t pc_obj;
    int index;
    wmap_marker_close();
    wmap_marker_swallow_event = -1;

    wmap_ui_travel_time_valid = false;
    wmap_ui_pan_active = false;
    wmap_ui_pan_moved = false;

    if (!wmap_ui_created) {
        return;
    }

    pc_obj = player_get_local_pc_obj();
    if (pc_obj != OBJ_HANDLE_NULL
        && (obj_field_int32_get(pc_obj, OBJ_F_FLAGS) & OF_OFF) != 0) {
        critter_toggle_on_off(pc_obj);
        ai_group_attacks_enable(pc_obj);
    }

    if (intgame_mode_set(INTGAME_MODE_MAIN) && wmap_ui_created) {
        wmap_ui_note_edit_end(false);
        wmap_ui_state_set(WMAP_UI_STATE_NORMAL);
        intgame_pc_lens_do(PC_LENS_MODE_NONE, NULL);
        ambient_lighting_enable();
        intgame_button_destroy(&wmap_ui_navigate_button_info);
        intgame_apply_translucent_black(wmap_ui_window, false);
        intgame_big_window_unlock();

        wmap_ui_window = TIG_WINDOW_HANDLE_INVALID;
        for (index = 0; index < WMAP_NOTE_TYPE_COUNT; index++) {
            tig_font_destroy(wmap_note_type_info[index].font);
        }
        tig_video_buffer_destroy(wmap_ui_coordinate_text_buffer);
        wmap_ui_created = 0;
        wmap_ui_obj = OBJ_HANDLE_NULL;
        wmap_ui_spell = -1;
        wmap_ui_destroy();
        wmap_ui_town_tiles_free();
    }
}

// 0x560F90
int wmap_ui_is_created(void)
{
    return wmap_ui_created;
}

// 0x560FA0
bool wmap_ui_create(void)
{
    TigVideoBufferCreateInfo vb_create_info;
    TigArtAnimData art_anim_data;
    TigArtBlitInfo art_blit_info;
    TigRect src_rect;
    TigRect dst_rect;
    TigFont font_desc;
    int64_t loc;
    tig_art_id_t art_id;
    int index;
    WmapUiMode mode;

    if (wmap_ui_created) {
        return true;
    }

    if (map_current_map() != map_by_type(MAP_TYPE_START_MAP)) {
        int area;

        if (!map_get_area(map_current_map(), &area)) {
            tig_debug_printf("WMapUI: wmap_ui_create: ERROR: Failed to find matching WorldMap Position/area!\n");
            return false;
        }

        if (area == -1) {
            return false;
        }

        loc = area_get_location(area);
    } else {
        loc = obj_field_int64_get(player_get_local_pc_obj(), OBJ_F_LOCATION);
    }

    if (!wmap_load_worldmap_info()) {
        if (wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tile_directory[0] == '\0'
            && wmap_ui_townmap == TOWNMAP_NONE) {
            MesFileEntry mes_file_entry;
            UiMessage ui_message;

            mes_file_entry.num = 605;
            mes_get_msg(wmap_ui_worldmap_mes_file, &mes_file_entry);
            ui_message.type = UI_MSG_TYPE_FEEDBACK;
            ui_message.str = mes_file_entry.str;
            intgame_message_window_display_msg(&ui_message);
            return false;
        }

        if (is_default_wmap()) {
            wmap_ui_mode_info[wmap_ui_mode].scroll_x_max = 2000 - wmap_ui_mode_info[wmap_ui_mode].rect.width;
            wmap_ui_mode_info[wmap_ui_mode].scroll_y_max = 2000 - wmap_ui_mode_info[wmap_ui_mode].rect.height;
        } else {
            int64_t limit_x;
            int64_t limit_y;

            location_limits_get(&limit_x, &limit_y);
            wmap_ui_mode_info[wmap_ui_mode].scroll_x_max = (int)(limit_x / 64) - wmap_ui_mode_info[wmap_ui_mode].rect.width;
            wmap_ui_mode_info[wmap_ui_mode].scroll_y_max = (int)(limit_y / 64) - wmap_ui_mode_info[wmap_ui_mode].rect.height;
        }

        if (is_default_wmap()) {
            wmap_ui_sector_x_origin = 2000;
        } else {
            int64_t limit_x;
            int64_t limit_y;

            location_limits_get(&limit_x, &limit_y);
            wmap_ui_sector_x_origin = (int)(limit_x / 64);
        }

        if (wmap_legacy_initialization_seen == 0) {
            wmap_legacy_initialization_seen = 1;
        }
    }

    wmap_ui_textedit_focused = false;

    int panel_height = 400;
    if (settings_get_value(&settings, WORLDMAP_TOOLBAR_KEY)) {
        tig_art_interface_id_create(NAME_MAP_MAIN_XL, 0, 0, 0, &art_id);
        if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK) {
            panel_height = 450;
        } else {
            tig_debug_printf("wmap_ui_create: MapMain_Xl.art unavailable; using MapMain.art.\n");
            tig_art_interface_id_create(138, 0, 0, 0, &art_id);
        }
    } else {
        tig_art_interface_id_create(138, 0, 0, 0, &art_id);
    }

    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        tig_debug_printf("wmap_ui_create: ERROR: tig_art_anim_data failed!\n");
        exit(EXIT_SUCCESS); // FIXME: Should be EXIT_FAILURE.
    }

    src_rect.x = wmap_ui_window_frame.x;
    src_rect.y = 0;
    src_rect.width = wmap_ui_window_frame.width;
    src_rect.height = panel_height;

    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width = wmap_ui_window_frame.width;
    dst_rect.height = panel_height;

    art_blit_info.flags = 0;
    art_blit_info.art_id = art_id;
    art_blit_info.src_rect = &src_rect;
    art_blit_info.dst_rect = &dst_rect;

    if (!intgame_big_window_lock_height(wmap_ui_message_filter, &wmap_ui_window, panel_height)) {
        // FIXME: Message is misleading.
        tig_debug_printf("wmap_ui_create: ERROR: tig_art_anim_data failed!\n");
        exit(EXIT_SUCCESS); // FIXME: Should be EXIT_FAILURE.
    }

    tig_window_blit_art(wmap_ui_window, &art_blit_info);

    // CE: opt the world-map UI into the optional near-black see-through
    // alpha so the dark panel regions blend with the iso world below.
    // Disabled on close so the next big-window user starts opaque.
    intgame_apply_translucent_black(wmap_ui_window, true);

    font_desc.flags = TIG_FONT_NO_ALPHA_BLEND | TIG_FONT_SHADOW;
    tig_art_interface_id_create(840, 0, 0, 0, &(font_desc.art_id));

    for (index = 0; index < WMAP_NOTE_TYPE_COUNT; index++) {
        font_desc.color = wmap_note_type_info[index].color;
        tig_font_create(&font_desc, &(wmap_note_type_info[index].font));
    }

    for (index = 0; index < 2; index++) {
        intgame_button_create_ex(wmap_ui_window,
            &wmap_ui_window_frame,
            &(wmap_ui_zoom_button_info[index]),
            TIG_BUTTON_MOMENTARY);
    }

    intgame_button_create_ex(wmap_ui_window,
        &wmap_ui_window_frame,
        &wmap_ui_navigate_button_info,
        TIG_BUTTON_LATCH);
    intgame_button_create_ex(wmap_ui_window,
        &wmap_ui_window_frame,
        &wmap_ui_travel_button_info,
        wmap_ui_travel_button_flags | TIG_BUTTON_HIDDEN);

    vb_create_info.flags = TIG_VIDEO_BUFFER_CREATE_COLOR_KEY | TIG_VIDEO_BUFFER_CREATE_SYSTEM_MEMORY;
    vb_create_info.width = wmap_note_type_icon_max_width + 203;
    vb_create_info.height = 50;
    vb_create_info.background_color = wmap_ui_color_key;
    vb_create_info.color_key = wmap_ui_color_key;

    if (tig_video_buffer_create(&vb_create_info, &wmap_ui_coordinate_text_buffer) != TIG_OK) {
        tig_debug_printf("wmap_ui_create: ERROR: tig_video_buffer_create failed!\n");
        exit(EXIT_SUCCESS); // FIXME: Should be `EXIT_FAILURE`.
    }

    wmap_ui_pc_lens.window_handle = wmap_ui_window;
    wmap_ui_pc_lens.rect = &wmap_ui_pc_lens_frame;
    tig_art_interface_id_create(198, 0, 0, 0, &(wmap_ui_pc_lens.art_id));
    intgame_pc_lens_do(PC_LENS_MODE_PASSTHROUGH, &wmap_ui_pc_lens);

    for (index = area_count() - 1; index > 0; index--) {
        if (area_is_known(player_get_local_pc_obj(), index)) {
            wmap_ui_area_note_add(index);
        }
    }

    wmap_ui_position_from_location_set(loc);
    wmap_ui_scroll_origin_set(&(wmap_ui_mode_info[wmap_ui_mode].player_coordinates), false);

    mode = wmap_ui_mode;
    wmap_ui_mode = -1;
    wmap_ui_mode_set(mode);

    wmap_unused_selection = -1;
    wmap_ui_compass_arrow_frame_set(1);
    wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].waypoint_edit_enabled = wmap_ui_world_travel_available != 0;
    wmap_ui_created = true;

    return true;
}

// 0x561430
void wmap_ui_position_from_location_set(int64_t location)
{
    WmapCoords coords;
    int x;
    int y;

    wmap_ui_location_to_coords(location, &coords);

    x = coords.x;
    if (x > 99999 || x == 0) {
        x = 200;
    }

    y = coords.y;
    if (y > 99999 || y == 0) {
        y = 180;
    }

    wmap_ui_current_position_set(x, y);
}

// 0x561490
void wmap_ui_location_to_coords(int64_t location, WmapCoords* coords)
{
    coords->x = wmap_ui_sector_x_origin - ((location >> 6) & 0x3FFFFFF);
    coords->y = (location >> 32) >> 6;
}

// 0x5614C0
void wmap_ui_current_position_set(int x, int y)
{
    wmap_ui_mode_info[wmap_ui_mode].player_coordinates.x = x;
    wmap_ui_mode_info[wmap_ui_mode].player_coordinates.y = y;
}

// 0x5614F0
bool is_default_wmap(void)
{
    if (wmap_ui_mode != WMAP_UI_MODE_WORLD) {
        return false;
    }

    if (map_current_map() == map_by_type(MAP_TYPE_START_MAP)) {
        return false;
    }

    return true;
}

// 0x5615D0
bool wmap_ui_state_set(WmapUiState state)
{
    bool refresh = false;
    WmapInfo* wmap_info;
    tig_art_id_t art_id;
    int64_t pc_obj;
    int64_t loc;

    wmap_info = &(wmap_ui_mode_info[wmap_ui_mode]);
    if (wmap_ui_state != state) {
        switch (state) {
        case WMAP_UI_STATE_NORMAL:
            wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].waypoint_edit_enabled = wmap_ui_world_travel_available != 0;
            break;
        case WMAP_UI_STATE_MOVING:
            if (!wmap_ui_player_prepare_for_travel()) {
                return false;
            }

            intgame_pc_lens_do(PC_LENS_MODE_BLACKOUT, &wmap_ui_pc_lens);
            ambient_lighting_disable();
            pc_obj = player_get_local_pc_obj();
            if (pc_obj != OBJ_HANDLE_NULL
                && (obj_field_int32_get(pc_obj, OBJ_F_FLAGS) & OF_OFF) == 0) {
                critter_toggle_on_off(pc_obj);
                ai_stop_attacking(pc_obj);
            }
            gsound_stop_all(25);
            wmap_ui_music_handle = gsound_play_music_indefinitely("sound\\music\\arcanum.mp3", 25);
            break;
        case WMAP_UI_STATE_EDITING:
            wmap_ui_selected_waypoint = -1;
            tig_art_interface_id_create(21, 0, 0, 0, &art_id);
            tig_mouse_cursor_set_art_id(art_id);
            refresh = true;
            break;
        default:
            break;
        }

        switch (wmap_ui_state) {
        case WMAP_UI_STATE_WAYPOINT_DRAG:
            wmap_ui_selected_waypoint_snap();
            refresh = true;
            break;
        case WMAP_UI_STATE_MOVING:
            tig_sound_destroy(wmap_ui_music_handle);
            pc_obj = player_get_local_pc_obj();
            if (pc_obj != OBJ_HANDLE_NULL
                && (obj_field_int32_get(pc_obj, OBJ_F_FLAGS) & OF_OFF) != 0) {
                critter_toggle_on_off(pc_obj);
                ai_group_attacks_enable(pc_obj);
            }
            timeevent_clear_all_typed(TIMEEVENT_TYPE_WORLDMAP);
            wmap_ui_state = state;
            intgame_pc_lens_do(PC_LENS_MODE_PASSTHROUGH, &wmap_ui_pc_lens);
            wmap_ui_coords_to_location(&(wmap_info->player_coordinates), &loc);
            wmap_ui_teleport(loc);
            wmap_ui_travel_arrival_apply(loc);
            break;
        case WMAP_UI_STATE_EDITING:
            wmap_ui_note_edit_end(1);
            tig_art_interface_id_create(0, 0, 0, 0, &art_id);
            tig_mouse_cursor_set_art_id(art_id);
            break;
        case WMAP_UI_STATE_NAVIGATING:
            tig_button_state_change(wmap_ui_navigate_button_info.button_handle, TIG_BUTTON_STATE_RELEASED);
            break;
        }

        wmap_ui_state = state;

        if (refresh) {
            wmap_ui_travel_time_refresh();
            wmap_ui_mode_info[wmap_ui_mode].refresh();
        }
    }

    return true;
}

// 0x561800
void wmap_ui_coords_to_location(WmapCoords* coords, int64_t* loc_ptr)
{
    int x;
    int y;

    x = ((wmap_ui_sector_x_origin - coords->x) << 6) + 32;
    y = (coords->y << 6) + 32;
    *loc_ptr = location_make(x, y);
}

// 0x561860
bool wmap_ui_teleport(int64_t loc)
{
    TeleportData teleport_data;

    sector_flush(0);

    teleport_data.flags = TELEPORT_RENDER_LOCK;
    teleport_data.obj = player_get_local_pc_obj();
    teleport_data.loc = loc;
    teleport_data.map = map_by_type(MAP_TYPE_START_MAP);
    return teleport_do(&teleport_data);
}

// TODO: Review, split.
//
// 0x5618B0
bool wmap_ui_message_filter(TigMessage* msg)
{
    if (wmap_marker_input(msg)) return true;
    WmapInfo* wmap_info;
    MesFileEntry mes_file_entry;
    char str[48];
    UiMessage ui_message;
    bool inside = false;
    TigMessage tmp_msg = *msg;
    msg = &tmp_msg;

    // Convert mouse position from screen coordinate system to centered 800x600
    // area.
    if (msg->type == TIG_MESSAGE_MOUSE) {
        TigRect rect = { 0, 0, 800, 600 };
        hrp_apply(&rect, GRAVITY_CENTER_HORIZONTAL | GRAVITY_CENTER_VERTICAL);
        msg->data.mouse.x -= rect.x;
        msg->data.mouse.y -= rect.y;
    }

    wmap_ui_handle_scroll();

    wmap_info = &(wmap_ui_mode_info[wmap_ui_mode]);

    if (msg->type == TIG_MESSAGE_MOUSE) {
        switch (msg->data.mouse.event) {
        case TIG_MESSAGE_MOUSE_RIGHT_BUTTON_DOWN:
            if (msg->data.mouse.repeat) {
                return wmap_ui_pan_active;
            }
            wmap_ui_pan_active = wmap_ui_state != WMAP_UI_STATE_MOVING
                && wmap_ui_state != WMAP_UI_STATE_WAYPOINT_DRAG
                && msg->data.mouse.x >= wmap_info->interactive_rect.x
                && msg->data.mouse.y >= wmap_info->interactive_rect.y
                && msg->data.mouse.x < wmap_info->interactive_rect.x + wmap_info->interactive_rect.width
                && msg->data.mouse.y < wmap_info->interactive_rect.y + wmap_info->interactive_rect.height;
            wmap_ui_pan_moved = false;
            wmap_ui_pan_x = msg->data.mouse.x;
            wmap_ui_pan_y = msg->data.mouse.y;
            return wmap_ui_pan_active;
        case TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN:
            if (wmap_ui_pan_active) {
                return true;
            }
            if (msg->data.mouse.x < wmap_info->interactive_rect.x
                || msg->data.mouse.y < wmap_info->interactive_rect.y
                || msg->data.mouse.x >= wmap_info->interactive_rect.x + wmap_info->interactive_rect.width
                || msg->data.mouse.y >= wmap_info->interactive_rect.y + wmap_info->interactive_rect.height) {
                if (wmap_ui_state == WMAP_UI_STATE_NAVIGATING) {
                    wmap_ui_navigate(msg->data.mouse.x, msg->data.mouse.y);
                    return true;
                }
                return false;
            }

            if (wmap_info->screen_to_map_coordinates != NULL) {
                wmap_info->screen_to_map_coordinates(msg->data.mouse.x, msg->data.mouse.y, &wmap_ui_mouse_coordinates);
            }

            switch (wmap_ui_mode) {
            case WMAP_UI_MODE_WORLD:
                switch (wmap_ui_state) {
                case WMAP_UI_STATE_NORMAL: {
                    if (wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].waypoint_edit_enabled) {
                        if (wmap_ui_route_waypoint_hit_test(&wmap_ui_mouse_coordinates, &wmap_ui_selected_waypoint)) {
                            wmap_ui_selected_waypoint_value_hook_stub(wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].waypoints[wmap_ui_selected_waypoint].travel_value);
                            wmap_ui_state_set(WMAP_UI_STATE_WAYPOINT_DRAG);
                            wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].waypoint_edit_enabled = 0;
                            return true;
                        }

                        if (wmap_ui_destination_coords_check(&wmap_ui_mouse_coordinates) && wmap_ui_route_destination_add(&wmap_ui_mouse_coordinates)) {
                            wmap_ui_selected_waypoint = wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].length - 1;
                            wmap_ui_state_set(WMAP_UI_STATE_WAYPOINT_DRAG);
                            wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].waypoint_edit_enabled = 0;
                            return true;
                        }

                        return true;
                    }

                    return true;
                }
                case WMAP_UI_STATE_WAYPOINT_DRAG:
                    if (!wmap_ui_world_travel_available) {
                        return true;
                    }

                    wmap_ui_scroll_at_edges(msg->data.mouse.x, msg->data.mouse.y);

                    if (!wmap_ui_destination_coords_check(&wmap_ui_mouse_coordinates)) {
                        return true;
                    }

                    wmap_ui_waypoint_drag_hook_stub(wmap_ui_selected_waypoint, &wmap_ui_mouse_coordinates);

                    return true;
                case WMAP_UI_STATE_EDITING: {
                    int id;
                    if (find_note_by_coords(&wmap_ui_mouse_coordinates, &id)) {
                        wmap_ui_note_edit_by_id(id);
                    } else {
                        wmap_ui_new_note_edit_begin(&wmap_ui_mouse_coordinates);
                    }
                    return true;
                }
                case WMAP_UI_STATE_NAVIGATING:
                    wmap_ui_navigate(msg->data.mouse.x, msg->data.mouse.y);
                    return true;
                default:
                    return true;
                }
            case WMAP_UI_MODE_TOWN:
                switch (wmap_ui_state) {
                case WMAP_UI_STATE_NORMAL:
                    if (wmap_ui_routes[WMAP_ROUTE_TYPE_TOWN].waypoint_edit_enabled) {
                        if (wmap_ui_route_waypoint_hit_test(&wmap_ui_mouse_coordinates, &wmap_ui_selected_waypoint)) {
                            wmap_ui_selected_waypoint_value_hook_stub(wmap_ui_routes[WMAP_ROUTE_TYPE_TOWN].waypoints[wmap_ui_selected_waypoint].travel_value);
                            wmap_ui_routes[WMAP_ROUTE_TYPE_TOWN].waypoint_edit_enabled = 0;
                            return true;
                        }

                        if (wmap_ui_destination_coords_check(&wmap_ui_mouse_coordinates) && wmap_ui_route_destination_add(&wmap_ui_mouse_coordinates)) {
                            wmap_ui_selected_waypoint = wmap_ui_routes[WMAP_ROUTE_TYPE_TOWN].length - 1;
                            wmap_ui_routes[WMAP_ROUTE_TYPE_TOWN].waypoint_edit_enabled = 0;
                            return true;
                        }
                    }
                    return true;
                case WMAP_UI_STATE_NAVIGATING:
                    wmap_ui_navigate(msg->data.mouse.x, msg->data.mouse.y);
                    return true;
                default:
                    return true;
                }
            default:
                return true;
            }
        case TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP:
            if (wmap_ui_pan_active) {
                return true;
            }
            switch (wmap_ui_mode) {
            case WMAP_UI_MODE_WORLD:
                wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].waypoint_edit_enabled = wmap_ui_world_travel_available != 0;
                if (wmap_ui_state == WMAP_UI_STATE_WAYPOINT_DRAG || wmap_ui_state == WMAP_UI_STATE_NAVIGATING) {
                    wmap_ui_state_set(WMAP_UI_STATE_NORMAL);
                }

                // CE: Move selection of the Teleport spell destination out of
                // the "mouse down" event handler. This prevents delivering a
                // subsequent "mouse up" event to the destination map (which
                // may have unexpected consequences like starting a dialog).
                if (wmap_ui_selecting) {
                    wmap_info->screen_to_map_coordinates(msg->data.mouse.x, msg->data.mouse.y, &wmap_ui_mouse_coordinates);

                    int64_t loc;
                    wmap_ui_coords_to_location(&wmap_ui_mouse_coordinates, &loc);

                    int area = area_get_nearest_known_area(loc, player_get_local_pc_obj(), wmap_ui_known_area_search_radius);
                    if (area <= 0) {
                        return true;
                    }

                    loc = area_get_location(area);
                    if (loc == 0 || wmap_ui_spell == -1) {
                        return true;
                    }

                    int64_t pc_obj = player_get_local_pc_obj();
                    if (antiteleport_check(pc_obj, loc)) {
                        if (player_is_local_pc_obj(wmap_ui_obj)) {
                            magictech_casting_cost_consume_callback(wmap_ui_obj, wmap_ui_spell);
                        }

                        if (area_get_last_known_area(pc_obj) == area) {
                            area_reset_last_known_area(pc_obj);
                        }

                        wmap_ui_teleport(loc);
                        wmap_ui_close();
                    } else {
                        mes_file_entry.num = 640; // "You are unable to teleport to this location. Something is blocking you."
                        mes_get_msg(wmap_ui_worldmap_mes_file, &mes_file_entry);
                        ui_display_warning(mes_file_entry.str);
                    }

                    return true;
                }

                break;
            case WMAP_UI_MODE_CONTINENT:
                if (wmap_ui_townmap == TOWNMAP_NONE
                    && msg->data.mouse.x >= wmap_info->interactive_rect.x
                    && msg->data.mouse.y >= wmap_info->interactive_rect.y
                    && msg->data.mouse.x < wmap_info->interactive_rect.x + wmap_info->interactive_rect.width
                    && msg->data.mouse.y < wmap_info->interactive_rect.y + wmap_info->interactive_rect.height
                    && wmap_ui_state == WMAP_UI_STATE_NORMAL) {
                    wmap_ui_overview_to_map_coords(msg->data.mouse.x, msg->data.mouse.y, &wmap_ui_mouse_coordinates);
                    wmap_ui_mode_set(WMAP_UI_MODE_WORLD);
                    wmap_ui_scroll_origin_set(&wmap_ui_mouse_coordinates, 0);
                    wmap_ui_current_rect_refresh();
                }
                break;
            case WMAP_UI_MODE_TOWN:
                wmap_ui_routes[WMAP_ROUTE_TYPE_TOWN].waypoint_edit_enabled = 1;
                if (wmap_ui_state == WMAP_UI_STATE_WAYPOINT_DRAG || wmap_ui_state == WMAP_UI_STATE_NAVIGATING) {
                    wmap_ui_state_set(WMAP_UI_STATE_NORMAL);
                }
                break;
            case WMAP_UI_MODE_COUNT:
                // Should be unreachable.
                break;
            }

            if (intgame_panel_close_lens_check_pt_unscale(msg->data.mouse.x, msg->data.mouse.y)) {
                wmap_ui_close();
                return true;
            }

            // Click outside the world map window and outside both HUD strips
            // dismisses the overlay (hi-res convenience).
            if (wmap_ui_window != TIG_WINDOW_HANDLE_INVALID) {
                TigWindowData menu_wd;
                int screen_x = msg->data.mouse.x + (hrp_iso_window_width_get() - 800) / 2;
                int screen_y = msg->data.mouse.y + (hrp_iso_window_height_get() - 600) / 2;
                if (tig_window_data(wmap_ui_window, &menu_wd) == TIG_OK
                    && intgame_panel_should_dismiss_overlay_click(screen_x, screen_y, &menu_wd.rect)) {
                    wmap_ui_close();
                    return true;
                }
            }

            return false;
        case TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP: {
            wmap_ui_pan_active = false;
            if (wmap_ui_pan_moved) {
                // Releasing a pan must not delete a route waypoint or cancel editing.
                wmap_ui_pan_moved = false;
                return true;
            }

            if (wmap_ui_state == WMAP_UI_STATE_EDITING) {
                if (wmap_ui_mode == WMAP_UI_MODE_WORLD) {
                    wmap_ui_state_set(WMAP_UI_STATE_NORMAL);
                    return true;
                }
                return false;
            }

            WmapRouteType route_type = wmap_ui_mode == WMAP_UI_MODE_TOWN
                ? WMAP_ROUTE_TYPE_TOWN
                : WMAP_ROUTE_TYPE_WORLD;
            if (wmap_ui_selected_waypoint == -1) {
                wmap_ui_selected_waypoint = wmap_ui_routes[route_type].length - 1;
            }

            if (wmap_ui_selected_waypoint >= 0) {
                wmap_ui_route_waypoint_remove(wmap_ui_selected_waypoint);
                if (wmap_ui_selected_waypoint >= wmap_ui_routes[route_type].length) {
                    wmap_ui_selected_waypoint = wmap_ui_routes[route_type].length - 1;
                }
            }

            if (wmap_ui_state == WMAP_UI_STATE_MOVING) {
                wmap_ui_state_set(WMAP_UI_STATE_NORMAL);
            }

            return true;
        }
        case TIG_MESSAGE_MOUSE_MOVE:
            if (wmap_ui_pan_active) {
                int dx = wmap_ui_pan_x - msg->data.mouse.x;
                int dy = wmap_ui_pan_y - msg->data.mouse.y;

                // Keep an ordinary right click usable despite small mouse jitter.
                if (!wmap_ui_pan_moved && dx > -3 && dx < 3 && dy > -3 && dy < 3) {
                    return true;
                }

                wmap_ui_pan_moved = true;
                wmap_ui_pan_x = msg->data.mouse.x;
                wmap_ui_pan_y = msg->data.mouse.y;
                wmap_ui_scroll_by(dx, dy);
                return true;
            }
            if (msg->data.mouse.x >= wmap_info->interactive_rect.x
                && msg->data.mouse.y >= wmap_info->interactive_rect.y
                && msg->data.mouse.x < wmap_info->interactive_rect.x + wmap_info->interactive_rect.width
                && msg->data.mouse.y < wmap_info->interactive_rect.y + wmap_info->interactive_rect.height) {
                if (wmap_ui_mode == WMAP_UI_MODE_WORLD || wmap_ui_mode == WMAP_UI_MODE_CONTINENT) {
                    wmap_info->screen_to_map_coordinates(msg->data.mouse.x, msg->data.mouse.y, &wmap_ui_mouse_coordinates);
                }

                if (wmap_ui_mode != WMAP_UI_MODE_TOWN) {
                    wmap_ui_draw_coords(&wmap_ui_mouse_coordinates);
                }

                inside = true;
            }

            switch (wmap_ui_mode) {
            case WMAP_UI_MODE_WORLD:
                switch (wmap_ui_state) {
                case WMAP_UI_STATE_NORMAL:
                    if (inside) {
                        wmap_info->screen_to_map_coordinates(msg->data.mouse.x, msg->data.mouse.y, &wmap_ui_mouse_coordinates);

                        int id;
                        if (find_note_by_coords(&wmap_ui_mouse_coordinates, &id)) {
                            if (wmap_info->hovered_note_id != id) {
                                WmapNote* note = wmap_ui_note_find_by_id(id);
                                wmap_info->hovered_note_id = id;
                                intgame_message_window_display_str(-1, (note->flags & 0x2) != 0
                                    && note->id > 0 && note->id < area_count() ? area_get_description(note->id) : note->str);
                            }
                            return true;
                        }
                    }

                    if (wmap_info->hovered_note_id != -1) {
                        wmap_info->hovered_note_id = -1;
                        intgame_message_window_clear();
                    }

                    return true;
                case WMAP_UI_STATE_WAYPOINT_DRAG:
                    if (wmap_info) {
                        wmap_ui_scroll_at_edges(msg->data.mouse.x, msg->data.mouse.y);
                        if (wmap_ui_destination_coords_check(&wmap_ui_mouse_coordinates)) {
                            wmap_ui_waypoint_drag_hook_stub(wmap_ui_selected_waypoint, &wmap_ui_mouse_coordinates);
                        }
                    }

                    return true;
                default:
                    return true;
                }
            case WMAP_UI_MODE_CONTINENT: {
                if (inside) {
                    wmap_info->screen_to_map_coordinates(msg->data.mouse.x, msg->data.mouse.y, &wmap_ui_mouse_coordinates);

                    int id;
                    if (find_note_by_coords(&wmap_ui_mouse_coordinates, &id)) {
                        if (wmap_info->hovered_note_id != id) {
                            WmapNote* note = wmap_ui_note_find_by_id(id);
                            wmap_info->hovered_note_id = id;
                            intgame_message_window_display_str(-1, (note->flags & 0x2) != 0
                                && note->id > 0 && note->id < area_count() ? area_get_description(note->id) : note->str);
                        }
                    } else {
                        if (wmap_info->hovered_note_id != -1) {
                            wmap_info->hovered_note_id = -1;
                            intgame_message_window_clear();
                        }
                    }
                }

                return true;
            }
            case WMAP_UI_MODE_TOWN:
                switch (wmap_ui_state) {
                case WMAP_UI_STATE_NORMAL:
                    if (inside) {
                        wmap_info->screen_to_map_coordinates(msg->data.mouse.x, msg->data.mouse.y, &wmap_ui_mouse_coordinates);

                        int id;
                        if (find_note_by_coords(&wmap_ui_mouse_coordinates, &id)) {
                            if (wmap_info->hovered_note_id != id) {
                                WmapNote* note = wmap_ui_note_find_by_id(id);
                                wmap_info->hovered_note_id = id;
                                intgame_message_window_display_str(-1, note->str);
                            }
                        }
                    } else {
                        if (wmap_info->hovered_note_id != -1) {
                            wmap_info->hovered_note_id = -1;
                            intgame_message_window_clear();
                        }
                    }

                    return true;
                case WMAP_UI_STATE_WAYPOINT_DRAG:
                    if (inside) {
                        wmap_ui_scroll_at_edges(msg->data.mouse.x, msg->data.mouse.y);
                        if (!wmap_ui_destination_coords_check(&wmap_ui_mouse_coordinates)) {
                            return true;
                        }
                    }

                    return true;
                default:
                    return true;
                }
            default:
                return true;
            }
        case TIG_MESSAGE_MOUSE_WHEEL:
            wmap_ui_scroll_from_mouse_wheel(msg->data.mouse.dx, msg->data.mouse.dy);
            break;
        default:
            break;
        }
    }

    switch (msg->type) {
    case TIG_MESSAGE_BUTTON:
        switch (msg->data.button.state) {
        case TIG_BUTTON_STATE_PRESSED:
            if (msg->data.button.button_handle == wmap_ui_navigate_button_info.button_handle) {
                if (wmap_ui_state != WMAP_UI_STATE_MOVING && wmap_ui_state_set(WMAP_UI_STATE_NAVIGATING)) {
                    wmap_ui_navigate(msg->data.button.x, msg->data.button.y);
                }
                return true;
            }
            return false;
        case TIG_BUTTON_STATE_RELEASED:
            if (msg->data.button.button_handle == wmap_ui_zoom_button_info[0].button_handle) {
                if (wmap_ui_townmap == TOWNMAP_NONE) {
                    wmap_ui_mode_set(WMAP_UI_MODE_WORLD);
                    return true;
                }

                if (!wmap_ui_selecting) {
                    wmap_ui_mode_set(WMAP_UI_MODE_TOWN);
                    return true;
                }

                return true;
            }

            if (msg->data.button.button_handle == wmap_ui_zoom_button_info[1].button_handle) {
                wmap_ui_mode_set(WMAP_UI_MODE_CONTINENT);
                return true;
            }

            if (msg->data.button.button_handle == wmap_ui_navigate_button_info.button_handle) {
                if (wmap_info->navigatable && !wmap_ui_navigating) {
                    wmap_ui_scroll_origin_set(&(wmap_info->player_coordinates), true);
                } else {
                    wmap_ui_navigating = false;
                }
                return true;
            }

            if (msg->data.button.button_handle == wmap_ui_travel_button_info.button_handle) {
                if (wmap_ui_state == WMAP_UI_STATE_EDITING) {
                    wmap_ui_state_set(WMAP_UI_STATE_NORMAL);
                }

                if (wmap_ui_state == WMAP_UI_STATE_NORMAL) {
                    if (wmap_ui_world_travel_available) {
                        WmapRouteType route_type = wmap_ui_mode == WMAP_UI_MODE_TOWN
                            ? WMAP_ROUTE_TYPE_TOWN
                            : WMAP_ROUTE_TYPE_WORLD;
                        if (wmap_ui_routes[route_type].length > 0) {
                            wmap_ui_selected_waypoint = -1;

                            if (wmap_ui_state_set(WMAP_UI_STATE_MOVING)) {
                                TimeEvent timeevent;
                                DateTime datetime;

                                timeevent.type = TIMEEVENT_TYPE_WORLDMAP;
                                datetime_init(&datetime, 50);
                                timeevent_add_delay(&timeevent, &datetime);

                                return true;
                            }
                        }
                    } else {
                        if (wmap_ui_mode == WMAP_UI_MODE_TOWN && wmap_ui_routes[WMAP_ROUTE_TYPE_TOWN].length > 0) {
                            int64_t points[MOVEMENT_ROUTE_CAPACITY];
                            int count = wmap_ui_routes[WMAP_ROUTE_TYPE_TOWN].length;
                            for (int idx = 0; idx < count; idx++) {
                                points[idx] = wmap_ui_routes[WMAP_ROUTE_TYPE_TOWN].waypoints[idx].loc;
                            }
                            if (anim_goal_move_route(player_get_local_pc_obj(), points, count)) {
                                wmap_ui_close();
                            }
                        }
                    }
                    return true;
                } else if (wmap_ui_state == WMAP_UI_STATE_MOVING) {
                    wmap_ui_state_set(WMAP_UI_STATE_NORMAL);
                }

                return true;
            }

            return false;
        case TIG_BUTTON_STATE_MOUSE_INSIDE:
            mes_file_entry.num = -1;

            if (msg->data.button.button_handle == wmap_ui_zoom_button_info[0].button_handle) {
                mes_file_entry.num = 501; // "Map View"
            } else if (msg->data.button.button_handle == wmap_ui_zoom_button_info[1].button_handle) {
                mes_file_entry.num = 502; // "Continent View"
            } else if (msg->data.button.button_handle == wmap_ui_navigate_button_info.button_handle) {
                mes_file_entry.num = 510; // "Scroll Map"
            } else if (msg->data.button.button_handle == wmap_ui_travel_button_info.button_handle) {
                mes_file_entry.num = 512; // "Toggle Walking Waypoint Path"
            } else {
                return false;
            }

            if (mes_search(wmap_ui_worldmap_mes_file, &mes_file_entry)) {
                mes_get_msg(wmap_ui_worldmap_mes_file, &mes_file_entry);
                snprintf(str, sizeof(str), "%s\n%s", wmap_ui_action, mes_file_entry.str);
                ui_message.type = UI_MSG_TYPE_FEEDBACK;
                ui_message.str = str;
                intgame_message_window_display_msg(&ui_message);
            } else {
                tig_debug_printf("WmapUI: ERROR: Hover Text for button is Unreachable!\n");
            }
            return true;
        case TIG_BUTTON_STATE_MOUSE_OUTSIDE:
            mes_file_entry.num = -1;

            if (msg->data.button.button_handle == wmap_ui_zoom_button_info[0].button_handle) {
                mes_file_entry.num = 501;
                intgame_message_window_clear();
                return true;
            }

            if (msg->data.button.button_handle == wmap_ui_zoom_button_info[1].button_handle) {
                mes_file_entry.num = 502;
                intgame_message_window_clear();
                return true;
            }

            if (msg->data.button.button_handle == wmap_ui_navigate_button_info.button_handle) {
                mes_file_entry.num = 510;
                intgame_message_window_clear();
                return true;
            }

            if (msg->data.button.button_handle == wmap_ui_travel_button_info.button_handle) {
                mes_file_entry.num = 512;
                intgame_message_window_clear();
                return true;
            }

            return false;
        default:
            return false;
        }
    case TIG_MESSAGE_TEXT_INPUT:
        return textedit_ui_process_message(msg);
    case TIG_MESSAGE_KEYBOARD:
        if (wmap_ui_textedit_focused) {
            return textedit_ui_process_message(msg);
        }

        if (input_binding_matches_keyboard(INPUT_ACTION_CENTER_CAMERA, msg)) {
            if (wmap_ui_state != WMAP_UI_STATE_MOVING) {
                gsound_play_sfx(0, 1);
                wmap_ui_scroll_origin_set(&(wmap_info->player_coordinates), true);
            }
            return true;
        }

        if (input_binding_matches_keyboard(INPUT_ACTION_SCROLL_LEFT, msg)) {
            if (wmap_ui_state != WMAP_UI_STATE_MOVING) {
                gsound_play_sfx(0, 1);
                wmap_ui_scroll_with_kb(6);
            }
            return true;
        }

        if (input_binding_matches_keyboard(INPUT_ACTION_SCROLL_UP, msg)) {
            if (wmap_ui_state != WMAP_UI_STATE_MOVING) {
                gsound_play_sfx(0, 1);
                wmap_ui_scroll_with_kb(0);
            }
            return true;
        }

        if (input_binding_matches_keyboard(INPUT_ACTION_SCROLL_RIGHT, msg)) {
            if (wmap_ui_state != WMAP_UI_STATE_MOVING) {
                gsound_play_sfx(0, 1);
                wmap_ui_scroll_with_kb(2);
            }
            return true;
        }

        if (input_binding_matches_keyboard(INPUT_ACTION_SCROLL_DOWN, msg)) {
            if (wmap_ui_state != WMAP_UI_STATE_MOVING) {
                gsound_play_sfx(0, 1);
                wmap_ui_scroll_with_kb(4);
            }
            return true;
        }

        switch (msg->data.keyboard.scancode) {
        case SDL_SCANCODE_Z:
            if (msg->data.keyboard.pressed) {
                return false;
            }

            if (wmap_ui_state == WMAP_UI_STATE_MOVING) {
                wmap_ui_mode_set(WMAP_UI_MODE_TOWN);
                return true;
            }

            gsound_play_sfx(0, 1);

            if (wmap_ui_mode != 1) {
                wmap_ui_mode_set(WMAP_UI_MODE_CONTINENT);
                return true;
            }

            if (wmap_ui_townmap == TOWNMAP_NONE) {
                wmap_ui_mode_set(WMAP_UI_MODE_WORLD);
                return true;
            }

            return true;
        case SDL_SCANCODE_DELETE:
            if (msg->data.keyboard.pressed) {
                return false;
            }

            // TODO: Incomplete.
            return true;
        case SDL_SCANCODE_D:
            if (!msg->data.keyboard.pressed
                && gamelib_cheat_level_get() >= 3
                && wmap_ui_mode == WMAP_UI_MODE_WORLD) {
                gsound_play_sfx(0, 1);

                for (int area = area_count() - 1; area > 0; area--) {
                    area_set_known(player_get_local_pc_obj(), area);
                    wmap_ui_area_note_add(area);
                }

                wmap_info->refresh();
            }
            return true;
        default:
            return false;
        }
    default:
        return false;
    }
}

// 0x5627B0
bool wmap_ui_destination_coords_check(WmapCoords* coords)
{
    int64_t loc;

    if (wmap_ui_mode == WMAP_UI_MODE_TOWN) {
        return 1;
    }

    wmap_ui_coords_to_location(coords, &loc);
    return wmap_ui_destination_location_check_stub(loc);
}

// 0x5627F0
bool wmap_ui_destination_location_check_stub(int64_t loc)
{
    (void)loc;

    return true;
}

// 0x562800
void wmap_ui_area_note_add(int id)
{
    WmapNote note;
    const char* name;

    note.id = id;
    note.flags = 0x2;
    note.location = 0;
    note.type = WMAP_NOTE_TYPE_LOC;

    name = area_get_name(id);
    if (name != NULL) {
        strncpy(note.str, name, sizeof(note.str));
        wmap_ui_location_to_coords(area_get_location(id), &(note.coords));
        wmap_ui_note_insert_or_update(&note);
    }
}

// 0x562880
void wmap_ui_draw_coords(WmapCoords* coords)
{
    int x;
    int y;
    int index;
    int64_t limit_x;
    int64_t limit_y;
    int64_t coordinate;
    MesFileEntry direction_label;
    char str[128];
    TigRect src_rect;
    TigRect dst_rect;

    x = wmap_ui_offset_x + coords->x;
    y = wmap_ui_offset_y + coords->y;

    for (index = 0; index < 2; index++) {
        if (tig_video_buffer_fill(wmap_ui_coordinate_text_buffer, &wmap_ui_coordinate_text_rect, wmap_ui_color_key) == TIG_OK) {
            tig_window_fill(wmap_ui_window,
                &wmap_ui_lat_long_frame[index],
                tig_color_make(0, 0, 0));

            if (is_default_wmap()) {
                limit_x = 2000;
            } else {
                location_limits_get(&limit_x, &limit_y);
                limit_x /= 64;
            }

            if (index == 0) {
                coordinate = limit_x - x;
                direction_label.num = coordinate < 0 ? 913 : 912;

            } else {
                coordinate = y;
                direction_label.num = coordinate < 0 ? 910 : 911;

            }
            direction_label.str = (char*)mes_fallback_get("worldmap", direction_label.num);
            mes_search(wmap_ui_worldmap_mes_file, &direction_label);
            snprintf(str, sizeof(str), "%lld %s", (long long)(coordinate < 0 ? -coordinate : coordinate), direction_label.str);

            tig_font_push(wmap_note_type_info[WMAP_NOTE_TYPE_NOTE].font);
            if (tig_font_write(wmap_ui_coordinate_text_buffer, str, &wmap_ui_coordinate_text_rect, &dst_rect) == TIG_OK) {
                dst_rect.x = wmap_ui_lat_long_frame[index].x;
                dst_rect.y = wmap_ui_lat_long_frame[index].y;

                src_rect.x = 0;
                src_rect.y = 0;
                src_rect.width = dst_rect.width;
                src_rect.height = dst_rect.height;

                tig_window_copy_from_vbuffer(wmap_ui_window,
                    &dst_rect,
                    wmap_ui_coordinate_text_buffer,
                    &src_rect);
            }
            tig_font_pop();
        }
    }
}

// 0x562A20
void wmap_ui_navigate(int x, int y)
{
    if (!wmap_ui_mode_info[wmap_ui_mode].navigatable) {
        return;
    }

    if (x >= wmap_ui_navigation_dead_zone.x
        && y >= wmap_ui_navigation_dead_zone.y
        && x < wmap_ui_navigation_dead_zone.x + wmap_ui_navigation_dead_zone.width
        && y < wmap_ui_navigation_dead_zone.y + wmap_ui_navigation_dead_zone.height) {
        return;
    }

    wmap_ui_navigating = true;

    if (y < wmap_ui_navigation_dead_zone.y) {
        wmap_ui_scroll_internal(0, 8);
    } else if (y > wmap_ui_navigation_dead_zone.y + wmap_ui_navigation_dead_zone.height) {
        wmap_ui_scroll_internal(4, 8);
    }

    if (x < wmap_ui_navigation_dead_zone.x) {
        wmap_ui_scroll_internal(6, 8);
    } else if (x > wmap_ui_navigation_dead_zone.x + wmap_ui_navigation_dead_zone.width) {
        wmap_ui_scroll_internal(2, 8);
    }
}

// 0x562AF0
void wmap_ui_scroll_at_edges(int x, int y)
{
    if (y < wmap_ui_mode_info[wmap_ui_mode].interactive_rect.y + 23) {
        wmap_ui_scroll_internal(0, 10);
    } else if (y > wmap_ui_mode_info[wmap_ui_mode].interactive_rect.y + wmap_ui_mode_info[wmap_ui_mode].interactive_rect.height - 23) {
        wmap_ui_scroll_internal(4, 10);
    }

    if (x < wmap_ui_mode_info[wmap_ui_mode].interactive_rect.x + 23) {
        wmap_ui_scroll_internal(6, 10);
    } else if (x > wmap_ui_mode_info[wmap_ui_mode].interactive_rect.x + wmap_ui_mode_info[wmap_ui_mode].interactive_rect.width - 23) {
        wmap_ui_scroll_internal(2, 10);
    }
}

// 0x562B70
void wmap_ui_mode_set(WmapUiMode mode)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    int compass_mode;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;
    TigArtFrameData art_frame_data;
    TigArtBlitInfo art_blit_info;
    TigRect src_rect;
    TigRect dst_rect;

    wmap_ui_pan_active = false;
    wmap_ui_pan_moved = false;
    wmap_ui_state_set(WMAP_UI_STATE_NORMAL);

    if (wmap_ui_mode == mode) {
        return;
    }

    switch (mode) {
    case WMAP_UI_MODE_CONTINENT:
        if (wmap_ui_mode_info[WMAP_UI_MODE_WORLD].str[0] == '\0') {
            mes_file_entry.num = 605;
            mes_get_msg(wmap_ui_worldmap_mes_file, &mes_file_entry);

            ui_message.type = UI_MSG_TYPE_FEEDBACK;
            ui_message.str = mes_file_entry.str;
            intgame_message_window_display_msg(&ui_message);
            return;
        }
        break;
    case WMAP_UI_MODE_TOWN:
        if (wmap_ui_townmap == TOWNMAP_NONE) {
            return;
        }
        break;
    default:
        break;
    }

    wmap_marker_close();
    wmap_ui_mode = mode;

    switch (mode) {
    case WMAP_UI_MODE_CONTINENT:
        if (wmap_ui_travel_button_info.button_handle != TIG_BUTTON_HANDLE_INVALID) {
            tig_button_hide(wmap_ui_travel_button_info.button_handle);
        }
        break;
    case WMAP_UI_MODE_TOWN:
        if (wmap_ui_travel_button_info.button_handle != TIG_BUTTON_HANDLE_INVALID) {
            tig_button_show(wmap_ui_travel_button_info.button_handle);
        }

        if (!wmap_load_townmap_info()) {
            wmap_ui_mode_set(WMAP_UI_MODE_WORLD);
            return;
        }
        break;
    default:
        if (wmap_ui_world_travel_available) {
            if (wmap_ui_travel_button_info.button_handle != TIG_BUTTON_HANDLE_INVALID) {
                tig_button_show(wmap_ui_travel_button_info.button_handle);
            }
        } else {
            if (wmap_ui_travel_button_info.button_handle != TIG_BUTTON_HANDLE_INVALID) {
                tig_button_hide(wmap_ui_travel_button_info.button_handle);
            }
        }
        break;
    }

    if (wmap_ui_window != TIG_WINDOW_HANDLE_INVALID) {
        compass_mode = mode;
        if (!wmap_ui_world_travel_available) {
            compass_mode = 2;
        }

        if (wmap_ui_mode_info[compass_mode].compass_art_number != -1) {
            tig_art_interface_id_create(wmap_ui_mode_info[compass_mode].compass_art_number, 0, 0, 0, &art_id);

            if (tig_art_anim_data(art_id, &art_anim_data) == TIG_OK
                && tig_art_frame_data(art_id, &art_frame_data) == TIG_OK) {
                src_rect.x = 0;
                src_rect.y = 0;
                src_rect.width = art_frame_data.width;
                src_rect.height = art_frame_data.height;

                dst_rect.x = wmap_ui_compass_rect.x;
                dst_rect.y = wmap_ui_compass_rect.y;
                dst_rect.width = art_frame_data.width;
                dst_rect.height = art_frame_data.height;

                art_blit_info.flags = 0;
                art_blit_info.art_id = art_id;
                art_blit_info.src_rect = &src_rect;
                art_blit_info.dst_rect = &dst_rect;
                tig_window_blit_art(wmap_ui_window, &art_blit_info);
            }
        }

        wmap_ui_mode_info[wmap_ui_mode].refresh();
        wmap_ui_draw_coords(&wmap_ui_mouse_coordinates);
        wmap_ui_draw_travel_time();
    }
}

// 0x562D80
bool wmap_load_townmap_info(void)
{
    WmapInfo* wmap_info;
    int x;
    int y;
    int index;

    wmap_info = &(wmap_ui_mode_info[wmap_ui_mode]);
    wmap_ui_routes[WMAP_ROUTE_TYPE_TOWN].length = 0;

    if (!townmap_info(wmap_ui_townmap, &wmap_ui_tmi)) {
        wmap_ui_townmap = TOWNMAP_NONE;
        return false;
    }

    townmap_loc_to_coords(&wmap_ui_tmi,
        obj_field_int64_get(player_get_local_pc_obj(), OBJ_F_LOCATION),
        &x,
        &y);

    wmap_info->player_coordinates.x = x;
    wmap_info->player_coordinates.y = y;
    wmap_info->scroll_x = 0;
    wmap_info->scroll_y = 0;

    strcpy(wmap_info->tile_directory, townmap_name(wmap_ui_townmap));
    wmap_info->num_hor_tiles = wmap_ui_tmi.num_hor_tiles;
    wmap_info->num_vert_tiles = wmap_ui_tmi.num_vert_tiles;
    wmap_info->num_tiles = wmap_ui_tmi.num_hor_tiles * wmap_ui_tmi.num_vert_tiles;
    wmap_info->tiles = (WmapTile*)CALLOC(sizeof(*wmap_info->tiles), wmap_info->num_tiles);

    if (!wmap_ui_mode_tile_art_lock(2, 0)) {
        tig_debug_printf("wmap_load_townmap_info: ERROR: wmap_ui_mode_tile_art_lock failed!\n");
        exit(EXIT_FAILURE);
    }

    wmap_info->tile_width = wmap_info->tiles[0].rect.width;
    wmap_info->tile_height = wmap_info->tiles[0].rect.height;
    wmap_info->map_width = wmap_info->tile_width * wmap_info->num_hor_tiles;
    wmap_info->map_height = wmap_info->tile_height * wmap_info->num_vert_tiles;

    wmap_ui_mode_tile_art_unlock(2, 0);
    wmap_ui_mode_tile_art_unload(2, 0);

    for (index = 0; index < wmap_info->num_tiles; index++) {
        wmap_ui_tile_render_state_init(&(wmap_info->tiles[index]));
        wmap_info->tiles[index].rect.width = wmap_info->tile_width;
        wmap_info->tiles[index].rect.height = wmap_info->tile_height;
    }

    wmap_info->center_x_max = wmap_info->num_hor_tiles * wmap_info->tile_width - wmap_info->viewport_half_width;
    wmap_info->scroll_x_max = wmap_info->num_hor_tiles * wmap_info->tile_width - wmap_info->rect.width;
    wmap_info->center_y_max = wmap_info->num_vert_tiles * wmap_info->tile_height - wmap_info->viewport_half_height;
    wmap_info->scroll_y_max = wmap_info->num_vert_tiles * wmap_info->tile_height - wmap_info->rect.height;

    wmap_ui_scroll_origin_set(&(wmap_info->player_coordinates), false);

    return true;
}

// 0x562F90
void wmap_ui_tile_render_state_init(WmapTile* tile)
{
    tile->flags = 0;
    tile->reserved_data = NULL;
    tile->reserved_data_size = 0;
}

// 0x562FA0
bool wmap_ui_tile_art_lock(int tile)
{
    return wmap_ui_mode_tile_art_lock(wmap_ui_mode, tile);
}

// 0x562FC0
void wmap_ui_tile_art_unlock(int tile)
{
    wmap_ui_mode_tile_art_unlock(wmap_ui_mode, tile);
}

// 0x562FE0
void wmap_ui_tile_art_unload(int tile)
{
    wmap_ui_mode_tile_art_unload(wmap_ui_mode, tile);
}

// 0x563000
bool wmap_ui_mode_tile_art_lock(WmapUiMode mode, int tile)
{
    char path[TIG_MAX_PATH];

    if ((wmap_ui_mode_info[mode].tiles[tile].flags & 0x1) == 0) {
        if (mode == 2) {
            snprintf(path, sizeof(path),
                "townmap\\%s\\%s%06d.bmp",
                wmap_ui_mode_info[mode].tile_directory,
                wmap_ui_mode_info[mode].tile_directory,
                tile);
            if (!tig_file_exists(path, NULL)) {
                snprintf(path, sizeof(path),
                    "townmap\\%s\\%s%06d.bmp",
                    wmap_ui_mode_info[mode].tile_directory,
                    wmap_ui_mode_info[mode].tile_directory,
                    0);
            }
        } else {
            snprintf(path, sizeof(path),
                "%s%03d",
                wmap_ui_mode_info[mode].tile_directory,
                tile + 1);
        }

        if (wmap_ui_tile_art_load(path, &(wmap_ui_mode_info[mode].tiles[tile].video_buffer), &(wmap_ui_mode_info[mode].tiles[tile].rect))) {
            wmap_ui_mode_info[mode].tiles[tile].flags |= 0x1;
            wmap_ui_mode_info[mode].num_loaded_tiles++;
        } else {
            tig_debug_printf("WMapUI: Blit: ERROR: Bmp Load Failed!\n");
        }
    }

    return true;
}

// 0x5630F0
bool wmap_ui_tile_art_load(const char* path, TigVideoBuffer** video_buffer_ptr, TigRect* rect)
{
    TigBmp bmp;
    TigVideoBufferData video_buffer_data;

    if (wmap_ui_mode != WMAP_UI_MODE_TOWN) {
        snprintf(bmp.name, sizeof(bmp.name), "WorldMap\\%s.bmp", path);
        path = bmp.name;
    }

    if (tig_video_buffer_load_from_bmp(path, video_buffer_ptr, 0x1) != TIG_OK) {
        tig_debug_printf("WMapUI: Pic [%s] FAILED to load!\n", path);
        return false;
    }

    if (tig_video_buffer_data(*video_buffer_ptr, &video_buffer_data) != TIG_OK) {
        return false;
    }

    rect->x = 0;
    rect->y = 0;
    rect->width = video_buffer_data.width;
    rect->height = video_buffer_data.height;

    return true;
}

// 0x563200
bool wmap_ui_mode_tile_art_unlock(WmapUiMode mode, int tile)
{
    (void)mode;
    (void)tile;

    return true;
}

// 0x563210
void wmap_ui_mode_tile_art_unload(WmapUiMode mode, int tile)
{
    if ((wmap_ui_mode_info[mode].tiles[tile].flags & 0x1) != 0) {
        if (tig_video_buffer_destroy(wmap_ui_mode_info[mode].tiles[tile].video_buffer) == TIG_OK) {
            wmap_ui_mode_info[mode].tiles[tile].flags &= ~0x1;
            wmap_ui_mode_info[mode].num_loaded_tiles--;
        } else {
            tig_debug_printf("WMapUI: Destroy: ERROR: Video Buffer Destroy FAILED!\n");
        }
    }
}

// 0x563270
void wmap_ui_current_rect_refresh(void)
{
    if (wmap_ui_mode_info[wmap_ui_mode].refresh_rect != NULL) {
        wmap_ui_mode_info[wmap_ui_mode].refresh_rect(&(wmap_ui_mode_info[wmap_ui_mode].rect));
    }
}

// 0x5632A0
void wmap_ui_scroll_rect_refresh(int direction, int reserved_horizontal, int reserved_vertical, int scroll_distance)
{
    (void)reserved_horizontal;
    (void)reserved_vertical;
    (void)scroll_distance;

    switch (direction) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        if (wmap_ui_mode_info[wmap_ui_mode].refresh_rect != NULL) {
            wmap_ui_mode_info[wmap_ui_mode].refresh_rect(&(wmap_ui_mode_info[wmap_ui_mode].rect));
        }
        break;
    default:
        tig_debug_printf("WMapUI: Error: Refresh Scroll Direction out of bounds!\n");
        break;
    }
}

// 0x563300
void wmap_ui_viewport_scroll(int direction, int reserved_horizontal, int reserved_vertical, int scroll_distance)
{
    WmapInfo* wmap_info;
    TigRect one;
    TigRect two;
    TigRect three;
    int dx;
    int dy;

    wmap_info = &(wmap_ui_mode_info[wmap_ui_mode]);
    one = wmap_info->rect;
    two = wmap_ui_nav_cvr_frame;
    three = one;

    switch (direction) {
    case 0:
        dx = 0;
        dy = scroll_distance;
        one.height = scroll_distance;
        three.width = 0;
        break;
    case 1:
        dx = -scroll_distance;
        dy = scroll_distance;
        one.height = scroll_distance;
        two.x = wmap_ui_nav_cvr_frame.x - scroll_distance;
        three.x += three.width - scroll_distance;
        three.y += dy;
        three.width = dy;
        three.height -= dy;
        break;
    case 2:
        dx = -scroll_distance;
        dy = 0;
        one.width = 0;
        two.x = wmap_ui_nav_cvr_frame.x - scroll_distance;
        two.width += scroll_distance;
        three.x += three.width - scroll_distance;
        three.width = scroll_distance;
        break;
    case 3:
        dx = -scroll_distance;
        dy = -scroll_distance;
        one.height = wmap_ui_nav_cvr_frame.height + scroll_distance;
        one.y = one.height + three.y - (scroll_distance + wmap_ui_nav_cvr_frame.height);
        two.width = 0;
        three.x += three.width - scroll_distance;
        three.width = scroll_distance;
        three.height -= scroll_distance;
        break;
    case 4:
        dx = 0;
        dy = -scroll_distance;
        one.height = wmap_ui_nav_cvr_frame.height + scroll_distance;
        one.y = one.height + three.y - (wmap_ui_nav_cvr_frame.height + scroll_distance);
        two.width = 0;
        three.width = 0;
        break;
    case 5:
        dx = scroll_distance;
        dy = -scroll_distance;
        one.y = one.height + three.y - (wmap_ui_nav_cvr_frame.height + scroll_distance);
        one.height = wmap_ui_nav_cvr_frame.height + scroll_distance;
        two.width = 0;
        three.x += three.width - scroll_distance;
        three.width = scroll_distance;
        three.height -= scroll_distance;
        break;
    case 6:
        dx = scroll_distance;
        dy = 0;
        one.width = 0;
        three.width = scroll_distance;
        two.width = scroll_distance + wmap_ui_nav_cvr_frame.width;
        break;
    case 7:
        dx = scroll_distance;
        dy = scroll_distance;
        one.height = scroll_distance;
        two.x = wmap_ui_nav_cvr_frame.x - scroll_distance;
        three.y = dy + three.y;
        three.width = dy;
        three.height -= dy;
        break;
    }

    tig_window_scroll_rect(wmap_ui_window, &(wmap_info->rect), dx, dy);

    if (wmap_info->refresh_rect != NULL) {
        if (one.width != 0) {
            wmap_info->refresh_rect(&one);
        }

        if (three.width != 0) {
            wmap_info->refresh_rect(&three);
        }

        if (two.width != 0) {
            wmap_info->refresh_rect(&two);
        }
    }
}

// 0x563590
void wmap_ui_scroll_origin_set(WmapCoords* coords, bool refresh)
{
    WmapInfo* wmap_info;
    int scroll_x;
    int scroll_y;

    wmap_info = &(wmap_ui_mode_info[wmap_ui_mode]);

    scroll_x = coords->x - wmap_info->viewport_half_width;
    if (scroll_x < 0) {
        scroll_x = 0;
    } else if (scroll_x > wmap_info->viewport_half_width + wmap_info->center_x_max) {
        scroll_x = wmap_info->center_x_max;
    }

    scroll_y = coords->y - wmap_info->viewport_half_height;
    if (scroll_y < 0) {
        scroll_y = 0;
    } else if (scroll_y > wmap_info->viewport_half_width + wmap_info->center_y_max) {
        scroll_y = wmap_info->center_y_max;
    }

    if (scroll_x != wmap_info->scroll_x
        || scroll_y != wmap_info->scroll_y) {
        wmap_info->scroll_x = scroll_x;
        wmap_info->scroll_y = scroll_y;

        if (refresh) {
            wmap_info->refresh();
        }
    }
}

// 0x563610
void wmap_ui_handle_scroll(void)
{
    // NOTE: Very odd initial values.
    int horizontal_direction = -69;
    int vertical_direction = -69;
    bool scroll_horizontally = false;

    if (tig_timer_between(wmap_ui_last_scroll_time, gamelib_ping_time) >= 10) {
        wmap_ui_last_scroll_time = gamelib_ping_time;

        if (wmap_ui_state != WMAP_UI_STATE_MOVING) {
            if (input_binding_is_pressed(INPUT_ACTION_SCROLL_LEFT)) {
                scroll_horizontally = true;
                horizontal_direction = 6;
            } else if (input_binding_is_pressed(INPUT_ACTION_SCROLL_RIGHT)) {
                scroll_horizontally = true;
                horizontal_direction = 2;
            }

            if (input_binding_is_pressed(INPUT_ACTION_SCROLL_UP)) {
                vertical_direction = 0;
            } else if (input_binding_is_pressed(INPUT_ACTION_SCROLL_DOWN)) {
                vertical_direction = 4;
            } else {
                if (!scroll_horizontally) {
                    // Neither left/right nor up/down keys are pressed, no need
                    // to continue with scrolling.
                    return;
                }
            }

            switch (horizontal_direction) {
            case 6:
                switch (vertical_direction) {
                case 0:
                    wmap_ui_scroll_with_kb(7);
                    break;
                case 4:
                    wmap_ui_scroll_with_kb(5);
                    break;
                default:
                    wmap_ui_scroll_with_kb(6);
                    break;
                }
                break;
            case 2:
                switch (vertical_direction) {
                case 0:
                    wmap_ui_scroll_with_kb(1);
                    break;
                case 4:
                    wmap_ui_scroll_with_kb(3);
                    break;
                default:
                    wmap_ui_scroll_with_kb(2);
                    break;
                }
                break;
            default:
                switch (vertical_direction) {
                case 0:
                    wmap_ui_scroll_with_kb(0);
                    break;
                case 4:
                    wmap_ui_scroll_with_kb(4);
                    break;
                }
                break;
            }
        }
    }
}

// 0x563750
void wmap_ui_scroll_with_kb(int direction)
{
    wmap_ui_scroll_internal(direction, 4);
}

// 0x563770
void wmap_ui_scroll(int direction)
{
    if (wmap_ui_state != WMAP_UI_STATE_MOVING) {
        wmap_ui_scroll_internal(direction, 8);
    }
}

// 0x563790
void wmap_ui_scroll_internal(int direction, int scale)
{
    int sx;
    int sy;
    int dx;
    int dy;

    sx = scale * wmap_ui_scroll_speed_x;
    sy = scale * wmap_ui_scroll_speed_y;

    dx = 0;
    dy = 0;

    switch (direction) {
    case 0:
        dy = -sy;
        break;
    case 1:
        dx = sx;
        dy = -sy;
        break;
    case 2:
        dx = sx;
        break;
    case 3:
        dx = sx;
        dy = sy;
        break;
    case 4:
        dy = sy;
        break;
    case 5:
        dx = -sx;
        dy = sy;
        break;
    case 6:
        dx = -sx;
        break;
    case 7:
        dx = -sx;
        dy = -sy;
        break;
    }

    wmap_ui_scroll_by(dx, dy);
}

// 0x563AC0
void wmap_ui_mouse_to_map_coords(int x, int y, WmapCoords* coords)
{
    coords->x = x + wmap_ui_mode_info[wmap_ui_mode].scroll_x - wmap_ui_mode_info[wmap_ui_mode].rect.x;
    coords->y = y + wmap_ui_mode_info[wmap_ui_mode].scroll_y - wmap_ui_mode_info[wmap_ui_mode].rect.y - wmap_ui_window_frame.y;
}

// 0x563B10
void wmap_ui_overview_to_map_coords(int x, int y, WmapCoords* coords)
{
    TigRect rect;
    int64_t width;
    int64_t height;

    rect = wmap_ui_mode_info[wmap_ui_mode].rect;

    if (is_default_wmap()) {
        width = 2000;
        height = 2000;
    } else {
        int64_t limit_x;
        int64_t limit_y;
        location_limits_get(&limit_x, &limit_y);

        width = limit_x / 64;
        height = limit_y / 64;
    }

    coords->x = (int)(((x - rect.x) * width) / rect.width);
    coords->y = (int)(((y - rect.y - wmap_ui_window_frame.y) * height) / rect.height);
}

// 0x563C00
void wmap_ui_screen_to_scrolled_coords(int x, int y, WmapCoords* coords)
{
    coords->x = x + wmap_ui_mode_info[wmap_ui_mode].scroll_x - wmap_ui_mode_info[wmap_ui_mode].rect.x;
    coords->y = y + wmap_ui_mode_info[wmap_ui_mode].scroll_y - wmap_ui_mode_info[wmap_ui_mode].rect.y - wmap_ui_window_frame.y;
}

// 0x563C60
bool wmap_ui_note_insert_or_update(WmapNote* note)
{
    WmapInfo* wmap_info;
    int index;

    wmap_info = &(wmap_ui_mode_info[wmap_ui_mode]);

    if (wmap_info->notes == NULL) {
        return false;
    }

    // Attempt to find and update by id.
    for (index = 0; index < *wmap_info->num_notes; index++) {
        if (wmap_info->notes[index].id == note->id) {
            // Note already exists, copy over everything.
            wmap_info->notes[index] = *note;
            wmap_ui_note_bounds_update(&(wmap_info->notes[index]));
            return true;
        }
    }

    if (index < 199) {
        // Note does not exists, but there is a room for a new note.
        wmap_info->notes[index] = *note;
        wmap_ui_note_bounds_update(&(wmap_info->notes[index]));
        note->flags &= ~0x4;
        (*wmap_info->num_notes)++;
        wmap_ui_note_changed_hook_stub(wmap_info->notes[index].id, wmap_ui_mode);
        return true;
    }

    // Note does not exists and there is no room for a new one.
    return false;
}

// 0x563D50
void wmap_ui_note_bounds_update(WmapNote* note)
{
    note->bounds.x = note->coords.x;
    note->bounds.y = note->coords.y;
    note->bounds.width = wmap_note_type_icon_max_width + 203;
    note->bounds.height = 50;
}

// 0x563D80
void wmap_ui_note_changed_hook_stub(int note_index, int change_flags)
{
    (void)note_index;
    (void)change_flags;
}

// 0x563D90
WmapNote* wmap_ui_note_find_by_id(int id)
{
    int index;

    for (index = 0; index < *wmap_ui_mode_info[wmap_ui_mode].num_notes; index++) {
        if (wmap_ui_mode_info[wmap_ui_mode].notes[index].id == id) {
            return &(wmap_ui_mode_info[wmap_ui_mode].notes[index]);
        }
    }

    return NULL;
}

// 0x563DE0
bool find_note_by_coords(WmapCoords* coords, int* id_ptr)
{
    return find_note_by_coords_mode(coords, id_ptr, wmap_ui_mode);
}

// 0x563E00
bool find_note_by_coords_mode(WmapCoords* coords, int* id_ptr, WmapUiMode mode)
{
    int dx;
    int dy;
    WmapInfo* wmap_info;
    int idx;
    WmapNote* note;

    switch (mode) {
    case WMAP_UI_MODE_WORLD:
    case WMAP_UI_MODE_TOWN:
        dx = wmap_note_type_icon_max_width / 2;
        dy = wmap_note_type_icon_max_height / 2;
        break;
    case WMAP_UI_MODE_CONTINENT:
        dx = 20;
        dy = 20;
        break;
    default:
        if (id_ptr != NULL) {
            *id_ptr = -1;
        }
        return false;
    }

    wmap_info = &(wmap_ui_mode_info[mode]);
    if (wmap_info->notes != NULL) {
        for (int pass = 0; pass < 2; pass++) {
            for (idx = *wmap_info->num_notes - 1; idx >= 0; idx--) {
                note = &(wmap_info->notes[idx]);
                if ((note->flags & MAP_MARKER_PERSONAL) != 0
                    && !settings_get_value(&settings, WORLDMAP_TOOLBAR_KEY)) continue;
                if (((note->flags & MAP_MARKER_PERSONAL) != 0) != (pass == 0)) continue;
                if (wmap_note_type_info[note->type].enabled
                    && (int64_t)coords->x >= (int64_t)note->coords.x - dx
                    && (int64_t)coords->x <= (int64_t)note->coords.x + dx
                    && (int64_t)coords->y >= (int64_t)note->coords.y - dy
                    && (int64_t)coords->y <= (int64_t)note->coords.y + dy) {
                    if (id_ptr != NULL) {
                        *id_ptr = note->id;
                    }
                    return true;
                }
            }
        }
    }

    if (id_ptr != NULL) {
        *id_ptr = -1;
    }

    return false;
}

// 0x563F00
bool wmap_ui_coords_snap_to_player(WmapCoords* coords, int64_t* location_ptr)
{
    int64_t requested_location;
    int64_t pc_location;

    if (location_ptr == NULL) {
        return false;
    }

    wmap_ui_coords_to_location(coords, &requested_location);
    pc_location = obj_field_int64_get(player_get_local_pc_obj(), OBJ_F_LOCATION);
    if (location_dist(requested_location, pc_location) < wmap_ui_known_area_search_radius) {
        wmap_ui_location_to_coords(pc_location, coords);
        *location_ptr = pc_location;
        return true;
    }

    *location_ptr = 0;
    return false;
}

// 0x563F90
void wmap_ui_new_note_edit_begin(WmapCoords* coords)
{
    wmap_ui_new_note.str[0] = '\0';
    wmap_ui_new_note.type = wmap_ui_selected_note_icon;
    wmap_ui_new_note.coords = *coords;
    wmap_ui_note_bounds_update(&wmap_ui_new_note);
    wmap_ui_new_note.location = 0;
    wmap_ui_coords_snap_to_player(&(wmap_ui_new_note.coords), &(wmap_ui_new_note.location));
    wmap_ui_note_edit_begin(&wmap_ui_new_note);
}

// 0x564000
void wmap_ui_note_icon_select(int icon_index)
{
    int note_types[3];

    note_types[0] = WMAP_NOTE_TYPE_NOTE;
    note_types[1] = WMAP_NOTE_TYPE_SKULL;
    note_types[2] = WMAP_NOTE_TYPE_CHEST;
    wmap_ui_selected_note_icon = note_types[icon_index];
}

// 0x564030
void wmap_ui_note_edit_begin(WmapNote* note)
{
    iso_interface_window_set(ROTWIN_TYPE_MAP_NOTE);
    wmap_ui_edited_note = note;
    wmap_ui_textedit.buffer = note->str;
    textedit_ui_focus(&wmap_ui_textedit);
    wmap_ui_textedit_focused = true;
    intgame_text_edit_refresh(note->str, wmap_note_type_info[WMAP_NOTE_TYPE_NOTE].font);
}

// 0x564070
void wmap_ui_note_edit_end(bool clear_displayed_text)
{
    wmap_ui_edited_note = 0;
    wmap_ui_new_note.str[0] = '\0';
    wmap_ui_new_note.id = -1;
    wmap_ui_textedit_focused = false;
    textedit_ui_unfocus(&wmap_ui_textedit);
    wmap_ui_textedit.buffer = NULL;
    if (clear_displayed_text) {
        intgame_text_edit_refresh(" ", wmap_note_type_info[WMAP_NOTE_TYPE_NOTE].font);
    }
}

// 0x5640C0
void wmap_ui_textedit_on_enter(TextEdit* textedit)
{
    bool should_refresh;

    if (*textedit->buffer != '\0' && wmap_ui_note_has_nonspace_text(textedit->buffer)) {
        if (textedit->buffer == wmap_ui_new_note.str) {
            wmap_ui_edited_note->flags = 0;
            should_refresh = wmap_note_add(wmap_ui_edited_note);
        } else {
            should_refresh = true;
        }
    } else {
        should_refresh = wmap_note_remove(wmap_ui_edited_note);
    }

    wmap_ui_note_edit_end(1);

    if (should_refresh) {
        wmap_ui_mode_info[wmap_ui_mode].refresh();
    }
}

// 0x564140
bool wmap_note_add(WmapNote* note)
{
    return wmap_note_add_mode(note, wmap_ui_mode);
}

// 0x564160
bool wmap_note_add_mode(WmapNote* note, WmapUiMode mode)
{
    int note_index;

    if (wmap_ui_mode_info[mode].notes == NULL
        || *wmap_ui_mode_info[mode].num_notes >= 200) {
        return false;
    }

    note_index = *wmap_ui_mode_info[mode].num_notes;

    int id = wmap_ui_mode_info[mode].next_note_id;
    if (mode == WMAP_UI_MODE_WORLD && id <= area_count()) id = area_count() + 1;
    // The world-note counter was not persisted by older saves. Allocate above
    // every current ID instead of overwriting a loaded personal pin.
    for (int i = 0; i < note_index; i++) {
        if (wmap_ui_mode_info[mode].notes[i].id >= id) {
            if (wmap_ui_mode_info[mode].notes[i].id == INT_MAX) return false;
            id = wmap_ui_mode_info[mode].notes[i].id + 1;
        }
    }
    if (id < 0 || id == INT_MAX) return false;
    note->id = id;
    wmap_ui_mode_info[mode].next_note_id = id + 1;
    note->flags &= ~0x4;
    wmap_ui_mode_info[mode].notes[note_index] = *note;
    wmap_ui_note_bounds_update(&(wmap_ui_mode_info[mode].notes[note_index]));
    (*wmap_ui_mode_info[mode].num_notes)++;
    wmap_ui_note_changed_hook_stub(wmap_ui_mode_info[mode].notes[note_index].id, mode);

    return true;
}

// 0x564210
bool wmap_note_remove(WmapNote* note)
{
    WmapInfo* wmap_info;
    int index;

    wmap_info = &(wmap_ui_mode_info[wmap_ui_mode]);

    if (wmap_info->notes == NULL) {
        return false;
    }

    if (note->id == -1) {
        return false;
    }

    // Find note by id.
    for (index = 0; index < *wmap_info->num_notes; index++) {
        if (wmap_info->notes[index].id == note->id) {
            break;
        }
    }

    if (index >= *wmap_info->num_notes) {
        // Note does not exists.
        return false;
    }

    wmap_ui_note_buffer_free(&(wmap_info->notes[index]));
    wmap_ui_note_removed_hook_stub(wmap_info->notes[index].id, wmap_ui_mode);

    (*wmap_info->num_notes)--;

    // Move subsequent notes up (one at a time).
    while (index < *wmap_info->num_notes) {
        wmap_info->notes[index] = wmap_info->notes[index + 1];
        index++;
    }

    return true;
}

// 0x5642E0
void wmap_ui_note_removed_hook_stub(int id, WmapUiMode mode)
{
    (void)id;
    (void)mode;
}

// 0x5642F0
void wmap_ui_note_buffer_free(WmapNote* note)
{
    if ((note->flags & 0x4) != 0) {
        tig_video_buffer_destroy(note->video_buffer);
        note->flags &= ~0x4;
    }
}

// 0x564320
void wmap_ui_textedit_on_change(TextEdit* textedit)
{
    if (*textedit->buffer != '\0') {
        intgame_text_edit_refresh(textedit->buffer, wmap_note_type_info[WMAP_NOTE_TYPE_NOTE].font);
    } else {
        intgame_text_edit_refresh(" ", wmap_note_type_info[WMAP_NOTE_TYPE_NOTE].font);
    }
}

// 0x564360
void wmap_ui_note_edit_by_id(int id)
{
    WmapInfo* wmap_info;
    int index;
    int cnt;

    wmap_info = &(wmap_ui_mode_info[wmap_ui_mode]);
    cnt = *wmap_info->num_notes;
    if (wmap_info->notes != NULL) {
        for (index = 0; index < cnt; index++) {
            if (wmap_info->notes[index].id == id) {
                break;
            }
        }

        if (index < cnt && (wmap_info->notes[index].flags & 0x2) == 0) {
            wmap_ui_note_edit_begin(&(wmap_info->notes[index]));
        }
    }
}

// 0x5643C0
bool wmap_ui_note_has_nonspace_text(const char* str)
{
    while (*str != '\0') {
        if (*str != ' ') {
            return true;
        }
        str++;
    }

    return false;
}

// 0x5643E0
bool wmap_ui_route_destination_add(WmapCoords* coords)
{
    int start = 0;
    WmapRouteType route_type = WMAP_ROUTE_TYPE_WORLD;
    int wp_idx;
    WmapRouteWaypoint* wp;
    int64_t from;
    int64_t to;
    MesFileEntry mes_file_entry;
    WmapPathInfo path_info;
    int steps;
    UiMessage ui_message;

    if (wmap_ui_world_travel_available) {
        if (wmap_ui_mode == WMAP_UI_MODE_TOWN) {
            route_type = WMAP_ROUTE_TYPE_TOWN;
        }
    } else {
        if (wmap_ui_mode != WMAP_UI_MODE_TOWN) {
            return false;
        }
        route_type = WMAP_ROUTE_TYPE_TOWN;
    }

    wp_idx = wmap_ui_routes[route_type].length;
    if (wp_idx >= MOVEMENT_ROUTE_CAPACITY) {
        mes_file_entry.num = 611;
        mes_get_msg(wmap_ui_worldmap_mes_file, &mes_file_entry);
        ui_message.type = UI_MSG_TYPE_FEEDBACK;
        ui_message.str = mes_file_entry.str;
        intgame_message_window_display_msg(&ui_message);
        return false;
    }

    wp = &(wmap_ui_routes[route_type].waypoints[wp_idx]);

    if (wmap_ui_mode == WMAP_UI_MODE_TOWN) {
        if (wp_idx == 0) {
            from = obj_field_int64_get(player_get_local_pc_obj(), OBJ_F_LOCATION);
        } else {
            from = wmap_ui_routes[route_type].waypoints[wp_idx - 1].loc;
        }

        townmap_coords_to_loc(&wmap_ui_tmi, coords->x, coords->y, &to);

        steps = anim_path_probe_between_locations(player_get_local_pc_obj(), from, to);
        if (steps == 0) {
            // "Your path is blocked. Try clicking closer to the previous
            // waypoint."
            mes_file_entry.num = 610;
            mes_get_msg(wmap_ui_worldmap_mes_file, &mes_file_entry);
            ui_message.type = UI_MSG_TYPE_FEEDBACK;
            ui_message.str = mes_file_entry.str;
            intgame_message_window_display_msg(&ui_message);
            return false;
        }
    } else {
        if (wp_idx == 0) {
            wmap_ui_coords_to_location(&(wmap_ui_mode_info[wmap_ui_mode].player_coordinates), &from);
        } else {
            start = wmap_ui_routes[route_type].waypoints[wp_idx - 1].start + wmap_ui_routes[route_type].waypoints[wp_idx - 1].steps;
            from = wmap_ui_routes[route_type].waypoints[wp_idx - 1].loc;
        }

        path_info.from = sector_id_from_loc(from);

        wmap_ui_coords_to_location(coords, &to);
        path_info.to = sector_id_from_loc(to);
        path_info.max_rotations = 5000 - start;
        path_info.rotations = &(wmap_ui_world_path_rotations[start]);

        steps = wmap_path_make(&path_info);
        if (steps == 0) {
            // "Your path is blocked. You must locate either a bridge, pass or
            // means of transporation in order to travel to this destination."
            mes_file_entry.num = 601;
            mes_get_msg(wmap_ui_worldmap_mes_file, &mes_file_entry);
            ui_message.type = UI_MSG_TYPE_FEEDBACK;
            ui_message.str = mes_file_entry.str;
            intgame_message_window_display_msg(&ui_message);
            return false;
        }
    }

    wmap_ui_selected_waypoint = wmap_ui_routes[route_type].length;

    wp->steps = steps;
    wp->start = start;
    wp->reserved_flags = 0;
    wp->coords = *coords;
    wp->loc = to;
    wp->travel_value = -1;

    wmap_ui_waypoint_travel_value_set(wmap_ui_routes[route_type].length, wmap_ui_routes[route_type].default_travel_value, false);
    wmap_ui_routes[route_type].length++;

    wmap_ui_travel_time_refresh();

    wmap_ui_mode_info[wmap_ui_mode].refresh();

    return true;
}

// 0x564780
bool wmap_ui_route_waypoint_hit_test(WmapCoords* coords, int* idx_ptr)
{
    WmapRouteType route_type;
    int dx;
    int dy;
    int idx;

    route_type = wmap_ui_mode == WMAP_UI_MODE_TOWN
        ? WMAP_ROUTE_TYPE_TOWN
        : WMAP_ROUTE_TYPE_WORLD;
    dx = wmap_ui_note_width / 2 + 5;
    dy = wmap_ui_note_height / 2 + 5;

    for (idx = wmap_ui_routes[route_type].length - 1; idx >= 0; idx--) {
        if (coords->x >= wmap_ui_routes[route_type].waypoints[idx].coords.x - dx
            && coords->x <= wmap_ui_routes[route_type].waypoints[idx].coords.x + dx
            && coords->y >= wmap_ui_routes[route_type].waypoints[idx].coords.y - dy
            && coords->y <= wmap_ui_routes[route_type].waypoints[idx].coords.y + dy) {
            *idx_ptr = idx;
            return true;
        }
    }

    return false;
}

// 0x564830
void wmap_ui_waypoint_drag_hook_stub(int waypoint_index, WmapCoords* coords)
{
    (void)waypoint_index;
    (void)coords;
}

// 0x564840
void wmap_ui_route_waypoint_remove(int waypoint_index)
{
    WmapRouteType route_type;
    int index;

    route_type = wmap_ui_mode == WMAP_UI_MODE_TOWN
        ? WMAP_ROUTE_TYPE_TOWN
        : WMAP_ROUTE_TYPE_WORLD;
    if (wmap_ui_routes[route_type].length > 0 && waypoint_index < wmap_ui_routes[route_type].length) {
        wmap_ui_routes[route_type].length--;

        for (index = waypoint_index; index < wmap_ui_routes[route_type].length; index++) {
            wmap_ui_routes[route_type].waypoints[index] = wmap_ui_routes[route_type].waypoints[index + 1];
        }

        wmap_ui_travel_time_refresh();
        wmap_ui_mode_info[wmap_ui_mode].refresh();

        if (wmap_ui_routes[route_type].length == 0) {
            wmap_ui_current_path_step = 0;
        }
    }
}

// 0x5648E0
void wmap_ui_waypoint_travel_value_set(int waypoint_index, int travel_value, bool refresh)
{
    WmapRouteType route_type;

    if (waypoint_index == -1) {
        return;
    }

    route_type = wmap_ui_mode == WMAP_UI_MODE_TOWN
        ? WMAP_ROUTE_TYPE_TOWN
        : WMAP_ROUTE_TYPE_WORLD;
    if (wmap_ui_routes[route_type].waypoints[waypoint_index].travel_value == travel_value) {
        return;
    }

    wmap_ui_routes[route_type].waypoints[waypoint_index].travel_value = travel_value;

    if (refresh) {
        wmap_ui_travel_time_refresh();
        wmap_ui_mode_info[wmap_ui_mode].refresh();
    }
}

// 0x564940
void wmap_ui_selected_waypoint_snap(void)
{
    if (wmap_ui_selected_waypoint != -1 && wmap_ui_mode != WMAP_UI_MODE_TOWN) {
        wmap_ui_waypoint_snap_to_known_area(&(wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].waypoints[wmap_ui_selected_waypoint]));
    }
}

// 0x564970
void wmap_ui_waypoint_snap_to_known_area(WmapRouteWaypoint* wp)
{
    int area;

    area = area_get_nearest_known_area(wp->loc, player_get_local_pc_obj(), wmap_ui_known_area_search_radius);
    if (area > 0) {
        wp->loc = area_get_location(area);
        wmap_ui_location_to_coords(wp->loc, &(wp->coords));
    }
}

// 0x5649C0
void wmap_ui_travel_time_refresh(void)
{
    wmap_ui_draw_travel_time();
}

static void wmap_ui_draw_travel_time(void)
{
    WmapRoute* route = &wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD];
    // The bitmap font has 21-pixel glyph cells; a shorter drawing rectangle
    // makes tig_font_write silently skip the entire line.
    TigRect rect = { 684, 222, 86, 24 };
    TigArtBlitInfo blit_info = { 0 };
    TigFont font = { 0 };
    MesFileEntry day_label;
    MesFileEntry hour_label;
    char str[128];
    int hours = 0;
    int days;
    int index;

    if (wmap_ui_window == TIG_WINDOW_HANDLE_INVALID || wmap_ui_coordinate_text_buffer == NULL) {
        return;
    }

    // Restore the inset inside the travel-time frame when changing the estimate.
    tig_art_interface_id_create(138, 0, 0, 0, &blit_info.art_id);
    blit_info.src_rect = &rect;
    blit_info.dst_rect = &rect;
    tig_window_blit_art(wmap_ui_window, &blit_info);

    // Town movement and teleport selection do not use world travel time.
    if (wmap_ui_mode == WMAP_UI_MODE_TOWN || !wmap_ui_world_travel_available || wmap_ui_selecting || route->length == 0) {
        wmap_ui_travel_time_valid = false;
        return;
    }

    if (!wmap_ui_travel_time_valid || wmap_ui_state != WMAP_UI_STATE_MOVING
        || tig_timer_elapsed(wmap_ui_travel_time_updated) >= 1000) {
        for (index = 0; index < route->length; index++) {
            WmapRouteWaypoint* wp = &route->waypoints[index];
            int start = wp->start > wmap_ui_current_path_step ? wp->start : wmap_ui_current_path_step;
            int remaining = wp->start + wp->steps - start;

            // Each sector step takes one hour. The travel callback does not
            // advance game time on the final step of each waypoint.
            if (remaining > 1) {
                hours += remaining - 1;
            }
        }

        wmap_ui_travel_time_hours = hours;
        wmap_ui_travel_time_valid = true;
        tig_timer_now(&wmap_ui_travel_time_updated);
    } else {
        // Redraw the cached value if another part of the window is refreshed.
        hours = wmap_ui_travel_time_hours;
    }

    days = hours / 24;
    hours %= 24;
    day_label.num = days == 1 ? 901 : 900;
    day_label.str = (char*)mes_fallback_get("worldmap", day_label.num);
    mes_search(wmap_ui_worldmap_mes_file, &day_label);
    // French uses the singular for zero as well as one ("0 heure").
    bool singular_hours = hours == 1
        || (hours == 0 && gamelib_current_language_get() == GAME_LANGUAGE_FRENCH);
    hour_label.num = days == 0 ? (singular_hours ? 904 : 903) : 905;
    hour_label.str = (char*)mes_fallback_get("worldmap", hour_label.num);
    mes_search(wmap_ui_worldmap_mes_file, &hour_label);
    if (days == 0) {
        snprintf(str, sizeof(str), "%d %s", hours, hour_label.str);
    } else if (hours == 0) {
        snprintf(str, sizeof(str), "%d %s", days, day_label.str);
    } else {
        snprintf(str, sizeof(str), "%d %s, %d%s", days, day_label.str, hours, hour_label.str);
    }

    tig_font_push(wmap_note_type_info[WMAP_NOTE_TYPE_NOTE].font);
    font.str = str;
    tig_font_measure(&font);
    if (days > 0 && font.width > rect.width) {
        // Keep very long durations on one line using the localized short unit.
        day_label.num = 902;
        day_label.str = (char*)mes_fallback_get("worldmap", day_label.num);
        mes_search(wmap_ui_worldmap_mes_file, &day_label);
        if (hours == 0) {
            snprintf(str, sizeof(str), "%d %s", days, day_label.str);
        } else {
            snprintf(str, sizeof(str), "%d %s, %d%s", days, day_label.str, hours, hour_label.str);
        }
        font.width = 0;
        tig_font_measure(&font);
    }
    if (font.width < rect.width) {
        rect.x += (rect.width - font.width) / 2;
        rect.width = font.width;
    }
    rect.y += 1;
    tig_window_text_write(wmap_ui_window, str, &rect);
    tig_font_pop();
}

// 0x5649D0
void wmap_ui_selected_waypoint_value_hook_stub(int waypoint_value)
{
    (void)waypoint_value;
}

// 0x5649F0
bool wmap_ui_travel_arrival_apply(int64_t loc)
{
    WmapInfo* wmap_info;
    int area;

    wmap_info = &(wmap_ui_mode_info[wmap_ui_mode]);
    area = area_get_nearest_area_in_range(loc, true);
    if (area > 0) {
        loc = area_get_location(area);
    }

    if (wmap_ui_teleport(loc)) {
        wmap_ui_location_to_coords(loc, &(wmap_info->player_coordinates));
    }

    if (wmap_info->refresh != NULL) {
        wmap_info->refresh();
    }

    ambient_lighting_enable();

    return area > 0;
}

// 0x564A70
void wmap_ui_nearby_area_discover(int64_t pc_obj, int64_t loc)
{
    int area;

    area = area_get_nearest_area_in_range(loc, false);
    if (area > 0) {
        if (!area_is_known(pc_obj, area)) {
            if (area_get_last_known_area(pc_obj) == area) {
                area_reset_last_known_area(pc_obj);
            }
            area_set_known(pc_obj, area);
            wmap_ui_area_note_add(area);
        }

        if (area_get_last_known_area(pc_obj) == area) {
            area_reset_last_known_area(pc_obj);
        }
    }
}

// 0x564AF0
void wmap_ui_mark_townmap(int64_t obj)
{
    int64_t pc_loc;
    int pc_townmap;
    int64_t obj_loc;
    int obj_townmap;
    WmapNote note;

    pc_loc = obj_field_int64_get(player_get_local_pc_obj(), OBJ_F_LOCATION);
    pc_townmap = townmap_get(sector_id_from_loc(pc_loc));

    obj_loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    obj_townmap = townmap_get(sector_id_from_loc(obj_loc));

    if (pc_townmap != obj_townmap) {
        return;
    }

    if (pc_townmap == 0) {
        tig_debug_printf("WmapUI: WARNING: Object attempted to mark TownMap when no TownMap is available!\n");
        return;
    }

    if (!townmap_info(pc_townmap, &wmap_ui_tmi)) {
        return;
    }

    townmap_loc_to_coords(&wmap_ui_tmi, obj_loc, &(note.coords.x), &(note.coords.y));

    if (find_note_by_coords_mode(&(note.coords), NULL, WMAP_UI_MODE_TOWN)) {
        return;
    }

    object_examine(obj, OBJ_HANDLE_NULL, note.str);
    wmap_ui_note_bounds_update(&note);
    note.location = 0;
    note.flags = 0x2;
    note.type = WMAP_NOTE_TYPE_QUEST;

    wmap_ui_town_notes_count = pc_townmap;

    if (!wmap_note_add_mode(&note, 2)) {
        tig_debug_printf("WmapUI: WARNING: Attempt to mark TownMap Failed: Note didn't Add!\n");
    }
}

// 0x564C20
bool wmap_ui_bkg_process_callback(TimeEvent* timeevent)
{
    bool waypoint_reached;
    bool enter_destination;
    WmapInfo* wmap_info;
    DateTime travel_duration;
    DateTime datetime;
    TimeEvent next_timeevent;
    WmapCoords previous_coordinates;
    WmapCoords next_coordinates;
    int64_t loc;

    (void)timeevent;

    waypoint_reached = false;
    enter_destination = false;
    wmap_info = &(wmap_ui_mode_info[wmap_ui_mode]);

    if (wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].length > 0) {
        previous_coordinates = wmap_info->player_coordinates;

        next_coordinates = wmap_info->player_coordinates;
        wmap_ui_world_route_step(&next_coordinates);
        wmap_ui_destination_to_nearest_area(&next_coordinates, &loc);
        wmap_info->player_coordinates = next_coordinates;

        wmap_ui_nearby_area_discover(player_get_local_pc_obj(), loc);

        if (wmap_ui_current_path_step >= wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].waypoints[0].start + wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].waypoints[0].steps) {
            waypoint_reached = true;

            wmap_ui_route_waypoint_remove(0);

            if (wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].length > 0) {
                if (!wmap_ui_player_prepare_for_travel()) {
                    while (wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].length > 0) {
                        wmap_ui_route_waypoint_remove(0);
                    }

                    wmap_ui_travel_arrival_apply(loc);
                }
            } else {
                if (wmap_ui_travel_arrival_apply(loc)) {
                    enter_destination = true;
                }
            }
        }

        wmap_ui_scroll_origin_set(&wmap_info->player_coordinates, false);
        wmap_ui_compass_arrow_frame_set(wmap_ui_compass_arrow_frame_calc(&wmap_info->player_coordinates, &wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].waypoints[0].coords));
        wmap_ui_travel_time_refresh();
        wmap_ui_mode_info[wmap_ui_mode].refresh();
    }

    wmap_ui_direct_travel_hours_estimate(&previous_coordinates, &(wmap_info->player_coordinates), &travel_duration);
    if (!waypoint_reached) {
        timeevent_inc_datetime(&travel_duration);
    }

    if (enter_destination) {
        wmap_ui_state_set(WMAP_UI_STATE_NORMAL);
        wmap_ui_close();
        return true;
    }

    if (wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].length <= 0
        || wmap_ui_current_path_step >= wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].waypoints[0].start + wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].waypoints[0].steps) {
        wmap_ui_state_set(WMAP_UI_STATE_NORMAL);
        wmap_ui_close();
        wmap_ui_current_path_step = 0;
        wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].length = 0;
        return true;
    }

    next_timeevent.type = TIMEEVENT_TYPE_WORLDMAP;
    datetime_init(&datetime, 62);
    timeevent_add_delay(&next_timeevent, &datetime);
    return true;
}

// 0x564E30
void wmap_ui_destination_to_nearest_area(WmapCoords* coords, int64_t* loc_ptr)
{
    int x;
    int y;
    int area;

    x = ((wmap_ui_sector_x_origin - coords->x) << 6) + 32;
    y = (coords->y << 6) + 32;
    *loc_ptr = location_make(x, y);

    area = area_get_nearest_known_area(*loc_ptr, player_get_local_pc_obj(), wmap_ui_known_area_search_radius);
    if (area > 0) {
        if (!area_is_known(player_get_local_pc_obj(), area)) {
            *loc_ptr = area_get_location(area);
            wmap_ui_location_to_coords(*loc_ptr, coords);
        }
    }
}

// 0x564EE0
int64_t wmap_ui_direct_travel_hours_estimate(WmapCoords* from_coords, WmapCoords* to_coords, DateTime* datetime)
{
    int64_t from_location;
    int64_t to_location;
    int travel_hours;

    wmap_ui_coords_to_location(from_coords, &from_location);
    wmap_ui_coords_to_location(to_coords, &to_location);
    travel_hours = (int)(location_dist(from_location, to_location) / 64);

    if (datetime != NULL) {
        datetime_init(datetime, 3600000 * travel_hours);
    }

    return travel_hours;
}

// 0x564F60
void wmap_ui_notify_sector_changed(int64_t pc_obj, int64_t sec)
{
    wmap_ui_townmap = townmap_get(sec);
    if (wmap_ui_townmap != TOWNMAP_NONE) {
        wmap_ui_nearby_area_discover(pc_obj, sector_loc_from_id(sec));
        wmap_ui_town_notes_load();
        ui_set_map_button(UI_PRIMARY_BUTTON_TOWNMAP);
    } else {
        wmap_ui_town_notes_clear();
        ui_set_map_button(UI_PRIMARY_BUTTON_WORLDMAP);
    }
    wmap_ui_travel_context_refresh();
}

// 0x564FD0
void wmap_ui_town_notes_load(void)
{
    char path[TIG_MAX_PATH];
    TigFile* stream;
    int index;
    bool success = true;

    if (wmap_ui_town_notes_count != wmap_ui_townmap) {
        wmap_ui_town_notes_clear();
    }
    wmap_ui_town_notes_count = wmap_ui_townmap;

    if (wmap_ui_town_notes_count != 0) {
        snprintf(path, sizeof(path),
            "%s\\%s.tmn",
            "Save\\Current",
            townmap_name(wmap_ui_town_notes_count));

        stream = tig_file_fopen(path, "rb");
        if (stream == NULL) {
            return;
        }

        for (index = 0; index < wmap_ui_num_town_notes; index++) wmap_ui_note_buffer_free(&wmap_ui_town_notes[index]);

        if (tig_file_fread(&wmap_ui_num_town_notes, sizeof(wmap_ui_num_town_notes), 1, stream) == 1
            && tig_file_fread(&(wmap_ui_mode_info[WMAP_UI_MODE_TOWN].next_note_id), sizeof(wmap_ui_mode_info[WMAP_UI_MODE_TOWN].next_note_id), 1, stream) == 1
            && wmap_ui_num_town_notes >= 0 && wmap_ui_num_town_notes <= SDL_arraysize(wmap_ui_town_notes)) {
            for (index = 0; index < wmap_ui_num_town_notes; index++) {
                if (!wmap_ui_town_note_load(&(wmap_ui_town_notes[index]), stream)) {
                    wmap_ui_num_town_notes = index;
                    success = false;
                    break;
                }
            }
        } else {
            wmap_ui_num_town_notes = 0;
            success = false;
        }

        tig_file_fclose(stream);

        if (!success) {
            tig_debug_printf("wmap_ui_town_notes_load(): Warning reading data!\n");
        }
    }
}

// 0x5650C0
void wmap_ui_town_notes_clear(void)
{
    if (wmap_ui_town_notes_count != 0) {
        wmap_ui_town_notes_save();
    }
    wmap_ui_town_notes_count = 0;
    for (int index = 0; index < wmap_ui_num_town_notes; index++) wmap_ui_note_buffer_free(&wmap_ui_town_notes[index]);
    wmap_ui_num_town_notes = 0;
}

// 0x5650E0
int wmap_ui_compass_arrow_frame_calc(WmapCoords* a, WmapCoords* b)
{
    if (b->x < a->x) {
        if (b->y < a->y) {
            return 3 * wmap_ui_compass_arrow_num_frames / 4;
        } else {
            return wmap_ui_compass_arrow_num_frames / 2;
        }
    } else {
        if (b->y < a->y) {
            return 0;
        } else {
            return wmap_ui_compass_arrow_num_frames / 4;
        }
    }
}

// 0x565130
void wmap_ui_compass_arrow_frame_set(int frame)
{
    (void)frame;
}

// NOTE: It's just a guess, never used.
//
// 0x5649E0
int wmap_ui_compass_arrow_frame_get(void)
{
    return 0;
}

// 0x565140
bool wmap_ui_player_prepare_for_travel(void)
{
    if (!anim_object_interrupt_by_priority(player_get_local_pc_obj(), 3, 0, 1)) {
        return false;
    }

    magictech_demaintain_spells(player_get_local_pc_obj());

    return true;
}

// 0x565170
void wmap_ui_world_route_step(WmapCoords* coords)
{
    int64_t loc;
    int64_t adjacent_sec;

    wmap_ui_coords_to_location(coords, &loc);
    sector_in_dir(sector_id_from_loc(loc), wmap_ui_world_path_rotations[wmap_ui_current_path_step], &adjacent_sec);

    wmap_ui_current_path_step++;
    if (wmap_ui_current_path_step < wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].waypoints[0].start + wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].waypoints[0].steps) {
        wmap_ui_location_to_coords(sector_loc_from_id(adjacent_sec), coords);
    } else {
        wmap_ui_location_to_coords(wmap_ui_routes[WMAP_ROUTE_TYPE_WORLD].waypoints[0].loc, coords);
    }
}

// 0x565230
void wmap_ui_overview_draw(void)
{
    TigVideoBufferBlitInfo vb_blit_info;
    TigArtFrameData art_frame_data;
    TigArtBlitInfo art_blit_info;
    TigRect src_rect;
    TigRect dst_rect;
    TigRect tmp_rect;
    TigRect vb_dst_rect;
    WmapNote* note;
    WmapCoords coords;
    int idx;
    int area;

    if (wmap_ui_mode_info[WMAP_UI_MODE_WORLD].str[0] == '\0') {
        return;
    }

    if (!wmap_ui_tile_art_load(wmap_ui_mode_info[WMAP_UI_MODE_WORLD].str, &(wmap_ui_mode_info[WMAP_UI_MODE_WORLD].video_buffer), &(wmap_ui_mode_info[WMAP_UI_MODE_WORLD].overview_image_rect))) {
        return;
    }

    wmap_ui_mode_info[WMAP_UI_MODE_WORLD].flags |= 0x01;

    tig_window_fill(wmap_ui_window, &wmap_ui_canvas_frame, tig_color_make(0, 0, 0));

    vb_dst_rect = wmap_ui_mode_info[wmap_ui_mode].rect;

    vb_blit_info.flags = 0;
    vb_blit_info.src_video_buffer = wmap_ui_mode_info[WMAP_UI_MODE_WORLD].video_buffer;
    vb_blit_info.src_rect = &(wmap_ui_mode_info[WMAP_UI_MODE_WORLD].overview_image_rect);
    vb_blit_info.dst_rect = &vb_dst_rect;

    if (tig_window_vbid_get(wmap_ui_window, &vb_blit_info.dst_video_buffer) != TIG_OK
        || tig_video_buffer_blit(&vb_blit_info) != TIG_OK) {
        tig_debug_printf("WMapUI: Zoomed Blit: ERROR: Blit FAILED!\n");
        return;
    }

    art_blit_info.flags = 0;
    art_blit_info.src_rect = &src_rect;
    art_blit_info.dst_rect = &dst_rect;

    for (idx = 0; idx < *wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_notes; idx++) {
        note = &(wmap_ui_mode_info[WMAP_UI_MODE_WORLD].notes[idx]);
        if (wmap_note_type_info[note->type].enabled) {
            tmp_rect.x = vb_dst_rect.x + note->coords.x;
            tmp_rect.y = vb_dst_rect.y + note->coords.y + wmap_ui_window_frame.y;
            wmap_ui_overview_position_project_legacy(tmp_rect.x, tmp_rect.y, &coords);
            tmp_rect.x = vb_dst_rect.x + coords.x;
            tmp_rect.y = vb_dst_rect.y + coords.y;
            tmp_rect.width = 2;
            tmp_rect.height = 2;
            tig_window_fill(wmap_ui_window, &tmp_rect, tig_color_make(255, 0, 0));
        }
    }

    area = area_get_last_known_area(player_get_local_pc_obj());
    if (area != AREA_UNKNOWN) {
        wmap_ui_location_to_coords(area_get_location(area), &coords);
        tmp_rect.x = vb_dst_rect.x + note->coords.x;
        tmp_rect.y = vb_dst_rect.y + note->coords.y + wmap_ui_window_frame.y;
        wmap_ui_overview_position_project_legacy(tmp_rect.x, tmp_rect.y, &coords);
        tmp_rect.x = vb_dst_rect.x + coords.x - 1;
        tmp_rect.y = vb_dst_rect.y + coords.y - 1;
        tmp_rect.width = 5;
        tmp_rect.height = 5;
        tig_window_fill(wmap_ui_window, &tmp_rect, tig_color_make(255, 255, 0));
    }

    tmp_rect.x = vb_dst_rect.x + wmap_ui_mode_info[WMAP_UI_MODE_WORLD].player_coordinates.x;
    tmp_rect.y = vb_dst_rect.y + wmap_ui_mode_info[WMAP_UI_MODE_WORLD].player_coordinates.y + wmap_ui_window_frame.y;
    wmap_ui_overview_position_project_legacy(tmp_rect.x, tmp_rect.y, &coords);
    tmp_rect.x = vb_dst_rect.x + coords.x;
    tmp_rect.y = vb_dst_rect.y + coords.y;
    tmp_rect.width = 3;
    tmp_rect.height = 3;
    tig_window_fill(wmap_ui_window, &tmp_rect, tig_color_make(0, 255, 0));

    if (tig_art_interface_id_create(217, 0, 0, 0, &(art_blit_info.art_id)) == TIG_OK
        && tig_art_frame_data(art_blit_info.art_id, &art_frame_data) == TIG_OK) {
        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.width = art_frame_data.width;
        src_rect.height = art_frame_data.height;

        dst_rect.y = 382 - wmap_ui_window_frame.y;
        dst_rect.x = 294;
        dst_rect.width = art_frame_data.width;
        dst_rect.height = art_frame_data.height;

        art_blit_info.flags = 0;
        art_blit_info.src_rect = &src_rect;
        art_blit_info.dst_rect = &dst_rect;
        tig_window_blit_art(wmap_ui_window, &art_blit_info);
    }
}

// 0x5656B0
void wmap_ui_overview_position_project_legacy(int x, int y, WmapCoords* coords)
{
    TigRect rect;
    int64_t width;
    int64_t height;
    int offset_x;
    int offset_y;

    rect = wmap_ui_mode_info[wmap_ui_mode].rect;

    if (is_default_wmap()) {
        width = 2000;
        height = 2000;
    } else {
        int64_t limit_x;
        int64_t limit_y;
        location_limits_get(&limit_x, &limit_y);

        width = limit_x / 64;
        height = limit_y / 64;
    }

    offset_x = x - rect.x;
    if (offset_x == 0) {
        offset_x = 1;
    }
    coords->x = (int)(offset_x * rect.width / width);

    offset_y = y - wmap_ui_window_frame.y - rect.y;
    if (offset_y == 0) {
        offset_y = 1;
    }
    coords->y = (int)(offset_y * rect.height / height);
}

// 0x5657A0
void wmap_world_refresh_rect(TigRect* rect)
{
    WmapInfo* wmap_info;
    TigRect tmp_rect;
    TigVideoBufferBlitInfo vb_blit_info;
    TigRect vb_src_rect;
    TigRect vb_dst_rect;
    TigArtBlitInfo art_blit_info;
    TigRect src_rect;
    TigRect dst_rect;
    int offset_x;
    int offset_y;
    WmapTile* tile;
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int x;
    int y;
    int idx;
    WmapNote* note;
    int area;

    wmap_info = &(wmap_ui_mode_info[wmap_ui_mode]);

    if (rect == NULL) {
        rect = &(wmap_info->rect);
    }

    tmp_rect = *rect;

    tig_window_fill(wmap_ui_window, &tmp_rect, tig_color_make(0, 0, 0));

    if (wmap_info->tile_directory[0] == '\0') {
        return;
    }

    offset_x = wmap_info->scroll_x;
    offset_y = wmap_info->scroll_y;

    art_blit_info.flags = 0;
    art_blit_info.src_rect = &src_rect;
    art_blit_info.dst_rect = &dst_rect;

    vb_src_rect.width = 5;
    vb_src_rect.height = 5;

    vb_blit_info.flags = 0;
    vb_blit_info.src_rect = &vb_src_rect;
    vb_blit_info.dst_rect = &vb_dst_rect;

    if (tig_window_vbid_get(wmap_ui_window, &(vb_blit_info.dst_video_buffer)) != TIG_OK) {
        tig_debug_printf("WMapUI: Zoomed Blit: ERROR: Blit FAILED!\n");
        return;
    }

    min_x = offset_x / wmap_info->tile_width;
    if (min_x < 0) {
        min_x = 0;
    }

    min_y = offset_y / wmap_info->tile_height;
    if (min_y < 0) {
        min_y = 0;
    }

    tile = &(wmap_info->tiles[min_y * wmap_info->num_hor_tiles + min_x]);

    max_x = tmp_rect.width / tile->rect.width + min_x + 1;
    if (max_x > wmap_info->num_hor_tiles) {
        max_x = wmap_info->num_hor_tiles;
    }

    max_y = tmp_rect.height / tile->rect.height + min_y + 2;
    if (max_y > wmap_info->num_vert_tiles) {
        max_y = wmap_info->num_vert_tiles;
    }

    for (y = min_y; y < max_y; y++) {
        for (x = min_x; x < max_x; x++) {
            idx = y * wmap_info->num_hor_tiles + x;
            tile = &(wmap_info->tiles[idx]);
            vb_src_rect.x = tmp_rect.x + tile->rect.width * x - offset_x;
            vb_src_rect.y = tmp_rect.y + tile->rect.height * y - offset_y;
            vb_src_rect.width = tile->rect.width;
            vb_src_rect.height = tile->rect.height;

            if (tig_rect_intersection(&vb_src_rect, &tmp_rect, &vb_dst_rect) != TIG_OK) {
                break;
            }

            vb_src_rect.x = vb_dst_rect.x - vb_src_rect.x;
            vb_src_rect.y = vb_dst_rect.y - vb_src_rect.y;
            vb_src_rect.width = vb_dst_rect.width;
            vb_src_rect.height = vb_dst_rect.height;

            if (wmap_ui_tile_art_lock(idx)) {
                vb_blit_info.src_video_buffer = tile->video_buffer;
                if (tig_video_buffer_blit(&vb_blit_info) != TIG_OK) {
                    tig_debug_printf("WMapUI: Zoomed Blit: ERROR: Blit FAILED!\n");
                    return;
                }

                wmap_ui_tile_art_unlock(idx);
            }
        }
    }

    art_blit_info.flags = 0;
    art_blit_info.src_rect = &src_rect;
    art_blit_info.dst_rect = &dst_rect;

    vb_src_rect.x = 0;
    vb_src_rect.y = 0;

    vb_dst_rect.x = 0;
    vb_dst_rect.y = 0;

    for (idx = 0; idx < *wmap_info->num_notes; idx++) {
        note = &(wmap_info->notes[idx]);
        if ((note->flags & MAP_MARKER_PERSONAL) != 0
            && !settings_get_value(&settings, WORLDMAP_TOOLBAR_KEY)) continue;
        if (wmap_note_type_info[note->type].enabled
            && wmap_ui_note_check_stub(note)) {
            wmap_ui_note_icon_draw(note->type, &(note->coords), &tmp_rect, rect, wmap_info, (note->flags & MAP_MARKER_COMPLETED) != 0);
            if ((note->flags & 0x2) != 0) wmap_ui_note_blit(note, &tmp_rect, rect);
        }
    }

    wmap_ui_route_draw(wmap_info, &tmp_rect, rect);

    area = area_get_last_known_area(player_get_local_pc_obj());
    if (area != AREA_UNKNOWN) {
        WmapCoords coords;

        wmap_ui_location_to_coords(area_get_location(area), &coords);
        wmap_ui_note_icon_draw(WMAP_NOTE_TYPE_NEW_LOC, &coords, &tmp_rect, rect, wmap_info, false);
    }

    offset_x = wmap_info->player_coordinates.x - wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].width / 2 - wmap_info->scroll_x;
    offset_y = wmap_info->player_coordinates.y + wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].height / -2 - wmap_info->scroll_y;

    vb_dst_rect.x = offset_x + tmp_rect.x;
    vb_dst_rect.y = offset_y + tmp_rect.y;
    vb_dst_rect.width = wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].width;
    vb_dst_rect.height = wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].height;

    if (tig_rect_intersection(&vb_dst_rect, rect, &vb_dst_rect) == TIG_OK) {
        dst_rect = vb_dst_rect;
        vb_dst_rect.x -= tmp_rect.x + offset_x;
        vb_dst_rect.y -= tmp_rect.y + offset_y;

        art_blit_info.flags = 0;
        art_blit_info.art_id = wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].art_id;
        art_blit_info.src_rect = &vb_dst_rect;
        art_blit_info.dst_rect = &dst_rect;
        art_blit_info.dst_video_buffer = wmap_ui_coordinate_text_buffer;
        tig_window_blit_art(wmap_ui_window, &art_blit_info);
    }

    if (tig_rect_intersection(&wmap_ui_nav_cvr_frame, rect, &dst_rect) == TIG_OK) {
        src_rect.x = wmap_ui_nav_cvr_frame.x - dst_rect.x;
        src_rect.y = wmap_ui_nav_cvr_frame.y - dst_rect.y;
        src_rect.width = wmap_ui_nav_cvr_width - (wmap_ui_nav_cvr_frame.x - dst_rect.x);
        src_rect.height = wmap_ui_nav_cvr_height - (wmap_ui_nav_cvr_frame.y - dst_rect.y);

        art_blit_info.flags = 0;
        art_blit_info.art_id = wmap_ui_nav_cvr_art_id;
        art_blit_info.src_rect = &src_rect;
        art_blit_info.dst_rect = &dst_rect;
        tig_window_blit_art(wmap_ui_window, &art_blit_info);
    }
}

// 0x565CF0
bool wmap_ui_note_check_stub(WmapNote* note)
{
    (void)note;

    return true;
}

// 0x565D00
void wmap_ui_note_blit(WmapNote* note, TigRect* viewport_rect, TigRect* clip_rect)
{
    TigWindowBlitInfo win_blit_info;
    TigRect src_rect;
    TigRect dst_rect;
    int x;
    int y;
    int dx;
    int dy;

    wmap_note_vbid_lock(note);

    if ((note->flags & 0x4) != 0) {
        x = 0;
        y = 0;
        dx = viewport_rect->x + note->coords.x - wmap_ui_mode_info[wmap_ui_mode].scroll_x;
        dy = viewport_rect->y + note->coords.y - wmap_ui_mode_info[wmap_ui_mode].scroll_y;

        if (wmap_ui_mode == WMAP_UI_MODE_WORLD && note->id <= area_count()) {
            area_get_wm_offset(note->id, &x, &y);
        }

        x += dx - 10;
        y += dy - 10;

        dst_rect.x = x;
        dst_rect.y = y;
        dst_rect.width = note->bounds.width;
        dst_rect.height = note->bounds.height;

        if (tig_rect_intersection(&dst_rect, clip_rect, &dst_rect) == TIG_OK) {
            src_rect.x = dst_rect.x - x;
            src_rect.y = dst_rect.y - y;
            src_rect.width = dst_rect.width;
            src_rect.height = dst_rect.height;

            win_blit_info.type = TIG_WINDOW_BLIT_VIDEO_BUFFER_TO_WINDOW;
            win_blit_info.vb_blit_flags = 0;
            win_blit_info.src_video_buffer = note->video_buffer;
            win_blit_info.src_rect = &src_rect;
            win_blit_info.dst_window_handle = wmap_ui_window;
            win_blit_info.dst_rect = &dst_rect;
            tig_window_blit(&win_blit_info);
        }
    }
}

// 0x565E40
void wmap_note_vbid_lock(WmapNote* note)
{
    TigVideoBufferCreateInfo vb_create_info;
    TigRect dirty_rect;

    if ((note->flags & 0x4) == 0) {
        vb_create_info.flags = TIG_VIDEO_BUFFER_LOCKED | TIG_VIDEO_BUFFER_VIDEO_MEMORY;
        vb_create_info.width = wmap_note_type_icon_max_width + 203;
        vb_create_info.height = 50;
        vb_create_info.background_color = wmap_ui_color_key;
        vb_create_info.color_key = wmap_ui_color_key;
        if (tig_video_buffer_create(&vb_create_info, &(note->video_buffer)) != TIG_OK) {
            tig_debug_printf("wmap_note_vbid_lock: ERROR: tig_video_buffer_create failed!\n");
            exit(EXIT_SUCCESS); // FIXME: Should be EXIT_FAILURE.
        }

        note->flags |= 0x4;

        wmap_ui_tile_buffer_draw(note->video_buffer, &(note->bounds));
        TigFont font = *(TigFont*)wmap_note_type_info[note->type].font;
        if ((note->flags & MAP_MARKER_COMPLETED) != 0) font.color = tig_color_make(128, 128, 128);
        char title[MAP_MARKER_TITLE_SIZE], body[MAP_MARKER_BODY_SIZE];
        map_marker_split(note->str, title, body);
        tig_font_push((tig_font_handle_t)&font);
        tig_font_write(note->video_buffer, (note->flags & MAP_MARKER_PERSONAL) ? title : note->str, &wmap_ui_note_bounds, &dirty_rect);
        tig_font_pop();
    }
}

// 0x565F00
void wmap_ui_tile_buffer_draw(TigVideoBuffer* video_buffer, TigRect* rect)
{
    TigVideoBufferBlitInfo vb_blit_info;
    TigRect src_rect;
    TigRect dst_rect;
    int offset_x;
    int offset_y;
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int idx;
    WmapTile* entry;
    int col;
    int row;
    TigRect bounds;

    if (rect == NULL) {
        return;
    }

    bounds.x = rect->x;
    bounds.y = rect->y;
    bounds.width = rect->width;
    bounds.height = rect->height;

    src_rect.width = 5;
    src_rect.height = 5;

    offset_x = wmap_ui_mode_info[WMAP_UI_MODE_WORLD].scroll_x;
    offset_y = wmap_ui_mode_info[WMAP_UI_MODE_WORLD].scroll_y;

    vb_blit_info.flags = TIG_VIDEO_BUFFER_BLIT_BLEND_ALPHA_CONST;
    vb_blit_info.src_rect = &src_rect;
    vb_blit_info.alpha[0] = 0;
    vb_blit_info.alpha[1] = 0;
    vb_blit_info.alpha[2] = 0;
    vb_blit_info.alpha[3] = 0;
    vb_blit_info.dst_video_buffer = video_buffer;
    vb_blit_info.dst_rect = &dst_rect;

    min_x = offset_x / wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tile_width;
    if (min_x < 0) {
        min_x = 0;
    }

    min_y = offset_y / wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tile_height;
    if (min_y < 0) {
        min_y = 0;
    }

    idx = min_x + wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_hor_tiles * min_y;
    entry = &(wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tiles[idx]);

    max_x = rect->width / entry->rect.width + min_x + 1;
    if (max_x > wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_hor_tiles) {
        max_x = wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_hor_tiles;
    }

    max_y = rect->height / entry->rect.height + min_y + 2;
    if (max_y > wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_vert_tiles) {
        max_y = wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_vert_tiles;
    }

    for (row = min_y; row < max_y; row++) {
        for (col = min_x; col < max_x; col++) {
            idx = col + wmap_ui_mode_info[WMAP_UI_MODE_WORLD].num_hor_tiles * row;
            entry = &(wmap_ui_mode_info[WMAP_UI_MODE_WORLD].tiles[idx]);

            src_rect.x = bounds.x + src_rect.width * min_x - offset_x;
            src_rect.y = bounds.y + src_rect.height * min_y - offset_y;

            if (tig_rect_intersection(&src_rect, &bounds, &dst_rect) == TIG_OK) {
                src_rect.x = dst_rect.x - src_rect.x;
                src_rect.y = dst_rect.y - src_rect.y;
                src_rect.width = dst_rect.width;
                src_rect.height = dst_rect.height;

                if (wmap_ui_tile_art_lock(idx)) {
                    dst_rect.x = 0;
                    dst_rect.y = 0;
                    vb_blit_info.src_video_buffer = entry->video_buffer;

                    if (tig_video_buffer_blit(&vb_blit_info) != TIG_OK) {
                        tig_debug_printf("WMapUI: Zoomed Blit: ERROR: Blit FAILED!\n");
                        return;
                    }

                    wmap_ui_tile_art_unlock(idx);
                }
            }
        }
    }
}

// 0x566130
void wmap_town_refresh_rect(TigRect* rect)
{
    int min_x;
    int min_y;
    int max_y;
    int max_x;
    int row;
    int col;
    int idx;
    TigRect bounds;
    TigRect dirty_rect;
    TigVideoBufferBlitInfo vb_blit_info;
    TigRect vb_src_rect;
    TigRect vb_dst_rect;
    TigArtBlitInfo art_blit_info;
    TigRect art_src_rect;
    TigRect art_dst_rect;
    WmapTile* entry;
    int64_t loc;
    int offset_x;
    int offset_y;
    ObjectList objects;
    ObjectNode* node;
    int note;

    if (rect == NULL) {
        rect = &(wmap_ui_mode_info[WMAP_UI_MODE_TOWN].rect);
    }

    offset_x = wmap_ui_mode_info[WMAP_UI_MODE_TOWN].scroll_x;
    offset_y = wmap_ui_mode_info[WMAP_UI_MODE_TOWN].scroll_y;
    bounds = wmap_ui_mode_info[WMAP_UI_MODE_TOWN].rect;

    vb_blit_info.flags = 0;
    vb_blit_info.src_rect = &vb_src_rect;
    vb_blit_info.dst_rect = &vb_dst_rect;

    if (tig_window_vbid_get(wmap_ui_window, &(vb_blit_info.dst_video_buffer)) != TIG_OK) {
        tig_debug_printf("WMapUI: wmap_town_refresh_rect(): tig_window_vbid_get() failed!\n");
        return;
    }

    dirty_rect.x = rect->x;
    dirty_rect.y = rect->y + wmap_ui_window_frame.y;
    dirty_rect.width = rect->width;
    dirty_rect.height = rect->height;
    tig_window_invalidate_rect(&dirty_rect);

    dirty_rect.x = rect->x;
    dirty_rect.y = rect->y;
    dirty_rect.width = rect->width;
    dirty_rect.height = rect->height;
    tig_window_fill(wmap_ui_window, &dirty_rect, tig_color_make(0, 0, 0));

    min_x = offset_x / wmap_ui_mode_info[WMAP_UI_MODE_TOWN].tile_width;
    if (min_x < 0) {
        min_x = 0;
    }

    min_y = offset_y / wmap_ui_mode_info[WMAP_UI_MODE_TOWN].tile_height;
    if (min_y < 0) {
        min_y = 0;
    }

    idx = min_x + min_y * wmap_ui_mode_info[WMAP_UI_MODE_TOWN].num_hor_tiles;
    entry = &(wmap_ui_mode_info[WMAP_UI_MODE_TOWN].tiles[idx]);

    max_x = bounds.width / entry->rect.width + min_x + 2;
    if (max_x > wmap_ui_mode_info[WMAP_UI_MODE_TOWN].num_hor_tiles) {
        max_x = wmap_ui_mode_info[WMAP_UI_MODE_TOWN].num_hor_tiles;
    }

    max_y = bounds.height / entry->rect.height + min_y + 3;
    if (max_y > wmap_ui_mode_info[WMAP_UI_MODE_TOWN].num_vert_tiles) {
        max_y = wmap_ui_mode_info[WMAP_UI_MODE_TOWN].num_vert_tiles;
    }

    for (row = min_y; row < max_y; row++) {
        for (col = min_x; col < max_x; col++) {
            idx = col + row * wmap_ui_mode_info[WMAP_UI_MODE_TOWN].num_hor_tiles;
            entry = &(wmap_ui_mode_info[WMAP_UI_MODE_TOWN].tiles[idx]);

            vb_src_rect.width = entry->rect.width;
            vb_src_rect.height = entry->rect.height;
            vb_src_rect.x = bounds.x + col * vb_src_rect.width - offset_x;
            vb_src_rect.y = bounds.y + row * vb_src_rect.height - offset_y;

            if (tig_rect_intersection(&vb_src_rect, &dirty_rect, &vb_dst_rect) == TIG_OK) {
                vb_src_rect.x = vb_dst_rect.x - vb_src_rect.x;
                vb_src_rect.y = vb_dst_rect.y - vb_src_rect.y;
                vb_src_rect.width = vb_dst_rect.width;
                vb_src_rect.height = vb_dst_rect.height;

                if (wmap_ui_tile_art_lock(idx)) {
                    if (townmap_is_waitable(wmap_ui_townmap)) {
                        vb_blit_info.flags = 0;
                        vb_blit_info.src_video_buffer = entry->video_buffer;
                        if (tig_video_buffer_blit(&vb_blit_info) != TIG_OK) {
                            tig_debug_printf("WMapUI: TownMap Blit: ERROR: Blit FAILED!\n");
                            return;
                        }
                    } else if (townmap_tile_blit_info(wmap_ui_townmap, idx, &vb_blit_info)) {
                        vb_blit_info.src_video_buffer = entry->video_buffer;
                        if (tig_video_buffer_blit(&vb_blit_info) != TIG_OK) {
                            tig_debug_printf("WMapUI: TownMap Blit: ERROR: Blit FAILED!\n");
                            return;
                        }
                        wmap_ui_tile_art_unlock(idx);
                    } else {
                        tig_window_fill(wmap_ui_window, &vb_dst_rect, tig_color_make(0, 0, 0));
                        wmap_ui_tile_art_unlock(idx);
                    }
                }
            }
        }
    }

    art_blit_info.flags = 0;
    art_blit_info.src_rect = &art_src_rect;
    art_blit_info.dst_rect = &art_dst_rect;

    for (note = 0; note < *wmap_ui_mode_info[WMAP_UI_MODE_TOWN].num_notes; note++) {
        offset_x = wmap_ui_mode_info[WMAP_UI_MODE_TOWN].notes[note].coords.x;
        offset_y = wmap_ui_mode_info[WMAP_UI_MODE_TOWN].notes[note].coords.y;

        offset_x -= wmap_note_type_info[WMAP_NOTE_TYPE_QUEST].width / 2 + wmap_ui_mode_info[WMAP_UI_MODE_TOWN].scroll_x;
        offset_y -= wmap_note_type_info[WMAP_NOTE_TYPE_QUEST].height / 2 + wmap_ui_mode_info[WMAP_UI_MODE_TOWN].scroll_y;

        vb_dst_rect.x = bounds.x + offset_x;
        vb_dst_rect.y = bounds.y + offset_y;
        vb_dst_rect.width = wmap_note_type_info[WMAP_NOTE_TYPE_QUEST].width;
        vb_dst_rect.height = wmap_note_type_info[WMAP_NOTE_TYPE_QUEST].height;

        if (tig_rect_intersection(&vb_dst_rect, &dirty_rect, &vb_dst_rect) == TIG_OK) {
            art_dst_rect = vb_dst_rect;
            vb_dst_rect.x -= bounds.x + offset_x;
            vb_dst_rect.y -= bounds.y + offset_y;

            art_blit_info.flags = 0;
            art_blit_info.art_id = wmap_note_type_info[WMAP_NOTE_TYPE_QUEST].art_id;
            art_blit_info.src_rect = &vb_dst_rect;
            art_blit_info.dst_video_buffer = wmap_ui_coordinate_text_buffer;
            art_blit_info.dst_rect = &art_dst_rect;
            tig_window_blit_art(wmap_ui_window, &art_blit_info);
        }
    }

    wmap_ui_route_draw(&(wmap_ui_mode_info[WMAP_UI_MODE_TOWN]), &bounds, &dirty_rect);
    offset_x = wmap_ui_mode_info[WMAP_UI_MODE_TOWN].player_coordinates.x - wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].width / 2 - wmap_ui_mode_info[WMAP_UI_MODE_TOWN].scroll_x;
    offset_y = wmap_ui_mode_info[WMAP_UI_MODE_TOWN].player_coordinates.y - wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].height / 2 - wmap_ui_mode_info[WMAP_UI_MODE_TOWN].scroll_y;

    vb_dst_rect.x = bounds.x + offset_x;
    vb_dst_rect.y = bounds.y + offset_y;
    vb_dst_rect.width = wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].width;
    vb_dst_rect.height = wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].height;

    if (tig_rect_intersection(&vb_dst_rect, &dirty_rect, &vb_dst_rect) == TIG_OK) {
        art_dst_rect = vb_dst_rect;
        vb_dst_rect.x -= bounds.x + offset_x;
        vb_dst_rect.y -= bounds.y + offset_y;

        art_blit_info.flags = 0;
        art_blit_info.art_id = wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].art_id;
        art_blit_info.src_rect = &vb_dst_rect;
        art_blit_info.dst_video_buffer = wmap_ui_coordinate_text_buffer;
        art_blit_info.dst_rect = &art_dst_rect;
        tig_window_blit_art(wmap_ui_window, &art_blit_info);
    }

    object_list_all_followers(player_get_local_pc_obj(), &objects);
    node = objects.head;
    while (node != NULL) {
        loc = obj_field_int64_get(node->obj, OBJ_F_LOCATION);
        townmap_loc_to_coords(&wmap_ui_tmi, loc, &offset_x, &offset_y);
        offset_x -= wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].width / 2 + wmap_ui_mode_info[WMAP_UI_MODE_TOWN].scroll_x;
        offset_y -= wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].height / 2 + wmap_ui_mode_info[WMAP_UI_MODE_TOWN].scroll_y;

        vb_dst_rect.x = bounds.x + offset_x;
        vb_dst_rect.y = bounds.y + offset_y;
        vb_dst_rect.width = wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].width;
        vb_dst_rect.height = wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].height;

        if (tig_rect_intersection(&vb_dst_rect, &dirty_rect, &vb_dst_rect) == TIG_OK) {
            art_dst_rect = vb_dst_rect;
            vb_dst_rect.x -= bounds.x + offset_x;
            vb_dst_rect.y -= bounds.y + offset_y;

            art_blit_info.flags = 0;
            art_blit_info.art_id = wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].art_id;
            art_blit_info.src_rect = &vb_dst_rect;
            art_blit_info.dst_video_buffer = wmap_ui_coordinate_text_buffer;
            art_blit_info.dst_rect = &art_dst_rect;
            tig_window_blit_art(wmap_ui_window, &art_blit_info);
        }

        node = node->next;
    }
    object_list_destroy(&objects);

    if (tig_net_is_active()) {
        object_list_party(player_get_local_pc_obj(), &objects);
        node = objects.head;
        while (node != NULL) {
            if (!player_is_local_pc_obj(node->obj)) {
                loc = obj_field_int64_get(node->obj, OBJ_F_LOCATION);
                townmap_loc_to_coords(&wmap_ui_tmi, loc, &offset_x, &offset_y);
                offset_x -= wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].width / 2 + wmap_ui_mode_info[WMAP_UI_MODE_TOWN].scroll_x;
                offset_y -= wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].height / 2 + wmap_ui_mode_info[WMAP_UI_MODE_TOWN].scroll_y;

                vb_dst_rect.x = bounds.x + offset_x;
                vb_dst_rect.y = bounds.y + offset_y;
                vb_dst_rect.width = wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].width;
                vb_dst_rect.height = wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].height;

                if (tig_rect_intersection(&vb_dst_rect, &dirty_rect, &vb_dst_rect) == TIG_OK) {
                    art_dst_rect = vb_dst_rect;
                    vb_dst_rect.x -= bounds.x + offset_x;
                    vb_dst_rect.y -= bounds.y + offset_y;

                    art_blit_info.flags = 0;
                    art_blit_info.art_id = wmap_note_type_info[WMAP_NOTE_TYPE_CROSS].art_id;
                    art_blit_info.src_rect = &vb_dst_rect;
                    art_blit_info.dst_video_buffer = wmap_ui_coordinate_text_buffer;
                    art_blit_info.dst_rect = &art_dst_rect;
                    tig_window_blit_art(wmap_ui_window, &art_blit_info);
                }
            }
            node = node->next;
        }
        object_list_destroy(&objects);
    }

    if (tig_rect_intersection(&wmap_ui_nav_cvr_frame, &dirty_rect, &art_dst_rect) == TIG_OK) {
        art_src_rect.x = art_dst_rect.x - wmap_ui_nav_cvr_frame.x;
        art_src_rect.y = art_dst_rect.y - wmap_ui_nav_cvr_frame.y;
        art_src_rect.width = art_dst_rect.width;
        art_src_rect.height = art_dst_rect.height;

        art_blit_info.flags = 0;
        art_blit_info.art_id = wmap_ui_nav_cvr_art_id;
        art_blit_info.src_rect = &art_src_rect;
        art_blit_info.dst_rect = &art_dst_rect;
        tig_window_blit_art(wmap_ui_window, &art_blit_info);
    }
}

// 0x566A80
void wmap_ui_route_draw(WmapInfo* map_info, TigRect* viewport_rect, TigRect* clip_rect)
{
    WmapRouteType route_type;
    TigArtBlitInfo art_blit_info;
    TigRect src_rect;
    TigRect dst_rect;
    TigLine line;
    TigDrawLineModeInfo line_mode_info;
    TigDrawLineStyleInfo line_style_info;
    int index;
    int x1;
    int y1;
    int x2;
    int y2;

    route_type = wmap_ui_mode == WMAP_UI_MODE_TOWN
        ? WMAP_ROUTE_TYPE_TOWN
        : WMAP_ROUTE_TYPE_WORLD;
    if (wmap_ui_routes[route_type].length <= 0) {
        return;
    }

    line_mode_info.mode = TIG_DRAW_LINE_MODE_NORMAL;
    line_mode_info.thickness = 1;
    tig_draw_set_line_mode(&line_mode_info);

    line_style_info.style = TIG_LINE_STYLE_DASHED;
    tig_draw_set_line_style(&line_style_info);

    x1 = viewport_rect->x + map_info->player_coordinates.x - map_info->scroll_x;
    y1 = viewport_rect->y + map_info->player_coordinates.y - map_info->scroll_y;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = wmap_note_type_info[WMAP_NOTE_TYPE_NOTE].width;
    src_rect.height = wmap_note_type_info[WMAP_NOTE_TYPE_NOTE].height;

    art_blit_info.flags = 0;
    art_blit_info.art_id = wmap_note_type_info[WMAP_NOTE_TYPE_NOTE].art_id;
    art_blit_info.src_rect = &src_rect;
    art_blit_info.dst_video_buffer = wmap_ui_coordinate_text_buffer;

    for (index = 0; index < wmap_ui_routes[route_type].length; index++) {
        x2 = viewport_rect->x + wmap_ui_routes[route_type].waypoints[index].coords.x - map_info->scroll_x;
        y2 = viewport_rect->y + wmap_ui_routes[route_type].waypoints[index].coords.y - map_info->scroll_y;

        line.x1 = x1;
        line.y1 = y1;
        line.x2 = x2;
        line.y2 = y2;

        if (tig_line_intersection(clip_rect, &line) == TIG_OK) {
            tig_window_line(wmap_ui_window,
                &line,
                index == wmap_ui_selected_waypoint ? wmap_ui_selected_waypoint_color : wmap_ui_waypoint_color);
        }

        src_rect.x = x2 - wmap_note_type_info[WMAP_NOTE_TYPE_WAYPOINT].width / 2;
        src_rect.y = y2 - wmap_note_type_info[WMAP_NOTE_TYPE_WAYPOINT].height / 2;
        src_rect.width = wmap_note_type_info[WMAP_NOTE_TYPE_WAYPOINT].width;
        src_rect.height = wmap_note_type_info[WMAP_NOTE_TYPE_WAYPOINT].height;
        art_blit_info.art_id = wmap_note_type_info[WMAP_NOTE_TYPE_WAYPOINT].art_id;

        if (tig_rect_intersection(&src_rect, clip_rect, &src_rect) == TIG_OK) {
            dst_rect = src_rect;

            src_rect.x -= x2 - wmap_note_type_info[WMAP_NOTE_TYPE_WAYPOINT].width / 2;
            src_rect.y -= y2 - wmap_note_type_info[WMAP_NOTE_TYPE_WAYPOINT].height / 2;

            art_blit_info.dst_rect = &dst_rect;
            tig_window_blit_art(wmap_ui_window, &art_blit_info);
        }

        x1 = x2;
        y1 = y2;
    }
}

// 0x566CC0
void wmap_ui_get_current_location(int64_t* loc_ptr)
{
    if (wmap_ui_created && wmap_ui_state == WMAP_UI_STATE_MOVING) {
        wmap_ui_coords_to_location(&wmap_ui_mode_info[wmap_ui_mode].player_coordinates, loc_ptr);
    } else {
        *loc_ptr = obj_field_int64_get(player_get_local_pc_obj(), OBJ_F_LOCATION);
    }
}

// 0x566D10
void wmap_ui_note_icon_draw(int type, WmapCoords* coords, TigRect* viewport_rect, TigRect* clip_rect, WmapInfo* wmap_info, bool completed)
{
    int dx;
    int dy;
    TigArtBlitInfo art_blit_info;
    TigRect src_rect;
    TigRect dst_rect;

    dx = coords->x - wmap_note_type_info[type].width / 2 - wmap_info->scroll_x;
    dy = coords->y - wmap_note_type_info[type].height / 2 - wmap_info->scroll_y;

    src_rect.x = viewport_rect->x + dx;
    src_rect.y = viewport_rect->y + dy;
    src_rect.width = wmap_note_type_info[type].width;
    src_rect.height = wmap_note_type_info[type].height;
    if (tig_rect_intersection(&src_rect, clip_rect, &src_rect) == TIG_OK) {
        dst_rect = src_rect;

        src_rect.x -= viewport_rect->x + dx;
        src_rect.y -= viewport_rect->y + dy;

        art_blit_info.flags = completed ? TIG_ART_BLT_BLEND_COLOR_CONST : 0;
        art_blit_info.color = tig_color_make(128, 128, 128);
        art_blit_info.art_id = wmap_note_type_info[type].art_id;
        art_blit_info.dst_video_buffer = wmap_ui_coordinate_text_buffer;
        art_blit_info.src_rect = &src_rect;
        art_blit_info.dst_rect = &dst_rect;
        tig_window_blit_art(wmap_ui_window, &art_blit_info);
    }
}

void wmap_ui_scroll_by(int dx, int dy)
{
    WmapInfo* wmap_info;
    int offset_x;
    int offset_y;
    TigRect rect;

    wmap_info = &(wmap_ui_mode_info[wmap_ui_mode]);

    offset_x = wmap_info->scroll_x;
    offset_y = wmap_info->scroll_y;

    wmap_info->scroll_x += dx;
    wmap_info->scroll_y += dy;

    if (wmap_info->scroll_x < 0) {
        wmap_info->scroll_x = 0;
    } else if (wmap_info->scroll_x > wmap_info->scroll_x_max) {
        wmap_info->scroll_x = wmap_info->scroll_x_max;
    }

    if (wmap_info->scroll_y < 0) {
        wmap_info->scroll_y = 0;
    } else if (wmap_info->scroll_y > wmap_info->scroll_y_max) {
        wmap_info->scroll_y = wmap_info->scroll_y_max;
    }

    dx = wmap_info->scroll_x - offset_x;
    dy = wmap_info->scroll_y - offset_y;
    if (dx == 0 && dy == 0) {
        return;
    }

    if (wmap_info->refresh_rect == NULL) {
        return;
    }

    if (wmap_ui_mode != WMAP_UI_MODE_TOWN) {
        wmap_info->refresh_rect(&(wmap_info->rect));
        return;
    }

    tig_window_scroll_rect(wmap_ui_window,
        &(wmap_info->rect),
        -dx,
        -dy);

    if (dx > 0) {
        rect.x = wmap_ui_nav_cvr_frame.x - dx;
        rect.y = wmap_ui_nav_cvr_frame.y;
        rect.width = wmap_ui_nav_cvr_frame.width + dx;
        rect.height = wmap_ui_nav_cvr_frame.height;
        if (dy > 0) {
            rect.y -= dy;
            rect.height += dy;
        }
        wmap_info->refresh_rect(&rect);

        rect = wmap_info->rect;
        rect.x += rect.width - dx;
        rect.width = dx;
        if (dy > 0) {
            rect.y -= dy;
            rect.height += dy;
        }
        wmap_info->refresh_rect(&rect);
    } else if (dx < 0) {
        rect.x = wmap_ui_nav_cvr_frame.x;
        rect.y = wmap_ui_nav_cvr_frame.y;
        rect.height = wmap_ui_nav_cvr_frame.height;
        rect.width = wmap_ui_nav_cvr_frame.width - dx;
        if (dy > 0) {
            rect.y -= dy;
            rect.height += dy;
        }
        wmap_info->refresh_rect(&rect);

        rect = wmap_info->rect;
        rect.width = -dx;
        if (dy > 0) {
            rect.y -= dy;
            rect.height += dy;
        }
        wmap_info->refresh_rect(&rect);
    }

    if (dy > 0) {
        rect.x = wmap_ui_nav_cvr_frame.x;
        rect.y = wmap_ui_nav_cvr_frame.y - dy;
        rect.width = wmap_ui_nav_cvr_frame.width;
        rect.height = wmap_ui_nav_cvr_frame.height + dy;
        wmap_info->refresh_rect(&rect);

        rect = wmap_info->rect;
        rect.y += rect.height - dy;
        rect.height = dy;
        wmap_info->refresh_rect(&rect);
    } else if (dy < 0) {
        rect = wmap_ui_nav_cvr_frame;
        wmap_info->refresh_rect(&rect);

        rect = wmap_info->rect;
        rect.height = -dy;
        wmap_info->refresh_rect(&rect);
    }
}

void wmap_ui_scroll_from_mouse_wheel(int dx, int dy)
{
    wmap_ui_scroll_by(dx * wmap_ui_scroll_speed_x, -dy * wmap_ui_scroll_speed_y);
}

static const char* wmap_marker_label(int id)
{
    return mes_localized_get("map_markers", id);
}

static void wmap_marker_close(void)
{
    if (wmap_marker_text_focused) textedit_ui_unfocus(&wmap_marker_edit);
    wmap_marker_text_focused = false;
    ui_search_bar_close(&wmap_marker_window);
}

void wmap_ui_markers_visible(bool visible)
{
    if (!visible) {
        wmap_marker_close();
        wmap_marker_swallow_event = -1;
        wmap_marker_swallow_enter = false;
    }
}

static void wmap_marker_draw(void)
{
    if (wmap_marker_window == TIG_WINDOW_HANDLE_INVALID) return;
    char text[MAP_MARKER_TEXT_SIZE + 2];
    int cursor = SDL_clamp(textedit_ui_pos_get(), 0, (int)strlen(wmap_marker_description));
    memcpy(text, wmap_marker_description, cursor);
    text[cursor] = '_';
    SDL_strlcpy(text + cursor + 1, wmap_marker_description + cursor, sizeof(text) - cursor - 1);
    // Keep the caret visible even when the description exceeds the field width.
    int first = 0;
    char prefix[MAP_MARKER_TEXT_SIZE + 2];
    tig_font_push(logbook_ui_toolbar_font());
    while (first < cursor) {
        memcpy(prefix, text + first, cursor - first + 1);
        prefix[cursor - first + 1] = '\0';
        TigFont measure = { 0 };
        measure.str = prefix;
        tig_font_measure(&measure);
        if (measure.width <= 390) break;
        first++;
    }
    tig_font_pop();
    ui_search_bar_cell(wmap_marker_window, (TigRect){ 0, 0, 400, 26 }, logbook_ui_toolbar_font(), text + first, false);
    ui_search_bar_cell(wmap_marker_window, (TigRect){ 408, 0, 65, 26 }, logbook_ui_toolbar_font(), wmap_marker_label(28), false);
}

static void wmap_marker_text_changed(TextEdit* edit)
{
    (void)edit;
    wmap_marker_draw();
}

static void wmap_marker_text_enter(TextEdit* edit)
{
    (void)edit;
    wmap_marker_swallow_enter = true;
    wmap_marker_commit();
}

static void wmap_marker_open(WmapNote* note, WmapCoords coords, int mouse_x, int mouse_y)
{
    wmap_marker_close();
    TigWindowData parent;
    if (tig_window_data(wmap_ui_window, &parent) != TIG_OK) return;
    WmapInfo* info = &wmap_ui_mode_info[wmap_ui_mode];
    TigRect rect = { mouse_x - parent.rect.x, mouse_y - parent.rect.y + 8, 473, 26 };
    rect.x = SDL_clamp(rect.x, info->rect.x, info->rect.x + info->rect.width - rect.width);
    rect.y = SDL_clamp(rect.y, info->rect.y, info->rect.y + info->rect.height - rect.height);
    if (!ui_search_bar_open(&wmap_marker_window, wmap_ui_window, rect, wmap_ui_message_filter)) return;
    wmap_marker_mode = wmap_ui_mode;
    wmap_marker_draft = note != NULL ? *note : (WmapNote){ 0 };
    if (note == NULL) {
        wmap_marker_draft.id = -1;
        wmap_marker_draft.coords = coords;
        wmap_marker_draft.type = WMAP_NOTE_TYPE_NOTE;
    }
    wmap_marker_draft.flags = (wmap_marker_draft.flags & ~0x4) | MAP_MARKER_PERSONAL;
    wmap_marker_draft.video_buffer = NULL;
    SDL_strlcpy(wmap_marker_description, wmap_marker_draft.str, sizeof(wmap_marker_description));
    for (char* ch = wmap_marker_description; *ch != '\0'; ch++) {
        if (*ch == '\n' || *ch == '\r') *ch = ' ';
    }
    textedit_ui_focus(&wmap_marker_edit);
    wmap_marker_text_focused = true;
    wmap_marker_draw();
}

static void wmap_marker_refresh(void)
{
    if (wmap_ui_mode == WMAP_UI_MODE_TOWN) wmap_ui_town_notes_save();
    wmap_ui_mode_info[wmap_ui_mode].hovered_note_id = -1;
    wmap_ui_mode_info[wmap_ui_mode].refresh();
    intgame_message_window_clear();
}

static void wmap_marker_commit(void)
{
    if (wmap_ui_mode != wmap_marker_mode) { wmap_marker_close(); return; }
    if (!map_marker_description_valid(wmap_marker_description)) {
        ui_display_warning((char*)wmap_marker_label(17));
        return;
    }
    SDL_strlcpy(wmap_marker_draft.str, wmap_marker_description, sizeof(wmap_marker_draft.str));
    if (wmap_marker_draft.id == -1) {
        if (!wmap_note_add(&wmap_marker_draft)) {
            ui_display_warning((char*)wmap_marker_label(18));
            return;
        }
    } else {
        WmapNote* note = wmap_ui_note_find_by_id(wmap_marker_draft.id);
        if (note == NULL || (note->flags & 0x2) != 0) { wmap_marker_close(); return; }
        wmap_ui_note_buffer_free(note);
        *note = wmap_marker_draft;
        wmap_ui_note_bounds_update(note);
    }
    wmap_marker_close();
    wmap_marker_refresh();
}

static bool wmap_marker_overlay_input(TigMessage* msg)
{
    if (msg->type == TIG_MESSAGE_KEYBOARD) {
        if (msg->data.keyboard.scancode == SDL_SCANCODE_ESCAPE) {
            if (!msg->data.keyboard.pressed) wmap_marker_close();
        } else {
            textedit_ui_process_message(msg);
            wmap_marker_draw();
        }
        return true;
    }
    if (msg->type == TIG_MESSAGE_TEXT_INPUT) {
        char input[256];
        language_input_to_legacy(msg->data.text.text, input, sizeof(input), gamelib_current_language_get());
        TigMessage converted = *msg;
        converted.data.text.text = input;
        textedit_ui_process_message(&converted);
        return true;
    }
    if (msg->type != TIG_MESSAGE_MOUSE) return msg->type == TIG_MESSAGE_BUTTON;
    if (msg->data.mouse.event != TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP) return true;
    TigWindowData data;
    if (tig_window_data(wmap_marker_window, &data) != TIG_OK) return true;
    int x = msg->data.mouse.x - data.rect.x, y = msg->data.mouse.y - data.rect.y;
    if (ui_search_bar_contains(&(TigRect){ 408, 0, 65, 26 }, x, y)) wmap_marker_commit();
    else if (!ui_search_bar_contains(&data.rect, msg->data.mouse.x, msg->data.mouse.y)) wmap_marker_close();
    return true;
}

static bool wmap_marker_input(TigMessage* msg)
{
    if (!settings_get_value(&settings, WORLDMAP_TOOLBAR_KEY)) {
        wmap_ui_markers_visible(false);
        return false;
    }
    if (map_marker_release_gesture(msg, wmap_marker_swallow_event)) {
        wmap_marker_swallow_event = -1;
        return true;
    }
    if (wmap_marker_swallow_enter && msg->type == TIG_MESSAGE_KEYBOARD
        && (msg->data.keyboard.scancode == SDL_SCANCODE_RETURN || msg->data.keyboard.scancode == SDL_SCANCODE_KP_ENTER)) {
        if (!msg->data.keyboard.pressed) wmap_marker_swallow_enter = false;
        return true;
    }
    bool deleting = map_marker_delete_gesture(msg, (SDL_GetModState() & SDL_KMOD_ALT) != 0);
    bool editing = map_marker_edit_gesture(msg, (SDL_GetModState() & SDL_KMOD_CTRL) != 0);
    if ((deleting || editing) && !wmap_ui_selecting && wmap_ui_state == WMAP_UI_STATE_NORMAL
        && wmap_ui_mode != WMAP_UI_MODE_CONTINENT) {
        TigRect centered = { 0, 0, 800, 600 };
        hrp_apply(&centered, GRAVITY_CENTER_HORIZONTAL | GRAVITY_CENTER_VERTICAL);
        int x = msg->data.mouse.x - centered.x, y = msg->data.mouse.y - centered.y;
        WmapInfo* info = &wmap_ui_mode_info[wmap_ui_mode];
        if (ui_search_bar_contains(&info->interactive_rect, x, y)) {
            WmapCoords coords;
            info->screen_to_map_coordinates(x, y, &coords);
            wmap_marker_swallow_event = TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP;
            WmapNote* note = NULL;
            for (int i = *info->num_notes - 1; i >= 0; i--) {
                WmapNote* candidate = &info->notes[i];
                if ((candidate->flags & 0x2) == 0
                    && llabs((long long)coords.x - candidate->coords.x) <= wmap_note_type_icon_max_width / 2
                    && llabs((long long)coords.y - candidate->coords.y) <= wmap_note_type_icon_max_height / 2) {
                    note = candidate;
                    break;
                }
            }
            if (deleting) {
                wmap_marker_close();
                if (note != NULL) { wmap_note_remove(note); wmap_marker_refresh(); }
            } else wmap_marker_open(note, coords, msg->data.mouse.x, msg->data.mouse.y);
            return true;
        }
    }
    if (wmap_marker_window != TIG_WINDOW_HANDLE_INVALID) return wmap_marker_overlay_input(msg);
    return false;
}
