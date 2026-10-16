#ifndef ARCANUM_GAME_CONVERSATION_HISTORY_H_
#define ARCANUM_GAME_CONVERSATION_HISTORY_H_

#include <stdint.h>

#include "game/timeevent.h"
#include "game/mes.h"

#define CONVERSATION_HISTORY_CAPACITY 256
typedef struct ConversationHistoryEntry {
    DateTime date;
    char speaker[128];
    char text[MAX_STRING];
} ConversationHistoryEntry;

void conversation_history_reset(void);
void conversation_history_add(const char* speaker, const char* text, DateTime date);
int conversation_history_count(void);
uint64_t conversation_history_revision(void);
const ConversationHistoryEntry* conversation_history_get(int index);
bool conversation_history_save(void);
void conversation_history_load(void);

#endif
