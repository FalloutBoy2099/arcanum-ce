#include "game/benchmark_data.h"
#include <tig/file.h>
#include <stdio.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)

int main(void)
{
    CHECK(SDL_Init(0));
    CHECK(SDL_CreateDirectory("fingerprint-repository"));
    CHECK(SDL_CreateDirectory("fingerprint-repository/Save"));
    CHECK(SDL_CreateDirectory("fingerprint-repository/Save/Current"));
    const char* physical = "fingerprint-repository/Save/Current/benchmark-payload.bin";
    const char* virtual_path = "Save\\Current\\benchmark-payload.bin";
    unsigned char data[20017];
    for (int i = 0; i < sizeof(data); i++) data[i] = (unsigned char)(i * 31);
    FILE* file = fopen(physical, "wb");
    CHECK(file != NULL);
    if (file == NULL) return 1;
    CHECK(fwrite(data, 1, sizeof(data), file) == sizeof(data));
    fclose(file);
    CHECK(tig_file_repository_add("fingerprint-repository"));
    // This is the precise bug: C-runtime fopen misses a mounted module path.
    FILE* wrong = fopen(virtual_path, "rb");
    CHECK(wrong == NULL);
    if (wrong != NULL) fclose(wrong);
    Uint32 crc;
    Uint64 length;
    CHECK(benchmark_fingerprint(virtual_path, &crc, &length) == BENCHMARK_FINGERPRINT_OK);
    CHECK(length == sizeof(data) && crc == SDL_crc32(0, data, sizeof(data)));
    CHECK(benchmark_fingerprint("Save/Current/benchmark-payload.bin", &crc, &length) == BENCHMARK_FINGERPRINT_OK);
    CHECK(length == sizeof(data) && crc == SDL_crc32(0, data, sizeof(data)));
    file = fopen(physical, "wb");
    CHECK(file != NULL);
    if (file != NULL) fclose(file);
    CHECK(benchmark_fingerprint(virtual_path, &crc, &length) == BENCHMARK_FINGERPRINT_OK);
    CHECK(length == 0 && crc == 0);
    CHECK(tig_file_remove(virtual_path) == 0);
    CHECK(benchmark_fingerprint(virtual_path, &crc, &length) == BENCHMARK_FINGERPRINT_MISSING);
    CHECK(length == 0 && crc == 0);
    CHECK(tig_file_repository_remove("fingerprint-repository"));
    tig_file_exit();
    SDL_Quit();
    printf("Benchmark virtual save fingerprints: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
