// IDXTABLE
// ---
//
// The IDXTABLE module provides `IdxTable` object which resembles
// `std::unordered_map<int, T>` from C++ STL.
//
// NOTES
//
// - The `IdxTable` uses integers as keys. Negative integer keys are the same as
// positive, that is:
//
// ```pseudocode
// idxtable[100] == idxtable[-100];
// ```
//
// - Internally `IdxTable` uses a number of buckets to make lookup a little bit
// more efficient in large collections. The number of buckets is considered
// implementation detail and should not be changed. FYI it's 63 in Arcanum and
// 256 in ToEE.

#include "tig/idxtable.h"

#include <limits.h>
#include <string.h>

#include "tig/file.h"
#include "tig/memory.h"

// Number of buckets allocated for each `TigIdxTable` object.
#define MAX_BUCKETS 63

// Magic 32 bit number in the file stream denoting start of serialized
// `TigIdxTable` representation.
#define START_SENTINEL 0xAB1EE1BAu

// Magic 32 bit number in the file stream denoting end of serialized
// `TigIdxTable` representation.
#define END_SENTINEL 0xE1BAAB1Eu

static void tig_idxtable_entry_destroy(TigIdxTableEntry* entry);
static TigIdxTableEntry* tig_idxtable_entry_create(int size);
static TigIdxTableEntry* tig_idxtable_entry_copy(TigIdxTableEntry* src, int size);
static int tig_idxtable_entry_count(TigIdxTableEntry* entry);


// 0x534320
void tig_idxtable_init(TigIdxTable* idxtable, int value_size)
{
    memset(idxtable, 0, sizeof(*idxtable));
    if (value_size <= 0) return;
    idxtable->buckets = (TigIdxTableEntry**)CALLOC(MAX_BUCKETS, sizeof(*idxtable->buckets));
    if (idxtable->buckets == NULL) return;
    idxtable->buckets_count = MAX_BUCKETS;
    idxtable->value_size = value_size;
}

// 0x534370
void tig_idxtable_exit(TigIdxTable* idxtable)
{
    int index;
    TigIdxTableEntry* entry;
    TigIdxTableEntry* next;

    for (index = 0; index < idxtable->buckets_count; index++) {
        // NOTE: Original code is slightly different.
        entry = idxtable->buckets[index];
        while (entry != NULL) {
            next = entry->next;
            tig_idxtable_entry_destroy(entry);
            entry = next;
        }
    }

    FREE(idxtable->buckets);
    memset(idxtable, 0, sizeof(*idxtable));
}

// 0x5343C0
void tig_idxtable_copy(TigIdxTable* dst, TigIdxTable* src)
{
    if (dst == src) return;
    TigIdxTable copied;
    tig_idxtable_init(&copied, src->value_size);
    if (copied.buckets == NULL) {
        *dst = copied;
        return;
    }
    for (int index = 0; index < src->buckets_count; index++) {
        copied.buckets[index] = tig_idxtable_entry_copy(src->buckets[index], src->value_size);
        if (src->buckets[index] != NULL && copied.buckets[index] == NULL) {
            tig_idxtable_exit(&copied);
            *dst = copied;
            return;
        }
    }
    copied.count = src->count;
    *dst = copied;
}

// 0x534430
size_t tig_idxtable_write(TigIdxTable* idxtable, TigFile* stream)
{
    unsigned int grd;
    int index;
    int count;
    TigIdxTableEntry* entry;
    size_t bytes_written = 0;

    grd = START_SENTINEL;
    bytes_written += tig_file_fwrite(&grd, sizeof(grd), 1, stream);

    bytes_written += tig_file_fwrite(&(idxtable->buckets_count), sizeof(idxtable->buckets_count), 1, stream);
    bytes_written += tig_file_fwrite(&(idxtable->value_size), sizeof(idxtable->value_size), 1, stream);

    for (index = 0; index < idxtable->buckets_count; index++) {
        count = tig_idxtable_entry_count(idxtable->buckets[index]);
        bytes_written += tig_file_fwrite(&count, sizeof(count), 1, stream);

        entry = idxtable->buckets[index];
        while (entry != NULL) {
            bytes_written += tig_file_fwrite(&(entry->key), sizeof(entry->key), 1, stream);
            bytes_written += tig_file_fwrite(entry->value, idxtable->value_size, 1, stream);
            entry = entry->next;
        }
    }

    grd = END_SENTINEL;
    bytes_written += tig_file_fwrite(&grd, sizeof(grd), 1, stream);

    return bytes_written;
}

