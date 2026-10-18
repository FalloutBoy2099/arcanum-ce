#include "ui/logbook_ui.h"
#include <tig/performance.h>
#include "ui/cache_key.h"
#include "ui/search_bar.h"
#include "ui/journal_index.h"
#include "ui/textedit_ui.h"
#include "ui/text_pagination.h"
#include "game/area.h"
#include "game/conversation_history.h"
#include "game/journal_memory.h"
#include "game/journal_text.h"
#include "game/name.h"
#include "game/obj_pool.h"
#include "game/reaction.h"

#include <stdio.h>

#include "game/background.h"
#include "game/bless.h"
#include "game/critter.h"
#include "game/curse.h"
#include "game/description.h"
#include "game/effect.h"
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/item.h"
#include "game/input_bindings.h"
#include "game/location.h"
#include "game/language.h"
#include "game/logbook.h"
#include "game/mes.h"
#include "game/modern_font.h"
#include "game/player.h"
#include "game/quest.h"
#include "game/reputation.h"
#include "game/rumor.h"
#include "game/snd.h"
#include "game/timeevent.h"
#include "game/ui.h"
#include "ui/intgame.h"
#include "ui/types.h"

enum LogbookUiButton {
    LOGBOOK_UI_BUTTON_TURN_PAGE_LEFT,
    LOGBOOK_UI_BUTTON_TURN_PAGE_RIGHT,
    LOGBOOK_UI_BUTTON_COUNT,
};

enum LogbookUiTab {
    // Also includes hidden Bloopers and Quotes.
    LOGBOOK_UI_TAB_RUMORS_AND_NOTES,
    LOGBOOK_UI_TAB_QUESTS,
    LOGBOOK_UI_TAB_REPUTATIONS,
    LOGBOOK_UI_TAB_BLESSINGS_AND_CURSES,
    LOGBOOK_UI_TAB_KILLS_AND_INJURES,
    LOGBOOK_UI_TAB_BACKGROUND,
    LOGBOOK_UI_TAB_KEYS,
    LOGBOOK_UI_TAB_COUNT,
};

static void logbook_ui_create(void);
static void logbook_ui_destroy(void);
static bool logbook_ui_message_filter(TigMessage* msg);
static void logbook_ui_draw_panel(int border_art_num, bool preserve_page);
static void logbook_ui_switch_tab(int tab, bool preserve_page);
static void logbook_ui_refresh(void);
static int logbook_ui_draw_page_spread(int start_entry, int max_entry);
static int logbook_ui_draw_entry(int index, TigRect* rect, bool dry_run, bool can_truncate);
static int logbook_ui_draw_text(char* buffer, tig_font_handle_t font, TigRect* rect, bool dry_run, bool can_truncate, bool add_line_spacing);
static void logbook_ui_load_data(void);
static void logbook_ui_load_index(void);
static bool logbook_ui_format_entry(char* buffer, size_t maxlen, int index);
static void logbook_ui_search_changed(TextEdit* edit);
static void logbook_ui_search_finished(TextEdit* edit);
static void logbook_ui_draw_search_bar(void);
static bool logbook_ui_search_input(TigMessage* msg);
static int logbook_ui_draw_search_text(char* text, tig_font_handle_t font, TigRect* rect,
    bool dry_run, bool can_truncate, bool spacing, bool heading);
static void logbook_ui_toggle_index(void);
static void logbook_ui_index_hover(int x, int y);
static void logbook_ui_index_open_quests(int index);
static void logbook_ui_load_subject_quests(void);
static void logbook_ui_toggle_history(void);
static void logbook_ui_load_history(void);
static void logbook_ui_clear_history_chunks(void);
static void logbook_ui_clear_index_cache(void);
static void logbook_ui_format_rumor(char* buffer, size_t maxlen, int index);
static void logbook_ui_format_timestamp(char* buffer, size_t maxlen, int index);
static void logbook_ui_format_quest(char* buffer, size_t maxlen, int index);
static void logbook_ui_format_reputation(char* buffer, size_t maxlen, int index);
static void logbook_ui_format_blessing_or_curse(char* buffer, size_t maxlen, int index);
static void logbook_ui_format_kill_or_injury(char* buffer, size_t maxlen, int index);
static void logbook_ui_format_background(char* buffer, size_t maxlen, int index);
static void logbook_ui_format_key(char* buffer, size_t maxlen, int index);
static tig_color_t logbook_ui_color(const char* key, int default_red, int default_green, int default_blue);
static bool logbook_ui_parse_color(const char* str, int* red, int* green, int* blue);

/**
 * Handle to the logbook UI window.
 *
 * 0x5C33F0
 */
static tig_window_handle_t logbook_ui_window = TIG_WINDOW_HANDLE_INVALID;
static tig_window_handle_t logbook_ui_search_window = TIG_WINDOW_HANDLE_INVALID;
static char logbook_ui_search[128];
static bool logbook_ui_search_focused;
static bool logbook_ui_search_filter = true;
static TextEdit logbook_ui_search_edit = { 0, logbook_ui_search, sizeof(logbook_ui_search),
    logbook_ui_search_finished, logbook_ui_search_changed, logbook_ui_search_finished };
static TigRect logbook_ui_search_cells[3] = {
    { 0, 0, 300, 22 }, { 310, 0, 80, 22 }, { 393, 0, 80, 22 },
};

/**
 * 0x5C33F8
 */
static TigRect logbook_ui_background_rects[2] = {
    { 0, 41, 800, 400 },
    { 150, 11, 501, 365 },
};

/**
 * Defines a rect where PC lens is located in the logbook UI.
 *
 * 0x5C3418
 */
static TigRect logbook_ui_pc_lens_rect = { 25, 24, 89, 89 };

/**
 * Defines logbook UI navigation/page-turn buttons.
 *
 * 0x5C3428
 */
static UiButtonInfo logbook_ui_page_buttons[LOGBOOK_UI_BUTTON_COUNT] = {
    { 213, 380, 272, TIG_BUTTON_HANDLE_INVALID },
    { 675, 380, 273, TIG_BUTTON_HANDLE_INVALID },
};

static UiButtonInfo logbook_ui_index_button = { 213, 77, NAME_JOURNAL_INDEX_BUTTON, TIG_BUTTON_HANDLE_INVALID };
static bool logbook_ui_index_mode;
static int logbook_ui_saved_page;
static int logbook_ui_saved_spreads[512];
typedef struct JournalIndexEntry {
    const char* name;
    bool heading;
    bool hoverable;
} JournalIndexEntry;
static JournalIndexEntry logbook_ui_index_entries[3000];
static bool logbook_ui_index_cache_valid;
static uint64_t logbook_ui_index_cache_key;
static int logbook_ui_index_cache_count;
static TigRect logbook_ui_index_entry_rects[3000];
static int logbook_ui_index_hovered = -1;
static char logbook_ui_index_subject[MAX_STRING];
static int logbook_ui_index_saved_page;
static int logbook_ui_index_saved_spreads[512];
static bool logbook_ui_history_mode;
static int logbook_ui_history_saved_page;
static int logbook_ui_history_saved_spreads[512];
static char* logbook_ui_history_chunks[1000];
static int logbook_ui_history_chunk_count;
static bool logbook_ui_measure_only;
static bool logbook_ui_history_cache_valid;
static uint64_t logbook_ui_history_cache_key;
static int logbook_ui_history_layout_spreads[512];
static int logbook_ui_history_latest_page;

/**
 * Defines logbook UI tab buttons.
 *
 * 0x5C3448
 */
static UiButtonInfo logbook_ui_tab_buttons[LOGBOOK_UI_TAB_COUNT] = {
    /*     LOGBOOK_UI_TAB_RUMORS_AND_NOTES */ { 696, 83, 265, TIG_BUTTON_HANDLE_INVALID },
    /*               LOGBOOK_UI_TAB_QUESTS */ { 696, 129, 267, TIG_BUTTON_HANDLE_INVALID },
    /*          LOGBOOK_UI_TAB_REPUTATIONS */ { 696, 175, 271, TIG_BUTTON_HANDLE_INVALID },
    /* LOGBOOK_UI_TAB_BLESSINGS_AND_CURSES */ { 695, 221, 266, TIG_BUTTON_HANDLE_INVALID },
    /*    LOGBOOK_UI_TAB_KILLS_AND_INJURES */ { 696, 267, 270, TIG_BUTTON_HANDLE_INVALID },
    /*           LOGBOOK_UI_TAB_BACKGROUND */ { 696, 313, 268, TIG_BUTTON_HANDLE_INVALID },
    /*                 LOGBOOK_UI_TAB_KEYS */ { 696, 359, 269, TIG_BUTTON_HANDLE_INVALID },
};

/**
 * Defines text area for the left and right pages.
 *
 * 0x5C34B8
 */
static TigRect logbook_ui_page_content_rects[2] = {
    { 222, 72, 215, 270 },
    { 468, 72, 215, 270 },
};

/**
 * Defines page header area for the left and right pages where the page title
 * is rendered.
 *
 * 0x5C34D8
 */
static TigRect logbook_ui_page_header_rects[2] = {
    { 236, 43, 187, 19 },
    { 482, 43, 187, 19 },
};

/**
 * Defines page footer area for the left and right pages where the page number
 * is rendered.
 *
 * 0x5C34F8
 */
static TigRect logbook_ui_page_footer_rects[2] = {
    { 222, 346, 215, 15 },
    { 468, 346, 215, 15 },
};

/**
 * Height (in pixels) of a single text line. Used as inter-entry spacing so that
 * consecutive entries are visually separated by one blank line.
 *
 * 0x63CBF0
 */
static int logbook_ui_line_height;

/**
 * Per-entry quest state values for the "Quests" tab.
 *
 * 0x63CBF4
 */
static int logbook_ui_quest_states[3000];

/**
 * 0x63FAD8
 */
static int64_t logbook_ui_obj;

/**
 * Green strikethrough font. Used for quests that have been completed by the
 * player's party.
 *
 * 0x63FAE0
 */
static tig_font_handle_t logbook_ui_font_quest_completed;

/**
 * Per-entry data array shared across all tabs. Its interpretation varies by
 * active tab:
 *
 *  - Rumors & Notes: rumor number.
 *  - Quests: quest number.
 *  - Reputations: reputation number.
 *  - Blessings/Curses: blessing or curse ID
 *  - Kills & Injuries: name of the injured critter (only for entries beyond 9).
 *  - Background: element [0] holds the background number, subsequent elements
 *    hold the character offset of the start of each paginated sub-page, the
 *    last element (at `logbook_ui_entry_count` index) serving as a one-past-end
 *     sentinel equal to the total text length.
 *  - Keys: key ID.
 *
 * 0x63FAE4
 */
static int logbook_ui_entry_ids[3000];

/**
 * Black strikethrough font. Used for rumors that have been quelled (proven
 * false) and for injuries that have since healed.
 *
 * 0x6429C4
 */
static tig_font_handle_t logbook_ui_font_struck;

/**
 * Centred black font. Used for page-number labels in the footer areas.
 *
 * 0x6429C8
 */
static tig_font_handle_t logbook_ui_font_page_numbers;

/**
 * "quotes.mes"
 *
 * 0x6429CC
 */
static mes_file_handle_t quotes_mes_file;

/**
 * Plain red font. Used for curse entries in the Blessings & Curses tab.
 *
 * 0x6429D0
 */
static tig_font_handle_t logbook_ui_font_curse;

/**
 * Per-entry timestamp array. For most tabs this records when the event
 * occurred.
 *
 * For the "Kills & Injuries" tab, the `milliseconds` field of each injury
 * entry repurposed to store type of injury, rather than a time value.
 *
 * 0x6429D8
 */
static DateTime logbook_ui_entry_datetimes[3000];

/**
 * Current page number, 1-indexed and always odd (pages are shown two at a
 * time, page 1 is the first spread, page 3 is the second, etc.).
 *
 * 0x648798
 */
static int logbook_ui_current_page;

/**
 * Maps each page-spread index to the index of the first entry displayed on
 * that spread. Index 0 always maps to entry 0 (the beginning of the list).
 * Entries beyond the current spread are invalidated (-1) when a new tab is
 * selected or the logbook is reset.
 *
 * 0x64879C
 */
static int logbook_ui_page_spread_starts[512];

/**
 * Red strikethrough font. Used for quests that were botched or completed by
 * someone other than the player's party.
 *
 * 0x64892C
 */
static tig_font_handle_t logbook_ui_font_quest_failed;

/**
 * Large centred font. Used for the tab-name heading printed above each page.
 *
 * 0x648930
 */
static tig_font_handle_t logbook_ui_font_header;

/**
 * Default plain black font. Used for normal entries and as the base style when
 * no other condition applies.
 *
 * 0x648934
 */
static tig_font_handle_t logbook_ui_font_default;

/**
 * Blue font. Used for active quests and blessings.
 *
 * 0x64893C
 */
static tig_font_handle_t logbook_ui_font_active;

