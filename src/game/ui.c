#include "game/ui.h"

// 0x5E8658
static UiCallbacks ui_callbacks;

// 0x5E87C8
static TigIdxTable ui_pending_messages;

// 0x5E87D8
static int ui_next_message_id;

// 0x45FD40
void ui_init(UiCallbacks* callbacks)
{
    if (callbacks != NULL) {
        ui_callbacks = *callbacks;
    }

    tig_idxtable_init(&ui_pending_messages, sizeof(UiMessage));
}

// 0x460150
void ui_exit(void)
{
    tig_idxtable_exit(&ui_pending_messages);
}

// 0x460160
bool ui_timeevent_process(TimeEvent* timeevent)
{
    UiMessage ui_message;

    if (timeevent->params[0].integer_value != 0) {
        tig_debug_printf("UI: Unknown type of timeevent passed to ui_timeevent_process (%d).\n",
            timeevent->params[0].integer_value);
    } else {
        tig_idxtable_get(&ui_pending_messages, timeevent->params[1].integer_value, &ui_message);
        ui_display_msg(&ui_message);
        FREE(ui_message.str);
    }

    return true;
}

// 0x4601C0
void ui_character_panels_refresh(void)
{
    if (ui_callbacks.character_panels_refresh != NULL) {
        ui_callbacks.character_panels_refresh();
    }
}

// 0x4601D0
void ui_object_destroyed_notify(int64_t obj)
{
    if (ui_callbacks.object_destroyed_notify != NULL) {
        ui_callbacks.object_destroyed_notify(obj);
    }
}

// 0x4601F0
void ui_update_inven(int64_t obj)
{
    if (ui_callbacks.update_inven != NULL) {
        ui_callbacks.update_inven(obj);
    }
}

// 0x460240
void ui_refresh_health_bar(int64_t obj)
{
    if (ui_callbacks.refresh_health_bar != NULL) {
        ui_callbacks.refresh_health_bar(obj);
    }
}

// 0x460260
void ui_refresh_fatigue_bar(int64_t obj)
{
    if (ui_callbacks.refresh_fatigue_bar != NULL) {
        ui_callbacks.refresh_fatigue_bar(obj);
    }
}

// 0x460280
void ui_selected_object_set(int64_t obj)
{
    if (ui_callbacks.selected_object_set != NULL) {
        ui_callbacks.selected_object_set(obj);
    }
}

// 0x4602A0
void ui_notify_item_inserted_or_removed(int64_t item_obj, bool removed, int inventory_location)
{
    if (ui_callbacks.notify_item_inserted_or_removed != NULL) {
        ui_callbacks.notify_item_inserted_or_removed(item_obj, removed, inventory_location);
    }
}

// 0x4602D0
void ui_show_inven_loot(int64_t pc_obj, int64_t target_obj)
{
    if (ui_callbacks.show_inven_loot != NULL) {
        ui_callbacks.show_inven_loot(pc_obj, target_obj);
    }
}

// 0x460300
void ui_show_inven_identify(int64_t pc_obj, int64_t target_obj)
{
    if (ui_callbacks.show_inven_identify != NULL) {
        ui_callbacks.show_inven_identify(pc_obj, target_obj);
    }
}

// 0x460330
void ui_tech_adjust_degree(int64_t obj, int tech, int action)
{
    if (ui_callbacks.tech_adjust_degree != NULL) {
        ui_callbacks.tech_adjust_degree(obj, tech, action);
    }
}

// 0x460360
void ui_skill_preprocess(int64_t obj, int type)
{
    if (ui_callbacks.skill_preprocess != NULL) {
        ui_callbacks.skill_preprocess(obj, type);
    }
}

// 0x460380
void ui_skill_activate(TargetDescriptor* td, int64_t obj, int skill)
{
    if (ui_callbacks.skill_activate != NULL) {
        ui_callbacks.skill_activate(td, obj, skill);
    }
}

// 0x4603B0
void ui_adjust_skill(int64_t obj, int skill, int action)
{
    if (ui_callbacks.adjust_skill != NULL) {
        ui_callbacks.adjust_skill(obj, skill, action);
    }
}

