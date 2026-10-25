#include "ui/inven_ui.h"
#include "ui/asset_warmup.h"
#include "ui/item_compare.h"

#include <stdio.h>

#include "game/ai.h"
#include "game/critter.h"
#include "game/dialog.h"
#include "game/gsound.h"
#include "game/hrp.h"
#include "game/item.h"
#include "game/mes.h"
#include "game/mp_utils.h"
#include "game/multiplayer.h"
#include "game/obj.h"
#include "game/obj_private.h"
#include "game/object.h"
#include "game/player.h"
#include "game/portrait.h"
#include "game/proto.h"
#include "game/reaction.h"
#include "game/script.h"
#include "game/sfx.h"
#include "game/skill.h"
#include "game/snd.h"
#include "game/spell.h"
#include "game/stat.h"
#include "game/target.h"
#include "ui/hotkey_ui.h"
#include "ui/intgame.h"
#include "ui/item_ui.h"
#include "ui/mainmenu_ui.h"
#include "ui/scrollbar_ui.h"
#include "ui/skill_ui.h"
#include "ui/spell_ui.h"
#include "ui/tb_ui.h"

typedef enum InvenUiPanel {
    INVEN_UI_PANEL_INVENTORY,
    INVEN_UI_PANEL_PAPERDOLL,
} InvenUiPanel;

typedef enum InvenUiSummary {
    INVEN_UI_SUMMARY_WEIGHT,
    INVEN_UI_SUMMARY_ENCUMBRANCE,
    INVEN_UI_SUMMARY_INFO_SPEED,
} InvenUiSummary;

typedef enum InvenUiTotalStat {
    INVEN_UI_TOTAL_ATTACK,
    INVEN_UI_TOTAL_DEFENSE,
} InvenUiTotalStat;

typedef struct InventoryDeferredDrag {
    /* 0000 */ int x;
    /* 0004 */ int y;
    /* 0008 */ int64_t item_obj;
    /* 0010 */ int64_t original_owner_obj;
    /* 0018 */ int64_t player_obj;
    /* 0020 */ int mode;
    /* 0024 */ int original_inventory_slot;
    /* 0028 */ int target_scroll_offset;
    /* 002C */ int player_scroll_offset;
} InventoryDeferredDrag;

// Serializeable.
static_assert(sizeof(InventoryDeferredDrag) == 0x30, "wrong size");

static bool inven_ui_close_if_created(void);
static bool inven_ui_message_filter(TigMessage* msg);
static void inven_ui_follower_cycle(bool next);
static bool inven_ui_dragged_item_plant(bool* success_ptr);
static bool inven_ui_dragged_item_steal(bool* success_ptr);
static void inven_ui_drop_on_player_inventory(int inventory_location);
static void inven_ui_drop_on_target_inventory(int inventory_location);
static void inven_ui_deferred_drag_request(int x, int y);
static bool inven_ui_deferred_drag_context_free(void* userinfo);
static bool inven_ui_deferred_drag_complete(void* userinfo);
static void inven_ui_dragged_item_drop_at_owner(void);
static int inven_ui_inventory_slot_at_xy(int x, int y, int64_t* parent_obj_ptr);
static int64_t inven_ui_item_at_xy(int x, int y, int64_t* parent_obj_ptr);
static void redraw_inven_fix_bad_inven_obj(int64_t obj);
static bool inven_ui_invalid_parent_link_remove(int64_t item_obj, int64_t expected_owner);
static void redraw_inven(bool initial_draw);
static tig_art_id_t inven_ui_background_art_id_get(void);
static void inven_ui_trade_offer_refresh(int64_t item_obj, int64_t owner_obj);
static void inven_ui_identification_quote_show(int64_t obj);
static void inven_ui_repair_offer_refresh(int64_t obj);
static void inven_ui_drag_item_insert_or_transfer(int64_t item_obj, int64_t target_obj, int new_inventory_location, int transaction_flags);
static void inven_ui_transfer_visible_items(int64_t from_obj, int64_t to_obj);
static bool inven_ui_gambling_offer_check(int64_t obj, bool silent);
static void inven_ui_item_gamble(int64_t obj);
static void inven_ui_target_inventory_scrollbar_create(void);
static void inven_ui_player_inventory_scrollbar_create(void);
static void inven_ui_player_scroll_value_changed(int value);
static void inven_ui_player_background_draw(TigRect* rect);
static void inven_ui_target_inventory_scrollbar_destroy(void);
static void inven_ui_target_inventory_scrollbar_show(void);
static void inven_ui_target_inventory_scrollbar_hide(void);
static void inven_ui_target_scroll_value_changed(int value);
static void inven_ui_target_background_draw(TigRect* rect);

// 0x5CAC58
static TigRect inven_ui_inventory_paperdoll_inv_slot_rects[9] = {
    /*    HELMET */ { 151, 107, 64, 64 },
    /*     RING1 */ { 247, 107, 32, 32 },
    /*     RING2 */ { 279, 107, 32, 32 },
    /* MEDALLION */ { 247, 139, 64, 32 },
    /*    WEAPON */ { 23, 170, 96, 128 },
    /*    SHIELD */ { 247, 171, 96, 128 },
    /*     ARMOR */ { 119, 171, 128, 160 },
    /*  GAUNTLET */ { 55, 107, 64, 64 },
    /*     BOOTS */ { 150, 331, 64, 64 },
};

// 0x5CACE8
static TigRect inven_ui_barter_pc_paperdoll_inv_slot_rects[9] = {
    /*    HELMET */ { 496, 107, 64, 64 },
    /*     RING1 */ { 592, 107, 32, 32 },
    /*     RING2 */ { 624, 107, 32, 32 },
    /* MEDALLION */ { 592, 139, 64, 32 },
    /*    WEAPON */ { 368, 170, 96, 128 },
    /*    SHIELD */ { 592, 171, 96, 128 },
    /*     ARMOR */ { 464, 171, 128, 160 },
    /*  GAUNTLET */ { 400, 107, 64, 64 },
    /*     BOOTS */ { 495, 331, 64, 64 },
};

// 0x5CAD78
static TigRect inven_ui_barter_npc_paperdoll_inv_slot_rects[9] = {
    /*    HELMET */ { 157, 182, 42, 42 },
    /*     RING1 */ { 220, 182, 21, 21 },
    /*     RING2 */ { 241, 182, 21, 21 },
    /* MEDALLION */ { 220, 203, 42, 21 },
    /*    WEAPON */ { 73, 223, 63, 85 },
    /*    SHIELD */ { 220, 224, 63, 85 },
    /*     ARMOR */ { 136, 224, 84, 106 },
    /*  GAUNTLET */ { 94, 182, 42, 42 },
    /*     BOOTS */ { 156, 330, 42, 42 },
};

// 0x5CAE08
static TigRect inven_ui_loot_npc_paperdoll_inv_slot_rects[9] = {
    /*    HELMET */ { 157, 182, 42, 42 },
    /*     RING1 */ { 220, 182, 21, 21 },
    /*     RING2 */ { 241, 182, 21, 21 },
    /* MEDALLION */ { 220, 203, 42, 21 },
    /*    WEAPON */ { 73, 223, 63, 85 },
    /*    SHIELD */ { 220, 224, 63, 85 },
    /*     ARMOR */ { 136, 224, 84, 106 },
    /*  GAUNTLET */ { 94, 182, 42, 42 },
    /*     BOOTS */ { 156, 330, 42, 42 },
};

// 0x5CAE98
static TigRect inven_ui_inventory_summary_rects[] = {
    /*      WEIGHT */ { 145, 29, 185, 15 },
    /* ENCUMBRANCE */ { 145, 44, 185, 15 },
    /*  INFO_SPEED */ { 145, 59, 185, 15 },
};

// 0x5CAEC8
static TigRect inven_ui_barter_pc_summary_rects[] = {
    /*      WEIGHT */ { 428, 32, 185, 15 },
    /* ENCUMBRANCE */ { 428, 47, 185, 15 },
    /*  INFO_SPEED */ { 428, 62, 185, 15 },
};

// 0x5CAEF8
static TigRect inven_ui_inventory_total_stat_rects[] = {
    /*  ATTACK */ { 80, 358, 23, 35 },
    /* DEFENSE */ { 261, 358, 23, 35 },
};

// 0x5CAF18
static TigRect inven_ui_barter_pc_total_stat_rects[] = {
    /*  ATTACK */ { 422, 358, 23, 35 },
    /* DEFENSE */ { 606, 358, 23, 35 },
};

// 0x5CAF38
static TigRect inven_ui_barter_npc_total_stat_rects[] = {
    /*  ATTACK */ { 105, 354, 22, 16 },
    /* DEFENSE */ { 227, 354, 22, 16 },
};

// 0x5CAF58
static int item_ui_item_silhouette_nums[] = {
    /*    HELMET */ 241, // cvr_helmet.art
    /*     RING1 */ 246, // cvr_ring1.art
    /*     RING2 */ 247, // cvr_ring2.art
    /* MEDALLION */ 245, // cvr_medalion.art
    /*    WEAPON */ 249, // cvr_weapon.art
    /*    SHIELD */ 248, // cvr_shield.art
    /*     ARMOR */ 244, // cvr_armor.art
    /*  GAUNTLET */ 243, // cvr_gauntlet.art
    /*     BOOTS */ 242, // cvr_boot.art
};

// 0x5CAF80
static TigRect inven_ui_loot_target_portrait_rect = { 183, 33, 100, 78 };

// 0x5CAF90
static TigRect inven_ui_barter_scrollbar_rect = { 330, 168, 17, 224 };

// 0x5CAFA0
static TigRect inven_ui_loot_scrollbar_rect = { 330, 136, 17, 256 };

// 0x5CAFB0
static TigRect inven_ui_npc_action_rect = { 180, 29, 155, 65 };

// 0x5CAFC0
static int inven_ui_gold_ammo_type_x[5] = {
    729,
    729,
    729,
    729,
    729,
};

// 0x5CAFD4
static int inven_ui_gold_ammo_type_y[5] = {
    70,
    93,
    116,
    139,
    162,
};

// 0x6810E0
static int64_t inven_ui_drag_item_obj;

// 0x6810E8
static int inven_ui_drag_original_location;

// 0x6810F0
static ScrollbarId inven_ui_target_inventory_scrollbar;
static ScrollbarId inven_ui_player_inventory_scrollbar;
static int inven_ui_player_scroll_offset;
static bool inven_ui_player_scrollbar_created;

// 0x6810F8
static mes_file_handle_t inven_ui_mes_file;

// 0x6810FC
static bool inven_ui_target_is_follower;

// 0x681100
static tig_button_handle_t inven_ui_cycle_left_btn;

// 0x681108
static TigRect inven_ui_gamble_box_frame;

// 0x681118
static bool inven_ui_have_use_box;

// 0x68111C
static int inven_ui_player_inventory_slots[INVENTORY_CRITTER_CELLS];

// 0x6812FC
static char inven_ui_transaction_object_id_text[128];

// 0x681390
static tig_font_handle_t inven_ui_combat_totals_font;

// 0x68137C
static tig_window_handle_t inven_ui_window_handle;

// 0x681380
static TigRect inven_ui_use_box_frame;

// 0x681394
static bool inven_ui_have_drop_box;

// 0x681398
static tig_button_handle_t inven_ui_cycle_right_btn;

// 0x68139C
static bool inven_ui_created;

// 0x6813A0
static tig_button_handle_t inven_ui_target_total_attack_btn;

// 0x6813A4
static int inven_ui_drop_box_image;

// 0x6813A8
static int64_t inven_ui_target_inventory_owner;

// 0x6813B0
static TigRect inven_ui_drop_box_frame;

// 0x6813C0
static char inven_ui_transaction_target_name[128];

// 0x681440
static int inven_ui_trade_price;

// 0x681444
static tig_button_handle_t inven_ui_arrange_items_btn;

// 0x681448
static bool inven_ui_have_gamble_box;

// 0x681450
static int64_t inven_ui_drag_original_owner;

// 0x681458
static int64_t inven_ui_quoted_item_obj;

// 0x681460
static tig_button_handle_t inven_ui_total_attack_value_btn;

// 0x681464
static tig_button_handle_t inven_ui_take_all_btn;

// 0x681468
static char inven_ui_transaction_source_name[128];

// 0x6814E8
static tig_button_handle_t inven_ui_total_defence_image_btn;

// 0x6814EC
static tig_button_handle_t inven_ui_inventory_btn;

// 0x6814F0
static bool inven_ui_arrange_vertical;

// 0x6814F8
static int64_t inven_ui_pc_obj;

// 0x681500
static tig_button_handle_t inven_ui_target_paperdoll_btn;

// 0x681504
static int inven_ui_use_box_image;

// 0x681508
static int inven_ui_target_scroll_offset;

// 0x68150C
static InvenUiPanel inven_ui_panel;

// 0x681510
static int inven_ui_target_max_scroll_offset;

// 0x681514
static InvenUiPanel inven_ui_target_panel;

// 0x681518
static int inven_ui_target_inventory_slots[960];

// 0x682418
static tig_font_handle_t inven_ui_description_font;

// 0x68241C
static char inven_ui_trade_dialog_text[1000];

// 0x682804
static char inven_ui_offer_text_buffer[1000];

// 0x682BEC
static char inven_ui_transaction_item_name[128];

// 0x682C6C
static tig_button_handle_t inven_ui_total_attack_image_btn;

// 0x682C70
static int inven_ui_gamble_box_image;

// 0x682C74
static tig_font_handle_t inven_ui_detail_font;

// 0x682C78
static int64_t inven_ui_target_obj;

// 0x682C80
static tig_button_handle_t inven_ui_paperdoll_btn;

// 0x682C84
static tig_button_handle_t inven_ui_target_total_defence_btn;

// 0x682C88
static tig_button_handle_t inven_ui_total_defence_value_btn;

// 0x682C8C
static char inven_ui_target_description[MAX_STRING];

// 0x68345C
static tig_font_handle_t inven_ui_resource_count_font;

// 0x683460
static tig_button_handle_t inven_ui_target_inventory_btn;

// 0x683464
static InvenUiMode inven_ui_mode;

// 0x68346C
static bool inven_ui_mouse_down_seen;

// 0x683470
static int inven_ui_drag_slot_offset;

// 0x739F58
static int inven_ui_close_requested;

// 0x739F60
static int inven_ui_transaction_flags;

// 0x739F68
static int64_t inven_ui_transaction_item_obj;

// 0x739F70
static int64_t inven_ui_transaction_source_obj;

// 0x739F78
static int64_t inven_ui_transaction_target_obj;

// 0x739F80
static int inven_ui_transaction_destination_slot;

// 0x739F84
static int inven_ui_transaction_price;

// 0x572060
bool inven_ui_init(GameInitInfo* init_info)
{
    const int interface_art_numbers[] = { 223, 235, 825, 234, 221, 340, 341, 337, 349, 338, 339, 676, 677, 757, 758, 808 };
    for (size_t i = 0; i < SDL_arraysize(interface_art_numbers); i++) ui_asset_warmup_register(interface_art_numbers[i]);
    for (size_t i = 0; i < SDL_arraysize(item_ui_item_silhouette_nums); i++) ui_asset_warmup_register(item_ui_item_silhouette_nums[i]);
    TigFont font;

    (void)init_info;

    if (!mes_load("mes\\inven_ui.mes", &inven_ui_mes_file)) {
        return false;
    }

    font.flags = 0;
    tig_art_interface_id_create(27, 0, 0, 0, &(font.art_id));
    font.str = NULL;
    font.color = tig_color_make(255, 255, 255);
    tig_font_create(&font, &inven_ui_description_font);

    tig_art_interface_id_create(229, 0, 0, 0, &(font.art_id));
    tig_font_create(&font, &inven_ui_detail_font);

    tig_art_interface_id_create(486, 0, 0, 0, &(font.art_id));
    tig_font_create(&font, &inven_ui_combat_totals_font);

    font.flags = TIG_FONT_CENTERED;
    tig_art_interface_id_create(171, 0, 0, 0, &(font.art_id));
    tig_font_create(&font, &inven_ui_resource_count_font);

    return true;
}

// 0x572190
void inven_ui_exit(void)
{
    tig_font_destroy(inven_ui_description_font);
    tig_font_destroy(inven_ui_detail_font);
    tig_font_destroy(inven_ui_combat_totals_font);
    tig_font_destroy(inven_ui_resource_count_font);
    mes_unload(inven_ui_mes_file);
}

// 0x5721D0
void inven_ui_reset(void)
{
    if (inven_ui_created) {
        inven_ui_destroy();
    }

    inven_ui_target_obj = 0;
    inven_ui_target_inventory_owner = 0;
    inven_ui_pc_obj = OBJ_HANDLE_NULL;
    inven_ui_drag_item_obj = OBJ_HANDLE_NULL;
    inven_ui_drag_original_owner = 0;
    inven_ui_quoted_item_obj = 0;
    inven_ui_drag_original_location = -1;
}

// 0x572240
bool inven_ui_open(int64_t pc_obj, int64_t target_obj, int mode)
{
    if (inven_ui_close_if_created()) {
        return true;
    }

    if (tig_net_is_active() && !tig_net_is_host()) {
        UiRequestPacket pkt;

        pkt.type = 100;
        pkt.subtype = 12;
        mp_object_id_get_nullable(pc_obj, &(pkt.d.z.pc_id));
        mp_object_id_get_nullable(target_obj, &(pkt.d.z.target_id));
        pkt.d.z.inventory_mode = mode;
        pkt.d.z.stage = 0;
        tig_net_send_app_all(&pkt, sizeof(pkt));

        return true;
    }

    if (!inven_ui_open_preconditions_check(pc_obj, target_obj, mode)) {
        return false;
    }

    if (!inven_ui_game_mode_select(pc_obj, target_obj, mode)) {
        return false;
    }

    inven_ui_target_inventory_prepare(pc_obj, target_obj, mode);
    return inven_ui_create(pc_obj, target_obj, mode);
}

// 0x572340
bool inven_ui_close_if_created(void)
{
    if (!inven_ui_created) {
        return false;
    }

    if (inven_ui_drag_item_obj != OBJ_HANDLE_NULL) {
        inven_ui_drag_cancel_and_restore();
    }

    inven_ui_destroy();

    return true;
}

