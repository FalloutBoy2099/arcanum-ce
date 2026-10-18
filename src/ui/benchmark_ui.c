#include "ui/benchmark_ui.h"
#include "ui/benchmark_input.h"
#include "ui/benchmark_widget.h"

#include <stdio.h>
#include <tig/performance.h>

#include "game/anim.h"
#include "game/benchmark.h"
#include "game/benchmark_data.h"
#include "game/camera_follow.h"
#include "game/combat.h"
#include "game/conversation_history.h"
#include "game/critter.h"
#include "game/gamelib.h"
#include "game/iso_zoom.h"
#include "game/location.h"
#include "game/map.h"
#include "game/mes.h"
#include "game/modern_font.h"
#include "game/object.h"
#include "game/obj_pool.h"
#include "game/path.h"
#include "game/player.h"
#include "game/random.h"
#include "ui/intgame.h"
#include "ui/logbook_ui.h"
#include "ui/mainmenu_ui.h"
#include "ui/schematic_ui.h"

static const char* saved_keys[] = {
    HUD_STAGE_KEY, TRANSLUCENT_BLACK_UI_KEY, ISO_ZOOM_ENABLED_KEY, ISO_ZOOM_MIN_KEY,
    ISO_ZOOM_MAX_KEY, SCROLL_USING_BORDERS_KEY, CAMERA_FOLLOWS_PLAYER_KEY,
    AUTOSAVE_TRAVEL_KEY, AUTOSAVE_SLEEP_KEY,
};
static char* saved_values[SDL_arraysize(saved_keys)];
static SettingsFlags saved_flags;
static unsigned int saved_random;
static float saved_zoom;
static int64_t saved_origin_x, saved_origin_y;
static BenchmarkRun run;
static bool attempted;
static bool waiting_for_route;
static tig_timestamp_t route_attempt_time;
static bool active;
static bool abort_requested;
static bool previous_logging;
static int64_t pc;
static int map;
static int64_t route[4];
static FILE* trace;
static Uint64 session;
static unsigned int first_interval;
static tig_window_handle_t world_window = TIG_WINDOW_HANDLE_INVALID;
static tig_font_handle_t widget_font = TIG_FONT_HANDLE_INVALID;
static char widget_font_path[TIG_MAX_PATH];
static int notice_id;

static void show_notice(const char* status)
{
    notice_id = benchmark_notice_id(status);
    if (notice_id == 0) return;
    UiMessage message = { 0 };
    message.type = UI_MSG_TYPE_FEEDBACK;
    message.str = (char*)mes_localized_get("benchmark_ui", notice_id);
    intgame_message_window_display_msg(&message);
}

static void update_widget(void)
{
    TigWindowData world;
    bool visible = tig_get_active() && !active && !mainmenu_ui_is_active() && intgame_mode_get() == INTGAME_MODE_MAIN
        && gamelib_loaded_save_name()[0] != '\0' && player_get_local_pc_obj() != OBJ_HANDLE_NULL
        && obj_handle_is_valid(player_get_local_pc_obj()) && tig_window_data(world_window, &world) == TIG_OK;
    if (visible && widget_font == TIG_FONT_HANDLE_INVALID) {
        TigFont font = { 0 };
        font.flags = TIG_FONT_SHADOW | TIG_FONT_CENTERED;
        font.color = tig_color_make(245, 235, 205);
        font.modern_shadow_opacity = 220;
        font.modern_shadow_distance = 1;
        tig_art_misc_id_create(TIG_ART_SYSTEM_FONT, 0, &font.art_id);
        if (modern_font_enabled() && modern_font_path() != NULL) {
            SDL_strlcpy(widget_font_path, modern_font_path(), sizeof(widget_font_path));
            font.flags |= TIG_FONT_MODERN;
            font.modern_font_path = widget_font_path;
            font.modern_font_size = 14;
        }
        tig_font_create(&font, &widget_font);
    }
    TigRect bounds = visible ? world.rect : (TigRect){ 0 };
    benchmark_widget_update(bounds, visible, widget_font, mes_localized_get("benchmark_ui", 0),
        notice_id != 0 ? mes_localized_get("benchmark_ui", notice_id) : "");
}