// 0x4603E0
void ui_set_skill_training(int64_t obj, int skill, int training)
{
    if (ui_callbacks.set_skill_training != NULL) {
        ui_callbacks.set_skill_training(obj, skill, training);
    }
}

// 0x460410
void ui_dialog_script_and_greeting(int64_t pc_obj, int64_t npc_obj)
{
    if (ui_callbacks.dialog_script_and_greeting != NULL) {
        ui_callbacks.dialog_script_and_greeting(pc_obj, npc_obj);
    }
}

// 0x460460
int ui_is_in_dialog(int64_t obj)
{
    if (ui_callbacks.is_in_dialog != NULL) {
        return ui_callbacks.is_in_dialog(obj);
    } else {
        return 0;
    }
}

// 0x460480
void ui_spell_add(int64_t obj, int spell)
{
    if (ui_callbacks.spell_add != NULL) {
        ui_callbacks.spell_add(obj, spell);
    }
}

// 0x4604A0
bool ui_spell_maintain_add(int mt_id)
{
    if (ui_callbacks.spell_maintain_add != NULL) {
        return ui_callbacks.spell_maintain_add(mt_id);
    } else {
        return false;
    }
}

// 0x4604C0
void ui_spell_maintain_end(int mt_id)
{
    if (ui_callbacks.spell_maintain_end != NULL) {
        ui_callbacks.spell_maintain_end(mt_id);
    }
}

// 0x4604E0
void ui_spell_maintain_refresh(void)
{
    if (ui_callbacks.spell_maintain_refresh != NULL) {
        ui_callbacks.spell_maintain_refresh();
    }
}

// 0x4604F0
void ui_object_examine_and_select(int64_t obj, int64_t observer_obj)
{
    if (ui_callbacks.object_examine_and_select != NULL) {
        ui_callbacks.object_examine_and_select(obj, observer_obj);
    }
}

// 0x460520
void ui_end_death(void)
{
    if (ui_callbacks.end_death != NULL) {
        ui_callbacks.end_death();
    }
}

// 0x460530
void ui_end_game(void)
{
    if (ui_callbacks.end_game != NULL) {
        ui_callbacks.end_game();
    }
}

// 0x460540
void ui_hotkey_slot_clear(int slot)
{
    if (ui_callbacks.hotkey_slot_clear != NULL) {
        ui_callbacks.hotkey_slot_clear(slot);
    }
}

// 0x460560
void ui_item_location_assign(int64_t item_obj, int64_t owner_obj, int inventory_location)
{
    if (ui_callbacks.item_location_assign != NULL) {
        ui_callbacks.item_location_assign(item_obj, owner_obj, inventory_location);
    }
}

// 0x460590
int ui_item_hotkey_bind(int64_t item_obj, int slot)
{
    if (ui_callbacks.item_hotkey_bind != NULL) {
        return ui_callbacks.item_hotkey_bind(item_obj, slot);
    } else {
        return 0;
    }
}

// 0x4605C0
void ui_targeting_reset(void)
{
    if (ui_callbacks.targeting_reset != NULL) {
        ui_callbacks.targeting_reset();
    }
}

// 0x4605D0
void ui_counters_refresh(void)
{
    if (ui_callbacks.counters_refresh != NULL) {
        ui_callbacks.counters_refresh();
    }
}

// 0x4605E0
void ui_spell_apply(int64_t source_obj, void* spell_context, int spell)
{
    if (ui_callbacks.spell_apply != NULL) {
        ui_callbacks.spell_apply(source_obj, spell_context, spell);
    }
}

// 0x460610
void ui_display_warning(char* str)
{
    if (ui_callbacks.display_warning != NULL) {
        ui_callbacks.display_warning(str);
    }
}

// 0x460630
void ui_display_msg(UiMessage* ui_message)
{
    if (ui_callbacks.display_msg != NULL) {
        ui_callbacks.display_msg(ui_message);
    }
}

// 0x460650
void ui_message_schedule_delayed(UiMessage* ui_message, unsigned int milliseconds)
{
    int id;

    DateTime datetime;
    TimeEvent timeevent;

    id = ui_next_message_id++;
    ui_message->str = STRDUP(ui_message->str);
    timeevent.type = TIMEEVENT_TYPE_UI;
    timeevent.params[0].integer_value = 0;
    timeevent.params[1].integer_value = id;
    datetime_init(&datetime, milliseconds);
    tig_idxtable_set(&ui_pending_messages, id, ui_message);
    timeevent_add_delay(&timeevent, &datetime);
}