// 0x534520
bool tig_idxtable_read(TigIdxTable* idxtable, TigFile* stream)
{
    if (idxtable == NULL || stream == NULL) return false;
    uint32_t sentinel;
    int buckets;
    int value_size;
    if (tig_file_fread(&sentinel, sizeof(sentinel), 1, stream) != 1 || sentinel != START_SENTINEL
        || tig_file_fread(&buckets, sizeof(buckets), 1, stream) != 1 || buckets != MAX_BUCKETS
        || tig_file_fread(&value_size, sizeof(value_size), 1, stream) != 1 || value_size <= 0
        || value_size != idxtable->value_size) return false;
    TigIdxTable loaded;
    tig_idxtable_init(&loaded, value_size);
    if (loaded.buckets == NULL) return false;
    for (int bucket = 0; bucket < buckets; bucket++) {
        int count;
        if (tig_file_fread(&count, sizeof(count), 1, stream) != 1 || count < 0 || count > INT_MAX - loaded.count) goto error;
        int position = tig_file_ftell(stream);
        int length = tig_file_filelength(stream);
        if (position < 0 || length < position || (size_t)count > (size_t)(length - position) / ((size_t)value_size + sizeof(int))) goto error;
        TigIdxTableEntry** tail = &loaded.buckets[bucket];
        for (int i = 0; i < count; i++) {
            int key;
            if (tig_file_fread(&key, sizeof(key), 1, stream) != 1
                || (key < 0 && key != INT_MIN) || (unsigned int)key % (unsigned int)buckets != (unsigned int)bucket) goto error;
            TigIdxTableEntry* entry = tig_idxtable_entry_create(value_size);
            if (entry == NULL) goto error;
            entry->key = key;
            entry->next = NULL;
            *tail = entry;
            loaded.count++;
            if (tig_file_fread(entry->value, value_size, 1, stream) != 1) goto error;
            tail = &entry->next;
        }
    }
    if (tig_file_fread(&sentinel, sizeof(sentinel), 1, stream) != 1 || sentinel != END_SENTINEL) goto error;
    tig_idxtable_exit(idxtable);
    *idxtable = loaded;
    return true;
error:
    tig_idxtable_exit(&loaded);
    return false;
}

// 0x534650
bool tig_idxtable_enumerate(TigIdxTable* idxtable, TigIdxTableEnumerateCallback callback, void* context)
{
    int index;
    TigIdxTableEntry* entry;

    for (index = 0; index < idxtable->buckets_count; index++) {
        entry = idxtable->buckets[index];
        while (entry != NULL) {
            if (!callback(entry->value, entry->key, context)) {
                return false;
            }
            entry = entry->next;
        }
    }

    return true;
}

// 0x5346A0
void tig_idxtable_set(TigIdxTable* idxtable, int key, void* value)
{
    if (idxtable == NULL || idxtable->buckets == NULL || idxtable->buckets_count <= 0) return;
    int bucket;
    TigIdxTableEntry* entry;

    if (key < 0 && key != INT_MIN) {
        key = -key;
    }

    bucket = (int)((unsigned int)key % (unsigned int)idxtable->buckets_count);
    entry = idxtable->buckets[bucket];
    while (entry != NULL) {
        if (entry->key == key) {
            memcpy(entry->value, value, idxtable->value_size);
            return;
        }
        entry = entry->next;
    }

    if (idxtable->count == INT_MAX) return;
    entry = tig_idxtable_entry_create(idxtable->value_size);
    if (entry == NULL) return;
    memcpy(entry->value, value, idxtable->value_size);
    entry->key = key;
    entry->next = idxtable->buckets[bucket];
    idxtable->buckets[bucket] = entry;

    idxtable->count++;
}

