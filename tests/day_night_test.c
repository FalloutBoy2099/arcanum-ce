#include "game/day_night.h"
#include <math.h>
#include <stdio.h>

static int failures;
#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #value); failures++; } } while (0)

int main(void)
{
    TigColorGrade noon = day_night_grade(12, 1);
    CHECK(noon.red == 1 && noon.green == 1 && noon.blue == 1 && noon.saturation == 1);
    TigColorGrade night = day_night_grade(0, 1);
    CHECK(night.blue > night.green && night.green > night.red);
    TigColorGrade sunset = day_night_grade(19, 1);
    CHECK(sunset.red > sunset.green && sunset.green > sunset.blue);
    TigColorGrade disabled = day_night_grade(0, 0);
    CHECK(disabled.red == 1 && disabled.green == 1 && disabled.blue == 1 && disabled.saturation == 1);
    TigColorGrade wrapped = day_night_grade(24, 1);
    CHECK(memcmp(&night, &wrapped, sizeof(night)) == 0);
    wrapped = day_night_grade(-24, 1);
    CHECK(memcmp(&night, &wrapped, sizeof(night)) == 0);
    TigColorGrade before = day_night_grade(18.9999f, 1);
    TigColorGrade after = day_night_grade(19.0001f, 1);
    CHECK(fabsf(before.red - after.red) < 0.0001f && fabsf(before.blue - after.blue) < 0.0001f);
    for (int minute = 0; minute < 1440; minute++) {
        TigColorGrade grade = day_night_grade(minute / 60.0f, 0.6f);
        CHECK(isfinite(grade.red) && grade.red > 0 && grade.red < 1.2f);
        CHECK(isfinite(grade.green) && grade.green > 0 && grade.green < 1.2f);
        CHECK(isfinite(grade.blue) && grade.blue > 0 && grade.blue < 1.2f);
    }
    return failures ? 1 : 0;
}
