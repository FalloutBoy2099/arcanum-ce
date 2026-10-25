#include "game/mes.h"

#include <limits.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tig/tig.h>

typedef struct MesFile {
    /* 0000 */ char path[TIG_MAX_PATH];
    /* 0104 */ int refcount;
    /* 0108 */ int num_entries;
    /* 010C */ int max_entries;
    /* 0110 */ MesFileEntry* entries;
    /* 0114 */ char* data;
    /* 0118 */ size_t size;
} MesFile;

static bool find_mes_file(const char* path, int* index_ptr);
static void mes_load_internal(MesFile* mes_file);
static bool parse_entry(TigFile* stream, MesFileEntry* mes_file_entry);
static bool parse_field(TigFile* stream, char* buffer);
static TigFile* mes_file_open(const char* path, const char* mode);
static void mes_file_close(TigFile* stream);
static int consume_next_char(TigFile* stream);
static int compare_mes_file_entry(const void* a, const void* b);
static void copy_mes_file_entry(MesFileEntry* dst, MesFileEntry* src, char* str);
static void check_duplicates(MesFile* mes_file);

/**
 * Current line number during parsing, used for error reporting.
 *
 * 0x6018DC
 */
static int mes_file_parse_line;

/**
 * Buffer for storing a single line during parsing.
 *
 * 0x6018E0
 */
static char mes_file_parse_buffer[MAX_STRING];

/**
 * Path of the currently parsed message file, used for error reporting.
 *
 * 0x6020B0
 */
static const char* current_mes_file_path;

/**
 * The length of string in the parsing buffer (`mes_file_parse_buffer`).
 *
 * 0x6020B4
 */
static int mes_file_parse_len;

/**
 * Buffer for storing error message when a requested message number of not
 * found.
 *
 * See `mes_get_msg`.
 *
 * 0x6020B8
 */
static char mes_tmp_str[MAX_STRING];

/**
 * Current position in the parsing buffer.
 *
 * 0x602888
 */
static int mes_file_parse_pos;

/**
 * The number of loaded message files in `mes_files`.
 *
 * 0x60288C
 */
static int mes_files_length;

/**
 * The capacity of the `mes_files` array.
 *
 * 0x602890
 */
static int mes_files_capacity;

/**
 * Array of loaded message files.
 *
 * 0x602894
 */
static MesFile* mes_files;

typedef struct LocalizedCatalog {
    const char* name;
    mes_file_handle_t primary;
    mes_file_handle_t fallback;
    bool loaded;
} LocalizedCatalog;

static LocalizedCatalog localized_catalogs[] = {
    { "map_markers", MES_FILE_HANDLE_INVALID, MES_FILE_HANDLE_INVALID, false },
    { "crafting_ui", MES_FILE_HANDLE_INVALID, MES_FILE_HANDLE_INVALID, false },
    { "journal_index", MES_FILE_HANDLE_INVALID, MES_FILE_HANDLE_INVALID, false },
    { "options_ui", MES_FILE_HANDLE_INVALID, MES_FILE_HANDLE_INVALID, false },
    { "combat_info", MES_FILE_HANDLE_INVALID, MES_FILE_HANDLE_INVALID, false },
    { "savegame", MES_FILE_HANDLE_INVALID, MES_FILE_HANDLE_INVALID, false },
    { "language_names", MES_FILE_HANDLE_INVALID, MES_FILE_HANDLE_INVALID, false },
    { "worldmap", MES_FILE_HANDLE_INVALID, MES_FILE_HANDLE_INVALID, false },
    { "follower_ui", MES_FILE_HANDLE_INVALID, MES_FILE_HANDLE_INVALID, false },
    { "item_compare_ui", MES_FILE_HANDLE_INVALID, MES_FILE_HANDLE_INVALID, false },
    { "benchmark_ui", MES_FILE_HANDLE_INVALID, MES_FILE_HANDLE_INVALID, false },
    { "shader_ui", MES_FILE_HANDLE_INVALID, MES_FILE_HANDLE_INVALID, false },
};