/**
 * Total number of entries loaded for the currently active tab.
 *
 * For the "Background" tab this represents the number of paginated sub-pages.
 *
 * 0x648938
 */
static int logbook_ui_entry_count;

/**
 * Kill and injury statistics array populated by `logbook_get_kills`.
 *
 * 0x648940
 */
static int logbook_ui_kill_stats[LBK_COUNT];

/**
 * Index of the last entry successfully rendered onto the current page spread.
 * Updated by `logbook_ui_draw_page_spread`() and used to determine whether the
 * "next page" button should be visible.
 *
 * 0x648974
 */
static int logbook_ui_last_visible_entry;

/**
 * Currently active tab. Persists across close/open cycles.
 *
 * 0x648978
 */
static int logbook_ui_tab;

/**
 * The number of art in `interface.mes` for border around book spread.
 *
 * 0x64897C
 */
static int logbook_ui_border_art_num;

/**
 * Flag indicating that "Rumors & Notes" tab operates in "Bloopers & Quotes"
 * mode. This hidden easter egg is activated by holding Left Ctrl + Left Alt
 * while opening the logbook.
 *
 * 0x648980
 */
static bool logbook_ui_quotes_mode;

/**
 * "logbk_ui.mes"
 *
 * 0x648984
 */
static mes_file_handle_t logbook_ui_mes_file;

/**
 * Per-entry font handle.
 *
 * 0x648988
 */
static tig_font_handle_t logbook_ui_entry_fonts[3000];

/**
 * Flag indicating whether the logbook UI is initialized.
 *
 * 0x64B868
 */
static bool logbook_ui_initialized;

/**
 * Flag indicating whether the logbook UI is currently displayed.
 *
 * 0x64B86C
 */
static bool logbook_ui_created;

/**
 * Called when the game is initialized.
 *
 * 0x53EB10
 */
bool logbook_ui_init(GameInitInfo* init_info)
{
    TigFont font_info;
    int index;
    TigFontFlags body_font_flags = 0;
    float body_font_scale = 1.0f;
    int body_font_art_num = 229;
    const char* modern_path = NULL;

    (void)init_info;

    if (settings_get_value(&settings, LOGBOOK_FONT_KEY) != 0) {
        body_font_art_num = 230;
    }
    if (modern_font_enabled()) {
        modern_path = modern_font_path_for(LOGBOOK_FONT_PATH_KEY);
        if (modern_path != NULL) {
            body_font_flags |= TIG_FONT_MODERN;
            body_font_scale = modern_font_size_for(LOGBOOK_FONT_SIZE_KEY);
        }
    }

    // Load UI messages (required).
    if (!mes_load("mes\\logbk_ui.mes", &logbook_ui_mes_file)) {
        return false;
    }

    // Load quotes (required).
    if (!mes_load("mes\\quotes.mes", &quotes_mes_file)) {
        return false;
    }

    // Plain black - default body text.
    font_info.flags = body_font_flags;
    tig_art_interface_id_create(body_font_art_num, 0, 0, 0, &(font_info.art_id));
    font_info.str = NULL;
    font_info.color = logbook_ui_color(LOGBOOK_COLOR_DEFAULT_KEY, 0, 0, 0);
    font_info.scale = body_font_scale;
    font_info.modern_font_path = modern_path;
    font_info.modern_font_size = body_font_scale;
    font_info.modern_shadow_color = tig_color_make(10, 10, 10);
    font_info.modern_shadow_opacity = 255;
    font_info.modern_shadow_distance = 1;
    tig_font_create(&font_info, &logbook_ui_font_default);

    // Centred black - page numbers in the footer.
    font_info.flags = TIG_FONT_CENTERED | body_font_flags;
    tig_art_interface_id_create(body_font_art_num, 0, 0, 0, &(font_info.art_id));
    font_info.str = NULL;
    font_info.color = logbook_ui_color(LOGBOOK_COLOR_PAGE_KEY, 0, 0, 0);
    font_info.scale = body_font_scale;
    tig_font_create(&font_info, &logbook_ui_font_page_numbers);

    // Black strikethrough - quelled rumors and healed injuries.
    font_info.flags = TIG_FONT_STRIKE_THROUGH | body_font_flags;
    tig_art_interface_id_create(body_font_art_num, 0, 0, 0, &(font_info.art_id));
    font_info.str = NULL;
    font_info.color = logbook_ui_color(LOGBOOK_COLOR_STRUCK_KEY, 0, 0, 0);
    font_info.strike_through_color = font_info.color;
    font_info.scale = body_font_scale;
    tig_font_create(&font_info, &logbook_ui_font_struck);

    // Green strikethrough - quests completed by the player's party.
    font_info.flags = TIG_FONT_STRIKE_THROUGH | body_font_flags;
    tig_art_interface_id_create(body_font_art_num, 0, 0, 0, &(font_info.art_id));
    font_info.str = NULL;
    font_info.color = logbook_ui_color(LOGBOOK_COLOR_COMPLETED_KEY, 0, 120, 0);
    font_info.strike_through_color = font_info.color;
    font_info.scale = body_font_scale;
    tig_font_create(&font_info, &logbook_ui_font_quest_completed);

    // Red strikethrough - quests that were botched or completed by others.
    font_info.flags = TIG_FONT_STRIKE_THROUGH | body_font_flags;
    tig_art_interface_id_create(body_font_art_num, 0, 0, 0, &(font_info.art_id));
    font_info.str = NULL;
    font_info.color = logbook_ui_color(LOGBOOK_COLOR_FAILED_KEY, 150, 0, 0);
    font_info.strike_through_color = font_info.color;
    font_info.scale = body_font_scale;
    tig_font_create(&font_info, &logbook_ui_font_quest_failed);

    // Large centred - tab headings above each page.
    font_info.flags = TIG_FONT_CENTERED;
    tig_art_interface_id_create(27, 0, 0, 0, &(font_info.art_id));
    font_info.str = NULL;
    font_info.color = logbook_ui_color(LOGBOOK_COLOR_DEFAULT_KEY, 0, 0, 0);
    font_info.scale = 1.0f;
    tig_font_create(&font_info, &logbook_ui_font_header);

    // Blue - active quests and blessings currently in effect.
    font_info.flags = body_font_flags;
    tig_art_interface_id_create(body_font_art_num, 0, 0, 0, &(font_info.art_id));
    font_info.str = NULL;
    font_info.color = logbook_ui_color(LOGBOOK_COLOR_ACTIVE_KEY, 0, 0, 150);
    font_info.scale = body_font_scale;
    tig_font_create(&font_info, &logbook_ui_font_active);

    // Red - active curses currently in effect.
    font_info.flags = body_font_flags;
    tig_art_interface_id_create(body_font_art_num, 0, 0, 0, &(font_info.art_id));
    font_info.str = NULL;
    font_info.color = logbook_ui_color(LOGBOOK_COLOR_CURSE_KEY, 150, 0, 0);
    font_info.scale = body_font_scale;
    tig_font_create(&font_info, &logbook_ui_font_curse);

    // Measure a single line of the default font (used as inter-entry spacing).
    tig_font_push(logbook_ui_font_default);
    font_info.str = "test";
    font_info.width = 0;
    tig_font_measure(&font_info);
    logbook_ui_line_height = font_info.height;
    tig_font_pop();

    logbook_ui_border_art_num = -1;

    // Initialize page-spread history: spread 0 starts at entry 0.
    for (index = 0; index < SDL_arraysize(logbook_ui_page_spread_starts); index++) {
        logbook_ui_page_spread_starts[index] = -1;
    }
    logbook_ui_page_spread_starts[0] = 0;

    logbook_ui_tab = LOGBOOK_UI_TAB_RUMORS_AND_NOTES;
    logbook_ui_index_mode = false;
    logbook_ui_obj = OBJ_HANDLE_NULL;
    logbook_ui_current_page = 1;
    logbook_ui_initialized = true;

    return true;
}

/**
 * Called when the game shuts down.
 *
 * 0x53EF40
 */
void logbook_ui_exit(void)
{
    ui_search_bar_close(&logbook_ui_search_window);
    if (logbook_ui_search_focused) textedit_ui_unfocus(&logbook_ui_search_edit);
    logbook_ui_search_focused = false;
    logbook_ui_clear_index_cache();
    logbook_ui_clear_history_chunks();
    mes_unload(logbook_ui_mes_file);
    mes_unload(quotes_mes_file);
    tig_font_destroy(logbook_ui_font_default);
    tig_font_destroy(logbook_ui_font_page_numbers);
    tig_font_destroy(logbook_ui_font_struck);
    tig_font_destroy(logbook_ui_font_quest_completed);
    tig_font_destroy(logbook_ui_font_quest_failed);
    tig_font_destroy(logbook_ui_font_header);
    tig_font_destroy(logbook_ui_font_active);
    tig_font_destroy(logbook_ui_font_curse);
    logbook_ui_initialized = false;
}

/**
 * Called when the game is being reset.
 *
 * 0x53EFD0
 */
void logbook_ui_reset(void)
{
    int index;
    ui_search_bar_close(&logbook_ui_search_window);
    if (logbook_ui_search_focused) textedit_ui_unfocus(&logbook_ui_search_edit);
    logbook_ui_search_focused = false;
    logbook_ui_search[0] = '\0';
    logbook_ui_search_filter = true;
    logbook_ui_index_hovered = -1;
    logbook_ui_index_subject[0] = '\0';
    logbook_ui_index_mode = false;
    logbook_ui_history_mode = false;
    logbook_ui_clear_history_chunks();
    logbook_ui_clear_index_cache();

    if (logbook_ui_created) {
        logbook_ui_close();
    }

    logbook_ui_tab = LOGBOOK_UI_TAB_RUMORS_AND_NOTES;

    for (index = 0; index < SDL_arraysize(logbook_ui_page_spread_starts); index++) {
        logbook_ui_page_spread_starts[index] = -1;
    }
    logbook_ui_page_spread_starts[0] = 0;

    logbook_ui_obj = OBJ_HANDLE_NULL;
    logbook_ui_border_art_num = -1;
    logbook_ui_current_page = 1;
}

/**
 * Toggles the logbook UI.
 *
 * 0x53F020
 */
void logbook_ui_open(int64_t obj)
{
    // Close logbook UI if it's currently visible.
    if (logbook_ui_created) {
        logbook_ui_close();
        return;
    }

    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    if (critter_is_dead(player_get_local_pc_obj())) {
        return;
    }

    if (!intgame_mode_set(INTGAME_MODE_MAIN)) {
        return;
    }

    if (!intgame_mode_set(INTGAME_MODE_LOGBOOK)) {
        return;
    }

    logbook_ui_obj = obj;
    logbook_ui_create();
}

/**
 * Closes the logbook window and switches the game back to the main mode.
 *
 * 0x53F090
 */
void logbook_ui_close(void)
{
    if (logbook_ui_created
        && intgame_mode_set(INTGAME_MODE_MAIN)) {
        logbook_ui_destroy();
        logbook_ui_obj = OBJ_HANDLE_NULL;
    }
}

/**
 * Returns `true` if the logbook window is currently open.
 *
 * 0x53F0D0
 */
bool logbook_ui_is_created(void)
{
    return logbook_ui_created;
}

/**
 * Creates the logbook UI window.
 *
 * 0x53F0E0
 */
