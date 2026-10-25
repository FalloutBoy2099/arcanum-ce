#define tig_file_fread runtime_test_read
#define tig_file_ftell runtime_test_tell
#define tig_file_filelength runtime_test_length
#include "../src/game/magictech_runtime.c"
#include "../src/game/mt_obj_node.c"
#include <stdio.h>

static int failures, checked_cost, checked_spell, race, mastery = -1, recover_calls;
static bool handles_valid = true;
static MagicTechRunInfo runs[MAGICTECH_RUN_CAPACITY];
static MagicTechInfo spells[MT_SPELL_COUNT];
MagicTechRunInfo* magictech_run_info = runs;
MagicTechInfo* magictech_spells = spells;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #expr); failures++; } } while (0)

typedef struct RuntimeStream {
    unsigned char bytes[400000];
    size_t length;
    size_t position;
} RuntimeStream;
static RuntimeStream data;

size_t runtime_test_read(void* output, size_t size, size_t count, TigFile* stream)
{
    RuntimeStream* input = (RuntimeStream*)stream;
    if (count > (input->length - input->position) / size) return 0;
    memcpy(output, input->bytes + input->position, size * count);
    input->position += size * count;
    return count;
}
int runtime_test_tell(TigFile* stream) { return (int)((RuntimeStream*)stream)->position; }
int runtime_test_length(TigFile* stream) { return (int)((RuntimeStream*)stream)->length; }
static void append(const void* bytes, size_t size)
{
    CHECK(data.length + size <= sizeof(data.bytes));
    memcpy(data.bytes + data.length, bytes, size);
    data.length += size;
}
static void integer(int value) { append(&value, sizeof(value)); }
static void header(int allocated, int count) { memset(&data, 0, sizeof(data)); integer(allocated); integer(count); }
static void node(bool detached)
{
    ObjectID id = { 0 };
    int64_t location = 0;
    append(&id, sizeof(id)); append(&location, sizeof(location)); integer(0);
    integer(0); integer(0);
    if (detached) append(&location, sizeof(location));
}
static void record(int id, int spell, int action, int nodes)
{
    integer(id); integer(spell); integer(action);
    for (int i = 0; i < 4; i++) node(true);
    integer(nodes);
    for (int i = 0; i < nodes; i++) node(false);
    integer(0);
    integer(0); integer(MAGICTECH_RUN_ACTIVE); integer(0); integer(0);
    DateTime time = { 0 }; append(&time, sizeof(time)); integer(0);
}
int obj_field_int32_get(int64_t obj, int field) { (void)obj; (void)field; return OBJ_TYPE_PC; }
int stat_level_get(int64_t obj, int stat) { (void)obj; (void)stat; return race; }
int spell_mastery_get(int64_t obj) { (void)obj; return mastery; }
bool magictech_run_object_handles_recover(MagicTechRunInfo* run) { (void)run; recover_calls++; return handles_valid; }
bool magictech_cost_check_or_consume(int64_t obj, int cost, bool consume, int spell)
{
    (void)obj; CHECK(!consume); checked_cost = cost; checked_spell = spell; return cost <= 20;
}
bool object_load_obj_handle_safe(int64_t* obj, ObjectReference* reference, TigFile* stream)
{
    ObjectID id;
    int64_t location;
    int map;
    *obj = 0; memset(reference, 0, sizeof(*reference));
    return runtime_test_read(&id, sizeof(id), 1, stream) == 1
        && runtime_test_read(&location, sizeof(location), 1, stream) == 1
        && runtime_test_read(&map, sizeof(map), 1, stream) == 1;
}
bool object_save_obj_handle_safe(int64_t* obj, ObjectReference* reference, TigFile* stream)
{
    (void)obj; (void)reference; (void)stream; return false;
}
static int free_nodes(void)
{
    int count = 0;
    for (MagicTechObjectNode* node = mt_obj_node_head; node != NULL; node = node->next) count++;
    return count;
}
static void reset_runs(void)
{
    for (int i = 0; i < MAGICTECH_RUN_CAPACITY; i++) {
        magictech_loaded_nodes_release(&runs[i]);
        memset(&runs[i], 0, sizeof(runs[i])); runs[i].id = -1;
    }
}

