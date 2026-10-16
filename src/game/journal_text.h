#ifndef ARCANUM_GAME_JOURNAL_TEXT_H_
#define ARCANUM_GAME_JOURNAL_TEXT_H_

#include <stdbool.h>
#include <stddef.h>

// MES files use the active legacy code page. Preserve accented letters when
// matching whole names, folding Latin capitals without depending on OS locale.
static unsigned char journal_text_fold(unsigned char c)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7)) return c + 32;
    return c;
}

static bool journal_text_word(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c >= 0x80;
}

static bool journal_text_mentions(const char* text, const char* name)
{
    if (text == NULL || name == NULL || name[0] == '\0') return false;
    for (size_t i = 0; text[i] != '\0'; i++) {
        if (i != 0 && journal_text_word((unsigned char)text[i - 1])) continue;
        size_t j = 0;
        while (name[j] != '\0' && text[i + j] != '\0'
            && journal_text_fold((unsigned char)name[j]) == journal_text_fold((unsigned char)text[i + j])) j++;
        if (name[j] == '\0' && !journal_text_word((unsigned char)text[i + j])) return true;
    }
    return false;
}

static int journal_text_compare(const char* a, const char* b)
{
    while (*a != '\0' && journal_text_fold((unsigned char)*a) == journal_text_fold((unsigned char)*b)) { a++; b++; }
    return (int)journal_text_fold((unsigned char)*a) - (int)journal_text_fold((unsigned char)*b);
}

#endif
