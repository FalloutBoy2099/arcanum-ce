#ifndef TIG_ASSET_PREFETCH_H_
#define TIG_ASSET_PREFETCH_H_

#include "tig/types.h"

typedef struct TigAssetData {
    const void* bytes;
    size_t size;
    void* token;
} TigAssetData;

typedef struct TigAssetPrefetchConfig {
    size_t cache_bytes;
    size_t reader_bytes;
    size_t file_limit;
    int entries;
    int reader_jobs;
} TigAssetPrefetchConfig;

typedef struct TigAssetPrefetchStatistics {
    TigAssetPrefetchConfig config;
    size_t resident_bytes;
    size_t metadata_bytes;
    size_t peak_resident_bytes;
    uint64_t evictions;
    size_t ready_entries;
    size_t pinned_entries;
} TigAssetPrefetchStatistics;

bool tig_asset_prefetch_configure(const TigAssetPrefetchConfig* config);
void tig_asset_prefetch_statistics(TigAssetPrefetchStatistics* statistics);

bool tig_asset_prefetch_enable(bool enabled);
bool tig_asset_prefetch_is_enabled(void);
void tig_asset_prefetch_request(const char* path, int priority);
void tig_asset_prefetch_poll(void);
bool tig_asset_prefetch_pin_ready(const char* path, TigAssetData* data);
bool tig_asset_prefetch_acquire(const char* path, TigAssetData* data);
void tig_asset_prefetch_release(TigAssetData* data);
void tig_asset_prefetch_reset(void);

#endif
