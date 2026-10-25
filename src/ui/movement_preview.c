#include "ui/movement_preview.h"

#include <math.h>
#include <string.h>

#include "game/anim.h"
#include "game/combat.h"
#include "game/gamelib.h"
#include "game/iso_zoom.h"
#include "game/location.h"
#include "game/modern_font.h"
#include "game/obj.h"
#include "game/ui.h"

#define MOVEMENT_PREVIEW_STEPS 200

typedef struct MovementPreviewPoint {
    int x;
    int y;
    tig_color_t color;
    bool marker;
} MovementPreviewPoint;

static struct {
    TigVideoBuffer* buffer;
    tig_window_handle_t window;
    TigRect viewport;
    TigRect bounds;
    int64_t character;
    int64_t from;
    int64_t destination;
    int64_t locations[MOVEMENT_PREVIEW_STEPS + 1];
    MovementPreviewPoint points[MOVEMENT_PREVIEW_STEPS + 1];
    int8_t rotations[MOVEMENT_PREVIEW_STEPS];
    int count;
    int opacity;
    tig_timestamp_t last_search;
    bool queried;
    bool reachable;
    bool combat;
    bool visible;
} preview;

void movement_preview_clear(void)
{
    if (preview.visible) gamelib_invalidate_rect(&preview.bounds);
    preview.visible = false;
}

void movement_preview_destroy(void)
{
    movement_preview_clear();
    if (preview.buffer != NULL) tig_video_buffer_destroy(preview.buffer);
    memset(&preview, 0, sizeof(preview));
}

static MovementPreviewPoint movement_preview_project(int64_t location, int offset_x, int offset_y, float zoom)
{
    int64_t x, y;
    location_xy(location, &x, &y);
    int center_x = preview.viewport.width / 2;
    int center_y = preview.viewport.height / 2;
    MovementPreviewPoint point = { 0 };
    point.x = center_x + (int)roundf((float)(x + 40 + offset_x - center_x) * zoom);
    point.y = center_y + (int)roundf((float)(y + 20 + offset_y - center_y) * zoom);
    return point;
}

static void movement_preview_line(TigLine line, tig_color_t color)
{
    SDL_Rect clip = { 0, 0, preview.viewport.width, preview.viewport.height };
    if (SDL_GetRectAndLineIntersection(&clip, &line.x1, &line.y1, &line.x2, &line.y2)) {
        tig_video_buffer_line(preview.buffer, &line, &preview.viewport, color);
    }
}

static void movement_preview_mark(MovementPreviewPoint point, int radius, tig_color_t key)
{
    TigLine sides[4] = {
        { point.x, point.y - radius, point.x + radius + 1, point.y },
        { point.x + radius + 1, point.y, point.x, point.y + radius },
        { point.x, point.y + radius, point.x - radius - 1, point.y },
        { point.x - radius - 1, point.y, point.x, point.y - radius },
    };
    tig_color_t color = point.color != key ? point.color : tig_color_make(0, 0, 0);
    for (int side = 0; side < 4; side++) movement_preview_line(sides[side], color);
}

