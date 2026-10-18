#include "game/activity_time.h"

#include <math.h>
#include <stdlib.h>

#include "game/combat.h"
#include "game/critter.h"
#include "game/gamelib.h"
#include "game/gfade.h"
#include "game/light_scheme.h"
#include "game/map.h"
#include "game/player.h"
#include "game/skill.h"
#include "ui/charedit_ui.h"
#include "ui/dialog_ui.h"
#include "ui/intgame.h"
#include "ui/mainmenu_ui.h"

#define MAX_ACTIVITY_MS (720U * 3600000U)

static const char* const keys[ACTIVITY_TIME_COUNT] = {
    "training_apprentice", "training_expert", "training_master", "educator",
    "stat_points", "skill_points", "spell_points", "tech_points", "schematic",
};
static uint64_t pending_ms[ACTIVITY_TIME_COUNT];

#define ACTIVITY_TIME_FILE "Save\\Current\\activity_time.bin"

unsigned int activity_time_parse_hours(const char* text)
{
    if (text == NULL || *text == '\0') return 0;
    char* end;
    double hours = strtod(text, &end);
    while (*end == ' ' || *end == '\t') end++;
    if (end == text || *end != '\0' || !isfinite(hours) || hours <= 0.0 || hours > 720.0) return 0;
    return (unsigned int)(hours * 3600000.0 + 0.5);
}

void activity_time_init(void)
{
    activity_time_reset();
    for (int i = 0; i < ACTIVITY_TIME_COUNT; i++) settings_register(&settings, keys[i], "0", NULL);
}

void activity_time_reset(void)
{
    for (int i = 0; i < ACTIVITY_TIME_COUNT; i++) pending_ms[i] = 0;
}

bool activity_time_save(void)
{
    TigFile* file = tig_file_fopen(ACTIVITY_TIME_FILE, "wb");
    if (file == NULL) return false;
    uint32_t data[ACTIVITY_TIME_COUNT + 2] = { 1, ACTIVITY_TIME_COUNT };
    for (int i = 0; i < ACTIVITY_TIME_COUNT; i++) data[i + 2] = (uint32_t)pending_ms[i];
    bool ok = tig_file_fwrite(data, sizeof(data), 1, file) == 1;
    if (tig_file_fclose(file) != 0) ok = false;
    return ok;
}

void activity_time_load(void)
{
    activity_time_reset();
    TigFile* file = tig_file_fopen(ACTIVITY_TIME_FILE, "rb");
    if (file == NULL) return;
    uint32_t data[ACTIVITY_TIME_COUNT + 2];
    bool ok = tig_file_fread(data, sizeof(data), 1, file) == 1
        && data[0] == 1 && data[1] == ACTIVITY_TIME_COUNT;
    tig_file_fclose(file);
    if (!ok) return;
    for (int i = 0; i < ACTIVITY_TIME_COUNT; i++) if (data[i + 2] > MAX_ACTIVITY_MS) return;
    for (int i = 0; i < ACTIVITY_TIME_COUNT; i++) pending_ms[i] = data[i + 2];
}

void activity_time_record(int64_t actor, ActivityTimeCategory category, int count)
{
    if (category < 0 || category >= ACTIVITY_TIME_COUNT || count == 0
        || gamelib_in_load() || gamelib_in_reset() || charedit_is_creation()) return;
    int64_t pc = player_get_local_pc_obj();
    if (pc == OBJ_HANDLE_NULL || actor == OBJ_HANDLE_NULL
        || (actor != pc && critter_pc_leader_get(actor) != pc)) return;
    unsigned int duration = activity_time_parse_hours(settings_get_str_value(&settings, keys[category]));
    uint64_t amount = (uint64_t)duration * (count < 0 ? -(int64_t)count : count);
    if (count < 0) {
        pending_ms[category] = amount >= pending_ms[category] ? 0 : pending_ms[category] - amount;
    } else {
        pending_ms[category] += amount;
        if (pending_ms[category] > MAX_ACTIVITY_MS) pending_ms[category] = MAX_ACTIVITY_MS;
    }
}

void activity_time_training(int64_t actor, int before, int after)
{
    if (after > before && after >= TRAINING_APPRENTICE && after <= TRAINING_MASTER) {
        activity_time_record(actor, (ActivityTimeCategory)(after - TRAINING_APPRENTICE), 1);
    }
}

void activity_time_ping(void)
{
    uint64_t total = 0;
    for (int i = 0; i < ACTIVITY_TIME_COUNT; i++) total += pending_ms[i];
    if (total == 0) return;
    if (gamelib_in_load() || gamelib_in_reset()) { activity_time_reset(); return; }
    int64_t pc = player_get_local_pc_obj();
    if (pc == OBJ_HANDLE_NULL || map_is_clearing_objects() || gamelib_in_save()
        || combat_turn_based_is_active() || combat_critter_is_combat_mode_active(pc)
        || dialog_ui_is_local_pc_in_dialog() || charedit_is_created()
        || mainmenu_ui_is_active() || intgame_mode_get() != INTGAME_MODE_MAIN) return;
    unsigned int duration = total > MAX_ACTIVITY_MS ? MAX_ACTIVITY_MS : (unsigned int)total;
    activity_time_reset(); // Consume once before rendering; never re-enter this charge.
    FadeData fade = { 0, tig_color_make(0, 0, 0), 20, 0.35f };
    gfade_run(&fade);
    DateTime delta;
    delta.days = duration / 86400000U;
    delta.milliseconds = duration % 86400000U;
    timeevent_inc_datetime(&delta);
    light_scheme_set_hour(datetime_current_hour());
    gamelib_invalidate_rect(NULL);
    gamelib_draw();
    tig_window_display();
    fade.flags = FADE_IN;
    gfade_run(&fade);
}
