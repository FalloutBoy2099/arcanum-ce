#define tig_sound_quick_play quest_test_sound
#define tig_file_fread quest_test_read
#define tig_file_fwrite quest_test_write
#include "../src/game/quest.c"
#include <stdio.h>

static int failures, reads, writes, experience, sounds, alignment, intelligence = 10;
static PcQuestState states[3][2000];
static MesFileEntry definitions[3];
static int definition_count;
static bool missing;
static char* xp_text = "800";
static int saved[1000];
static bool truncated;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #expr); failures++; } } while (0)

bool mes_load(const char* path, mes_file_handle_t* handle)
{
    if (strstr(path, "xp_quest")) { *handle = 1; return true; }
    if (strstr(path, "gamequest.mes")) { *handle = 2; return !missing; }
    if (strstr(path, "gamequestlogdumb")) { *handle = 4; return true; }
    *handle = 3;
    return true;
}
bool mes_unload(mes_file_handle_t handle) { (void)handle; return true; }
int mes_entries_count(mes_file_handle_t handle) { (void)handle; return definition_count; }
bool mes_get_entry(mes_file_handle_t handle, int index, MesFileEntry* entry)
{
    (void)handle;
    if (index < 0 || index >= definition_count) return false;
    *entry = definitions[index];
    return true;
}
bool mes_search(mes_file_handle_t handle, MesFileEntry* entry)
{
    if (handle == 1 && entry->num == 1) { entry->str = xp_text; return true; }
    if (handle >= 3 && entry->num == 1000) { entry->str = handle == 3 ? "normal" : "simple"; return true; }
    return false;
}
int obj_field_int32_get(int64_t obj, int field) { (void)field; return obj == 1 || obj == 2 ? OBJ_TYPE_PC : OBJ_TYPE_NPC; }
void obj_arrayfield_pc_quest_get(int64_t obj, int field, int index, void* value)
{
    (void)field; reads++;
    CHECK(index >= 1000 && index <= 1999);
    if (index >= 1000 && index <= 1999) *(PcQuestState*)value = states[obj][index];
}
void obj_arrayfield_pc_quest_set(int64_t obj, int field, int index, void* value)
{
    (void)field; writes++;
    CHECK(index >= 1000 && index <= 1999);
    if (index >= 1000 && index <= 1999) states[obj][index] = *(PcQuestState*)value;
}
void obj_arrayfield_pc_quest_copy_to_flat(int64_t obj, int field, int count, void* data)
{
    (void)field; CHECK(count == 1999); memcpy(data, states[obj], sizeof(states[obj]));
}
void critter_give_xp(int64_t obj, int value) { (void)obj; experience += value; }
int stat_level_get(int64_t obj, int stat) { (void)obj; (void)stat; return intelligence; }
int stat_base_get(int64_t obj, int stat) { (void)obj; (void)stat; return alignment; }
int stat_base_set(int64_t obj, int stat, int value) { (void)obj; (void)stat; alignment = value; return value; }
int reaction_get(int64_t npc, int64_t pc) { (void)npc; (void)pc; return 50; }
void reaction_adj(int64_t npc, int64_t pc, int value) { (void)npc; (void)pc; (void)value; }
bool player_is_local_pc_obj(int64_t obj) { return obj == 1; }
void ui_toggle_primary_button(UiPrimaryButton button, bool on) { (void)button; (void)on; }
void quest_test_sound(int id) { (void)id; sounds++; }
DateTime game_time_get(void) { DateTime value = { 0 }; return value; }
int datetime_compare(const DateTime* a, const DateTime* b) { return memcmp(a, b, sizeof(*a)); }
size_t quest_test_read(void* data, size_t size, size_t count, TigFile* stream)
{
    (void)stream; CHECK(size * count == sizeof(saved));
    memcpy(data, saved, truncated ? sizeof(saved) / 2 : sizeof(saved));
    return truncated ? 0 : count;
}
size_t quest_test_write(const void* data, size_t size, size_t count, TigFile* stream)
{
    (void)stream; CHECK(size * count == sizeof(saved)); memcpy(saved, data, sizeof(saved)); return count;
}

static char valid[] = "1 +10 0 1 2 3 4 5 6 10 11 12 13 14 15 16 20 21 22 23 24 25 26";
static void reload(void)
{
    quest_mod_unload();
    definition_count = 2;
    definitions[0] = (MesFileEntry){ 1000, valid };
    definitions[1] = (MesFileEntry){ 1999, valid };
    memset(states, 0, sizeof(states));
    missing = false;
    CHECK(quest_mod_load());
}

