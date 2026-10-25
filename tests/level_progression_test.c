#include "game/level_progression.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #value); failures++; } } while (0)

int main(int argc, char** argv)
{
    CHECK(argc == 2);
    if (argc != 2) return 1;
    FILE* file = fopen(argv[1], "rb");
    CHECK(file != NULL);
    if (file == NULL) return 1;
    char storage[LEVEL_LIMIT][24] = { 0 };
    const char* entries[LEVEL_LIMIT] = { 0 };
    int expected[LEVEL_LIMIT] = { 0 };
    char line[128];
    int lines = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        int level, experience;
        CHECK(sscanf(line, "{%d}{%d}", &level, &experience) == 2);
        CHECK(level >= 1 && level <= LEVEL_LIMIT);
        if (level < 1 || level > LEVEL_LIMIT) continue;
        CHECK(entries[level - 1] == NULL);
        snprintf(storage[level - 1], sizeof(storage[0]), "%d", experience);
        entries[level - 1] = storage[level - 1];
        expected[level - 1] = experience;
        lines++;
    }
    fclose(file);
    CHECK(lines == LEVEL_LIMIT);
    int thresholds[LEVEL_LIMIT];
    CHECK(level_progression_build(thresholds, entries, LEVEL_LIMIT));
    CHECK(memcmp(thresholds, expected, sizeof(expected)) == 0);
    CHECK(thresholds[49] == 1300000 && thresholds[50] == 1353000);
    CHECK(thresholds[74] == 2926000 && thresholds[99] == 5201000);
    CHECK(thresholds[126] == 8360000 && thresholds[127] == 8491000);
    for (int i = 1; i < LEVEL_LIMIT; i++) CHECK(thresholds[i] > thresholds[i - 1]);
    int accumulated = 5;
    for (int level = 2; level <= LEVEL_PLAYABLE_MAX; level++) {
        int points = level_progression_bonus_points(level - 1, level);
        CHECK(points == (level % 5 == 0 ? 2 : 1));
        accumulated += points;
    }
    CHECK(accumulated == 156 && LEVEL_UNSPENT_POINTS_MAX == INT_MAX);
    CHECK(level_progression_bonus_points(1, 127) == 151);
    CHECK(level_progression_bonus_points(49, 51) == 3);
    CHECK(level_progression_bonus_points(100, 127) == 32);
    CHECK(level_progression_bonus_points(127, 127) == 0);
    CHECK(level_progression_bonus_points(127, 128) == 0);
    CHECK(level_progression_percent(127, INT_MAX, thresholds) == 0);
    CHECK(level_progression_percent(-1, 1000, thresholds) == 0);
    CHECK(level_progression_percent(INT_MAX, 1000, thresholds) == 0);
    CHECK(level_progression_percent(126, INT_MAX, thresholds) == 1000);
    CHECK(level_progression_percent(126, 0, thresholds) == 0);
    CHECK(level_progression_percent(126, (thresholds[125] + thresholds[126]) / 2, thresholds) == 500);
    for (int level = 1; level < LEVEL_PLAYABLE_MAX; level++) {
        CHECK(level_progression_percent(level, thresholds[level - 1], thresholds) == 0);
        CHECK(level_progression_percent(level, thresholds[level], thresholds) == 1000);
    }
    const char* custom[LEVEL_LIMIT];
    memcpy(custom, entries, sizeof(custom));
    for (int i = 50; i < LEVEL_LIMIT; i++) custom[i] = NULL;
    custom[50] = "999999999";
    CHECK(level_progression_build(thresholds, custom, LEVEL_LIMIT));
    CHECK(memcmp(thresholds, expected, sizeof(expected)) == 0);
    custom[50] = "1400000";
    custom[51] = "1500000";
    CHECK(level_progression_build(thresholds, custom, LEVEL_LIMIT));
    CHECK(thresholds[50] == 1400000 && thresholds[51] == 1500000 && thresholds[52] == 1555000);
    const char* bad[] = { NULL, "0", "-1", "123invalid", "999999999999999999999999", "2000000001" };
    for (int i = 0; i < (int)(sizeof(bad) / sizeof(bad[0])); i++) {
        memcpy(custom, entries, sizeof(custom));
        custom[25] = bad[i];
        CHECK(!level_progression_build(thresholds, custom, LEVEL_LIMIT));
        memcpy(custom, entries, sizeof(custom));
        custom[50] = bad[i];
        if (bad[i] != NULL) CHECK(!level_progression_build(thresholds, custom, LEVEL_LIMIT));
    }
    memcpy(custom, entries, sizeof(custom));
    custom[49] = "1999999999";
    for (int i = 50; i < LEVEL_LIMIT; i++) custom[i] = NULL;
    CHECK(!level_progression_build(thresholds, custom, LEVEL_LIMIT));
    CHECK(!level_progression_build(NULL, entries, LEVEL_LIMIT));
    CHECK(!level_progression_build(thresholds, entries, LEVEL_LIMIT - 1));
    printf("Level progression: %d failures\n", failures);
    return failures != 0;
}
