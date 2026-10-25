#include "tig/asset_prefetch.h"

#include <string.h>

#include "tig/async_asset_reader.h"
#include "tig/cache_budget.h"
#include "tig/debug.h"
#include <inttypes.h>
#include <limits.h>
#include "tig/file.h"
#include "tig/performance.h"


typedef enum AssetPrefetchState {
    ASSET_PREFETCH_EMPTY,
    ASSET_PREFETCH_QUEUED,
    ASSET_PREFETCH_READING,
    ASSET_PREFETCH_READY,
    ASSET_PREFETCH_FAILED,
} AssetPrefetchState;

typedef struct AssetPrefetchEntry {
    AssetPrefetchState state;
    char path[TIG_MAX_PATH];
    TigAssetSource* sources;
    int source_count;
    int priority;
    int pins;
    Uint64 generation;
    Uint64 tag;
    Uint64 last_use;
    void* bytes;
    size_t size;
    bool from_archive;
} AssetPrefetchEntry;

static AsyncAssetReader* reader;
static AssetPrefetchEntry* entries;
static Uint64 generation;
static Uint64 sequence;
static size_t resident_bytes;
static size_t metadata_bytes;
static size_t peak_resident_bytes;
static uint64_t eviction_count;
static TigAssetPrefetchConfig active_config;
static TigAssetPrefetchConfig requested_config;
static bool configured;

static bool asset_config_valid(const TigAssetPrefetchConfig* config)
{
    if (config->entries <= 0 || config->entries > 8192 || config->reader_jobs <= 0 || config->reader_jobs > 4096
        || config->file_limit == 0 || config->file_limit > INT_MAX) return false;
    size_t entries_size = (size_t)config->entries * sizeof(AssetPrefetchEntry);
    size_t reader_size = async_asset_reader_overhead(config->reader_jobs);
    return config->cache_bytes > entries_size && config->file_limit <= config->cache_bytes - entries_size
        && config->reader_bytes > reader_size && config->file_limit <= config->reader_bytes - reader_size;
}

bool tig_asset_prefetch_configure(const TigAssetPrefetchConfig* config)
{
    if (reader != NULL || entries != NULL) return false;
    if (config != NULL && !asset_config_valid(config)) return false;
    configured = config != NULL;
    if (configured) requested_config = *config;
    return true;
}

void tig_asset_prefetch_statistics(TigAssetPrefetchStatistics* statistics)
{
    if (statistics == NULL) return;
    *statistics = (TigAssetPrefetchStatistics){ .config = active_config, .resident_bytes = resident_bytes,
        .metadata_bytes = metadata_bytes, .peak_resident_bytes = peak_resident_bytes, .evictions = eviction_count };
    if (entries != NULL) for (int index = 0; index < active_config.entries; index++) {
        if (entries[index].state == ASSET_PREFETCH_READY) statistics->ready_entries++;
        if (entries[index].pins != 0) statistics->pinned_entries++;
    }
}

static void asset_sources_clear(AssetPrefetchEntry* entry)
{
    if (entry->sources != NULL) {
        metadata_bytes -= (size_t)entry->source_count * sizeof(*entry->sources);
        SDL_free(entry->sources);
        entry->sources = NULL;
    }
    entry->source_count = 0;
}

static void asset_entry_clear(AssetPrefetchEntry* entry)
{
    asset_sources_clear(entry);
    resident_bytes -= entry->size;
    SDL_free(entry->bytes);
    memset(entry, 0, sizeof(*entry));
}

static AssetPrefetchEntry* asset_eviction_candidate(bool data_only)
{
    AssetPrefetchEntry* oldest = NULL;
    for (int i = 0; i < active_config.entries; i++) {
        AssetPrefetchEntry* entry = &entries[i];
        if (entry->pins == 0 && (!data_only || entry->size != 0) && entry->state != ASSET_PREFETCH_READING
            && (oldest == NULL || entry->last_use < oldest->last_use)) oldest = entry;
    }
    return oldest;
}

static void asset_evict(AssetPrefetchEntry* entry)
{
    if (entry->state == ASSET_PREFETCH_READY) {
        eviction_count++;
        tig_performance_count(TIG_PERF_PREFETCH_EVICTIONS, 1);
        tig_performance_count(TIG_PERF_PREFETCH_EVICTED_BYTES, entry->size);
    }
    asset_entry_clear(entry);
}