void logbook_ui_create(void)
{
    int index;
    tig_button_handle_t button_handles[LOGBOOK_UI_TAB_COUNT];
    PcLens pc_lens;

    if (logbook_ui_created) {
        return;
    }

    if (!intgame_big_window_lock_height(logbook_ui_message_filter, &logbook_ui_window,
            settings_get_value(&settings, LOGBOOK_TOOLBAR_KEY) ? 450 : 400)) {
        tig_debug_printf("logbk_ui_create: ERROR: intgame_big_window_lock failed!\n");
        exit(EXIT_SUCCESS); // FIXME: Should be EXIT_FAILURE.
    }

    // Hidden easter egg: holding Left Ctrl + Left Alt while opening logbook
    // switches the "Rumors & Notes" tab into "Bloopers & Quotes" mode.
    if (tig_kb_get_modifier(SDL_KMOD_LCTRL) && tig_kb_get_modifier(SDL_KMOD_LALT)) {
        logbook_ui_index_mode = false;
        logbook_ui_tab = LOGBOOK_UI_TAB_RUMORS_AND_NOTES;
        logbook_ui_current_page = 1;
        logbook_ui_quotes_mode = true;
        gsound_play_sfx(SND_INTERFACE_BLESS, 1);
    }

    ui_panel_background_draw(logbook_ui_window,
        settings_get_value(&settings, LOGBOOK_TOOLBAR_KEY) ? NAME_BOOK_SIDE_XL : 264, 0, 0);

    // Create page-turn buttons. Both start hidden, and will be made visible
    // when there is a need for them.
    for (index = 0; index < LOGBOOK_UI_BUTTON_COUNT; index++) {
        intgame_button_create_ex(logbook_ui_window,
            &(logbook_ui_background_rects[0]),
            &(logbook_ui_page_buttons[index]),
            TIG_BUTTON_HIDDEN | TIG_BUTTON_MOMENTARY);
    }
    intgame_button_create_ex(logbook_ui_window,
        &logbook_ui_background_rects[0], &logbook_ui_index_button,
        TIG_BUTTON_HIDDEN | TIG_BUTTON_MOMENTARY);

    // Create tab buttons and group them as a radio set so exactly one tab is
    // always selected. The buttons start hidden but all will be made visible
    // a moment later, in `logbook_ui_draw_panel`.
    for (index = 0; index < LOGBOOK_UI_TAB_COUNT; index++) {
        intgame_button_create_ex(logbook_ui_window,
            &(logbook_ui_background_rects[0]),
            &(logbook_ui_tab_buttons[index]),
            TIG_BUTTON_HIDDEN | TIG_BUTTON_MOMENTARY);
        button_handles[index] = logbook_ui_tab_buttons[index].button_handle;
    }
    tig_button_radio_group_create(LOGBOOK_UI_TAB_COUNT, button_handles, logbook_ui_tab);

    // Center viewport on the player (so that the lens display proper
    // surroundings).
    location_origin_set(obj_field_int64_get(logbook_ui_obj, OBJ_F_LOCATION));

    // Enable the PC lens.
    pc_lens.window_handle = logbook_ui_window;
    pc_lens.rect = &logbook_ui_pc_lens_rect;
    tig_art_interface_id_create(198, 0, 0, 0, &(pc_lens.art_id));
    intgame_pc_lens_do(PC_LENS_MODE_PASSTHROUGH, &pc_lens);

    // Render the main logbook ui and the initial page content.
    logbook_ui_draw_panel(261, true);

    ui_toggle_primary_button(UI_PRIMARY_BUTTON_LOGBOOK, false);
    gsound_play_sfx(SND_INTERFACE_BOOK_OPEN, 1);

    logbook_ui_created = true;
}

/**
 * Destroys the logbook window and releases all associated resources.
 *
 * 0x53F2F0
 */
void logbook_ui_destroy(void)
{
    ui_search_bar_close(&logbook_ui_search_window);
    if (logbook_ui_search_focused) textedit_ui_unfocus(&logbook_ui_search_edit);
    logbook_ui_search_focused = false;
    if (!logbook_ui_created) {
        return;
    }

    intgame_pc_lens_do(PC_LENS_MODE_NONE, NULL);
    intgame_big_window_unlock();
    logbook_ui_window = TIG_WINDOW_HANDLE_INVALID;
    gsound_play_sfx(SND_INTERFACE_BOOK_CLOSE, 1);
    logbook_ui_created = false;
    logbook_ui_index_hovered = -1;

    // If quotes mode was active, restore the page counter to 1 so the next
    // open (in normal mode) starts from the beginning.
    if (logbook_ui_quotes_mode) {
        logbook_ui_current_page = 1;
    }

    logbook_ui_quotes_mode = false;
}

/**
 * Called when an event occurred.
 *
 * 0x53F350
 */
bool logbook_ui_message_filter(TigMessage* msg)
{
    int index;
    MesFileEntry mes_file_entry;
    UiMessage ui_message;

    if (logbook_ui_search_input(msg)) return true;

    if (msg->type == TIG_MESSAGE_KEYBOARD || msg->type == TIG_MESSAGE_MOUSE) {
        for (int action = 0; action < 4; action++) {
            InputBindingNavigationEvent event = input_binding_navigation_event((InputAction)(INPUT_ACTION_LOGBOOK_PREVIOUS + action), msg);
            if (event == INPUT_NAVIGATION_NONE) continue;
            // Consume both edges and held-button repeats before HUD handling.
            if (event == INPUT_NAVIGATION_CONSUME) return true;
            TigMessage button = { 0 };
            button.type = TIG_MESSAGE_BUTTON;
            if (action < 2) {
                int tab = (logbook_ui_tab + (action == 0 ? -1 : 1) + LOGBOOK_UI_TAB_COUNT) % LOGBOOK_UI_TAB_COUNT;
                button.data.button.button_handle = logbook_ui_tab_buttons[tab].button_handle;
                button.data.button.state = TIG_BUTTON_STATE_PRESSED;
                tig_button_state_change(button.data.button.button_handle, TIG_BUTTON_STATE_PRESSED);
            } else {
                int index = action == 2 ? LOGBOOK_UI_BUTTON_TURN_PAGE_LEFT : LOGBOOK_UI_BUTTON_TURN_PAGE_RIGHT;
                button.data.button.button_handle = logbook_ui_page_buttons[index].button_handle;
                button.data.button.state = TIG_BUTTON_STATE_RELEASED;
            }
            return logbook_ui_message_filter(&button);
        }
    }

    if (msg->type == TIG_MESSAGE_MOUSE) {
        if (logbook_ui_index_mode && logbook_ui_index_subject[0] == '\0') {
            logbook_ui_index_hover(msg->data.mouse.x, msg->data.mouse.y);
            if (msg->data.mouse.event == TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP
                && logbook_ui_index_hovered >= 0) {
                logbook_ui_index_open_quests(logbook_ui_index_hovered);
                return true;
            }
        }
        // Clicking on the PC lens closes logbook UI.
        if (msg->data.mouse.event == TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP
            && intgame_panel_close_lens_check_pt(msg->data.mouse.x, msg->data.mouse.y)) {
            logbook_ui_close();
            return true;
        }
        // Click outside the logbook window and outside both HUD strips
        // dismisses the overlay (hi-res convenience).
        if (msg->data.mouse.event == TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP
            && logbook_ui_window != TIG_WINDOW_HANDLE_INVALID) {
            TigWindowData menu_wd;
            if (tig_window_data(logbook_ui_window, &menu_wd) == TIG_OK
                && intgame_panel_should_dismiss_overlay_click(msg->data.mouse.x, msg->data.mouse.y, &menu_wd.rect)) {
                logbook_ui_close();
                return true;
            }
        }
        return false;
    }

    if (msg->type == TIG_MESSAGE_BUTTON) {
        switch (msg->data.button.state) {
        case TIG_BUTTON_STATE_PRESSED:
            // A tab button was pressed: switch to that tab and reset the page
            // position.
            for (index = 0; index < LOGBOOK_UI_TAB_COUNT; index++) {
                if (logbook_ui_tab_buttons[index].button_handle == msg->data.button.button_handle) {
                    logbook_ui_switch_tab(index, false);
                    return true;
                }
            }
            return false;
        case TIG_BUTTON_STATE_RELEASED:
            if (logbook_ui_index_button.button_handle == msg->data.button.button_handle) {
                if (logbook_ui_tab == LOGBOOK_UI_TAB_RUMORS_AND_NOTES) {
                    logbook_ui_toggle_history();
                } else {
                    logbook_ui_toggle_index();
                }
                return true;
            }
            // Page-turn buttons: navigate one spread forward or backward.
            if (logbook_ui_page_buttons[LOGBOOK_UI_BUTTON_TURN_PAGE_LEFT].button_handle == msg->data.button.button_handle) {
                logbook_ui_turn_page_left();
                return true;
            }
            if (logbook_ui_page_buttons[LOGBOOK_UI_BUTTON_TURN_PAGE_RIGHT].button_handle == msg->data.button.button_handle) {
                logbook_ui_turn_page_right();
                return true;
            }
            return false;
        case TIG_BUTTON_STATE_MOUSE_INSIDE:
            if (logbook_ui_index_button.button_handle == msg->data.button.button_handle) {
                ui_message.type = UI_MSG_TYPE_FEEDBACK;
                ui_message.str = (char*)mes_localized_get("journal_index",
                    logbook_ui_tab == LOGBOOK_UI_TAB_RUMORS_AND_NOTES
                        ? (logbook_ui_history_mode ? 12 : 10)
                        : (logbook_ui_index_subject[0] != '\0' ? 6 : logbook_ui_index_mode ? 4 : 0));
                intgame_message_window_display_msg(&ui_message);
                return true;
            }
            // Show the tab's tooltip in the message window while the cursor
            // hovers over it.
            for (index = 0; index < LOGBOOK_UI_TAB_COUNT; index++) {
                if (logbook_ui_tab_buttons[index].button_handle == msg->data.button.button_handle) {
                    mes_file_entry.num = index;
                    mes_get_msg(logbook_ui_mes_file, &mes_file_entry);

                    ui_message.type = UI_MSG_TYPE_FEEDBACK;
                    ui_message.str = mes_file_entry.str;
                    intgame_message_window_display_msg(&ui_message);

                    return true;
                }
            }
            return false;
        case TIG_BUTTON_STATE_MOUSE_OUTSIDE:
            if (logbook_ui_index_button.button_handle == msg->data.button.button_handle) {
                intgame_message_window_clear();
                return true;
            }
            // Clear the message window when the cursor leaves a tab button.
            for (index = 0; index < LOGBOOK_UI_TAB_COUNT; index++) {
                if (logbook_ui_tab_buttons[index].button_handle == msg->data.button.button_handle) {
                    intgame_message_window_clear();
                    return true;
                }
            }
            return false;
        }
        return false;
    }

    return false;
}

/**
 * Draws the logbook book spread.
 *
 * `art_num` is the art number of the border around "book" image. The only value
 * used is 261 ("book_quest.art"). There are other art graphics, that have
 * slightly different tint.
 *
 * 0x53F490
 */
void logbook_ui_draw_panel(int border_art_num, bool preserve_page)
{
    TigRect src_rect;
    TigRect dst_rect;
    TigArtBlitInfo blit_info;
    tig_button_handle_t selected_button_handle;
    int selected_button_index;
    int index;

    if (logbook_ui_border_art_num != -1) {
        // A border is already drawn. Skip the redraw if the caller is not
        // forcing a refresh and the requested art is the default.
        if (!preserve_page && border_art_num == 261) {
            return;
        }
    } else {
        // The border is not drawn yet.
        border_art_num = 261;
    }

    logbook_ui_border_art_num = border_art_num;

    ui_panel_background_draw(logbook_ui_window,
        settings_get_value(&settings, LOGBOOK_TOOLBAR_KEY) ? NAME_BOOK_MAIN_XL : 260, 165, 0);

    // The page border keeps its original size and top position.
    src_rect = (TigRect){ 0, 0, 800, 400 };
    dst_rect = src_rect;
    blit_info.flags = 0;
    blit_info.src_rect = &src_rect;
    blit_info.dst_rect = &dst_rect;

    // Redraw the specified border around two-page spread.
    tig_art_interface_id_create(logbook_ui_border_art_num, 0, 0, 0, &(blit_info.art_id));
    dst_rect.x = 172;
    dst_rect.y = 23;
    tig_window_blit_art(logbook_ui_window, &blit_info);

    // Reveal all tab buttons (they were hidden at creation time) and determine
    // which one is currently selected so we can restore it after the redraw.
    selected_button_handle = tig_button_radio_selected_peer_get(logbook_ui_tab_buttons[0].button_handle);
    selected_button_index = 0;

    for (index = 0; index < LOGBOOK_UI_TAB_COUNT; index++) {
        tig_button_show(logbook_ui_tab_buttons[index].button_handle);

        // Find the index of selected tab.
        if (selected_button_handle == logbook_ui_tab_buttons[index].button_handle) {
            selected_button_index = index;
        }
    }

    gsound_play_sfx(SND_INTERFACE_BOOK_SWITCH, 1);

    // Load data and render the entries for the selected tab.
    logbook_ui_switch_tab(selected_button_index, preserve_page);
}

/**
 * Switches the active logbook tab to `tab` and refreshes the displayed pages.
 *
 * When `preserve_page` is false the page-spread history is cleared and the
 * display returns to the very first entry of the new tab. When true the
 * existing page position is kept, which is used when reopening the
 * logbook to restore the previously viewed spread.
 *
 * 0x53F5F0
 */
void logbook_ui_switch_tab(int tab, bool preserve_page)
{
    int index;
    logbook_ui_index_hovered = -1;

    if (!preserve_page) {
        logbook_ui_history_mode = false;
        logbook_ui_index_mode = false;
        logbook_ui_index_subject[0] = '\0';
        for (index = 0; index < SDL_arraysize(logbook_ui_page_spread_starts); index++) {
            logbook_ui_page_spread_starts[index] = -1;
        }
        logbook_ui_page_spread_starts[0] = 0;
        logbook_ui_current_page = 1;
        logbook_ui_tab = tab;
    }

    if (logbook_ui_history_mode) logbook_ui_load_history();
    else if (logbook_ui_index_mode && logbook_ui_index_subject[0] != '\0') logbook_ui_load_subject_quests();
    else if (logbook_ui_index_mode) logbook_ui_load_index();
    else logbook_ui_load_data();
    logbook_ui_refresh();
    gsound_play_sfx(SND_INTERFACE_BOOK_PAGE_TURN, 1);
}

/**
 * Advances to the next page spread if one exists and the page-spread history
 * array is not full. Records the starting entry of the new spread so the
 * player can navigate back.
 *
 * 0x53F640
 */
