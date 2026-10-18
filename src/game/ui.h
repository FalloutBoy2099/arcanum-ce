#ifndef ARCANUM_GAME_UI_H_
#define ARCANUM_GAME_UI_H_

#define BARTER_COLOR_BETTER_KEY "barter_color_better"
#define BARTER_COLOR_WORSE_KEY "barter_color_worse"
#define BARTER_COLOR_EQUAL_KEY "barter_color_equal"

#define SCHEMATIC_TOOLBAR_KEY "schematic_toolbar"
#define LOGBOOK_TOOLBAR_KEY "logbook_toolbar"
#define WORLDMAP_TOOLBAR_KEY "worldmap_toolbar"
#define SCHEMATIC_COLOR_AVAILABLE_KEY "schematic_color_available"
#define SCHEMATIC_COLOR_MISSING_KEY "schematic_color_missing"
#define SCHEMATIC_OPACITY_AVAILABLE_KEY "schematic_opacity_available"
#define SCHEMATIC_OPACITY_MISSING_KEY "schematic_opacity_missing"

#include "game/context.h"
#include "game/schematic.h"
#include "game/timeevent.h"
#include "game/written.h"

typedef enum UiMessageType {
    UI_MSG_TYPE_LEVEL,
    UI_MSG_TYPE_POISON,
    UI_MSG_TYPE_CURSE,
    UI_MSG_TYPE_BLESS,
    UI_MSG_TYPE_EXCLAMATION,
    UI_MSG_TYPE_QUESTION,
    UI_MSG_TYPE_FEEDBACK,
    UI_MSG_TYPE_SKILL,
    UI_MSG_TYPE_SPELL,
    UI_MSG_TYPE_COLLEGE,
    UI_MSG_TYPE_TECH,
    UI_MSG_TYPE_DEGREE,
    UI_MSG_TYPE_STAT,
    UI_MSG_TYPE_SCHEMATIC,
    UI_MSG_TYPE_COUNT,
} UiMessageType;

typedef struct UiMessage {
    int type;
    char* str;
    int primary_value;
    int secondary_value;
    int64_t caster_obj;
} UiMessage;

typedef enum UiPrimaryButton {
    UI_PRIMARY_BUTTON_CHAR,
    UI_PRIMARY_BUTTON_LOGBOOK,
    UI_PRIMARY_BUTTON_TOWNMAP,
    UI_PRIMARY_BUTTON_WORLDMAP,
    UI_PRIMARY_BUTTON_INVENTORY,
    UI_PRIMARY_BUTTON_COUNT,
} UiPrimaryButton;

// TODO: Remove forward declarations.
typedef struct InventoryTransactionPacket InventoryTransactionPacket;
typedef struct TargetDescriptor TargetDescriptor;