static bool asset_make_room(size_t bytes)
{
    if (bytes > active_config.cache_bytes) return false;
    while (metadata_bytes > active_config.cache_bytes - bytes
        || resident_bytes > active_config.cache_bytes - bytes - metadata_bytes) {
        AssetPrefetchEntry* oldest = asset_eviction_candidate(true);
        if (oldest == NULL) return false;
        asset_evict(oldest);
    }
    return true;
}

bool tig_asset_prefetch_enable(bool enabled)
{
    if (!enabled) {
        if (entries != NULL) for (int i = 0; i < active_config.entries; i++) if (entries[i].pins != 0) return false;
        if (reader != NULL) async_asset_reader_destroy(reader);
        reader = NULL;
        if (entries != NULL) {
            tig_debug_printf("Asset cache: peak=%zu bytes, evictions=%" PRIu64 ".\n", peak_resident_bytes, eviction_count);
            for (int i = 0; i < active_config.entries; i++) asset_entry_clear(&entries[i]);
            SDL_free(entries);
        }
        entries = NULL;
        resident_bytes = metadata_bytes = peak_resident_bytes = 0;
        eviction_count = 0;
        memset(&active_config, 0, sizeof(active_config));
        return true;
    }
    if (reader != NULL) return true;
    if (configured) active_config = requested_config;
    else {
        const TigCacheBudget* budget = tig_cache_budget_get();
        active_config = (TigAssetPrefetchConfig){ (size_t)budget->prefetch_bytes, (size_t)budget->reader_bytes,
            (size_t)budget->file_limit, budget->prefetch_entries, budget->reader_jobs };
    }
    if (!asset_config_valid(&active_config)) { memset(&active_config, 0, sizeof(active_config)); return false; }
    entries = SDL_calloc(active_config.entries, sizeof(*entries));
    if (entries != NULL) metadata_bytes = (size_t)active_config.entries * sizeof(*entries);
    reader = async_asset_reader_create(active_config.reader_jobs, active_config.file_limit,
        active_config.reader_bytes - async_asset_reader_overhead(active_config.reader_jobs));
    if (entries == NULL || reader == NULL) { tig_asset_prefetch_enable(false); return false; }
    generation++;
    async_asset_reader_reset(reader, generation);
    tig_debug_printf("Asset cache budgets: entries=%d cache=%zu reader=%zu file=%zu bytes.\n",
        active_config.entries, active_config.cache_bytes, active_config.reader_bytes, active_config.file_limit);
    return true;
}

bool tig_asset_prefetch_is_enabled(void)
{
    return reader != NULL;
}

void tig_asset_prefetch_reset(void)
{
    if (reader == NULL) return;
    generation++;
    async_asset_reader_reset(reader, generation);
    for (int i = 0; i < active_config.entries; i++) {
        if (entries[i].pins == 0) asset_entry_clear(&entries[i]);
    }
}

void tig_asset_prefetch_poll(void)
{
    if (reader == NULL) return;
    for (int i = 0; i < active_config.entries; i++) {
        AssetPrefetchEntry* entry = &entries[i];
        if (entry->generation != generation || entry->state != ASSET_PREFETCH_READING) continue;
        AsyncAssetResult result;
        if (!async_asset_reader_take(reader, generation, entry->tag, &result)) continue;
        asset_sources_clear(entry);
        if (result.success && asset_make_room(result.size)) {
            entry->bytes = result.bytes;
            entry->from_archive = result.from_archive;
            entry->size = result.size;
            resident_bytes += result.size;
            if (resident_bytes > peak_resident_bytes) peak_resident_bytes = resident_bytes;
            result.bytes = NULL;
            entry->state = ASSET_PREFETCH_READY;
            tig_performance_count(TIG_PERF_PREFETCH_READY_BYTES, entry->size);
            if (entry->from_archive) tig_performance_count(TIG_PERF_PREFETCH_ARCHIVE_READY_BYTES, entry->size);
        } else entry->state = ASSET_PREFETCH_FAILED;
        async_asset_result_release(&result);
    }
    for (;;) {
        AssetPrefetchEntry* entry = NULL;
        for (int i = 0; i < active_config.entries; i++) {
            AssetPrefetchEntry* candidate = &entries[i];
            if (candidate->generation == generation && candidate->state == ASSET_PREFETCH_QUEUED
                && (entry == NULL || candidate->priority > entry->priority
                    || (candidate->priority == entry->priority && candidate->tag < entry->tag))) entry = candidate;
        }
        if (entry == NULL) break;
        if (async_asset_reader_request_sources(reader, entry->sources, entry->source_count, generation, entry->tag, entry->priority))
            entry->state = ASSET_PREFETCH_READING;
        else break;
    }
}