static void write_status(const char* status)
{
    FILE* file = fopen("benchmark-status.txt", "w");
    if (file != NULL) { fprintf(file, "%s\n", status); fclose(file); }
}

static void write_file_fingerprint(FILE* metadata, const char* path)
{
    Uint32 crc;
    Uint64 length;
    BenchmarkFingerprintStatus status = benchmark_fingerprint(path, &crc, &length);
    if (status == BENCHMARK_FINGERPRINT_MISSING) { fprintf(metadata, "file.%s=missing\n", path); return; }
    fprintf(metadata, "file.%s=%s:%08x:%llu\n", path, status == BENCHMARK_FINGERPRINT_ERROR ? "error" : "crc32",
        (unsigned int)crc, (unsigned long long)length);
}

static void close_panels(void)
{
    logbook_ui_close();
    schematic_ui_close();
}

static void finish(const char* status)
{
    if (!active) return;
    write_status(status);
    active = false;
    tig_message_event_filter_set(NULL);
    tig_message_input_block_set(false);
    if (trace != NULL) {
        const BenchmarkStep* step = benchmark_step(run.step);
        int64_t location = obj_handle_is_valid(pc) ? obj_field_int64_get(pc, OBJ_F_LOCATION) : 0;
        fprintf(trace, "%llu,%s,%s,%u,%u,%u,%d,%lld,%lld\n", (unsigned long long)session,
            step != NULL ? step->name : "complete", status, first_interval,
            tig_performance_interval(), run.frames, map,
            (long long)location_get_x(location), (long long)location_get_y(location));
        fclose(trace);
        trace = NULL;
    }
    tig_performance_set_enabled(false, NULL);
    close_panels();
    for (int i = 0; i < SDL_arraysize(saved_keys); i++) {
        settings_set_str_value(&settings, saved_keys[i], saved_values[i]);
        FREE(saved_values[i]);
        saved_values[i] = NULL;
    }
    settings.flags = saved_flags;
    random_seed(saved_random);
    iso_zoom_set_target(saved_zoom);
    location_origin_pixel_set(saved_origin_x, saved_origin_y);
    intgame_show();
    iso_interface_refresh();
    if (previous_logging) tig_performance_set_enabled(true, NULL);
    tig_debug_printf("Automatic benchmark: %s. See benchmark-phases.csv.\n", status);
    show_notice(status);
}

static bool input_filter(const SDL_Event* event)
{
    const char* reason = benchmark_input_interrupt_reason(event->type);
    if (reason != NULL) {
        finish(reason);
        return true;
    }
    switch (event->type) {
    case SDL_EVENT_KEY_DOWN:
        if (event->key.scancode == SDL_SCANCODE_ESCAPE) abort_requested = true;
        return false;
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
        return false;
    default:
        return true;
    }
}

static bool enter_step(const BenchmarkStep* step)
{
    write_status(step->name);
    first_interval = tig_performance_interval();
    if (step->argument == 1 && (step->action == BENCHMARK_INDEX || step->action == BENCHMARK_CONVERSATIONS)) {
        close_panels();
        logbook_ui_open(pc);
    }
    switch (step->action) {
    case BENCHMARK_WARMUP:
        close_panels();
        settings_set_value(&settings, TRANSLUCENT_BLACK_UI_KEY, 0);
        settings_set_value(&settings, HUD_STAGE_KEY, 0);
        intgame_show();
        iso_zoom_reset();
        location_origin_set(obj_field_int64_get(pc, OBJ_F_LOCATION));
        break;
    case BENCHMARK_MOVE:
        return anim_goal_move_to_tile(pc, route[step->argument]);
    case BENCHMARK_HIDDEN:
        settings_set_value(&settings, HUD_STAGE_KEY, 3);
        intgame_show();
        break;
    case BENCHMARK_TRANSLUCENT:
        settings_set_value(&settings, TRANSLUCENT_BLACK_UI_KEY, 1);
        settings_set_value(&settings, HUD_STAGE_KEY, 0);
        intgame_show();
        break;
    case BENCHMARK_ZOOM:
        if (!iso_zoom_is_available()) return false;
        iso_zoom_set_target(0.75f);
        return iso_zoom_target() == 0.75f;
    case BENCHMARK_RUMORS:
        iso_zoom_reset();
        logbook_ui_open(pc);
        return logbook_ui_show_view(LOGBOOK_VIEW_RUMORS);
    case BENCHMARK_CONVERSATIONS:
        return logbook_ui_show_view(LOGBOOK_VIEW_CONVERSATIONS);
    case BENCHMARK_INDEX:
        return logbook_ui_show_view(LOGBOOK_VIEW_QUEST_INDEX);
    case BENCHMARK_SCHEMATICS:
        close_panels();
        schematic_ui_toggle(pc, pc);
        return schematic_ui_is_created();
    default:
        break;
    }
    return true;
}