typedef void (*UiCharacterPanelsRefresh)(void);
typedef void (*UiObjectDestroyedNotify)(int64_t obj);
typedef void (*UiUpdateInven)(int64_t obj);
typedef void (*UI_CALLBACKS_FUNC_C)(void);
typedef void (*UiRefreshHealthBar)(int64_t obj);
typedef void (*UiRefreshFatigueBar)(int64_t obj);
typedef void (*UiSelectedObjectSet)(int64_t obj);
typedef void (*UiNotifyItemInsertedOrRemoved)(int64_t item_obj, bool removed, int inventory_location);
typedef void (*UiShowInvenLoot)(int64_t pc_obj, int64_t target_obj);
typedef void (*UiShowInvenIdentify)(int64_t pc_obj, int64_t target_obj);
typedef void (*UiTechAdjustDegree)(int64_t obj, int tech, int action);
typedef void (*UiSkillPreprocess)(int64_t obj, int type);
typedef void (*UiSkillActivate)(TargetDescriptor* td, int64_t obj, int a3);
typedef void (*UiAdjustSkillFunc)(int64_t obj, int skill, int action);
typedef void (*UiSetSkillTrainingFunc)(int64_t obj, int skill, int training);
typedef void (*UiDialogScriptAndGreeting)(int64_t a1, int64_t a2);
typedef bool (*UiIsInDialog)(int64_t obj);
typedef void (*UiSpellAdd)(int64_t obj, int spell);
typedef bool (*UiSpellMaintainAdd)(int mt_id);
typedef void (*UiSpellMaintainEnd)(int magictech);
typedef void (*UiSpellMaintainRefresh)(void);
typedef void (*UiObjectExamineAndSelect)(int64_t a1, int64_t a2);
typedef void (*UiEndDeath)(void);
typedef void (*UiEndGame)(void);
typedef void (*UiHotkeySlotClear)(int a1);
typedef void (*UiItemLocationAssign)(int64_t a1, int64_t a2, int a3);
typedef bool (*UiItemHotkeyBind)(int64_t a1, int a2);
typedef void (*UiTargetingReset)(void);
typedef void (*UiCountersRefresh)(void);
typedef void (*UiSpellApply)(int64_t a1, void* a3, int a4);
typedef void (*UiDisplayWarning)(char* str);
typedef void (*UiDisplayMsg)(UiMessage* ui_message);
typedef void (*UiNumberedFeedbackDisplay)(int a1);
typedef void (*UiMainmenuRequest)(void);
typedef void (*UiWrittenStartObj)(int64_t written_obj, int64_t pc_obj);
typedef void (*UiWrittenStartType)(WrittenType written_type, int num);
typedef void (*UiItemActivate)(int64_t owner_obj, int64_t item_obj);
typedef void (*UiFollowerRefresh)(void);
typedef void (*UiFollowerUpdate)(void);
typedef void (*UiTogglePrimaryButton)(UiPrimaryButton btn, bool on);
typedef void (*UiSetMapButton)(UiPrimaryButton btn);
typedef void (*UiNotifySectorChanged)(int64_t a1, int64_t a2);
typedef void (*UiWrittenNewspaperHeadline)(int newspaper, char* str);
typedef void (*UiSleepToggle)(int64_t bed_obj);
typedef void (*UiChareditErrorMsg)(int type, int param);
typedef void (*UiChareditRefresh)(void);
typedef void (*UiProgressBarInit)(int max_value);
typedef void (*UiProgressBarUpdate)(int value);
typedef void (*UiWmapSelect)(int64_t obj, int spell);
typedef bool (*UiSchematicProcess)(int schematic, int64_t primary_obj, int64_t secondary_obj);
typedef bool (*UiSchematicFeedback)(bool success, int64_t primary_obj, int64_t secondary_obj);
typedef void (*UiCharacterSheetOpen)(int64_t a1, int64_t a2);
typedef bool (*UiItemTransactionProcess)(InventoryTransactionPacket* pkt);
typedef void (*UiInventoryDragBegin)(int64_t obj);
typedef void (*UiInventoryDragRestoreAndRefreshCursor)(void);
typedef void (*UiInventoryDragObjectInvalidate)(int64_t obj);
typedef void (*UiGameUiLibReset)(void);
typedef void (*UiStartDialog)(int64_t pc_obj, int64_t npc_obj, int script_num, int script_line, int num);
typedef void (*UiEndDialog)(int64_t pc_obj, int a3);
typedef void (*UI_CALLBACKS_FUNC_F8)(int a1, int a2, void* a3);
typedef bool (*UiMainmenuReturn)(tig_button_handle_t button_handle);
typedef void (*UiRefreshCursor)(void);
typedef int (*UiTargetSkillResolve)(int a1, TargetDescriptor* td);
typedef void (*UiInventoryDragCancel)(void);
typedef bool (*UiWmapRndTimeEventProcess)(TimeEvent* timeevent);
typedef void (*UiSchematicOpenRequest)(int64_t a1, int64_t a2);
typedef void (*UiQueueSlide)(int slide);
typedef bool (*UiGameUiLibModLoad)(void);
typedef bool (*UiMultiplayerConfirmation1902)(void);
typedef bool (*UiMultiplayerConfirmation1903)(void);
typedef bool (*UiMultiplayerNamedConfirmation)(const char* a1, const char* a2);
typedef void (*UiSchematicInfoGet)(int schematic, SchematicInfo* schematic_info);
typedef int64_t (*UiInventoryTargetObjectGet)(void);
typedef void (*UiHoveredObjectRefresh)(int64_t obj);
typedef bool (*UiInventoryOpenPreconditionsCheck)(int64_t a1, int64_t a2, int a3);
typedef bool (*UiInventoryGameModeSelect)(int64_t a1, int64_t a2, int a3);
typedef void (*UiInventoryTargetPrepare)(int64_t a1, int64_t a2, int a3);
typedef bool (*UiInvenCreate)(int64_t pc_obj, int64_t target_obj, int mode);
typedef void (*UI_CALLBACKS_FUNC_144)(const char* a1);
typedef void (*UI_CALLBACKS_FUNC_148)(const char* a1);
typedef void (*UI_CALLBACKS_FUNC_14C)(const char* a1, const char* a2);
typedef void (*UI_CALLBACKS_FUNC_150)(const char* a1, const char* a2);
typedef void (*UI_CALLBACKS_FUNC_154)(int a1, int a2);
typedef void (*UI_CALLBACKS_FUNC_158)(const char* a1, const char* a2);
typedef void (*UI_CALLBACKS_FUNC_15C)(void);
typedef void (*UiShowInvenNpcIdentify)(int64_t pc_obj, int64_t target_obj);

