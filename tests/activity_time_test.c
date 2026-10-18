#include "game/activity_time.h"
#include "game/gamelib.h"
#include "game/gfade.h"
#include "game/skill.h"
#include "ui/intgame.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

Settings settings;
static const char* keys[ACTIVITY_TIME_COUNT];
static const char* values[ACTIVITY_TIME_COUNT];
static int registered, failures, fades, advances, order;
static unsigned int elapsed;
static bool loading, resetting, saving, creating, sheet, dialogue, menu, combat, clearing;
static int64_t player = 1;
static IntgameMode mode = INTGAME_MODE_MAIN;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)

void settings_register(Settings* instance, const char* key, const char* value, SettingsValueChangedFunc callback)
{
    (void)instance; CHECK(callback == NULL && registered < ACTIVITY_TIME_COUNT);
    keys[registered] = key; values[registered++] = value;
}
const char* settings_get_str_value(Settings* instance, const char* key)
{
    (void)instance;
    for (int i = 0; i < registered; i++) if (strcmp(key, keys[i]) == 0) return values[i];
    CHECK(false); return NULL;
}
int64_t player_get_local_pc_obj(void) { return player; }
int64_t critter_pc_leader_get(int64_t actor) { return actor == 2 ? 1 : 0; }
bool gamelib_in_load(void) { return loading; }
bool gamelib_in_reset(void) { return resetting; }
bool gamelib_in_save(void) { return saving; }
bool charedit_is_creation(void) { return creating; }
bool charedit_is_created(void) { return sheet; }
bool dialog_ui_is_local_pc_in_dialog(void) { return dialogue; }
bool mainmenu_ui_is_active(void) { return menu; }
bool combat_turn_based_is_active(void) { return combat; }
bool combat_critter_is_combat_mode_active(int64_t obj) { CHECK(obj == 1); return combat; }
bool map_is_clearing_objects(void) { return clearing; }
IntgameMode intgame_mode_get(void) { return mode; }
void gfade_run(FadeData* fade)
{
    CHECK(fade->steps > 0 && fade->color == 0);
    if (fade->flags == 0) { CHECK(order == 0); order = 1; }
    else { CHECK(fade->flags == FADE_IN && order == 6); order = 0; }
    fades++;
}
bool timeevent_inc_datetime(DateTime* time) {
    CHECK(order == 1 && time->milliseconds < 86400000U);
    order = 2; elapsed = time->days * 86400000U + time->milliseconds;
    CHECK(elapsed > 0); advances++; return true;
}
int datetime_current_hour(void) { CHECK(order == 2); return 17; }
bool light_scheme_set_hour(int hour) { CHECK(order == 2 && hour == 17); order = 3; return true; }
void gamelib_invalidate_rect(TigRect* rect) { CHECK(rect == NULL && order == 3); order = 4; }
bool gamelib_draw(void) { CHECK(order == 4); order = 5; return true; }
int tig_window_display(void) { CHECK(order == 5); order = 6; return TIG_OK; }

static void reset(void)
{
    registered = fades = advances = order = 0; elapsed = 0;
    loading = resetting = saving = creating = sheet = dialogue = menu = combat = clearing = false;
    player = 1; mode = INTGAME_MODE_MAIN; activity_time_init();
    CHECK(registered == ACTIVITY_TIME_COUNT);
}

