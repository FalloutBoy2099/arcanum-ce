#include "tig/database.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>

#include <fpattern/fpattern.h>
#include <zlib.h>

#include "tig/compat.h"
#include "tig/find_file.h"
#include "tig/file_position.h"
#include "tig/memory.h"
#include "tig/performance.h"

#define FOURCC_DAT SDL_FOURCC(' ', 'T', 'A', 'D')
#define FOURCC_DAT1 SDL_FOURCC('1', 'T', 'A', 'D')
#define DECOMPRESSION_BUFFER_SIZE 0x4000

typedef struct DecompressionContext {
    /* 0000 */ z_stream zstrm;
    /* 0038 */ unsigned char buffer[DECOMPRESSION_BUFFER_SIZE];
} DecompressionContext;

#define TIG_DATABASE_FILE_UNGOTTEN 0x01
#define TIG_DATABASE_FILE_EOF 0x02
#define TIG_DATABASE_FILE_ERROR 0x04
#define TIG_DATABASE_FILE_TEXT_MODE 0x08

typedef struct TigDatabaseFileHandle {
    unsigned int flags;
    TigDatabase* database;
    TigDatabaseEntry* entry;
    FILE* underlying_stream;
    int pos;
    size_t compressed_pos;
    int ungotten;
    DecompressionContext* decompression_context;
    TigDatabaseFileHandle* next;
} TigDatabaseFileHandle;

static void tig_database_find_prepare(TigDatabaseFindFileData* ffd);
static void tig_database_load_ignored(TigDatabase* database);
static int num_path_segments(const char* path);
static int tig_database_find_entry_by_path(const void* path_ptr, const void* entry_ptr);
static bool tig_database_fclose_internal(TigDatabaseFileHandle* stream);
static bool tig_database_fopen_internal(TigDatabase* database, TigDatabaseEntry* entry, const char* mode, TigDatabaseFileHandle* stream);
static int tig_database_fgetc_internal(TigDatabaseFileHandle* stream);
static size_t tig_database_fread_internal(void* buffer, size_t size, TigDatabaseFileHandle* stream);

// 0x638BBC
static unsigned char tig_database_decompression_buffer[DECOMPRESSION_BUFFER_SIZE];

// 0x63CBC0
static TigDatabase* tig_database_open_databases_head;