typedef struct UiCallbacks {
    /* 0000 */ UiCharacterPanelsRefresh character_panels_refresh;
    /* 0004 */ UiObjectDestroyedNotify object_destroyed_notify;
    /* 0008 */ UiUpdateInven update_inven;
    /* 000C */ UI_CALLBACKS_FUNC_C field_C;
    /* 0010 */ UiRefreshHealthBar refresh_health_bar;
    /* 0014 */ UiRefreshFatigueBar refresh_fatigue_bar;
    /* 0018 */ UiSelectedObjectSet selected_object_set;
    /* 001C */ UiNotifyItemInsertedOrRemoved notify_item_inserted_or_removed;
    /* 0020 */ UiShowInvenLoot show_inven_loot;
    /* 0024 */ UiShowInvenIdentify show_inven_identify;
    /* 0028 */ UiTechAdjustDegree tech_adjust_degree;
    /* 002C */ UiSkillPreprocess skill_preprocess;
    /* 0030 */ UiSkillActivate skill_activate;
    /* 0034 */ UiAdjustSkillFunc adjust_skill;
    /* 0038 */ UiSetSkillTrainingFunc set_skill_training;
    /* 003C */ UiDialogScriptAndGreeting dialog_script_and_greeting;
    /* 0044 */ UiIsInDialog is_in_dialog;
    /* 0048 */ UiSpellAdd spell_add;
    /* 004C */ UiSpellMaintainAdd spell_maintain_add;
    /* 0050 */ UiSpellMaintainEnd spell_maintain_end;
    /* 0054 */ UiSpellMaintainRefresh spell_maintain_refresh;
    /* 0058 */ UiObjectExamineAndSelect object_examine_and_select;
    /* 005C */ UiEndDeath end_death;
    /* 0060 */ UiEndGame end_game;
    /* 0064 */ UiHotkeySlotClear hotkey_slot_clear;
    /* 0068 */ UiItemLocationAssign item_location_assign;
    /* 006C */ UiItemHotkeyBind item_hotkey_bind;
    /* 0070 */ UiTargetingReset targeting_reset;
    /* 0074 */ UiCountersRefresh counters_refresh;
    /* 0078 */ UiSpellApply spell_apply;
    /* 007C */ UiDisplayWarning display_warning;
    /* 0080 */ UiDisplayMsg display_msg;
    /* 0084 */ UiNumberedFeedbackDisplay numbered_feedback_display;
    /* 0088 */ UiMainmenuRequest mainmenu_request;
    /* 008C */ UiWrittenStartObj written_start_obj;
    /* 0090 */ UiWrittenStartType written_start_type;
    /* 0094 */ UiItemActivate item_activate;
    /* 0098 */ UiFollowerRefresh follower_refresh;
    /* 009C */ UiFollowerUpdate follower_update;
    /* 00A0 */ UiTogglePrimaryButton toggle_primary_button;
    /* 00A4 */ UiSetMapButton set_map_button;
    /* 00A8 */ UiNotifySectorChanged notify_sector_changed;
    /* 00AC */ UiWrittenNewspaperHeadline written_newspaper_headline;
    /* 00B0 */ UiSleepToggle sleep_toggle;
    /* 00B4 */ UiChareditErrorMsg charedit_error_msg;
    /* 00B8 */ UiChareditRefresh charedit_refresh;
    /* 00BC */ UiProgressBarInit progressbar_init;
    /* 00C0 */ UiProgressBarUpdate progressbar_update;
    /* 00C4 */ UiWmapSelect wmap_select;
    /* 00C8 */ UiSchematicProcess schematic_process;
    /* 00CC */ UiSchematicFeedback schematic_feedback;
    /* 00D0 */ UiCharacterSheetOpen character_sheet_open;
    /* 00D4 */ UiItemTransactionProcess item_transaction_process;
    /* 00D8 */ UiInventoryDragBegin inventory_drag_begin;
    /* 00DC */ UiInventoryDragRestoreAndRefreshCursor inventory_drag_restore_and_refresh_cursor;
    /* 00E0 */ UiInventoryDragObjectInvalidate inventory_drag_object_invalidate;
    /* 00E4 */ UiGameUiLibReset gameuilib_reset;
    /* 00E8 */ UiStartDialog start_dialog;
    /* 00EC */ UiEndDialog end_dialog;
    /* 00F8 */ UI_CALLBACKS_FUNC_F8 field_F8;
    /* 00FC */ UiMainmenuReturn mainmenu_return;
    /* 0100 */ UiRefreshCursor refresh_cursor;
    /* 0104 */ UiTargetSkillResolve target_skill_resolve;
    /* 0108 */ UiInventoryDragCancel inventory_drag_cancel;
    /* 010C */ UiWmapRndTimeEventProcess wmap_rnd_timeevent_process;
    /* 0110 */ UiSchematicOpenRequest schematic_open_request;
    /* 0114 */ UiQueueSlide queue_slide;
    /* 0118 */ UiGameUiLibModLoad gameuilib_mod_load;
    /* 011C */ UiMultiplayerConfirmation1902 multiplayer_confirmation_1902;
    /* 0120 */ UiMultiplayerConfirmation1903 multiplayer_confirmation_1903;
    /* 0124 */ UiMultiplayerNamedConfirmation multiplayer_named_confirmation;
    /* 0128 */ UiSchematicInfoGet schematic_info_get;
    /* 012C */ UiInventoryTargetObjectGet inventory_target_object_get;
    /* 0130 */ UiHoveredObjectRefresh hovered_object_refresh;
    /* 0134 */ UiInventoryOpenPreconditionsCheck inventory_open_preconditions_check;
    /* 0138 */ UiInventoryGameModeSelect inventory_game_mode_select;
    /* 013C */ UiInventoryTargetPrepare inventory_target_prepare;
    /* 0140 */ UiInvenCreate inven_create;
    /* 0144 */ UI_CALLBACKS_FUNC_144 field_144;
    /* 0148 */ UI_CALLBACKS_FUNC_148 field_148;
    /* 014C */ UI_CALLBACKS_FUNC_14C field_14C;
    /* 0150 */ UI_CALLBACKS_FUNC_150 field_150;
    /* 0154 */ UI_CALLBACKS_FUNC_154 field_154;
    /* 0158 */ UI_CALLBACKS_FUNC_158 field_158;
    /* 015C */ UI_CALLBACKS_FUNC_15C field_15C;
    /* 0160 */ UiShowInvenNpcIdentify show_inven_npc_identify;
} UiCallbacks;

