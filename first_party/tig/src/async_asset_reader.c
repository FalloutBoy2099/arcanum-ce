#include "tig/async_asset_reader.h"

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <zlib.h>
#include "tig/file_position.h"

typedef enum AsyncAssetState {
    ASYNC_ASSET_EMPTY,
    ASYNC_ASSET_QUEUED,
    ASYNC_ASSET_READING,
    ASYNC_ASSET_READY,
} AsyncAssetState;

typedef struct AsyncAssetJob {
    AsyncAssetState state;
    TigAssetSource sources[TIG_ASSET_SOURCE_CAPACITY];
    int path_count;
    Uint64 generation;
    Uint64 tag;
    Uint64 order;
    int priority;
    size_t reserved;
    AsyncAssetResult result;
} AsyncAssetJob;

struct AsyncAssetReader {
    SDL_Mutex* mutex;
    SDL_Condition* condition;
    SDL_Thread* thread;
    AsyncAssetJob* jobs;
    int capacity;
    size_t file_limit;
    size_t budget;
    size_t reserved;
    Uint64 generation;
    Uint64 order;
    bool stopping;
};

static void clear_job(AsyncAssetReader* reader, AsyncAssetJob* job)
{
    SDL_free(job->result.bytes);
    reader->reserved -= job->reserved;
    memset(job, 0, sizeof(*job));
}

static AsyncAssetResult read_file(const TigAssetSource* source, size_t limit, Uint64 tag, bool* opened)
{
    AsyncAssetResult result = { .tag = tag, .from_archive = source->archive };
    FILE* file = fopen(source->path, "rb");
    *opened = file != NULL;
    if (file == NULL) return result;
    int64_t length = -1;
    if (source->archive) {
        if (source->offset >= 0 && source->size <= limit && source->size <= INT_MAX
            && tig_stdio_seek(file, source->offset, SEEK_SET) == 0) length = source->size;
    } else if (tig_stdio_seek(file, 0, SEEK_END) == 0) {
        length = tig_stdio_tell(file);
        if (tig_stdio_seek(file, 0, SEEK_SET) != 0) length = -1;
    }
    if (length >= 0 && (uint64_t)length <= limit && length <= INT_MAX) {
        result.bytes = SDL_malloc(length == 0 ? 1 : (size_t)length);
        if (result.bytes != NULL) {
            if (!source->compressed) {
                result.success = fread(result.bytes, 1, (size_t)length, file) == (size_t)length && !ferror(file);
            } else {
                z_stream stream = { 0 };
                unsigned char input[4096];
                uint32_t remaining = source->stored_size;
                stream.next_out = result.bytes;
                stream.avail_out = length == 0 ? 1 : (uInt)length;
                if (inflateInit(&stream) == Z_OK) {
                    int status = Z_OK;
                    while (status == Z_OK) {
                        if (stream.avail_in == 0 && remaining != 0) {
                            size_t chunk = remaining < sizeof(input) ? remaining : sizeof(input);
                            if (fread(input, 1, chunk, file) != chunk) break;
                            remaining -= (uint32_t)chunk;
                            stream.next_in = input;
                            stream.avail_in = (uInt)chunk;
                        }
                        status = inflate(&stream, Z_NO_FLUSH);
                    }
                    result.success = status == Z_STREAM_END && stream.total_out == (uLong)length
                        && stream.total_in == source->stored_size;
                    inflateEnd(&stream);
                }
            }
            if (result.success) result.size = (size_t)length;
            else { SDL_free(result.bytes); result.bytes = NULL; }
        }
    }
    fclose(file);
    return result;
}