void movement_preview_update(tig_window_handle_t window, int64_t character, int64_t destination)
{
    TigWindowData window_data;
    if (tig_window_data(window, &window_data) != TIG_OK) {
        movement_preview_clear();
        return;
    }
    TigRect viewport = { 0, 0, window_data.rect.width, window_data.rect.height };
    if (preview.buffer != NULL && (preview.window != window
        || preview.viewport.width != viewport.width || preview.viewport.height != viewport.height)) {
        movement_preview_destroy();
    }
    preview.window = window;
    preview.viewport = viewport;
    int64_t from = obj_field_int64_get(character, OBJ_F_LOCATION);
    bool combat = combat_critter_is_combat_mode_active(character);
    tig_timestamp_t now;
    tig_timer_now(&now);
    bool changed = !preview.queried || preview.character != character || preview.from != from
        || preview.destination != destination || preview.combat != combat;
    tig_duration_t elapsed = tig_timer_between(preview.last_search, now);
    if (changed && preview.queried && preview.reachable && preview.character == character
        && preview.destination == destination && preview.combat == combat) {
        for (int step = 1; step <= preview.count; step++) {
            if (preview.locations[step] != from) continue;
            preview.count -= step;
            memmove(preview.rotations, preview.rotations + step, (size_t)preview.count);
            memmove(preview.locations, preview.locations + step, (size_t)(preview.count + 1) * sizeof(preview.locations[0]));
            preview.from = from;
            changed = false;
            break;
        }
    }
    if (changed && preview.queried && elapsed < 200) {
        if (preview.visible) gamelib_invalidate_rect(&preview.bounds);
        preview.visible = false;
        return;
    }
    if (changed || elapsed >= 1000) {
        preview.character = character;
        preview.from = from;
        preview.destination = destination;
        preview.combat = combat;
        preview.last_search = now;
        preview.queried = true;
        preview.count = anim_path_preview(character, destination, preview.rotations, MOVEMENT_PREVIEW_STEPS);
        if (preview.count < 0 || preview.count > MOVEMENT_PREVIEW_STEPS) preview.count = 0;
        preview.locations[0] = from;
        for (int step = 0; step < preview.count; step++) {
            if (!location_in_dir(preview.locations[step], preview.rotations[step], &preview.locations[step + 1])) {
                preview.count = 0;
                break;
            }
        }
        preview.reachable = preview.count > 0 && preview.locations[preview.count] == destination;
        if (!preview.reachable) {
            preview.count = 0;
            preview.locations[0] = destination;
        }
    }

    int opacity = modern_font_int_for(MOVEMENT_PREVIEW_OPACITY_KEY, 70, 0, 100);
    if (opacity == 0) {
        movement_preview_clear();
        return;
    }
    tig_color_t color = modern_font_color_for(MOVEMENT_PREVIEW_COLOR_KEY, 144, 216, 160);
    tig_color_t blocked = modern_font_color_for(MOVEMENT_PREVIEW_BLOCKED_COLOR_KEY, 240, 128, 128);
    tig_color_t beyond = modern_font_color_for(MOVEMENT_PREVIEW_BEYOND_AP_COLOR_KEY, 216, 176, 96);
    int available_steps = MOVEMENT_PREVIEW_STEPS;
    if (combat_turn_based_is_active()) {
        int cost = get_always_run(character) ? 1 : 2;
        available_steps = combat_action_points_get() / cost;
    }
    float zoom = iso_zoom_current();
    MovementPreviewPoint points[MOVEMENT_PREVIEW_STEPS + 1] = { 0 };
    TigRect bounds = { 0 };
    int right = 0, bottom = 0;
    for (int step = 0; step <= preview.count; step++) {
        int offset_x = 0, offset_y = 0;
        if (step == 0 && preview.reachable) {
            offset_x = obj_field_int32_get(character, OBJ_F_OFFSET_X);
            offset_y = obj_field_int32_get(character, OBJ_F_OFFSET_Y);
        }
        points[step] = movement_preview_project(preview.locations[step], offset_x, offset_y, zoom);
        points[step].color = !preview.reachable ? blocked : step > available_steps ? beyond : color;
        points[step].marker = step == preview.count || (step > 0 && preview.rotations[step - 1] != preview.rotations[step]);
        int x = points[step].x, y = points[step].y;
        if (step == 0) { bounds.x = x - 7; bounds.y = y - 7; right = x + 8; bottom = y + 8; }
        if (x - 7 < bounds.x) bounds.x = x - 7;
        if (y - 7 < bounds.y) bounds.y = y - 7;
        if (x + 8 > right) right = x + 8;
        if (y + 8 > bottom) bottom = y + 8;
    }
    bounds.width = right - bounds.x;
    bounds.height = bottom - bounds.y;
    if (tig_rect_intersection(&bounds, &viewport, &bounds) != TIG_OK) {
        movement_preview_clear();
        return;
    }
    if (preview.visible && preview.opacity == opacity
        && memcmp(preview.points, points, sizeof(points)) == 0) return;
    if (preview.visible) gamelib_invalidate_rect(&preview.bounds);
    tig_color_t key = tig_color_make(1, 2, 3);
    if (preview.buffer == NULL) {
        TigVideoBufferCreateInfo info = { 0 };
        info.flags = TIG_VIDEO_BUFFER_CREATE_SYSTEM_MEMORY | TIG_VIDEO_BUFFER_CREATE_COLOR_KEY;
        info.width = viewport.width;
        info.height = viewport.height;
        info.background_color = key;
        info.color_key = key;
        if (tig_video_buffer_create(&info, &preview.buffer) != TIG_OK) {
            preview.visible = false;
            return;
        }
    }
    if (preview.bounds.width > 0 && preview.bounds.height > 0) tig_video_buffer_fill(preview.buffer, &preview.bounds, key);
    tig_video_buffer_fill(preview.buffer, &bounds, key);
    for (int step = 1; step <= preview.count; step++) {
        TigLine line = { points[step - 1].x, points[step - 1].y, points[step].x, points[step].y };
        tig_color_t line_color = points[step].color != key ? points[step].color : tig_color_make(0, 0, 0);
        movement_preview_line(line, line_color);
    }
    for (int step = 0; step <= preview.count; step++) {
        if (points[step].marker) movement_preview_mark(points[step], step == preview.count ? 5 : 2, key);
    }
    memcpy(preview.points, points, sizeof(points));
    preview.opacity = opacity;
    preview.bounds = bounds;
    preview.visible = true;
    gamelib_invalidate_rect(&preview.bounds);
}

