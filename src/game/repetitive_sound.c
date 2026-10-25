#include "game/repetitive_sound.h"

#include <stddef.h>

static bool repetitive_sound_name_equals(const char* left, const char* right)
{
    while (*left != '\0' && *right != '\0') {
        unsigned char a = (unsigned char)*left++;
        unsigned char b = (unsigned char)*right++;
        if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if (a != b) return false;
    }
    return *left == *right;
}

bool repetitive_sound_matches(const char* path)
{
    static const char* const names[] = {
        "Krag_fidgeting.wav",
        "Spell_Elemental_Air_Vitality.wav",
        "Spell_Elemental_Earth_Strength.wav",
        "Spell_Elemental_Fire_Agility.wav",
        "Spell_Elemental_Water_Purity.wav",
        "spell_temporal_tempus_fugit.wav",
    };
    if (path == NULL) return false;
    const char* name = path;
    for (const char* cursor = path; *cursor != '\0'; cursor++) {
        if (*cursor == '/' || *cursor == '\\') name = cursor + 1;
    }
    for (size_t index = 0; index < sizeof(names) / sizeof(names[0]); index++) {
        if (repetitive_sound_name_equals(name, names[index])) return true;
    }
    return false;
}
