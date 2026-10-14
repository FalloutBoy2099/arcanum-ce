#include "game/tc.h"

#include <stdio.h>

#include "game/dialog.h"
#include "game/gamelib.h"
#include "game/highres_config.h"
#include "game/modern_font.h"

/**
 * The number of lines in the text conversation.
 */
#define TEXT_CONVERSATION_LINES DIALOG_MAX_OPTIONS

/**
 * Unscaled height of the conversation option box (in pixels).
 */
#define TEXT_CONVERSATION_BOX_HEIGHT 200

/**
 * Distance from the bottom of the iso view to the option box in non-compact
 * mode (in pixels).
 */
#define TEXT_CONVERSATION_BOTTOM_OFFSET 159

/**
 * Horizontal space before and after the option text (in pixels).
 */
#define TEXT_CONVERSATION_HOR_PADDING 6

/**
 * Vertical space between between lines (in pixels).
 */
#define TEXT_CONVERSATION_VERT_SPACING 3

static void tc_render_internal(TigRectListNode* node);
static int tc_dialog_font_size(void);
static float tc_dialog_font_scale(void);
static int tc_dialog_font_art_num(void);
static float tc_dialog_font_art_scale(void);
static int tc_dialog_options_min(void);
static int tc_dialog_margin_horizontal(void);
static int tc_dialog_margin_vertical(void);
static int tc_dialog_background_opacity(void);
static int tc_content_height_for_lines(int lines);
static void tc_update_content_rect_height(void);
static void tc_render_option(int index, bool highlighted);
static int tc_option_index_from_y(int y);

/**
 * Used to track index of the currently highlighted option.
 *
 * 0x5B7218
 */
static int tc_highlighted_idx = -1;

/**
 * The content area of the text conversation (in window coordinate system).
 *
 * 0x5FF4F8
 */
static TigRect tc_content_rect;

/**
 * The bounds of intermediate video buffers (in it's own coordinate system).
 *
 * 0x5FF508
 */
static TigRect tc_intermediate_video_buffer_rect;

/**
 * Video buffer used as a scratch space for rendering text options.
 *
 * This video buffer is transparent (color keyed).
 *
 * 0x5FF518
 */
static TigVideoBuffer* tc_scratch_video_buffer;

/**
 * Parent window bounds.
 *
 * 0x5FF520
 */
static TigRect tc_iso_window_rect;

/**
 * Font handle for rendering highlighted text option.
 *
 * 0x5FF530
 */
static tig_font_handle_t tc_yellow_font;

/**
 * Function pointer to invalidate a rectangle in the parent window.
 *
 * 0x5FF534
 */
static IsoInvalidateRectFunc* tc_iso_window_invalidate_rect;

/**
 * Font handle for rendering normal text option (i.e. not highlighted).
 *
 * 0x5FF538
 */
static tig_font_handle_t tc_white_font;

/**
 * Temporary video buffer used for rendering a portion of game window, covered
 * by text conversation.
 *
 * 0x5FF53C
 */
static TigVideoBuffer* tc_backdrop_video_buffer;

/**
 * Height of the line in the text conversation.
 *
 * 0x5FF540
 */
static int tc_line_height;

/**
 * Scale factor applied to conversation text and its geometry when the
 * `DialogScale` config option is greater than 1.0.
 */
static float tc_scale = 1.0f;

/**
 * Flag indicating some internal state which is never set. Its exact meaning is
 * unclear.
 *
 * 0x5FF544
 */
static bool dword_5FF544;

/**
 * Flag indicating whether the text conversation is in editor mode.
 *
 * 0x5FF548
 */
static bool tc_editor;

/**
 * Font handle for rendering text conversation line in red.
 *
 * The exact purpose is unclear. The flag which controls the use of this font
 * (`dword_5FF544`) is never set.
 *
 * 0x5FF54C
 */
static tig_font_handle_t tc_red_font;

/**
 * Handle to the parent window.
 *
 * 0x5FF550
 */
static tig_window_handle_t tc_iso_window_handle;

/**
 * Array of strings representing text conversation options.
 *
 * 0x5FF554
 */
static const char* tc_options[TEXT_CONVERSATION_LINES];

/**
 * Flag indicating whether the text conversation uses compact interface
 * positioning.
 */
static bool tc_compact;