// 0x572370
bool inven_ui_open_preconditions_check(int64_t pc_obj, int64_t target_obj, int mode)
{
    int rot;
    tig_art_id_t old_art_id;
    tig_art_id_t new_art_id;
    int err;
    int sound_id;
    MesFileEntry mes_file_entry;
    UiMessage ui_message;

    if (pc_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (critter_is_dead(pc_obj)) {
        return false;
    }

    if (obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return false;
    }

    if (mode == INVEN_UI_MODE_LOOT
        || mode == INVEN_UI_MODE_STEAL) {
        // FIX: To determine if a PC can see the target object, the PC is
        // instantly rotated toward the target before calling `ai_can_see`.
        // However, when the PC is walking or running along a plotted path
        // (expressed as a series of rotations), this instant rotation disrupts
        // the path, causing the PC to run straight to the target and ignore any
        // obstacles. The fix is straightforward: restore the previous rotation
        // if the PC cannot see the target.
        rot = object_rot(pc_obj, target_obj);
        old_art_id = obj_field_int32_get(pc_obj, OBJ_F_CURRENT_AID);
        new_art_id = tig_art_id_rotation_set(old_art_id, rot);
        object_set_current_aid(pc_obj, new_art_id);
        if (ai_can_see(pc_obj, target_obj) != 0) {
            object_set_current_aid(pc_obj, old_art_id);
            return false;
        }
    }

    if (target_obj != OBJ_HANDLE_NULL
        && (mode == INVEN_UI_MODE_LOOT
            || mode == INVEN_UI_MODE_IDENTIFY)) {
        if (obj_field_int32_get(target_obj, OBJ_F_TYPE) == OBJ_TYPE_CONTAINER) {
            if (mode == INVEN_UI_MODE_LOOT) {
                err = ai_attempt_open_container(pc_obj, target_obj);
                if (err != AI_ATTEMPT_OPEN_CONTAINER_OK) {
                    mes_file_entry.num = err;
                    mes_get_msg(inven_ui_mes_file, &mes_file_entry);

                    ui_message.type = UI_MSG_TYPE_EXCLAMATION;
                    ui_message.str = mes_file_entry.str;
                    intgame_message_window_display_msg(&ui_message);

                    sound_id = sfx_container_sound(target_obj, CONTAINER_SOUND_LOCKED);
                    gsound_play_sfx(sound_id, 1);
                    return false;
                }

                if (!object_script_execute(pc_obj, target_obj, OBJ_HANDLE_NULL, SAP_USE, 0)) {
                    sound_id = sfx_container_sound(target_obj, CONTAINER_SOUND_LOCKED);
                    gsound_play_sfx(sound_id, 1);
                    return false;
                }
            }
        } else {
            if (mode == INVEN_UI_MODE_LOOT) {
                if (!object_script_execute(pc_obj, target_obj, OBJ_HANDLE_NULL, SAP_USE, 0)) {
                    return false;
                }
            }
        }
    }

    return true;
}

// 0x572510
bool inven_ui_game_mode_select(int64_t pc_obj, int64_t target_obj, int mode)
{
    if (!intgame_mode_set(INTGAME_MODE_MAIN)) {
        return false;
    }

    switch (mode) {
    case INVEN_UI_MODE_INVENTORY:
        if (!intgame_mode_set(INTGAME_MODE_INVEN)) {
            return false;
        }
        return true;
    case INVEN_UI_MODE_BARTER:
        if (!intgame_mode_set(INTGAME_MODE_BARTER)) {
            return false;
        }
        if (pc_obj == target_obj) {
            return false;
        }
        return true;
    case INVEN_UI_MODE_LOOT:
    case INVEN_UI_MODE_IDENTIFY:
        if (!intgame_mode_set(INTGAME_MODE_LOOT)) {
            return false;
        }
        if (pc_obj == target_obj) {
            return false;
        }
        return true;
    case INVEN_UI_MODE_STEAL:
        if (!intgame_mode_set(INTGAME_MODE_STEAL)) {
            return false;
        }
        if (pc_obj == target_obj) {
            return false;
        }
        return true;
    case INVEN_UI_MODE_NPC_IDENTIFY:
        if (!intgame_mode_set(INTGAME_MODE_NPC_IDENTIFY)) {
            return false;
        }
        if (pc_obj == target_obj) {
            return false;
        }
        return true;
    case INVEN_UI_MODE_NPC_REPAIR:
        if (!intgame_mode_set(INTGAME_MODE_NPC_REPAIR)) {
            return false;
        }
        if (pc_obj == target_obj) {
            return false;
        }
        return true;
    default:
        return false;
    }
}

// 0x572640
void inven_ui_target_inventory_prepare(int64_t pc_obj, int64_t target_obj, int mode)
{
    int64_t substitute_inventory_obj;
    int64_t prototype_obj;
    int64_t gold_obj;
    int amt;
    int sound_id;

    if (target_obj == OBJ_HANDLE_NULL) {
        return;
    }

    if (mode == INVEN_UI_MODE_BARTER) {
        substitute_inventory_obj = critter_substitute_inventory_get(target_obj);
        if (substitute_inventory_obj != OBJ_HANDLE_NULL) {
            prototype_obj = proto_default_for_type_get(8);
            gold_obj = item_find_matching_prototype_filtered(substitute_inventory_obj, prototype_obj, 0);
            while (gold_obj != OBJ_HANDLE_NULL) {
                amt = item_gold_get(gold_obj);
                item_gold_transfer(substitute_inventory_obj, target_obj, amt, gold_obj);
                gold_obj = item_find_matching_prototype_filtered(substitute_inventory_obj, prototype_obj, 0);
            }
        }

        if (critter_pc_leader_get(target_obj) == OBJ_HANDLE_NULL) {
            item_identify_all(target_obj);
            item_identify_all(substitute_inventory_obj);
        }
    } else if (mode == INVEN_UI_MODE_LOOT || mode == INVEN_UI_MODE_IDENTIFY) {
        if (obj_field_int32_get(target_obj, OBJ_F_TYPE) == OBJ_TYPE_CONTAINER) {
            if (mode == INVEN_UI_MODE_LOOT) {
                if (tig_art_id_frame_get(obj_field_int32_get(target_obj, OBJ_F_CURRENT_AID)) == 0) {
                    object_inc_current_aid(target_obj);

                    sound_id = sfx_container_sound(target_obj, CONTAINER_SOUND_OPEN);
                    gsound_play_sfx(sound_id, 1);
                }
            }
            if ((obj_field_int32_get(target_obj, OBJ_F_CONTAINER_FLAGS) & OCOF_INVEN_SPAWN_ONCE) != 0) {
                item_inventory_source_spawn(target_obj);
            }
        } else {
            if (mode == INVEN_UI_MODE_LOOT) {
                gsound_play_sfx(SND_INTERFACE_ITEM_DROP, 1);
            }
        }
    }
}

// 0x5727B0
bool inven_ui_create(int64_t pc_obj, int64_t target_obj, int mode)
{
    TigRect rect;
    TigButtonData button_data;
    MesFileEntry mes_file_entry;
    UiMessage ui_message;
    PcLens pc_lens;
    tig_art_id_t art_id;
    tig_button_handle_t button_group[2];
    unsigned int critter_flags2;

    inven_ui_mode = mode;
    inven_ui_target_inventory_owner = target_obj;
    inven_ui_target_obj = target_obj;
    inven_ui_target_panel = INVEN_UI_PANEL_INVENTORY;
    inven_ui_panel = INVEN_UI_PANEL_INVENTORY;

    if (!intgame_big_window_lock(inven_ui_message_filter, &inven_ui_window_handle)) {
        intgame_mode_set(INTGAME_MODE_MAIN);
        return false;
    }

    // CE: opt the inventory family (inventory / paperdoll / loot /
    // barter — all use the shared big window) into the optional
    // near-black see-through alpha. Disabled on destroy so other big-
    // window users (worldmap, logbook, charedit, schematic) don't
    // inherit it.
    intgame_apply_translucent_black(inven_ui_window_handle, true);

    button_data.flags = TIG_BUTTON_MOMENTARY;
    button_data.window_handle = inven_ui_window_handle;
    tig_art_interface_id_create(340, 0, 0, 0, &(button_data.art_id));
    button_data.mouse_down_snd_id = SND_INTERFACE_BUTTON_MEDIUM;
    button_data.mouse_up_snd_id = SND_INTERFACE_BUTTON_MEDIUM_RELEASE;
    button_data.mouse_enter_snd_id = -1;
    button_data.mouse_exit_snd_id = -1;
    button_data.x = 763;
    button_data.y = 197;
    if (tig_button_create(&button_data, &inven_ui_arrange_items_btn) != TIG_OK) {
        intgame_big_window_unlock();
        intgame_mode_set(INTGAME_MODE_MAIN);
        return false;
    }

    inven_ui_paperdoll_btn = TIG_BUTTON_HANDLE_INVALID;
    inven_ui_inventory_btn = TIG_BUTTON_HANDLE_INVALID;
    inven_ui_target_paperdoll_btn = TIG_BUTTON_HANDLE_INVALID;
    inven_ui_target_inventory_btn = TIG_BUTTON_HANDLE_INVALID;
    inven_ui_total_attack_image_btn = TIG_BUTTON_HANDLE_INVALID;
    inven_ui_total_defence_image_btn = TIG_BUTTON_HANDLE_INVALID;
    inven_ui_total_attack_value_btn = TIG_BUTTON_HANDLE_INVALID;
    inven_ui_total_defence_value_btn = TIG_BUTTON_HANDLE_INVALID;
    inven_ui_target_total_attack_btn = TIG_BUTTON_HANDLE_INVALID;
    inven_ui_target_total_defence_btn = TIG_BUTTON_HANDLE_INVALID;
    inven_ui_take_all_btn = TIG_BUTTON_HANDLE_INVALID;
    inven_ui_cycle_left_btn = TIG_BUTTON_HANDLE_INVALID;
    inven_ui_cycle_right_btn = TIG_BUTTON_HANDLE_INVALID;

    if (inven_ui_mode == INVEN_UI_MODE_INVENTORY
        || inven_ui_mode == INVEN_UI_MODE_NPC_IDENTIFY
        || inven_ui_mode == INVEN_UI_MODE_NPC_REPAIR) {
        button_data.mouse_down_snd_id = -1;
        button_data.mouse_up_snd_id = -1;
        tig_art_interface_id_create(676, 0, 0, 0, &(button_data.art_id));
        button_data.x = 29;
        button_data.y = 318;
        if (tig_button_create(&button_data, &inven_ui_total_attack_image_btn) != TIG_OK) {
            intgame_big_window_unlock();
            intgame_mode_set(INTGAME_MODE_MAIN);
            return false;
        }

        tig_art_interface_id_create(677, 0, 0, 0, &(button_data.art_id));
        button_data.x = 254;
        button_data.y = 319;
        if (tig_button_create(&button_data, &inven_ui_total_defence_image_btn) != TIG_OK) {
            intgame_big_window_unlock();
            intgame_mode_set(INTGAME_MODE_MAIN);
            return false;
        }

        button_data.y = inven_ui_inventory_total_stat_rects[INVEN_UI_TOTAL_ATTACK].y;
        button_data.x = inven_ui_inventory_total_stat_rects[INVEN_UI_TOTAL_ATTACK].x;
        button_data.art_id = TIG_ART_ID_INVALID;
        button_data.width = inven_ui_inventory_total_stat_rects[INVEN_UI_TOTAL_ATTACK].width;
        button_data.height = inven_ui_inventory_total_stat_rects[INVEN_UI_TOTAL_ATTACK].height;
        if (tig_button_create(&button_data, &inven_ui_total_attack_value_btn)) {
            intgame_big_window_unlock();
            intgame_mode_set(INTGAME_MODE_MAIN);
            return false;
        }

        button_data.x = inven_ui_inventory_total_stat_rects[INVEN_UI_TOTAL_DEFENSE].x;
        button_data.y = inven_ui_inventory_total_stat_rects[INVEN_UI_TOTAL_DEFENSE].y;
        button_data.art_id = TIG_ART_ID_INVALID;
        button_data.width = inven_ui_inventory_total_stat_rects[INVEN_UI_TOTAL_DEFENSE].width;
        button_data.height = inven_ui_inventory_total_stat_rects[INVEN_UI_TOTAL_DEFENSE].height;
        if (tig_button_create(&button_data, &inven_ui_total_defence_value_btn) != TIG_OK) {
            intgame_big_window_unlock();
            intgame_mode_set(INTGAME_MODE_MAIN);
            return false;
        }

        button_data.mouse_down_snd_id = SND_INTERFACE_BUTTON_MEDIUM;
        button_data.mouse_up_snd_id = SND_INTERFACE_BUTTON_MEDIUM_RELEASE;
    } else {
        button_data.flags = TIG_BUTTON_HIDDEN;
        button_data.mouse_down_snd_id = -1;
        button_data.mouse_up_snd_id = -1;
        tig_art_interface_id_create(676, 0, 0, 0, &(button_data.art_id));
        button_data.x = 374;
        button_data.y = 318;
        if (tig_button_create(&button_data, &inven_ui_total_attack_image_btn) != TIG_OK) {
            intgame_big_window_unlock();
            intgame_mode_set(INTGAME_MODE_MAIN);
            return false;
        }

        tig_art_interface_id_create(677, 0, 0, 0, &(button_data.art_id));
        button_data.x = 599;
        button_data.y = 319;
        if (tig_button_create(&button_data, &inven_ui_total_defence_image_btn) != TIG_OK) {
            intgame_big_window_unlock();
            intgame_mode_set(INTGAME_MODE_MAIN);
            return false;
        }

        button_data.y = inven_ui_barter_pc_total_stat_rects[INVEN_UI_TOTAL_ATTACK].y;
        button_data.x = inven_ui_barter_pc_total_stat_rects[INVEN_UI_TOTAL_ATTACK].x;
        button_data.art_id = TIG_ART_ID_INVALID;
        button_data.width = inven_ui_barter_pc_total_stat_rects[INVEN_UI_TOTAL_ATTACK].width;
        button_data.height = inven_ui_barter_pc_total_stat_rects[INVEN_UI_TOTAL_ATTACK].height;
        if (tig_button_create(&button_data, &inven_ui_total_attack_value_btn) != TIG_OK) {
            intgame_big_window_unlock();
            intgame_mode_set(INTGAME_MODE_MAIN);
            return false;
        }

        button_data.x = inven_ui_barter_pc_total_stat_rects[INVEN_UI_TOTAL_DEFENSE].x;
        button_data.y = inven_ui_barter_pc_total_stat_rects[INVEN_UI_TOTAL_DEFENSE].y;
        button_data.art_id = TIG_ART_ID_INVALID;
        button_data.width = inven_ui_barter_pc_total_stat_rects[INVEN_UI_TOTAL_DEFENSE].width;
        button_data.height = inven_ui_barter_pc_total_stat_rects[INVEN_UI_TOTAL_DEFENSE].height;
        if (tig_button_create(&button_data, &inven_ui_total_defence_value_btn) != TIG_OK) {
            intgame_big_window_unlock();
            intgame_mode_set(INTGAME_MODE_MAIN);
            return false;
        }

        button_data.flags = TIG_BUTTON_MOMENTARY;
        button_data.mouse_down_snd_id = SND_INTERFACE_BUTTON_MEDIUM;
        button_data.mouse_up_snd_id = SND_INTERFACE_BUTTON_MEDIUM_RELEASE;
        if (inven_ui_mode == INVEN_UI_MODE_LOOT) {
            tig_art_interface_id_create(808, 0, 0, 0, &(button_data.art_id));
            button_data.x = 301;
            button_data.y = 47;
            if (tig_button_create(&button_data, &inven_ui_take_all_btn) != TIG_OK) {
                intgame_big_window_unlock();
                intgame_mode_set(INTGAME_MODE_MAIN);
                return false;
            }
        }

        button_data.flags = TIG_BUTTON_TOGGLE | TIG_BUTTON_LATCH;
        tig_art_interface_id_create(338, 0, 0, 0, &(button_data.art_id));
        button_data.x = 713;
        button_data.y = 37;
        if (tig_button_create(&button_data, &inven_ui_paperdoll_btn) != TIG_OK) {
            intgame_big_window_unlock();
            intgame_mode_set(INTGAME_MODE_MAIN);
            return false;
        }

        tig_art_interface_id_create(339, 0, 0, 0, &(button_data.art_id));
        button_data.x = 751;
        button_data.y = 37;
        if (tig_button_create(&button_data, &inven_ui_inventory_btn) != TIG_OK) {
            intgame_big_window_unlock();
            intgame_mode_set(INTGAME_MODE_MAIN);
            return false;
        }

        button_group[0] = inven_ui_inventory_btn;
        button_group[1] = inven_ui_paperdoll_btn;
        tig_button_radio_group_create(2, button_group, 0);

        if (obj_field_int32_get(inven_ui_target_inventory_owner, OBJ_F_TYPE) != OBJ_TYPE_CONTAINER) {
            button_data.flags = TIG_BUTTON_TOGGLE | TIG_BUTTON_LATCH;
            tig_art_interface_id_create(338, 0, 0, 0, &button_data.art_id);
            if (inven_ui_mode == INVEN_UI_MODE_BARTER) {
                button_data.x = 8;
                button_data.y = 123;
            } else {
                button_data.x = 23;
                button_data.y = 108;
            }
            if (tig_button_create(&button_data, &inven_ui_target_paperdoll_btn) != TIG_OK) {
                intgame_big_window_unlock();
                intgame_mode_set(INTGAME_MODE_MAIN);
                return false;
            }

            tig_art_interface_id_create(339, 0, 0, 0, &button_data.art_id);
            if (inven_ui_mode == INVEN_UI_MODE_BARTER) {
                button_data.x = 46;
                button_data.y = 123;
            } else {
                button_data.x = 74;
                button_data.y = 108;
            }
            if (tig_button_create(&button_data, &inven_ui_target_inventory_btn) != TIG_OK) {
                intgame_big_window_unlock();
                intgame_mode_set(INTGAME_MODE_MAIN);
                return false;
            }

            button_group[0] = inven_ui_target_inventory_btn;
            button_group[1] = inven_ui_target_paperdoll_btn;
            tig_button_radio_group_create(2, button_group, 0);

            button_data.flags = TIG_BUTTON_HIDDEN;
            button_data.mouse_down_snd_id = -1;
            button_data.mouse_up_snd_id = -1;
            button_data.art_id = TIG_ART_ID_INVALID;
            button_data.x = 75;
            button_data.y = 329;
            button_data.width = 55;
            button_data.height = 40;
            if (tig_button_create(&button_data, &inven_ui_target_total_attack_btn) != TIG_OK) {
                intgame_big_window_unlock();
                intgame_mode_set(INTGAME_MODE_MAIN);
                return false;
            }

            button_data.art_id = TIG_ART_ID_INVALID;
            button_data.x = 225;
            button_data.y = 329;
            button_data.width = 55;
            button_data.height = 40;
            if (tig_button_create(&button_data, &inven_ui_target_total_defence_btn) != TIG_OK) {
                intgame_big_window_unlock();
                intgame_mode_set(INTGAME_MODE_MAIN);
                return false;
            }
        }
    }

    inven_ui_pc_obj = pc_obj;
    item_inventory_slots_get(inven_ui_pc_obj, inven_ui_player_inventory_slots);

    if (inven_ui_mode == INVEN_UI_MODE_IDENTIFY
        || inven_ui_mode == INVEN_UI_MODE_NPC_IDENTIFY
        || inven_ui_mode == INVEN_UI_MODE_NPC_REPAIR) {
        inven_ui_have_drop_box = false;
        inven_ui_have_use_box = false;
    } else {
        inven_ui_have_drop_box = true;
        inven_ui_drop_box_image = 224;
        inven_ui_drop_box_frame.x = 703;
        inven_ui_drop_box_frame.y = 385;

        inven_ui_have_use_box = true;
        inven_ui_use_box_image = 342;
        inven_ui_use_box_frame.x = 703;
        inven_ui_use_box_frame.y = 331;
    }

    if (inven_ui_mode == INVEN_UI_MODE_BARTER) {
        inven_ui_have_gamble_box = true;
        inven_ui_gamble_box_image = 345;
        inven_ui_gamble_box_frame.x = 703;
        inven_ui_gamble_box_frame.y = 277;
    } else {
        inven_ui_have_gamble_box = false;
    }

    inven_ui_target_max_scroll_offset = 5;

    if (inven_ui_target_inventory_owner != OBJ_HANDLE_NULL) {
        object_examine(inven_ui_target_obj, inven_ui_pc_obj, inven_ui_target_description);
        inven_ui_target_max_scroll_offset = 4;
        if (inven_ui_mode == INVEN_UI_MODE_BARTER) {
            inven_ui_target_max_scroll_offset = 5;
            inven_ui_target_inventory_owner = critter_substitute_inventory_get(inven_ui_target_obj);
            if (inven_ui_target_inventory_owner != OBJ_HANDLE_NULL) {
                inven_ui_target_max_scroll_offset = 89;
            } else {
                inven_ui_target_inventory_owner = inven_ui_target_obj;
            }
        } else if ((inven_ui_mode == INVEN_UI_MODE_LOOT
                       || inven_ui_mode == INVEN_UI_MODE_IDENTIFY)
            && obj_field_int32_get(inven_ui_target_inventory_owner, OBJ_F_TYPE) == OBJ_TYPE_CONTAINER) {
            inven_ui_target_max_scroll_offset = 88;
            item_decay_process_disable();
        }

        inven_ui_target_max_scroll_offset =
            (obj_field_int32_get(inven_ui_target_inventory_owner, OBJ_F_TYPE) == OBJ_TYPE_CONTAINER
                ? INVENTORY_CONTAINER_ROWS : INVENTORY_CRITTER_ROWS)
            - (inven_ui_mode == INVEN_UI_MODE_BARTER ? 7 : 8);

        item_inventory_slots_get(inven_ui_target_inventory_owner, inven_ui_target_inventory_slots);
    }

    inven_ui_trade_dialog_text[0] = '\0';
    inven_ui_trade_price = -1;

    if (inven_ui_mode == INVEN_UI_MODE_BARTER
        && critter_leader_get(inven_ui_target_obj) == pc_obj) {
        inven_ui_target_is_follower = 1;
        if (critter_follower_next(inven_ui_target_obj) != inven_ui_target_obj) {
            button_data.flags = TIG_BUTTON_MOMENTARY;
            button_data.window_handle = inven_ui_window_handle;
            tig_art_interface_id_create(757, 0, 0, 0, &(button_data.art_id));
            button_data.x = 155;
            button_data.y = 55;
            if (tig_button_create(&button_data, &inven_ui_cycle_left_btn) != TIG_OK) {
                intgame_big_window_unlock();
                intgame_mode_set(INTGAME_MODE_MAIN);
                return false;
            }

            tig_art_interface_id_create(758, 0, 0, 0, &(button_data.art_id));
            button_data.x = 253;
            button_data.y = 55;
            if (tig_button_create(&button_data, &inven_ui_cycle_right_btn) != TIG_OK) {
                intgame_big_window_unlock();
                intgame_mode_set(INTGAME_MODE_MAIN);
                return false;
            }
        }
    } else {
        inven_ui_target_is_follower = 0;
    }

    inven_ui_target_scroll_offset = 0;
    inven_ui_player_scroll_offset = 0;
    inven_ui_quoted_item_obj = OBJ_HANDLE_NULL;

    multiplayer_inventory_viewer_set(inven_ui_pc_obj, inven_ui_target_inventory_owner);
    redraw_inven_fix_bad_inven_obj(inven_ui_pc_obj);
    redraw_inven_fix_bad_inven_obj(inven_ui_target_inventory_owner);

    if (!inven_ui_target_is_follower
        && inven_ui_target_inventory_owner != OBJ_HANDLE_NULL
        && obj_field_int32_get(inven_ui_target_inventory_owner, OBJ_F_TYPE) != OBJ_TYPE_PC
        && multiplayer_inventory_viewer_count(inven_ui_target_inventory_owner) <= 1) {
        item_arrange_inventory(inven_ui_target_inventory_owner, false);
        item_inventory_slots_get(inven_ui_target_inventory_owner, inven_ui_target_inventory_slots);
    }

    inven_ui_target_inventory_scrollbar_create();
    inven_ui_player_inventory_scrollbar_create();
    redraw_inven(true);
    inven_ui_drag_item_obj = OBJ_HANDLE_NULL;
    location_origin_set(obj_field_int64_get(inven_ui_pc_obj, OBJ_F_LOCATION));

    pc_lens.window_handle = inven_ui_window_handle;
    if (inven_ui_mode == INVEN_UI_MODE_INVENTORY
        || inven_ui_mode == INVEN_UI_MODE_NPC_IDENTIFY
        || inven_ui_mode == INVEN_UI_MODE_NPC_REPAIR) {
        rect.x = 11;
        rect.y = 9;
        art_id = 233;
    } else {
        rect.x = 16;
        rect.y = 17;
        art_id = inven_ui_mode == INVEN_UI_MODE_BARTER ? 231 : 232;
    }

    rect.width = 89;
    rect.height = 89;
    pc_lens.rect = &rect;
    tig_art_interface_id_create(art_id, 0, 0, 0, &(pc_lens.art_id));
    if (!mainmenu_ui_is_active()) {
        intgame_pc_lens_do(PC_LENS_MODE_PASSTHROUGH, &pc_lens);
    }

    inven_ui_close_requested = 0;

    if (player_is_local_pc_obj(pc_obj)) {
        ui_toggle_primary_button(UI_PRIMARY_BUTTON_INVENTORY, false);
    }

    if (inven_ui_target_inventory_owner != OBJ_HANDLE_NULL) {
        item_inventory_respawn_schedule(inven_ui_target_inventory_owner);
    }

    critter_flags2 = obj_field_int32_get(inven_ui_pc_obj, OBJ_F_CRITTER_FLAGS2);
    if ((critter_flags2 & OCF2_ITEM_STOLEN) != 0) {
        critter_flags2 &= ~OCF2_ITEM_STOLEN;
        obj_field_int32_set(inven_ui_pc_obj, OBJ_F_CRITTER_FLAGS2, critter_flags2);

        mes_file_entry.num = 7;
        mes_get_msg(inven_ui_mes_file, &mes_file_entry);

        ui_message.type = UI_MSG_TYPE_EXCLAMATION;
        ui_message.str = mes_file_entry.str;
        intgame_message_window_display_msg(&ui_message);
    }

    inven_ui_arrange_vertical = false;
    inven_ui_created = true;

    return true;
}

// 0x573440
void inven_ui_destroy(void)
{
    tig_art_id_t art_id;
    int sound_id;
    IntgameMode mode;

    if (!inven_ui_created) {
        return;
    }

    inven_ui_created = false;
    multiplayer_inventory_viewer_set(inven_ui_pc_obj, 0);

    if ((inven_ui_mode == INVEN_UI_MODE_LOOT
            || inven_ui_mode == INVEN_UI_MODE_IDENTIFY)
        && obj_field_int32_get(inven_ui_target_inventory_owner, OBJ_F_TYPE) == OBJ_TYPE_CONTAINER) {
        if (inven_ui_mode == INVEN_UI_MODE_LOOT) {
            art_id = obj_field_int32_get(inven_ui_target_inventory_owner, OBJ_F_CURRENT_AID);
            if (tig_art_id_frame_get(art_id) == 1
                && (obj_field_int32_get(inven_ui_target_inventory_owner, OBJ_F_CONTAINER_FLAGS) & OCOF_STICKY) == 0) {
                object_dec_current_aid(inven_ui_target_inventory_owner);

                sound_id = sfx_container_sound(inven_ui_target_inventory_owner, CONTAINER_SOUND_CLOSE);
                gsound_play_sfx(sound_id, 1);
            }
        }
        item_decay_process_enable();
        mp_junk_pile_cleanup_request(inven_ui_target_inventory_owner);
    }

    intgame_pc_lens_do(PC_LENS_MODE_NONE, NULL);
    if (inven_ui_player_scrollbar_created) {
        scrollbar_ui_control_destroy(inven_ui_player_inventory_scrollbar);
        inven_ui_player_scrollbar_created = false;
    }
    inven_ui_target_inventory_scrollbar_destroy();
    // CE: drop the near-black alpha before unlocking so the next
    // big-window user (e.g. worldmap) starts opaque.
    intgame_apply_translucent_black(inven_ui_window_handle, false);
    intgame_big_window_unlock();
    iso_interface_refresh();
    mode = intgame_mode_get();
    if (mode != INTGAME_MODE_INVEN
        && mode != INTGAME_MODE_LOOT
        && mode != INTGAME_MODE_STEAL
        && mode != INTGAME_MODE_BARTER
        && mode != INTGAME_MODE_NPC_IDENTIFY
        && mode != INTGAME_MODE_NPC_REPAIR) {
        intgame_mode_set(INTGAME_MODE_MAIN);
    }
    intgame_mode_set(INTGAME_MODE_MAIN);
}

// 0x573590
void inven_ui_notify_object_destroyed(int64_t obj)
{
    if (!inven_ui_created) {
        return;
    }

    if (obj == inven_ui_pc_obj
        || obj == inven_ui_target_inventory_owner
        || obj == inven_ui_target_obj) {
        inven_ui_destroy();
    }
}

// 0x5735F0
int inven_ui_is_created(void)
{
    return inven_ui_created;
}

// Personal items use their owner's equipment; merchant stock uses the buyer's.
int64_t inven_ui_comparison_owner(int64_t item_obj)
{
    if (!inven_ui_created) return OBJ_HANDLE_NULL;
    return item_compare_inventory_owner(item_obj, inven_ui_pc_obj,
        inven_ui_target_obj, inven_ui_target_inventory_owner, inven_ui_mode == INVEN_UI_MODE_BARTER);
}

// 0x573600
int64_t inven_ui_target_object_get(void)
{
    return inven_ui_created ? inven_ui_target_obj : OBJ_HANDLE_NULL;
}

// 0x573620
int64_t inven_ui_drag_item_obj_get(void)
{
    return inven_ui_drag_item_obj;
}

// 0x573630
void inven_ui_item_drag_begin(int64_t obj)
{
    tig_art_id_t inv_art_id;
    tig_art_id_t mouse_art_id;

    if (!inven_ui_created && inven_ui_pc_obj == OBJ_HANDLE_NULL) {
        inven_ui_pc_obj = player_get_local_pc_obj();
    }

    inven_ui_drag_original_owner = inven_ui_pc_obj;
    inven_ui_drag_item_obj = obj;
    inven_ui_drag_original_location = obj_field_int32_get(obj, OBJ_F_ITEM_INV_LOCATION);

    inv_art_id = obj_field_int32_get(inven_ui_drag_item_obj, OBJ_F_ITEM_INV_AID);
    intgame_refresh_cursor();
    mouse_art_id = tig_mouse_cursor_get_art_id();
    tig_mouse_hide();
    tig_mouse_cursor_set_art_id(inv_art_id);
    tig_mouse_cursor_overlay(mouse_art_id, 2, 2);
    tig_mouse_cursor_set_offset(2, 2);
    tig_mouse_show();
}

// 0x5736E0
void inven_ui_drag_cursor_refresh(void)
{
    tig_art_id_t inv_art_id;
    tig_art_id_t mouse_art_id;

    if (inven_ui_drag_item_obj == OBJ_HANDLE_NULL) {
        return;
    }

    inv_art_id = obj_field_int32_get(inven_ui_drag_item_obj, OBJ_F_ITEM_INV_AID);
    mouse_art_id = tig_mouse_cursor_get_art_id();
    tig_mouse_hide();
    tig_mouse_cursor_set_art_id(inv_art_id);
    tig_mouse_cursor_overlay(mouse_art_id, 2, 2);
    tig_mouse_cursor_set_offset(2, 2);
    tig_mouse_show();
}

// 0x573730
void inven_ui_mouse_down_clear(void)
{
    inven_ui_mouse_down_seen = false;
}

// 0x573740
void inven_ui_item_cursor_begin(int64_t item_obj, bool prompt_for_quantity)
{
    tig_art_id_t mouse_art_id;
    tig_art_id_t inv_art_id;

    if (!inven_ui_created && inven_ui_pc_obj == OBJ_HANDLE_NULL) {
        inven_ui_pc_obj = player_get_local_pc_obj();
    }

    mouse_art_id = tig_mouse_cursor_get_art_id();
    inven_ui_drag_original_owner = inven_ui_pc_obj;
    inven_ui_drag_item_obj = item_obj;
    inven_ui_drag_original_location = obj_field_int32_get(item_obj, OBJ_F_ITEM_INV_LOCATION);

    if (!inven_ui_created) {
        inven_ui_drag_item_insert_or_transfer(inven_ui_drag_item_obj, OBJ_HANDLE_NULL, -1, prompt_for_quantity ? 4 : 36);
        if (inven_ui_drag_item_obj == OBJ_HANDLE_NULL) {
            return;
        }
    }

    inv_art_id = obj_field_int32_get(inven_ui_drag_item_obj, OBJ_F_ITEM_INV_AID);
    tig_mouse_hide();
    tig_mouse_cursor_set_art_id(inv_art_id);
    tig_mouse_cursor_overlay(mouse_art_id, 2, 2);
    tig_mouse_cursor_set_offset(2, 2);
    tig_mouse_show();
}

// 0x573840
void inven_ui_drag_object_clear(void)
{
    inven_ui_drag_item_obj = OBJ_HANDLE_NULL;
}

// 0x573897
static inline bool inven_ui_message_filter_handle_button_pressed(tig_button_handle_t button_handle)
{
    if (button_handle == inven_ui_inventory_btn) {
        inven_ui_panel = INVEN_UI_PANEL_INVENTORY;
        tig_button_hide(inven_ui_total_attack_image_btn);
        tig_button_hide(inven_ui_total_defence_image_btn);
        tig_button_hide(inven_ui_total_attack_value_btn);
        tig_button_hide(inven_ui_total_defence_value_btn);
        redraw_inven(false);
        return true;
    }

    if (button_handle == inven_ui_paperdoll_btn) {
        inven_ui_panel = INVEN_UI_PANEL_PAPERDOLL;
        tig_button_show(inven_ui_total_attack_image_btn);
        tig_button_show(inven_ui_total_defence_image_btn);
        tig_button_show(inven_ui_total_attack_value_btn);
        tig_button_show(inven_ui_total_defence_value_btn);
        redraw_inven(false);
        return true;
    }

    if (button_handle == inven_ui_target_inventory_btn) {
        inven_ui_target_panel = INVEN_UI_PANEL_INVENTORY;
        inven_ui_target_inventory_scrollbar_show();
        tig_button_hide(inven_ui_target_total_attack_btn);
        tig_button_hide(inven_ui_target_total_defence_btn);
        redraw_inven(false);
        redraw_inven(false);
        return true;
    }

    if (button_handle == inven_ui_target_paperdoll_btn) {
        inven_ui_target_panel = INVEN_UI_PANEL_PAPERDOLL;
        inven_ui_target_inventory_scrollbar_hide();
        tig_button_show(inven_ui_target_total_attack_btn);
        tig_button_show(inven_ui_target_total_defence_btn);
        redraw_inven(false);
        return true;
    }

    if (button_handle == inven_ui_total_attack_image_btn
        || button_handle == inven_ui_total_attack_value_btn) {
        iso_interface_window_set(ROTWIN_TYPE_MSG);
        intgame_message_window_display_attack(inven_ui_pc_obj);
        return true;
    }

    if (button_handle == inven_ui_total_defence_image_btn
        || button_handle == inven_ui_total_defence_value_btn) {
        iso_interface_window_set(ROTWIN_TYPE_MSG);
        intgame_message_window_display_defense(inven_ui_pc_obj);
        return true;
    }

    if (button_handle == inven_ui_target_total_attack_btn) {
        iso_interface_window_set(ROTWIN_TYPE_MSG);
        intgame_message_window_display_attack(inven_ui_target_obj);
        return true;
    }

    if (button_handle == inven_ui_target_total_defence_btn) {
        iso_interface_window_set(ROTWIN_TYPE_MSG);
        intgame_message_window_display_defense(inven_ui_target_obj);
        return true;
    }

    return false;
}

// 0x573A95
static inline bool inven_ui_message_filter_handle_button_released(tig_button_handle_t button_handle)
{
    if (button_handle == inven_ui_arrange_items_btn) {
        item_arrange_inventory(inven_ui_pc_obj, inven_ui_arrange_vertical);
        item_inventory_slots_get(inven_ui_pc_obj, inven_ui_player_inventory_slots);
        if (inven_ui_target_is_follower) {
            item_arrange_inventory(inven_ui_target_inventory_owner, inven_ui_arrange_vertical);
            item_inventory_slots_get(inven_ui_target_inventory_owner, inven_ui_target_inventory_slots);
        }
        inven_ui_arrange_vertical = !inven_ui_arrange_vertical;
        redraw_inven(false);
        return true;
    }

    if (button_handle == inven_ui_take_all_btn) {
        inven_ui_transfer_visible_items(inven_ui_target_inventory_owner, inven_ui_pc_obj);
        return true;
    }

    if (button_handle == inven_ui_cycle_left_btn) {
        inven_ui_follower_cycle(false);
        return true;
    }

    if (button_handle == inven_ui_cycle_right_btn) {
        inven_ui_follower_cycle(true);
        return true;
    }

    return false;
}

// 0x573BAF
static inline bool inven_ui_mssage_filter_handle_button_hovered(tig_button_handle_t button_handle)
{
    if (button_handle == inven_ui_total_attack_image_btn || button_handle == inven_ui_total_attack_value_btn) {
        intgame_message_window_display_attack(inven_ui_pc_obj);
        return true;
    }

    if (button_handle == inven_ui_total_defence_image_btn || button_handle == inven_ui_total_defence_value_btn) {
        intgame_message_window_display_defense(inven_ui_pc_obj);
        return true;
    }

    if (button_handle == inven_ui_target_total_attack_btn) {
        intgame_message_window_display_attack(inven_ui_target_obj);
        return true;
    }

    if (button_handle == inven_ui_target_total_defence_btn) {
        intgame_message_window_display_defense(inven_ui_target_obj);
        return true;
    }

    return false;
}

// 0x573C80
static inline void inven_ui_message_filter_handle_mouse_idle(int x, int y)
{
    int64_t hovered_item_obj;
    int64_t owner_obj;

    if (inven_ui_drag_item_obj == OBJ_HANDLE_NULL) {
        hovered_item_obj = inven_ui_item_at_xy(x, y, &owner_obj);
        if (hovered_item_obj != inven_ui_quoted_item_obj) {
            inven_ui_quoted_item_obj = hovered_item_obj;
            if (inven_ui_quoted_item_obj != OBJ_HANDLE_NULL) {
                tb_ui_examine_and_select_object(inven_ui_pc_obj, inven_ui_quoted_item_obj);
                switch (inven_ui_mode) {
                case INVEN_UI_MODE_BARTER:
                    inven_ui_trade_offer_refresh(inven_ui_quoted_item_obj, owner_obj);
                    break;
                case INVEN_UI_MODE_NPC_IDENTIFY:
                    inven_ui_identification_quote_show(inven_ui_quoted_item_obj);
                    break;
                case INVEN_UI_MODE_NPC_REPAIR:
                    inven_ui_repair_offer_refresh(inven_ui_quoted_item_obj);
                    break;
                default:
                    redraw_inven(false);
                    break;
                }
            } else {
                inven_ui_trade_dialog_text[0] = '\0';
                inven_ui_trade_price = -1;
                redraw_inven(false);
            }
        }
    }
}

// 0x573D61
static inline bool inven_ui_message_filter_handle_mouse_move(int x, int y)
{
    if (inven_ui_drag_item_obj != OBJ_HANDLE_NULL) {
        if (inven_ui_have_drop_box) {
            if (x >= inven_ui_drop_box_frame.x
                && x < inven_ui_drop_box_frame.x + inven_ui_drop_box_frame.width
                && y >= inven_ui_drop_box_frame.y
                && y < inven_ui_drop_box_frame.y + inven_ui_drop_box_frame.height) {
                if (inven_ui_drop_box_image == 224) {
                    if (inven_ui_drag_original_owner == inven_ui_target_inventory_owner) {
                        switch (inven_ui_mode) {
                        case INVEN_UI_MODE_BARTER:
                            if (inven_ui_target_is_follower) {
                                inven_ui_drop_box_image = 225;
                            } else {
                                inven_ui_drop_box_image = 348;
                            }
                            break;
                        case INVEN_UI_MODE_LOOT:
                        case INVEN_UI_MODE_STEAL:
                            inven_ui_drop_box_image = 348;
                            break;
                        default:
                            inven_ui_drop_box_image = 225;
                            break;
                        }
                    } else {
                        inven_ui_drop_box_image = 225;
                    }

                    redraw_inven(false);
                    return true;
                }
            } else {
                if (inven_ui_drop_box_image != 224) {
                    inven_ui_drop_box_image = 224;
                    redraw_inven(false);
                    return true;
                }
            }
        }

        if (inven_ui_have_gamble_box) {
            if (x >= inven_ui_gamble_box_frame.x
                && x < inven_ui_gamble_box_frame.x + inven_ui_gamble_box_frame.width
                && y >= inven_ui_gamble_box_frame.y
                && y < inven_ui_gamble_box_frame.y + inven_ui_gamble_box_frame.height) {
                if (inven_ui_gamble_box_image == 345) {
                    inven_ui_gamble_box_image = inven_ui_gambling_offer_check(inven_ui_drag_item_obj, 0) ? 344 : 346;
                    redraw_inven(false);
                    return true;
                }
            } else {
                if (inven_ui_gamble_box_image != 345) {
                    inven_ui_gamble_box_image = 345;
                    redraw_inven(false);
                    return true;
                }
            }
        }

        if (inven_ui_have_use_box) {
            if (x >= inven_ui_use_box_frame.x
                && x < inven_ui_use_box_frame.x + inven_ui_use_box_frame.width
                && y >= inven_ui_use_box_frame.y
                && y < inven_ui_use_box_frame.y + inven_ui_use_box_frame.height) {
                if (inven_ui_use_box_image == 342) {
                    if ((inven_ui_drag_original_owner == inven_ui_target_inventory_owner
                            && (inven_ui_mode == INVEN_UI_MODE_BARTER
                                || inven_ui_mode == INVEN_UI_MODE_LOOT
                                || inven_ui_mode == INVEN_UI_MODE_STEAL))
                        || item_check_use_box_flag(inven_ui_pc_obj, inven_ui_drag_item_obj)) {
                        inven_ui_use_box_image = 347;
                    } else {
                        inven_ui_use_box_image = 343;
                    }
                    redraw_inven(false);
                    return true;
                }
            } else {
                if (inven_ui_use_box_image != 342) {
                    inven_ui_use_box_image = 342;
                    redraw_inven(false);
                    return true;
                }
            }
        }
    }

    return false;
}

// 0x574080
static inline bool inven_ui_message_filter_handle_mouse_lbutton_down(int x, int y)
{
    int64_t clicked_item_obj;
    int64_t owner_obj;
    TargetDescriptor td;

    switch (intgame_mode_get()) {
    case INTGAME_MODE_INVEN:
    case INTGAME_MODE_LOOT:
    case INTGAME_MODE_STEAL:
    case INTGAME_MODE_BARTER:
        inven_ui_mouse_down_seen = true;
        if (inven_ui_drag_item_obj == OBJ_HANDLE_NULL) {
            inven_ui_drag_item_obj = inven_ui_item_at_xy(x, y, &inven_ui_drag_original_owner);
            if (inven_ui_drag_item_obj != OBJ_HANDLE_NULL) {
                inven_ui_drag_slot_offset = inven_ui_inventory_slot_at_xy(x, y, &owner_obj);
                inven_ui_drag_original_location = item_inventory_location_get(inven_ui_drag_item_obj);
                inven_ui_drag_slot_offset -= inven_ui_drag_original_location;
                inven_ui_deferred_drag_request(x, y);
            }
            return true;
        }
        break;
    case INTGAME_MODE_SKILL:
        clicked_item_obj = inven_ui_item_at_xy(x, y, &inven_ui_drag_original_owner);
        if (clicked_item_obj != OBJ_HANDLE_NULL) {
            target_descriptor_set_obj(&td, clicked_item_obj);
            skill_ui_apply(&td);
            skill_ui_cancel();
            return true;
        }
        break;
    case INTGAME_MODE_SPELL:
        clicked_item_obj = inven_ui_item_at_xy(x, y, &inven_ui_drag_original_owner);
        if (clicked_item_obj != OBJ_HANDLE_NULL) {
            target_descriptor_set_obj(&td, clicked_item_obj);
            spell_ui_apply(&td);
            return true;
        }
        break;
    case 16:
        clicked_item_obj = inven_ui_item_at_xy(x, y, &inven_ui_drag_original_owner);
        if (clicked_item_obj != OBJ_HANDLE_NULL) {
            target_descriptor_set_obj(&td, clicked_item_obj);
            item_ui_apply(&td);
            return true;
        }
        break;
    default:
        break;
    }

    return false;
}

// 0x5747E4
static inline bool inven_ui_message_filter_handle_mouse_lbutton_up_accept_drop(TigMessage* msg, bool* close_requested_ptr)
{
    int64_t dragged_item_obj;
    int64_t owner_obj;
    int inventory_location;
    int new_inventory_location;
    int err;
    int64_t old_item_obj;
    int returned_item_slot;

    if (inven_ui_have_drop_box
        && msg->data.mouse.x >= inven_ui_drop_box_frame.x
        && msg->data.mouse.x < inven_ui_drop_box_frame.x + inven_ui_drop_box_frame.width
        && msg->data.mouse.y >= inven_ui_drop_box_frame.y
        && msg->data.mouse.y < inven_ui_drop_box_frame.y + inven_ui_drop_box_frame.height) {
        if (inven_ui_drag_original_owner == inven_ui_target_inventory_owner) {
            switch (inven_ui_mode) {
            case INVEN_UI_MODE_BARTER:
                if (inven_ui_target_is_follower) {
                    inven_ui_drag_item_insert_or_transfer(inven_ui_drag_item_obj, OBJ_HANDLE_NULL, -1, 2);
                } else {
                    inven_ui_drag_cancel_and_restore();
                }
                break;
            case INVEN_UI_MODE_LOOT:
            case INVEN_UI_MODE_STEAL:
                inven_ui_drag_cancel_and_restore();
                break;
            default:
                inven_ui_drag_item_insert_or_transfer(inven_ui_drag_item_obj, OBJ_HANDLE_NULL, -1, 2);
                break;
            }
        } else {
            inven_ui_drag_item_insert_or_transfer(inven_ui_drag_item_obj, OBJ_HANDLE_NULL, -1, 2);
        }

        inven_ui_drag_item_obj = OBJ_HANDLE_NULL;
        inven_ui_drop_box_image = 224;

        return false;
    }

    if (inven_ui_have_gamble_box
        && msg->data.mouse.x >= inven_ui_gamble_box_frame.x
        && msg->data.mouse.x < inven_ui_gamble_box_frame.x + inven_ui_gamble_box_frame.width
        && msg->data.mouse.y >= inven_ui_gamble_box_frame.y
        && msg->data.mouse.y < inven_ui_gamble_box_frame.y + inven_ui_gamble_box_frame.height) {
        dragged_item_obj = inven_ui_drag_item_obj;
        inven_ui_drag_cancel_and_restore();
        inven_ui_item_gamble(dragged_item_obj);
        inven_ui_drag_item_obj = OBJ_HANDLE_NULL;
        inven_ui_gamble_box_image = 345;

        return false;
    }

    if (inven_ui_have_use_box
        && msg->data.mouse.x >= inven_ui_use_box_frame.x
        && msg->data.mouse.x < inven_ui_use_box_frame.x + inven_ui_use_box_frame.width
        && msg->data.mouse.y >= inven_ui_use_box_frame.y
        && msg->data.mouse.y < inven_ui_use_box_frame.y + inven_ui_use_box_frame.height) {
        if ((inven_ui_drag_original_owner == inven_ui_target_inventory_owner
                && (inven_ui_mode == INVEN_UI_MODE_BARTER
                    || inven_ui_mode == INVEN_UI_MODE_LOOT
                    || inven_ui_mode == INVEN_UI_MODE_STEAL))
            || item_check_use_box_flag(inven_ui_pc_obj, inven_ui_drag_item_obj)) {
            inven_ui_drag_cancel_and_restore();
            inven_ui_drag_item_obj = OBJ_HANDLE_NULL;
            inven_ui_use_box_image = 342;

            return false;
        }

        if ((obj_field_int32_get(inven_ui_drag_item_obj, OBJ_F_ITEM_FLAGS) & OIF_USE_IS_THROW) == 0
            || inven_ui_mode != INVEN_UI_MODE_INVENTORY) {
            dragged_item_obj = inven_ui_drag_item_obj;
            inven_ui_drag_cancel_and_restore();
            if (!combat_turn_based_is_active()
                || combat_turn_based_whos_turn_get() == inven_ui_pc_obj) {
                if (tig_kb_get_modifier(SDL_KMOD_SHIFT)) {
                    item_use_on_obj(inven_ui_pc_obj, dragged_item_obj, inven_ui_pc_obj);
                } else {
                    item_use_on_obj(inven_ui_pc_obj, dragged_item_obj, OBJ_HANDLE_NULL);
                }
            }

            if (!inven_ui_created) {
                return true;
            }

            inven_ui_drag_item_obj = OBJ_HANDLE_NULL;
            inven_ui_use_box_image = 342;
        } else {
            inven_ui_drag_item_insert_or_transfer(inven_ui_drag_item_obj, OBJ_HANDLE_NULL, -1, 4);
            redraw_inven(false);
            item_inventory_slots_get(inven_ui_pc_obj, inven_ui_player_inventory_slots);
            if (inven_ui_target_inventory_owner != OBJ_HANDLE_NULL) {
                item_inventory_slots_get(inven_ui_target_inventory_owner, inven_ui_target_inventory_slots);
            }
        }

        return false;
    }

    inventory_location = inven_ui_inventory_slot_at_xy(msg->data.mouse.x, msg->data.mouse.y, &owner_obj);
    if (IS_HOTKEY_INV_LOC(inventory_location)) {
        return false;
    }

    if (IS_WEAR_INV_LOC(inventory_location)) {
        if (inven_ui_mode == INVEN_UI_MODE_BARTER) {
            if (inven_ui_drag_original_owner != inven_ui_pc_obj && owner_obj != inven_ui_pc_obj && !inven_ui_target_is_follower) {
                inven_ui_drag_cancel_and_restore();
                return false;
            }
        } else {
            if (inven_ui_mode == INVEN_UI_MODE_STEAL && inven_ui_drag_original_owner != inven_ui_pc_obj && owner_obj != inven_ui_pc_obj) {
                inven_ui_drag_cancel_and_restore();
                return false;
            }
        }

        err = item_equipment_slot_check(inven_ui_drag_item_obj, inventory_location, owner_obj);
        if (err != 0) {
            item_error_msg(owner_obj, err);
            inven_ui_drag_cancel_and_restore();
        }

        old_item_obj = item_wield_get(owner_obj, inventory_location);
        if (old_item_obj == OBJ_HANDLE_NULL || old_item_obj == inven_ui_drag_item_obj) {
            if (owner_obj == inven_ui_pc_obj) {
                if (!inven_ui_dragged_item_steal(close_requested_ptr)) {
                    inven_ui_drop_on_player_inventory(inventory_location);
                }
            } else {
                if (!inven_ui_dragged_item_plant(close_requested_ptr)) {
                    inven_ui_drop_on_target_inventory(inventory_location);
                }
            }

            inven_ui_drag_item_obj = OBJ_HANDLE_NULL;
            return false;
        }

        if (owner_obj != inven_ui_pc_obj) {
            inven_ui_drag_cancel_and_restore();
            return false;
        }

        err = item_check_remove(old_item_obj);
        if (err != ITEM_CANNOT_OK) {
            if (owner_obj == inven_ui_pc_obj) {
                item_error_msg(owner_obj, err);
            }
            inven_ui_drag_cancel_and_restore();
            return false;
        }

        if ((inven_ui_mode == INVEN_UI_MODE_BARTER
                || inven_ui_mode == INVEN_UI_MODE_STEAL)
            && inven_ui_drag_original_owner != owner_obj) {
            inven_ui_drag_cancel_and_restore();
            return false;
        }

        critter_encumbrance_recalc_feedback_disable();
        item_remove(inven_ui_drag_item_obj);
        new_inventory_location = item_inventory_location_get(old_item_obj);
        item_remove(old_item_obj);
        err = item_check_insert(old_item_obj, owner_obj, &returned_item_slot);
        if (err == ITEM_CANNOT_OK) {
            item_insert(inven_ui_drag_item_obj, inven_ui_drag_original_owner, inven_ui_drag_original_location);
            inven_ui_drag_item_insert_or_transfer(inven_ui_drag_item_obj, owner_obj, inventory_location, 1);
            item_insert(old_item_obj, owner_obj, returned_item_slot);
            inven_ui_drag_item_obj = old_item_obj;
            inven_ui_drag_original_location = returned_item_slot;
            inven_ui_drag_original_owner = owner_obj;
            inven_ui_drag_slot_offset = 0;
            inven_ui_deferred_drag_request(-1, -1);
        } else {
            item_error_msg(owner_obj, err);
            item_insert(old_item_obj, owner_obj, new_inventory_location);
            inven_ui_drag_item_restore_or_drop();
        }
        critter_encumbrance_recalc_feedback_enable();

        return false;
    }

    if (inventory_location - inven_ui_drag_slot_offset < 0) {
        if (!hotkey_ui_inventory_drop_process(TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP)) {
            inven_ui_drag_cancel_and_restore();
        }
    } else if (owner_obj == inven_ui_pc_obj) {
        if (!inven_ui_dragged_item_steal(close_requested_ptr)) {
            inven_ui_drop_on_player_inventory(inventory_location - inven_ui_drag_slot_offset);
        }
    } else {
        if (inven_ui_dragged_item_plant(close_requested_ptr)) {
            inven_ui_drop_on_target_inventory(inventory_location - inven_ui_drag_slot_offset);
        }
    }

    hotkey_ui_dragged_items_reset();

    inven_ui_drag_item_obj = OBJ_HANDLE_NULL;
    return false;
}

// 0x574559
static inline bool inven_ui_message_filter_handle_mouse_lbutton_up(int x, int y, bool* close_requested_ptr)
{
    int64_t clicked_item_obj;
    int64_t owner_obj;
    // Capture whether *this* inven_ui session received the matching
    // mouse-down before we clear the flag. Used below to guard the
    // dismiss-on-click paths against a stray mouse-up whose down came
    // from a different window — most importantly the dialog "Trade"
    // option that opens BARTER: dialog handles the down, barter handles
    // the up, and without this guard the up would immediately close the
    // just-opened barter screen.
    bool inven_saw_down = (inven_ui_mouse_down_seen != 0);
    inven_ui_mouse_down_seen = 0;
    if (intgame_panel_close_lens_check_pt_unscale(x, y)) {
        if (!mainmenu_ui_is_active()) {
            if (inven_ui_drag_item_obj != OBJ_HANDLE_NULL) {
                if (inven_ui_mode == INVEN_UI_MODE_INVENTORY) {
                    inven_ui_drag_item_insert_or_transfer(inven_ui_drag_item_obj, OBJ_HANDLE_NULL, -1, 4);
                    redraw_inven(false);
                    item_inventory_slots_get(inven_ui_pc_obj, inven_ui_player_inventory_slots);
                    if (inven_ui_target_inventory_owner) {
                        item_inventory_slots_get(inven_ui_target_inventory_owner, inven_ui_target_inventory_slots);
                    }
                    return false;
                }

                inven_ui_drag_cancel_and_restore();
            }

            if (inven_saw_down) {
                *close_requested_ptr = true;
            }
        }

        return false;
    }

    // Click outside the inventory window and outside both HUD strips
    // dismisses the overlay (hi-res convenience; at 800x600 nothing falls
    // outside both menu and HUD so behavior is unchanged).
    // Requires inven_saw_down so a stray mouse-up from a click that
    // started in dialog (e.g. the "Trade" option that opens barter)
    // doesn't immediately dismiss the freshly opened panel.
    if (inven_saw_down
        && !mainmenu_ui_is_active()
        && inven_ui_drag_item_obj == OBJ_HANDLE_NULL
        && inven_ui_window_handle != TIG_WINDOW_HANDLE_INVALID) {
        TigWindowData menu_wd;
        int screen_x = x + (hrp_iso_window_width_get() - 800) / 2;
        int screen_y = y + (hrp_iso_window_height_get() - 600) / 2;
        if (tig_window_data(inven_ui_window_handle, &menu_wd) == TIG_OK
            && intgame_panel_should_dismiss_overlay_click(screen_x, screen_y, &menu_wd.rect)) {
            *close_requested_ptr = true;
            return false;
        }
    }

    if (inven_ui_drag_item_obj == OBJ_HANDLE_NULL) {
        clicked_item_obj = inven_ui_item_at_xy(x, y, &owner_obj);
        if (clicked_item_obj == OBJ_HANDLE_NULL) {
            return false;
        }

        switch (inven_ui_mode) {
        case INVEN_UI_MODE_NPC_IDENTIFY:
            inven_ui_identification_quote_show(clicked_item_obj);
            if (inven_ui_trade_price > 0
                && item_gold_get(inven_ui_pc_obj) >= inven_ui_trade_price) {
                item_perform_identify_service(clicked_item_obj, inven_ui_target_obj, inven_ui_pc_obj, inven_ui_trade_price);
            } else {
                dialog_copy_npc_not_enough_money_msg(inven_ui_target_obj, inven_ui_pc_obj, inven_ui_trade_dialog_text);
            }
            break;
        case INVEN_UI_MODE_NPC_REPAIR:
            inven_ui_repair_offer_refresh(clicked_item_obj);
            if (inven_ui_trade_price > 0
                && item_gold_get(inven_ui_pc_obj) >= inven_ui_trade_price) {
                skill_perform_repair_service(clicked_item_obj, inven_ui_target_obj, inven_ui_pc_obj, inven_ui_trade_price);
            } else {
                dialog_copy_npc_not_enough_money_msg(inven_ui_target_obj, inven_ui_pc_obj, inven_ui_trade_dialog_text);
            }
            break;
        default:
            return false;
        }

        inven_ui_trade_dialog_text[0] = '\0';
        inven_ui_trade_price = -1;
        redraw_inven(false);

        return false;
    }

    if (inven_ui_have_drop_box
        && x >= inven_ui_drop_box_frame.x
        && y >= inven_ui_drop_box_frame.y
        && x < inven_ui_drop_box_frame.x + inven_ui_drop_box_frame.width
        && y < inven_ui_drop_box_frame.y + inven_ui_drop_box_frame.height) {
        if (inven_ui_drag_original_owner != inven_ui_target_inventory_owner
            || (inven_ui_mode == INVEN_UI_MODE_BARTER && inven_ui_target_is_follower)
            || (inven_ui_mode != INVEN_UI_MODE_STEAL
                && inven_ui_mode != INVEN_UI_MODE_LOOT)) {
            inven_ui_drag_item_insert_or_transfer(inven_ui_drag_item_obj, OBJ_HANDLE_NULL, -1, 2);
        } else {
            inven_ui_drag_cancel_and_restore();
        }
        inven_ui_drag_item_obj = OBJ_HANDLE_NULL;
        inven_ui_drop_box_image = 224;
    } else if (inven_ui_have_gamble_box
        && x >= inven_ui_gamble_box_frame.x
        && y >= inven_ui_gamble_box_frame.y
        && x < inven_ui_gamble_box_frame.x + inven_ui_gamble_box_frame.width
        && y < inven_ui_gamble_box_frame.y + inven_ui_gamble_box_frame.height) {
        int64_t item_obj = inven_ui_drag_item_obj;
        inven_ui_drag_cancel_and_restore();
        inven_ui_item_gamble(item_obj);
        inven_ui_drag_item_obj = OBJ_HANDLE_NULL;
        inven_ui_gamble_box_image = 345;
    } else if (inven_ui_have_use_box
        && x >= inven_ui_use_box_frame.x
        && y >= inven_ui_use_box_frame.y
        && x < inven_ui_use_box_frame.x + inven_ui_use_box_frame.width
        && y < inven_ui_use_box_frame.y + inven_ui_use_box_frame.height) {
        if ((inven_ui_drag_original_owner != inven_ui_target_inventory_owner
                || (inven_ui_mode != INVEN_UI_MODE_BARTER
                    && inven_ui_mode != INVEN_UI_MODE_LOOT
                    && inven_ui_mode != INVEN_UI_MODE_STEAL))
            && item_check_use_box_flag(inven_ui_pc_obj, inven_ui_drag_item_obj) == ITEM_CANNOT_OK) {
            if ((obj_field_int32_get(inven_ui_drag_item_obj, OBJ_F_ITEM_FLAGS) & OIF_USE_IS_THROW) != 0
                && inven_ui_mode == INVEN_UI_MODE_INVENTORY) {
                inven_ui_drag_item_insert_or_transfer(inven_ui_drag_item_obj, OBJ_HANDLE_NULL, -1, 0x4);
                redraw_inven(false);
                item_inventory_slots_get(inven_ui_pc_obj, inven_ui_player_inventory_slots);
                if (inven_ui_target_inventory_owner != OBJ_HANDLE_NULL) {
                    item_inventory_slots_get(inven_ui_target_inventory_owner, inven_ui_target_inventory_slots);
                }
            } else {
                int64_t item_obj = inven_ui_drag_item_obj;
                inven_ui_drag_cancel_and_restore();
                if (!combat_turn_based_is_active()
                    || combat_turn_based_whos_turn_get() == inven_ui_pc_obj) {
                    if (tig_kb_get_modifier(SDL_KMOD_SHIFT)) {
                        item_use_on_obj(inven_ui_pc_obj, item_obj, inven_ui_pc_obj);
                    } else {
                        item_use_on_obj(inven_ui_pc_obj, item_obj, OBJ_HANDLE_NULL);
                    }
                }
                if (!inven_ui_created) {
                    return true;
                }
            }
        } else {
            inven_ui_drag_cancel_and_restore();
        }
        inven_ui_drag_item_obj = OBJ_HANDLE_NULL;
        inven_ui_use_box_image = 342;
    } else {
        int64_t parent_obj;
        int inventory_location = inven_ui_inventory_slot_at_xy(x, y, &parent_obj);
        if (!IS_HOTKEY_INV_LOC(inventory_location)) {
            if (IS_WEAR_INV_LOC(inventory_location)) {
                if ((inven_ui_mode != INVEN_UI_MODE_BARTER
                        || inven_ui_drag_original_owner == inven_ui_pc_obj
                        || parent_obj == inven_ui_pc_obj
                        || inven_ui_target_is_follower)
                    && (inven_ui_mode != INVEN_UI_MODE_STEAL
                        || inven_ui_drag_original_owner == inven_ui_pc_obj
                        || parent_obj == inven_ui_pc_obj)) {
                    int wield_reason = item_equipment_slot_check(inven_ui_drag_item_obj, inventory_location, parent_obj);
                    if (wield_reason == ITEM_CANNOT_OK) {
                        int64_t old_item_obj = item_wield_get(parent_obj, inventory_location);
                        if (old_item_obj != OBJ_HANDLE_NULL
                            && old_item_obj != inven_ui_drag_item_obj) {
                            if (parent_obj == inven_ui_pc_obj) {
                                int unwield_reason = item_check_remove(old_item_obj);
                                if (unwield_reason == ITEM_CANNOT_OK) {
                                    if ((inven_ui_mode != INVEN_UI_MODE_BARTER
                                            && inven_ui_mode != INVEN_UI_MODE_STEAL)
                                        || inven_ui_drag_original_owner == parent_obj) {
                                        critter_encumbrance_recalc_feedback_disable();
                                        item_remove(inven_ui_drag_item_obj);
                                        int old_inventory_location = item_inventory_location_get(old_item_obj);
                                        item_remove(old_item_obj);
                                        int new_inventory_location;
                                        int transfer_reason = item_check_insert(old_item_obj, parent_obj, &new_inventory_location);
                                        if (transfer_reason == ITEM_CANNOT_OK) {
                                            item_insert(inven_ui_drag_item_obj, inven_ui_drag_original_owner, inven_ui_drag_original_location);
                                            inven_ui_drag_item_insert_or_transfer(inven_ui_drag_item_obj, parent_obj, inventory_location, 0x1);
                                            item_insert(old_item_obj, parent_obj, new_inventory_location);
                                            inven_ui_drag_item_obj = old_item_obj;
                                            inven_ui_drag_original_location = new_inventory_location;
                                            inven_ui_drag_original_owner = parent_obj;
                                            inven_ui_drag_slot_offset = 0;
                                            inven_ui_deferred_drag_request(-1, -1);
                                        } else {
                                            item_error_msg(parent_obj, transfer_reason);
                                            item_insert(old_item_obj, parent_obj, old_inventory_location);
                                            inven_ui_drag_item_restore_or_drop();
                                        }
                                        critter_encumbrance_recalc_feedback_enable();
                                    } else {
                                        inven_ui_drag_cancel_and_restore();
                                    }
                                } else {
                                    if (parent_obj == inven_ui_pc_obj) {
                                        item_error_msg(parent_obj, unwield_reason);
                                    }
                                }
                            } else {
                                inven_ui_drag_cancel_and_restore();
                            }
                        } else {
                            if (parent_obj == inven_ui_pc_obj) {
                                if (!inven_ui_dragged_item_steal(close_requested_ptr)) {
                                    inven_ui_drop_on_player_inventory(inventory_location);
                                }
                            } else {
                                if (!inven_ui_dragged_item_plant(close_requested_ptr)) {
                                    inven_ui_drop_on_target_inventory(inventory_location);
                                }
                            }
                            inven_ui_drag_item_obj = OBJ_HANDLE_NULL;
                        }
                    } else {
                        item_error_msg(parent_obj, wield_reason);
                        inven_ui_drag_cancel_and_restore();
                    }
                } else {
                    inven_ui_drag_cancel_and_restore();
                }
            } else {
                if (inventory_location - inven_ui_drag_slot_offset < 0) {
                    if (!hotkey_ui_inventory_drop_process(TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP)) {
                        inven_ui_drag_cancel_and_restore();
                    }
                } else if (parent_obj == inven_ui_pc_obj) {
                    if (!inven_ui_dragged_item_steal(close_requested_ptr)) {
                        inven_ui_drop_on_player_inventory(inventory_location - inven_ui_drag_slot_offset);
                    }
                } else {
                    if (!inven_ui_dragged_item_plant(close_requested_ptr)) {
                        inven_ui_drop_on_target_inventory(inventory_location - inven_ui_drag_slot_offset);
                    }
                }
                hotkey_ui_dragged_items_reset();
                inven_ui_drag_item_obj = OBJ_HANDLE_NULL;
            }
        }
    }

    redraw_inven(false);
    item_inventory_slots_get(inven_ui_pc_obj, inven_ui_player_inventory_slots);
    if (inven_ui_target_inventory_owner) {
        item_inventory_slots_get(inven_ui_target_inventory_owner, inven_ui_target_inventory_slots);
    }

    if (!inven_ui_drag_item_obj) {
        intgame_refresh_cursor();
    }
    if (!close_requested_ptr) {
        return true;
    }

    return false;
}

// 0x5742A4
static inline bool inven_ui_message_filter_handle_mouse_rbutton_up(int x, int y, bool* close_requested_ptr)
{
    int64_t selected_obj;
    int64_t owner_obj;
    int64_t destination_owner;
    int rc;
    int destination_slot;
    int64_t target_inventory_owner;
    int inventory_location;

    selected_obj = inven_ui_item_at_xy(x, y, &owner_obj);
    if (selected_obj == OBJ_HANDLE_NULL) {
        return false;
    }

    if (inven_ui_mode == INVEN_UI_MODE_INVENTORY) {
        inventory_location = item_inventory_location_get(selected_obj);
        if (!IS_WEAR_INV_LOC(inventory_location)) {
            inventory_location = item_location_get(selected_obj);
            if (inventory_location == -1
                && item_check_use_box_flag(inven_ui_pc_obj, selected_obj) == ITEM_CANNOT_OK) {
                if (!combat_turn_based_is_active()
                    || combat_turn_based_whos_turn_get() == inven_ui_pc_obj) {
                    item_use_on_obj(inven_ui_pc_obj, selected_obj, inven_ui_pc_obj);
                }

                if (!inven_ui_created) {
                    return false;
                }
            }
        }
    } else if (inven_ui_mode == INVEN_UI_MODE_BARTER
        || inven_ui_mode == INVEN_UI_MODE_LOOT
        || inven_ui_mode == INVEN_UI_MODE_STEAL) {
        destination_owner = inven_ui_pc_obj;
        if (owner_obj == inven_ui_pc_obj) {
            destination_owner = inven_ui_target_inventory_owner;
            target_inventory_owner = inven_ui_target_inventory_owner;
        } else {
            target_inventory_owner = inven_ui_pc_obj;
        }

        rc = item_check_insert(selected_obj, destination_owner, &destination_slot);
        if (rc != ITEM_CANNOT_OK) {
            item_error_msg(owner_obj, rc);
            return true;
        }

        inven_ui_drag_item_obj = selected_obj;
        inven_ui_drag_original_owner = owner_obj;
        inven_ui_drag_slot_offset = inven_ui_inventory_slot_at_xy(x, y, &selected_obj);
        inven_ui_drag_original_location = item_inventory_location_get(inven_ui_drag_item_obj);
        inven_ui_drag_slot_offset -= inven_ui_drag_original_location;
        inven_ui_deferred_drag_request(x, y);
        if (inven_ui_drag_item_obj != OBJ_HANDLE_NULL) {
            if (target_inventory_owner == inven_ui_pc_obj) {
                if (!inven_ui_dragged_item_steal(close_requested_ptr)) {
                    inven_ui_drop_on_player_inventory(destination_slot);
                }
            } else {
                if (!inven_ui_dragged_item_plant(close_requested_ptr)) {
                    inven_ui_drop_on_target_inventory(destination_slot);
                }
            }
            inven_ui_drag_item_obj = OBJ_HANDLE_NULL;
        }
    }

    redraw_inven(false);
    item_inventory_slots_get(inven_ui_pc_obj, inven_ui_player_inventory_slots);
    if (inven_ui_target_inventory_owner != OBJ_HANDLE_NULL) {
        item_inventory_slots_get(inven_ui_target_inventory_owner, inven_ui_target_inventory_slots);
    }

    if (inven_ui_drag_item_obj == OBJ_HANDLE_NULL) {
        intgame_refresh_cursor();
    }

    if (!close_requested_ptr) {
        return true;
    }

    return false;
}

// NOTE: This function is huge and hard to work with. Some chunks were
// externalized into separate inline functions above. This leads to some
// duplication of the code and acts as an optimization fence for compiler.
//
// 0x573850
bool inven_ui_message_filter(TigMessage* msg)
{
    bool close_requested = false;
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

    if (inven_ui_drag_item_obj != OBJ_HANDLE_NULL) {
        scrollbar_ui_begin_ignore_events();
    } else {
        scrollbar_ui_end_ignore_events();
    }

    switch (msg->type) {
    case TIG_MESSAGE_BUTTON:
        // CE: Ignore button events while dragging an item to steal it. This
        // prevents handling press events that would switch between the
        // inventory and paperdoll panels (these buttons form a radio group and
        // use the "pressed" event as their main handler, not "released" as with
        // usual buttons).
        if (!(inven_ui_mode == INVEN_UI_MODE_STEAL
                && inven_ui_drag_item_obj != OBJ_HANDLE_NULL)) {
            switch (msg->data.button.state) {
            case TIG_BUTTON_STATE_PRESSED:
                // 0x573897
                if (inven_ui_message_filter_handle_button_pressed(msg->data.button.button_handle)) {
                    return true;
                }
                break;
            case TIG_BUTTON_STATE_RELEASED:
                // 0x573A95
                if (inven_ui_message_filter_handle_button_released(msg->data.button.button_handle)) {
                    return true;
                }
                break;
            case TIG_BUTTON_STATE_MOUSE_INSIDE:
                // 0x573BAF
                if (inven_ui_mssage_filter_handle_button_hovered(msg->data.button.button_handle)) {
                    return true;
                }
                break;
            }
        }
        break;
    case TIG_MESSAGE_MOUSE:
        if (msg->data.mouse.event == TIG_MESSAGE_MOUSE_IDLE) {
            // 0x573C80
            inven_ui_message_filter_handle_mouse_idle(msg->data.mouse.x, msg->data.mouse.y);
        } else if (msg->data.mouse.event == TIG_MESSAGE_MOUSE_MOVE) {
            // 0x573D61
            if (inven_ui_message_filter_handle_mouse_move(msg->data.mouse.x, msg->data.mouse.y)) {
                return true;
            }
        } else if (msg->data.mouse.event == TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN
            && !inven_ui_mouse_down_seen
            && inven_ui_mode != INVEN_UI_MODE_IDENTIFY
            && inven_ui_mode != INVEN_UI_MODE_NPC_IDENTIFY
            && inven_ui_mode != INVEN_UI_MODE_NPC_REPAIR) {
            // 0x574080
            if (inven_ui_message_filter_handle_mouse_lbutton_down(msg->data.mouse.x, msg->data.mouse.y)) {
                return true;
            }
        } else if (msg->data.mouse.event == TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP
            || (msg->data.mouse.event == TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP && intgame_hotkey_is_dragging())) {
            // 0x574559
            if (inven_ui_message_filter_handle_mouse_lbutton_up(msg->data.mouse.x, msg->data.mouse.y, &close_requested)) {
                return true;
            }
        } else if (msg->data.mouse.event == TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP
            && inven_ui_drag_item_obj == OBJ_HANDLE_NULL
            && (inven_ui_mode == INVEN_UI_MODE_INVENTORY
                || inven_ui_mode == INVEN_UI_MODE_BARTER
                || inven_ui_mode == INVEN_UI_MODE_LOOT
                || inven_ui_mode == INVEN_UI_MODE_STEAL)) {
            // 0x5742A4
            if (inven_ui_message_filter_handle_mouse_rbutton_up(msg->data.mouse.x, msg->data.mouse.y, &close_requested)) {
                return true;
            }
        }
        break;
    default:
        break;
    }

    if (inven_ui_mode == INVEN_UI_MODE_BARTER
        && inven_ui_drag_item_obj == OBJ_HANDLE_NULL
        && intgame_mode_get() != INTGAME_MODE_QUANTITY
        && (critter_is_sleeping(inven_ui_target_obj) || !critter_is_active(inven_ui_target_obj))) {
        close_requested = true;
    }

    if (close_requested || inven_ui_close_requested) {
        inven_ui_destroy();
        return true;
    }

    return false;
}

// 0x574FD0
void inven_ui_follower_cycle(bool next)
{
    int64_t follower_obj;
    int64_t pc_obj;
    int mode;

    inven_ui_drag_cancel_and_restore();

    follower_obj = inven_ui_target_inventory_owner;

    if (next) {
        do {
            follower_obj = critter_follower_next(follower_obj);
        } while (!inven_ui_can_open_inven(inven_ui_pc_obj, follower_obj));
    } else {
        do {
            follower_obj = critter_follower_prev(follower_obj);
        } while (!inven_ui_can_open_inven(inven_ui_pc_obj, follower_obj));
    }

    if (follower_obj != inven_ui_target_inventory_owner) {
        pc_obj = inven_ui_pc_obj;
        mode = inven_ui_mode;
        inven_ui_destroy();
        inven_ui_open(pc_obj, follower_obj, mode);
    }
}

// 0x575080
bool inven_ui_can_open_inven(int64_t pc_obj, int64_t npc_obj)
{
    if (inven_ui_created && npc_obj == inven_ui_target_inventory_owner) {
        return true;
    }

    if (ai_can_speak(npc_obj, pc_obj, false) != AI_SPEAK_OK) {
        return false;
    }

    if ((obj_field_int32_get(npc_obj, OBJ_F_CRITTER_FLAGS) & (OCF_ANIMAL | OCF_MECHANICAL)) != 0) {
        return false;
    }

    if ((obj_field_int32_get(npc_obj, OBJ_F_SPELL_FLAGS) & OSF_SUMMONED) != 0) {
        return false;
    }

    return true;
}

// 0x575100
bool inven_ui_dragged_item_plant(bool* success_ptr)
{
    int64_t item_obj;

    if (inven_ui_mode != INVEN_UI_MODE_STEAL || inven_ui_drag_original_owner != inven_ui_pc_obj) {
        return false;
    }

    item_obj = inven_ui_drag_item_obj;
    inven_ui_drag_cancel_and_restore();

    if (skill_plant_item(inven_ui_pc_obj, inven_ui_target_obj, item_obj)) {
        *success_ptr = true;
    }

    return true;
}

// 0x575180
bool inven_ui_dragged_item_steal(bool* success_ptr)
{
    int64_t item_obj;

    if (inven_ui_mode != INVEN_UI_MODE_STEAL || inven_ui_drag_original_owner == inven_ui_pc_obj) {
        return false;
    }

    item_obj = inven_ui_drag_item_obj;
    inven_ui_drag_cancel_and_restore();

    if (skill_steal_item(inven_ui_pc_obj, inven_ui_target_obj, item_obj)) {
        *success_ptr = true;
    }

    return true;
}

// 0x575200
void inven_ui_drop_on_player_inventory(int inventory_location)
{
    if (inven_ui_mode == INVEN_UI_MODE_BARTER && inven_ui_drag_original_owner != inven_ui_pc_obj) {
        if (inven_ui_trade_price == -1 || inven_ui_drag_item_obj != inven_ui_quoted_item_obj) {
            inven_ui_trade_offer_refresh(inven_ui_drag_item_obj, inven_ui_target_obj);
        }

        if (inven_ui_trade_price != 0 || inven_ui_target_is_follower) {
            if (item_gold_get(inven_ui_pc_obj) < inven_ui_trade_price) {
                dialog_copy_npc_not_enough_money_msg(inven_ui_target_obj, inven_ui_pc_obj, inven_ui_trade_dialog_text);
                inven_ui_drag_cancel_and_restore();
            } else {
                inven_ui_drag_item_insert_or_transfer(inven_ui_drag_item_obj, inven_ui_pc_obj, inventory_location, 1);
            }
        } else {
            inven_ui_drag_cancel_and_restore();
            inven_ui_trade_price = -1;
        }
    } else {
        inven_ui_drag_item_insert_or_transfer(inven_ui_drag_item_obj, inven_ui_pc_obj, inventory_location, 1);
    }

    inven_ui_quoted_item_obj = OBJ_HANDLE_NULL;
}

// 0x575360
void inven_ui_drop_on_target_inventory(int inventory_location)
{
    int cost;

    if (inven_ui_mode != INVEN_UI_MODE_BARTER) {
        inven_ui_drag_item_insert_or_transfer(inven_ui_drag_item_obj, inven_ui_target_inventory_owner, inventory_location, 1);
        inven_ui_quoted_item_obj = OBJ_HANDLE_NULL;
        return;
    }

    if (inven_ui_drag_original_owner != inven_ui_pc_obj) {
        if (!inven_ui_target_is_follower) {
            inven_ui_drag_cancel_and_restore();
        } else {
            inven_ui_drag_item_insert_or_transfer(inven_ui_drag_item_obj, inven_ui_target_inventory_owner, inventory_location, 1);
        }
        inven_ui_quoted_item_obj = OBJ_HANDLE_NULL;
        return;
    }

    if (inven_ui_trade_price == -1 || inven_ui_drag_item_obj != inven_ui_quoted_item_obj) {
        inven_ui_trade_offer_refresh(inven_ui_drag_item_obj, inven_ui_pc_obj);
    }

    if (inven_ui_trade_price != 0 || inven_ui_target_is_follower) {
        cost = item_gold_get(inven_ui_target_obj);
        if (cost < inven_ui_trade_price) {
            inven_ui_trade_price = cost;
        }

        inven_ui_drag_item_insert_or_transfer(inven_ui_drag_item_obj, inven_ui_target_inventory_owner, inventory_location, 1);
        inven_ui_quoted_item_obj = OBJ_HANDLE_NULL;
        inven_ui_trade_price = -1;
    } else {
        inven_ui_drag_cancel_and_restore();
        inven_ui_quoted_item_obj = OBJ_HANDLE_NULL;
        inven_ui_trade_price = -1;
    }
}

// 0x5754C0
void inven_ui_deferred_drag_request(int x, int y)
{
    InventoryDeferredDrag* entry;

    entry = (InventoryDeferredDrag*)MALLOC(sizeof(*entry));
    entry->x = x;
    entry->y = y;
    entry->item_obj = inven_ui_drag_item_obj;
    entry->original_owner_obj = inven_ui_drag_original_owner;
    entry->player_obj = inven_ui_pc_obj;
    entry->mode = inven_ui_mode;
    entry->original_inventory_slot = inven_ui_drag_original_location;
    entry->target_scroll_offset = inven_ui_target_scroll_offset;
    entry->player_scroll_offset = inven_ui_player_scroll_offset;

    inven_ui_drag_item_obj = OBJ_HANDLE_NULL;

    multiplayer_object_lock_request(obj_get_id(entry->item_obj), inven_ui_deferred_drag_complete, entry, inven_ui_deferred_drag_context_free, entry);
}

// 0x575580
bool inven_ui_deferred_drag_context_free(void* userinfo)
{
    InventoryDeferredDrag* entry = (InventoryDeferredDrag*)userinfo;

    FREE(entry);

    return true;
}

// 0x5755A0
bool inven_ui_deferred_drag_complete(void* userinfo)
{
    InventoryDeferredDrag* entry = (InventoryDeferredDrag*)userinfo;
    tig_art_id_t art_id;
    int sound_id;
    TigArtFrameData art_frame_data;

    inven_ui_drag_item_obj = entry->item_obj;
    multiplayer_drag_item_hide(player_get_local_pc_obj(), entry->item_obj);
    art_id = obj_field_int32_get(entry->item_obj, OBJ_F_ITEM_INV_AID);

    if ((entry->x == -1 && entry->y == -1)
        || IS_WEAR_INV_LOC(entry->original_inventory_slot)
        || IS_HOTKEY_INV_LOC(entry->original_inventory_slot)) {
        entry->x = 0;
        entry->y = 0;
    } else {
        if (entry->original_owner_obj == entry->player_obj) {
            entry->x -= 368 + 32 * (entry->original_inventory_slot % 10);
            entry->y -= 49 + 32 * (entry->original_inventory_slot / 10 - entry->player_scroll_offset);
        } else if (entry->mode == 1) {
            entry->x -= 8 + 32 * (entry->original_inventory_slot % 10);
            entry->y -= 209 - 32 * (entry->target_scroll_offset - entry->original_inventory_slot / 10);
        } else {
            entry->x -= 8 + 32 * (entry->original_inventory_slot % 10);
            entry->y -= 177 - 32 * (entry->target_scroll_offset - entry->original_inventory_slot / 10);
        }
    }

    // CE: Fix item dragging visual glitches by maintaining both item icon
    // position (which is now perfectly aligned to the occupied slots), and
    // arrow cursor position (which now stays intact). That makes dragging
    // within the inventories more predicable.
    if (tig_art_frame_data(art_id, &art_frame_data) == TIG_OK) {
        int width;
        int height;

        // Retrieve icon size and scale it to slot size.
        item_inv_icon_size(entry->item_obj, &width, &height);
        width *= 32;
        height *= 32;

        // Adjust position to the center of the occupied slots.
        entry->x -= (width - art_frame_data.width) / 2;
        entry->y -= (height - art_frame_data.height) / 2;

        // Slightly move icon to the top and left, which gives the illusion of
        // depth.
        entry->x += 4;
        entry->y += 4;
    } else {
        // NOTE: This should not normally happen, but if it is, here is the
        // fallback (original approach).
        entry->x += 2;
        entry->y += 2;
    }

    tig_mouse_hide();
    tig_mouse_cursor_set_art_id(art_id);
    tig_art_interface_id_create(0, 0, 0, 0, &art_id);

    // CE: Maintain cursor position. Without adjustment for hot point the cursor
    // jumps down when button is pressed down.
    if (tig_art_frame_data(art_id, &art_frame_data) == TIG_OK) {
        entry->x -= art_frame_data.hot_x;
        entry->y -= art_frame_data.hot_y;
        tig_mouse_cursor_overlay(art_id, entry->x, entry->y);
        entry->x += art_frame_data.hot_x;
        entry->y += art_frame_data.hot_y;
    } else {
        // NOTE: This should not normally happen.
        tig_mouse_cursor_overlay(art_id, entry->x, entry->y);
    }

    tig_mouse_cursor_set_offset(entry->x, entry->y);
    tig_mouse_show();

    redraw_inven(false);

    sound_id = sfx_item_sound(entry->item_obj, entry->original_owner_obj, OBJ_HANDLE_NULL, ITEM_SOUND_PICKUP);
    gsound_play_sfx(sound_id, 1);

    FREE(entry);

    return true;
}

// 0x575770
void inven_ui_drag_cancel_and_restore(void)
{
    int64_t parent_obj;
    int sound_id;

    if (inven_ui_drag_item_obj == OBJ_HANDLE_NULL) {
        return;
    }

    if (!inven_ui_created) {
        if (inven_ui_pc_obj == OBJ_HANDLE_NULL) {
            inven_ui_pc_obj = player_get_local_pc_obj();
        }

        if (IS_HOTKEY_INV_LOC(inven_ui_drag_original_location)) {
            sound_id = sfx_item_sound(inven_ui_drag_item_obj, inven_ui_drag_original_owner, OBJ_HANDLE_NULL, ITEM_SOUND_DROP);
            gsound_play_sfx(sound_id, 1);

            if (!hotkey_ui_dragged_items_reset()) {
                item_insert(inven_ui_drag_item_obj, inven_ui_drag_original_owner, inven_ui_drag_original_location);
            }

            inven_ui_drag_item_obj = OBJ_HANDLE_NULL;
            return;
        }

        if (!inven_ui_created) {
            inven_ui_drag_item_restore_or_drop();
            return;
        }
    }

    if (item_parent(inven_ui_drag_item_obj, &parent_obj)
        && obj_field_int32_get(inven_ui_drag_item_obj, OBJ_F_ITEM_INV_LOCATION) != -1) {
        if (!tig_net_is_active() || !multiplayer_is_locked()) {
            multiplayer_drag_item_show(player_get_local_pc_obj(), inven_ui_drag_item_obj);
        }

        sound_id = sfx_item_sound(inven_ui_drag_item_obj, inven_ui_drag_original_owner, OBJ_HANDLE_NULL, ITEM_SOUND_DROP);
        gsound_play_sfx(sound_id, 1);

        hotkey_ui_dragged_items_reset();
        inven_ui_drag_item_obj = OBJ_HANDLE_NULL;
        redraw_inven(false);
    } else {
        inven_ui_drag_item_restore_or_drop();
    }
}

// 0x575930
void inven_ui_drag_item_restore_or_drop(void)
{
    int sound_id;
    int inventory_location;

    if (!inven_ui_created) {
        if (inven_ui_pc_obj == OBJ_HANDLE_NULL) {
            inven_ui_pc_obj = player_get_local_pc_obj();
        }
    }

    if (!item_inventory_contains_handle(inven_ui_drag_item_obj, inven_ui_drag_original_owner)) {
        if (IS_WEAR_INV_LOC(inven_ui_drag_original_location)
            && item_wield_get(inven_ui_drag_original_owner, inven_ui_drag_original_location) == OBJ_HANDLE_NULL) {
            item_insert(inven_ui_drag_item_obj, inven_ui_drag_original_owner, inven_ui_drag_original_location);
        } else if (IS_HOTKEY_INV_LOC(inven_ui_drag_original_location)
            && !item_find_at_inventory_location(inven_ui_drag_original_owner, inven_ui_drag_original_location)) {
            item_insert(inven_ui_drag_item_obj, inven_ui_drag_original_owner, inven_ui_drag_original_location);
        } else if (!IS_WEAR_INV_LOC(inven_ui_drag_original_location)
            && !IS_HOTKEY_INV_LOC(inven_ui_drag_original_location)
            && inven_ui_drag_original_owner == inven_ui_pc_obj
            && item_inventory_slots_has_room_for(inven_ui_drag_item_obj, inven_ui_pc_obj, inven_ui_drag_original_location, inven_ui_player_inventory_slots)) {
            item_insert(inven_ui_drag_item_obj, inven_ui_drag_original_owner, inven_ui_drag_original_location);
        } else if (!IS_WEAR_INV_LOC(inven_ui_drag_original_location)
            && !IS_HOTKEY_INV_LOC(inven_ui_drag_original_location)
            && inven_ui_drag_original_owner == inven_ui_target_inventory_owner
            && item_inventory_slots_has_room_for(inven_ui_drag_item_obj, inven_ui_target_inventory_owner, inven_ui_drag_original_location, inven_ui_target_inventory_slots)) {
            item_insert(inven_ui_drag_item_obj, inven_ui_drag_original_owner, inven_ui_drag_original_location);
        } else if (item_check_insert(inven_ui_drag_item_obj, inven_ui_drag_original_owner, &inventory_location) == ITEM_CANNOT_OK) {
            item_insert(inven_ui_drag_item_obj, inven_ui_drag_original_owner, inventory_location);
        } else {
            inven_ui_dragged_item_drop_at_owner();
        }
    }

    multiplayer_drag_item_show(player_get_local_pc_obj(), inven_ui_drag_item_obj);

    sound_id = sfx_item_sound(inven_ui_drag_item_obj, inven_ui_drag_original_owner, OBJ_HANDLE_NULL, ITEM_SOUND_DROP);
    gsound_play_sfx(sound_id, 1);

    inven_ui_drag_item_obj = OBJ_HANDLE_NULL;
}

// 0x575BE0
void inven_ui_dragged_item_drop_at_owner(void)
{
    int64_t location;
    tig_sound_handle_t sound_id;

    location = obj_field_int64_get(inven_ui_drag_original_owner, OBJ_F_LOCATION);
    item_drop_into_junk_pile_with_decay(inven_ui_drag_item_obj, location);

    sound_id = sfx_item_sound(inven_ui_drag_item_obj, inven_ui_drag_original_owner, OBJ_HANDLE_NULL, ITEM_SOUND_DROP);
    gsound_play_sfx(sound_id, 1);
}

// 0x575C50
void inven_ui_drag_object_invalidate(int64_t obj)
{
    if (inven_ui_drag_item_obj != OBJ_HANDLE_NULL && obj == inven_ui_drag_item_obj) {
        multiplayer_drag_item_show(player_get_local_pc_obj(), inven_ui_drag_item_obj);
        inven_ui_drag_item_obj = OBJ_HANDLE_NULL;
        intgame_refresh_cursor();
        redraw_inven(false);
    }
}

// 0x575CB0
int inven_ui_inventory_slot_at_xy(int x, int y, int64_t* parent_obj_ptr)
{
    int idx;
    int col;
    int row;

    switch (inven_ui_mode) {
    case INVEN_UI_MODE_INVENTORY:
    case INVEN_UI_MODE_NPC_IDENTIFY:
    case INVEN_UI_MODE_NPC_REPAIR:
        *parent_obj_ptr = inven_ui_pc_obj;

        for (idx = 0; idx < 9; idx++) {
            if (x >= inven_ui_inventory_paperdoll_inv_slot_rects[idx].x
                && y - 41 >= inven_ui_inventory_paperdoll_inv_slot_rects[idx].y
                && x < inven_ui_inventory_paperdoll_inv_slot_rects[idx].x + inven_ui_inventory_paperdoll_inv_slot_rects[idx].width
                && y - 41 < inven_ui_inventory_paperdoll_inv_slot_rects[idx].y + inven_ui_inventory_paperdoll_inv_slot_rects[idx].height) {
                return idx + 1000;
            }
        }

        break;
    case INVEN_UI_MODE_BARTER:
        if (inven_ui_target_panel == INVEN_UI_PANEL_PAPERDOLL) {
            *parent_obj_ptr = inven_ui_target_obj;

            for (idx = 0; idx < 9; idx++) {
                if (x >= inven_ui_barter_npc_paperdoll_inv_slot_rects[idx].x
                    && y - 41 >= inven_ui_barter_npc_paperdoll_inv_slot_rects[idx].y
                    && x < inven_ui_barter_npc_paperdoll_inv_slot_rects[idx].x + inven_ui_barter_npc_paperdoll_inv_slot_rects[idx].width
                    && y - 41 < inven_ui_barter_npc_paperdoll_inv_slot_rects[idx].y + inven_ui_barter_npc_paperdoll_inv_slot_rects[idx].height) {
                    return idx + 1000;
                }
            }
        } else {
            *parent_obj_ptr = inven_ui_target_inventory_owner;

            if (x - 8 >= 0 && y - 209 >= 0) {
                col = (x - 8) / 32;
                row = (y - 209) / 32;
                if (col < 10 && row < 7) {
                    return col + 10 * (inven_ui_target_scroll_offset + row);
                }
            }
        }

        break;
    default:
        if (inven_ui_target_panel == INVEN_UI_PANEL_PAPERDOLL) {
            *parent_obj_ptr = inven_ui_target_obj;

            for (idx = 0; idx < 9; idx++) {
                if (x >= inven_ui_loot_npc_paperdoll_inv_slot_rects[idx].x
                    && y - 41 >= inven_ui_loot_npc_paperdoll_inv_slot_rects[idx].y
                    && x < inven_ui_loot_npc_paperdoll_inv_slot_rects[idx].x + inven_ui_loot_npc_paperdoll_inv_slot_rects[idx].width
                    && y - 41 < inven_ui_loot_npc_paperdoll_inv_slot_rects[idx].y + inven_ui_loot_npc_paperdoll_inv_slot_rects[idx].height) {
                    return idx + 1000;
                }
            }
        } else {
            *parent_obj_ptr = inven_ui_target_inventory_owner;

            if (x - 8 >= 0 && y - 177 >= 0) {
                col = (x - 8) / 32;
                row = (y - 177) / 32;
                if (col < 10 && row < 8) {
                    return col + 10 * (inven_ui_target_scroll_offset + row);
                }
            }
        }

        break;
    }

    *parent_obj_ptr = inven_ui_pc_obj;

    if (inven_ui_panel == INVEN_UI_PANEL_PAPERDOLL) {
        for (idx = 0; idx < 9; idx++) {
            if (x >= inven_ui_barter_pc_paperdoll_inv_slot_rects[idx].x
                && y - 41 >= inven_ui_barter_pc_paperdoll_inv_slot_rects[idx].y
                && x < inven_ui_barter_pc_paperdoll_inv_slot_rects[idx].x + inven_ui_barter_pc_paperdoll_inv_slot_rects[idx].width
                && y - 41 < inven_ui_barter_pc_paperdoll_inv_slot_rects[idx].y + inven_ui_barter_pc_paperdoll_inv_slot_rects[idx].height) {
                return idx + 1000;
            }
        }
    } else {
        if (x - 368 >= 0 && y - 49 >= 0) {
            col = (x - 368) / 32;
            row = (y - 49) / 32;
            if (col < 10 && row < 12) {
                return col + 10 * (row + inven_ui_player_scroll_offset);
            }
        }
    }

    return -1;
}

// 0x575FA0
int64_t inven_ui_item_at_xy(int x, int y, int64_t* parent_obj_ptr)
{
    int inventory_location;
    int64_t parent_obj;
    int64_t item_obj = OBJ_HANDLE_NULL;

    inventory_location = inven_ui_inventory_slot_at_xy(x, y, &parent_obj);
    if (inventory_location != -1) {
        if (IS_WEAR_INV_LOC(inventory_location)) {
            item_obj = item_wield_get(parent_obj, inventory_location);
        } else if (IS_HOTKEY_INV_LOC(inventory_location)) {
            item_obj = item_find_at_inventory_location(parent_obj, inventory_location);
        } else {
            if (parent_obj == inven_ui_pc_obj) {
                if (inven_ui_player_inventory_slots[inventory_location] != 0) {
                    item_obj = obj_arrayfield_handle_get(parent_obj,
                        OBJ_F_CRITTER_INVENTORY_LIST_IDX,
                        inven_ui_player_inventory_slots[inventory_location] - 1);
                }
            } else {
                if (inven_ui_target_inventory_slots[inventory_location] != 0) {
                    item_obj = obj_arrayfield_handle_get(parent_obj,
                        obj_field_int32_get(parent_obj, OBJ_F_TYPE) == OBJ_TYPE_CONTAINER
                            ? OBJ_F_CONTAINER_INVENTORY_LIST_IDX
                            : OBJ_F_CRITTER_INVENTORY_LIST_IDX,
                        inven_ui_target_inventory_slots[inventory_location] - 1);
                }
            }
        }

        if (item_obj != OBJ_HANDLE_NULL) {
            if ((obj_field_int32_get(item_obj, OBJ_F_FLAGS) & OF_OFF) != 0) {
                return OBJ_HANDLE_NULL;
            }
            if ((obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS) & OIF_NO_DISPLAY) != 0) {
                return OBJ_HANDLE_NULL;
            }
        }
    }

    if (parent_obj_ptr != NULL) {
        *parent_obj_ptr = parent_obj;
    }

    return item_obj;
}

