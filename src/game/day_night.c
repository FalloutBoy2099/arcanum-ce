#include "game/day_night.h"
#include <math.h>

TigColorGrade day_night_grade(float hour, float strength)
{
    static const float hours[] = { 0, 5, 7, 9, 17, 19, 21, 24 };
    static const TigColorGrade grades[] = {
        { 0.78f, 0.88f, 1.12f, 0.88f },
        { 0.78f, 0.88f, 1.12f, 0.88f },
        { 1.10f, 1.02f, 0.90f, 1.04f },
        { 1, 1, 1, 1 },
        { 1, 1, 1, 1 },
        { 1.14f, 0.98f, 0.82f, 1.06f },
        { 0.78f, 0.88f, 1.12f, 0.88f },
        { 0.78f, 0.88f, 1.12f, 0.88f },
    };
    if (!isfinite(hour) || !isfinite(strength)) return (TigColorGrade){ 1, 1, 1, 1 };
    strength = strength < 0 ? 0 : strength > 1 ? 1 : strength;
    hour = fmodf(hour, 24.0f);
    if (hour < 0) hour += 24.0f;
    int index = 0;
    while (index < 6 && hour >= hours[index + 1]) index++;
    float blend = (hour - hours[index]) / (hours[index + 1] - hours[index]);
    blend = blend * blend * (3.0f - 2.0f * blend);
    TigColorGrade grade;
    grade.red = 1 + (grades[index].red + blend * (grades[index + 1].red - grades[index].red) - 1) * strength;
    grade.green = 1 + (grades[index].green + blend * (grades[index + 1].green - grades[index].green) - 1) * strength;
    grade.blue = 1 + (grades[index].blue + blend * (grades[index + 1].blue - grades[index].blue) - 1) * strength;
    grade.saturation = 1 + (grades[index].saturation + blend * (grades[index + 1].saturation - grades[index].saturation) - 1) * strength;
    return grade;
}