// 0x53BC50
TigDatabase* tig_database_open(const char* path)
{
    FILE* stream;
    int id;
    int name_table_size;
    TigGuid guid;
    int entry_table_size;
    int entry_table_offset;
    TigDatabase* database;
    TigDatabaseEntry* entry;
    int64_t offset;
    unsigned int index;
    int name_size;
    char* name;
    int64_t pos;
    int64_t size;
    int64_t table_end;
    int footer_size;
    size_t names_remaining;
    uint32_t stored_offset;

    stream = fopen(path, "rb");
    if (stream == NULL) {
        return NULL;
    }

    if (tig_stdio_seek(stream, -12, SEEK_END) != 0) {
        fclose(stream);
        return false;
    }

    if (fread(&id, sizeof(id), 1, stream) != 1) {
        fclose(stream);
        return false;
    }

    if (fread(&name_table_size, sizeof(name_table_size), 1, stream) != 1) {
        fclose(stream);
        return false;
    }

    footer_size = id == FOURCC_DAT ? 12 : 24;
    if (id == FOURCC_DAT) {
        memset(&guid, 0, sizeof(guid));
    } else if (id == FOURCC_DAT1) {
        if (tig_stdio_seek(stream, -24, SEEK_END) != 0) {
            fclose(stream);
            return false;
        }

        if (fread(&guid, sizeof(guid), 1, stream) != 1) {
            fclose(stream);
            return false;
        }

        if (fread(&id, sizeof(id), 1, stream) != 1) {
            fclose(stream);
            return false;
        }
    } else {
        fclose(stream);
        return false;
    }

    if (fread(&entry_table_offset, sizeof(entry_table_offset), 1, stream) != 1) {
        fclose(stream);
        return false;
    }

    if (tig_stdio_seek(stream, -INT64_C(4) - entry_table_offset, SEEK_END) != 0) {
        fclose(stream);
        return false;
    }

    if (fread(&entry_table_size, sizeof(entry_table_size), 1, stream) != 1) {
        fclose(stream);
        return false;
    }

    pos = tig_stdio_tell(stream);
    if (pos == -1
        || tig_stdio_seek(stream, 0, SEEK_END) != 0) {
        fclose(stream);
        return false;
    }

    size = tig_stdio_tell(stream);
    if (size == -1
        || tig_stdio_seek(stream, pos, SEEK_SET) != 0) {
        fclose(stream);
        return false;
    }

    table_end = size - footer_size;
    if (entry_table_offset < 8 || name_table_size < 0 || pos > table_end
        || (int64_t)entry_table_offset > size - 4) {
        fclose(stream);
        return NULL;
    }

    offset = size - entry_table_size - entry_table_offset;
    if (offset < 0 || offset > size) {
        // Some module PATCH archives created by community tools do not store
        // the optional tree-position marker before the entry table. Their
        // entry offsets are already absolute, matching DAT2.1 extractor docs.
        offset = 0;
    }

    database = (TigDatabase*)MALLOC(sizeof(TigDatabase));

    if (fread(&(database->entries_count), sizeof(database->entries_count), 1, stream) != 1) {
        // FIX: Memory leak.
        FREE(database);
        fclose(stream);
        return NULL;
    }

    pos = tig_stdio_tell(stream);
    if ((uint64_t)database->entries_count > SIZE_MAX / sizeof(TigDatabaseEntry)
        || pos < 0 || pos > table_end
        || database->entries_count > (uint64_t)(table_end - pos) / 25
        || (uint64_t)name_table_size > (uint64_t)(table_end - pos)) {
        FREE(database);
        fclose(stream);
        return NULL;
    }

    database->guid = guid;
    database->path = STRDUP(path);
    database->entries = (TigDatabaseEntry*)MALLOC(sizeof(TigDatabaseEntry) * database->entries_count);
    database->open_file_handles_head = NULL;
    database->name_table = (char*)MALLOC(name_table_size);

    names_remaining = (size_t)name_table_size;
    name = database->name_table;
    for (index = 0; index < database->entries_count; index++) {
        if (fread(&name_size, sizeof(name_size), 1, stream) != 1) {
            break;
        }

        pos = tig_stdio_tell(stream);
        if (name_size <= 0 || (size_t)name_size > names_remaining
            || pos < 0 || table_end - pos < 20
            || name_size > table_end - pos - 20
            || fread(name, name_size, 1, stream) != 1
            || name[name_size - 1] != '\0') {
            break;
        }

        entry = &(database->entries[index]);

        if (tig_stdio_seek(stream, 4, SEEK_CUR) != 0
            || fread(&(entry->flags), sizeof(entry->flags), 1, stream) != 1
            || fread(&(entry->size), sizeof(entry->size), 1, stream) != 1
            || fread(&(entry->compressed_size), sizeof(entry->compressed_size), 1, stream) != 1
            || fread(&stored_offset, sizeof(stored_offset), 1, stream) != 1) {
            break;
        }

        entry->offset = offset + stored_offset;
        if ((entry->flags & TIG_DATABASE_ENTRY_DIRECTORY) == 0) {
            uint32_t stored_size = (entry->flags & TIG_DATABASE_ENTRY_COMPRESSED) != 0
                ? entry->compressed_size : entry->size;
            if (entry->size > INT_MAX || stored_size > INT_MAX
                || entry->offset < 0 || entry->offset > size
                || stored_size > (uint64_t)(size - entry->offset)) {
                break;
            }
        }

        compat_windows_path_to_native(name);
        SDL_strlwr(name);

        entry->path = name;
        entry->flags &= ~TIG_DATABASE_ENTRY_OVERRIDE_PRESENT;
        entry->flags |= TIG_DATABASE_ENTRY_OVERRIDE_UNCHECKED;

        name += name_size;
        names_remaining -= (size_t)name_size;
    }

    fclose(stream);

    if (index < database->entries_count) {
        FREE(database->name_table);
        FREE(database->entries);
        FREE(database->path);
        FREE(database);
        return false;
    }

    database->next = tig_database_open_databases_head;
    tig_database_open_databases_head = database;

    tig_database_load_ignored(database);

    return database;
}

