#include "game/benchmark_data.h"
#include <tig/file.h>

BenchmarkFingerprintStatus benchmark_fingerprint(const char* virtual_path, Uint32* crc, Uint64* length)
{
    *crc = 0;
    *length = 0;
    TigFile* file = tig_file_fopen(virtual_path, "rb");
    if (file == NULL) return BENCHMARK_FINGERPRINT_MISSING;
    unsigned char buffer[8192];
    size_t bytes;
    while ((bytes = tig_file_fread(buffer, 1, sizeof(buffer), file)) != 0) {
        *crc = SDL_crc32(*crc, buffer, bytes);
        *length += bytes;
    }
    bool failed = tig_file_ferror(file) != 0;
    if (tig_file_fclose(file) != 0) failed = true;
    return failed ? BENCHMARK_FINGERPRINT_ERROR : BENCHMARK_FINGERPRINT_OK;
}
