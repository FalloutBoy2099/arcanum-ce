#ifndef ARCANUM_EXPERIMENT_ASYNC_ASSET_READER_H_
#define ARCANUM_EXPERIMENT_ASYNC_ASSET_READER_H_

#include <SDL3/SDL.h>
#include "tig/asset_source.h"

typedef struct AsyncAssetReader AsyncAssetReader;

typedef struct AsyncAssetResult {
    Uint64 tag;
    bool success;
    bool from_archive;
    void* bytes;
    size_t size;
} AsyncAssetResult;

size_t async_asset_reader_overhead(int capacity);

AsyncAssetReader* async_asset_reader_create(int capacity, size_t file_limit, size_t staging_budget);
bool async_asset_reader_request_sources(AsyncAssetReader* reader, const TigAssetSource* sources, int count, Uint64 generation, Uint64 tag, int priority);
bool async_asset_reader_request_candidates(AsyncAssetReader* reader, const char* const* paths, int count, Uint64 generation, Uint64 tag, int priority);
bool async_asset_reader_request(AsyncAssetReader* reader, const char* absolute_path, Uint64 generation, Uint64 tag, int priority);
bool async_asset_reader_take(AsyncAssetReader* reader, Uint64 generation, Uint64 tag, AsyncAssetResult* result);
void async_asset_reader_reset(AsyncAssetReader* reader, Uint64 generation);
void async_asset_result_release(AsyncAssetResult* result);
void async_asset_reader_destroy(AsyncAssetReader* reader);

#endif