// 0x53BF40
bool tig_database_close(TigDatabase* database)
{
    TigDatabase* prev_database;
    TigDatabase* curr_database;
    TigDatabaseFileHandle* curr_file_handle;
    TigDatabaseFileHandle* next_file_handle;

    prev_database = NULL;
    curr_database = tig_database_open_databases_head;
    while (curr_database != NULL && curr_database != database) {
        prev_database = curr_database;
        curr_database = curr_database->next;
    }

    if (curr_database == NULL) {
        return false;
    }

    if (prev_database != NULL) {
        prev_database->next = curr_database->next;
    } else {
        tig_database_open_databases_head = curr_database->next;
    }

    curr_file_handle = database->open_file_handles_head;
    while (curr_file_handle != NULL) {
        next_file_handle = curr_file_handle->next;
        tig_database_fclose(curr_file_handle);
        curr_file_handle = next_file_handle;
    }

    FREE(database->name_table);
    FREE(database->entries);
    FREE(database->path);
    FREE(database);

    return true;
}

// 0x53BFD0
bool tig_database_find_first_entry(TigDatabase* database, const char* pattern, TigDatabaseFindFileData* ffd)
{
    memset(ffd, 0, sizeof(*ffd));

    ffd->database = database;
    ffd->pattern = STRDUP(pattern);
    ffd->path_segments_count = num_path_segments(pattern);

    compat_windows_path_to_native(ffd->pattern);
    SDL_strlwr(ffd->pattern);

    if (!fpattern_isvalid(pattern)) {
        ffd->index = database->entries_count;
        return false;
    }

    tig_database_find_prepare(ffd);

    return tig_database_find_next_entry(ffd);
}

// 0x53C030
void tig_database_find_prepare(TigDatabaseFindFileData* ffd)
{
    char* pch = ffd->pattern;
    char ch;
    int l;
    int r;
    int m;
    int cmp;

    while (*pch != '\0' && *pch != '*' && *pch != '?') {
        pch++;
    }

    ch = *pch;
    *pch = '\0';

    l = 0;
    r = ffd->database->entries_count - 1;

    // Use binary search to find index of the first entry that match the left
    // side of the pattern (or exact index if supplied pattern is exact file
    // name).
    while (l <= r) {
        m = (l + r) / 2;
        cmp = SDL_strcmp(ffd->database->entries[m].path, ffd->pattern);
        if (cmp < 0) {
            l = m + 1;
        } else if (cmp > 0) {
            r = m - 1;
        } else {
            // See below.
            l = m;
            break;
        }
    }

    *pch = ch;

    ffd->index = l;
}

// 0x53C0D0
bool tig_database_find_next_entry(TigDatabaseFindFileData* ffd)
{
    unsigned int index;

    index = ffd->index;
    if (index >= ffd->database->entries_count) {
        return false;
    }

    while (index < ffd->database->entries_count) {
        if (fpattern_matchn(ffd->pattern, ffd->database->entries[index].path)
            && num_path_segments(ffd->database->entries[index].path) == ffd->path_segments_count
            && (ffd->database->entries[index].flags & TIG_DATABASE_ENTRY_IGNORED) == 0) {
            // Found.
            ffd->name = ffd->database->entries[index].path;
            ffd->size = ffd->database->entries[index].size;
            ffd->is_directory = (ffd->database->entries[index].flags & TIG_DATABASE_ENTRY_DIRECTORY) != 0;

            break;
        }
        index++;
    }

    ffd->index = index + 1;

    return index < ffd->database->entries_count;
}

// 0x53C190
bool tig_database_find_close(TigDatabaseFindFileData* ffd)
{
    FREE(ffd->pattern);
    memset(ffd, 0, sizeof(*ffd));
    return true;
}