// 0x576100
void inven_ui_update(int64_t obj)
{
    if (!inven_ui_created) {
        return;
    }

    if (obj == inven_ui_pc_obj) {
        item_inventory_slots_get(inven_ui_pc_obj, inven_ui_player_inventory_slots);
        redraw_inven(false);
    } else if (obj == inven_ui_target_inventory_owner || obj == inven_ui_target_obj) {
        item_inventory_slots_get(inven_ui_target_inventory_owner, inven_ui_target_inventory_slots);
        redraw_inven(false);
    } else if (obj == OBJ_HANDLE_NULL) {
        item_inventory_slots_get(inven_ui_pc_obj, inven_ui_player_inventory_slots);
        if (inven_ui_target_inventory_owner != OBJ_HANDLE_NULL) {
            item_inventory_slots_get(inven_ui_target_inventory_owner, inven_ui_target_inventory_slots);
        }
        redraw_inven(false);
    }
}

// 0x5761D0
void redraw_inven_fix_bad_inven_obj(int64_t obj)
{
    int type;
    int inventory_num_fld;
    int inventory_list_fld;
    int cnt;
    int i;
    int j;
    int k;
    int64_t item_obj;
    tig_art_id_t art_id;
    int inventory_location;

    if (obj == OBJ_HANDLE_NULL) {
        return;
    }

    type = obj_field_int32_get(obj, OBJ_F_TYPE);
    if (type == OBJ_TYPE_CONTAINER) {
        inventory_num_fld = OBJ_F_CONTAINER_INVENTORY_NUM;
        inventory_list_fld = OBJ_F_CONTAINER_INVENTORY_LIST_IDX;
    } else {
        inventory_num_fld = OBJ_F_CRITTER_INVENTORY_NUM;
        inventory_list_fld = OBJ_F_CRITTER_INVENTORY_LIST_IDX;
    }

    cnt = obj_field_int32_get(obj, inventory_num_fld);
    for (i = 0; i < cnt; i++) {
        item_obj = obj_arrayfield_handle_get(obj, inventory_list_fld, i);
        for (j = i + 1; j < cnt; j++) {
            if (item_obj == obj_arrayfield_handle_get(obj, inventory_list_fld, j)) {
                tig_debug_printf("redraw_inven_fix_bad_inven_obj: WARNING: Found duplicate handle for object in inventory...CORRECTING!\n");
                cnt--;
                for (k = j; k < cnt; k++) {
                    item_obj = obj_arrayfield_handle_get(obj, inventory_list_fld, k + 1);
                    obj_arrayfield_obj_set(obj, inventory_list_fld, k, item_obj);
                }

                obj_arrayfield_length_set(obj, inventory_list_fld, cnt);
                obj_field_int32_set(obj, inventory_num_fld, cnt);
                j--;
            }
        }
    }

    for (i = 0; i < cnt; i++) {
        item_obj = obj_arrayfield_handle_get(obj, inventory_list_fld, i);
        if (inven_ui_invalid_parent_link_remove(item_obj, obj) && i < cnt - 1) {
            cnt = obj_field_int32_get(obj, inventory_num_fld);
            i--;
        }
    }

    for (i = 0; i < cnt; i++) {
        item_obj = obj_arrayfield_handle_get(obj, inventory_list_fld, i);

        art_id = obj_field_int32_get(item_obj, OBJ_F_ITEM_INV_AID);
        if (tig_art_item_id_disposition_get(art_id) != TIG_ART_ITEM_DISPOSITION_GROUND) {
            continue;
        }

        art_id = tig_art_item_id_disposition_set(art_id, TIG_ART_ITEM_DISPOSITION_INVENTORY);
        obj_field_int32_set(item_obj, OBJ_F_ITEM_INV_AID, art_id);

        inventory_location = item_inventory_location_get(item_obj);
        if (IS_WEAR_INV_LOC(inventory_location) || IS_HOTKEY_INV_LOC(inventory_location)) {
            continue;
        }

        if (item_check_remove(item_obj) == ITEM_CANNOT_OK) {
            item_drop_nearby(item_obj);
            item_arrange_inventory(item_obj, false);
            if (item_check_insert(item_obj, obj, &inventory_location) == ITEM_CANNOT_OK) {
                if (!item_transfer_ex(item_obj, obj, inventory_location)) {
                    cnt--;
                }
            } else {
                cnt--;
            }
            i = -1;
        } else {
            tig_debug_printf("Warning! A object with bad inventory art could not be removed and re-inserted. It may cause inventory graphic errors.\n");
        }
    }
}

