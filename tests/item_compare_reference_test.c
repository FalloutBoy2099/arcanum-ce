#include "ui/item_compare.h"
#include "game/item.h"
#include <stdio.h>
#include <string.h>

// Fixture inventory replaces engine access only. The production reference
// selector is linked unchanged, and must request the actual corresponding slot.
static int failures, requested_slot, candidate_type, candidate_slot;
static int64_t equipment[9];
static int64_t companion_equipment[9];
static int worth[18], types[18];
static int64_t item_owner, requested_owner;
static bool has_parent = true;
static int wield_calls;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)

int obj_field_int32_get(int64_t obj, int fld)
{
    CHECK(fld == OBJ_F_TYPE);
    if (obj == 98 || obj == 97) return OBJ_TYPE_NPC;
    if (obj == 96) return OBJ_TYPE_CONTAINER;
    if (obj == 99) return OBJ_TYPE_PC;
    return obj == 100 ? candidate_type : types[obj - 1];
}
bool item_parent(int64_t item, int64_t* parent)
{
    CHECK(item == 100);
    *parent = item_owner;
    return has_parent;
}
int64_t critter_pc_leader_get(int64_t obj)
{
    CHECK(obj == 98 || obj == 97);
    return obj == 98 ? 99 : OBJ_HANDLE_NULL;
}
int item_location_get(int64_t obj) { CHECK(obj == 100); return candidate_slot; }
int64_t item_wield_get(int64_t owner, int slot)
{
    CHECK(owner == 99 || owner == 98);
    requested_owner = owner;
    requested_slot = slot;
    wield_calls++;
    CHECK(slot >= FIRST_WEAR_INV_LOC && slot <= LAST_WEAR_INV_LOC);
    return owner == 98 ? companion_equipment[slot - FIRST_WEAR_INV_LOC] : equipment[slot - FIRST_WEAR_INV_LOC];
}
int item_worth(int64_t obj) { CHECK(obj >= 1 && obj <= 18); return worth[obj - 1]; }

static void test_inventory_owners(void)
{
    item_owner = 99;
    CHECK(item_compare_inventory_owner(100, 99, 0, 0, false) == 99);
    CHECK(item_compare_inventory_owner(100, 99, 98, 98, true) == 99);
    item_owner = 98;
    CHECK(item_compare_inventory_owner(100, 99, 98, 98, false) == 98);
    CHECK(item_compare_inventory_owner(100, 99, 98, 98, true) == 98);
    CHECK(item_compare_inventory_owner(100, 98, 0, 0, false) == 98);
    CHECK(item_compare_inventory_owner(100, 99, 97, 96, false) == 0); // Other inventory, not visible.
    item_owner = 97;
    CHECK(item_compare_inventory_owner(100, 99, 97, 96, true) == 99); // Merchant.
    CHECK(item_compare_inventory_owner(100, 99, 97, 97, false) == 0); // Pickpocket/loot NPC.
    item_owner = 96;
    CHECK(item_compare_inventory_owner(100, 99, 97, 96, true) == 99); // Substitute stock.
    CHECK(item_compare_inventory_owner(100, 99, 96, 96, false) == 0); // Chest.
    has_parent = false;
    CHECK(item_compare_inventory_owner(100, 99, 97, 96, true) == 0);
    has_parent = true; item_owner = 0;
    CHECK(item_compare_inventory_owner(100, 99, 0, 0, true) == 0);
    CHECK(item_compare_inventory_owner(0, 99, 97, 96, true) == 0);
    CHECK(item_compare_inventory_owner(100, 0, 97, 96, true) == 0);

    // Different loadouts: all slots, including rings, must use the hovered side.
    for (int i = 0; i < 9; i++) {
        equipment[i] = i + 1; companion_equipment[i] = i + 10;
        types[i] = types[i + 9] = OBJ_TYPE_ARMOR;
        worth[i] = worth[i + 9] = 100 + i;
    }
    types[4] = types[13] = OBJ_TYPE_WEAPON;
    for (int slot = FIRST_WEAR_INV_LOC; slot <= LAST_WEAR_INV_LOC; slot++) {
        if (slot == ITEM_INV_LOC_RING2) continue; // Ring candidates map to ring 1.
        candidate_slot = slot;
        candidate_type = slot == ITEM_INV_LOC_WEAPON ? OBJ_TYPE_WEAPON : OBJ_TYPE_ARMOR;
        item_owner = 99;
        int64_t owner = item_compare_inventory_owner(100, 99, 98, 98, true);
        CHECK(item_compare_reference(owner, 100) == slot - FIRST_WEAR_INV_LOC + 1);
        CHECK(requested_owner == 99);
        item_owner = 98;
        owner = item_compare_inventory_owner(100, 99, 98, 98, true);
        CHECK(item_compare_reference(owner, 100) == slot - FIRST_WEAR_INV_LOC + 10);
        CHECK(requested_owner == 98);
    }
    candidate_type = OBJ_TYPE_ARMOR; candidate_slot = ITEM_INV_LOC_RING1;
    worth[11] = 1;
    CHECK(item_compare_reference(98, 100) == 12); // Companion's weaker ring, not player's.
    CHECK(item_compare_reference(99, 100) == 2);
}

