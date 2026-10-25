#include "game/repetitive_sound.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const char* const muted[] = {
        "Krag_fidgeting.wav",
        "sound\\Critters\\KRAG_FIDGETING.WAV",
        "sound/Spells/Spell_Elemental_Air_Vitality.wav",
        "Spell_Elemental_Earth_Strength.wav",
        "Spell_Elemental_Fire_Agility.wav",
        "Spell_Elemental_Water_Purity.wav",
        "sound\\spell_temporal_tempus_fugit.wav",
        "SPELL_TEMPORAL_TEMPUS_FUGIT.WAV",
    };
    const char* const allowed[] = {
        NULL, "", "sound/", "Krag_fidgeting.wav.bak", "Other_Krag_fidgeting.wav",
        "Spell_Elemental_Air_Vitality_Cast.wav", "Spell_Elemental_Earth_Strength_End.wav",
        "spell_temporal_tempus_fugit_cast.wav", "Krag_attack.wav",
        "sound/Krag_fidgeting.wav/another.wav", "sound/Music/Tarant.mp3",
    };
    for (size_t index = 0; index < sizeof(muted) / sizeof(muted[0]); index++) {
        if (!repetitive_sound_matches(muted[index])) {
            fprintf(stderr, "Unrecognized repetitive sound: %s\n", muted[index]);
            return EXIT_FAILURE;
        }
    }
    for (size_t index = 0; index < sizeof(allowed) / sizeof(allowed[0]); index++) {
        if (repetitive_sound_matches(allowed[index])) {
            fprintf(stderr, "Unrelated sound filtered at index %zu\n", index);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
