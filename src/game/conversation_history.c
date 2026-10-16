#include "game/conversation_history.h"

#define HISTORY_FILE "Save\\Current\\conversations.bin"
static ConversationHistoryEntry entries[CONVERSATION_HISTORY_CAPACITY];
static int start;
static int count;
static uint64_t revision;

uint64_t conversation_history_revision(void) { return revision; }

void conversation_history_reset(void)
{
    start = count = 0;
    revision++;
}

void conversation_history_add(const char* speaker, const char* text, DateTime date)
{
    if (speaker == NULL || text == NULL || text[0] == '\0') return;
    revision++;
    int slot = (start + count) % CONVERSATION_HISTORY_CAPACITY;
    if (count == CONVERSATION_HISTORY_CAPACITY) start = (start + 1) % CONVERSATION_HISTORY_CAPACITY;
    else count++;
    entries[slot].date = date;
    SDL_strlcpy(entries[slot].speaker, speaker, sizeof(entries[slot].speaker));
    SDL_strlcpy(entries[slot].text, text, sizeof(entries[slot].text));
}

int conversation_history_count(void) { return count; }

const ConversationHistoryEntry* conversation_history_get(int index)
{
    return index >= 0 && index < count ? &entries[(start + index) % CONVERSATION_HISTORY_CAPACITY] : NULL;
}

bool conversation_history_save(void)
{
    TigFile* file = tig_file_fopen(HISTORY_FILE, "wb");
    if (file == NULL) return false;
    unsigned int version = 1;
    bool ok = tig_file_fwrite(&version, sizeof(version), 1, file) == 1
        && tig_file_fwrite(&count, sizeof(count), 1, file) == 1;
    for (int i = 0; ok && i < count; i++) {
        ok = tig_file_fwrite(conversation_history_get(i), sizeof(ConversationHistoryEntry), 1, file) == 1;
    }
    if (tig_file_fclose(file) != 0) ok = false;
    return ok;
}

void conversation_history_load(void)
{
    conversation_history_reset();
    TigFile* file = tig_file_fopen(HISTORY_FILE, "rb");
    if (file == NULL) return;
    unsigned int version;
    int loaded;
    bool ok = tig_file_fread(&version, sizeof(version), 1, file) == 1 && version == 1
        && tig_file_fread(&loaded, sizeof(loaded), 1, file) == 1
        && loaded >= 0 && loaded <= CONVERSATION_HISTORY_CAPACITY;
    if (ok && loaded > 0) ok = tig_file_fread(entries, sizeof(entries[0]), loaded, file) == (size_t)loaded;
    tig_file_fclose(file);
    if (ok) {
        count = loaded;
        for (int i = 0; i < count; i++) {
            entries[i].speaker[sizeof(entries[i].speaker) - 1] = '\0';
            entries[i].text[sizeof(entries[i].text) - 1] = '\0';
        }
    }
}
