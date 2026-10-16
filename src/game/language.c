#include "game/language.h"

#include <SDL3/SDL_stdinc.h>

static const char* names[] = { "english", "french", "spanish", "italian", "russian" };
static const char* suffixes[] = { "en", "fr", "es", "it", "ru" };

void language_input_to_legacy(const char* utf8, char* buffer, size_t size, GameLanguage language)
{
    if (size == 0) return;
    size_t offset = 0;
    while (utf8 != NULL && *utf8 != '\0' && offset + 1 < size) {
        Uint32 codepoint = SDL_StepUTF8(&utf8, NULL);
        unsigned char byte = '?';
        if (codepoint < 128) byte = (unsigned char)codepoint;
        else if (language == GAME_LANGUAGE_RUSSIAN) {
            if (codepoint >= 0x410 && codepoint <= 0x44F) byte = (unsigned char)(codepoint - 0x410 + 0xC0);
            else if (codepoint == 0x401) byte = 0xA8;
            else if (codepoint == 0x451) byte = 0xB8;
        } else if (codepoint >= 0xA0 && codepoint <= 0xFF) byte = (unsigned char)codepoint;
        // Common typographic punctuation in pasted names/searches.
        if (codepoint == 0x2018) byte = 0x91;
        if (codepoint == 0x2019) byte = 0x92;
        if (codepoint == 0x201C) byte = 0x93;
        if (codepoint == 0x201D) byte = 0x94;
        if (codepoint == 0x2013) byte = 0x96;
        if (codepoint == 0x2014) byte = 0x97;
        if (codepoint == 0x2026) byte = 0x85;
        if (codepoint == 0x20AC) byte = 0x80;
        if (language != GAME_LANGUAGE_RUSSIAN) {
            if (codepoint == 0x152) byte = 0x8C;
            if (codepoint == 0x153) byte = 0x9C;
            if (codepoint == 0x178) byte = 0x9F;
        }
        buffer[offset++] = (char)byte;
    }
    buffer[offset] = '\0';
}

static unsigned char language_search_fold(unsigned char ch, GameLanguage language)
{
    if (ch >= 'A' && ch <= 'Z') return ch + 32;
    if (language == GAME_LANGUAGE_RUSSIAN) {
        if (ch >= 0xC0 && ch <= 0xDF) return ch + 32;
        if (ch == 0xA8) return 0xB8;
    } else {
        if (ch >= 0xC0 && ch <= 0xDE && ch != 0xD7) return ch + 32;
        if (ch == 0x8C) return 0x9C;
        if (ch == 0x9F) return 0xFF;
    }
    return ch;
}

const char* language_text_find(const char* text, const char* query, GameLanguage language)
{
    if (text == NULL || query == NULL) return NULL;
    if (query[0] == '\0') return text;
    for (size_t i = 0; text[i] != '\0'; i++) {
        size_t j = 0;
        while (query[j] != '\0' && text[i + j] != '\0'
            && language_search_fold((unsigned char)query[j], language) == language_search_fold((unsigned char)text[i + j], language)) j++;
        if (query[j] == '\0') return text + i;
    }
    return NULL;
}

bool language_text_contains(const char* text, const char* query, GameLanguage language)
{
    return language_text_find(text, query, language) != NULL;
}

GameLanguage language_from_name(const char* name)
{
    if (name != NULL) {
        for (int i = 0; i < GAME_LANGUAGE_COUNT; i++) {
            if (SDL_strcasecmp(name, names[i]) == 0 || SDL_strcasecmp(name, suffixes[i]) == 0) {
                return (GameLanguage)i;
            }
        }
    }
    return GAME_LANGUAGE_ENGLISH;
}

const char* language_name(GameLanguage language)
{
    return names[language >= 0 && language < GAME_LANGUAGE_COUNT ? language : GAME_LANGUAGE_ENGLISH];
}

const char* language_suffix(GameLanguage language)
{
    return suffixes[language >= 0 && language < GAME_LANGUAGE_COUNT ? language : GAME_LANGUAGE_ENGLISH];
}

GameLanguage language_archive_language(const char* path)
{
    if (path == NULL) return GAME_LANGUAGE_NONE;
    const char* basename = path;
    for (const char* cursor = path; *cursor != '\0'; cursor++) {
        if (*cursor == '/' || *cursor == '\\') basename = cursor + 1;
    }
    size_t length = SDL_strlen(basename);
    if (length < 8 || SDL_strcasecmp(basename + length - 4, ".dat") != 0
        || basename[length - 7] != '_') {
        return GAME_LANGUAGE_NONE;
    }
    for (int i = 0; i < GAME_LANGUAGE_COUNT; i++) {
        if (SDL_strncasecmp(basename + length - 6, suffixes[i], 2) == 0) {
            return (GameLanguage)i;
        }
    }
    return GAME_LANGUAGE_NONE;
}

bool language_archive_is_enabled(const char* path, GameLanguage language)
{
    GameLanguage archive = language_archive_language(path);
    return archive == GAME_LANGUAGE_NONE || archive == language;
}

int language_archive_load_phase(const char* path, GameLanguage language)
{
    if (path == NULL) return -1;
    GameLanguage archive = language_archive_language(path);
    if (archive != GAME_LANGUAGE_NONE) return archive == language ? 1 : -1;
    const char* file = path;
    for (const char* cursor = path; *cursor != '\0'; cursor++) {
        if (*cursor == '/' || *cursor == '\\') file = cursor + 1;
    }
    size_t length = SDL_strlen(file);
    return length > 4 && SDL_strcasecmp(file + length - 4, ".dat") == 0
        && SDL_strncasecmp(file, "arcanum", 7) == 0 ? 0 : -1;
}
