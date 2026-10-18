#ifndef ARCANUM_UI_INVEN_UI_H_
#define ARCANUM_UI_INVEN_UI_H_

#include "game/context.h"
#include "game/mp_utils.h"

typedef enum InvenUiMode {
    INVEN_UI_MODE_INVENTORY,
    INVEN_UI_MODE_BARTER,
    INVEN_UI_MODE_LOOT,
    INVEN_UI_MODE_STEAL,
    INVEN_UI_MODE_IDENTIFY,
    INVEN_UI_MODE_NPC_IDENTIFY,
    INVEN_UI_MODE_NPC_REPAIR,
} InvenUiMode;

bool inven_ui_init(GameInitInfo* init_info);
void inven_ui_exit(void);
void inven_ui_reset(void);
bool inven_ui_open_preconditions_check(int64_t pc_obj, int64_t target_obj, int mode);
bool inven_ui_game_mode_select(int64_t pc_obj, int64_t target_obj, int mode);
bool inven_ui_open(int64_t pc_obj, int64_t target_obj, int mode);
void inven_ui_target_inventory_prepare(int64_t pc_obj, int64_t target_obj, int mode);
bool inven_ui_create(int64_t pc_obj, int64_t target_obj, int mode);
void inven_ui_destroy(void);
void inven_ui_notify_object_destroyed(int64_t obj);
int inven_ui_is_created(void);
int64_t inven_ui_comparison_owner(int64_t item_obj);
int64_t inven_ui_target_object_get(void);
int64_t inven_ui_drag_item_obj_get(void);
void inven_ui_item_drag_begin(int64_t obj);
void inven_ui_drag_cursor_refresh(void);
void inven_ui_mouse_down_clear(void);
void inven_ui_item_cursor_begin(int64_t a1, bool a2);
void inven_ui_drag_object_clear(void);
bool inven_ui_can_open_inven(int64_t pc_obj, int64_t npc_obj);
void inven_ui_drag_cancel_and_restore(void);
void inven_ui_drag_item_restore_or_drop(void);
void inven_ui_drag_object_invalidate(int64_t obj);
void inven_ui_update(int64_t obj);
void inven_ui_item_transaction_send(int a1);
bool inven_ui_item_transaction_process(InventoryTransactionPacket* pkt);
int64_t inven_ui_transaction_item_get(void);

#endif /* ARCANUM_UI_INVEN_UI_H_ */