// 0x53C1C0
int tig_database_filelength(TigDatabaseFileHandle* stream)
{
    return stream->entry->size;
}

// 0x53C1D0
bool tig_database_get_entry(TigDatabase* database, const char* path, TigDatabaseEntry** entry_ptr)
{
    char native_path[TIG_MAX_PATH];

    strcpy(native_path, path);
    compat_windows_path_to_native(native_path);
    SDL_strlwr(native_path);

    *entry_ptr = (TigDatabaseEntry*)bsearch(native_path, database->entries, database->entries_count, sizeof(*database->entries), tig_database_find_entry_by_path);
    if (*entry_ptr == NULL) {
        return false;
    }

    if (((*entry_ptr)->flags & TIG_DATABASE_ENTRY_IGNORED) != 0) {
        *entry_ptr = NULL;
        return false;
    }

    return true;
}

// 0x53C220
int tig_database_fclose(TigDatabaseFileHandle* stream)
{
    int rc;

    rc = tig_database_fclose_internal(stream) ? 0 : -1;
    FREE(stream);

    return rc;
}

// 0x53C250
int tig_database_fflush(TigDatabaseFileHandle* stream)
{
    // Database does not support writing;
    (void)stream;

    return -1;
}

// 0x53C260
TigDatabaseFileHandle* tig_database_fopen(TigDatabase* database, const char* file_name, const char* mode)
{
    TigDatabaseEntry* entry;

    if (!tig_database_get_entry(database, file_name, &entry)) {
        return false;
    }

    return tig_database_fopen_entry(database, entry, mode);
}

// 0x53C2A0
TigDatabaseFileHandle* tig_database_reopen(TigDatabase* database, const char* path, const char* mode, TigDatabaseFileHandle* stream)
{
    tig_database_fclose_internal(stream);

    TigDatabaseEntry* entry;
    if (!tig_database_get_entry(database, path, &entry)) {
        FREE(stream);
        return NULL;
    }

    if (!tig_database_fopen_internal(database, entry, mode, stream)) {
        FREE(stream);
        return NULL;
    }

    return stream;
}

// 0x53C300
TigDatabaseFileHandle* tig_database_fopen_entry(TigDatabase* database, TigDatabaseEntry* entry, const char* mode)
{
    TigDatabaseFileHandle* stream;

    stream = (TigDatabaseFileHandle*)MALLOC(sizeof(*stream));
    if (!tig_database_fopen_internal(database, entry, mode, stream)) {
        FREE(stream);
        return NULL;
    }

    return stream;
}

// 0x53C340
int tig_database_setbuf(TigDatabaseFileHandle* stream, char* buffer)
{
    if (buffer != NULL) {
        return tig_database_setvbuf(stream, buffer, 0, 512);
    } else {
        return tig_database_setvbuf(stream, NULL, _IONBF, 512);
    }
}

// 0x53C370
int tig_database_setvbuf(TigDatabaseFileHandle* stream, char* buffer, int mode, size_t size)
{
    // Database does not support writing.
    (void)stream;
    (void)buffer;
    (void)mode;
    (void)size;

    return 1;
}

// 0x53C3A0
int tig_database_vfprintf(TigDatabaseFileHandle* stream, const char* fmt, va_list args)
{
    // Database does not support writing.
    (void)stream;
    (void)fmt;
    (void)args;

    return -1;
}

// 0x53C3B0
int tig_database_fgetc(TigDatabaseFileHandle* stream)
{
    int ch;

    ch = tig_database_fgetc_internal(stream);
    if (ch == -1) {
        return -1;
    }

    if ((stream->flags & TIG_DATABASE_FILE_TEXT_MODE) != 0 && ch == '\r') {
        int next = tig_database_fgetc_internal(stream);
        if (next == '\n') return '\n';
        if (next != -1) tig_database_ungetc(next, stream);
    }

    return ch;
}