static const char* mes_localized_lookup(const char* name, int id, bool fallback_only)
{
    for (int i = 0; i < SDL_arraysize(localized_catalogs); i++) {
        LocalizedCatalog* catalog = &localized_catalogs[i];
        if (strcmp(catalog->name, name) != 0) continue;
        if (!catalog->loaded) {
            char path[TIG_MAX_PATH];
            snprintf(path, sizeof(path), "mes\\%s.mes", name);
            mes_load(path, &catalog->primary);
            snprintf(path, sizeof(path), "mes\\ce_fallback\\%s.mes", name);
            mes_load(path, &catalog->fallback);
            catalog->loaded = true;
        }
        MesFileEntry entry = { id, NULL };
        if (!fallback_only && catalog->primary != MES_FILE_HANDLE_INVALID
            && mes_search(catalog->primary, &entry)) return entry.str;
        if (catalog->fallback != MES_FILE_HANDLE_INVALID
            && mes_search(catalog->fallback, &entry)) return entry.str;
        break;
    }
    // A neutral marker if both resource catalogs are missing.
    return "?";
}

const char* mes_localized_get(const char* catalog, int id)
{
    return mes_localized_lookup(catalog, id, false);
}

const char* mes_fallback_get(const char* catalog, int id)
{
    return mes_localized_lookup(catalog, id, true);
}

void mes_localized_exit(void)
{
    for (int i = 0; i < SDL_arraysize(localized_catalogs); i++) {
        LocalizedCatalog* catalog = &localized_catalogs[i];
        if (catalog->primary != MES_FILE_HANDLE_INVALID) mes_unload(catalog->primary);
        if (catalog->fallback != MES_FILE_HANDLE_INVALID) mes_unload(catalog->fallback);
        catalog->primary = MES_FILE_HANDLE_INVALID;
        catalog->fallback = MES_FILE_HANDLE_INVALID;
        catalog->loaded = false;
    }
}

/**
 * Loads a message file and assigns a handle for accessing it.
 *
 * 0x4D4290
 */
bool mes_load(const char* path, mes_file_handle_t* mes_file_handle_ptr)
{
    int index;
    MesFile mes_file = { 0 };
    if (path == NULL || mes_file_handle_ptr == NULL || strlen(path) >= sizeof(mes_file.path)) return false;

    // Check if the file is already loaded.
    if (find_mes_file(path, &index)) {
        // File has already been loaded, increment refcount.
        mes_files[index].refcount++;
        *mes_file_handle_ptr = (mes_file_handle_t)index;
        return true;
    }

    if (index < 0) {
        *mes_file_handle_ptr = MES_FILE_HANDLE_INVALID;
        return false;
    }
    // Initialize new mes file object.
    strcpy(mes_file.path, path);
    mes_file.refcount = 1;
    mes_file.num_entries = 0;
    mes_file.max_entries = 0;
    mes_file.entries = NULL;

    // Keep current path for error reporting.
    current_mes_file_path = path;
    mes_load_internal(&mes_file);

    // Check if any entries were loaded.
    if (mes_file.num_entries == 0) {
        FREE(mes_file.entries);
        FREE(mes_file.data);
        if (mes_file_handle_ptr != NULL) {
            *mes_file_handle_ptr = MES_FILE_HANDLE_INVALID;
        }
        return false;
    }

    mes_files[index] = mes_file;
    mes_files_length++;

    *mes_file_handle_ptr = (mes_file_handle_t)index;

    return true;
}

/**
 * Retrieves a message entry by its number from a mes file.
 *
 * 0x4D43A0
 */
void mes_get_msg(mes_file_handle_t mes_file_handle, MesFileEntry* mes_file_entry)
{
    MesFileEntry* found;

    // Search for the entry using binary search.
    found = bsearch(mes_file_entry,
        mes_files[mes_file_handle].entries,
        mes_files[mes_file_handle].num_entries,
        sizeof(MesFileEntry),
        compare_mes_file_entry);

    // Set an error message if not found.
    if (found == NULL) {
        snprintf(mes_tmp_str, sizeof(mes_tmp_str),
            "Error! Missing line %d in %s",
            mes_file_entry->num,
            mes_files[mes_file_handle].path);
        mes_file_entry->str = mes_tmp_str;
        return;
    }

    *mes_file_entry = *found;
}