/**
 * Flag indicating whether the text conversation is currently active and
 * visible.
 *
 * 0x5FF568
 */
static bool tc_active;
// CE: vertical offset (design-coord pixels) applied to the dialog
// options backdrop so it sits lower when the HUD bar is cropped.
// Driven by intgame via tc_set_bottom_gap_offset().
static int tc_bottom_gap_offset = 0;
// Remember the last compact value seen by tc_clear so we can re-lay
// out at any time (e.g. on a gap-offset change) without needing the
// caller to re-pass it.

/**
 * Called when the game is initialized.
 *
 * 0x4C9280
 */
bool tc_init(GameInitInfo* init_info)
{
    TigWindowData window_data;
    TigVideoBufferCreateInfo vb_create_info;
    tig_art_id_t art_id;
    TigFont font;
    const char* modern_path;

    tc_editor = init_info->editor;

    // Make sure we're not in the editor.
    if (tc_editor) {
        return true;
    }

    // Retrieve window data to set up content rectangle.
    if (tig_window_data(init_info->iso_window_handle, &window_data) != TIG_OK) {
        return false;
    }

    tc_iso_window_handle = init_info->iso_window_handle;
    tc_iso_window_invalidate_rect = init_info->invalidate_rect_func;

    tc_iso_window_rect = window_data.rect;

    // Enlarge conversation text when requested via arcanum.ini.
    tc_scale = tc_dialog_font_scale();
    if (tc_scale < highres_config_get()->dialog_scale) {
        tc_scale = highres_config_get()->dialog_scale;
    }
    if (modern_font_enabled() && tc_scale < modern_font_size_for(DIALOG_FONT_SIZE_KEY) / 12.0f) {
        tc_scale = modern_font_size_for(DIALOG_FONT_SIZE_KEY) / 12.0f;
    }
    if (tc_scale < 1.0f) {
        tc_scale = 1.0f;
    }

    // Set up the intermediate video buffers' rect. Height scales with the text
    // so enlarged option lines are not clipped.
    tc_intermediate_video_buffer_rect.x = 0;
    tc_intermediate_video_buffer_rect.y = 0;
    tc_intermediate_video_buffer_rect.width = window_data.rect.width;
    tc_intermediate_video_buffer_rect.height = (int)(TEXT_CONVERSATION_BOX_HEIGHT * tc_scale + 0.5f);

    // Create the scratch video buffer for rendering text (color keyed for
    // transparency).
    vb_create_info.flags = TIG_VIDEO_BUFFER_CREATE_COLOR_KEY | TIG_VIDEO_BUFFER_CREATE_SYSTEM_MEMORY;
    vb_create_info.width = tc_intermediate_video_buffer_rect.width;
    vb_create_info.height = tc_intermediate_video_buffer_rect.height;
    vb_create_info.color_key = tig_color_make(0, 0, 0);
    vb_create_info.background_color = vb_create_info.color_key;
    if (tig_video_buffer_create(&vb_create_info, &tc_scratch_video_buffer) != TIG_OK) {
        return false;
    }

    // Create the backdrop video buffer for rendering the tinted background.
    vb_create_info.flags = TIG_VIDEO_BUFFER_CREATE_SYSTEM_MEMORY;
    vb_create_info.width = tc_intermediate_video_buffer_rect.width;
    vb_create_info.height = tc_intermediate_video_buffer_rect.height;
    vb_create_info.background_color = 0;
    if (tig_video_buffer_create(&vb_create_info, &tc_backdrop_video_buffer) != TIG_OK) {
        tig_video_buffer_destroy(tc_scratch_video_buffer);
        return false;
    }

    // Set up font creation parameters.
    tig_art_interface_id_create(tc_dialog_font_art_num(), 0, 0, 0, &art_id);
    font.flags = TIG_FONT_SHADOW;
    font.art_id = art_id;
    font.modern_font_path = NULL;
    font.modern_font_size = 12.0f;
    font.modern_shadow_color = tig_color_make(10, 10, 10);
    font.modern_shadow_opacity = 255;
    font.modern_shadow_distance = 1;

    modern_path = NULL;
    if (modern_font_enabled()) {
        modern_path = modern_font_path_for(DIALOG_FONT_PATH_KEY);
        if (modern_path != NULL) {
            font.flags |= TIG_FONT_MODERN;
            font.modern_font_path = modern_path;
            font.modern_font_size = modern_font_size_for(DIALOG_FONT_SIZE_KEY);
        }
    }

    // Enlarge conversation text when requested.
    font.scale = (font.flags & TIG_FONT_MODERN) != 0 ? 1.0f : tc_dialog_font_art_scale();
    if ((font.flags & TIG_FONT_MODERN) == 0 && font.scale > 1.0f) {
        font.flags |= TIG_FONT_SCALE;
    }

    // Create white font for normal text.
    if (settings_get_value(&settings, DIALOGUE_EMOTE_DIM_KEY) != 0
        && settings_get_value(&settings, DIALOGUE_EMOTE_DIM_CHOICES_KEY) != 0) {
        font.flags |= TIG_FONT_DIM_BRACKETS;
    }
    font.bracket_dim_percent = modern_font_int_for(DIALOGUE_EMOTE_DIM_PERCENT_KEY, 60, 0, 100);
    font.color = tig_color_make(255, 255, 255);
    tig_font_create(&font, &tc_white_font);

    // Create yellow font for highlighted text.
    font.flags &= ~TIG_FONT_DIM_BRACKETS;
    font.color = tig_color_make(255, 255, 0);
    tig_font_create(&font, &tc_yellow_font);

    // Create red font, the purpose is unclear. This font is only used instead
    // of the yellow font, but appropriate variable `dword_5FF544` is never set.
    font.color = tig_color_make(255, 0, 0);
    tig_font_create(&font, &tc_red_font);

    // Calculate the line height based on the available vertical space.
    tc_line_height = (tc_intermediate_video_buffer_rect.height - ((TEXT_CONVERSATION_LINES + 1) * TEXT_CONVERSATION_VERT_SPACING)) / TEXT_CONVERSATION_LINES;

    return true;
}