void logbook_ui_turn_page_right(void)
{
    if (logbook_ui_last_visible_entry < logbook_ui_entry_count - 1
        && (logbook_ui_current_page - 1) / 2 < SDL_arraysize(logbook_ui_page_spread_starts) - 1) {
        logbook_ui_current_page += 2;
        logbook_ui_index_hovered = -1;
        logbook_ui_page_spread_starts[(logbook_ui_current_page - 1) / 2] = logbook_ui_last_visible_entry + 1;
        logbook_ui_refresh();
        gsound_play_sfx(SND_INTERFACE_BOOK_PAGE_TURN, 1);
    }
}

/**
 * Retreats to the previous page spread if one exists.
 *
 * 0x53F6A0
 */
void logbook_ui_turn_page_left(void)
{
    if (logbook_ui_page_spread_starts[(logbook_ui_current_page - 1) / 2] > 0) {
        logbook_ui_current_page -= 2;
        logbook_ui_index_hovered = -1;
        logbook_ui_refresh();
        gsound_play_sfx(SND_INTERFACE_BOOK_PAGE_TURN, 1);
    }
}

/**
 * Redraws the entire visible page spread for the current tab and page number.
 *
 * 0x53F6E0
 */
void logbook_ui_refresh(void)
{
    MesFileEntry mes_file_entry;
    int index;
    char buffer[80];
    memset(logbook_ui_index_entry_rects, 0, sizeof(logbook_ui_index_entry_rects));

    // Hide navigation buttons before redrawing. They are re-shown below when
    // appropriate.
    tig_button_hide(logbook_ui_page_buttons[LOGBOOK_UI_BUTTON_TURN_PAGE_LEFT].button_handle);
    tig_button_hide(logbook_ui_page_buttons[LOGBOOK_UI_BUTTON_TURN_PAGE_RIGHT].button_handle);
    tig_button_hide(logbook_ui_index_button.button_handle);

    ui_panel_background_draw(logbook_ui_window,
        settings_get_value(&settings, LOGBOOK_TOOLBAR_KEY) ? NAME_BOOK_MAIN_XL : 260, 165, 0);

    // Render entries onto the page spread.
    if (logbook_ui_entry_count != 0) {
        logbook_ui_last_visible_entry = logbook_ui_draw_page_spread(logbook_ui_page_spread_starts[(logbook_ui_current_page - 1) / 2], logbook_ui_entry_count - 1);
    } else {
        logbook_ui_last_visible_entry = 0;
    }

    // Show nagivation buttons where applicable.
    if (logbook_ui_page_spread_starts[(logbook_ui_current_page - 1) / 2] > 0) {
        tig_button_show(logbook_ui_page_buttons[LOGBOOK_UI_BUTTON_TURN_PAGE_LEFT].button_handle);
    }

    if (logbook_ui_last_visible_entry < logbook_ui_entry_count - 1) {
        tig_button_show(logbook_ui_page_buttons[LOGBOOK_UI_BUTTON_TURN_PAGE_RIGHT].button_handle);
    }

    // Display the tab name in the header of both pages. The "Bloopers & Quotes"
    // easter egg uses mes entry 100 instead of the normal tab index so it can
    // show a distinct heading.
    mes_file_entry.num = logbook_ui_tab == LOGBOOK_UI_TAB_RUMORS_AND_NOTES && logbook_ui_quotes_mode
        ? 100
        : logbook_ui_tab;
    mes_get_msg(logbook_ui_mes_file, &mes_file_entry);

    if (logbook_ui_index_mode && logbook_ui_index_subject[0] == '\0') {
        mes_file_entry.str = (char*)mes_localized_get("journal_index", 0);
    }
    if (logbook_ui_history_mode) mes_file_entry.str = (char*)mes_localized_get("journal_index", 10);
    if (logbook_ui_tab == LOGBOOK_UI_TAB_RUMORS_AND_NOTES
        || (!logbook_ui_history_mode && logbook_ui_tab == LOGBOOK_UI_TAB_QUESTS)) {
        tig_button_show(logbook_ui_index_button.button_handle);
    }

    tig_font_push(logbook_ui_font_header);
    for (index = 0; index < 2; index++) {
        snprintf(buffer, sizeof(buffer), "%c%s%c", '-', mes_file_entry.str, '-');
        tig_window_text_write(logbook_ui_window, buffer, &(logbook_ui_page_header_rects[index]));
    }
    tig_font_pop();

    // Write the page numbers (e.g. "-3-" on the left, "-4-" on the right).
    tig_font_push(logbook_ui_font_page_numbers);
    for (index = 0; index < 2; index++) {
        snprintf(buffer, sizeof(buffer), "%c%d%c", '-', logbook_ui_current_page + index, '-');
        tig_window_text_write(logbook_ui_window, buffer, &(logbook_ui_page_footer_rects[index]));
    }
    tig_font_pop();
    logbook_ui_draw_search_bar();
}

/**
 * Renders entries from `start_entry` up to `max_entry` onto the current page
 * spread, filling the left page first and then overflowing onto the right page.
 * Returns the index of the last entry that was fully rendered.
 *
 * The function renders entries sequentially. When the remaining vertical space
 * on the left page is exhausted it switches to the right page. If an entry
 * still does not fit on the right page (or if both pages are exhausted) the
 * rendering stops.
 *
 * TODO: Review.
 *
 * 0x53F8F0
 */
static int logbook_ui_measure_page_entry(int entry, int page, int remaining,
    bool dry_run, bool first, void* context)
{
    (void)context;
    TigRect rect = logbook_ui_page_content_rects[page];
    rect.y += rect.height - remaining;
    rect.height = remaining;
    return logbook_ui_draw_entry(entry, &rect, dry_run, first);
}

int logbook_ui_draw_page_spread(int start_entry, int max_entry)
{
    int heights[] = { logbook_ui_page_content_rects[0].height, logbook_ui_page_content_rects[1].height };
    return ui_text_paginate_spread(start_entry, max_entry, heights, logbook_ui_measure_page_entry, NULL);
}

/**
 * Formats and renders a single logbook entry at position `index` into `rect`.
 *
 * 0x53F9E0
 */
int logbook_ui_draw_entry(int index, TigRect* rect, bool dry_run, bool can_truncate)
{
    char buffer[MAX_STRING];
    if (logbook_ui_history_mode) {
        SDL_strlcpy(buffer, logbook_ui_history_chunks[index], sizeof(buffer));
        return logbook_ui_draw_search_text(buffer, logbook_ui_font_default, rect, dry_run || logbook_ui_measure_only, can_truncate, true, false);
    }
    if (logbook_ui_index_mode && logbook_ui_index_subject[0] != '\0') {
        if (index == 0 || logbook_ui_entry_ids[index] == -2) {
            snprintf(buffer, sizeof(buffer), "%s", index == 0
                ? logbook_ui_index_subject : mes_localized_get("journal_index", 5));
            return logbook_ui_draw_search_text(buffer, index == 0 ? logbook_ui_font_header : logbook_ui_font_default,
                rect, dry_run, can_truncate, true, true);
        }
    } else if (logbook_ui_index_mode) {
        snprintf(buffer, sizeof(buffer), "%s", logbook_ui_index_entries[index].name);
        tig_font_handle_t font = logbook_ui_index_entries[index].heading ? logbook_ui_font_header
            : index == logbook_ui_index_hovered ? logbook_ui_font_active : logbook_ui_font_default;
        int height = logbook_ui_draw_search_text(buffer, font, rect, dry_run, can_truncate,
            logbook_ui_index_entries[index].heading, logbook_ui_index_entries[index].heading);
        if (!dry_run && height > 0 && logbook_ui_index_entries[index].hoverable) {
            TigFont measure = { 0 };
            measure.str = buffer;
            measure.width = rect->width;
            tig_font_push(font);
            tig_font_measure(&measure);
            tig_font_pop();
            logbook_ui_index_entry_rects[index] = (TigRect){ rect->x, rect->y,
                SDL_min(measure.width, rect->width), SDL_min(height, rect->height) };
        }
        return height;
    }
    bool add_line_spacing = logbook_ui_format_entry(buffer, sizeof(buffer), index);
    return logbook_ui_draw_search_text(buffer, logbook_ui_entry_fonts[index], rect, dry_run, can_truncate, add_line_spacing, false);
}

static bool logbook_ui_format_entry(char* buffer, size_t maxlen, int index)
{
    bool add_line_spacing;

    // Determine whether an extra blank-line gap should follow this entry.
    add_line_spacing = index < logbook_ui_entry_count - 1;
    buffer[0] = '\0';

    switch (logbook_ui_tab) {
    case LOGBOOK_UI_TAB_RUMORS_AND_NOTES:
        logbook_ui_format_rumor(buffer, maxlen, index);
        break;
    case LOGBOOK_UI_TAB_QUESTS:
        logbook_ui_format_quest(buffer, maxlen, index);
        break;
    case LOGBOOK_UI_TAB_REPUTATIONS:
        logbook_ui_format_reputation(buffer, maxlen, index);
        break;
    case LOGBOOK_UI_TAB_BLESSINGS_AND_CURSES:
        logbook_ui_format_blessing_or_curse(buffer, maxlen, index);
        break;
    case LOGBOOK_UI_TAB_KILLS_AND_INJURES:
        logbook_ui_format_kill_or_injury(buffer, maxlen, index);
        // "Kills & Injuries" entries are fixed-layout.
        add_line_spacing = false;
        break;
    case LOGBOOK_UI_TAB_BACKGROUND:
        logbook_ui_format_background(buffer, maxlen, index);
        break;
    case LOGBOOK_UI_TAB_KEYS:
        logbook_ui_format_key(buffer, maxlen, index);
        break;
    }

    return add_line_spacing;
}

static JournalIndexWorkspace logbook_ui_index_workspace;

static int logbook_ui_index_compare(const void* a, const void* b)
{
    return journal_text_compare(((const JournalIndexEntry*)a)->name, ((const JournalIndexEntry*)b)->name);
}

static void logbook_ui_index_candidate(JournalIndexCandidate* candidates, int* count, int description)
{
    // Common descriptions identify generic creatures/roles, not named NPCs.
    if (description < 30000) return;
    const char* name = description_get(description);
    if (name == NULL || name[0] == '\0' || *count >= 8192) return;
    if (!journal_index_add_description(&logbook_ui_index_workspace, description)) return;
    candidates[*count].name = name;
    candidates[*count].description = description;
    candidates[*count].known = logbook_ui_obj == player_get_local_pc_obj() && journal_memory_knows(description);
    candidates[*count].person = true;
    (*count)++;
}

static void logbook_ui_clear_index_cache(void)
{
    for (int i = 0; i < logbook_ui_index_cache_count; i++) {
        FREE((char*)logbook_ui_index_entries[i].name);
    }
    logbook_ui_index_cache_count = 0;
    logbook_ui_index_cache_valid = false;
}

// Indexing consumes text, not page layout. In particular, paginating Background
// measures every successive word prefix and was repeated even on cache hits.
static void logbook_ui_scan_index_tab(uint64_t* key, bool match)
{
    if (key != NULL) ui_cache_key_bytes(key, &logbook_ui_tab, sizeof(logbook_ui_tab));
    if (logbook_ui_tab == LOGBOOK_UI_TAB_BACKGROUND) {
        const char* body = background_description_get_body(background_text_get(logbook_ui_obj));
        if (body == NULL) body = "";
        int count = 1;
        if (key != NULL) {
            ui_cache_key_bytes(key, &count, sizeof(count));
            ui_cache_key_string(key, body);
        }
        if (match) journal_index_match_mentions(&logbook_ui_index_workspace, body);
        return;
    }
    logbook_ui_load_data();
    if (key != NULL) ui_cache_key_bytes(key, &logbook_ui_entry_count, sizeof(logbook_ui_entry_count));
    for (int i = 0; i < logbook_ui_entry_count; i++) {
        char text[MAX_STRING] = { 0 };
        logbook_ui_format_entry(text, sizeof(text), i);
        if (key != NULL) ui_cache_key_string(key, text);
        if (match) journal_index_match_mentions(&logbook_ui_index_workspace, text);
    }
}

