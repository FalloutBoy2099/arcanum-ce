#include "game/journal_memory.h"

#include <tig/tig.h>

#define JOURNAL_MEMORY_CAPACITY 4096
#define JOURNAL_MEMORY_PATH "Save\\Current\\journal_index.bin"

static int descriptions[JOURNAL_MEMORY_CAPACITY];
static int count;

void journal_memory_reset(void)
{
    count = 0;
}

bool journal_memory_knows(int description)
{
    for (int i = 0; i < count; i++) {
        if (descriptions[i] == description) return true;
    }
    return false;
}

void journal_memory_remember(int description)
{
    if (description > 0 && count < JOURNAL_MEMORY_CAPACITY && !journal_memory_knows(description)) {
        descriptions[count++] = description;
    }
}

int journal_memory_count(void)
{
    return count;
}

int journal_memory_get(int index)
{
    return index >= 0 && index < count ? descriptions[index] : 0;
}

// A separate, optional file keeps the original data.sav format compatible.
bool journal_memory_save(void)
{
    unsigned int version = 1;
    TigFile* stream = tig_file_fopen(JOURNAL_MEMORY_PATH, "wb");
    if (stream == NULL) return false;
    bool ok = tig_file_fwrite(&version, sizeof(version), 1, stream) == 1
        && tig_file_fwrite(&count, sizeof(count), 1, stream) == 1
        && (count == 0 || tig_file_fwrite(descriptions, sizeof(descriptions[0]), count, stream) == (size_t)count);
    if (tig_file_fclose(stream) != 0) ok = false;
    return ok;
}

void journal_memory_load(void)
{
    unsigned int version;
    int loaded_count;
    journal_memory_reset();
    TigFile* stream = tig_file_fopen(JOURNAL_MEMORY_PATH, "rb");
    if (stream == NULL) return; // Original saves do not contain this file.
    bool ok = tig_file_fread(&version, sizeof(version), 1, stream) == 1 && version == 1
        && tig_file_fread(&loaded_count, sizeof(loaded_count), 1, stream) == 1
        && loaded_count >= 0 && loaded_count <= JOURNAL_MEMORY_CAPACITY
        && (loaded_count == 0 || tig_file_fread(descriptions, sizeof(descriptions[0]), loaded_count, stream) == (size_t)loaded_count);
    tig_file_fclose(stream);
    if (ok) {
        count = loaded_count;
        for (int i = 0; i < count; i++) {
            if (descriptions[i] <= 0) { journal_memory_reset(); break; }
        }
    } else {
        tig_debug_printf("Journal index: ignoring invalid optional memory file.\n");
    }
}