// 0x5764B0
bool inven_ui_invalid_parent_link_remove(int64_t item_obj, int64_t expected_owner)
{
    int64_t parent_obj;

    if (!item_parent(item_obj, &parent_obj)) {
        tig_debug_printf("Warning: redraw_inven_fix_bad_inven called on item that doesn't think it has a parent.\n");
        item_force_remove(item_obj, expected_owner);
        return true;
    }

    if (parent_obj != expected_owner) {
        tig_debug_printf("Warning: redraw_inven_fix_bad_inven called on item with different parent.\n");
        item_force_remove(item_obj, expected_owner);
        return true;
    }

    return false;
}

// 0x576520
void redraw_inven(bool initial_draw)
{
    TigArtBlitInfo art_blit_info;
    TigArtFrameData art_frame_data;
    TigArtAnimData art_anim_data;
    TigRect src_rect;
    TigRect dst_rect;
    TigFont font_desc;
    TigPaletteModifyInfo palette_modify_info;
    MesFileEntry mes_file_entry1;
    MesFileEntry mes_file_entry2;
    TigRect* text_rects;
    char str[80];
    int portrait;
    size_t pos;
    int index;
    bool filled_inv_slots[9];
    int inventory_cnt;
    int64_t item_obj;
    int item_type;
    unsigned int item_flags;
    int inventory_location;
    int weapon_min_str;
    bool weapon_too_heavy;

    art_blit_info.art_id = inven_ui_background_art_id_get();
    if (tig_art_frame_data(art_blit_info.art_id, &art_frame_data) != TIG_OK) {
        return;
    }

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = art_frame_data.width;
    src_rect.height = art_frame_data.height;

    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width = art_frame_data.width;
    dst_rect.height = art_frame_data.height;

    art_blit_info.flags = 0;
    art_blit_info.src_rect = &src_rect;
    art_blit_info.dst_rect = &dst_rect;
    tig_window_blit_art(inven_ui_window_handle, &art_blit_info);

    // 0x5765A5
    switch (inven_ui_mode) {
    case INVEN_UI_MODE_BARTER:
        if (intgame_examine_portrait(inven_ui_pc_obj, inven_ui_target_obj, &portrait)) {
            portrait_draw_native(inven_ui_target_obj, portrait, inven_ui_window_handle, 183, 33);
        } else {
            tig_art_interface_id_create(portrait, 0, 0, 0, &(art_blit_info.art_id));
            tig_art_frame_data(art_blit_info.art_id, &art_frame_data);

            src_rect.x = 0;
            src_rect.y = 0;
            src_rect.width = art_frame_data.width;
            src_rect.height = art_frame_data.width;

            dst_rect.x = 183;
            dst_rect.y = 33;
            dst_rect.width = art_frame_data.width;
            dst_rect.height = art_frame_data.width;

            art_blit_info.flags = 0;
            art_blit_info.src_rect = &src_rect;
            art_blit_info.dst_rect = &dst_rect;
            tig_window_blit_art(inven_ui_window_handle, &art_blit_info);
        }

        tig_font_push(inven_ui_description_font);
        src_rect.x = 215;
        src_rect.y = 102;
        src_rect.width = 200;
        src_rect.height = 20;

        font_desc.str = inven_ui_target_description;
        font_desc.width = 0;
        tig_font_measure(&font_desc);

        src_rect.x -= font_desc.width / 2;
        tig_window_text_write(inven_ui_window_handle, font_desc.str, &src_rect);
        tig_font_pop();

        tig_font_push(inven_ui_detail_font);
        src_rect.x = 89;
        src_rect.y = 128;
        src_rect.width = 253;
        src_rect.height = 30;

        if (inven_ui_target_is_follower && inven_ui_trade_dialog_text[0] == '\0') {

            mes_file_entry1.num = 9;
            mes_get_msg(inven_ui_mes_file, &mes_file_entry1);

            mes_file_entry2.num = 6;
            mes_get_msg(inven_ui_mes_file, &mes_file_entry2);

            snprintf(inven_ui_trade_dialog_text, sizeof(inven_ui_trade_dialog_text),
                "%s: %d %s\n",
                mes_file_entry1.str,
                item_total_weight(inven_ui_target_obj),
                mes_file_entry2.str);
            pos = strlen(inven_ui_trade_dialog_text);

            mes_file_entry1.num = 10;
            mes_get_msg(inven_ui_mes_file, &mes_file_entry1);

            snprintf(&(inven_ui_trade_dialog_text[pos]), sizeof(inven_ui_trade_dialog_text) - pos,
                "%s: %lld/%d\n", mes_file_entry1.str,
                (long long)item_total_bulk(inven_ui_target_obj), item_bulk_capacity(inven_ui_target_obj));
        } else {
            pos = 0;
        }

        tig_window_text_write(inven_ui_window_handle, inven_ui_trade_dialog_text, &src_rect);

        if (pos != 0) {
            inven_ui_trade_dialog_text[0] = '\0';
        }
        tig_font_pop();
        break;
    case INVEN_UI_MODE_LOOT:
    case INVEN_UI_MODE_STEAL:
    case INVEN_UI_MODE_IDENTIFY:
        if (intgame_examine_portrait(inven_ui_pc_obj, inven_ui_target_obj, &portrait)) {
            portrait_draw_native(inven_ui_target_obj, portrait, inven_ui_window_handle, inven_ui_loot_target_portrait_rect.x, inven_ui_loot_target_portrait_rect.y);
        } else {
            tig_art_interface_id_create(portrait, 0, 0, 0, &(art_blit_info.art_id));
            tig_art_frame_data(art_blit_info.art_id, &art_frame_data);

            src_rect.x = 0;
            src_rect.y = 0;
            src_rect.width = art_frame_data.width;
            src_rect.height = art_frame_data.width;

            dst_rect.x = inven_ui_loot_target_portrait_rect.x;
            dst_rect.y = inven_ui_loot_target_portrait_rect.y;
            dst_rect.width = art_frame_data.width;
            dst_rect.height = art_frame_data.width;

            art_blit_info.flags = 0;
            art_blit_info.src_rect = &src_rect;
            art_blit_info.dst_rect = &dst_rect;
            tig_window_blit_art(inven_ui_window_handle, &art_blit_info);
        }
        break;
    case INVEN_UI_MODE_NPC_IDENTIFY:
    case INVEN_UI_MODE_NPC_REPAIR:
        if (intgame_examine_portrait(inven_ui_pc_obj, inven_ui_target_obj, &portrait)) {
            portrait_draw_32x32(inven_ui_target_obj, portrait, inven_ui_window_handle, 141, 36);
        } else {
            tig_art_interface_id_create(portrait, 0, 0, 0, &(art_blit_info.art_id));
            tig_art_frame_data(art_blit_info.art_id, &art_frame_data);

            src_rect.x = 0;
            src_rect.y = 0;
            src_rect.width = art_frame_data.width;
            src_rect.height = art_frame_data.width;

            dst_rect.x = 141;
            dst_rect.y = 36;
            dst_rect.width = art_frame_data.width / 2;
            dst_rect.height = art_frame_data.width / 2;

            art_blit_info.flags = 0;
            art_blit_info.src_rect = &src_rect;
            art_blit_info.dst_rect = &dst_rect;
            tig_window_blit_art(inven_ui_window_handle, &art_blit_info);
        }

        tig_font_push(inven_ui_detail_font);
        src_rect = inven_ui_npc_action_rect;
        tig_window_text_write(inven_ui_window_handle, inven_ui_trade_dialog_text, &src_rect);
        tig_font_pop();
        break;
    default:
        break;
    }

    // 0x576B04
    tig_art_interface_id_create(221, 0, 0, 0, &(art_blit_info.art_id));
    if (tig_art_frame_data(art_blit_info.art_id, &art_frame_data) != TIG_OK) {
        return;
    }

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = art_frame_data.width;
    src_rect.height = art_frame_data.width;

    dst_rect.x = 358;
    dst_rect.y = 0;
    dst_rect.width = art_frame_data.width;
    dst_rect.height = art_frame_data.width;

    art_blit_info.flags = 0;
    art_blit_info.src_rect = &src_rect;
    art_blit_info.dst_rect = &dst_rect;
    tig_window_blit_art(inven_ui_window_handle, &art_blit_info);

    if (inven_ui_panel == INVEN_UI_PANEL_PAPERDOLL) {
        tig_art_interface_id_create(341, 0, 0, 0, &(art_blit_info.art_id));
        if (tig_art_frame_data(art_blit_info.art_id, &art_frame_data) != TIG_OK) {
            return;
        }

        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.width = art_frame_data.width;
        src_rect.height = art_frame_data.height;

        dst_rect.x = 358;
        dst_rect.y = 0;
        dst_rect.width = art_frame_data.width;
        dst_rect.height = art_frame_data.height;

        art_blit_info.flags = 0;
        art_blit_info.src_rect = &src_rect;
        art_blit_info.dst_rect = &dst_rect;
        tig_window_blit_art(inven_ui_window_handle, &art_blit_info);
    }

    // 0x576C38
    if (inven_ui_mode == INVEN_UI_MODE_INVENTORY) {
        text_rects = inven_ui_inventory_summary_rects;
    } else {
        if (inven_ui_panel == INVEN_UI_PANEL_PAPERDOLL) {
            text_rects = inven_ui_barter_pc_summary_rects;
        } else {
            text_rects = NULL;
        }
    }

    if (text_rects != NULL) {

        tig_font_push(inven_ui_detail_font);

        mes_file_entry1.num = 9;
        mes_get_msg(inven_ui_mes_file, &mes_file_entry1);

        mes_file_entry2.num = 6;
        mes_get_msg(inven_ui_mes_file, &mes_file_entry2);

        snprintf(str, sizeof(str),
            "%s: %d %s",
            mes_file_entry1.str,
            item_total_weight(inven_ui_pc_obj),
            mes_file_entry2.str);
        tig_window_text_write(inven_ui_window_handle, str, &(text_rects[INVEN_UI_SUMMARY_WEIGHT]));

        mes_file_entry1.num = 10;
        mes_get_msg(inven_ui_mes_file, &mes_file_entry1);

        snprintf(str, sizeof(str), "%s: %lld/%d", mes_file_entry1.str,
            (long long)item_total_bulk(inven_ui_pc_obj), item_bulk_capacity(inven_ui_pc_obj));
        tig_window_text_write(inven_ui_window_handle, str, &(text_rects[INVEN_UI_SUMMARY_ENCUMBRANCE]));

        mes_file_entry1.num = 11;
        mes_get_msg(inven_ui_mes_file, &mes_file_entry1);

        snprintf(str, sizeof(str),
            "%s: %d",
            mes_file_entry1.str,
            stat_level_get(inven_ui_pc_obj, STAT_SPEED));
        tig_window_text_write(inven_ui_window_handle, str, &(text_rects[INVEN_UI_SUMMARY_INFO_SPEED]));

        tig_font_pop();
    }

    // 0x576DFE
    if (inven_ui_mode == INVEN_UI_MODE_INVENTORY
        || inven_ui_mode == INVEN_UI_MODE_NPC_IDENTIFY
        || inven_ui_mode == INVEN_UI_MODE_NPC_REPAIR) {
        text_rects = inven_ui_inventory_total_stat_rects;
    } else {
        if (inven_ui_panel == INVEN_UI_PANEL_PAPERDOLL) {
            text_rects = inven_ui_barter_pc_total_stat_rects;
        } else {
            text_rects = NULL;
        }
    }

    if (text_rects != NULL) {
        tig_font_push(inven_ui_combat_totals_font);

        snprintf(str, sizeof(str), "%d", item_total_attack(inven_ui_pc_obj));

        font_desc.str = str;
        font_desc.width = 0;
        tig_font_measure(&font_desc);

        src_rect.x = text_rects[0].x + text_rects[0].width - font_desc.width;
        src_rect.y = text_rects[0].y;
        src_rect.width = text_rects[0].width;
        src_rect.height = text_rects[0].height;
        tig_window_text_write(inven_ui_window_handle, str, &src_rect);

        snprintf(str, sizeof(str), "%d", item_total_defence(inven_ui_pc_obj));
        tig_window_text_write(inven_ui_window_handle, str, &(text_rects[1]));

        tig_font_pop();
    }

    // 0x576F05
    tig_font_push(inven_ui_resource_count_font);
    for (index = 0; index < 5; index++) {
        int value;

        if (index == 0) {
            value = item_gold_get(inven_ui_pc_obj);
        } else {
            value = item_ammo_quantity_get(inven_ui_pc_obj, index - 1);
        }

        src_rect.x = inven_ui_gold_ammo_type_x[index];
        src_rect.y = inven_ui_gold_ammo_type_y[index];
        src_rect.width = 58;
        src_rect.height = 20;

        snprintf(str, sizeof(str), "%d", value);
        tig_window_text_write(inven_ui_window_handle, str, &src_rect);
    }
    tig_font_pop();

    // 0x576FBE
    if (inven_ui_have_drop_box) {
        tig_art_interface_id_create(inven_ui_drop_box_image, 0, 0, 0, &(art_blit_info.art_id));
        if (tig_art_frame_data(art_blit_info.art_id, &art_frame_data) != TIG_OK) {
            return;
        }

        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.width = art_frame_data.width;
        src_rect.height = art_frame_data.width;

        dst_rect.x = 703;
        dst_rect.y = 344;
        dst_rect.width = art_frame_data.width;
        dst_rect.height = art_frame_data.width;

        art_blit_info.flags = 0;
        art_blit_info.src_rect = &src_rect;
        art_blit_info.dst_rect = &dst_rect;
        tig_window_blit_art(inven_ui_window_handle, &art_blit_info);

        inven_ui_drop_box_frame.width = art_frame_data.width;
        inven_ui_drop_box_frame.height = art_frame_data.height;
    }

    // 0x577070
    if (inven_ui_have_gamble_box) {
        tig_art_interface_id_create(inven_ui_gamble_box_image, 0, 0, 0, &(art_blit_info.art_id));
        if (tig_art_frame_data(art_blit_info.art_id, &art_frame_data) != TIG_OK) {
            return;
        }

        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.width = art_frame_data.width;
        src_rect.height = art_frame_data.width;

        dst_rect.x = 703;
        dst_rect.y = 236;
        dst_rect.width = art_frame_data.width;
        dst_rect.height = art_frame_data.width;

        art_blit_info.flags = 0;
        art_blit_info.src_rect = &src_rect;
        art_blit_info.dst_rect = &dst_rect;
        tig_window_blit_art(inven_ui_window_handle, &art_blit_info);

        inven_ui_gamble_box_frame.width = art_frame_data.width;
        inven_ui_gamble_box_frame.height = art_frame_data.height;
    }

    // 0x57711C
    if (inven_ui_have_use_box) {
        tig_art_interface_id_create(inven_ui_use_box_image, 0, 0, 0, &(art_blit_info.art_id));
        if (tig_art_frame_data(art_blit_info.art_id, &art_frame_data) != TIG_OK) {
            return;
        }

        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.width = art_frame_data.width;
        src_rect.height = art_frame_data.width;

        dst_rect.x = 703;
        dst_rect.y = 290;
        dst_rect.width = art_frame_data.width;
        dst_rect.height = art_frame_data.width;

        art_blit_info.flags = 0;
        art_blit_info.src_rect = &src_rect;
        art_blit_info.dst_rect = &dst_rect;
        tig_window_blit_art(inven_ui_window_handle, &art_blit_info);

        inven_ui_use_box_frame.width = art_frame_data.width;
        inven_ui_use_box_frame.height = art_frame_data.height;
    }

    // 0x5771C8
    if (inven_ui_mode == INVEN_UI_MODE_INVENTORY
        || inven_ui_mode == INVEN_UI_MODE_NPC_IDENTIFY
        || inven_ui_mode == INVEN_UI_MODE_NPC_REPAIR) {
        text_rects = inven_ui_inventory_paperdoll_inv_slot_rects;
        memset(filled_inv_slots, 0, sizeof(filled_inv_slots));
    } else {
        if (inven_ui_panel == INVEN_UI_PANEL_PAPERDOLL) {
            text_rects = inven_ui_barter_pc_paperdoll_inv_slot_rects;
            memset(filled_inv_slots, 0, sizeof(filled_inv_slots));
        } else {
            text_rects = NULL;
        }
    }

    inventory_cnt = obj_field_int32_get(inven_ui_pc_obj, OBJ_F_CRITTER_INVENTORY_NUM);
    dst_rect = src_rect;
    art_blit_info.src_rect = &src_rect;
    art_blit_info.dst_rect = &dst_rect;

    for (index = 0; index < inventory_cnt; index++) {
        item_obj = obj_arrayfield_handle_get(inven_ui_pc_obj, OBJ_F_CRITTER_INVENTORY_LIST_IDX, index);
        if ((obj_field_int32_get(item_obj, OBJ_F_FLAGS) & OF_OFF) != 0) {
            continue;
        }

        item_flags = obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS);
        if ((item_flags & OIF_NO_DISPLAY) != 0) {
            if (!initial_draw) {
                continue;
            }
            item_flags &= ~OIF_NO_DISPLAY;
            obj_field_int32_set(item_obj, OBJ_F_ITEM_FLAGS, item_flags);
        }

        // 0x5772EE
        inventory_location = item_inventory_location_get(item_obj);
        if (IS_HOTKEY_INV_LOC(inventory_location)) {
            continue;
        }

        art_blit_info.flags = 0;

        // 0x57730A
        if (IS_WEAR_INV_LOC(inventory_location)) {
            if (inven_ui_mode == INVEN_UI_MODE_INVENTORY
                || inven_ui_panel == INVEN_UI_PANEL_PAPERDOLL
                || inven_ui_mode == INVEN_UI_MODE_NPC_IDENTIFY
                || inven_ui_mode == INVEN_UI_MODE_NPC_REPAIR) {
                weapon_too_heavy = false;
                if (inventory_location == ITEM_INV_LOC_WEAPON) {
                    weapon_min_str = item_weapon_min_strength(item_obj, inven_ui_pc_obj);
                    if (stat_level_get(inven_ui_pc_obj, STAT_STRENGTH) < weapon_min_str) {
                        dst_rect = text_rects[4];
                        tig_window_tint(inven_ui_window_handle,
                            &dst_rect,
                            tig_color_make(64, 0, 0),
                            0);
                        weapon_too_heavy = true;
                    }
                }

                switch (obj_field_int32_get(item_obj, OBJ_F_TYPE)) {
                case OBJ_TYPE_WEAPON:
                    art_blit_info.art_id = obj_field_int32_get(item_obj, OBJ_F_WEAPON_PAPER_DOLL_AID);
                    break;
                case OBJ_TYPE_ARMOR:
                    art_blit_info.art_id = obj_field_int32_get(item_obj, OBJ_F_ARMOR_PAPER_DOLL_AID);
                    break;
                default:
                    art_blit_info.art_id = obj_field_int32_get(item_obj, OBJ_F_ITEM_INV_AID);
                    break;
                }

                if (tig_art_frame_data(art_blit_info.art_id, &art_frame_data) == TIG_OK) {
                    src_rect.x = 0;
                    src_rect.y = 0;
                    src_rect.height = art_frame_data.height;
                    src_rect.width = art_frame_data.width;

                    dst_rect.x = text_rects[inventory_location - 1000].x
                        + (text_rects[inventory_location - 1000].width - art_frame_data.width) / 2;
                    dst_rect.y = text_rects[inventory_location - 1000].y
                        + (text_rects[inventory_location - 1000].height - art_frame_data.height) / 2;
                    dst_rect.width = art_frame_data.width;
                    dst_rect.height = art_frame_data.height;

                    tig_window_blit_art(inven_ui_window_handle, &art_blit_info);

                    if (weapon_too_heavy) {
                        mes_file_entry1.num = 8;
                        mes_get_msg(inven_ui_mes_file, &mes_file_entry1);

                        snprintf(str, sizeof(str), "%s: %d", mes_file_entry1.str, weapon_min_str);
                        tig_font_push(inven_ui_detail_font);
                        font_desc.str = str;
                        font_desc.width = 0;
                        tig_font_measure(&font_desc);
                        dst_rect.height = font_desc.height;
                        dst_rect.y += dst_rect.height - font_desc.height;
                        tig_window_text_write(inven_ui_window_handle, str, &dst_rect);
                        tig_font_pop();
                    }
                }

                filled_inv_slots[inventory_location - 1000] = true;
            }

            continue;
        }

        // 0x5775A7
        if (inven_ui_panel == INVEN_UI_PANEL_INVENTORY) {
            int x;
            int y;
            int width;
            int height;

            x = 32 * (inventory_location % 10) + 368;
            y = 32 * (inventory_location / 10 - inven_ui_player_scroll_offset) + 8;
            item_inv_icon_size(item_obj, &width, &height);
            width *= 32;
            height *= 32;
            if (y + height <= 8 || y >= 392) {
                continue;
            }

            if (item_obj == inven_ui_quoted_item_obj) {
                dst_rect.x = x;
                dst_rect.y = y < 8 ? 8 : y;
                dst_rect.width = width;
                dst_rect.height = (y + height > 392 ? 392 : y + height) - dst_rect.y;

                tig_window_tint(inven_ui_window_handle,
                    &dst_rect,
                    tig_color_make(64, 0, 0),
                    0);
            }

            art_blit_info.art_id = obj_field_int32_get(item_obj, OBJ_F_ITEM_INV_AID);
            if (tig_art_item_id_destroyed_get(obj_field_int32_get(item_obj, OBJ_F_CURRENT_AID)) != 0) {
                art_blit_info.art_id = tig_art_item_id_destroyed_set(art_blit_info.art_id, 0);
                tig_art_anim_data(art_blit_info.art_id, &art_anim_data);

                palette_modify_info.dst_palette = tig_palette_create();
                palette_modify_info.src_palette = art_anim_data.palette1;
                palette_modify_info.flags = TIG_PALETTE_MODIFY_TINT;
                palette_modify_info.tint_color = tig_color_make(255, 0, 0);
                tig_palette_modify(&palette_modify_info);

                art_blit_info.flags = TIG_ART_BLT_PALETTE_OVERRIDE;
                art_blit_info.palette = palette_modify_info.dst_palette;
            } else {
                palette_modify_info.dst_palette = NULL;
            }

            if (tig_art_frame_data(art_blit_info.art_id, &art_frame_data) == TIG_OK) {
                src_rect.x = 0;
                src_rect.y = 0;
                src_rect.height = art_frame_data.height;
                src_rect.width = art_frame_data.width;

                dst_rect.x = x + (width - art_frame_data.width) / 2;
                dst_rect.y = y + (height - art_frame_data.height) / 2;
                dst_rect.width = art_frame_data.width;
                dst_rect.height = art_frame_data.height;

                if (dst_rect.y < 8) {
                    int clipped = 8 - dst_rect.y;
                    src_rect.y += clipped;
                    src_rect.height -= clipped;
                    dst_rect.y = 8;
                    dst_rect.height -= clipped;
                }
                if (dst_rect.y + dst_rect.height > 392) {
                    dst_rect.height = 392 - dst_rect.y;
                    src_rect.height = dst_rect.height;
                }
                if (src_rect.height > 0) {
                    tig_window_blit_art(inven_ui_window_handle, &art_blit_info);
                }
            }

            if (palette_modify_info.dst_palette != NULL) {
                tig_palette_destroy(palette_modify_info.dst_palette);
            }
        }
    }

    // 0x577837
    art_blit_info.flags = 0;
    if (text_rects != NULL) {
        for (index = 0; index < 9; index++) {
            if (!filled_inv_slots[index]) {
                tig_art_interface_id_create(item_ui_item_silhouette_nums[index], 0, 0, 0, &(art_blit_info.art_id));

                dst_rect.x = text_rects[index].x;
                dst_rect.y = text_rects[index].y;
                dst_rect.width = text_rects[index].width;
                dst_rect.height = text_rects[index].height;

                src_rect.x = 0;
                src_rect.y = 0;
                src_rect.width = inven_ui_inventory_paperdoll_inv_slot_rects[index].width;
                src_rect.height = inven_ui_inventory_paperdoll_inv_slot_rects[index].height;

                tig_window_blit_art(inven_ui_window_handle, &art_blit_info);
            }
        }
    }

    // 0x5778D8
    if (inven_ui_target_inventory_owner != OBJ_HANDLE_NULL
        && inven_ui_mode != INVEN_UI_MODE_NPC_IDENTIFY
        && inven_ui_mode != INVEN_UI_MODE_NPC_REPAIR) {
        int64_t target_obj;
        int grid_top;
        int grid_height;
        int grid_left;
        int inventory_list_fld;

        if (inven_ui_target_panel == INVEN_UI_PANEL_PAPERDOLL) {
            target_obj = inven_ui_target_obj;
            memset(filled_inv_slots, 0, sizeof(filled_inv_slots));

            if (inven_ui_mode == INVEN_UI_MODE_BARTER) {
                tig_art_interface_id_create(337, 0, 0, 0, &(art_blit_info.art_id));
            } else {
                tig_art_interface_id_create(349, 0, 0, 0, &(art_blit_info.art_id));
            }
            if (tig_art_frame_data(art_blit_info.art_id, &art_frame_data) != TIG_OK) {
                return;
            }

            src_rect.x = 0;
            src_rect.y = 0;
            src_rect.width = art_frame_data.width;
            src_rect.height = art_frame_data.width;

            dst_rect.x = 0;
            dst_rect.y = 0;
            dst_rect.width = art_frame_data.width;
            dst_rect.height = art_frame_data.width;

            if (inven_ui_mode == INVEN_UI_MODE_BARTER) {
                dst_rect.y = 157;
                text_rects = inven_ui_barter_npc_paperdoll_inv_slot_rects;
            } else {
                dst_rect.y = 134;
                text_rects = inven_ui_loot_npc_paperdoll_inv_slot_rects;
            }

            art_blit_info.flags = 0;
            art_blit_info.src_rect = &src_rect;
            art_blit_info.dst_rect = &dst_rect;
            tig_window_blit_art(inven_ui_window_handle, &art_blit_info);

            tig_font_push(inven_ui_detail_font);
            snprintf(str, sizeof(str), "%d", item_total_attack(inven_ui_target_obj));
            font_desc.str = str;
            font_desc.width = 0;
            tig_font_measure(&font_desc);
            src_rect.x = inven_ui_barter_npc_total_stat_rects[INVEN_UI_TOTAL_ATTACK].x + inven_ui_barter_npc_total_stat_rects[INVEN_UI_TOTAL_ATTACK].width - font_desc.width;
            src_rect.y = inven_ui_barter_npc_total_stat_rects[INVEN_UI_TOTAL_ATTACK].y;
            src_rect.width = inven_ui_barter_npc_total_stat_rects[INVEN_UI_TOTAL_ATTACK].width;
            src_rect.height = inven_ui_barter_npc_total_stat_rects[INVEN_UI_TOTAL_ATTACK].height;
            tig_window_text_write(inven_ui_window_handle, str, &src_rect);
            snprintf(str, sizeof(str), "%d", item_total_defence(inven_ui_target_obj));
            tig_window_text_write(inven_ui_window_handle, str, &(inven_ui_barter_npc_total_stat_rects[INVEN_UI_TOTAL_DEFENSE]));
            tig_font_pop();
        } else {
            target_obj = inven_ui_target_inventory_owner;
            grid_left = 8;
            if (inven_ui_mode == INVEN_UI_MODE_BARTER) {
                grid_top = 168;
                grid_height = 224;
            } else {
                grid_top = 136;
                grid_height = 256;
            }
        }

        if (obj_field_int32_get(target_obj, OBJ_F_TYPE) == OBJ_TYPE_CONTAINER) {
            inventory_cnt = obj_field_int32_get(target_obj, OBJ_F_CONTAINER_INVENTORY_NUM);
            inventory_list_fld = OBJ_F_CONTAINER_INVENTORY_LIST_IDX;
        } else {
            inventory_cnt = obj_field_int32_get(target_obj, OBJ_F_CRITTER_INVENTORY_NUM);
            inventory_list_fld = OBJ_F_CRITTER_INVENTORY_LIST_IDX;
        }

        src_rect.x = 0;
        src_rect.y = 0;

        art_blit_info.src_rect = &src_rect;
        art_blit_info.dst_rect = &dst_rect;

        for (index = 0; index < inventory_cnt; index++) {
            item_obj = obj_arrayfield_handle_get(target_obj, inventory_list_fld, index);
            if ((obj_field_int32_get(item_obj, OBJ_F_FLAGS) & OF_OFF) != 0) {
                continue;
            }

            item_type = obj_field_int32_get(item_obj, OBJ_F_TYPE);
            item_flags = obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS);
            if ((item_flags & OIF_NO_DISPLAY) != 0) {
                if (!initial_draw || (item_type != OBJ_TYPE_GOLD && item_type != OBJ_TYPE_AMMO)) {
                    continue;
                }
                item_flags &= ~OIF_NO_DISPLAY;
                obj_field_int32_set(item_obj, OBJ_F_ITEM_FLAGS, item_flags);
            }

            inventory_location = item_inventory_location_get(item_obj);

            art_blit_info.flags = 0;
            if (IS_HOTKEY_INV_LOC(inventory_location)) {
                continue;
            }

            if (IS_WEAR_INV_LOC(inventory_location)) {
                // 0x577CA0
                if (inven_ui_target_panel == INVEN_UI_PANEL_PAPERDOLL) {
                    weapon_too_heavy = false;
                    if (inventory_location == ITEM_INV_LOC_WEAPON) {
                        weapon_min_str = item_weapon_min_strength(item_obj, target_obj);
                        if (stat_level_get(target_obj, STAT_STRENGTH) < weapon_min_str) {
                            dst_rect = text_rects[4];
                            tig_window_tint(inven_ui_window_handle,
                                &dst_rect,
                                tig_color_make(64, 0, 0),
                                0);
                            weapon_too_heavy = true;
                        }
                    }

                    switch (item_type) {
                    case OBJ_TYPE_WEAPON:
                        art_blit_info.art_id = obj_field_int32_get(item_obj, OBJ_F_WEAPON_PAPER_DOLL_AID);
                        break;
                    case OBJ_TYPE_ARMOR:
                        art_blit_info.art_id = obj_field_int32_get(item_obj, OBJ_F_ARMOR_PAPER_DOLL_AID);
                        break;
                    default:
                        art_blit_info.art_id = obj_field_int32_get(item_obj, OBJ_F_ITEM_INV_AID);
                        break;
                    }

                    if (tig_art_frame_data(art_blit_info.art_id, &art_frame_data) == TIG_OK) {
                        src_rect.width = art_frame_data.width;
                        src_rect.height = art_frame_data.height;

                        dst_rect.width = art_frame_data.width * 2 / 3;
                        dst_rect.height = art_frame_data.height * 2 / 3;
                        dst_rect.x = text_rects[inventory_location - 1000].x + (text_rects[inventory_location - 1000].width - dst_rect.width) / 2;
                        dst_rect.y = text_rects[inventory_location - 1000].y + (text_rects[inventory_location - 1000].height - dst_rect.height) / 2;

                        tig_window_blit_art(inven_ui_window_handle, &art_blit_info);

                        if (weapon_too_heavy) {
                            mes_file_entry1.num = 8;
                            mes_get_msg(inven_ui_mes_file, &mes_file_entry1);
                            snprintf(str, sizeof(str), "%s: %d", mes_file_entry1.str, weapon_min_str);
                            tig_font_push(inven_ui_detail_font);
                            font_desc.str = str;
                            font_desc.width = 0;
                            tig_font_measure(&font_desc);
                            dst_rect.y += dst_rect.height - font_desc.height;
                            dst_rect.height = font_desc.height;
                            tig_window_text_write(inven_ui_window_handle, str, &dst_rect);
                            tig_font_pop();
                        }

                        filled_inv_slots[inventory_location - 1000] = true;
                    }
                }

                continue;
            }

            // 0x577F41
            if (inven_ui_target_panel == INVEN_UI_PANEL_INVENTORY) {
                int x;
                int y;
                int width;
                int height;

                x = grid_left + 32 * (inventory_location % 10);
                y = grid_top + 32 * (inventory_location / 10 - inven_ui_target_scroll_offset);

                if (y < grid_height + grid_top) {
                    art_blit_info.art_id = obj_field_int32_get(item_obj, OBJ_F_ITEM_INV_AID);
                    if (tig_art_item_id_destroyed_get(obj_field_int32_get(item_obj, OBJ_F_CURRENT_AID))) {
                        art_blit_info.art_id = tig_art_item_id_destroyed_set(art_blit_info.art_id, 0);
                        tig_art_anim_data(art_blit_info.art_id, &art_anim_data);

                        palette_modify_info.dst_palette = tig_palette_create();
                        palette_modify_info.src_palette = art_anim_data.palette1;
                        palette_modify_info.flags = TIG_PALETTE_MODIFY_TINT;
                        palette_modify_info.tint_color = tig_color_make(255, 0, 0);
                        tig_palette_modify(&palette_modify_info);

                        art_blit_info.flags = TIG_ART_BLT_PALETTE_OVERRIDE;
                        art_blit_info.palette = palette_modify_info.dst_palette;
                    } else {
                        palette_modify_info.dst_palette = NULL;
                    }

                    if (tig_art_frame_data(art_blit_info.art_id, &art_frame_data) == TIG_OK) {
                        if (y + art_frame_data.height > grid_top) {
                            src_rect.x = 0;
                            src_rect.y = 0;
                            src_rect.width = art_frame_data.width;
                            src_rect.height = art_frame_data.height;

                            item_inv_icon_size(item_obj, &width, &height);
                            x += (32 * width - art_frame_data.width) / 2;
                            y += (32 * height - art_frame_data.height) / 2;

                            if (y < grid_top) {
                                height = y - grid_top + src_rect.height;
                                src_rect.y += grid_top - y;
                                src_rect.height = height;
                                y = grid_top;
                            } else {
                                height = src_rect.height;
                            }

                            if (y + height > grid_top + grid_height) {
                                height = grid_top + grid_height - y;
                                src_rect.height = height;
                            }

                            dst_rect.x = x;
                            dst_rect.y = y;
                            dst_rect.width = src_rect.width;
                            dst_rect.height = height;

                            tig_window_blit_art(inven_ui_window_handle, &art_blit_info);
                        }
                    }

                    if (palette_modify_info.dst_palette != NULL) {
                        tig_palette_destroy(palette_modify_info.dst_palette);
                    }
                }
            }
        }

        art_blit_info.flags = 0;
        if (inven_ui_target_panel == INVEN_UI_PANEL_PAPERDOLL) {
            for (index = 0; index < 9; index++) {
                if (!filled_inv_slots[index]) {
                    tig_art_interface_id_create(item_ui_item_silhouette_nums[index], 0, 0, 0, &(art_blit_info.art_id));

                    dst_rect.x = text_rects[index].x;
                    dst_rect.y = text_rects[index].y;
                    dst_rect.width = text_rects[index].width;
                    dst_rect.height = text_rects[index].height;

                    src_rect.width = inven_ui_inventory_paperdoll_inv_slot_rects[index].width;
                    src_rect.height = inven_ui_inventory_paperdoll_inv_slot_rects[index].height;

                    tig_window_blit_art(inven_ui_window_handle, &art_blit_info);
                }
            }
        }
    }

    intgame_pc_lens_redraw();
    scrollbar_ui_control_redraw(inven_ui_target_inventory_scrollbar);
    if (inven_ui_player_scrollbar_created) {
        bool show = inven_ui_mode == INVEN_UI_MODE_INVENTORY
            || inven_ui_mode == INVEN_UI_MODE_NPC_IDENTIFY || inven_ui_mode == INVEN_UI_MODE_NPC_REPAIR
            || inven_ui_panel == INVEN_UI_PANEL_INVENTORY;
        if (show) {
            scrollbar_ui_control_show(inven_ui_player_inventory_scrollbar);
            scrollbar_ui_control_redraw(inven_ui_player_inventory_scrollbar);
        } else {
            scrollbar_ui_control_hide(inven_ui_player_inventory_scrollbar);
        }
    }
}