// 0x4606C0
void ui_numbered_feedback_display(int message_id)
{
    if (ui_callbacks.numbered_feedback_display != NULL) {
        ui_callbacks.numbered_feedback_display(message_id);
    }
}

// 0x4606E0
void ui_mainmenu_request(void)
{
    if (ui_callbacks.mainmenu_request != NULL) {
        ui_callbacks.mainmenu_request();
    }
}

// 0x4606F0
void ui_written_start_obj(int64_t written_obj, int64_t pc_obj)
{
    if (ui_callbacks.written_start_obj != NULL) {
        ui_callbacks.written_start_obj(written_obj, pc_obj);
    }
}

// 0x460720
void ui_written_start_type(WrittenType written_type, int num)
{
    if (ui_callbacks.written_start_type != NULL) {
        ui_callbacks.written_start_type(written_type, num);
    }
}

// 0x460740
void ui_item_activate(int64_t owner_obj, int64_t item_obj)
{
    if (ui_callbacks.item_activate != NULL) {
        ui_callbacks.item_activate(owner_obj, item_obj);
    }
}

// 0x460770
void ui_follower_refresh(void)
{
    if (ui_callbacks.follower_refresh != NULL) {
        ui_callbacks.follower_refresh();
    }
}

// 0x460780
void ui_follower_update(void)
{
    if (ui_callbacks.follower_update != NULL) {
        ui_callbacks.follower_update();
    }
}

// 0x460790
void ui_toggle_primary_button(UiPrimaryButton btn, bool on)
{
    if (ui_callbacks.toggle_primary_button != NULL) {
        ui_callbacks.toggle_primary_button(btn, on);
    }
}

// 0x4607B0
void ui_set_map_button(UiPrimaryButton btn)
{
    if (ui_callbacks.set_map_button != NULL) {
        ui_callbacks.set_map_button(btn);
    }
}

// 0x4607D0
void ui_notify_sector_changed(int64_t sec, int64_t obj)
{
    if (ui_callbacks.notify_sector_changed != NULL) {
        ui_callbacks.notify_sector_changed(sec, obj);
    }
}

// 0x460800
void ui_written_newspaper_headline(int newspaper, char* str)
{
    if (ui_callbacks.written_newspaper_headline != NULL) {
        ui_callbacks.written_newspaper_headline(newspaper, str);
    }
}

// 0x460820
void ui_sleep_toggle(int64_t bed_obj)
{
    if (ui_callbacks.sleep_toggle != NULL) {
        ui_callbacks.sleep_toggle(bed_obj);
    }
}

// 0x460840
void ui_charedit_error_msg(int type, int param)
{
    if (ui_callbacks.charedit_error_msg != NULL) {
        ui_callbacks.charedit_error_msg(type, param);
    }
}

// 0x460860
void ui_charedit_refresh(void)
{
    if (ui_callbacks.charedit_refresh != NULL) {
        ui_callbacks.charedit_refresh();
    }
}

// 0x460870
void ui_progressbar_init(int max_value)
{
    if (ui_callbacks.progressbar_init != NULL) {
        ui_callbacks.progressbar_init(max_value);
    }
}

// 0x460890
void ui_progressbar_update(int value)
{
    if (ui_callbacks.progressbar_update != NULL) {
        ui_callbacks.progressbar_update(value);
    }
}

// 0x4608B0
void ui_wmap_select(int64_t obj, int spell)
{
    if (ui_callbacks.wmap_select != NULL) {
        ui_callbacks.wmap_select(obj, spell);
    }
}

// 0x4608D0
void ui_schematic_process(int schematic, int64_t primary_obj, int64_t secondary_obj)
{
    if (ui_callbacks.schematic_process != NULL) {
        ui_callbacks.schematic_process(schematic, primary_obj, secondary_obj);
    }
}

// 0x460900
void ui_schematic_feedback(bool success, int64_t primary_obj, int64_t secondary_obj)
{
    if (ui_callbacks.schematic_feedback != NULL) {
        ui_callbacks.schematic_feedback(success, primary_obj, secondary_obj);
    }
}