/**
 * Called when the game shuts down.
 *
 * 0x4C9540
 */
void tc_exit(void)
{
    // Make sure we're not in the editor.
    if (tc_editor) {
        return;
    }

    // Destroy fonts.
    tig_font_destroy(tc_red_font);
    tig_font_destroy(tc_yellow_font);
    tig_font_destroy(tc_white_font);

    // Destroy intermediate buffers.
    tig_video_buffer_destroy(tc_backdrop_video_buffer);
    tig_video_buffer_destroy(tc_scratch_video_buffer);

    tc_iso_window_handle = TIG_WINDOW_HANDLE_INVALID;
    tc_iso_window_invalidate_rect = NULL;
}

/**
 * Called when the window size has changed.
 *
 * 0x4C95A0
 */
void tc_resize(GameResizeInfo* resize_info)
{
    bool was_active;

    // Preserve the active state to restore it after resizing.
    was_active = tc_active;
    if (was_active) {
        tc_hide();
    }

    // Update window properties.
    tc_iso_window_handle = resize_info->window_handle;
    tc_iso_window_rect = resize_info->window_rect;

    // Restore the active state if it was active before resizing.
    if (was_active) {
        tc_show();
    }
}

/**
 * Called to draw contents to the game window.
 *
 * 0x4C95F0
 */
void tc_set_bottom_gap_offset(int offset)
{
    if (tc_editor || offset == tc_bottom_gap_offset) {
        return;
    }
    if (tc_active) {
        tc_iso_window_invalidate_rect(&tc_content_rect);
    }
    tc_bottom_gap_offset = offset;
    tc_update_content_rect_height();
    if (tc_active) {
        tc_iso_window_invalidate_rect(&tc_content_rect);
    }
}

bool tc_is_active(void)
{
    return tc_active;
}

TigRect tc_get_content_rect(void)
{
    return tc_content_rect;
}

void tc_draw(GameDrawInfo* draw_info)
{
    // Make sure we're not in the editor.
    if (tc_editor) {
        return;
    }

    // Make sure text conversation is currently active.
    if (!tc_active) {
        return;
    }

    tc_render_internal(*draw_info->rects);
}

/**
 * Called when game isometric window is scrolled (either with mouse or
 * keyboard).
 *
 * 0x4C9620
 */