void tig_asset_prefetch_request(const char* path, int priority)
{
    if (reader == NULL || path == NULL || strlen(path) >= TIG_MAX_PATH) return;
    tig_asset_prefetch_poll();
    AssetPrefetchEntry* slot = NULL;
    for (int i = 0; i < active_config.entries; i++) {
        AssetPrefetchEntry* entry = &entries[i];
        if (entry->state != ASSET_PREFETCH_EMPTY && entry->generation == generation && SDL_strcasecmp(entry->path, path) == 0) {
            if (priority > entry->priority) {
                entry->priority = priority;
                if (entry->state == ASSET_PREFETCH_READING) {
                    async_asset_reader_request_sources(reader, entry->sources, entry->source_count, generation, entry->tag, priority);
                }
            }
            entry->last_use = ++sequence;
            return;
        }
        if (entry->state == ASSET_PREFETCH_EMPTY && slot == NULL) slot = entry;
    }
    if (slot == NULL) {
        slot = asset_eviction_candidate(false);
        if (slot != NULL) asset_evict(slot);
    }
    if (slot == NULL) return;
    asset_entry_clear(slot);
    TigAssetSource sources[TIG_ASSET_SOURCE_CAPACITY];
    int source_count = tig_file_snapshot_sources(path, sources);
    for (int i = 0; i < source_count; i++) {
        const TigAssetSource* source = &sources[i];
        if (source->archive && (source->size > active_config.file_limit
                || (source->compressed && source->stored_size > active_config.file_limit))) {
            source_count = 0;
            break;
        }
    }
    if (source_count == 0) {
        tig_performance_count(TIG_PERF_PREFETCH_SOURCE_REJECTIONS, 1);
        return;
    }
    size_t source_bytes = (size_t)source_count * sizeof(*sources);
    if (!asset_make_room(source_bytes)) return;
    slot->sources = SDL_malloc(source_bytes);
    if (slot->sources == NULL) return;
    memcpy(slot->sources, sources, source_bytes);
    slot->source_count = source_count;
    metadata_bytes += source_bytes;
    SDL_strlcpy(slot->path, path, sizeof(slot->path));
    slot->state = ASSET_PREFETCH_QUEUED;
    slot->generation = generation;
    slot->tag = ++sequence;
    slot->last_use = sequence;
    slot->priority = priority;
    tig_performance_count(TIG_PERF_PREFETCH_REQUESTS, 1);
    tig_asset_prefetch_poll();
}

static bool asset_acquire_ready(const char* path, TigAssetData* data, bool measured)
{
    if (reader == NULL || path == NULL || data == NULL) return false;
    tig_asset_prefetch_poll();
    for (int i = 0; i < active_config.entries; i++) {
        AssetPrefetchEntry* entry = &entries[i];
        if (entry->state == ASSET_PREFETCH_READY && entry->generation == generation && SDL_strcasecmp(entry->path, path) == 0) {
            entry->pins++;
            entry->last_use = ++sequence;
            *data = (TigAssetData){ entry->bytes, entry->size, entry };
            if (measured) {
                tig_performance_count(TIG_PERF_PREFETCH_HITS, 1);
                if (entry->from_archive) tig_performance_count(TIG_PERF_PREFETCH_ARCHIVE_HITS, 1);
            }
            return true;
        }
    }
    if (measured) tig_performance_count(TIG_PERF_PREFETCH_MISSES, 1);
    return false;
}

bool tig_asset_prefetch_acquire(const char* path, TigAssetData* data)
{
    return asset_acquire_ready(path, data, true);
}

bool tig_asset_prefetch_pin_ready(const char* path, TigAssetData* data)
{
    return asset_acquire_ready(path, data, false);
}

void tig_asset_prefetch_release(TigAssetData* data)
{
    if (data == NULL || data->token == NULL) return;
    AssetPrefetchEntry* entry = data->token;
    entry->pins--;
    if (entry->pins == 0 && entry->generation != generation) asset_entry_clear(entry);
    memset(data, 0, sizeof(*data));
}
