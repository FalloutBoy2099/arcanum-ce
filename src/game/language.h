#ifndef ARCANUM_GAME_LANGUAGE_H_
#define ARCANUM_GAME_LANGUAGE_H_

#include <stdbool.h>
#include <stddef.h>

#define LANGUAGE_KEY "language"

typedef enum GameLanguage {
    GAME_LANGUAGE_NONE = -1,
    GAME_LANGUAGE_ENGLISH,
    GAME_LANGUAGE_FRENCH,
    GAME_LANGUAGE_SPANISH,
    GAME_LANGUAGE_ITALIAN,
    GAME_LANGUAGE_RUSSIAN,
    GAME_LANGUAGE_COUNT,
} GameLanguage;

GameLanguage language_from_name(const char* name);
const char* language_name(GameLanguage language);
const char* language_suffix(GameLanguage language);
GameLanguage language_archive_language(const char* path);
bool language_archive_is_enabled(const char* path, GameLanguage language);
int language_archive_load_phase(const char* path, GameLanguage language);
void language_input_to_legacy(const char* utf8, char* buffer, size_t size, GameLanguage language);
const char* language_text_find(const char* text, const char* query, GameLanguage language);
bool language_text_contains(const char* text, const char* query, GameLanguage language);

#endif
