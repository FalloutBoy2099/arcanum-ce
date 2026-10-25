#include <stdio.h>
#include <string.h>

#include <tig/database.h>
#include <tig/file.h>
#include <tig/memory.h>
#include <tig/performance.h>

static int benchmark_native_manifest(const char* manifest, const char* capture)
{
    FILE* list = fopen(manifest, "r");
    if (list == NULL || !tig_performance_set_enabled(true, capture)) return 1;
    int failures = 0;
    for (int pass = 0; pass < 3; pass++) {
        rewind(list);
        unsigned int selected = 0;
        Uint64 decoded = 0;
        Uint64 pass_start = SDL_GetPerformanceCounter();
        char path[512];
        while (fgets(path, sizeof(path), list) != NULL) {
            path[strcspn(path, "\r\n")] = '\0';
            if (path[0] == '\0') continue;
            selected++;
            Uint64 open_start = tig_performance_begin();
            TigFile* file = tig_file_fopen(path, "rb");
            tig_performance_end(TIG_PERF_ART_FILE_OPEN, open_start);
            if (file == NULL) { failures++; continue; }
            int size = tig_file_filelength(file);
            if (size <= 0 || size > 4 * 1024 * 1024) { failures++; tig_file_fclose(file); continue; }
            void* buffer = SDL_malloc(size);
            if (buffer == NULL) { failures++; tig_file_fclose(file); continue; }
            Uint64 start = SDL_GetPerformanceCounter();
            size_t count = tig_file_fread(buffer, 1, size, file);
            double elapsed = (SDL_GetPerformanceCounter() - start) * 1000.0 / SDL_GetPerformanceFrequency();
            printf("asset pass=%d file=%s bytes=%d read_ms=%.3f\n", pass, path, size, elapsed);
            if (count != (size_t)size) failures++;
            decoded += count;
            tig_file_fclose(file);
            SDL_free(buffer);
        }
        printf("pass=%d selected=%u decoded_bytes=%llu total_ms=%.3f failures=%d\n", pass, selected,
            (unsigned long long)decoded, (SDL_GetPerformanceCounter() - pass_start) * 1000.0 / SDL_GetPerformanceFrequency(), failures);
        tig_performance_flush();
    }
    fclose(list);
    tig_performance_set_enabled(false, NULL);
    tig_memory_exit();
    SDL_Quit();
    return failures != 0;
}

int main(int argc, char** argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: asset_io_benchmark archive.dat|@manifest.txt capture.csv\n");
        return 2;
    }
    if (!SDL_Init(0) || tig_memory_init(NULL) != TIG_OK) return 1;
    if (argv[1][0] == '@') return benchmark_native_manifest(argv[1] + 1, argv[2]);
    TigDatabase* database = tig_database_open(argv[1]);
    if (database == NULL) { fprintf(stderr, "Cannot open archive\n"); return 1; }
    unsigned int eligible = 0;
    for (unsigned int i = 0; i < database->entries_count; i++) {
        TigDatabaseEntry* entry = &database->entries[i];
        size_t length = strlen(entry->path);
        if (length >= 4 && SDL_strcasecmp(entry->path + length - 4, ".art") == 0
            && entry->size > 0 && entry->size <= 4 * 1024 * 1024
            && (entry->flags & TIG_DATABASE_ENTRY_DIRECTORY) == 0) eligible++;
    }
    unsigned int stride = eligible > 64 ? eligible / 64 : 1;
    if (!tig_performance_set_enabled(true, argv[2])) return 1;
    int failures = 0;
    for (int pass = 0; pass < 3; pass++) {
        unsigned int ordinal = 0, selected = 0;
        Uint64 decoded = 0;
        Uint64 pass_start = SDL_GetPerformanceCounter();
        for (unsigned int i = 0; i < database->entries_count && selected < 64; i++) {
            TigDatabaseEntry* entry = &database->entries[i];
            size_t length = strlen(entry->path);
            if (length < 4 || SDL_strcasecmp(entry->path + length - 4, ".art") != 0
                || entry->size == 0 || entry->size > 4 * 1024 * 1024
                || (entry->flags & TIG_DATABASE_ENTRY_DIRECTORY) != 0) continue;
            if (ordinal++ % stride != 0) continue;
            selected++;
            void* buffer = SDL_malloc(entry->size);
            TigDatabaseFileHandle* file = tig_database_fopen_entry(database, entry, "rb");
            if (buffer == NULL || file == NULL) {
                failures++;
                SDL_free(buffer);
                if (file != NULL) tig_database_fclose(file);
                continue;
            }
            Uint64 start = SDL_GetPerformanceCounter();
            size_t count = tig_database_fread(buffer, 1, entry->size, file);
            double elapsed = (SDL_GetPerformanceCounter() - start) * 1000.0 / SDL_GetPerformanceFrequency();
            printf("asset pass=%d file=%s bytes=%u read_ms=%.3f\n", pass, entry->path, entry->size, elapsed);
            if (count != entry->size) failures++;
            decoded += count;
            tig_database_fclose(file);
            SDL_free(buffer);
        }
        printf("pass=%d selected=%u decoded_bytes=%llu total_ms=%.3f failures=%d\n", pass, selected,
            (unsigned long long)decoded, (SDL_GetPerformanceCounter() - pass_start) * 1000.0 / SDL_GetPerformanceFrequency(), failures);
        tig_performance_flush();
    }
    tig_performance_set_enabled(false, NULL);
    tig_database_close(database);
    tig_memory_exit();
    SDL_Quit();
    return failures != 0;
}