static void logbook_ui_load_index(void)
{
    Uint64 perf_start = tig_performance_begin();
    journal_index_begin(&logbook_ui_index_workspace);
    JournalIndexCandidate* candidates = logbook_ui_index_workspace.candidates;
    int count = 0;
    int saved_tab = logbook_ui_tab;
    bool saved_quotes = logbook_ui_quotes_mode;
    int64_t obj;
    int iter;
    mes_file_handle_t people;
    MesFileEntry entry;

    for (int area = 1; area < area_count() && count < 8192; area++) {
        const char* name = area_get_name(area);
        if (name == NULL || name[0] == '\0') continue;
        candidates[count].name = name;
        candidates[count].known = area_is_known(logbook_ui_obj, area);
        count++;
    }

    // Prototypes provide typed names, and instances provide names overridden
    // by module maps. Merely loading an object never makes its name known.
    if (obj_pool_walk_first(&obj, &iter)) {
        do {
            if (obj_field_int32_get(obj, OBJ_F_TYPE) != OBJ_TYPE_NPC) continue;
            int description = obj_field_int32_get(obj, OBJ_F_DESCRIPTION);
            if (logbook_ui_obj == player_get_local_pc_obj()
                && !obj_is_proto(obj) && reaction_met_before(obj, logbook_ui_obj)) {
                journal_memory_remember(description);
            }
            logbook_ui_index_candidate(candidates, &count, description);
        } while (obj_pool_walk_next(&obj, &iter));
    }
    for (int i = 0; i < journal_memory_count(); i++) {
        logbook_ui_index_candidate(candidates, &count, journal_memory_get(i));
    }

    // Optional module catalog identifies NPCs mentioned before their map has
    // been loaded. It contains description IDs, never unrestricted MES text.
    if (mes_load("mes\\journal_people.mes", &people)) {
        if (mes_find_first(people, &entry)) {
            do {
                logbook_ui_index_candidate(candidates, &count, entry.num);
            } while (mes_find_next(people, &entry));
        }
        mes_unload(people);
    }

    // Scan only entries actually visible to this player, across every journal
    // section. Developer quotes and unknown quest/rumor text are excluded.
    logbook_ui_index_mode = false;
    for (int i = 0; i < count; i++) {
        if (logbook_ui_obj == player_get_local_pc_obj()
            && candidates[i].person && journal_memory_knows(candidates[i].description)) candidates[i].known = true;
    }
    logbook_ui_quotes_mode = false;
    uint64_t key = UI_CACHE_KEY_INIT;
    GameLanguage language = gamelib_current_language_get();
    ui_cache_key_bytes(&key, &language, sizeof(language));
    ui_cache_key_bytes(&key, &logbook_ui_obj, sizeof(logbook_ui_obj));
    for (int i = 0; i < count; i++) {
        ui_cache_key_string(&key, candidates[i].name);
        ui_cache_key_bytes(&key, &candidates[i].known, sizeof(candidates[i].known));
        ui_cache_key_bytes(&key, &candidates[i].person, sizeof(candidates[i].person));
        ui_cache_key_bytes(&key, &candidates[i].description, sizeof(candidates[i].description));
    }
    for (int id = 0; id <= 3; id++) ui_cache_key_string(&key, mes_localized_get("journal_index", id));
    // A cold opening has nothing to validate: fingerprint texts during the
    // construction pass instead. Neither pass needs display pagination.
    bool validated_texts = logbook_ui_index_cache_valid;
    for (int tab = 0; validated_texts && tab < LOGBOOK_UI_TAB_COUNT; tab++) {
        logbook_ui_tab = tab;
        logbook_ui_scan_index_tab(&key, false);
    }
    if (logbook_ui_index_cache_valid && logbook_ui_index_cache_key == key) {
        logbook_ui_tab = saved_tab;
        logbook_ui_quotes_mode = saved_quotes;
        logbook_ui_index_mode = true;
        logbook_ui_entry_count = logbook_ui_index_cache_count;
        tig_performance_count(TIG_PERF_INDEX_CACHE_HITS, 1);
        goto index_done;
    }
    logbook_ui_clear_index_cache();
    journal_index_prepare_mentions(&logbook_ui_index_workspace, count);
    for (int tab = 0; tab < LOGBOOK_UI_TAB_COUNT; tab++) {
        logbook_ui_tab = tab;
        logbook_ui_scan_index_tab(validated_texts ? NULL : &key, true);
    }
    logbook_ui_tab = saved_tab;
    logbook_ui_quotes_mode = saved_quotes;
    logbook_ui_index_mode = true;

    logbook_ui_entry_count = 0;
    for (int section = 0; section < 2; section++) {
        int heading = logbook_ui_entry_count++;
        logbook_ui_index_entries[heading] = (JournalIndexEntry){ mes_localized_get("journal_index", section + 1), true };
        int start = logbook_ui_entry_count;
        for (int i = 0; i < count && logbook_ui_entry_count < SDL_arraysize(logbook_ui_index_entries) - 3; i++) {
            if (candidates[i].known && candidates[i].person == (section == 1)) {
                logbook_ui_index_entries[logbook_ui_entry_count++] = (JournalIndexEntry){ candidates[i].name, false, true };
            }
        }
        qsort(&logbook_ui_index_entries[start], logbook_ui_entry_count - start, sizeof(JournalIndexEntry), logbook_ui_index_compare);
        // Multiple NPCs/prototypes may share a name: list that name only once.
        int end = logbook_ui_entry_count;
        logbook_ui_entry_count = start;
        for (int i = start; i < end; i++) {
            if (logbook_ui_entry_count == start
                || journal_text_compare(logbook_ui_index_entries[i].name, logbook_ui_index_entries[logbook_ui_entry_count - 1].name) != 0) {
                logbook_ui_index_entries[logbook_ui_entry_count++] = logbook_ui_index_entries[i];
            }
        }
        if (start == logbook_ui_entry_count) {
            logbook_ui_index_entries[logbook_ui_entry_count++] = (JournalIndexEntry){ mes_localized_get("journal_index", 3), false };
        }
    }
    // Own cached names: object pools and module resources may change between
    // openings. Revalidation checks their current contents before any reuse.
    for (int i = 0; i < logbook_ui_entry_count; i++) {
        logbook_ui_index_entries[i].name = STRDUP(logbook_ui_index_entries[i].name);
    }
    logbook_ui_index_cache_count = logbook_ui_entry_count;
    logbook_ui_index_cache_key = key;
    logbook_ui_index_cache_valid = true;
index_done:
    while (logbook_ui_current_page > 1
        && logbook_ui_page_spread_starts[(logbook_ui_current_page - 1) / 2] >= logbook_ui_entry_count) {
        logbook_ui_current_page -= 2;
    }
    tig_performance_end(TIG_PERF_JOURNAL, perf_start);
}

static void logbook_ui_toggle_index(void)
{
    if (logbook_ui_history_mode || logbook_ui_tab != LOGBOOK_UI_TAB_QUESTS) return;
    logbook_ui_index_hovered = -1;
    if (logbook_ui_index_mode && logbook_ui_index_subject[0] != '\0') {
        logbook_ui_index_subject[0] = '\0';
        logbook_ui_current_page = logbook_ui_index_saved_page;
        memcpy(logbook_ui_page_spread_starts, logbook_ui_index_saved_spreads, sizeof(logbook_ui_index_saved_spreads));
        logbook_ui_load_index();
    } else if (logbook_ui_index_mode) {
        logbook_ui_index_mode = false;
        logbook_ui_current_page = logbook_ui_saved_page;
        memcpy(logbook_ui_page_spread_starts, logbook_ui_saved_spreads, sizeof(logbook_ui_saved_spreads));
        logbook_ui_load_data();
    } else {
        logbook_ui_saved_page = logbook_ui_current_page;
        memcpy(logbook_ui_saved_spreads, logbook_ui_page_spread_starts, sizeof(logbook_ui_saved_spreads));
        memset(logbook_ui_page_spread_starts, -1, sizeof(logbook_ui_page_spread_starts));
        logbook_ui_page_spread_starts[0] = 0;
        logbook_ui_current_page = 1;
        logbook_ui_load_index();
    }
    logbook_ui_refresh();
    gsound_play_sfx(SND_INTERFACE_BOOK_PAGE_TURN, 1);
}

static void logbook_ui_load_subject_quests(void)
{
    // Start with the player's known quests and their original status fonts.
    logbook_ui_load_data();
    int count = logbook_ui_entry_count;
    // Reserve the first entry for the selected subject. Shift once before
    // filtering so adding that heading cannot overwrite the next quest.
    memmove(&logbook_ui_entry_ids[1], logbook_ui_entry_ids, count * sizeof(logbook_ui_entry_ids[0]));
    memmove(&logbook_ui_entry_datetimes[1], logbook_ui_entry_datetimes, count * sizeof(logbook_ui_entry_datetimes[0]));
    memmove(&logbook_ui_quest_states[1], logbook_ui_quest_states, count * sizeof(logbook_ui_quest_states[0]));
    memmove(&logbook_ui_entry_fonts[1], logbook_ui_entry_fonts, count * sizeof(logbook_ui_entry_fonts[0]));
    int out = 1;
    for (int i = 1; i <= count; i++) {
        char text[MAX_STRING] = "";
        quest_copy_description(logbook_ui_obj, logbook_ui_entry_ids[i], text, sizeof(text));
        if (!journal_text_mentions(text, logbook_ui_index_subject)) continue;
        logbook_ui_entry_ids[out] = logbook_ui_entry_ids[i];
        logbook_ui_entry_datetimes[out] = logbook_ui_entry_datetimes[i];
        logbook_ui_quest_states[out] = logbook_ui_quest_states[i];
        logbook_ui_entry_fonts[out++] = logbook_ui_entry_fonts[i];
    }
    logbook_ui_entry_ids[0] = -1;
    if (out == 1) logbook_ui_entry_ids[out++] = -2;
    logbook_ui_entry_count = out;
    while (logbook_ui_current_page > 1
        && logbook_ui_page_spread_starts[(logbook_ui_current_page - 1) / 2] >= out) {
        logbook_ui_current_page -= 2;
    }
}

static void logbook_ui_index_open_quests(int index)
{
    if (index < 0 || index >= logbook_ui_entry_count || !logbook_ui_index_entries[index].hoverable) return;
    SDL_strlcpy(logbook_ui_index_subject, logbook_ui_index_entries[index].name, sizeof(logbook_ui_index_subject));
    logbook_ui_index_saved_page = logbook_ui_current_page;
    memcpy(logbook_ui_index_saved_spreads, logbook_ui_page_spread_starts, sizeof(logbook_ui_index_saved_spreads));
    memset(logbook_ui_page_spread_starts, -1, sizeof(logbook_ui_page_spread_starts));
    logbook_ui_page_spread_starts[0] = 0;
    logbook_ui_current_page = 1;
    logbook_ui_index_hovered = -1;
    logbook_ui_load_subject_quests();
    logbook_ui_refresh();
    gsound_play_sfx(SND_INTERFACE_BOOK_PAGE_TURN, 1);
}

static void logbook_ui_clear_history_chunks(void)
{
    logbook_ui_history_cache_valid = false;
    for (int i = 0; i < logbook_ui_history_chunk_count; i++) {
        FREE(logbook_ui_history_chunks[i]);
        logbook_ui_history_chunks[i] = NULL;
    }
    logbook_ui_history_chunk_count = 0;
}