void tc_scroll(int dx, int dy)
{
    TigRect rect;

    // Make sure we're not in the editor.
    if (tc_editor) {
        return;
    }

    // Make sure text conversation is currently active.
    if (!tc_active) {
        return;
    }

    rect = tc_content_rect;

    // Expand affected rect based on horizontal scroll.
    if (dx < 0) {
        rect.x += dx;
        rect.width -= dx;
    } else if (dx > 0) {
        rect.width += dx;
    }

    // Expand affected rect based on vertical scroll.
    if (dy < 0) {
        rect.y += dy;
        rect.height -= dy;
    } else if (dy > 0) {
        rect.height += dy;
    }

    // Invalidate the affected rect (which now includes portions affected by
    // scrolling) to trigger a redraw.
    tc_iso_window_invalidate_rect(&rect);
}

/**
 * Shows the text conversation.
 *
 * This function does nothing if conversation is already visible.
 *
 * 0x4C96C0
 */
void tc_show(void)
{
    // Make sure we're not in the editor.
    if (tc_editor) {
        return;
    }

    // Make sure text conversation is not currently active.
    if (tc_active) {
        return;
    }

    // Mark as visible and trigger a redraw.
    tc_active = true;
    tc_iso_window_invalidate_rect(&tc_content_rect);
}

/**
 * Hides the text conversation.
 *
 * This function does nothing if conversation is not currently visible.
 *
 * 0x4C96F0
 */
void tc_hide(void)
{
    // Make sure we're not in the editor.
    if (tc_editor) {
        return;
    }

    // Make sure text conversation is currently active.
    if (!tc_active) {
        return;
    }

    tc_active = false;
    tc_iso_window_invalidate_rect(&tc_content_rect);
}

/**
 * Clears the text conversation area.
 *
 * 0x4C9720
 */
void tc_clear(bool compact)
{
    int idx;

    // Make sure we're not in the editor.
    if (tc_editor) {
        return;
    }

    // Clear scratch (text) video buffer with black color (which is also a key
    // color, effectively making it transparent).
    tig_video_buffer_fill(tc_scratch_video_buffer,
        &tc_intermediate_video_buffer_rect,
        tig_color_make(0, 0, 0));

    // Reset all option strings.
    for (idx = 0; idx < TEXT_CONVERSATION_LINES; idx++) {
        tc_options[idx] = NULL;
    }

    // Invalidate current content rect (if any).
    tc_iso_window_invalidate_rect(&tc_content_rect);

    // Reset highlight state and some internal flag (which is unused).
    tc_highlighted_idx = -1;
    dword_5FF544 = false;
    tc_compact = compact;

    // Set default content size and center it horizontally.
    tc_content_rect.width = 400;
    tc_content_rect.x = (tc_iso_window_rect.width - tc_content_rect.width) / 2;
    tc_update_content_rect_height();
}

/**
 * Sets a text conversation option at the specified index.
 *
 * 0x4C9810
 */
void tc_set_option(int index, const char* str)
{
    // Make sure we're not in the editor.
    if (tc_editor) {
        return;
    }

    // Store the option string. Note that we don't create a copy here.
    tc_options[index] = str;
    tc_update_content_rect_height();

    tc_render_option(index, false);
}

/**
 * Returns the index of text conversation option being pressed.
 *
 * 0x4C9A10
 */
