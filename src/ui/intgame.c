#include "ui/intgame.h"

#include <stdio.h>

#include "ui/item_compare.h"
#include "ui/movement_preview.h"
#include "game/modern_font.h"

#include "game/dialog_camera.h"
#include "game/iso_zoom.h"
#include "game/location.h"
#include "game/ai.h"
#include "game/anim.h"
#include "game/camera_follow.h"
#include "game/bless.h"
#include "game/broadcast.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/curse.h"
#include "game/damage_type.h"
#include "game/descriptions.h"
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/hrp.h"
#include "game/input_bindings.h"
#include "game/item.h"
#include "game/item_effect.h"
#include "game/level.h"
#include "game/light.h"
#include "game/light_scheme.h"
#include "game/magictech.h"
#include "game/map.h"
#include "game/mt_item.h"
#include "game/obj.h"
#include "game/obj_private.h"
#include "game/object.h"
#include "game/player.h"
#include "game/portrait.h"
#include "game/proto.h"
#include "game/reaction.h"
#include "game/resistance.h"
#include "game/scroll.h"
#include "game/sfx.h"
#include "game/skill.h"
#include "game/snd.h"
#include "game/spell.h"
#include "game/stat.h"
#include "game/target.h"
#include "game/tc.h"
#include "game/tech.h"
#include "game/timeevent.h"
#include "ui/anim_ui.h"
#include "ui/broadcast_ui.h"
#include "ui/charedit_ui.h"
#include "ui/compact_ui.h"
#include "ui/dialog_ui.h"
#include "ui/fate_ui.h"
#include "ui/follower_ui.h"
#include "ui/gameuilib.h"
#include "ui/hotkey_ui.h"
#include "ui/inven_ui.h"
#include "ui/iso.h"
#include "ui/item_ui.h"
#include "ui/logbook_ui.h"
#include "ui/mainmenu_ui.h"
#include "ui/roller_ui.h"
#include "ui/schematic_ui.h"
#include "ui/search_bar.h"
#include "ui/status_panel.h"
#include "ui/scrollbar_ui.h"
#include "ui/skill_ui.h"
#include "ui/sleep_ui.h"
#include "ui/spell_ui.h"
#include "ui/tb_ui.h"
#include "ui/textedit_ui.h"
#include "ui/wmap_ui.h"
#include "ui/written_ui.h"

#define MAX_INTERFACE_WINDOW_ROTATION_STEPS 6

#define MAX_MESSAGE_HISTORY_ITEMS 10
#define MAX_MESSAGE_HISTORY_STRING_SIZE 200

typedef enum IntgameCounter {
    INTGAME_COUNTER_HEALTH,
    INTGAME_COUNTER_FATIGUE,
    INTGAME_COUNTER_FATE,
    INTGAME_COUNTER_MONEY,
    INTGAME_COUNTER_MANA,
    INTGAME_COUNTER_POISON,
    INTGAME_COUNTER_COUNT,
} IntgameCounter;

typedef enum IntgamePrimaryButton {
    INTGAME_PRIMARY_BUTTON_LOGBOOK,
    INTGAME_PRIMARY_BUTTON_CHAR,
    INTGAME_PRIMARY_BUTTON_INVENTORY,
    INTGAME_PRIMARY_BUTTON_MAP,
    INTGAME_PRIMARY_BUTTON_FATE,
    INTGAME_PRIMARY_BUTTON_COUNT,
} IntgamePrimaryButton;

typedef enum IntgameSecondaryButton {
    INTGAME_SECONDARY_BUTTON_SKILLS,
    INTGAME_SECONDARY_BUTTON_SPELLS,
    INTGAME_SECONDARY_BUTTON_COMBAT,
    INTGAME_SECONDARY_BUTTON_SCHEMATICS,
    INTGAME_SECONDARY_BUTTON_COUNT,
} IntgameSecondaryButton;

typedef enum IntgameQuantityButton {
    INTGAME_QUANTITY_BUTTON_TAKE_ALL,
    INTGAME_QUANTITY_BUTTON_PLUS,
    INTGAME_QUANTITY_BUTTON_MINUS,
    INTGAME_QUANTITY_BUTTON_OK,
    INTGAME_QUANTITY_BUTTON_CANCEL,
    INTGAME_QUANTITY_BUTTON_COUNT,
} IntgameQuantityButton;

typedef enum IntgamePenalty {
    INTGAME_PENALTY_MSR,
    INTGAME_PENALTY_RANGE,
    INTGAME_PENALTY_PERCEPTION,
    INTGAME_PENALTY_COVER,
    INTGAME_PENALTY_LIGHT,
    INTGAME_PENALTY_INJURY,
    INTGAME_PENALTY_BLOCKED_SHOT,
    INTGAME_PENALTY_MAGIC_TECH,
    INTGAME_PENALTY_COUNT,
} IntgamePenalty;

#define INTGAME_PENALTY_SLOTS 6

typedef struct IntgameIsoWindowTypeInfo {
    /* 0000 */ TigRect rect;
    /* 0010 */ tig_window_handle_t window_handle;
} IntgameIsoWindowTypeInfo;

#define MSG_TEXT_HALIGN_LEFT 0x01u
#define MSG_TEXT_HALIGN_RIGHT 0x02u
#define MSG_TEXT_HALIGN_CENTER 0x04u
#define MSG_TEXT_VALIGN_CENTER 0x08u
#define MSG_TEXT_SECONDARY 0x10u
#define MSG_TEXT_TRUNCATE 0x20u

static bool button_create_flags(UiButtonInfo* button_info, unsigned int flags);
static bool button_create_no_art(UiButtonInfo* button_info, int width, int height);
static void intgame_draw_counter(int counter, int value, int digits);
static void intgame_draw_bar_rect(TigRect* rect);
static void intgame_ammo_icon_refresh(tig_art_id_t art_id);
static bool iso_interface_message_filter(TigMessage* msg);
static void intgame_secondary_button_toggle(IntgameSecondaryButton btn, RotatingWindowType window_type);
static bool handle_button_unhover(TigMessage* msg);
static bool intgame_handle_center_camera_binding(TigMessage* msg);
static bool intgame_handle_follower_binding(TigMessage* msg);
static void intgame_center_on_player(void);
static void intgame_combat_mode_toggle(void);
static void intgame_pending_object_examine(void);
static void intgame_main_target_action_process(TargetDescriptor* td);
static void intgame_hotkey_item_equip(int64_t critter_obj, Hotkey* hotkey, int inventory_location);
static bool intgame_action_buttons_create(void);
static bool intgame_rotating_window_frame_draw(RotatingWindowType window_type, int progress);
static void iso_interface_window_disable(RotatingWindowType window_type);
static void intgame_message_window_write_text_centered(char* str, TigRect* rect);
static bool intgame_message_window_write_text(tig_window_handle_t window_handle, char* str, TigRect* rect, tig_font_handle_t font, unsigned int flags);
static bool intgame_spells_init(void);
static void intgame_spells_show_college_spells(int group);
static void intgame_spells_hide_college_spells(int group);
static bool intgame_mt_spells_init(void);
static void intgame_mt_spells_disable(void);
static void iso_interface_window_enable(RotatingWindowType window_type);
static void intgame_mt_spells_enable(void);
static int find_interface_window_index(int x, int y);
static void intgame_hover_refresh_from_mouse(void);
static bool intgame_adjust_mouse_for_zoom(int x, int y, int* adj_x, int* adj_y);
static void intgame_action_tooltips_reset(void);
static bool intgame_point_is_over_window(int x, int y);
static void intgame_hover_refresh_from_message(TigMessage* msg);
static void intgame_center_on_object(int64_t obj);
static void intgame_force_fullscreen(void);
static void intgame_unforce_fullscreen(void);
static void intgame_target_flags_refresh(void);
static bool intgame_target_pick_at_mouse(int x, int y, TargetDescriptor* td);
static void intgame_rotating_window_step(RotatingWindowType window_type, int step);
static void iso_interface_window_swap(RotatingWindowType window_type);
static void intgame_hud_apply_clips(void);
// CE: MINI-peek press handler. Returns true if the press should be
// swallowed (peek-expand to MEDIUM, or return-from-peek to MINI),
// false if it should fall through to normal rotwin toggle-off.
static bool intgame_hud_handle_mini_peek_press(void);
// CE: re-establish MINI invariants (stage=MINI, type=SKILLS, button
// state synced) regardless of how we got here.
static void intgame_hud_enter_mini_with_skills(void);
// CE: true when the HUD is in the MINI crop stage. Used by code that
// runs ahead of where intgame_hud_stage is declared.
static bool intgame_hud_in_mini_stage(void);
static void intgame_clock_refresh(void);
static void intgame_character_sheet_open(int64_t obj, ChareditMode mode);
static void intgame_message_history_append(UiMessage* ui_message);
static void intgame_message_history_scroll_up(void);
static void intgame_message_history_scroll_down(void);
static void intgame_message_refresh(bool play_sound);
static void intgame_message_draw(tig_window_handle_t window_handle, UiMessage* ui_message, bool play_sound);
static void intgame_spell_maintain_art_set_func(UiButtonInfo* button, int slot, tig_art_id_t art_id, tig_window_handle_t window_handle);
static void intgame_spell_maintain_refresh_func(tig_button_handle_t button_handle, UiButtonInfo* info, int slot, bool active, tig_window_handle_t window_handle);
static void intgame_refresh_quantity(void);
static void intgame_object_hover_process(TigMessage* msg);
static void intgame_examine_critter(int64_t pc_obj, int64_t critter_obj, char* str);
static void intgame_message_window_draw_image(tig_window_handle_t window_handle, int num);
static void intgame_message_percentage_bar_draw(int background_art_number, int foreground_art_number, TigRect* rect, int value);
static void intgame_weapon_skill_summary_draw(int64_t source_obj, int64_t target_obj);
static void intgame_message_icon_draw(tig_window_handle_t window_handle, int art_num, int x, int y);
static int intgame_item_icon_get(int64_t item_obj);
static void intgame_examine_item(int64_t pc_obj, int64_t item_obj, char* str);
static void append_stat(char* buffer, size_t maxlen, int num, int min, int max, int adj, bool is_modifier, ItemCompareStats* stats);
static void format_weapon_stats(int64_t weapon_obj, char* buffer, size_t maxlen, ItemCompareStats* stats);
static void format_armor_stats(int64_t armor_obj, char* buffer, size_t maxlen, ItemCompareStats* stats);
static void intgame_examine_scenery(int64_t pc_obj, int64_t scenery_obj, char* str);
static void intgame_examine_portal(int64_t pc_obj, int64_t portal_obj, char* str);
static void intgame_examine_container(int64_t pc_obj, int64_t container_obj, char* str);
static void intgame_draw_portrait(int64_t obj, int portrait, tig_window_handle_t window_handle, int x, int y);
static void intgame_refresh_primary_button(UiPrimaryButton btn);
static void intgame_refresh_experience_gauges(int64_t obj);
static void intgame_magictech_item_panel_open(int64_t item_obj);
static void intgame_mt_button_enable(void);
static void intgame_mt_button_disable(void);
static bool intgame_big_window_create(void);
static void intgame_big_window_destroy(void);
static bool intgame_big_window_message_filter(TigMessage* msg);

// 0x5C6378
static tig_window_handle_t intgame_maintain_fs_windows[5] = {
    TIG_WINDOW_HANDLE_INVALID,
    TIG_WINDOW_HANDLE_INVALID,
    TIG_WINDOW_HANDLE_INVALID,
    TIG_WINDOW_HANDLE_INVALID,
    TIG_WINDOW_HANDLE_INVALID,
};

// 0x5C6390
static TigRect intgame_interface_window_frames[2] = {
    { 0, 0, 800, 41 },
    { 0, 441, 800, 159 },
};

// 0x5C63B0
static TigRect intgame_pc_lens_normal_dst_frame = { 311, 96, 178, 178 };

// 0x5C63C0
static TigRect intgame_pc_lens_fullscreen_dst_frame = { 311, 196, 178, 178 };

// 0x5C63D0
static int intgame_mt_window_index = -1;

// 0x5C63D8
static TigRect intgame_health_bar_frame = { 14, 472, 28, 88 };

// 0x5C63E8
static TigRect intgame_fatigue_bar_frame = { 754, 473, 28, 88 };

// 0x5C63F8
static IntgameIsoWindowTypeInfo intgame_number_boxes[INTGAME_COUNTER_COUNT] = {
    /*  INTGAME_COUNTER_HEALTH */ { { 15, 578, 29, 12 }, TIG_WINDOW_HANDLE_INVALID },
    /* INTGAME_COUNTER_FATIGUE */ { { 755, 578, 27, 12 }, TIG_WINDOW_HANDLE_INVALID },
    /*    INTGAME_COUNTER_FATE */ { { 190, 17, 24, 12 }, TIG_WINDOW_HANDLE_INVALID },
    /*   INTGAME_COUNTER_MONEY */ { { 104, 512, 50, 20 }, TIG_WINDOW_HANDLE_INVALID },
    /*    INTGAME_COUNTER_MANA */ { { 264, 562, 50, 20 }, TIG_WINDOW_HANDLE_INVALID },
    /*  INTGAME_COUNTER_POISON */ { { 15, 500, 29, 12 }, TIG_WINDOW_HANDLE_INVALID },
};

// 0x5C6470
static UiButtonInfo intgame_ammo_button_info = { 61, 509, 251, TIG_BUTTON_HANDLE_INVALID };

// 0x5C6480
static UiButtonInfo intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_COUNT] = {
    /*     INTGAME_SECONDARY_BUTTON_SKILLS */ { 693, 456, 472, TIG_BUTTON_HANDLE_INVALID },
    /*     INTGAME_SECONDARY_BUTTON_SPELLS */ { 649, 494, 473, TIG_BUTTON_HANDLE_INVALID },
    /*     INTGAME_SECONDARY_BUTTON_COMBAT */ { 86, 457, 470, TIG_BUTTON_HANDLE_INVALID },
    /* INTGAME_SECONDARY_BUTTON_SCHEMATICS */ { 693, 539, 471, TIG_BUTTON_HANDLE_INVALID },
};

// 0x5C64C0
static UiButtonInfo intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_COUNT] = {
    /*   INTGAME_PRIMARY_BUTTON_LOGBOOK */ { 41, 2, 187, TIG_BUTTON_HANDLE_INVALID },
    /*      INTGAME_PRIMARY_BUTTON_CHAR */ { 4, 2, 169, TIG_BUTTON_HANDLE_INVALID },
    /* INTGAME_PRIMARY_BUTTON_INVENTORY */ { 115, 2, 186, TIG_BUTTON_HANDLE_INVALID },
    /*       INTGAME_PRIMARY_BUTTON_MAP */ { 78, 2, 193, TIG_BUTTON_HANDLE_INVALID },
    /*      INTGAME_PRIMARY_BUTTON_FATE */ { 157, 9, 137, TIG_BUTTON_HANDLE_INVALID },
};

// 0x5C6510
static int intgame_ui_primary_button_highlighted_icons[UI_PRIMARY_BUTTON_COUNT] = {
    /*      UI_PRIMARY_BUTTON_CHAR */ 561, // "char_on.art"
    /*   UI_PRIMARY_BUTTON_LOGBOOK */ 560, // "log_on.art"
    /*   UI_PRIMARY_BUTTON_TOWNMAP */ 558, // "tmap_on.art"
    /*  UI_PRIMARY_BUTTON_WORLDMAP */ 195, // "wmap_on.art"
    /* UI_PRIMARY_BUTTON_INVENTORY */ 559, // "invn_on.art"
};

// 0x5C6524
static int intgame_ui_primary_button_normal_icons[UI_PRIMARY_BUTTON_COUNT] = {
    /*      UI_PRIMARY_BUTTON_CHAR */ 169, // "char_but.art"
    /*   UI_PRIMARY_BUTTON_LOGBOOK */ 187, // "log_but.art"
    /*   UI_PRIMARY_BUTTON_TOWNMAP */ 193, // "tmap_but.art"
    /*  UI_PRIMARY_BUTTON_WORLDMAP */ 194, // "wmap_but.art"
    /* UI_PRIMARY_BUTTON_INVENTORY */ 186, // "invn_but.art"
};

// 0x5C6538
static UiButtonInfo intgame_sleep_button_info = { 605, 9, 137, TIG_BUTTON_HANDLE_INVALID };

// 0x5C6548
static UiButtonInfo intgame_rotwin_button_info[ROTWIN_TYPE_COUNT] = {
    /*        ROTWIN_TYPE_MSG */ { 196, 492, 354, TIG_BUTTON_HANDLE_INVALID },
    // CE: SKILLS / SPELLS chrome sits 1px higher than the other rotwin
    // arts to align with the inner UI elements (college buttons / spell
    // slots / skill widgets) that were placed relative to the visual
    // indent inside the art frame.
    /*     ROTWIN_TYPE_SPELLS */ { 196, 491, 8, TIG_BUTTON_HANDLE_INVALID },
    /*     ROTWIN_TYPE_SKILLS */ { 196, 491, 274, TIG_BUTTON_HANDLE_INVALID },
    /*       ROTWIN_TYPE_CHAT */ { 196, 492, 642, TIG_BUTTON_HANDLE_INVALID },
    /*      ROTWIN_TYPE_TRAPS */ { 196, 492, 290, TIG_BUTTON_HANDLE_INVALID },
    /*   ROTWIN_TYPE_DIALOGUE */ { 196, 492, 354, TIG_BUTTON_HANDLE_INVALID },
    /*   ROTWIN_TYPE_MAP_NOTE */ { 196, 492, 200, TIG_BUTTON_HANDLE_INVALID },
    /*  ROTWIN_TYPE_BROADCAST */ { 196, 492, 291, TIG_BUTTON_HANDLE_INVALID },
    /*  ROTWIN_TYPE_MAGICTECH */ { 196, 492, 564, TIG_BUTTON_HANDLE_INVALID },
    /*   ROTWIN_TYPE_QUANTITY */ { 196, 492, 298, TIG_BUTTON_HANDLE_INVALID },
    /* ROTWIN_TYPE_MP_KICKBAN */ { 196, 492, 842, TIG_BUTTON_HANDLE_INVALID },
};

// 0x5C65F8
static UiButtonInfo intgame_message_history_scroll_buttons[] = {
    { 210, 545, 0, TIG_BUTTON_HANDLE_INVALID },
    { 210, 504, 0, TIG_BUTTON_HANDLE_INVALID },
};

// 0x5C6618
static UiButtonInfo intgame_college_buttons[COLLEGE_COUNT] = {
    /*        COLLEGE_CONVEYANCE */ { 201, 497, -1, TIG_BUTTON_HANDLE_INVALID },
    /*        COLLEGE_DIVINATION */ { 226, 497, -1, TIG_BUTTON_HANDLE_INVALID },
    /*               COLLEGE_AIR */ { 251, 497, -1, TIG_BUTTON_HANDLE_INVALID },
    /*             COLLEGE_EARTH */ { 276, 497, -1, TIG_BUTTON_HANDLE_INVALID },
    /*              COLLEGE_FIRE */ { 301, 497, -1, TIG_BUTTON_HANDLE_INVALID },
    /*             COLLEGE_WATER */ { 326, 497, -1, TIG_BUTTON_HANDLE_INVALID },
    /*             COLLEGE_FORCE */ { 351, 497, -1, TIG_BUTTON_HANDLE_INVALID },
    /*            COLLEGE_MENTAL */ { 376, 497, -1, TIG_BUTTON_HANDLE_INVALID },
    /*              COLLEGE_META */ { 400, 497, -1, TIG_BUTTON_HANDLE_INVALID },
    /*             COLLEGE_MORPH */ { 425, 497, -1, TIG_BUTTON_HANDLE_INVALID },
    /*            COLLEGE_NATURE */ { 450, 497, -1, TIG_BUTTON_HANDLE_INVALID },
    /* COLLEGE_NECROMANTIC_BLACK */ { 475, 497, -1, TIG_BUTTON_HANDLE_INVALID },
    /* COLLEGE_NECROMANTIC_WHITE */ { 500, 497, -1, TIG_BUTTON_HANDLE_INVALID },
    /*          COLLEGE_PHANTASM */ { 525, 497, -1, TIG_BUTTON_HANDLE_INVALID },
    /*         COLLEGE_SUMMONING */ { 550, 497, -1, TIG_BUTTON_HANDLE_INVALID },
    /*          COLLEGE_TEMPORAL */ { 575, 497, -1, TIG_BUTTON_HANDLE_INVALID },
};

// 0x5C6718
static UiButtonInfo intgame_spell_buttons[SPELL_COUNT] = {
    /*               SPELL_DISARM */ { 284, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*    SPELL_UNLOCKING_CANTRIP */ { 334, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*         SPELL_UNSEEN_FORCE */ { 384, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*   SPELL_SPATIAL_DISTORTION */ { 435, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*        SPELL_TELEPORTATION */ { 485, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*      SPELL_SENSE_ALIGNMENT */ { 284, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*         SPELL_SEE_CONTENTS */ { 334, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*            SPELL_READ_AURA */ { 384, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*         SPELL_SENSE_HIDDEN */ { 435, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*        SPELL_DIVINE_MAGICK */ { 485, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*      SPELL_VITALITY_OF_AIR */ { 284, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*       SPELL_POISON_VAPOURS */ { 334, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*           SPELL_CALL_WINDS */ { 384, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*          SPELL_BODY_OF_AIR */ { 435, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*   SPELL_CALL_AIR_ELEMENTAL */ { 485, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*    SPELL_STRENGTH_OF_EARTH */ { 284, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*          SPELL_STONE_THROW */ { 334, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*        SPELL_WALL_OF_STONE */ { 384, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*        SPELL_BODY_OF_STONE */ { 435, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /* SPELL_CALL_EARTH_ELEMENTAL */ { 485, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*      SPELL_AGILITY_OF_FIRE */ { 284, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*         SPELL_WALL_OF_FIRE */ { 334, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*            SPELL_FIREFLASH */ { 384, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*         SPELL_BODY_OF_FIRE */ { 435, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*  SPELL_CALL_FIRE_ELEMENTAL */ { 485, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*      SPELL_PURITY_OF_WATER */ { 284, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*             SPELL_CALL_FOG */ { 334, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*        SPELL_SQUALL_OF_ICE */ { 384, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*        SPELL_BODY_OF_WATER */ { 435, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /* SPELL_CALL_WATER_ELEMENTAL */ { 485, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /* SPELL_SHIELD_OF_PROTECTION */ { 284, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*                 SPELL_JOLT */ { 334, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*        SPELL_WALL_OF_FORCE */ { 384, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*    SPELL_BOLT_OF_LIGHTNING */ { 435, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*         SPELL_DISINTEGRATE */ { 485, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*                SPELL_CHARM */ { 284, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*                 SPELL_STUN */ { 334, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*           SPELL_DRAIN_WILL */ { 384, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*            SPELL_NIGHTMARE */ { 435, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*        SPELL_DOMINATE_WILL */ { 485, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*        SPELL_RESIST_MAGICK */ { 284, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*      SPELL_DISPERSE_MAGICK */ { 334, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*       SPELL_DWEOMER_SHIELD */ { 384, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*      SPELL_BONDS_OF_MAGICK */ { 435, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*    SPELL_REFLECTION_SHIELD */ { 485, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*       SPELL_HARDENED_HANDS */ { 284, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*               SPELL_WEAKEN */ { 334, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*               SPELL_SHRINK */ { 384, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*       SPELL_FLESH_TO_STONE */ { 435, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*            SPELL_POLYMORPH */ { 485, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*          SPELL_CHARM_BEAST */ { 284, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*             SPELL_ENTANGLE */ { 334, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*        SPELL_CONTROL_BEAST */ { 384, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*        SPELL_SUCCOUR_BEAST */ { 435, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*           SPELL_REGENERATE */ { 485, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*                 SPELL_HARM */ { 284, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*       SPELL_CONJURE_SPIRIT */ { 334, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*        SPELL_SUMMON_UNDEAD */ { 384, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*        SPELL_CREATE_UNDEAD */ { 435, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*          SPELL_QUENCH_LIFE */ { 485, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*        SPELL_MINOR_HEALING */ { 284, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*          SPELL_HALT_POISON */ { 334, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*        SPELL_MAJOR_HEALING */ { 384, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*            SPELL_SANCTUARY */ { 435, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*            SPELL_RESURRECT */ { 485, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*           SPELL_ILLUMINATE */ { 284, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*                SPELL_FLASH */ { 334, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*           SPELL_BLUR_SIGHT */ { 384, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*     SPELL_PHANTASMAL_FIEND */ { 435, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*         SPELL_INVISIBILITY */ { 485, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*    SPELL_PLAGUE_OF_INSECTS */ { 284, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*      SPELL_ORCISH_CHAMPION */ { 334, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*        SPELL_GUARDIAN_OGRE */ { 384, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*             SPELL_HELLGATE */ { 435, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*             SPELL_FAMILIAR */ { 485, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*             SPELL_MAGELOCK */ { 284, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*         SPELL_CONGEAL_TIME */ { 334, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*               SPELL_HASTEN */ { 384, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*               SPELL_STASIS */ { 435, 528, -1, TIG_BUTTON_HANDLE_INVALID },
    /*         SPELL_TEMPUS_FUGIT */ { 485, 528, -1, TIG_BUTTON_HANDLE_INVALID },
};

// 0x5C6C18
static UiButtonInfo intgame_mt_spell_buttons[] = {
    { 353, 542, -1, TIG_BUTTON_HANDLE_INVALID },
    { 402, 542, -1, TIG_BUTTON_HANDLE_INVALID },
    { 451, 542, -1, TIG_BUTTON_HANDLE_INVALID },
    { 501, 542, -1, TIG_BUTTON_HANDLE_INVALID },
    { 550, 542, -1, TIG_BUTTON_HANDLE_INVALID },
};

// 0x5C6C68
static UiButtonInfo intgame_skill_buttons[4] = {
    { 288, 514, -1, TIG_BUTTON_HANDLE_INVALID },
    { 351, 514, -1, TIG_BUTTON_HANDLE_INVALID },
    { 414, 514, -1, TIG_BUTTON_HANDLE_INVALID },
    { 477, 514, -1, TIG_BUTTON_HANDLE_INVALID },
};

// 0x5C6CA8
static UiButtonInfo intgame_map_note_edit_buttons[] = {
    { 396, 550, 201, TIG_BUTTON_HANDLE_INVALID },
    { 511, 550, 202, TIG_BUTTON_HANDLE_INVALID },
    { 281, 549, 203, TIG_BUTTON_HANDLE_INVALID },
    { 213, 509, 146, TIG_BUTTON_HANDLE_INVALID },
    { 213, 536, 148, TIG_BUTTON_HANDLE_INVALID },
    { 213, 563, 145, TIG_BUTTON_HANDLE_INVALID },
};

// 0x5C6D08
static UiButtonInfo intgame_quantity_buttons[INTGAME_QUANTITY_BUTTON_COUNT] = {
    /* INTGAME_QUANTITY_BUTTON_TAKE_ALL */ { 364, 548, 299, TIG_BUTTON_HANDLE_INVALID },
    /*     INTGAME_QUANTITY_BUTTON_PLUS */ { 479, 547, 805, TIG_BUTTON_HANDLE_INVALID },
    /*    INTGAME_QUANTITY_BUTTON_MINUS */ { 479, 559, 804, TIG_BUTTON_HANDLE_INVALID },
    /*       INTGAME_QUANTITY_BUTTON_OK */ { 545, 506, 33, TIG_BUTTON_HANDLE_INVALID },
    /*   INTGAME_QUANTITY_BUTTON_CANCEL */ { 545, 557, 32, TIG_BUTTON_HANDLE_INVALID },
};

// 0x5C6D58
static RotatingWindowType intgame_saved_rotating_window_type = ROTWIN_TYPE_INVALID;

// 0x5C6D60
struct IntgameIsoWindowTypeInfo intgame_rotwin_text_frame[ROTWIN_TYPE_COUNT] = {
    /*        ROTWIN_TYPE_MSG */ { { 211, 503, 383, 82 }, TIG_WINDOW_HANDLE_INVALID },
    // CE: SPELLS/SKILLS chrome shifted up 1px (see intgame_rotwin_button_info);
    // their inner text rows shift in lockstep so the rolling text stays
    // aligned with the chrome's visible text well.
    /*     ROTWIN_TYPE_SPELLS */ { { 208, 573, 387, 18 }, TIG_WINDOW_HANDLE_INVALID },
    /*     ROTWIN_TYPE_SKILLS */ { { 208, 566, 387, 18 }, TIG_WINDOW_HANDLE_INVALID },
    /*       ROTWIN_TYPE_CHAT */ { { 291, 566, 268, 19 }, TIG_WINDOW_HANDLE_INVALID },
    /*      ROTWIN_TYPE_TRAPS */ { { 208, 574, 387, 18 }, TIG_WINDOW_HANDLE_INVALID },
    /*   ROTWIN_TYPE_DIALOGUE */ { { 211, 507, 383, 84 }, TIG_WINDOW_HANDLE_INVALID },
    /*   ROTWIN_TYPE_MAP_NOTE */ { { 262, 508, 303, 24 }, TIG_WINDOW_HANDLE_INVALID },
    /*  ROTWIN_TYPE_BROADCAST */ { { 220, 503, 350, 82 }, TIG_WINDOW_HANDLE_INVALID },
    /*  ROTWIN_TYPE_MAGICTECH */ { { 355, 506, 227, 18 }, TIG_WINDOW_HANDLE_INVALID },
    /*   ROTWIN_TYPE_QUANTITY */ { { 0, 0, 0, 0 }, TIG_WINDOW_HANDLE_INVALID },
    /* ROTWIN_TYPE_MP_KICKBAN */ { { 0, 0, 0, 0 }, TIG_WINDOW_HANDLE_INVALID },
};

// 0x5C6E40
static UiButtonInfo intgame_maintain_buttons[] = {
    { 281, 3, 188, TIG_BUTTON_HANDLE_INVALID },
    { 331, 3, 189, TIG_BUTTON_HANDLE_INVALID },
    { 381, 3, 190, TIG_BUTTON_HANDLE_INVALID },
    { 431, 3, 191, TIG_BUTTON_HANDLE_INVALID },
    { 481, 3, 192, TIG_BUTTON_HANDLE_INVALID },
};

// 0x5C6E90
static UiButtonInfo intgame_maintain_fs_buttons[] = {
    { 281, 3, 188, TIG_BUTTON_HANDLE_INVALID },
    { 331, 3, 189, TIG_BUTTON_HANDLE_INVALID },
    { 381, 3, 190, TIG_BUTTON_HANDLE_INVALID },
    { 431, 3, 191, TIG_BUTTON_HANDLE_INVALID },
    { 481, 3, 192, TIG_BUTTON_HANDLE_INVALID },
};

// 0x5C6EE0
static TigRect intgame_maintain_window_rects[5] = {
    { 281, 3, 32, 32 },
    { 331, 3, 32, 32 },
    { 381, 3, 32, 32 },
    { 431, 3, 32, 32 },
    { 481, 3, 32, 32 },
};

// 0x5C6F30
static UiButtonInfo intgame_legacy_action_button = { 616, 455, 182, TIG_BUTTON_HANDLE_INVALID };

// 0x5C6F40
static UiButtonInfo intgame_recent_action_buttons[] = {
    { 69, 548, -1, TIG_BUTTON_HANDLE_INVALID },
    { 114, 548, -1, TIG_BUTTON_HANDLE_INVALID },
};

// 0x5C6F60
static int intgame_recent_action_default_skills[] = {
    1,
    0,
};

// 0x5C6F68
static UiButtonInfo intgame_mt_button_info = { 161, 443, 563, TIG_BUTTON_HANDLE_INVALID };

// 0x5C6F78
static int intgame_rotwin_step = MAX_INTERFACE_WINDOW_ROTATION_STEPS;

// 0x5C6F80
static TigRect intgame_clock_frame = { 648, 5, 128, 30 };

// 0x5C6F90
static UiButtonInfo intgame_clock_button_info = { 0, 0, -1, TIG_BUTTON_HANDLE_INVALID };

// 0x5C6FA0
static int intgame_race_icons[RACE_COUNT] = {
    /*     RACE_HUMAN */ 375, // racehumanicon.art
    /*     RACE_DWARF */ 378, // racehelficon.art
    /*       RACE_ELF */ 376, // raceelficon.art
    /*  RACE_HALF_ELF */ 377, // racehelficon.art
    /*     RACE_GNOME */ 379, // racegnomeicon.art
    /*  RACE_HALFLING */ 380, // racehalflingicon.art
    /*  RACE_HALF_ORC */ 381, // racehorcicon.art
    /* RACE_HALF_OGRE */ 382, // racehogreicon.art
    /*  RACE_DARK_ELF */ 376, // raceelficon.art
    /*      RACE_OGRE */ 382, // racehogreicon.art
    /*       RACE_ORC */ 381, // racehorcicon.art
};

// 0x5C6FCC
static int intgame_alignment_icons[6] = {
    634, // "icon_lawfulevil.art"
    390, // "icon_evil.art"
    389, // "icon_chaotic.art"
    388, // "icon_lawful.art"
    387, // "icon_good.art"
    386, // "icon_lawfulgood.art"
};

// 0x5C6FE4
static int intgame_weapon_icons[TIG_ART_WEAPON_TYPE_COUNT] = {
    /*        TIG_ART_WEAPON_TYPE_NO_WEAPON */ 391,
    /*          TIG_ART_WEAPON_TYPE_UNARMED */ 391,
    /*           TIG_ART_WEAPON_TYPE_DAGGER */ 391,
    /*            TIG_ART_WEAPON_TYPE_SWORD */ 391,
    /*              TIG_ART_WEAPON_TYPE_AXE */ 391,
    /*             TIG_ART_WEAPON_TYPE_MACE */ 391,
    /*           TIG_ART_WEAPON_TYPE_PISTOL */ 392,
    /* TIG_ART_WEAPON_TYPE_TWO_HANDED_SWORD */ 391,
    /*              TIG_ART_WEAPON_TYPE_BOW */ 395,
    /*                TIG_ART_WEAPON_TYPE_9 */ 391,
    /*            TIG_ART_WEAPON_TYPE_RIFLE */ 392,
    /*               TIG_ART_WEAPON_TYPE_11 */ 391,
    /*               TIG_ART_WEAPON_TYPE_12 */ 391,
    /*            TIG_ART_WEAPON_TYPE_STAFF */ 391,
    /*               TIG_ART_WEAPON_TYPE_14 */ 391,
};

// 0x5C7020
static int intgame_ammo_icons[TIG_ART_AMMO_TYPE_COUNT] = {
    /*  TIG_ART_AMMO_TYPE_ARROW */ 398,
    /* TIG_ART_AMMO_TYPE_BULLET */ 399,
    /* TIG_ART_AMMO_TYPE_CHARGE */ 400,
    /*   TIG_ART_AMMO_TYPE_FUEL */ 401,
};

// 0x5C7030
static int intgame_armor_type_icons[TIG_ART_ARMOR_TYPE_COUNT] = {
    /*     TIG_ART_ARMOR_TYPE_UNDERWEAR */ 402,
    /*      TIG_ART_ARMOR_TYPE_VILLAGER */ 402,
    /*       TIG_ART_ARMOR_TYPE_LEATHER */ 405,
    /*         TIG_ART_ARMOR_TYPE_CHAIN */ 405,
    /*         TIG_ART_ARMOR_TYPE_PLATE */ 405,
    /*          TIG_ART_ARMOR_TYPE_ROBE */ 408,
    /* TIG_ART_ARMOR_TYPE_PLATE_CLASSIC */ 405,
    /*     TIG_ART_ARMOR_TYPE_BARBARIAN */ 405,
    /*  TIG_ART_ARMOR_TYPE_CITY_DWELLER */ 402,
};

// 0x5C7054
static int intgame_armor_coverage_icons[TIG_ART_ARMOR_COVERAGE_COUNT] = {
    /*     TIG_ART_ARMOR_COVERAGE_TORSO */ 0,
    /*    TIG_ART_ARMOR_COVERAGE_SHIELD */ 405,
    /*    TIG_ART_ARMOR_COVERAGE_HELMET */ 405,
    /* TIG_ART_ARMOR_COVERAGE_GAUNTLETS */ 405,
    /*     TIG_ART_ARMOR_COVERAGE_BOOTS */ 405,
    /*      TIG_ART_ARMOR_COVERAGE_RING */ 411,
    /* TIG_ART_ARMOR_COVERAGE_MEDALLION */ 414,
};

// 0x5C7070
static int intgame_written_icons[WRITTEN_TYPE_COUNT] = {
    /*      WRITTEN_TYPE_BOOK */ 428, // "booksicon.art"
    /*      WRITTEN_TYPE_NOTE */ 429, // "note_icon.art"
    /* WRITTEN_TYPE_NEWSPAPER */ 431, // "newpaper_icon.art"
    /*  WRITTEN_TYPE_TELEGRAM */ 430, // "telegram_icon.art"
    /*     WRITTEN_TYPE_IMAGE */ 428, // "booksicon.art"
    /* WRITTEN_TYPE_SCHEMATIC */ 462, // "schematic_icon.art"
    /*    WRITTEN_TYPE_PLAQUE */ 428, // "booksicon.art"
};

// 0x5C708C
static int intgame_message_icons[UI_MSG_TYPE_COUNT] = {
    /*       UI_MSG_TYPE_LEVEL */ 438, // "levelupicon.art" - Level Up Icon
    /*      UI_MSG_TYPE_POISON */ 439, // "poisoned_icon.art" - Poisoned Icon
    /*       UI_MSG_TYPE_CURSE */ 440, // "hexedicon.art" - Cursed Icon
    /*       UI_MSG_TYPE_BLESS */ 441, // "blessicon.art" - Blessed Icon
    /* UI_MSG_TYPE_EXCLAMATION */ 442, // "exclaimation_icon.art" - Exclamation Icon
    /*    UI_MSG_TYPE_QUESTION */ 443, // "question_icon.art" - Question Icon
    /*    UI_MSG_TYPE_FEEDBACK */ 444, // "levelupicon.art" - Arcanum Icon
    /*       UI_MSG_TYPE_SKILL */ 0,
    /*       UI_MSG_TYPE_SPELL */ 0,
    /*     UI_MSG_TYPE_COLLEGE */ 0,
    /*        UI_MSG_TYPE_TECH */ 0,
    /*      UI_MSG_TYPE_DEGREE */ 0,
    /*        UI_MSG_TYPE_STAT */ 0,
    /*   UI_MSG_TYPE_SCHEMATIC */ 0,
};

// 0x5C70C8
static TigRect intgame_message_title_rect = { 290, 63, 291, 19 };

// 0x5C70D8
static TigRect intgame_message_line_2_rect = { 290, 87, 291, 18 };

// 0x5C70E8
static TigRect intgame_message_line_3_rect = { 290, 105, 291, 18 };

// 0x5C70F8
static TigRect intgame_message_line_4_rect = { 290, 123, 291, 18 };

// 0x5C7108
static TigRect intgame_message_full_text_rect = { 290, 63, 291, 78 };

// 0x5C7118
static TigRect intgame_message_scenery_text_rect = { 290, 63, 291, 55 };

// 0x5C7128
static TigRect intgame_message_lower_text_rect = { 290, 105, 291, 36 };

// 0x5C7138
static TigRect intgame_message_description_rect = { 290, 87, 291, 54 };

// 0x5C7148
static TigRect intgame_message_wide_title_rect = { 217, 63, 364, 19 };

// 0x5C7158
static TigRect intgame_message_wide_text_rect = { 217, 63, 364, 78 };

// 0x5C7168
static TigRect intgame_message_wide_description_rect = { 217, 87, 364, 54 };

// 0x5C7178
static int intgame_mode_cursors[INTGAME_MODE_COUNT] = {
    /*         INTGAME_MODE_MAIN */ -1,
    /*        INTGAME_MODE_SPELL */ 21,
    /*        INTGAME_MODE_SKILL */ 352,
    /*       INTGAME_MODE_DIALOG */ 0,
    /*       INTGAME_MODE_BARTER */ 0,
    /*         INTGAME_MODE_WMAP */ 0,
    /*        INTGAME_MODE_SLEEP */ 0,
    /*      INTGAME_MODE_LOGBOOK */ 0,
    /*        INTGAME_MODE_INVEN */ 0,
    /*     INTGAME_MODE_CHAREDIT */ 0,
    /*         INTGAME_MODE_LOOT */ 0,
    /*        INTGAME_MODE_STEAL */ 0,
    /*           INTGAME_MODE_RESERVED_12 */ 0,
    /*     INTGAME_MODE_QUANTITY */ 0,
    /*    INTGAME_MODE_SCHEMATIC */ 0,
    /*      INTGAME_MODE_WRITTEN */ 0,
    /*         INTGAME_MODE_ITEM */ 21,
    /*           INTGAME_MODE_RESERVED_17 */ 0,
    /*     INTGAME_MODE_FOLLOWER */ -1,
    /* INTGAME_MODE_NPC_IDENTIFY */ 0,
    /*   INTGAME_MODE_NPC_REPAIR */ 0,
};

// 0x5C71D0
static UiButtonInfo intgame_experience_gauge_buttons[10] = {
    { 211, 37, 773, TIG_BUTTON_HANDLE_INVALID },
    { 249, 37, 774, TIG_BUTTON_HANDLE_INVALID },
    { 287, 37, 775, TIG_BUTTON_HANDLE_INVALID },
    { 327, 37, 776, TIG_BUTTON_HANDLE_INVALID },
    { 365, 37, 777, TIG_BUTTON_HANDLE_INVALID },
    { 403, 37, 778, TIG_BUTTON_HANDLE_INVALID },
    { 439, 37, 779, TIG_BUTTON_HANDLE_INVALID },
    { 478, 37, 780, TIG_BUTTON_HANDLE_INVALID },
    { 516, 37, 781, TIG_BUTTON_HANDLE_INVALID },
    { 555, 37, 782, TIG_BUTTON_HANDLE_INVALID },
};

// 0x5C7270
static UiButtonInfo intgame_experience_gauge_frame = { 216, 47, 772, TIG_BUTTON_HANDLE_INVALID };

// 0x5C7280
static uint64_t intgame_hover_target_flags = TGT_OBJECT;

// 0x5C7288
static tig_window_handle_t intgame_fs_hotkey_window = TIG_WINDOW_HANDLE_INVALID;

// 0x5C728C
static int intgame_ammo_icon_art_numbers[TIG_ART_AMMO_TYPE_COUNT] = {
    /*  TIG_ART_AMMO_TYPE_ARROW */ 250,
    /* TIG_ART_AMMO_TYPE_BULLET */ 251,
    /* TIG_ART_AMMO_TYPE_CHARGE */ 252,
    /*   TIG_ART_AMMO_TYPE_FUEL */ 253,
};

static int intgame_unused_lookup_values[] = {
    0,
    199,
    495,
    80,
    48,
};

// 0x5C72B0
static int intgame_pc_lens_render_unlocked = 1;

static bool intgame_camera_pan_active;
static bool intgame_camera_pan_moved;
static int intgame_camera_pan_x;
static int intgame_camera_pan_y;

// 0x5C72B4
static bool intgame_mode_scrolling[INTGAME_MODE_COUNT] = {
    /*         INTGAME_MODE_MAIN */ true,
    /*        INTGAME_MODE_SPELL */ true,
    /*        INTGAME_MODE_SKILL */ true,
    /*       INTGAME_MODE_DIALOG */ true,
    /*       INTGAME_MODE_BARTER */ false,
    /*         INTGAME_MODE_WMAP */ true,
    /*        INTGAME_MODE_SLEEP */ false,
    /*      INTGAME_MODE_LOGBOOK */ false,
    /*        INTGAME_MODE_INVEN */ false,
    /*     INTGAME_MODE_CHAREDIT */ false,
    /*         INTGAME_MODE_LOOT */ false,
    /*        INTGAME_MODE_STEAL */ false,
    /*           INTGAME_MODE_RESERVED_12 */ false,
    /*     INTGAME_MODE_QUANTITY */ false,
    /*    INTGAME_MODE_SCHEMATIC */ false,
    /*      INTGAME_MODE_WRITTEN */ false,
    /*         INTGAME_MODE_ITEM */ true,
    /*           INTGAME_MODE_RESERVED_17 */ false,
    /*     INTGAME_MODE_FOLLOWER */ true,
    /* INTGAME_MODE_NPC_IDENTIFY */ false,
    /*   INTGAME_MODE_NPC_REPAIR */ false,
};

// 0x5C7308
static int intgame_clock_last_strip_offset = -1;

// 0x5C730C
static int intgame_moon_phase_day_thresholds[8] = {
    0,
    4,
    9,
    13,
    14,
    18,
    23,
    27,
};

// 0x5C732C
static unsigned int intgame_penalty_flags[INTGAME_PENALTY_COUNT] = {
    /*          INTGAME_PENALTY_MSR */ SKILL_INVOCATION_PENALTY_MSR,
    /*        INTGAME_PENALTY_RANGE */ SKILL_INVOCATION_PENALTY_RANGE,
    /*   INTGAME_PENALTY_PERCEPTION */ SKILL_INVOCATION_PENALTY_PERCEPTION,
    /*        INTGAME_PENALTY_COVER */ SKILL_INVOCATION_PENALTY_COVER,
    /*        INTGAME_PENALTY_LIGHT */ SKILL_INVOCATION_PENALTY_LIGHT,
    /*       INTGAME_PENALTY_INJURY */ SKILL_INVOCATION_PENALTY_INJURY,
    /* INTGAME_PENALTY_BLOCKED_SHOT */ SKILL_INVOCATION_BLOCKED_SHOT,
    /*   INTGAME_PENALTY_MAGIC_TECH */ SKILL_INVOCATION_MAGIC_TECH_PENALTY,
};

// 0x5C734C
static int intgame_penalty_icons[INTGAME_PENALTY_COUNT] = {
    /*          INTGAME_PENALTY_MSR */ 586, // "pen_msr.art"
    /*        INTGAME_PENALTY_RANGE */ 588, // "pen_range.art"
    /*   INTGAME_PENALTY_PERCEPTION */ 587, // "pen_perception.art"
    /*        INTGAME_PENALTY_COVER */ 583, // "pen_cover.art"
    /*        INTGAME_PENALTY_LIGHT */ 585, // "pen_light.art"
    /*       INTGAME_PENALTY_INJURY */ 584, // "pen_injury.art"
    /* INTGAME_PENALTY_BLOCKED_SHOT */ 845, // "blockedshot.art"
    /*   INTGAME_PENALTY_MAGIC_TECH */ 846, // "magic-tech-penalty.art"
};

// 0x5C736C
static int intgame_penalty_slot_x[INTGAME_PENALTY_SLOTS] = {
    /*        INTGAME_PENALTY_MSR */ 210,
    /*      INTGAME_PENALTY_RANGE */ 248,
    /* INTGAME_PENALTY_PERCEPTION */ 210,
    /*      INTGAME_PENALTY_COVER */ 248,
    /*      INTGAME_PENALTY_LIGHT */ 210,
    /*     INTGAME_PENALTY_INJURY */ 248,
};

// 0x5C7384
static int intgame_penalty_slot_y[INTGAME_PENALTY_SLOTS] = {
    /*        INTGAME_PENALTY_MSR */ 84,
    /*      INTGAME_PENALTY_RANGE */ 84,
    /* INTGAME_PENALTY_PERCEPTION */ 104,
    /*      INTGAME_PENALTY_COVER */ 104,
    /*      INTGAME_PENALTY_LIGHT */ 124,
    /*     INTGAME_PENALTY_INJURY */ 124,
};

// 0x5C739C
static int intgame_iso_window_width = 800;

// 0x5C73A0
static int intgame_iso_window_height = 600;

// 0x5C73A4
static tig_window_handle_t intgame_iso_window = TIG_WINDOW_HANDLE_INVALID;

// 0x64C470
static tig_font_handle_t intgame_morph15_blue_font;

// 0x64C474
static TigVideoBuffer* intgame_vital_bar_staging_buffer;

// 0x64C478
static int intgame_max_quantity;

// 0x64C47C
static int intgame_clock_strip_widths[2];

// 0x64C484
static int intgame_ui_primary_button_icons[UI_PRIMARY_BUTTON_COUNT];

// 0x64C498
static tig_font_handle_t intgame_flare12_white_font;

// 0x64C49C
static tig_font_handle_t intgame_flare12_red_font;
static tig_font_handle_t intgame_compare_worse_font;
static tig_font_handle_t intgame_compare_better_font;
static tig_font_handle_t intgame_compare_equal_font;

// 0x64C4A0
static tig_font_handle_t intgame_flare14_white_font;

// 0x64C4A8
static UiButtonInfo intgame_rotwin_text_buttons[5];

// 0x64C4F8
static tig_window_handle_t intgame_interface_windows[2];

// 0x64C500
static tig_font_handle_t intgame_flare12_blue_font;

// 0x64C504
static mes_file_handle_t intgame_mes_file;

// 0x64C508
static tig_window_handle_t intgame_big_window_handle;

// 0x64C510
static TigRect intgame_pc_lens_dst_rect;

// 0x64C520
static PcLens intgame_pc_lens;

// 0x64C52C
static tig_window_handle_t intgame_iso_window_handle;

// 0x64C530
static int intgame_selected_spell_college;

// 0x64C534
static UiPrimaryButton intgame_map_button;

// 0x64C538
static tig_font_handle_t intgame_morph15_orange_font;

// 0x64C540
static UiMessage intgame_message_history[MAX_MESSAGE_HISTORY_ITEMS];

// 0x64C630
static bool intgame_big_window_locked;

// 0x64C634
static IntgameModeState intgame_mode_state;

// 0x64C660
static TigRect intgame_pc_lens_src_rect;

// 0x64C670
static tig_font_handle_t intgame_cloister18_font;

// 0x64C674
static int intgame_action_heading_message_id;

// 0x64C678
static int intgame_quantity;

// 0x64C67C
static bool intgame_compact_interface;

// 0x64C680
static bool intgame_fullscreen;

// 0x64C688
static int64_t intgame_magictech_item_obj;

// 0x64C690
static int64_t intgame_selected_obj;

// 0x64C698
static TigRect intgame_pc_lens_blackout_draw_rect;

// 0x64C6A8
static RotatingWindowType intgame_iso_window_type;

// 0x64C6AC
static RotatingWindowType intgame_requested_rotating_window_type;

// 0x64C6B0
static bool intgame_vital_bars_enabled;

// 0x64C6B4
static bool intgame_iso_interface_created;

// 0x64C6BC
static PcLensMode intgame_pc_lens_mode;

// 0x64C6C0
static int intgame_message_history_size;

// 0x64C6C4
static int intgame_message_history_end;

// 0x64C6C8
static int intgame_message_history_curr;

// 0x64C6CC
static bool (*intgame_dialog_process_event_func)(TigMessage* msg);

// 0x64C6D0
static int intgame_clock_debug_text_enabled;

// 0x64C6D4
static void (*intgame_message_display_callback)(UiMessage* ui_message);

// 0x64C6D8
static int intgame_mouse_movement_active;

// 0x64C6DC
static bool intgame_fullscreen_forced;

// 0x64C6E0
static bool intgame_rotating_window_active;

// 0x64C6E4
static TigVideoBuffer* intgame_pc_lens_video_buffer;

// 0x64C6F0
static unsigned int intgame_iso_window_flags;

// 0x739F88
tig_font_handle_t intgame_morph15_white_font;

// 0x549B70
bool intgame_init(GameInitInfo* init_info)
{
    TigFont font;

    (void)init_info;

    if (!mes_load("mes\\intgame.mes", &intgame_mes_file)) {
        return false;
    }

    if (!intgame_big_window_create()) {
        mes_unload(intgame_mes_file);
        return false;
    }

    font.flags = 0;
    tig_art_interface_id_create(27, 0, 0, 0, &(font.art_id));
    font.str = NULL;
    font.color = tig_color_make(255, 255, 255);
    tig_font_create(&font, &intgame_morph15_white_font);

    font.flags = 0;
    tig_art_interface_id_create(27, 0, 0, 0, &(font.art_id));
    font.str = NULL;
    font.color = tig_color_make(100, 100, 255);
    tig_font_create(&font, &intgame_morph15_blue_font);

    font.flags = 0;
    tig_art_interface_id_create(27, 0, 0, 0, &(font.art_id));
    font.str = NULL;
    font.color = tig_color_make(255, 114, 0);
    tig_font_create(&font, &intgame_morph15_orange_font);

    font.flags = 0;
    tig_art_interface_id_create(229, 0, 0, 0, &(font.art_id));
    font.str = NULL;
    font.color = tig_color_make(255, 255, 255);
    tig_font_create(&font, &intgame_flare12_white_font);

    font.flags = 0;
    tig_art_interface_id_create(229, 0, 0, 0, &(font.art_id));
    font.str = NULL;
    font.color = tig_color_make(255, 0, 0);
    tig_font_create(&font, &intgame_flare12_red_font);
    font.color = modern_font_color_for(BARTER_COLOR_WORSE_KEY, 255, 0, 0);
    tig_font_create(&font, &intgame_compare_worse_font);
    font.color = modern_font_color_for(BARTER_COLOR_BETTER_KEY, 0, 255, 0);
    tig_font_create(&font, &intgame_compare_better_font);
    font.color = modern_font_color_for(BARTER_COLOR_EQUAL_KEY, 245, 245, 245);
    tig_font_create(&font, &intgame_compare_equal_font);

    font.flags = 0;
    tig_art_interface_id_create(229, 0, 0, 0, &(font.art_id));
    font.str = NULL;
    font.color = tig_color_make(0, 0, 255);
    tig_font_create(&font, &intgame_flare12_blue_font);

    font.flags = 0;
    tig_art_interface_id_create(230, 0, 0, 0, &(font.art_id));
    font.str = NULL;
    font.color = tig_color_make(255, 255, 255);
    tig_font_create(&font, &intgame_flare14_white_font);

    memcpy(intgame_ui_primary_button_icons, intgame_ui_primary_button_normal_icons, sizeof(intgame_ui_primary_button_icons));
    intgame_map_button = UI_PRIMARY_BUTTON_TOWNMAP;
    intgame_action_heading_message_id = -1;

    return true;
}

// 0x549F00
void intgame_reset(void)
{
    movement_preview_destroy();
    int index;

    intgame_action_tooltips_reset();

    intgame_mouse_movement_active = 0;
    intgame_refresh_cursor();
    hotkey_ui_reset_recent_actions();
    intgame_clock_process_callback(NULL);
    iso_interface_window_swap(ROTWIN_TYPE_MSG);

    for (index = 0; index < 10; index++) {
        hotkey_ui_slot_clear(index);
    }

    intgame_map_button = UI_PRIMARY_BUTTON_TOWNMAP;
    memcpy(intgame_ui_primary_button_icons, intgame_ui_primary_button_normal_icons, sizeof(intgame_ui_primary_button_icons));
    intgame_mode_and_message_history_reset();
}

// 0x549F60
void intgame_resize(GameResizeInfo* resize_info)
{
    movement_preview_destroy();
    int index;
    TigWindowData window_data;
    TigRect rect;

    hotkey_ui_end();
    intgame_iso_window_handle = resize_info->window_handle;

    if (intgame_fs_hotkey_window != TIG_WINDOW_HANDLE_INVALID) {
        tig_window_destroy(intgame_fs_hotkey_window);
        intgame_fs_hotkey_window = TIG_WINDOW_HANDLE_INVALID;
    }

    if (!intgame_compact_interface) {
        hotkey_ui_start(intgame_interface_windows[1], &(intgame_interface_window_frames[1]), intgame_interface_windows[1], false);

        for (index = 0; index < 5; index++) {
            tig_window_hide(intgame_maintain_fs_windows[index]);
        }
    } else {
        window_data.flags = TIG_WINDOW_ALWAYS_ON_TOP;
        window_data.rect.x = 196;
        window_data.rect.y = 563;
        window_data.rect.width = 411;
        window_data.rect.height = 37;
        window_data.color_key = tig_color_make(5, 5, 5);
        hrp_apply(&(window_data.rect), GRAVITY_CENTER_HORIZONTAL | GRAVITY_BOTTOM);

        if (tig_window_create(&window_data, &intgame_fs_hotkey_window) != TIG_OK) {
            tig_debug_printf("intgame_resize: ERROR: couldn't create window!");
            tig_exit();
        }

        rect.x = 0;
        rect.y = 0;
        rect.width = window_data.rect.width;
        rect.height = window_data.rect.height;
        tig_window_fill(intgame_fs_hotkey_window,
            &rect,
            tig_color_make(5, 5, 5));

        rect.x = 196;
        rect.y = 563;
        rect.width = 411;
        rect.height = 37;
        hotkey_ui_start(intgame_fs_hotkey_window, &rect, TIG_WINDOW_HANDLE_INVALID, true);

        for (index = 0; index < 5; index++) {
            if (spell_ui_maintain_has(index)) {
                tig_window_show(intgame_maintain_fs_windows[index]);
            }
        }
    }

    if (tig_window_is_hidden(intgame_iso_window_handle)) {
        intgame_hide();
    }
}

// 0x54A130
void intgame_exit(void)
{
    movement_preview_destroy();
    intgame_action_tooltips_reset();
    tig_font_destroy(intgame_morph15_white_font);
    tig_font_destroy(intgame_morph15_blue_font);
    tig_font_destroy(intgame_morph15_orange_font);
    tig_font_destroy(intgame_flare12_white_font);
    tig_font_destroy(intgame_flare12_red_font);
    tig_font_destroy(intgame_compare_worse_font);
    tig_font_destroy(intgame_compare_better_font);
    tig_font_destroy(intgame_compare_equal_font);
    tig_font_destroy(intgame_flare12_blue_font);
    tig_font_destroy(intgame_flare14_white_font);
    intgame_big_window_destroy();
    mes_unload(intgame_mes_file);
}

// 0x54A1A0
bool intgame_save(TigFile* stream)
{
    if (stream == NULL) return false;
    if (tig_file_fwrite(&intgame_iso_window_type, sizeof(intgame_iso_window_type), 1, stream) != 1) return false;
    if (tig_file_fwrite(&intgame_selected_spell_college, sizeof(intgame_selected_spell_college), 1, stream) != 1) return false;
    if (!hotkey_ui_save(stream)) return false;
    if (tig_file_fwrite(intgame_ui_primary_button_icons, sizeof(*intgame_ui_primary_button_icons), UI_PRIMARY_BUTTON_COUNT, stream) != UI_PRIMARY_BUTTON_COUNT) return false;
    if (tig_file_fwrite(&intgame_map_button, sizeof(intgame_map_button), 1, stream) != 1) return false;

    return true;
}

// 0x54A220
bool intgame_load(GameLoadInfo* load_info)
{
    int legacy_saved_state;
    int btn;
    int64_t obj;

    intgame_action_tooltips_reset();

    if (load_info->stream == NULL) return false;
    if (tig_file_fread(&legacy_saved_state, sizeof(legacy_saved_state), 1, load_info->stream) != 1) return false;
    if (tig_file_fread(&intgame_selected_spell_college, sizeof(intgame_selected_spell_college), 1, load_info->stream) != 1) return false;

    if (intgame_iso_window_type == ROTWIN_TYPE_SPELLS) {
        tig_button_state_change(intgame_college_buttons[intgame_selected_spell_college].button_handle, TIG_BUTTON_STATE_PRESSED);
    }

    if (!hotkey_ui_load(load_info)) return false;
    if (tig_file_fread(intgame_ui_primary_button_icons, sizeof(*intgame_ui_primary_button_icons), UI_PRIMARY_BUTTON_COUNT, load_info->stream) != UI_PRIMARY_BUTTON_COUNT) return false;
    if (tig_file_fread(&intgame_map_button, sizeof(intgame_map_button), 1, load_info->stream) != 1) return false;

    for (btn = 0; btn < UI_PRIMARY_BUTTON_COUNT; btn++) {
        intgame_refresh_primary_button(btn);
    }

    intgame_counters_refresh();
    intgame_refresh_cursor();
    intgame_mt_button_enable();

    obj = player_get_local_pc_obj();
    if (obj != OBJ_HANDLE_NULL) {
        intgame_refresh_experience_gauges(obj);
        intgame_draw_counter(INTGAME_COUNTER_FATE,
            stat_level_get(obj, STAT_FATE_POINTS),
            2);
    }

    return true;
}

// 0x54A330
void iso_interface_create(tig_window_handle_t window_handle)
{
    TigWindowData window_data;
    TigVideoBufferCreateInfo vb_create_info;
    TigArtBlitInfo art_blit_info;
    int index;
    tig_art_id_t art_id;
    TigArtAnimData art_anim_data;
    TigArtFrameData art_frame_data;
    int iwid;
    TigFont font_desc;

    intgame_pc_lens_dst_rect = intgame_pc_lens_normal_dst_frame;

    // CE: These adjustments should be on par with `intgame_toggle_interface`.
    intgame_pc_lens_dst_rect.x = (800 - intgame_pc_lens_dst_rect.width) / 2;
    intgame_pc_lens_dst_rect.y = (600 - intgame_pc_lens_dst_rect.height) / 2;
    hrp_apply(&intgame_pc_lens_dst_rect, GRAVITY_CENTER_HORIZONTAL | GRAVITY_CENTER_VERTICAL);

    intgame_iso_window_handle = window_handle;
    intgame_vital_bars_enabled = 1;
    intgame_iso_interface_created = false;

    tig_window_data(window_handle, &window_data);

    vb_create_info.flags = 0;
    vb_create_info.width = intgame_health_bar_frame.width / 2;
    vb_create_info.height = intgame_health_bar_frame.height;
    vb_create_info.background_color = 0;
    if (tig_video_buffer_create(&vb_create_info, &intgame_vital_bar_staging_buffer) != TIG_OK) {
        tig_debug_printf("iso_interface_create: ERROR: couldn't create video buffer!\n");
        exit(EXIT_SUCCESS); // FIXME: Should be EXIT_FAILURE
    }

    window_data.flags = TIG_WINDOW_MESSAGE_FILTER;
    window_data.message_filter = iso_interface_message_filter;

    art_blit_info.flags = 0;
    art_blit_info.src_rect = &(window_data.rect);
    art_blit_info.dst_rect = &(window_data.rect);

    for (index = 0; index < 2; index++) {
        if (index == 0) {
            tig_art_interface_id_create(185, 0, 0, 0, &art_id);
        } else {
            tig_art_interface_id_create(184, 0, 0, 0, &art_id);
        }

        if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
            tig_debug_printf("iso_interface_create: ERROR: couldn't grab art anim data!\n");
            exit(EXIT_SUCCESS); // FIXME: Should be EXIT_FAILURE
        }

        window_data.background_color = art_anim_data.color_key;
        window_data.rect = intgame_interface_window_frames[index];
        art_blit_info.art_id = art_id;

        switch (index) {
        case 0:
            hrp_apply(&(window_data.rect), GRAVITY_CENTER_HORIZONTAL | GRAVITY_TOP);
            break;
        case 1:
            hrp_apply(&(window_data.rect), GRAVITY_CENTER_HORIZONTAL | GRAVITY_BOTTOM);
            break;
        }

        if (tig_window_create(&window_data, &(intgame_interface_windows[index])) != TIG_OK) {
            tig_debug_printf("iso_interface_create: ERROR: couldn't create window!\n");
            exit(EXIT_SUCCESS); // FIXME: Should be EXIT_FAILURE
        }

        window_data.rect.x = 0;
        window_data.rect.y = 0;
        tig_window_blit_art(intgame_interface_windows[index], &art_blit_info);

        // CE: bottom strip opts into per-pixel see-through for the
        // panel art's near-black background. Threshold 8 catches the
        // mixed near-black shades (010008, 000808, 000000) exactly
        // without snagging dark chrome detail; opacity 128 = 50%
        // src / 50% dst. The iso window is the underlay — the
        // blend samples its VB directly (which always has the current
        // world content) instead of reading the stale screen surface.
        // Gated by the TranslucentBlackUI cfg flag.
        if (index == 1 && settings_get_value(&settings, TRANSLUCENT_BLACK_UI_KEY)) {
            tig_window_near_black_alpha_set(intgame_interface_windows[index],
                true, intgame_iso_window, 8, 128);
        }
    }

    for (index = 0; index < 5; index++) {
        intgame_button_create(&(intgame_maintain_buttons[index]));
        tig_button_hide(intgame_maintain_buttons[index].button_handle);
    }

    for (index = 0; index < INTGAME_SECONDARY_BUTTON_COUNT; index++) {
        if (index == INTGAME_SECONDARY_BUTTON_SKILLS
            || index == INTGAME_SECONDARY_BUTTON_SPELLS) {
            button_create_flags(&(intgame_secondary_buttons[index]), 0x2);
        } else {
            intgame_button_create(&(intgame_secondary_buttons[index]));
        }
    }

    for (index = 0; index < INTGAME_PRIMARY_BUTTON_COUNT; index++) {
        intgame_button_create(&(intgame_primary_buttons[index]));
    }

    intgame_button_create(&intgame_sleep_button_info);
    intgame_action_buttons_create();

    intgame_iso_interface_created = true;
    intgame_selected_spell_college = 0;

    for (index = 0; index < 11; index++) {
        iwid = find_interface_window_index(intgame_rotwin_text_frame[index].rect.x, intgame_rotwin_text_frame[index].rect.y);
        if (iwid == -1) {
            tig_debug_printf("iso_interface_create: ERROR: find iwid match!\n");
            exit(EXIT_SUCCESS); // FIXME: Should be EXIT_FAILURE
        }

        intgame_rotwin_text_frame[index].rect.x -= intgame_interface_window_frames[iwid].x;
        intgame_rotwin_text_frame[index].rect.y -= intgame_interface_window_frames[iwid].y;
        intgame_rotwin_text_frame[index].window_handle = intgame_interface_windows[iwid];
    }

    for (index = 0; index < INTGAME_COUNTER_COUNT; index++) {
        iwid = find_interface_window_index(intgame_number_boxes[index].rect.x, intgame_number_boxes[index].rect.y);
        if (iwid == -1) {
            tig_debug_printf("iso_interface_create: ERROR: find iwid match!\n");
            exit(EXIT_SUCCESS); // FIXME: Should be EXIT_FAILURE
        }

        intgame_number_boxes[index].rect.x -= intgame_interface_window_frames[iwid].x;
        intgame_number_boxes[index].rect.y -= intgame_interface_window_frames[iwid].y;
        intgame_number_boxes[index].window_handle = intgame_interface_windows[iwid];
    }

    hotkey_ui_start(intgame_interface_windows[1], &(intgame_interface_window_frames[1]), intgame_interface_windows[1], false);
    intgame_button_create(&intgame_legacy_action_button);
    intgame_button_create(&intgame_mt_button_info);
    intgame_mt_button_disable();

    intgame_mode_state_reset(&intgame_mode_state);
    target_flags_set(TGT_OBJECT | TGT_OBJ_NO_T_WALL | TGT_TILE);
    intgame_pc_lens_mode = PC_LENS_MODE_NONE;

    font_desc.str = NULL;
    tig_font_measure(&font_desc);
    tig_art_interface_id_create(171, 0, 0, 0, &(font_desc.art_id));
    tig_font_create(&font_desc, &intgame_cloister18_font);

    intgame_clock_button_info.x = intgame_clock_frame.x;
    intgame_clock_button_info.y = intgame_clock_frame.y;
    button_create_no_art(&intgame_clock_button_info, intgame_clock_frame.width, intgame_clock_frame.height);

    if (tig_art_interface_id_create(207, 0, 0, 0, &art_id) != TIG_OK
        || tig_art_frame_data(art_id, &art_frame_data) != TIG_OK) {
        tig_debug_printf("iso_interface_create: ERROR: clock stuff failed!\n");
        exit(EXIT_FAILURE);
    }
    intgame_clock_strip_widths[0] = art_frame_data.width;

    if (tig_art_interface_id_create(208, 0, 0, 0, &art_id) != TIG_OK
        || tig_art_frame_data(art_id, &art_frame_data) != TIG_OK) {
        tig_debug_printf("iso_interface_create: ERROR: clock stuff failed!\n");
        exit(EXIT_FAILURE);
    }
    intgame_clock_strip_widths[1] = art_frame_data.width;

    intgame_clock_process_callback(NULL);
    intgame_draw_bar(INTGAME_BAR_HEALTH);
    intgame_draw_bar(INTGAME_BAR_FATIGUE);

    // NOTE: Looks meaningless.
    font_desc.str = NULL;
    tig_font_measure(&font_desc);
    font_desc.flags = ~TIG_FONT_SHADOW;
    tig_art_interface_id_create(230, 0, 0, 0, &(font_desc.art_id));

    intgame_counters_refresh();

    for (index = 0; index < MAX_MESSAGE_HISTORY_ITEMS; index++) {
        intgame_message_history[index].str = (char*)MALLOC(MAX_MESSAGE_HISTORY_STRING_SIZE);
    }

    intgame_mt_window_index = find_interface_window_index(intgame_rotwin_button_info[ROTWIN_TYPE_MAGICTECH].x, intgame_rotwin_button_info[ROTWIN_TYPE_MAGICTECH].y);
    if (intgame_mt_window_index == -1) {
        tig_debug_printf("Intgame: ERROR: Couldn't match magic-tech window!\n");
        exit(EXIT_FAILURE);
    }

    if (intgame_is_compact_interface()) {
        for (index = 0; index < 2; index++) {
            tig_window_hide(intgame_interface_windows[index]);
        }
    }

    for (index = 0; index < 5; index++) {
        window_data.flags = TIG_WINDOW_ALWAYS_ON_TOP;
        window_data.rect = intgame_maintain_window_rects[index];
        window_data.color_key = tig_color_make(5, 5, 5);
        hrp_apply(&(window_data.rect), GRAVITY_CENTER_HORIZONTAL | GRAVITY_TOP);

        if (tig_window_create(&window_data, &(intgame_maintain_fs_windows[index])) != TIG_OK) {
            tig_debug_printf("intgame_resize: ERROR: Couldn't create spellFSWid: %d!\n", index);
            tig_exit();
            return;
        }

        // FIXME: Wrong rect (should be at 0,0).
        tig_window_fill(intgame_maintain_fs_windows[index],
            &(intgame_maintain_window_rects[index]),
            tig_color_make(0, 0, 0));
    }

    for (index = 0; index < 5; index++) {
        intgame_button_create_ex(intgame_maintain_fs_windows[index],
            &(intgame_maintain_window_rects[index]),
            &(intgame_maintain_fs_buttons[index]),
            true);
        tig_button_hide(intgame_maintain_fs_buttons[index].button_handle);
    }

    intgame_hide();
}

// 0x54A9A0
void iso_interface_destroy(void)
{
    int index;

    intgame_action_tooltips_reset();

    if (intgame_iso_interface_created) {
        for (index = 0; index < 2; index++) {
            tig_window_destroy(intgame_interface_windows[index]);
        }

        tig_font_destroy(intgame_cloister18_font);

        for (index = 0; index < MAX_MESSAGE_HISTORY_ITEMS; index++) {
            FREE(intgame_message_history[index].str);
        }

        for (index = 0; index < 5; index++) {
            if (intgame_maintain_fs_windows[index] != TIG_WINDOW_HANDLE_INVALID) {
                tig_window_destroy(intgame_maintain_fs_windows[index]);
                intgame_maintain_fs_windows[index] = TIG_WINDOW_HANDLE_INVALID;
            }
        }
    }

    tig_video_buffer_destroy(intgame_vital_bar_staging_buffer);
}

// 0x54AA30
void intgame_mode_and_message_history_reset(void)
{
    intgame_mode_state_reset(&intgame_mode_state);
    intgame_message_history_size = 0;
    intgame_message_history_end = 0;
    intgame_message_history_curr = 0;
}

// 0x54AA60
bool intgame_button_create_ex(tig_window_handle_t window_handle, TigRect* rect, UiButtonInfo* button_info, unsigned int flags)
{
    TigButtonData button_data;

    button_data.flags = flags;
    button_data.window_handle = window_handle;
    button_data.x = button_info->x - rect->x;
    button_data.y = button_info->y - rect->y;
    tig_art_interface_id_create(button_info->art_num, 0, 0, 0, &(button_data.art_id));
    button_data.mouse_down_snd_id = SND_INTERFACE_BUTTON_MEDIUM;
    button_data.mouse_up_snd_id = SND_INTERFACE_BUTTON_MEDIUM_RELEASE;
    button_data.mouse_enter_snd_id = -1;
    button_data.mouse_exit_snd_id = -1;
    return tig_button_create(&button_data, &(button_info->button_handle)) == TIG_OK;
}

// 0x54AAE0
bool intgame_button_create(UiButtonInfo* button_info)
{
    int index;

    index = find_interface_window_index(button_info->x, button_info->y);
    if (index == -1) {
        return false;
    }

    return intgame_button_create_ex(intgame_interface_windows[index], &(intgame_interface_window_frames[index]), button_info, TIG_BUTTON_MOMENTARY);
}

// 0x54AB20
bool button_create_flags(UiButtonInfo* button_info, unsigned int flags)
{
    int index;

    index = find_interface_window_index(button_info->x, button_info->y);
    if (index == -1) {
        return false;
    }

    return intgame_button_create_ex(intgame_interface_windows[index], &(intgame_interface_window_frames[index]), button_info, flags);
}

// 0x54ABD0
bool button_create_no_art(UiButtonInfo* button_info, int width, int height)
{
    int index;
    TigButtonData button_data;

    index = find_interface_window_index(button_info->x, button_info->y);
    if (index == -1) {
        return false;
    }

    button_data.window_handle = intgame_interface_windows[index];
    button_data.x = button_info->x - intgame_interface_window_frames[index].x;
    button_data.y = button_info->y - intgame_interface_window_frames[index].y;
    button_data.width = width;
    button_data.height = height;
    button_data.flags = TIG_BUTTON_MOMENTARY;
    button_data.art_id = TIG_ART_ID_INVALID;
    button_data.mouse_down_snd_id = -1;
    button_data.mouse_up_snd_id = -1;
    button_data.mouse_enter_snd_id = -1;
    button_data.mouse_exit_snd_id = -1;
    return tig_button_create(&button_data, &(button_info->button_handle)) == TIG_OK;
}

// 0x54AC70
void intgame_button_destroy(UiButtonInfo* button_info)
{
    tig_button_destroy(button_info->button_handle);
    button_info->button_handle = TIG_BUTTON_HANDLE_INVALID;
}

// 0x54AD00
void intgame_draw_counter(int counter, int value, int digits)
{
    IntgameIsoWindowTypeInfo* info;
    TigRect rect;
    TigFont font_desc;
    char format[12];
    char str[80];
    int pos;

    if (!intgame_iso_interface_created) {
        return;
    }

    if (counter == INTGAME_COUNTER_MANA
        && intgame_iso_window_type != ROTWIN_TYPE_MAGICTECH) {
        return;
    }

    info = &(intgame_number_boxes[counter]);

    tig_font_push(intgame_cloister18_font);

    if (counter != INTGAME_COUNTER_POISON) {
        info->rect.height++;
        tig_window_fill(info->window_handle,
            &(info->rect),
            tig_color_make(0, 0, 0));
        info->rect.height--;
    }

    if (value < 0 && (counter == INTGAME_COUNTER_MANA || counter == INTGAME_COUNTER_MONEY)) {
        if (value == -1) {
            for (pos = 0; pos < digits; pos++) {
                str[pos] = '-';
            }
            str[pos] = '\0';
        } else {
            for (pos = 0; pos < digits; pos++) {
                str[pos] = '?';
            }
            str[pos] = '\0';
        }
    } else {
        snprintf(format, sizeof(format), "%%0%dd", digits);
        snprintf(str, sizeof(str), format, value);
    }

    font_desc.str = str;
    font_desc.width = 0;
    tig_font_measure(&font_desc);

    if (font_desc.width < info->rect.width) {
        rect.x = info->rect.x + (info->rect.width - font_desc.width) / 2;
        rect.y = info->rect.y + (info->rect.height - font_desc.height) / 2;
        rect.width = font_desc.width;
        rect.height = font_desc.height;
    } else {
        font_desc.width = info->rect.width;
        tig_font_measure(&font_desc);

        if (font_desc.height > info->rect.height) {
            tig_font_pop();
            return;
        }

        rect.x = info->rect.x;
        rect.y = info->rect.y + (info->rect.height - font_desc.height) / 2;
        rect.width = font_desc.width;
        rect.height = font_desc.height;
    }

    tig_window_text_write(info->window_handle, str, &rect);
    tig_font_pop();
}

// 0x54AEE0
void intgame_draw_bar(int bar)
{
    switch (bar) {
    case INTGAME_BAR_HEALTH:
        intgame_draw_bar_rect(&intgame_health_bar_frame);
        break;
    case INTGAME_BAR_FATIGUE:
        intgame_draw_bar_rect(&intgame_fatigue_bar_frame);
        break;
    }
}

// 0x54AF10
void intgame_draw_bar_rect(TigRect* rect)
{
    int64_t pc_obj;
    TigRect rects[INTGAME_BAR_COUNT];
    int nums[INTGAME_BAR_COUNT];
    int poison;
    int bar;
    TigArtBlitInfo art_blit_info;
    TigRect blit_rect;
    TigRect tmp_rect;
    TigRect dst_rect;
    int value;
    int fullness;
    int filled_height;
    int empty_height;

    if (!intgame_vital_bars_enabled) {
        return;
    }

    pc_obj = player_get_local_pc_obj();
    if (pc_obj == OBJ_HANDLE_NULL) {
        return;
    }

    rects[INTGAME_BAR_HEALTH] = intgame_health_bar_frame;
    rects[INTGAME_BAR_FATIGUE] = intgame_fatigue_bar_frame;

    poison = stat_level_get(pc_obj, STAT_POISON_LEVEL);
    nums[INTGAME_BAR_HEALTH] = poison > 0 ? 17 : 18;
    nums[INTGAME_BAR_FATIGUE] = 19;

    for (bar = 0; bar < INTGAME_BAR_COUNT; bar++) {
        tmp_rect = rects[bar];
        if (tig_rect_intersection(&tmp_rect, rect, &blit_rect) == TIG_OK) {
            blit_rect.x -= intgame_interface_window_frames[1].x;
            blit_rect.y -= intgame_interface_window_frames[1].y;
            tig_art_interface_id_create(184, 0, 0, 0, &(art_blit_info.art_id));

            art_blit_info.src_rect = &blit_rect;
            art_blit_info.dst_rect = &blit_rect;
            art_blit_info.flags = 0;
            tig_window_blit_art(intgame_interface_windows[1], &art_blit_info);
        }

        if (bar == INTGAME_BAR_HEALTH) {
            value = object_hp_max(pc_obj);
            if (value != 0) {
                fullness = 100 * object_hp_current(pc_obj) / value;
            } else {
                fullness = 50;
            }
        } else {
            value = critter_fatigue_max(pc_obj);
            if (value != 0) {
                fullness = 100 * critter_fatigue_current(pc_obj) / value;
            } else {
                fullness = 50;
            }
        }

        filled_height = fullness * tmp_rect.height / 100;
        empty_height = tmp_rect.height - filled_height;
        if (empty_height > 0) {
            tmp_rect.x = rects[bar].x;
            tmp_rect.y = rects[bar].y;
            tmp_rect.width = rects[bar].width;
            tmp_rect.height = empty_height;

            if (tig_rect_intersection(&tmp_rect, rect, &blit_rect) == TIG_OK) {
                tmp_rect.x = blit_rect.x - tmp_rect.x;
                tmp_rect.y = blit_rect.y - tmp_rect.y;
                tmp_rect.width = blit_rect.width;
                tmp_rect.height = blit_rect.height;
                tig_art_interface_id_create(20, 0, 0, 0, &(art_blit_info.art_id));

                dst_rect.width = tmp_rect.width;
                dst_rect.height = tmp_rect.height;
                dst_rect.x = blit_rect.x - intgame_interface_window_frames[1].x;
                dst_rect.y = blit_rect.y - intgame_interface_window_frames[1].y;

                art_blit_info.flags = 0;
                art_blit_info.src_rect = &tmp_rect;
                art_blit_info.dst_rect = &dst_rect;
                tig_window_blit_art(intgame_interface_windows[1], &art_blit_info);
            }
        }

        if (filled_height > 0) {
            int top_cap_height;
            int visible_fill_height;
            int empty_top_offset;

            tmp_rect.x = rects[bar].x;
            tmp_rect.y = rects[bar].y;
            tmp_rect.width = rects[bar].width;

            top_cap_height = 8;
            visible_fill_height = filled_height + 8;
            empty_top_offset = empty_height - 8;
            if (visible_fill_height > rects[bar].height) {
                top_cap_height += rects[bar].height - visible_fill_height;
                empty_top_offset += visible_fill_height - rects[bar].height;
                visible_fill_height = rects[bar].height;
            }
            tmp_rect.y += empty_top_offset;
            tmp_rect.height = visible_fill_height;

            if (tig_rect_intersection(&tmp_rect, rect, &blit_rect) == TIG_OK) {
                tmp_rect.x = blit_rect.x - tmp_rect.x;
                tmp_rect.y = blit_rect.y - top_cap_height - tmp_rect.y + 8;
                tmp_rect.width = blit_rect.width;
                tmp_rect.height = blit_rect.height;
                tig_art_interface_id_create(nums[bar], 0, 0, 0, &(art_blit_info.art_id));

                dst_rect.x = blit_rect.x - intgame_interface_window_frames[1].x;
                dst_rect.width = tmp_rect.width;
                dst_rect.y = blit_rect.y - intgame_interface_window_frames[1].y;
                dst_rect.height = tmp_rect.height;

                art_blit_info.flags = 0;
                art_blit_info.src_rect = &tmp_rect;
                art_blit_info.dst_rect = &dst_rect;
                tig_window_blit_art(intgame_interface_windows[1], &art_blit_info);
            }
        }

        if (bar == INTGAME_BAR_HEALTH) {
            intgame_draw_counter(INTGAME_COUNTER_HEALTH, object_hp_current(pc_obj), 3);
            if (poison > 0) {
                intgame_draw_counter(INTGAME_COUNTER_POISON, poison, 3);
            }
        } else {
            intgame_draw_counter(INTGAME_COUNTER_FATIGUE, critter_fatigue_current(pc_obj), 3);
        }
    }

    compact_ui_draw();
}

// 0x54B3A0
void intgame_draw_bars(void)
{
    intgame_draw_bar(INTGAME_BAR_HEALTH);
    intgame_draw_bar(INTGAME_BAR_FATIGUE);
}

// 0x54B3C0
void intgame_counters_refresh(void)
{
    int qty;
    int art_num;
    int64_t pc_obj;
    int64_t item_obj;
    int ammo_type;
    tig_art_id_t art_id;
    int mana;

    qty = 0;
    art_num = 474;
    pc_obj = player_get_local_pc_obj();

    if (pc_obj != OBJ_HANDLE_NULL) {
        item_obj = item_wield_get(pc_obj, ITEM_INV_LOC_WEAPON);
        if (item_obj != OBJ_HANDLE_NULL) {
            ammo_type = item_weapon_ammo_type(item_obj);
            if (ammo_type != 10000) {
                qty = item_ammo_quantity_get(pc_obj, ammo_type);
                art_num = intgame_ammo_icon_art_numbers[ammo_type];
            } else {
                qty = obj_field_int32_get(item_obj, OBJ_F_ITEM_MANA_STORE);
                if (qty > 0) {
                    art_num = 469;
                } else {
                    qty = item_gold_get(pc_obj);
                }
            }
        } else {
            qty = item_gold_get(pc_obj);
        }
    }

    intgame_draw_counter(INTGAME_COUNTER_MONEY, qty, 6);
    tig_art_interface_id_create(art_num, 0, 0, 0, &art_id);
    intgame_ammo_icon_refresh(art_id);

    if (intgame_magictech_item_obj != OBJ_HANDLE_NULL
        && intgame_iso_window_type == ROTWIN_TYPE_MAGICTECH) {
        if ((obj_field_int32_get(intgame_magictech_item_obj, OBJ_F_ITEM_FLAGS) & OIF_IDENTIFIED) != 0) {
            mana = obj_field_int32_get(intgame_magictech_item_obj, OBJ_F_ITEM_SPELL_MANA_STORE);
            if (mana >= 0) {
                intgame_draw_counter(INTGAME_COUNTER_MANA, mana, 3);
            } else {
                intgame_draw_counter(INTGAME_COUNTER_MANA, -1, 3);
            }
        } else {
            intgame_draw_counter(INTGAME_COUNTER_MANA, -2, 3);
        }
    }
}

// 0x54B500
void intgame_ammo_icon_refresh(tig_art_id_t art_id)
{
    int index;
    TigRect src_rect;
    TigRect dst_rect;
    TigArtFrameData art_frame_data;
    TigArtBlitInfo blt;

    index = find_interface_window_index(intgame_ammo_button_info.x, intgame_ammo_button_info.y);
    if (index == -1) {
        tig_debug_printf("intgame_ammo_icon_refresh: ERROR: couldn't find iwid match!\n");
        exit(EXIT_SUCCESS); // FIXME: Should be EXIT_FAILURE.
    }

    blt.art_id = art_id;
    tig_art_frame_data(art_id, &art_frame_data);

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = art_frame_data.width;
    src_rect.height = art_frame_data.height;

    dst_rect.x = intgame_ammo_button_info.x - intgame_interface_window_frames[index].x;
    dst_rect.y = intgame_ammo_button_info.y - intgame_interface_window_frames[index].y;
    dst_rect.width = art_frame_data.width;
    dst_rect.height = art_frame_data.height;

    blt.flags = 0;
    blt.src_rect = &src_rect;
    blt.dst_rect = &dst_rect;
    tig_window_blit_art(intgame_interface_windows[index], &blt);
}

static void intgame_shader_toggle(const char* key, int feedback_base)
{
    bool enabled = settings_get_value(&settings, key) == 0;
    settings_set_value(&settings, key, enabled);
    if (strcmp(key, DAY_NIGHT_SHADER_KEY) == 0) light_scheme_day_night_tick();
    UiMessage feedback = { 0 };
    feedback.type = UI_MSG_TYPE_FEEDBACK;
    feedback.str = (char*)mes_localized_get("shader_ui", feedback_base + (enabled ? 1 : 0));
    intgame_message_window_display_msg(&feedback);
}

static bool intgame_handle_view_controls(TigMessage* msg)
{
    if (mainmenu_ui_is_active() || textedit_ui_is_focused()
        || !intgame_mode_supports_scrolling(intgame_mode_get())
        || intgame_mode_get() == INTGAME_MODE_WMAP) {
        return false;
    }
    if (msg->type == TIG_MESSAGE_MOUSE && msg->data.mouse.event == TIG_MESSAGE_MOUSE_WHEEL
        && tig_kb_get_modifier(SDL_KMOD_CTRL) && intgame_mouse_message_is_over_window(msg)) {
        iso_zoom_wheel(msg->data.mouse.dy);
        gamelib_invalidate_rect(NULL);
        return true;
    }
    if (msg->type != TIG_MESSAGE_KEYBOARD) {
        return false;
    }
    bool zoom_in = input_binding_matches_keyboard(INPUT_ACTION_ZOOM_IN, msg);
    bool zoom_out = input_binding_matches_keyboard(INPUT_ACTION_ZOOM_OUT, msg);
    bool zoom_reset = input_binding_matches_keyboard(INPUT_ACTION_ZOOM_RESET, msg);
    bool cycle_hud = input_binding_matches_keyboard(INPUT_ACTION_CYCLE_HUD, msg);
    bool toggle_shader = input_binding_matches_keyboard(INPUT_ACTION_TOGGLE_DAY_NIGHT_SHADER, msg);
    bool toggle_water = input_binding_matches_keyboard(INPUT_ACTION_TOGGLE_WATER_SHADER, msg);
    if (!zoom_in && !zoom_out && !zoom_reset && !cycle_hud && !toggle_shader && !toggle_water) {
        return false;
    }
    if (!msg->data.keyboard.pressed) {
        if (zoom_in) iso_zoom_step_in();
        if (zoom_out) iso_zoom_step_out();
        if (zoom_reset) iso_zoom_reset();
        if (cycle_hud) intgame_hud_user_toggle();
        if (toggle_shader) intgame_shader_toggle(DAY_NIGHT_SHADER_KEY, 0);
        if (toggle_water) intgame_shader_toggle(WATER_SHADER_KEY, 2);
        gamelib_invalidate_rect(NULL);
    }
    return true;
}

static bool intgame_handle_camera_pan(TigMessage* msg)
{
    if (intgame_mode_get() != INTGAME_MODE_MAIN
        || mainmenu_ui_is_active()
        || intgame_dialog_process_event_func != NULL
        || inven_ui_drag_item_obj_get() != OBJ_HANDLE_NULL
        || (combat_turn_based_is_active()
            && !player_is_local_pc_obj(combat_turn_based_whos_turn_get()))) {
        intgame_camera_pan_active = false;
        intgame_camera_pan_moved = false;
        return false;
    }

    if (msg->type != TIG_MESSAGE_MOUSE) {
        return false;
    }

    switch (msg->data.mouse.event) {
    case TIG_MESSAGE_MOUSE_RIGHT_BUTTON_DOWN:
        if (!msg->data.mouse.repeat) {
            intgame_camera_pan_active = intgame_mouse_message_is_over_window(msg) && !intgame_mouse_movement_active;
            intgame_camera_pan_moved = false;
            intgame_camera_pan_x = msg->data.mouse.x;
            intgame_camera_pan_y = msg->data.mouse.y;
        }
        return intgame_camera_pan_active;
    case TIG_MESSAGE_MOUSE_MOVE:
        if (intgame_camera_pan_active) {
            int dx = msg->data.mouse.x - intgame_camera_pan_x;
            int dy = msg->data.mouse.y - intgame_camera_pan_y;

            // Preserve ordinary right clicks despite small mouse jitter.
            if (!intgame_camera_pan_moved && dx > -3 && dx < 3 && dy > -3 && dy < 3) {
                return true;
            }

            intgame_camera_pan_moved = true;
            intgame_camera_pan_x = msg->data.mouse.x;
            intgame_camera_pan_y = msg->data.mouse.y;
            scroll_from_mouse_drag(dx, dy);
            return true;
        }
        break;
    case TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP: {
        bool moved = intgame_camera_pan_moved;
        intgame_camera_pan_active = false;
        intgame_camera_pan_moved = false;
        // A drag must not also rotate the player or toggle combat on release.
        return moved;
    }
    case TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN:
    case TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP:
        return intgame_camera_pan_active;
    default:
        break;
    }

    return false;
}

// 0x54B5D0
bool intgame_message_filter(TigMessage* msg)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    TigButtonState button_state;
    TigWindowData window_data;
    int index;
    DateTime datetime;
    tig_art_id_t art_id;
    char time_str_buffer[80];
    char buffer[80];

    if (intgame_handle_view_controls(msg) || intgame_handle_camera_pan(msg)) {
        return true;
    }

    if (scrollbar_ui_process_event(msg)
        || handle_button_unhover(msg)
        || hotkey_ui_process_event(msg)) {
        return true;
    }

    if (intgame_dialog_process_event_func != NULL) {
        if (msg->type != TIG_MESSAGE_KEYBOARD) {
            return intgame_dialog_process_event_func(msg);
        }

        if (msg->data.keyboard.pressed) {
            return intgame_dialog_process_event_func(msg);
        }

        if (!input_binding_matches_keyboard(INPUT_ACTION_MAIN_MENU, msg)
            && !input_binding_matches_keyboard(INPUT_ACTION_OPTIONS, msg)) {
            return intgame_dialog_process_event_func(msg);
        }

        return false;
    }

    if (combat_turn_based_is_active()) {
        if (!player_is_local_pc_obj(combat_turn_based_whos_turn_get())) {
            if (msg->type == TIG_MESSAGE_KEYBOARD
                && msg->data.keyboard.pressed
                && input_binding_matches_keyboard(INPUT_ACTION_TOGGLE_COMBAT, msg)) {
                combat_turn_based_toggle();
                return true;
            }
            return false;
        }
    }

    if (msg->type == TIG_MESSAGE_MOUSE) {
        if (msg->data.mouse.event == TIG_MESSAGE_MOUSE_MOVE || mainmenu_ui_is_active()) {
            return false;
        }

        if (msg->data.mouse.event == TIG_MESSAGE_MOUSE_IDLE) {
            if (intgame_action_heading_message_id != -1) {
                mes_file_entry.num = intgame_action_heading_message_id;
                ui_message.type = UI_MSG_TYPE_FEEDBACK;
                if (mes_search(intgame_mes_file, &mes_file_entry)) {
                    ui_message.str = mes_file_entry.str;
                    intgame_message_window_display_msg(&ui_message);
                }
            }

            if (msg->data.mouse.y < intgame_interface_window_frames[1].y) {
                if (intgame_iso_window_type == ROTWIN_TYPE_SPELLS
                    || intgame_iso_window_type == ROTWIN_TYPE_SKILLS) {
                    iso_interface_window_set(ROTWIN_TYPE_MSG);
                    return false;
                }
            } else if (msg->data.mouse.x >= 10
                && msg->data.mouse.x <= 790
                && msg->data.mouse.y <= 590
                && intgame_iso_window_type == ROTWIN_TYPE_MSG) {
                if (tig_button_state_get(intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SPELLS].button_handle, &button_state) == TIG_OK
                    && button_state == TIG_BUTTON_STATE_PRESSED) {
                    iso_interface_window_set(ROTWIN_TYPE_SPELLS);
                } else if (tig_button_state_get(intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SKILLS].button_handle, &button_state) == TIG_OK
                    && button_state == TIG_BUTTON_STATE_PRESSED) {
                    iso_interface_window_set(ROTWIN_TYPE_SKILLS);
                } else {
                    return false;
                }

                if (inven_ui_drag_item_obj_get() == OBJ_HANDLE_NULL) {
                    intgame_refresh_cursor();
                    return false;
                }
            }

            return false;
        }

        if (input_binding_matches_mouse(INPUT_ACTION_CENTER_CAMERA, msg)) {
            intgame_center_on_player();
            return true;
        }

        if (input_binding_matches_mouse(INPUT_ACTION_TOGGLE_COMBAT, msg)) {
            intgame_combat_mode_toggle();
            return true;
        }

        switch (msg->data.mouse.event) {
        case TIG_MESSAGE_MOUSE_RIGHT_BUTTON_DOWN:
            if (inven_ui_drag_item_obj_get() != OBJ_HANDLE_NULL) {
                return false;
            }

            switch (intgame_mode_get()) {
            case INTGAME_MODE_WMAP:
            case INTGAME_MODE_LOGBOOK:
            case INTGAME_MODE_CHAREDIT:
            case INTGAME_MODE_SCHEMATIC:
            case INTGAME_MODE_WRITTEN:
                if (tig_window_data(intgame_iso_window_handle, &window_data) == TIG_OK) {
                    if (msg->data.mouse.x < window_data.rect.x
                        || msg->data.mouse.x - window_data.rect.x > window_data.rect.width) {
                        intgame_mode_set(INTGAME_MODE_MAIN);
                    }
                    if (msg->data.mouse.y < window_data.rect.y
                        || msg->data.mouse.y - window_data.rect.y > window_data.rect.height) {
                        intgame_mode_set(INTGAME_MODE_MAIN);
                    }
                }
                break;
            default:
                break;
            }

            if (hotkey_ui_begin_drag()) {
                return true;
            }
            break;
        case TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP:
            if (inven_ui_drag_item_obj_get() != OBJ_HANDLE_NULL) {
                hotkey_ui_inventory_drag_begin();
            }
            if (hotkey_ui_drag_drop_process(TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP)) {
                return true;
            }
            break;
        case TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP:
            if (inven_ui_drag_item_obj_get() != OBJ_HANDLE_NULL) {
                hotkey_ui_inventory_drag_begin();
            }
            if (hotkey_ui_drag_drop_process(TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP)) {
                return true;
            }
            break;
        default:
            break;
        }

        return false;
    }

    if (msg->type == TIG_MESSAGE_BUTTON) {
        if (msg->data.button.state == TIG_BUTTON_STATE_RELEASED) {
            // CE: There is no dedicated "menu" button in the default UI, so
            // it's impossible to open the main menu to save/load game on mobile
            // platforms. For now, use the clock for that.
            if (msg->data.button.button_handle == intgame_clock_button_info.button_handle) {
                if (intgame_mode_get() == INTGAME_MODE_MAIN) {
                    if (dialog_ui_is_local_pc_in_dialog()
                        || wmap_ui_is_created()
                        || (combat_turn_based_is_active()
                            && player_get_local_pc_obj() != combat_turn_based_whos_turn_get())) {
                        mainmenu_ui_start(MM_TYPE_IN_PLAY_LOCKED);
                    } else {
                        mainmenu_ui_start(MM_TYPE_IN_PLAY);
                    }
                }
                return true;
            }

            if (msg->data.button.button_handle == intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SPELLS].button_handle) {
                iso_interface_window_set(ROTWIN_TYPE_SPELLS);
                return true;
            }

            if (msg->data.button.button_handle == intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SKILLS].button_handle) {
                iso_interface_window_set(ROTWIN_TYPE_SKILLS);
                return true;
            }

            if (msg->data.button.button_handle == intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_COMBAT].button_handle) {
                intgame_combat_mode_toggle();
                return true;
            }

            if (msg->data.button.button_handle == intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SCHEMATICS].button_handle) {
                schematic_ui_toggle(player_get_local_pc_obj(), player_get_local_pc_obj());
                return true;
            }

            if (msg->data.button.button_handle == intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_LOGBOOK].button_handle) {
                logbook_ui_open(player_get_local_pc_obj());
                return true;
            }

            if (msg->data.button.button_handle == intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_CHAR].button_handle) {
                intgame_character_sheet_open(player_get_local_pc_obj(), CHAREDIT_MODE_ACTIVE);
                return true;
            }

            if (msg->data.button.button_handle == intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_INVENTORY].button_handle) {
                inven_ui_open(player_get_local_pc_obj(), OBJ_HANDLE_NULL, INVEN_UI_MODE_INVENTORY);
                return true;
            }

            if (msg->data.button.button_handle == intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_MAP].button_handle) {
                wmap_ui_open();
                return true;
            }

            if (msg->data.button.button_handle == intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_FATE].button_handle) {
                fate_ui_toggle(player_get_local_pc_obj());
                return true;
            }

            if (msg->data.button.button_handle == intgame_sleep_button_info.button_handle) {
                sleep_ui_toggle(OBJ_HANDLE_NULL);
                return true;
            }

            if (msg->data.button.button_handle == intgame_mt_button_info.button_handle) {
                intgame_magictech_item_panel_open(OBJ_HANDLE_NULL);
                return true;
            }

            switch (intgame_iso_window_type) {
            case ROTWIN_TYPE_MSG:
                if (msg->data.button.button_handle == intgame_message_history_scroll_buttons[1].button_handle) {
                    if (inven_ui_drag_item_obj_get() == OBJ_HANDLE_NULL) {
                        intgame_message_history_scroll_up();
                        return true;
                    }
                    inven_ui_drag_cancel_and_restore();
                    intgame_refresh_cursor();
                    return true;
                }
                if (msg->data.button.button_handle == intgame_message_history_scroll_buttons[0].button_handle) {
                    if (inven_ui_drag_item_obj_get() == OBJ_HANDLE_NULL) {
                        intgame_message_history_scroll_down();
                        return true;
                    }
                    inven_ui_drag_cancel_and_restore();
                    intgame_refresh_cursor();
                    return true;
                }
                break;
            case ROTWIN_TYPE_SPELLS:
                for (index = 0; index < 5; index++) {
                    if (intgame_spell_buttons[5 * intgame_selected_spell_college + index].art_num != -1
                        && msg->data.button.button_handle == intgame_spell_buttons[5 * intgame_selected_spell_college + index].button_handle) {
                        if (spell_is_known(player_get_local_pc_obj(), 5 * intgame_selected_spell_college + index)) {
                            hotkey_ui_recent_action_set(3, 5 * intgame_selected_spell_college + index, OBJ_HANDLE_NULL);
                            spell_ui_activate(player_get_local_pc_obj(), 5 * intgame_selected_spell_college + index);
                            return true;
                        }
                    }
                }
                break;
            case ROTWIN_TYPE_SKILLS:
                for (index = 0; index < 4; index++) {
                    if (msg->data.button.button_handle == intgame_skill_buttons[index].button_handle) {
                        hotkey_ui_recent_action_set(2, index, OBJ_HANDLE_NULL);
                        skill_ui_action_activate(player_get_local_pc_obj(), index);
                        return true;
                    }
                }
                break;
            case ROTWIN_TYPE_CHAT:
                break;
            case ROTWIN_TYPE_MP_KICKBAN:
                break;
            case ROTWIN_TYPE_MAGICTECH:
                for (index = 0; index < 5; index++) {
                    if (intgame_mt_spell_buttons[index].art_num != -1
                        && msg->data.button.button_handle == intgame_mt_spell_buttons[index].button_handle) {
                        spell_ui_item_spell_activate(intgame_magictech_item_obj, index);
                        return true;
                    }
                }
                break;
            case ROTWIN_TYPE_MAP_NOTE:
                if (msg->data.button.button_handle == intgame_map_note_edit_buttons[0].button_handle) {
                    textedit_ui_clear();
                    return true;
                }

                if (msg->data.button.button_handle == intgame_map_note_edit_buttons[1].button_handle) {
                    textedit_ui_restore();
                    return true;
                }

                if (msg->data.button.button_handle == intgame_map_note_edit_buttons[2].button_handle) {
                    textedit_ui_submit();
                    return true;
                }
                break;
            case ROTWIN_TYPE_QUANTITY:
                if (msg->data.button.button_handle == intgame_quantity_buttons[INTGAME_QUANTITY_BUTTON_TAKE_ALL].button_handle) {
                    intgame_quantity = intgame_max_quantity;
                    intgame_refresh_quantity();
                    return true;
                }

                if (msg->data.button.button_handle == intgame_quantity_buttons[INTGAME_QUANTITY_BUTTON_OK].button_handle) {
                    inven_ui_item_transaction_send(intgame_quantity);
                    intgame_mode_set(INTGAME_MODE_MAIN);
                    return true;
                }

                if (msg->data.button.button_handle == intgame_quantity_buttons[INTGAME_QUANTITY_BUTTON_CANCEL].button_handle) {
                    intgame_mode_set(INTGAME_MODE_MAIN);
                    return true;
                }
                break;
            default:
                break;
            }

            for (index = 0; index < 5; index++) {
                if (msg->data.button.button_handle == intgame_maintain_buttons[index].button_handle) {
                    spell_ui_maintain_click(index);
                    return true;
                }
            }

            for (index = 0; index < 5; index++) {
                if (msg->data.button.button_handle == intgame_maintain_fs_buttons[index].button_handle) {
                    spell_ui_maintain_click(index);
                    return true;
                }
            }

            return false;
        } // msg->data.button.state == TIG_BUTTON_STATE_RELEASED

        if (msg->data.button.state == TIG_BUTTON_STATE_PRESSED) {
            if (msg->data.button.button_handle == intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SPELLS].button_handle) {
                if (tig_button_state_get(intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SKILLS].button_handle, &button_state) == TIG_OK
                    && button_state == TIG_BUTTON_STATE_PRESSED) {
                    tig_button_state_change(intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SKILLS].button_handle, TIG_BUTTON_STATE_RELEASED);
                }
                iso_interface_window_set(ROTWIN_TYPE_SPELLS);
                return true;
            }

            if (msg->data.button.button_handle == intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SKILLS].button_handle) {
                if (tig_button_state_get(intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SPELLS].button_handle, &button_state) == TIG_OK
                    && button_state == TIG_BUTTON_STATE_PRESSED) {
                    tig_button_state_change(intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SPELLS].button_handle, TIG_BUTTON_STATE_RELEASED);
                }
                iso_interface_window_set(ROTWIN_TYPE_SKILLS);
                return true;
            }

            switch (intgame_iso_window_type) {
            case ROTWIN_TYPE_SPELLS:
                for (index = 0; index < COLLEGE_COUNT; index++) {
                    if (spell_college_small_icon(index) != -1
                        && msg->data.button.button_handle == intgame_college_buttons[index].button_handle) {
                        intgame_spells_hide_college_spells(intgame_selected_spell_college);
                        intgame_selected_spell_college = index;
                        intgame_spells_show_college_spells(intgame_selected_spell_college);
                        return true;
                    }
                }
                break;
            case ROTWIN_TYPE_QUANTITY:
                if (msg->data.button.button_handle == intgame_quantity_buttons[INTGAME_QUANTITY_BUTTON_PLUS].button_handle) {
                    if (intgame_quantity < intgame_max_quantity) {
                        intgame_quantity++;
                        intgame_refresh_quantity();
                    }
                    return true;
                }
                if (msg->data.button.button_handle == intgame_quantity_buttons[INTGAME_QUANTITY_BUTTON_MINUS].button_handle) {
                    if (intgame_quantity > 0) {
                        intgame_quantity--;
                        intgame_refresh_quantity();
                    }
                    return true;
                }
                break;
            case ROTWIN_TYPE_MAP_NOTE:
                for (index = 3; index < 6; index++) {
                    if (msg->data.button.button_handle == intgame_map_note_edit_buttons[index].button_handle) {
                        wmap_ui_note_icon_select(index - 3);
                        return true;
                    }
                }
                break;
            default:
                break;
            }

            return false;
        } // msg->data.button.state == TIG_BUTTON_STATE_PRESSED

        if (msg->data.button.state == TIG_BUTTON_STATE_MOUSE_INSIDE) {
            if (msg->data.button.button_handle == intgame_clock_button_info.button_handle) {
                datetime = game_time_get();

                mes_file_entry.num = 22; // "Current Time"
                mes_get_msg(intgame_mes_file, &mes_file_entry);
                datetime_format_time(&datetime, time_str_buffer, sizeof(time_str_buffer));
                snprintf(buffer, sizeof(buffer),
                    "%s: %s",
                    mes_file_entry.str,
                    time_str_buffer);

                mes_file_entry.num = 23; // "Current Date"
                mes_get_msg(intgame_mes_file, &mes_file_entry);
                datetime_format_date(&datetime, time_str_buffer, sizeof(time_str_buffer));
                snprintf(&(buffer[strlen(buffer)]), sizeof(buffer) - strlen(buffer),
                    "   %s: %s",
                    mes_file_entry.str,
                    time_str_buffer);

                ui_message.type = UI_MSG_TYPE_FEEDBACK;
                ui_message.str = buffer;
                intgame_message_window_display_msg(&ui_message);
                return true;
            }

            if (msg->data.button.button_handle == intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SPELLS].button_handle) {
                intgame_action_heading_message_id = 1000; // "Spells"
                return true;
            }

            if (msg->data.button.button_handle == intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SKILLS].button_handle) {
                intgame_action_heading_message_id = 1001; // "Skills"
                return true;
            }

            if (msg->data.button.button_handle == intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_COMBAT].button_handle) {
                intgame_action_heading_message_id = 1002; // "Combat Toggle"
                return true;
            }

            if (msg->data.button.button_handle == intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SCHEMATICS].button_handle) {
                intgame_action_heading_message_id = 1003; // "Schematics"
                return true;
            }

            if (msg->data.button.button_handle == intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_LOGBOOK].button_handle) {
                intgame_action_heading_message_id = 1004; // "Log Book"
                return true;
            }

            if (msg->data.button.button_handle == intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_CHAR].button_handle) {
                intgame_action_heading_message_id = 1005; // "Character Editor"
                return true;
            }

            if (msg->data.button.button_handle == intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_INVENTORY].button_handle) {
                intgame_action_heading_message_id = 1006; // "Inventory"
                return true;
            }

            if (msg->data.button.button_handle == intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_MAP].button_handle) {
                intgame_action_heading_message_id = 1007; // "Maps"
                return true;
            }

            if (msg->data.button.button_handle == intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_FATE].button_handle) {
                intgame_action_heading_message_id = 1008; // "Fate Points"
                return true;
            }

            if (msg->data.button.button_handle == intgame_sleep_button_info.button_handle) {
                intgame_action_heading_message_id = (tig_net_is_active()) ? 1010 : 1009;
            }

            switch (intgame_iso_window_type) {
            case ROTWIN_TYPE_SPELLS:
                for (index = 0; index < 16; index++) {
                    if (msg->data.button.button_handle == intgame_college_buttons[index].button_handle) {
                        intgame_message_window_display_college(index);
                        return true;
                    }
                }
                for (index = 0; index < 5; index++) {
                    if (msg->data.button.button_handle == intgame_spell_buttons[5 * intgame_selected_spell_college + index].button_handle) {
                        intgame_message_window_display_spell(5 * intgame_selected_spell_college + index);
                        return true;
                    }
                }
                break;
            case ROTWIN_TYPE_MAGICTECH:
                for (index = 0; index < 5; index++) {
                    if (msg->data.button.button_handle == intgame_mt_spell_buttons[index].button_handle) {
                        intgame_message_window_display_spell(mt_item_spell(intgame_magictech_item_obj, index));
                        return true;
                    }
                }
                break;
            case ROTWIN_TYPE_SKILLS:
                for (index = 0; index < 4; index++) {
                    if (msg->data.button.button_handle == intgame_skill_buttons[index].button_handle) {
                        intgame_message_window_display_skill(index);
                        return true;
                    }
                }
                break;
            case ROTWIN_TYPE_MSG:
                if (msg->data.button.button_handle == intgame_message_history_scroll_buttons[1].button_handle) {
                    if (inven_ui_drag_item_obj_get() == OBJ_HANDLE_NULL) {
                        art_id = tig_mouse_cursor_get_art_id();
                        switch (tig_art_num_get(art_id)) {
                        case 0:
                        case 353:
                            tig_art_interface_id_create(498, 0, 0, 0, &art_id);
                            tig_mouse_hide();
                            tig_mouse_cursor_set_art_id(art_id);
                            tig_mouse_show();
                            break;
                        }
                    }
                    return true;
                }
                if (msg->data.button.button_handle == intgame_message_history_scroll_buttons[0].button_handle) {
                    if (inven_ui_drag_item_obj_get() == OBJ_HANDLE_NULL) {
                        art_id = tig_mouse_cursor_get_art_id();
                        switch (tig_art_num_get(art_id)) {
                        case 0:
                        case 353:
                            tig_art_interface_id_create(499, 0, 0, 0, &art_id);
                            tig_mouse_hide();
                            tig_mouse_cursor_set_art_id(art_id);
                            tig_mouse_show();
                            break;
                        }
                    }
                    return true;
                }
                break;
            default:
                break;
            }

            for (index = 0; index < 5; index++) {
                if (msg->data.button.button_handle == intgame_maintain_buttons[index].button_handle) {
                    spell_ui_maintain_hover(index);
                    return true;
                }
            }

            for (index = 0; index < 5; index++) {
                if (msg->data.button.button_handle == intgame_maintain_fs_buttons[index].button_handle) {
                    spell_ui_maintain_hover(index);
                    return true;
                }
            }

            return false;
        } // msg->data.button.state == TIG_BUTTON_STATE_MOUSE_INSIDE

        return false;
    } // msg->type == TIG_MESSAGE_BUTTON

    if (msg->type == TIG_MESSAGE_KEYBOARD) {
        if (textedit_ui_is_focused()) {
            return textedit_ui_process_message(msg);
        }

        if (msg->data.keyboard.pressed) {
            switch (msg->data.keyboard.scancode) {
            case SDL_SCANCODE_BACKSPACE:
            case SDL_SCANCODE_DELETE:
                if (intgame_iso_window_type == ROTWIN_TYPE_QUANTITY) {
                    intgame_quantity /= 10;
                    intgame_refresh_quantity();
                }
                return true;
            case SDL_SCANCODE_COMMA:
            case SDL_SCANCODE_PERIOD:
            case SDL_SCANCODE_SLASH:
                intgame_refresh_cursor();
                return false;
            case SDL_SCANCODE_KP_7:
                if (intgame_iso_window_type == ROTWIN_TYPE_QUANTITY) {
                    intgame_quantity = 10 * intgame_quantity + 7;
                    if (intgame_quantity > intgame_max_quantity) {
                        intgame_quantity = intgame_max_quantity;
                    }
                    intgame_refresh_quantity();
                }
                return true;
            case SDL_SCANCODE_KP_8:
                if (intgame_iso_window_type == ROTWIN_TYPE_QUANTITY) {
                    intgame_quantity = 10 * intgame_quantity + 8;
                    if (intgame_quantity > intgame_max_quantity) {
                        intgame_quantity = intgame_max_quantity;
                    }
                    intgame_refresh_quantity();
                }
                return true;
            case SDL_SCANCODE_KP_9:
                if (intgame_iso_window_type == ROTWIN_TYPE_QUANTITY) {
                    intgame_quantity = 10 * intgame_quantity + 9;
                    if (intgame_quantity > intgame_max_quantity) {
                        intgame_quantity = intgame_max_quantity;
                    }
                    intgame_refresh_quantity();
                }
                return true;
            case SDL_SCANCODE_KP_4:
                if (intgame_iso_window_type == ROTWIN_TYPE_QUANTITY) {
                    intgame_quantity = 10 * intgame_quantity + 4;
                    if (intgame_quantity > intgame_max_quantity) {
                        intgame_quantity = intgame_max_quantity;
                    }
                    intgame_refresh_quantity();
                }
                return true;
            case SDL_SCANCODE_KP_5:
                if (intgame_iso_window_type == ROTWIN_TYPE_QUANTITY) {
                    intgame_quantity = 10 * intgame_quantity + 5;
                    if (intgame_quantity > intgame_max_quantity) {
                        intgame_quantity = intgame_max_quantity;
                    }
                    intgame_refresh_quantity();
                }
                return true;
            case SDL_SCANCODE_KP_6:
                if (intgame_iso_window_type == ROTWIN_TYPE_QUANTITY) {
                    intgame_quantity = 10 * intgame_quantity + 6;
                    if (intgame_quantity > intgame_max_quantity) {
                        intgame_quantity = intgame_max_quantity;
                    }
                    intgame_refresh_quantity();
                }
                return true;
            case SDL_SCANCODE_KP_1:
                if (intgame_iso_window_type == ROTWIN_TYPE_QUANTITY) {
                    intgame_quantity = 10 * intgame_quantity + 1;
                    if (intgame_quantity > intgame_max_quantity) {
                        intgame_quantity = intgame_max_quantity;
                    }
                    intgame_refresh_quantity();
                }
                return true;
            case SDL_SCANCODE_KP_2:
                if (intgame_iso_window_type == ROTWIN_TYPE_QUANTITY) {
                    intgame_quantity = 10 * intgame_quantity + 2;
                    if (intgame_quantity > intgame_max_quantity) {
                        intgame_quantity = intgame_max_quantity;
                    }
                    intgame_refresh_quantity();
                }
                return true;
            case SDL_SCANCODE_KP_3:
                if (intgame_iso_window_type == ROTWIN_TYPE_QUANTITY) {
                    intgame_quantity = 10 * intgame_quantity + 3;
                    if (intgame_quantity > intgame_max_quantity) {
                        intgame_quantity = intgame_max_quantity;
                    }
                    intgame_refresh_quantity();
                }
                return true;
            case SDL_SCANCODE_KP_0:
                if (intgame_iso_window_type == ROTWIN_TYPE_QUANTITY) {
                    intgame_quantity = 10 * intgame_quantity;
                    if (intgame_quantity > intgame_max_quantity) {
                        intgame_quantity = intgame_max_quantity;
                    }
                    intgame_refresh_quantity();
                }
                return true;
            default:
                break;
            }
        } else {
            if (input_binding_matches_keyboard(INPUT_ACTION_SKILLS, msg)) {
                if (!mainmenu_ui_is_active()) {
                    intgame_secondary_button_toggle(INTGAME_SECONDARY_BUTTON_SKILLS, ROTWIN_TYPE_SKILLS);
                }
                return true;
            }

            if (input_binding_matches_keyboard(INPUT_ACTION_SPELLS, msg)) {
                if (!mainmenu_ui_is_active()) {
                    intgame_secondary_button_toggle(INTGAME_SECONDARY_BUTTON_SPELLS, ROTWIN_TYPE_SPELLS);
                }
                return true;
            }

            switch (msg->data.keyboard.scancode) {
            case SDL_SCANCODE_COMMA:
            case SDL_SCANCODE_PERIOD:
            case SDL_SCANCODE_SLASH:
                intgame_refresh_cursor();
                return false;
            default:
                break;
            }
        }

        if (msg->data.keyboard.pressed) {
            if (input_binding_matches_keyboard(INPUT_ACTION_TOGGLE_COMBAT, msg)) {
                combat_turn_based_toggle();
                gsound_play_sfx(0, 1);
                return true;
            }

            if (input_binding_matches_keyboard(INPUT_ACTION_CHARACTER, msg)) {
                intgame_character_sheet_open(player_get_local_pc_obj(), CHAREDIT_MODE_ACTIVE);
                gsound_play_sfx(0, 1);
                return true;
            }

            if (input_binding_matches_keyboard(INPUT_ACTION_INVENTORY, msg)) {
                inven_ui_open(player_get_local_pc_obj(), OBJ_HANDLE_NULL, INVEN_UI_MODE_INVENTORY);
                gsound_play_sfx(0, 1);
                return true;
            }

            if (input_binding_matches_keyboard(INPUT_ACTION_LOGBOOK, msg)) {
                logbook_ui_open(player_get_local_pc_obj());
                gsound_play_sfx(0, 1);
                return true;
            }

            if (input_binding_matches_keyboard(INPUT_ACTION_MAP, msg)) {
                wmap_ui_open();
                gsound_play_sfx(0, 1);
                return true;
            }

            if (input_binding_matches_keyboard(INPUT_ACTION_FATE, msg)) {
                fate_ui_toggle(player_get_local_pc_obj());
                gsound_play_sfx(0, 1);
                return true;
            }

            if (input_binding_matches_keyboard(INPUT_ACTION_COMBAT_MODE, msg)) {
                intgame_combat_mode_toggle();
                gsound_play_sfx(0, 1);
                return true;
            }

            if (input_binding_matches_keyboard(INPUT_ACTION_SLEEP, msg)) {
                // Plain S opens the sleep menu; let Ctrl/Cmd+S fall through
                // to main.c's Save Game shortcut.
                if (tig_kb_get_modifier(SDL_KMOD_CTRL | SDL_KMOD_GUI)) {
                    return false;
                }
                sleep_ui_toggle(OBJ_HANDLE_NULL);
                gsound_play_sfx(0, 1);
                return true;
            }

            if (input_binding_matches_keyboard(INPUT_ACTION_SCHEMATICS, msg)) {
                schematic_ui_toggle(player_get_local_pc_obj(), player_get_local_pc_obj());
                gsound_play_sfx(0, 1);
                return true;
            }

            if (input_binding_matches_keyboard(INPUT_ACTION_CHAT, msg)) {
                if (intgame_iso_window_type == ROTWIN_TYPE_QUANTITY) {
                    inven_ui_item_transaction_send(intgame_quantity);
                    intgame_mode_set(INTGAME_MODE_MAIN);
                    gsound_play_sfx(0, 1);
                } else if (!mainmenu_ui_is_active()) {
                    broadcast_ui_open();
                    gsound_play_sfx(0, 1);
                }
                return true;
            }
        }

        return false;
    } // msg->type == TIG_MESSAGE_KEYBOARD

    if (msg->type == TIG_MESSAGE_TEXT_INPUT) {
        return textedit_ui_process_message(msg);
    }

    return false;
}

// 0x54C8E0
bool iso_interface_message_filter(TigMessage* msg)
{
    // NOTE: Strange case - this function is huge but it's binary identical to
    // 0x54B5D0.
    return intgame_message_filter(msg);
}

static bool intgame_handle_center_camera_binding(TigMessage* msg)
{
    if ((msg->type == TIG_MESSAGE_KEYBOARD
            && !msg->data.keyboard.pressed
            && input_binding_matches_keyboard(INPUT_ACTION_CENTER_CAMERA, msg))
        || (msg->type == TIG_MESSAGE_MOUSE
            && input_binding_matches_mouse(INPUT_ACTION_CENTER_CAMERA, msg))) {
        intgame_center_on_player();
        return true;
    }

    return false;
}

static bool intgame_handle_follower_binding(TigMessage* msg)
{
    int index;
    int64_t loc;

    if (msg->type != TIG_MESSAGE_KEYBOARD
        || msg->data.keyboard.pressed) {
        return false;
    }

    index = input_binding_follower_index(msg);
    if (index == -1) {
        return false;
    }

    intgame_get_location_under_cursor(&loc);
    broadcast_follower_shortcut_execute(index, loc);

    return true;
}

// 0x54DBF0
void intgame_secondary_button_toggle(IntgameSecondaryButton btn, RotatingWindowType window_type)
{
    TigButtonState state;
    int opposite_btn;

    tig_button_state_get(intgame_secondary_buttons[btn].button_handle, &state);
    if (state != TIG_BUTTON_STATE_PRESSED) {
        opposite_btn = btn == INTGAME_SECONDARY_BUTTON_SPELLS
            ? INTGAME_SECONDARY_BUTTON_SKILLS
            : INTGAME_SECONDARY_BUTTON_SPELLS;
        tig_button_state_change(intgame_secondary_buttons[opposite_btn].button_handle, TIG_BUTTON_STATE_RELEASED);
        intgame_force_fullscreen();
        tig_button_state_change(intgame_secondary_buttons[btn].button_handle, TIG_BUTTON_STATE_PRESSED);
        iso_interface_window_set(window_type);
    } else {
        // CE: when the user is in MINI-peek for this same rotwin (TAB
        // expanded MINI to MEDIUM via re-press), a second re-press
        // should NOT dismiss the rotwin — it should collapse the crop
        // back to MINI while leaving SKILLS/SPELLS active. Otherwise
        // MINI's slim text row would lose its content. Button stays
        // PRESSED, fullscreen stays forced.
        if (intgame_iso_window_type == window_type
            && intgame_hud_handle_mini_peek_press()) {
            return;
        }
        iso_interface_window_set(ROTWIN_TYPE_MSG);
        tig_button_state_change(intgame_secondary_buttons[btn].button_handle, TIG_BUTTON_STATE_RELEASED);
        intgame_unforce_fullscreen();
    }
}

// 0x54DC80
bool handle_button_unhover(TigMessage* msg)
{
    int index;

    // FIX: Check for message type before checking for button state.
    if (msg->type != TIG_MESSAGE_BUTTON
        || msg->data.button.state != TIG_BUTTON_STATE_MOUSE_OUTSIDE) {
        return false;
    }

    intgame_pending_object_examine();

    if (msg->data.button.button_handle == intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SPELLS].button_handle
        || msg->data.button.button_handle == intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SKILLS].button_handle
        || msg->data.button.button_handle == intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_COMBAT].button_handle
        || msg->data.button.button_handle == intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SCHEMATICS].button_handle
        || msg->data.button.button_handle == intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_LOGBOOK].button_handle
        || msg->data.button.button_handle == intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_CHAR].button_handle
        || msg->data.button.button_handle == intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_INVENTORY].button_handle
        || msg->data.button.button_handle == intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_MAP].button_handle
        || msg->data.button.button_handle == intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_FATE].button_handle
        || msg->data.button.button_handle == intgame_sleep_button_info.button_handle) {
        intgame_action_heading_message_id = -1;
        intgame_message_window_clear();
        return true;
    }

    switch (intgame_iso_window_type) {
    case ROTWIN_TYPE_MSG:
        if (msg->data.button.button_handle == intgame_message_history_scroll_buttons[1].button_handle
            || msg->data.button.button_handle == intgame_message_history_scroll_buttons[0].button_handle) {
            if (inven_ui_drag_item_obj_get() == OBJ_HANDLE_NULL) {
                intgame_refresh_cursor();
            }
            return true;
        }
        if (msg->data.button.button_handle == intgame_clock_button_info.button_handle) {
            intgame_message_window_clear_internal();
        }
        break;
    case ROTWIN_TYPE_SPELLS:
    case ROTWIN_TYPE_SKILLS:
    case ROTWIN_TYPE_MAGICTECH:
        intgame_message_window_clear_internal();
        break;
    default:
        break;
    }

    for (index = 0; index < 5; index++) {
        if (msg->data.button.button_handle == intgame_maintain_buttons[index].button_handle) {
            intgame_message_window_clear_internal();
            return true;
        }
    }

    for (index = 0; index < 5; index++) {
        if (msg->data.button.button_handle == intgame_maintain_fs_buttons[index].button_handle) {
            intgame_message_window_clear_internal();
            return true;
        }
    }

    return false;
}

// 0x54DE50
void intgame_process_event(TigMessage* msg)
{
    int64_t pc_obj;
    int64_t loc = 0;
    TargetDescriptor td;
    TigMouseState mouse_state;
    TigMessage fake_mouse_move;

    pc_obj = player_get_local_pc_obj();

    if (combat_turn_based_is_active()) {
        if (combat_turn_based_whos_turn_get() != pc_obj) {
            return;
        }
    }

    if (msg->type == TIG_MESSAGE_KEYBOARD
        && !textedit_ui_is_focused()
        && !msg->data.keyboard.pressed) {
        int hotkey_index = input_binding_hotkey_index(msg);
        if (hotkey_index != -1) {
            hotkey_ui_slot_activate(hotkey_index);
        }
    }

    switch (intgame_mode_get()) {
    case INTGAME_MODE_MAIN:
        switch (msg->type) {
        case TIG_MESSAGE_MOUSE:
            if (intgame_handle_center_camera_binding(msg)) {
                break;
            }

            switch (msg->data.mouse.event) {
            case TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN:
                if (intgame_mouse_message_is_over_window(msg)
                    && intgame_target_pick_at_mouse(msg->data.mouse.x, msg->data.mouse.y, &td)) {
                    if (td.is_loc) {
                        if (inven_ui_drag_item_obj_get() != OBJ_HANDLE_NULL) {
                            int64_t dragged_item_obj;

                            dragged_item_obj = inven_ui_drag_item_obj_get();
                            if (hotkey_ui_is_dragging()) {
                                hotkey_ui_cancel_drag();
                                inven_ui_item_cursor_begin(dragged_item_obj, false);
                                if (inven_ui_drag_item_obj_get() != OBJ_HANDLE_NULL) {
                                    dragged_item_obj = inven_ui_drag_item_obj_get();
                                }
                            }

                            if (!hotkey_ui_is_dragging()) {
                                if (critter_is_active(pc_obj)) {
                                    inven_ui_drag_object_clear();
                                    intgame_refresh_cursor();
                                    anim_goal_throw_item(pc_obj, dragged_item_obj, td.loc);
                                } else {
                                    inven_ui_drag_cancel_and_restore();
                                }
                            }
                        } else if (critter_is_active(pc_obj)
                            && !tig_kb_get_modifier(SDL_KMOD_SHIFT)) {
                            if ((tig_kb_get_modifier(SDL_KMOD_CTRL)
                                    || input_binding_is_pressed(INPUT_ACTION_ALWAYS_RUN))
                                && !settings_get_value(&settings, ALWAYS_RUN_KEY)) {
                                anim_goal_run_to_tile(pc_obj, td.loc);
                            } else {
                                anim_goal_move_to_tile(pc_obj, td.loc);
                            }

                            if (intgame_mouse_movement_active) {
                                anim_last_goal_relaxed_pathfinding_enable_networked();
                            }
                            intgame_mouse_movement_active = true;
                        }
                    } else if (!intgame_mouse_movement_active) {
                        intgame_main_target_action_process(&td);
                    }
                }
                break;
            case TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP:
                intgame_mouse_movement_active = false;
                break;
            case TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP:
                if (pc_obj != OBJ_HANDLE_NULL) {
                    unsigned int spell_flags;
                    unsigned int critter_flags;

                    spell_flags = obj_field_int32_get(pc_obj, OBJ_F_SPELL_FLAGS);
                    critter_flags = obj_field_int32_get(pc_obj, OBJ_F_CRITTER_FLAGS);
                    if ((spell_flags & OSF_STONED) == 0
                        && (critter_flags & (OCF_PARALYZED | OCF_STUNNED)) == 0
                        && critter_is_active(pc_obj)
                        && !critter_is_prone(pc_obj)) {
                        if (combat_critter_is_combat_mode_active(pc_obj)) {
                            if (anim_is_current_goal_type(pc_obj, AG_ANIM_FIDGET, NULL)
                                || !anim_foreground_goal_find(pc_obj, 0)) {
                                intgame_combat_mode_toggle();
                            }
                        } else {
                            if (critter_is_concealed(pc_obj)
                                && !anim_foreground_goal_find(pc_obj, NULL)) {
                                critter_set_concealed(pc_obj, false);
                            }
                        }

                        if (anim_object_interrupt_by_priority(pc_obj, 3, 0, 0)) {
                            combat_refresh_fidget_if_allowed(pc_obj);

                            tig_mouse_get_state(&mouse_state);
                            if (location_at_zoomed(mouse_state.x, mouse_state.y, iso_zoom_current(), &loc)
                                && intgame_mouse_message_is_over_window(msg)) {
                                int64_t pc_loc;
                                tig_art_id_t aid;
                                int rot;

                                pc_loc = obj_field_int64_get(pc_obj, OBJ_F_LOCATION);
                                aid = obj_field_int32_get(pc_obj, OBJ_F_CURRENT_AID);
                                rot = location_rot(pc_loc, loc);
                                if (!anim_foreground_goal_find(pc_obj, 0)) {
                                    anim_goal_rotate(pc_obj, rot);
                                } else if (anim_is_current_goal_type(pc_obj, AG_ANIM_FIDGET, NULL)) {
                                    object_set_current_aid(pc_obj, tig_art_id_rotation_set(aid, rot));
                                }
                            }
                        }
                    }

                    if (inven_ui_drag_item_obj_get() != OBJ_HANDLE_NULL) {
                        inven_ui_drag_cancel_and_restore();
                        intgame_refresh_cursor();
                    }
                }
                break;
            case TIG_MESSAGE_MOUSE_WHEEL:
                if (iso_zoom_is_available()) {
                    iso_zoom_wheel(msg->data.mouse.dy);
                    gamelib_invalidate_rect(NULL);
                }
                break;
            case TIG_MESSAGE_MOUSE_IDLE:
                intgame_hover_refresh_from_message(msg);
            default:
                break;
            }
            break;
        case TIG_MESSAGE_KEYBOARD:
            if (!textedit_ui_is_focused()
                && !msg->data.keyboard.pressed) {
                intgame_handle_follower_binding(msg);
                intgame_handle_center_camera_binding(msg);
            }
            break;
        case TIG_MESSAGE_PING:
            if (tig_mouse_get_state(&mouse_state) == TIG_OK) {
                fake_mouse_move.timestamp = msg->timestamp;
                fake_mouse_move.type = TIG_MESSAGE_MOUSE;
                fake_mouse_move.data.mouse.x = mouse_state.x;
                fake_mouse_move.data.mouse.y = mouse_state.y;
                fake_mouse_move.data.mouse.event = TIG_MESSAGE_MOUSE_MOVE;
                intgame_object_hover_process(&fake_mouse_move);
            }
            break;
        default:
            break;
        }
        return;
    case INTGAME_MODE_SPELL:
        switch (msg->type) {
        case TIG_MESSAGE_MOUSE:
            switch (msg->data.mouse.event) {
            case TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP:
                if (!inven_ui_is_created()) {
                    int mx = msg->data.mouse.x;
                    int my = msg->data.mouse.y;
                    bool picked;
                    if (intgame_adjust_mouse_for_zoom(mx, my, &mx, &my)) {
                        picked = target_pick_at_virtual_xy(mx, my, &td, intgame_fullscreen);
                    } else {
                        picked = target_pick_at_screen_xy(mx, my, &td, intgame_fullscreen);
                    }
                    if (picked) {
                        spell_ui_apply(&td);
                    } else if (target_last_rejection_get() == 0x100000) {
                        spell_ui_error_target_not_damaged();
                    }
                }
                break;
            case TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP:
                spell_ui_cancel();
                break;
            case TIG_MESSAGE_MOUSE_IDLE:
                intgame_hover_refresh_from_message(msg);
                break;
            default:
                break;
            }
            break;
        case TIG_MESSAGE_KEYBOARD:
            if (!textedit_ui_is_focused()) {
                if (!msg->data.keyboard.pressed) {
                    switch (msg->data.keyboard.scancode) {
                    case SDL_SCANCODE_LALT:
                    case SDL_SCANCODE_RALT:
                        if (!tig_kb_get_modifier(SDL_KMOD_ALT)) {
                            spell_ui_aggressive_mode_on();
                        }
                        break;
                    default:
                        break;
                    }
                    intgame_handle_follower_binding(msg);
                    intgame_handle_center_camera_binding(msg);
                } else {
                    switch (msg->data.keyboard.scancode) {
                    case SDL_SCANCODE_LALT:
                    case SDL_SCANCODE_RALT:
                        spell_ui_aggressive_mode_off();
                        break;
                    default:
                        break;
                    }
                }
            }
            break;
        case TIG_MESSAGE_PING:
            if (tig_mouse_get_state(&mouse_state) == TIG_OK) {
                fake_mouse_move.timestamp = msg->timestamp;
                fake_mouse_move.type = TIG_MESSAGE_MOUSE;
                fake_mouse_move.data.mouse.x = mouse_state.x;
                fake_mouse_move.data.mouse.y = mouse_state.y;
                fake_mouse_move.data.mouse.event = TIG_MESSAGE_MOUSE_MOVE;
                intgame_object_hover_process(&fake_mouse_move);
            }
            break;
        default:
            break;
        }
        return;
    case INTGAME_MODE_SKILL:
        switch (msg->type) {
        case TIG_MESSAGE_MOUSE:
            switch (msg->data.mouse.event) {
            case TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP: {
                int mx = msg->data.mouse.x;
                int my = msg->data.mouse.y;
                if (intgame_adjust_mouse_for_zoom(mx, my, &mx, &my)) {
                    if (target_pick_at_virtual_xy(mx, my, &td, intgame_fullscreen)) {
                        skill_ui_apply(&td);
                    }
                } else if (target_pick_at_screen_xy(mx, my, &td, intgame_fullscreen)) {
                    skill_ui_apply(&td);
                }
                break;
            }
            case TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP:
                skill_ui_cancel();
                break;
            case TIG_MESSAGE_MOUSE_IDLE:
                intgame_hover_refresh_from_message(msg);
                break;
            default:
                break;
            }
            break;
        case TIG_MESSAGE_KEYBOARD:
            if (!textedit_ui_is_focused()) {
                if (!msg->data.keyboard.pressed) {
                    intgame_handle_follower_binding(msg);
                    intgame_handle_center_camera_binding(msg);
                }
            }
            break;
        case TIG_MESSAGE_PING:
            if (tig_mouse_get_state(&mouse_state) == TIG_OK) {
                fake_mouse_move.timestamp = msg->timestamp;
                fake_mouse_move.type = TIG_MESSAGE_MOUSE;
                fake_mouse_move.data.mouse.x = mouse_state.x;
                fake_mouse_move.data.mouse.y = mouse_state.y;
                fake_mouse_move.data.mouse.event = TIG_MESSAGE_MOUSE_MOVE;
                intgame_object_hover_process(&fake_mouse_move);
            }
            break;
        default:
            break;
        }
        return;
    case INTGAME_MODE_DIALOG:
        switch (msg->type) {
        case TIG_MESSAGE_MOUSE:
            switch (msg->data.mouse.event) {
            case TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN:
                if (intgame_mouse_message_is_over_window(msg)) {
                    int mx = msg->data.mouse.x;
                    int my = msg->data.mouse.y;
                    bool picked;
                    if (intgame_adjust_mouse_for_zoom(mx, my, &mx, &my)) {
                        picked = target_pick_at_virtual_xy(mx, my, &td, intgame_fullscreen);
                    } else {
                        picked = target_pick_at_screen_xy(mx, my, &td, intgame_fullscreen);
                    }
                    if (picked
                        && td.is_loc
                        && !inven_ui_drag_item_obj_get()
                        && !critter_is_dead(pc_obj)
                        && !tig_kb_get_modifier(SDL_KMOD_SHIFT)) {
                        if ((tig_kb_get_modifier(SDL_KMOD_CTRL)
                                || input_binding_is_pressed(INPUT_ACTION_ALWAYS_RUN))
                            && !settings_get_value(&settings, ALWAYS_RUN_KEY)) {
                            anim_goal_run_to_tile(pc_obj, td.loc);
                        } else {
                            anim_goal_move_to_tile(pc_obj, td.loc);
                        }

                        if (intgame_mouse_movement_active) {
                            anim_last_goal_relaxed_pathfinding_enable_networked();
                        }
                        intgame_mouse_movement_active = true;
                    }
                }
                break;
            case TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP:
                intgame_mouse_movement_active = false;
                break;
            case TIG_MESSAGE_MOUSE_IDLE:
                intgame_hover_refresh_from_message(msg);
                break;
            default:
                break;
            }
            break;
        case TIG_MESSAGE_PING:
            if (intgame_dialog_process_event_func == NULL
                && tig_mouse_get_state(&mouse_state) == TIG_OK) {
                fake_mouse_move.timestamp = msg->timestamp;
                fake_mouse_move.type = TIG_MESSAGE_MOUSE;
                fake_mouse_move.data.mouse.x = mouse_state.x;
                fake_mouse_move.data.mouse.y = mouse_state.y;
                fake_mouse_move.data.mouse.event = TIG_MESSAGE_MOUSE_MOVE;
                intgame_object_hover_process(&fake_mouse_move);
            }
            break;
        default:
            break;
        }
        return;
    case INTGAME_MODE_ITEM:
        switch (msg->type) {
        case TIG_MESSAGE_MOUSE:
            switch (msg->data.mouse.event) {
            case TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP: {
                int mx = msg->data.mouse.x;
                int my = msg->data.mouse.y;
                bool picked;
                if (intgame_adjust_mouse_for_zoom(mx, my, &mx, &my)) {
                    picked = target_pick_at_virtual_xy(mx, my, &td, intgame_fullscreen);
                } else {
                    picked = target_pick_at_screen_xy(mx, my, &td, intgame_fullscreen);
                }
                if (picked) {
                    item_ui_apply(&td);
                } else if (target_last_rejection_get() == 0x100000) {
                    spell_ui_error_target_not_damaged();
                }
                break;
            }
            case TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP:
                item_ui_deactivate();
                break;
            case TIG_MESSAGE_MOUSE_IDLE:
                intgame_hover_refresh_from_message(msg);
                break;
            default:
                break;
            }
            break;
        case TIG_MESSAGE_PING:
            if (tig_mouse_get_state(&mouse_state) == TIG_OK) {
                fake_mouse_move.timestamp = msg->timestamp;
                fake_mouse_move.type = TIG_MESSAGE_MOUSE;
                fake_mouse_move.data.mouse.x = mouse_state.x;
                fake_mouse_move.data.mouse.y = mouse_state.y;
                fake_mouse_move.data.mouse.event = TIG_MESSAGE_MOUSE_MOVE;
                intgame_object_hover_process(&fake_mouse_move);
            }
            break;
        default:
            break;
        }
        return;
    case INTGAME_MODE_FOLLOWER:
        switch (msg->type) {
        case TIG_MESSAGE_MOUSE:
            switch (msg->data.mouse.event) {
            case TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP: {
                int mx = msg->data.mouse.x;
                int my = msg->data.mouse.y;
                if (intgame_adjust_mouse_for_zoom(mx, my, &mx, &my)) {
                    if (target_pick_at_virtual_xy(mx, my, &td, intgame_fullscreen)) {
                        follower_ui_execute_order(&td);
                    }
                } else if (target_pick_at_screen_xy(mx, my, &td, intgame_fullscreen)) {
                    follower_ui_execute_order(&td);
                }
                break;
            }
            case TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP:
                follower_ui_end_order_mode();
                break;
            case TIG_MESSAGE_MOUSE_IDLE:
                intgame_hover_refresh_from_message(msg);
                break;
            default:
                break;
            }
            break;
        case TIG_MESSAGE_KEYBOARD:
            if (textedit_ui_is_focused()
                && !msg->data.keyboard.pressed) {
                intgame_handle_follower_binding(msg);
                intgame_handle_center_camera_binding(msg);
            }
            break;
        case TIG_MESSAGE_PING:
            if (tig_mouse_get_state(&mouse_state) == TIG_OK) {
                fake_mouse_move.timestamp = msg->timestamp;
                fake_mouse_move.type = TIG_MESSAGE_MOUSE;
                fake_mouse_move.data.mouse.x = mouse_state.x;
                fake_mouse_move.data.mouse.y = mouse_state.y;
                fake_mouse_move.data.mouse.event = TIG_MESSAGE_MOUSE_MOVE;
                intgame_object_hover_process(&fake_mouse_move);
            }
            break;
        default:
            break;
        }
        return;
    default:
        break;
    }
}

// 0x54EA80
void intgame_target_action_dispatch(TargetDescriptor* td)
{
    int64_t pc_obj;
    TargetDescriptorEvaluation evaluation;

    pc_obj = player_get_local_pc_obj();

    if (!combat_turn_based_is_active()
        || combat_turn_based_whos_turn_get() == pc_obj) {
        evaluation.source_obj = pc_obj;
        evaluation.self_obj = pc_obj;
        evaluation.descriptor = td;

        switch (intgame_mode_get()) {
        case INTGAME_MODE_SPELL:
            if (target_descriptor_evaluate_current(&evaluation)) {
                spell_ui_apply(td);
            }
            break;
        case INTGAME_MODE_SKILL:
            if (target_descriptor_evaluate_current(&evaluation)) {
                skill_ui_apply(td);
            }
            break;
        case INTGAME_MODE_ITEM:
            if (target_descriptor_evaluate_current(&evaluation)) {
                item_ui_apply(td);
            }
            break;
        default:
            intgame_main_target_action_process(td);
            break;
        }
    }
}

// 0x54EB50
bool intgame_hotkey_is_dragging(void)
{
    return hotkey_ui_is_dragging();
}

// 0x54EB60
void intgame_center_on_player(void)
{
    int64_t obj;
    int64_t loc;
    int64_t x;
    int64_t y;

    obj = player_get_local_pc_obj();
    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    camera_follow_note_recenter();
    loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
    location_calc_dist_from_screen_center(loc, &x, &y);

    if (x != 0 || y != 0) {
        location_origin_set(loc);
        iso_redraw();
    } else {
        if (combat_turn_based_is_active()) {
            obj = combat_turn_based_whos_turn_get();
            if (obj != OBJ_HANDLE_NULL) {
                loc = obj_field_int64_get(obj, OBJ_F_LOCATION);
                location_origin_set(loc);
                iso_redraw();
            }
        }
    }
}

// 0x54EBF0
void intgame_combat_mode_toggle(void)
{
    int64_t pc_obj;
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    int64_t obj;

    pc_obj = player_get_local_pc_obj();
    if (critter_is_dead(pc_obj)) {
        return;
    }

    if (combat_critter_is_combat_mode_active(pc_obj)) {
        if (combat_can_exit_combat_mode(pc_obj)) {
            combat_critter_deactivate_combat_mode(pc_obj);
        } else {
            mes_file_entry.num = 24; // "You cannot exit combat-mode when under attack."
            mes_get_msg(intgame_mes_file, &mes_file_entry);

            ui_message.type = UI_MSG_TYPE_FEEDBACK;
            ui_message.str = mes_file_entry.str;
            intgame_message_window_display_msg(&ui_message);
        }
    } else {
        combat_critter_activate_combat_mode(pc_obj);
    }

    intgame_hover_refresh_from_mouse();

    obj = intgame_selected_obj;
    if (obj == OBJ_HANDLE_NULL) {
        obj = object_hover_obj_get();
    }

    if (obj != OBJ_HANDLE_NULL) {
        object_hover_obj_set(OBJ_HANDLE_NULL);
        object_hover_obj_set(obj);
        if (obj != OBJ_HANDLE_NULL) {
            tb_ui_examine_and_select_object(pc_obj, obj);
        }
    }
}

// 0x54ECD0
void intgame_pending_object_examine(void)
{
    if (intgame_selected_obj == OBJ_HANDLE_NULL) {
        return;
    }

    if (object_hover_obj_get() != OBJ_HANDLE_NULL) {
        return;
    }

    object_hover_obj_set(intgame_selected_obj);
    object_hover_obj_set(OBJ_HANDLE_NULL);
    tb_ui_examine_and_select_object(player_get_local_pc_obj(), intgame_selected_obj);
}

// TODO: Lots of jumps, check.
//
// 0x54ED30
void intgame_main_target_action_process(TargetDescriptor* td)
{
    int64_t pc_obj;
    int64_t item_obj = OBJ_HANDLE_NULL;
    int64_t target_loc = 0;
    int target_type;
    unsigned int spell_flags;
    unsigned int critter_flags;
    AnimGoalData goal_data;
    AnimID anim_id;
    int anim;
    bool use_alternate_pathfinding = false;

    pc_obj = player_get_local_pc_obj();
    if (pc_obj == OBJ_HANDLE_NULL) {
        return;
    }

    if (!critter_is_active(pc_obj)) {
        return;
    }

    target_type = obj_field_int32_get(td->obj, OBJ_F_TYPE);
    if (!td->is_loc && td->obj == pc_obj) {
        return;
    }

    spell_flags = obj_field_int32_get(pc_obj, OBJ_F_SPELL_FLAGS);
    critter_flags = obj_field_int32_get(pc_obj, OBJ_F_CRITTER_FLAGS);

    if ((spell_flags & OSF_STONED) != 0
        && (critter_flags & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
        return;
    }

    if (inven_ui_drag_item_obj_get() != OBJ_HANDLE_NULL) {
        if ((spell_flags & OSF_POLYMORPHED) != 0) {
            return;
        }

        item_obj = inven_ui_drag_item_obj_get();
        if (hotkey_ui_is_dragging()) {
            hotkey_ui_cancel_drag();
            inven_ui_item_cursor_begin(item_obj, false);
        }

        if (hotkey_ui_is_dragging()) {
            return;
        }

        target_loc = obj_field_int64_get(td->obj, OBJ_F_LOCATION);
        if (inven_ui_drag_item_obj_get() != OBJ_HANDLE_NULL) {
            item_obj = inven_ui_drag_item_obj_get();
        }

        inven_ui_drag_object_clear();
        intgame_refresh_cursor();

        anim = AG_THROW_ITEM;
    } else {
        if (!combat_critter_is_combat_mode_active(pc_obj)) {
            if (anim_is_attacking(td->obj, 0, pc_obj)) {
                combat_critter_activate_combat_mode(pc_obj);
            }
        }

        if (combat_critter_is_combat_mode_active(pc_obj)) {
            switch (target_type) {
            case OBJ_TYPE_WALL:
            case OBJ_TYPE_PORTAL:
                if (tig_kb_get_modifier(SDL_KMOD_ALT)) {
                    if (tig_kb_get_modifier(SDL_KMOD_SHIFT)) {
                        anim = AG_ATTEMPT_ATTACK;
                    } else {
                        anim = AG_ATTACK;
                    }
                } else {
                    if ((spell_flags & OSF_POLYMORPHED) != 0) {
                        return;
                    }

                    anim = AG_USE_OBJECT;
                    use_alternate_pathfinding = true;
                }
                break;
            case OBJ_TYPE_CONTAINER:
                if ((spell_flags & OSF_POLYMORPHED) != 0) {
                    return;
                }

                if (tig_kb_get_modifier(SDL_KMOD_ALT)) {
                    if (tig_kb_get_modifier(SDL_KMOD_SHIFT)) {
                        anim = AG_ATTEMPT_ATTACK;
                    } else {
                        anim = AG_ATTACK;
                    }
                } else {
                    anim = AG_USE_CONTAINER;
                    use_alternate_pathfinding = true;
                }
                break;
            case OBJ_TYPE_SCENERY:
                if (tig_kb_get_modifier(SDL_KMOD_ALT)) {
                    if (tig_kb_get_modifier(SDL_KMOD_SHIFT)) {
                        anim = AG_ATTEMPT_ATTACK;
                    } else {
                        anim = AG_ATTACK;
                    }
                } else {
                    if ((spell_flags & OSF_POLYMORPHED) != 0) {
                        return;
                    }

                    anim = AG_USE_OBJECT;
                    use_alternate_pathfinding = true;
                }
                break;
            case OBJ_TYPE_WEAPON:
            case OBJ_TYPE_AMMO:
            case OBJ_TYPE_ARMOR:
            case OBJ_TYPE_GOLD:
            case OBJ_TYPE_FOOD:
            case OBJ_TYPE_SCROLL:
            case OBJ_TYPE_KEY:
            case OBJ_TYPE_KEY_RING:
            case OBJ_TYPE_WRITTEN:
            case OBJ_TYPE_GENERIC:
                if ((spell_flags & OSF_POLYMORPHED) != 0) {
                    return;
                }

                anim = AG_PICKUP_ITEM;
                break;
            case OBJ_TYPE_PC:
            case OBJ_TYPE_NPC:
                if (critter_is_dead(td->obj)) {
                    if (tig_kb_get_modifier(SDL_KMOD_ALT)) {
                        anim = AG_USE_CONTAINER;
                        use_alternate_pathfinding = true;
                    }
                } else {
                    if (tig_kb_get_modifier(SDL_KMOD_SHIFT)) {
                        anim = AG_ATTEMPT_ATTACK;
                    } else {
                        anim = AG_ATTACK;
                    }

                    if (!td->is_loc
                        && player_is_local_pc_obj(critter_pc_leader_get(td->obj)
                            && !tig_kb_is_key_pressed(SDL_SCANCODE_LALT))) {
                        return;
                    }
                }
                break;
            case OBJ_TYPE_TRAP:
                anim = AG_MOVE_TO_TILE;
                target_loc = obj_field_int64_get(td->obj, OBJ_F_LOCATION);
                break;
            default:
                return;
            }

            if (anim == AG_ATTACK || anim == AG_ATTEMPT_ATTACK) {
                if ((spell_flags & OSF_BODY_OF_AIR) != 0) {
                    MesFileEntry mes_file_entry;
                    UiMessage ui_message;

                    mes_file_entry.num = 25; // "You cannot attack in this form."
                    mes_get_msg(intgame_mes_file, &mes_file_entry);
                    ui_message.type = UI_MSG_TYPE_FEEDBACK;
                    ui_message.str = mes_file_entry.str;
                    intgame_message_window_display_msg(&ui_message);
                    return;
                }
            }
        } else {
            switch (target_type) {
            case OBJ_TYPE_PORTAL:
                if ((spell_flags & OSF_POLYMORPHED) != 0) {
                    return;
                }

                anim = AG_USE_OBJECT;
                use_alternate_pathfinding = true;
                break;
            case OBJ_TYPE_CONTAINER:
                if ((spell_flags & OSF_POLYMORPHED) != 0) {
                    return;
                }

                anim = AG_USE_CONTAINER;
                use_alternate_pathfinding = true;
                break;
            case OBJ_TYPE_SCENERY:
                anim = AG_USE_OBJECT;
                use_alternate_pathfinding = true;
                break;
            case OBJ_TYPE_WEAPON:
            case OBJ_TYPE_AMMO:
            case OBJ_TYPE_ARMOR:
            case OBJ_TYPE_GOLD:
            case OBJ_TYPE_FOOD:
            case OBJ_TYPE_SCROLL:
            case OBJ_TYPE_KEY:
            case OBJ_TYPE_KEY_RING:
            case OBJ_TYPE_WRITTEN:
            case OBJ_TYPE_GENERIC:
                if ((spell_flags & OSF_POLYMORPHED) != 0) {
                    return;
                }

                if (tig_kb_get_modifier(SDL_KMOD_ALT)) {
                    object_pull_adjacent_to_source(pc_obj, td->obj);
                    return;
                }

                anim = AG_PICKUP_ITEM;
                break;
            case OBJ_TYPE_PC:
            case OBJ_TYPE_NPC:
                if (!critter_is_dead(td->obj)
                    || (obj_field_int32_get(td->obj, OBJ_F_SPELL_FLAGS) & OSF_SPOKEN_WITH_DEAD) != 0) {
                    // FIXME: Useless.
                    obj_field_int32_get(td->obj, OBJ_F_CURRENT_AID);

                    if (player_is_pc_obj(td->obj)) {
                        return;
                    }

                    anim = AG_TALK;
                } else {
                    if ((spell_flags & OSF_POLYMORPHED) != 0) {
                        return;
                    }

                    if (tig_kb_get_modifier(SDL_KMOD_ALT)) {
                        object_pull_adjacent_to_source(pc_obj, td->obj);
                        return;
                    }

                    anim = AG_USE_CONTAINER;
                    use_alternate_pathfinding = true;
                }
                break;
            case OBJ_TYPE_TRAP:
                anim = AG_MOVE_TO_TILE;
                target_loc = obj_field_int64_get(td->obj, OBJ_F_LOCATION);
                break;
            default:
                return;
            }
        }
    }

    // 0x54F25B
    if (!anim_goal_data_init_without_interrupt(&goal_data, pc_obj, anim)) {
        return;
    }

    goal_data.params[AGDATA_TARGET_OBJ].obj = td->obj;

    if (target_loc != 0) {
        goal_data.params[AGDATA_TARGET_TILE].loc = target_loc;
    }

    if (item_obj != OBJ_HANDLE_NULL) {
        goal_data.params[AGDATA_SCRATCH_OBJ].obj = item_obj;
    }

    if (anim == AG_ATTACK || anim == AG_ATTEMPT_ATTACK) {
        if (tig_kb_is_key_pressed(SDL_SCANCODE_COMMA)) {
            goal_data.params[AGDATA_SCRATCH_VAL3].data = 1;
        } else if (tig_kb_is_key_pressed(SDL_SCANCODE_PERIOD)) {
            goal_data.params[AGDATA_SCRATCH_VAL3].data = 2;
        } else if (tig_kb_is_key_pressed(SDL_SCANCODE_SLASH)) {
            goal_data.params[AGDATA_SCRATCH_VAL3].data = 3;
        } else {
            goal_data.params[AGDATA_SCRATCH_VAL3].data = 0;
        }

        int64_t weapon_obj = item_wield_get(pc_obj, ITEM_INV_LOC_WEAPON);
        if (weapon_obj != OBJ_HANDLE_NULL
            && obj_field_int32_get(weapon_obj, OBJ_F_TYPE) == OBJ_TYPE_WEAPON
            && (obj_field_int32_get(weapon_obj, OBJ_F_WEAPON_FLAGS) & OWF_DEFAULT_THROWS) != 0
            && !item_check_remove(weapon_obj)) {
            int64_t throwable_instance_obj = item_find_first_matching_prototype(pc_obj, weapon_obj);
            if (throwable_instance_obj == OBJ_HANDLE_NULL) {
                throwable_instance_obj = weapon_obj;
            }

            item_remove(throwable_instance_obj);

            goal_data.params[AGDATA_TARGET_TILE].loc = obj_field_int64_get(td->obj, OBJ_F_LOCATION);
            goal_data.params[AGDATA_SCRATCH_OBJ].obj = throwable_instance_obj;

            anim_goal_throw_item(pc_obj, throwable_instance_obj, obj_field_int64_get(td->obj, OBJ_F_LOCATION));
            return;
        }
    }

    if (combat_auto_attack_get(pc_obj)) {
        if (anim_matching_goal_is_active(pc_obj, &goal_data)
            || !anim_object_interrupt_by_priority(pc_obj, 3, 0, 0)) {
            return;
        }

        unsigned int flags = 0;
        if (tig_net_is_active()) {
            if (tig_kb_get_modifier(SDL_KMOD_SHIFT)) {
                flags |= 0x100;
            } else if (tig_kb_get_modifier(SDL_KMOD_CTRL)
                || ((!input_binding_is_pressed(INPUT_ACTION_ALWAYS_RUN) || get_always_run(pc_obj))
                    && !tig_kb_get_modifier(SDL_KMOD_CTRL))) {
                flags |= 0x40;
            }

            if (use_alternate_pathfinding) {
                flags |= 0x4000;
            }
        }

        if (!anim_goal_add_ex(&goal_data, &anim_id, flags)) {
            return;
        }

        if (tig_net_is_active()) {
            return;
        }
    } else if (anim_foreground_goal_find(pc_obj, &anim_id)) {
        // 0x54F68E
        if (combat_turn_based_is_active()) {
            AnimID fidget_anim_id;
            if (anim_is_current_goal_type(pc_obj, AG_ANIM_FIDGET, &fidget_anim_id)
                && anim_id_is_equal(&anim_id, &fidget_anim_id)
                && num_goal_subslots_in_use(&anim_id) < 4) {
                if (is_anim_forever(&anim_id)) {
                    if (anim_object_interrupt_by_priority(pc_obj, 3, false, false)) {
                        if (!anim_goal_add(&goal_data, &anim_id)) {
                            return;
                        }
                    }
                } else {
                    // __FILE__: "C:\Troika\Code\Game\gameuilib\Intgame.c"
                    // __LINE__: 5088
                    if (!anim_subgoal_add(anim_id, &goal_data, __FILE__, __LINE__)) {
                        return;
                    }
                }
            }
        } else if (anim_matching_goal_is_active(pc_obj, &goal_data)) {
            if (anim == AG_ATTACK || anim == AG_ATTEMPT_ATTACK) {
                if (num_goal_subslots_in_use(&anim_id) < 4) {
                    if (is_anim_forever(&anim_id)) {
                        if (anim_object_interrupt_by_priority(pc_obj, 3, 0, 0)
                            && !anim_goal_add(&goal_data, &anim_id)) {
                            return;
                        }
                    } else {
                        // __FILE__: "C:\Troika\Code\Game\gameuilib\Intgame.c"
                        // __LINE__: 5026
                        if (!anim_subgoal_add(anim_id, &goal_data, __FILE__, __LINE__)) {
                            return;
                        }
                    }
                }
            } else {
                if (anim_object_interrupt_by_priority(pc_obj, 3, false, false)
                    && anim_goal_add(&goal_data, &anim_id)) {
                    if (tig_kb_get_modifier(SDL_KMOD_SHIFT)) {
                        anim_stop_requested_set(anim_id);
                    } else if (tig_kb_get_modifier(SDL_KMOD_CTRL)) {
                        turn_on_running(anim_id);
                    } else {
                        if (input_binding_is_pressed(INPUT_ACTION_ALWAYS_RUN)) {
                            if (get_always_run(pc_obj)
                                && !tig_kb_get_modifier(SDL_KMOD_CTRL)) {
                                turn_on_running(anim_id);
                            }
                        } else {
                            if (!tig_kb_get_modifier(SDL_KMOD_CTRL)) {
                                turn_on_running(anim_id);
                            }
                        }
                    }

                    if (use_alternate_pathfinding) {
                        anim_alternate_pathfinding_enable(anim_id);
                    }
                }
            }
        } else if (anim_object_interrupt_by_priority(pc_obj, 3, false, false)) {
            if (tig_net_is_active()
                && !tig_kb_get_modifier(SDL_KMOD_SHIFT)
                && !tig_kb_get_modifier(SDL_KMOD_CTRL)) {
                // NOTE: Some useless checks.
            }

            if (anim_goal_add(&goal_data, &anim_id)
                && !tig_net_is_active()) {
                if (tig_kb_get_modifier(SDL_KMOD_SHIFT)) {
                    anim_stop_requested_set(anim_id);
                } else if (tig_kb_get_modifier(SDL_KMOD_CTRL)) {
                    turn_on_running(anim_id);
                } else {
                    if (input_binding_is_pressed(INPUT_ACTION_ALWAYS_RUN)) {
                        if (get_always_run(pc_obj)
                            && !tig_kb_get_modifier(SDL_KMOD_CTRL)) {
                            turn_on_running(anim_id);
                        }
                    } else {
                        if (!tig_kb_get_modifier(SDL_KMOD_CTRL)) {
                            turn_on_running(anim_id);
                        }
                    }
                }

                if (use_alternate_pathfinding) {
                    anim_alternate_pathfinding_enable(anim_id);
                }
            }
        }
    } else {
        if (!anim_goal_add(&goal_data, &anim_id)) {
            return;
        }
    }

    // 0x54FB19
    if (tig_kb_get_modifier(SDL_KMOD_SHIFT)) {
        anim_stop_requested_set(anim_id);
    } else {
        if (tig_kb_get_modifier(SDL_KMOD_CTRL)) {
            turn_on_running(anim_id);
        } else {
            if (input_binding_is_pressed(INPUT_ACTION_ALWAYS_RUN)) {
                if (get_always_run(pc_obj)
                    && !tig_kb_get_modifier(SDL_KMOD_CTRL)) {
                    turn_on_running(anim_id);
                }
            } else {
                if (!tig_kb_get_modifier(SDL_KMOD_CTRL)) {
                    turn_on_running(anim_id);
                }
            }
        }
    }

    if (use_alternate_pathfinding) {
        anim_alternate_pathfinding_enable(anim_id);
    }
}

// 0x54FCF0
void intgame_hotkey_activate(Hotkey* hotkey)
{
    int64_t pc_obj;
    int64_t weapon_obj;
    int64_t candidate_item_obj;
    int64_t hotkey_item_obj;

    if ((hotkey->flags & HOTKEY_DRAGGED) != 0) {
        return;
    }

    pc_obj = player_get_local_pc_obj();
    if (pc_obj == OBJ_HANDLE_NULL) {
        return;
    }

    if (critter_is_dead(pc_obj)) {
        return;
    }

    if (critter_is_unconscious(pc_obj)) {
        return;
    }

    if ((obj_field_int32_get(pc_obj, OBJ_F_CRITTER_FLAGS) & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
        return;
    }

    switch (hotkey->type) {
    case HOTKEY_NONE:
        // Should be unreachable.
        break;
    case HOTKEY_ITEM:
        intgame_mode_set(INTGAME_MODE_MAIN);
        object_handle_reference_recover_if_stale(&(hotkey->item_obj));
        if (obj_field_handle_get(hotkey->item_obj.obj, OBJ_F_ITEM_PARENT) == pc_obj) {
            hotkey_item_obj = hotkey->item_obj.obj;
            if (obj_field_int32_get(hotkey_item_obj, OBJ_F_TYPE) != OBJ_TYPE_WRITTEN
                || (item_check_use_box_flag(pc_obj, hotkey_item_obj)
                    && (obj_field_int32_get(hotkey_item_obj, OBJ_F_ITEM_FLAGS) & OIF_USE_IS_THROW) == 0)) {
                candidate_item_obj = item_find_first_matching_prototype(pc_obj, hotkey_item_obj);
                if (candidate_item_obj != OBJ_HANDLE_NULL) {
                    hotkey_item_obj = candidate_item_obj;
                }
            }

            if (item_check_use_box_flag(pc_obj, hotkey_item_obj)
                || (obj_field_int32_get(hotkey_item_obj, OBJ_F_ITEM_FLAGS) & OIF_USE_IS_THROW) != 0) {
                switch (obj_field_int32_get(hotkey_item_obj, OBJ_F_TYPE)) {
                case OBJ_TYPE_WEAPON:
                    intgame_hotkey_item_equip(pc_obj, hotkey, ITEM_INV_LOC_WEAPON);
                    break;
                case OBJ_TYPE_AMMO:
                case OBJ_TYPE_GOLD:
                    break;
                case OBJ_TYPE_ARMOR:
                    intgame_hotkey_item_equip(pc_obj, hotkey, item_location_get(hotkey_item_obj));
                    break;
                default:
                    candidate_item_obj = inven_ui_drag_item_obj_get();
                    if (candidate_item_obj != hotkey_item_obj) {
                        if (candidate_item_obj != OBJ_HANDLE_NULL) {
                            inven_ui_drag_cancel_and_restore();
                        }
                        inven_ui_destroy();
                        inven_ui_item_cursor_begin(hotkey_item_obj, true);
                    }
                    break;
                }
            } else if (tig_kb_get_modifier(SDL_KMOD_SHIFT)) {
                item_use_on_obj(pc_obj, hotkey_item_obj, pc_obj);
            } else {
                item_use_on_obj(pc_obj, hotkey_item_obj, OBJ_HANDLE_NULL);
            }
        }
        break;
    case HOTKEY_SKILL:
        skill_ui_action_activate(pc_obj, hotkey->data);
        break;
    case HOTKEY_SPELL:
        spell_ui_activate(pc_obj, hotkey->data);
        break;
    case HOTKEY_ITEM_SPELL:
        object_handle_reference_recover_if_stale(&(hotkey->item_obj));

        weapon_obj = item_wield_get(pc_obj, ITEM_INV_LOC_WEAPON);
        if (obj_field_handle_get(hotkey->item_obj.obj, OBJ_F_ITEM_PARENT) == pc_obj) {
            if (weapon_obj == hotkey->item_obj.obj) {
                spell_ui_manual_spell_activate(hotkey->item_obj.obj, hotkey->data);
            } else if (weapon_obj == OBJ_HANDLE_NULL || item_unequip(weapon_obj)) {
                if (item_wield_set(hotkey->item_obj.obj, ITEM_INV_LOC_WEAPON)) {
                    spell_ui_manual_spell_activate(hotkey->item_obj.obj, hotkey->data);
                }
            }
        }
        break;
    }
}

// 0x550000
void intgame_hotkey_item_equip(int64_t critter_obj, Hotkey* hotkey, int inventory_location)
{
    int64_t item_obj;
    int equip_result;
    int sound_id;

    item_obj = item_wield_get(critter_obj, inventory_location);
    if (item_obj == hotkey->item_obj.obj) {
        return;
    }

    if (item_obj != OBJ_HANDLE_NULL) {
        equip_result = item_equipment_slot_check(hotkey->item_obj.obj, inventory_location, critter_obj);
        if (equip_result != 0 && equip_result != 4) {
            item_error_msg(critter_obj, equip_result);
            return;
        }

        if (!item_unequip(item_obj)) {
            return;
        }
    }

    equip_result = item_equipment_slot_check(hotkey->item_obj.obj, inventory_location, critter_obj);
    if (equip_result != 0) {
        item_error_msg(critter_obj, equip_result);
        if (item_obj != OBJ_HANDLE_NULL) {
            item_wield_set(item_obj, inventory_location);
        }
        return;
    }

    if (!item_wield_set(hotkey->item_obj.obj, inventory_location)) {
        item_error_msg(critter_obj, 0);
        if (item_obj != OBJ_HANDLE_NULL) {
            item_wield_set(item_obj, inventory_location);
        }
        return;
    }

    if (item_obj != OBJ_HANDLE_NULL) {
        sound_id = sfx_item_sound(item_obj, critter_obj, OBJ_HANDLE_NULL, ITEM_SOUND_DROP);
    } else {
        sound_id = sfx_item_sound(hotkey->item_obj.obj, critter_obj, OBJ_HANDLE_NULL, ITEM_SOUND_DROP);
    }

    if (sound_id != -1) {
        gsound_play_sfx(sound_id, 1);
    }

    hotkey_ui_entry_clear(hotkey);
}

// 0x550150
void intgame_hotkey_highlight(Hotkey* hotkey)
{
    if ((hotkey->flags & HOTKEY_DRAGGED) != 0) {
        return;
    }

    switch (hotkey->type) {
    case HOTKEY_NONE:
        // Should be unreachable.
        break;
    case HOTKEY_ITEM:
        tb_ui_examine_and_select_object(player_get_local_pc_obj(), hotkey->item_obj.obj);
        break;
    case HOTKEY_SKILL:
        intgame_message_window_display_skill(hotkey->data);
        break;
    case HOTKEY_SPELL:
    case HOTKEY_ITEM_SPELL:
        intgame_message_window_display_spell(hotkey->data);
        break;
    }
}

// 0x5501C0
bool intgame_action_buttons_create(void)
{
    int index;
    tig_button_handle_t college_radio_group[COLLEGE_COUNT];
    int college_radio_group_size = 0;
    int selected_college_index = 0;
    tig_button_handle_t group[3];

    for (index = 0; index < 2; index++) {
        button_create_no_art(&(intgame_message_history_scroll_buttons[index]), 382, 41);
    }

    for (index = 0; index < COLLEGE_COUNT; index++) {
        intgame_college_buttons[index].art_num = spell_college_small_icon(index);
        if (intgame_college_buttons[index].art_num != -1) {
            intgame_button_create(&(intgame_college_buttons[index]));
            college_radio_group[college_radio_group_size] = intgame_college_buttons[index].button_handle;

            if (index == intgame_selected_spell_college) {
                selected_college_index = college_radio_group_size;
            }

            college_radio_group_size++;
        }
    }

    for (index = 0; index < 4; index++) {
        intgame_skill_buttons[index].art_num = skill_ui_action_button_art_num_get(index);
        if (intgame_skill_buttons[index].art_num != -1) {
            intgame_button_create(&(intgame_skill_buttons[index]));
        }
    }

    intgame_spells_init();
    intgame_mt_spells_init();

    for (index = 0; index < 5; index++) {
        intgame_rotwin_text_buttons[index].x = intgame_rotwin_text_frame[5].rect.x;
        intgame_rotwin_text_buttons[index].y = intgame_rotwin_text_frame[5].rect.y + intgame_rotwin_text_frame[5].rect.y / 5;
        intgame_rotwin_text_buttons[index].art_num = -1;
        intgame_rotwin_text_buttons[index].button_handle = TIG_BUTTON_HANDLE_INVALID;
        button_create_no_art(&(intgame_rotwin_text_buttons[index]), intgame_rotwin_text_frame[5].rect.width, intgame_rotwin_text_frame[5].rect.y / 5);
    }

    for (index = 0; index < 6; index++) {
        intgame_button_create(&(intgame_map_note_edit_buttons[index]));
    }

    for (index = 3; index < 6; index++) {
        group[index - 3] = intgame_map_note_edit_buttons[index].button_handle;
    }
    tig_button_radio_group_create(3, group, 0);

    for (index = 0; index < INTGAME_QUANTITY_BUTTON_COUNT; index++) {
        intgame_button_create(&(intgame_quantity_buttons[index]));
    }

    for (index = 1; index < 11; index++) {
        iso_interface_window_disable(index);
    }

    intgame_rotating_window_frame_draw(ROTWIN_TYPE_MSG, 100);

    tig_button_radio_group_create(college_radio_group_size,
        college_radio_group,
        selected_college_index);

    intgame_iso_window_type = ROTWIN_TYPE_MSG;

    intgame_saved_rotating_window_type = ROTWIN_TYPE_INVALID;

    return true;
}

// 0x5503F0
bool intgame_rotating_window_frame_draw(RotatingWindowType window_type, int progress)
{
    int iwid;
    TigArtFrameData art_frame_data;
    TigArtBlitInfo art_blit_info;
    TigRect src_rect;
    TigRect dst_rect;

    iwid = find_interface_window_index(intgame_rotwin_button_info[window_type].x, intgame_rotwin_button_info[window_type].y);
    if (iwid == -1) {
        return false;
    }

    tig_art_interface_id_create(intgame_rotwin_button_info[window_type].art_num, 0, 0, 0, &(art_blit_info.art_id));
    tig_art_frame_data(art_blit_info.art_id, &art_frame_data);

    src_rect.width = art_frame_data.width;
    src_rect.height = progress * art_frame_data.height / 100;
    src_rect.x = 0;
    src_rect.y = art_frame_data.height - src_rect.height;

    dst_rect.width = src_rect.width;
    dst_rect.height = src_rect.height;
    dst_rect.x = intgame_rotwin_button_info[window_type].x - intgame_interface_window_frames[iwid].x;
    dst_rect.y = intgame_rotwin_button_info[window_type].y - intgame_interface_window_frames[iwid].y;

    art_blit_info.flags = 0;
    art_blit_info.src_rect = &src_rect;
    art_blit_info.dst_rect = &dst_rect;

    return tig_window_blit_art(intgame_interface_windows[iwid], &art_blit_info);
}

// 0x5504F0
void iso_interface_window_disable(RotatingWindowType window_type)
{
    int index;

    switch (window_type) {
    case ROTWIN_TYPE_MSG:
        for (index = 0; index < 2; index++) {
            tig_button_hide(intgame_message_history_scroll_buttons[index].button_handle);
        }
        break;
    case ROTWIN_TYPE_SPELLS:
        for (index = 0; index < COLLEGE_COUNT; index++) {
            if (spell_college_small_icon(index) != -1) {
                tig_button_hide(intgame_college_buttons[index].button_handle);
            }
        }
        intgame_spells_hide_college_spells(intgame_selected_spell_college);
        break;
    case ROTWIN_TYPE_SKILLS:
        for (index = 0; index < 4; index++) {
            if (skill_ui_action_button_art_num_get(index) != -1) {
                tig_button_hide(intgame_skill_buttons[index].button_handle);
            }
        }
        break;
    case ROTWIN_TYPE_CHAT:
        break;
    case ROTWIN_TYPE_TRAPS:
        break;
    case ROTWIN_TYPE_DIALOGUE:
        for (index = 0; index < 5; index++) {
            tig_button_hide(intgame_rotwin_text_buttons[index].button_handle);
        }
        break;
    case ROTWIN_TYPE_MAP_NOTE:
        for (index = 0; index < 6; index++) {
            tig_button_hide(intgame_map_note_edit_buttons[index].button_handle);
        }
        break;
    case ROTWIN_TYPE_BROADCAST:
        break;
    case ROTWIN_TYPE_MAGICTECH:
        intgame_mt_spells_disable();
        break;
    case ROTWIN_TYPE_QUANTITY:
        for (index = 0; index < INTGAME_QUANTITY_BUTTON_COUNT; index++) {
            tig_button_hide(intgame_quantity_buttons[index].button_handle);
        }
        break;
    case ROTWIN_TYPE_MP_KICKBAN:
        break;
    default:
        tig_debug_printf("iso_interface_window_disable: ERROR: window type out of range!\n");
        break;
    }

    intgame_rotating_window_active = false;
}

// 0x5506C0
void iso_interface_window_set(RotatingWindowType window_type)
{
    broadcast_ui_close();

    if (intgame_iso_window_type == ROTWIN_TYPE_QUANTITY) {
        intgame_mode_set(INTGAME_MODE_MAIN);
    }

    // CE: capture the effective result before the swap, since the swap
    // updates intgame_iso_window_type. Same-type re-press is the user's
    // dismiss gesture (e.g. K → SKILLS → K → MSG).
    bool toggled_off = (intgame_iso_window_type == window_type);
    RotatingWindowType effective = toggled_off ? ROTWIN_TYPE_MSG : window_type;

    // CE: MINI-peek special case. See intgame_hud_handle_mini_peek_press
    // for the full state machine. If it handles the press, we return —
    // the rotwin type stays as-is, only the crop stage flips.
    if (toggled_off && intgame_hud_handle_mini_peek_press()) {
        return;
    }

    intgame_rotwin_step = MAX_INTERFACE_WINDOW_ROTATION_STEPS;
    if (toggled_off) {
        intgame_requested_rotating_window_type = ROTWIN_TYPE_MSG;
        iso_interface_window_swap(ROTWIN_TYPE_MSG);
    } else {
        intgame_requested_rotating_window_type = window_type;
        iso_interface_window_swap(window_type);
    }

    // CE: HUD-crop coupling.
    // - Real rotwin (skills/spells/magictech-weapon/etc.) invoked from
    //   MINI/HIDDEN: pop to MEDIUM and stash the prior stage.
    // - Result is MSG (toggle-off or explicit dismiss): restore the
    //   stashed stage so the user returns to where they were.
    // Auto-reverts that route through iso_interface_window_swap directly
    // (cursor leaving the bar, mode exits) bypass this entirely.
    if (effective == ROTWIN_TYPE_MSG) {
        intgame_hud_restore_after_rotwin();
    } else if (effective != ROTWIN_TYPE_INVALID) {
        intgame_hud_auto_pop_for_rotwin();
    }
}

// 0x550720
void intgame_message_window_clear(void)
{
    if (!intgame_iso_interface_created) {
        return;
    }

    if (intgame_iso_window_type != ROTWIN_TYPE_MSG) {
        return;
    }

    if (intgame_message_display_callback != NULL) {
        intgame_message_display_callback(0);
    } else {
        intgame_message_window_clear_internal();
    }
}

// 0x550750
void intgame_message_window_display_msg(UiMessage* ui_message)
{
    intgame_message_history_append(ui_message);
    if (intgame_message_display_callback != NULL) {
        intgame_message_display_callback(ui_message);
    }
}

// 0x550770
void intgame_message_window_display_str(int unused_message_id, char* str)
{
    UiMessage ui_message;

    (void)unused_message_id;

    if (intgame_message_display_callback != NULL) {
        ui_message.type = UI_MSG_TYPE_FEEDBACK;
        ui_message.str = str;
        intgame_message_display_callback(&ui_message);
    } else if (intgame_iso_interface_created) {
        intgame_message_window_write_text_centered(str, &(intgame_rotwin_text_frame[intgame_iso_window_type].rect));
    }
}

// 0x5507D0
void intgame_message_callback_set(void (*func)(UiMessage* ui_message))
{
    intgame_message_display_callback = func;
}

// 0x5507E0
void intgame_message_window_display_spell(int spl)
{
    UiMessage ui_message;

    if (intgame_iso_window_type != ROTWIN_TYPE_MSG) {
        intgame_message_window_write_text_centered(spell_name(spl), &(intgame_rotwin_text_frame[intgame_iso_window_type].rect));
    } else {
        ui_message.type = UI_MSG_TYPE_SPELL;
        ui_message.primary_value = spl;
        ui_message.secondary_value = 0;
        ui_message.caster_obj = player_get_local_pc_obj();
        intgame_message_window_display_msg(&ui_message);
        intgame_message_history_append(&ui_message);
    }
}

// 0x550860
void intgame_message_window_display_college(int college)
{
    UiMessage ui_message;

    if (intgame_iso_window_type != ROTWIN_TYPE_MSG) {
        intgame_message_window_write_text_centered(spell_college_name(college), &(intgame_rotwin_text_frame[intgame_iso_window_type].rect));
    } else {
        ui_message.type = UI_MSG_TYPE_COLLEGE;
        ui_message.primary_value = college;
        intgame_message_window_display_msg(&ui_message);
        intgame_message_history_append(&ui_message);
    }
}

// 0x5508C0
void intgame_message_window_display_skill(int value)
{
    UiMessage ui_message;

    if (intgame_iso_window_type != ROTWIN_TYPE_MSG) {
        intgame_message_window_write_text_centered(skill_ui_action_name_get(value), &(intgame_rotwin_text_frame[intgame_iso_window_type].rect));
    } else {
        ui_message.type = UI_MSG_TYPE_SKILL;
        ui_message.primary_value = skill_ui_action_skill_get(value);
        ui_message.secondary_value = 0;
        intgame_message_window_display_msg(&ui_message);
    }
}

// 0x550930
void intgame_message_window_clear_internal(void)
{
    if (!intgame_iso_interface_created) {
        return;
    }

    if (intgame_is_compact_interface()) {
        compact_ui_message_window_acquire();
        compact_ui_message_window_release();
    } else {
        tig_window_fill(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
            &(intgame_rotwin_text_frame[intgame_iso_window_type].rect),
            tig_color_make(0, 0, 0));
    }
}

// 0x5509C0
void intgame_message_window_write_text_centered(char* str, TigRect* rect)
{
    if (!intgame_iso_interface_created) {
        return;
    }

    if (intgame_rotwin_text_frame[intgame_iso_window_type].rect.width == 0) {
        return;
    }

    intgame_message_window_clear_internal();
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        str,
        rect,
        intgame_flare12_white_font,
        MSG_TEXT_HALIGN_CENTER | MSG_TEXT_VALIGN_CENTER);
}

// 0x550A10
bool intgame_message_window_write_text(tig_window_handle_t window_handle, char* str, TigRect* rect, tig_font_handle_t font, unsigned int flags)
{
    TigFont font_desc;
    TigRect text_rect;
    int width;
    int rc;

    if (!intgame_iso_interface_created) {
        return false;
    }

    tig_font_push(font);

    font_desc.str = str;
    font_desc.width = 0;
    tig_font_measure(&font_desc);

    text_rect.x = rect->x;
    text_rect.y = rect->y;

    if (intgame_is_compact_interface()) {
        text_rect.x -= 210;
        text_rect.y -= 59;
    }

    width = rect->width;
    if ((flags & MSG_TEXT_SECONDARY) != 0) {
        width /= 2;
        text_rect.x += width;
    }

    if (font_desc.width >= width) {
        font_desc.width = width;
        tig_font_measure(&font_desc);
        if (font_desc.height > rect->height) {
            font_desc.height = rect->height;
        }
    } else {
        if ((flags & MSG_TEXT_HALIGN_CENTER) != 0) {
            text_rect.x += (width - font_desc.width) / 2;
        } else if ((flags & MSG_TEXT_HALIGN_RIGHT) != 0) {
            text_rect.x += width - font_desc.width;
        }
    }

    if ((flags & MSG_TEXT_VALIGN_CENTER) != 0) {
        text_rect.y = rect->y + (rect->height - font_desc.height) / 2;

        if (intgame_is_compact_interface()) {
            text_rect.y -= 59;
        }
    }

    text_rect.width = font_desc.width;
    text_rect.height = font_desc.height;

    if (intgame_is_compact_interface()) {
        if (str[0] == '\0') {
            tig_font_pop();
            return true;
        }

        window_handle = compact_ui_message_window_acquire();
    }

    rc = tig_window_text_write(window_handle, str, &text_rect);
    if (rc != TIG_OK) {
        if ((flags & MSG_TEXT_TRUNCATE) != 0) {
            size_t pos;
            char ch;

            pos = strlen(str);
            while (rc != TIG_OK && pos > 0) {
                ch = str[pos - 1];
                str[pos - 1] = '\0';
                rc = tig_window_text_write(window_handle, str, &text_rect);
                str[pos - 1] = ch;
                pos--;
            }
        }
    }

    tig_font_pop();

    return rc == TIG_OK;
}

// 0x550BD0
bool intgame_spells_init(void)
{
    int clg;
    int spl;
    int lvl;

    for (clg = 0; clg < COLLEGE_COUNT; clg++) {
        for (lvl = 0; lvl < 5; lvl++) {
            spl = clg * 5 + lvl;
            intgame_spell_buttons[spl].art_num = spell_icon(spl);
            if (intgame_spell_buttons[spl].art_num != -1
                && !intgame_button_create(&(intgame_spell_buttons[spl]))) {
                return false;
            }
        }

        intgame_spells_hide_college_spells(clg);
    }

    return true;
}

// 0x550C60
void intgame_spells_show_college_spells(int clg)
{
    int64_t pc_obj;
    int spl;
    int lvl;

    pc_obj = player_get_local_pc_obj();
    if (pc_obj != OBJ_HANDLE_NULL) {
        for (lvl = 0; lvl < 5; lvl++) {
            spl = clg * 5 + lvl;
            if (!spell_is_known(pc_obj, spl)) {
                break;
            }

            if (intgame_spell_buttons[spl].button_handle != TIG_BUTTON_HANDLE_INVALID) {
                tig_button_show(intgame_spell_buttons[spl].button_handle);
            }
        }
    }
}

// 0x550CD0
void intgame_spells_hide_college_spells(int clg)
{
    int spl;
    int lvl;

    for (lvl = 0; lvl < 5; lvl++) {
        spl = clg * 5 + lvl;
        if (intgame_spell_buttons[spl].button_handle != TIG_BUTTON_HANDLE_INVALID) {
            tig_button_hide(intgame_spell_buttons[spl].button_handle);
        }
    }

    intgame_rotating_window_frame_draw(intgame_iso_window_type, 100);
}

// 0x550D20
bool intgame_mt_spells_init(void)
{
    int index;

    for (index = 0; index < 5; index++) {
        intgame_mt_spell_buttons[index].art_num = spell_icon(0);
        if (intgame_mt_spell_buttons[index].art_num != -1) {
            if (!intgame_button_create(&(intgame_mt_spell_buttons[index]))) {
                return false;
            }
        }
    }

    intgame_mt_spells_disable();

    return true;
}

// 0x550D60
void intgame_mt_spells_disable(void)
{
    int index;

    for (index = 0; index < 5; index++) {
        if (intgame_mt_spell_buttons[index].button_handle != TIG_BUTTON_HANDLE_INVALID) {
            tig_button_hide(intgame_mt_spell_buttons[index].button_handle);
        }
    }

    intgame_rotating_window_frame_draw(intgame_iso_window_type, 100);
}

// 0x550DA0
void intgame_pc_lens_do(PcLensMode mode, PcLens* pc_lens)
{
    TigVideoBufferCreateInfo vb_create_info;
    TigRect src_rect;
    TigRect dst_rect;
    TigArtBlitInfo blit_info;

    if (intgame_pc_lens_mode == mode) {
        return;
    }

    switch (intgame_pc_lens_mode) {
    case PC_LENS_MODE_NONE:
        break;
    case PC_LENS_MODE_PASSTHROUGH:
        if (tig_video_buffer_destroy(intgame_pc_lens_video_buffer) == TIG_OK) {
            intgame_pc_lens_video_buffer = NULL;
        }
        break;
    case PC_LENS_MODE_BLACKOUT:
        light_toggle();
        break;
    }

    switch (mode) {
    case PC_LENS_MODE_NONE:
        intgame_pc_lens.window_handle = TIG_WINDOW_HANDLE_INVALID;
        iso_draw_rect_set(NULL);
        iso_redraw();

        if (intgame_pc_lens_render_unlocked < 1) {
            gamelib_renderlock_release();
            intgame_pc_lens_render_unlocked++;
        }
        break;
    case PC_LENS_MODE_PASSTHROUGH:
        intgame_center_on_object(player_get_local_pc_obj());

        intgame_pc_lens.window_handle = pc_lens->window_handle;
        intgame_pc_lens.art_id = pc_lens->art_id;
        intgame_pc_lens.rect = &intgame_pc_lens_src_rect;
        intgame_pc_lens_src_rect = *pc_lens->rect;

        vb_create_info.width = intgame_pc_lens_dst_rect.width;
        vb_create_info.height = intgame_pc_lens_dst_rect.height;
        vb_create_info.flags = 0;
        vb_create_info.background_color = 0;

        if (tig_video_buffer_create(&vb_create_info, &intgame_pc_lens_video_buffer) == TIG_OK) {
            intgame_pc_lens_redraw();
        }

        iso_draw_rect_set(&intgame_pc_lens_dst_rect);

        if (intgame_pc_lens_render_unlocked < 1) {
            gamelib_renderlock_release();
            intgame_pc_lens_render_unlocked++;
        }
        break;
    case PC_LENS_MODE_BLACKOUT:
        light_toggle();

        intgame_pc_lens.window_handle = pc_lens->window_handle;
        intgame_pc_lens.art_id = pc_lens->art_id;
        intgame_pc_lens.rect = &intgame_pc_lens_src_rect;
        intgame_pc_lens_src_rect = *pc_lens->rect;

        tig_window_fill(intgame_pc_lens.window_handle,
            &intgame_pc_lens_src_rect,
            tig_color_make(0, 0, 0));

        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.width = intgame_pc_lens_src_rect.width;
        src_rect.height = intgame_pc_lens_src_rect.height;

        dst_rect.x = intgame_pc_lens.rect->x;
        dst_rect.y = intgame_pc_lens.rect->y;
        dst_rect.width = intgame_pc_lens_src_rect.width;
        dst_rect.height = intgame_pc_lens_src_rect.height;

        blit_info.flags = 0;
        blit_info.art_id = intgame_pc_lens.art_id;
        blit_info.src_rect = &src_rect;
        blit_info.dst_rect = &dst_rect;
        tig_window_blit_art(intgame_pc_lens.window_handle, &blit_info);

        iso_draw_rect_set(&intgame_pc_lens_blackout_draw_rect);

        gamelib_renderlock_acquire();
        intgame_pc_lens_render_unlocked--;
        break;
    }

    intgame_pc_lens_mode = mode;
}

// 0x551000
bool intgame_pc_lens_check_pt(int x, int y)
{
    TigWindowData window_data;

    if (intgame_pc_lens.window_handle == TIG_WINDOW_HANDLE_INVALID) {
        return false;
    }

    if (tig_window_data(intgame_pc_lens.window_handle, &window_data) != TIG_OK) {
        return false;
    }

    if (intgame_pc_lens.rect == NULL) {
        return false;
    }

    return x >= window_data.rect.x + intgame_pc_lens.rect->x
        && x < window_data.rect.x + intgame_pc_lens.rect->x + intgame_pc_lens.rect->width
        && y >= window_data.rect.y + intgame_pc_lens.rect->y
        && y < window_data.rect.y + intgame_pc_lens.rect->y + intgame_pc_lens.rect->height;
}

// CE: Super ugly wrapper around `intgame_pc_lens_check_pt` that converts
// specified coordinates from old "fullscreen" 800x600 window to screen
// coordinates before delegating actual work to `intgame_pc_lens_check_pt`.
bool intgame_pc_lens_check_pt_unscale(int x, int y)
{
    hrp_center(&x, &y);
    return intgame_pc_lens_check_pt(x, y);
}

// The five gameplay panels close through the circular portrait, not the
// transparent corners of its rectangular backing buffer.
bool intgame_panel_close_lens_check_pt(int x, int y)
{
    TigWindowData window;
    if (!intgame_pc_lens_check_pt(x, y)
        || tig_window_data(intgame_pc_lens.window_handle, &window) != TIG_OK
        || intgame_pc_lens.rect->width <= 0 || intgame_pc_lens.rect->height <= 0) {
        return false;
    }
    const TigRect* rect = intgame_pc_lens.rect;
    double dx = (x - window.rect.x - rect->x + 0.5) * 2.0 / rect->width - 1.0;
    double dy = (y - window.rect.y - rect->y + 0.5) * 2.0 / rect->height - 1.0;
    return dx * dx + dy * dy <= 1.0;
}

bool intgame_panel_close_lens_check_pt_unscale(int x, int y)
{
    hrp_center(&x, &y);
    return intgame_panel_close_lens_check_pt(x, y);
}

// True if a mouse click should dismiss an overlay menu: the click is
// outside the given menu rect AND not on either of the iso-interface HUD
// strips. All coordinates are in screen space.
//
// The 800x600 HUD strips are 800px wide and centered horizontally at hi-res,
// so empty screen area to either side of the strip (in the surrounding world
// view) is correctly treated as "outside HUD" and triggers dismissal.
// CE: one-shot guard against a stray click-outside dismiss. Overlays
// opened from a DIALOGUE option (worldmap / barter / charedit /
// schematic / identify) are selected on LEFT_BUTTON_DOWN —
// tc_handle_message only acts on mouse-down, dialogue options are
// mouse-only — so the paired LEFT_BUTTON_UP lands on the freshly
// opened overlay, outside its window, and would trip
// intgame_should_dismiss_overlay_click → instant close. The dialogue
// option handler arms this flag at the open; the next dismiss check
// (the paired mouse-up, the only event that calls the helper) clears
// it and returns false instead of dismissing. Because dialogue
// selection is mouse-only the paired up is guaranteed, so the flag is
// never left dangling for a keyboard path.
static bool intgame_overlay_dismiss_suppress_once = false;

void intgame_suppress_overlay_dismiss_once(void)
{
    intgame_overlay_dismiss_suppress_once = true;
}

bool intgame_should_dismiss_overlay_click(int screen_x, int screen_y, const TigRect* menu_rect)
{
    TigRect strip;

    // Consume the one stray mouse-up that pairs with the dialogue
    // option's mouse-down (see intgame_suppress_overlay_dismiss_once).
    // The helper is only ever called on LEFT_BUTTON_UP, so the first
    // call after a dialogue-driven open is exactly that stray up.
    if (intgame_overlay_dismiss_suppress_once) {
        intgame_overlay_dismiss_suppress_once = false;
        return false;
    }

    if (menu_rect == NULL) {
        return false;
    }

    if (screen_x >= menu_rect->x
        && screen_x < menu_rect->x + menu_rect->width
        && screen_y >= menu_rect->y
        && screen_y < menu_rect->y + menu_rect->height) {
        return false;
    }

    strip = intgame_interface_window_frames[0];
    hrp_apply(&strip, GRAVITY_CENTER_HORIZONTAL | GRAVITY_TOP);
    if (screen_x >= strip.x
        && screen_x < strip.x + strip.width
        && screen_y >= strip.y
        && screen_y < strip.y + strip.height) {
        return false;
    }

    strip = intgame_interface_window_frames[1];
    hrp_apply(&strip, GRAVITY_CENTER_HORIZONTAL | GRAVITY_BOTTOM);
    if (screen_x >= strip.x
        && screen_x < strip.x + strip.width
        && screen_y >= strip.y
        && screen_y < strip.y + strip.height) {
        return false;
    }

    return true;
}

bool intgame_panel_should_dismiss_overlay_click(int screen_x, int screen_y, const TigRect* menu_rect)
{
    // Always consume the dialogue-opening mouse-up guard, even in group mode.
    bool outside = intgame_should_dismiss_overlay_click(screen_x, screen_y, menu_rect);
    return settings_get_value(&settings, WINDOW_CLOSE_MODE_KEY) != 0 && outside;
}

// 0x551080
void intgame_pc_lens_redraw(void)
{
    TigArtBlitInfo blit_info;
    TigRect src_rect;
    TigRect dst_rect;

    if (intgame_pc_lens_video_buffer != NULL) {
        tig_window_copy(intgame_pc_lens.window_handle,
            intgame_pc_lens.rect,
            intgame_iso_window_handle,
            &intgame_pc_lens_dst_rect);

        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.width = intgame_pc_lens.rect->width;
        src_rect.height = intgame_pc_lens.rect->height;

        if (tig_window_copy_to_vbuffer(intgame_iso_window_handle, &intgame_pc_lens_dst_rect, intgame_pc_lens_video_buffer, &src_rect) == TIG_OK) {
            dst_rect = src_rect;
            dst_rect.x = intgame_pc_lens.rect->x;
            dst_rect.y = intgame_pc_lens.rect->y;

            blit_info.art_id = intgame_pc_lens.art_id;
            blit_info.flags = 0;
            blit_info.src_rect = &src_rect;
            blit_info.dst_rect = &dst_rect;
            tig_window_blit_art(intgame_pc_lens.window_handle, &blit_info);
        }
    }
}

// 0x551160
void iso_interface_refresh(void)
{
    int64_t pc_obj;
    int64_t obj;

    if (!intgame_iso_interface_created) {
        return;
    }

    pc_obj = player_get_local_pc_obj();

    iso_interface_window_disable(intgame_iso_window_type);
    iso_interface_window_enable(intgame_iso_window_type);

    if (intgame_iso_window_type == ROTWIN_TYPE_MSG) {
        if (pc_obj != OBJ_HANDLE_NULL) {
            if (intgame_selected_obj != OBJ_HANDLE_NULL) {
                tb_ui_examine_and_select_object(pc_obj, intgame_selected_obj);
            } else if ((obj = object_hover_obj_get()) != OBJ_HANDLE_NULL) {
                tb_ui_examine_and_select_object(pc_obj, obj);
            } else {
                intgame_message_refresh(false);
            }
        } else {
            intgame_message_refresh(false);
        }
    }

    if (pc_obj != OBJ_HANDLE_NULL) {
        intgame_draw_counter(INTGAME_COUNTER_FATE,
            stat_level_get(pc_obj, STAT_FATE_POINTS),
            2);
        intgame_refresh_experience_gauges(pc_obj);
    }
}

// 0x551210
void iso_interface_window_enable(RotatingWindowType window_type)
{
    int64_t pc_obj;
    int64_t obj;
    int fld;
    int qty_fld;
    int index;
    TigArtBlitInfo blit_info;
    TigArtFrameData art_frame_data;
    TigRect src_rect;
    TigRect dst_rect;

    if (intgame_rotating_window_active) {
        iso_interface_window_disable(intgame_iso_window_type);
    }

    intgame_rotating_window_frame_draw(window_type, 100);
    intgame_iso_window_type = window_type;

    pc_obj = player_get_local_pc_obj();

    switch (window_type) {
    case ROTWIN_TYPE_MSG:
        for (index = 0; index < 2; index++) {
            tig_button_show(intgame_message_history_scroll_buttons[index].button_handle);
        }
        break;
    case ROTWIN_TYPE_SPELLS:
        for (index = 0; index < COLLEGE_COUNT; index++) {
            if (spell_college_small_icon(index) != -1
                && spell_college_is_known(pc_obj, index)) {
                tig_button_show(intgame_college_buttons[index].button_handle);
            }
        }
        tig_button_state_change(intgame_college_buttons[intgame_selected_spell_college].button_handle, TIG_BUTTON_STATE_PRESSED);
        intgame_spells_show_college_spells(intgame_selected_spell_college);
        break;
    case ROTWIN_TYPE_SKILLS:
        for (index = 0; index < 4; index++) {
            if (skill_ui_action_button_art_num_get(index) != -1) {
                tig_button_show(intgame_skill_buttons[index].button_handle);
            }
        }
        break;
    case ROTWIN_TYPE_CHAT:
        break;
    case ROTWIN_TYPE_TRAPS:
        break;
    case ROTWIN_TYPE_DIALOGUE:
        for (index = 0; index < 5; index++) {
            tig_button_show(intgame_rotwin_text_buttons[index].button_handle);
        }
        break;
    case ROTWIN_TYPE_MAP_NOTE:
        for (index = 0; index < 6; index++) {
            tig_button_show(intgame_map_note_edit_buttons[index].button_handle);
        }
        break;
    case ROTWIN_TYPE_BROADCAST:
        break;
    case ROTWIN_TYPE_MAGICTECH:
        intgame_mt_spells_enable();
        break;
    case ROTWIN_TYPE_QUANTITY:
        for (index = 0; index < INTGAME_QUANTITY_BUTTON_COUNT; index++) {
            tig_button_show(intgame_quantity_buttons[index].button_handle);
        }

        intgame_quantity = 0;

        obj = inven_ui_transaction_item_get();
        fld = item_stack_counter_field(obj, &qty_fld);
        if (fld != -1) {
            intgame_max_quantity = obj_field_int32_get(obj, qty_fld);
            switch (fld) {
            case OBJ_F_CRITTER_GOLD:
                tig_art_item_id_create(0, 2, 0, 0, 0, 3, 0, 0, &(blit_info.art_id));
                break;
            case OBJ_F_CRITTER_ARROWS:
                tig_art_item_id_create(0, 2, 0, 0, 0, 1, 0, 0, &(blit_info.art_id));
                break;
            case OBJ_F_CRITTER_BULLETS:
                tig_art_item_id_create(1, 2, 0, 0, 0, 1, 0, 0, &(blit_info.art_id));
                break;
            case OBJ_F_CRITTER_POWER_CELLS:
                tig_art_item_id_create(2, 2, 0, 0, 0, 1, 0, 0, &(blit_info.art_id));
                break;
            case OBJ_F_CRITTER_FUEL:
                tig_art_item_id_create(3, 2, 0, 0, 0, 1, 0, 0, &(blit_info.art_id));
                break;
            }

            tig_art_frame_data(blit_info.art_id, &art_frame_data);

            src_rect.x = 0;
            src_rect.y = 0;
            src_rect.width = art_frame_data.width;
            src_rect.height = art_frame_data.height;

            dst_rect.x = 269;
            dst_rect.y = 93;
            dst_rect.width = art_frame_data.width;
            dst_rect.height = art_frame_data.height;

            blit_info.flags = 0;
            blit_info.src_rect = &src_rect;
            blit_info.dst_rect = &dst_rect;
            tig_window_blit_art(intgame_interface_windows[1], &blit_info);
        } else {
            intgame_max_quantity = 1;
        }
        intgame_refresh_quantity();
        break;
    case ROTWIN_TYPE_MP_KICKBAN:
        break;
    default:
        tig_debug_printf("iso_interface_window_enable: ERROR: window type out of range!");
        break;
    }

    intgame_rotating_window_active = true;
}

// 0x551660
void intgame_mt_spells_enable(void)
{
    int index;
    int spl;
    tig_art_id_t art_id;

    if (player_get_local_pc_obj() == OBJ_HANDLE_NULL
        || intgame_magictech_item_obj == OBJ_HANDLE_NULL) {
        return;
    }

    if ((obj_field_int32_get(intgame_magictech_item_obj, OBJ_F_ITEM_FLAGS) & OIF_IDENTIFIED) != 0
        && obj_field_int32_get(intgame_magictech_item_obj, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY) > 0) {
        for (index = 0; index < 5; index++) {
            spl = mt_item_spell(intgame_magictech_item_obj, index);
            if (spl != -1
                && !magictech_is_tech(spl)
                && intgame_mt_spell_buttons[index].art_num != -1) {
                intgame_mt_spell_buttons[index].art_num = spell_icon(spl);
                tig_art_interface_id_create(intgame_mt_spell_buttons[index].art_num, 0, 0, 0, &art_id);
                tig_button_set_art(intgame_mt_spell_buttons[index].button_handle, art_id);
                tig_button_show(intgame_mt_spell_buttons[index].button_handle);
            }
        }
    }

    intgame_counters_refresh();
}

// 0x551740
int find_interface_window_index(int x, int y)
{
    int index;

    for (index = 0; index < 2; index++) {
        if (x >= intgame_interface_window_frames[index].x
            && y >= intgame_interface_window_frames[index].y
            && x < intgame_interface_window_frames[index].x + intgame_interface_window_frames[index].width
            && y < intgame_interface_window_frames[index].y + intgame_interface_window_frames[index].height) {
            return index;
        }
    }

    return -1;
}

// 0x5517A0
bool intgame_mouse_message_is_over_window(TigMessage* msg)
{
    tig_window_handle_t window_handle;
    TigWindowData window_data;

    if (tig_window_get_at_position(msg->data.mouse.x, msg->data.mouse.y, &window_handle) != TIG_OK) {
        return false;
    }

    if (window_handle != intgame_iso_window_handle) {
        // FIXME: Meaningless.
        tig_window_data(window_handle, &window_data);
        return false;
    }

    return true;
}

// 0x5517F0
void intgame_hover_refresh_from_mouse(void)
{
    TigMouseState mouse_state;
    TigMessage msg;

    if (tig_mouse_get_state(&mouse_state) == TIG_OK) {
        // NOTE: Fake TigMessage object, albeit incomplete - underlying code
        // is only interested in mouse coordinates.
        msg.data.mouse.x = mouse_state.x;
        msg.data.mouse.y = mouse_state.y;
        intgame_hover_refresh_from_message(&msg);
    }
}

// 0x551830
bool intgame_get_location_under_cursor(int64_t* loc_ptr)
{
    TigMouseState mouse_state;
    TargetDescriptor td;
    int x, y;

    if (tig_mouse_get_state(&mouse_state) == TIG_OK
        && intgame_point_is_over_window(mouse_state.x, mouse_state.y)) {
        intgame_adjust_mouse_for_zoom(mouse_state.x, mouse_state.y, &x, &y);
        if (target_pick_at_screen_xy_ex(x, y, &td, TGT_TILE, intgame_fullscreen)
            && td.is_loc) {
            *loc_ptr = td.loc;
            return true;
        }
    }

    *loc_ptr = 0;
    return false;
}

// 0x5518C0
bool intgame_point_is_over_window(int x, int y)
{
    tig_window_handle_t window_handle;
    TigWindowData window_data;

    if (tig_window_get_at_position(x, y, &window_handle) != TIG_OK) {
        return false;
    }

    if (window_handle != intgame_iso_window_handle) {
        // FIXME: Meaningless.
        tig_window_data(window_handle, &window_data);
        return false;
    }

    return true;
}

static bool intgame_adjust_mouse_for_zoom(int x, int y, int* adj_x, int* adj_y)
{
    float z = iso_zoom_current();

    if (z != 1.0f) {
        int64_t ax;
        int64_t ay;

        location_zoom_adjust_screen_xy(x, y, z, &ax, &ay);
        *adj_x = (int)ax;
        *adj_y = (int)ay;
        return true;
    }

    *adj_x = x;
    *adj_y = y;
    return false;
}

// 0x551910
void intgame_hover_refresh_from_message(TigMessage* msg)
{
    TargetDescriptor td;
    int x;
    int y;

    if (intgame_mouse_message_is_over_window(msg)) {
        intgame_target_flags_refresh();

        intgame_adjust_mouse_for_zoom(msg->data.mouse.x, msg->data.mouse.y, &x, &y);

        if (!map_is_clearing_objects()) {
            if (target_pick_at_screen_xy_ex(x, y, &td, intgame_hover_target_flags, intgame_fullscreen)) {
                if (!td.is_loc) {
                    tb_ui_examine_and_select_object(player_get_local_pc_obj(), td.obj);
                    object_hover_obj_set(td.obj);
                }
            } else if (combat_turn_based_is_active()
                && target_pick_at_screen_xy_ex(x, y, &td, TGT_TILE, intgame_fullscreen)
                && td.is_loc
                && intgame_mode_get() == INTGAME_MODE_MAIN) {
                combat_check_move_to(player_get_local_pc_obj(), td.loc);
            }
        }
    }
}

// 0x551A00
IntgameMode intgame_mode_get(void)
{
    return intgame_mode_state_current(&intgame_mode_state);
}

// 0x551A10
void intgame_center_on_object(int64_t obj)
{
    int64_t location;
    int64_t x;
    int64_t y;

    if (obj != OBJ_HANDLE_NULL) {
        location = obj_field_int64_get(obj, OBJ_F_LOCATION);
        location_calc_dist_from_screen_center(location, &x, &y);
        if (x != 0 || y != 0) {
            location_origin_set(location);
            iso_redraw();
        }
    }
}

// 0x551A80
bool intgame_mode_set(IntgameMode mode)
{
    int64_t pc_obj;
    int64_t obj;
    IntgameMode prev_mode;
    bool cancel_drag = false;
    bool returning_to_parent = false;
    bool unwind_targeting_mode = false;
    bool entering_child = false;

    intgame_camera_pan_active = false;
    intgame_camera_pan_moved = false;

    while (1) {
        IntgameModeTransition transition;
        IntgameModeResult result = intgame_mode_state_begin(&intgame_mode_state, mode, &transition);
        if (result == INTGAME_MODE_REENTRANT) return true;
        if (result != INTGAME_MODE_READY) {
            tig_debug_printf("intgame_mode_set: rejected mode %d (reason %d).\n", mode, result);
            return false;
        }
        pc_obj = player_get_local_pc_obj();
        prev_mode = transition.from;
        mode = transition.to;
        entering_child = transition.pushing;
        returning_to_parent = transition.popping;

        if (!transition.pushing) {
            if (inven_ui_drag_item_obj_get() == OBJ_HANDLE_NULL) {
                intgame_refresh_cursor();
            }
        }

        switch (prev_mode) {
        case INTGAME_MODE_MAIN:
        case INTGAME_MODE_FOLLOWER:
            cancel_drag = true;
            target_flags_set(TGT_OBJECT | TGT_OBJ_NO_T_WALL | TGT_TILE);
            break;
        case INTGAME_MODE_SPELL:
            cancel_drag = true;
            unwind_targeting_mode = true;
            spell_ui_cancel();
            combat_check_cast_spell(player_get_local_pc_obj());
            break;
        case INTGAME_MODE_SKILL:
            cancel_drag = true;
            unwind_targeting_mode = true;
            skill_ui_cancel();
            combat_check_use_skill(player_get_local_pc_obj());
            break;
        case INTGAME_MODE_DIALOG:
            cancel_drag = 1;
            if (returning_to_parent) {
                dialog_ui_end_dialog(player_get_local_pc_obj(), 0);
            } else {
                dialog_ui_notify_dialog_ended(player_get_local_pc_obj());
            }
            break;
        case INTGAME_MODE_BARTER:
            if (mode != INTGAME_MODE_QUANTITY) {
                inven_ui_destroy();
            }
            cancel_drag = true;
            if (mode != INTGAME_MODE_QUANTITY) {
                intgame_unforce_fullscreen();
            }
            break;
        case INTGAME_MODE_WMAP:
            cancel_drag = true;
            wmap_ui_close();
            if (intgame_iso_window_type == ROTWIN_TYPE_MAP_NOTE) {
                iso_interface_window_set(ROTWIN_TYPE_MSG);
            }
            scroll_set_scroll_func(NULL);
            intgame_unforce_fullscreen();
            break;
        case INTGAME_MODE_SLEEP:
            cancel_drag = true;
            sleep_ui_close();
            break;
        case INTGAME_MODE_LOGBOOK:
            cancel_drag = true;
            logbook_ui_close();
            intgame_unforce_fullscreen();
            break;
        case INTGAME_MODE_INVEN:
            switch (mode) {
            case INTGAME_MODE_SPELL:
            case INTGAME_MODE_SKILL:
            case INTGAME_MODE_QUANTITY:
            case INTGAME_MODE_ITEM:
                cancel_drag = true;
                break;
            default:
                cancel_drag = false;
                inven_ui_destroy();
                break;
            }

            if (mode != INTGAME_MODE_QUANTITY) {
                intgame_unforce_fullscreen();
            }
            break;
        case INTGAME_MODE_CHAREDIT:
            cancel_drag = true;
            charedit_close();
            intgame_unforce_fullscreen();
            break;
        case INTGAME_MODE_LOOT:
            if (mode == INTGAME_MODE_SPELL || mode == INTGAME_MODE_SKILL) {
                if (mode != INTGAME_MODE_QUANTITY) {
                    intgame_unforce_fullscreen();
                }
            } else if (mode != INTGAME_MODE_QUANTITY) {
                inven_ui_destroy();
            }
            break;
        case INTGAME_MODE_STEAL:
            if (mode != INTGAME_MODE_QUANTITY) {
                inven_ui_destroy();
                intgame_unforce_fullscreen();
            }
            break;
        case INTGAME_MODE_QUANTITY:
            iso_interface_window_set(ROTWIN_TYPE_MSG);
            obj = inven_ui_transaction_item_get();
            item_flags_unset(obj, OIF_NO_DISPLAY);

            inven_ui_update(OBJ_HANDLE_NULL);
            break;
        case INTGAME_MODE_SCHEMATIC:
            cancel_drag = true;
            schematic_ui_close();
            intgame_unforce_fullscreen();
            break;
        case INTGAME_MODE_WRITTEN:
            cancel_drag = true;
            written_ui_close();
            intgame_unforce_fullscreen();
            break;
        case INTGAME_MODE_ITEM:
            item_ui_deactivate();
            if (inven_ui_drag_item_obj_get() != OBJ_HANDLE_NULL) {
                inven_ui_drag_cancel_and_restore();
                intgame_refresh_cursor();
            }
            break;
        case INTGAME_MODE_NPC_IDENTIFY:
        case INTGAME_MODE_NPC_REPAIR:
            if (mode != INTGAME_MODE_QUANTITY) {
                inven_ui_destroy();
            }
            cancel_drag = true;
            intgame_unforce_fullscreen();
            break;
        default:
            break;
        }

        switch (mode) {
        case INTGAME_MODE_MAIN:
        case INTGAME_MODE_SPELL:
        case INTGAME_MODE_SKILL:
            intgame_hover_refresh_from_mouse();
            break;
        case INTGAME_MODE_DIALOG:
            compact_ui_message_window_hide();
            if (!entering_child) {
                dialog_ui_notify_dialog_started(player_get_local_pc_obj());
            }
            break;
        case INTGAME_MODE_BARTER:
        case INTGAME_MODE_LOGBOOK:
        case INTGAME_MODE_INVEN:
        case INTGAME_MODE_CHAREDIT:
        case INTGAME_MODE_LOOT:
        case INTGAME_MODE_STEAL:
        case INTGAME_MODE_SCHEMATIC:
        case INTGAME_MODE_WRITTEN:
        case INTGAME_MODE_NPC_IDENTIFY:
        case INTGAME_MODE_NPC_REPAIR:
            intgame_force_fullscreen();
            break;
        case INTGAME_MODE_WMAP:
            intgame_force_fullscreen();
            scroll_set_scroll_func(wmap_ui_scroll);
            break;
        case INTGAME_MODE_QUANTITY:
            obj = inven_ui_transaction_item_get();
            item_flags_set(obj, OIF_NO_DISPLAY);
            inven_ui_update(OBJ_HANDLE_NULL);
            break;
        default:
            break;
        }

        if (cancel_drag) {
            if (inven_ui_drag_item_obj_get() != OBJ_HANDLE_NULL) {
                inven_ui_drag_cancel_and_restore();
            }
        }

        // Keep the historical ordering: cursor refresh occurs after the new
        // mode is stored, while recursive close callbacks are still blocked.
        intgame_mode_state_commit(&intgame_mode_state, &transition);
        intgame_refresh_cursor();

        cancel_drag = false;
        intgame_mode_state_end(&intgame_mode_state);

        if (!unwind_targeting_mode) {
            return true;
        }

        mode = INTGAME_MODE_MAIN;
        entering_child = false;
        returning_to_parent = false;
        unwind_targeting_mode = false;
    }
}

// 0x551F20
void intgame_force_fullscreen(void)
{
    if (intgame_is_compact_interface()) {
        intgame_toggle_interface();
        intgame_fullscreen_forced = true;
    }
}

// 0x551F40
void intgame_unforce_fullscreen(void)
{
    if (intgame_fullscreen_forced) {
        if (!intgame_is_compact_interface()) {
            intgame_toggle_interface();
            intgame_fullscreen_forced = false;
        }
    }
}

// 0x551F70
bool intgame_mode_supports_scrolling(IntgameMode mode)
{
    return mode >= INTGAME_MODE_MAIN && mode < INTGAME_MODE_COUNT
        && intgame_mode_scrolling[mode];
}

// 0x551F80
void intgame_target_flags_refresh(void)
{
    int64_t pc_obj;

    pc_obj = player_get_local_pc_obj();
    if (pc_obj != OBJ_HANDLE_NULL) {
        if (intgame_mode_get() != INTGAME_MODE_MAIN) {
            intgame_hover_target_flags = TGT_OBJECT;
            return;
        }

        if (combat_critter_is_combat_mode_active(pc_obj)) {
            if (tig_kb_get_modifier(SDL_KMOD_ALT)) {
                target_flags_set(TGT_OBJECT | TGT_OBJ_NO_SELF | TGT_OBJ_NO_T_WALL | TGT_TILE);
                intgame_hover_target_flags = TGT_OBJECT | TGT_OBJ_NO_SELF | TGT_OBJ_NO_T_WALL;
            } else {
                target_flags_set(TGT_OBJECT | TGT_OBJ_NO_SELF | TGT_OBJ_NO_ST_CRITTER_DEAD | TGT_OBJ_NO_T_WALL | TGT_TILE | TGT_NON_PARTY_CRITTERS);
                intgame_hover_target_flags = TGT_OBJECT | TGT_OBJ_NO_SELF | TGT_OBJ_NO_ST_CRITTER_DEAD | TGT_OBJ_NO_T_WALL | TGT_NON_PARTY_CRITTERS;
            }
            return;
        }
    }

    target_flags_set(TGT_OBJECT | TGT_OBJ_NO_T_WALL | TGT_TILE);
    intgame_hover_target_flags = TGT_OBJECT;
}

// 0x552050
bool intgame_target_pick_at_mouse(int x, int y, TargetDescriptor* td)
{
    if (intgame_adjust_mouse_for_zoom(x, y, &x, &y)) {
        intgame_target_flags_refresh();
        return target_pick_at_virtual_xy(x, y, td, intgame_fullscreen);
    }
    intgame_target_flags_refresh();
    return target_pick_at_screen_xy(x, y, td, intgame_fullscreen);
}

// 0x552070
RotatingWindowType iso_interface_window_get(void)
{
    return intgame_iso_window_type;
}

// 0x552080
void iso_interface_window_set_animated(RotatingWindowType window_type)
{
    if (window_type != ROTWIN_TYPE_INVALID) {
        intgame_rotating_window_step(window_type, intgame_rotwin_step);

        if (intgame_rotwin_step < MAX_INTERFACE_WINDOW_ROTATION_STEPS) {
            intgame_rotwin_step++;
            anim_ui_event_add(ANIM_UI_EVENT_TYPE_ROTATE_INTERFACE, window_type);
        } else {
            intgame_saved_rotating_window_type = intgame_iso_window_type;
        }
    }
}

// 0x5520D0
void intgame_rotating_window_step(RotatingWindowType window_type, int step)
{
    if (intgame_iso_window_type != window_type) {
        if (step == 0) {
            iso_interface_window_disable(intgame_iso_window_type);
        } else if (step == MAX_INTERFACE_WINDOW_ROTATION_STEPS) {
            iso_interface_window_enable(window_type);
        } else {
            intgame_rotating_window_frame_draw(window_type, 100 * step / MAX_INTERFACE_WINDOW_ROTATION_STEPS);
        }
    }
}

// 0x552130
void iso_interface_window_swap(RotatingWindowType window_type)
{
    // CE: MINI's slim-row hack assumes SKILLS rotwin is the active
    // chrome (skill_rot art has a text well at art-local +71 that
    // exactly matches the MINI band). The engine internally swaps
    // the rotwin on world-hover (MSG / examine / etc.) — when stage
    // is MINI, ignore those swaps so the slim row keeps showing
    // SKILLS content instead of jittering between chrome layouts.
    // Allow re-swaps to SKILLS itself (used by the MINI-restore path).
    if (intgame_hud_in_mini_stage()
        && intgame_iso_window_type == ROTWIN_TYPE_SKILLS
        && window_type != ROTWIN_TYPE_SKILLS) {
        return;
    }
    iso_interface_window_disable(intgame_iso_window_type);
    intgame_rotwin_step = MAX_INTERFACE_WINDOW_ROTATION_STEPS;
    iso_interface_window_enable(window_type);
    intgame_saved_rotating_window_type = intgame_iso_window_type;
    // CE: the bottom-strip clip band is anchored to the active chrome's
    // strip-local y. SKILLS/SPELLS chrome lives 1px higher than other
    // rotwin arts, so a type swap (e.g. MSG -> SKILLS) shifts the band
    // by that delta. Re-apply so the crop stays flush with the new
    // chrome. No-op in FULL stage (clears to NULL clip both times).
    intgame_hud_apply_clips();
}

// 0x552160
void intgame_text_edit_refresh(const char* str, tig_font_handle_t font)
{
    intgame_text_edit_refresh_color(str,
        font,
        tig_color_make(0, 0, 0),
        0);
}

// 0x5521B0
void intgame_text_edit_refresh_color(const char* str, tig_font_handle_t font, tig_color_t color, bool scroll_to_end)
{
    tig_window_handle_t window_handle;
    TigRect rect;
    TigFont font_desc;

    window_handle = intgame_rotwin_text_frame[intgame_iso_window_type].window_handle;
    rect = intgame_rotwin_text_frame[intgame_iso_window_type].rect;

    tig_window_fill(window_handle, &rect, color);

    if (str != NULL && *str != '\0') {
        tig_font_push(font);
        font_desc.width = 0;
        font_desc.str = str;
        tig_font_measure(&font_desc);

        // NOTE: Many jumps, check.
        if (font_desc.width > rect.width && scroll_to_end) {
            while (font_desc.width > rect.width && *str != '\0') {
                font_desc.width = 0;
                font_desc.str = str++;
                tig_font_measure(&font_desc);
            }

            if (*str == '\0') {
                tig_font_pop();
                return;
            }
        }

        rect.width = font_desc.width;

        tig_window_text_write(window_handle, str, &rect);
        tig_font_pop();
    } else {
        rect.width = 0;
    }

    tig_font_push(font);
    font_desc.width = 0;
    font_desc.str = "*";
    tig_font_measure(&font_desc);
    rect.x += rect.width + 3;
    rect.width = font_desc.width;
    if (tig_window_text_write(window_handle, "*", &rect) != TIG_OK) {
        tig_debug_printf("intgame_text_edit_refresh_color: ERROR: tig_window_text_write failed!\n");
    }
    tig_font_pop();
}

// 0x5522F0
void intgame_clock_refresh(void)
{
    char str[32];
    TigRect rect;
    TigArtBlitInfo art_blit_info;
    TigRect src_rect;
    TigRect dst_rect;
    TigFont font_desc;
    DateTime datetime;
    int strip_offset;
    int cycle;
    int num_cycles;
    int width;
    int dst_x;
    int dst_width;

    if (!intgame_iso_interface_created) {
        return;
    }

    if (intgame_clock_debug_text_enabled) {
        snprintf(str, sizeof(str), "%d Hours", datetime_current_hour());

        rect.x = intgame_interface_window_frames[0].x + 650;
        rect.y = intgame_interface_window_frames[0].y + 5;
        rect.width = 50;
        rect.height = 25;

        font_desc.str = str;
        font_desc.width = 0;
        tig_font_measure(&font_desc);

        if (tig_window_fill(intgame_interface_windows[0], &rect, tig_color_make(0, 0, 0)) == TIG_OK) {
            tig_window_text_write(intgame_interface_windows[0], str, &rect);
        }
        return;
    }

    datetime = game_time_get();
    strip_offset = (intgame_clock_strip_widths[1] + intgame_clock_strip_widths[0]
             + (datetime_seconds_since_reference_date(&datetime) + 73800) % 86400 * (intgame_clock_strip_widths[1] + intgame_clock_strip_widths[0]) / 86400
             - intgame_clock_frame.width / 2)
        % (intgame_clock_strip_widths[1] + intgame_clock_strip_widths[0]);
    if (intgame_clock_last_strip_offset == strip_offset) {
        return;
    }

    intgame_clock_last_strip_offset = strip_offset;
    tig_window_fill(intgame_interface_windows[0], &intgame_clock_frame, tig_color_make(0, 0, 0));

    dst_x = intgame_clock_frame.x;
    dst_width = intgame_clock_frame.width;

    cycle = 0;
    num_cycles = 0;
    while (dst_width > 0 && num_cycles < 3) {
        width = intgame_clock_strip_widths[cycle];
        if (cycle == 0) {
            // 207: "clk_timestrip.art"
            if (tig_art_interface_id_create(207, 0, 0, 0, &(art_blit_info.art_id)) != TIG_OK) {
                tig_debug_printf("intgame_clock_refresh: ERROR: tig_art_interface_id_create failed!\n");
                return;
            }
        } else {
            int moon_phase_day;
            int idx;

            moon_phase_day = ((datetime_seconds_since_reference_date(&datetime) + 43200) / 84600) % 28;

            for (idx = 0; idx < 8; idx++) {
                if (moon_phase_day <= intgame_moon_phase_day_thresholds[idx]) {
                    break;
                }
            }

            // 208 - 215: "clk_moon#.art"
            if (tig_art_interface_id_create(208 + idx, 0, 0, 0, &(art_blit_info.art_id)) != TIG_OK) {
                tig_debug_printf("intgame_clock_refresh: ERROR: tig_art_interface_id_create2 failed!\n");
                return;
            }
        }

        if (strip_offset < width) {
            src_rect.x = strip_offset;
            src_rect.y = 0;
            src_rect.width = SDL_min(width - strip_offset, dst_width);
            src_rect.height = intgame_clock_frame.height;

            dst_rect.x = dst_x;
            dst_rect.y = intgame_clock_frame.y;
            dst_rect.width = src_rect.width;
            dst_rect.height = intgame_clock_frame.height;

            art_blit_info.flags = 0;
            art_blit_info.src_rect = &src_rect;
            art_blit_info.dst_rect = &dst_rect;
            tig_window_blit_art(intgame_interface_windows[0], &art_blit_info);

            dst_x += src_rect.width;
            dst_width -= src_rect.width;

            strip_offset = 0;
        } else {
            strip_offset -= width;
        }

        cycle = 1 - cycle;
        num_cycles++;
    }

    // 216: "timepnts.art"
    if (tig_art_interface_id_create(216, 0, 0, 0, &(art_blit_info.art_id)) != TIG_OK) {
        tig_debug_printf("intgame_clock_refresh: ERROR: tig_art_interface_id_create3 failed!\n");
        return;
    }

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = 100;
    src_rect.height = 100;

    dst_rect.x = 708;
    dst_rect.y = 6;
    dst_rect.width = 100;
    dst_rect.height = 100;

    art_blit_info.flags = 0;
    art_blit_info.src_rect = &src_rect;
    art_blit_info.dst_rect = &dst_rect;
    tig_window_blit_art(intgame_interface_windows[0], &art_blit_info);
}

// 0x5526F0
bool intgame_clock_process_callback(TimeEvent* timeevent)
{
    DateTime datetime;
    TimeEvent next_timeevent;

    (void)timeevent;

    if (intgame_iso_interface_created) {
        intgame_clock_refresh();
        next_timeevent.type = TIMEEVENT_TYPE_CLOCK;
        datetime_init(&datetime, 3600000);
        timeevent_clear_one_typed(TIMEEVENT_TYPE_CLOCK);
        timeevent_add_delay(&next_timeevent, &datetime);
    }

    return true;
}

// 0x552740
void intgame_character_sheet_open(int64_t obj, ChareditMode mode)
{
    if (!intgame_iso_interface_created) {
        return;
    }

    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    charedit_open(obj, mode);
}

// 0x552770
void intgame_message_history_append(UiMessage* ui_message)
{
    // 0x64C6EC
    static tig_timestamp_t intgame_last_persistent_message_time;

    int prev_idx;

    if (ui_message->type >= UI_MSG_TYPE_FEEDBACK && ui_message->type <= UI_MSG_TYPE_STAT) {
        if (tig_timer_elapsed(intgame_last_persistent_message_time) > 3000
            && intgame_iso_window_type == ROTWIN_TYPE_MSG) {
            intgame_message_window_clear_internal();
            intgame_message_draw(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
                ui_message,
                true);
        }
    } else {
        tig_timer_now(&intgame_last_persistent_message_time);

        prev_idx = intgame_message_history_end;
        if (intgame_message_history_size < MAX_MESSAGE_HISTORY_ITEMS) {
            intgame_message_history_end = intgame_message_history_size;
        } else {
            intgame_message_history_end = (intgame_message_history_end + 1) % MAX_MESSAGE_HISTORY_ITEMS;
        }

        if (intgame_message_history_size != 0
            && (ui_message->type == UI_MSG_TYPE_EXCLAMATION
                || ui_message->type == UI_MSG_TYPE_QUESTION)
            && SDL_strcasecmp(ui_message->str, intgame_message_history[prev_idx].str) == 0) {
            intgame_message_history_end = prev_idx;
            intgame_message_history_curr = prev_idx;
            intgame_message_refresh(true);
            return;
        }

        if (intgame_message_history_size < MAX_MESSAGE_HISTORY_ITEMS) {
            intgame_message_history_size += 1;
        }

        intgame_message_history[intgame_message_history_end].type = ui_message->type;
        strncpy(intgame_message_history[intgame_message_history_end].str, ui_message->str, MAX_MESSAGE_HISTORY_STRING_SIZE);
        intgame_message_history[intgame_message_history_end].str[MAX_MESSAGE_HISTORY_STRING_SIZE - 1] = '\0';
        intgame_message_history[intgame_message_history_end].primary_value = ui_message->primary_value;
        intgame_message_history[intgame_message_history_end].secondary_value = ui_message->secondary_value;
        intgame_message_history_curr = intgame_message_history_end;
        intgame_message_refresh(true);
    }
}

// 0x5528E0
void intgame_message_history_scroll_up(void)
{
    int idx;

    if (intgame_iso_window_type == ROTWIN_TYPE_MSG) {
        idx = (intgame_message_history_curr + MAX_MESSAGE_HISTORY_ITEMS - 1) % MAX_MESSAGE_HISTORY_ITEMS;
        if (idx != intgame_message_history_end
            && (intgame_message_history_end - idx + MAX_MESSAGE_HISTORY_ITEMS) % MAX_MESSAGE_HISTORY_ITEMS < intgame_message_history_size) {
            intgame_message_history_curr = idx;
        }
    }

    intgame_message_refresh(false);
}

// 0x552930
void intgame_message_history_scroll_down(void)
{
    if (intgame_iso_window_type == ROTWIN_TYPE_MSG) {
        if (intgame_message_history_curr != intgame_message_history_end) {
            intgame_message_history_curr = (intgame_message_history_curr + 1) % MAX_MESSAGE_HISTORY_ITEMS;
        }
    }

    intgame_message_refresh(false);
}

// 0x552960
void intgame_message_refresh(bool play_sound)
{
    if (!intgame_iso_interface_created) {
        return;
    }

    iso_interface_window_set(ROTWIN_TYPE_MSG);

    if (intgame_iso_window_type == ROTWIN_TYPE_MSG) {
        if (intgame_message_history_size > 0) {
            intgame_message_window_clear_internal();
            intgame_message_draw(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
                &(intgame_message_history[intgame_message_history_curr]),
                play_sound);
        }
    }
}

// 0x5529C0
void intgame_message_draw(tig_window_handle_t window_handle, UiMessage* ui_message, bool play_sound)
{
    MesFileEntry mes_file_entry1;
    MesFileEntry mes_file_entry2;
    char str[MAX_STRING];

    if (intgame_is_compact_interface()) {
        window_handle = compact_ui_message_window_acquire();
    }

    switch (ui_message->type) {
    case UI_MSG_TYPE_LEVEL:
        intgame_message_window_draw_image(window_handle, intgame_message_icons[ui_message->type]);

        mes_file_entry1.num = 21; // "Level Up"
        mes_get_msg(intgame_mes_file, &mes_file_entry1);
        intgame_message_window_write_text(window_handle,
            mes_file_entry1.str,
            &intgame_message_title_rect,
            intgame_morph15_white_font,
            MSG_TEXT_HALIGN_LEFT);

        mes_file_entry1.num = 11; // "Congratulations! You are now level %d."
        mes_get_msg(intgame_mes_file, &mes_file_entry1);
        snprintf(str, sizeof(str), mes_file_entry1.str, ui_message->primary_value);
        intgame_message_window_write_text(window_handle,
            str,
            &intgame_message_line_2_rect,
            intgame_flare12_white_font,
            MSG_TEXT_HALIGN_LEFT);

        if (ui_message->secondary_value != -1) {
            mes_file_entry1.num = 12; // "You now have %d character point(s) to spend."
            mes_get_msg(intgame_mes_file, &mes_file_entry1);
            snprintf(str, sizeof(str), mes_file_entry1.str, ui_message->secondary_value);
            intgame_message_window_write_text(window_handle,
                str,
                &intgame_message_line_3_rect,
                intgame_flare12_white_font,
                MSG_TEXT_HALIGN_LEFT);
        } else {
            intgame_message_window_write_text(window_handle,
                ui_message->str,
                &intgame_message_lower_text_rect,
                intgame_flare12_white_font,
                MSG_TEXT_HALIGN_LEFT);
        }

        if (play_sound) {
            gsound_play_sfx(SND_INTERFACE_LEVEL_UP, 1);
        }
        break;
    case UI_MSG_TYPE_POISON:
        intgame_message_window_draw_image(window_handle, intgame_message_icons[ui_message->type]);
        intgame_message_window_write_text(window_handle,
            ui_message->str,
            &intgame_message_title_rect,
            intgame_morph15_white_font,
            MSG_TEXT_HALIGN_LEFT);

        mes_file_entry1.num = 13; // "You have absorbed %d unit(s) of poison."
        mes_get_msg(intgame_mes_file, &mes_file_entry1);
        snprintf(str, sizeof(str), mes_file_entry1.str, ui_message->primary_value);
        intgame_message_window_write_text(window_handle,
            str,
            &intgame_message_line_2_rect,
            intgame_flare12_red_font,
            MSG_TEXT_HALIGN_LEFT);

        if (play_sound) {
            gsound_play_sfx(SND_INTERFACE_POISONED, 1);
        }
        break;
    case UI_MSG_TYPE_CURSE:
        intgame_message_window_draw_image(window_handle, intgame_message_icons[ui_message->type]);
        intgame_message_window_write_text(window_handle,
            ui_message->str,
            &intgame_message_title_rect,
            intgame_morph15_white_font,
            MSG_TEXT_HALIGN_LEFT);

        curse_copy_description(ui_message->primary_value, str, sizeof(str));
        intgame_message_window_write_text(window_handle,
            str,
            &intgame_message_description_rect,
            intgame_flare12_white_font,
            MSG_TEXT_HALIGN_LEFT);

        if (play_sound) {
            gsound_play_sfx(SND_INTERFACE_CURSED, 1);
        }
        break;
    case UI_MSG_TYPE_BLESS:
        intgame_message_window_draw_image(window_handle, intgame_message_icons[ui_message->type]);
        intgame_message_window_write_text(window_handle,
            ui_message->str,
            &intgame_message_title_rect,
            intgame_morph15_white_font,
            MSG_TEXT_HALIGN_LEFT);

        bless_copy_description(ui_message->primary_value, str, sizeof(str));
        intgame_message_window_write_text(window_handle,
            str,
            &intgame_message_description_rect,
            intgame_flare12_white_font,
            MSG_TEXT_HALIGN_LEFT);

        if (play_sound) {
            gsound_play_sfx(SND_INTERFACE_BLESS, 1);
        }
        break;
    case UI_MSG_TYPE_EXCLAMATION:
        if (play_sound) {
            gsound_play_sfx(SND_INTERFACE_EXCLAMATION, 1);
        }
        // FALLTHROUGH
    case UI_MSG_TYPE_QUESTION:
    case UI_MSG_TYPE_FEEDBACK: {
        size_t pos = 0;
        bool rc;

        intgame_message_window_draw_image(window_handle, intgame_message_icons[ui_message->type]);

        while (ui_message->str[pos] != '\0' && ui_message->str[pos] != '\n') {
            pos++;
        }

        if (ui_message->str[pos] == '\n') {
            ui_message->str[pos] = '\0';
            intgame_message_window_write_text(window_handle,
                ui_message->str,
                &intgame_message_title_rect,
                intgame_morph15_white_font,
                MSG_TEXT_HALIGN_LEFT);
            rc = intgame_message_window_write_text(window_handle,
                ui_message->str + pos + 1,
                &intgame_message_description_rect,
                intgame_flare12_white_font,
                MSG_TEXT_HALIGN_LEFT);
            ui_message->str[pos] = '\n';
        } else {
            rc = intgame_message_window_write_text(window_handle, ui_message->str, &intgame_message_full_text_rect, intgame_flare12_white_font, 1);
        }

        if (!rc) {
            intgame_message_window_clear_internal();
            if (ui_message->str[pos] == '\n') {
                ui_message->str[pos] = '\0';
                intgame_message_window_write_text(window_handle, ui_message->str,
                    &intgame_message_wide_title_rect,
                    intgame_morph15_white_font,
                    MSG_TEXT_HALIGN_LEFT | MSG_TEXT_TRUNCATE);
                intgame_message_window_write_text(window_handle,
                    ui_message->str + pos + 1,
                    &intgame_message_wide_description_rect,
                    intgame_flare12_white_font,
                    MSG_TEXT_HALIGN_LEFT | MSG_TEXT_TRUNCATE);
                ui_message->str[pos] = '\n';
            } else {
                intgame_message_window_write_text(window_handle,
                    ui_message->str,
                    &intgame_message_wide_text_rect,
                    intgame_flare12_white_font,
                    MSG_TEXT_HALIGN_LEFT | MSG_TEXT_TRUNCATE);
            }
        }
        break;
    }
    case UI_MSG_TYPE_SKILL:
        intgame_message_window_draw_image(window_handle, intgame_message_icons[UI_MSG_TYPE_FEEDBACK]);
        intgame_message_window_write_text(window_handle,
            IS_TECH_SKILL(ui_message->primary_value)
                ? tech_skill_name(GET_TECH_SKILL(ui_message->primary_value))
                : basic_skill_name(GET_BASIC_SKILL(ui_message->primary_value)),
            &intgame_message_title_rect,
            intgame_morph15_white_font,
            MSG_TEXT_HALIGN_LEFT);
        intgame_message_window_write_text(window_handle,
            IS_TECH_SKILL(ui_message->primary_value)
                ? tech_skill_description(GET_TECH_SKILL(ui_message->primary_value))
                : basic_skill_description(GET_BASIC_SKILL(ui_message->primary_value)),
            &intgame_message_description_rect,
            intgame_flare12_white_font,
            MSG_TEXT_HALIGN_LEFT);

        if (ui_message->secondary_value != 0) {
            mes_file_entry1.num = 28 + (IS_TECH_SKILL(ui_message->primary_value) ? tech_skill_stat(GET_TECH_SKILL(ui_message->primary_value)) : basic_skill_stat(GET_BASIC_SKILL(ui_message->primary_value)));
            mes_get_msg(intgame_mes_file, &mes_file_entry1);
            snprintf(str, sizeof(str),
                "%s: %d",
                mes_file_entry1.str,
                ui_message->secondary_value);
            intgame_message_window_write_text(window_handle,
                str,
                &intgame_message_title_rect,
                intgame_flare12_red_font,
                MSG_TEXT_HALIGN_RIGHT);
        }
        break;
    case UI_MSG_TYPE_SPELL: {
        intgame_message_window_draw_image(window_handle, spell_college_large_icon(ui_message->primary_value / 5));
        intgame_message_window_write_text(window_handle,
            spell_name(ui_message->primary_value),
            &intgame_message_title_rect,
            intgame_morph15_white_font,
            MSG_TEXT_HALIGN_LEFT);

        mes_file_entry1.num = 73; // "Bonus to Heal skill"
        mes_get_msg(intgame_mes_file, &mes_file_entry1);
        snprintf(str, sizeof(str),
            "%s: %d",
            mes_file_entry1.str,
            spell_min_willpower(ui_message->primary_value));
        intgame_message_window_write_text(window_handle,
            str,
            &intgame_message_line_2_rect,
            intgame_flare12_white_font,
            MSG_TEXT_HALIGN_LEFT);

        mes_file_entry1.num = 59; // "Damage"
        mes_get_msg(intgame_mes_file, &mes_file_entry1);

        int cast_cost;
        int maintain_cost;
        int maintain_period;

        cast_cost = spell_cast_cost(ui_message->primary_value, ui_message->caster_obj);
        maintain_cost = spell_maintain_cost(ui_message->primary_value, ui_message->caster_obj, &maintain_period);
        if (maintain_period == 1) {
            mes_file_entry2.num = 74; // "second"
            mes_get_msg(intgame_mes_file, &mes_file_entry2);
            snprintf(str, sizeof(str),
                "%s: %d  (%d / %s)",
                mes_file_entry1.str,
                cast_cost,
                maintain_cost,
                mes_file_entry2.str);
        } else if (maintain_period > 1) {
            mes_file_entry2.num = 75; // "seconds"
            mes_get_msg(intgame_mes_file, &mes_file_entry2);
            snprintf(str, sizeof(str),
                "%s: %d  (%d / %d %s)",
                mes_file_entry1.str,
                cast_cost,
                maintain_cost,
                maintain_period,
                mes_file_entry2.str);
        } else {
            snprintf(str, sizeof(str),
                "%s: %d",
                mes_file_entry1.str,
                cast_cost);
        }
        intgame_message_window_write_text(window_handle,
            str,
            &intgame_message_line_2_rect,
            intgame_flare12_white_font,
            MSG_TEXT_HALIGN_RIGHT);

        intgame_message_window_write_text(window_handle,
            spell_description(ui_message->primary_value),
            &intgame_message_lower_text_rect,
            intgame_flare12_white_font,
            MSG_TEXT_HALIGN_LEFT);
        break;
    }
    case UI_MSG_TYPE_COLLEGE:
        intgame_message_window_draw_image(window_handle, spell_college_large_icon(ui_message->primary_value));
        intgame_message_window_write_text(window_handle,
            spell_college_name(ui_message->primary_value),
            &intgame_message_title_rect,
            intgame_morph15_white_font,
            MSG_TEXT_HALIGN_LEFT);
        intgame_message_window_write_text(window_handle,
            spell_college_description(ui_message->primary_value),
            &intgame_message_description_rect,
            intgame_flare12_white_font,
            MSG_TEXT_HALIGN_LEFT);
        break;
    case UI_MSG_TYPE_TECH:
        intgame_message_window_draw_image(window_handle, intgame_message_icons[UI_MSG_TYPE_FEEDBACK]);
        intgame_message_window_write_text(window_handle,
            tech_discipline_name_get(ui_message->primary_value),
            &intgame_message_title_rect,
            intgame_morph15_white_font,
            MSG_TEXT_HALIGN_LEFT);
        intgame_message_window_write_text(window_handle,
            tech_discipline_description_get(ui_message->primary_value),
            &intgame_message_description_rect,
            intgame_flare12_white_font,
            MSG_TEXT_HALIGN_LEFT);
        break;
    case UI_MSG_TYPE_DEGREE:
        intgame_message_window_draw_image(window_handle, intgame_message_icons[UI_MSG_TYPE_FEEDBACK]);
        intgame_message_window_write_text(window_handle,
            tech_degree_name_get(ui_message->primary_value % 8),
            &intgame_message_title_rect,
            intgame_morph15_white_font,
            MSG_TEXT_HALIGN_LEFT);

        mes_file_entry1.num = 17;
        mes_get_msg(intgame_mes_file, &mes_file_entry1);
        snprintf(str, sizeof(str),
            "%s: %d",
            mes_file_entry1.str,
            tech_degree_min_intelligence_get(ui_message->primary_value % 8));
        intgame_message_window_write_text(window_handle,
            str,
            &intgame_message_title_rect,
            intgame_flare12_white_font,
            MSG_TEXT_HALIGN_RIGHT);

        intgame_message_window_write_text(window_handle,
            tech_degree_description_get(ui_message->primary_value % 8, ui_message->primary_value / 8),
            &intgame_message_description_rect,
            intgame_flare12_white_font,
            MSG_TEXT_HALIGN_LEFT);
        break;
    case UI_MSG_TYPE_STAT: {
        size_t pos = 0;

        intgame_message_window_draw_image(window_handle, intgame_message_icons[UI_MSG_TYPE_FEEDBACK]);

        while (ui_message->str[pos] != '\0' && ui_message->str[pos] != '\n') {
            pos++;
        }

        if (ui_message->str[pos] == '\n') {
            ui_message->str[pos] = '\0';
            intgame_message_window_write_text(window_handle,
                ui_message->str,
                &intgame_message_title_rect,
                intgame_morph15_white_font,
                MSG_TEXT_HALIGN_LEFT);
            intgame_message_window_write_text(window_handle,
                ui_message->str + pos + 1,
                &intgame_message_description_rect,
                intgame_flare12_white_font,
                MSG_TEXT_HALIGN_LEFT);
            ui_message->str[pos] = '\n';
        } else {
            intgame_message_window_write_text(window_handle,
                ui_message->str,
                &intgame_message_full_text_rect,
                intgame_flare12_white_font,
                MSG_TEXT_HALIGN_LEFT);
        }
        break;
    }
    case UI_MSG_TYPE_SCHEMATIC:
        intgame_message_window_draw_image(window_handle, intgame_message_icons[UI_MSG_TYPE_FEEDBACK]);
        intgame_message_window_write_text(window_handle,
            schematic_ui_product_name(ui_message->primary_value),
            &intgame_message_title_rect,
            intgame_morph15_white_font,
            MSG_TEXT_HALIGN_LEFT);
        intgame_message_window_write_text(window_handle,
            ui_message->str,
            &intgame_message_description_rect,
            intgame_flare12_white_font,
            MSG_TEXT_HALIGN_LEFT);
        break;
    }
}

// 0x553320
bool intgame_dialog_begin(bool (*func)(TigMessage* msg))
{
    intgame_dialog_process_event_func = func;
    tc_show();
    intgame_mode_set(INTGAME_MODE_MAIN);
    intgame_mode_set(INTGAME_MODE_DIALOG);

    return true;
}

// 0x553350
void intgame_dialog_end(void)
{
    intgame_dialog_process_event_func = NULL;
    tc_hide();
    dialog_camera_end(player_get_local_pc_obj());
    intgame_mode_set(INTGAME_MODE_MAIN);
}

// 0x553370
void intgame_dialog_clear(void)
{
    tc_clear(intgame_compact_interface);
}

// 0x553380
void intgame_dialog_set_option(int index, const char* str)
{
    tc_set_option(index, str);
}

// 0x5533A0
int intgame_dialog_get_option(TigMessage* msg)
{
    return tc_handle_message(msg);
}

// TODO: Reuse `iso_interface_window_get`.
//
// 0x5533B0
RotatingWindowType iso_interface_window_get_2(void)
{
    return intgame_iso_window_type;
}

// 0x5533C0
void intgame_spell_maintain_art_set_func(UiButtonInfo* button, int slot, tig_art_id_t art_id, tig_window_handle_t window_handle)
{
    int64_t pc_obj;
    int num_slots;
    TigArtFrameData art_frame_data;
    TigArtBlitInfo art_blit_info;
    TigRect src_rect;
    TigRect dst_rect;

    if (!intgame_iso_interface_created) {
        return;
    }

    if (art_id == TIG_ART_ID_INVALID) {
        pc_obj = player_get_local_pc_obj();
        if (pc_obj != OBJ_HANDLE_NULL) {
            num_slots = stat_level_get(pc_obj, STAT_INTELLIGENCE) / 4;
        } else {
            num_slots = 5;
        }

        if (tig_art_interface_id_create(188 + slot, 0, 0, 0, &art_id) == TIG_OK) {
            tig_button_set_art(button->button_handle, art_id);
            tig_button_hide(button->button_handle);
        }

        if (slot < num_slots) {
            if (tig_art_frame_data(art_id, &art_frame_data) == TIG_OK) {
                src_rect.x = 0;
                src_rect.y = 0;
                src_rect.width = art_frame_data.width;
                src_rect.height = art_frame_data.height;

                dst_rect.x = 0;
                dst_rect.y = 0;
                dst_rect.width = art_frame_data.width;
                dst_rect.height = art_frame_data.height;

                art_blit_info.flags = 0;
                art_blit_info.art_id = art_id;
                art_blit_info.src_rect = &src_rect;
                art_blit_info.dst_rect = &dst_rect;

                if (window_handle == intgame_interface_windows[0]) {
                    dst_rect.x = button->x;
                    dst_rect.y = button->y;
                } else {
                    dst_rect.x = button->x - intgame_maintain_window_rects[slot].x;
                    dst_rect.y = button->y - intgame_maintain_window_rects[slot].y;
                }

                tig_window_blit_art(window_handle, &art_blit_info);
            }
        } else {
            if (tig_art_interface_id_create(628 + slot, 0, 0, 0, &art_id) == TIG_OK
                && tig_art_frame_data(art_id, &art_frame_data) == TIG_OK) {
                src_rect.x = 0;
                src_rect.y = 0;
                src_rect.width = art_frame_data.width;
                src_rect.height = art_frame_data.height;

                dst_rect.x = button->x - 1;
                dst_rect.y = button->y - 1;
                dst_rect.width = art_frame_data.width;
                dst_rect.height = art_frame_data.height;

                art_blit_info.flags = 0;
                art_blit_info.art_id = art_id;
                art_blit_info.src_rect = &src_rect;
                art_blit_info.dst_rect = &dst_rect;

                tig_window_blit_art(window_handle, &art_blit_info);
            }
        }

        if (window_handle != intgame_interface_windows[0]) {
            tig_window_hide(intgame_maintain_fs_windows[slot]);
        }
    } else {
        if (window_handle != intgame_interface_windows[0]) {
            tig_window_show(intgame_maintain_fs_windows[slot]);
        }
        tig_button_set_art(button->button_handle, art_id);
        tig_button_show(button->button_handle);
    }
}

// 0x553620
void intgame_spell_maintain_art_set(int slot, tig_art_id_t art_id)
{
    if (!intgame_iso_interface_created) {
        return;
    }

    intgame_spell_maintain_art_set_func(&(intgame_maintain_buttons[slot]),
        slot,
        art_id,
        intgame_interface_windows[0]);
    intgame_spell_maintain_art_set_func(&(intgame_maintain_fs_buttons[slot]),
        slot,
        art_id,
        intgame_maintain_fs_windows[slot]);
}

// 0x553670
void intgame_spell_maintain_refresh_func(tig_button_handle_t button_handle, UiButtonInfo* info, int slot, bool active, tig_window_handle_t window_handle)
{
    TigButtonData button_data;
    TigArtFrameData art_frame_data;
    TigArtBlitInfo art_blit_info;
    TigRect src_rect;
    TigRect dst_rect;
    int current_num;
    bool hidden;
    tig_art_id_t art_id;

    if (tig_button_data(button_handle, &button_data) != TIG_OK) {
        return;
    }

    current_num = tig_art_num_get(button_data.art_id);
    if (active) {
        if (current_num == slot + 188 && tig_button_is_hidden(button_handle, &hidden) == TIG_OK) {
            tig_button_show(button_handle);
            if (tig_art_interface_id_create(slot + 188, 0, 0, 0, &art_id) != TIG_OK) {
                tig_debug_printf("intgame_spell_maintain_refresh_func: ERROR: tig_art_interface_id_create failed!\n");
                return;
            }

            tig_button_set_art(button_handle, art_id);
            if (tig_art_interface_id_create(slot + 628, 0, 0, 0, &art_id) != TIG_OK) {
                return;
            }

            if (tig_art_frame_data(art_id, &art_frame_data) != TIG_OK) {
                return;
            }

            if (tig_art_interface_id_create(185, 0, 0, 0, &art_id) != TIG_OK) {
                return;
            }

            src_rect.x = info->x - 1;
            src_rect.y = info->y - 1;
            src_rect.width = art_frame_data.width;
            src_rect.height = art_frame_data.height;

            dst_rect.x = src_rect.x;
            dst_rect.y = info->y - 1;
            dst_rect.width = art_frame_data.width;
            dst_rect.height = art_frame_data.height;

            art_blit_info.flags = 0;
            art_blit_info.art_id = art_id;
            art_blit_info.src_rect = &src_rect;
            art_blit_info.dst_rect = &dst_rect;
            tig_window_blit_art(window_handle, &art_blit_info);
        }
    } else if (current_num != slot + 628) {
        if (current_num == slot + 188) {
            if (tig_art_interface_id_create(slot + 188, 0, 0, 0, &art_id)) {
                tig_button_set_art(button_handle, art_id);
                tig_button_hide(button_handle);
            }

            if (tig_art_interface_id_create(slot + 628, 0, 0, 0, &art_id) != TIG_OK) {
                return;
            }

            if (tig_art_frame_data(art_id, &art_frame_data) != TIG_OK) {
                return;
            }

            src_rect.x = info->x - 1;
            src_rect.y = info->y - 1;
            src_rect.width = art_frame_data.width;
            src_rect.height = art_frame_data.height;

            dst_rect.x = 0;
            dst_rect.y = 0;
            dst_rect.width = art_frame_data.width;
            dst_rect.height = art_frame_data.height;

            art_blit_info.flags = 0;
            art_blit_info.art_id = art_id;
            art_blit_info.src_rect = &dst_rect;
            art_blit_info.dst_rect = &src_rect;
            tig_window_blit_art(window_handle, &art_blit_info);
        } else {
            spell_ui_maintain_click(slot);
        }
    }
}

// 0x553910
void intgame_spell_maintain_refresh(int slot, bool active)
{
    intgame_spell_maintain_refresh_func(intgame_maintain_buttons[slot].button_handle,
        &(intgame_maintain_buttons[slot]),
        slot,
        active,
        intgame_interface_windows[0]);
    intgame_spell_maintain_refresh_func(intgame_maintain_fs_buttons[slot].button_handle,
        &(intgame_maintain_fs_buttons[slot]),
        slot,
        active,
        intgame_maintain_fs_windows[slot]);
}

// 0x553960
void intgame_refresh_quantity(void)
{
    roller_ui_draw(intgame_quantity, intgame_interface_windows[1], 404, 104, 6, 0);
}

// 0x553990
void intgame_refresh_cursor(void)
{
    bool have_weapon = false;
    int64_t pc_obj;
    tig_art_id_t art_id;
    int art_num;

    pc_obj = player_get_local_pc_obj();
    if (pc_obj != OBJ_HANDLE_NULL) {
        art_id = obj_field_int32_get(pc_obj, OBJ_F_CURRENT_AID);
        if (tig_art_critter_id_weapon_get(art_id) != TIG_ART_WEAPON_TYPE_NO_WEAPON) {
            have_weapon = true;
        }
    }

    if (!hotkey_ui_is_dragging()) {
        // FIXME: Meaningless.
        tig_mouse_cursor_get_art_id();

        art_num = intgame_mode_cursors[intgame_mode_get()];
        if (art_num == -1) {
            if (have_weapon) {
                if (tig_kb_is_key_pressed(SDL_SCANCODE_COMMA)) {
                    art_num = 818; // "cursor-called-head.art"
                } else if (tig_kb_is_key_pressed(SDL_SCANCODE_PERIOD)) {
                    art_num = 819; // "cursor-called-leg.art"
                } else if (tig_kb_is_key_pressed(SDL_SCANCODE_SLASH)) {
                    art_num = 820; // "cursor-called-arm.art"
                } else {
                    art_num = 353; // "battlecur.art"
                }
            } else {
                art_num = 0; // "cursor.art"
            }
        }

        tig_art_interface_id_create(art_num, 0, 0, 0, &art_id);
        tig_mouse_cursor_set_art_id(art_id);

        inven_ui_drag_cursor_refresh();
    }
}

// 0x553A60
void intgame_item_mode_cursor_set(int art_num)
{
    intgame_mode_cursors[INTGAME_MODE_ITEM] = art_num;
}

// 0x553A70
void intgame_object_hover_process(TigMessage* msg)
{
    int64_t obj;
    TargetDescriptor td;
    int x;
    int y;

    if (!intgame_mouse_message_is_over_window(msg)) {
        return;
    }

    obj = object_hover_obj_get();
    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    intgame_adjust_mouse_for_zoom(msg->data.mouse.x, msg->data.mouse.y, &x, &y);

    if (target_pick_at_screen_xy_ex(x, y, &td, intgame_hover_target_flags, intgame_fullscreen)) {
        if (obj != td.obj) {
            tb_ui_examine_and_select_object(player_get_local_pc_obj(), td.obj);
            object_hover_obj_set(td.obj);
        }
    } else {
        if (intgame_selected_obj != OBJ_HANDLE_NULL || object_hover_obj_get() == OBJ_HANDLE_NULL) {
            if (intgame_iso_window_type != ROTWIN_TYPE_CHAT) {
                if (obj_handle_is_valid(intgame_selected_obj)) {
                    object_hover_obj_set(intgame_selected_obj);
                    object_hover_obj_set(OBJ_HANDLE_NULL);
                    tb_ui_examine_and_select_object(player_get_local_pc_obj(), intgame_selected_obj);
                }
                intgame_selected_obj = OBJ_HANDLE_NULL;
            }
        } else {
            if (intgame_iso_window_type != ROTWIN_TYPE_CHAT) {
                intgame_message_window_display_str(-1, "");
                compact_ui_message_window_release();
            }
            object_hover_obj_set(OBJ_HANDLE_NULL);
        }
    }
}

// 0x553BE0
void intgame_examine_object(int64_t pc_obj, int64_t target_obj, char* str)
{
    int type;

    if (intgame_iso_window_type != ROTWIN_TYPE_CHAT) {
        if (intgame_iso_window_type != ROTWIN_TYPE_MSG) {
            intgame_message_window_display_str(-1, str);
        } else {
            type = obj_field_int32_get(target_obj, OBJ_F_TYPE);
            switch (type) {
            case OBJ_TYPE_WALL:
                break;
            case OBJ_TYPE_PORTAL:
                intgame_examine_portal(pc_obj, target_obj, str);
                break;
            case OBJ_TYPE_CONTAINER:
                intgame_examine_container(pc_obj, target_obj, str);
                break;
            case OBJ_TYPE_SCENERY:
                intgame_examine_scenery(pc_obj, target_obj, str);
                break;
            case OBJ_TYPE_PROJECTILE:
                break;
            case OBJ_TYPE_WEAPON:
            case OBJ_TYPE_AMMO:
            case OBJ_TYPE_ARMOR:
            case OBJ_TYPE_GOLD:
            case OBJ_TYPE_FOOD:
            case OBJ_TYPE_SCROLL:
            case OBJ_TYPE_KEY:
            case OBJ_TYPE_KEY_RING:
            case OBJ_TYPE_WRITTEN:
            case OBJ_TYPE_GENERIC:
                intgame_examine_item(pc_obj, target_obj, str);
                break;
            case OBJ_TYPE_PC:
            case OBJ_TYPE_NPC:
                intgame_examine_critter(pc_obj, target_obj, str);
                break;
            default:
                intgame_message_window_display_str(-1, str);
                break;
            }
        }
    }
}

// 0x553D10
bool intgame_examine_portrait(int64_t pc_obj, int64_t target_obj, int* portrait_ptr)
{
    unsigned int critter_flags;

    *portrait_ptr = 432; // levelupicon.art

    switch (obj_field_int32_get(target_obj, OBJ_F_TYPE)) {
    case OBJ_TYPE_PORTAL:
        *portrait_ptr = tig_art_portal_id_type_get(obj_field_int32_get(target_obj, OBJ_F_CURRENT_AID)) == TIG_ART_PORTAL_TYPE_WINDOW
            ? 786 // iconwindow.art
            : 436; // door_icon.art
        return false;
    case OBJ_TYPE_CONTAINER:
        switch (proto_number_from_instance(target_obj)) {
        case BP_JUNK_PILE:
            *portrait_ptr = 832; // cont_junk.art
            break;
        case BP_SAFE_1:
        case BP_SAFE_2:
            *portrait_ptr = 834; // cont_safe.art
            break;
        case BP_RUBBISH_BIN:
            *portrait_ptr = 835; // cont_trash.art
            break;
        case BP_BODY:
            *portrait_ptr = 833; // cont_body.art
            break;
        case BP_ALTAR_GOOD:
            *portrait_ptr = 837; // cont_altar_good.art
            break;
        case BP_ALTAR_NEUTRAL:
            *portrait_ptr = 838; // cont_altar_neutral.art
            break;
        case BP_ALTAR_EVIL:
            *portrait_ptr = 839; // cont_altar_evil.art
            break;
        case BP_PLANT_CONTAINER:
            *portrait_ptr = 836; // cont_trash.art
            break;
        default:
            *portrait_ptr = 435; // containericon.art
            break;
        }
        return false;
    case OBJ_TYPE_SCENERY:
        *portrait_ptr = 437; // levelupicon.art
        return false;
    case OBJ_TYPE_WEAPON:
    case OBJ_TYPE_AMMO:
    case OBJ_TYPE_ARMOR:
    case OBJ_TYPE_GOLD:
    case OBJ_TYPE_FOOD:
    case OBJ_TYPE_SCROLL:
    case OBJ_TYPE_KEY:
    case OBJ_TYPE_KEY_RING:
    case OBJ_TYPE_WRITTEN:
    case OBJ_TYPE_GENERIC:
        *portrait_ptr = intgame_item_icon_get(target_obj);
        return false;
    case OBJ_TYPE_PC:
        *portrait_ptr = portrait_get(target_obj);
        return true;
    case OBJ_TYPE_NPC:
        if (critter_pc_leader_get(target_obj) == pc_obj) {
            int portrait = portrait_get(target_obj);
            if (portrait != 0) {
                *portrait_ptr = portrait;
                return true;
            }
        }

        critter_flags = obj_field_int32_get(target_obj, OBJ_F_CRITTER_FLAGS);
        if ((critter_flags & OCF_UNDEAD) != 0) {
            *portrait_ptr = 384; // undead.art
            return false;
        }

        if ((critter_flags & OCF_MONSTER) != 0) {
            *portrait_ptr = 383; // monstericon.art
            return false;
        }

        if ((critter_flags & OCF_ANIMAL) != 0) {
            *portrait_ptr = 385; // animalicon.art
            return false;
        }

        if ((critter_flags & OCF_MECHANICAL) != 0) {
            *portrait_ptr = 434; // generaltechicon.art
            return false;
        }

        // Generic race-specific icon.
        *portrait_ptr = intgame_race_icons[stat_level_get(target_obj, STAT_RACE)];
        return false;
    default:
        return false;
    }
}

// 0x553F70
void intgame_examine_critter(int64_t pc_obj, int64_t critter_obj, char* str)
{
    int obj_type;
    bool is_detecting_alignment;
    int alignment;
    MesFileEntry mes_file_entry;
    char buffer[80];
    int64_t leader_obj;

    obj_type = obj_field_int32_get(critter_obj, OBJ_F_TYPE);
    intgame_message_window_clear_internal();

    leader_obj = critter_pc_leader_get(critter_obj);

    is_detecting_alignment = (obj_field_int32_get(pc_obj, OBJ_F_SPELL_FLAGS) & OSF_DETECTING_ALIGNMENT) != 0;
    if (is_detecting_alignment) {
        alignment = stat_level_get(critter_obj, STAT_ALIGNMENT);
    }

    if (pc_obj != critter_obj
        && combat_critter_is_combat_mode_active(pc_obj)
        && !critter_is_dead(critter_obj)) {
        intgame_weapon_skill_summary_draw(pc_obj, critter_obj);
    } else {
        if (is_detecting_alignment) {
            int step = (stat_level_max(critter_obj, STAT_ALIGNMENT) - stat_level_min(critter_obj, STAT_ALIGNMENT)) / 6;
            int alignment_type = (alignment - stat_level_min(critter_obj, STAT_ALIGNMENT)) / step;
            if (alignment_type > 5) {
                alignment_type = 5;
            }
            intgame_message_window_draw_image(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle, intgame_alignment_icons[alignment_type]);
        } else {
            int portrait;

            if (intgame_examine_portrait(pc_obj, critter_obj, &portrait)) {
                intgame_draw_portrait(critter_obj, portrait, intgame_rotwin_text_frame[intgame_iso_window_type].window_handle, 217, 69);
            } else {
                intgame_message_window_draw_image(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle, portrait);
            }
        }
    }

    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        str,
        &intgame_message_title_rect,
        intgame_morph15_white_font,
        MSG_TEXT_HALIGN_LEFT);

    if (critter_is_dead(critter_obj)) {
        mes_file_entry.num = 16; // "Dead"
        mes_get_msg(intgame_mes_file, &mes_file_entry);
        intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
            mes_file_entry.str,
            &intgame_message_line_2_rect,
            intgame_flare12_red_font,
            MSG_TEXT_HALIGN_LEFT);
        return;
    }

    if (obj_type == OBJ_TYPE_NPC) {
        if (critter_is_concealed(pc_obj)) {
            mes_file_entry.num = 37; // "Prowling state"
            mes_get_msg(intgame_mes_file, &mes_file_entry);

            int awareness_dist;
            int see_extra_dist = ai_can_see(critter_obj, pc_obj);
            if (see_extra_dist != 0) {
                awareness_dist = ai_can_hear(critter_obj, pc_obj, LOUDNESS_SILENT);
                if (awareness_dist >= see_extra_dist) {
                    awareness_dist = see_extra_dist;
                }
            } else {
                awareness_dist = 0;
            }

            if (critter_is_sleeping(critter_obj) && awareness_dist == 0) {
                awareness_dist = 1;
            }

            MesFileEntry suffix;
            switch (awareness_dist) {
            case 0:
                suffix.num = 42; // "Aware!!!"
                break;
            case 1:
                suffix.num = 41; // "Perilous!"
                break;
            case 2:
                suffix.num = 40; // "Dangerous"
                break;
            case 3:
                suffix.num = 39; // "Risky"
                break;
            default:
                suffix.num = 38; // "Safe"
                break;
            }

            mes_get_msg(intgame_mes_file, &suffix);
            snprintf(buffer, sizeof(buffer),
                "%s: %s",
                mes_file_entry.str,
                suffix.str);
        } else {
            if (is_detecting_alignment) {
                mes_file_entry.num = 36; // "Alignment"
                mes_get_msg(intgame_mes_file, &mes_file_entry);
                snprintf(buffer, sizeof(buffer),
                    "%s: %d",
                    mes_file_entry.str,
                    alignment / 10);
            } else {
                int reaction_value = reaction_get(critter_obj, pc_obj);
                int reaction_level = reaction_translate(reaction_value);
                const char* reaction_name = reaction_get_name(reaction_level);

                mes_file_entry.num = 1; // "Reaction"
                mes_get_msg(intgame_mes_file, &mes_file_entry);

                snprintf(buffer, sizeof(buffer),
                    "%s: %d (%s)",
                    mes_file_entry.str,
                    reaction_value,
                    reaction_name);
            }
        }

        intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
            buffer,
            &intgame_message_line_2_rect,
            intgame_flare12_white_font,
            MSG_TEXT_HALIGN_LEFT);
    }

    mes_file_entry.num = 0; // "Level"
    mes_get_msg(intgame_mes_file, &mes_file_entry);

    snprintf(buffer, sizeof(buffer),
        "%s: %d",
        mes_file_entry.str,
        stat_level_get(critter_obj, STAT_LEVEL));
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        buffer,
        &intgame_message_line_2_rect,
        intgame_flare12_white_font,
        MSG_TEXT_HALIGN_RIGHT);

    int cur_hp = object_hp_current(critter_obj);
    int max_hp = object_hp_max(critter_obj);
    int hp_ratio = 100 * cur_hp / max_hp;
    if (stat_level_get(critter_obj, STAT_POISON_LEVEL) > 0) {
        intgame_message_percentage_bar_draw(665, 666, &intgame_message_line_3_rect, hp_ratio);
    } else {
        intgame_message_percentage_bar_draw(463, 464, &intgame_message_line_3_rect, hp_ratio);
    }

    if (pc_obj == critter_obj || leader_obj == pc_obj) {
        snprintf(buffer, sizeof(buffer), "%d/%d", cur_hp, max_hp);
    } else {
        snprintf(buffer, sizeof(buffer), "%d%%", hp_ratio);
    }
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        buffer,
        &intgame_message_line_3_rect,
        intgame_flare12_red_font,
        MSG_TEXT_HALIGN_RIGHT);

    int cur_fatigue = critter_fatigue_current(critter_obj);
    int max_fatigue = critter_fatigue_max(critter_obj);
    int fatigue_ratio = 100 * cur_fatigue / max_fatigue;
    intgame_message_percentage_bar_draw(465, 466, &intgame_message_line_4_rect, fatigue_ratio);

    if (pc_obj == critter_obj || leader_obj == pc_obj) {
        snprintf(buffer, sizeof(buffer), "%d/%d", cur_fatigue, max_fatigue);
    } else {
        snprintf(buffer, sizeof(buffer), "%d%%", fatigue_ratio);
    }
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        buffer,
        &intgame_message_line_4_rect,
        intgame_flare12_blue_font,
        MSG_TEXT_HALIGN_RIGHT);
}

// 0x554560
void intgame_message_window_draw_image(tig_window_handle_t window_handle, int num)
{
    tig_art_id_t art_id;
    TigArtFrameData art_frame_data;
    TigArtBlitInfo blit_info;
    TigRect src_rect;
    TigRect dst_rect;

    if (intgame_is_compact_interface()) {
        window_handle = compact_ui_message_window_acquire();
    }

    tig_art_interface_id_create(num, 0, 0, 0, &art_id);
    tig_art_frame_data(art_id, &art_frame_data);

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = art_frame_data.width;
    src_rect.height = art_frame_data.height;

    dst_rect.x = 217;
    dst_rect.y = 69;
    dst_rect.width = art_frame_data.width;
    dst_rect.height = art_frame_data.height;

    if (intgame_is_compact_interface()) {
        dst_rect.x -= 210;
        dst_rect.y -= 59;
    }

    blit_info.art_id = art_id;
    blit_info.flags = 0;
    blit_info.src_rect = &src_rect;
    blit_info.dst_rect = &dst_rect;
    tig_window_blit_art(window_handle, &blit_info);
}

// 0x554640
void intgame_message_percentage_bar_draw(int background_art_number, int foreground_art_number, TigRect* rect, int value)
{
    tig_window_handle_t window_handle;
    tig_art_id_t art_id;
    TigArtFrameData art_frame_data;
    TigArtBlitInfo art_blit_info;
    TigRect src_rect;
    TigRect dst_rect;

    window_handle = intgame_rotwin_text_frame[intgame_iso_window_type].window_handle;
    if (intgame_is_compact_interface()) {
        window_handle = compact_ui_message_window_acquire();
    }

    if (value < 0) {
        value = 0;
    } else if (value > 100) {
        value = 100;
    }

    if (value != 0) {
        tig_art_interface_id_create(background_art_number, 0, 0, 0, &art_id);
        tig_art_frame_data(art_id, &art_frame_data);

        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.width = value * art_frame_data.width / 100;
        if (src_rect.width < art_frame_data.width) {
            src_rect.width++;
        }
        src_rect.height = art_frame_data.height;

        dst_rect.x = rect->x;
        dst_rect.y = rect->y;
        dst_rect.width = src_rect.width;
        dst_rect.height = art_frame_data.height;

        if (intgame_is_compact_interface()) {
            dst_rect.x -= 210;
            dst_rect.y -= 59;
        }

        art_blit_info.flags = 0;
        art_blit_info.art_id = art_id;
        art_blit_info.src_rect = &src_rect;
        art_blit_info.dst_rect = &dst_rect;

        tig_window_blit_art(window_handle, &art_blit_info);
    }

    if (value != 100) {
        tig_art_interface_id_create(foreground_art_number, 0, 0, 0, &art_id);
        tig_art_frame_data(art_id, &art_frame_data);

        src_rect.x = art_frame_data.width - art_frame_data.width * (100 - value) / 100;
        src_rect.y = 0;
        src_rect.height = art_frame_data.height;
        src_rect.width = art_frame_data.width * (100 - value) / 100;

        dst_rect.x = src_rect.x + rect->x;
        dst_rect.y = rect->y;
        dst_rect.width = src_rect.width;
        dst_rect.height = art_frame_data.height;

        if (intgame_is_compact_interface()) {
            dst_rect.x -= 210;
            dst_rect.y -= 59;
        }

        art_blit_info.flags = 0;
        art_blit_info.art_id = art_id;
        art_blit_info.src_rect = &src_rect;
        art_blit_info.dst_rect = &dst_rect;

        tig_window_blit_art(window_handle, &art_blit_info);
    }
}

// 0x554830
void intgame_weapon_skill_summary_draw(int64_t source_obj, int64_t target_obj)
{
    tig_window_handle_t window_handle;
    int64_t weapon_obj;
    int skill;
    int effectiveness;
    SkillInvocation skill_invocation;
    int aptitude_penalty_percent;
    TigRect rect;
    char str[20];
    int penalty;
    int slot;
    MesFileEntry mes_file_entry;

    window_handle = intgame_rotwin_text_frame[intgame_iso_window_type].window_handle;
    if (intgame_is_compact_interface()) {
        window_handle = compact_ui_message_window_acquire();
    }

    intgame_message_icon_draw(window_handle, 582, 207, 57);

    weapon_obj = item_wield_get(source_obj, ITEM_INV_LOC_WEAPON);
    skill = item_weapon_skill(weapon_obj);
    if (IS_TECH_SKILL(skill)) {
        effectiveness = tech_skill_effectiveness(source_obj, GET_TECH_SKILL(skill), target_obj);
    } else {
        effectiveness = basic_skill_effectiveness(source_obj, GET_BASIC_SKILL(skill), target_obj);
    }

    skill_invocation_init(&skill_invocation);
    object_handle_reference_init(source_obj, &(skill_invocation.source));
    object_handle_reference_init(target_obj, &(skill_invocation.target));
    object_handle_reference_init(weapon_obj, &(skill_invocation.item));
    skill_invocation.skill = skill;

    effectiveness -= skill_invocation_difficulty(&skill_invocation);

    if (weapon_obj != OBJ_HANDLE_NULL
        && skill != SKILL_THROWING) {
        aptitude_penalty_percent = item_target_aptitude_penalty_get(weapon_obj, source_obj, target_obj);
        if (aptitude_penalty_percent > 0) {
            if (aptitude_penalty_percent > 20) {
                skill_invocation.flags |= SKILL_INVOCATION_MAGIC_TECH_PENALTY;
            }
            effectiveness -= effectiveness * aptitude_penalty_percent / 100;
        }
    }

    if (effectiveness < 0 || (skill_invocation.flags & (SKILL_INVOCATION_BLOCKED_SHOT | SKILL_INVOCATION_PENALTY_RANGE)) != 0) {
        effectiveness = 0;
    }

    if ((skill_invocation.flags & SKILL_INVOCATION_PENALTY_MASK) != 0) {
        rect.x = 215;
        rect.y = 66;
        rect.width = 64;
        rect.height = 64;

        snprintf(str, sizeof(str), "%d%%", effectiveness);
        intgame_message_window_write_text(window_handle,
            str,
            &rect,
            intgame_flare14_white_font,
            MSG_TEXT_HALIGN_CENTER);

        slot = 0;
        for (penalty = 0; penalty < INTGAME_PENALTY_COUNT; penalty++) {
            if (slot >= INTGAME_PENALTY_SLOTS) {
                break;
            }

            if ((intgame_penalty_flags[penalty] & skill_invocation.flags) != 0) {
                intgame_message_icon_draw(window_handle,
                    intgame_penalty_icons[penalty],
                    intgame_penalty_slot_x[slot],
                    intgame_penalty_slot_y[slot]);
                slot++;
            }
        }
    } else {
        mes_file_entry.num = 20;
        mes_get_msg(intgame_mes_file, &mes_file_entry);

        rect.x = 215;
        rect.y = 85;
        rect.width = 64;
        rect.height = 64;

        intgame_message_window_write_text(window_handle,
            mes_file_entry.str,
            &rect,
            intgame_flare12_white_font,
            MSG_TEXT_HALIGN_CENTER);

        snprintf(str, sizeof(str), "%d%%", effectiveness);
        rect.y += 18;
        intgame_message_window_write_text(window_handle,
            str,
            &rect,
            intgame_flare14_white_font,
            MSG_TEXT_HALIGN_CENTER);
    }
}

// 0x554B00
void intgame_message_icon_draw(tig_window_handle_t window_handle, int art_num, int x, int y)
{
    tig_art_id_t art_id;
    TigArtFrameData art_frame_data;
    TigArtBlitInfo blit_info;
    TigRect src_rect;
    TigRect dst_rect;

    if (intgame_is_compact_interface()) {
        window_handle = compact_ui_message_window_acquire();
        x -= 210;
        y -= 59;
    }

    tig_art_interface_id_create(art_num, 0, 0, 0, &art_id);
    tig_art_frame_data(art_id, &art_frame_data);

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = art_frame_data.width;
    src_rect.height = art_frame_data.height;

    dst_rect.x = x;
    dst_rect.y = y;
    dst_rect.width = art_frame_data.width;
    dst_rect.height = art_frame_data.height;

    blit_info.art_id = art_id;
    blit_info.flags = 0;
    blit_info.src_rect = &src_rect;
    blit_info.dst_rect = &dst_rect;
    tig_window_blit_art(window_handle, &blit_info);

    compact_ui_message_window_box();
}

// 0x554BE0
tig_art_id_t intgame_item_interface_art_id_get(int64_t obj)
{
    tig_art_id_t art_id = TIG_ART_ID_INVALID;
    int art_num;

    if (obj != OBJ_HANDLE_NULL) {
        art_num = intgame_item_icon_get(obj);
        tig_art_interface_id_create(art_num, 0, 0, 0, &art_id);
    }

    return art_id;
}

// 0x554C20
int intgame_item_icon_get(int64_t item_obj)
{
    int obj_type;
    int complexity;
    int num;
    tig_art_id_t art_id;
    int armor_coverage;

    obj_type = obj_field_int32_get(item_obj, OBJ_F_TYPE);
    complexity = obj_field_int32_get(item_obj, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY);

    if (item_is_identified(item_obj)
        && (obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS) & OIF_HEXED) != 0) {
        num = 440;

        switch (obj_type) {
        case OBJ_TYPE_WEAPON:
            art_id = obj_field_int32_get(item_obj, OBJ_F_ITEM_USE_AID_FRAGMENT);
            switch (tig_art_item_id_subtype_get(art_id)) {
            case TIG_ART_WEAPON_TYPE_BOW:
                num = 792;
                break;
            case TIG_ART_WEAPON_TYPE_DAGGER:
            case TIG_ART_WEAPON_TYPE_SWORD:
            case TIG_ART_WEAPON_TYPE_AXE:
            case TIG_ART_WEAPON_TYPE_MACE:
            case TIG_ART_WEAPON_TYPE_TWO_HANDED_SWORD:
            case TIG_ART_WEAPON_TYPE_STAFF:
                num = 791;
                break;
            }
            break;
        case OBJ_TYPE_ARMOR:
            art_id = obj_field_int32_get(item_obj, OBJ_F_ITEM_INV_AID);
            armor_coverage = tig_art_item_id_armor_coverage_get(art_id);
            switch (armor_coverage) {
            case TIG_ART_ARMOR_COVERAGE_TORSO:
                art_id = obj_field_int32_get(item_obj, OBJ_F_ITEM_USE_AID_FRAGMENT);
                switch (tig_art_item_id_subtype_get(art_id)) {
                case TIG_ART_ARMOR_TYPE_VILLAGER:
                case TIG_ART_ARMOR_TYPE_CITY_DWELLER:
                    num = 794;
                    break;
                case TIG_ART_ARMOR_TYPE_ROBE:
                    num = 797;
                    break;
                default:
                    num = 793;
                    break;
                }
                break;
            case TIG_ART_ARMOR_COVERAGE_RING:
                num = 796;
                break;
            case TIG_ART_ARMOR_COVERAGE_MEDALLION:
                num = 795;
                break;
            default:
                num = 793;
                break;
            }
            break;
        }
        return num;
    }

    switch (obj_type) {
    case OBJ_TYPE_WEAPON:
        art_id = obj_field_int32_get(item_obj, OBJ_F_ITEM_USE_AID_FRAGMENT);
        num = intgame_weapon_icons[tig_art_item_id_subtype_get(art_id)];
        if (complexity > 0) {
            num += 1;
        } else if (complexity < 0) {
            num += 2;
        }
        break;
    case OBJ_TYPE_AMMO:
        num = intgame_ammo_icons[obj_field_int32_get(item_obj, OBJ_F_AMMO_TYPE)];
        break;
    case OBJ_TYPE_ARMOR:
        art_id = obj_field_int32_get(item_obj, OBJ_F_ITEM_INV_AID);
        armor_coverage = tig_art_item_id_armor_coverage_get(art_id);
        switch (armor_coverage) {
        case TIG_ART_ARMOR_COVERAGE_TORSO:
            art_id = obj_field_int32_get(item_obj, OBJ_F_ITEM_USE_AID_FRAGMENT);
            num = intgame_armor_type_icons[tig_art_item_id_subtype_get(art_id)];
            break;
        default:
            num = intgame_armor_coverage_icons[armor_coverage];
            break;
        }
        if (complexity > 0) {
            num += 1;
        } else if (complexity < 0) {
            num += 2;
        }
        break;
    case OBJ_TYPE_GOLD:
        num = 417;
        break;
    case OBJ_TYPE_FOOD:
        num = 423;
        if (complexity > 0) {
            num = 418;
        } else if (complexity < 0) {
            switch (obj_field_int32_get(item_obj, OBJ_F_ITEM_DISCIPLINE)) {
            case TECH_HERBOLOGY:
                num = 420;
                break;
            case TECH_CHEMISTRY:
                num = 421;
                break;
            case TECH_THERAPEUTICS:
                num = 419;
                break;
            }
        }
        break;
    case OBJ_TYPE_SCROLL:
        num = 425;
        break;
    case OBJ_TYPE_KEY:
        num = 426;
        break;
    case OBJ_TYPE_KEY_RING:
        num = 427;
        break;
    case OBJ_TYPE_WRITTEN:
        num = intgame_written_icons[obj_field_int32_get(item_obj, OBJ_F_WRITTEN_SUBTYPE)];
        break;
    case OBJ_TYPE_GENERIC:
        num = 432;
        if (complexity > 0) {
            num = 433;
        } else if (complexity < 0) {
            num = 434;
        }
        break;
    }

    return num;
}

static void intgame_write_comparison(const char* text, const TigRect* rect,
    const ItemCompareRange* ranges, int count, bool right)
{
    if (!intgame_iso_interface_created) return;
    TigRect target = *rect;
    tig_window_handle_t window = intgame_rotwin_text_frame[intgame_iso_window_type].window_handle;
    if (intgame_is_compact_interface()) {
        target.x -= 210;
        target.y -= 59;
        window = compact_ui_message_window_acquire();
    }
    const tig_font_handle_t fonts[4] = {intgame_flare12_white_font, intgame_compare_worse_font,
        intgame_compare_better_font, intgame_compare_equal_font};
    item_compare_write(window, text, target, ranges, count, fonts, right);
}

static void intgame_parse_effect_stats(int64_t item, const char* text, ItemCompareStats* stats)
{
    int type = obj_field_int32_get(item, OBJ_F_TYPE);
    int first = type == OBJ_TYPE_WEAPON ? 43 : 50;
    int last = type == OBJ_TYPE_WEAPON ? 49 : 56;
    for (int id = first; id <= last; id++) {
        MesFileEntry label = {id};
        mes_get_msg(intgame_mes_file, &label);
        item_compare_parse_stat(text, label.str, id, true, stats);
    }
    if (type == OBJ_TYPE_WEAPON) {
        const char* alias = mes_localized_get("item_compare_ui", 0);
        if (alias != NULL) item_compare_parse_stat(text, alias, 43, true, stats);
        alias = mes_localized_get("item_compare_ui", 1);
        if (alias != NULL) item_compare_parse_stat(text, alias, 46, true, stats);
    }
    for (int stat = STAT_STRENGTH; stat <= STAT_CHARISMA; stat++) {
        item_compare_parse_stat(text, stat_short_name(stat), 100 + stat, true, stats);
    }
    if (type == OBJ_TYPE_ARMOR) {
        MesFileEntry label = {8}; // A speed bonus in an armor description.
        mes_get_msg(intgame_mes_file, &label);
        item_compare_parse_stat(text, label.str, 8, true, stats);
        if (item_armor_coverage(item) == TIG_ART_ARMOR_COVERAGE_GAUNTLETS) {
            label.num = 43;
            mes_get_msg(intgame_mes_file, &label);
            item_compare_parse_stat(text, label.str, 43, false, stats);
        }
    }
}

static void intgame_reference_stats(int64_t item, ItemCompareStats* stats)
{
    char buffer[MAX_STRING];
    if (obj_field_int32_get(item, OBJ_F_TYPE) == OBJ_TYPE_WEAPON) {
        format_weapon_stats(item, buffer, sizeof(buffer), stats);
    } else {
        format_armor_stats(item, buffer, sizeof(buffer), stats);
    }
    if (obj_field_int32_get(item, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY) <= 0 || item_is_identified(item)) {
        const char* effects = item_effect_get(obj_field_int32_get(item, OBJ_F_ITEM_DESCRIPTION_EFFECTS));
        if (effects != NULL && *effects != '\0') intgame_parse_effect_stats(item, effects, stats);
    }
}

// 0x554F10
void intgame_examine_item(int64_t pc_obj, int64_t item_obj, char* str)
{
    int obj_type;
    int64_t parent_obj;
    bool is_identified;
    int complexity;
    MesFileEntry mes_file_entry;
    MesFileEntry suffix;
    char buffer[MAX_STRING];
    int quantity_fld;
    int value;
    int64_t comparison_owner = inven_ui_comparison_owner(item_obj);
    int64_t reference = OBJ_HANDLE_NULL;
    ItemCompareStats stats = {0}, reference_stats = {0};
    ItemCompareRange ranges[ITEM_COMPARE_RANGE_CAPACITY];
    int range_count = 0;
    if (comparison_owner != OBJ_HANDLE_NULL) {
        reference = item_compare_reference(comparison_owner, item_obj);
        pc_obj = comparison_owner;
    }

    obj_type = obj_field_int32_get(item_obj, OBJ_F_TYPE);

    intgame_message_window_clear_internal();

    if (comparison_owner == OBJ_HANDLE_NULL && item_parent(item_obj, &parent_obj)
        && parent_obj != OBJ_HANDLE_NULL
        && IS_WEAR_INV_LOC(item_inventory_location_get(item_obj))) {
        pc_obj = parent_obj;
    }

    intgame_message_window_draw_image(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        intgame_item_icon_get(item_obj));

    is_identified = item_is_identified(item_obj);
    complexity = obj_field_int32_get(item_obj, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY);

    if ((obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS) & OIF_HEXED) != 0
        && is_identified) {
        intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
            str,
            &intgame_message_title_rect,
            intgame_morph15_orange_font,
            MSG_TEXT_HALIGN_LEFT);
    } else {
        intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
            str,
            &intgame_message_title_rect,
            complexity > 0 ? intgame_morph15_blue_font : intgame_morph15_white_font,
            MSG_TEXT_HALIGN_LEFT);
    }

    if (obj_type == OBJ_TYPE_WEAPON) {
        value = item_weapon_magic_speed(item_obj, pc_obj);

        mes_file_entry.num = 8; // "Speed"
        mes_get_msg(intgame_mes_file, &mes_file_entry);

        snprintf(buffer, sizeof(buffer),
            "%s: %d",
            mes_file_entry.str,
            value);
        if (reference != OBJ_HANDLE_NULL) {
            int start = (int)strlen(mes_file_entry.str) + 2;
            ItemCompareRange field = {start, (int)strspn(buffer + start, "-0123456789"),
                item_compare_value(value, item_weapon_magic_speed(reference, pc_obj), false)};
            intgame_write_comparison(buffer, &intgame_message_title_rect, &field, 1, true);
        } else {
            intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
                buffer,
                &intgame_message_title_rect,
                intgame_flare12_white_font,
                MSG_TEXT_HALIGN_RIGHT);
        }
    }

    switch (obj_type) {
    case OBJ_TYPE_WEAPON:
        mes_file_entry.num = obj_field_int32_get(item_obj, OBJ_F_ITEM_DESCRIPTION_EFFECTS);
        mes_file_entry.str = item_effect_get(mes_file_entry.num);
        if (mes_file_entry.str != NULL
            && *mes_file_entry.str != '\0'
            && (complexity <= 0 || is_identified)) {
            SDL_strlcpy(buffer, mes_file_entry.str, sizeof(buffer));
            if (reference != OBJ_HANDLE_NULL) {
                intgame_reference_stats(reference, &reference_stats);
                intgame_parse_effect_stats(item_obj, buffer, &stats);
                range_count = item_compare_stats(&stats, &reference_stats, ranges);
            }
        } else {
            if (reference != OBJ_HANDLE_NULL) {
                intgame_reference_stats(reference, &reference_stats);
            }
            format_weapon_stats(item_obj, buffer, sizeof(buffer), &stats);
            if (reference != OBJ_HANDLE_NULL) range_count = item_compare_stats(&stats, &reference_stats, ranges);
        }
        break;
    case OBJ_TYPE_AMMO:
    case OBJ_TYPE_GOLD:
        item_stack_counter_field(item_obj, &quantity_fld);
        value = obj_field_int32_get(item_obj, quantity_fld);

        mes_file_entry.num = 6; // "Quantity"
        mes_get_msg(intgame_mes_file, &mes_file_entry);

        snprintf(buffer, sizeof(buffer),
            "%s: %d",
            mes_file_entry.str,
            value);
        break;
    case OBJ_TYPE_ARMOR:
        mes_file_entry.num = obj_field_int32_get(item_obj, OBJ_F_ITEM_DESCRIPTION_EFFECTS);
        mes_file_entry.str = item_effect_get(mes_file_entry.num);
        if (mes_file_entry.str != NULL
            && *mes_file_entry.str != '\0'
            && (complexity <= 0 || is_identified)) {
            SDL_strlcpy(buffer, mes_file_entry.str, sizeof(buffer));
            if (reference != OBJ_HANDLE_NULL) {
                intgame_reference_stats(reference, &reference_stats);
                intgame_parse_effect_stats(item_obj, buffer, &stats);
                range_count = item_compare_stats(&stats, &reference_stats, ranges);
            }
        } else {
            if (reference != OBJ_HANDLE_NULL) {
                intgame_reference_stats(reference, &reference_stats);
            }
            format_armor_stats(item_obj, buffer, sizeof(buffer), &stats);
            if (reference != OBJ_HANDLE_NULL) range_count = item_compare_stats(&stats, &reference_stats, ranges);
        }
        break;
    case OBJ_TYPE_FOOD:
    case OBJ_TYPE_SCROLL:
    case OBJ_TYPE_WRITTEN:
    case OBJ_TYPE_GENERIC:
        buffer[0] = '\0';
        if (complexity <= 0 || is_identified) {
            mes_file_entry.num = obj_field_int32_get(item_obj, OBJ_F_ITEM_DESCRIPTION_EFFECTS);
            mes_file_entry.str = item_effect_get(mes_file_entry.num);
            if (mes_file_entry.str != NULL) {
                strcpy(buffer, mes_file_entry.str);
            }
        }
        if (obj_type == OBJ_TYPE_GENERIC) {
            unsigned int flags = obj_field_int32_get(item_obj, OBJ_F_GENERIC_FLAGS);
            if ((flags & OGF_IS_LOCKPICK) != 0) {
                mes_file_entry.num = 71; // "Bonus to Pick Locks skill"
            } else if ((flags & OGF_IS_HEALING_ITEM) != 0) {
                mes_file_entry.num = 72; // "Bonus to Heal skill"
            } else {
                mes_file_entry.num = -1;
            }

            if (mes_file_entry.num != -1) {
                mes_get_msg(intgame_mes_file, &mes_file_entry);
                snprintf(buffer, sizeof(buffer),
                    "%s: %+d%%",
                    mes_file_entry.str,
                    obj_field_int32_get(item_obj, OBJ_F_GENERIC_USAGE_BONUS));
            }
        }
        break;
    case OBJ_TYPE_KEY:
        buffer[0] = '\0';
        break;
    case OBJ_TYPE_KEY_RING:
        value = item_get_keys(item_obj, NULL);

        mes_file_entry.num = 7; // "Keys"
        mes_get_msg(intgame_mes_file, &mes_file_entry);

        snprintf(buffer, sizeof(buffer),
            "%s: %d",
            mes_file_entry.str,
            value);
        break;
    }

    if (buffer[0] != '\0' || !intgame_is_compact_interface()) {
        if (range_count > 0) {
            intgame_write_comparison(buffer, &intgame_message_line_2_rect, ranges, range_count, false);
        } else {
            intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
                buffer,
                &intgame_message_line_2_rect,
                intgame_flare12_white_font,
                MSG_TEXT_HALIGN_LEFT);
        }
    }

    if (is_identified
        && (obj_type == OBJ_TYPE_WEAPON
            || obj_type == OBJ_TYPE_ARMOR
            || obj_type == OBJ_TYPE_SCROLL)) {
        if (complexity > 0) {
            value = item_effective_power(item_obj, pc_obj);

            mes_file_entry.num = 2; // "Magic power available"
            mes_get_msg(intgame_mes_file, &mes_file_entry);

            snprintf(buffer, sizeof(buffer),
                "%s: %d%%",
                mes_file_entry.str,
                100 * value / complexity);
            if (reference != OBJ_HANDLE_NULL && item_is_identified(reference)
                && obj_field_int32_get(reference, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY) > 0) {
                int start = (int)strlen(mes_file_entry.str) + 2;
                ItemCompareRange field = {start, (int)strspn(buffer + start, "+-0123456789"),
                    item_compare_value(100 * value / complexity, 100 * item_effective_power(reference, pc_obj) / obj_field_int32_get(reference, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY), false)};
                intgame_write_comparison(buffer, &intgame_message_line_3_rect, &field, 1, false);
            } else {
                intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
                    buffer,
                    &intgame_message_line_3_rect,
                    intgame_flare12_white_font,
                    MSG_TEXT_HALIGN_LEFT);
            }
        } else if (complexity < 0) {
            value = item_aptitude_crit_failure_chance(item_obj, pc_obj);

            mes_file_entry.num = 3; // "Aptitude adj to chance of critical failure"
            mes_get_msg(intgame_mes_file, &mes_file_entry);

            snprintf(buffer, sizeof(buffer),
                "%s: %+d%%",
                mes_file_entry.str,
                value);
            if (reference != OBJ_HANDLE_NULL && item_is_identified(reference)
                && obj_field_int32_get(reference, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY) < 0) {
                int start = (int)strlen(mes_file_entry.str) + 2;
                ItemCompareRange field = {start, (int)strspn(buffer + start, "+-0123456789"),
                    item_compare_value(value, item_aptitude_crit_failure_chance(reference, pc_obj), true)};
                intgame_write_comparison(buffer, &intgame_message_line_3_rect, &field, 1, false);
            } else {
                intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
                    buffer,
                    &intgame_message_line_3_rect,
                    intgame_flare12_white_font,
                    MSG_TEXT_HALIGN_LEFT);
            }
        }
    }

    buffer[0] = '\0';
    value = obj_field_int32_get(item_obj, OBJ_F_ITEM_SPELL_MANA_STORE);

    if (complexity > 0 && value != 0) {
        mes_file_entry.num = 19; // "Uses"
        mes_get_msg(intgame_mes_file, &mes_file_entry);

        if (is_identified) {
            if (value > 0) {
                snprintf(buffer, sizeof(buffer), "%s: +", mes_file_entry.str);
            } else {
                snprintf(buffer, sizeof(buffer), "%s: %d", mes_file_entry.str, value);
            }
        } else {
            snprintf(buffer, sizeof(buffer), "%s: ??", mes_file_entry.str);
        }
    } else {
        switch (obj_type) {
        case OBJ_TYPE_GENERIC:
            if ((obj_field_int32_get(item_obj, OBJ_F_GENERIC_FLAGS) & OGF_IS_HEALING_ITEM) != 0) {
                mes_file_entry.num = 19; // "Uses"
                mes_get_msg(intgame_mes_file, &mes_file_entry);

                snprintf(buffer, sizeof(buffer),
                    "%s: %d",
                    mes_file_entry.str,
                    obj_field_int32_get(item_obj, OBJ_F_GENERIC_USAGE_BONUS));
            }
            break;
        case OBJ_TYPE_WEAPON: {
            int ammo_type = item_weapon_ammo_type(item_obj);
            if (ammo_type != 10000) {
                int consumption = obj_field_int32_get(item_obj, OBJ_F_WEAPON_AMMO_CONSUMPTION);
                if (consumption > 0) {
                    snprintf(buffer, sizeof(buffer),
                        "%s: %d",
                        ammunition_type_get_name(ammo_type),
                        consumption);
                }
            }
            break;
        }
        }
    }

    if (buffer[0] != '\0' || !intgame_is_compact_interface()) {
        intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
            buffer,
            &intgame_message_line_3_rect,
            intgame_flare12_white_font,
            MSG_TEXT_HALIGN_RIGHT);
    }

    value = item_weight(item_obj, pc_obj);
    mes_file_entry.num = 4; // "Weight"
    mes_get_msg(intgame_mes_file, &mes_file_entry);
    suffix.num = 5; // "stone"
    mes_get_msg(intgame_mes_file, &suffix);
    snprintf(buffer, sizeof(buffer),
        "%s: %d %s",
        mes_file_entry.str,
        value,
        suffix.str);
    if (reference != OBJ_HANDLE_NULL) {
        int start = (int)strlen(mes_file_entry.str) + 2;
        ItemCompareRange field = {start, (int)strspn(buffer + start, "-0123456789"),
            item_compare_value(value, item_weight(reference, pc_obj), true)};
        intgame_write_comparison(buffer, &intgame_message_line_4_rect, &field, 1, false);
    } else {
        intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
            buffer,
            &intgame_message_line_4_rect,
            intgame_flare12_white_font,
            MSG_TEXT_HALIGN_LEFT);
    }

    if (tig_art_item_id_destroyed_get(obj_field_int32_get(item_obj, OBJ_F_CURRENT_AID)) == 0) {
        if (obj_type == OBJ_TYPE_WEAPON
            || obj_type == OBJ_TYPE_ARMOR
            || (obj_type == OBJ_TYPE_GENERIC
                && (obj_field_int32_get(item_obj, OBJ_F_GENERIC_FLAGS) & 0x20) != 0)) {
            snprintf(buffer, sizeof(buffer),
                "%d/%d",
                object_hp_current(item_obj),
                object_hp_max(item_obj));
            if (reference != OBJ_HANDLE_NULL) {
                int first = (int)(strchr(buffer, '/') - buffer);
                ItemCompareRange health[2] = {
                    {0, first, item_compare_value(object_hp_current(item_obj), object_hp_current(reference), false)},
                    {first + 1, (int)strlen(buffer) - first - 1, item_compare_value(object_hp_max(item_obj), object_hp_max(reference), false)},
                };
                intgame_write_comparison(buffer, &intgame_message_line_4_rect, health, 2, true);
            } else {
                intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
                    buffer,
                    &intgame_message_line_4_rect,
                    intgame_flare12_white_font,
                    MSG_TEXT_HALIGN_RIGHT);
            }
        }
    }
}


// 0x555780
void append_stat(char* buffer, size_t maxlen, int num, int min, int max, int adj, bool is_modifier, ItemCompareStats* stats)
{
    MesFileEntry mes_file_entry;
    char tmp[80];

    int start;

    if (min == 0 && max == 0 && adj == 0) {
        return;
    }

    mes_file_entry.num = num;
    mes_get_msg(intgame_mes_file, &mes_file_entry);

    SDL_strlcat(buffer, mes_file_entry.str, maxlen);
    SDL_strlcat(buffer, ":", maxlen);

    start = (int)strlen(buffer);

    if (max != 0) {
        snprintf(tmp, sizeof(tmp), "%d-%d", min, max);
        SDL_strlcat(buffer, tmp, maxlen);
    } else if (min != 0) {
        if (is_modifier) {
            snprintf(tmp, sizeof(tmp), "%+d", min);
        } else {
            snprintf(tmp, sizeof(tmp), "%d", min);
        }
        SDL_strlcat(buffer, tmp, maxlen);
    }

    if (adj != 0) {
        snprintf(tmp, sizeof(tmp), "(%+d)", adj);
        SDL_strlcat(buffer, tmp, maxlen);
    }

    if (stats != NULL && stats->count < ITEM_COMPARE_STAT_CAPACITY) {
        stats->entries[stats->count++] = (ItemCompareStat){num, min, max, adj, start, (int)strlen(buffer) - start, is_modifier};
    }
    SDL_strlcat(buffer, "  ", maxlen);
}

// 0x555910
void format_weapon_stats(int64_t weapon_obj, char* buffer, size_t maxlen, ItemCompareStats* stats)
{
    bool identified;
    int min;
    int max;
    int adj;

    identified = obj_field_int32_get(weapon_obj, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY) > 0
        && item_is_identified(weapon_obj);
    buffer[0] = '\0';

    // D
    min = obj_arrayfield_int32_get(weapon_obj, OBJ_F_WEAPON_DAMAGE_LOWER_IDX, 0);
    max = obj_arrayfield_int32_get(weapon_obj, OBJ_F_WEAPON_DAMAGE_UPPER_IDX, 0);
    if (identified) {
        adj = obj_arrayfield_int32_get(weapon_obj, OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, 0);
    } else {
        adj = 0;
    }
    append_stat(buffer, maxlen, 43, min, max, adj, false, stats);

    // FT
    min = obj_arrayfield_int32_get(weapon_obj, OBJ_F_WEAPON_DAMAGE_LOWER_IDX, 4);
    max = obj_arrayfield_int32_get(weapon_obj, OBJ_F_WEAPON_DAMAGE_UPPER_IDX, 4);
    if (identified) {
        adj = obj_arrayfield_int32_get(weapon_obj, OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, 4);
    } else {
        adj = 0;
    }
    append_stat(buffer, maxlen, 44, min, max, adj, false, stats);

    // TH
    min = obj_field_int32_get(weapon_obj, OBJ_F_WEAPON_BONUS_TO_HIT);
    if (identified) {
        adj = obj_field_int32_get(weapon_obj, OBJ_F_WEAPON_MAGIC_HIT_ADJ);
    } else {
        adj = 0;
    }
    append_stat(buffer, maxlen, 45, min, 0, adj, true, stats);

    // RNG
    min = obj_field_int32_get(weapon_obj, OBJ_F_WEAPON_RANGE);
    if (min == 1) {
        min = 0;
    }
    if (identified) {
        adj = obj_field_int32_get(weapon_obj, OBJ_F_WEAPON_MAGIC_RANGE_ADJ);
    } else {
        adj = 0;
    }
    append_stat(buffer, maxlen, 46, min, 0, adj, false, stats);

    // PD
    min = obj_arrayfield_int32_get(weapon_obj, OBJ_F_WEAPON_DAMAGE_LOWER_IDX, 1);
    max = obj_arrayfield_int32_get(weapon_obj, OBJ_F_WEAPON_DAMAGE_UPPER_IDX, 1);
    if (identified) {
        adj = obj_arrayfield_int32_get(weapon_obj, OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, 1);
    } else {
        adj = 0;
    }
    append_stat(buffer, maxlen, 47, min, max, adj, false, stats);

    // FD
    min = obj_arrayfield_int32_get(weapon_obj, OBJ_F_WEAPON_DAMAGE_LOWER_IDX, 3);
    max = obj_arrayfield_int32_get(weapon_obj, OBJ_F_WEAPON_DAMAGE_UPPER_IDX, 3);
    if (identified) {
        adj = obj_arrayfield_int32_get(weapon_obj, OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, 3);
    } else {
        adj = 0;
    }
    append_stat(buffer, maxlen, 48, min, max, adj, false, stats);

    // ED
    min = obj_arrayfield_int32_get(weapon_obj, OBJ_F_WEAPON_DAMAGE_LOWER_IDX, 2);
    max = obj_arrayfield_int32_get(weapon_obj, OBJ_F_WEAPON_DAMAGE_UPPER_IDX, 2);
    if (identified) {
        adj = obj_arrayfield_int32_get(weapon_obj, OBJ_F_WEAPON_MAGIC_DAMAGE_ADJ_IDX, 2);
    } else {
        adj = 0;
    }
    append_stat(buffer, maxlen, 49, min, max, adj, false, stats);
}

// 0x555B50
void format_armor_stats(int64_t armor_obj, char* buffer, size_t maxlen, ItemCompareStats* stats)
{
    bool identified;
    int value;
    int adj;

    identified = obj_field_int32_get(armor_obj, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY) > 0
        && item_is_identified(armor_obj);
    buffer[0] = '\0';

    // AC
    value = obj_field_int32_get(armor_obj, OBJ_F_ARMOR_AC_ADJ);
    if (identified) {
        adj = obj_field_int32_get(armor_obj, OBJ_F_ARMOR_MAGIC_AC_ADJ);
    } else {
        adj = 0;
    }
    append_stat(buffer, maxlen, 50, value, 0, adj, false, stats);

    // DR
    value = obj_arrayfield_int32_get(armor_obj, OBJ_F_ARMOR_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_NORMAL);
    if (identified) {
        adj = obj_arrayfield_int32_get(armor_obj, OBJ_F_ARMOR_MAGIC_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_NORMAL);
    } else {
        adj = 0;
    }
    append_stat(buffer, maxlen, 51, value, 0, adj, true, stats);

    // PR
    value = obj_arrayfield_int32_get(armor_obj, OBJ_F_ARMOR_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_POISON);
    if (identified) {
        adj = obj_arrayfield_int32_get(armor_obj, OBJ_F_ARMOR_MAGIC_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_POISON);
    } else {
        adj = 0;
    }
    append_stat(buffer, maxlen, 52, value, 0, adj, true, stats);

    // FR
    value = obj_arrayfield_int32_get(armor_obj, OBJ_F_ARMOR_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_FIRE);
    if (identified) {
        adj = obj_arrayfield_int32_get(armor_obj, OBJ_F_ARMOR_MAGIC_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_FIRE);
    } else {
        adj = 0;
    }
    append_stat(buffer, maxlen, 53, value, 0, adj, true, stats);

    // ER
    value = obj_arrayfield_int32_get(armor_obj, OBJ_F_ARMOR_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_ELECTRICAL);
    if (identified) {
        adj = obj_arrayfield_int32_get(armor_obj, OBJ_F_ARMOR_MAGIC_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_ELECTRICAL);
    } else {
        adj = 0;
    }
    append_stat(buffer, maxlen, 54, value, 0, adj, true, stats);

    // MR
    value = obj_arrayfield_int32_get(armor_obj, OBJ_F_ARMOR_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_MAGIC);
    if (identified) {
        adj = obj_arrayfield_int32_get(armor_obj, OBJ_F_ARMOR_MAGIC_RESISTANCE_ADJ_IDX, RESISTANCE_TYPE_MAGIC);
    } else {
        adj = 0;
    }
    append_stat(buffer, maxlen, 55, value, 0, adj, true, stats);

    // NP
    value = obj_field_int32_get(armor_obj, OBJ_F_ARMOR_SILENT_MOVE_ADJ);
    if (identified) {
        adj = obj_field_int32_get(armor_obj, OBJ_F_ARMOR_MAGIC_SILENT_MOVE_ADJ);
    } else {
        adj = 0;
    }
    append_stat(buffer, maxlen, 56, value, 0, adj, true, stats);

    // D
    if (item_armor_coverage(armor_obj) == TIG_ART_ARMOR_COVERAGE_GAUNTLETS) {
        value = obj_field_int32_get(armor_obj, OBJ_F_ARMOR_UNARMED_BONUS_DAMAGE);
    } else {
        value = 0;
    }
    append_stat(buffer, maxlen, 43, value, 0, 0, true, stats);
}

// 0x555D80
void intgame_examine_scenery(int64_t pc_obj, int64_t scenery_obj, char* str)
{
    int portrait;
    char buffer[2000];

    if (str[0] == '\0') {
        return;
    }

    intgame_message_window_clear_internal();

    if (intgame_examine_portrait(pc_obj, scenery_obj, &portrait)) {
        intgame_draw_portrait(scenery_obj, portrait, intgame_rotwin_text_frame[intgame_iso_window_type].window_handle, 217, 69);
    } else {
        intgame_message_window_draw_image(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle, portrait);
    }

    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        str,
        &intgame_message_scenery_text_rect,
        intgame_morph15_white_font,
        MSG_TEXT_HALIGN_LEFT);

    if ((obj_field_int32_get(scenery_obj, OBJ_F_FLAGS) & OF_INVULNERABLE) == 0) {
        snprintf(buffer, sizeof(buffer),
            "%d/%d",
            object_hp_current(scenery_obj),
            object_hp_max(scenery_obj));
        intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
            buffer,
            &intgame_message_line_4_rect,
            intgame_flare12_red_font,
            MSG_TEXT_HALIGN_RIGHT);
    }

    if ((obj_field_int32_get(scenery_obj, OBJ_F_SCENERY_FLAGS) & OSCF_MARKS_TOWNMAP) != 0) {
        wmap_ui_mark_townmap(scenery_obj);
    }
}

// 0x555EC0
void intgame_examine_portal(int64_t pc_obj, int64_t portal_obj, char* str)
{
    int portrait;
    ObjectPortalFlags portal_flags;
    MesFileEntry mes_file_entry;
    char buffer[MAX_STRING];

    intgame_message_window_clear_internal();

    if (intgame_examine_portrait(pc_obj, portal_obj, &portrait)) {
        intgame_draw_portrait(portal_obj, portrait, intgame_rotwin_text_frame[intgame_iso_window_type].window_handle, 217, 69);
    } else {
        intgame_message_window_draw_image(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle, portrait);
    }

    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        str,
        &intgame_message_title_rect,
        intgame_morph15_white_font,
        MSG_TEXT_HALIGN_LEFT);

    portal_flags = obj_field_int32_get(portal_obj, OBJ_F_PORTAL_FLAGS);
    if ((portal_flags & OPF_JAMMED) != 0) {
        mes_file_entry.num = 26; // "Jammed"
    } else if ((portal_flags & OPF_MAGICALLY_HELD) != 0) {
        mes_file_entry.num = 27; // "Magically held"
    } else if (object_locked_get(portal_obj)) {
        mes_file_entry.num = 9; // "Locked"
    } else {
        mes_file_entry.num = 10; // "Unlocked"
    }
    mes_get_msg(intgame_mes_file, &mes_file_entry);
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        mes_file_entry.str,
        &intgame_message_line_4_rect,
        intgame_flare12_red_font,
        MSG_TEXT_HALIGN_LEFT);

    snprintf(buffer, sizeof(buffer),
        "%d/%d",
        object_hp_current(portal_obj),
        object_hp_max(portal_obj));
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        buffer,
        &intgame_message_line_4_rect,
        intgame_flare12_red_font,
        MSG_TEXT_HALIGN_RIGHT);
}

// 0x556040
void intgame_examine_container(int64_t pc_obj, int64_t container_obj, char* str)
{
    int portrait;
    unsigned int container_flags;
    MesFileEntry mes_file_entry;
    char buffer[MAX_STRING];

    intgame_message_window_clear_internal();

    if (intgame_examine_portrait(pc_obj, container_obj, &portrait)) {
        intgame_draw_portrait(container_obj, portrait, intgame_rotwin_text_frame[intgame_iso_window_type].window_handle, 217, 69);
    } else {
        intgame_message_window_draw_image(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle, portrait);
    }

    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        str,
        &intgame_message_title_rect,
        intgame_morph15_white_font,
        MSG_TEXT_HALIGN_LEFT);

    container_flags = obj_field_int32_get(container_obj, OBJ_F_CONTAINER_FLAGS);
    if ((container_flags & OCOF_JAMMED)) {
        mes_file_entry.num = 26; // "Jammed"
    } else if ((container_flags & OCOF_MAGICALLY_HELD) != 0) {
        mes_file_entry.num = 27; // "Magically held"
    } else if (object_locked_get(container_obj)) {
        mes_file_entry.num = 9; // "Locked"
    } else {
        mes_file_entry.num = 10; // "Unlocked"
    }
    mes_get_msg(intgame_mes_file, &mes_file_entry);
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        mes_file_entry.str,
        &intgame_message_line_4_rect,
        intgame_flare12_red_font,
        MSG_TEXT_HALIGN_LEFT);

    if ((obj_field_int32_get(container_obj, OBJ_F_FLAGS) & OF_INVULNERABLE) == 0) {
        snprintf(buffer, sizeof(buffer),
            "%d/%d",
            object_hp_current(container_obj),
            object_hp_max(container_obj));
        intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
            buffer,
            &intgame_message_line_4_rect,
            intgame_flare12_red_font,
            MSG_TEXT_HALIGN_RIGHT);
    }
}

// 0x5561D0
void intgame_draw_portrait(int64_t obj, int portrait, tig_window_handle_t window_handle, int x, int y)
{
    if (intgame_is_compact_interface()) {
        window_handle = compact_ui_message_window_acquire();
        x -= 211;
        y -= 59;
    }

    portrait_draw_native(obj, portrait, window_handle, x, y);
}

// 0x556220
void intgame_message_window_display_attack(int64_t obj)
{
    MesFileEntry mes_file_entry;
    char str[MAX_STRING];
    int64_t weapon_obj;
    int skill;
    int effectiveness;
    int min_dam;
    int max_dam;

    mes_file_entry.num = 57; // "Total Attack"
    mes_get_msg(intgame_mes_file, &mes_file_entry);

    snprintf(str, sizeof(str),
        "%s: %d",
        mes_file_entry.str,
        item_total_attack(obj));

    if (intgame_iso_window_type != ROTWIN_TYPE_MSG) {
        intgame_message_window_display_str(-1, str);
        return;
    }

    intgame_message_window_clear_internal();

    intgame_message_window_draw_image(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle, 675);
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        str,
        &intgame_message_title_rect,
        intgame_morph15_white_font,
        MSG_TEXT_HALIGN_LEFT);

    weapon_obj = item_wield_get(obj, ITEM_INV_LOC_WEAPON);
    skill = item_weapon_skill(weapon_obj);

    mes_file_entry.num = 69; // "Base To Hit"
    mes_get_msg(intgame_mes_file, &mes_file_entry);

    if (IS_TECH_SKILL(skill)) {
        effectiveness = tech_skill_effectiveness(obj, GET_TECH_SKILL(skill), OBJ_HANDLE_NULL);
    } else {
        effectiveness = basic_skill_effectiveness(obj, GET_BASIC_SKILL(skill), OBJ_HANDLE_NULL);
    }

    snprintf(str, sizeof(str), "%s: %d%%", mes_file_entry.str, effectiveness);
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        str,
        &intgame_message_line_2_rect,
        intgame_flare12_white_font,
        MSG_TEXT_HALIGN_LEFT);

    mes_file_entry.num = 59; // "Fatigue"
    mes_get_msg(intgame_mes_file, &mes_file_entry);

    item_weapon_damage(weapon_obj, obj, DAMAGE_TYPE_FATIGUE, skill, 0, &min_dam, &max_dam);
    if (max_dam != 0) {
        snprintf(str, sizeof(str),
            "%s: %d-%d",
            mes_file_entry.str,
            min_dam,
            max_dam);
    } else {
        snprintf(str, sizeof(str), "%s: 0", mes_file_entry.str);
    }
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        str,
        &intgame_message_line_3_rect,
        intgame_flare12_white_font,
        MSG_TEXT_HALIGN_LEFT);

    mes_file_entry.num = 61; // "Fire Damage"
    mes_get_msg(intgame_mes_file, &mes_file_entry);

    item_weapon_damage(weapon_obj, obj, DAMAGE_TYPE_FIRE, skill, 0, &min_dam, &max_dam);
    if (max_dam != 0) {
        snprintf(str, sizeof(str),
            "%s: %d-%d",
            mes_file_entry.str,
            min_dam,
            max_dam);
    } else {
        snprintf(str, sizeof(str), "%s: 0", mes_file_entry.str);
    }
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        str,
        &intgame_message_line_4_rect,
        intgame_flare12_white_font,
        MSG_TEXT_HALIGN_LEFT);

    mes_file_entry.num = 58; // "Damage"
    mes_get_msg(intgame_mes_file, &mes_file_entry);

    item_weapon_damage(weapon_obj, obj, DAMAGE_TYPE_NORMAL, skill, 0, &min_dam, &max_dam);
    if (max_dam != 0) {
        snprintf(str, sizeof(str),
            "%s: %d-%d",
            mes_file_entry.str,
            min_dam,
            max_dam);
    } else {
        snprintf(str, sizeof(str), "%s: 0", mes_file_entry.str);
    }
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        str,
        &intgame_message_line_2_rect,
        intgame_flare12_white_font,
        MSG_TEXT_HALIGN_LEFT | MSG_TEXT_SECONDARY);

    mes_file_entry.num = 60; // "Electrical Damage"
    mes_get_msg(intgame_mes_file, &mes_file_entry);

    item_weapon_damage(weapon_obj, obj, DAMAGE_TYPE_ELECTRICAL, skill, 0, &min_dam, &max_dam);
    if (max_dam != 0) {
        snprintf(str, sizeof(str),
            "%s: %d-%d",
            mes_file_entry.str,
            min_dam,
            max_dam);
    } else {
        snprintf(str, sizeof(str), "%s: 0", mes_file_entry.str);
    }
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        str,
        &intgame_message_line_3_rect,
        intgame_flare12_white_font,
        MSG_TEXT_HALIGN_LEFT | MSG_TEXT_SECONDARY);

    mes_file_entry.num = 62; // "Poison"
    mes_get_msg(intgame_mes_file, &mes_file_entry);

    item_weapon_damage(weapon_obj, obj, DAMAGE_TYPE_POISON, skill, 0, &min_dam, &max_dam);
    if (max_dam != 0) {
        snprintf(str, sizeof(str),
            "%s: %d-%d",
            mes_file_entry.str,
            min_dam,
            max_dam);
    } else {
        snprintf(str, sizeof(str), "%s: 0", mes_file_entry.str);
    }
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        str,
        &intgame_message_line_4_rect,
        intgame_flare12_white_font,
        MSG_TEXT_HALIGN_LEFT | MSG_TEXT_SECONDARY);
}

// 0x5566B0
void intgame_message_window_display_defense(int64_t obj)
{
    MesFileEntry mes_file_entry;
    char buffer[MAX_STRING];
    int value;

    mes_file_entry.num = 63; // "Total Defense"
    mes_get_msg(intgame_mes_file, &mes_file_entry);
    value = item_total_defence(obj);
    snprintf(buffer, sizeof(buffer),
        "%s: %d",
        mes_file_entry.str,
        value);

    if (intgame_iso_window_type != ROTWIN_TYPE_MSG) {
        intgame_message_window_display_str(-1, buffer);
        return;
    }

    intgame_message_window_clear_internal();

    intgame_message_window_draw_image(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle, 674);
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        buffer,
        &intgame_message_title_rect,
        intgame_morph15_white_font,
        MSG_TEXT_HALIGN_LEFT);

    mes_file_entry.num = 70; // "Total AC"
    mes_get_msg(intgame_mes_file, &mes_file_entry);
    value = object_get_ac(obj, 1);
    if (value != 0) {
        snprintf(buffer, sizeof(buffer),
            "%s: %d",
            mes_file_entry.str,
            value);
    } else {
        snprintf(buffer, sizeof(buffer),
            "%s: 0",
            mes_file_entry.str);
    }
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        buffer,
        &intgame_message_line_2_rect,
        intgame_flare12_white_font,
        MSG_TEXT_HALIGN_LEFT);

    mes_file_entry.num = 65; // "Magic Resistance"
    mes_get_msg(intgame_mes_file, &mes_file_entry);
    value = object_get_resistance(obj, RESISTANCE_TYPE_MAGIC, true);
    if (value != 0) {
        snprintf(buffer, sizeof(buffer),
            "%s: %d%%",
            mes_file_entry.str,
            value);
    } else {
        snprintf(buffer, sizeof(buffer),
            "%s: 0",
            mes_file_entry.str);
    }
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        buffer,
        &intgame_message_line_3_rect,
        intgame_flare12_white_font,
        MSG_TEXT_HALIGN_LEFT);

    mes_file_entry.num = 67; // "Fire Resistance"
    mes_get_msg(intgame_mes_file, &mes_file_entry);
    value = object_get_resistance(obj, RESISTANCE_TYPE_FIRE, true);
    if (value != 0) {
        snprintf(buffer, sizeof(buffer),
            "%s: %d%%",
            mes_file_entry.str,
            value);
    } else {
        snprintf(buffer, sizeof(buffer),
            "%s: 0",
            mes_file_entry.str);
    }
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        buffer,
        &intgame_message_line_4_rect,
        intgame_flare12_white_font,
        MSG_TEXT_HALIGN_LEFT);

    mes_file_entry.num = 64; // "Damage Resistance"
    mes_get_msg(intgame_mes_file, &mes_file_entry);
    value = object_get_resistance(obj, RESISTANCE_TYPE_NORMAL, true);
    if (value != 0) {
        snprintf(buffer, sizeof(buffer),
            "%s: %d%%",
            mes_file_entry.str,
            value);
    } else {
        snprintf(buffer, sizeof(buffer),
            "%s: 0",
            mes_file_entry.str);
    }
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        buffer,
        &intgame_message_line_2_rect,
        intgame_flare12_white_font,
        MSG_TEXT_HALIGN_LEFT | MSG_TEXT_SECONDARY);

    mes_file_entry.num = 66; // "Electrical Resistance"
    mes_get_msg(intgame_mes_file, &mes_file_entry);
    value = object_get_resistance(obj, RESISTANCE_TYPE_ELECTRICAL, true);
    if (value != 0) {
        snprintf(buffer, sizeof(buffer),
            "%s: %d%%",
            mes_file_entry.str,
            value);
    } else {
        snprintf(buffer, sizeof(buffer),
            "%s: 0",
            mes_file_entry.str);
    }
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        buffer,
        &intgame_message_line_3_rect,
        intgame_flare12_white_font,
        MSG_TEXT_HALIGN_LEFT | MSG_TEXT_SECONDARY);

    mes_file_entry.num = 68; // "Poison Resistance"
    mes_get_msg(intgame_mes_file, &mes_file_entry);
    value = object_get_resistance(obj, RESISTANCE_TYPE_POISON, true);
    if (value != 0) {
        snprintf(buffer, sizeof(buffer),
            "%s: %d%%",
            mes_file_entry.str,
            value);
    } else {
        snprintf(buffer, sizeof(buffer),
            "%s: 0",
            mes_file_entry.str);
    }
    intgame_message_window_write_text(intgame_rotwin_text_frame[intgame_iso_window_type].window_handle,
        buffer,
        &intgame_message_line_4_rect,
        intgame_flare12_white_font,
        MSG_TEXT_HALIGN_LEFT | MSG_TEXT_SECONDARY);
}

// 0x556A90
void intgame_toggle_primary_button(UiPrimaryButton btn, bool on)
{
    if (on) {
        switch (btn) {
        case UI_PRIMARY_BUTTON_CHAR:
            if (charedit_is_created()) {
                return;
            }
            break;
        case UI_PRIMARY_BUTTON_LOGBOOK:
            if (logbook_ui_is_created()) {
                return;
            }
            break;
        case UI_PRIMARY_BUTTON_TOWNMAP:
        case UI_PRIMARY_BUTTON_WORLDMAP:
            if (wmap_ui_is_created()) {
                return;
            }
            break;
        case UI_PRIMARY_BUTTON_INVENTORY:
            if (inven_ui_is_created()) {
                return;
            }
            break;
        case UI_PRIMARY_BUTTON_COUNT:
            // Should be unreachable.
            break;
        }

        intgame_ui_primary_button_icons[btn] = intgame_ui_primary_button_highlighted_icons[btn];
        intgame_refresh_primary_button(btn);
    } else {
        intgame_ui_primary_button_icons[btn] = intgame_ui_primary_button_normal_icons[btn];
        intgame_refresh_primary_button(btn);
    }
}

// 0x556B70
void intgame_set_map_button(UiPrimaryButton btn)
{
    intgame_map_button = btn;
    intgame_refresh_primary_button(btn);
}

// 0x556B90
void intgame_refresh_primary_button(UiPrimaryButton btn)
{
    tig_button_handle_t button_handle;
    tig_art_id_t art_id;

    switch (btn) {
    case UI_PRIMARY_BUTTON_CHAR:
        button_handle = intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_CHAR].button_handle;
        break;
    case UI_PRIMARY_BUTTON_LOGBOOK:
        button_handle = intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_LOGBOOK].button_handle;
        break;
    case UI_PRIMARY_BUTTON_TOWNMAP:
        if (intgame_map_button != UI_PRIMARY_BUTTON_TOWNMAP) {
            return;
        }

        button_handle = intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_MAP].button_handle;
        break;
    case UI_PRIMARY_BUTTON_WORLDMAP:
        if (intgame_map_button != UI_PRIMARY_BUTTON_WORLDMAP) {
            return;
        }

        button_handle = intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_MAP].button_handle;
        break;
    case UI_PRIMARY_BUTTON_INVENTORY:
        button_handle = intgame_primary_buttons[INTGAME_PRIMARY_BUTTON_INVENTORY].button_handle;
        break;
    case UI_PRIMARY_BUTTON_COUNT:
        // Should be unreachable.
        break;
    }

    tig_art_interface_id_create(intgame_ui_primary_button_icons[btn], 0, 0, 0, &art_id);
    tig_button_set_art(button_handle, art_id);
}

// 0x556C20
void intgame_refresh_experience_gauges(int64_t obj)
{
    TigArtFrameData art_frame_data;
    TigArtBlitInfo art_blit_info;
    TigRect src_rect;
    TigRect dst_rect;
    tig_art_id_t art_id;
    int cnt;
    int index;

    if (tig_art_interface_id_create(184, 0, 0, 0, &art_id) == TIG_OK) {
        src_rect.x = 211;
        src_rect.y = 37;
        src_rect.width = 384;
        src_rect.height = 14;

        art_blit_info.flags = 0;
        art_blit_info.art_id = art_id;
        art_blit_info.src_rect = &src_rect;
        art_blit_info.dst_rect = &src_rect;
        tig_window_blit_art(intgame_interface_windows[1], &art_blit_info);
    }

    cnt = 11 * level_progress_towards_next_level(obj) / 10;
    for (index = 0; index < cnt / 100; index++) {
        if (tig_art_interface_id_create(intgame_experience_gauge_buttons[index].art_num, 0, 0, 0, &art_id) == TIG_OK
            && tig_art_frame_data(art_id, &art_frame_data) == TIG_OK) {
            src_rect.x = 0;
            src_rect.y = 0;
            src_rect.width = art_frame_data.width;
            src_rect.height = art_frame_data.height;

            dst_rect.x = intgame_experience_gauge_buttons[index].x;
            dst_rect.y = intgame_experience_gauge_buttons[index].y;
            dst_rect.width = src_rect.width;
            dst_rect.height = src_rect.height;

            art_blit_info.flags = 0;
            art_blit_info.art_id = art_id;
            art_blit_info.src_rect = &src_rect;
            art_blit_info.dst_rect = &dst_rect;
            tig_window_blit_art(intgame_interface_windows[1], &art_blit_info);
        }
    }

    if (cnt % 100 != 0) {
        if (tig_art_interface_id_create(intgame_experience_gauge_frame.art_num, 0, 0, 0, &art_id) == TIG_OK
            && tig_art_frame_data(art_id, &art_frame_data) == TIG_OK) {
            src_rect.x = 0;
            src_rect.y = 0;
            src_rect.width = art_frame_data.width * (cnt % 100) / 100;
            src_rect.height = art_frame_data.height;

            dst_rect.x = intgame_experience_gauge_frame.x;
            dst_rect.y = intgame_experience_gauge_frame.y;
            dst_rect.width = src_rect.width;
            dst_rect.height = src_rect.height;

            art_blit_info.flags = 0;
            art_blit_info.art_id = art_id;
            art_blit_info.src_rect = &src_rect;
            art_blit_info.dst_rect = &dst_rect;
            tig_window_blit_art(intgame_interface_windows[1], &art_blit_info);
        }
    }
}

// 0x556E60
void intgame_magictech_item_reference_validate(void)
{
    int64_t parent_obj;

    if (intgame_magictech_item_obj != OBJ_HANDLE_NULL) {
        parent_obj = obj_field_handle_get(intgame_magictech_item_obj, OBJ_F_ITEM_PARENT);
        if (parent_obj == OBJ_HANDLE_NULL || !player_is_local_pc_obj(parent_obj)) {
            intgame_magictech_item_obj = OBJ_HANDLE_NULL;
        }
    }
}

// 0x556EA0
void intgame_magictech_item_panel_open(int64_t item_obj)
{
    if (item_obj != OBJ_HANDLE_NULL) {
        intgame_magictech_item_obj = item_obj;
    } else {
        intgame_magictech_item_obj = item_wield_get(player_get_local_pc_obj(), ITEM_INV_LOC_WEAPON);
    }
    iso_interface_window_set(ROTWIN_TYPE_MAGICTECH);
}

// 0x556EF0
void intgame_mt_button_enable(void)
{
    bool hidden;
    int64_t obj;
    int64_t item_obj;
    int mana_store;
    unsigned int flags;

    if (tig_button_is_hidden(intgame_mt_button_info.button_handle, &hidden) == TIG_OK && hidden) {
        obj = player_get_local_pc_obj();
        item_obj = item_wield_get(obj, ITEM_INV_LOC_WEAPON);
        if (item_obj != OBJ_HANDLE_NULL) {
            mana_store = obj_field_int32_get(item_obj, OBJ_F_ITEM_SPELL_MANA_STORE);
            flags = obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS);
            if ((mana_store != 0 || (flags & OIF_IS_MAGICAL) != 0)
                && obj_field_int32_get(item_obj, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY) > 0) {
                tig_button_show(intgame_mt_button_info.button_handle);
            }
        }
    }
}

// 0x556F80
void intgame_mt_button_disable(void)
{
    bool hidden;
    tig_art_id_t art_id;
    TigArtFrameData art_frame_data;
    TigArtBlitInfo art_blit_info;
    TigRect rect;

    if (tig_button_is_hidden(intgame_mt_button_info.button_handle, &hidden) == TIG_OK && !hidden) {
        tig_button_hide(intgame_mt_button_info.button_handle);

        if (tig_art_interface_id_create(intgame_mt_button_info.art_num, 0, 0, 0, &art_id) != TIG_OK) {
            tig_debug_printf("Intgame: intgame_mt_button_disable: ERROR: Can't find Interface Art: %d!\n", intgame_mt_button_info.art_num);
            return;
        }

        tig_art_frame_data(art_id, &art_frame_data);

        if (tig_art_interface_id_create(184, 0, 0, 0, &art_id) != TIG_OK) {
            tig_debug_printf("Intgame: intgame_mt_button_disable: ERROR: Can't find Interface Art: %d!\n", 184);
        }

        rect.x = intgame_mt_button_info.x - intgame_interface_window_frames[1].x;
        rect.y = intgame_mt_button_info.y - intgame_interface_window_frames[1].y;
        rect.width = art_frame_data.width;
        rect.height = art_frame_data.height;

        art_blit_info.flags = 0;
        art_blit_info.art_id = art_id;
        art_blit_info.src_rect = &rect;
        art_blit_info.dst_rect = &rect;
        tig_window_blit_art(intgame_interface_windows[1], &art_blit_info);
    }
}

// 0x5570A0
void intgame_selected_object_set(int64_t obj)
{
    intgame_selected_obj = obj;
    if (intgame_selected_obj != OBJ_HANDLE_NULL) {
        tb_ui_examine_and_select_object(player_get_local_pc_obj(), intgame_selected_obj);
    }
}

// 0x5570D0
void intgame_notify_item_inserted_or_removed(int64_t item_obj, bool removed, int inventory_location)
{
    int index;
    Hotkey* hotkey;

    for (index = 0; index < 10; index++) {
        hotkey = hotkey_ui_slot_get(index);
        if (removed && hotkey->item_obj.obj == item_obj) {
            hotkey_ui_slot_clear_callback(index);
        }

        if (hotkey->type == HOTKEY_ITEM
            && hotkey->item_obj.obj != OBJ_HANDLE_NULL) {
            hotkey->count = item_count_items_matching_prototype(player_get_local_pc_obj(), hotkey->item_obj.obj);
            intgame_hotkey_refresh(index);
        }
    }

    if (removed) {
        hotkey_ui_notify_item_inserted_or_removed(item_obj, removed);
        if (intgame_magictech_item_obj == item_obj
            && intgame_iso_window_type == ROTWIN_TYPE_MAGICTECH) {
            iso_interface_window_set(ROTWIN_TYPE_MSG);
            intgame_magictech_item_obj = OBJ_HANDLE_NULL;
        }

        if (inventory_location == ITEM_INV_LOC_WEAPON) {
            intgame_mt_button_disable();
        }
    } else {
        intgame_mt_button_enable();
    }

    if (hotkey_ui_is_dragging()) {
        intgame_refresh_cursor();
        hotkey_ui_dragging = false;
        hotkey_ui_dragging_index = -1;
    }
}

// 0x5571C0
void intgame_refresh_health_bar(int64_t obj)
{
    if (player_is_local_pc_obj(obj)) {
        anim_ui_event_add(ANIM_UI_EVENT_TYPE_UPDATE_HEALTH_BAR, -1);
    }

    if (object_hover_obj_get() == obj || intgame_selected_obj == obj) {
        tb_ui_examine_and_select_object(player_get_local_pc_obj(), obj);
    }

    follower_ui_update_obj(obj);
}

// 0x557230
bool intgame_big_window_create(void)
{
    TigWindowData window_data;

    window_data.flags = TIG_WINDOW_MESSAGE_FILTER;
    window_data.rect.x = 0;
    window_data.rect.y = 41;
    window_data.rect.width = 800;
    window_data.rect.height = 400;
    window_data.background_color = 0;
    window_data.message_filter = intgame_big_window_message_filter;
    hrp_apply(&(window_data.rect), GRAVITY_CENTER_HORIZONTAL | GRAVITY_CENTER_VERTICAL);

    if (tig_window_create(&window_data, &intgame_big_window_handle) != TIG_OK) {
        return false;
    }

    tig_window_hide(intgame_big_window_handle);
    intgame_big_window_locked = false;

    return true;
}

// 0x5572B0
void intgame_big_window_destroy(void)
{
    tig_window_destroy(intgame_big_window_handle);
}

// 0x5572C0
bool intgame_big_window_message_filter(TigMessage* msg)
{
    return false;
}

// 0x5572D0
bool intgame_big_window_lock(TigWindowMessageFilterFunc func, tig_window_handle_t* window_handle_ptr)
{
    return intgame_big_window_lock_height(func, window_handle_ptr, 400);
}

bool intgame_big_window_lock_height(TigWindowMessageFilterFunc func, tig_window_handle_t* window_handle_ptr, int height)
{
    if (intgame_big_window_locked) {
        return false;
    }

    if ((height != 400 && height != 450) || !ui_panel_window_height(&intgame_big_window_handle, height)) return false;

    intgame_big_window_locked = true;
    tig_window_message_filter_set(intgame_big_window_handle, func);
    tig_window_show(intgame_big_window_handle);
    tig_window_restack(intgame_big_window_handle);
    *window_handle_ptr = intgame_big_window_handle;

    return true;
}

// 0x557330
void intgame_big_window_unlock(void)
{
    intgame_big_window_locked = false;
    tig_window_button_destroy(intgame_big_window_handle);
    tig_window_message_filter_set(intgame_big_window_handle, intgame_big_window_message_filter);
    tig_window_hide(intgame_big_window_handle);
}

// 0x557370
void intgame_target_action_points_preview(int64_t source_obj, int64_t target_obj)
{
    unsigned int spell_flags;
    unsigned int critter_flags;
    int target_obj_type;

    if (source_obj == OBJ_HANDLE_NULL) {
        return;
    }

    if (!critter_is_active(source_obj)) {
        return;
    }

    if (target_obj == source_obj) {
        return;
    }

    spell_flags = obj_field_int32_get(source_obj, OBJ_F_SPELL_FLAGS);
    critter_flags = obj_field_int32_get(source_obj, OBJ_F_CRITTER_FLAGS);

    if ((spell_flags & OSF_STONED) != 0
        || (critter_flags & (OCF_PARALYZED | OCF_STUNNED)) != 0) {
        return;
    }

    target_obj_type = obj_field_int32_get(target_obj, OBJ_F_TYPE);

    // NOTE: The code below omit some unused checks from the original code. I'm
    // not sure if it's a bug or not.
    switch (intgame_mode_get()) {
    case INTGAME_MODE_MAIN:
        if (inven_ui_drag_item_obj_get() == OBJ_HANDLE_NULL) {
            switch (target_obj_type) {
            case OBJ_TYPE_WALL:
            case OBJ_TYPE_PORTAL:
            case OBJ_TYPE_SCENERY:
                if (tig_kb_get_modifier(SDL_KMOD_ALT)) {
                    combat_check_attack(source_obj, target_obj);
                } else {
                    if ((spell_flags & OSF_POLYMORPHED) == 0) {
                        combat_check_use_obj(source_obj, target_obj);
                    }
                }
                break;
            case OBJ_TYPE_CONTAINER:
                if ((spell_flags & OSF_POLYMORPHED) == 0) {
                    if (tig_kb_get_modifier(SDL_KMOD_ALT)) {
                        combat_check_attack(source_obj, target_obj);
                    } else {
                        combat_check_use_obj(source_obj, target_obj);
                    }
                }
                break;
            case OBJ_TYPE_WEAPON:
            case OBJ_TYPE_AMMO:
            case OBJ_TYPE_ARMOR:
            case OBJ_TYPE_GOLD:
            case OBJ_TYPE_FOOD:
            case OBJ_TYPE_SCROLL:
            case OBJ_TYPE_KEY:
            case OBJ_TYPE_KEY_RING:
            case OBJ_TYPE_WRITTEN:
            case OBJ_TYPE_GENERIC:
                if ((spell_flags & OSF_POLYMORPHED) == 0) {
                    combat_check_pick_item(source_obj, target_obj);
                }
                break;
            case OBJ_TYPE_PC:
            case OBJ_TYPE_NPC:
                if (critter_is_dead(target_obj)) {
                    if (tig_kb_get_modifier(SDL_KMOD_ALT)) {
                        combat_check_use_obj(source_obj, target_obj);
                    }
                } else {
                    if (!player_is_local_pc_obj(critter_pc_leader_get(target_obj)) || tig_kb_get_modifier(SDL_KMOD_LALT)) {
                        combat_check_attack(source_obj, target_obj);
                    }
                }
                break;
            case OBJ_TYPE_TRAP:
                combat_check_move_to(source_obj, obj_field_int64_get(target_obj, OBJ_F_LOCATION));
                break;
            }
        }
        break;
    case INTGAME_MODE_SPELL:
        combat_check_cast_spell(source_obj);
        break;
    case INTGAME_MODE_SKILL:
        combat_check_use_skill(source_obj);
        break;
    default:
        break;
    }
}

// 0x557670
void intgame_there_is_nothing_to_loot(void)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;

    mes_file_entry.num = 2000;
    mes_get_msg(intgame_mes_file, &mes_file_entry);

    ui_message.type = UI_MSG_TYPE_FEEDBACK;
    ui_message.str = mes_file_entry.str;
    intgame_message_window_display_msg(&ui_message);
}

// 0x5576B0
void intgame_secondary_panel_background_draw(void)
{
    TigRect rect;
    TigArtFrameData art_frame_data;
    TigArtBlitInfo art_blit_info;

    art_blit_info.flags = 0;
    art_blit_info.src_rect = &rect;
    art_blit_info.dst_rect = &rect;
    tig_art_interface_id_create(185, 0, 0, 0, &(art_blit_info.art_id));

    tig_art_frame_data(art_blit_info.art_id, &art_frame_data);

    rect.x = 0;
    rect.y = 0;
    rect.width = art_frame_data.width;
    rect.height = art_frame_data.height;
    tig_window_blit_art(intgame_interface_windows[0], &art_blit_info);
}

// 0x557730
void intgame_numbered_feedback_display(int index)
{
    MesFileEntry mes_file_entry;
    UiMessage ui_message;

    mes_file_entry.num = index + 3000;
    if (mes_search(intgame_mes_file, &mes_file_entry)) {
        mes_get_msg(intgame_mes_file, &mes_file_entry);

        ui_message.type = UI_MSG_TYPE_FEEDBACK;
        ui_message.str = mes_file_entry.str;
        intgame_message_window_display_msg(&ui_message);
    }
}

// 0x557790
void intgame_hovered_object_refresh(int64_t obj)
{
    if (obj != OBJ_HANDLE_NULL
        && obj == object_hover_obj_get()) {
        tb_ui_examine_and_select_object(player_get_local_pc_obj(), obj);
    }
}

// 0x5577D0
unsigned int intgame_get_iso_window_flags(void)
{
    return intgame_iso_window_flags;
}

// 0x5577E0
void intgame_set_iso_window_flags(unsigned int flags)
{
    intgame_iso_window_flags = flags;
}

// 0x5577F0
void intgame_set_iso_window_width(int width)
{
    intgame_iso_window_width = width;
}

// 0x557800
void intgame_set_iso_window_height(int height)
{
    intgame_iso_window_height = height;
}

// 0x557810
bool intgame_create_iso_window(tig_window_handle_t* window_handle_ptr)
{
    TigWindowData window_data;

    window_data.flags = intgame_iso_window_flags | TIG_WINDOW_ALWAYS_ON_BOTTOM | TIG_WINDOW_VIDEO_MEMORY;
    window_data.rect.x = 0;
    window_data.rect.y = 0;
    window_data.rect.width = intgame_iso_window_width;
    window_data.rect.height = intgame_iso_window_height;
    window_data.background_color = 0;

    if (tig_window_create(&window_data, window_handle_ptr) != TIG_OK) {
        tig_debug_printf("intgame_create_iso_window: ERROR: window create failed!\n");
        tig_exit();
        return false;
    }

    intgame_iso_window = *window_handle_ptr;
    return true;
}

// 0x5578C0
bool intgame_is_compact_interface(void)
{
    return intgame_compact_interface;
}

// 0x5578D0
void intgame_set_fullscreen(void)
{
    intgame_fullscreen = true;
}

// 0x5578E0
void intgame_toggle_interface(void)
{
    TigWindowData window_data;
    GameResizeInfo resize_info;
    int index;

    if (!intgame_fullscreen) {
        return;
    }

    tig_debug_printf("Resizing Iso View...");

    resize_info.window_handle = intgame_iso_window_handle;
    intgame_compact_interface = !intgame_compact_interface;

    if (intgame_iso_window != TIG_WINDOW_HANDLE_INVALID) {
        if (tig_window_data(intgame_iso_window_handle, &window_data) != TIG_OK) {
            tig_debug_printf("intgame_toggle_interface: ERROR: tig_window_data failed!\n");
            exit(EXIT_FAILURE);
        }

        resize_info.window_rect = window_data.rect;
        resize_info.content_rect = window_data.rect;

        if (intgame_compact_interface) {
            intgame_pc_lens_dst_rect = intgame_pc_lens_fullscreen_dst_frame;
            intgame_pc_lens_dst_rect.x = (800 - intgame_pc_lens_dst_rect.width) / 2;
            intgame_pc_lens_dst_rect.y = (600 - intgame_pc_lens_dst_rect.height) / 2;
            hrp_apply(&intgame_pc_lens_dst_rect, GRAVITY_CENTER_HORIZONTAL | GRAVITY_CENTER_VERTICAL);

            for (index = 0; index < 2; index++) {
                tig_window_hide(intgame_interface_windows[index]);
            }

            gamelib_resize(&resize_info);
            gameuilib_resize(&resize_info);

            compact_ui_create();
        } else {
            intgame_pc_lens_dst_rect = intgame_pc_lens_normal_dst_frame;
            intgame_pc_lens_dst_rect.x = (800 - intgame_pc_lens_dst_rect.width) / 2;
            intgame_pc_lens_dst_rect.y = (600 - intgame_pc_lens_dst_rect.height) / 2;
            hrp_apply(&intgame_pc_lens_dst_rect, GRAVITY_CENTER_HORIZONTAL | GRAVITY_CENTER_VERTICAL);

            gamelib_resize(&resize_info);
            gameuilib_resize(&resize_info);

            for (index = 0; index < 2; index++) {
                tig_window_show(intgame_interface_windows[index]);
            }

            compact_ui_destroy();
        }
    }

    if (tig_window_is_hidden(intgame_iso_window_handle)) {
        intgame_hide();
    }

    tig_debug_printf("completed.\n");
}

// TODO: Reuse `iso_interface_window_get`.
//
// 0x557AA0
RotatingWindowType iso_interface_window_get_3(void)
{
    return intgame_iso_window_type;
}

// 0x557AB0
int intgame_selected_spell_college_get(void)
{
    return intgame_selected_spell_college;
}

// 0x557AC0
void intgame_spell_button_info_get(int clg, int lvl, UiButtonInfo* button_info)
{
    if (button_info != NULL) {
        *button_info = intgame_spell_buttons[clg * 5 + lvl];
    }
}

// 0x557B00
int64_t intgame_spell_item_get(void)
{
    return intgame_magictech_item_obj;
}

// 0x557B10
mes_file_handle_t intgame_hotkey_mes_file(void)
{
    return intgame_mes_file;
}

// 0x557B20
UiButtonInfo* intgame_recent_action_button_get(int index)
{
    return &(intgame_recent_action_buttons[index]);
}

// 0x557B30
void intgame_recent_action_button_position_set(int index, int x, int y)
{
    intgame_recent_action_buttons[index].x = x;
    intgame_recent_action_buttons[index].y = y;
}

// 0x557B50
int intgame_recent_action_default_skill_get(int index)
{
    return intgame_recent_action_default_skills[index];
}

// 0x557B60
int intgame_mouse_skill_button_get(void)
{
    TigMouseState mouse_state;
    TigWindowData window_data;
    TigButtonData button_data;
    int x;
    int y;
    int index;

    if (intgame_iso_window_type != ROTWIN_TYPE_SKILLS) {
        return 4;
    }

    tig_mouse_get_state(&mouse_state);

    // Check if mouse position is within rotating window bounds.
    if (tig_window_data(intgame_interface_windows[1], &window_data) != TIG_OK
        || mouse_state.x < window_data.rect.x
        || mouse_state.y < window_data.rect.y
        || mouse_state.y >= window_data.rect.x + window_data.rect.width
        || mouse_state.y >= window_data.rect.y + window_data.rect.height) {
        return 4;
    }

    // Convert mouse position from screen coordinate system to rotating
    // window coordinate system.
    x = mouse_state.x - window_data.rect.x;
    y = mouse_state.y - window_data.rect.y;

    for (index = 0; index < 4; index++) {
        if (tig_button_data(intgame_skill_buttons[index].button_handle, &button_data) != TIG_OK) {
            break;
        }

        if (x >= button_data.x
            && y >= button_data.y
            && x < button_data.x + button_data.width
            && y < button_data.y + button_data.height) {
            return index;
        }
    }

    return 4;
}

// 0x557C00
int intgame_mouse_item_spell_button_get(void)
{
    TigMouseState mouse_state;
    TigWindowData window_data;
    TigButtonData button_data;
    int x;
    int y;
    int index;
    int64_t pc_obj;

    if (intgame_iso_window_type != ROTWIN_TYPE_MAGICTECH) {
        return 5;
    }

    tig_mouse_get_state(&mouse_state);

    // Check if mouse position is within rotating window bounds.
    if (tig_window_data(intgame_interface_windows[1], &window_data) != TIG_OK
        || mouse_state.x < window_data.rect.x
        || mouse_state.y < window_data.rect.y
        || mouse_state.y >= window_data.rect.x + window_data.rect.width
        || mouse_state.y >= window_data.rect.y + window_data.rect.height) {
        return 5;
    }

    // Convert mouse position from screen coordinate system to rotating window
    // coordinate system.
    x = mouse_state.x - window_data.rect.x;
    y = mouse_state.y - window_data.rect.y;

    pc_obj = player_get_local_pc_obj();
    if (pc_obj == OBJ_HANDLE_NULL) {
        return 5;
    }

    for (index = 0; index < 5; index++) {
        if (tig_button_data(intgame_mt_spell_buttons[index].button_handle, &button_data) != TIG_OK) {
            return 6;
        }

        if (x >= button_data.x
            && y >= button_data.y
            && x < button_data.x + button_data.width
            && y < button_data.y + button_data.height) {
            if (!magictech_has_item_triggers(mt_item_spell(intgame_magictech_item_obj, index))) {
                return index;
            } else {
                return 5;
            }
        }
    }

    return 5;
}

// 0x557CF0
int intgame_mouse_spell_button_get(void)
{
    TigMouseState mouse_state;
    TigWindowData window_data;
    TigButtonData button_data;
    int x;
    int y;
    int index;
    int64_t pc_obj;

    if (iso_interface_window_get_3() != ROTWIN_TYPE_SPELLS) {
        return 5;
    }

    tig_mouse_get_state(&mouse_state);

    // Check if mouse position is within rotating window bounds.
    if (tig_window_data(intgame_interface_windows[1], &window_data) != TIG_OK
        || mouse_state.x < window_data.rect.x
        || mouse_state.y < window_data.rect.y
        || mouse_state.y >= window_data.rect.x + window_data.rect.width
        || mouse_state.y >= window_data.rect.y + window_data.rect.height) {
        return 5;
    }

    // Convert mouse position from screen coordinate system to rotating window
    // coordinate system.
    x = mouse_state.x - window_data.rect.x;
    y = mouse_state.y - window_data.rect.y;

    pc_obj = player_get_local_pc_obj();
    if (pc_obj == OBJ_HANDLE_NULL) {
        return 5;
    }

    for (index = 0; index < 5; index++) {
        if (tig_button_data(intgame_spell_buttons[5 * intgame_selected_spell_college + index].button_handle, &button_data) != TIG_OK) {
            return 6;
        }

        if (x >= button_data.x
            && y >= button_data.y
            && x < button_data.x + button_data.width
            && y < button_data.y + button_data.height) {
            if (spell_is_known(pc_obj, 5 * intgame_selected_spell_college + index)) {
                return index;
            } else {
                return 5;
            }
        }
    }

    return 5;
}

void intgame_hide(void)
{
    int index;
    TigRect rect;

    logbook_ui_search_bar_visible(false);
    schematic_ui_toolbar_visible(false);
    wmap_ui_markers_visible(false);

    intgame_action_tooltips_reset();

    if (!intgame_iso_interface_created) {
        return;
    }

    tig_window_hide(intgame_iso_window_handle);

    for (index = 0; index < 2; index++) {
        tig_window_hide(intgame_interface_windows[index]);
    }

    if (intgame_fs_hotkey_window != TIG_WINDOW_HANDLE_INVALID) {
        tig_window_hide(intgame_fs_hotkey_window);
    }

    for (index = 0; index < 5; index++) {
        tig_window_hide(intgame_maintain_fs_windows[index]);
    }

    follower_ui_hide();

    // Move bottom bar window up so its aligned with mainmenu covers.
    rect = intgame_interface_window_frames[1];
    hrp_apply(&rect, GRAVITY_CENTER_HORIZONTAL | GRAVITY_CENTER_VERTICAL);
    tig_window_move(intgame_interface_windows[1], rect.x, rect.y);

    if (!intgame_is_compact_interface()) {
        tig_window_show(intgame_interface_windows[1]);
    }
}

// CE: TAB-cycle HUD-crop state. Bound to the TAB key in main.c.
//
// Four stages cycle FULL → MEDIUM → MINI → HIDDEN → FULL. Instead
// of hiding/moving the strip windows, we use tig's per-window clip
// rect (tig_window_clip_rect_set) to composite only a chosen band
// of the bottom strip — the rest of the bar's VB stays intact, so
// when the user cycles back to FULL the original bar reappears with
// zero rebuilding. Buttons, rotwin chrome, text writes etc. all
// continue working in their natural positions; the only thing that
// changes is which pixels make it to screen.
//
// The top strip is always either fully visible (FULL) or fully
// clipped (everything else).
//
// Stage-relative bands within the BOTTOM strip's VB (strip-relative
// design coords; the strip is 800x159 anchored at y=441 in 800x600
// reference space):
//   FULL    → no clip; whole strip composites
//   MEDIUM  → rotwin region: (196, 51, 410, 107) — the full
//             skill_rot / dialoguewindow frame area
//   MINI    → slim row: (205, 122, 394, 25) — the cropped 18px
//             rollover row (skill_rot region at art-local (9,71))
//   HIDDEN  → clip to a 0-sized rect; nothing composites
//
// When the user presses K/M (skills/spells) and the current stage
// is HIDDEN or MINI, the cycle auto-pops to MEDIUM so the rotwin
// becomes interactable. Cycling TAB or pressing K again reverts.

typedef enum IntgameHudStage {
    INTGAME_HUD_STAGE_FULL = 0,
    INTGAME_HUD_STAGE_MEDIUM = 1,
    INTGAME_HUD_STAGE_MINI = 2,
    INTGAME_HUD_STAGE_HIDDEN = 3,
    INTGAME_HUD_STAGE_COUNT,
} IntgameHudStage;

static IntgameHudStage intgame_hud_stage = INTGAME_HUD_STAGE_FULL;
// CE: pre-rotwin stage snapshot. When the user is in MINI/HIDDEN and
// invokes a rotwin (K/M, magictech weapon, etc.), we auto-pop to MEDIUM
// and remember where to return when the rotwin is dismissed. Cleared
// by user-driven TAB cycling (manual override).
static IntgameHudStage intgame_hud_saved_stage = INTGAME_HUD_STAGE_FULL;
static bool intgame_hud_has_saved_stage = false;
// CE: MINI is a "hack" stage — it silently keeps SKILLS active so the
// slim text row shows real hover content. Pressing K (or whichever key
// matches the active rotwin) while MINI is up should NOT dismiss the
// rotwin (that would swap to MSG and visually break the row); instead
// it peek-expands to MEDIUM. A second press returns to MINI without
// dismissing. This flag tracks "we're MEDIUM but only because MINI
// peeked," so we know to return-to-MINI instead of toggling-off.
static bool intgame_hud_peek_from_mini = false;
// Mirror bool kept for the existing intgame_hud_is_user_hidden()
// consumers (camera-follow margin math, fate/sleep top-bar dock).
static bool intgame_hud_user_hidden;

// Stage-band design coords inside the bottom strip's 800x159 VB.
#define INTGAME_HUD_BOTTOM_W 800
#define INTGAME_HUD_BOTTOM_H 159
#define INTGAME_HUD_MEDIUM_BAND_X 196
#define INTGAME_HUD_MEDIUM_BAND_Y 51
#define INTGAME_HUD_MEDIUM_BAND_W 410
#define INTGAME_HUD_MEDIUM_BAND_H 107
#define INTGAME_HUD_MINI_BAND_X 205
#define INTGAME_HUD_MINI_BAND_Y 122
#define INTGAME_HUD_MINI_BAND_W 394
#define INTGAME_HUD_MINI_BAND_H 25

void intgame_show(void)
{
    int index;
    TigRect rect;

    if (!intgame_iso_interface_created) {
        return;
    }

    tig_window_show(intgame_iso_window_handle);

    if (!intgame_is_compact_interface()) {
        for (index = 0; index < 2; index++) {
            tig_window_show(intgame_interface_windows[index]);
        }
    }

    if (intgame_fs_hotkey_window != TIG_WINDOW_HANDLE_INVALID) {
        tig_window_show(intgame_fs_hotkey_window);
    }

    for (index = 0; index < 5; index++) {
        if (spell_ui_maintain_has(index)) {
            tig_window_show(intgame_maintain_fs_windows[index]);
        }
    }

    // Restore bottom bar position.
    rect = intgame_interface_window_frames[1];
    hrp_apply(&rect, GRAVITY_CENTER_HORIZONTAL | GRAVITY_BOTTOM);
    tig_window_move(intgame_interface_windows[1], rect.x, rect.y);

    follower_ui_show();

    tig_window_near_black_alpha_set(intgame_interface_windows[1],
        settings_get_value(&settings, TRANSLUCENT_BLACK_UI_KEY) != 0,
        intgame_iso_window, 8, 128);

    int desired_stage = settings_get_value(&settings, HUD_STAGE_KEY);
    if (desired_stage >= 0 && desired_stage < INTGAME_HUD_STAGE_COUNT) {
        while ((int)intgame_hud_stage != desired_stage) {
            intgame_hud_user_toggle();
        }
    }

    // CE: Re-apply the current TAB stage's clip after a modal menu
    // cycle (Esc main menu, Options, Save/Load) — intgame_show is
    // called on close and would otherwise leave the strips fully
    // visible regardless of the user's TAB stage.
    intgame_hud_apply_clips();
    logbook_ui_search_bar_visible(true);
    schematic_ui_toolbar_visible(true);
    wmap_ui_markers_visible(true);
}

// Compute and install the per-strip clip rects for the current
// stage. Idempotent — safe to call from any path that may have
// reset the strips (intgame_show, mode exits, etc.).
static void intgame_hud_apply_clips(void)
{
    if (!intgame_iso_interface_created) {
        return;
    }
    intgame_hud_user_hidden = (intgame_hud_stage != INTGAME_HUD_STAGE_FULL);

    // CE: invalidate the world (iso VB) under both bar strips on every
    // stage change. The iso layer only paints into its VB for regions
    // gamelib has been asked to redraw; while a strip is opaque on top,
    // iso leaves that band stale. When the clip uncovers part of the
    // strip, the compositor blits iso's stale pixels through the hole
    // and the user sees an edge smear. Invalidating here forces the
    // world to repaint into iso's VB so the newly-exposed band has
    // fresh content by the next composite.
    for (int strip_idx = 0; strip_idx < 2; strip_idx++) {
        if (intgame_interface_windows[strip_idx] == TIG_WINDOW_HANDLE_INVALID) {
            continue;
        }
        TigWindowData strip_wd;
        if (tig_window_data(intgame_interface_windows[strip_idx], &strip_wd) == TIG_OK) {
            iso_invalidate_rect(&strip_wd.rect);
        }
    }

    // Top strip: only visible in FULL stage.
    if (intgame_interface_windows[0] != TIG_WINDOW_HANDLE_INVALID) {
        if (intgame_hud_stage == INTGAME_HUD_STAGE_FULL) {
            tig_window_clip_rect_set(intgame_interface_windows[0], NULL);
        } else {
            // Zero-size clip → window composites nothing.
            TigRect empty = { 0, 0, 0, 0 };
            tig_window_clip_rect_set(intgame_interface_windows[0], &empty);
        }
    }

    // Bottom strip: stage-specific band.
    if (intgame_interface_windows[1] != TIG_WINDOW_HANDLE_INVALID) {
        TigWindowData wd;
        if (tig_window_data(intgame_interface_windows[1], &wd) != TIG_OK) {
            return;
        }
        // wd.rect is the strip's screen-coord frame after hrp_apply.
        // Bands below are strip-local (design coords); add the
        // frame origin to get screen coords for the clip rect.
        int sx = wd.rect.x;
        int sy = wd.rect.y;
        TigRect band;
        // CE: anchor the band's Y to the active rotwin's chrome top so
        // a per-type anchor offset (e.g. SKILLS/SPELLS shifted 1px up
        // vs MSG/dialogue) doesn't leave a sliver of bar background
        // above/below the chrome. Width and X stay constant — the band
        // is always 410px wide centered on the chrome anchor.
        int chrome_strip_y = INTGAME_HUD_MEDIUM_BAND_Y;
        if (intgame_iso_window_type >= 0
            && intgame_iso_window_type < ROTWIN_TYPE_COUNT) {
            int btn_y = intgame_rotwin_button_info[intgame_iso_window_type].y;
            // Skip the zero-sized stub entries (QUANTITY / MP_KICKBAN).
            if (btn_y > intgame_interface_window_frames[1].y) {
                chrome_strip_y = btn_y - intgame_interface_window_frames[1].y;
            }
        }
        switch (intgame_hud_stage) {
        case INTGAME_HUD_STAGE_FULL:
            tig_window_clip_rect_set(intgame_interface_windows[1], NULL);
            break;
        case INTGAME_HUD_STAGE_MEDIUM:
            band.x = sx + INTGAME_HUD_MEDIUM_BAND_X;
            band.y = sy + chrome_strip_y;
            band.width = INTGAME_HUD_MEDIUM_BAND_W;
            band.height = INTGAME_HUD_MEDIUM_BAND_H;
            tig_window_clip_rect_set(intgame_interface_windows[1], &band);
            break;
        case INTGAME_HUD_STAGE_MINI:
            // MINI shows just the inner text-row band of the chrome art
            // (art-local row at +71). Slides with the chrome anchor.
            band.x = sx + INTGAME_HUD_MINI_BAND_X;
            band.y = sy + chrome_strip_y + (INTGAME_HUD_MINI_BAND_Y - INTGAME_HUD_MEDIUM_BAND_Y);
            band.width = INTGAME_HUD_MINI_BAND_W;
            band.height = INTGAME_HUD_MINI_BAND_H;
            tig_window_clip_rect_set(intgame_interface_windows[1], &band);
            break;
        case INTGAME_HUD_STAGE_HIDDEN:
        default: {
            TigRect empty = { 0, 0, 0, 0 };
            tig_window_clip_rect_set(intgame_interface_windows[1], &empty);
            break;
        }
        }
    }

    // Reposition top-bar-docked panels so they sit flush against the
    // screen top when the bar is hidden, or below the bar when it's
    // shown. No-op if the respective panel isn't currently open.
    fate_ui_reposition();
    sleep_ui_reposition();

    // Notify the dialog options backdrop of the new bar-gap so it can
    // drop down into the freed space. No-op when no dialog is active.
    tc_set_bottom_gap_offset(intgame_hud_bottom_gap_offset());
}

void intgame_hud_user_toggle(void)
{
    if (!intgame_iso_interface_created) {
        return;
    }
    // Manual TAB cycle = user override; drop any auto-pop snapshot so
    // a later rotwin dismiss won't snap them back to a stage they've
    // since moved past.
    intgame_hud_has_saved_stage = false;
    intgame_hud_peek_from_mini = false;
    IntgameHudStage prev_stage = intgame_hud_stage;
    intgame_hud_stage = (intgame_hud_stage + 1) % INTGAME_HUD_STAGE_COUNT;
    settings_set_value(&settings, HUD_STAGE_KEY, intgame_hud_stage);

    // CE: MINI's only purpose is the slim rollover row — and that row
    // is driven by whichever rotwin is currently active. On the way
    // INTO MINI, silently force the SKILLS rotwin so the row shows
    // skill names / hover text. On the way OUT of MINI (cycling to
    // HIDDEN or wrapping back to FULL), drop the rotwin back to MSG
    // so we don't leave SKILLS arbitrarily active afterwards. Use
    // iso_interface_window_swap directly (not _set) to skip the
    // K/M-style auto-pop-to-MEDIUM hook — we want the type change
    // without the stage override. The swap itself re-applies clips,
    // so we skip the manual apply below in those branches.
    bool entering_mini = (intgame_hud_stage == INTGAME_HUD_STAGE_MINI
        && prev_stage != INTGAME_HUD_STAGE_MINI);
    bool leaving_mini = (prev_stage == INTGAME_HUD_STAGE_MINI
        && intgame_hud_stage != INTGAME_HUD_STAGE_MINI);

    if (entering_mini) {
        intgame_hud_enter_mini_with_skills();
        return;
    }
    if (leaving_mini) {
        // Sync the secondary buttons back to RELEASED so the next K
        // / M press behaves as an activation, not a dismiss-of-stale.
        tig_button_state_change(
            intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SKILLS].button_handle,
            TIG_BUTTON_STATE_RELEASED);
        tig_button_state_change(
            intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SPELLS].button_handle,
            TIG_BUTTON_STATE_RELEASED);
        if (intgame_iso_window_type != ROTWIN_TYPE_MSG) {
            iso_interface_window_swap(ROTWIN_TYPE_MSG);
            return;
        }
    }
    intgame_hud_apply_clips();
}

// Auto-pop to MEDIUM when the user invokes a rotwin (K/M/etc.)
// while the HUD is in MINI or HIDDEN. Called from the existing
// iso_interface_window_set hook.
void intgame_hud_auto_pop_for_rotwin(void)
{
    if (!intgame_iso_interface_created) {
        return;
    }
    if (intgame_hud_stage == INTGAME_HUD_STAGE_MINI
        || intgame_hud_stage == INTGAME_HUD_STAGE_HIDDEN) {
        // Stash the user's pre-rotwin stage so dismissing the rotwin
        // can return to it. Don't overwrite an existing snapshot — a
        // rotwin-to-rotwin switch (SKILLS->SPELLS) is still part of
        // the same pop, and we want to restore the *original* state.
        if (!intgame_hud_has_saved_stage) {
            intgame_hud_saved_stage = intgame_hud_stage;
            intgame_hud_has_saved_stage = true;
        }
        intgame_hud_stage = INTGAME_HUD_STAGE_MEDIUM;
        intgame_hud_apply_clips();
    }
}

// Restore the pre-rotwin stage stashed by intgame_hud_auto_pop_for_rotwin.
// Called when the rotwin returns to MSG (user re-pressed the dismiss key,
// hovered off, magictech weapon cleared, etc.). No-op if nothing was
// stashed or the interface isn't up.
void intgame_hud_restore_after_rotwin(void)
{
    if (!intgame_iso_interface_created) {
        return;
    }
    if (!intgame_hud_has_saved_stage) {
        return;
    }
    IntgameHudStage restored = intgame_hud_saved_stage;
    intgame_hud_has_saved_stage = false;
    if (restored == INTGAME_HUD_STAGE_MINI) {
        // Use the centralized MINI-invariant helper so type, stage,
        // and button state all line up.
        intgame_hud_enter_mini_with_skills();
    } else {
        intgame_hud_stage = restored;
        intgame_hud_apply_clips();
    }
}

// CE: enforce MINI invariants. MINI's whole point is "SKILLS rotwin
// silently active so the slim row shows skill hover content". Any
// path that lands us in MINI must call this so the rotwin and the
// secondary button state stay in sync — TAB-into-MINI, MEDIUM-peek
// return, dismiss-while-peeked, etc.
static bool intgame_hud_in_mini_stage(void)
{
    return intgame_iso_interface_created
        && intgame_hud_stage == INTGAME_HUD_STAGE_MINI;
}

static void intgame_hud_enter_mini_with_skills(void)
{
    if (!intgame_iso_interface_created) {
        return;
    }
    intgame_hud_stage = INTGAME_HUD_STAGE_MINI;
    if (intgame_iso_window_type != ROTWIN_TYPE_SKILLS) {
        tig_button_state_change(
            intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SPELLS].button_handle,
            TIG_BUTTON_STATE_RELEASED);
        tig_button_state_change(
            intgame_secondary_buttons[INTGAME_SECONDARY_BUTTON_SKILLS].button_handle,
            TIG_BUTTON_STATE_PRESSED);
        // swap re-applies clips for us.
        iso_interface_window_swap(ROTWIN_TYPE_SKILLS);
    } else {
        intgame_hud_apply_clips();
    }
}

static bool intgame_hud_handle_mini_peek_press(void)
{
    if (!intgame_iso_interface_created) {
        return false;
    }
    // MINI -> MEDIUM peek: rotwin stays active, only the crop expands.
    if (intgame_hud_stage == INTGAME_HUD_STAGE_MINI) {
        intgame_hud_peek_from_mini = true;
        intgame_hud_stage = INTGAME_HUD_STAGE_MEDIUM;
        intgame_hud_apply_clips();
        return true;
    }
    // MEDIUM (peeked) -> MINI return: same condition, opposite direction.
    // Re-enter MINI invariants so SKILLS is active even if the user
    // switched the rotwin to SPELLS while peeking.
    if (intgame_hud_peek_from_mini
        && intgame_hud_stage == INTGAME_HUD_STAGE_MEDIUM) {
        intgame_hud_peek_from_mini = false;
        intgame_hud_enter_mini_with_skills();
        return true;
    }
    return false;
}

bool intgame_hud_is_user_hidden(void)
{
    return intgame_hud_user_hidden;
}

int intgame_hud_top_offset(void)
{
    // 41 = top HUD strip design height. Fate/sleep panels dock at
    // y=41 when the top bar is visible (FULL), y=0 otherwise.
    return intgame_hud_user_hidden ? 0 : 41;
}

// Half of the bottom strip's currently hidden height (in design coords).
// The bottom strip is 159px tall; in MEDIUM/MINI/HIDDEN stages portions
// of it are clipped out. tc.c (dialog options backdrop) shifts down by
// this amount when shown so it takes half of the visual gap that the
// cropped bar leaves behind.
int intgame_hud_bottom_gap_offset(void)
{
    switch (intgame_hud_stage) {
    case INTGAME_HUD_STAGE_FULL:
        return 0;
    case INTGAME_HUD_STAGE_MEDIUM:
        // Chrome 107px visible; ~52 hidden. Half = 26.
        return 26;
    case INTGAME_HUD_STAGE_MINI:
        // Only the 25px slim row visible; 134 hidden. Half = 67.
        return 67;
    case INTGAME_HUD_STAGE_HIDDEN:
        // Entire 159 hidden. Half = 79.
        return 79;
    default:
        return 0;
    }
}

void intgame_hud_tick_invalidate_alpha_strips(void)
{
    // Only visible translucent regions sample the world's buffer. Opaque or
    // cropped-away HUD areas do not require a world redraw each tick.
    if (!intgame_iso_interface_created) {
        return;
    }
    TigRect rect;
    if (tig_window_translucent_underlay_rect(intgame_interface_windows[1], intgame_iso_window, &rect)) {
        iso_invalidate_rect(&rect);
    }
    if (intgame_big_window_locked
        && tig_window_translucent_underlay_rect(intgame_big_window_handle, intgame_iso_window, &rect)) {
        iso_invalidate_rect(&rect);
    }
}

// CE: Apply the optional TranslucentBlackUI effect to a window. Used
// by inventory / paperdoll / loot / barter screens (and anything else
// sharing the intgame_big_window_handle) to opt in/out at lock/unlock
// time, since the big window is shared across UIs that may or may not
// want the effect. enable=false disables it.
void intgame_apply_translucent_black(tig_window_handle_t window_handle, bool enable)
{
    if (window_handle == TIG_WINDOW_HANDLE_INVALID) {
        return;
    }
    if (enable && !settings_get_value(&settings, TRANSLUCENT_BLACK_UI_KEY)) {
        // Cfg disables the feature globally — ensure the window has
        // alpha cleared in case it was set by a prior owner.
        tig_window_near_black_alpha_set(window_handle, false,
            TIG_WINDOW_HANDLE_INVALID, 0, 0);
        return;
    }
    tig_window_near_black_alpha_set(window_handle, enable,
        intgame_iso_window, 8, 128);
}

#define ACTION_INFO_FRAME_WIDTH 210
#define ACTION_INFO_FRAME_HEIGHT 24

static UiStatusPanel intgame_action_info_panels[2] = {
    { .window = TIG_WINDOW_HANDLE_INVALID }, { .window = TIG_WINDOW_HANDLE_INVALID },
};
static tig_timestamp_t intgame_action_tooltips_last_tick;

static void intgame_action_info_hide(void)
{
    for (int side = 0; side < 2; side++) ui_status_panel_hide(&intgame_action_info_panels[side]);
}

static void intgame_action_tooltips_reset(void)
{
    intgame_action_tooltips_last_tick = 0;
    for (int side = 0; side < 2; side++) ui_status_panel_destroy(&intgame_action_info_panels[side]);
}

static void intgame_action_info_draw(int side, const char* text)
{
    TigRect anchor = intgame_interface_window_frames[1];

    hrp_apply(&anchor, GRAVITY_CENTER_HORIZONTAL | GRAVITY_BOTTOM);
    if (intgame_hud_stage == INTGAME_HUD_STAGE_MEDIUM || intgame_hud_stage == INTGAME_HUD_STAGE_MINI) {
        int chrome_y = INTGAME_HUD_MEDIUM_BAND_Y;
        if (intgame_iso_window_type >= 0 && intgame_iso_window_type < ROTWIN_TYPE_COUNT) {
            int button_y = intgame_rotwin_button_info[intgame_iso_window_type].y;
            if (button_y > intgame_interface_window_frames[1].y) {
                chrome_y = button_y - intgame_interface_window_frames[1].y;
            }
        }
        anchor.y += chrome_y;
        if (intgame_hud_stage == INTGAME_HUD_STAGE_MINI) {
            anchor.y += INTGAME_HUD_MINI_BAND_Y - INTGAME_HUD_MEDIUM_BAND_Y;
        }
    } else if (intgame_hud_stage == INTGAME_HUD_STAGE_HIDDEN || intgame_is_compact_interface()) {
        anchor.y = hrp_iso_window_height_get();
    }
    // Leave the central turn-based action-point bar unobstructed.
    int x = side == 0 ? anchor.x + 6 : anchor.x + anchor.width - ACTION_INFO_FRAME_WIDTH - 6;
    int y = anchor.y - ACTION_INFO_FRAME_HEIGHT - 2;
    ui_status_panel_draw(&intgame_action_info_panels[side],
        (TigRect){ x, y, ACTION_INFO_FRAME_WIDTH, ACTION_INFO_FRAME_HEIGHT }, text,
        intgame_flare12_white_font, tig_color_make(0, 0, 0), tig_color_make(144, 112, 48));
}

static const char* intgame_action_info_label(int id)
{
    return mes_localized_get("combat_info", id);
}

static void intgame_action_info_build(int64_t pc, int mode, char* left, size_t left_size, char* right, size_t right_size)
{
    TigMouseState mouse;
    TargetDescriptor td;
    int x, y;
    if (intgame_camera_pan_active || hotkey_ui_is_dragging()
        || inven_ui_drag_item_obj_get() != OBJ_HANDLE_NULL
        || tig_mouse_get_state(&mouse) != TIG_OK || !intgame_point_is_over_window(mouse.x, mouse.y)) return;
    intgame_adjust_mouse_for_zoom(mouse.x, mouse.y, &x, &y);
    if (!target_pick_at_screen_xy_ex(x, y, &td, TGT_TILE | TGT_OBJECT | TGT_OBJ_T_CRITTER, intgame_fullscreen)) return;
    int64_t loc = td.is_loc ? td.loc : obj_field_int64_get(td.obj, OBJ_F_LOCATION);
    int distance = (int)location_dist(obj_field_int64_get(pc, OBJ_F_LOCATION), loc);
    if (mode == INTGAME_MODE_SPELL) {
        int fatigue, range;
        if (!spell_ui_cast_preview(&fatigue, &range)) return;
        if (fatigue >= 0) {
            SDL_snprintf(left, left_size, "%s %d | %s %d",
                intgame_action_info_label(6), fatigue,
                intgame_action_info_label(5), distance);
        } else {
            SDL_snprintf(left, left_size, "%s | %s %d",
                intgame_action_info_label(7),
                intgame_action_info_label(5), distance);
        }
        SDL_snprintf(right, right_size, "%s : %d", intgame_action_info_label(3), range);
    } else if (!td.is_loc && td.obj != pc && !critter_is_dead(td.obj)) {
        int64_t weapon = combat_critter_weapon(pc);
        int range = item_weapon_skill(weapon) == SKILL_THROWING
            ? item_throwing_distance(weapon, pc) : item_weapon_range(weapon, pc);
        int hit_loc = tig_kb_is_key_pressed(SDL_SCANCODE_COMMA) ? HIT_LOC_HEAD
            : tig_kb_is_key_pressed(SDL_SCANCODE_PERIOD) ? HIT_LOC_LEG
            : tig_kb_is_key_pressed(SDL_SCANCODE_SLASH) ? HIT_LOC_ARM : HIT_LOC_TORSO;
        int chance = combat_hit_chance_preview(pc, td.obj, hit_loc);
        if (distance > range) chance = 0;
        SDL_snprintf(left, left_size, "%s %d %s | %s %d",
            intgame_action_info_label(1), combat_attack_cost(pc),
            intgame_action_info_label(9), intgame_action_info_label(5), distance);
        SDL_snprintf(right, right_size, "%s ~%d%% | %s %d",
            intgame_action_info_label(2), chance, intgame_action_info_label(3), range);
    } else if (td.is_loc) {
        int cost = combat_move_cost(pc, loc, false);
        if (cost == 0 && distance != 0) {
            SDL_strlcpy(left, intgame_action_info_label(8), left_size);
        } else {
            SDL_snprintf(left, left_size, "%s %d %s | %s %d",
                intgame_action_info_label(4), cost,
                intgame_action_info_label(9), intgame_action_info_label(5), distance);
        }
    }
}

void intgame_action_tooltips_tick(void)
{
    int mode = intgame_mode_get();
    if (!intgame_iso_interface_created || mainmenu_ui_is_active()
        || (mode != INTGAME_MODE_MAIN && mode != INTGAME_MODE_SPELL)
        || intgame_dialog_process_event_func != NULL || tig_window_is_hidden(intgame_iso_window_handle)) {
        intgame_action_info_hide();
        return;
    }
    int64_t pc = player_get_local_pc_obj();
    if (pc == OBJ_HANDLE_NULL || !obj_handle_is_valid(pc)
        || !settings_get_value(&settings, ACTION_TOOLTIPS_KEY)
        || (!combat_critter_is_combat_mode_active(pc) && !combat_turn_based_is_active())) {
        intgame_action_info_hide();
        return;
    }
    // Bound pathfinding to five updates per second, only during combat.
    if (tig_timer_between(intgame_action_tooltips_last_tick, gamelib_ping_time) < 200) return;
    intgame_action_tooltips_last_tick = gamelib_ping_time;
    char left[128] = "";
    char right[128] = "";
    intgame_action_info_build(pc, mode, left, sizeof(left), right, sizeof(right));
    // Both frames remain visible during combat; clear unavailable information
    // rather than retaining the previous target or following the pointer.
    intgame_action_info_draw(0, left);
    intgame_action_info_draw(1, right);
}

void intgame_movement_preview_tick(void)
{
    TigMouseState mouse;
    TargetDescriptor target;
    int x, y;
    int64_t pc = player_get_local_pc_obj();
    if (!intgame_iso_interface_created || gamelib_in_load() || gamelib_in_reset()
        || gamelib_in_save() || !map_is_valid() || map_is_clearing_objects()
        || mainmenu_ui_is_active() || intgame_mode_get() != INTGAME_MODE_MAIN
        || intgame_dialog_process_event_func != NULL || intgame_camera_pan_active
        || tig_window_is_hidden(intgame_iso_window_handle)
        || !settings_get_value(&settings, MOVEMENT_PREVIEW_KEY)
        || pc == OBJ_HANDLE_NULL || !obj_handle_is_valid(pc) || !critter_is_active(pc)
        || hotkey_ui_is_dragging() || inven_ui_drag_item_obj_get() != OBJ_HANDLE_NULL
        || (combat_turn_based_is_active() && combat_turn_based_whos_turn_get() != pc)
        || tig_mouse_get_state(&mouse) != TIG_OK || !intgame_point_is_over_window(mouse.x, mouse.y)) {
        movement_preview_clear();
        return;
    }
    intgame_adjust_mouse_for_zoom(mouse.x, mouse.y, &x, &y);
    if (!target_pick_at_screen_xy_ex(x, y, &target, TGT_TILE | TGT_OBJECT, intgame_fullscreen)
        || !target.is_loc || target.loc == obj_field_int64_get(pc, OBJ_F_LOCATION)) {
        movement_preview_clear();
        return;
    }
    movement_preview_update(intgame_iso_window_handle, pc, target.loc);
}