// 0x5782D0
tig_art_id_t inven_ui_background_art_id_get(void)
{
    tig_art_id_t art_id;
    int num;

    if (inven_ui_mode == INVEN_UI_MODE_INVENTORY
        || inven_ui_mode == INVEN_UI_MODE_NPC_IDENTIFY
        || inven_ui_mode == INVEN_UI_MODE_NPC_REPAIR) {
        num = 223;
    } else if (inven_ui_mode == INVEN_UI_MODE_BARTER) {
        num = inven_ui_cycle_left_btn != -1 ? 825 : 235;
    } else {
        num = 234;
    }

    tig_art_interface_id_create(num, 0, 0, 0, &art_id);

    return art_id;
}

// 0x578330
void inven_ui_trade_offer_refresh(int64_t item_obj, int64_t owner_obj)
{
    int gold;
    int pos;

    if (inven_ui_target_is_follower) {
        inven_ui_trade_price = 0;
        return;
    }

    if (owner_obj == inven_ui_pc_obj) {
        inven_ui_trade_price = item_cost(item_obj, inven_ui_pc_obj, inven_ui_target_obj, 0);
        if (inven_ui_trade_price != 0) {
            gold = item_gold_get(inven_ui_target_obj);
            if (object_script_execute(item_obj, inven_ui_target_obj, inven_ui_pc_obj, SAP_BUY_OBJECT, 0)) {
                pos = 0;
            } else {
                dialog_copy_npc_normally_wont_buy_msg(inven_ui_target_obj, inven_ui_pc_obj, inven_ui_trade_dialog_text);
                pos = (int)strlen(inven_ui_trade_dialog_text);
            }

            if (inven_ui_trade_price > gold) {
                dialog_copy_npc_buy_for_less_msg(inven_ui_target_obj, inven_ui_pc_obj, inven_ui_offer_text_buffer);
                snprintf(&(inven_ui_trade_dialog_text[pos]), sizeof(inven_ui_trade_dialog_text) - pos,
                    inven_ui_offer_text_buffer,
                    inven_ui_trade_price,
                    gold);
            } else {
                dialog_copy_npc_buy_msg(inven_ui_target_obj, inven_ui_pc_obj, inven_ui_offer_text_buffer);
                snprintf(&(inven_ui_trade_dialog_text[pos]), sizeof(inven_ui_trade_dialog_text) - pos,
                    inven_ui_offer_text_buffer,
                    inven_ui_trade_price);
            }
        } else {
            if ((obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS) & OIF_STOLEN) == 0) {
                dialog_copy_npc_wont_buy_msg(inven_ui_target_obj, inven_ui_pc_obj, inven_ui_trade_dialog_text);
            } else {
                dialog_copy_npc_wont_buy_stolen_msg(inven_ui_target_obj, inven_ui_pc_obj, inven_ui_trade_dialog_text);
            }
        }
    } else {
        inven_ui_trade_price = item_cost(item_obj, inven_ui_target_obj, inven_ui_pc_obj, 0);
        if (inven_ui_trade_price != 0) {
            if ((obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS) & OIF_WONT_SELL) != 0
                || IS_WEAR_INV_LOC(item_inventory_location_get(item_obj))) {
                dialog_copy_npc_normally_wont_sell_msg(inven_ui_target_obj, inven_ui_pc_obj, inven_ui_trade_dialog_text);
                pos = (int)strlen(inven_ui_trade_dialog_text);
            } else {
                pos = 0;
            }

            dialog_copy_npc_sell_msg(inven_ui_target_obj, inven_ui_pc_obj, inven_ui_offer_text_buffer);
            snprintf(&(inven_ui_trade_dialog_text[pos]), sizeof(inven_ui_trade_dialog_text) - pos,
                inven_ui_offer_text_buffer,
                inven_ui_trade_price);

            if (basic_skill_training_get(inven_ui_pc_obj, BASIC_SKILL_HAGGLE) >= TRAINING_APPRENTICE) {
                int worth;
                int discount;

                worth = item_worth(item_obj);
                discount = 100 * (inven_ui_trade_price - worth) / worth;
                if (discount == 0) {
                    discount = 1;
                }

                snprintf(inven_ui_offer_text_buffer, sizeof(inven_ui_offer_text_buffer), "(%d%%)", discount);
                strcat(inven_ui_trade_dialog_text, inven_ui_offer_text_buffer);
            }
        } else {
            dialog_copy_npc_wont_sell_msg(inven_ui_target_obj, inven_ui_pc_obj, inven_ui_trade_dialog_text);
        }
    }

    redraw_inven(false);
}