// 0x460930
void ui_character_sheet_open(int64_t pc_obj, int64_t target_obj)
{
    if (ui_callbacks.character_sheet_open != NULL) {
        ui_callbacks.character_sheet_open(pc_obj, target_obj);
    }
}

// 0x460960
void ui_item_transaction_process(InventoryTransactionPacket* pkt)
{
    if (ui_callbacks.item_transaction_process != NULL) {
        ui_callbacks.item_transaction_process(pkt);
    }
}

// 0x460980
void ui_inventory_drag_begin(int64_t item_obj)
{
    if (ui_callbacks.inventory_drag_begin != NULL) {
        ui_callbacks.inventory_drag_begin(item_obj);
    }
}

// 0x4609D0
void ui_gameuilib_reset(void)
{
    if (ui_callbacks.gameuilib_reset != NULL) {
        ui_callbacks.gameuilib_reset();
    }
}

// 0x4609E0
void ui_start_dialog(int64_t pc_obj, int64_t npc_obj, int script_num, int script_line, int num)
{
    if (ui_callbacks.start_dialog != NULL) {
        ui_callbacks.start_dialog(pc_obj, npc_obj, script_num, script_line, num);
    }
}

// 0x460A20
void ui_end_dialog(int64_t pc_obj, int end_reason)
{
    if (ui_callbacks.end_dialog != NULL) {
        ui_callbacks.end_dialog(pc_obj, end_reason);
    }
}

// 0x460AB0
void ui_optional_event_dispatch(int event_type, int event_code, void* event_payload)
{
    if (ui_callbacks.optional_event_callback != NULL) {
        ui_callbacks.optional_event_callback(event_type, event_code, event_payload);
    }
}

// 0x460AD0
void ui_mainmenu_return(int result)
{
    if (ui_callbacks.mainmenu_return != NULL) {
        ui_callbacks.mainmenu_return(result);
    }
}

// 0x460AF0
void ui_refresh_cursor(void)
{
    if (ui_callbacks.refresh_cursor != NULL) {
        ui_callbacks.refresh_cursor();
    }
}

// 0x460B00
int ui_target_skill_resolve(int skill, TargetDescriptor* td)
{
    if (ui_callbacks.target_skill_resolve != NULL) {
        return ui_callbacks.target_skill_resolve(skill, td);
    } else {
        return 0;
    }
}

// 0x460B20
void ui_inventory_drag_cancel(void)
{
    if (ui_callbacks.inventory_drag_cancel != NULL) {
        ui_callbacks.inventory_drag_cancel();
    }
}

// 0x460B30
bool ui_wmap_rnd_timeevent_process(TimeEvent* timeevent)
{
    if (ui_callbacks.wmap_rnd_timeevent_process != NULL) {
        return ui_callbacks.wmap_rnd_timeevent_process(timeevent);
    } else {
        return false;
    }
}

// 0x460B50
void ui_schematic_open_request(int64_t primary_obj, int64_t secondary_obj)
{
    if (ui_callbacks.schematic_open_request != NULL) {
        ui_callbacks.schematic_open_request(primary_obj, secondary_obj);
    }
}

// 0x460B80
void ui_queue_slide(int slide)
{
    if (ui_callbacks.queue_slide != NULL) {
        ui_callbacks.queue_slide(slide);
    }
}

// 0x460BA0
bool ui_gameuilib_mod_load(void)
{
    if (ui_callbacks.gameuilib_mod_load != NULL) {
        return ui_callbacks.gameuilib_mod_load();
    } else {
        return false;
    }
}

// 0x460BB0
int ui_multiplayer_confirmation_1902(void)
{
    if (ui_callbacks.multiplayer_confirmation_1902 != NULL) {
        return ui_callbacks.multiplayer_confirmation_1902();
    } else {
        return 0;
    }
}

// 0x460BC0
int ui_multiplayer_confirmation_1903(void)
{
    if (ui_callbacks.multiplayer_confirmation_1903 != NULL) {
        return ui_callbacks.multiplayer_confirmation_1903();
    } else {
        return 0;
    }
}

// 0x460BE0
int ui_multiplayer_named_confirmation(const char* title, const char* message)
{
    if (ui_callbacks.multiplayer_named_confirmation != NULL) {
        return ui_callbacks.multiplayer_named_confirmation(title, message);
    } else {
        return 0;
    }
}