static void logbook_ui_load_history(void)
{
    uint64_t key = UI_CACHE_KEY_INIT;
    uint64_t revision = conversation_history_revision();
    GameLanguage language = gamelib_current_language_get();
    ui_cache_key_bytes(&key, &revision, sizeof(revision));
    ui_cache_key_bytes(&key, &language, sizeof(language));
    bool toolbar_enabled = settings_get_value(&settings, LOGBOOK_TOOLBAR_KEY) != 0;
    ui_cache_key_bytes(&key, &toolbar_enabled, sizeof(toolbar_enabled));
    ui_cache_key_bytes(&key, &logbook_ui_search_filter, sizeof(logbook_ui_search_filter));
    if (toolbar_enabled && logbook_ui_search_filter) ui_cache_key_string(&key, logbook_ui_search);
    ui_cache_key_bytes(&key, &logbook_ui_font_default, sizeof(logbook_ui_font_default));
    ui_cache_key_bytes(&key, &logbook_ui_page_content_rects, sizeof(logbook_ui_page_content_rects));
    ui_cache_key_bytes(&key, &logbook_ui_line_height, sizeof(logbook_ui_line_height));
    TigFont* body_font = (TigFont*)logbook_ui_font_default;
    ui_cache_key_bytes(&key, &body_font->flags, sizeof(body_font->flags));
    ui_cache_key_bytes(&key, &body_font->art_id, sizeof(body_font->art_id));
    ui_cache_key_bytes(&key, &body_font->scale, sizeof(body_font->scale));
    ui_cache_key_bytes(&key, &body_font->modern_font, sizeof(body_font->modern_font));
    ui_cache_key_bytes(&key, &body_font->modern_font_size, sizeof(body_font->modern_font_size));
    int requested_page = logbook_ui_current_page;
    if (logbook_ui_history_cache_valid && logbook_ui_history_cache_key == key) {
        logbook_ui_entry_count = logbook_ui_history_chunk_count;
        memcpy(logbook_ui_page_spread_starts, logbook_ui_history_layout_spreads, sizeof(logbook_ui_history_layout_spreads));
        logbook_ui_current_page = SDL_min(requested_page, logbook_ui_history_latest_page);
        tig_performance_count(TIG_PERF_HISTORY_CACHE_HITS, 1);
        return;
    }
    Uint64 perf_start = tig_performance_begin();
    logbook_ui_clear_history_chunks();
    tig_font_push(logbook_ui_font_default);
    for (int i = 0; i < conversation_history_count() && logbook_ui_history_chunk_count < 1000; i++) {
        const ConversationHistoryEntry* entry = conversation_history_get(i);
        char date[80];
        DateTime datetime = entry->date;
        datetime_format_datetime(&datetime, date, sizeof(date));
        char prefix[256];
        snprintf(prefix, sizeof(prefix), "%s\n%s\n", date, entry->speaker);
        size_t prefix_length = strlen(prefix);
        size_t offset = 0;
        size_t length = strlen(entry->text);
        while (offset < length && logbook_ui_history_chunk_count < 1000) {
            char buffer[MAX_STRING];
            size_t low = 1;
            size_t high = SDL_min(length - offset, sizeof(buffer) - prefix_length - 1);
            size_t best = 0;
            while (low <= high) {
                size_t span = (low + high) / 2;
                memcpy(buffer, prefix, prefix_length);
                memcpy(buffer + prefix_length, entry->text + offset, span);
                buffer[prefix_length + span] = '\0';
                TigFont font = { 0 };
                font.str = buffer;
                font.width = logbook_ui_page_content_rects[0].width;
                tig_font_measure(&font);
                if (font.height + logbook_ui_line_height <= logbook_ui_page_content_rects[0].height) {
                    best = span; low = span + 1;
                } else high = span - 1;
            }
            if (best == 0) break;
            // Prefer a word boundary when a long message continues on another
            // page; never discard the unrendered remainder of the dialogue.
            if (offset + best < length) {
                size_t boundary = best;
                while (boundary > 1 && entry->text[offset + boundary - 1] != ' '
                    && entry->text[offset + boundary - 1] != '\n') boundary--;
                if (boundary > 1) best = boundary;
            }
            memcpy(buffer, prefix, prefix_length);
            memcpy(buffer + prefix_length, entry->text + offset, best);
            buffer[prefix_length + best] = '\0';
            logbook_ui_history_chunks[logbook_ui_history_chunk_count++] = STRDUP(buffer);
            offset += best;
        }
    }
    tig_font_pop();
    if (logbook_ui_history_chunk_count == 0) {
        logbook_ui_history_chunks[logbook_ui_history_chunk_count++] = STRDUP(mes_localized_get("journal_index", 11));
    }
    logbook_ui_entry_count = logbook_ui_history_chunk_count;
    tig_performance_end(TIG_PERF_JOURNAL, perf_start);
    // Compute the whole pagination once. The retained fragments and page
    // starts serve reopening, returning from another tab, and latest-page entry.
    Uint64 layout_start = tig_performance_begin();
    memset(logbook_ui_page_spread_starts, -1, sizeof(logbook_ui_page_spread_starts));
    logbook_ui_page_spread_starts[0] = 0;
    logbook_ui_current_page = 1;
    logbook_ui_measure_only = true;
    int first = 0;
    while ((logbook_ui_current_page - 1) / 2 < SDL_arraysize(logbook_ui_page_spread_starts) - 1) {
        int last = logbook_ui_draw_page_spread(first, logbook_ui_entry_count - 1);
        if (last < first || last >= logbook_ui_entry_count - 1) break;
        first = last + 1;
        logbook_ui_current_page += 2;
        logbook_ui_page_spread_starts[(logbook_ui_current_page - 1) / 2] = first;
    }
    logbook_ui_measure_only = false;
    logbook_ui_history_latest_page = logbook_ui_current_page;
    memcpy(logbook_ui_history_layout_spreads, logbook_ui_page_spread_starts, sizeof(logbook_ui_history_layout_spreads));
    logbook_ui_current_page = SDL_min(requested_page, logbook_ui_history_latest_page);
    logbook_ui_history_cache_key = key;
    logbook_ui_history_cache_valid = true;
    tig_performance_end(TIG_PERF_JOURNAL_LAYOUT, layout_start);
}

bool logbook_ui_show_view(LogbookView view)
{
    if (!logbook_ui_created || view < LOGBOOK_VIEW_RUMORS || view > LOGBOOK_VIEW_QUEST_INDEX) return false;
    logbook_ui_switch_tab(view == LOGBOOK_VIEW_QUEST_INDEX ? LOGBOOK_UI_TAB_QUESTS : LOGBOOK_UI_TAB_RUMORS_AND_NOTES, false);
    if (view == LOGBOOK_VIEW_CONVERSATIONS) logbook_ui_toggle_history();
    else if (view == LOGBOOK_VIEW_QUEST_INDEX) logbook_ui_toggle_index();
    return true;
}

static void logbook_ui_toggle_history(void)
{
    logbook_ui_index_hovered = -1;
    if (logbook_ui_history_mode) {
        logbook_ui_history_mode = false;
        logbook_ui_current_page = logbook_ui_history_saved_page;
        memcpy(logbook_ui_page_spread_starts, logbook_ui_history_saved_spreads, sizeof(logbook_ui_history_saved_spreads));
        logbook_ui_load_data();
    } else {
        if (logbook_ui_index_mode) {
            logbook_ui_index_mode = false;
            logbook_ui_index_subject[0] = '\0';
            logbook_ui_current_page = logbook_ui_saved_page;
            memcpy(logbook_ui_page_spread_starts, logbook_ui_saved_spreads, sizeof(logbook_ui_saved_spreads));
        }
        logbook_ui_history_saved_page = logbook_ui_current_page;
        memcpy(logbook_ui_history_saved_spreads, logbook_ui_page_spread_starts, sizeof(logbook_ui_history_saved_spreads));
        memset(logbook_ui_page_spread_starts, -1, sizeof(logbook_ui_page_spread_starts));
        logbook_ui_page_spread_starts[0] = 0;
        logbook_ui_current_page = 1;
        logbook_ui_history_mode = true;
        logbook_ui_load_history();
        logbook_ui_current_page = logbook_ui_history_latest_page;
    }
    logbook_ui_refresh();
    gsound_play_sfx(SND_INTERFACE_BOOK_PAGE_TURN, 1);
}

static void logbook_ui_index_hover(int x, int y)
{
    TigWindowData window;
    int hovered = -1;
    if (tig_window_data(logbook_ui_window, &window) == TIG_OK) {
        x -= window.rect.x;
        y -= window.rect.y;
        for (int i = 0; i < logbook_ui_entry_count; i++) {
            const TigRect* rect = &logbook_ui_index_entry_rects[i];
            if (rect->width > 0 && rect->height > 0
                && x >= rect->x && x < rect->x + rect->width
                && y >= rect->y && y < rect->y + rect->height) {
                hovered = i;
                break;
            }
        }
    }
    if (hovered != logbook_ui_index_hovered) {
        logbook_ui_index_hovered = hovered;
        // Restore the page background as well as the text, so the previous
        // hover returns to black without accumulating glyphs.
        logbook_ui_refresh();
    }
}

/**
 * Measures or renders `buffer` using `font` inside `rect`.
 *
 * If the text's measured height exceeds the available height, the behaviour
 * is controlled with `can_truncate`. When truncation is allowed, characters
 * are stripped from the end one at a time until it fits and logs a warning. If
 * truncation is not allowed, the function returns `0` immediately.
 *
 * Returns the height of the rendered (or measured) text. If `add_line_spacing`
 * is `true`, additional line heighgt is added to the return value so the caller
 * reserves space for a blank-line gap after this entry. Returns -1 if the
 * buffer is empty.
 *
 * 0x53FAD0
 */
tig_font_handle_t logbook_ui_toolbar_font(void)
{
    return logbook_ui_font_default;
}

static void logbook_ui_draw_search_bar(void)
{
    if (!settings_get_value(&settings, LOGBOOK_TOOLBAR_KEY)) {
        ui_search_bar_close(&logbook_ui_search_window);
        if (logbook_ui_search_focused) textedit_ui_unfocus(&logbook_ui_search_edit);
        logbook_ui_search_focused = false;
        logbook_ui_search[0] = '\0';
        return;
    }
    if (!ui_search_bar_open(&logbook_ui_search_window, logbook_ui_window,
            (TigRect){ 220, 418, 473, 22 }, logbook_ui_message_filter)) return;
    char text[256];
    snprintf(text, sizeof(text), "%s: %s%s", mes_localized_get("journal_index", 20),
        logbook_ui_search, logbook_ui_search_focused ? "_" : "");
    ui_search_bar_cell(logbook_ui_search_window, logbook_ui_search_cells[0], logbook_ui_font_default, text, false);
    for (int i = 1; i < 3; i++) {
        ui_search_bar_cell(logbook_ui_search_window, logbook_ui_search_cells[i], logbook_ui_font_default,
            mes_localized_get("journal_index", 20 + i), i == 1 ? logbook_ui_search_filter : !logbook_ui_search_filter);
    }
}

static void logbook_ui_search_changed(TextEdit* edit)
{
    (void)edit;
    logbook_ui_current_page = 1;
    logbook_ui_index_hovered = -1;
    memset(logbook_ui_page_spread_starts, -1, sizeof(logbook_ui_page_spread_starts));
    logbook_ui_page_spread_starts[0] = 0;
    // Returning from a subview must not restore pagination from an older query.
    logbook_ui_saved_page = logbook_ui_index_saved_page = logbook_ui_history_saved_page = 1;
    memcpy(logbook_ui_saved_spreads, logbook_ui_page_spread_starts, sizeof(logbook_ui_saved_spreads));
    memcpy(logbook_ui_index_saved_spreads, logbook_ui_page_spread_starts, sizeof(logbook_ui_index_saved_spreads));
    memcpy(logbook_ui_history_saved_spreads, logbook_ui_page_spread_starts, sizeof(logbook_ui_history_saved_spreads));
    if (logbook_ui_history_mode) logbook_ui_load_history();
    logbook_ui_refresh();
}

static void logbook_ui_search_finished(TextEdit* edit)
{
    textedit_ui_unfocus(edit);
    logbook_ui_search_focused = false;
    logbook_ui_refresh();
}

void logbook_ui_search_bar_visible(bool visible)
{
    if (!visible && logbook_ui_search_focused) {
        textedit_ui_unfocus(&logbook_ui_search_edit);
        logbook_ui_search_focused = false;
    }
    if (logbook_ui_search_window != TIG_WINDOW_HANDLE_INVALID) {
        if (visible && logbook_ui_created && settings_get_value(&settings, LOGBOOK_TOOLBAR_KEY)) tig_window_show(logbook_ui_search_window);
        else tig_window_hide(logbook_ui_search_window);
    }
}

static bool logbook_ui_search_input(TigMessage* msg)
{
    if (!settings_get_value(&settings, LOGBOOK_TOOLBAR_KEY)) return false;
    if (logbook_ui_search_focused && (msg->type == TIG_MESSAGE_KEYBOARD || msg->type == TIG_MESSAGE_TEXT_INPUT)) {
        if (msg->type == TIG_MESSAGE_KEYBOARD && msg->data.keyboard.scancode == SDL_SCANCODE_ESCAPE) {
            if (!msg->data.keyboard.pressed) {
                logbook_ui_search[0] = '\0';
                textedit_ui_unfocus(&logbook_ui_search_edit);
                logbook_ui_search_focused = false;
                logbook_ui_search_changed(&logbook_ui_search_edit);
            }
            return true;
        }
        if (msg->type == TIG_MESSAGE_TEXT_INPUT) {
            char text[128];
            language_input_to_legacy(msg->data.text.text, text, sizeof(text), gamelib_current_language_get());
            TigMessage converted = *msg;
            converted.data.text.text = text;
            textedit_ui_process_message(&converted);
        } else textedit_ui_process_message(msg);
        return true;
    }
    if (msg->type == TIG_MESSAGE_KEYBOARD && msg->data.keyboard.scancode == SDL_SCANCODE_F
        && (SDL_GetModState() & SDL_KMOD_CTRL) != 0) {
        if (!msg->data.keyboard.pressed) {
            logbook_ui_search_focused = true;
            textedit_ui_focus(&logbook_ui_search_edit);
            logbook_ui_refresh();
        }
        return true;
    }
    if (msg->type == TIG_MESSAGE_MOUSE) {
        TigWindowData data;
        if (logbook_ui_search_window != TIG_WINDOW_HANDLE_INVALID
            && tig_window_data(logbook_ui_search_window, &data) == TIG_OK) {
            int x = msg->data.mouse.x - data.rect.x;
            int y = msg->data.mouse.y - data.rect.y;
            TigRect bounds = { 0, 0, 473, 22 };
            if (ui_search_bar_contains(&bounds, x, y)) {
                if (msg->data.mouse.event == TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP) {
                    if (ui_search_bar_contains(&logbook_ui_search_cells[0], x, y)) {
                        logbook_ui_search_focused = true;
                        textedit_ui_focus(&logbook_ui_search_edit);
                        logbook_ui_refresh();
                    } else {
                        for (int i = 1; i < 3; i++) {
                            if (ui_search_bar_contains(&logbook_ui_search_cells[i], x, y)) {
                                textedit_ui_unfocus(&logbook_ui_search_edit);
                                logbook_ui_search_focused = false;
                                logbook_ui_search_filter = i == 1;
                                logbook_ui_search_changed(&logbook_ui_search_edit);
                            }
                        }
                    }
                }
                return true;
            }
        }
        if (msg->data.mouse.event == TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP && logbook_ui_search_focused) {
            logbook_ui_search_finished(&logbook_ui_search_edit);
        }
    }
    return false;
}