// 0x53C3F0
char* tig_database_fgets(char* buffer, int max_count, TigDatabaseFileHandle* stream)
{
    if (buffer == NULL || stream == NULL || max_count <= 0) return NULL;
    int count = 0;
    while (count < max_count - 1) {
        int ch = tig_database_fgetc(stream);
        if (ch == -1) {
            if (count == 0) return NULL;
            break;
        }
        buffer[count++] = (unsigned char)ch;
        if (ch == '\n') break;
    }
    buffer[count] = '\0';
    return buffer;
}

// 0x53C440
int tig_database_fputc(int ch, TigDatabaseFileHandle* stream)
{
    // Database does not support writing.
    (void)ch;
    (void)stream;

    return -1;
}

// 0x53C450
int tig_database_fputs(const char* buffer, TigDatabaseFileHandle* stream)
{
    // Database does not support writing.
    (void)buffer;
    (void)stream;

    return -1;
}

// 0x53C460
int tig_database_ungetc(int ch, TigDatabaseFileHandle* stream)
{
    if (stream == NULL || ch == -1 || stream->pos <= 0) {
        return -1;
    }

    if ((stream->flags & (TIG_DATABASE_FILE_UNGOTTEN | TIG_DATABASE_FILE_ERROR)) != 0) {
        return -1;
    }

    stream->flags &= ~TIG_DATABASE_FILE_EOF;
    stream->flags |= TIG_DATABASE_FILE_UNGOTTEN;
    stream->ungotten = (unsigned char)ch;
    stream->pos--;

    return ch;
}

// 0x53C4A0
size_t tig_database_fread(void* buffer, size_t size, size_t count, TigDatabaseFileHandle* stream)
{
    if (stream == NULL || size == 0 || count == 0) return 0;
    if (buffer == NULL || count > SIZE_MAX / size || stream->pos < 0
        || (unsigned int)stream->pos > stream->entry->size) {
        stream->flags |= TIG_DATABASE_FILE_ERROR;
        return 0;
    }
    if ((stream->flags & (TIG_DATABASE_FILE_EOF | TIG_DATABASE_FILE_ERROR)) != 0) return 0;
    size_t bytes_to_read = size * count;
    size_t available = stream->entry->size - (unsigned int)stream->pos;
    if (bytes_to_read > available) {
        bytes_to_read = available;
        stream->flags |= TIG_DATABASE_FILE_EOF;
    }
    size_t bytes_read = 0;
    unsigned char* bytes = buffer;
    if (bytes_to_read != 0 && (stream->flags & TIG_DATABASE_FILE_UNGOTTEN) != 0) {
        stream->flags &= ~TIG_DATABASE_FILE_UNGOTTEN;
        stream->pos++;
        *bytes++ = (unsigned char)stream->ungotten;
        bytes_to_read--;
        bytes_read++;
    }
    if (bytes_to_read != 0) bytes_read += tig_database_fread_internal(bytes, bytes_to_read, stream);
    return bytes_read / size;
}

// 0x53C520
size_t tig_database_fwrite(const void* buffer, size_t size, size_t count, TigDatabaseFileHandle* stream)
{
    // Database does not support writing.
    (void)buffer;
    (void)size;
    (void)stream;

    return count - 1;
}

// 0x53C530
int tig_database_fgetpos(TigDatabaseFileHandle* stream, int* off)
{
    *off = tig_database_ftell(stream);

    // FIXME: `fgetpos` should return 0 on success.
    return 1;
}

