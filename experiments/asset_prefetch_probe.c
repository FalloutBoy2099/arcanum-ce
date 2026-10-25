#include <stdio.h>
#include <string.h>

#include <tig/asset_prefetch.h>
#include <tig/file.h>
#include <tig/memory.h>
#include <tig/performance.h>

int main(int argc, char** argv)
{
    if (argc != 4) { fprintf(stderr, "Usage: asset_prefetch_probe repositories.txt resource capture.csv\n"); return 2; }
    if (!SDL_Init(0) || tig_memory_init(NULL) != TIG_OK) return 1;
    FILE* manifest = fopen(argv[1], "r");
    if (manifest == NULL) return 1;
    char path[512];
    int repositories = 0, failures = 0;
    while (fgets(path, sizeof(path), manifest) != NULL) {
        path[strcspn(path, "\r\n")] = '\0';
        if (path[0] == '\0') continue;
        if (!tig_file_repository_add(path)) failures++;
        else repositories++;
    }
    fclose(manifest);
    TigAssetSource candidates[TIG_ASSET_SOURCE_CAPACITY];
    int count = tig_file_snapshot_sources(argv[2], candidates);
    int archives = 0;
    for (int i = 0; i < count; i++) if (candidates[i].archive) archives++;
    printf("repositories=%d candidates=%d archives=%d\n", repositories, count, archives);
    if (!tig_performance_set_enabled(true, argv[3]) || !tig_asset_prefetch_enable(true)) return 1;
    tig_asset_prefetch_request(argv[2], 100);
    TigAssetData data = { 0 };
    Uint64 deadline = SDL_GetTicks() + 10000;
    bool ready = false;
    while (!(ready = tig_asset_prefetch_acquire(argv[2], &data)) && SDL_GetTicks() < deadline) SDL_Delay(1);
    if (!ready) failures++;
    bool equal = false;
    if (ready) {
        TigFile* file = tig_file_fopen(argv[2], "rb");
        if (file == NULL) failures++;
        else {
            int length = tig_file_filelength(file);
            void* expected = length >= 0 ? SDL_malloc(length == 0 ? 1 : length) : NULL;
            if (expected != NULL && (size_t)length == data.size && tig_file_fread(expected, 1, length, file) == data.size)
                equal = memcmp(expected, data.bytes, data.size) == 0;
            if (!equal) failures++;
            SDL_free(expected);
            tig_file_fclose(file);
        }
    }
    printf("ready=%d bytes=%llu identical_to_vfs=%d requests=%llu hits=%llu source_rejections=%llu limit_rejections=%llu archive_bytes=%llu archive_hits=%llu\n",
        ready, (unsigned long long)data.size, equal,
        (unsigned long long)tig_performance_counter(TIG_PERF_PREFETCH_REQUESTS),
        (unsigned long long)tig_performance_counter(TIG_PERF_PREFETCH_HITS),
        (unsigned long long)tig_performance_counter(TIG_PERF_PREFETCH_SOURCE_REJECTIONS),
        (unsigned long long)tig_performance_counter(TIG_PERF_PREFETCH_SOURCE_LIMIT_REJECTIONS),
        (unsigned long long)tig_performance_counter(TIG_PERF_PREFETCH_ARCHIVE_READY_BYTES),
        (unsigned long long)tig_performance_counter(TIG_PERF_PREFETCH_ARCHIVE_HITS));
    tig_asset_prefetch_release(&data);
    tig_asset_prefetch_enable(false);
    tig_performance_set_enabled(false, NULL);
    tig_file_exit();
    tig_memory_exit();
    SDL_Quit();
    return failures != 0;
}