/**
 * Retrieves the first message entry from a mes file.
 *
 * 0x4D4420
 */
bool mes_find_first(mes_file_handle_t mes_file_handle, MesFileEntry* mes_file_entry)
{
    if (mes_files[mes_file_handle].num_entries == 0) {
        return false;
    }

    *mes_file_entry = mes_files[mes_file_handle].entries[0];

    return true;
}

/**
 * Retrieves the next message entry from a mes file.
 *
 * 0x4D4460
 */
bool mes_find_next(mes_file_handle_t mes_file_handle, MesFileEntry* mes_file_entry)
{
    MesFileEntry* found;
    int exists;
    int index;

    // Search for the current entry to find its index.
    found = tig_bsearch(mes_file_entry,
        mes_files[mes_file_handle].entries,
        mes_files[mes_file_handle].num_entries,
        sizeof(MesFileEntry),
        compare_mes_file_entry,
        &exists);

    index = (int)(found - mes_files[mes_file_handle].entries);
    if (exists) {
        // Move to the next entry if the current one exists.
        index++;
    }

    // Check range.
    if (index >= mes_files[mes_file_handle].num_entries) {
        return false;
    }

    *mes_file_entry = mes_files[mes_file_handle].entries[index];

    return true;
}

/**
 * Returns the total number of entries in a mes file.
 *
 * 0x4D44E0
 */
int mes_entries_count(mes_file_handle_t mes_file_handle)
{
    return mes_files[mes_file_handle].num_entries;
}

/**
 * Retrieves a message entry by its index in a mes file.
 *
 * 0x4D4500
 */
bool mes_get_entry(mes_file_handle_t mes_file_handle, int index, MesFileEntry* mes_file_entry)
{
    if (index < 0 || index >= mes_files[mes_file_handle].num_entries) {
        return false;
    }

    *mes_file_entry = mes_files[mes_file_handle].entries[index];

    return true;
}

/**
 * Counts the number of entries within a specified range of entry numbers.
 *
 * 0x4D4550
 */
int mes_entries_count_in_range(mes_file_handle_t mes_file_handle, int start, int end)
{
    MesFileEntry mes_file_entry;
    MesFileEntry* found;
    int index;
    int cnt = 0;

    // Search for the starting entry.
    mes_file_entry.num = start;
    found = bsearch(&mes_file_entry,
        mes_files[mes_file_handle].entries,
        mes_files[mes_file_handle].num_entries,
        sizeof(MesFileEntry),
        compare_mes_file_entry);

    if (found != NULL) {
        // Count entries from the starting point until the end number is exceeded.
        index = (int)(found - mes_files[mes_file_handle].entries);
        while (index < mes_files[mes_file_handle].num_entries) {
            if (mes_files[mes_file_handle].entries[index].num > end) {
                break;
            }
            index++;
            cnt++;
        }
    }

    return cnt;
}

/**
 * Searches for a message entry by its number in a mes file.
 *
 * 0x4D45E0
 */
bool mes_search(mes_file_handle_t mes_file_handle, MesFileEntry* mes_file_entry)
{
    MesFileEntry* found;

    // Search for the entry using binary search.
    found = bsearch(mes_file_entry,
        mes_files[mes_file_handle].entries,
        mes_files[mes_file_handle].num_entries,
        sizeof(MesFileEntry),
        compare_mes_file_entry);

    if (found == NULL) {
        return false;
    }

    *mes_file_entry = *found;

    return true;
}

/**
 * Unloads a mes file, freeing resources if no references remain.
 *
 * 0x4D4630
 */
bool mes_unload(mes_file_handle_t mes_file_handle)
{
    if (mes_file_handle != MES_FILE_HANDLE_INVALID) {
        // Decrease the reference count.
        mes_files[mes_file_handle].refcount--;

        // Free resources if no references remain.
        if (mes_files[mes_file_handle].refcount == 0) {
            FREE(mes_files[mes_file_handle].entries);
            FREE(mes_files[mes_file_handle].data);
            mes_files_length--;
        }

        // Free the global array if no open files remain.
        if (mes_files_length == 0) {
            FREE(mes_files);
            mes_files = NULL;
            mes_files_capacity = 0;
        }
    }

    return true;
}

