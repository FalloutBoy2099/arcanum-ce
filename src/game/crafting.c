#include "game/crafting.h"
#include <tig/performance.h>

#include "game/critter.h"
#include "game/gamelib.h"
#include "game/item.h"
#include "game/object.h"
#include "game/player.h"
#include "game/proto.h"
#include "game/tech.h"

typedef struct CraftingItem { int64_t obj; int64_t owner; int64_t prototype; int units; } CraftingItem;
typedef struct CraftingAlternatives { int64_t prototypes[3]; } CraftingAlternatives;
#define CRAFTING_ITEM_CAPACITY 4096
static bool products_dropped;

bool crafting_products_dropped(void) { return products_dropped; }

bool crafting_learned_schematic_known(int schematic, int degree)
{
    return schematic >= 2000 && schematic < 4000 && (schematic - 2000) % 10 == 0
        && ((schematic - 2000) % 200) / 10 < degree;
}

static int crafting_items(int64_t crafter, CraftingItem* items, int capacity)
{
    Uint64 start = tig_performance_begin();
    int64_t owners[64];
    int owner_count = 1;
    owners[0] = crafter;
    int64_t leader = obj_field_int32_get(crafter, OBJ_F_TYPE) == OBJ_TYPE_PC ? crafter : critter_pc_leader_get(crafter);
    if (leader == player_get_local_pc_obj() && leader != OBJ_HANDLE_NULL
        && settings_get_value(&settings, CRAFTING_PARTY_COMPONENTS_KEY) != 0) {
        if (leader != crafter) owners[owner_count++] = leader;
        int followers = obj_arrayfield_length_get(leader, OBJ_F_CRITTER_FOLLOWER_IDX);
        for (int i = 0; i < followers && owner_count < SDL_arraysize(owners); i++) {
            int64_t follower = obj_arrayfield_handle_get(leader, OBJ_F_CRITTER_FOLLOWER_IDX, i);
            if (follower != OBJ_HANDLE_NULL && follower != crafter && !critter_is_dead(follower)
                && !critter_is_unconscious(follower) && object_dist(crafter, follower) <= 10) owners[owner_count++] = follower;
        }
    }
    int count = 0;
    for (int owner = 0; owner < owner_count; owner++) {
        if (owners[owner] != crafter && object_dist(crafter, owners[owner]) > 10) continue;
        int length = obj_field_int32_get(owners[owner], OBJ_F_CRITTER_INVENTORY_NUM);
        for (int i = 0; i < length && count < capacity; i++) {
            int64_t obj = obj_arrayfield_handle_get(owners[owner], OBJ_F_CRITTER_INVENTORY_LIST_IDX, i);
            if (obj == OBJ_HANDLE_NULL || IS_WEAR_INV_LOC(item_inventory_location_get(obj))) continue;
            if (tig_art_item_id_destroyed_get(obj_field_int32_get(obj, OBJ_F_CURRENT_AID)) != 0) continue;
            int64_t prototype = obj_field_handle_get(obj, OBJ_F_PROTOTYPE_HANDLE);
            if (prototype == OBJ_HANDLE_NULL || obj_field_int32_get(obj, OBJ_F_DESCRIPTION) != obj_field_int32_get(prototype, OBJ_F_DESCRIPTION)) continue;
            int field;
            sub_462410(obj, &field);
            int units = field != -1 ? obj_field_int32_get(obj, field) : 1;
            if (units > 0) items[count++] = (CraftingItem){ obj, owners[owner], prototype, units };
        }
    }
    tig_performance_end(TIG_PERF_CRAFTING, start);
    return count;
}

static CraftingAlternatives crafting_resolve(const int alternatives[3])
{
    CraftingAlternatives resolved = { 0 };
    for (int i = 0; i < 3; i++) {
        if (alternatives[i] > 0) resolved.prototypes[i] = sub_4685A0(alternatives[i]);
    }
    return resolved;
}

static int crafting_alternative(const CraftingItem* item, const CraftingAlternatives* alternatives)
{
    for (int i = 0; i < 3; i++) {
        if (alternatives->prototypes[i] != OBJ_HANDLE_NULL
            && item->prototype == alternatives->prototypes[i]) return i;
    }
    return -1;
}

static int crafting_count_items(const CraftingItem* items, int count, const CraftingAlternatives* alternatives)
{
    int units = 0;
    for (int i = 0; i < count; i++) {
        if (crafting_alternative(&items[i], alternatives) >= 0) units += SDL_min(items[i].units, 99999 - units);
    }
    return units;
}

static bool crafting_component_from_items(const CraftingItem* items, int count,
    const CraftingAlternatives* alternatives, CraftingIngredient* ingredient)
{
    bool found = false;
    for (int alternative = 0; alternative < 3 && !found; alternative++) {
        for (int i = 0; i < count; i++) {
            if (crafting_alternative(&items[i], alternatives) == alternative) {
                *ingredient = (CraftingIngredient){ items[i].obj, items[i].owner, alternative };
                found = true; break;
            }
        }
    }
    return found;
}

static bool crafting_pair_from_items(const CraftingItem* items, int count,
    const CraftingAlternatives alternatives[2], CraftingIngredient ingredients[2])
{
    bool found = false;
    for (int a = 0; a < 3 && !found; a++) {
        for (int i = 0; i < count && !found; i++) {
            if (crafting_alternative(&items[i], &alternatives[0]) != a) continue;
            for (int b = 0; b < 3 && !found; b++) {
                for (int j = 0; j < count; j++) {
                    if (crafting_alternative(&items[j], &alternatives[1]) != b || (i == j && items[i].units < 2)) continue;
                    ingredients[0] = (CraftingIngredient){ items[i].obj, items[i].owner, a };
                    ingredients[1] = (CraftingIngredient){ items[j].obj, items[j].owner, b };
                    found = true; break;
                }
            }
        }
    }
    return found;
}