// 0x5786C0
void inven_ui_identification_quote_show(int64_t obj)
{
    if (item_is_identified(obj)) {
        dialog_copy_npc_wont_identify_already_msg(inven_ui_target_obj, inven_ui_pc_obj, inven_ui_trade_dialog_text);
    } else {
        inven_ui_trade_price = spell_money(SPELL_DIVINE_MAGICK);
        dialog_copy_npc_identify_msg(inven_ui_target_obj, inven_ui_pc_obj, inven_ui_offer_text_buffer);
        snprintf(inven_ui_trade_dialog_text, sizeof(inven_ui_trade_dialog_text), inven_ui_offer_text_buffer, inven_ui_trade_price);
    }
    redraw_inven(false);
}

// 0x578760
void inven_ui_repair_offer_refresh(int64_t obj)
{
    int hp_max;
    int hp_dam;
    tig_art_id_t art_id;

    hp_max = object_hp_max(obj);
    art_id = obj_field_int32_get(obj, OBJ_F_CURRENT_AID);
    if (tig_art_item_id_destroyed_get(art_id) != 0) {
        if (tech_skill_training_get(inven_ui_target_obj, TECH_SKILL_REPAIR) != TRAINING_MASTER) {
            dialog_copy_npc_wont_repair_broken_msg(inven_ui_target_obj, inven_ui_pc_obj, inven_ui_trade_dialog_text);
            redraw_inven(false);
            return;
        }

        hp_dam = hp_max;
    } else {
        hp_dam = object_hp_damage_get(obj);
    }

    if (hp_dam != 0) {
        inven_ui_trade_price = hp_dam * (item_cost(obj, inven_ui_pc_obj, inven_ui_target_obj, 1) / 2) / hp_max;
        if (inven_ui_trade_price < 2) {
            inven_ui_trade_price = 2;
        }

        dialog_copy_npc_repair_msg(inven_ui_target_obj, inven_ui_pc_obj, inven_ui_offer_text_buffer);
        snprintf(inven_ui_trade_dialog_text, sizeof(inven_ui_trade_dialog_text), inven_ui_offer_text_buffer, inven_ui_trade_price);
        redraw_inven(false);
    } else {
        dialog_copy_npc_wont_repair_undamaged_msg(inven_ui_target_obj, inven_ui_pc_obj, inven_ui_trade_dialog_text);
        redraw_inven(false);
    }
}

