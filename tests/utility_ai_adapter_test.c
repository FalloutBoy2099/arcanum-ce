#define tig_timer_now adapter_test_now
#define tig_timer_elapsed adapter_test_elapsed
#include "../src/game/ai_utility_adapter.c"
#include <stdio.h>

static int failures, mode = 2, current_map = 7, minimum_distance = 1;
static int health[32], fatigue[32], costs[MT_SPELL_COUNT], types[32], collect_count[MAGICTECH_AI_ACTION_COUNT];
static unsigned int npc_flags, critter_flags;
static int64_t actor_leader = 2, potion_owner = 1, actor_location = 10;
static int interrupted = -1, moves;
static bool turn_based, moving, move_ok = true;
static int path_result = 1;
static tig_timestamp_t now = 1000;
static MagicTechInfo spells[MT_SPELL_COUNT];
static MagicTechRunInfo runs[512];
static MagicTechAiActionListEntry available[MAGICTECH_AI_ACTION_COUNT][3];
static int available_count[MAGICTECH_AI_ACTION_COUNT];
MagicTechInfo* magictech_spells = spells;
MagicTechRunInfo* magictech_run_info = runs;
Settings settings;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #expr); failures++; } } while (0)

int settings_get_value(Settings* value, const char* key) { (void)value; CHECK(strcmp(key, "utility_ai") == 0); return mode; }
int obj_field_int32_get(int64_t obj, int field)
{
    if (field == OBJ_F_TYPE) return types[obj];
    if (obj == 1 && field == OBJ_F_NPC_FLAGS) return npc_flags;
    if (obj == 1 && field == OBJ_F_CRITTER_FLAGS) return critter_flags;
    return 0;
}
int64_t obj_field_int64_get(int64_t obj, int field) { (void)field; return obj == 1 ? actor_location : 20; }
int obj_arrayfield_length_get(int64_t obj, int field) { (void)obj; (void)field; return 0; }
int64_t obj_arrayfield_handle_get(int64_t obj, int field, int index) { (void)obj; (void)field; (void)index; return 0; }
int64_t critter_pc_leader_get(int64_t obj) { return obj == 1 ? actor_leader : 0; }
int critter_fatigue_current(int64_t obj) { return fatigue[obj]; }
int critter_fatigue_max(int64_t obj) { (void)obj; return 100; }
int critter_num_followers(int64_t obj, bool excluded) { (void)obj; (void)excluded; return 0; }
bool critter_is_dead(int64_t obj) { return health[obj] == 0; }
bool critter_is_unconscious(int64_t obj) { (void)obj; return false; }
int object_hp_current(int64_t obj) { return health[obj]; }
int object_hp_max(int64_t obj) { (void)obj; return 100; }
int64_t object_dist(int64_t a, int64_t b) { return a == b ? 0 : 4; }
int stat_level_get(int64_t obj, int stat) { (void)obj; (void)stat; return 0; }
int magictech_active_maintenance_cost_estimate(int64_t obj, int horizon) { (void)obj; (void)horizon; return 0; }
bool magictech_is_under_influence_of(int64_t obj, int spell) { (void)obj; (void)spell; return false; }
void magictech_interrupt_delayed(int id) { interrupted = id; }
int spell_cast_cost(int spell, int64_t obj) { (void)obj; return costs[spell]; }
int spell_maintain_cost(int spell, int64_t obj, int* period) { (void)spell; (void)obj; *period = 0; return 0; }
int64_t item_wield_get(int64_t obj, int slot) { (void)obj; (void)slot; return 0; }
bool item_parent(int64_t item, int64_t* owner) { CHECK(item == 10); *owner = potion_owner; return potion_owner != 0; }
int item_check_use_box_flag(int64_t obj, int64_t item) { (void)obj; (void)item; return ITEM_CANNOT_OK; }
bool mt_item_valid_ai_action(int64_t item) { (void)item; return true; }
int mt_ai_secondary_defense_score_get(int spell) { (void)spell; return 0; }
int mt_ai_utility_actions_collect(int64_t obj, int action, MagicTechAiActionListEntry* entries, int capacity)
{
    CHECK(obj == 1 && capacity == MT_AI_UTILITY_RESOURCE_LIMIT);
    collect_count[action]++;
    memcpy(entries, available[action], available_count[action] * sizeof(*entries));
    return available_count[action];
}
int ai_magictech_action_check(int64_t actor, int64_t item, int64_t target, int spell)
{
    CHECK(actor == 1 && target != 0 && spell >= 0); (void)item; return 0;
}
void ai_copy_params(int64_t obj, AiParams* params) { (void)obj; memset(params, 0, sizeof(*params)); params->minimum_combat_distance = minimum_distance; }
void ai_danger_source(int64_t obj, int* type, int64_t* target) { (void)obj; *type = AI_DANGER_SOURCE_TYPE_NONE; *target = 0; }
int ai_can_see(int64_t actor, int64_t target) { (void)actor; (void)target; return 0; }
void ai_objects_in_radius(int64_t obj, int radius, ObjectList* objects, unsigned int flags) { (void)obj; (void)radius; (void)flags; memset(objects, 0, sizeof(*objects)); }
void object_list_destroy(ObjectList* objects) { (void)objects; }
bool combat_turn_based_is_active(void) { return turn_based; }
int map_current_map(void) { return current_map; }
int path_make(PathCreateInfo* path) { CHECK(path->max_rotations == 1); return path_result; }
bool location_in_dir(int64_t origin, int direction, int64_t* destination) { *destination = origin + direction + 1; return true; }
int64_t location_dist(int64_t a, int64_t b) { return a > b ? a - b : b - a; }
bool anim_goal_move_to_tile(int64_t obj, int64_t destination) { CHECK(obj == 1 && destination != 0); moves++; return move_ok; }
bool anim_is_current_goal_type(int64_t obj, int type, AnimID* id) { (void)id; CHECK(obj == 1 && type == AG_MOVE_TO_TILE); return moving; }
int adapter_test_now(tig_timestamp_t* timestamp) { *timestamp = now; return TIG_OK; }
tig_duration_t adapter_test_elapsed(tig_timestamp_t start) { return (tig_duration_t)(now - start); }
DateTime game_time_get(void) { DateTime time = { 0 }; return time; }

