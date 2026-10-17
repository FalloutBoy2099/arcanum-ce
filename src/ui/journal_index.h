#ifndef ARCANUM_UI_JOURNAL_INDEX_H_
#define ARCANUM_UI_JOURNAL_INDEX_H_

#include <stdbool.h>
#include <stddef.h>
#define JOURNAL_INDEX_LIMIT 8192

typedef struct JournalIndexCandidate {
    const char* name;
    int description;
    bool known;
    bool person;
} JournalIndexCandidate;

typedef struct JournalIndexWorkspace {
    JournalIndexCandidate candidates[JOURNAL_INDEX_LIMIT];
    int descriptions[JOURNAL_INDEX_LIMIT * 2];
    int heads[256];
    int next[JOURNAL_INDEX_LIMIT];
} JournalIndexWorkspace;

void journal_index_begin(JournalIndexWorkspace* workspace);
bool journal_index_add_description(JournalIndexWorkspace* workspace, int description);
void journal_index_prepare_mentions(JournalIndexWorkspace* workspace, int count);
// Scan each visible text once, selecting only names with a matching first byte.
void journal_index_match_mentions(JournalIndexWorkspace* workspace, const char* text);

#endif