// 0x53C550
int tig_database_fseek(TigDatabaseFileHandle* stream, int offset, int origin)
{
    int64_t pos;

    if ((stream->flags & TIG_DATABASE_FILE_ERROR) != 0) {
        return 1;
    }

    // Streams opened in text mode can only be rewinded to the beginning.
    if ((stream->flags & TIG_DATABASE_FILE_TEXT_MODE) != 0
        && offset != 0
        && origin != SEEK_SET) {
        return 1;
    }

    switch (origin) {
    case SEEK_SET:
        pos = offset;
        break;
    case SEEK_CUR:
        pos = (int64_t)offset + stream->pos;
        break;
    case SEEK_END:
        pos = (int64_t)offset + stream->entry->size;
        break;
    default:
        return 1;
    }

    // FIX: Original code treats seeking to the end of stream as error.
    if (pos < 0 || pos > stream->entry->size) {
        stream->flags |= TIG_DATABASE_FILE_ERROR;
        return 1;
    }

    if ((stream->entry->flags & TIG_DATABASE_ENTRY_PLAIN) != 0) {
        if (tig_stdio_seek(stream->underlying_stream, stream->entry->offset + pos, SEEK_SET) != 0) {
            stream->flags |= TIG_DATABASE_FILE_ERROR;
            return 1;
        }
    } else if ((stream->entry->flags & TIG_DATABASE_ENTRY_COMPRESSED) != 0) {
        size_t bytes_to_skip;
        int physical_pos = stream->pos + ((stream->flags & TIG_DATABASE_FILE_UNGOTTEN) != 0 ? 1 : 0);

        if (pos < physical_pos) {
            if (tig_stdio_seek(stream->underlying_stream, stream->entry->offset, SEEK_SET) != 0) {
                stream->flags |= TIG_DATABASE_FILE_ERROR;
                return 1;
            }

            stream->compressed_pos = 0;
            stream->pos = 0;
            inflateEnd(&(stream->decompression_context->zstrm));

            stream->decompression_context->zstrm.next_in = NULL;
            stream->decompression_context->zstrm.avail_in = 0;
            stream->decompression_context->zstrm.next_out = NULL;
            stream->decompression_context->zstrm.avail_out = 0;

            if (inflateInit(&(stream->decompression_context->zstrm)) != Z_OK) {
                stream->flags |= TIG_DATABASE_FILE_ERROR;
                return 1;
            }
        } else {
            stream->pos = physical_pos;
        }
        bytes_to_skip = (size_t)(pos - stream->pos);
        while (bytes_to_skip >= DECOMPRESSION_BUFFER_SIZE) {
            if (tig_database_fread_internal(tig_database_decompression_buffer, DECOMPRESSION_BUFFER_SIZE, stream) != DECOMPRESSION_BUFFER_SIZE) {
                return 1;
            }

            bytes_to_skip -= DECOMPRESSION_BUFFER_SIZE;
        }

        if (bytes_to_skip > 0) {
            if (tig_database_fread_internal(tig_database_decompression_buffer, bytes_to_skip, stream) != bytes_to_skip) {
                return 1;
            }
        }
    }

    if ((stream->flags & TIG_DATABASE_FILE_ERROR) != 0) return 1;
    stream->pos = (int)pos;
    stream->flags &= ~(TIG_DATABASE_FILE_UNGOTTEN | TIG_DATABASE_FILE_EOF);
    return 0;
}

// 0x53C6C0
int tig_database_fsetpos(TigDatabaseFileHandle* stream, int off)
{
    return tig_database_fseek(stream, off, SEEK_SET);
}

// 0x53C6E0
int tig_database_ftell(TigDatabaseFileHandle* stream)
{
    return stream->pos;
}

// 0x53C6F0
void tig_database_rewind(TigDatabaseFileHandle* stream)
{
    tig_database_clearerr(stream);
    tig_database_fseek(stream, 0, SEEK_SET);
}

// 0x53C710
void tig_database_clearerr(TigDatabaseFileHandle* stream)
{
    stream->flags &= ~(TIG_DATABASE_FILE_EOF | TIG_DATABASE_FILE_ERROR);
}

// 0x53C720
int tig_database_feof(TigDatabaseFileHandle* stream)
{
    return stream->flags & TIG_DATABASE_FILE_EOF;
}

// 0x53C730
int tig_database_ferror(TigDatabaseFileHandle* stream)
{
    return stream->flags & TIG_DATABASE_FILE_ERROR;
}