int crafting_component_count(int64_t crafter, const int alternatives[3])
{
    CraftingItem* items = MALLOC(CRAFTING_ITEM_CAPACITY * sizeof(*items));
    if (items == NULL) return 0;
    int count = crafting_items(crafter, items, CRAFTING_ITEM_CAPACITY);
    CraftingAlternatives resolved = crafting_resolve(alternatives);
    int units = crafting_count_items(items, count, &resolved);
    FREE(items);
    return units;
}

bool crafting_find_component(int64_t crafter, const int alternatives[3], CraftingIngredient* ingredient)
{
    CraftingItem* items = MALLOC(CRAFTING_ITEM_CAPACITY * sizeof(*items));
    if (items == NULL) return false;
    int count = crafting_items(crafter, items, CRAFTING_ITEM_CAPACITY);
    CraftingAlternatives resolved = crafting_resolve(alternatives);
    bool found = crafting_component_from_items(items, count, &resolved, ingredient);
    FREE(items);
    return found;
}

bool crafting_find_pair(int64_t crafter, const SchematicInfo* schematic, CraftingIngredient ingredients[2])
{
    CraftingItem* items = MALLOC(CRAFTING_ITEM_CAPACITY * sizeof(*items));
    if (items == NULL) return false;
    int count = crafting_items(crafter, items, CRAFTING_ITEM_CAPACITY);
    CraftingAlternatives resolved[2] = { crafting_resolve(schematic->item1), crafting_resolve(schematic->item2) };
    bool found = crafting_pair_from_items(items, count, resolved, ingredients);
    FREE(items);
    return found;
}

void crafting_preview(int64_t crafter, const SchematicInfo* schematic, CraftingPreview* preview)
{
    if (preview == NULL) return;
    *preview = (CraftingPreview){ 0 };
    if (crafter == OBJ_HANDLE_NULL || schematic == NULL) return;
    CraftingItem* items = MALLOC(CRAFTING_ITEM_CAPACITY * sizeof(*items));
    if (items == NULL) return;
    int count = crafting_items(crafter, items, CRAFTING_ITEM_CAPACITY);
    CraftingAlternatives resolved[2] = { crafting_resolve(schematic->item1), crafting_resolve(schematic->item2) };
    for (int i = 0; i < 2; i++) {
        preview->stock[i] = crafting_count_items(items, count, &resolved[i]);
        preview->component_available[i] = crafting_component_from_items(items, count, &resolved[i], &preview->components[i]);
        int alternative = preview->component_available[i] ? preview->components[i].alternative : 0;
        int64_t prototype = resolved[i].prototypes[alternative];
        preview->prototypes[i] = prototype;
        if (prototype != OBJ_HANDLE_NULL) {
            int tech = obj_field_int32_get(prototype, OBJ_F_ITEM_DISCIPLINE);
            int required = -obj_field_int32_get(prototype, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY);
            preview->discipline[i] = tech;
            preview->complexity[i] = required;
            preview->expertise_available[i] = tech_degree_level_get(crafter, tech) >= required;
        }
    }
    CraftingIngredient pair[2];
    preview->pair_available = crafting_pair_from_items(items, count, resolved, pair);
    FREE(items);
}

bool crafting_process(int64_t crafter, const SchematicInfo* schematic)
{
    products_dropped = false;
    if (tig_net_is_active() && !tig_net_is_host()) return false;
    CraftingIngredient ingredients[2];
    if (schematic->qty < 1 || schematic->qty > 100 || !crafting_find_pair(crafter, schematic, ingredients)) return false;
    for (int i = 0; i < 2; i++) {
        int64_t prototype = sub_4685A0(i == 0 ? schematic->item1[ingredients[0].alternative] : schematic->item2[ingredients[1].alternative]);
        int tech = obj_field_int32_get(prototype, OBJ_F_ITEM_DISCIPLINE);
        int required = -obj_field_int32_get(prototype, OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY);
        if (tech_degree_level_get(crafter, tech) < required) return false;
    }
    int alternative = ingredients[0].alternative != 0 ? ingredients[0].alternative : ingredients[1].alternative;
    int64_t prototype = sub_4685A0(schematic->prod[alternative]);
    if (prototype == OBJ_HANDLE_NULL) return false;
    int64_t products[100];
    int64_t loc = obj_field_int64_get(crafter, OBJ_F_LOCATION);
    for (int i = 0; i < schematic->qty; i++) {
        if (!object_create(prototype, loc, &products[i])) {
            for (int j = 0; j < i; j++) object_destroy(products[j]);
            return false; // No ingredients are lost if product creation fails.
        }
    }
    for (int i = 0; i < 2; i++) {
        int type = obj_field_int32_get(ingredients[i].obj, OBJ_F_TYPE);
        if (type == OBJ_TYPE_AMMO) {
            item_ammo_transfer(ingredients[i].owner, OBJ_HANDLE_NULL, 1,
                obj_field_int32_get(ingredients[i].obj, OBJ_F_AMMO_TYPE), ingredients[i].obj);
        } else if (type == OBJ_TYPE_GOLD) {
            item_gold_transfer(ingredients[i].owner, OBJ_HANDLE_NULL, 1, ingredients[i].obj);
        } else object_destroy(ingredients[i].obj);
    }
    for (int i = 0; i < schematic->qty; i++) {
        // Full inventories leave the created item at the crafter's feet.
        if (!item_transfer(products[i], crafter)) products_dropped = true;
    }
    return true;
}
