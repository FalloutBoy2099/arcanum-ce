#include "game/race_identity.h"

#include <stddef.h>
#include <string.h>
#include "game/stat.h"

static const char* const race_markers[RACE_COUNT] = {
    "HU", "DW", "EL", "HE", "GN", "HA", "HO", "HG", "DE", "OG", "OC",
};

const char* race_identity_marker(int race)
{
    return race >= 0 && race < RACE_COUNT ? race_markers[race] : NULL;
}

static bool race_identity_prefix_equals(const char* text, const char* prefix)
{
    for (int index = 0; index < 3; index++) {
        unsigned char value = (unsigned char)text[index];
        if (value == 0) return false;
        if (value >= 'a' && value <= 'z') value -= 'a' - 'A';
        if (value != (unsigned char)prefix[index]) return false;
    }
    return true;
}

static bool race_identity_build_prefix(int race, int gender, char* prefix)
{
    const char* marker = race_identity_marker(race);
    if (marker == NULL || gender < 0 || gender >= GENDER_COUNT) return false;
    prefix[0] = marker[0];
    prefix[1] = marker[1];
    prefix[2] = gender == GENDER_FEMALE ? 'F' : 'M';
    prefix[3] = 0;
    return true;
}

bool race_identity_portrait_matches(int race, int gender, bool npc, const char* name)
{
    char prefix[4];
    if (name == NULL || !race_identity_build_prefix(race, gender, prefix)) return false;
    if (race_identity_prefix_equals(name, prefix) || race_identity_prefix_equals(name, "ANY")) return true;
    if (npc && race_identity_prefix_equals(name, "NPC")) return true;
    int legacy_race;
    switch (race) {
    case RACE_DARK_ELF: legacy_race = RACE_ELF; break;
    case RACE_OGRE: legacy_race = RACE_HALF_OGRE; break;
    case RACE_ORC: legacy_race = RACE_HALF_ORC; break;
    default: return false;
    }
    return race_identity_build_prefix(legacy_race, gender, prefix)
        && race_identity_prefix_equals(name, prefix);
}

bool race_identity_background_matches(int race, int gender, bool npc, const char* conditions)
{
    char prefix[4];
    if (!race_identity_build_prefix(race, gender, prefix)) return false;
    if (conditions == NULL || conditions[0] == 0) return true;
    for (const char* token = conditions; *token != 0; token++) {
        if (race_identity_prefix_equals(token, prefix) || race_identity_prefix_equals(token, "ANY")
            || (npc && race_identity_prefix_equals(token, "NPC"))) return true;
    }
    return false;
}