// 0x460C00
void ui_schematic_info_get(int schematic, SchematicInfo* schematic_info)
{
    if (ui_callbacks.schematic_info_get != NULL) {
        ui_callbacks.schematic_info_get(schematic, schematic_info);
    }
}

// 0x460C20
int64_t ui_inventory_target_object_get(void)
{
    if (ui_callbacks.inventory_target_object_get != NULL) {
        return ui_callbacks.inventory_target_object_get();
    } else {
        return OBJ_HANDLE_NULL;
    }
}

// 0x460C30
void ui_hovered_object_refresh(int64_t obj)
{
    if (ui_callbacks.hovered_object_refresh != NULL) {
        ui_callbacks.hovered_object_refresh(obj);
    }
}

// 0x460C50
int ui_inventory_open_preconditions_check(int64_t pc_obj, int64_t target_obj, int mode)
{
    if (ui_callbacks.inventory_open_preconditions_check != NULL) {
        return ui_callbacks.inventory_open_preconditions_check(pc_obj, target_obj, mode);
    } else {
        return 0;
    }
}

// 0x460C80
int ui_inventory_game_mode_select(int64_t pc_obj, int64_t target_obj, int mode)
{
    if (ui_callbacks.inventory_game_mode_select != NULL) {
        return ui_callbacks.inventory_game_mode_select(pc_obj, target_obj, mode);
    } else {
        return 0;
    }
}

// 0x460CB0
void ui_inventory_target_prepare(int64_t pc_obj, int64_t target_obj, int mode)
{
    if (ui_callbacks.inventory_target_prepare != NULL) {
        ui_callbacks.inventory_target_prepare(pc_obj, target_obj, mode);
    }
}

// 0x460CE0
bool ui_inven_create(int64_t pc_obj, int64_t target_obj, int mode)
{
    if (ui_callbacks.inven_create != NULL) {
        return ui_callbacks.inven_create(pc_obj, target_obj, mode);
    } else {
        return false;
    }
}

// 0x460D10
void ui_reserved_string_callback_144_dispatch(const char* text)
{
    if (ui_callbacks.reserved_string_callback_144 != NULL) {
        ui_callbacks.reserved_string_callback_144(text);
    }
}

// 0x460D30
void ui_reserved_string_callback_148_dispatch(const char* text)
{
    if (ui_callbacks.reserved_string_callback_148 != NULL) {
        ui_callbacks.reserved_string_callback_148(text);
    }
}

// 0x460D50
void ui_reserved_string_pair_callback_14c_dispatch(const char* first_text, const char* second_text)
{
    if (ui_callbacks.reserved_string_pair_callback_14c != NULL) {
        ui_callbacks.reserved_string_pair_callback_14c(first_text, second_text);
    }
}

// 0x460D70
void ui_reserved_string_pair_callback_150_dispatch(const char* first_text, const char* second_text)
{
    if (ui_callbacks.reserved_string_pair_callback_150 != NULL) {
        ui_callbacks.reserved_string_pair_callback_150(first_text, second_text);
    }
}

// 0x460D90
void ui_reserved_integer_pair_callback_154_dispatch(int first_value, int second_value)
{
    if (ui_callbacks.reserved_integer_pair_callback_154 != NULL) {
        ui_callbacks.reserved_integer_pair_callback_154(first_value, second_value);
    }
}

// 0x460DB0
void ui_reserved_string_pair_callback_158_dispatch(const char* first_text, const char* second_text)
{
    if (ui_callbacks.reserved_string_pair_callback_158 != NULL) {
        ui_callbacks.reserved_string_pair_callback_158(first_text, second_text);
    }
}

// 0x460DD0
void ui_reserved_callback_15c_dispatch(void)
{
    if (ui_callbacks.reserved_callback_15c != NULL) {
        ui_callbacks.reserved_callback_15c();
    }
}

// 0x460DE0
void ui_show_inven_npc_identify(int64_t pc_obj, int64_t target_obj)
{
    if (ui_callbacks.show_inven_npc_identify != NULL) {
        ui_callbacks.show_inven_npc_identify(pc_obj, target_obj);
    }
}