static int SDLCALL asset_reader_worker(void* context)
{
    AsyncAssetReader* reader = context;
    SDL_LockMutex(reader->mutex);
    while (!reader->stopping) {
        AsyncAssetJob* selected = NULL;
        for (int i = 0; i < reader->capacity; i++) {
            AsyncAssetJob* job = &reader->jobs[i];
            if (job->state == ASYNC_ASSET_QUEUED && (selected == NULL
                || job->priority > selected->priority
                || (job->priority == selected->priority && job->order < selected->order))) selected = job;
        }
        if (selected == NULL) {
            SDL_WaitCondition(reader->condition, reader->mutex);
            continue;
        }
        selected->state = ASYNC_ASSET_READING;
        TigAssetSource sources[TIG_ASSET_SOURCE_CAPACITY];
        int path_count = selected->path_count;
        memcpy(sources, selected->sources, sizeof(sources));
        Uint64 generation = selected->generation;
        Uint64 tag = selected->tag;
        SDL_UnlockMutex(reader->mutex);
        AsyncAssetResult result = { .tag = tag };
        for (int i = 0; i < path_count; i++) {
            bool opened;
            result = read_file(&sources[i], reader->file_limit, tag, &opened);
            if (opened) break;
        }
        SDL_LockMutex(reader->mutex);
        if (reader->stopping || generation != reader->generation) {
            SDL_free(result.bytes);
            clear_job(reader, selected);
        } else {
            reader->reserved -= selected->reserved;
            selected->reserved = result.success ? (result.size == 0 ? 1 : result.size) : 0;
            reader->reserved += selected->reserved;
            selected->result = result;
            selected->state = ASYNC_ASSET_READY;
        }
    }
    SDL_UnlockMutex(reader->mutex);
    return 0;
}

size_t async_asset_reader_overhead(int capacity)
{
    if (capacity <= 0 || capacity > 4096) return 0;
    return sizeof(AsyncAssetReader) + (size_t)capacity * sizeof(AsyncAssetJob);
}

AsyncAssetReader* async_asset_reader_create(int capacity, size_t file_limit, size_t staging_budget)
{
    if (capacity <= 0 || capacity > 4096 || file_limit == 0 || file_limit > staging_budget) return NULL;
    AsyncAssetReader* reader = SDL_calloc(1, sizeof(*reader));
    if (reader == NULL) return NULL;
    reader->jobs = SDL_calloc(capacity, sizeof(*reader->jobs));
    reader->mutex = SDL_CreateMutex();
    reader->condition = SDL_CreateCondition();
    reader->capacity = capacity;
    reader->file_limit = file_limit;
    reader->budget = staging_budget;
    if (reader->jobs == NULL || reader->mutex == NULL || reader->condition == NULL) {
        async_asset_reader_destroy(reader);
        return NULL;
    }
    reader->thread = SDL_CreateThread(asset_reader_worker, "asset-reader", reader);
    if (reader->thread == NULL) { async_asset_reader_destroy(reader); return NULL; }
    return reader;
}

bool async_asset_reader_request(AsyncAssetReader* reader, const char* path, Uint64 generation, Uint64 tag, int priority)
{
    return async_asset_reader_request_candidates(reader, &path, 1, generation, tag, priority);
}

bool async_asset_reader_request_candidates(AsyncAssetReader* reader, const char* const* paths, int count, Uint64 generation, Uint64 tag, int priority)
{
    if (paths == NULL || count < 1 || count > TIG_ASSET_SOURCE_CAPACITY) return false;
    TigAssetSource sources[TIG_ASSET_SOURCE_CAPACITY] = { 0 };
    for (int i = 0; i < count; i++) {
        if (paths[i] == NULL || strlen(paths[i]) >= sizeof(sources[i].path)) return false;
        SDL_strlcpy(sources[i].path, paths[i], sizeof(sources[i].path));
    }
    return async_asset_reader_request_sources(reader, sources, count, generation, tag, priority);
}