// 0x53CCE0
void tig_database_load_ignored(TigDatabase* database)
{
    char path[TIG_MAX_PATH];
    SDL_PathInfo path_info;
    FILE* stream;
    char file_name[TIG_MAX_PATH];
    size_t file_name_length;
    int file_name_segments_count;
    unsigned int index;

    strcpy(path, database->path);
    strcat(path, ".ignore");

    if (!SDL_GetPathInfo(path, &path_info)) {
        return;
    }

    stream = fopen(path, "rt");
    if (stream == NULL) {
        return;
    }

    while (fgets(file_name, sizeof(file_name), stream) != NULL) {
        file_name_length = strlen(file_name);
        if (file_name[file_name_length - 1] == '\n') {
            file_name[file_name_length - 1] = '\0';
        }

        compat_windows_path_to_native(file_name);
        SDL_strlwr(file_name);

        if (fpattern_isvalid(file_name)) {
            file_name_segments_count = num_path_segments(file_name);

            for (index = 0; index < database->entries_count; index++) {
                if (fpattern_matchn(file_name, database->entries[index].path)
                    && num_path_segments(database->entries[index].path) == file_name_segments_count) {
                    database->entries[index].flags |= TIG_DATABASE_ENTRY_IGNORED;
                }
            }
        }
    }

    fclose(stream);
}

// 0x53CE80
int num_path_segments(const char* path)
{
    int count;
    const char* pch;

    count = 0;
    pch = path;
    while (*pch != '\0') {
        if (*pch == '\\' || *pch == '/') {
            count++;
        }
        pch++;
    }

    return count;
}

// 0x53CEB0
int tig_database_find_entry_by_path(const void* path_ptr, const void* entry_ptr)
{
    const char* path = (const char*)path_ptr;
    const TigDatabaseEntry* entry = (const TigDatabaseEntry*)entry_ptr;
    return SDL_strcmp(path, entry->path);
}

// 0x53CED0
bool tig_database_fclose_internal(TigDatabaseFileHandle* stream)
{
    TigDatabaseFileHandle* prev;
    TigDatabaseFileHandle* curr;

    prev = NULL;
    curr = stream->database->open_file_handles_head;
    while (curr != NULL && curr != stream) {
        prev = curr;
        curr = curr->next;
    }

    if (curr == NULL) {
        return false;
    }

    if (prev != NULL) {
        prev->next = curr->next;
    } else {
        stream->database->open_file_handles_head = curr->next;
    }

    fclose(stream->underlying_stream);

    if ((stream->entry->flags & TIG_DATABASE_ENTRY_COMPRESSED) != 0) {
        inflateEnd(&(stream->decompression_context->zstrm));
        FREE(stream->decompression_context);
    }

    memset(stream, 0, sizeof(*stream));

    return true;
}

// 0x53CF50
bool tig_database_fopen_internal(TigDatabase* database, TigDatabaseEntry* entry, const char* mode, TigDatabaseFileHandle* stream)
{
    if (mode[0] == 'w') {
        return false;
    }

    memset(stream, 0, sizeof(*stream));

    stream->underlying_stream = fopen(database->path, "rb");
    if (stream->underlying_stream == NULL) {
        return false;
    }

    if (tig_stdio_seek(stream->underlying_stream, entry->offset, SEEK_SET) != 0) {
        fclose(stream->underlying_stream);
        return false;
    }

    stream->entry = entry;

    if (mode[1] == 't') {
        stream->flags |= TIG_DATABASE_FILE_TEXT_MODE;
    }

    if ((entry->flags & TIG_DATABASE_ENTRY_COMPRESSED) != 0) {
        stream->decompression_context = (DecompressionContext*)MALLOC(sizeof(DecompressionContext));
        stream->decompression_context->zstrm.next_in = stream->decompression_context->buffer;
        stream->decompression_context->zstrm.avail_in = 0;
        stream->decompression_context->zstrm.zalloc = Z_NULL;
        stream->decompression_context->zstrm.zfree = Z_NULL;
        stream->decompression_context->zstrm.opaque = Z_NULL;

        if (inflateInit(&(stream->decompression_context->zstrm)) != Z_OK) {
            FREE(stream->decompression_context);
            fclose(stream->underlying_stream);
            return false;
        }
    }

    stream->database = database;
    stream->next = database->open_file_handles_head;
    database->open_file_handles_head = stream;

    return true;
}

