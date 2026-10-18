#include "ui/item_compare.h"
#include "game/critter.h"
#include "game/item.h"

int64_t item_compare_inventory_owner(int64_t item, int64_t character,
    int64_t target, int64_t stock, bool barter)
{
    int64_t parent;
    if (item == OBJ_HANDLE_NULL || character == OBJ_HANDLE_NULL
        || !item_parent(item, &parent) || parent == OBJ_HANDLE_NULL) return OBJ_HANDLE_NULL;
    if (parent == character) return character;
    if (parent != target && parent != stock) return OBJ_HANDLE_NULL;
    // Companion exchange uses the barter window too: resolve the companion
    // before the merchant case so each side uses its own equipment/aptitude.
    if (obj_field_int32_get(parent, OBJ_F_TYPE) == OBJ_TYPE_NPC
        && critter_pc_leader_get(parent) == character) return parent;
    return barter ? character : OBJ_HANDLE_NULL;
}

int64_t item_compare_reference(int64_t owner, int64_t item)
{
    if (owner == OBJ_HANDLE_NULL || item == OBJ_HANDLE_NULL) return OBJ_HANDLE_NULL;
    int type = obj_field_int32_get(item, OBJ_F_TYPE);
    if (type != OBJ_TYPE_WEAPON && type != OBJ_TYPE_ARMOR) return OBJ_HANDLE_NULL;
    int slot = item_location_get(item);
    if (slot < FIRST_WEAR_INV_LOC || slot > LAST_WEAR_INV_LOC) return OBJ_HANDLE_NULL;
    int64_t reference = item_wield_get(owner, slot);
    // Same intrinsic-value ranking used by item_wield_best for armor.
    if (slot == ITEM_INV_LOC_RING1) {
        int64_t second = item_wield_get(owner, ITEM_INV_LOC_RING2);
        if (reference == OBJ_HANDLE_NULL
            || (second != OBJ_HANDLE_NULL && item_worth(second) < item_worth(reference))) {
            reference = second;
        }
    }
    if (reference != OBJ_HANDLE_NULL && obj_field_int32_get(reference, OBJ_F_TYPE) != type) return OBJ_HANDLE_NULL;
    return reference;
}