int tc_handle_message(TigMessage* msg)
{
    int index;

    // Make sure we're not in the editor.
    if (tc_editor) {
        return -1;
    }

    // Make sure text conversation is currently active.
    if (!tc_active) {
        return -1;
    }

    if (msg->type == TIG_MESSAGE_KEYBOARD
        && !msg->data.keyboard.pressed
        && settings_get_value(&settings, DIALOGUE_OPTION_NUMBERS_KEY) != 0) {
        index = msg->data.keyboard.scancode >= SDL_SCANCODE_1 && msg->data.keyboard.scancode <= SDL_SCANCODE_9
            ? msg->data.keyboard.scancode - SDL_SCANCODE_1
            : msg->data.keyboard.scancode == SDL_SCANCODE_0 ? 9 : -1;
        return index >= 0 && index < TEXT_CONVERSATION_LINES && tc_options[index] != NULL ? index : -1;
    }

    // Only process mouse messages.
    if (msg->type != TIG_MESSAGE_MOUSE) {
        return -1;
    }

    // Check if the mouse is within the content rectangle.
    if (msg->data.mouse.x < tc_iso_window_rect.x + tc_content_rect.x
        || msg->data.mouse.y < tc_iso_window_rect.y + tc_content_rect.y
        || msg->data.mouse.x >= tc_iso_window_rect.x + tc_content_rect.x + tc_content_rect.width
        || msg->data.mouse.y >= tc_iso_window_rect.y + tc_content_rect.y + tc_content_rect.height) {
        // Mouse cursor is outside of the text conversation area, clear the
        // highlighted option.
        if (tc_highlighted_idx != -1) {
            tc_set_option(tc_highlighted_idx, tc_options[tc_highlighted_idx]);
            tc_highlighted_idx = -1;
        }
        return -1;
    }

    switch (msg->data.mouse.event) {
    case TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN:
        if (msg->data.mouse.repeat) {
            return -1;
        }

        // Calculate the option index based on the mouse's vertical position.
        index = tc_option_index_from_y(msg->data.mouse.y - (tc_iso_window_rect.y + tc_content_rect.y));
        if (index != -1 && tc_options[index] != NULL) {
            if (tc_highlighted_idx != -1) {
                tc_render_option(tc_highlighted_idx, false);
                tc_highlighted_idx = -1;
            }
            return index;
        }
        return -1;
    case TIG_MESSAGE_MOUSE_MOVE:
        // Calculate the option index based on the mouse's vertical position.
        index = tc_option_index_from_y(msg->data.mouse.y - (tc_iso_window_rect.y + tc_content_rect.y));

        // Clear the previous highlight if mouse moved to a different option.
        if (tc_highlighted_idx != index && tc_highlighted_idx != -1) {
            tc_render_option(tc_highlighted_idx, false);
        }

        // Highlight the new option if it exists.
        if (index != -1 && tc_options[index] != NULL) {
            tc_render_option(index, true);
            tc_highlighted_idx = index;
        } else {
            tc_highlighted_idx = -1;
        }
        return -1;
    default:
        return -1;
    }
}

int tc_content_height_for_lines(int lines)
{
    int min_lines;

    min_lines = tc_dialog_options_min();
    if (lines < min_lines) {
        lines = min_lines;
    } else if (lines > TEXT_CONVERSATION_LINES) {
        lines = TEXT_CONVERSATION_LINES;
    }

    return tc_dialog_margin_vertical() * 2 + lines * tc_line_height + (lines + 1) * TEXT_CONVERSATION_VERT_SPACING;
}

int tc_dialog_font_size(void)
{
    int value;

    value = settings_get_value(&settings, DIALOG_FONT_KEY);
    if (value < 0) {
        value = 0;
    } else if (value > 2) {
        value = 2;
    }

    return value;
}

float tc_dialog_font_scale(void)
{
    switch (tc_dialog_font_size()) {
    case 1:
        return 14.0f / 12.0f;
    case 2:
        return 16.0f / 12.0f;
    default:
        return 1.0f;
    }
}

int tc_dialog_font_art_num(void)
{
    switch (tc_dialog_font_size()) {
    case 1:
    case 2:
        return 230; // "Flare14Font.art".
    default:
        return 229; // "Flare12Font.art".
    }
}

float tc_dialog_font_art_scale(void)
{
    switch (tc_dialog_font_size()) {
    case 2:
        return 16.0f / 14.0f;
    default:
        return 1.0f;
    }
}

int tc_dialog_options_min(void)
{
    int value;

    value = settings_get_value(&settings, DIALOG_OPTIONS_MIN_KEY);
    if (value < 1) {
        value = 1;
    } else if (value > TEXT_CONVERSATION_LINES) {
        value = TEXT_CONVERSATION_LINES;
    }

    return value;
}

int tc_dialog_margin_horizontal(void)
{
    return modern_font_int_for(DIALOG_MARGIN_KEY, 16, TEXT_CONVERSATION_HOR_PADDING, 80);
}

int tc_dialog_margin_vertical(void)
{
    return modern_font_int_for(DIALOG_MARGIN_VERTICAL_KEY, 8, 0, 80);
}

int tc_dialog_background_opacity(void)
{
    return modern_font_int_for(DIALOG_BACKGROUND_OPACITY_KEY, 18, 0, 255);
}

