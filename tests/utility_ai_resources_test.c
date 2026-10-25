#include <stdio.h>
#include <string.h>
#include "game/ai.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/item.h"
#include "game/magictech.h"
#include "game/mt_ai.h"
#include "game/spell.h"
#include "game/stat.h"
#include "game/utility_ai.h"

typedef struct ResourceItem {
    int type;
    unsigned int flags;
    int mana;
    int action;
    int spells[5];
} ResourceItem;

static ResourceItem items[64];
static int64_t inventory[64];
static int inventory_count;
static bool known[MT_SPELL_COUNT];
static int fatigue_percent = 80;
static int failures;
static MagicTechInfo spell_data[MT_SPELL_COUNT];
MagicTechInfo* magictech_spells = spell_data;
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #condition); failures++; } } while (0)

int obj_field_int32_get(int64_t obj, int field)
{
    if (obj == 1) {
        if (field == OBJ_F_TYPE) return OBJ_TYPE_NPC;
        if (field == OBJ_F_CRITTER_INVENTORY_NUM) return inventory_count;
        return 0;
    }
    if (obj < 0 || obj >= 64) return 0;
    if (field == OBJ_F_TYPE) return items[obj].type;
    if (field == OBJ_F_ITEM_FLAGS) return items[obj].flags;
    if (field == OBJ_F_ITEM_SPELL_MANA_STORE) return items[obj].mana;
    if (field == OBJ_F_ITEM_AI_ACTION) return items[obj].action;
    if (field >= OBJ_F_ITEM_SPELL_1 && field <= OBJ_F_ITEM_SPELL_5) return items[obj].spells[field - OBJ_F_ITEM_SPELL_1];
    return 0;
}

int64_t obj_arrayfield_handle_get(int64_t obj, int field, int index)
{
    (void)obj; (void)field;
    return inventory[index];
}

int64_t critter_pc_leader_get(int64_t obj) { (void)obj; return 2; }
int64_t item_wield_get(int64_t obj, int location) { (void)obj; (void)location; return OBJ_HANDLE_NULL; }
bool combat_critter_is_combat_mode_active(int64_t obj) { (void)obj; return true; }
int ai_critter_fatigue_ratio(int64_t obj) { (void)obj; return fatigue_percent; }
int stat_level_get(int64_t obj, int stat) { (void)obj; return stat == STAT_MAGICK_POINTS ? 1 : 0; }
bool spell_is_known(int64_t obj, int spell) { (void)obj; return spell >= 0 && spell < MT_SPELL_COUNT && known[spell]; }
bool magictech_item_effective_power_check(int64_t actor, int64_t target, int64_t item)
{
    (void)actor; (void)target; (void)item;
    return true;
}

static bool contains(const MagicTechAiActionListEntry* entries, int count, int spell, int64_t item)
{
    for (int i = 0; i < count; i++) if (entries[i].spell == spell && entries[i].item_obj == item) return true;
    return false;
}

static void potion_init(int item, int spell)
{
    items[item] = (ResourceItem){ .type = OBJ_TYPE_FOOD, .flags = OIF_IDENTIFIED, .mana = 1,
        .spells = { spell, 10000, 10000, 10000, 10000 } };
}