int main(void)
{
    const int slots[] = {ITEM_INV_LOC_HELMET, ITEM_INV_LOC_RING1, ITEM_INV_LOC_MEDALLION,
        ITEM_INV_LOC_SHIELD, ITEM_INV_LOC_ARMOR, ITEM_INV_LOC_GAUNTLET, ITEM_INV_LOC_BOOTS};
    for (int i = 0; i < 9; i++) { equipment[i] = i + 1; worth[i] = 100 + i; types[i] = OBJ_TYPE_ARMOR; }
    candidate_type = OBJ_TYPE_ARMOR;
    for (int i = 0; i < sizeof(slots)/sizeof(slots[0]); i++) {
        candidate_slot = slots[i];
        CHECK(item_compare_reference(99, 100) == candidate_slot - FIRST_WEAR_INV_LOC + 1);
    }
    candidate_slot = ITEM_INV_LOC_RING1;
    worth[2] = 1;
    CHECK(item_compare_reference(99,100) == 3); // The second ring is weaker.
    worth[1] = 1;
    CHECK(item_compare_reference(99,100) == 2); // Stable tie.
    equipment[1] = 0;
    CHECK(item_compare_reference(99,100) == 3);
    equipment[2] = 0;
    CHECK(item_compare_reference(99,100) == 0);
    candidate_slot = ITEM_INV_LOC_BOOTS;
    equipment[8] = 0;
    CHECK(item_compare_reference(99,100) == 0);
    const int non_equipment[] = {OBJ_TYPE_AMMO, OBJ_TYPE_GOLD, OBJ_TYPE_FOOD, OBJ_TYPE_SCROLL,
        OBJ_TYPE_KEY, OBJ_TYPE_KEY_RING, OBJ_TYPE_WRITTEN, OBJ_TYPE_GENERIC};
    for (int i = 0; i < sizeof(non_equipment)/sizeof(non_equipment[0]); i++) {
        candidate_type = non_equipment[i]; wield_calls = 0;
        CHECK(item_compare_reference(99,100) == 0 && wield_calls == 0);
    }
    candidate_type = OBJ_TYPE_WEAPON; candidate_slot = ITEM_INV_LOC_WEAPON;
    wield_calls = 0;
    CHECK(item_compare_reference(99,100) == 0); // A wrong-type reference is rejected.
    CHECK(wield_calls == 1 && requested_slot == ITEM_INV_LOC_WEAPON);
    types[4] = OBJ_TYPE_WEAPON;
    CHECK(item_compare_reference(99,100) == 5);
    candidate_slot = -1;
    CHECK(item_compare_reference(99,100) == 0);
    CHECK(item_compare_reference(0,100) == 0 && item_compare_reference(99,0) == 0);
    test_inventory_owners();
    printf("Equipment slots and weakest ring: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