void ui_init(UiCallbacks* callbacks);
void ui_exit(void);
bool ui_timeevent_process(TimeEvent* timeevent);
void ui_character_panels_refresh(void);
void ui_object_destroyed_notify(int64_t obj);
void ui_update_inven(int64_t obj);
void ui_refresh_health_bar(int64_t obj);
void ui_refresh_fatigue_bar(int64_t obj);
void ui_selected_object_set(int64_t obj);
void ui_notify_item_inserted_or_removed(int64_t item_obj, bool removed, int inventory_location);
void ui_show_inven_loot(int64_t pc_obj, int64_t target_obj);
void ui_show_inven_identify(int64_t pc_obj, int64_t target_obj);
void ui_tech_adjust_degree(int64_t obj, int tech, int action);
void ui_skill_preprocess(int64_t obj, int type);
void ui_skill_activate(TargetDescriptor* td, int64_t obj, int a3);
void ui_adjust_skill(int64_t obj, int skill, int action);
void ui_set_skill_training(int64_t obj, int skill, int training);
void ui_dialog_script_and_greeting(int64_t a1, int64_t a2);
int ui_is_in_dialog(int64_t obj);
void ui_spell_add(int64_t obj, int spell);
bool ui_spell_maintain_add(int mt_id);
void ui_spell_maintain_end(int mt_id);
void ui_spell_maintain_refresh(void);
void ui_object_examine_and_select(int64_t a1, int64_t a2);
void ui_end_death(void);
void ui_end_game(void);
void ui_hotkey_slot_clear(int a1);
void ui_item_location_assign(int64_t a1, int64_t a2, int a3);
int ui_item_hotkey_bind(int64_t a1, int a2);
void ui_targeting_reset(void);
void ui_counters_refresh(void);
void ui_spell_apply(int64_t a1, void* a2, int a3);
void ui_display_warning(char* str);
void ui_display_msg(UiMessage* ui_message);
void ui_message_schedule_delayed(UiMessage* ui_message, unsigned int milliseconds);
void ui_numbered_feedback_display(int a1);
void ui_mainmenu_request(void);
void ui_written_start_obj(int64_t written_obj, int64_t pc_obj);
void ui_written_start_type(WrittenType written_type, int num);
void ui_item_activate(int64_t a1, int64_t a2);
void ui_follower_refresh(void);
void ui_follower_update(void);
void ui_toggle_primary_button(UiPrimaryButton btn, bool on);
void ui_set_map_button(UiPrimaryButton btn);
void ui_notify_sector_changed(int64_t a1, int64_t a2);
void ui_written_newspaper_headline(int newspaper, char* str);
void ui_sleep_toggle(int64_t bed_obj);
void ui_charedit_error_msg(int type, int param);
void ui_charedit_refresh(void);
void ui_progressbar_init(int max_value);
void ui_progressbar_update(int value);
void ui_wmap_select(int64_t obj, int spell);
void ui_schematic_process(int schematic, int64_t primary_obj, int64_t secondary_obj);
void ui_schematic_feedback(bool success, int64_t primary_obj, int64_t secondary_obj);
void ui_character_sheet_open(int64_t a1, int64_t a2);
void ui_item_transaction_process(InventoryTransactionPacket* pkt);
void ui_inventory_drag_begin(int64_t a1);
void ui_gameuilib_reset(void);
void ui_start_dialog(int64_t pc_obj, int64_t npc_obj, int script_num, int script_line, int num);
void ui_end_dialog(int64_t a1, int a3);
void ui_optional_event_dispatch(int a1, int a2, void* a3);
void ui_mainmenu_return(int a1);
void ui_refresh_cursor(void);
int ui_target_skill_resolve(int a1, TargetDescriptor* td);
void ui_inventory_drag_cancel(void);
bool ui_wmap_rnd_timeevent_process(TimeEvent* timeevent);
void ui_schematic_open_request(int64_t a1, int64_t a2);
void ui_queue_slide(int slide);
bool ui_gameuilib_mod_load(void);
int ui_multiplayer_confirmation_1902(void);
int ui_multiplayer_confirmation_1903(void);
int ui_multiplayer_named_confirmation(const char* a1, const char* a2);
void ui_schematic_info_get(int schematic, SchematicInfo* schematic_info);
int64_t ui_inventory_target_object_get(void);
void ui_hovered_object_refresh(int64_t a1);
int ui_inventory_open_preconditions_check(int64_t a1, int64_t a2, int a3);
int ui_inventory_game_mode_select(int64_t a1, int64_t a2, int a3);
void ui_inventory_target_prepare(int64_t a1, int64_t a2, int a3);
bool ui_inven_create(int64_t pc_obj, int64_t target_obj, int mode);
void ui_reserved_string_callback_144_dispatch(const char* a1);
void ui_reserved_string_callback_148_dispatch(const char* a1);
void ui_reserved_string_pair_callback_14c_dispatch(const char* a1, const char* a2);
void ui_reserved_string_pair_callback_150_dispatch(const char* a1, const char* a2);
void ui_reserved_integer_pair_callback_154_dispatch(int a1, int a2);
void ui_reserved_string_pair_callback_158_dispatch(const char* a1, const char* a2);
void ui_reserved_callback_15c_dispatch(void);
void ui_show_inven_npc_identify(int64_t pc_obj, int64_t target_obj);
void ui_mp_charedit_cache_traits(int player);
void ui_mp_charedit_trait_inc(int player, int trait, int param);
void ui_mp_charedit_trait_dec(int player, int trait, int param);

#endif /* ARCANUM_GAME_UI_H_ */