static int logbook_ui_draw_search_text(char* text, tig_font_handle_t font, TigRect* rect,
    bool dry_run, bool can_truncate, bool spacing, bool heading)
{
    if (!settings_get_value(&settings, LOGBOOK_TOOLBAR_KEY)) {
        return logbook_ui_draw_text(text, font, rect, dry_run, can_truncate, spacing);
    }
    if (!heading && logbook_ui_search_filter && logbook_ui_search[0] != '\0'
        && !language_text_contains(text, logbook_ui_search, gamelib_current_language_get())) return -1;
    int height = logbook_ui_draw_text(text, font, rect, dry_run, can_truncate, spacing);
    if (height > 0 && !dry_run && !logbook_ui_search_filter && logbook_ui_search[0] != '\0') {
        TigTextRange ranges[MAX_STRING / 2];
        int count = 0;
        int length = (int)strlen(logbook_ui_search);
        const char* cursor = text;
        while (count < SDL_arraysize(ranges)
            && (cursor = language_text_find(cursor, logbook_ui_search, gamelib_current_language_get())) != NULL) {
            ranges[count++] = (TigTextRange){ (int)(cursor - text), length };
            cursor += length;
        }
        if (count > 0) {
            tig_font_push(font);
            tig_window_text_underline(logbook_ui_window, text, rect, ranges, count);
            tig_font_pop();
        }
    }
    return height;
}

int logbook_ui_draw_text(char* buffer, tig_font_handle_t font, TigRect* rect, bool dry_run, bool can_truncate, bool add_line_spacing)
{
    TigFont font_info;
    bool warned = false;
    size_t pos;

    if (buffer[0] == '\0') {
        return -1;
    }

    tig_font_push(font);

    font_info.str = buffer;
    font_info.width = rect->width;

    // Measure the text, trimming it character by character if it is too tall
    // and truncation is permitted.
    while (true) {
        tig_font_measure(&font_info);
        if (font_info.height <= rect->height) {
            break;
        }

        if (!can_truncate || (pos = strlen(buffer)) == 0) {
            tig_font_pop();
            return 0;
        }

        if (!warned) {
            tig_debug_printf("Had to shorten logbook entry: %s\n", buffer);
        }

        buffer[pos - 1] = '\0';
        warned = true;
    }

    if (!dry_run) {
        tig_window_text_write(logbook_ui_window, buffer, rect);
    }

    tig_font_pop();

    if (add_line_spacing) {
        // Return height + one blank line so the next entry is visually
        // separated from this one.
        return logbook_ui_line_height + font_info.height;
    }

    return font_info.height;
}

/**
 * Populates the internal arrays for the currently active tab by querying the
 * relevant game system. Also assigns the appropriate display font to each entry
 * based on its state.
 *
 * 0x53FBB0
 */
void logbook_ui_load_data(void)
{
    int index;

    if (logbook_ui_tab == LOGBOOK_UI_TAB_RUMORS_AND_NOTES) {
        RumorLogbookEntry rumors[MAX_RUMORS]; // NOTE: Forces `alloca(72000)`.

        // Retrieve rumors data.
        logbook_ui_entry_count = rumor_get_logbook_data(logbook_ui_obj, rumors);

        if (logbook_ui_quotes_mode) {
            // In the "Bloopers & Quotes" mode, ignore the rumors, and instead
            // load original dev quotes. Entries array is not used.
            logbook_ui_entry_count = mes_num_entries(quotes_mes_file);
            for (index = 0; index < logbook_ui_entry_count; index++) {
                logbook_ui_entry_fonts[index] = logbook_ui_font_default;
            }
        } else {
            for (index = 0; index < logbook_ui_entry_count; index++) {
                logbook_ui_entry_ids[index] = rumors[index].num;
                logbook_ui_entry_datetimes[index] = rumors[index].datetime;

                // Quelled rumors are struck through to indicate they were
                // proven false.
                if (rumors[index].quelled) {
                    logbook_ui_entry_fonts[index] = logbook_ui_font_struck;
                } else {
                    logbook_ui_entry_fonts[index] = logbook_ui_font_default;
                }
            }
        }

        return;
    }

    if (logbook_ui_tab == LOGBOOK_UI_TAB_QUESTS) {
        QuestLogbookEntry quests[2000]; // NOTE: Forces `alloca(48000)`.

        // Retrieve quests data.
        logbook_ui_entry_count = quest_get_logbook_data(logbook_ui_obj, quests);

        for (index = 0; index < logbook_ui_entry_count; index++) {
            logbook_ui_entry_ids[index] = quests[index].num;
            logbook_ui_entry_datetimes[index] = quests[index].datetime;
            logbook_ui_quest_states[index] = quests[index].state;

            // Font reflects the quest's resolution state.
            switch (logbook_ui_quest_states[index]) {
            case QUEST_STATE_COMPLETED:
                logbook_ui_entry_fonts[index] = logbook_ui_font_quest_completed;
                break;
            case QUEST_STATE_OTHER_COMPLETED:
            case QUEST_STATE_BOTCHED:
                logbook_ui_entry_fonts[index] = logbook_ui_font_quest_failed;
                break;
            case QUEST_STATE_ACCEPTED:
            case QUEST_STATE_ACHIEVED:
                logbook_ui_entry_fonts[index] = logbook_ui_font_active;
                break;
            default:
                logbook_ui_entry_fonts[index] = logbook_ui_font_default;
                break;
            }
        }

        return;
    }

    if (logbook_ui_tab == LOGBOOK_UI_TAB_REPUTATIONS) {
        ReputationLogbookEntry reps[2000]; // NOTE: Forces `alloca(32000)`.

        // Retrieve reputations data.
        logbook_ui_entry_count = reputation_get_logbook_data(logbook_ui_obj, reps);

        for (index = 0; index < logbook_ui_entry_count; index++) {
            logbook_ui_entry_ids[index] = reps[index].reputation;
            logbook_ui_entry_datetimes[index] = reps[index].datetime;

            logbook_ui_entry_fonts[index] = logbook_ui_font_default;
        }

        return;
    }

    if (logbook_ui_tab == LOGBOOK_UI_TAB_BLESSINGS_AND_CURSES) {
        CurseLogbookEntry curses[512];
        BlessLogbookEntry blessings[512];
        int num_blessings;
        int num_curses;
        int bless_idx;
        int curse_idx;
        int idx;

        // Retrieve blessings and curses data.
        num_blessings = bless_get_logbook_data(logbook_ui_obj, blessings);
        num_curses = curse_get_logbook_data(logbook_ui_obj, curses);
        logbook_ui_entry_count = num_blessings + num_curses;

        // Merge the two sets in chronological order (earliest first).
        bless_idx = 0;
        curse_idx = 0;
        for (idx = 0; idx < logbook_ui_entry_count; idx++) {
            if (bless_idx < num_blessings
                && (curse_idx == num_curses || datetime_compare(&(blessings[bless_idx].datetime), &(curses[curse_idx].datetime)))) {
                logbook_ui_entry_ids[idx] = blessings[bless_idx].id;
                logbook_ui_entry_datetimes[idx] = blessings[bless_idx].datetime;
                logbook_ui_entry_fonts[idx] = logbook_ui_font_active;
                bless_idx++;
            } else {
                logbook_ui_entry_ids[idx] = curses[curse_idx].id;
                logbook_ui_entry_datetimes[idx] = curses[curse_idx].datetime;
                logbook_ui_entry_fonts[idx] = logbook_ui_font_curse;
                curse_idx++;
            }
        }

        return;
    }

    if (logbook_ui_tab == LOGBOOK_UI_TAB_KILLS_AND_INJURES) {
        int desc;
        int injury;
        unsigned int flags;
        int cnt;

        // Retrieve kill statistics.
        logbook_get_kills(logbook_ui_obj, logbook_ui_kill_stats);

        // Kill stats use default fonts.
        for (index = 0; index < 9; index++) {
            logbook_ui_entry_fonts[index] = logbook_ui_font_default;
        }

        logbook_ui_entry_count = 9;

        // Add injury history:
        //  - Entry ID is the name of critter who inflicted the injury
        //  - The milliseconds of time data is the type of injury
        //
        // All injuries starts striked out (see below).
        index = logbook_find_first_injury(logbook_ui_obj, &desc, &injury);
        while (index != 0) {
            logbook_ui_entry_ids[logbook_ui_entry_count] = desc;
            logbook_ui_entry_datetimes[logbook_ui_entry_count].milliseconds = injury;
            logbook_ui_entry_fonts[logbook_ui_entry_count] = logbook_ui_font_struck;
            logbook_ui_entry_count++;
            index = logbook_find_next_injury(logbook_ui_obj, index, &desc, &injury);
        }

        // For any injury type that is currently active on the critter, change
        // the font of the most recent matching entry from struck-through back
        // to the default (i.e. remove the strikethrough for an injury that has
        // not yet healed).
        flags = obj_field_int32_get(logbook_ui_obj, OBJ_F_CRITTER_FLAGS);

        if ((flags & OCF_BLINDED) != 0) {
            for (index = logbook_ui_entry_count - 1; index >= 9; index--) {
                if (logbook_ui_entry_datetimes[index].milliseconds == LBI_BLINDED) {
                    logbook_ui_entry_fonts[index] = logbook_ui_font_default;
                    break;
                }
            }
        }

        if ((flags & OCF_CRIPPLED_LEGS_BOTH) != 0) {
            for (index = logbook_ui_entry_count - 1; index >= 9; index--) {
                if (logbook_ui_entry_datetimes[index].milliseconds == LBI_CRIPPLED_LEG) {
                    logbook_ui_entry_fonts[index] = logbook_ui_font_default;
                    break;
                }
            }
        }

        if ((flags & OCF_CRIPPLED_ARMS_BOTH) != 0) {
            for (index = logbook_ui_entry_count - 1; index >= 9; index--) {
                if (logbook_ui_entry_datetimes[index].milliseconds == LBI_CRIPPLED_ARM) {
                    logbook_ui_entry_fonts[index] = logbook_ui_font_default;
                    break;
                }
            }
        }

        if ((flags & OCF_CRIPPLED_ARMS_ONE) != 0) {
            for (index = logbook_ui_entry_count - 1; index >= 9; index--) {
                if (logbook_ui_entry_datetimes[index].milliseconds == LBI_CRIPPLED_ARM) {
                    logbook_ui_entry_fonts[index] = logbook_ui_font_default;
                    break;
                }
            }
        }

        // Special case - scarring is also in the logbook ego array, but it is
        // managed as effect, rather than being stored in the flags. Actual
        // count of scarring effects does not matter.
        cnt = effect_count_effects_of_type(logbook_ui_obj, EFFECT_SCARRING);
        if (cnt > 0) {
            for (index = logbook_ui_entry_count - 1; index >= 9; index--) {
                if (logbook_ui_entry_datetimes[index].milliseconds == LBI_SCARRED) {
                    logbook_ui_entry_fonts[index] = logbook_ui_font_default;
                    break;
                }
            }
        }

        return;
    }

    if (logbook_ui_tab == LOGBOOK_UI_TAB_BACKGROUND) {
        char str[MAX_STRING];
        char* curr;
        size_t pos;
        size_t remaining_len;
        size_t truncate_pos;
        size_t prev_truncate_pos;
        TigFont font_desc;
        char ch;

        logbook_ui_entry_count = 1;

        // Special case - entry at index 0 is the number of background, font at
        // index 0 will be used for rendering all background text.
        logbook_ui_entry_ids[0] = background_text_get(logbook_ui_obj);
        logbook_ui_entry_fonts[0] = logbook_ui_font_default;

        strcpy(str, background_description_get_body(logbook_ui_entry_ids[0]));
        remaining_len = strlen(str);

        // Measure how many characters fit on one page and split the text at
        // word boundaries until the remainder fits.
        tig_font_push(logbook_ui_entry_fonts[0]);
        font_desc.str = str;
        font_desc.width = logbook_ui_page_content_rects[0].width;
        tig_font_measure(&font_desc);

        prev_truncate_pos = 0;
        curr = str;
        while (font_desc.height > logbook_ui_page_content_rects[0].height) {
            truncate_pos = 0;

            // Scan forward to find the last word boundary that still fits on
            // the page.
            for (pos = 0; pos < remaining_len; pos++) {
                ch = curr[pos];
                if (ch == ' ' || ch == '\n') {
                    curr[pos] = '\0';
                    tig_font_measure(&font_desc);
                    curr[pos] = ch;

                    if (font_desc.height > logbook_ui_page_content_rects[0].height) {
                        break;
                    }

                    truncate_pos = pos + 1;
                }
            }

            // If no word boundary was found (single word longer than a page)
            // stop splitting. The oversized text will be rendered with whatever
            // clipping the rendering system provides.
            if (truncate_pos == 0) {
                break;
            }

            prev_truncate_pos += truncate_pos;

            // Record this page split: store the character offset of the new
            // page's start. The font is the same for all background sub-pages
            // (stored at font index 0).
            logbook_ui_entry_fonts[logbook_ui_entry_count] = logbook_ui_entry_fonts[0];
            logbook_ui_entry_ids[logbook_ui_entry_count] = (int)prev_truncate_pos;
            logbook_ui_entry_count++;

            // Advance curr to the start of the next page and re-measure.
            remaining_len -= truncate_pos;
            font_desc.str = &(curr[truncate_pos]);
            tig_font_measure(&font_desc);
        }

        // Write the one-past-end sentinel so
        // `logbook_ui_format_background_page` can determine the length of the
        // final page's text.
        logbook_ui_entry_ids[logbook_ui_entry_count] = (int)(prev_truncate_pos + remaining_len);
        tig_font_pop();

        return;
    }

    if (logbook_ui_tab == LOGBOOK_UI_TAB_KEYS) {
        // Retrieve keys in possession.
        logbook_ui_entry_count = item_get_keys(logbook_ui_obj, logbook_ui_entry_ids);
        if (logbook_ui_entry_count > 0) {
            for (index = 0; index < logbook_ui_entry_count; index++) {
                logbook_ui_entry_fonts[index] = logbook_ui_font_default;
            }
        }
        return;
    }
}