bool async_asset_reader_request_sources(AsyncAssetReader* reader, const TigAssetSource* sources, int count, Uint64 generation, Uint64 tag, int priority)
{
    if (reader == NULL || sources == NULL || count < 1 || count > TIG_ASSET_SOURCE_CAPACITY) return false;
    for (int i = 0; i < count; i++) {
        const char* path = sources[i].path;
        size_t length = SDL_strnlen(path, sizeof(sources[i].path));
        if (length == sizeof(sources[i].path) || length < 1
            || !(path[0] == '/' || (length > 2 && path[1] == ':' && (path[2] == '/' || path[2] == '\\'))
                || (length > 2 && path[0] == '\\' && path[1] == '\\'))) return false;
        if (sources[i].compressed && (!sources[i].archive || sources[i].stored_size > reader->file_limit)) return false;
    }
    SDL_LockMutex(reader->mutex);
    if (reader->stopping || generation != reader->generation) { SDL_UnlockMutex(reader->mutex); return false; }
    AsyncAssetJob* slot = NULL;
    for (int i = 0; i < reader->capacity; i++) {
        AsyncAssetJob* job = &reader->jobs[i];
        if (job->state != ASYNC_ASSET_EMPTY && job->generation == generation && job->tag == tag) {
            bool same = job->path_count == count;
            for (int candidate = 0; same && candidate < count; candidate++) same = strcmp(job->sources[candidate].path, sources[candidate].path) == 0
                && job->sources[candidate].offset == sources[candidate].offset
                && job->sources[candidate].size == sources[candidate].size
                && job->sources[candidate].stored_size == sources[candidate].stored_size
                && job->sources[candidate].archive == sources[candidate].archive
                && job->sources[candidate].compressed == sources[candidate].compressed;
            if (same && priority > job->priority) job->priority = priority;
            SDL_UnlockMutex(reader->mutex);
            return same;
        }
        if (job->state == ASYNC_ASSET_EMPTY && slot == NULL) slot = job;
    }
    if (slot == NULL || reader->reserved > reader->budget - reader->file_limit) {
        SDL_UnlockMutex(reader->mutex);
        return false;
    }
    *slot = (AsyncAssetJob){ .state = ASYNC_ASSET_QUEUED, .generation = generation, .tag = tag,
        .order = reader->order++, .priority = priority, .reserved = reader->file_limit };
    slot->path_count = count;
    memcpy(slot->sources, sources, count * sizeof(*sources));
    reader->reserved += slot->reserved;
    SDL_SignalCondition(reader->condition);
    SDL_UnlockMutex(reader->mutex);
    return true;
}

bool async_asset_reader_take(AsyncAssetReader* reader, Uint64 generation, Uint64 tag, AsyncAssetResult* result)
{
    if (reader == NULL || result == NULL) return false;
    SDL_LockMutex(reader->mutex);
    if (generation == reader->generation) {
        for (int i = 0; i < reader->capacity; i++) {
            AsyncAssetJob* job = &reader->jobs[i];
            if (job->state == ASYNC_ASSET_READY && job->generation == generation && job->tag == tag) {
                *result = job->result;
                job->result.bytes = NULL;
                clear_job(reader, job);
                SDL_UnlockMutex(reader->mutex);
                return true;
            }
        }
    }
    SDL_UnlockMutex(reader->mutex);
    return false;
}

void async_asset_reader_reset(AsyncAssetReader* reader, Uint64 generation)
{
    if (reader == NULL) return;
    SDL_LockMutex(reader->mutex);
    if (generation > reader->generation) {
        reader->generation = generation;
        for (int i = 0; i < reader->capacity; i++) {
            if (reader->jobs[i].state != ASYNC_ASSET_READING) clear_job(reader, &reader->jobs[i]);
        }
    }
    SDL_UnlockMutex(reader->mutex);
}

void async_asset_result_release(AsyncAssetResult* result)
{
    if (result == NULL) return;
    SDL_free(result->bytes);
    memset(result, 0, sizeof(*result));
}

void async_asset_reader_destroy(AsyncAssetReader* reader)
{
    if (reader == NULL) return;
    if (reader->thread != NULL) {
        SDL_LockMutex(reader->mutex);
        reader->stopping = true;
        SDL_SignalCondition(reader->condition);
        SDL_UnlockMutex(reader->mutex);
        SDL_WaitThread(reader->thread, NULL);
    }
    if (reader->jobs != NULL) {
        for (int i = 0; i < reader->capacity; i++) SDL_free(reader->jobs[i].result.bytes);
        SDL_free(reader->jobs);
    }
    if (reader->condition != NULL) SDL_DestroyCondition(reader->condition);
    if (reader->mutex != NULL) SDL_DestroyMutex(reader->mutex);
    SDL_free(reader);
}