int main(int argc, char** argv)
{
    reset_runs();
    runs[400] = (MagicTechRunInfo){ .id = 400, .spell = 70, .action = MAGICTECH_ACTION_BEGIN, .flags = MAGICTECH_RUN_ACTIVE };
    runs[400].source_obj.obj = 1;
    spells[70].cost = 3; spells[70].maintenance.cost = 1;
    spells[5].cost = 90; spells[5].maintenance.cost = 40;
    CHECK(magictech_run_cost_check_for_mode(400, false));
    CHECK(checked_cost == 3 && checked_spell == 70);
    runs[400].action = MAGICTECH_ACTION_MAINTAIN;
    CHECK(magictech_run_cost_check_for_mode(400, false) && checked_cost == 1);
    CHECK(magictech_run_cost_check_for_mode(400, true) && checked_cost == 1);
    race = RACE_DWARF;
    CHECK(magictech_run_cost_check_for_mode(400, false) && checked_cost == 2);
    runs[400].action = MAGICTECH_ACTION_BEGIN;
    CHECK(magictech_run_cost_check_for_mode(400, false) && checked_cost == 6);
    mastery = COLLEGE_FROM_SPELL(70);
    CHECK(magictech_run_cost_check_for_mode(400, false) && checked_cost == 4);
    CHECK(magictech_run_cost_check_for_mode(400, true) && checked_cost == 2);
    spells[70].cost = INT_MAX; mastery = -1;
    CHECK(!magictech_run_cost_check_for_mode(400, false));
    spells[70].cost = -1;
    CHECK(!magictech_run_cost_check_for_mode(400, false));
    runs[400].flags |= MAGICTECH_RUN_FREE;
    CHECK(magictech_run_cost_check_for_mode(400, false));
    runs[400].spell = MT_SPELL_COUNT;
    CHECK(!magictech_run_cost_check_for_mode(400, false));
    const int invalid_ids[] = { INT_MIN, -2, -1, 512, INT_MAX };
    MagicTechRunInfo* found = (MagicTechRunInfo*)1;
    for (int i = 0; i < (int)SDL_arraysize(invalid_ids); i++) {
        int before = recover_calls;
        CHECK(!magictech_id_to_run_info(invalid_ids[i], &found));
        CHECK(found == NULL && recover_calls == before);
    }
    CHECK(!magictech_id_to_run_info(0, NULL));
    runs[511] = (MagicTechRunInfo){ .id = 511, .spell = 222, .action = MAGICTECH_ACTION_END_CALLBACK, .flags = MAGICTECH_RUN_ACTIVE };
    CHECK(magictech_id_to_run_info(511, &found) && found == &runs[511]);
    handles_valid = false; CHECK(!magictech_id_to_run_info(511, &found) && found == NULL); handles_valid = true;

    reset_runs();
    int allocated = 17;
    const int legacy_counters[] = { -1, INT_MIN, 513, INT_MAX };
    for (int index = 0; index < (int)SDL_arraysize(legacy_counters); index++) {
        header(legacy_counters[index], 512); integer(-512);
        size_t table_end = data.length;
        integer(0x12345678);
        CHECK(magictech_runs_load((TigFile*)&data, runs, &allocated));
        CHECK(allocated == 0 && data.position == table_end && runs[511].id == -1);
    }
    for (int argument = 1; argument < argc; argument++) {
        FILE* input = fopen(argv[argument], "rb");
        CHECK(input != NULL);
        if (input == NULL) continue;
        memset(&data, 0, sizeof(data));
        data.length = fread(data.bytes, 1, sizeof(data.bytes), input);
        CHECK(!ferror(input) && feof(input));
        fclose(input);
        CHECK(magictech_runs_load((TigFile*)&data, runs, &allocated));
        CHECK(data.position == data.length);
        printf("Save run table: %s, active: %d\n", argv[argument], allocated);
        reset_runs();
    }
    allocated = 17;
    const int invalid_counts[] = { -1, INT_MIN, 513, INT_MAX };
    for (int i = 0; i < (int)SDL_arraysize(invalid_counts); i++) {
        header(0, invalid_counts[i]);
        CHECK(!magictech_runs_load((TigFile*)&data, runs, &allocated) && allocated == 17);
        header(invalid_counts[i], 512);
        CHECK(!magictech_runs_load((TigFile*)&data, runs, &allocated));
    }
    const int invalid_extents[] = { 0, 513, -513, INT_MIN, INT_MAX };
    for (int i = 0; i < (int)SDL_arraysize(invalid_extents); i++) {
        header(0, 512); integer(invalid_extents[i]);
        CHECK(!magictech_runs_load((TigFile*)&data, runs, &allocated));
    }
    header(0, 512); integer(-511); integer(2);
    CHECK(!magictech_runs_load((TigFile*)&data, runs, &allocated));
    header(1, 512); integer(1); record(0, 70, MAGICTECH_ACTION_MAINTAIN, 2); integer(-511);
    size_t complete_length = data.length;
    unsigned char complete[1024]; CHECK(complete_length <= sizeof(complete)); memcpy(complete, data.bytes, complete_length);
    CHECK(magictech_runs_load((TigFile*)&data, runs, &allocated));
    CHECK(allocated == 1 && runs[0].spell == 70 && runs[511].id == -1);
    CHECK(runs[0].objlist != NULL && runs[0].objlist->next != NULL);
    int pool_before = free_nodes();
    for (size_t size = 0; size < complete_length; size++) {
        data.position = 0; data.length = size;
        CHECK(!magictech_runs_load((TigFile*)&data, runs, &allocated));
        CHECK(allocated == 1 && runs[0].id == 0 && runs[0].objlist != NULL);
        CHECK(free_nodes() == pool_before);
    }
    for (int field = 0; field < 3; field++) {
        for (int i = 0; i < 3; i++) {
            int invalid = i == 0 ? -2 : i == 1 ? INT_MAX : field == 0 ? 512 : field == 1 ? MT_SPELL_COUNT : MAGICTECH_ACTION_COUNT;
            memcpy(data.bytes, complete, complete_length); data.length = complete_length; data.position = 0;
            memcpy(data.bytes + 12 + field * sizeof(int), &invalid, sizeof(invalid));
            CHECK(!magictech_runs_load((TigFile*)&data, runs, &allocated));
            CHECK(free_nodes() == pool_before && allocated == 1);
        }
    }
    memcpy(data.bytes, complete, complete_length); data.length = complete_length; data.position = 0;
    int wrong_slot = 1; memcpy(data.bytes + 12, &wrong_slot, sizeof(wrong_slot));
    CHECK(!magictech_runs_load((TigFile*)&data, runs, &allocated) && free_nodes() == pool_before);
    header(1, 512); integer(1); record(0, 70, MAGICTECH_ACTION_BEGIN, -1); integer(-511);
    CHECK(!magictech_runs_load((TigFile*)&data, runs, &allocated));
    CHECK(free_nodes() == pool_before);
    header(0, 512); integer(-512);
    CHECK(magictech_runs_load((TigFile*)&data, runs, &allocated));
    CHECK(allocated == 0 && runs[0].id == -1 && runs[0].objlist == NULL);

    header(1, 512); integer(-511); integer(1); record(511, 222, MAGICTECH_ACTION_END_CALLBACK, 0);
    CHECK(magictech_runs_load((TigFile*)&data, runs, &allocated));
    CHECK(allocated == 1 && runs[511].id == 511 && runs[0].id == -1);
    header(0, 512); integer(-512);
    CHECK(magictech_runs_load((TigFile*)&data, runs, &allocated));
    CHECK(!magictech_runs_load(NULL, runs, &allocated));
    MagicTechRunInfo* saved_table = magictech_run_info;
    magictech_run_info = NULL;
    CHECK(!magictech_id_to_run_info(0, &found) && found == NULL);
    magictech_run_info = saved_table;
    int ids[512], loaded;
    for (int i = 0; i < (int)SDL_arraysize(invalid_counts); i++) {
        memset(&data, 0, sizeof(data)); integer(invalid_counts[i]);
        CHECK(!magictech_map_runs_load((TigFile*)&data, runs, &allocated, ids, &loaded));
        CHECK(loaded == 0 && allocated == 0);
    }
    header(0, 0); data.length = 0; integer(2); record(511, 70, MAGICTECH_ACTION_BEGIN, 1); record(511, 70, MAGICTECH_ACTION_BEGIN, 1);
    CHECK(magictech_map_runs_load((TigFile*)&data, runs, &allocated, ids, &loaded));
    CHECK(loaded == 2 && allocated == 2 && ids[0] == 511 && ids[1] == 0);
    CHECK(runs[511].id == 511 && runs[0].id == 0);
    pool_before = free_nodes();
    header(0, 0); data.length = 0; integer(2); record(1, 70, MAGICTECH_ACTION_BEGIN, 1); record(2, 70, MAGICTECH_ACTION_BEGIN, 1); data.length--;
    CHECK(!magictech_map_runs_load((TigFile*)&data, runs, &allocated, ids, &loaded));
    CHECK(loaded == 0 && allocated == 2 && runs[1].id == -1 && free_nodes() == pool_before);
    for (int slot = 0; slot < 512; slot++) if (runs[slot].id == -1) runs[slot].id = slot;
    header(0, 0); data.length = 0; integer(1); record(1, 70, MAGICTECH_ACTION_BEGIN, 0);
    CHECK(!magictech_map_runs_load((TigFile*)&data, runs, &allocated, ids, &loaded));
    reset_runs(); mt_obj_node_exit();
    printf("MagicTech runtime: %d failures\n", failures);
    return failures ? 1 : 0;
}