// 0x534730
bool tig_idxtable_get(TigIdxTable* idxtable, int key, void* value)
{
    if (idxtable == NULL || idxtable->buckets == NULL || idxtable->buckets_count <= 0) return false;
    int bucket;
    TigIdxTableEntry* entry;

    if (key < 0 && key != INT_MIN) {
        key = -key;
    }

    bucket = (int)((unsigned int)key % (unsigned int)idxtable->buckets_count);
    entry = idxtable->buckets[bucket];
    while (entry != NULL) {
        if (entry->key == key) {
            memcpy(value, entry->value, idxtable->value_size);
            return true;
        }
        entry = entry->next;
    }

    return false;
}

// 0x534780
bool tig_idxtable_contains(TigIdxTable* idxtable, int key)
{
    if (idxtable == NULL || idxtable->buckets == NULL || idxtable->buckets_count <= 0) return false;
    int bucket;
    TigIdxTableEntry* entry;

    if (key < 0 && key != INT_MIN) {
        key = -key;
    }

    bucket = (int)((unsigned int)key % (unsigned int)idxtable->buckets_count);
    entry = idxtable->buckets[bucket];
    while (entry != NULL) {
        if (entry->key == key) {
            return true;
        }
        entry = entry->next;
    }

    return false;
}

// 0x5347C0
void tig_idxtable_remove(TigIdxTable* idxtable, int key)
{
    if (idxtable == NULL || idxtable->buckets == NULL || idxtable->buckets_count <= 0) return;
    int bucket;
    TigIdxTableEntry** entry_ptr;
    TigIdxTableEntry* entry;

    if (key < 0 && key != INT_MIN) {
        key = -key;
    }

    // NOTE: Original implementation is different.
    bucket = (int)((unsigned int)key % (unsigned int)idxtable->buckets_count);
    entry_ptr = &(idxtable->buckets[bucket]);

    while (*entry_ptr != NULL) {
        entry = *entry_ptr;
        if (entry->key == key) {
            *entry_ptr = entry->next;
            tig_idxtable_entry_destroy(entry);
            idxtable->count--;
            break;
        }
        entry_ptr = &(entry->next);
    }
}

// 0x534840
void tig_idxtable_entry_destroy(TigIdxTableEntry* entry)
{
    FREE(entry->value);
    FREE(entry);
}

// 0x534860
TigIdxTableEntry* tig_idxtable_entry_create(int size)
{
    TigIdxTableEntry* entry;

    entry = (TigIdxTableEntry*)MALLOC(sizeof(*entry));
    if (entry == NULL) return NULL;
    entry->value = MALLOC(size);
    if (entry->value == NULL) {
        FREE(entry);
        return NULL;
    }

    return entry;
}

// 0x534880
TigIdxTableEntry* tig_idxtable_entry_copy(TigIdxTableEntry* entry, int size)
{
    TigIdxTableEntry* head = NULL;
    TigIdxTableEntry** tail = &head;
    while (entry != NULL) {
        TigIdxTableEntry* copy = tig_idxtable_entry_create(size);
        if (copy == NULL) {
            while (head != NULL) {
                TigIdxTableEntry* next = head->next;
                tig_idxtable_entry_destroy(head);
                head = next;
            }
            return NULL;
        }
        memcpy(copy->value, entry->value, size);
        copy->key = entry->key;
        copy->next = NULL;
        *tail = copy;
        tail = &copy->next;
        entry = entry->next;
    }
    return head;
}

// 0x5348D0
int tig_idxtable_entry_count(TigIdxTableEntry* entry){
int count = 0;
    while (entry != NULL) {
        count++;
        entry = entry->next;
    }
    return count;
}

// 0x5348F0
// 0x534990
int tig_idxtable_count(TigIdxTable* idxtable)
{
    return idxtable->count;
}