static Ai context(void)
{
    Ai ai = { 0 };
    ai.obj = 1; ai.danger_source = 3; ai.danger_type = AI_DANGER_SOURCE_TYPE_COMBAT_FOCUS;
    return ai;
}
static void resources_reset(void)
{
    memset(available_count, 0, sizeof(available_count));
    memset(collect_count, 0, sizeof(collect_count));
}

int main(void)
{
    for (int index = 0; index < 32; index++) { health[index] = 100; fatigue[index] = 100; types[index] = OBJ_TYPE_NPC; }
    types[2] = OBJ_TYPE_PC; types[10] = OBJ_TYPE_FOOD;
    Ai ai = context();
    CHECK(ai_utility_mode(&ai) == 2);
    mode = 0; CHECK(ai_utility_mode(&ai) == 0);
    mode = 4; CHECK(ai_utility_mode(&ai) == 0);
    mode = 1; CHECK(ai_utility_mode(&ai) == 1);
    actor_leader = 0; mode = 2; CHECK(ai_utility_mode(&ai) == 0);
    mode = 3; CHECK(ai_utility_mode(&ai) == 3);
    types[1] = OBJ_TYPE_PC; CHECK(ai_utility_mode(&ai) == 0); types[1] = OBJ_TYPE_NPC;
    actor_leader = 2; mode = 2;
    int heal = MAGICTECH_AI_ACTION_HEAL_LIGHT;
    available_count[heal] = 2;
    available[heal][0] = (MagicTechAiActionListEntry){ .spell = 1, .item_obj = 0 };
    available[heal][1] = (MagicTechAiActionListEntry){ .spell = 1, .item_obj = 10 };
    spells[1].ai.values[heal] = 80;
    health[1] = 20; fatigue[1] = 10; costs[1] = 80;
    Ai before = ai;
    CHECK(!ai_utility_spell_select(&ai, false));
    CHECK(memcmp(&ai, &before, sizeof(ai)) == 0);
    memset(collect_count, 0, sizeof(collect_count));
    CHECK(ai_utility_spell_select(&ai, true));
    CHECK(ai.utility_selected && ai.action_type == AI_ACTION_TYPE_USE_ITEM && ai.item_obj == 10 && ai.danger_source == 1);
    for (int action = 0; action < MAGICTECH_AI_ACTION_COUNT; action++) CHECK(collect_count[action] <= 1);
    ai = context(); fatigue[1] = 100; costs[1] = 1;
    CHECK(ai_utility_spell_select(&ai, true));
    CHECK(ai.action_type == AI_ACTION_TYPE_CAST_SPELL && ai.item_obj == 0 && ai.spell == 1);
    ai = context();
    available[heal][0] = (MagicTechAiActionListEntry){ .spell = 1, .item_obj = 10 };
    available[heal][1] = (MagicTechAiActionListEntry){ .spell = 2, .item_obj = 0 };
    spells[1].ai.values[heal] = 100; costs[2] = 1; potion_owner = 2;
    CHECK(ai_utility_spell_select(&ai, true));
    CHECK(ai.action_type == AI_ACTION_TYPE_CAST_SPELL && ai.spell == 2 && ai.item_obj == 0);
    resources_reset();
    health[1] = 100; health[2] = 5; potion_owner = 1;
    available_count[heal] = 1; available[heal][0] = (MagicTechAiActionListEntry){ .spell = 2, .item_obj = 0 };
    ai = context(); CHECK(ai_utility_spell_select(&ai, true)); CHECK(ai.danger_source == 2);
    resources_reset(); health[2] = 100; ai = context(); before = ai;
    CHECK(!ai_utility_spell_select(&ai, true)); CHECK(memcmp(&ai, &before, sizeof(ai)) == 0);
    ai.danger_source = 0; CHECK(!ai_utility_spell_select(&ai, true));

    ai = context(); ai_utility_reset(); now = 1000; actor_location = 10;
    ai_retreat_cooldowns[0] = (AiRetreatCooldown){ .actor = 1, .map = 7, .started = now, .delay = 5000, .destination = 9 };
    moving = true; now = 1500; CHECK(ai_utility_retreat_pending(&ai));
    moving = false; CHECK(!ai_utility_retreat_pending(&ai)); moving = true;
    actor_location = 9; CHECK(!ai_utility_retreat_pending(&ai)); actor_location = 10;
    current_map = 8; CHECK(!ai_utility_retreat_pending(&ai)); current_map = 7;
    now = 3000; CHECK(!ai_utility_retreat_pending(&ai));
    ai.action_type = AI_ACTION_TYPE_RETREAT; ai.move_destination = 8;
    CHECK(ai_utility_action_perform(&ai)); CHECK(moves == 1 && ai_retreat_cooldowns[0].delay == 5000);
    CHECK(ai_retreat_cooldowns[0].destination == 8 && ai_utility_retreat_pending(&ai));
    npc_flags = ONF_AI_WAIT_HERE; CHECK(!ai_utility_action_perform(&ai) && moves == 1); npc_flags = 0;
    turn_based = true; CHECK(!ai_utility_action_perform(&ai)); turn_based = false;
    path_result = 0; CHECK(!ai_utility_action_perform(&ai)); path_result = 1;
    ai_utility_reset(); CHECK(!ai_utility_retreat_pending(&ai));

    ai = context(); ai.action_type = AI_ACTION_TYPE_RELEASE_MAINTENANCE; ai.maintenance_id = 4; ai.spell = 3;
    fatigue[1] = 10;
    runs[4] = (MagicTechRunInfo){ .id = 4, .spell = 3, .action = MAGICTECH_ACTION_MAINTAIN, .flags = MAGICTECH_RUN_ACTIVE | MAGICTECH_RUN_UTILITY_AI };
    runs[4].source_obj.obj = 1; runs[4].parent_obj.obj = 1; runs[4].target_obj.obj = 3;
    spells[3].maintenance.cost = 2; spells[3].maintenance.period = 1; spells[3].ai.values[MAGICTECH_AI_ACTION_OFFENSIVE] = 1;
    CHECK(ai_utility_action_perform(&ai)); CHECK(interrupted == 4 && (runs[4].flags & MAGICTECH_RUN_UTILITY_AI) == 0);
    interrupted = -1; CHECK(!ai_utility_action_perform(&ai) && interrupted == -1);
    runs[4].flags |= MAGICTECH_RUN_UTILITY_AI | MAGICTECH_RUN_FREE;
    CHECK(!ai_utility_action_perform(&ai));
    runs[4].flags &= ~MAGICTECH_RUN_FREE; runs[4].target_obj.obj = 2;
    CHECK(!ai_utility_action_perform(&ai));
    runs[4].target_obj.obj = 3; fatigue[1] = 50;
    CHECK(!ai_utility_action_perform(&ai));
    printf("Utility adapter: %d failures\n", failures);
    return failures ? 1 : 0;
}