// 0x5788C0
void inven_ui_drag_item_insert_or_transfer(int64_t item_obj, int64_t target_obj, int new_inventory_location, int transaction_flags)
{
    int reason;
    int64_t parent_obj;
    int old_inventory_location;
    int sound_id;
    int obj_type;
    int qty_fld;
    int qty;

    multiplayer_drag_item_show(player_get_local_pc_obj(), item_obj);

    if (target_obj == inven_ui_drag_original_owner) {
        reason = item_check_remove(item_obj);
        if (reason == 0) {
            if (!item_parent(item_obj, &parent_obj)) {
                return;
            }

            old_inventory_location = item_inventory_location_get(item_obj);
            if (!IS_WEAR_INV_LOC(new_inventory_location)) {
                if (target_obj == inven_ui_pc_obj) {
                    item_inventory_slots_set(item_obj, old_inventory_location, inven_ui_player_inventory_slots, INVENTORY_CRITTER_CELLS, 0);
                    if (!item_inventory_slots_has_room_for(item_obj, target_obj, new_inventory_location, inven_ui_player_inventory_slots)) {
                        return;
                    }
                } else {
                    item_inventory_slots_set(item_obj, old_inventory_location, inven_ui_target_inventory_slots, INVENTORY_CONTAINER_CELLS, 0);
                    if (!item_inventory_slots_has_room_for(item_obj, target_obj, new_inventory_location, inven_ui_target_inventory_slots)) {
                        return;
                    }
                }
            }

            critter_encumbrance_recalc_feedback_disable();

            reason = item_check_bulk_insert(item_obj, target_obj, new_inventory_location);
            if (reason == ITEM_CANNOT_OK) {
                item_remove(item_obj);
                item_insert(item_obj, target_obj, new_inventory_location);
            } else {
                item_error_msg(target_obj, reason);
            }
            critter_encumbrance_recalc_feedback_enable();
        } else {
            if (item_parent(item_obj, &parent_obj)) {
                item_error_msg(item_obj, reason);
            }
        }

        sound_id = sfx_item_sound(item_obj, target_obj, OBJ_HANDLE_NULL, ITEM_SOUND_DROP);
        gsound_play_sfx(sound_id, 1);
    } else {
        obj_type = obj_field_int32_get(item_obj, OBJ_F_TYPE);
        inven_ui_transaction_source_obj = inven_ui_drag_original_owner;
        inven_ui_transaction_destination_slot = new_inventory_location;
        inven_ui_transaction_target_obj = target_obj;
        inven_ui_transaction_item_obj = item_obj;

        if ((transaction_flags & 0x6) != 0) {
            inven_ui_trade_price = -1;
        }

        inven_ui_transaction_price = inven_ui_trade_price;
        inven_ui_transaction_flags = transaction_flags;

        if ((obj_type == OBJ_TYPE_GOLD || obj_type == OBJ_TYPE_AMMO)) {
            inven_ui_drag_cancel_and_restore();

            item_stack_counter_field(item_obj, &qty_fld);
            qty = obj_field_int32_get(item_obj, qty_fld);
            if (qty != 1) {
                if ((inven_ui_mode == INVEN_UI_MODE_LOOT
                        && (transaction_flags & 0x01) != 0
                        && target_obj == inven_ui_pc_obj)
                    || (transaction_flags & 0x20) != 0) {
                    inven_ui_item_transaction_send(qty);
                } else {
                    intgame_mode_set(INTGAME_MODE_QUANTITY);
                    iso_interface_window_set(ROTWIN_TYPE_QUANTITY);
                    intgame_refresh_cursor();
                }

                return;
            }
        }

        inven_ui_item_transaction_send(1);
    }
}