static void start(bool manual)
{
    attempted = true;
    write_status("starting");
    pc = player_get_local_pc_obj();
    map = map_current_map();
    if (critter_is_dead(pc) || critter_is_unconscious(pc)
        || combat_critter_is_combat_mode_active(pc) || combat_turn_based_is_active()) {
        tig_debug_printf("Automatic benchmark: requires a conscious character outside combat.\n");
        write_status("requires_safe_save");
        show_notice("requires_safe_save");
        return;
    }
    if (!benchmark_route_find(pc, route)) {
        tig_debug_printf("Automatic benchmark: no reachable route; move to an open area. Retrying in 3 seconds.\n");
        write_status("route_unavailable");
        attempted = manual;
        waiting_for_route = !manual;
        tig_timer_now(&route_attempt_time);
        show_notice("route_unavailable");
        return;
    }
    waiting_for_route = false;
    trace = fopen("benchmark-phases.csv", "a+");
    if (trace == NULL) { write_status("output_unavailable"); show_notice("output_unavailable"); return; }
    fseek(trace, 0, SEEK_END);
    if (ftell(trace) == 0) fputs("run,phase,status,first_interval,last_interval,frames,map,x,y\n", trace);
    previous_logging = tig_performance_is_enabled();
    tig_performance_set_enabled(false, NULL);
    if (!tig_performance_set_enabled(true, "benchmark-performance.csv")) {
        fclose(trace); trace = NULL;
        if (previous_logging) tig_performance_set_enabled(true, NULL);
        write_status("output_unavailable");
        show_notice("output_unavailable");
        return;
    }
    session = tig_performance_session();
    saved_flags = settings.flags;
    saved_random = random_state();
    saved_zoom = iso_zoom_target();
    location_origin_get(&saved_origin_x, &saved_origin_y);
    for (int i = 0; i < SDL_arraysize(saved_keys); i++) {
        const char* value = settings_get_str_value(&settings, saved_keys[i]);
        saved_values[i] = STRDUP(value != NULL ? value : "");
    }
    FILE* metadata = fopen("benchmark-runs.txt", "a");
    if (metadata != NULL) {
        fprintf(metadata, "run=%llu\nscenario=%d\nsave=%s\nmap=%d\nseed=0xCE2026\nplatform=%s\nconversations=%d\n",
            (unsigned long long)session, BENCHMARK_SCENARIO_VERSION, gamelib_loaded_save_name(), map, SDL_GetPlatform(), conversation_history_count());
        for (SettingsEntry* entry = settings.entries; entry != NULL; entry = entry->next) {
            fprintf(metadata, "setting.%s=%s\n", entry->key, entry->value);
        }
        for (int i = 0; i < 4; i++) fprintf(metadata, "waypoint%d=%lld,%lld\n", i,
            (long long)location_get_x(route[i]), (long long)location_get_y(route[i]));
        write_file_fingerprint(metadata, "Save/Current/data.sav");
        write_file_fingerprint(metadata, "Save/Current/data2.sav");
        write_file_fingerprint(metadata, "Save/Current/conversations.bin");
        write_file_fingerprint(metadata, "Save/Current/journal_index.bin");
        fputc('\n', metadata);
        fclose(metadata);
    }
    active = true;
    abort_requested = false;
    benchmark_run_init(&run);
    random_seed(0xCE2026);
    settings_set_value(&settings, SCROLL_USING_BORDERS_KEY, 0);
    settings_set_value(&settings, CAMERA_FOLLOWS_PLAYER_KEY, 0);
    settings_set_value(&settings, AUTOSAVE_TRAVEL_KEY, 0);
    settings_set_value(&settings, AUTOSAVE_SLEEP_KEY, 0);
    settings_set_value(&settings, ISO_ZOOM_ENABLED_KEY, 1);
    settings_set_str_value(&settings, ISO_ZOOM_MIN_KEY, "0.5");
    settings_set_str_value(&settings, ISO_ZOOM_MAX_KEY, "1.75");
    tig_mouse_set_position(400, 200);
    tig_message_event_filter_set(input_filter);
    tig_message_input_block_set(true);
}