void tc_update_content_rect_height(void)
{
    TigRect old_rect;
    int lines;
    int idx;
    int bottom_offset;

    old_rect = tc_content_rect;

    lines = 0;
    for (idx = 0; idx < TEXT_CONVERSATION_LINES; idx++) {
        if (tc_options[idx] != NULL) {
            lines = idx + 1;
        }
    }

    tc_content_rect.height = tc_content_height_for_lines(lines);

    // Adjust vertical position depending on normal vs. compact mode while
    // keeping the bottom edge anchored where vanilla placed the dialogue area.
    bottom_offset = tc_compact ? 37 : 159;
    bottom_offset -= tc_bottom_gap_offset;
    if (bottom_offset < 0) {
        bottom_offset = 0;
    }
    tc_content_rect.y = tc_iso_window_rect.height - tc_content_rect.height - bottom_offset;

    if (tc_active) {
        tc_iso_window_invalidate_rect(&old_rect);
        tc_iso_window_invalidate_rect(&tc_content_rect);
    }
}

void tc_render_option(int index, bool highlighted)
{
    TigRect rect;
    TigRect dirty_rect;
    char prefix[16];
    TigFont prefix_font = { 0 };
    int prefix_width = 0;

    if (index < 0 || index >= TEXT_CONVERSATION_LINES || tc_options[index] == NULL) {
        return;
    }

    rect.x = tc_dialog_margin_horizontal();
    rect.y = tc_dialog_margin_vertical() + index * (tc_line_height + TEXT_CONVERSATION_VERT_SPACING) + TEXT_CONVERSATION_VERT_SPACING;
    rect.width = tc_intermediate_video_buffer_rect.width - tc_dialog_margin_horizontal() * 2;
    rect.height = tc_line_height;

    // Clear scratch (text) video buffer with black color (which is also a key
    // color, effectively making it transparent).
    tig_video_buffer_fill(tc_scratch_video_buffer,
        &rect,
        tig_color_make(0, 0, 0));

    if (highlighted) {
        if (dword_5FF544) {
            // NOTE: Since `dword_5FF544` is never set, this code path is dead.
            tig_font_push(tc_red_font);
        } else {
            tig_font_push(tc_yellow_font);
        }
    } else {
        tig_font_push(tc_white_font);
    }

    if (settings_get_value(&settings, DIALOGUE_OPTION_NUMBERS_KEY) != 0) {
        prefix_font.str = "99. ";
        tig_font_measure(&prefix_font);
        prefix_width = prefix_font.width;
        snprintf(prefix, sizeof(prefix), "%d. ", index + 1);
        TigRect prefix_rect = rect;
        prefix_rect.width = prefix_width;
        tig_font_write(tc_scratch_video_buffer, prefix, &prefix_rect, &dirty_rect);
        rect.x += prefix_width;
        rect.width -= prefix_width;
    }
    tig_font_write(tc_scratch_video_buffer, tc_options[index], &rect, &dirty_rect);
    tig_font_pop();

    if (tc_content_rect.width < dirty_rect.width + prefix_width + tc_dialog_margin_horizontal() * 2) {
        tc_content_rect.width = dirty_rect.width + prefix_width + tc_dialog_margin_horizontal() * 2;
        tc_content_rect.x = (tc_iso_window_rect.width - tc_content_rect.width) / 2;
    }

    if (tc_active) {
        tc_iso_window_invalidate_rect(&tc_content_rect);
    }
}

int tc_option_index_from_y(int y)
{
    int index;
    int option_y;

    if (y < 0 || y >= tc_content_rect.height) {
        return -1;
    }

    for (index = 0; index < TEXT_CONVERSATION_LINES; index++) {
        option_y = tc_dialog_margin_vertical() + index * (tc_line_height + TEXT_CONVERSATION_VERT_SPACING) + TEXT_CONVERSATION_VERT_SPACING;
        if (y >= option_y && y < option_y + tc_line_height) {
            return index;
        }
    }

    return -1;
}

/**
 * Measures the width required to render a string and return overflow in pixels.
 *
 * This function is used to validate that a particular dialog option will fit
 * into text conversation area (remember there is no wrapping).
 *
 * Return `0` if the is no overflow.
 *
 * 0x4C9B90
 */