/**
 * Duplicate of `mes_entries_count`.
 *
 * 0x4D46D0
 */
int mes_num_entries(mes_file_handle_t mes_file_handle)
{
    return mes_files[mes_file_handle].num_entries;
}

/**
 * Merges entries from one mes file into another.
 *
 * 0x4D46F0
 */
void mes_merge(mes_file_handle_t dst, mes_file_handle_t src)
{
    if (dst < 0 || dst >= mes_files_capacity || src < 0 || src >= mes_files_capacity
        || mes_files[dst].refcount == 0 || mes_files[src].refcount == 0) return;
    MesFile* destination = &mes_files[dst];
    const MesFile* source = &mes_files[src];
    if (source->num_entries > INT_MAX - destination->num_entries
        || source->size > (size_t)INT_MAX - destination->size) return;
    int count = destination->num_entries + source->num_entries;
    if ((size_t)count > SIZE_MAX / sizeof(MesFileEntry)) return;
    size_t size = destination->size + source->size;
    char* data = (char*)MALLOC(size);
    MesFileEntry* entries = (MesFileEntry*)MALLOC((size_t)count * sizeof(*entries));
    if (data == NULL || entries == NULL) {
        FREE(data);
        FREE(entries);
        return;
    }
    memcpy(data, destination->data, destination->size);
    memcpy(data + destination->size, source->data, source->size);
    for (int index = 0; index < destination->num_entries; index++) {
        entries[index] = destination->entries[index];
        entries[index].str = data + (destination->entries[index].str - destination->data);
    }
    for (int index = 0; index < source->num_entries; index++) {
        entries[destination->num_entries + index] = source->entries[index];
        entries[destination->num_entries + index].str = data + destination->size
            + (source->entries[index].str - source->data);
    }
    FREE(destination->data);
    FREE(destination->entries);
    destination->data = data;
    destination->entries = entries;
    destination->size = size;
    destination->num_entries = count;
    destination->max_entries = count;
    qsort(entries, count, sizeof(*entries), compare_mes_file_entry);
    check_duplicates(destination);
}

/**
 * Searches for an existing message file by path and returns its index.
 *
 * Returns `true` if the file is already loaded, `false` otherwise.
 *
 * 0x4D4840
 */
bool find_mes_file(const char* path, int* index_ptr)
{
    int index;
    int candidate = -1;

    // Iterate through the message files array to find a matching path or a
    // first free slot.
    for (index = 0; index < mes_files_capacity; index++) {
        if (mes_files[index].refcount != 0) {
            if (SDL_strcasecmp(path, mes_files[index].path) == 0) {
                *index_ptr = index;
                return true;
            }
        } else {
            if (candidate == -1) {
                candidate = index;
            }
        }
    }

    // Use the free slot if found.
    if (candidate != -1) {
        *index_ptr = candidate;
        return false;
    }

    *index_ptr = index;

    // No free slot is available, expand message files array.
    if (mes_files_capacity > INT_MAX - 10 || (size_t)mes_files_capacity + 10 > SIZE_MAX / sizeof(MesFile)) {
        *index_ptr = -1;
        return false;
    }
    int capacity = mes_files_capacity + 10;
    MesFile* resized = (MesFile*)REALLOC(mes_files, sizeof(*resized) * (size_t)capacity);
    if (resized == NULL) {
        *index_ptr = -1;
        return false;
    }
    mes_files = resized;
    mes_files_capacity = capacity;

    // Initialize new slots with zero refcount.
    while (index < mes_files_capacity) {
        mes_files[index++].refcount = 0;
    }

    return false;
}

/**
 * Internal helper that loads and parses a message file.
 *
 * 0x4D4920
 */