int main(void)
{
    MagicTechAi legacy = { 0 };
    MagicTechAiActionListEntry entries[MT_AI_UTILITY_RESOURCE_LIMIT + 1];
    int action = MAGICTECH_AI_ACTION_HEAL_LIGHT;
    for (int spell = 0; spell < MT_SPELL_COUNT; spell++) {
        for (int category = 0; category < MAGICTECH_AI_ACTION_COUNT; category++) spell_data[spell].ai.values[category] = -1;
    }
    spell_data[1].ai.values[action] = 100;
    spell_data[2].ai.values[action] = 90;
    spell_data[3].ai.values[action] = 60;
    known[1] = known[2] = true;
    potion_init(10, 3);
    inventory[inventory_count++] = 10;
    CHECK(mt_ai_init(NULL));
    mt_ai_create(&legacy, 1, action);
    CHECK(legacy.actions[action].cnt == 2);
    CHECK(!contains(legacy.actions[action].entries, legacy.actions[action].cnt, 3, 10));
    int count = mt_ai_utility_actions_collect(1, action, entries, MT_AI_UTILITY_RESOURCE_LIMIT);
    CHECK(count == 3);
    CHECK(entries[0].item_obj == OBJ_HANDLE_NULL);
    CHECK(entries[1].item_obj == 10);
    CHECK(contains(entries, count, 1, OBJ_HANDLE_NULL));
    CHECK(contains(entries, count, 2, OBJ_HANDLE_NULL));
    CHECK(contains(entries, count, 3, 10));
    fatigue_percent = 16;
    count = mt_ai_utility_actions_collect(1, action, entries, MT_AI_UTILITY_RESOURCE_LIMIT);
    UtilityAiCandidate candidates[MT_AI_UTILITY_RESOURCE_LIMIT] = { 0 };
    UtilityAiContext context = { .health_percent = 100, .fatigue = 8, .max_fatigue = 50, .threat_distance = 4 };
    for (int i = 0; i < count; i++) {
        candidates[i] = (UtilityAiCandidate){ .action = UTILITY_AI_HEAL, .spell = entries[i].spell,
            .item = entries[i].item_obj, .target_health_percent = 20,
            .base_score = spell_data[entries[i].spell].ai.values[action],
            .initial_cost = entries[i].item_obj == OBJ_HANDLE_NULL ? 12 : 0,
            .consumable = entries[i].item_obj != OBJ_HANDLE_NULL };
    }
    int selected = utility_ai_choose(&context, candidates, count, NULL, NULL, NULL);
    CHECK(selected >= 0);
    if (selected >= 0) CHECK(candidates[selected].item == 10);
    context.fatigue = 50;
    for (int i = 0; i < count; i++) if (candidates[i].item == OBJ_HANDLE_NULL) candidates[i].initial_cost = 5;
    selected = utility_ai_choose(&context, candidates, count, NULL, NULL, NULL);
    CHECK(selected >= 0);
    if (selected >= 0) CHECK(candidates[selected].item == OBJ_HANDLE_NULL);
    CHECK(legacy.actions[action].cnt == 2);
    items[10].mana = 0;
    count = mt_ai_utility_actions_collect(1, action, entries, MT_AI_UTILITY_RESOURCE_LIMIT);
    CHECK(count == 2 && !contains(entries, count, 3, 10));
    items[10].mana = 1;
    known[1] = known[2] = false;
    count = mt_ai_utility_actions_collect(1, action, entries, MT_AI_UTILITY_RESOURCE_LIMIT);
    CHECK(count == 1 && contains(entries, count, 3, 10));
    items[10].spells[1] = 3;
    potion_init(12, 3);
    inventory[inventory_count++] = 12;
    count = mt_ai_utility_actions_collect(1, action, entries, MT_AI_UTILITY_RESOURCE_LIMIT);
    CHECK(count == 2);
    CHECK(contains(entries, count, 3, 10) && contains(entries, count, 3, 12));
    items[10].spells[1] = -2;
    items[10].spells[2] = MT_SPELL_COUNT;
    CHECK(mt_ai_utility_actions_collect(1, action, entries, MT_AI_UTILITY_RESOURCE_LIMIT) == 2);
    items[11] = (ResourceItem){ .type = OBJ_TYPE_FOOD, .flags = OIF_VALID_AI_ACTION | OIF_CAN_USE_BOX,
        .action = MAGICTECH_AI_ACTION_FATIGUE_RECOVER };
    inventory[inventory_count++] = 11;
    count = mt_ai_utility_actions_collect(1, MAGICTECH_AI_ACTION_FATIGUE_RECOVER, entries, MT_AI_UTILITY_RESOURCE_LIMIT);
    CHECK(count == 1 && contains(entries, count, -1, 11));
    inventory_count--;
    CHECK(mt_ai_utility_actions_collect(1, MAGICTECH_AI_ACTION_FATIGUE_RECOVER, entries, MT_AI_UTILITY_RESOURCE_LIMIT) == 0);
    known[1] = known[2] = true;
    entries[MT_AI_UTILITY_RESOURCE_LIMIT] = (MagicTechAiActionListEntry){ 12345, 67890 };
    count = mt_ai_utility_actions_collect(1, action, entries, MT_AI_UTILITY_RESOURCE_LIMIT + 1);
    CHECK(count == 4);
    CHECK(entries[MT_AI_UTILITY_RESOURCE_LIMIT].spell == 12345 && entries[MT_AI_UTILITY_RESOURCE_LIMIT].item_obj == 67890);
    for (int item = 20; item < 60; item++) {
        potion_init(item, 3);
        inventory[inventory_count++] = item;
    }
    count = mt_ai_utility_actions_collect(1, action, entries, MT_AI_UTILITY_RESOURCE_LIMIT + 1);
    CHECK(count == MT_AI_UTILITY_RESOURCE_LIMIT);
    CHECK(entries[0].item_obj == OBJ_HANDLE_NULL && entries[1].item_obj == 10);
    CHECK(entries[MT_AI_UTILITY_RESOURCE_LIMIT].spell == 12345 && entries[MT_AI_UTILITY_RESOURCE_LIMIT].item_obj == 67890);
    CHECK(mt_ai_utility_actions_collect(1, action, entries, 0) == 0);
    CHECK(mt_ai_utility_actions_collect(1, -1, entries, 32) == 0);
    CHECK(mt_ai_utility_actions_collect(1, action, NULL, 32) == 0);
    mt_ai_create(&legacy, 1, action);
    CHECK(legacy.actions[action].cnt == 2);
    CHECK(!contains(legacy.actions[action].entries, legacy.actions[action].cnt, 3, 10));
    mt_ai_exit();
    tig_memory_exit();
    return failures != 0;
}