int tc_check_size(const char* str)
{
    TigFont font;
    int width;

    font.width = 0;
    font.str = str;

    tig_font_push(tc_white_font);
    tig_font_measure(&font);
    tig_font_pop();

    width = font.width - tc_intermediate_video_buffer_rect.width + TEXT_CONVERSATION_HOR_PADDING * 2;
    if (settings_get_value(&settings, DIALOGUE_OPTION_NUMBERS_KEY) != 0) {
        font.str = "99. ";
        font.width = 0;
        tig_font_push(tc_white_font);
        tig_font_measure(&font);
        tig_font_pop();
        width += font.width;
    }
    if (width < 0) {
        width = 0;
    }

    return width;
}

int tc_reserved_height(float dialog_scale)
{
    if (dialog_scale < 1.0f) {
        dialog_scale = 1.0f;
    }

    return (int)(TEXT_CONVERSATION_BOX_HEIGHT * dialog_scale + 0.5f) + TEXT_CONVERSATION_BOTTOM_OFFSET;
}

/**
 * Renders the text conversation.
 *
 * `node` is a head of list of dirty rects.
 *
 * 0x4C9BE0
 */
void tc_render_internal(TigRectListNode* node)
{
    TigRect src_rect;
    TigRect dst_rect;
    TigWindowBlitInfo win_to_vb_blt;
    TigWindowBlitInfo vb_to_win_blt;
    TigRectListNode* tmp_head;
    TigRectListNode* tmp_node;

    // Set up blit operation to copy from window to backdrop buffer.
    win_to_vb_blt.type = TIG_WINDOW_BLT_WINDOW_TO_VIDEO_BUFFER;
    win_to_vb_blt.vb_blit_flags = 0;
    win_to_vb_blt.src_window_handle = tc_iso_window_handle;
    win_to_vb_blt.src_rect = &src_rect;
    win_to_vb_blt.dst_video_buffer = tc_backdrop_video_buffer;
    win_to_vb_blt.dst_rect = &dst_rect;

    // Set up blit operation to copy from backdrop buffer to window.
    vb_to_win_blt.type = TIG_WINDOW_BLIT_VIDEO_BUFFER_TO_WINDOW;
    vb_to_win_blt.vb_blit_flags = 0;
    vb_to_win_blt.src_video_buffer = tc_backdrop_video_buffer;
    vb_to_win_blt.src_rect = &dst_rect;
    vb_to_win_blt.dst_window_handle = tc_iso_window_handle;
    vb_to_win_blt.dst_rect = &src_rect;

    tmp_head = NULL;

    while (node != NULL) {
        if (tig_rect_intersection(&tc_content_rect, &(node->rect), &src_rect) == TIG_OK) {
            // Calculate the destination rect relative to the content rect.
            dst_rect.x = src_rect.x - tc_content_rect.x;
            dst_rect.y = src_rect.y - tc_content_rect.y;
            dst_rect.width = src_rect.width;
            dst_rect.height = src_rect.height;

            // Copy the window content to the backdrop buffer, tint it, and copy
            // it back.
            tig_window_blit(&win_to_vb_blt);
            if (tc_dialog_background_opacity() > 0) {
                tig_video_buffer_blend_color(tc_backdrop_video_buffer,
                    &dst_rect,
                    tig_color_make(0, 0, 0),
                    tc_dialog_background_opacity());
            }
            tig_window_blit(&vb_to_win_blt);

            // Store the affected rect in a new list for optimized text
            // rendering.
            tmp_node = tig_rect_node_create();
            tmp_node->rect = dst_rect;
            tmp_node->next = tmp_head;
            tmp_head = tmp_node;
        }
        node = node->next;
    }

    // Switch to render from scratch (text) buffer to window.
    vb_to_win_blt.src_video_buffer = tc_scratch_video_buffer;

    while (tmp_head != NULL) {
        vb_to_win_blt.src_rect = &(tmp_head->rect);

        // Adjust the source rect to window coordinates.
        src_rect = tmp_head->rect;
        src_rect.x += tc_content_rect.x;
        src_rect.y += tc_content_rect.y;
        tig_window_blit(&vb_to_win_blt);

        // Free this node.
        tmp_node = tmp_head->next;
        tig_rect_node_destroy(tmp_head);
        tmp_head = tmp_node;
    }
}