bool benchmark_ui_init(GameInitInfo* info)
{
    world_window = info->iso_window_handle;
    attempted = false;
    waiting_for_route = false;
    notice_id = 0;
    return true;
}

bool benchmark_ui_is_active(void) { return active; }

void benchmark_ui_exit(void)
{
    finish("interrupted_shutdown");
    benchmark_widget_close();
    if (widget_font != TIG_FONT_HANDLE_INVALID) tig_font_destroy(widget_font);
    widget_font = TIG_FONT_HANDLE_INVALID;
    notice_id = 0;
}

void benchmark_ui_resize(GameResizeInfo* info)
{
    benchmark_widget_close();
    world_window = info->window_handle;
}

void benchmark_ui_begin_frame(void)
{
    if (benchmark_widget_take_request() && !active) {
        notice_id = 0;
        waiting_for_route = false;
        if (!mainmenu_ui_is_active() && intgame_mode_get() == INTGAME_MODE_MAIN
            && gamelib_loaded_save_name()[0] != '\0' && player_get_local_pc_obj() != OBJ_HANDLE_NULL
            && obj_handle_is_valid(player_get_local_pc_obj())) start(true);
    }
    if (!active && !attempted && settings_get_value(&settings, BENCHMARK_AUTORUN_KEY)
        && (!waiting_for_route || tig_timer_elapsed(route_attempt_time) >= 3000)
        && gamelib_loaded_save_name()[0] != '\0' && !mainmenu_ui_is_active()
        && player_get_local_pc_obj() != OBJ_HANDLE_NULL && obj_handle_is_valid(player_get_local_pc_obj())) start(false);
    update_widget();
    if (!active) return;
    IntgameMode mode = intgame_mode_get();
    if (abort_requested || player_get_local_pc_obj() != pc || !obj_handle_is_valid(pc)
        || map_current_map() != map || critter_is_dead(pc) || critter_is_unconscious(pc)
        || combat_critter_is_combat_mode_active(pc) || combat_turn_based_is_active()
        || mainmenu_ui_is_active()
        || (mode != INTGAME_MODE_MAIN && mode != INTGAME_MODE_LOGBOOK && mode != INTGAME_MODE_SCHEMATIC)) {
        finish(abort_requested ? "cancelled" : "aborted"); return;
    }
    if (benchmark_run_enter(&run) && !enter_step(benchmark_step(run.step))) finish("action_failed");
}

void benchmark_ui_end_frame(void)
{
    if (!active) return;
    const BenchmarkStep* step = benchmark_step(run.step);
    if ((step->action == BENCHMARK_CONVERSATIONS || step->action == BENCHMARK_INDEX)
        && (run.frames == 60 || run.frames == 120)) {
        if (run.frames == 60) logbook_ui_turn_page_left();
        else logbook_ui_turn_page_right();
    }
    if (step->action == BENCHMARK_SCHEMATICS && (run.frames == 60 || run.frames == 120)) {
        schematic_ui_turn_page(run.frames == 60 ? 1 : -1);
    }
    bool ready = step->action != BENCHMARK_MOVE
        || obj_field_int64_get(pc, OBJ_F_LOCATION) == route[step->argument];
    unsigned int frames = run.frames + 1;
    if (benchmark_run_advance(&run, ready)) {
        unsigned int last_interval = tig_performance_interval();
        tig_performance_flush();
        fprintf(trace, "%llu,%s,ok,%u,%u,%u,%d,%lld,%lld\n", (unsigned long long)session,
            step->name, first_interval, last_interval, frames, map,
            (long long)location_get_x(obj_field_int64_get(pc, OBJ_F_LOCATION)),
            (long long)location_get_y(obj_field_int64_get(pc, OBJ_F_LOCATION)));
        fflush(trace);
        if (run.finished) finish("finished");
    } else if (run.failed) finish("timeout");
}
