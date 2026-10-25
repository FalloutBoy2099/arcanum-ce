#include "game/race_identity.h"
#include "game/stat.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #value); failures++; } } while (0)

int main(void)
{
    const char* const markers[] = { "HU", "DW", "EL", "HE", "GN", "HA", "HO", "HG", "DE", "OG", "OC" };
    for (int race = 0; race < RACE_COUNT; race++) {
        CHECK(strcmp(race_identity_marker(race), markers[race]) == 0);
        for (int gender = 0; gender < GENDER_COUNT; gender++) {
            char text[32];
            snprintf(text, sizeof(text), "%s%cportrait", markers[race], gender == GENDER_FEMALE ? 'F' : 'M');
            CHECK(race_identity_portrait_matches(race, gender, false, text));
            text[3] = 0;
            CHECK(race_identity_background_matches(race, gender, false, text));
            CHECK(!race_identity_portrait_matches(race, 1 - gender, false, text));
            CHECK(!race_identity_background_matches(race, 1 - gender, false, text));
        }
    }
    CHECK(race_identity_portrait_matches(RACE_DARK_ELF, GENDER_FEMALE, false, "def01"));
    CHECK(race_identity_portrait_matches(RACE_DARK_ELF, GENDER_FEMALE, false, "ELF01"));
    CHECK(race_identity_portrait_matches(RACE_ORC, GENDER_MALE, false, "HOM01"));
    CHECK(!race_identity_background_matches(RACE_ORC, GENDER_MALE, false, "HOM"));
    CHECK(race_identity_background_matches(RACE_OGRE, GENDER_MALE, false, "HUM, ogm, DEM"));
    CHECK(race_identity_background_matches(RACE_ORC, GENDER_MALE, false, "any"));
    CHECK(!race_identity_background_matches(RACE_ORC, GENDER_MALE, false, "NPC"));
    CHECK(race_identity_background_matches(RACE_ORC, GENDER_MALE, true, "NPC"));
    CHECK(race_identity_marker(-1) == NULL && race_identity_marker(RACE_COUNT) == NULL);
    CHECK(!race_identity_portrait_matches(INT_MAX, 0, false, "ANY"));
    CHECK(!race_identity_background_matches(-1, 0, false, "ANY"));
    CHECK(!race_identity_portrait_matches(0, GENDER_COUNT, false, "ANY"));
    CHECK(!race_identity_portrait_matches(0, 0, false, ""));
    CHECK(!race_identity_portrait_matches(0, 0, false, "H"));
    CHECK(!race_identity_portrait_matches(0, 0, false, "HU"));
    CHECK(!race_identity_portrait_matches(0, 0, false, NULL));
    CHECK(race_identity_background_matches(0, 0, false, NULL));
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