int main(void)
{
    CHECK(activity_time_parse_hours("0.5") == 1800000);
    CHECK(activity_time_parse_hours(" 1.25 \t") == 4500000);
    CHECK(activity_time_parse_hours("720") == 2592000000U);
    const char* invalid[] = { NULL, "", " ", "0", "-1", "nan", "inf", "1h", "1,5", "721", "1e99" };
    for (int i = 0; i < (int)(sizeof(invalid) / sizeof(*invalid)); i++) CHECK(activity_time_parse_hours(invalid[i]) == 0);
    reset(); activity_time_record(1, ACTIVITY_TIME_SCHEMATIC, 1); activity_time_ping();
    CHECK(advances == 0 && fades == 0);
    values[ACTIVITY_TIME_APPRENTICE] = "0.5";
    values[ACTIVITY_TIME_EXPERT] = "2"; values[ACTIVITY_TIME_MASTER] = "4";
    activity_time_training(1, TRAINING_NONE, TRAINING_APPRENTICE);
    activity_time_training(1, TRAINING_APPRENTICE, TRAINING_APPRENTICE);
    activity_time_training(1, TRAINING_APPRENTICE, TRAINING_NONE);
    activity_time_training(1, TRAINING_MASTER, 99);
    activity_time_ping(); CHECK(elapsed == 1800000 && advances == 1 && fades == 2);
    activity_time_ping(); CHECK(advances == 1);
    activity_time_training(1, TRAINING_APPRENTICE, TRAINING_EXPERT);
    activity_time_training(1, TRAINING_EXPERT, TRAINING_MASTER);
    activity_time_ping(); CHECK(elapsed == 21600000 && advances == 2);
    values[ACTIVITY_TIME_SKILL_POINTS] = "0.25";
    activity_time_record(2, ACTIVITY_TIME_SKILL_POINTS, 3);
    activity_time_record(2, ACTIVITY_TIME_SKILL_POINTS, -1);
    activity_time_record(3, ACTIVITY_TIME_SKILL_POINTS, 100);
    sheet = true; activity_time_ping(); CHECK(advances == 2);
    sheet = false; activity_time_ping(); CHECK(elapsed == 1800000 && advances == 3);
    for (int i = ACTIVITY_TIME_STAT_POINTS; i <= ACTIVITY_TIME_TECH_POINTS; i++) {
        values[i] = "1"; activity_time_record(1, (ActivityTimeCategory)i, 2);
        activity_time_record(1, (ActivityTimeCategory)i, -2);
    }
    activity_time_ping(); CHECK(advances == 3);

    reset(); values[ACTIVITY_TIME_SCHEMATIC] = "1";
    activity_time_record(1, ACTIVITY_TIME_SCHEMATIC, 1);
    bool* blockers[] = { &sheet, &dialogue, &menu, &combat, &clearing, &saving };
    for (int i = 0; i < (int)(sizeof(blockers) / sizeof(*blockers)); i++) {
        *blockers[i] = true; activity_time_ping(); CHECK(advances == 0); *blockers[i] = false;
    }
    mode = INTGAME_MODE_INVEN; activity_time_ping(); CHECK(advances == 0);
    mode = INTGAME_MODE_MAIN; activity_time_ping(); CHECK(elapsed == 3600000 && advances == 1);
    creating = true; activity_time_record(1, ACTIVITY_TIME_SCHEMATIC, 1); creating = false;
    loading = true; activity_time_record(1, ACTIVITY_TIME_SCHEMATIC, 1); loading = false;
    resetting = true; activity_time_record(1, ACTIVITY_TIME_SCHEMATIC, 1); resetting = false;
    activity_time_ping(); CHECK(advances == 1);
    activity_time_record(1, ACTIVITY_TIME_SCHEMATIC, 1);
    loading = true; activity_time_ping(); loading = false; activity_time_ping(); CHECK(advances == 1);
    activity_time_record(1, ACTIVITY_TIME_SCHEMATIC, 1);
    activity_time_reset(); activity_time_ping(); CHECK(advances == 1);
    activity_time_record(0, ACTIVITY_TIME_SCHEMATIC, 1);
    activity_time_record(1, ACTIVITY_TIME_COUNT, 1);
    activity_time_record(1, (ActivityTimeCategory)-1, 1);
    player = 0; activity_time_record(1, ACTIVITY_TIME_SCHEMATIC, 1); player = 1;
    activity_time_ping(); CHECK(advances == 1);
    values[ACTIVITY_TIME_SCHEMATIC] = "720";
    activity_time_record(1, ACTIVITY_TIME_SCHEMATIC, INT_MAX);
    activity_time_ping(); CHECK(elapsed == 2592000000U && advances == 2);
    // Pending charges survive a save made before the dialog/sheet closes.
    CHECK(SDL_CreateDirectory("Save")); CHECK(SDL_CreateDirectory("Save/Current"));
    CHECK(tig_file_repository_add("."));
    activity_time_record(1, ACTIVITY_TIME_SCHEMATIC, 1);
    CHECK(activity_time_save()); activity_time_reset(); activity_time_load();
    activity_time_ping(); CHECK(advances == 3 && elapsed == 2592000000U);
    TigFile* bad = tig_file_fopen("Save\\Current\\activity_time.bin", "wb");
    CHECK(bad != NULL);
    if (bad != NULL) { uint32_t invalid_version = 99; tig_file_fwrite(&invalid_version, 4, 1, bad); tig_file_fclose(bad); }
    activity_time_load(); activity_time_ping(); CHECK(advances == 3);
    tig_file_remove("Save\\Current\\activity_time.bin");
    activity_time_load(); activity_time_ping(); CHECK(advances == 3); // Old save with no sidecar.
    tig_file_repository_remove_all();
    printf("Activity durations and transitions: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