void mes_load_internal(MesFile* mes_file)
{
    TigFile* stream;
    MesFileEntry mes_file_entry;
    char buffer[MAX_STRING];
    int offset;

    stream = mes_file_open(mes_file->path, "rt");
    if (stream == NULL) {
        return;
    }

    // Allocate a buffer for the file's data based on its size. The actual size
    // for string data is smaller because it does not include comments, braces,
    // or new lines (unless they are part of a message entry).
    int length = tig_file_filelength(stream);
    mes_file->size = length > 0 ? (size_t)length : 0;
    if (mes_file->size == 0) {
        mes_file->size = 0;
        mes_file_close(stream);
        return;
    }
    mes_file->data = (char*)MALLOC(mes_file->size);
    if (mes_file->data == NULL) {
        mes_file->size = 0;
        mes_file_close(stream);
        return;
    }

    mes_file_entry.str = buffer;

    offset = 0;
    mes_file_parse_line = 1;

    // Parse entries until the end of the file.
    while (parse_entry(stream, &mes_file_entry)) {
        size_t entry_size = strlen(mes_file_entry.str) + 1;
        if (entry_size > (size_t)(mes_file->size - offset)) break;
        // Expand the entries array if necessary.
        if (mes_file->num_entries == mes_file->max_entries) {
            if (mes_file->max_entries > INT_MAX - 10
                || (size_t)mes_file->max_entries + 10 > SIZE_MAX / sizeof(MesFileEntry)) break;
            int capacity = mes_file->max_entries + 10;
            MesFileEntry* resized = (MesFileEntry*)REALLOC(mes_file->entries, sizeof(*resized) * (size_t)capacity);
            if (resized == NULL) break;
            mes_file->entries = resized;
            mes_file->max_entries = capacity;
        }

        // Copy the parsed entry to the entries array and update the data buffer.
        copy_mes_file_entry(&(mes_file->entries[mes_file->num_entries]),
            &mes_file_entry,
            &(mes_file->data[offset]));
        offset += (int)strlen(mes_file_entry.str) + 1;
        mes_file->num_entries++;
    }

    // Update the actual size of the data buffer.
    mes_file->size = offset;
    mes_file_close(stream);

    // Sort entries by number (since all lookups use binary search).
    if (mes_file->num_entries != 0) {
        qsort(mes_file->entries,
            mes_file->num_entries,
            sizeof(MesFileEntry),
            compare_mes_file_entry);
    }

    // Check for duplicate entry numbers.
    check_duplicates(mes_file);
}

/**
 * Parses a single entry from the file stream.
 *
 * 0x4D4A60
 */
bool parse_entry(TigFile* stream, MesFileEntry* mes_file_entry)
{
    char buffer[MAX_STRING];

    // Parse the number field.
    if (!parse_field(stream, buffer)) {
        return false;
    }

    // Validate the number field.
    if (!SDL_isdigit(buffer[0])) {
        tig_debug_printf("Warning! Bad number field on line: %d (%s)\n",
            mes_file_parse_line,
            current_mes_file_path);
    }

    char* end;
    long long identifier = strtoll(buffer, &end, 10);
    if (end == buffer || identifier < INT_MIN || identifier > INT_MAX) return false;
    mes_file_entry->num = (int)identifier;

    // Parse the text field.
    if (!parse_field(stream, buffer)) {
        tig_debug_printf("Warning! Missing text on line: %d (%s)\n",
            mes_file_parse_line,
            current_mes_file_path);
        return false;
    }

    strcpy(mes_file_entry->str, buffer);

    return true;
}

/**
 * Parses a single field enclosed in braces from the file stream.
 *
 * 0x4D4B50
 */
