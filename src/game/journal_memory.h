#ifndef ARCANUM_GAME_JOURNAL_MEMORY_H_
#define ARCANUM_GAME_JOURNAL_MEMORY_H_

#include <stdbool.h>

void journal_memory_reset(void);
void journal_memory_remember(int description);
bool journal_memory_knows(int description);
int journal_memory_count(void);
int journal_memory_get(int index);
bool journal_memory_save(void);
void journal_memory_load(void);

#endif
