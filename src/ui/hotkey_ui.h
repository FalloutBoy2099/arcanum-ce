#ifndef ARCANUM_UI_HOTKEY_UI_H_
#define ARCANUM_UI_HOTKEY_UI_H_

#include "game/context.h"
#include "game/obj.h"
#include "ui/types.h"

extern int hotkey_ui_dragging_index;
extern bool hotkey_ui_dragging;

bool hotkey_ui_init(GameInitInfo* init_info);
void hotkey_ui_exit(void);
void hotkey_ui_resize(GameResizeInfo* resize_info);
bool hotkey_ui_start(tig_window_handle_t primary_window_handle, TigRect* rect, tig_window_handle_t secondary_window_handle, bool fullscreen);
void hotkey_ui_end(void);
void hotkey_ui_reset_recent_actions(void);
bool hotkey_ui_save(TigFile* stream);
bool hotkey_ui_load(GameLoadInfo* load_info);
void hotkey_ui_inventory_drag_begin(void);
bool hotkey_ui_process_event(TigMessage* msg);
bool hotkey_ui_is_dragging(void);
void intgame_hotkey_refresh(int index);
void hotkey_ui_entry_clear(Hotkey* hotkey);
bool hotkey_ui_begin_drag(void);
void hotkey_ui_slot_clear_callback(int index);
void hotkey_ui_recent_action_set(int type, int data, int64_t item_obj);
void hotkey_ui_cancel_drag(void);
bool hotkey_ui_drag_drop_process(TigMessageMouseEvent mouse_event);
bool hotkey_ui_inventory_drop_process(TigMessageMouseEvent mouse_event);
void hotkey_ui_item_location_assign(int64_t obj, int64_t a2, int a3);
bool hotkey_ui_item_slot_bind(int64_t obj, int a2);
void hotkey_ui_slot_activate(int index);
void hotkey_ui_slot_clear(int index);
void hotkey_ui_notify_item_inserted_or_removed(int64_t item_obj, bool removed);
Hotkey* hotkey_ui_slot_get(int index);
bool hotkey_ui_dragged_items_reset(void);
void hotkey_ui_notify_spell_removed(int spell);
void intgame_hotkeys_recover(void);

#endif /* ARCANUM_UI_HOTKEY_UI_H_ */