bool parse_field(TigFile* stream, char* buffer)
{
    int ch;
    int pos;
    bool too_long;

    // Skip characters until an opening brace is found.
    ch = consume_next_char(stream);
    while (ch != '{') {
        if (ch == EOF) {
            return false;
        }

        if (ch == '\n') {
            // Track line numbers for error reporting.
            mes_file_parse_line++;
        } else if (ch == '}') {
            tig_debug_printf("Warning! Possible missing left brace { on or before line %d (%s)\n",
                mes_file_parse_line,
                current_mes_file_path);
        }

        ch = consume_next_char(stream);
    }

    pos = 0;
    too_long = false;

    // Read characters until a closing brace is found.
    ch = consume_next_char(stream);
    while (ch != '}') {
        if (ch == EOF) {
            tig_debug_printf("Warning! Expected right bracket }, reached end of file (%s).\n",
                current_mes_file_path);
            return false;
        }

        if (ch == '\n') {
            // Track line numbers for error reporting.
            mes_file_parse_line++;
        } else if (ch == '{') {
            tig_debug_printf("Warning! Possible missing right brace } on or before line %d (%s)\n",
                mes_file_parse_line,
                current_mes_file_path);
        }

        // Store character in buffer if there's space.
        if (pos < MAX_STRING - 1) {
            buffer[pos++] = (char)ch;
        } else {
            too_long = true;
        }

        ch = consume_next_char(stream);
    }

    buffer[pos] = '\0';

    // Log a warning if the string was too long.
    if (too_long) {
        tig_debug_printf("Warning! String too long on line: %d (%s)\n",
            mes_file_parse_line,
            current_mes_file_path);
    }

    return true;
}

/**
 * Opens a mes file and initializes parsing state.
 *
 * 0x4D4C70
 */
TigFile* mes_file_open(const char* path, const char* mode)
{
    mes_file_parse_pos = 0;
    mes_file_parse_len = 0;
    return tig_file_fopen(path, mode);
}

/**
 * Closes a mes file stream.
 *
 * 0x4D4C90
 */
void mes_file_close(TigFile* stream)
{
    tig_file_fclose(stream);
}

/**
 * Reads the next character from the file stream, buffering lines as needed.
 *
 * 0x4D4CA0
 */
int consume_next_char(TigFile* stream)
{
    // Refill the buffer if the current position reaches the end.
    if (mes_file_parse_pos == mes_file_parse_len) {
        mes_file_parse_pos = 0;
        mes_file_parse_len = 0;

        if (tig_file_fgets(mes_file_parse_buffer, sizeof(mes_file_parse_buffer), stream) == NULL) {
            return EOF;
        }

        mes_file_parse_len = (int)strlen(mes_file_parse_buffer);
        if (mes_file_parse_len == 0) {
            return EOF;
        }
    }

    return (unsigned char)mes_file_parse_buffer[mes_file_parse_pos++];
}

/**
 * Compares two `MesFileEntry` structures by their num field for sorting.
 *
 * 0x4D4D10
 */
int compare_mes_file_entry(const void* a, const void* b)
{
    const MesFileEntry* entry_a = (const MesFileEntry*)a;
    const MesFileEntry* entry_b = (const MesFileEntry*)b;

    if (entry_a->num < entry_b->num) {
        return -1;
    } else if (entry_a->num > entry_b->num) {
        return 1;
    } else {
        return 0;
    }
}

/**
 * Copies a `MesFileEntry` and the associated string to a new location.
 *
 * 0x4D4D30
 */
void copy_mes_file_entry(MesFileEntry* dst, MesFileEntry* src, char* str)
{
    *dst = *src;
    dst->str = str;
    strcpy(str, src->str);
}

/**
 * Checks for duplicate entry numbers in a mes file and logs appropriate
 * warnings.
 *
 * 0x4D4D70
 */
void check_duplicates(MesFile* mes_file)
{
    // Loading and merging sort entries before this check. Report each group
    // once without changing entries or the existing binary-search selection.
    for (int first = 0; first < mes_file->num_entries;) {
        int end = first + 1;
        while (end < mes_file->num_entries
            && mes_file->entries[end].num == mes_file->entries[first].num) end++;
        if (end - first > 1) {
            tig_debug_printf("%s: %d lines numbered %d\n", mes_file->path,
                end - first, mes_file->entries[first].num);
        }
        first = end;
    }
}

/**
 * Prints statistics about currently loaded mes files.
 *
 * 0x4D4DD0
 */
void mes_stats(void)
{
    int index;

    for (index = 0; index < mes_files_capacity; index++) {
        if (mes_files[index].refcount > 0) {
            tig_debug_printf("Open Msg File: \"%s\", ref_count=%d.\n",
                mes_files[index].path,
                mes_files[index].refcount);
        }
    }
}