void movement_preview_draw(GameDrawInfo* draw_info)
{
    if (!preview.visible || preview.buffer == NULL || draw_info == NULL || draw_info->rects == NULL) return;
    TigVideoBuffer* destination;
    if (tig_window_vbid_get(preview.window, &destination) != TIG_OK
        || tig_video_buffer_lock(preview.buffer) != TIG_OK) return;
    if (tig_video_buffer_lock(destination) != TIG_OK) {
        tig_video_buffer_unlock(preview.buffer);
        return;
    }
    TigVideoBufferData source_data, destination_data;
    tig_video_buffer_data(preview.buffer, &source_data);
    tig_video_buffer_data(destination, &destination_data);
    if (source_data.bpp != 32 || destination_data.bpp != 32) {
        tig_video_buffer_unlock(destination);
        tig_video_buffer_unlock(preview.buffer);
        return;
    }
    TigRect destination_bounds = { 0, 0, destination_data.width, destination_data.height };
    int alpha = preview.opacity * 255 / 100;
    for (TigRectListNode* node = *draw_info->rects; node != NULL; node = node->next) {
        TigRect area;
        if (tig_rect_intersection(&node->rect, &preview.bounds, &area) != TIG_OK
            || tig_rect_intersection(&area, &destination_bounds, &area) != TIG_OK) continue;
        for (int y = area.y; y < area.y + area.height; y++) {
            const uint32_t* source = (const uint32_t*)((const uint8_t*)source_data.pixels
                + (size_t)y * source_data.pitch) + area.x;
            uint32_t* target = (uint32_t*)((uint8_t*)destination_data.pixels
                + (size_t)y * destination_data.pitch) + area.x;
            for (int x = 0; x < area.width; x++) {
                if (source[x] != (uint32_t)source_data.color_key) {
                    target[x] = alpha == 255 ? source[x] : tig_color_blend_alpha(source[x], target[x], alpha);
                }
            }
        }
    }
    tig_video_buffer_unlock(destination);
    tig_video_buffer_unlock(preview.buffer);
}