// 0x53D040
int tig_database_fgetc_internal(TigDatabaseFileHandle* stream)
{
    unsigned char ch;

    if ((stream->flags & TIG_DATABASE_FILE_UNGOTTEN) != 0) {
        stream->flags &= ~TIG_DATABASE_FILE_UNGOTTEN;
        stream->pos++;
        return stream->ungotten;
    }

    if ((unsigned int)stream->pos >= stream->entry->size) {
        stream->flags |= TIG_DATABASE_FILE_EOF;
    }

    if ((stream->flags & (TIG_DATABASE_FILE_EOF | TIG_DATABASE_FILE_ERROR)) != 0) {
        return -1;
    }

    if (tig_database_fread_internal(&ch, sizeof(ch), stream) != sizeof(ch)) {
        return -1;
    }

    return ch;
}

// 0x53D0A0
size_t tig_database_fread_internal(void* buffer, size_t size, TigDatabaseFileHandle* stream)
{
    if (size == 0) return 0;
    if (buffer == NULL || stream->pos < 0 || (unsigned int)stream->pos > stream->entry->size
        || size > stream->entry->size - (unsigned int)stream->pos) {
        stream->flags |= TIG_DATABASE_FILE_ERROR;
        return 0;
    }
    size_t produced = 0;
    if ((stream->entry->flags & TIG_DATABASE_ENTRY_PLAIN) != 0) {
        Uint64 started = tig_performance_begin();
        produced = fread(buffer, 1, size, stream->underlying_stream);
        tig_performance_end(TIG_PERF_ARCHIVE_READ, started);
        tig_performance_count(TIG_PERF_ARCHIVE_READ_BYTES, produced);
        if (produced != size) stream->flags |= TIG_DATABASE_FILE_ERROR;
    } else if ((stream->entry->flags & TIG_DATABASE_ENTRY_COMPRESSED) != 0) {
        z_stream* inflater = &stream->decompression_context->zstrm;
        inflater->next_out = buffer;
        inflater->avail_out = (uInt)size;
        while (inflater->avail_out != 0) {
            if (inflater->avail_in == 0) {
                if (stream->compressed_pos >= stream->entry->compressed_size) {
                    stream->flags |= TIG_DATABASE_FILE_ERROR;
                    break;
                }
                size_t remaining = stream->entry->compressed_size - stream->compressed_pos;
                size_t requested = remaining < DECOMPRESSION_BUFFER_SIZE ? remaining : DECOMPRESSION_BUFFER_SIZE;
                Uint64 started = tig_performance_begin();
                size_t actual = fread(stream->decompression_context->buffer, 1, requested, stream->underlying_stream);
                tig_performance_end(TIG_PERF_ARCHIVE_READ, started);
                tig_performance_count(TIG_PERF_ARCHIVE_READ_BYTES, actual);
                stream->compressed_pos += actual;
                if (actual == 0) {
                    stream->flags |= TIG_DATABASE_FILE_ERROR;
                    break;
                }
                inflater->avail_in = (uInt)actual;
                inflater->next_in = stream->decompression_context->buffer;
            }
            uInt input_before = inflater->avail_in;
            uInt output_before = inflater->avail_out;
            Uint64 started = tig_performance_begin();
            int status = inflate(inflater, Z_NO_FLUSH);
            tig_performance_end(TIG_PERF_ARCHIVE_INFLATE, started);
            if (status == Z_STREAM_END) {
                if (inflater->avail_out != 0) stream->flags |= TIG_DATABASE_FILE_ERROR;
                break;
            }
            if (status != Z_OK || (input_before == inflater->avail_in && output_before == inflater->avail_out)) {
                stream->flags |= TIG_DATABASE_FILE_ERROR;
                break;
            }
        }
        produced = size - inflater->avail_out;
    } else {
        stream->flags |= TIG_DATABASE_FILE_ERROR;
    }
    tig_performance_count(TIG_PERF_ARCHIVE_DECODED_BYTES, produced);
    stream->pos += (int)produced;
    return produced;
}
