#include <stdio.h>
#include <string.h>

#include <tig/async_asset_reader.h>
#include <tig/file.h>
#include <tig/memory.h>
#include "game/sector_art_preview.h"

int main(int argc, char** argv)
{
    if (argc != 3) { fprintf(stderr, "Usage: sector_prefetch_probe repositories.txt resource\n"); return 2; }
    if (!SDL_Init(0) || tig_memory_init(NULL) != TIG_OK) return 1;
    FILE* manifest = fopen(argv[1], "r");
    if (manifest == NULL) return 1;
    char path[512];
    int failures = 0;
    while (fgets(path, sizeof(path), manifest) != NULL) {
        path[strcspn(path, "\r\n")] = '\0';
        if (path[0] != '\0' && !tig_file_repository_add(path)) failures++;
    }
    fclose(manifest);
    TigAssetSource sources[TIG_ASSET_SOURCE_CAPACITY];
    int count = tig_file_snapshot_sources(argv[2], sources);
    AsyncAssetReader* reader = async_asset_reader_create(2, 2 * 1024 * 1024, 4 * 1024 * 1024);
    bool accepted = async_asset_reader_request_sources(reader, sources, count, 0, 1, 1);
    AsyncAssetResult data = { 0 };
    bool ready = false;
    Uint64 deadline = SDL_GetTicks() + 10000;
    while (accepted && !(ready = async_asset_reader_take(reader, 0, 1, &data)) && SDL_GetTicks() < deadline) SDL_Delay(1);
    bool equal = false;
    if (ready && data.success) {
        TigFile* file = tig_file_fopen(argv[2], "rb");
        if (file != NULL) {
            int length = tig_file_filelength(file);
            void* expected = length >= 0 ? SDL_malloc(length == 0 ? 1 : length) : NULL;
            if (expected != NULL && (size_t)length == data.size && tig_file_fread(expected, 1, length, file) == data.size)
                equal = memcmp(expected, data.bytes, data.size) == 0;
            SDL_free(expected);
            tig_file_fclose(file);
        }
    }
    uint32_t ids[SECTOR_ART_PREVIEW_CAPACITY];
    size_t art_count = 0;
    bool parsed = data.success && sector_art_preview_read(data.bytes, data.size, ids, &art_count);
    printf("sources=%d archive=%d compressed=%d accepted=%d ready=%d bytes=%llu identical_to_vfs=%d parsed=%d unique_art=%llu\n",
        count, count > 0 && sources[count - 1].archive, count > 0 && sources[count - 1].compressed,
        accepted, ready, (unsigned long long)data.size, equal, parsed, (unsigned long long)art_count);
    if (!equal || !parsed) failures++;
    async_asset_result_release(&data);
    async_asset_reader_destroy(reader);
    tig_file_exit();
    tig_memory_exit();
    SDL_Quit();
    return failures != 0;
}