/**
 * Formats a single "Rumors & Notes" entry into `buffer`.
 *
 * 0x540310
 */
void logbook_ui_format_rumor(char* buffer, size_t maxlen, int index)
{
    MesFileEntry mes_file_entry;
    size_t pos;

    if (logbook_ui_quotes_mode) {
        // Special case - in "Bloopers & Quotes" mode, the entries array is
        // not used used. The index itself is a number of message in
        // `quotes.mes` file.
        mes_file_entry.num = index;
        if (mes_search(quotes_mes_file, &mes_file_entry)) {
            strcpy(buffer, mes_file_entry.str);
        } else {
            buffer[0] = '\0';
        }
    } else {
        // Start with timestamp.
        logbook_ui_format_timestamp(buffer, maxlen, index);

        // Jump to next line.
        pos = strlen(buffer);
        buffer[pos++] = '\n';

        rumor_copy_logbook_str(logbook_ui_obj, logbook_ui_entry_ids[index], &(buffer[pos]), maxlen - pos);
    }
}

/**
 * Formats the date/time header for the entry at `index` into `buffer`.
 *
 * 0x5403C0
 */
void logbook_ui_format_timestamp(char* buffer, size_t maxlen, int index)
{
    DateTime* datetime;
    int year;
    int month;
    int day;
    int hour;
    int minute;
    char am_pm;
    MesFileEntry mes_file_entry;

    datetime = &(logbook_ui_entry_datetimes[index]);
    month = datetime_get_month(datetime);
    day = datetime_get_day(datetime);
    year = datetime_get_year(datetime);
    hour = datetime_get_hour(datetime);
    minute = datetime_get_minute(datetime);

    // Convert 24-hour time to 12-hour with AM/PM indicator.
    if (hour < 12) {
        am_pm = 'a';
        if (hour == 0) {
            hour = 12;
        }
    } else {
        am_pm = 'p';
        if (hour > 12) {
            hour -= 12;
        }
    }

    // Retrieve localized month name.
    mes_file_entry.num = month + 6;
    mes_get_msg(logbook_ui_mes_file, &mes_file_entry);

    snprintf(buffer, maxlen,
        "%s %d, %d  %d:%.2d%cm    ",
        mes_file_entry.str,
        day,
        year,
        hour,
        minute,
        am_pm);
}

/**
 * Formats a single "Quests" entry into `buffer`.
 *
 * 0x540470
 */
void logbook_ui_format_quest(char* buffer, size_t maxlen, int index)
{
    MesFileEntry mes_file_entry;
    size_t pos;

    // Start with timestamp.
    logbook_ui_format_timestamp(buffer, maxlen, index);

    // Retrieve and append quest state.
    mes_file_entry.num = logbook_ui_quest_states[index] + 19;
    mes_get_msg(logbook_ui_mes_file, &mes_file_entry);
    SDL_strlcat(buffer, mes_file_entry.str, maxlen);

    // Jump to next line.
    pos = strlen(buffer);
    buffer[pos++] = '\n';

    quest_copy_description(logbook_ui_obj, logbook_ui_entry_ids[index], &(buffer[pos]), maxlen - pos);
}

/**
 * Formats a single "Reputations" entry into `buffer`.
 *
 * 0x540510
 */
void logbook_ui_format_reputation(char* buffer, size_t maxlen, int index)
{
    size_t pos;

    // Start with timestamp.
    logbook_ui_format_timestamp(buffer, maxlen, index);

    // Jump to next line.
    pos = strlen(buffer);
    buffer[pos++] = '\n';

    reputation_name(logbook_ui_entry_ids[index], &(buffer[pos]), maxlen - pos);
}

/**
 * Formats a single "Blessings & Curses" entry into `buffer`.
 *
 * 0x540550
 */
void logbook_ui_format_blessing_or_curse(char* buffer, size_t maxlen, int index)
{
    size_t pos;

    // Start with timestamp.
    logbook_ui_format_timestamp(buffer, maxlen, index);

    // Jump to next line.
    pos = strlen(buffer);
    buffer[pos++] = '\n';

    // Special case - blessing vs. curses are determined using font.
    if (logbook_ui_entry_fonts[index] == logbook_ui_font_active) {
        bless_copy_name(logbook_ui_entry_ids[index], &(buffer[pos]), maxlen - pos);
    } else {
        curse_copy_name(logbook_ui_entry_ids[index], &(buffer[pos]), maxlen - pos);
    }
}

/**
 * Formats a single "Kills & Injuries" entry into `buffer`.
 *
 * 0x5405C0
 */
void logbook_ui_format_kill_or_injury(char* buffer, size_t maxlen, int index)
{
    MesFileEntry mes_file_entry;
    const char* desc_str;
    char tmp[80];
    int desc;

    if (index < 7) {
        // Retrieve stat label.
        mes_file_entry.num = 26 + index;
        mes_get_msg(logbook_ui_mes_file, &mes_file_entry);

        // NOTE: Original code is slightly different but does the same thing.
        if (index == 0) {
            // Index 0 - "Total kills".
            SDL_itoa(logbook_ui_kill_stats[LBK_TOTAL_KILLS], tmp, 10);
            snprintf(buffer, maxlen, "%s: %s", mes_file_entry.str, tmp);
        } else {
            // Indices 1-6: name of tehe most notable critter in each category.
            switch (index) {
            case 1:
                desc = logbook_ui_kill_stats[LBK_MOST_POWERFUL_NAME];
                break;
            case 2:
                desc = logbook_ui_kill_stats[LBK_LEAST_POWERFUL_NAME];
                break;
            case 3:
                desc = logbook_ui_kill_stats[LBK_MOST_GOOD_NAME];
                break;
            case 4:
                desc = logbook_ui_kill_stats[LBK_MOST_EVIL_NAME];
                break;
            case 5:
                desc = logbook_ui_kill_stats[LBK_MOST_MAGICAL_NAME];
                break;
            case 6:
                desc = logbook_ui_kill_stats[LBK_MOST_TECH_NAME];
                break;
            default:
                // Should be unreachable.
                assert(0);
            }

            if (desc > 0) {
                desc_str = description_get(desc);
            } else {
                desc_str = NULL;
            }

            if (desc_str != NULL) {
                snprintf(buffer, maxlen, "%s: %s", mes_file_entry.str, desc_str);
            } else {
                // No critter for this category yet.
                snprintf(buffer, maxlen, "%s: -----", mes_file_entry.str);
            }
        }
    } else if (index == 7) {
        // Separator.
        buffer[0] = '\0';
    } else if (index == 8) {
        // "Injury History"
        mes_file_entry.num = 33;
        mes_get_msg(logbook_ui_mes_file, &mes_file_entry);
        SDL_strlcpy(buffer, mes_file_entry.str, maxlen);
    } else {
        // Special case - time array used to store injury type.
        mes_file_entry.num = 34 + logbook_ui_entry_datetimes[index].milliseconds;
        mes_get_msg(logbook_ui_mes_file, &mes_file_entry);

        // Entries array stores name of critter who inflicted this injury.
        desc_str = description_get(logbook_ui_entry_ids[index]);
        if (desc_str != NULL) {
            snprintf(buffer, maxlen, "%s %s", mes_file_entry.str, desc_str);
        }
    }
}

/**
 * Formats one paginated sub-page of the "Background" tab text into `buffer`.
 *
 * 0x540760
 */
void logbook_ui_format_background(char* buffer, size_t maxlen, int index)
{
    const char* body;
    int start;
    int end;

    // Entry at index 0 is special - it contains background number.
    body = background_description_get_body(logbook_ui_entry_ids[0]);

    if (index != 0) {
        // All other entries are position indices where appropriate sub-page
        // starts.
        start = logbook_ui_entry_ids[index];
    } else {
        // The first sub-page always starts at the beginning.
        start = 0;
    }

    // End of sub-page is start of next page.
    end = logbook_ui_entry_ids[index + 1] - start;

    strncpy(buffer, &(body[start]), end);
    buffer[end] = '\0';
}

/**
 * Formats a single "Keyring Contents" entry into `buffer` by copying the key's
 * name.
 *
 * 0x5407B0
 */
void logbook_ui_format_key(char* buffer, size_t maxlen, int index)
{
    strcpy(buffer, key_description_get(logbook_ui_entry_ids[index]));
}

/**
 * Debug/QA utility that iterates over every possible rumors and attempts to lay
 * out both the "normal" and "dumb" variant of each rumor using the current page
 * geometry. Any rumor that would overflow the page triggers a truncation
 * warning to console.
 *
 * This function is not called during normal gameplay. The command line switch
 * `-logcheck` actives when the game is about to start (see `main`).
 *
 * 0x5407F0
 */
void logbook_ui_check(void)
{
    int index;
    char buffer[MAX_STRING];
    size_t pos;
    TigRect rect;

    // Retrieve current in-game time.
    logbook_ui_entry_datetimes[0] = game_time_get();

    // Note that the maximum number of rumors is 2000, but this check was
    // probably either future-proof, or was completed before rumor limit was
    // introduced.
    for (index = 0; index < 3000; index++) {
        // Checking normal intelligence.
        rect = logbook_ui_page_content_rects[0];

        logbook_ui_format_timestamp(buffer, sizeof(buffer), 0);
        pos = strlen(buffer);
        buffer[pos++] = '\n';

        rumor_copy_logbook_normal_str(index, &(buffer[pos]), sizeof(buffer) - pos);
        if (buffer[pos] != '\0') {
            tig_debug_printf("Checking rumor %d\n", index);
            logbook_ui_draw_text(buffer, logbook_ui_font_default, &rect, true, true, true);
        }

        // Checking low intelligence.
        rect = logbook_ui_page_content_rects[0];

        logbook_ui_format_timestamp(buffer, sizeof(buffer), 0);
        pos = strlen(buffer);
        buffer[pos++] = '\n';

        rumor_copy_logbook_dumb_str(index, &(buffer[pos]), sizeof(buffer) - pos);
        if (buffer[pos] != '\0') {
            tig_debug_printf("Checking dumb rumor %d\n", index);
            logbook_ui_draw_text(buffer, logbook_ui_font_default, &rect, true, true, true);
        }
    }
}

static tig_color_t logbook_ui_color(const char* key, int default_red, int default_green, int default_blue)
{
    const char* str;
    int red;
    int green;
    int blue;

    str = settings_get_str_value(&settings, key);
    if (str != NULL && logbook_ui_parse_color(str, &red, &green, &blue)) {
        return tig_color_make(red, green, blue);
    }

    return tig_color_make(default_red, default_green, default_blue);
}

static bool logbook_ui_parse_color(const char* str, int* red, int* green, int* blue)
{
    unsigned int value;
    int r;
    int g;
    int b;

    if (str == NULL) {
        return false;
    }

    if (str[0] == '#') {
        str++;
    } else if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2;
    }

    if (SDL_strlen(str) == 6 && sscanf(str, "%x", &value) == 1) {
        *red = (value >> 16) & 0xFF;
        *green = (value >> 8) & 0xFF;
        *blue = value & 0xFF;
        return true;
    }

    if (sscanf(str, "%d,%d,%d", &r, &g, &b) == 3
        && r >= 0 && r <= 255
        && g >= 0 && g <= 255
        && b >= 0 && b <= 255) {
        *red = r;
        *green = g;
        *blue = b;
        return true;
    }

    return false;
}