// 0x578B80
void inven_ui_item_transaction_send(int transaction_quantity)
{
    InventoryTransactionPacket* pkt;

    pkt = (InventoryTransactionPacket*)MALLOC(sizeof(*pkt));
    pkt->type = 81;
    pkt->price = inven_ui_transaction_price;
    pkt->transaction_flags = inven_ui_transaction_flags;

    if (inven_ui_transaction_source_obj != OBJ_HANDLE_NULL) {
        pkt->source_owner_id = obj_get_id(inven_ui_transaction_source_obj);
    } else {
        pkt->source_owner_id.type = OID_TYPE_NULL;
    }

    pkt->destination_slot = inven_ui_transaction_destination_slot;

    if (inven_ui_transaction_item_obj != OBJ_HANDLE_NULL) {
        pkt->item_id = obj_get_id(inven_ui_transaction_item_obj);
    } else {
        pkt->item_id.type = OID_TYPE_NULL;
    }

    if (inven_ui_transaction_target_obj != OBJ_HANDLE_NULL) {
        pkt->destination_owner_id = obj_get_id(inven_ui_transaction_target_obj);
    } else {
        pkt->destination_owner_id.type = OID_TYPE_NULL;
    }

    if (inven_ui_target_obj != OBJ_HANDLE_NULL) {
        pkt->merchant_id = obj_get_id(inven_ui_target_obj);
    } else {
        pkt->merchant_id.type = OID_TYPE_NULL;
    }

    if (inven_ui_target_inventory_owner != OBJ_HANDLE_NULL) {
        pkt->target_inventory_owner_id = obj_get_id(inven_ui_target_inventory_owner);
    } else {
        pkt->target_inventory_owner_id.type = OID_TYPE_NULL;
    }

    if (inven_ui_pc_obj != OBJ_HANDLE_NULL) {
        pkt->pc_id = obj_get_id(inven_ui_pc_obj);
    } else {
        pkt->pc_id.type = OID_TYPE_NULL;
    }

    pkt->quantity = transaction_quantity;
    pkt->client_id = tig_net_local_client_id_get();

    if (tig_net_is_active()) {
        object_examine(obj_pool_perm_lookup(pkt->item_id),
            obj_pool_perm_lookup(pkt->item_id),
            inven_ui_transaction_item_name);
        objid_id_to_str(inven_ui_transaction_object_id_text, pkt->item_id);

        if (pkt->destination_owner_id.type != OID_TYPE_NULL) {
            object_examine(obj_pool_perm_lookup(pkt->destination_owner_id),
                obj_pool_perm_lookup(pkt->destination_owner_id),
                inven_ui_transaction_target_name);
        } else {
            strcpy(inven_ui_transaction_target_name, "nobody");
        }

        if (pkt->source_owner_id.type != OID_TYPE_NULL) {
            object_examine(obj_pool_perm_lookup(pkt->source_owner_id),
                obj_pool_perm_lookup(pkt->source_owner_id),
                inven_ui_transaction_source_name);
        } else {
            strcpy(inven_ui_transaction_source_name, "nobody");
        }

        if (pkt->quantity > 1) {
            tig_debug_printf("UI: InvenItemTransfer - %d %ss(%s) going from %s to %s\n",
                pkt->quantity,
                inven_ui_transaction_item_name,
                inven_ui_transaction_object_id_text,
                inven_ui_transaction_source_name,
                inven_ui_transaction_target_name);
        } else {
            tig_debug_printf("UI: InvenItemTransfer - %s(%s) going from %s to %s\n",
                inven_ui_transaction_item_name,
                inven_ui_transaction_object_id_text,
                inven_ui_transaction_source_name,
                inven_ui_transaction_target_name);
        }
    }

    inven_ui_item_transaction_process(pkt);
}

// 0x578EA0
bool inven_ui_item_transaction_process(InventoryTransactionPacket* pkt)
{
    int64_t source_owner;
    int64_t item_obj;
    int64_t destination_owner;
    int64_t merchant_obj;
    int64_t inventory_target_obj;
    int64_t pc_obj;
    int qty;
    int gold_amt;
    int ammo_type;
    unsigned int flags;
    int cost;
    int inventory_location;
    bool error;
    unsigned int item_type;
    int worth;

    qty = pkt->quantity;
    flags = pkt->transaction_flags;
    cost = pkt->price;
    inventory_location = pkt->destination_slot;

    source_owner = obj_pool_perm_lookup(pkt->source_owner_id);
    item_obj = obj_pool_perm_lookup(pkt->item_id);
    destination_owner = obj_pool_perm_lookup(pkt->destination_owner_id);
    merchant_obj = obj_pool_perm_lookup(pkt->merchant_id);
    inventory_target_obj = obj_pool_perm_lookup(pkt->target_inventory_owner_id);
    pc_obj = obj_pool_perm_lookup(pkt->pc_id);

    if (tig_net_is_active()
        && !tig_net_is_host()) {
        tig_net_send_app_all(pkt, sizeof(*pkt));
    }

    FREE(pkt);

    if (qty == 0) {
        return true;
    }

    if (pc_obj != OBJ_HANDLE_NULL
        && player_is_local_pc_obj(pc_obj)
        && (flags & 0x18) != 0) {
        gold_amt = qty * item_cost(item_obj, merchant_obj, pc_obj, false);
        if (gold_amt > item_gold_get(pc_obj)) {
            dialog_copy_npc_gamble_msg(merchant_obj, pc_obj, 4, inven_ui_offer_text_buffer);
            snprintf(inven_ui_trade_dialog_text, sizeof(inven_ui_trade_dialog_text), inven_ui_offer_text_buffer, gold_amt);
            return true;
        }
    }

    error = false;
    item_type = obj_field_int32_get(item_obj, OBJ_F_TYPE);

    if ((flags & 0x10) == 0) {
        if (item_type == OBJ_TYPE_GOLD) {
            if (!tig_net_is_active() || tig_net_is_host()) {
                if (!item_gold_transfer(source_owner, destination_owner, qty, item_obj)) {
                    return true;
                }

                if ((flags & 0x06) != 0) {
                    int64_t loc = obj_field_int64_get(source_owner, OBJ_F_LOCATION);
                    int64_t gold_obj = item_gold_create(qty, loc);
                    if (pc_obj != OBJ_HANDLE_NULL) {
                        int sound_id = sfx_item_sound(gold_obj, source_owner, OBJ_HANDLE_NULL, ITEM_SOUND_DROP);
                        mp_player_sound_play(pc_obj, sound_id);
                    }
                    if ((flags & 0x04) != 0) {
                        object_pickup(gold_obj, source_owner);
                        if (pc_obj != OBJ_HANDLE_NULL && player_is_local_pc_obj(pc_obj)) {
                            inven_ui_item_drag_begin(gold_obj);
                            inven_ui_drag_original_owner = source_owner;
                        }
                    } else if ((flags & 0x02) != 0) {
                        object_pickup(gold_obj, source_owner);
                        item_drop_into_junk_pile_with_decay(gold_obj, loc);
                    }
                }
            } else {
                if ((flags & 0x04) != 0) {
                    inven_ui_drag_original_owner = source_owner;
                }
            }
        } else if (item_type == OBJ_TYPE_AMMO) {
            ammo_type = obj_field_int32_get(item_obj, OBJ_F_AMMO_TYPE);
            if (!tig_net_is_active() || tig_net_is_host()) {
                if (!item_ammo_transfer(source_owner, destination_owner, qty, ammo_type, item_obj)) {
                    return true;
                }

                if ((flags & 0x06) != 0) {
                    int64_t loc = obj_field_int64_get(source_owner, OBJ_F_LOCATION);
                    int64_t ammo_obj = item_ammo_create(qty, ammo_type, loc);
                    if (pc_obj != OBJ_HANDLE_NULL) {
                        int sound_id = sfx_item_sound(ammo_obj, source_owner, OBJ_HANDLE_NULL, ITEM_SOUND_PICKUP);
                        mp_player_sound_play(pc_obj, sound_id);
                    }
                    if ((flags & 0x04) != 0) {
                        object_pickup(ammo_obj, source_owner);
                        if (pc_obj != OBJ_HANDLE_NULL && player_is_local_pc_obj(pc_obj)) {
                            inven_ui_item_drag_begin(ammo_obj);
                            inven_ui_drag_original_owner = source_owner;
                        }
                    } else if ((flags & 0x02) != 0) {
                        object_pickup(ammo_obj, source_owner);
                        item_drop_into_junk_pile_with_decay(ammo_obj, loc);
                    }
                }
            } else {
                if ((flags & 0x04) != 0) {
                    inven_ui_drag_original_owner = source_owner;
                }
            }
        } else {
            if ((flags & 0x04) != 0) {
                if (!tig_net_is_active() || tig_net_is_host()) {
                    if (item_check_remove(item_obj) == ITEM_CANNOT_OK) {
                        item_remove(item_obj);
                    } else {
                        error = true;
                    }
                }
            } else if ((flags & 0x01) != 0) {
                if (!tig_net_is_active() || tig_net_is_host()) {
                    if (destination_owner != pc_obj) {
                        if (IS_WEAR_INV_LOC(inventory_location)) {
                            if (!item_transfer_ex(item_obj, merchant_obj, inventory_location)) {
                                error = true;
                            }
                        } else {
                            if (!item_transfer_ex(item_obj, inventory_target_obj, inventory_location)) {
                                error = true;
                            }
                        }
                    } else {
                        if (!item_transfer_ex(item_obj, destination_owner, inventory_location)) {
                            error = true;
                        }
                    }

                    if (pc_obj != OBJ_HANDLE_NULL) {
                        int sound_id = sfx_item_sound(item_obj, destination_owner, OBJ_HANDLE_NULL, ITEM_SOUND_DROP);
                        mp_player_sound_play(pc_obj, sound_id);
                    }
                }
            } else if ((flags & 0x02) != 0) {
                if (!tig_net_is_active() || tig_net_is_host()) {
                    item_drop(item_obj);
                }
                if (pc_obj != OBJ_HANDLE_NULL) {
                    int sound_id = sfx_item_sound(item_obj, destination_owner, OBJ_HANDLE_NULL, ITEM_SOUND_DROP);
                    mp_player_sound_play(pc_obj, sound_id);
                }
            }
        }
    }

    // 68
    if ((flags & 0x08) == 0) {
        if ((flags & 0x10) != 0) {
            cost = item_cost(item_obj, merchant_obj, pc_obj, false);
        }
    } else {
        cost = 0;
    }

    worth = item_worth(item_obj) * qty;
    if (!error) {
        if (cost > 0) {
            gold_amt = qty * cost;
            if (!tig_net_is_active() || tig_net_is_host()) {
                if (destination_owner == pc_obj) {
                    if (item_gold_transfer(destination_owner, merchant_obj, gold_amt, OBJ_HANDLE_NULL)) {
                        if (gold_amt < worth) {
                            gsound_play_sfx(8158, 1);
                        } else {
                            reaction_adjust_for_trade_surplus(merchant_obj, pc_obj, gold_amt - worth);
                        }
                    } else {
                        dialog_copy_npc_not_enough_money_msg(merchant_obj, pc_obj, inven_ui_trade_dialog_text);
                        error = true;
                    }
                } else {
                    if (item_gold_transfer(merchant_obj, source_owner, gold_amt, OBJ_HANDLE_NULL)) {
                        if (worth < gold_amt) {
                            gsound_play_sfx(8158, 1);
                        } else {
                            reaction_adjust_for_trade_surplus(merchant_obj, pc_obj, worth - gold_amt);
                        }
                    } else {
                        error = true;
                    }
                }
            }
        }
    }

    if (error) {
        if ((flags & 0x10) == 0) {
            if (!tig_net_is_active() || tig_net_is_host()) {
                if (item_type == OBJ_TYPE_GOLD) {
                    item_gold_transfer(destination_owner, source_owner, qty, OBJ_HANDLE_NULL);
                } else if (item_type == OBJ_TYPE_AMMO) {
                    item_ammo_transfer(destination_owner, source_owner, qty, ammo_type, OBJ_HANDLE_NULL);
                }
            }
        } else {
            gsound_play_sfx(8158, 1);
        }
    }

    // 85
    if (!error
        && destination_owner != OBJ_HANDLE_NULL
        && destination_owner != pc_obj
        && inven_ui_mode == INVEN_UI_MODE_BARTER
        && !inven_ui_target_is_follower
        && (!tig_net_is_active() || tig_net_is_host())) {
        unsigned int item_flags = obj_field_int32_get(item_obj, OBJ_F_ITEM_FLAGS);
        if ((item_flags & OIF_IDENTIFIED) == 0) {
            item_flags |= OIF_IDENTIFIED;
            obj_field_int32_set(item_obj, OBJ_F_ITEM_FLAGS, item_flags);
        }
    }

    if (inven_ui_created && player_is_local_pc_obj(pc_obj)) {
        redraw_inven(false);
        item_inventory_slots_get(pc_obj, inven_ui_player_inventory_slots);
        if (inventory_target_obj != OBJ_HANDLE_NULL) {
            item_inventory_slots_get(inventory_target_obj, inven_ui_target_inventory_slots);
        }
    }

    if (pc_obj != OBJ_HANDLE_NULL
        && player_is_local_pc_obj(pc_obj)
        && (flags & 0x04) != 0
        && !error) {
        inven_ui_close_requested = 1;
    }

    if ((flags & 0x08) != 0) {
        dialog_copy_npc_gamble_msg(merchant_obj, pc_obj, 0, inven_ui_trade_dialog_text);
        reaction_adjust_for_trade_surplus(merchant_obj, pc_obj, -worth);
    } else if ((flags & 0x10) != 0) {
        dialog_copy_npc_gamble_msg(merchant_obj, pc_obj, 1, inven_ui_offer_text_buffer);
        snprintf(inven_ui_trade_dialog_text, sizeof(inven_ui_trade_dialog_text), inven_ui_offer_text_buffer, gold_amt);
    }

    return true;
}

// 0x579760
int64_t inven_ui_transaction_item_get(void)
{
    return inven_ui_transaction_item_obj;
}

// 0x579770
void inven_ui_transfer_visible_items(int64_t from_obj, int64_t to_obj)
{
    int64_t* items;
    int cnt;
    int idx;
    int inventory_location;

    inven_ui_drag_original_owner = from_obj;

    cnt = item_list_get(from_obj, &items);
    for (idx = 0; idx < cnt; idx++) {
        if ((obj_field_int32_get(items[idx], OBJ_F_FLAGS) & OF_OFF) == 0
            && (obj_field_int32_get(items[idx], OBJ_F_ITEM_FLAGS) & OIF_NO_DISPLAY) == 0
            && item_check_insert(items[idx], to_obj, &inventory_location) == ITEM_CANNOT_OK) {
            inven_ui_drag_item_insert_or_transfer(items[idx], to_obj, inventory_location, 0x21);
        }
    }

    item_list_free(items);
}

// 0x579840
bool inven_ui_gambling_offer_check(int64_t obj, bool silent)
{
    int item_price;
    bool gamble_succeeded = false;
    int training;
    size_t pos;

    if (inven_ui_drag_original_owner == inven_ui_pc_obj) {
        if (!silent) {
            dialog_copy_npc_gamble_msg(inven_ui_target_obj, inven_ui_pc_obj, 2, inven_ui_trade_dialog_text);
        }
        return false;
    }

    if (basic_skill_points_get(inven_ui_target_obj, BASIC_SKILL_GAMBLING) == 0) {
        if (!silent) {
            dialog_copy_npc_gamble_msg(inven_ui_target_obj, inven_ui_pc_obj, 9, inven_ui_trade_dialog_text);
        }
        return false;
    }

    item_price = item_cost(obj, inven_ui_target_obj, inven_ui_pc_obj, 0);
    if (item_price > skill_gambling_max_item_cost(inven_ui_pc_obj)) {
        if (!silent) {
            dialog_copy_npc_gamble_msg(inven_ui_target_obj, inven_ui_pc_obj, 3, inven_ui_trade_dialog_text);
        }
        return false;
    }

    if (item_price > item_gold_get(inven_ui_pc_obj)) {
        if (!silent) {
            dialog_copy_npc_gamble_msg(inven_ui_target_obj, inven_ui_pc_obj, 4, inven_ui_offer_text_buffer);
            snprintf(inven_ui_trade_dialog_text, sizeof(inven_ui_trade_dialog_text), inven_ui_offer_text_buffer, item_price);
        }
        return false;
    }

    training = basic_skill_training_get(inven_ui_target_obj, BASIC_SKILL_GAMBLING);
    if (IS_WEAR_INV_LOC(item_inventory_location_get(obj))) {
        if (training < TRAINING_EXPERT) {
            if (!silent) {
                dialog_copy_npc_gamble_msg(inven_ui_target_obj, inven_ui_pc_obj, 5, inven_ui_trade_dialog_text);
            }
            return false;
        }

        gamble_succeeded = true;
    }

    if ((obj_field_int32_get(obj, OBJ_F_ITEM_FLAGS) & OIF_WONT_SELL) != 0) {
        if (training < TRAINING_MASTER) {
            if (!silent) {
                dialog_copy_npc_gamble_msg(inven_ui_target_obj, inven_ui_pc_obj, 6, inven_ui_trade_dialog_text);
            }
            return false;
        }

        gamble_succeeded = true;
    }

    if (!silent) {
        if (gamble_succeeded) {
            dialog_copy_npc_gamble_msg(inven_ui_target_obj, inven_ui_pc_obj, 7, inven_ui_trade_dialog_text);
            strcat(inven_ui_trade_dialog_text, " ");
            pos = strlen(inven_ui_trade_dialog_text);
        } else {
            pos = 0;
        }

        dialog_copy_npc_gamble_msg(inven_ui_target_obj, inven_ui_pc_obj, 8, inven_ui_offer_text_buffer);
        snprintf(&(inven_ui_trade_dialog_text[pos]), sizeof(inven_ui_trade_dialog_text) - pos,
            inven_ui_offer_text_buffer,
            item_price);
    }

    return true;
}

// 0x579B60
void inven_ui_item_gamble(int64_t obj)
{
    SkillInvocation skill_invocation;

    if (inven_ui_gambling_offer_check(obj, 1)) {
        skill_invocation_init(&skill_invocation);
        object_handle_reference_init(inven_ui_pc_obj, &(skill_invocation.source));
        object_handle_reference_init(inven_ui_target_obj, &(skill_invocation.target));
        object_handle_reference_init(obj, &(skill_invocation.item));
        skill_invocation.skill = SKILL_GAMBLING;
        skill_invocation.modifier = 0;
        if (skill_invocation_run(&skill_invocation)) {
            // FIXME: Unclear flags.
            inven_ui_drag_item_insert_or_transfer(obj, inven_ui_pc_obj, 0, (skill_invocation.flags & SKILL_INVOCATION_SUCCESS) != 0 ? 0x12 : 0x11);
            inven_ui_trade_price = -1;
        }
    }
}

// 0x579C40
void inven_ui_target_inventory_scrollbar_create(void)
{
    ScrollbarUiControlInfo info;

    scrollbar_ui_end_ignore_events();

    if (inven_ui_mode == INVEN_UI_MODE_INVENTORY
        || inven_ui_mode == INVEN_UI_MODE_NPC_IDENTIFY
        || inven_ui_mode == INVEN_UI_MODE_NPC_REPAIR) {
        return;
    }

    info.flags = SB_INFO_VALID
        | SB_INFO_CONTENT_RECT
        | SB_INFO_MAX_VALUE
        | SB_INFO_MIN_VALUE
        | SB_INFO_LINE_STEP
        | SB_INFO_VALUE
        | SB_INFO_ON_VALUE_CHANGED
        | SB_INFO_ON_REFRESH;
    info.on_value_changed = inven_ui_target_scroll_value_changed;
    info.value = inven_ui_target_scroll_offset;
    info.max_value = inven_ui_target_max_scroll_offset;
    info.min_value = 0;

    if (inven_ui_mode == 1) {
        info.scrollbar_rect = inven_ui_barter_scrollbar_rect;
        info.content_rect.x = 8;
        info.content_rect.y = 168;
        info.content_rect.width = inven_ui_barter_scrollbar_rect.width + 320;
        info.content_rect.height = 224;
    } else {
        info.scrollbar_rect = inven_ui_loot_scrollbar_rect;
        info.content_rect.x = 8;
        info.content_rect.y = 136;
        info.content_rect.width = inven_ui_loot_scrollbar_rect.width + 320;
        info.content_rect.height = 256;
    }

    info.on_refresh = inven_ui_target_background_draw;
    info.line_step = 1;
    scrollbar_ui_control_create(&inven_ui_target_inventory_scrollbar, &info, inven_ui_window_handle);
    scrollbar_ui_control_redraw(inven_ui_target_inventory_scrollbar);
}

// 0x579D70
void inven_ui_target_inventory_scrollbar_destroy(void)
{
    if (inven_ui_mode == INVEN_UI_MODE_INVENTORY
        || inven_ui_mode == INVEN_UI_MODE_NPC_IDENTIFY
        || inven_ui_mode == INVEN_UI_MODE_NPC_REPAIR) {
        return;
    }

    scrollbar_ui_control_destroy(inven_ui_target_inventory_scrollbar);
    scrollbar_ui_end_ignore_events();
}

// 0x579DA0
void inven_ui_target_inventory_scrollbar_show(void)
{
    if (inven_ui_mode == INVEN_UI_MODE_INVENTORY
        || inven_ui_mode == INVEN_UI_MODE_NPC_IDENTIFY
        || inven_ui_mode == INVEN_UI_MODE_NPC_REPAIR) {
        return;
    }

    scrollbar_ui_control_show(inven_ui_target_inventory_scrollbar);
}

// 0x579DD0
void inven_ui_target_inventory_scrollbar_hide(void)
{
    if (inven_ui_mode == INVEN_UI_MODE_INVENTORY
        || inven_ui_mode == INVEN_UI_MODE_NPC_IDENTIFY
        || inven_ui_mode == INVEN_UI_MODE_NPC_REPAIR) {
        return;
    }

    scrollbar_ui_control_hide(inven_ui_target_inventory_scrollbar);
}

// 0x579E00
void inven_ui_target_scroll_value_changed(int value)
{
    if (inven_ui_mode == INVEN_UI_MODE_INVENTORY
        || inven_ui_mode == INVEN_UI_MODE_NPC_IDENTIFY
        || inven_ui_mode == INVEN_UI_MODE_NPC_REPAIR) {
        return;
    }

    inven_ui_target_scroll_offset = value;
    redraw_inven(false);
}

// 0x579E30
void inven_ui_target_background_draw(TigRect* rect)
{
    TigArtBlitInfo blit_info;

    if (inven_ui_mode == INVEN_UI_MODE_INVENTORY
        || inven_ui_mode == INVEN_UI_MODE_NPC_IDENTIFY
        || inven_ui_mode == INVEN_UI_MODE_NPC_REPAIR) {
        return;
    }

    blit_info.flags = 0;
    blit_info.art_id = inven_ui_background_art_id_get();
    blit_info.src_rect = rect;
    blit_info.dst_rect = rect;
    tig_window_blit_art(inven_ui_window_handle, &blit_info);
}

static void inven_ui_player_scroll_value_changed(int value)
{
    inven_ui_player_scroll_offset = value;
    redraw_inven(false);
}

static void inven_ui_player_background_draw(TigRect* rect)
{
    TigArtBlitInfo info = { 0 };
    info.art_id = inven_ui_background_art_id_get();
    info.src_rect = rect;
    info.dst_rect = rect;
    tig_window_blit_art(inven_ui_window_handle, &info);
}

static void inven_ui_player_inventory_scrollbar_create(void)
{
    ScrollbarUiControlInfo info = { 0 };
    info.flags = SB_INFO_VALID | SB_INFO_CONTENT_RECT | SB_INFO_MAX_VALUE | SB_INFO_MIN_VALUE
        | SB_INFO_VALUE | SB_INFO_LINE_STEP | SB_INFO_PAGE_STEP | SB_INFO_ON_VALUE_CHANGED | SB_INFO_ON_REFRESH;
    info.scrollbar_rect = (TigRect){ 689, 8, 12, 384 };
    info.content_rect = (TigRect){ 368, 8, 333, 384 };
    info.min_value = 0;
    info.max_value = INVENTORY_CRITTER_ROWS - 12;
    info.value = inven_ui_player_scroll_offset;
    info.line_step = 1;
    info.page_step = 12;
    info.on_value_changed = inven_ui_player_scroll_value_changed;
    info.on_refresh = inven_ui_player_background_draw;
    inven_ui_player_scrollbar_created = scrollbar_ui_control_create(&inven_ui_player_inventory_scrollbar, &info, inven_ui_window_handle);
}