int main(void)
{
    Quest parsed = { 0 };
    CHECK(quest_definition_parse("0\t-50\n-1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1  ", &parsed));
    CHECK(parsed.experience_level == 0 && parsed.alignment_adjustment == -50 && parsed.dumb_dialog[6] == -1);
    CHECK(!quest_definition_parse("1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0", &parsed));
    CHECK(!quest_definition_parse("1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0", &parsed));
    CHECK(quest_init(NULL));
    reload();
    CHECK(quest_dialog_line(1, 3, 1000) == 0);
    CHECK(quest_state_set(1, 1999, QUEST_STATE_ACCEPTED, 0) == QUEST_STATE_ACCEPTED);
    CHECK(quest_dialog_line(1, 3, 1999) == 2);
    char description[32] = "stale";
    quest_copy_description(1, 1000, description, sizeof(description));
    CHECK(strcmp(description, "normal") == 0);
    intelligence = 3;
    CHECK(quest_dialog_line(1, 3, 1000) == 20);
    quest_copy_description(1, 1000, description, sizeof(description));
    CHECK(strcmp(description, "simple") == 0);
    intelligence = 10;
    quest_mod_unload();
    quest_copy_description(1, 1000, description, sizeof(description));
    CHECK(description[0] == '\0');

    const char* bad[] = { "", "1 2", "x 0", "-1 0", "1 +-50", "2147483648 0",
        "1 0 -2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0",
        "1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 extra",
        "1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 2147483648" };
    for (int i = 0; i < (int)SDL_arraysize(bad); i++) {
        definitions[1].str = (char*)bad[i];
        CHECK(!quest_mod_load());
        CHECK(quests[0].normal_dialog[0] == -1);
        CHECK(quests[0].alignment_adjustment == 0);
    }
    const int invalid_ids[] = { INT_MIN, -1, 0, 999, 2000, INT_MAX };
    for (int i = 0; i < (int)SDL_arraysize(invalid_ids); i++) {
        definitions[1] = (MesFileEntry){ invalid_ids[i], valid };
        CHECK(quest_mod_load());
        CHECK(quests[0].normal_dialog[0] == 0);
    }
    definitions[1] = (MesFileEntry){ 1, "auxiliary catalog entry" };
    definitions[2] = (MesFileEntry){ 1999, valid };
    definition_count = 3;
    CHECK(quest_mod_load());
    CHECK(quests[0].normal_dialog[0] == 0 && quests[999].normal_dialog[6] == 6);
    missing = true;
    CHECK(quest_mod_load());
    CHECK(quest_global_state_get(1000) == QUEST_STATE_ACCEPTED);
    reload();
    for (int i = 0; i < (int)SDL_arraysize(invalid_ids); i++) {
        int id = invalid_ids[i], old_reads = reads, old_writes = writes;
        CHECK(quest_state_get(1, id) == QUEST_STATE_UNKNOWN);
        CHECK(quest_state_set(1, id, QUEST_STATE_COMPLETED, 3) == QUEST_STATE_UNKNOWN);
        CHECK(quest_global_state_get(id) == QUEST_STATE_UNKNOWN);
        CHECK(quest_global_state_set(id, QUEST_STATE_COMPLETED) == QUEST_STATE_UNKNOWN);
        CHECK(quest_unbotch(1, id) == QUEST_STATE_UNKNOWN);
        CHECK(quest_dialog_line(1, 3, id) == 0);
        strcpy(description, "stale");
        quest_copy_description(1, id, description, sizeof(description));
        CHECK(description[0] == '\0');
        CHECK(reads == old_reads && writes == old_writes && experience == 0);
    }
    CHECK(quest_state_set(1, 1000, -1, 3) == QUEST_STATE_UNKNOWN);
    CHECK(quest_state_set(1, 1000, QUEST_STATE_COUNT, 3) == QUEST_STATE_UNKNOWN);
    CHECK(quest_global_state_set(1000, INT_MAX) == QUEST_STATE_UNKNOWN);
    CHECK(quest_state_set(1, 1000, QUEST_STATE_COMPLETED, 3) == QUEST_STATE_COMPLETED);
    CHECK(experience == 800 && alignment == 10 && sounds == 1);
    CHECK(quest_state_set(1, 1000, QUEST_STATE_COMPLETED, 3) == QUEST_STATE_COMPLETED);
    CHECK(experience == 800 && sounds == 1);
    char* bad_xp[] = { "-1", "2147483648", "800junk", "" };
    for (int i = 0; i < (int)SDL_arraysize(bad_xp); i++) { xp_text = bad_xp[i]; CHECK(quest_get_xp(1) == 0); }
    xp_text = "800";

    reload();
    states[1][999].state = QUEST_STATE_ACCEPTED;
    states[1][1000].state = QUEST_STATE_ACCEPTED;
    states[1][1001].state = INT_MAX;
    states[1][1999].state = QUEST_STATE_ACCEPTED | QUEST_BOTCHED_MODIFIER;
    CHECK(quest_state_get(1, 1001) == QUEST_STATE_UNKNOWN);
    QuestLogbookEntry entries[1000];
    CHECK(quest_get_logbook_data(1, entries) == 2);
    CHECK(quest_copy_accepted(1, 2));
    CHECK(states[2][999].state == 0 && states[2][1001].state == 0);
    CHECK(quest_state_get(2, 1000) == QUEST_STATE_ACCEPTED);
    CHECK(quest_state_get(2, 1999) == QUEST_STATE_BOTCHED);
    CHECK(quest_unbotch(2, 1999) == QUEST_STATE_ACCEPTED);
    quest_global_state_set(1001, QUEST_STATE_BOTCHED);
    CHECK(quest_unbotch(1, 1001) == QUEST_STATE_UNKNOWN);
    CHECK(quest_global_state_get(1001) == QUEST_STATE_BOTCHED);

    CHECK(quest_save((TigFile*)1));
    GameLoadInfo info = { 0 }; info.stream = (TigFile*)1;
    quest_reset();
    CHECK(quest_load(&info));
    CHECK(quest_global_state_get(1001) == QUEST_STATE_BOTCHED);
    saved[0] = QUEST_STATE_COMPLETED;
    saved[999] = INT_MAX;
    CHECK(!quest_load(&info));
    CHECK(quest_global_state_get(1000) == QUEST_STATE_ACCEPTED);
    saved[999] = QUEST_STATE_ACCEPTED; truncated = true;
    CHECK(!quest_load(&info));
    CHECK(quest_global_state_get(1000) == QUEST_STATE_ACCEPTED);
    CHECK(!quest_load(NULL));
    quest_mod_unload(); quest_exit();
    CHECK(quest_state_get(1, 1000) == QUEST_STATE_UNKNOWN);
    printf("Quest validation: %d failures\n", failures);
    return failures ? 1 : 0;
}
