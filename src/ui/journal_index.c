#include "ui/journal_index.h"
#include "game/journal_text.h"
#include <stdint.h>
#include <string.h>

void journal_index_begin(JournalIndexWorkspace* workspace)
{
    memset(workspace->candidates, 0, sizeof(workspace->candidates));
    memset(workspace->descriptions, 0, sizeof(workspace->descriptions));
}

bool journal_index_add_description(JournalIndexWorkspace* workspace, int description)
{
    if (description <= 0) return false;
    size_t mask = JOURNAL_INDEX_LIMIT * 2 - 1;
    size_t slot = ((uint32_t)description * 2654435761U) & mask;
    for (size_t probes = 0; probes <= mask; probes++, slot = (slot + 1) & mask) {
        if (workspace->descriptions[slot] == description) return false;
        if (workspace->descriptions[slot] == 0) {
            workspace->descriptions[slot] = description;
            return true;
        }
    }
    return false;
}

void journal_index_prepare_mentions(JournalIndexWorkspace* workspace, int count)
{
    memset(workspace->heads, -1, sizeof(workspace->heads));
    for (int index = 0; index < count; index++) {
        const char* name = workspace->candidates[index].name;
        if (name == NULL || name[0] == '\0') continue;
        unsigned char first = journal_text_fold((unsigned char)name[0]);
        workspace->next[index] = workspace->heads[first];
        workspace->heads[first] = index;
    }
}

void journal_index_match_mentions(JournalIndexWorkspace* workspace, const char* text)
{
    if (text == NULL) return;
    for (size_t offset = 0; text[offset] != '\0'; offset++) {
        if (offset != 0 && journal_text_word((unsigned char)text[offset - 1])) continue;
        unsigned char first = journal_text_fold((unsigned char)text[offset]);
        for (int index = workspace->heads[first]; index >= 0; index = workspace->next[index]) {
            JournalIndexCandidate* candidate = &workspace->candidates[index];
            if (candidate->known) continue;
            size_t length = 0;
            while (candidate->name[length] != '\0' && text[offset + length] != '\0'
                && journal_text_fold((unsigned char)candidate->name[length]) == journal_text_fold((unsigned char)text[offset + length])) length++;
            if (candidate->name[length] == '\0' && !journal_text_word((unsigned char)text[offset + length])) candidate->known = true;
        }
    }
}
